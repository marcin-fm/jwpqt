//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//  These routines are intended to interface with EDICT, which is    //
//  a Japanese/English Dictionary developed and copyrighted by       //
//  James William Breen.                                             //
//                                                                   //   
//===================================================================//

//===================================================================
//
//  This module implements the dictionary search and mantinance routines.
//  These handle all interactions with the dictionaries and with searching.
//
//  The main dicitonary procedure involves searching Jim Breen's Japanese-
//  English Dictionary based on the index file used by JWP.  
//
//  Understanding the index file:
//
//  The dictionary is index by a string based on a EUC character 
//  combination.  The index is based on ascii characters of three 
//  or more chracters, kana strings of 2 or more kana, and single 
//  kanji.  All of the indexs are mixxed togetther and sorted in
//  numberical order, allowing use of a binary search.  Each index
//  points to a match string location in the dictionary file.  Note 
//  that these matches may actually be in the middle of a dictionary
//  entry.
//
//  The index has some odd characteristics.  Kanji and ascii are 
//  indexed wherever they occure in the text, but kara strings are only
//  indexed when they begin a word!
//
//  A search is conducted by performing a binary search on the index.  
//  For each index value we get the line from the actual dictionary and
//  compare with our key.  When we find the first match that matches 
//  our key, we have the beginning of our actuall data.  We then containue
//  getting the next index, and then the next data from the dicitionary,
//  until we get to the point when the lines gotten do not match the key.
//
//  Understanding dicitioanry entries:
//
//  Each entry inthe dictionary is a EUC line of text, terminated by a
//  cr-lf pair. (I do not count on this actually being a cr-lf, and can 
//  handle any comination [cr, lf, lf-cr, cr-lf].).  The format of each
//  line depends on weather or not there is kanji for the word in 
//  in quetion:
//
//      <kanji> [<kana>] /<defintion>/<definition>/ ...
//
//              or if there is no kanji:
//
//      <kana> /<definition>/<definition>
//
//  The index seach will give us the location of what we are searching 
//  for within a line of text (definititely not necessarily at the 
//  actual beginning of the line).  To handle this problem i use a 
//  centered buffer approche (see below).  Generally, we backup to the 
//  beginning of the line.  
//
//  Each of the <definition>s contains a complete definition of the 
//  word.  Since words are offten used to mean different things, these
//  defintions can vary a lot.  Within each definition, however, there
//  may be key string.  This string:
//
//          (<key>,<key>,...)
//
//  Tells us soemthing about the type of defintiion.  For example, the 
//  string (pl,pn,giv,fem) tells us that this entry is a place name, and
//  a female given name.  
//
//  I allow selective searching based on the type of entry.  The main 
//  desire of this is to allow filtering of personal names, and place 
//  names.  For example searching on the kanji NI (two) you get 738 
//  entries.  Removing place names and personal names, reduces this to
//  277 entries.  
//
//  Filtering keys correctly took a lot of work.  See the routine 
//  JWP_dict::search_dict() for a detailed descritption of the 
//  filtering process.
//
//  The dictionary flags:
//
//  The dictionary search rotoutines use flags that are stored in 
//  the global configuration.  These flags can be preserved over a 
//  instances of JWPce:
//
//      dict_auto     -- Causes automatic searching of the dictionary.
//      dict_compress -- Causes results display to be use a compressed 
//                       format.  This allows more lines to be visible,
//                       but makes them harder to read.
//
//  The rest of the flags are stored as bit field in the dict_bits
//  area.  These bits correspond to the rejected fields of the dict_keys
//  and are unrolled into those fields when the dictionary is open and 
//  put back when the dictionay dialog is closed.
//
//  The dominate keys are:
//
//      DICTKEY_END    -- Results must match the end of a word.
//      DICTKEY_BEGIN  -- Reuslts must match the beginning of a word.
//      DICTKEY_NAMES  -- Rejects all personal names.
//      DICTKEY_PLACES -- Rejects all place names.
//
//  User dictionary:
//
//  The user dictionary is kept in the same format as EDICT.  When the 
//  dictionary is loaded a '\n' character is placed in front of the 
//  actual dictionary.  This allows the search backward in the file to
//  correctly process the first entry in the file.  This is handled in 
//  in the main dictionaries by filling the dictionary buffer with '\n'
//  characters before loading short dictionary entries.
//
//  The dictionaries file:
//
//  The dicitonaries to be searched are stored in a file called "dict.cfg".
//  This file is actually an almost text file with a binary marker.  
//
//  The format of the file is as follows:
//
//      magic       -- Coded long int to make sure we have the correct file.
//      
//      flags<0>    -- Zero terminated string containning the flags for this dictionary.
//      name<0>     -- Zero terminated string containning the name for this dictionary.
//      file<0>     -- Zero termianted string containning the filename for this dictionary.
//
//      Repeat the dictionary pattern for each dictionary.
//
//      0000        -- Four bytes of zeros to indicate end of the file.
//
//  The flags are actually ascii characters, but the defintions for these are
//  all that is used.
//
//      DICTFLAG_EUC        'E'         // Format EUC
//      DICTFLAG_UTF8       'U'         // Format UTF8
//      DICTFLAG_MIXED      'M'         // Format Mixed (discuraged format)
//      DICTFLAG_INDEX      'I'         // Has index file
//      DICTFLAG_NAMES      'N'         // Has names
//      DICTFLAG_NAMESONLY  'O'         // Has only names
//      DICTFLAG_CLASSICAL  'c'         // Is a classical dictionary    
//      DICTFLAG_USER       'u'         // Is the user dictionary.
//      DICTFLAG_BUFFER     'B'         // Uses a buffered search
//      DICTFLAG_SEARCH     'S'         // Is searched
//      DICTFLAG_QUIET      'Q'         // Quiet handling of errors
//      DICTFLAG_KEEP       'K'         // Keep dictionary in memory or open.
//
#include "jwpce.h"
#include "jwp_clip.h"
#include "jwp_conf.h"
#include "jwp_conv.h"
#include "jwp_dict.h"
#include "jwp_edit.h"
#include "jwp_file.h"
#include "jwp_find.h"
#include "jwp_jisc.h"
#include "jwp_help.h"
#include "jwp_inpt.h"
#include "jwp_misc.h"
#include <commctrl.h>

#define COLUMN_NAME     0
#define COLUMN_FORMAT   1
#define COLUMN_TYPE     2
#define COLUMN_NAMES    3
#define COLUMN_SPECIAL  4
#define COLUMN_KEEP     5
#define COLUMN_QUIET    6
#define COLUMN_FILE     7

//#define DICT_DEBUG    // Used for debugging the NT dictionary problem -- REMOVE WHEN DONE!

//===================================================================
//
//  Compile time options.
//
#define DROP_AUTOADD        // Definining this value changes the way dictioanries 
                            //   that are droped on the dictionaries dialog are 
                            //   processed.  If this value is defined, the dictionaries
                            //   are automatically added to the list, without opening
                            //   the edit dictionary dialog.  If this is not defined 
                            //   the edit dictionary dialog is open for each file 
                            //   dropped.  Note, when an error occures, the edit
                            //   dictionary dialog will still be open.

#define FAST_PRIMARY        // Defining this values cuases the fast primary entry search to 
                            //   be used.  I wrote two checks for determining if this is a primary 
                            //   entry.  The FAST_PRIMARY check requries that the primary flag be 
                            //   the last thing on the line.  This basically looks for the \n, and 
                            //   then looks backwards for the (P).  The actuall pattern used in 
                            //   EDICT 15JUN01 V01-001 is "/(P)/\n".  The other routine looks for
                            //   the (P) anywhere in the meaning part of the entry.  This is slower.  
                            //   For the moment we can use the fast search.

//===================================================================
//
//  Macros and defintions
//
#define NAME_MAINDICT       TEXT("edict")       // Name of dictionary file.
#define NAME_NAMEDICT       TEXT("enamdict")    // Name of the name dictionary.
#define NAME_USERDICT       TEXT("user.dct")    // Name of user dictionary file.
#define NAME_CLASSICAL      TEXT("classical")   // Name of classical dictionary.
#define NAME_DICTIONARIES   TEXT("jwpce.dic")   // Name of dictionaries file.

#define SIZE_LINE       512                     // Offest into the buffer for the search point.
#define SIZE_DICTBUFFER 1024                    // Size of the buffer read from the dicitonary.

#define HIRAGANA_A      0x2422      // Hiragana values used in the advanced search.
#define HIRAGANA_I      0x2424      //   These values are simply taken from the 
#define HIRAGANA_U      0x2426      //   JIS table and are included here because 
#define HIRAGANA_KU     0x242f      //   it is easy to do so.
#define HIRAGANA_GU     0x2430
#define HIRAGANA__TU    0x2443
#define HIRAGANA_TSU    0x2444
#define HIRAGANA_NU     0x244c
#define HIRAGANA_FU     0x2455
#define HIRAGANA_BU     0x2456
#define HIRAGANA_MU     0x2460
#define HIRAGANA_RU     0x246b
#define HIRAGANA_WA     0x246f
#define HIRAGANA_N      0x2473

#define EUC_CAMA    (KANJI_CAMA  | 0x8080)          // EUC match characters.  
#define EUC_SLASH   (KANJI_SLASH | 0x8080)          
#define UTF_CAMA    (0xefbc8f)                      // UTF match charactes.  
#define UTF_SLASH   (0xe38081)

#define EUC_MATCH(p,x)   (((p)[0] == (x>>8)) & ((p)[1] == (x&0xff)))                         // Odd methode necessary for MIPS processor/compiler error
#define UTF_MATCH(p,x)   (((p)[0] == (x>>16)) & ((p)[1] == ((x>>8)&0xff)) & ((p)[2] == (x&0xff)))
#define IS_BEGIN(c)      (((c) == '[') || ((c) == ' ') || ((c) == '/') || ISCRLF(c))         // Valid being of entry conditions
#define IS_END_N(c)      (((c) == ']') || ((c) == ' ') || ((c) == '/'))                      // Normal end of entry conditions
#define IS_END(p)        (IS_END_N(*(p)) || (classical_part && (((p)[0] == ')') || EUC_MATCH(p,EUC_CAMA) || EUC_MATCH(p,EUC_SLASH))))   // Test for valid end character
#define IS_UTFEND(p)     (IS_END_N(*(p)) || (classical_part && (((p)[0] == ')') || UTF_MATCH(p,UTF_CAMA) || UTF_MATCH(p,UTF_SLASH))))   // Test for valid end with classical
#define IS_ASCIIEND(p)   ((p)[0] == '/')
#define IS_ASCIIBEGIN(p) (((p)[0] == '/') || (((p)[0] == ' ') && ((p)[-1] == ')')))

#define ISPATTERN(x)    (((x) == '*') || ((x) == '?') || ((x) == '[') || ((x) == ']'))

//===================================================================
//
//  static data and definitions.
//
//  Most of these have to do with the dict_key structure, and a the dict_keys
//  variable.  These simply keep track of conditions for filtering a dictionary
//  entry.  The dict_keys array has two false entries in the front.  These 
//  entries are for use in the Dictionary Options dialog.
//

#if 0 // ### I really wanted to make the JWP_dict class allocated on the fly, 
      //       when I tried to do this, however, I found a bug in MSVC++, and
      //       could not get around it by disabling the optimiziation, so I have
      //       gone back to the static allocation.  This worked in Debug mode,
      //       but not in Release mode.
      
static JWP_dict *jwp_dict = NULL;   // Static instance of the dictionary
                                    //   class.  This is used to hold the
                                    //   current instance so dialog box 
                                    //   procedure can access it.  Additonally,
                                    //   if this value is non-NULL, you are 
                                    //   alread in the dictionary.
#endif

//--------------------------------
//
//  These variables were moved here from the class because the class item
//  is now allocated on the fly, thus to allow these variables to be 
//  used over more than one instance.  Using this method, the these 
//  files can be loaded the first time the dictionary is used and remain
//  in memory until the system is shutdown.
//
static class Dictionary *dicts;     // Pointer to list of dictionaries.

static SIZE_window user_size;       // Class for dynamic sizing of the window.
static SIZE_window dict_size;       // Class for dynamic sizing of the dictionary dialog.

#define NUMBER_DICTKEYS ((int) (sizeof(dict_keys)/sizeof(struct dict_key)))

struct dict_key {       // Structure for keeping track of filtering conditions.
  char   key[5];        // Key as is containned in a directory entry.
  byte   reject;        // Non-zero if this key is to be filtered out.
  short  text;          // Text description of the key for the options dialog.
};

//--------------------------------
//
//  These are various search elemetns that you can optionally search on.
//  This a keys specified in the dictionary.  Commeted out thins are 
//  believed (by me) to not be worth it.
//  
//
static struct dict_key dict_keys[] = {
  { "/({[",0,IDS_DO_BEGIN                                                      },
  { "/({[",0,IDS_DO_END                                                        },
  { "s"   ,0,IDS_DO_PERSONALNAMES                                              },
  { "p"   ,0,IDS_DO_PLACENAMES                                                 }, 
  { "vulg",0,IDS_DO_VULGAR                                                     },
  { "X"   ,0,IDS_DO_X                                                          },
  { "col" ,0,IDS_DO_COLLOQUIAL                                                 },
  { "m-sl",0,IDS_DO_MANGA                                                      },
  { "sl"  ,0,IDS_DO_SLANG                                                      },
  { "MA"  ,0,IDS_DO_MARIAL                                                     },
  { "id"  ,0,IDS_DO_IDIOMATIC                                                  },
  { "arch",0,IDS_DO_ARCHAIC                                                    },
  { "obs" ,0,IDS_DO_OBSOLETE                                                   },
  { "obsc",0,IDS_DO_OBSCURE                                                    },
  { "ok"  ,0,IDS_DO_KANA                                                       },
  { "abbr",0,IDS_DO_ABBREVIATION                                               },
  { "fam" ,0,IDS_DO_FAMILIAR                                                   },
  { "pol" ,0,IDS_DO_POLITE                                                     },
  { "hum" ,0,IDS_DO_HUMBLE                                                     },
  { "hon" ,0,IDS_DO_HONORIFIC                                                  },
//{ "an"  ,0,TEXT("adjectival nouns or quasi-adjectives (keiyoudoshi)")        },
//{ "a-no",0,TEXT("nouns which may take the genitive case particle no")        },
//{ "vs"  ,0,TEXT("nouns or participles which take the auxillary verb suru")   },
//{ "vt"  ,0,TEXT("transitive verbs")                                          },
//{ "vi"  ,0,TEXT("intransitive verbs")                                        },
//{ "I"   ,0,TEXT("Type I (godan) verbs (only when type is not implicit)")     },
//{ "IV"  ,0,TEXT("Type IV (irregular) verbs, such as gosaru")                 }, 
  { "fem" ,0,IDS_DO_FEMALE                                                     },
  { "male",0,IDS_DO_MALE                                                       },
  { "pref",0,IDS_DO_PREFIX                                                     },
  { "suf" ,0,IDS_DO_SUFFIX                                                     },
//{ "uk"  ,0,TEXT("words usually written in kana alone")                       },
//{ "uK"  ,0,TEXT("words usually written in kanji alone")                      },
  { "oK"  ,0,IDS_DO_KANJI                                                      },
//{ "ik"  ,0,TEXT("words containing irregular kana usage")                     },
//{ "iK"  ,0,TEXT("words containing irregular kanji usage")                    },
//{ "io"  ,0,TEXT("words words containing irregular okurigana usage")          },
};

//===================================================================
//
//  Exported data (also JWP_dict).
//

JWP_history dict_history (IDS_HIST_DICTIONARY);         // History buffer for the dictionary.

//===================================================================
//
//  static routines.
//
static int    dictionary_compare (KANJI *buf1,KANJI *buf2);                         // Comparison routine for sorting dictionary entries.
static int    euc_comp           (byte *ptr,KANJI *key,int length);                 // Compare a key against a dictionary entry.
static int    euc_post           (byte *ptr,KANJI *key,int count);                  // Post-search patter match routine.
static int    euc_pre            (byte *ptr,KANJI *key,int count);                  // Pre-search patter match routine.            
static byte  *format_line        (EUC_buffer *line,byte *data,int user,int format); // Format a dictionary line for display.
static TCHAR *index_name         (TCHAR *name,TCHAR *buffer);                       // Get index file name from dictionary file name.
static byte  *load_dict          (tchar *name);                                     // Make memory image of a dictionary file.
static int    mix_comp           (byte *ptr,KANJI *key,int length);                 // Compare a key against a dictionary entry.
static KANJI *setup_compare      (KANJI *buffer,KANJI *&end);                       // Sets up a dictionary comparison.
static int    test_key           (byte *ptr,char *key);                             // Test a string against being a dictionary type key.
static int    utf_back           (byte *ptr,int index);                             // Move to the beginning of a UTF-8 character.
static int    utf_comp           (byte *ptr,KANJI *key,int length,int &utf_length); // Compare a key against a dictionary entry.
static int    utf_post           (byte *ptr,KANJI *key,int count);                  // Post-search patter match routine.
static int    utf_pre            (byte *ptr,KANJI *key,int count);                  // Pre-search patter match routine.            
static int    utf_size           (int ch);                                          // Determine number of bytes in a UTF8 character

//--------------------------------
//
//  Dialog procedure for the dictionary.  This is bascially a stub that 
//  calls JWP_dict::dlg_dictionary.
//
static BOOL CALLBACK dialog_dictionary (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_dict.dlg_dictionary(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  Dialog procedure for the dictionary options.  This is bascially a stub that 
//  calls JWP_dict::dlg_dictoptions.
//
static BOOL CALLBACK dialog_dictoptions (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_dict.dlg_dictoptions(hwnd,message,LOWORD(wParam)));
}

//--------------------------------
//
//  Compair two dictionary entries.  This is used for sotring dictionary entries.
//
//      buf1,buf2 -- Pointers to dictionary entries (null terminated)
//
//      return    -- Non-zero indicates buf2 should come before buf1.
//  
static int dictionary_compare (KANJI *buf1,KANJI *buf2) {
  KANJI *p1,*p2,*l1,*l2;
//
//  Basic compare.  This pickup up case of a real difference in the string, or a difference in
//  the length of the strings.
//
  p1 = setup_compare(buf1,l1);
  p2 = setup_compare(buf2,l2);
  for ( ; (p1 != l1) && (p2 != l2) && ((*p1 & 0x00ff) == (*p2 & 0x00ff)); p1++, p2++);
  if ((p1 != l1) && (p2 != l2)) return ((*p2 & 0x00ff) < (*p1 & 0x00ff));           // Real difference in strings.
  if ((p1 == l1) && (p2 != l2)) return (false);                                     // buf1 shorter than buf2, but identical
  if ((p1 != l1) && (p2 == l2)) return (true);                                      // buf2 shorter than buf1, but identical
//
//  Same string, so do compare for kana type differences.
//
  p1 = setup_compare(buf1,l1);
  p2 = setup_compare(buf2,l2);
  for ( ; (p1 != l1) && (p2 != l2) && (*p1 == *p2); p1++, p2++);
  if ((p1 != l1) || (p2 != l2)) return (*p2 < *p1);
//
//  Down to same length, and same kana.  Do a compare on the whole string, and end this.
//
  for ( ; *buf1 && *buf2 && (*buf1 == *buf2); buf1++, buf2++);
  return (*buf2 < *buf1);
}

//--------------------------------
//
//  Dictionary comparison routine for EUC dictionaries (also used for mixed dictionaries).
//
//      key    -- JIS string to be compared with.
//      ptr    -- Pointer to location in dictionary to be compared.
//      length -- Length of the key.
//
//      RETURN -- Zero indicates the key and data match.  A non-zero value
//                indicates they don't match.  Positive indicates key is 
//                later than the data.  Netgitive indicates oposite.
//
static int euc_comp (byte *ptr,KANJI *key,int length) {
  int   i,j;
  KANJI c;
  for (j = i = 0; i < length; i++, j++) {
    if (0x80 & ptr[j]) {
      c = ptr[j];
      if (c == 0xa5) c = 0xa4;
      c = 0x7f7f & ((c << 8) | ptr[++j]);
    }
    else {
      c = tolower(ptr[j]);
    }
    if (c != key[i]) return (key[i]-c);
  }
  return (0);
}

//--------------------------------
//
//  Pattern testing routine.  This routine is a recusively called patter to match post-search sequences 
//  of characters.  The rouine is designed to attempt to match a single character in the pattern, and
//  call itself, until no characters are left to call.  The * character can generate a large number of
//  sub calls, and will search for the longest matching string.
//
//      ptr    -- Pointer to dictionary buffer at the first character beyond the search match (ie what we are trying to match).
//      key    -- Remmainning key to match.
//      count  -- Number of characters in the key to match.
//
//      RETURN -- -1 indicates a match failure.  Any other value indicates the number of characters
//                used in the match.  Remember to include sub characters from further calls.
//
static int euc_post (byte *ptr,KANJI *key,int count) {
  int c,i,len;
  if (!count) return (0);
  switch (*key) {
    case '[':                                   // This is not possible!
         return (-1);
    case ']':                                   // Match end
         if (!IS_END_N(*ptr)) return (-1);
         len = 0;
         break;
    case '*':                                   // Matches any number of characters.
         for (len = 0; !IS_END_N(ptr[len]); len++);
         while (len >= 0) {
           i = euc_post(ptr+len,key+1,count-1);
           if (i != -1) return (i+len);
           if (ptr[--len] & 0x80) len--;
         }
         return (-1);
    case '?':                                   // Matches any single character
         if (IS_END_N(*ptr)) return (-1);
         if (*ptr & 0x80) len = 2; else len = 1;
         break;
    default:                                    // Standard character
         c = *ptr;
         if (c & 0x80) {
           c   = 0x7f7f & ((c << 8) | (ptr[1]));
           len = 2;
         }
         else {
           c   = tolower(c);
           len = 1;
         }
         if (c != *key) return (-1);
         break;
  }
  i = euc_post(ptr+len,key+1,count-1);          // Common point for most patterns other than *.
  if (i == -1) return (-1);
  return (i+len);
}

//--------------------------------
//
//  Patter testing routine.  This routine is a recusively called patter to match pre-search sequences 
//  of characters.  The rouine is designed to attempt to match a single character in the pattern, and
//  call itself, until no characters are left to call.  The * character can generate a large number of
//  sub calls, and will search for the longest matching string.
//
//  Note, this rotuien removes the key elements from the end, not the beginning, so you pass the same 
//  key to each subcall and just change the count.
//
//      ptr    -- Pointer to dictionary buffer at the first character beyond the search match (ie what we are trying to match).
//      key    -- Remmainning key to match.
//      count  -- Number of characters in the key to match.
//
//      RETURN -- -1 indicates a match failure.  Any other value indicates the number of characters
//                used in the match.  Remember to include sub characters from further calls.
//
static int euc_pre (byte *ptr,KANJI *key,int count) {
  int c,i,len;
  if (!count) return (0);
  switch (key[--count]) {                       // Matches any number of characters.
    case ']':                                   // This is not possible!
         return (-1);
    case '[':                                   // Match beginning
         if (!IS_BEGIN(*ptr)) return (-1);
         len = 0;
         break;
    case '*':
         for (len = 0; !IS_BEGIN(ptr[-len]); len++);
         while (len >= 0) {
           i = euc_pre(ptr-len,key,count);
           if (i != -1) return (i+len);
           if (ptr[--len] & 0x80) len--;
         }
         return (-1);
    case '?':                                   // Matches any single character
         if (IS_BEGIN(*ptr)) return (-1);
         if (*ptr & 0x80) len = 2; else len = 1;
         break;
    default:                                    // Standard character
         c = *ptr;
         if (c & 0x80) {
           c   = 0x7f7f & ((ptr[-1] << 8) | c);
           len = 2;
         }
         else {
           c   = tolower(c);
           len = 1;
         }
         if (c != key[count]) return (-1);
         break;
  }
  i = euc_pre(ptr-len,key,count);               // Common point for most patterns other than *.
  if (i == -1) return (-1);
  return (i+len);
}

//--------------------------------
//
//  This routine fromats a dictionary line for display.  This is used
//  by the user dictionary editor and the main dictonary routines.
//
//      line      -- EUC_buffer object used to access the list box.
//      data      -- Poiner into the dictionary line (not the beginning 
//                   of the line).
//      user      -- Set to true for the user dictionary editor.  This 
//                   allows the slash characters to remain in the line.
//      format    -- This indicates the format.  This is used in the 
//                   expansion of the meaning field.
//
//      RETURN    -- The return value is a pointer to the next dictionary 
//                   line element.  This is used in the user dictionary to 
//                   advance through the dictionary.  If an error occures 
//                   in parsing the dictionary, a value of NULL is returned 
//                   to indicate an error.
//
static byte *format_line (EUC_buffer *line,byte *data,int user,int format) {
  int ch,i;
  int first_line = true;
  line->clear ();                               // Intialize line buffer.
  for (i = 0; i < SIZE_LINE; i++) {             // Limit string length.
    if (!*data || ISCRLF(data[1])) break;       // End of line so exit, or error condition (past end of buffer)
    if ((*data & 0x80) && ((format == DICT_EUC) || first_line)) {  // Output kanji/kana character.
      ch = *data++;
      line->put_char ((ch << 8) | *data);
    }
    else if (first_line) {                      // First line has special characters.
      switch (*data) {
        case '[':
             line->put_char (jwp_config.cfg.dict_compress ? '[' : KANJI_LBRACKET);
             break;
        case ']':
             line->put_char (jwp_config.cfg.dict_compress ? ']' : KANJI_RBRACKET);
             break;
        case ' ':
             line->put_char ('\t');
             break;
        case '/':                               // First '/' indicates end of first line, just text after here.
             first_line = false;
             if (!jwp_config.cfg.dict_compress) line->flush (-1);
             break;
        default:
             line->put_char (*data);
             break;
      }
    }
    else if (!user && (*data == '/')) {         // After first line just output, but change '/' into ', '.
      line->put_char (',');
      line->put_char (' ');
    }
    else line->put_char (*data);
    data++;
  }
  line->flush (-1);                             // Flush last line.
  data += 2;                                    // Calculate location of next line.
  if (ISCRLF(*data)) data++;
  return (data);
}

//--------------------------------
//
//  This routine generate the name of an index file from the name of the dictionary file.
//
//  This is basically an extension replacement routine.  If there is no extension, then ".jdx" is
//  added.  If there is an extension, it is replaced with ".jdx".
//
//      name   -- Dictionary file name.
//      buffer -- Buffer to hold the result.  If this is set to NULL, the result is placed
//                back in the name buffer.
//
//      RETURN -- Pointer to the result.
//
static TCHAR *index_name (TCHAR *name,TCHAR *buffer) {
  int i;
  if (!buffer) buffer = name; else lstrcpy (buffer,name);
  for (i = lstrlen(buffer)-1; (i > 0) && (buffer[i] != '\\') && (buffer[i] != '.'); i--);
  if (buffer[i] == '.') buffer[i] = 0;
  lstrcat (buffer,TEXT(".jdx"));
  return  (buffer);
}

//--------------------------------
//
//  Utility routine to load a dictionary file from disk if the file
//  is available.  The routine does not deal with errors.
//
//      name -- Name of file to load.
//
static byte *load_dict (tchar *name) {
  int    i;
  byte  *dict;
  HANDLE handle;
  unsigned long done;

  handle = CreateFile(name,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null);
  if (handle == INVALID_HANDLE_VALUE) return (NULL);
  i = GetFileSize(handle,NULL);
  if ((dict = (byte *) calloc(1,i+5))) {
    dict[0] = '\n';                          // Make it look like there is an earlier 
    ReadFile(handle,dict+1,i,&done,NULL);    //   entry (so we can search to beginning)
  }
  CloseHandle (handle);
  return (dict);
}

//--------------------------------
//
//  Does the setup for the dictioanry compair.  This is basically a hunt for the kana
//  part of the dictionary entry.
//
//      buffer -- Pointer to dictionary line.
//      end    -- On exit this contains a pointer to the end of kana string.
//
//      return -- Pointer to the bigginning fo the kana string.
//
static KANJI *setup_compare (KANJI *buffer,KANJI *&end) {
  KANJI *ptr,*p;
  for (ptr = buffer; *ptr && (*ptr != '[') && (*ptr != KANJI_LBRACKET); ptr++);
  if (*ptr) {
    for (p = ++ptr; *p && (*p != ']') && (*p != KANJI_RBRACKET); p++);
  }
  else {
    for (p = ptr = buffer; *p && (*p != '\t'); p++);
  }
  end = p;
  return (ptr);
}

//--------------------------------
//
//  This is a utility routien used by the entry type filtering in the 
//  JWP_dict::search_dict() routine.  This routine attempts to see if 
//  the string pointed to matches a particular dictionary type key.  The
//  conditions for such a match are to match the charcters in the key, 
//  and be followed by a ',' or a ')'.
//
//  Note the first character of the input string is skiped, because it
//  is assumed that this is either '(', or ','.
//
//      ptr -- String to be tested.
//      key -- Key to test against (null termianted ascii string).
//
static int test_key (byte *ptr,char *key) {
  int len = strlen(key);
  if (strncmp((char *) ++ptr,key,len)) return (0);
  if ((ptr[len] == ',') || (ptr[len] == ')')) return (len+1);
  return (0);
}

//--------------------------------
//
//  Small utility routine to move a counter back to the beginning of a UTF8 character.
//  The extensions to UTF characters all have the form 10xx,xxxx.  We can use this to 
//  move backwards through the list.to find the beginning of a character.
//
//      ptr    -- String to be moved in.
//      index  -- Index value to start with, and move back to the beginning of the character.
//
//      RETURN -- Index of the beginning of the character.  If the character is ascii, this 
//                may be the same as index.
//
static int utf_back (byte *ptr,int index) {
  while (UTF8_VALUEC == (ptr[index] & UTF8_MASKC)) index--;
  return (index);
}

//--------------------------------
//
//  Dictionary comparison routine for UTF8 dictionaries 
//
//      key    -- JIS string to be compared with.
//      ptr    -- Pointer to location in dictionary to be compared.
//      length -- Length of the key.
//
//      RETURN -- Zero indicates the key and data match.  A non-zero value
//                indicates they don't match.  Positive indicates key is 
//                later than the data.  Netgitive indicates oposite.
//
static int utf_comp (byte *ptr,KANJI *key,int length,int &utf_length) {
  int   i,j;
  KANJI c;
  byte  *start;
  start = ptr;
  for (j = i = 0; i < length; i++, j++) {
    c = utf2jis (ptr);
    if       (c < 0x80)                     c = tolower(c);
    else if ((c & 0xff00) == BASE_KATAKANA) c = BASE_HIRAGANA | (c & 0x00ff);
    if (c != key[i]) return (key[i]-c);
  }
  utf_length = ptr-start;
  return (0);
}

//--------------------------------
//
//  Pattern testing routine.  This routine is a recusively called patter to match post-search sequences 
//  of characters.  The rouine is designed to attempt to match a single character in the pattern, and
//  call itself, until no characters are left to call.  The * character can generate a large number of
//  sub calls, and will search for the longest matching string.
//
//      ptr    -- Pointer to dictionary buffer at the first character beyond the search match (ie what we are trying to match).
//      key    -- Remmainning key to match.
//      count  -- Number of characters in the key to match.
//
//      RETURN -- -1 indicates a match failure.  Any other value indicates the number of characters
//                used in the match.  Remember to include sub characters from further calls.
//
static int utf_post (byte *ptr,KANJI *key,int count) {
  int   c,i,len;
  byte *p;
  if (!count) return (0);
  switch (*key) {
    case '[':                                   // This is not possible!
         return (-1);
    case ']':                                   // Match end
         if (!IS_END_N(*ptr)) return (-1);
         len = 0;
         break;
    case '*':                                   // Matches any number of characters.
         for (len = 0; !IS_END_N(ptr[len]); len++);
         while (len >= 0) {
           i = utf_post(ptr+len,key+1,count-1);
           if (i != -1) return (i+len);
           len = utf_back(ptr,len-1);
         }
         return (-1);
    case '?':                                   // Matches any single character
         if (IS_END_N(*ptr)) return (-1);
         len = utf_size(*ptr);
         break;
    default:                                    // Standard character
         p   = ptr;;
         c   = utf2jis(p);
         if (c != *key) return (-1);
         len = p-ptr;
         break;
  }
  i = utf_post(ptr+len,key+1,count-1);          // Common point for most patterns other than *.
  if (i == -1) return (-1);
  return (i+len);
}

//--------------------------------
//
//  Patter testing routine.  This routine is a recusively called patter to match pre-search sequences 
//  of characters.  The rouine is designed to attempt to match a single character in the pattern, and
//  call itself, until no characters are left to call.  The * character can generate a large number of
//  sub calls, and will search for the longest matching string.
//
//  Note, this rotuien removes the key elements from the end, not the beginning, so you pass the same 
//  key to each subcall and just change the count.
//
//      ptr    -- Pointer to dictionary buffer at the first character beyond the search match (ie what we are trying to match).
//      key    -- Remmainning key to match.
//      count  -- Number of characters in the key to match.
//
//      RETURN -- -1 indicates a match failure.  Any other value indicates the number of characters
//                used in the match.  Remember to include sub characters from further calls.
//
static int utf_pre (byte *ptr,KANJI *key,int count) {
  int   c,i,len;
  byte *p;
  if (!count) return (0);
  switch (key[--count]) {                       // Matches any number of characters.
    case ']':                                   // This is not possible!
         return (-1);
    case '[':                                   // Match beginning
         if (!IS_BEGIN(*ptr)) return (-1);
         len = 0;
         break;
    case '*':
         for (len = 0; !IS_BEGIN(ptr[-len]); len++);
         while (len >= 0) {
           i = utf_pre(ptr-len,key,count);
           if (i != -1) return (i+len);
           len -= utf_size(ptr[-len]);
         }
         return (-1);
    case '?':                                   // Matches any single character
         if (IS_BEGIN(*ptr)) return (-1);
         len = 1-utf_back(ptr,0);
         break;
    default:                                    // Standard character
         len = 1-utf_back(ptr,0);
         p   = ptr-len+1;
         c   = utf2jis(p);
         if (c != key[count]) return (-1);
         break;
  }
  i = utf_pre(ptr-len,key,count);               // Common point for most patterns other than *.
  if (i == -1) return (-1);
  return (i+len);
}

//--------------------------------
//
//  Determine the number of bytes used by a UTF8 character.  This is based on
//  examination of a single byte of the character.  Generally, this is assuming
//  we are looking at the lead character which contains a ID indicating the number
//  of bytes.
//      
//      ch     -- Character to examine.
//
//      REUTRN -- Number of bytes in the character.
//
static int utf_size (int ch) {
  if (UTF8_VALUE1 == (ch & UTF8_MASK1)) return (1);
  if (UTF8_VALUE2 == (ch & UTF8_MASK2)) return (2);
  if (UTF8_VALUE3 == (ch & UTF8_MASK3)) return (3);
  if (UTF8_VALUE4 == (ch & UTF8_MASK4)) return (4);
  return (1);
}

//===================================================================
//
//  Exported routines.
//

//--------------------------------
//
//  This is a rotuine called during clean up to deallocate memory helded
//  by the dictionary routines.
//
void free_dictionary () {
  Dictionary *dict;
  while (dicts) {
    dict  = dicts;
    dicts = dict->next;
    dict->close ();
    delete dict;
  }
  return;
}

//===================================================================
//
//  Begin class Dictionary routines..
//
//  These routines handle processing of the dictionary list and all other
//  stuff that needs to be done with the dictionary list.
//

//-------------------------------------------------------------------
//
//  Definitions
//

//
//  Flags as writtin in the dictionary configuration file.
//
#define DICTFLAG_EUC        'E'         // Format EUC
#define DICTFLAG_UTF8       'U'         // Format UTF8
#define DICTFLAG_MIXED      'M'         // Format Mixed (discuraged format)
#define DICTFLAG_INDEX      'I'         // Has index file
#define DICTFLAG_NAMES      'N'         // Has names
#define DICTFLAG_NAMESONLY  'O'         // Has only names
#define DICTFLAG_CLASSICAL  'c'         // Is a classical dictionary    
#define DICTFLAG_USER       'u'         // Is the user dictionary.
#define DICTFLAG_BUFFER     'B'         // Uses a buffered search
#define DICTFLAG_SEARCH     'S'         // Is searched
#define DICTFLAG_QUIET      'Q'         // Quiet handling of errors
#define DICTFLAG_KEEP       'K'         // Keep dictionary in memory or open.

//
//  Flags used by JWP_dict::add_dict()
//
#define ADD_SEARCH          0x0001      // Search this dictionary if it exists.
#define ADD_INDEX           0x0002      // If you can find an index file use it.

//
//  Magic id for dictionary configuration file
//
#define DICT_MAGIC          0x12bc3e76  // Magic ID

//-------------------------------------------------------------------
//
//  class Dictionary routines.
//

//--------------------------------
//
//  Destructor.
//  fields
//
Dictionary::~Dictionary () {
  close ();
  if (name)     free (name);
  if (filename) free (filename);
  name = filename = NULL;
  return;
}

//--------------------------------
//
//  Add a this dictionary to the dictionary list in the dialog box.
//
//      list   -- Pointer to the dialog box control.
//      index  -- Zero based offset for the entry.
//
void Dictionary::addlist (HWND list,int index) {
  LVITEM item;
  int    i;
  memset (&item,0,sizeof(item));
  item.mask     = LVIF_TEXT;
  item.iItem    = index;
  item.iSubItem = COLUMN_NAME;
  item.pszText  = name;
  ListView_InsertItem (list,&item);
  switch (format) {
    case DICT_EUC      : i = IDS_DC_FORMATEUC  ; break;
    case DICT_UTF8     : i = IDS_DC_FORMATUTF8 ; break;
    case DICT_MIXED    : i = IDS_DC_FORMATMIXED; break;
    default:             i = IDS_DC_ERR        ; break;
  }
  ListView_SetItemText (list,index,COLUMN_FORMAT,get_string(i));
  switch (names) {
    case DICT_NONAMES  : i = IDS_DC_NONAMES    ; break;
    case DICT_NAMES    : i = IDS_DC_NAMES      ; break;
    case DICT_NAMESONLY: i = IDS_DC_NAMESONLY  ; break;
    default:             i = IDS_DC_ERR        ; break;
  }
  ListView_SetItemText (list,index,COLUMN_NAMES,get_string(i));
  switch (special) {
    case DICT_NORMAL   : i = IDS_DC_NO         ; break;
    case DICT_CLASSICAL: i = IDS_DC_CLASSICAL  ; break;
    case DICT_USER     : i = IDS_DC_USER       ; break;
    default:             i = IDS_DC_ERR        ; break;
  }
  ListView_SetItemText  (list,index,COLUMN_SPECIAL,get_string(i));
  if      (idx)      i = IDS_DC_TYPEIDX;
  else if (buffered) i = IDS_DC_TYPEBUF;
  else               i = IDS_DC_TYPEMEM;
  ListView_SetItemText  (list,index,COLUMN_TYPE ,get_string(i));
  ListView_SetItemText  (list,index,COLUMN_KEEP ,get_string(keep  ? IDS_DC_YES : IDS_DC_NO));
  ListView_SetItemText  (list,index,COLUMN_QUIET,get_string(quiet ? IDS_DC_YES : IDS_DC_NO));
  ListView_SetItemText  (list,index,COLUMN_FILE ,filename);
  ListView_SetItemState (list,index,searched ? 2<<12 : 1<<12,LVIS_STATEIMAGEMASK);
  return;
}

//--------------------------------
//
//  Close a dictioanry and deallocate any resources allocated.
//
void Dictionary::close () {
  if (memory) free (memory);
  if (dict  != INVALID_HANDLE_VALUE) CloseHandle (dict);
  if (index != INVALID_HANDLE_VALUE) CloseHandle (index);
  index  = INVALID_HANDLE_VALUE;
  dict   = INVALID_HANDLE_VALUE;
  memory = NULL;
  return;
}

//--------------------------------
//
//  Reads the first entry in a dictionary and stores this is the description.  This may also 
//  be used to generate the name for the dictionary.  Often the first line contains a dictionary
//  ID, but in some dictionaries, it just contains the frist entry.
//
//      hwnd   -- Dialog window.
//      doname -- If non-zero the name field will also be set.
//
void Dictionary::get_firstline (HWND hwnd,int doname) {
  byte  *ptr,buffer[SIZE_BUFFER+10];
  KANJI  kbuffer[SIZE_BUFFER];
  int    i,length;
//
//  Open file and get the first line for the description.
//
  length = 0;
  if (!read_sample(buffer,SIZE_BUFFER)) {
    for (ptr = buffer; *ptr && (*ptr != '/'); ptr++);
    if (IsDlgButtonChecked(hwnd,IDC_DSUTF8)) {
      for (ptr++; *ptr && (*ptr != '/'); ) kbuffer[length++] = utf2jis(ptr);
    }
    else {
      for (ptr++; *ptr && (*ptr != '/'); ptr++) kbuffer[length++] = *ptr;
    }
    if (!*ptr) {
      JMessageBox (hwnd,IDS_DS_ERRORNOTDICT,IDS_DS_ERROR,MB_OK | MB_ICONWARNING,filename);
      length = 0;
    }
  }
  SendDlgItemMessage (hwnd,IDC_DSDESCRIPTION,JE_SETTEXT,length,(LPARAM) kbuffer);
//
//  Setup the name.  
//
  if (!doname) return;
  GetDlgItemText (hwnd,IDC_DSNAME,(TCHAR *) buffer,SIZE_BUFFER/2);
  if (lstrlen((TCHAR *) buffer) && (IDYES != JMessageBox(hwnd,IDS_DS_ALREADYTEXT,IDS_DS_ALREADYTITLE,MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2))) return;
  if (!length) {
    SetDlgItemText (hwnd,IDC_DSNAME,filename);
    return;
  }
#ifdef WINCE
  for (i = 0; i < length; i++) kbuffer[i] = jis2unicode(kbuffer[i]);
  kbuffer[i] = 0;
  SetDlgItemText (hwnd,IDC_DSNAME,(TCHAR *) kbuffer);
#else  WINCE
  for (i = 0; i < length; i++) buffer[i] = (byte) (kbuffer[i]);
  buffer[i] = 0;
  SetDlgItemText (hwnd,IDC_DSNAME,(TCHAR *) buffer);
#endif WINCE
  return;
}

//--------------------------------
//
//  This routine determines some general information about a dictionary based on examining the 
//  file.  This routine checks for an index file as well as checking the file format.  These 
//  parameters are placed in the dialog box.
//
//      hwnd   -- Pointer to edit dictionary dialog.
//
//      RETURN -- A non-zero value idnicates the file does not exist.
//
#define SIZE_TESTBUFFER (2048)

int Dictionary::get_info (HWND hwnd) {
  int i,j = 0;
  byte  buffer[SIZE_TESTBUFFER+10];
//
//  Setup index.
//
  CheckDlgButton (hwnd,IDC_DSBUFFER,false);
  CheckDlgButton (hwnd,IDC_DSMEMORY,false);
  CheckDlgButton (hwnd,IDC_DSINDEX ,false);
  CheckDlgButton (hwnd,IDC_DSEUC   ,false);
  CheckDlgButton (hwnd,IDC_DSUTF8  ,false);
  CheckDlgButton (hwnd,IDC_DSMIXED ,false);
  CheckDlgButton (hwnd,FileExists(index_name(get_name(),(TCHAR *) buffer)) ? IDC_DSINDEX : IDC_DSMEMORY,true);
//
//  Read beginning of the file.
//
  if (read_sample(buffer,SIZE_TESTBUFFER+10)) {
    CheckDlgButton (hwnd,IDC_DSEUC,true);
    return (true);
  }
//
//  Check for ascii file.
//
  for (i = 0; (i < SIZE_TESTBUFFER) && (buffer[i] <= 0x7f); i++);
  if (i == SIZE_TESTBUFFER) {
    CheckDlgButton (hwnd,IDC_DSEUC,true);
    return (false);
  }
//
//  Check for UTF-8.
//
  for (i = 0; i < SIZE_TESTBUFFER; i++) {
    if ((UTF8_VALUE1 == (buffer[i] & UTF8_MASK1))) continue;
    if ((UTF8_VALUE2 == (buffer[i] & UTF8_MASK2)) && (UTF8_VALUEC == (buffer[i+1] & UTF8_MASKC))) { i++; continue; }
    if ((UTF8_VALUE3 == (buffer[i] & UTF8_MASK3)) && (UTF8_VALUEC == (buffer[i+1] & UTF8_MASKC)) && (UTF8_VALUEC == (buffer[i+2] & UTF8_MASKC))) { i += 2; continue; }
    if ((UTF8_VALUE4 == (buffer[i] & UTF8_MASK4)) && (UTF8_VALUEC == (buffer[i+1] & UTF8_MASKC)) && (UTF8_VALUEC == (buffer[i+2] & UTF8_MASKC)) && (UTF8_VALUEC == (buffer[i+3] & UTF8_MASKC))) { i += 3; continue; }
    break;
  }
  if (i >= SIZE_TESTBUFFER) {
    CheckDlgButton (hwnd,IDC_DSUTF8,true);
    return (false);
  }
//
//  Check for EUC file
//
  for (i = 0; i < SIZE_TESTBUFFER; i++) {
    if  (buffer[i] <= 0x7f) continue;
    if ((buffer[i] >= 0x80) && (buffer[i+1] >= 0x80)) { i++; continue; }
    break;
  }
  if (i >= SIZE_TESTBUFFER) {
    CheckDlgButton (hwnd,IDC_DSEUC,true);
    return (false);
  }
//
//  Must be a mixed format
//
  CheckDlgButton (hwnd,IDC_DSMIXED,true);
  return (false);
}

//--------------------------------
//
//  This routine reads a line from the diction based on an index into 
//  the index file.
//
//      loc    -- Index into dictionary file to get data for.
//      buffer -- Buffer to read line into.  Note that this routine 
//                actually reads data into the middle of the buffer.
//                This is so you can back up in the buffer.  For 
//
//  From search_dict() routine:
//
//  This routine uses buffer and buf, to allow us to backward scan in 
//  the file.  This works as follows:
//
//  When a read from the dictionary takes place, a block of size SIZE_DICTBUFFER
//  bytes is read.  The cener of this block is the locationactually 
//  requested.  This gives us the capability to backup to the beginning 
//  of the dictonary entry.  
//
//  The pointer buf, points into the buffer and points the actual point
//  in the line that was being requested.
//
void Dictionary::get_line (int loc,byte *buffer) {
  long offset;
  unsigned long done;
  SetFilePointer (index,(loc+1)*sizeof(long),NULL,FILE_BEGIN);
  ReadFile (index,&offset,sizeof(long),&done,NULL);
  if (offset-SIZE_LINE < 0) {                       // Special case for near the beginning of the file.
    memset         (buffer,'\n',SIZE_LINE);         // This is required to prevent search errors.
    SetFilePointer (dict,0,NULL,FILE_BEGIN);
    ReadFile       (dict,buffer+SIZE_LINE-offset,SIZE_DICTBUFFER-SIZE_LINE+offset,&done,NULL);
  }
  else {                                            // General case.
    SetFilePointer (dict,offset-SIZE_LINE,NULL,FILE_BEGIN);
    ReadFile       (dict,buffer,SIZE_DICTBUFFER,&done,NULL);
  }
  return; 
}

//--------------------------------
//
//  Gets actual file name for this dictionary.  Since we support relative names, this can be
//  a bit more complicated 
//
//      buffer -- Location to store the name if necessary.  DO NOT USE THIS!
//
//      RETURN -- Pointer to file name.
//
TCHAR *Dictionary::get_name () {
#ifdef WINCE
  if (filename[0] == '\\') return (filename);
#else  WINCE
  if (filename[1] == ':') return (filename);
#endif WINCE
  return (jwp_config.name(filename,OPEN_READ,false));
}

//--------------------------------
//
//  Initialize the dictionary structrue.  This is used for all routines.
//
//      _name  -- Name of dictionary.
//      file   -- File name for dictionary.
//      flags  -- Flags for dictionary.
//
void Dictionary::init (TCHAR *_name,TCHAR *file,TCHAR *flags) {
  next     = NULL;
  memory   = NULL;
  dict     = INVALID_HANDLE_VALUE;
  index    = INVALID_HANDLE_VALUE;
  name     = strdup(_name);
  filename = strdup(file);
  format   = DICT_EUC;
  idx      = false;
  names    = DICT_NONAMES;
  special  = DICT_NORMAL;
  searched = false;
  buffered = false;
  keep     = false;
  quiet    = false;
  while (*flags) {
    switch (*flags++) {
      case DICTFLAG_EUC      : format   = DICT_EUC;       break;
      case DICTFLAG_UTF8     : format   = DICT_UTF8;      break;
      case DICTFLAG_MIXED    : format   = DICT_MIXED;     break;
      case DICTFLAG_INDEX    : idx      = true;           break;
      case DICTFLAG_NAMES    : names    = DICT_NAMES;     break;
      case DICTFLAG_NAMESONLY: names    = DICT_NAMESONLY; break;
      case DICTFLAG_CLASSICAL: special  = DICT_CLASSICAL; break;
      case DICTFLAG_USER     : special  = DICT_USER;      break;
      case DICTFLAG_BUFFER   : buffered = true;           break;
      case DICTFLAG_SEARCH   : searched = true;           break;
      case DICTFLAG_KEEP     : keep     = true;           break;
      case DICTFLAG_QUIET    : quiet    = true;           break;
      default: break;
    }
  }
  return;
}

//--------------------------------
//
//  Open a dictionary.
//
//      RETURN -- Non-zero value indicates an error.
//
int Dictionary::open () {
//
//  Indexed dictionaries.
//
  if (idx) {
    TCHAR *ptr,buffer[SIZE_BUFFER];
    if (dict != INVALID_HANDLE_VALUE) return (false);
    index_name (ptr = get_name(),buffer);
    dict  = CreateFile (ptr   ,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null);
    index = CreateFile (buffer,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null);
    if ((dict == INVALID_HANDLE_VALUE) || (index == INVALID_HANDLE_VALUE)) return (IDS_DD_DICTINDEX);
    index_max = (GetFileSize(index,NULL)/sizeof(long))-1;
    return (false);
  }
//
//  Memory (and buffered dictionaries).
//
  if (memory) return (false);                                           // Already open.
  if (!(memory = load_dict(get_name()))) return (IDS_DD_CANNOTOPEN);    // Open and read into memory.
  return (false);
}

//--------------------------------
//
//  This utility rotuine reads the first number bytes from a dictionary.  This is used to 
//  get the first line as well as to check the formatting of a dictionary.
//
//      buffer -- Location ot store the data.
//      size   -- Number of bytes to read.
//
//      RETURN -- A non-zero value indicates a read error.  This is either an allocation
//                error or a file open error.
//
int Dictionary::read_sample (byte *buffer,int size) {
  HANDLE        file;
  unsigned long done;
  file = CreateFile (get_name(),GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null);
  memset (buffer,0,size);
  if (file == INVALID_HANDLE_VALUE) return (true);
  ReadFile    (file,buffer,size,&done,NULL);
  CloseHandle (file);
  return      (false);
}

//--------------------------------
//
//  Writes a specific dictionary to a file.  The information is written at 3 null termianted strings.  The 
//  first string containns flags for the dictionary.  The second contains the operational name for the 
//  dictionary.  The third contains the file name for the dictionary.
//
//      file -- File to write to.
//
void Dictionary::write (HANDLE file) {
  TCHAR buffer[20],*ptr;
  unsigned long done;
//
//  Build flag string.
//
  ptr = buffer;
  switch (format) {
    case DICT_EUC:       *ptr++ = DICTFLAG_EUC;       break;
    case DICT_UTF8:      *ptr++ = DICTFLAG_UTF8;      break;
    case DICT_MIXED:     *ptr++ = DICTFLAG_MIXED;     break;
    default: break;
  }
  switch (names) {
    case DICT_NAMES:     *ptr++ = DICTFLAG_NAMES;     break;
    case DICT_NAMESONLY: *ptr++ = DICTFLAG_NAMESONLY; break;
    default: break;
  }
  switch (special) {
    case DICT_CLASSICAL: *ptr++ = DICTFLAG_CLASSICAL; break;
    case DICT_USER:      *ptr++ = DICTFLAG_USER;      break;
    default: break;
  }
  if (idx)      *ptr++ = DICTFLAG_INDEX;
  if (buffered) *ptr++ = DICTFLAG_BUFFER;
  if (searched) *ptr++ = DICTFLAG_SEARCH;
  if (keep)     *ptr++ = DICTFLAG_KEEP;
  if (quiet)    *ptr++ = DICTFLAG_QUIET;
  *ptr++ = 0;
//
//  Write strings
//
  WriteFile (file,buffer  ,sizeof(TCHAR)*(lstrlen(buffer  )+1),&done,NULL);
  WriteFile (file,name    ,sizeof(TCHAR)*(lstrlen(name    )+1),&done,NULL);
  WriteFile (file,filename,sizeof(TCHAR)*(lstrlen(filename)+1),&done,NULL);
  return;
}

//-------------------------------------------------------------------
//
//  class JWP_dict routines..
//

//--------------------------------
//
//  Create a new dictionary entry from three string inputs.
//
//      base    -- Pointer to add the dictionaries to.
//      name    -- Name of ditionary.
//      file    -- Name of file for dictionary.
//      flags   -- Pointer to flags string for dictionary.
//      startup -- Configuration flags.  Only two are currently supported:
//
//              ADD_INDEX  -- Check for index file.  If it exists, then treat dictionary as indexed.
//              ADD_SEARCH -- Check is dictionary exists.  If so add to the searched list.
//
//      RETURN  -- A non-zero value indicates an error.
//
int JWP_dict::add_dict (Dictionary *&base,TCHAR *name,TCHAR *file,TCHAR *flags,int startup) {
  Dictionary *dic,*ptr;
  TCHAR      *filename,buffer[SIZE_BUFFER];
  if (!(dic = new_dictionary(name,file,flags))) return (true);
  if (!base) base = dic;
    else {
      for (ptr = base; ptr->next; ptr = ptr->next);
      ptr->next = dic;
    }
  filename = dic->get_name();
  if ((startup & ADD_SEARCH) && FileExists(filename))                    dic->searched = true;
  if ((startup & ADD_INDEX ) && FileExists(index_name(filename,buffer))) dic->idx      = true;
  return (false);
}

//--------------------------------
//
//  Attempts to load the dictionaries list.  If the list does not exist, the list will be generated.
//
#define SKIP_STRING(x)  ((x)+lstrlen(x)+1)

void JWP_dict::load () {
  TCHAR      *base,*file,*flags,*name,*filename;
  long        magic;

  base = file = (TCHAR *) load_image(jwp_config.name(NAME_DICTIONARIES,OPEN_READ,true));
  if (file) memcpy (&magic,file,sizeof(long));
//
//  Initialize with default list.
//
  if (!file || (magic != DICT_MAGIC)) {
    JMessageBox (dialog,IDS_DICT_ERROR,IDS_WARNING,MB_OK | MB_ICONWARNING,jwp_config.name());
    def_dictionaries   (dicts);
    write_dictionaries ();
  }
//
//  Read the user list.
//
  else {
    file = (TCHAR *) (((byte *) file)+sizeof(long));
    while (*file) {
      flags    = file;
      name     = SKIP_STRING(flags);
      filename = SKIP_STRING(name);
      file     = SKIP_STRING(filename);
      add_dict (dicts,name,filename,flags,false);
    }
  }
//
//  Find and load the user dictionary.
//
  find_user ();
  free (base);
  return;
}

//--------------------------------
//
//  This routine intializes the default dictionary set.  Normally this is called jsut the 
//  first time you enter the dictionary.  You can force this routine, from the dictionaries
//  dialog, however.
//
//      base -- Pointer to where to build the list.  Normally this would be the global dictioanry
//              list, but the default option builds a scratch list.
//
#define FLAGS_CLASSICAL TEXT("Ec")
#define FLAGS_EDICT     TEXT("ENIS")
#define FLAGS_ENAMDIC   TEXT("ENIOQS")
#define FLAGS_USERDICT  TEXT("NMuSKQ")
#define FLAGS_LANGUAGE  TEXT("USQ")

void JWP_dict::def_dictionaries (Dictionary *&base) {
  TCHAR *lang;
  lang = get_string (IDS_DICT_LANGUAGE);
  if (lang[0] != '.') add_dict (base,lang          ,lang          ,FLAGS_LANGUAGE ,ADD_INDEX);
  add_dict (base,get_string(IDS_DICT_NAMECLASSICAL),NAME_CLASSICAL,FLAGS_CLASSICAL,ADD_SEARCH);
  add_dict (base,get_string(IDS_DICT_NAMEEDICT    ),NAME_MAINDICT ,FLAGS_EDICT    ,false);
  add_dict (base,get_string(IDS_DICT_NAMEENAMDICT ),NAME_NAMEDICT ,FLAGS_ENAMDIC  ,false);
  add_dict (base,get_string(IDS_DICT_NAMEUSER     ),NAME_USERDICT ,FLAGS_USERDICT ,false);
  return;
}

//--------------------------------
//
//  Write the dictioanries file to disk.  This file contains the configuration used for dictionaries.
//  The format of the file is as follows:
//
//      magic       -- Coded long int to make sure we have the correct file.
//      
//      flags<0>    -- Zero terminated string containning the flags for this dictionary.
//      name<0>     -- Zero terminated string containning the name for this dictionary.
//      file<0>     -- Zero termianted string containning the filename for this dictionary.
//
//      Repeat the dictionary pattern for each dictionary.
//
//      0000        -- Four bytes of zeros to indicate end of the file.
//
//      Error messages are attached to the dictionary window, not the dictioanries dialog.  This could be
//      improved at a later time.
//
int JWP_dict::write_dictionaries () {
  HANDLE file;
  if (INVALID_HANDLE_VALUE == (file = jwp_config.open(NAME_DICTIONARIES,OPEN_NEW,true))) {
    QUIET_ERROR {
      error  (IDS_DS_ERROROPEN,jwp_config.name());
      return (true);                    
    }
  }
//
//  Actual write.
//
  long magic = DICT_MAGIC;
  unsigned long done;
  Dictionary *dict;
  WriteFile(file,&magic,sizeof(magic),&done,NULL);
  for (dict = dicts; dict; dict = dict->next) dict->write (file);
  magic = 0;
  WriteFile(file,&magic,sizeof(magic),&done,NULL);
  CloseHandle (file);
  return (false);
}

//===================================================================
//
//  Begin user dictionary edit routines.
//
//  These routines handler the user dictionary and are basically 
//  operations supporting the EDIT_userdict class, which is dirived 
//  from the EDIT_list class.
//

//-------------------------------------------------------------------
//
//  Class definition.
//

class EDIT_userdict : public EDIT_list {
public:
  int   dlg_edituser(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
  byte *get_data    (void);                     // Get data from the list into working format.
  void  put_data    (byte *data,tchar *name);   // Put data into the dialog box for initialize.
private:
  int   edit        (void);                     // Edit/Add entry procedure.
};

static class EDIT_userdict *edit_userdict = NULL;   // Pointer to class instance so dialog procedure can find us.

//-------------------------------------------------------------------
//
//  Static routines.
//

//--------------------------------
//
//  Dialog box stub for the main dialog, simply calles the class rotuine.
//
static int dialog_edituser (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (edit_userdict->dlg_edituser(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  Dialog procedure for editing the user dictionary.  This a stub that 
//  calls JWP_dict::dlg_userdict
//
static BOOL CALLBACK dialog_userdict (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_dict.dlg_userdict(hwnd,message,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  class EDIT_userdict.
//

//--------------------------------
//
//  Dialog box procedure for editing an entry in the user dictionary.
//
//      IDC_DEKANA        Kana edit box (required)
//      IDC_DEKANJI       Kanji edit box (optional)
//      IDC_DEMEANING     Meaning edit box (required)
//
//      IDC_DDSTRING      Search string from dictionary dialog box (used to initialize user dict)
//
int EDIT_userdict::dlg_edituser (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  int lkana,lkanji;
  KANJI *kkana,*kkanji;
  TCHAR *ptr,buffer[SIZE_BUFFER];
  int i,j;

  switch (message) {
//
//  Initialize the dialog box.  Read the data from the kbuffer, and 
//  setup the dialog parameters.
//
    case WM_INITDIALOG:
         if (length) {          // this is an EDIT, so initialize the editor.
           for (i = 0; kbuffer[i] != '\t'; i++);
           i++;                 // kana only.
           if ((kbuffer[i] != KANJI_LBRACKET) && (kbuffer[i] != '[')) {
             SendDlgItemMessage (hwnd,IDC_DEKANA,JE_SETTEXT,i-1,(LPARAM) kbuffer);
           }
           else {               // Kana and kanji.
             SendDlgItemMessage (hwnd,IDC_DEKANJI,JE_SETTEXT,i-1,(LPARAM) kbuffer);
             for (i++, j = 0; kbuffer[i+j] != '\t'; j++);
             SendDlgItemMessage (hwnd,IDC_DEKANA,JE_SETTEXT,j-1,(LPARAM) (kbuffer+i));
             i += j+1;
           }                    // Setup meaning.
           for (j = 0; i < length; i++) buffer[j++] = (char) kbuffer[i];
           buffer[j] = 0;
           SetDlgItemText (hwnd,IDC_DEMEANING,buffer);
         }                      
         else {                 // This is an ADD so initialize from the Dicitionary Dailog Box.
           lkana = JEGetDlgItemText(jwp_dict.dialog,IDC_DDSTRING,&kkana);
           for (i = 0; i < lkana; i++) if (ISKANJI(kkana[i])) break;
           SendDlgItemMessage (hwnd,(i == lkana) ? IDC_DEKANA : IDC_DEKANJI,JE_SETTEXT,lkana,(LPARAM) kkana);
         }
         return (true);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_USEREDIT);
         return  (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           INPUT_CHECK (IDC_DEMEANING);
//
//  User wants to keep this conversion, so see what is up.
//
           case IDOK:
           case IDSEARCH:
//
//  Get strings and make sure they are not empty.
//
                lkana  = JEGetDlgItemText(hwnd,IDC_DEKANA ,&kkana);
                lkanji = JEGetDlgItemText(hwnd,IDC_DEKANJI,&kkanji);
                for (j = false, i = 0; i < lkana; i++) {    // Check for error caracters.
                  if (kkana[i] == ' ') break;
                  if (!ISKANA(kkana[i]) && (kkana[i] != KANJI_LONGVOWEL) && (kkana[i] != KANJI_TILDE)) j = true;
                }
                if (!lkana || (i != lkana)) {               // Kana string is empty or cotnains space -> ERROR!
                  JMessageBox (hwnd,IDS_DE_ERRORKANA,IDS_DE_ERROR,MB_OK | MB_ICONERROR);
                  SetFocus   (GetDlgItem(hwnd,IDC_DEKANA));
                  return (0);
                }                                           // Kana string contains non-kana chracters -> WARNING!
                if (j && (IDNO == JMessageBox(hwnd,IDS_DE_ERRORNONKANA,IDS_DE_ERROR,MB_YESNO | MB_ICONWARNING))) {
                  SetFocus (GetDlgItem(hwnd,IDC_DEKANA));
                  return   (0);
                }
//
//  Build buffer string.
//
                length = 0;
                if (!lkanji) {              // Just kana
                  for (i = 0; i < lkana; i++) kbuffer[length++] = kkana[i];
                }
                else {                      // Kana and kanji.
                  for (i = 0; i < lkanji; i++) kbuffer[length++] = kkanji[i];
                  kbuffer[length++] = '\t';
                  kbuffer[length++] = jwp_config.cfg.dict_compress ? '[' : KANJI_LBRACKET;
                  for (i = 0; i < lkana; i++) kbuffer[length++] = kkana[i];
                  kbuffer[length++] = jwp_config.cfg.dict_compress ? ']' : KANJI_RBRACKET;
                }
                kbuffer[length++] = '\t';
//
//  Setup line break so when we put back the data it will format correctly.
//
                line_break = jwp_config.cfg.dict_compress ? 0 : length;
//
//  Get meaning and format.
//
                GetDlgItemText (hwnd,IDC_DEMEANING,buffer,SIZE_BUFFER);
                if (!lstrlen(buffer)) {      // Make sure the meaning is not empty.
                  JMessageBox (hwnd,IDS_DE_ERRORMEANING,IDS_DE_ERROR,MB_OK | MB_ICONWARNING);
                  SetFocus   (GetDlgItem(hwnd,IDC_DEMEANING));
                  return (0);
                }
#ifdef WINCE                                                                    // This was modified to support extended
                for (ptr = buffer; *ptr; ptr++) kbuffer[length++] = *ptr;       //   ascii in the dictionary searches.
#else  WINCE                                                                            
                for (ptr = buffer; *ptr; ptr++) kbuffer[length++] = *((byte *) ptr);
#endif WINCE
                EndDialog (hwnd,true);
                return (0);
//
//  Didn't want to do it after all.
//
           case IDCANCEL:
                EndDialog (hwnd,false);
                return (0);
         }
  }
  return (false);
}

//--------------------------------
//
//  Required virtual function to edit an entry.  This just invokes the 
//  edit dialog box.  All of the real work is there.
//
int EDIT_userdict::edit () {
  return (JDialogBox(IDD_DICTUSEREDIT,dialog,(DLGPROC) dialog_edituser));
}

//--------------------------------
//
//  This routine gets the data from the from the list and converts it
//  into the format necessary for a dictionary file.  
//
//      RETURN -- Memory allocated copy of the buffer.  This is simply
//                the disk image of the string converted into a long string.
//
byte *EDIT_userdict::get_data () {
  int i,index;
  byte *data,*ptr;
                                                
  if (!(data = (byte *) calloc(1,size()))) { OutOfMemory (dialog); return (data); }
  ptr    = data;
  index  = 0;
  *ptr++ = '\n';                                // This is so the search will work correctly.
  while (index < count()) {                     // Whey there are items keep processing.
    get_buffer (index);                         // Get buffer.
//
//  Put first string (typically this is kanji, but could be kana).
//
    for (i = 0; kbuffer[i] != '\t'; i++) {      
      *ptr++ = (kbuffer[i] >> 8) | 0x80;
      *ptr++ = (kbuffer[i] & 0x00ff) | 0x80;
    }
    *ptr++ = ' ';                               // Separator.
    i++;
//
//  If have a selcond string we need to translate that.
//
    if ((kbuffer[i] == KANJI_LBRACKET) || (kbuffer[i] == '[')) {
      *ptr++ = '[';
      for (i++; (kbuffer[i] != KANJI_RBRACKET) && (kbuffer[i] != ']'); i++) {
        *ptr++ = (kbuffer[i] >> 8) | 0x80;
        *ptr++ = (kbuffer[i] & 0x00ff) | 0x80;
      }
      *ptr++ = ']';
      *ptr++ = ' ';
      i += 2;
    }
//
//  Put meanings, and terminating characters.
//
    *ptr++ = '/';
    for (; i < length; i++) *ptr++ = (byte) kbuffer[i];
    *ptr++ = '/';
//  *ptr++ = '\r';                              // UNIX termination is OK!
    *ptr++ = '\n';
    index  = next_item(index);                  // Move to next item.
  };
  return (data);
}

//--------------------------------
//
//  Required virtual fucntion.  This function takes a data block (from a file
//  or whatever) and puts it into the list box.
//
//      data -- Pointer to the data buffer.
//      name -- Name of data file being read.
//
void EDIT_userdict::put_data (byte *data,tchar *name) {
  if (*data == '\n') data++;        // This is used to skip extra '\n' at beggining of user dictionary.
  while (*data) {
    data = format_line (this,data,true,DICT_MIXED);
  }
  return;
}

//-------------------------------------------------------------------
//
//  class JWP_dict
//

//--------------------------------
//
//  Dialog box procedure used to edit the list.
//
#define RET     *((int *) lParam)

int JWP_dict::dlg_userdict (HWND hwnd,int msg,WPARAM wParam,LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG:
         user_size.wm_init (hwnd,IDC_EDITLIST,&jwp_config.cfg.size_user,true,0,0);
         add_dialog (user_dialog = hwnd,false);
         if (!(edit_userdict = new EDIT_userdict())) return (false);
         edit_userdict->init (hwnd,user->memory,IDS_DE_FILETYPE);
         POCKETPC_DIALOG     (hwnd);
         return (true);
    case WM_DESTROY:
         if (edit_userdict->changed) {
           if (IDYES == JMessageBox(hwnd,IDS_DU_CHANGEDTEXT,IDS_DU_CLOSE,MB_ICONWARNING | MB_YESNO)) save_user ();
         }
         SetFocus (GetParent(hwnd));
         user_dialog = null;
         remove_dialog (hwnd);
         delete edit_userdict;
         return (true);
//
//  Size change message.
//
#ifndef WINCE
    case WM_SIZING:
         user_size.wm_sizing ((RECT *) lParam);
         return (0);
#endif  WINCE
    case WM_SIZE:
         user_size.wm_size (wParam);
         return (0);
    case WM_MOVE:
         user_size.wm_move ();
         return (0);
//
//  Process message generated by system shut-down.
//
    case WMU_OKTODESTROY:
         if (!edit_userdict->changed) { RET = true; return (0); }
         switch (JMessageBox(hwnd,IDS_DU_CHANGEDTEXT,IDS_DU_CLOSE,MB_ICONQUESTION | MB_YESNOCANCEL)) {
           case IDNO:     RET = true;  break;
           case IDCANCEL: RET = false; break;
           case IDYES:    RET = !save_user(); break;
         }
         if (RET) edit_userdict->changed = false;
         return (0);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_USERDICT);
         return  (true);
#ifndef WINCE
    case WM_DROPFILES:                          // Drag & drop import dictionary
         edit_userdict->do_drop ((HDROP) wParam);
         return  (0);
#endif  WINCE
    case WM_COMMAND:
         switch (LOWORD(wParam)) {              
           case IDC_UDSORT:
                edit_userdict->list->sort (dictionary_compare);
                return (0);
           case IDC_EDITLISTADD:                // These events belong to the edit-list class.
           case IDC_EDITLIST:   
           case IDC_EDITLISTEDIT:
           case IDC_EDITLISTUP:
           case IDC_EDITLISTDOWN:
           case IDC_EDITLISTDELETE:
           case IDC_EDITLISTIMPORT:
           case IDC_EDITLISTINSERT:
           case IDC_EDITLISTFIND:
           case IDC_EDITLISTNEXT:
                edit_userdict->do_event (wParam);
                return (0);
//
//  User wants to keep the list so output to a buffer.
//
           case IDOK:                           // User wants to keep changes.
                if (edit_userdict->changed && save_user()) return (0);
           case IDCANCEL:       // **** FALL THORUGH ****
                edit_userdict->changed = false; // Prevents a request to save the data.
                DestroyWindow (hwnd);
                return (0);
         }
         break;
  }
  return (false);
}

//--------------------------------
//
//  Small utility routine to save the user dictionary.  This was seperated out so 
//  the user dictionary could be saved if you close the parent window and the suer 
//  dictionary gets clobered.  This allows the main handler to ask you if you want
//  to save your user dicdtionary.
//
//      RETURN -- A non-zero value indicates a failure to save the dictionary.
//
int JWP_dict::save_user () {
  HANDLE file;
  unsigned long done;
  user->close ();                               // Remove old dictionary.
  user->memory = edit_userdict->get_data ();    // Set new data as user dictionary.
  if (INVALID_HANDLE_VALUE == (file = jwp_config.open(user->get_name(),OPEN_NEW,true))) {
    QUIET_ERROR {
      edit_userdict->error (IDS_DE_ERROROPEN,user->get_name());
      return (true);                    
    }
  }                                       // Write conversions to disk.
  WriteFile   (file,user->memory+1,strlen((char *) user->memory+1),&done,NULL);
  CloseHandle (file);
  return      (false);
}

//
//  End User Dictionary Edit
//
//===================================================================

//===================================================================
//
//  Routines for working with the Dictionary properties dialog box.
//

//-------------------------------------------------------------------
//
//  Defintiions..
//
//  Flags passed to the edit-dictioary dialog box.  These indicate the type of editing we will do.
//
#define EDITDICT_ADD    0           // Adding a new entry (start with blank dialog).
#define EDITDICT_EDIT   1           // Editing a initial entry.
#define EDITDICT_DROP   2           // Respond to a drag and drop operation.
#define EDITDICT_NAME   3           // This is also a responce to a drag and drop, but is used inside the dcitionary dialog box.

//-------------------------------------------------------------------
//
//  Static routines..
//

static Dictionary *get_dictionary (HWND hwnd);      // Read the dialog box parameters to a Dictionary class object.
static TCHAR      *relative_name  (TCHAR *buffer);  // Reduce file name to a relative name

//--------------------------------
//
//  Get dictionary from edit dictionary dialog box.
//
//      hwnd   -- Pointer to the dialog box.
//
//      RETURN -- Pointer to a newly allocated dictionary object.  This object must be 
//                deleted when done with it.
//
static Dictionary *get_dictionary (HWND hwnd) {
  TCHAR *ptr,flags[20],name[SIZE_BUFFER],filename[SIZE_BUFFER];
  ptr = flags;
  if      (IsDlgButtonChecked(hwnd,IDC_DSUTF8     )) *ptr++ = DICTFLAG_UTF8;
  else if (IsDlgButtonChecked(hwnd,IDC_DSMIXED    )) *ptr++ = DICTFLAG_MIXED;
  else                                               *ptr++ = DICTFLAG_EUC;
  if      (IsDlgButtonChecked(hwnd,IDC_DSINDEX    )) *ptr++ = DICTFLAG_INDEX;
  else if (IsDlgButtonChecked(hwnd,IDC_DSBUFFER   )) *ptr++ = DICTFLAG_BUFFER;
  if      (IsDlgButtonChecked(hwnd,IDC_DSONLY     )) *ptr++ = DICTFLAG_NAMESONLY;
  else if (IsDlgButtonChecked(hwnd,IDC_DSNAMES    )) *ptr++ = DICTFLAG_NAMES;
  if      (IsDlgButtonChecked(hwnd,IDC_DSSEARCH   )) *ptr++ = DICTFLAG_SEARCH;
  if      (IsDlgButtonChecked(hwnd,IDC_DSCLASSICAL)) *ptr++ = DICTFLAG_CLASSICAL;
  if      (IsDlgButtonChecked(hwnd,IDC_DSUSER     )) *ptr++ = DICTFLAG_USER;
  if      (IsDlgButtonChecked(hwnd,IDC_DSQUIET    )) *ptr++ = DICTFLAG_QUIET;
  if      (IsDlgButtonChecked(hwnd,IDC_DSKEEP     )) *ptr++ = DICTFLAG_KEEP;
  *ptr = 0;
  GetDlgItemText (hwnd,IDC_DSNAME,name    ,SIZE_BUFFER);
  GetDlgItemText (hwnd,IDC_DSFILE,filename,SIZE_BUFFER);
  return (new_dictionary(name,relative_name(filename),flags));
}

//--------------------------------
//
//  Utility routine to reduce a name to a relative name if poissible.
//
//      buffer -- Contains full name.
//
//      RETURN -- Pointer to name to use.  If name cannot be made relative, this return
//                value will be the full name.  If this is a relative name, the relative
//                part of the name will be returned.
//
static TCHAR *relative_name (TCHAR *buffer) {
  TCHAR *ptr;
  ptr = jwp_config.name(TEXT(""),OPEN_WRITE,false);
  if (strnicmp(buffer,ptr,lstrlen(ptr))) return (buffer);
  return (buffer+lstrlen(ptr));
}

//-------------------------------------------------------------------
//
//  class JWP_dict routines.
//

//--------------------------------
//
//  Dialog box handler for the Dictionary property for add and edit dictionary.
//
int JWP_dict::dlg_editdict (HWND hwnd,int message,WPARAM wParam,LPARAM lParam) {
  int   i,j;
  TCHAR buffer[SIZE_BUFFER];
  switch (message) {
    case WM_INITDIALOG:
         switch (lParam) {
           case EDITDICT_EDIT:
                SetDlgItemText (hwnd,IDC_DSNAME     ,dic->name);
                SetDlgItemText (hwnd,IDC_DSFILE     ,dic->filename);
                CheckDlgButton (hwnd,IDC_DSSEARCH   ,dic->searched);
                CheckDlgButton (hwnd,IDC_DSKEEP     ,dic->keep);
                CheckDlgButton (hwnd,IDC_DSQUIET    ,dic->quiet);
                CheckDlgButton (hwnd,IDC_DSUSER     ,dic->special == DICT_USER);
                CheckDlgButton (hwnd,IDC_DSCLASSICAL,dic->special == DICT_CLASSICAL);
                switch (dic->format) {
                  default:
                  case DICT_EUC:   i = IDC_DSEUC;   break;
                  case DICT_UTF8:  i = IDC_DSUTF8;  break;
                  case DICT_MIXED: i = IDC_DSMIXED; break;
                }
                CheckDlgButton (hwnd,i,true);
                if      (dic->idx)      i = IDC_DSINDEX;
                else if (dic->buffered) i = IDC_DSBUFFER;
                else                    i = IDC_DSMEMORY;
                CheckDlgButton (hwnd,i,true);
                switch (dic->names) {
                  default:
                  case DICT_NAMES:     i = IDC_DSNAMES;   break;
                  case DICT_NONAMES:   i = IDC_DSNONAMES; break;
                  case DICT_NAMESONLY: i = IDC_DSONLY;    break;
                }
                CheckDlgButton (hwnd,i,true);
                if (dic->special == DICT_USER) {
                  EnableWindow (GetDlgItem(hwnd,IDC_DSEUC      ),false);
                  EnableWindow (GetDlgItem(hwnd,IDC_DSUTF8     ),false);
                  EnableWindow (GetDlgItem(hwnd,IDC_DSMIXED    ),false);
                  EnableWindow (GetDlgItem(hwnd,IDC_DSKEEP     ),false);
                  EnableWindow (GetDlgItem(hwnd,IDC_DSINDEX    ),false);
                  EnableWindow (GetDlgItem(hwnd,IDC_DSBUFFER   ),false);
                  EnableWindow (GetDlgItem(hwnd,IDC_DSMEMORY   ),false);
                  EnableWindow (GetDlgItem(hwnd,IDC_DSCLASSICAL),false);
                }
                dic->get_firstline (hwnd,false);
                delete dic;
                break;
           case EDITDICT_DROP:
           case EDITDICT_NAME:
                SetDlgItemText (hwnd,IDC_DSFILE,relative_name(drop));
                CheckDlgButton (hwnd,IDC_DSNAMES  ,true);
                CheckDlgButton (hwnd,IDC_DSONLY   ,false);
                CheckDlgButton (hwnd,IDC_DSNONAMES,false);
                dic = get_dictionary(hwnd);
                dic->get_info       (hwnd);
                dic->get_firstline  (hwnd,true);
                delete dic;
#if (!defined(WINCE) && defined(DROP_AUTOADD))
                if (lParam == EDITDICT_DROP) PostMessage(hwnd,WM_COMMAND,IDOK,0);     // If an error occures, the dialog will always be created.
#endif (!defined(WINCE) && defined(DROP_AUTOADD))
                break;
           case EDITDICT_ADD:
           default:
                break;
         }
         POCKETPC_DIALOG (hwnd);
         return          (0);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_ADDDICT);
         return  (true);
//
//  Dropfiles
//
#ifndef WINCE
    case WM_DROPFILES:
         if (DragQueryFile((HDROP) wParam,0,buffer,SIZE_BUFFER) > 0) {
           drop = buffer;
           SendMessage (hwnd,WM_INITDIALOG,0,EDITDICT_NAME);
         }
         DragFinish ((HDROP) wParam);
         return (0);
#endif WINCE
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           INPUT_CHECK (IDC_DSFILE);
           INPUT_CHECK (IDC_DSNAME);
           case IDC_DSUPDATE:
                dic = get_dictionary(hwnd);
                dic->get_firstline (hwnd,false);
                delete dic;
                return (0);
           case IDC_DSEUC:
           case IDC_DSUTF8:
           case IDC_DSMIXED:
                dic = get_dictionary(hwnd);
                dic->get_firstline (hwnd,false);
                delete dic;
                return (0);
           case IDC_DSAUTO:
                dic = get_dictionary(hwnd);
                dic->get_info      (hwnd);
                dic->get_firstline (hwnd,true);
                CheckDlgButton     (hwnd,IDC_DSNAMES  ,true);
                CheckDlgButton     (hwnd,IDC_DSONLY   ,false);
                CheckDlgButton     (hwnd,IDC_DSNONAMES,false);
                return (0);
//
//  Get file name from requester
//

           case IDC_DSBROWSE:
                OPENFILENAME  ofn;
                memset (&ofn,0,sizeof(ofn));
                dic = get_dictionary(hwnd);
                if (!dic->name[0]) buffer[0] = 0; else lstrcpy (buffer,dic->get_name());
                delete dic;
                ofn.lStructSize     = sizeof(ofn);
                ofn.hwndOwner       = hwnd;
                ofn.hInstance       = instance;
                ofn.lpstrFilter     = tab_string(IDS_DS_FILETYPE);  // Never could get it to do non-extension files.
                ofn.nFilterIndex    = 1;
                ofn.lpstrFile       = buffer;
                ofn.nMaxFile        = SIZE_BUFFER;
                ofn.Flags           = OFN_FILEMUSTEXIST  | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY  | OFN_EXPLORER;
#ifdef WINCE
                ofn.lpstrInitialDir = currentdir;                   // Use Windows CE current directory
#endif WINCE
                if (!GetOpenFileName(&ofn)) return (true);          // User canclled!
#ifdef WINCE
                set_currentdir (buffer,true);                       // Set Windows CE current directory
#endif WINCE
//
//  Setup dialog box with a particular file in mind.
//
                drop = buffer;
                SendMessage (hwnd,WM_INITDIALOG,0,EDITDICT_NAME);
                return (0);
           case IDOK:
           case IDSEARCH:
                dic = get_dictionary(hwnd);
//
//  Make sure we have a valid entry.
//
                i = dic->name     ? lstrlen(dic->name)     : 0;
                j = dic->filename ? lstrlen(dic->filename) : 0;
                if (!i || !j) {
                  JMessageBox (hwnd,IDS_DS_ERROREMPTY,IDS_DS_ERROR,MB_OK | MB_ICONWARNING);
                  SetFocus (GetDlgItem(hwnd,i ? IDC_DSFILE : IDC_DSNAME));
                  return (0);
                }
//
//  Check to see if file exits.  We will allow instalation of dictionaries
//  that don't exist, but we want to warn about it.
//
                index_name (dic->get_name(),buffer);
                if (!FileExists(dic->get_name()) || (dic->idx && !FileExists(buffer))) {
                  if (IDNO == JMessageBox(hwnd,IDS_DS_DOESNOTEXIST,IDS_AREYOUSURE,MB_YESNO | MB_ICONWARNING,dic->get_name())) return (0);
                }
                EndDialog (hwnd,true);
                return    (0);
           case IDCANCEL:
                EndDialog (hwnd,false);
                return    (0);
         }
         break;
  }
  return (0);
}

//
//  End of routines for processing the Dictionary properties dialog
//
//===================================================================

//===================================================================
//
//  Routines for managing the dictionary list.
//
//  These routines handle editing and chaning the dictionaries list.
//

//-------------------------------------------------------------------
//
//  Static routines..
//

static void        check_buttons (HWND hwnd);                           // Check activation state of dialog buttons.
static int         find_selected (HWND list);                           // Find selected item in list.
static Dictionary *get_dict      (HWND list,int index);                 // Get information from list as a Dictionary class object.
static void        insert_column (HWND list,int col,int id,int width);  // Insert a column into the list.

static int dialog_dictionaries   (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
static int dialog_editdictionary (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);

//--------------------------------
//
//  This routine changes the state of the buttons depending on what is sleected.
//
//      hwnd -- Pointer to the dialog box, not the list.
//
static void check_buttons (HWND hwnd) {
  TCHAR buffer[40];
  HWND  list;
  int   i,j;
  list = GetDlgItem(hwnd,IDC_DCLIST);
  i    = ListView_GetItemCount(list);
  j    = find_selected(list);
  ListView_GetItemText (list,j,COLUMN_SPECIAL,buffer,40);
  EnableWindow (GetDlgItem(hwnd,IDC_EDITLISTUP    ),j);
  EnableWindow (GetDlgItem(hwnd,IDC_EDITLISTDOWN  ),j < i-1);
  EnableWindow (GetDlgItem(hwnd,IDC_EDITLISTDELETE),lstrcmp(buffer,get_string(IDS_DC_USER)));
  return;
}

//--------------------------------
//
//  Stub procedure for calling ditionaries dialog box procedue
//
static int dialog_dictionaries (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_dict.dlg_dictionaries (hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  Dialog box stub for the main dialog, simply calles the class rotuine.
//
static int dialog_editdictionary (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_dict.dlg_editdict(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  The list has only a single selected item.  This routine returns the index of the stelected item.
//
//      list   -- Pointer to the list.
//
//      RETURN -- Index of selected item or zero.  Zero is the first item in the lsit.
//
static int find_selected (HWND list) {
  int i,j;
  i = ListView_GetItemCount(list);
  for (j = 0; j < i; j++) {
    if (ListView_GetItemState(list,j,LVIS_SELECTED)) return (j);
  }
  return (0);
}

//--------------------------------
//
//  Convert a list entry into the Dictionary class object.
//
//      list   -- Pointer to the list.
//      index  -- Item index to examine (zero based).
//
//      RETURN -- Poitner to a allocated Dictionary class object.  Make sure to 
//                delete this object at some point.
//
static Dictionary *get_dict (HWND list,int index) {
  TCHAR *ptr,flags[20],buf1[SIZE_BUFFER],buf2[SIZE_BUFFER];
  ptr  = flags;
  if (ListView_GetCheckState(list,index)) *ptr++ = DICTFLAG_SEARCH;
  ListView_GetItemText (list,index,COLUMN_FORMAT,buf1,SIZE_BUFFER);
  if      (!lstrcmp(buf1,get_string(IDS_DC_FORMATEUC ))) *ptr++ = DICTFLAG_EUC;
  else if (!lstrcmp(buf1,get_string(IDS_DC_FORMATUTF8))) *ptr++ = DICTFLAG_UTF8;
  else                                                   *ptr++ = DICTFLAG_MIXED;
  ListView_GetItemText (list,index,COLUMN_NAMES,buf1,SIZE_BUFFER);
  if      (!lstrcmp(buf1,get_string(IDS_DC_NAMES     ))) *ptr++ = DICTFLAG_NAMES;
  else if (!lstrcmp(buf1,get_string(IDS_DC_NAMESONLY ))) *ptr++ = DICTFLAG_NAMESONLY;
  ListView_GetItemText (list,index,COLUMN_SPECIAL,buf1,SIZE_BUFFER);
  if      (!lstrcmp(buf1,get_string(IDS_DC_USER      ))) *ptr++ = DICTFLAG_USER;
  else if (!lstrcmp(buf1,get_string(IDS_DC_CLASSICAL ))) *ptr++ = DICTFLAG_CLASSICAL;
  ListView_GetItemText (list,index,COLUMN_TYPE,buf1,SIZE_BUFFER);
  if      (!lstrcmp(buf1,get_string(IDS_DC_TYPEIDX   ))) *ptr++ = DICTFLAG_INDEX;
  else if (!lstrcmp(buf1,get_string(IDS_DC_TYPEBUF   ))) *ptr++ = DICTFLAG_BUFFER;
  ListView_GetItemText (list,index,COLUMN_KEEP,buf1,SIZE_BUFFER);
  if      (!lstrcmp(buf1,get_string(IDS_DC_YES       ))) *ptr++ = DICTFLAG_KEEP;
  ListView_GetItemText (list,index,COLUMN_QUIET,buf1,SIZE_BUFFER);
  if      (!lstrcmp(buf1,get_string(IDS_DC_YES       ))) *ptr++ = DICTFLAG_QUIET;
  *ptr = 0;
  ListView_GetItemText (list,index,COLUMN_NAME,buf1,SIZE_BUFFER);
  ListView_GetItemText (list,index,COLUMN_FILE,buf2,SIZE_BUFFER);
  return (new_dictionary(buf1,buf2,flags));
}

//--------------------------------
//  
//  Insert a column into the list.
//
//      list  -- Pointer to list.
//      col   -- Column number.
//      id    -- String table ID for the text label.
//      width -- Width of column.  A value of zero will calculate the width based on the width 
//               of the column name.
//
static void insert_column (HWND list,int col,int id,int width) {
  LVCOLUMN column;
  HDC      dc;
  SIZE     size;
  TCHAR   *name = get_string(id);
//
//  Find size of column if not given.
//
  if (!width) {
    dc = GetDC(list);
    GetTextExtentPoint32 (dc,name,lstrlen(name),&size);
    width = size.cx;
    GetTextExtentPoint32 (dc,TEXT("  "),2,&size);
    width += size.cx;
    ReleaseDC (list,dc);
  }
//
//  Generate the column.
//
  column.mask    = LVCF_TEXT | LVCF_WIDTH;
  column.cx      = width;
  column.pszText = name;
  ListView_InsertColumn (list,col,&column);
  return;
}

//-------------------------------------------------------------------
//
//  class JWP_dict routines.
//

//--------------------------------
//
//  Dialog box handler for the dictionaries dialog box.
//
//
#define  ListView_SetSelected(list,i)   ListView_SetItemState (list,i,LVIS_SELECTED,LVIS_SELECTED)      // Set selected (current) item

int JWP_dict::dlg_dictionaries (HWND hwnd,int message,WPARAM wParam,LPARAM lParam) {
  int         i,j;
  HWND        list;
  RECT        rect;
  Dictionary *base;
  TCHAR       buffer[SIZE_BUFFER];
  switch (message) {
    case WM_INITDIALOG:
         list = GetDlgItem(hwnd,IDC_DCLIST);
         GetClientRect (list,&rect);
         insert_column (list,COLUMN_NAME   ,IDS_DC_COLUMNNAME   ,(rect.right-rect.left)/3);
         insert_column (list,COLUMN_FORMAT ,IDS_DC_COLUMNFORMAT ,0);
         insert_column (list,COLUMN_TYPE   ,IDS_DC_COLUMNTYPE   ,0);
         insert_column (list,COLUMN_NAMES  ,IDS_DC_COLUMNNAMES  ,0);
         insert_column (list,COLUMN_SPECIAL,IDS_DC_COLUMNSPECIAL,0);
         insert_column (list,COLUMN_KEEP   ,IDS_DC_COLUMNKEEP   ,0);
         insert_column (list,COLUMN_QUIET  ,IDS_DC_COLUMNQUIET  ,0);
         insert_column (list,COLUMN_FILE   ,IDS_DC_COLUMNFILE   ,(rect.right-rect.left));
         ListView_SetExtendedListViewStyle (list,LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
         for (i = 0, dic = dicts; dic; dic = dic->next) dic->addlist (list,i++);
         ListView_SetSelected (list,0);
         SetFocus             (list);
         return               (false);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_DICTIONARIES);
         return  (true);
//
//  Process drag and drop.
//
#ifndef WINCE
    case WM_DROPFILES:
         list = GetDlgItem(hwnd,IDC_DCLIST);
         j    = ListView_GetItemCount(list);
         for (i = 0; DragQueryFile((HDROP) wParam,i,buffer,SIZE_BUFFER) > 0; i++) {
            drop = buffer;
            dic  = NULL;
            if (JDialogBox(IDD_DICTEDITDICTIONARIES,hwnd,(DLGPROC) dialog_editdictionary,EDITDICT_DROP)) {
               dic->addlist         (list,j  );       
               ListView_SetSelected (list,j++);
               delete dic;
            }
         }
         DragFinish ((HDROP) wParam);
         return (0);
#endif WINCE
//
//  Notify messages (only care from the list control.
//
    case WM_NOTIFY:
         if (wParam != IDC_DCLIST) break;
         switch (((NMHDR *) lParam)->code) {
           case NM_DBLCLK:                      // Double click -> edit
                goto EditItem;
                break;
           case LVN_DELETEITEM:
           case LVN_ITEMACTIVATE:
           case LVN_DELETEALLITEMS:
           case LVN_INSERTITEM:
           case LVN_ITEMCHANGED:
                check_buttons (hwnd);
                break;
           case LVN_KEYDOWN:
                switch (((NMLVKEYDOWN *) lParam)->wVKey) {
                  case VK_DELETE:               // Delete item.
                  case VK_BACK:
                       goto DeleteItem;
                  case VK_RETURN:               // Edit item
                       goto EditItem;
                  case VK_SPACE:                // Toggle search state
                       list = GetDlgItem(hwnd,IDC_DCLIST);
                       i    = find_selected(list);                
                       ListView_SetItemState (list,i,ListView_GetCheckState(list,i) ? 2<<12 : 1<<12,LVIS_STATEIMAGEMASK);
                       break;
                  default:
                       break;
                }
                break;
           default:
                break;
         }
//         check_buttons (hwnd);
         return (0);
//
//  Command messages.
//
    case WM_COMMAND:
         switch (LOWORD(wParam)) {              
           case IDC_DCDEFAULT:                      // Reinitialize list with default list.
                i    = 0;
                base = NULL;
                list = GetDlgItem(hwnd,IDC_DCLIST);
                ListView_DeleteAllItems (list);
                def_dictionaries (base);
                while (base) {
                  dic  = base;
                  base = dic->next;
                  dic->addlist (list,i++);
                  delete dic;
                }
                ListView_SetSelected (list,0);
                return (0);
           case IDC_EDITLISTUP:                     // Move item up.
                list = GetDlgItem(hwnd,IDC_DCLIST);
                i    = find_selected(list);                
                dic  = get_dict(list,i);
                ListView_DeleteItem  (list,i);
                dic->addlist         (list,i-1);
                ListView_SetSelected (list,i-1);
                delete dic;
                return (0);
           case IDC_EDITLISTDOWN:                   // Move item down
                list = GetDlgItem(hwnd,IDC_DCLIST);
                i    = find_selected(list);                
                dic  = get_dict(list,i);
                ListView_DeleteItem  (list,i);
                dic->addlist         (list,i+1);
                ListView_SetSelected (list,i+1);
                delete dic;
                return (0);
           case IDC_EDITLISTDELETE:                 // Delete item.
DeleteItem:;
                list = GetDlgItem(hwnd,IDC_DCLIST);
                i    = ListView_GetItemCount(list);
                j    = find_selected(list);
                ListView_GetItemText (list,j,COLUMN_SPECIAL,buffer,SIZE_BUFFER);
                if (!lstrcmp(buffer,get_string(IDS_DC_USER))) return (0);         // Don't allow the user dictionary to be deleted.
                ListView_DeleteItem  (list,j);
                if (j+1 == i) j--;
                ListView_SetSelected (list,j);
                return (0);
           case IDC_EDITLISTADD:                    // Add item
                list = GetDlgItem(hwnd,IDC_DCLIST);
                i    = find_selected(list);                
                dic  = NULL;
                drop = NULL;
                if (JDialogBox(IDD_DICTEDITDICTIONARIES,hwnd,(DLGPROC) dialog_editdictionary,EDITDICT_ADD)) {
                  dic->addlist         (list,i+1);       
                  ListView_SetSelected (list,i+1);
                  delete dic;
                }
                return (0);
           case IDC_EDITLISTEDIT:                   // Edit Item.
EditItem:;
                list = GetDlgItem(hwnd,IDC_DCLIST);
                i    = find_selected(list);                
                dic  = get_dict(list,i);
                drop = NULL;
                if (JDialogBox(IDD_DICTEDITDICTIONARIES,hwnd,(DLGPROC) dialog_editdictionary,EDITDICT_EDIT)) {
                  ListView_DeleteItem  (list,i);
                  dic->addlist         (list,i);       
                  ListView_SetSelected (list,i);
                  delete dic;
                }
                return (0);
//
//  User wants to keep the list so output to a buffer.
//
           case IDOK:                                   // User wants to keep changes.
//
//  Close user dictionary if necessary
//
                if (user_dialog) DestroyWindow (user_dialog);
//
//  Save dictionaries
//
                free_dictionary ();                     // Delete the old list.
                list = GetDlgItem(hwnd,IDC_DCLIST);
                i    = ListView_GetItemCount(list);
                for (j = i-1; j >= 0; j--) {            // Read the list out backwards to make the linking easier.
                  dic       = get_dict(list,j);
                  dic->next = dicts;
                  dicts     = dic;
                }
                find_user ();                           // Activate user dictionary.
                if (write_dictionaries()) {             // Save confinguration.
                  error  (IDS_DS_ERROROPEN,jwp_config.name());
                  return (0);
                }
                EndDialog (hwnd,true);
//
//  Cancel so don't save the list.
//
           case IDCANCEL:
                EndDialog (hwnd,false);
                return (0);
         }
         break;
  }
  return (false);
}

//--------------------------------
//
//  This utility routine finds the user dictionary and activates it.
//
//  This is necessary since we allow the user to change the location of the user
//  directory.  We always need a valid user dictionary pointer so we can allow the 
//  user to open the user dictionary at any time.
//
void JWP_dict::find_user () {
  Dictionary *dic;
  for (dic = dicts; dic && (dic->special != DICT_USER); dic = dic->next);
  user = dic;                             // Make sure we know where the user dictionary is.
  user->close ();
  user->open  ();
  return;
}

//
//  Edit dictionaries edit box.
//
//===================================================================

//===================================================================
//
//  begin class JWP_dict
//  This class implements the dictionary search.
//

JWP_dict jwp_dict;          // Class instance.

//--------------------------------
//
//  ID's for the maind dictionary dialog
//
//      IDC_DDSTRING   Search string (edit-control)
//      IDC_DDRESULT   Results (list-box)
//      IDC_DDBEGIN    Match only at beiginning (check-box).
//      IDC_DDEND      Match only at end (check-box)
//      IDC_DDNONAME   No names (check-box)
//      IDC_DDADVANCED Activates advanced searching (check-box)
//      IDC_DDSEARCH   Do search (button)
//      IDC_DDINSERT   Insert into file (button)
//      IDC_DDUSER     Edit user dictionary.
//      IDC_DDOPTIONS  Options (button)
//      IDC_DDDONE     Done (button)
//      IDC_DDSTATUS   Status (text-message).
//

//--------------------------------
//
//  Small utility routine used to check for a user abort when doing a search.
//  This routine will process all messages in the que and then check to see if the
//  state flag is set to abort.
//
//      RETURN -- A non-zero value indicates the user wants to abort the search.
//
int JWP_dict::check_abort () {
  MSG  msg; 
  while (PeekMessage(&msg,null,0,0,PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (state == DICTSTATE_ABORT);
}

//--------------------------------
//
//  This routine checks for primary (priority) entries and moves them to the beginning of the results list.
//
//  I wrote two checks for determining if this is a primary entry.  The FAST_PRIMARY check requries that
//  the primary flag be the last thing on the line.  This basically looks for the \n, and then looks backwards
//  for the (P).  The actuall pattern used in EDICT 15JUN01 V01-001 is "/(P)/\n".  The other routine looks for
//  the (P) anywhere in the meaning part of the entry.  This is slower.  For the moment we can use the fast
//  search.
//
//      ptr -- Pointer to dictionary entry line.
//
#define FAST_PRIMARY

void JWP_dict::check_primary (byte *ptr) {
  if (!jwp_config.cfg.dict_primaryfirst) return;
//
//  Does this have a (P) marker.
//
#ifdef FAST_PRIMARY
  while (*ptr != '\n') ptr++;
  if ((ptr[-4] != '(') || (ptr[-3] != 'P') || (ptr[-2] != ')')) return;
#else  FAST_PRIMARY
  while (*ptr != '/') ptr++;
  for (ptr++; (*ptr != '\n') && strncmp((char *) ptr,"(P)",3); ptr++);
  if (*ptr == '\n') return;
#endif FAST_PRIMARY
//
//  Yes, so move entry to beginning part of the list.
//
  int i;    
  i = list->begin(list->count-1);                           // Find beginning index for last entry.
  if (i != primary) list->move_block (i,primary);           // Don't do move it it would not change the location.
  primary = list->next(primary);                            // Corect primary pointer.
  list->move (0,false);                                     // Set currsor back to index 0.
  return;
}

//--------------------------------
//
//  This is the dialog box procedure for the main dictionary dialog.
//  This is where all commands are interpreted.
//
#if   (!defined(WINCE))                             // Define the range for dynamic controls
  #define IDC_DICT_FIRST    IDC_DDCLASSICAL
  #define IDC_DICT_LAST     IDC_DDJASCII
#elif (!(defined(WINCE_PPC) || defined(WINCE_POCKETPC)))
  #define IDC_DICT_FIRST    IDC_DDCLASSICAL
  #define IDC_DICT_LAST     IDC_DDPRIORITY
#else
  #define IDC_DICT_FIRST    0
  #define IDC_DICT_LAST     0
#endif

int JWP_dict::dlg_dictionary (HWND hwnd,int msg,WPARAM wParam,LPARAM lParam) {
  int i;
  static byte enable_clip;          // This is used to prevent errors when using the clipboard
                                    //   tracking.  This suppresses clipboard searching until
                                    //   after the first search command is received.
#ifndef WINCE
  static HWND clipview;             // Previous clipboard viewer.
#endif WINCE

  switch (msg) {
//
//  Intialize the dialog.  Intialize some varialbes, and setup the 
//  controls.
//
    case WM_INITDIALOG: 
         dict_size.wm_init (hwnd,IDC_DDRESULT,&jwp_config.cfg.size_dict,false,IDC_DICT_FIRST,IDC_DICT_LAST);
         active = true;
         add_dialog (hwnd,true);
         for (i = 0; i < NUMBER_DICTKEYS; i++) dict_keys[i].reject = ((jwp_config.cfg.dict_bits & (0x1L << i)) != 0);
         dialog  = hwnd;
         if (!dicts) load ();                                           
         initialize     (GetDlgItem(hwnd,IDC_DDRESULT));                                                        // Intialize the base class EUC_buffer.
         set_checkboxes ();
         wParam = SendDlgItemMessage (hwnd,IDC_DDSTRING,JE_LOAD,0,(LPARAM) jwp_file);
         SendDlgItemMessage (hwnd,IDC_DDSTRING,JE_SETHIST,IDC_DDHISTORY,(LPARAM) &dict_history);
         SendDlgItemMessage (hwnd,IDC_DDRESULT,JL_SETEXCLUDE,0,SendDlgItemMessage(hwnd,IDC_DDSTRING,JE_GETJWPFILE,0,0));
         if (wParam && jwp_config.cfg.dict_auto) PostMessage (hwnd,WM_COMMAND,IDSEARCH,0);
//
//  Setup the clipboard tracking
//
#ifndef WINCE
         enable_clip = false;                           // Suppress first call to clipboard, which is a rsult of us being added to the viewer list.
         clipview    = SetClipboardViewer (hwnd);       // Setup clipboard tracking
#endif WINCE
         POCKETPC_DIALOG (hwnd);
         return          (true);
//
//  Save the state of the dictonary keys.
//
    case WM_DESTROY:
         active = false;
         get_checkboxes ();
         jwp_config.cfg.dict_bits = 0;
         for (i = 0; i < NUMBER_DICTKEYS; i++) {
           if (dict_keys[i].reject) jwp_config.cfg.dict_bits |= (0x1L << i);
         }
#ifndef WINCE
         ChangeClipboardChain (hwnd,clipview);          // Terminate clipboard tracking
#endif WINCE
         remove_dialog (hwnd);
         return (0);
//
//  Size change.
//
#ifndef WINCE
    case WM_SIZING:
         dict_size.wm_sizing ((RECT *) lParam);
         return (0);
#endif  WINCE
    case WM_SIZE:
         dict_size.wm_size (wParam);
         return (0);
    case WM_MOVE:
         dict_size.wm_move ();
         return (0);
//
//  Clipboard tracking rotuines
//
#ifndef WINCE
    case WM_DRAWCLIPBOARD:
         if (clipview) SendMessage (clipview,msg,wParam,lParam);
         if (jwp_config.cfg.dict_watchclip && enable_clip && !jwp_clipboard) {
           JWP_file *string;
           string = (JWP_file *) SendDlgItemMessage(hwnd,IDC_DDSTRING,JE_GETJWPFILE,0,0);
           if (string->edit_clip()) {
             clipsearch = true;                         // Flag this as a clipboard search (disables some errors).
             PostMessage (hwnd,WM_COMMAND,IDSEARCH,0);
           }
         }
         enable_clip = true;                            // The first call to this routine is because we added to the chain.
         return (0);
    case WM_CHANGECBCHAIN:
         if (clipview == (HWND) wParam) clipview = (HWND) lParam; else SendMessage (clipview,msg,wParam,lParam);
         return (0);
#endif WINCE
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_GENERAL);
         return  (true);
//
//  This indicate the user is typing into the list box window.
//
    case WMU_EDITFROMLIST:          // Basic edit commands such as moving the cursor
         SetFocus           (GetDlgItem(hwnd,IDC_DDSTRING));
         SendDlgItemMessage (hwnd,IDC_DDSTRING,WM_KEYDOWN,wParam,lParam);
         return (0);
    case WMU_CHARFROMLIST:          // Basic character
         SetFocus           (GetDlgItem(hwnd,IDC_DDSTRING));
         SendDlgItemMessage (hwnd,IDC_DDSTRING,WM_CHAR,wParam,lParam);
         return (0);
#ifndef WINCE
    case WMU_IMEFROMLIST:           // IME input.
         SendDlgItemMessage (hwnd,IDC_DDSTRING,WM_IME_CHAR,wParam,lParam);
         return (0);
#endif WINCE
//
//  This is the main message control.
//
    case WM_COMMAND:        
         switch (LOWORD(wParam)) { 
//
//  User has intializted a search.
//
#ifndef WINCE_POCKETPC
           case IDOK:           // Search
#endif WINCE_POCKETPC
           case IDSEARCH:
                search_dict ();
                return   (0);
//
//  Sort the results.
//
           case IDC_DDSORT:
                list->sort (dictionary_compare);
                return (0);
//
//  The user has clicked on the Advanced checkbox.
//
           case IDC_DDNONAME:
                dict_keys[DICTKEY_PLACES].reject = dict_keys[DICTKEY_NAMES ].reject = !IsDlgButtonChecked(hwnd,IDC_DDNONAME);
                CheckDlgButton (hwnd,IDC_DDNONAME,dict_keys[DICTKEY_PLACES].reject);
                return (0);
//
//  History button
//
           case IDC_DDHISTORY:
                SendDlgItemMessage (hwnd,IDC_DDSTRING,JE_HISTORYLIST,0,0);
                return (0);
//
//  If user double clicks in the result window, we want to paste the 
//  results back into whoever called us.
//
//  This is the main paste back.
//
           case IDC_DDRESULT:
           case IDC_DDINSERT:
                SendDlgItemMessage (hwnd,IDC_DDRESULT,JL_INSERTTOFILE,0,0);
                return (0);
//
//  User wants to edit the user dictionary.
//
           case IDC_DDUSER:
                user_dictionary (hwnd);
                return (0);
//
//  User wants to edit the options.
//
           case IDC_DDOPTIONS:
                if (is_searching()) return (0);
                JDialogBox (IDD_DICTOPTIONS,hwnd,(DLGPROC) dialog_dictoptions);
                return (0);
//
//  We may be done.  First check to see if we are searching.  If so,
//  abort the sarch, but do not exit.
//    
#ifdef WINCE_POCKETPC
           case IDOK:
#endif WINCE_POCKETPC
           case IDCANCEL:
                if (state) {                            // Check to see if we are search, if so
                  state = DICTSTATE_ABORT;              //   abort search but don't exit.
                  return (1);
                }
                if (user_dialog) {                      // Check to see if the user dictionary is
                  SendMessage(user_dialog,WMU_OKTODESTROY,0,(long) &i); // open.  If so see what to
                  if (!i) return (0);                                   // do about closing/saving/etc.
                }
                DestroyWindow (hwnd);
                return    (0);
         }
  }
  return (false);
}

//--------------------------------
//
//  This is the dialog box handler for the Dictionary Options dialog box.
//
//      hwnd    -- Pointer to our dialog box
//      msg     -- What windows wants.
//      command -- This is the command part of WM_COMMAND messages.  This
//                 has been extracted in the stub routine.
//
int JWP_dict::dlg_dictoptions (HWND hwnd,int msg,int command) {
  int i,j;
  switch (msg) {
    case WM_INITDIALOG:
         get_checkboxes ();
         CheckDlgButton (hwnd,IDC_DOCOMPRESS ,jwp_config.cfg.dict_compress    );
         CheckDlgButton (hwnd,IDC_DOAUTO     ,jwp_config.cfg.dict_auto        );
         CheckDlgButton (hwnd,IDC_DOADVSEARCH,jwp_config.cfg.dict_advanced    );
         CheckDlgButton (hwnd,IDC_DOADVALWAYS,jwp_config.cfg.dict_always      );
         CheckDlgButton (hwnd,IDC_DOADVALL   ,jwp_config.cfg.dict_showall     );
         CheckDlgButton (hwnd,IDC_DOADVI     ,jwp_config.cfg.dict_iadj        );
         CheckDlgButton (hwnd,IDC_DOCLIPBOARD,jwp_config.cfg.dict_watchclip   );
         CheckDlgButton (hwnd,IDC_DOCLASSICAL,jwp_config.cfg.dict_classical   );
         CheckDlgButton (hwnd,IDC_DOPRIMARY  ,jwp_config.cfg.dict_primaryfirst);
         CheckDlgButton (hwnd,IDC_DOFULLASCII,jwp_config.cfg.dict_fullascii   );
         CheckDlgButton (hwnd,IDC_DOJASCII   ,jwp_config.cfg.dict_jascii2ascii);
         CheckDlgButton (hwnd,IDC_DOMARK     ,jwp_config.cfg.dict_advmark     );
         for (i = 0; i < NUMBER_DICTKEYS; i++) {
           SendDlgItemMessage (hwnd,IDC_DOEXCLUDE,LB_ADDSTRING,0,(long) get_string(dict_keys[i].text));           
           SendDlgItemMessage (hwnd,IDC_DOEXCLUDE,LB_SETSEL,dict_keys[i].reject,i);
         }
         SendDlgItemMessage (hwnd,IDC_DOEXCLUDE,LB_SETTOPINDEX,0,0);   // Need to set this or windows scrolls the list-box
         for (i = IDC_DOADVI; i <= IDC_DOMARK; i++) EnableWindow(GetDlgItem(hwnd,i),jwp_config.cfg.dict_advanced);
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_DICT_OPTIONS);
         return  (true);
    case WM_COMMAND:
         switch (command) {
           case IDC_DOADVSEARCH:
                j = IsDlgButtonChecked(hwnd,IDC_DOADVSEARCH);
                for (i = IDC_DOADVI; i <= IDC_DOMARK; i++) EnableWindow(GetDlgItem(hwnd,i),j);
                return (0);
           case IDC_DODICTIONARIES:
                JDialogBox (IDD_DICTIONARIES,hwnd,(DLGPROC) dialog_dictionaries); 
                return (0);
           case IDC_DOUSERDICT:
                user_dictionary (hwnd);
                return (0);
           case IDOK:
                jwp_config.cfg.dict_compress     = IsDlgButtonChecked(hwnd,IDC_DOCOMPRESS );
                jwp_config.cfg.dict_auto         = IsDlgButtonChecked(hwnd,IDC_DOAUTO     );
                jwp_config.cfg.dict_advanced     = IsDlgButtonChecked(hwnd,IDC_DOADVSEARCH);
                jwp_config.cfg.dict_always       = IsDlgButtonChecked(hwnd,IDC_DOADVALWAYS);
                jwp_config.cfg.dict_showall      = IsDlgButtonChecked(hwnd,IDC_DOADVALL   );
                jwp_config.cfg.dict_iadj         = IsDlgButtonChecked(hwnd,IDC_DOADVI     );
                jwp_config.cfg.dict_watchclip    = IsDlgButtonChecked(hwnd,IDC_DOCLIPBOARD);
                jwp_config.cfg.dict_classical    = IsDlgButtonChecked(hwnd,IDC_DOCLASSICAL);
                jwp_config.cfg.dict_primaryfirst = IsDlgButtonChecked(hwnd,IDC_DOPRIMARY  );
                jwp_config.cfg.dict_fullascii    = IsDlgButtonChecked(hwnd,IDC_DOFULLASCII);
                jwp_config.cfg.dict_jascii2ascii = IsDlgButtonChecked(hwnd,IDC_DOJASCII   );
                jwp_config.cfg.dict_advmark      = IsDlgButtonChecked(hwnd,IDC_DOMARK     );
                for (i = 0; i < NUMBER_DICTKEYS; i++) {
                  dict_keys[i].reject = (byte) SendDlgItemMessage(hwnd,IDC_DOEXCLUDE,LB_GETSEL,i,0);
                }
                set_checkboxes ();
           case IDCANCEL:   // *** FALL THROUGH ***
                EndDialog (hwnd,true);                  // Return value does not matter, because we don't use it.
                return (0);
         }
  }
  return (false);
}

//--------------------------------
//
//  This routine actually does a search.  The argumetns inciate what 
//  to be searched for.  In a simple search, this will be called once,
//  For an advanced search, this can be called many different times as
//  the search arguments is modified and many different endings are 
//  added.
//
//      RETURN -- Indicates the search status:
//
//          DICTSEARCH_MIXED -- Search contains ascii and kana.
//          DICTSEARCH_SHORT -- Too few characters to search for.
//          DICTSEARCH_OKAY  -- Search is OK (this does not mean any
//                              matches were found).
//
int JWP_dict::do_search () {
  int i,ascii,err,kanji,kana;
//
//  Get the EUC length of the search string.  We use this to find the end of
//  the matches.  For UTF data, we will let the search routine return the length.
//
  ascii = kana = kanji = euc_length = 0;
  for (i = 0; i < search_len; i++) {
    if (ISASCII(search_ptr[i])) {
      ascii++;
      euc_length++;
    }
    else {
      if (ISKANA(search_ptr[i])) kana++; else kanji++;
      euc_length += 2;
    }
  }
//
//  Check key for validity.
//
  if (ascii && (kana || kanji)) return (DICTSEARCH_MIXED);
  if ((ascii && (ascii < 3)) || (!ascii && !kanji && (kana < 2))) return (DICTSEARCH_SHORT);
//
//  Check for classical particles and jodoushi.
//
  classical_part = jwp_config.cfg.dict_classical && (search_ptr[0] == KANJI_LONGVOWEL);
//
//  This is the actuall search block.  This will search all dicitonaries
//  that are appropriate.
//
  for (dict = dicts; dict; dict = dict->next) {
//
//  We are not searching this dictionary.
//
    if (!dict->searched) continue;                                                          // Not searched.
    if ((dict->names == DICT_NAMESONLY) && nonames) continue;                               // Name only dictionary, but no names.
    if (!jwp_config.cfg.dict_classical && (dict->special == DICT_CLASSICAL)) continue;      // Classical dictionaries.
//
//  Setup classical and highligh flags.
//
    highlight (dict->special);
//
//  Actual searches
//
    if (err = dict->open()) {
      if (dict->quiet) continue;
      error (err,dict->get_name());
      continue;
    }
    dict->search (search_ptr,search_len);
    if (!dict->keep) dict->close();
    if (state == DICTSTATE_ABORT) return (DICTSEARCH_ABORT);
  }
//
//  Done!
//
  return (DICTSEARCH_OKAY);
}

//--------------------------------
//
//  Generate an error message.
//
//      error  -- Set non-zero for an error and zero for a warning.
//      format -- ID for a printf style format string.
//      ...    -- Arguments for printf string.
//
void JWP_dict::error (int format,...) {
  TCHAR buffer[SIZE_WORKING],string[SIZE_WORKING];
  va_list argptr;
  va_start   (argptr,format);
  GET_STRING (string,format);
  wvsprintf  (buffer,string,argptr);
  MessageBox (dialog,buffer,get_string(IDS_DS_ERROR),MB_OK | MB_ICONWARNING);
  return;
}

//--------------------------------
//
//  This routine is called when an entry in the dictionary has been 
//  found.  This rotuine will check the entry against the filters, 
//  and if the entry passess all filters, the entry will be put into
//  the list.
//
//      ptr    -- Pointer to the actual entry location in the dictionary
//                buffer.
//
void JWP_dict::euc_check (byte *ptr) {
  int   i,length;
  length = euc_length;
//
//  Check for patter postfix
//
  if (pattern) {
    i = euc_post(ptr+length,postfix_ptr,postfix_len);
    if (i == -1) {
      rejected++;
      message (NULL);
      return;
    }
    length += i;
    i = euc_pre(ptr-1,prefix_ptr,prefix_len);
    if (i == -1) {
      rejected++;
      message (NULL);
      return;
    }
    ptr    -= i;
    length += i;
  }
//
//  Check entry for valid begin/end requirements.
//
  if (euc_endbegin(ptr-1,ptr+length)) {
    rejected++;                                 // Check for match at beginning and ending of the search.
    message (NULL); 
    return;
  }
//
//  At this point, we have a an accepted entry, that matches the user's
//  beginning/ending of the line conditions.  
//
//  Now we begin to format the line, and have to check for excluded 
//  entry times.
//
  for ( ; !ISCRLF(*ptr); ptr--);                // Backup in the buffer to the beginning of the line.
  ptr++;
//
//  Filter out entries based on types of entries.
//
  if (filter_entry(ptr)) return;
//
//  Hooray! We have an entry that we actually want to keep.  This means 
//  we have to format the line and generate the output.  Most of the 
//  work in generating the output string is done by the class EUC_buffer.
//  (see top of file for class information).
//                                  
  matches++;                                    // Change count and display
  message       (NULL);                         // Change count
  format_line   (this,ptr,false,dict->format);  // Output line.
  check_primary (ptr);                          // Process primary entries.
  return;
}

//--------------------------------
//
//  This routine tests the beginning and end of entry contiditons.  This routine is only called once
//  and thus could be placed in the euc_check() routine, but this is easier to visualize.  In previous
//  versions this was one complicated if statement, but it is just too hard to read that way.
//
//      first  -- Pointer to first character.
//      last   -- Pointer to last character.
//
//      RETURN -- A non-zero value indicates this entry should be rejected.
//
#define EUC_CPARTEND(p)     (((p)[0] == ')') || EUC_MATCH(p,EUC_CAMA) || EUC_MATCH(p,EUC_SLASH))

int JWP_dict::euc_endbegin (byte *first,byte *last) {
  if (ascii_search) {
    if (dict_keys[DICTKEY_BEGIN].reject && !((*first == '/') || (!jwp_config.cfg.dict_fullascii && !isalnum(*first)) || ((first[0] == ' ') && (first[-1] == ')')))) return (true);
    if (dict_keys[DICTKEY_END  ].reject && !((*last  == '/') || (!jwp_config.cfg.dict_fullascii && !isalnum(*last )))) return (true);
    return (false);
  }
  if (dict_keys[DICTKEY_BEGIN].reject && !((*first == '[') || ISCRLF(*first) || classical_part)) return (true);
  if (dict_keys[DICTKEY_END  ].reject && !((*last  == ']') || (*last == ' ') || (classical_part && EUC_CPARTEND(last)))) return (true);
  return (false);
}

//--------------------------------
//
//  Check entry to see if it should be rejected because of user settings for filters.
//  The filter codes are stored in ascii, so we can use the same routine for UTF-8, 
//  EUC and Mixed dictionaries without any problem.
//
//      ptr    -- Pointer to dictionary entry.
//
//      RETURN -- A nonzero value idnciates this entry should be filtered (ie rejected).
//
#define NUMBER_DICTNAME ((int) (sizeof(names)/sizeof(char [6])))

int JWP_dict::filter_entry (byte *ptr) {
  byte *p,*p2,*p3;
  int i;
//
//  This next nasty little section of code handles removing entries, 
//  based on the entry type.  This is complicated because each entry 
//  can have multable definitions.  If the user has selected No Personal
//  Names, then we need to remove an entry that is a personal name only
//  in it's entirety.  If, however, the entry is both a place name and
//  a personal name, we need to simply remove the personal name type
//  id from the enrtry.
//
//  Just for review, a dictionary line looks like:
//
//      <kanji> [<kana>] /definition/definition/    ...
//                  or
//      <kana> /definition/definition/  ...
//
//  Within each definition there can be several type identifiers, 
//  placed in paraentheses.  For exapmle: (pl,pn,giv,fem), indicates a 
//  place name, and a female given name.
//
  if (!filter) return (false);                              // No filters so nothing to do.
  static char names[][2] = { "u","g","f","m" };             // These are extensions added to the (personal name) type
                                                            //   generally they follow the pn field.  JWPce does not  
                                                            //   do filtering based on the individual type of name, but
                                                            //   rather on the entire class of names, thus these will be
                                                            //   rejected if pn is rejected.
  int j;                                                    // Used to scan the extended keys.
  int len;                                                  // Length of key we are working with.
  int check = false;                                        // Set to non-zero when the entry is modified, 
                                                            //   will force an evaluation of the entry later.
  p = ptr;                                                  // pointer p will advance through the file.
  while (!ISCRLF(*p)) {                                        
    if (*p != '(') { p++; continue; }                       // We have maybe found a type key, if not off to next charcter.
    while (*p != ')') {                                     // Until we reach the end of the type key we need to keep examining each key.
      for (i=DICTKEY_START; i < NUMBER_DICTKEYS; i++) {     // See if we know what the key is.
        if (len = test_key(p,dict_keys[i].key)) break;
      }
      if (!len) {                                           // Special case, because we do not destinguish by name types
        i = DICTKEY_NAMES;                                  //   we have to reject all name types when the user chooses
        for (j = 0; j < NUMBER_DICTNAME; j++) {             //   to reject names.  Thus we let the standard array take 
          if (len = test_key(p,names[j])) break;            //   care of the first term (s=surname), and then remove all 
        }                                                   //   of the other name terms here.
      }
      if (!len) { p++; break; }                             // This is a cheat.  We dont' reconize the key so we will 
                                                            //   assume that this is just a note, and by exiting the 
                                                            //   inside key loop and advancing the pointer, we will 
                                                            //   actually be in the state of the looking for '(' again.
      if (!dict_keys[i].reject) { p += len; continue; }     // If this is not a rejected key, simply advance to the end of
                                                            //   the key, and continue.  We are still in a key (<key>,<key>).
      if ((*p == '(') && (p[len] == ')')) {                 // We have a key we want to reject, but the key started with '(',
        for (p2 = p+len; *p2 != '/'; p2++);                 //   and ended with ')'.  This means that we really want to 
        while (*p != '/') p--;                              //   reject this entrie definition.  (Note, other keys may 
        p3 = p;                                             //   have been removed to get us here.)  To remove an entire 
        while (!ISCRLF(*p2)) *p3++ = *p2++;                 //   definition, we find the beginning '/' and the ending '/', 
        *p3 = '\n';                                         //   then copy all characters from end to begining.  After this
        check = true;                                       //   we leave the character pointer at where the beginning of this
        break;                                              //   definition was, and exit the in-definiition loop.
      }                                                 
      p2 = p;                                               // Remember we were rejecting this key.  In this case, there are
      if (*p2 == '(') p2++;                                 //   either previous or following keys to be removed, thus we want 
      while (!ISCRLF(p2[len])) { p2[0] = p2[len]; p2++; }   //   to simply remove the key.  We do this by copying from end of key
      p2[0] = '\n';                                         //   to beginning of key.  (Note, we preserve '(' if it was the 
    }                                                       //   beginning of the sequence.)
  }                                                     
  if (check) {                                              // Modifiec entry, so do we still want it.  We count the '/'s.  If 
    for (i = 0, p = ptr; !ISCRLF(*p); p++) if (*p == '/') i++;  //   there are two or more we keep the enry.  One or less we have
    if (i <= 1) { rejected++; message (NULL); return(true); }   //   removed all definitions, so we reject it.
// ### Could add a filetered counter here.
  }
  return (false);
}

//--------------------------------
//
//  Reads the state of the dictionary dialog box's four check-boxes.
//
void JWP_dict::get_checkboxes () {
  dict_keys[DICTKEY_END   ].reject = IsDlgButtonChecked(dialog,IDC_DDEND      );
  dict_keys[DICTKEY_BEGIN ].reject = IsDlgButtonChecked(dialog,IDC_DDBEGIN    );
  jwp_config.cfg.dict_advanced     = IsDlgButtonChecked(dialog,IDC_DDADVANCED );
#if (!(defined(WINCE_PPC) || defined(WINCE_POCKETPC)))
  jwp_config.cfg.dict_classical    = IsDlgButtonChecked(dialog,IDC_DDCLASSICAL);
  jwp_config.cfg.dict_fullascii    = IsDlgButtonChecked(dialog,IDC_DDFULLASCII);
  jwp_config.cfg.dict_primaryfirst = IsDlgButtonChecked(dialog,IDC_DDPRIORITY );
#ifndef WINCE
  jwp_config.cfg.dict_watchclip    = IsDlgButtonChecked(dialog,IDC_DDCLIPBOARD);
  jwp_config.cfg.dict_jascii2ascii = IsDlgButtonChecked(dialog,IDC_DDJASCII   );
  jwp_config.cfg.dict_always       = IsDlgButtonChecked(dialog,IDC_DDADVALWAYS);
  jwp_config.cfg.dict_showall      = IsDlgButtonChecked(dialog,IDC_DDADVALL   );
  jwp_config.cfg.dict_iadj         = IsDlgButtonChecked(dialog,IDC_DDADVI     );
  jwp_config.cfg.dict_advmark      = IsDlgButtonChecked(dialog,IDC_DDMARK     );
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
#endif WINCE
  return;
}

//--------------------------------
//
//  Utility routine to get the character at the end of the search string.  This is used by the 
//  advanced search to manipulate the end of the string.
//
//      RETURN -- Character from the end of the string.
//
int JWP_dict::get_last () {
  if (postfix_len) return (postfix_ptr[postfix_len-1]);
  return (search_ptr[search_len-1]);
}

//--------------------------------
//
//  Simple routine to tell if we are in a search, and make a beep if so.
//
int JWP_dict::is_searching () {
  if (!state) return (false);
  MessageBeep (MB_ICONASTERISK); 
  return (true);  
}

//--------------------------------
//
//  Displays a message in the upper right corner of the dictionary 
//  dialog box.  This message generally intdicates the state of the 
//  current search.
//
//      message -- Pointer to a printf type format string for the 
//                 message.  Passing the value of NULL will cause the 
//                 standard matches/rejected message to be displayed.
//
void JWP_dict::message (tchar *format,...) {
  TCHAR buffer[256];
  va_list argptr;
  va_start (argptr,format);
  if (format) wvsprintf (buffer,format,argptr);
    else format_string(buffer,rejected ? IDS_DD_MATCHREJECT : IDS_DD_MATCH,matches,rejected);
  SetDlgItemText (dialog,IDC_DDSTATUS,buffer);
  return;
}

//--------------------------------
//
//  Utility routine to put a character at the end of the search string.  This is used by the 
//  advanced search to manipulate the end of the string.
//
//      kanji  -- Character to placed at the end of the string.
//
//      RETURN -- A non-zero return indicates the character was not placed and the search would 
//                be unchanged.  This is done as a result of combining patter searched with the
//                advanced search.  The routine will not change the character if there is no 
//                postfix or in the case of a patter character at the end.
//
int JWP_dict::put_last (KANJI kanji) {
  if      (!postfix_len)                          search_ptr[search_len-1] = kanji; 
  else if (ISPATTERN(postfix_ptr[postfix_len-1])) return (true);
  else                                            postfix_ptr[postfix_len-1] = kanji;
  return (false);
}

//--------------------------------
//
//  This is the client entry point into the dictionary system.  This is
//  called from a client, and sets some values, then simply enters the
//  dictionary search dialog box.  The major functions are all handled 
//  through the dialog box procedure.
//
//      file -- Context we are comming from.
//
void JWP_dict::search (JWP_file *file) {
//
//  If the dictionary is active then we want to transfer control to the current
//  dictionary dialog.  If the user is comming form any place other than the 
//  edit box in the dictionary then we want to reload the ditionary and maybe
//  do an automatic search.
//
  if (active) { 
    if (file->window != GetDlgItem(dialog,IDC_DDSTRING)) {
      if (jwp_config.cfg.dict_auto && SendDlgItemMessage(dialog,IDC_DDSTRING,JE_LOAD,0,(LPARAM) file)) {
        SendMessage (dialog,WM_COMMAND,IDSEARCH,0);
      }
    }
    SetForegroundWindow (dialog); 
    return; 
  }
//
//  No dictionary, so generate the dialog box.
//
  jwp_conv.clear (); 
  JCreateDialog (IDD_DICTIONARY,file->window,(DLGPROC) dialog_dictionary);
  return;
}

//--------------------------------
//
//  Searches the dictionary after adding a character to the end of the string.
//
//  This routine is used by the advanced search to check for various endings.
//
//      kanji  -- Kanji to add.
//
//      RETURN -- Indicats the search result.
//
//          DICTSEARCH_MIXED -- Search contains ascii and kana.
//          DICTSEARCH_SHORT -- Too few characters to search for.
//          DICTSEARCH_OKAY  -- Search is OK (this does not mean any
//                              matches were found).
//
int JWP_dict::search_add (KANJI kanji) {
  int abort;
  if (postfix_len) postfix_len++; else search_len++;
  put_last (kanji);
  abort = do_search();
  if (postfix_len) postfix_len--; else search_len--;
  return (abort);
}

//--------------------------------
//
//  Searches the dictionary after changing the end character of the string.
//
//  This routine is used by the advanced search to check for various endings.
//
//      kanji  -- Kanji to add.
//
//      RETURN -- Indicats the search result.
//
//          DICTSEARCH_MIXED -- Search contains ascii and kana.
//          DICTSEARCH_SHORT -- Too few characters to search for.
//          DICTSEARCH_OKAY  -- Search is OK (this does not mean any
//                              matches were found).
//
int JWP_dict::search_end (KANJI kanji) {
  if (put_last(kanji)) return (0);
  return (do_search());
}

//--------------------------------
//
//  This is the main search engine.  This is where you go when you
//  click SEARCH.  This handles getting the parameters from the dialog
//  box.  This routine could have been embedded (and it once was) in 
//  the dialog box handler, however, that is just too hard to deal with,
//  so I sepated it.
//
//  Special macros used to modify the search string then actually do the 
//  search.  These are simply defined to simply the writing of the code.
//  
//      SEARCH_END -- Changes the end character and then does a search.
//      SEARCH_ADD -- Adds a character to the end of the stirng and searches.
//
#define SEARCH_END(x) { if (abort = search_end(x)) break; }
#define SEARCH_ADD(x) { if (abort = search_add(x)) break; }

void JWP_dict::search_dict () {
  int    i,j;
  int    length;                    // Adjusted length of seach string from the user.
  KANJI  search[MAX_KEY_LENGTH+1];  // Copy of the edit-box search string that can be modified.
  int    first = true;              // Indicates first pass in a best fit search.
  int    abort;                     // Indicates the abort condition from the last search.
  char  *ptr;                       // Holds romaji for kana character at end of search string.
//
//  Intitlaize the search parameters.
//
  if (is_searching()) { clipsearch = false; return; }           // Already searching.
  get_checkboxes ();                                            // Get all the settings.
  length = JEGetDlgItemText(dialog,IDC_DDSTRING,&search_ptr);
  if (!length) return;                                          // No string so exit.
  if (length > MAX_KEY_LENGTH) length = MAX_KEY_LENGTH;         // Truncate user string if necessary.
  while (ISSPACE(*search_ptr)) { search_ptr++, length--; }      // Kill leading spaces.
  while (length && ISSPACE(search_ptr[length-1])) length--;     // Kill trailing spaces.
  SendDlgItemMessage (dialog,IDC_DDRESULT,JL_RESET,0,0);        // Clear list.
  primary = 0;                                                  // Set pointer for prinary entries.
//
//  Pre-pocess key.  
//
//  kana -> hiragana.
//  jascii -> ascii;
//  ascii -> lower case.
//
//  Patterns and prefixes are setup at this point.
//
  ascii_search = pattern = false;
//
//  Exception for processing of ????  This does not do a search but gets the ID from the dictionaries.
//
  ascii_search = pattern = false;
  if ((length == 4) && (search_ptr[0] == KANJI_QUESTION) && (search_ptr[1] == KANJI_QUESTION) && (search_ptr[2] == KANJI_QUESTION) && (search_ptr[3] == KANJI_QUESTION)) {
    search[0] = search[1] = search[2] = search[3] = KANJI_QUESTION;
  }
//
//  General processing.
//
  else {
    for (i = 0; i < length; i++) {
      search[i] = search_ptr[i];
      switch (search[i]) {
        case KANJI_LBRACE:
        case '[':
             pattern   = true;
             search[i] = '[';
             break;
        case KANJI_RBRACE:
        case ']':
             pattern   = true;
             search[i] = ']';
             break;
        case KANJI_ASTRIC:
        case '*': 
             pattern   = true;
             search[i] = '*'; 
             break;
        case KANJI_QUESTION:
        case '?': 
             pattern   = true;
             search[i] = '?'; 
             break;
        default:
             if      (ISASCII   (search[i])) { 
               search[i] = tolower(search[i]); 
               ascii_search = true; 
             }
             else if (ISJASCII(search[i]) && jwp_config.cfg.dict_jascii2ascii) {
               search[i] = jascii_to_ascii(search[i]);
               search[i] = tolower(search[i]);
               ascii_search = true;
             }
             else if (ISKATAKANA(search[i])) search[i] = BASE_HIRAGANA | (search[i] & 0xff);
             break;
      }
    }
  }
  search[length] = 0;                               // Fill to prevent some odd errors.
  if (ascii_search && pattern) pattern = false;     // Right now no pattern search in ascii.
//
//  Reconize searches for verbs in english, ie "to swim".  These are treated as a search for
//  swim, with a prefex for "to ", and use the patter search capabilities.
//
  if (ascii_search && (search[0] == 't') && (search[1] == 'o') && (search[2] == ' ')) {
    pattern     = true;
    prefix_ptr  = search;                           // Fixed prefex of "to "
    prefix_len  = 3;
    search_ptr  = prefix_ptr+3;                     // Main key
    search_len  = length-3;
    postfix_ptr = prefix_ptr+length;                // No postfix
    postfix_len = 0;
  }
//
//  Pickup real pattern searches
//
  else if (pattern) {
    for (i = 0; (i < length) && !ISKANJI(search[i]); i++);
    if (i == length) {
      error (IDS_DD_NOKANJI);
      state = DICTSTATE_IDLE;
      return;
    }
    prefix_ptr  = search;
    prefix_len  = i;
    search_ptr  = prefix_ptr+prefix_len;
    for (j = i+1; (j < length) && !ISPATTERN(search[j]); j++);
    search_len  = j-i;
    postfix_ptr = search_ptr+search_len;
    postfix_len = length-j;
  }
//
//  Default search
//
  else {
    search_len = length;
    search_ptr = search;
  }
//
//  Initialize the search system.
//
  matches = rejected = 0;
  state   = DICTSTATE_SEARCH;
  message (get_string(IDS_DD_SEARCHING));
//
//  Setup search flags out here so if we are doing an advanced search,
//  we don't have to evaluate these again and again.
//
  filter = false;
  for (i = DICTKEY_START; i < NUMBER_DICTKEYS; i++) if (dict_keys[i].reject) { filter = true; break; }
  nonames = dict_keys[DICTKEY_NAMES].reject && dict_keys[DICTKEY_PLACES].reject;
//
//  Do the search.
//
  abort = do_search();
  if (clipsearch && abort) abort = DICTSEARCH_ABORT;
  clipsearch = false;
  switch (abort) {
    case DICTSEARCH_SHORT:
         error (IDS_DD_ERRORLENGTH); 
         state = DICTSTATE_IDLE;
         return;
    case DICTSEARCH_MIXED:
         error (IDS_DD_ERRORASCIIKANA); 
         state = DICTSTATE_IDLE;
         return;
    case DICTSEARCH_ABORT:
         break;
  }
//''''''''''''''''''''''''''''''''''''''''''
//
//  Adaptive search engine
//
  KANJI last;
  if (!abort && !ascii_search && jwp_config.cfg.dict_advanced) {
    if (jwp_config.cfg.dict_advmark) {              // Separate advanced searches from the direct.
      put_label (IDS_DD_ADVANCED);
      primary = list->count;
    }
    while (!matches || (first && jwp_config.cfg.dict_always) || jwp_config.cfg.dict_showall) {
      if (first) first = false;
        else {
          if ((abort = do_search())) break;
        }
//
//  Get the last character in the search string.  If the character is a
//  kana then we have some processing to do.  If the last character is not 
//  a kana then there are less possible matches that we can do.
//
      last = get_last();
      if (!ISKANA(last)) {
        SEARCH_ADD (HIRAGANA_RU);                               // Could be an ichidan doushi.
        if (jwp_config.cfg.dict_iadj) SEARCH_ADD (HIRAGANA_I);  // Could be an i-adjative.
      }
//
//  Processisng for words ending in kana.
//
      else if (last == HIRAGANA_I  ) {              // te/ta-forms for ku and gu verbs.
        SEARCH_END (HIRAGANA_KU);
        SEARCH_END (HIRAGANA_GU);
//        put_last   (HIRAGANA_I);                    // Need to restore the character for later!
      }
      else if (last == HIRAGANA__TU) {              // te/ta-forms for u, tsu, and ru verbs.
        SEARCH_END (HIRAGANA_U);
        SEARCH_END (HIRAGANA_TSU);
        SEARCH_END (HIRAGANA_RU);
      }
      else if (last == HIRAGANA_N) {                // te/ta-forms for fu, bu, nu, mu verbs.
        SEARCH_END (HIRAGANA_NU);
        SEARCH_END (HIRAGANA_BU);
        SEARCH_END (HIRAGANA_MU);
      }
      else {
//
//  Get romaji value.
//
        if (last == HIRAGANA_WA) last = HIRAGANA_A;
        ptr = kana_to_ascii(last);
        i = strlen(ptr)-1;
//
//  Try imperative of ichidan doushi.
//
        if ((ptr[i] == 'i') || (ptr[i] == 'e')) SEARCH_ADD (HIRAGANA_RU);
//
//  Try treating as a godan doushi.
//
        if (ptr[i] != 'u') {
          i = (!i ? HIRAGANA_U : godan_kana(*ptr));
          if (i) SEARCH_END (i);
        }
//
//  Try to make an i-adjative.
//
//  ### This could be improved. ###
//
        if (jwp_config.cfg.dict_iadj) SEARCH_END (HIRAGANA_I);
      }
//
//  Shorten the string.
//
      if (ISPATTERN(last)) break;
      if (postfix_len) postfix_len--; else search_len--;
    }
  }
//
//  The is the cleanup steps.
//
  if (abort == DICTSEARCH_ABORT) message (get_string(rejected ? IDS_DD_ABORTREJECT : IDS_DD_ABORT),matches,rejected);
  if (matches) {                                // Handle results.
    SendDlgItemMessage (dialog,IDC_DDRESULT,JL_SETSEL,true,0);
    SetFocus (GetDlgItem(dialog,IDC_DDRESULT)); 
  }
  else {
    if (!rejected) message (get_string(IDS_DD_NOMATCH));
    MessageBeep (MB_ICONASTERISK);
  }
  state = DICTSTATE_IDLE;
  return;
}

//--------------------------------
//
//  Set the state of the four check-boxes in the main dictionary dialog.
//
void JWP_dict::set_checkboxes () {
  int i;
  if      ( dict_keys[DICTKEY_PLACES].reject &&  dict_keys[DICTKEY_NAMES].reject) i = BST_CHECKED;
  else if (!dict_keys[DICTKEY_PLACES].reject && !dict_keys[DICTKEY_NAMES].reject) i = BST_UNCHECKED;
  else                                                                            i = BST_INDETERMINATE;
  CheckDlgButton (dialog,IDC_DDNONAME   ,i);
  CheckDlgButton (dialog,IDC_DDBEGIN    ,dict_keys[DICTKEY_BEGIN].reject);
  CheckDlgButton (dialog,IDC_DDEND      ,dict_keys[DICTKEY_END  ].reject);
  CheckDlgButton (dialog,IDC_DDADVANCED ,jwp_config.cfg.dict_advanced);
#if (!(defined(WINCE_PPC) || defined(WINCE_POCKETPC)))
  CheckDlgButton (dialog,IDC_DDCLASSICAL,jwp_config.cfg.dict_classical   );
  CheckDlgButton (dialog,IDC_DDFULLASCII,jwp_config.cfg.dict_fullascii   );
  CheckDlgButton (dialog,IDC_DDPRIORITY ,jwp_config.cfg.dict_primaryfirst);
#ifndef WINCE
  CheckDlgButton (dialog,IDC_DDCLIPBOARD,jwp_config.cfg.dict_watchclip   );
  CheckDlgButton (dialog,IDC_DDJASCII   ,jwp_config.cfg.dict_jascii2ascii);
  CheckDlgButton (dialog,IDC_DDADVALWAYS,jwp_config.cfg.dict_always      );
  CheckDlgButton (dialog,IDC_DDADVALL   ,jwp_config.cfg.dict_showall     );
  CheckDlgButton (dialog,IDC_DDADVI     ,jwp_config.cfg.dict_iadj        );
  CheckDlgButton (dialog,IDC_DDMARK     ,jwp_config.cfg.dict_advmark     );
#endif WINCE
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  return;
}

//--------------------------------
//
//  This routine launches the user dictionary.  This will either bring the current 
//  user-dicitonary function to the front, or will create a new one.
//
void JWP_dict::user_dictionary (HWND hwnd) {
  if (user_dialog) SetForegroundWindow (user_dialog);
    else JCreateDialog (IDD_DICTUSER,hwnd,(DLGPROC) dialog_userdict); 
  return;
}

//--------------------------------
//
//  This routine is called when an entry in the dictionary has been 
//  found.  This rotuine will check the entry against the filters, 
//  and if the entry passess all filters, the entry will be put into
//  the list.
//
//      ptr    -- Pointer to the actual entry location in the dictionary
//                buffer.
//      length -- Length of user's search key.
//
void JWP_dict::utf_check (byte *ptr,int length) {
  int i;
//
//  Check for patter postfix
//
  if (pattern) {
    i = utf_post(ptr+length,postfix_ptr,postfix_len);
    if (i == -1) {
      rejected++;
      message (NULL);
      return;
    }
    length += i;
    i = utf_pre(ptr-1,prefix_ptr,prefix_len);
    if (i == -1) {
      rejected++;
      message (NULL);
      return;
    }
    ptr    -= i;
    length += i;
  }
//
//  Check entry for valid begin/end requirements.
//
  if (utf_endbegin(ptr-1,ptr+length)) {
    rejected++;                                 // Check for match at beginning and ending of the search.
    message (NULL); 
    return;
  }
//
//  At this point, we have a an accepted entry, that matches the user's
//  beginning/ending of the line conditions.  
//
//  Now we begin to format the line, and have to check for excluded 
//  entry times.
//
  for ( ; !ISCRLF(*ptr); ptr--);                // Backup in the buffer to the beginning of the line.
  ptr++;
//
//  Filter entry for removed ID's
//
  if (filter_entry(ptr)) return;
//
//  Hooray! We have an entry that we actually want to keep.  This means 
//  we have to format the line and generate the output.  Most of the 
//  work in generating the output string is done by the class EUC_buffer.
//  (see top of file for class information).
//                                  
  matches++;                                    // Change count and display
  message     (NULL);                           // Change count
//
//  This section formats and displays the entry.  Unlike EUC files, the rotuine is included 
//  here, because this is the only place it is needed.  With EUC we use that formatting line 
//  for the user dictionary also.
//
  int ch;
  int first_line = true;
  clear ();                                     // Intialize line buffer.
  for (i = 0; i < SIZE_LINE; i++) {             // Limit string length.
    if (!*ptr || ISCRLF(ptr[1])) break;         // End of line so exit, or error condition (past end of buffer)
    ch = utf2jis(ptr);
    if (first_line) {                           // First line has special characters.
      switch (ch) {
        case '[':
             put_char (jwp_config.cfg.dict_compress ? '[' : KANJI_LBRACKET);
             break;
        case ']':
             put_char (jwp_config.cfg.dict_compress ? ']' : KANJI_RBRACKET);
             break;
        case ' ':
             put_char ('\t');
             break;
        case '/':                                   // First '/' indicates end of first line, just text after here.
             first_line = false;
             if (!jwp_config.cfg.dict_compress) flush (-1);
             break;
        default:
             put_char (ch);
             break;
      }
    }
    else if (ch == '/') {                           // After first line just output, but change '/' into ', '.
      put_char (',');
      put_char (' ');
    }
    else put_char (ch);
  }
  flush (-1);                                       // Flush last line.
//
//  Check for primary entries.
//
  check_primary (ptr);                              // Process primary entries.
  return;
}

//--------------------------------
//
//  This routine tests the beginning and end of entry contiditons.  This routine is only called once
//  and thus could be placed in the utf_check() routine, but this is easier to visualize.  In previous
//  versions this was one complicated if statement, but it is just too hard to read that way.
//
//      first  -- Pointer to first character.
//      last   -- Pointer to last character.
//
//      RETURN -- A non-zero value indicates this entry should be rejected.
//
#define UTF_CPARTEND(p)     (((p)[0] == ')') || UTF_MATCH(p,UTF_CAMA) || UTF_MATCH(p,UTF_SLASH))

int JWP_dict::utf_endbegin (byte *first,byte *last) {
  if (ascii_search) {
    if (dict_keys[DICTKEY_BEGIN].reject && !((*first == '/') || (!jwp_config.cfg.dict_fullascii && !isalnum(*first)) || ((first[0] == ' ') && (first[-1] == ')')))) return (true);
    if (dict_keys[DICTKEY_END  ].reject && !((*last  == '/') || (!jwp_config.cfg.dict_fullascii && !isalnum(*last )))) return (true);
    return (false);
  }
  if (dict_keys[DICTKEY_BEGIN].reject && !((*first == '[') || ISCRLF(*first) || classical_part)) return (true);
  if (dict_keys[DICTKEY_END  ].reject && !((*last  == ']') || (*last == ' ') || (classical_part && EUC_CPARTEND(last)))) return (true);
  return (false);
}

//
//  End Class JWP_dict.
//
//===================================================================

//===================================================================
//
//  Dictionary search routines.  These are all containned in derived class.  Generally
//  There is a different routine for each type of search and for each encoding of the dictioanry.
//  Mixed dictioanries are searched using the EUC rotuines.
//

//--------------------------------
//
//  This is the search routine for indexed dictionaries.
//
//      key    -- Key to search for.
//      length -- Length of the key
//
void EUC_IDX_Dictionary::search (KANJI *key,int length) {
  long       top,bottom,middle,cut;     // Parameter for binary search.
  int        diff;                      // Difference in key comparisons.
  byte       buffer[SIZE_DICTBUFFER];   // Main buffer for reading in data from the dictionary.
  byte      *buf;                       // Pointer to the location in buffer when the user requested data is.
//
//  This routine uses buffer and buf, to allow us to backward scan in 
//  the file.  This works as follows:
//
//  When a read from the dictionary takes place, a block of size SIZE_DICTBUFFER
//  bytes is read.  The cener of this block is the location actually 
//  requested.  This gives us the capability to backup to the beginning 
//  of the dictonary entry.  
//
//  The pointer buf, points into the buffer and points the actual point
//  in the line that was being requested.
//
  buf = buffer+SIZE_LINE-1;
//
//  This section performs a binary search, looking for the beginning
//  of the index region dealing with the string that the user has entered.
//
  top    = 0;
  bottom = index_max;
  while (true) {
    middle = (top+bottom)/2;
    get_line (middle,buffer);
    diff = euc_comp(buf,key,length);
    if (top >= bottom-1) { cut = bottom; break; }
    if (diff > 0) top = middle; else bottom = middle;
  }
//
//  The actual loop.
//
  for (;; cut++) {
    if (jwp_dict.check_abort()) break;
//
//  Get the next line from the dictionary.  
//
    get_line (cut,buffer);
    if (euc_comp(buf,key,length)) break;        // No longer match.  Search is done, let's get out of here.
    jwp_dict.euc_check (buf);
  }
  return;
}

//--------------------------------
//
//  Performs a dictionary search based on a memory dictionary.  The
//  dictionary should be loaded before calling this routine.
//
//      key    -- Key to be searched for.
//      length -- Length of the key.
//
void EUC_MEM_Dictionary::search (KANJI *key,int length) {
  int   i;
  byte *ptr,*p,*p2,buffer[SIZE_BUFFER];

  for (ptr = memory+1; *ptr; ptr++) {
    if (euc_comp(ptr,key,length)) {             // No match.
      if (*ptr >= 0x80) ptr++;                  // For kana/kanji characters skip two bytes.
      continue;
    }
    for (i = 0, p = ptr; (i < SIZE_LINE-9) && !ISCRLF(*ptr); ptr--, i++); 
                                                // Find beginning of entry.
    buffer[0] = '\n';                           // Build duplicate entry.
    p2 = buffer+(p-ptr);                        // Calculate same relative place for end/begin 
    for (i = 1, ptr++; (i < SIZE_BUFFER-2) && !ISCRLF(*ptr); ptr++) buffer[i++] = *ptr;     
                                                // Diplicate entry (use ptr so skip rest of entry)
    buffer[i] = '\n';                           // Terminate enry.
    jwp_dict.euc_check (p2);                    // Process entry.
  }
  return;
}

//--------------------------------
//
//  This is the search routine for indexed dictionaries.
//
//      key    -- Key to search for.
//      length -- Length of the key
//
void UTF_IDX_Dictionary::search (KANJI *key,int length) {
  long       top,bottom,middle,cut;     // Parameter for binary search.
  int        diff;                      // Difference in key comparisons.
  int        utf_length;                // Length of UTF matched string.
  byte       buffer[SIZE_DICTBUFFER];   // Main buffer for reading in data from the dictionary.
  byte      *buf;                       // Pointer to the location in buffer when the user requested data is.
//
//  This routine uses buffer and buf, to allow us to backward scan in 
//  the file.  This works as follows:
//
//  When a read from the dictionary takes place, a block of size SIZE_DICTBUFFER
//  bytes is read.  The cener of this block is the location actually 
//  requested.  This gives us the capability to backup to the beginning 
//  of the dictonary entry.  
//
//  The pointer buf, points into the buffer and points the actual point
//  in the line that was being requested.
//
  buf = buffer+SIZE_LINE-1;
//
//  This section performs a binary search, looking for the beginning
//  of the index region dealing with the string that the user has entered.
//
  top    = 0;
  bottom = index_max;
  while (true) {
    middle=(top+bottom)/2;
    get_line (middle,buffer);
    diff = utf_comp(buf,key,length,utf_length);
    if (top >= bottom-1) { cut = bottom; break; }
    if (diff > 0) top = middle; else bottom = middle;
  }
//
//  The actual loop.
//
  for (;; cut++) {
    if (jwp_dict.check_abort()) break;
//
//  Get the next line from the dictionary.  
//
    get_line (cut,buffer);
    if (utf_comp(buf,key,length,utf_length)) break;     // No longer match.  Search is done, let's get out of here.
    jwp_dict.utf_check (buf,utf_length);
  }
  return;
}

//--------------------------------
//
//  Performs a dictionary search based on a memory dictionary.  The
//  dictionary should be loaded before calling this routine.
//
//      key    -- Key to be searched for.
//      length -- Length of the key.
//
void UTF_MEM_Dictionary::search (KANJI *key,int length) {
  int   i,utf_length;
  byte *ptr,*p,*p2,buffer[SIZE_BUFFER];

  for (ptr = memory+1; *ptr; ptr += utf_size(*ptr)) {
    if (utf_comp(ptr,key,length,utf_length)) continue;                      // No match.
    for (i = 0, p = ptr; (i < SIZE_LINE-9) && !ISCRLF(*ptr); ptr--, i++);   // Find beginning of entry.
    buffer[0] = '\n';                                                       // Build duplicate entry.
    p2 = buffer+(p-ptr);                                                    // Calculate same relative place for end/begin 
    for (i = 1, ptr++; (i < SIZE_BUFFER-2) && !ISCRLF(*ptr); ptr++) buffer[i++] = *ptr;     
                                                                            // Diplicate entry (use ptr so skip rest of entry)
    buffer[i] = '\n';                                                       // Terminate enry.
    jwp_dict.utf_check (p2,utf_length);                                     // Process entry.
  }
  return;
}

//
//  End search routines.
//
//===================================================================


// ### Should make dictionary class dynamically allocated

// ### Think about improvements based on changing the rejection keys.


//--------------------------------
//
//  Small utlity routine used to copy the clipboard contets to the current edit box.
//  This is currently only used in the dictionary routines, for clipboard tracking.
//  The commands are done directly to prevent activating the cursor routines in the 
//  edit box in case the edit box is not selected.
//
int JWP_file::edit_clip () {
  sel.type      = SELECT_EDIT;      // Select all text
  sel.pos1.para = first;
  sel.pos1.line = first->first;
  sel.pos1.pos  = 0;
  sel.pos2.para = last;
  sel.pos2.line = last->first;
  sel.pos2.pos  = last->length;
  sel.pos2.rel ();
  clip_paste (false);               // Paste.  This will remove all previous text
  return (true);
}























static Dictionary *new_dictionary (TCHAR *name,TCHAR *file,TCHAR *flags) {
  TCHAR      *ptr;
  Dictionary *dic;
  int    type   = DICT_MEMORY;
  int    format = DICT_EUC;
  for (ptr = flags; *ptr; ptr++) {
    switch (*ptr) {
      case DICTFLAG_UTF8:   format = DICT_UTF8;   break;
      case DICTFLAG_MIXED:  format = DICT_MIXED;  break;
      case DICTFLAG_INDEX:  type   = DICT_INDEX;  break;
      case DICTFLAG_BUFFER: type   = DICT_BUFFER; break;
      default:                                    break;
    }
  }
  switch (type) {
    case DICT_BUFFER:
// TODO -- This needs to be added.    
    case DICT_MEMORY: 
         switch (format) {
           default:
           case DICT_EUC:   dic = new EUC_MEM_Dictionary; break;
           case DICT_MIXED: dic = new MIX_MEM_Dictionary; break;
           case DICT_UTF8:  dic = new UTF_MEM_Dictionary; break;   
         }
         break;
    case DICT_INDEX:  
         switch (format) {
           default:
           case DICT_EUC:   dic = new EUC_IDX_Dictionary; break;
           case DICT_MIXED: dic = new MIX_IDX_Dictionary; break;
           case DICT_UTF8:  dic = new UTF_IDX_Dictionary; break;   
         }
         break;
  }
  dic->init(name,file,flags);
  return (dic);
}

//--------------------------------
//
//  This is the search routine for indexed dictionaries.
//
//      key    -- Key to search for.
//      length -- Length of the key
//
void MIX_IDX_Dictionary::search (KANJI *key,int length) {
  long       top,bottom,middle,cut;     // Parameter for binary search.
  int        diff;                      // Difference in key comparisons.
  byte       buffer[SIZE_DICTBUFFER];   // Main buffer for reading in data from the dictionary.
  byte      *buf;                       // Pointer to the location in buffer when the user requested data is.
//
//  This routine uses buffer and buf, to allow us to backward scan in 
//  the file.  This works as follows:
//
//  When a read from the dictionary takes place, a block of size SIZE_DICTBUFFER
//  bytes is read.  The cener of this block is the location actually 
//  requested.  This gives us the capability to backup to the beginning 
//  of the dictonary entry.  
//
//  The pointer buf, points into the buffer and points the actual point
//  in the line that was being requested.
//
  buf = buffer+SIZE_LINE-1;
//
//  This section performs a binary search, looking for the beginning
//  of the index region dealing with the string that the user has entered.
//
  top    = 0;
  bottom = index_max;
  while (true) {
    middle = (top+bottom)/2;
    get_line (middle,buffer);
    diff = mix_comp(buf,key,length);
    if (top >= bottom-1) { cut = bottom; break; }
    if (diff > 0) top = middle; else bottom = middle;
  }
//
//  The actual loop.
//
  for (;; cut++) {
    if (jwp_dict.check_abort()) break;
//
//  Get the next line from the dictionary.  
//
    get_line (cut,buffer);
    if (mix_comp(buf,key,length)) break;        // No longer match.  Search is done, let's get out of here.
    jwp_dict.euc_check (buf);
  }
  return;
}

//--------------------------------
//
//  Performs a dictionary search based on a memory dictionary.  The
//  dictionary should be loaded before calling this routine.
//
//      key    -- Key to be searched for.
//      length -- Length of the key.
//
void MIX_MEM_Dictionary::search (KANJI *key,int length) {
  int   i;
  byte *ptr,*p,*p2,buffer[SIZE_BUFFER];

  for (ptr = memory+1; *ptr; ptr++) {
    if (mix_comp(ptr,key,length)) {             // No match.
      if (*ptr >= 0x80) ptr++;                  // For kana/kanji characters skip two bytes.
      continue;
    }
    for (i = 0, p = ptr; (i < SIZE_LINE-9) && !ISCRLF(*ptr); ptr--, i++); 
                                                // Find beginning of entry.
    buffer[0] = '\n';                           // Build duplicate entry.
    p2 = buffer+(p-ptr);                        // Calculate same relative place for end/begin 
    for (i = 1, ptr++; (i < SIZE_BUFFER-2) && !ISCRLF(*ptr); ptr++) buffer[i++] = *ptr;     
                                                // Diplicate entry (use ptr so skip rest of entry)
    buffer[i] = '\n';                           // Terminate enry.
    jwp_dict.euc_check (p2);                    // Process entry.
  }
  return;
}

//--------------------------------
//
//  Dictionary comparison routine for Mixed dictionaries (also used for mixed dictionaries).
//
//      key    -- JIS string to be compared with.
//      ptr    -- Pointer to location in dictionary to be compared.
//      length -- Length of the key.
//
//      RETURN -- Zero indicates the key and data match.  A non-zero value
//                indicates they don't match.  Positive indicates key is 
//                later than the data.  Netgitive indicates oposite.
//
static int mix_comp (byte *ptr,KANJI *key,int length) {
  int   i,j;
  KANJI c;
  for (j = i = 0; i < length; i++, j++) {
    if ((0x80 & ptr[j]) && (!ISASCII(key[i]))) {
      c = ptr[j];
      if (c == 0xa5) c = 0xa4;
      c = 0x7f7f & ((c << 8) | ptr[++j]);
    }
    else {
      c = tolower(ptr[j]);
    }
    if (c != key[i]) return (key[i]-c);
  }
  return (0);
}












