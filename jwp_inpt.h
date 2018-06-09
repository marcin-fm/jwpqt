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
#ifndef jwp_inpt_h
#define jwp_inpt_h

#include "jwp_file.h"

#define BASE_KATAKANA   0x2500      // Base value for katakana characters.
#define BASE_HIRAGANA   0x2400      // Base value for hiragana characters.
#define BASE_JASCII     0x2300      // Page for JASCII
#define BASE_GREEK      0x2600      // Greek script
#define BASE_CYRILLIC   0x2700      // Russian/Cyrillic script
#define BASE_KANJI      0x3000      // Value of first actual kanji
#define BASE_KANJI2     0x5000      // First page of Type II (uncommon) kanji.

#define KANJI_SPACE     0x2121      // Space character in kanji.
#define KANJI_SLASH     0x213F      // Forward slash (Japanese).
#define KANJI_QUESTION  0x2129      // ? (Japanese)
#define KANJI_COMMA     0x2122      // Comma (Japanese)
#define KANJI_DOT       0x2126      // Center dot (Japanese)
#define KANJI_NUM_SIGN  0x2174      // Number sign (Japanese)
#define KANJI_ASTERISK  0x2176      // Asterisk (Japanese)
#define KANJI_ARROW     0x222A      // Horizontal arrow (use in user conversion editor)
#define KANJI_TILDE     0x2141      // ~ (Japanese)
#define KANJI_DASH      0x213D      // - (Japanese, not long vawel, just a dash)
#define KANJI_LONGVOWEL 0x213C      // - (Japanese)
#define KANJI_LPAREN    0x214A      // ( (Japanese)
#define KANJI_RPAREN    0x214B      // ) (Japanese)
#define KANJI_LBRACKET  0x215A      // [ (heavy Japanese)
#define KANJI_RBRACKET  0x215B      // ] (heavy Japanese)   
#define KANJI_LBRACE    0x214E      // [ (Japanese)
#define KANJI_RBRACE    0x214F      // ] (Japanese)
#define KANJI_BAD       0x2223      // Knaji used for invalid kanji (this is a solid box).

#define KATAKANA_REPT   0x2133      // Katakana repetition mark
#define KATAKANA_REPT_V 0x2134      // Katakana repetition mark (voiced)
#define HIRAGANA_REPT   0x2135      // Hiragana repetition mark
#define HIRAGANA_REPT_V 0x2136      // Hiragana repetition mark (voiced)
#define KANJI_REPT      0x2139      // Kanji repetition mark
#define HIRAGANA_WO     0x2472

#define ISCRLF(ch)     (((ch) == '\n') || ((ch) == '\r'))               // Is this a cr or lf 
#define ISSPACE(ch)    ((ch == ' ') || (ch == '\t') || (ch == 0x2121))  // Is this a space.
#define ISASCII(ch)    (((ch) & 0x7f00) == 0)                           // Is this any ASCII character
#define ISJIS(ch)      (((ch) & 0x7f00) != 0)                           // Is this a kanji or kana
#define ISHIRAGANA(ch) (((ch) & 0x7f00) == BASE_HIRAGANA)               // Is this hiragana
#define ISKATAKANA(ch) (((ch) & 0x7f00) == BASE_KATAKANA)               // Is this katakana
#define ISKANA(ch)     (ISHIRAGANA(ch) || ISKATAKANA(ch))               // Is this a kana
#define ISJASCII(ch)   (((ch) & 0x7f00) == BASE_JASCII)
#define ISGREEK(ch)    (((ch) & 0x7f00) == BASE_GREEK)
#define ISCYRILLIC(ch) (((ch) & 0x7f00) == BASE_CYRILLIC)
#define ISKANJI(ch)    (((ch)         ) >= BASE_KANJI)
#define ISRAREKANJI(ch)(((ch)         ) >= BASE_KANJI2)

//-------------------------------------------------------------------
//
//  Exported data (romaji->kana converter)
//
struct compound_kana {
  char string[6];     // sting pattern to match (embeded in table).
  byte kana  [2];     // Generated kana.  A value of zero in one byte will suppress that kana.
  byte hide;          // If true, do not display this conversion in the Character Information window. Only applicable to conversions that result in a single kana.
}; 
                                    
#define SIZE_DIRECT 83

extern char                  direct_kana[SIZE_DIRECT][4];   // Direct kana (each corresponds to a single value
extern struct compound_kana  compound_kana[];               // Compound kana list.  Mostly two kana characters, but there are some others.

//-------------------------------------------------------------------
//
//  Exported routines.
//
                                    // Different character classes returned by char_class().
#define CLASS_ASCII     1           // ASCII letter or number
#define CLASS_KATAKANA  2           // Katakana character
#define CLASS_HIRAGANA  3           // Hiragana character
#define CLASS_JASCII    4           // JASCII letter or number
#define CLASS_SPACE     5           // Space or tab (includes JASCII space)
#define CLASS_APUNCT    6           // ASCII punctuation
#define CLASS_KPUNCT    7           // JASCII punctuation.
#define CLASS_KANJI     8           // Kanji character
#define CLASS_JUNK      10          // Invalid character (usually a line break)

int    char_class    (int ch);      // Routine to get the class of a character.
int    same_class    (int ch1,int ch2);
int  jascii_to_ascii (int ch);      // Convert jascii character to ascii character.
char  *kana_to_ascii (int kana);    // Convert a kana character back to ascii (used in JWP_conv).

//-------------------------------------------------------------------
//
//  Class KANA_convert.
//
//  This specialized class simply handles conversion of ascii characters
//  to kana characters, and interfaces with the kanji generation rotuines.
//

typedef class KANA_convert {
public:
  inline KANA_convert () { erase (); }          // Initialization routine.
  void        clear    (void);                  // Clears state of convert.  Some characters may be output at this time (n,aieuo).
  void        do_char  (JWP_file *file,int ch); // Entry level routine, called by client to pass characters to the routinte.
  int         erase    (void);                  // Clear without outputing characters.
  void        force    (void);                  // Force output buffer to hiragana
  bool        pending_AIUEO (void)     { return (pending && index == 1 && isupper(buffer[0]) && !(buffer[1] = 0) && strspn(buffer,"AIUEO"  ) == 1); }   // The second-to-last conditional will always be true and is there specifically for its side effects.
  bool        pending_ambiguous (void) { return (pending && index == 1                       && !(buffer[1] = 0) && strspn(buffer,"AIUEONn") == 1); }   // Returns true if any ambiguous kana are pending.
private:
  void        out_kana (void);                  // Called to convert accumlated ASCII into KANA.
  void        put_kana (int ch);                // Exit level routine that puts kana back to the client.
  char      buffer[6];          // Buffer to accumulate characters from client.
  short     index;              // Pointer into buffer for next character.
  short     state;              // Cureent state (see description of state table).
  byte      pending;            // Holding a KANA for later conversion.
  byte      kanji_start;        // Set non-zero if first character of ASCII is capital.
  JWP_file *file;               // File where we were called from.  This lets us know where to put the characters back.        
} KANA_convert;

extern KANA_convert kana_convert;       // Instance of the KANA_convert class.

#endif jwp_inpt_h
