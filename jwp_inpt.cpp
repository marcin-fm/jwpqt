//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//===================================================================//

//===================================================================
//
//  This module is the primary processor of keyboard strokes from the 
//  user.  The main entry point is to the routine JWP_file::do_char.
//  This routine processes the key strokes.  For ASCII and JASCII modes,
//  this routine processes the characters directly.  For kanji mode,
//  the routines are passed to the KANA_convert::do_char routine which
//  handles converting the character to kana, and interfacinging with 
//  the kanji conversion rotuines.
//
//-------------------------------------------------------------------
//
//  Special note for Windows CE PPC's
//
//  Special handling needs to be done in the case of PPC's.  When the 
//  user renders a i character using a line and dot, the sequence of 
//  actions we will receive from the system is as follows:
//
//      'l' character
//      <back-space>
//      'i' character
//
//  This presents a serious problem for JWPce, since if you try to enter
//  ki, the sequence is kl<BS>i.  The l causes the k to be removed, and 
//  the BS removes the l, resulting in just an i.  
//
//  This mess is fixed as follows: 
//
//  First the state table is modified to add the l's following any 
//  character that can be followed by an i.  The l's all point to a 
//  state KANA_PPC_L.  This state cannot be resolved, so the next character
//  will destroy all of these caracters.
//
//  Second the KANA_convert::erase() rutine, has been modified.  If the
//  current state is KANA_PPC_L, then the erase will not simply 
//  erase, but will remove only the last character.
//
#include "jwpce.h" 
#include "jwp_conf.h"
#include "jwp_conv.h"
#include "jwp_file.h"
#include "jwp_inpt.h"
#include "jwp_stat.h"

//===================================================================
//
//  Static data tables.
//
//  Tables used for character coversions not local to just one routine.
//

//--------------------------------
//
//  Table relates the basic kana to there ascii representations.
//  To actually generate a JIS code from this table, you would find
//  the string match, then add 0x0021 to generate an offset from a 
//  base value.  The two base values are:
//
//          BASE_KATAKANA   0x2500  for katakana
//          BASE_HIRAGANA   0x2400  for hiragana.
//
char direct_kana[SIZE_DIRECT][4] = {
  "+a" ,"a" ,"+i","i" ,"+u" ,"u" ,"+e" ,"e"  ,"+o" ,"o" ,
  "ka" ,"ga","ki","gi","ku" ,"gu","ke" ,"ge" ,"ko" ,"go",
  "sa" ,"za","si","zi","su" ,"zu","se" ,"ze" ,"so" ,"zo",
  "ta" ,"da","ti","di","+tu","tu","du" ,"te" ,"de" ,"to",
  "do" ,"na","ni","nu","ne" ,"no","ha" ,"ba" ,"pa" ,"hi",
  "bi" ,"pi","hu","bu","pu" ,"he","be" ,"pe" ,"ho" ,"bo",
  "po" ,"ma","mi","mu","me" ,"mo","+ya","ya" ,"+yu","yu",
  "+yo","yo","ra","ri","ru" ,"re","ro" ,"+wa","wa" ,"wi",
  "we" ,"wo","n'",
};

//
//  Compound kana takes out most of the kana's alternate names, and most
//  of the odd kana conversions.  Note that regular compounds such as nya,
//  byo, etc are processed later.
//
//  Table of compound kana that are not of the simple form nya,byo, etc.
//  Simple form kana are made directly using a different section of code.
//  These kana are simply scanned against the table.  The table results 
//  in two kana codes that are combined with the base value to form 
//  two characters.  Placing a zero in the second value suppresses 
//  generation of the second character.
//
struct compound_kana compound_kana[] = {
  { "sha" , { 0x37,0x63 } },
  { "shi" , { 0x37,0x0  } },
  { "shu" , { 0x37,0x65 } },
  { "she" , { 0x37,0x27 } },
  { "sho" , { 0x37,0x67 } },
  { "ja"  , { 0x38,0x63 } },
  { "ji"  , { 0x38,0x0  } },
  { "ju"  , { 0x38,0x65 } },
  { "je"  , { 0x38,0x27 } },
  { "jo"  , { 0x38,0x67 } },
  { "jya" , { 0x38,0x63 } },
  { "jyu" , { 0x38,0x65 } },
  { "jyo" , { 0x38,0x67 } },
  { "chi" , { 0x41,0x0  } },
  { "ci"  , { 0x41,0x0  } },
  { "cha" , { 0x41,0x63 } },
  { "chu" , { 0x41,0x65 } },
  { "che" , { 0x41,0x27 } },
  { "cho" , { 0x41,0x67 } },
  { "tsu" , { 0x44,0x0  } },
  { "tzu" , { 0x44,0x0  } },
  { "dsu" , { 0x45,0x0  } },
  { "dzu" , { 0x45,0x0  } },
  { "+tsu", { 0x43,0x0  } },
  { "+tzu", { 0x43,0x0  } },
  { "la"  , { 0x69,0x0  } },
  { "li"  , { 0x6a,0x0  } },
  { "lu"  , { 0x6b,0x0  } },
  { "le"  , { 0x6c,0x0  } },
  { "lo"  , { 0x6d,0x0  } },
  { "lya" , { 0x6a,0x63 } },
  { "lyu" , { 0x6a,0x65 } },
  { "lyo" , { 0x6a,0x67 } },
  { "fa"  , { 0x55,0x21 } },
  { "fi"  , { 0x55,0x23 } },
  { "fu"  , { 0x55,0x0  } },
  { "fe"  , { 0x55,0x27 } },
  { "fo"  , { 0x55,0x29 } },
  { "ye"  , { 0x24,0x27 } },
  { "kwa" , { 0x2f,0x21 } },
  { "kwi" , { 0x2f,0x23 } },
  { "kwe" , { 0x2f,0x27 } },
  { "kwo" , { 0x2f,0x29 } },
  { "gwa" , { 0x30,0x21 } },
  { "gwi" , { 0x30,0x23 } },
  { "gwe" , { 0x30,0x27 } },
  { "gwo" , { 0x30,0x29 } },
  { "n"   , { 0x73,0x0  } },
  { "tha" , { 0x46,0x21 } },
  { "thi" , { 0x46,0x23 } },
  { "thu" , { 0x46,0x25 } },
  { "the" , { 0x46,0x27 } },
  { "tho" , { 0x46,0x29 } },
  { "dha" , { 0x47,0x21 } },
  { "dhi" , { 0x47,0x23 } },
  { "dhu" , { 0x47,0x25 } },
  { "dhe" , { 0x47,0x27 } },
  { "dho" , { 0x47,0x29 } },
  
  { ""    , { 0x00,0x00 } }
};

//--------------------------------
//
//  Table relates JASCII characters to ASCII characters.
//
//  Note that ASCII entries are often duplicated, because more than 
//  one JASCII code maps back to the same ASCII character.  During 
//  conversion from ASCII->JASCII the first entry in the table should
//  be taken.
//
#define SIZE_JASCII ((int) (sizeof(jascii_kanji)/sizeof(struct jascii_kanji)))

static struct jascii_kanji {
  char  ascii;      // ASCII character.    
  KANJI kanji;      // JASCII code.
} jascii_kanji[] = {
  { ' ',  0x2121 },
  { ',',  0x2122 },
  { '.',  0x2123 },
  { ':',  0x2127 },
  { ';',  0x2128 },
  { '?',  0x2129 },
  { '!',  0x212a },
  { '\"', 0x212b },
  { '^',  0x2130 },
  { '_',  0x2132 },
  { '-',  0x213c },
  { '-',  0x213d },
  { '-',  0x213e },
  { '/',  0x213f },
  { '\\', 0x2140 },
  { '~',  0x2141 },
  { '|',  0x2143 },
  { '`',  0x2146 },
  { '\'', 0x2147 },
  { '\"', 0x2148 },
  { '\"', 0x2149 },
  { '(',  0x214a },
  { ')',  0x214b },
  { '(',  0x214c },
  { ')',  0x214d },
  { '[',  0x214e },
  { ']',  0x214f },
  { '{',  0x2150 },
  { '}',  0x2151 },
  { '<',  0x2152 },
  { '>',  0x2153 },
  { '[',  0x215a },
  { ']',  0x215b },
  { '+',  0x215c },
  { '-',  0x215d },
  { 'x',  0x215f },
  { '=',  0x2161 },
  { '$',  0x2170 },
  { '%',  0x2173 },
  { '#',  0x2174 },
  { '&',  0x2175 },
  { '*',  0x2176 },
  { '@',  0x2177 },
  { '*',  0x2179 },
  { '*',  0x217a },
  { '*',  0x2228 },
  { '=',  0x222e },
  { '0',  0x2330 },
  { '1',  0x2331 },
  { '2',  0x2332 },
  { '3',  0x2333 },
  { '4',  0x2334 },
  { '5',  0x2335 },
  { '6',  0x2336 },
  { '7',  0x2337 },
  { '8',  0x2338 },
  { '9',  0x2339 },
};

//===================================================================
//
//  Static internal routines.  
//
//  Most of these are simply character processing routines.
//

#define CRANGE(ch,base,len) (((ch) >= (base)) && ((ch) < ((base)+(len))))   // Is ch in range starting at base and going for length.

//--------------------------------
//
//  Convert ASCII code to JASCII code.
//
//      ch     -- Input ASCII value.
//
//      RETURN -- JASCII value or zero to indicate an error.
//
static int ascii_to_jascii (int ch) {
  int i;
  if ((ch >= 'A') && (ch <= 'Z')) return (0x2341+ch-'A');   // Order is importaint here to to get
  if ((ch >= 'a') && (ch <= 'z')) return (0x2361+ch-'a');   //   correct processing of the x char.
  for (i = 0; i < SIZE_JASCII; i++) {
    if (jascii_kanji[i].ascii == ch) return (jascii_kanji[i].kanji);
  }
  return (0);
}
  
//--------------------------------
//
//  Converts direct kana from ASCII to kana codes.  In this case 
//  direct kana are simply the kana arrange the way they are in the 
//  JIS codes.  This allows simply converting to kana codes by 
//  matching the caracters and counting.
//
//      string -- String to be converted.
//
//      RETURN -- A value of zero is returned if the code could not
//                be converted any other value can be cobined with 
//                a base value to make an actuall kana code:
//
//                  BASE_KATAKANA   0x2500  for katakana
//                  BASE_HIRAGANA   0x2400  for hiragana.
//
//  Note the direct_kana table does not use pointer to strings, but
//  embends the strings directly in the table (saves lots of space), 
//  so string length is limited to 3 real characters, plus space.
//  Other codes must be handled via other coversion tables.
//
static int find_direct (char *string) {
  int i;
  for (i = 0; i < SIZE_DIRECT; i++) if (!strcmp(string,direct_kana[i])) return (i+0x21);
  return (0);
}

//===================================================================
//
//  Exported routines.
//
//  Most of these are simply character processing routines used in other places.
//

//--------------------------------
//
//  Identifies the class of a character.
//
//      ch     -- Character to be identifed.
//
//      RETURN -- Character class:
//
//              CLASS_JUNK   -- Not a valid character.
//              CLASS_SPACE  -- space, tab, etc.
//              CLASS_ASCII  -- ASCII letter or number.
//              CLASS_APUNCT -- ASCII puncuation
//              CLASS_JASCII -- JASCII letter or number
//              CLASS_KPUNCT -- JASCII/KANJI punctuation
//              CLASS_KANA   -- Hirigana or katakana
//              CLASS_KANJI  -- Kanji.
//
int char_class (int ch) {
  if (!ch)            return (CLASS_JUNK);
  if (ISKANJI(ch))    return (CLASS_KANJI);
  if (ISSPACE(ch))    return (CLASS_SPACE);
  if (ISKATAKANA(ch)) return (CLASS_KATAKANA);
  if (ISHIRAGANA(ch)) return (CLASS_HIRAGANA);
  if (ISJASCII(ch))   return (CLASS_JASCII);
  if (isalnum(ch))    return (CLASS_ASCII);
  if (ISASCII(ch))    return (CLASS_APUNCT);
  return (CLASS_KPUNCT);
}

//--------------------------------
//
//  Convert JASCII to ASCII.
//
//      ch     -- Input JASCII value.    
//
//      RETURN -- ASCII value or zero to indicate an error.
//
int jascii_to_ascii (int ch) {
  int i;
  for (i = 0; i < SIZE_JASCII; i++) {
    if (jascii_kanji[i].kanji == ch) return (jascii_kanji[i].ascii);
  }
  if (CRANGE(ch,0x2341,26)) return (ch-0x2341+'A');
  if (CRANGE(ch,0x2361,26)) return (ch-0x2361+'a');
  return (0);
}

//--------------------------------
//  
//  Converts a JIS kanji code into its ascii resprsentation.  The 
//  JIS code is check to make sure it is a valid kana.
//
//      kana   -- JIS code to convert.
//  
//      RETURN -- A pointer to a string for th ascii representation
//                of the code.  DO NOT MODIFY STRING!
//
char *kana_to_ascii (int kana) {
  if (!ISKANA(kana)) return (NULL);
  kana &= 0x007f;
  if ((kana < 0x21) || (kana > 0x73)) return (NULL);
  return (direct_kana[kana-0x21]);
}

//===================================================================
//
//  Begin Class KANA_convert.
//
//  This specialized class simply handles conversion of ascii characters
//  to kana characters, and interfaces with the kanji generation rotuines.
//

KANA_convert kana_convert;      // Class instance.

//--------------------------------
//
//  The state table realy should be imbedded in the routine 
//  KANA_convert::do_char, since this is the only place this is 
//  actually used.  I have placed it here, however, because the 
//  state flags are required elseware, and I wanted to keep all 
//  the parts together.
//
//  Understanding the state machine.  
//
//  The state machine is important to understanding the conversion of
//  ASCII to KANA.  The number of possible and impossible ASCII 
//  characters is very large, and not very systamatic.  To solve this 
//  problem most of the difficult work is done via the state system.
//  Some special cases are processed via exception.
//
//  The current state is an index into the kana_states table.  The 
//  state index is the first of a number of entries indicateing 
//  possible other states we can go to.  The length of this entry 
//  indicates the number of entries associated with this state.
//
//  To move to another state, from the current one, start moving 
//  through the number of enries.  At each one check the current 
//  character against the valid characters to enter the state.  If 
//  your character is found, the next marker indicates the next 
//  state.  If the next state is KANA_DONE, you cannot go beyond this
//  point.
//
//  A special value of KANA_PENDING can be returned as the state.
//  this is a special value used for aeiou that can be pending during
//  until the next character is typed.
//
//  Note in working with the state table, the table is very compact,
//  and that entries are often used for more than one state.  For 
//  example KANA_C entry contains the first entry of the KANA_S 
//  entry, which extends over the KANA_NORMAL entry.
//
//  Also note that the chracters are embedded into the table, i.e. 
//  not pointers to strings (saves space).
//
typedef struct kana_state {
  char valid[6];    // Valid characters to enter this state.
  byte length;      // Number of diffent state lines for this groupt.  (Only valid on first line of state group.)
  byte next;        // Next state.  KANA_DONE indicates we are at the end.
} kana_state;

#define KANA_DONE       100 // We are done, i.e. cannot go on.
#define KANA_BASE       0   // Table enry point.

#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
                            // The folloiwing are internal codes, not to be used by others.
#define KANA_F          15  // fa,fi,fu,fe,fo  f-
#define KANA_N          18  // na,ni,nu,ne,no  nyu,nyo,nya  n',n"
#define KANA_NORMAL_Y   19  // xa,xi,xu,xe,xo  xyu,xyo,xya
#define KANA_C          22  // chi,cha,chu,che,cho  ci
#define KANA_S          24  // sa,si,su,se,so  sha,shi,shu,she,sho
#define KANA_T          25  // ta,ti,tu,te,to  tsu,tzu  tha,the,thi,tho,thu
#define KANA_NORMAL     26  // xa,xi,xu,xe,xo
#define KANA_PLUS       30  // +a,+i,+u,+e,+o  +tu,+tsu,+tzu  +ya,+yu,+yo  +wa  +ka,+ke
#define KANA_GK         34  // ka,ki,ku,ke,ko  kya,kyu,kyo  kwa,kwi,kwe,kwo
#define KANA__TSU       38  // tu  tsu,tzu  (from +tu,+tsu,+tzu)
#define KANA_TSU        40  // tsu,tzu   (from +tsu,+tzu,tzu,tsu)
#define KANA_Y          41  // ya,yu,yo  ye,y=
#define KANA_GKNY       42  // xya,xyu,xyo   (from g, k, etc.)
#define KANA__WA        43  // +wa
#define KANA_W          44  // wa,wi,we,wo
#define KANA__K         46  // +ka +ke
#define KANA_UP         47  // ^^  ^.  ^-, ^+
#define KANA_AIEUO      48  // tha the tho thu thi
#define KANA_PPC_L      50  // Holding point for l received before a real i.
#define KANA_PENDING    51  // Place holder used to mark kana as waiting for next latter to resolve.

static kana_state kana_states[] = {
  { "'`"   ,15,KANA_DONE     }, // 00
  { "+"    ,0 ,KANA_PLUS     }, // 01   +a,+i,+u,+e,+o  +tu,+tsu,+tzu  +ya,+yu,+yo  +wa
  { "v"    ,0 ,KANA_NORMAL   }, // 02   va,vi,vu,ve,vo
  { "kg"   ,0 ,KANA_GK       }, // 03   ka,ki,ku,ke,ko  kya,kyu,kyo  kwa,kwi,kwe,kwo
  { "s"    ,0 ,KANA_S        }, // 04   sa,si,su,se,so  sha,shi,shu,she,sho  sya,syu,syo
  { "td"   ,0 ,KANA_T        }, // 05   ta,ti,tu,te,to  tsu,tzu  tya,tyu,tyo
  { "hbprm",0 ,KANA_NORMAL_Y }, // 06   ha,hi,hu,he,ho  hyu,hyo,hya
  { "jlz"  ,0 ,KANA_NORMAL_Y }, // 07
  { "y"    ,0 ,KANA_Y        }, // 08   ya,yu,yo  ye,y=
  { "w"    ,0 ,KANA_W        }, // 09   wa,wi,we,wo
  { "f"    ,0 ,KANA_F        }, // 10   fa,fi,fu,fe,fo  f-
  { "c"    ,0 ,KANA_C        }, // 11   chi,cha,chu,che,cho  ci
  { "n"    ,0 ,KANA_N        }, // 12   na,ni,nu,ne,no  nyu,nyo,nya  n'
  { "aieuo",0 ,KANA_PENDING  }, // 13
  { "^"    ,0 ,KANA_UP       }, // 14   ^^, ^., ^-

  { "aieuo",3 ,KANA_DONE     }, // 15   KANA_F 
  { "l"    ,0 ,KANA_PPC_L    }, // 16
  { "-"    ,0 ,KANA_DONE     }, // 17   

  { "'"    ,4 ,KANA_DONE     }, // 18   KANA_N
  { "aieuo",3 ,KANA_DONE     }, // 19   KANA_NORMAL_Y
  { "l"    ,0 ,KANA_PPC_L    }, // 20
  { "y"    ,0 ,KANA_GKNY     }, // 21

  { "i"    ,3 ,KANA_DONE     }, // 22   KANA_C
  { "l"    ,0 ,KANA_PPC_L    }, // 23
  { "h"    ,4 ,KANA_NORMAL   }, // 24   KANA_S
  { "y"    ,5 ,KANA_GKNY     }, // 25   KANA_T
  { "aiueo",2 ,KANA_DONE     }, // 26   KANA_NORMAL
  { "l"    ,0 ,KANA_PPC_L    }, // 27
  { "sz"   ,0 ,KANA_TSU      }, // 28
  { "h"    ,0 ,KANA_AIEUO    }, // 29 
  
  { "t"    ,6 ,KANA__TSU     }, // 30   KANA_PLUS
  { "y"    ,0 ,KANA_Y        }, // 31 
  { "w"    ,0 ,KANA__WA      }, // 32 
  { "k"    ,0 ,KANA__K       }, // 33
  { "aieuo",4 ,KANA_DONE     }, // 34   KANA_GK
  { "l"    ,0 ,KANA_PPC_L    }, // 35
  { "y"    ,0 ,KANA_GKNY     }, // 36
  { "w"    ,0 ,KANA_W        }, // 37

  { "u"    ,2 ,KANA_DONE     }, // 38   KANA__TSU
  { "zs"   ,1 ,KANA_TSU      }, // 39

  { "u"    ,1 ,KANA_DONE     }, // 40   KANA_TSU

  { "e="   ,2 ,KANA_DONE     }, // 41   KANA_Y
  { "auo"  ,1 ,KANA_DONE     }, // 42   KANA_GKNY
   
  { "a"    ,1 ,KANA_DONE     }, // 43   KAYA__WA

  { "aieo" ,2 ,KANA_DONE     }, // 44   KANA_W
  { "l"    ,0 ,KANA_PPC_L    }, // 45

  { "ea"   ,1 ,KANA_DONE     }, // 46   KANA__K

  { "^-.+" ,1 ,KANA_DONE     }, // 47   KANA_UP

  { "aieuo",2 ,KANA_DONE     }, // 48   KANA_AIEUO
  { "l"    ,0 ,KANA_PPC_L    }, // 49

  { ""     ,0 ,KANA_PPC_L    }, // 50   KANA_PPC_L      // This is a fake point to recieve the l 
                                                        //   Any further character will reset the 
                                                        //   input stream, but if the l is removed
                                                        //   and replaced with an i processing can 
                                                        //   proced as it should

  { ""     ,0 ,KANA_PENDING  }, // 51   KANA_PENDING    // Loop holding state.
};

#else (defined(WINCE_PPC) || defined(WINCE_POCKETPC))

#define KANA_F          15  // fa,fi,fu,fe,fo  f-
#define KANA_N          17  // na,ni,nu,ne,no  nyu,nyo,nya  n',n"
#define KANA_NORMAL_Y   18  // xa,xi,xu,xe,xo  xyu,xyo,xya
#define KANA_C          20  // chi,cha,chu,che,cho  ci
#define KANA_S          21  // sa,si,su,se,so  sha,shi,shu,she,sho
#define KANA_T          22  // ta,ti,tu,te,to  tsu,tzu
#define KANA_NORMAL     23  // xa,xi,xu,xe,xo
#define KANA_PLUS       26  // +a,+i,+u,+e,+o  +tu,+tsu,+tzu  +ya,+yu,+yo  +wa  +ka,+ke
#define KANA_GK         30  // ka,ki,ku,ke,ko  kya,kyu,kyo  kwa,kwi,kwe,kwo
#define KANA__TSU       33  // tu  tsu,tzu  (from +tu,+tsu,+tzu)
#define KANA_TSU        35  // tsu,tzu   (from +tsu,+tzu,tzu,tsu)
#define KANA_Y          36  // ya,yu,yo  ye,y=
#define KANA_GKNY       37  // xya,xyu,xyo   (from g, k, etc.)
#define KANA__WA        38  // +wa
#define KANA_W          39  // wa,wi,we,wo
#define KANA__K         40  // +ka +ke
#define KANA_UP         41  // ^^  ^.  ^-, ^+
#define KANA_AIEUO      42  // tha the tho thu thi
#define KANA_PENDING    43  // Place holder used to mark kana as waiting for next latter to resolve.

static kana_state kana_states[] = {
  { "'`"   ,15,KANA_DONE     }, // 00
  { "+"    ,0 ,KANA_PLUS     }, // 01   +a,+i,+u,+e,+o  +tu,+tsu,+tzu  +ya,+yu,+yo  +wa
  { "v"    ,0 ,KANA_NORMAL   }, // 02   va,vi,vu,ve,vo
  { "kg"   ,0 ,KANA_GK       }, // 03   ka,ki,ku,ke,ko  kya,kyu,kyo  kwa,kwi,kwe,kwo
  { "s"    ,0 ,KANA_S        }, // 04   sa,si,su,se,so  sha,shi,shu,she,sho  sya,syu,syo
  { "td"   ,0 ,KANA_T        }, // 05   ta,ti,tu,te,to  tsu,tzu  tya,tyu,tyo
  { "hbprm",0 ,KANA_NORMAL_Y }, // 06   ha,hi,hu,he,ho  hyu,hyo,hya
  { "jlz"  ,0 ,KANA_NORMAL_Y }, // 07
  { "y"    ,0 ,KANA_Y        }, // 08   ya,yu,yo  ye,y=
  { "w"    ,0 ,KANA_W        }, // 09   wa,wi,we,wo
  { "f"    ,0 ,KANA_F        }, // 10   fa,fi,fu,fe,fo  f-
  { "c"    ,0 ,KANA_C        }, // 11   chi,cha,chu,che,cho  ci
  { "n"    ,0 ,KANA_N        }, // 12   na,ni,nu,ne,no  nyu,nyo,nya  n'
  { "aieuo",0 ,KANA_PENDING  }, // 13
  { "^"    ,0 ,KANA_UP       }, // 14   ^^, ^., ^-

  { "aieuo",2 ,KANA_DONE     }, // 15   KANA_F 
  { "-"    ,0 ,KANA_DONE     }, // 16   

  { "'"    ,3 ,KANA_DONE     }, // 17   KANA_N
  { "aieuo",2 ,KANA_DONE     }, // 18   KANA_NORMAL_Y
  { "y"    ,0 ,KANA_GKNY     }, // 19

  { "i"    ,2 ,KANA_DONE     }, // 20   KANA_C
  { "h"    ,3 ,KANA_NORMAL   }, // 21   KANA_S
  { "y"    ,4 ,KANA_GKNY     }, // 22   KANA_T
  { "aiueo",1 ,KANA_DONE     }, // 23   KANA_NORMAL
  { "sz"   ,0 ,KANA_TSU      }, // 24
  { "h"    ,0 ,KANA_AIEUO    }, // 25
  
  { "t"    ,5 ,KANA__TSU     }, // 26   KANA_PLUS
  { "y"    ,0 ,KANA_Y        }, // 27 
  { "w"    ,0 ,KANA__WA      }, // 28 
  { "k"    ,0 ,KANA__K       }, // 29
  { "aieuo",3 ,KANA_DONE     }, // 20   KANA_GK
  { "y"    ,0 ,KANA_GKNY     }, // 31 
  { "w"    ,0 ,KANA_W        }, // 32

  { "u"    ,2 ,KANA_DONE     }, // 33   KANA__TSU
  { "zs"   ,1 ,KANA_TSU      }, // 34

  { "u"    ,1 ,KANA_DONE     }, // 35   KANA_TSU

  { "e="   ,2 ,KANA_DONE     }, // 36   KANA_Y
  { "auo"  ,1 ,KANA_DONE     }, // 37   KANA_GKNY
   
  { "a"    ,1 ,KANA_DONE     }, // 38   KAYA__WA

  { "aieo" ,1 ,KANA_DONE     }, // 39   KANA_W

  { "ea"   ,1 ,KANA_DONE     }, // 40   KANA__K

  { "^-.+" ,1 ,KANA_DONE     }, // 41   KANA_UP

  { "aieuo",1 ,KANA_DONE     }, // 42   KANA_AIEUO

  { ""     ,0 ,KANA_PENDING  }, // 43   KANA_PENDING    // Loop holding state.
};
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))

//--------------------------------
//
//  Clears the contents of the kana conversion system.  Any kana that
//  are pending are output at this point and the system is made ready
//  for new input.
//
void KANA_convert::clear () {
  if (pending) {                // Output pending kana.
    pending = false;
    out_kana ();
  }
  state       = KANA_BASE;      // Clear acumulation system.
  pending     = false;
  kanji_start = false;
  index       = 0;
  buffer[0]   = 0;
  jwp_stat.update (buffer);     // Clear status line display.
  return;
}

//--------------------------------
//
//  Main entry point.  Characters from the user enter through this 
//  routine.  They are accumulated and eventually kana is output.
//
//      f  -- Contect from which we were called, so where to put things
//            back to.
//      ch -- character form the user.
//
//  Note this routine is inherently messey, because there are a
//  number of special cases to deal with.
//
void KANA_convert::do_char (JWP_file *f,int ch) {
  char *ptr;
  int   c,i;
  static char *reserved = "'`^+lzmjvkgstdnhbprywfcaieuo";   // These are caracters eaten by the kana converter.
//
//
//
  if (file != f) clear ();
  file = f;
//
//  First check for double concident.  If so output a small tsu of the corect type.
//
  c = tolower(ch);
  if ((index == 1) && !pending && (((buffer[0] == ch) && isalpha(ch)) || ((buffer[0] == 't') && (ch == 'c')))) {
    if (c == ch) strcpy (buffer,"+tu"); else strcpy (buffer,"+TU");
    pending = true;                         // Setting pending will force clear() to otput the charcter
    clear ();
  }
//
//  Here is a check to pickup mp and mb, which are equivalent to np and nb.
//
  if ((index == 1) && (buffer[0] == 'm') && ((c == 'b') || (c == 'p'))) {
    pending = true;
    strcpy (buffer,"n");
    clear  ();
  }
//
//  Figure out if we want this charcter.  If we do not process the
//  character as a jascii character.
//
  if ((state == KANA_N) && (c == '"')) ch = c = '\'';
  for (ptr = reserved; *ptr && (c != *ptr); ptr++);                                     // accept reserved chacters.
  if ((state == KANA_UP) && ((c == '.') || (c == '-') || (c == '+'))) ptr = reserved;   // accept ^ kana extensions (^^,^.,^-).
  if (((state == KANA_Y) && (c == '=')) || ((state == KANA_F) && (c == '-'))) {         // accept y= and f- but block kanji generation
    ptr       = reserved;
    buffer[0] = tolower(buffer[0]);
  }
  if (!*ptr) {
    clear ();
    if      (ch == 'x') file->put_char (0x215f,CHAR_STOP);
    else if ((c = ascii_to_jascii(ch))) file->put_char (c,CHAR_STOP);
    return;
  }
//
//  We should only have valid characters at this point, so what we do 
//  is go around an infinite loop.  (Note the loop should only be 
//  executed at most twice or we are doomed to an infinite loop.)
//
//  In this loop, we first attempt to place the character in the 
//  current state.  If so we jump out to the found location.  If not, 
//  then we dump the current state via clear() and insert the character
//  into a new state.  (Note, wehen the state is dumped, any pending 
//  kana will be output [n, aieuo].)
//
  while (true) {
    for (i = 0; i < kana_states[state].length; i++) {
      for (ptr = kana_states[i+state].valid; *ptr && (c != *ptr); ptr++);
      if (*ptr) goto CharFound;                         // Found char in state so exit loop.
    }
    if (pending && isupper(buffer[0]) && (c == ch) && (buffer[0] != 'N')) {   
      kanji_start = true;                               // Did not find char so we are going to dump 
      buffer[0]   = tolower(buffer[0]);                 //   the state.  First must resolve pended 
    }                                                   //   character, if it is uppercase then is it 
    clear ();                                           //   katakana or hiragana (start kanji convert).  
  }                                                     //   This is resolved by the case of the new character.
//
//  We found our character a state that it belongs in.  Insert it into
//  the state.
//
CharFound:
  buffer[index++] = ch;                                 // Update buffer.
  buffer[index  ] = 0;
  state           = kana_states[i+state].next;          // Update state.
  if (state == KANA_PENDING) {                          // If aieou (KANA_PENDING) if just lower ouptut,
    if (ch == c) state = KANA_DONE; else pending = true;//   otherwise mark state as pending and resolve
  }                                                     //   with next character.
  if (state == KANA_N) pending = true;                  // n results in pending state.
  if (index > 1) pending = false;                       // If past first character cannot be pending (na,no,etc).
  if (state != KANA_DONE) jwp_stat.update (buffer); else out_kana ();   // Either update display or output kana.
  return;  
}

//--------------------------------
//
//  This small module clears the conversion buffer without outputing
//  pending characters.  This is used to respond to delete & back-space
//  keys, and to initialize the system.
//
//      RETURN -- Non-zero indicates there was stuff in the convert.
//
int KANA_convert::erase () {
//
//  Special handling needs to be done in the case of PPC's.  When the 
//  user renders a i character using a line ana dot, the sequence of 
//  actions we will receive from the system is as follows:
//
//      'l' character
//      <back-space>
//      'i' character
//
//  This presents a serious problem for JWPce, since if you try to enter
//  ki, the sequence is kl<BS>i.  The l causes the k to be removed, and 
//  the BS removes the l, resulting in just an i.  
//
//  This mess is fixed as follows: 
//
//  First the state table is modified to add the l's following any 
//  character that can be followed by an i.  The l's all point to a 
//  state KANA_PPC_L.  This state cannot be resolved, so the next character
//  will destroy all of these caracters.
//
//  Second the KANA_convert::erase() rutine, has been modified.  If the
//  current state is KANA_PPC_L, then the erase will not simply 
//  erase, but will remove only the last character.
//
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  int i,j,k;
  char temp[6];
  i       = state;      // Save state to generate return.
  pending = false;      // This will suppress output of characters.
  if (i != KANA_PPC_L) clear ();    // Not in special state so clear
    else {                          // Special state!
      j = index;                                        // Save char count
      for (k = 0; k < 6; k++) temp[k] = buffer[k];      // Save characters
      clear ();                                         // Clear
      for (k = 0; k < j-1; k++) do_char (file,temp[k]); // Put back some characters
    }
#else (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  int i;
  i       = state;      // Save state to generate return.
  pending = false;      // This will suppress output of characters.
  clear  ();            // clear system. (does not mater the context since no output)
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  return (i);           // Generate return, used for bs/del to determinine if they are done.
}

//--------------------------------
//
//  Forces a kana->kanji conversion based on a hiragana character.
//  Generally this routine is only called via an explicit conversion
//  (F2).  This routine froces the first chaacter to lower case and 
//  specifies that the character should start a kana to kanji conversion.
//  This allows A then F2 to do a conversion.
//
void KANA_convert::force () {
  if (pending) {                        // No chaacters so do nothing.
    do_char (file,'n');
    erase   ();
  }
  return;
}

//--------------------------------
//
//  This routine converst the kana entered as an ascii string by the
//  user into a number of kanji codes, that reperesent the kana (and 
//  a few other charcters).
//
//      file -- Context from which we were called so we know where to 
//              put back our characters
//
void KANA_convert::out_kana () {
  int         i,base;               // base is used to indicate hirgana or katakana output.
  char        *ptr;
// 
//  Setup for conversion from buffer contents to kana.
//
  base = BASE_KATAKANA;                                     // base indicates hiragana or katakana
  i = isupper(buffer[0]);                                   // i indicates may be start kana->kanji convert if not KATAKANA
                                        // I changed the test below to !islower.  This is basically to prevent 
                                        //   N' from being converted to HIRAGANA and a knaji start.  This is because the 
                                        //   ' is not lower or upper.  The previous test isupper() will fail or '.
  for (ptr = buffer; *ptr; ptr++) {                     
    if (('+' == *ptr) || !islower(*ptr)) *ptr = tolower(*ptr); else base = BASE_HIRAGANA;
  }
  if (i && (base == BASE_HIRAGANA)) kanji_start = true;     // Start of kana->kanji conversion.
//
//  Direct conversion takes out the kana in the order that they appear
//  in the JIS codes.  This takes out most of the one character kana 
//  with their most common name directly.
//
  if ((i = find_direct(buffer))) { 
    put_kana (base+i); 
    clear ();
    return; 
  }
//
//  Compound kana takes out most of the kana's alternate names, and most
//  of the odd kana conversions.  Note that regular compounds such as nya,
//  byo, etc are processed later.
//
//  Table of compound kana that are not of the simple form nya,byo, etc.
//  Simple form kana are made directly using a different section of code.
//  These kana are simply scanned against the table.  The table results 
//  in two kana codes that are combined with the base value to form 
//  two characters.  Placing a zero in the second value suppresses 
//  generation of the second character.
//
  for (i = 0; compound_kana[i].kana[0]; i++) {
    if (!strcmp(compound_kana[i].string,buffer)) {
      put_kana (base+compound_kana[i].kana[0]);
      if (compound_kana[i].kana[1]) put_kana (base+compound_kana[i].kana[1]);
      clear ();
      return;
    }
  }
//
//  Complex kana are special items.  In the case of these items, the 
//  katakana/hiragana switching is suppressed and the result is a fixed
//  character inserted into the file.
//
//  The table contains direct strings which are matched agains the 
//  user entered pattern.  The rsult is up to two direct kanji codes.
//  Entering a value of zero for the second code will suppress it.  
//  This allows entering of a single kanji.
//
  #define SIZE_COMPLEX    ((int) (sizeof(complex_kana)/sizeof(struct complex_kana)))
  static struct complex_kana {
    char  string[4];
    KANJI kana  [2];
  } complex_kana[] = {
    { "va" , { 0x2574,0x2521 } },
    { "vi" , { 0x2574,0x2523 } },
    { "vu" , { 0x2574,0x0    } },
    { "ve" , { 0x2574,0x2527 } },
    { "vo" , { 0x2574,0x2529 } },
    { "+ka", { 0x2575,0x0    } },
    { "+ke", { 0x2576,0x0    } },
    { "`"  , { 0x2156,0x0    } },   // These two are necessary because they 
    { "'"  , { 0x2157,0x0    } },   // translate to non-ASCII equivalents   
    { "y=" , { 0x216f,0x0    } },   // These are special additions (yen)
    { "f-" , { 0x2172,0x0    } },   //   (lira)
    { "^^" , { 0x2130,0x0    } },   //   (^)
    { "^." , { 0x2126,0x0    } },   //   (center .)
    { "^-" , { 0x2144,0x0    } },   //   (...)
    { "^+" , { 0x215c,0x0    } },   //   (+)
  };
  for (i = 0; i < SIZE_COMPLEX; i++) {
    if (!strcmp(complex_kana[i].string,buffer)) {
      put_kana (complex_kana[i].kana[0]);
      if (complex_kana[i].kana[1]) put_kana (complex_kana[i].kana[1]);
      clear ();
      return;
    }
  }
//
//  If we have gotten here the patter from the user has not been matched,
//  but if there are no errors in the code, then the only type of 
//  kana left are the basic compound kanas (nya, byo, ryu, etc.).  These
//  are generated by calling find direct twice.
//
  static char temp[4] = "xi";           // temp is used to search for first part of compount kana like kya,gyo, etc.
  temp[0]   = buffer[0];                // first kara is first char (n)+(i).
  buffer[0] = '+';                      // second kana is a small verion.
  i         = find_direct(buffer);      // had to move this here because put kana 
  put_kana (base+find_direct(temp));    // can clear the ascii->kana convert (to get h, aeiou, etc.).
  put_kana (base+i);
  clear    ();
  return;
}

//--------------------------------
//
//  Service routine that outputs kana from the ASCII->KANA conversion
//  routines.  This routine directs the output to the correct location,
//  and if necessary sends the information to the KANJI conversion 
//  routines.
//
//      file -- location of where to put the character (context from which we came from).
//      ch   -- KANJI code to output.
//
void KANA_convert::put_kana (int ch) {
  file->put_char (ch,kanji_start ? CHAR_START : CHAR_CONT);
  kanji_start = false;          // Prevents sequences generating copound kana from starting on each kana output.
  return;
}

//
//  End Class KANA_convert.
//
//===================================================================

//===================================================================
//
//  Begin Class JWP_file.
//
//  Parts of the JWP_file class related directly to the input of characters.
//

//--------------------------------
//
//  Main enrty point to this module.  This point is called whenever 
//  the user presses a key that gets processed as an input.
//
//      ch -- ASCII key code.
//
void JWP_file::do_char (int ch) {
  if (ch == '\t') { put_char (ch,CHAR_STOP); return; }  // TAB's are processed directly.
  if (ch > 0xff) {                                      // Characters outside the standard
    ch = unicode2jis(ch,KANJI_BAD);                     //   0-255 are treated as direct UNICODE
    put_char (ch,CHAR_STOP);                            //   input and processed as such.
    return;
  }
  if (ch < ' ') return;
  if (jwp_config.mode == MODE_KANJI) {                  // KANJI mode -> to converter
    kana_convert.do_char (this,ch);
    return;
  }
  if (jwp_config.mode == MODE_JASCII) {                 // JASCII mode -> convert ch to JASCII.
    if (!(ch = ascii_to_jascii(ch))) return;
  }
  put_char (ch,CHAR_STOP);
  return;
}

//--------------------------------
//
//  This is a slave routine designed so that do_char and the kana 
//  converter can call this routine.
//
//      ch   -- KANJI code to output.
//      code -- Contains code indicating what to do about kanji 
//              conversions:
//
//          CHAR_STOP  -- Force stop of kanji conversion.
//          CHAR_START -- Start a knaji conversion.
//          CHAR_CONT  -- Continue kanji conversion if in process.
//
void JWP_file::put_char (int ch,int code) {
  KANJI temp;
  int   pos;
  clear_cursor ();
  if (sel.type == SELECT_EDIT) {                                    // If there is edit mode selected text this will be 
    undo_start       ();                                            //   replaced, so delete the selected text.
    selection_delete ();
  }
  if ((sel.type == SELECT_KANJI) && (code != CHAR_CONT)) convert (CONVERT_FORCE); // Force kana->kanji conversion.
  if (code == CHAR_START) {                                         // Starting a kana->kanji conversion.
    if (cursor.pos > cursor.line->length) cursor.pos = cursor.line->length;
    find_pos (&cursor);
    sel.pos1 = cursor;
    sel.type = SELECT_KANJI;
  }
  undo_para (UNDO_TYPE);                                            // Actually put the character into the file.
  pos  = cursor.abs_pos();                                          // Save the cursor location so we can put the character in the  
  if (jwp_config.insert || cursor.eol()) {                          //   correct place.  We want the line info for better redraw.
    temp = ch;                                                      // Convert actual currsor to abs coordinates.  Also change the 
    cursor.para->ins_string (this,cursor.line,cursor.pos,&temp,1);  //   line to the first of para -- Make this a real abs location.
  }
  else {
    cursor.para->rep_char (this,cursor.line,cursor.pos,ch);
  }
  cursor.pos  = pos;
  cursor.line = cursor.para->first;
  cursor.advance (1);                           // Cannot use do_key because of word wrap.
  if (sel.type == SELECT_KANJI) {               // If this is a kana->kanji conversion,
    find_pos (&cursor);                         //   attempt to do the conversion.  Remember
    sel.pos2 = cursor;                          //   this could be any place in the cycle.
    convert   (CONVERT_ATTEMPT);
    edit_menu ();
  }
  undo_end   ();                                // End undo
  view_check ();                                // Check cursor visability.
  return;
}

//
//  End Class JWP_file.
//
//===================================================================







