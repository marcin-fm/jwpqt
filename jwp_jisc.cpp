//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//  The code do do conversion between ECU, JIS, and Shift-JIS        //
//  was taken from jconv.c which is copyright by Ken R. Lunde,       //
//  Adobe Systems Incorporated.  Full copyright notice is below.     //
//                                                                   //
//===================================================================//

//===================================================================
//
//  This module implements the translatin between JWP's internal coding
//  and various other formats.  These formats include various JIS formats,
//  Shift-JIS, EUC, UNICODE, and ASCII.  (ASCII format is only supported
//  as an import format.)
//
//  Note this class is base on the IO_cache class that provides both
//  input and output buffering.  Additionally, the IO_cache class 
//  provides an easy way for this class to operate on both files and 
//  clipboard contents, as well as an easy character counter.  See the 
//  jwp_cach.cpp/h files for informationon the IO_cache class.
//
//  Reading a file:
//
//  First establish an input stream.  This is done by calling a routine
//  like JIS_convert::input_file() or JIS_convert::input_clip(), etc.
//  Then you repeatedly call JIS_convert::input_char() until the routine
//  returns JIS_EOF (-1).  Errors are returned at every stange.  Note
//  that JIS_convert::input_char() returns a unicode character.
//
//  Wrinting a file:
//
//  First establish an output stream.  This is done by calling a 
//  routine like JIS_convert::output_file() or JIS_convert::output_clip().
//  You then repeadly call JIS_convert::outut_char() with each unicode
//  character.  Finally, when done you call JIS_convert::flush() to 
//  clean-out the buffers.  Error checking is only done internally.
//  The error is avaialbe at the time flush() is called.
//
//  For all operations, except JIS_convert::find_type(), you must also 
//  call JIS_convert::set_type to determinethe type of file conversion
//  used.  
//
//-----------------------
//
//  I have shamelessly stold most of the guts of this code from Ken R. 
//  Lunde's jconv code.  I have substancially reformated the code, and 
//  have added input and output buffering, as well restructing the 
//  code into c++.  Below is the copywrite from Ken R. Lunde's original
//  jconv.c
//
//  Also the UNICODE translators were written by me and not by Ken Lunde, 
//  so don't blame him if they don't work correctly.
//
//  Program: jconv.c
//  Version: 3.0
//  Date:    July 1, 1993
//  Author:  Ken R. Lunde, Adobe Systems Incorporated
//    EMAIL: lunde@mv.us.adobe.com
//    MAIL : 1585 Charleston Road, P.O. Box 7900, Mountain View, CA 94039-7900
//  Type:    A tool for converting the Japanese code of Japanese textfiles.
//  Code:    ANSI C (portable)
//  
//  PORTABILITY:
//  This source code was written so that it would be portable on C compilers which
//  conform to the ANSI C standard. It has been tested on a variety of compilers.
//  
//  I used THINK C and GNU C as my development platforms. I left in the Macintosh-
//  specific lines of code so that it would be easier to enhance/debug this tool
//  later. For those of you who wish to use this tool on the Macintosh, simply
//  add the ANSI library to the THINK C project, and then build the application.
//  Be sure that THINK_C has been defined, though, as the conditional compilation
//  depends on it. You then have a double-clickable application, which when
//  launched, will greet you with a Macintosh-style interface.
//  
//  DISTRIBUTION AND RESTRICTIONS ON USAGE:
//   1) Please give this source code away to your friends at no charge.
//   2) Please try to compile this source code on various platforms to check for
//      portablity, and please report back to me with any results be they good or
//      bad. Suggestions are always welcome.
//   3) Only use this tool on a copy of a file -- do not use an original. This
//      is just common sense.
//   4) This source code or a compiled version may be bundled with commercial
//      software as long as the author is notified beforehand. The author's name
//      should also be mentioned in the credits.
//   5) Feel free to use any of the algorithms for your own work. Many of them are
//      being used in other tools I have written.
//   6) The most current version can be obtained by requesting a copy directly
//      from me.
//
#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_file.h"
#include "jwp_inpt.h"
#include "jwp_jisc.h"

//===================================================================
//
//  Compile-time code generation options
//

//
//  SUPPORT_HALFKATA - If defined allows suport for half width katakana
//                     strings used in Shift-JIS and EUC encodings.  If 
//                     not defined half-width katakana sequences will not
//                     be read.  These sequences do not appear to be used
//                     much.
//

//===================================================================
//
//  Static tables
//
//  These are primarally conversion talbes for processing Unicode data.
//

#define NUMBER_KANJIUNICODE     ((int) (sizeof(kanji_unicode)/sizeof(KANJI)))

static KANJI kanji_unicode[] = {    // Main kanji table.  These are unicode values
  #include "jwp_ukan.dat"           //   associated with jwp_info type indexes.
};

#define NUMBER_MISCUNICODE      ((int) (sizeof(misc_unicode)/sizeof(KANJI)))

static KANJI misc_unicode[] = {     // Main symbol group, corresponds to JIS codes 0x2100
  #include "jwp_umis.dat"
};

static KANJI cp1250[128] = {        // Eastern Europe code page
  #include "jwp_cp1250.dat"
};

static KANJI cp1251[128] = {        // Cyrillic code page
  #include "jwp_cp1251.dat"
};

static KANJI cp1252[128] = {        // USA, West Europe code page
  #include "jwp_cp1252.dat"
};

static KANJI cp1253[128] = {        // Greek code page
  #include "jwp_cp1253.dat"
};

static KANJI cp1254[128] = {        // Turkish code page
  #include "jwp_cp1254.dat"
};

static KANJI cp1255[128] = {        // Hebrew code page
  #include "jwp_cp1255.dat"
};

static KANJI cp1256[128] = {        // Arabic code page
  #include "jwp_cp1256.dat"
};

static KANJI cp1257[128] = {        // Baltic code page
  #include "jwp_cp1257.dat"
};

static KANJI cp1258[128] = {        // Vietnamese code page
  #include "jwp_cp1258.dat"
};

#define NUMBER_EXTUNICODE       128

static KANJI *ext_unicode = cp1252;     // Extended ascii unicode for European character support

//===================================================================
//
//  Static functions.
//

//--------------------------------
//
//  Internal routine used to convert Shift-JIS codes into JIS codes.
//
//      p1,p2 -- Pointers to a Shift-JIS code on entry.  This will 
//               point to the JIS code on exit.
//
//  I got this routine from Ken R. Lunde's jconv.c and have reformated
//  it and changed the style of the code slightly.  
//
static void sjis2jis (int *p1,int *p2) {
  int c1 = *p1;
  int c2 = *p2;
  int adjust     = c2 < 159;
  int rowOffset  = (c1 < 160) ? 112 : 176;
  int cellOffset = adjust ? ((c2 > 127) ? 32 : 31) : 126;
  *p1  = ((c1 - rowOffset) << 1) - adjust;
  *p2 -= cellOffset;
}

//===================================================================
//
//  Exported functions.
//

//--------------------------------
//
//  This routien is a variant on JIS->UNICODE routine, but it always uses the US
//  code page.  This is primarally used to decode PinYin characters for UNICODE>
//
//      ch     -- Character to be converted.
//
//      RETURN -- Unicode value.

int ascii2unicode (int ch) {
  int    ret;
  KANJI *old;
  old         = ext_unicode;
  ext_unicode = cp1252;
  ret         = jis2unicode(ch);
  ext_unicode = old;
  return (ret);
}

//--------------------------------
//
//  The main function of this routine is to map the UNICODE table used for extneded 
//  character processing into the table used for the local Code page.  
//
void initialize_cp () {
  int page;
  if (!(page = jwp_config.cfg.code_page)) page = GetACP();
  switch (page) {
    case 1250: ext_unicode = cp1250; break;
    case 1251: ext_unicode = cp1251; break;
    case 1253: ext_unicode = cp1253; break;
    case 1254: ext_unicode = cp1254; break;
    case 1255: ext_unicode = cp1255; break;
    case 1256: ext_unicode = cp1256; break;
    case 1257: ext_unicode = cp1257; break;
    case 1258: ext_unicode = cp1258; break;
    default:   ext_unicode = cp1252; break;     // If we don't know the code page default to USA (my code page!)
  }
  return;
}

//--------------------------------
//
//  Translates from JIS code to Shift-JIS code.  This is used by the 
//  shift-JIS file writer and by the Character info screen.
//
//      ch     -- Imput character code (JIS code).
//      
//      RETURN -- Shift JIS code.
//
int jis2sjis (int ch) {
  unsigned c1,c2;
  c1  = (ch >> 8);
  c2  = (ch & 0xff);
  c2 += (c1 % 2) ? ((c2 > 95) ? 32 : 31) : 126;    // jis code -> sjis code [jis2sjis() from jconv.c]
  c1  = ((c1+1) >> 1) + ((c1 < 95) ? 112 : 176);   // jis code -> sjis code [jis2sjis() from jconv.c]
  return ((c1 << 8) | (c2));
}

//--------------------------------
//
//  This routine converts from JIS to Unicode codes.  This is one half
//  of the core of the Unicode support.
//
//      ch     -- Unicode character.
//
//      RETURN -- JIS character code.
//
int jis2unicode (int ch) {
  if ((ch <= 0x7e)                    ) return (ch);                    // ASCII
  if ((ch >= 0x0080) && (ch <= 0x00ff)) return (ext_unicode[ch-0x80]);  // Extended ASCII
  if ((ch >= 0x2330) && (ch <= 0x237a)) return (ch-0x2330+0xff10);      // Japanese ASCII
  if ((ch >= 0x2421) && (ch <= 0x2473)) return (ch-0x2421+0x3041);      // Hiragana
  if ((ch >= 0x2521) && (ch <= 0x2576)) return (ch-0x2521+0x30a1);      // Katakana
  if ((ch >= 0x2621) && (ch <= 0x2658)) {                               // Greek
    if (ch <= 0x2631) return (ch-0x2621+0x0391);
    if (ch <= 0x2638) return (ch-0x2621+0x0392);
    if (ch <  0x2641) return (0);
    if (ch <= 0x2651) return (ch-0x2621+0x0391);
    if (ch <= 0x2658) return (ch-0x2621+0x0392);
    return (0);
  }
  if ((ch >= 0x2721) && (ch <= 0x2771)) {                               // Cyrillic
    if (ch <= 0x2726) return (ch-0x2721+0x0410);
    if (ch == 0x2727) return (0x0401);
    if (ch <= 0x2741) return (ch-0x2722+0x0410);
    if (ch <  0x2751) return (0);
    if (ch <= 0x2756) return (ch-0x2751+0x0430);
    if (ch == 0x2757) return (0x0451);
    if (ch <= 0x2771) return (ch-0x2752+0x0430);
    return (0);
  }
  if ((ch >= 0x3021) && (ch <= 0x7426)) {                               // Kanji    
    if (((ch & 0x7f) < 0x21) || ((ch & 0x7f) > 0x7e)) return (0);       // Invalid JIS codes
    ch -= 0x3021;
    ch  = HIBYTE(ch)*94+LOBYTE(ch);
    return (kanji_unicode[ch]);
  }
  if ((ch >= 0x2121) && (ch <= 0x217e)) return (misc_unicode[ch-0x2121]);       // Main symbol group
  if ((ch >= 0x2221) && (ch <= 0x227e)) return (misc_unicode[ch-0x2221+94]);    // Secondary symbol group
  if ((ch >= 0x2821) && (ch <= 0x2840)) return (misc_unicode[ch-0x2821+94+94]); // Line drawing symbols
  return (0);
}

//--------------------------------
//
//  Convert JIS code into UTF-8 value.  Because UTF-8 can take up to 4 bytes to store a value, 
//  this is a varing length conversion.  Further, note that 4 byte values are not supported, since
//  they lead to extended UNICODE which is not supported.
//
//  This routine is called with a pointer to a data array.  A single JIS value is written,
//  and the pointer is advanced to the next data to be written.
//
//      ptr -- On entry a pointer to the start of a UTF-8 encoded character.  On exit, this
//             will point to the next UTF character.
//      ch  -- Character value to be written.
//
void jis2utf (byte *&ptr,int ch) {
  ch = jis2unicode(ch);
  if      (!(ch & UTF8_WRITE1)) *ptr++ = ch;        // Single bite codes 0xxxxxxx
  else if (!(ch & UTF8_WRITE2)) {                   // Double byte codes 110xxxxx 10yyyyyy   
    *ptr++ = (UTF8_VALUE2 | (ch >> 6));
    *ptr++ = (UTF8_VALUEC | (ch & 0x3f));
  }
  else {                                            // Three byte codes  1110xxxx 10yyyyyy 10zzzzzz
    *ptr++ = (UTF8_VALUE3 | (ch >> 12));
    *ptr++ = (UTF8_VALUEC | ((ch >> 6) & 0x3f));
    *ptr++ = (UTF8_VALUEC | (ch & 0x3f));
  }
  return;
}

//--------------------------------
//
//  This is the shift-JIS to JIS convert packaged to be more like the 
//  other converters, specifcally so that the JIS-table routines 
//  can use it.
//
//      ch     -- Shift-JIS character that can be converted to JIS.
//
//      RETURN -- JIS character value.
//
int sjis2jis (int ch) {
  int p1,p2;
  p1 = ch >> 8;
  p2 = ch & 0xff;
  sjis2jis (&p1,&p2);
  return ((p1 << 8) | p2);
}

static const unsigned short kata2half[][2] = {
                  {0x8142, 0xa1}, {0x8175, 0xa2}, {0x8176, 0xa3}, {0x8141, 0xa4}, {0x8145, 0xa5}, {0x8392, 0xa6}, {0x8340, 0xa7}, {0x8342, 0xa8}, {0x8344, 0xa9}, {0x8346, 0xaa}, {0x8348, 0xab}, {0x8383, 0xac}, {0x8385, 0xad}, {0x8387, 0xae}, {0x8362, 0xaf},
  {0x815b, 0xb0}, {0x8341, 0xb1}, {0x8343, 0xb2}, {0x8345, 0xb3}, {0x8347, 0xb4}, {0x8349, 0xb5}, {0x834a, 0xb6}, {0x834c, 0xb7}, {0x834e, 0xb8}, {0x8350, 0xb9}, {0x8352, 0xba}, {0x8354, 0xbb}, {0x8356, 0xbc}, {0x8358, 0xbd}, {0x835a, 0xbe}, {0x835c, 0xbf},
  {0x835e, 0xc0}, {0x8360, 0xc1}, {0x8363, 0xc2}, {0x8365, 0xc3}, {0x8367, 0xc4}, {0x8369, 0xc5}, {0x836a, 0xc6}, {0x836b, 0xc7}, {0x836c, 0xc8}, {0x836d, 0xc9}, {0x836e, 0xca}, {0x8371, 0xcb}, {0x8374, 0xcc}, {0x8377, 0xcd}, {0x837a, 0xce}, {0x837d, 0xcf},
  {0x837e, 0xd0}, {0x8380, 0xd1}, {0x8381, 0xd2}, {0x8382, 0xd3}, {0x8384, 0xd4}, {0x8386, 0xd5}, {0x8388, 0xd6}, {0x8389, 0xd7}, {0x838a, 0xd8}, {0x838b, 0xd9}, {0x838c, 0xda}, {0x838d, 0xdb}, {0x838f, 0xdc}, {0x8393, 0xdd}, {0x814a, 0xde}, {0x814b, 0xdf},
  {0x8394, 0xb3de}, {0, 0}
};

int sjisalternate(int sjis) {
  int i,half;
  if (sjis < 0x8141 || sjis > 0x8394) return 0;
  for (i = 0; half = kata2half[i][1]; i++) {
    if (sjis == kata2half[i][0]) return half;
    if (sjis < 0x834b || sjis > 0x837c) continue;
    if (sjis >= 0x8369 && sjis <= 0x836e) continue;
    if (sjis == 0x8362 || sjis == 0x8363) continue;
    if (sjis == kata2half[i][0]+1) return (half<<8|0xde);
    if (sjis == kata2half[i][0]+2 && sjis >= 0x8370) return (half<<8|0xdf);
  }
  return 0;
}

//--------------------------------
//
//  This routine converts from Unicode to JIS codes.  This is one half
//  of the core of the Unicode support.
//
//      ch     -- Unicode character.
//      bad    -- Character returned for untraslateable UNICODE.
//
//      RETURN -- JIS character code.
//
int unicode2jis (int ch,int bad) {
  int i;
  if ((ch <= 0x007e)                  ) return (ch);                // ASCII
  if ((ch >= 0x3041) && (ch <= 0x3093)) return (ch-0x3041+0x2421);  // Hiragana
  if ((ch >= 0x30a1) && (ch <= 0x30f6)) return (ch-0x30a1+0x2521);  // Katakana
  if (ch == 0x201a) return (bad);                                   // 0x82 misconstrued
  if (ch == 0x0192) return (bad);                                   // 0x83 misconstrued
  if (ch == 0x201e) return (bad);                                   // 0x84 misconstrued
  if ((ext_unicode != cp1253) && (ch >= 0x0391) && (ch <= 0x03c9)) {// Greek
    if (ch <= 0x03a1) return (ch-0x0391+0x2621);
    if (ch == 0x03a2) return (bad);
    if (ch <= 0x03a9) return (ch-0x0392+0x2621);
    if (ch <  0x03b1) return (bad);
    if (ch <= 0x03c1) return (ch-0x0391+0x2621);
    if (ch == 0x03c2) return (bad);
    if (ch <= 0x03c9) return (ch-0x0392+0x2621);
    return (bad);
  }
  if (ext_unicode != cp1251) {                                      // Cyrillic
    if (ch == 0x0401) return (0x2727);                                
    if (ch == 0x0451) return (0x2757);
    if ((ch >= 0x0410) && (ch <= 0x044f)) {
      if (ch <= 0x0415) return (ch-0x0410+0x2721);
      if (ch <= 0x042f) return (ch-0x0416+0x2728);
      if (ch <= 0x0435) return (ch-0x0430+0x2751);
      if (ch <= 0x044f) return (ch-0x0436+0x2758);
    }
  }
  for (i = 0; i < NUMBER_KANJIUNICODE; i++) {                       // The kanji
    if (kanji_unicode[i] == ch) {
      ch = i/94;
      return (((ch+0x30) << 8) | (i-ch*94)+0x21);
    }
  }
  for (i = 0; i < NUMBER_MISCUNICODE; i++) {                        // Symbol and pucntuation (main group)
    if (misc_unicode[i] == ch) {
      switch (i/94) {
        case 0: return (0x2121+i);
        case 1: return (0x2221+i-94);
        case 2: return (0x2821+i-94-94);
      }
    }
  }
  for (i = 0; i < NUMBER_EXTUNICODE; i++) {                         // Extneded ascii codes.
    if (ext_unicode[i] == ch) return (i+0x80);
  }
//
//  The Japanese ASCII needs to be here because some puncutation is stored within gaps
//   in the JASCII table, and if we move this before the misc codes they will not be found!    
//
  if ((ch >= 0xff10) && (ch <= 0xff5a)) return (ch-0xff10+0x2330);  // Japanese ASCII.
//
//  This converts Unicode's half-width katakana characters into full-width the same way as the SJIS/EUC conversion routines do except it does NOT attempt to read ahead in order to compose voiced morae.
//
#ifdef SUPPORT_HALFKATA
  if (ch >= 0xFF61 && ch <= 0xFF9F) {                               // Half-width katakana. Note that there are no precomposed characters in this set!
    extern byte mtable[][2];
    int c1,c2;
    c1 = mtable[ch-0xFF61][0];
    c2 = mtable[ch-0xFF61][1];
    sjis2jis (&c1,&c2);
    return (c1 << 8 | c2);
  }
#endif
  if ((ch == 0xfeff) || (ch == 0xfffe)) return (ch);                // UNICODE file markers.
  return (bad);                                                     // Invalid or untranslated UNICODE
}

//--------------------------------
//
//  Convert UNICODE data into UTF-8 value.  Because UTF-8 can take up to 4 bytes to store a value, 
//  this is a varing length conversion.  Further, note that 4 byte values are not supported, since
//  they lead to extended UNICODE which is not supported.
//
//  This routine is called with a pointer to a data array.  A single UNICODE value is written,
//  and the pointer is advanced to the next data to be written.
//
//      ptr -- On entry a pointer to the start of a UTF-8 encoded character.  On exit, this
//             will point to the next UTF character.
//      ch  -- Character value to be written.
//
#if 0       // For the moment only the jis2utf is necessary.  This may be required later.
void unicode2utf (byte *&ptr,int ch) {
  if      (!(ch & UTF8_WRITE1)) *ptr++ = ch;        // Single bite codes 0xxxxxxx
  else if (!(ch & UTF8_WRITE2)) {                   // Double byte codes 110xxxxx 10yyyyyy   
    *ptr++ = (UTF8_VALUE2 | (ch >> 6));
    *ptr++ = (UTF8_CONT   | (ch & 0x3f));
  }
  else {                                            // Three byte codes  1110xxxx 10yyyyyy 10zzzzzz
    *ptr++ = (UTF8_VALUE3 | (ch >> 12));
    *ptr++ = (UTF8_CONT   | ((ch >> 6) & 0x3f));
    *ptr++ = (UTF8_CONT   | (ch & 0x3f));
  }
  return;
}
#endif

//--------------------------------
//
//  Convert UTF-8 data into JIS value.  Because UTF-8 can take up to 4 bytes to store a value, 
//  this is a varing length conversion.  Further, note that 4 byte values are not supported, since
//  they lead to extended JIS which is not supported.
//
//  This routine is called with a pointer to a data array.  A single JIS value is extracted,
//  and the pointer is advanced to the next data to be analyzed.
//
//      ptr    -- On entry a pointer to the start of a UTF-8 encoded character.  On exit, this
//                will point to the next UTF character.
//
//      RETURN -- JIS charactger value or KANJI_BAD to indicate an error.
//
int utf2jis (byte *&ptr) {
  int i;
  if      (UTF8_VALUE1 == (*ptr & UTF8_MASK1)) {
    i = *ptr++;
  }
  else if (UTF8_VALUE2 == (*ptr & UTF8_MASK2)) {
    i = ((ptr[0] & 0x1f) << 6) | (ptr[1] & 0x3f);
    ptr += 2;
  }
  else if (UTF8_VALUE3 == (*ptr & UTF8_MASK3)) {
    i = ((ptr[0] & 0xf) << 12) | ((ptr[1] & 0x3f) << 6) | (ptr[2] & 0x3f);
    ptr += 3;
  }
  else if (UTF8_VALUE4 == (*ptr & UTF8_MASK4)) {
    i = 0xffff;
    ptr += 4;
  }
  return (unicode2jis(i,KANJI_BAD));
}

//--------------------------------
//
//  Convert UTF-8 data into UNICODE value.  Because UTF-8 can take up to 4 bytes to store a value, 
//  this is a varing length conversion.  Further, note that 4 byte values are not supported, since
//  they lead to extended UNICODE which is not supported.
//
//  This routine is called with a pointer to a data array.  A single UNICODE value is extracted,
//  and the pointer is advanced to the next data to be analyzed.
//
//      ptr    -- On entry a pointer to the start of a UTF-8 encoded character.  On exit, this
//                will point to the next UTF character.
//
//      RETURN -- UNICODE charactger value or 0xffff to indicate an error.
//
#if 0       // For the moment only the utf2jis is necessary.  This may be required later.
int utf2unicode (byte *&ptr) {
  int i;
  if      (UTF8_VALUE1 == (*ptr & UTF8_MASK1)) {
    i = *ptr++;
  }
  else if (UTF8_VALUE2 == (*ptr & UTF8_MASK2)) {
    i = ((ptr[0] & 0x1f) << 6) | (ptr[1] & 0x3f);
    ptr += 2;
  }
  else if (UTF8_VALUE3 == (*ptr & UTF8_MASK3)) {
    i = ((ptr[0] & 0xf) << 12) | ((ptr[1] & 0x3f) << 6) | (ptr[2] & 0x3f);
    ptr += 3;
  }
  else if (UTF8_VALUE4 == (*ptr & UTF8_MASK4)) {
    i = 0xffff;
    ptr += 4;
  }
  return (i);
}
#endif

//===================================================================
//
//  Begin Class JIS_convert.
//
//  This class implements the conver used for both input and output
//  of non-JWP format files.
//  

JIS_convert jis_convert;    // Class instance.

#define JIS_NL  10          // New Line char.
#define JIS_CR  13          // Carrage Return.
#define JIS_ESC 27          // Escape.
#define JIS_SS2 0x8E        // Half-width katakana marker.
#define EUC_212 0x8F        // JIS X 0212 initiator in EUC-JP

#define IS_EUC(c)       (((c) >= 161) && ((c) <= 254))      // EUC code for kanji
#define IS_HALFKATA(c)  (((c) >= 161) && ((c) <= 223))      // Range for half-width katakana (0xA1 - 0xDF).
#define IS_SJIS1(c)    ((((c) >= 129) && ((c) <= 159)) || (((c) >= 224) && ((c) <= 239)))   // JIS set #1
#define IS_SJIS2(c)     (((c) >= 64 ) && ((c) <= 252))      // JIS set #2
#define IS_MARU(c)      (((c) >= 202) && ((c) <= 206))      // Half-width katakana codes like pa, po, etc.
#define IS_NIGORI(c)   ((((c) >= 182) && ((c) <= 196)) || (((c) >= 202) && ((c) <= 206)) || ((c) == 179)) // Half-width katakana codes like ba, ga, etc.

#define UTF7_SHIFTIN    '+'         // Start UTF-7 shift sequence
#define UTF7_SHIFTOUT   '-'         // End UTF-7 shift sequence

//--------------------------------
//
//  Attempts to identify the type of a file.  This is done by attempting
//  to find a distinct escape code sequenc.  
//
//  I got this routine from Ken R. Lunde's jconv.c and have reformated
//  it and changed the style of the code slightly.  
//
//  To use this rouitne you must establish a input string for the 
//  rouginte's get_char() to operat on (such as calling input_file()).
//
//      RETURN -- Type of file.  If the file type cannot be identifed,
//                FILETYPE_ASCII will be returned, indicating that the
//                file has no Japanese escape sequences.  
//
//                The ambiquity between detecting EUC and Shift-JIS
//                file types is performed by defaulting to FILETYPE_SJS.
//
#define FILETYPE_EUCORSJIS  20  // Internal type used when we have determined
                                //   file is not ascii, but cannot tell if
                                //   it is EUC or Shift-JIS.

int JIS_convert::find_type () {
  int type = FILETYPE_ASCII;
  int c    = 0;
  int t;
  if ((t = is_unicode())) return (t);
  rewind ();
  if (is_utf8()) return (FILETYPE_UTF8);
  rewind ();
  while ((type == FILETYPE_EUCORSJIS) || (type == FILETYPE_ASCII)) {
    if (JIS_EOF == (c = get_char())) break;
    if (c == JIS_ESC) {
      c = get_char();
      if (c == '$') {
        c = get_char();
        if (c == 'B') return (FILETYPE_JIS);                    // ESC-$-B -> New JIS type.
        else if (c == '@') return (FILETYPE_OLD);               // ESC-$-@ -> Old JIS type.
      }
      else if (c == 'K') return (FILETYPE_NEC);                 // ESC-K -> JIS-NEC type. 
    }
    else if (c == JIS_SS2) {                                    // Halfwidth katakana escpae (Shift-JIS or EUC)
      c = get_char();
      if (c >= 161 && c <= 223) type = FILETYPE_EUCORSJIS;
      else if ((c != 127) && ((c >= 64) && (c <= 252))) return (FILETYPE_SJS);
    }
//
//  From here on we are trying to determine EUC or Shift-JIS (or nothing) 
//  base on the range of characters and tehir folliwng characers.
//
    else if ((c >= 129) && (c <= 159)) return (FILETYPE_SJS);   
    else if ((c >= 161) && (c <= 223)) {                        
      c = get_char();
      if ((c >= 240) && (c <= 254)) return (FILETYPE_EUC);
      else if ((c >= 161) && (c <= 223)) type = FILETYPE_EUCORSJIS;
      else if ((c >= 224) && (c <= 239)) {
        type = FILETYPE_EUCORSJIS;
        while ((c >= 64) && (type == FILETYPE_EUCORSJIS)) {
          if (c >= 129) {
            if ((c <= 141) || ((c >= 143) && (c <= 159))) return (FILETYPE_SJS);
            else if ((c >= 253) && (c <= 254)) return (FILETYPE_EUC);
          }
          if (JIS_EOF == (c = get_char())) break;
        }
      }
      else if (c <= 159) return (FILETYPE_SJS);
    }
    else if ((c >= 240) && (c <= 254)) return (FILETYPE_EUC);
    else if ((c >= 224) && (c <= 239)) {
      c = get_char();
      if (((c >= 64) && (c <= 126)) || ((c >= 128) && (c <= 160))) return (FILETYPE_SJS);
      else if ((c >= 253) && (c <= 254)) return (FILETYPE_EUC);
      else if ((c >= 161) && (c <= 252)) type = FILETYPE_EUCORSJIS;
    }
  }
//
//  Reached end of file.  If have identied as either EUC or Shift-JIS
//  then resolve to EUC.  Otherwise return default (should be ascii).
//
  if (type == FILETYPE_EUCORSJIS) type = FILETYPE_SJS;
  return (type);
}

//--------------------------------
//
//  This routine gets a number of bits from the bit-buffer.  Currently, the bit-buffer is 
//  only use by UTF-7 format, but this may change at some time.
//
//      count  -- Number of bits to extract from the buffer.
//      
//      RETURN -- Extracted bits.
//  
int JIS_convert::get_bits (int count) {
  int mask;
  mask  = (1 << count)-1;
  bits -= count;
  return ((bit_buffer >> bits) & mask);
}

//--------------------------------
//
//  This routine converts codes for half-width katakana used in the 
//  Shift-JIS and EUC file types.  These codes are embedded in a 
//  file in a differnt way.  In particular, the acents (ten-ten (nigori)
//  and maru are encoded into following characters.  This requires a 
//  look ahead in the file, which necesitates special handling of 
//  the input buffer in these cases.  These half-width katakana codes
//  do not appear to be heavially used.
//
//      p1,p2 -- Pointer to two bytes of input.  On exit they will
//               point to a JIS character code.
//
//  I got this routine from Ken R. Lunde's jconv.c and have reformated
//  it and changed the style of the code slightly.  
//
#ifdef SUPPORT_HALFKATA
  byte mtable[][2] = {
    {129,66 },{129,117},{129,118},{129,65 },{129,69 },{131,146},{131,64 },
    {131,66 },{131,68 },{131,70 },{131,72 },{131,131},{131,133},{131,135},
    {131,98 },{129,91 },{131,65 },{131,67 },{131,69 },{131,71 },{131,73 },
    {131,74 },{131,76 },{131,78 },{131,80 },{131,82 },{131,84 },{131,86 },
    {131,88 },{131,90 },{131,92 },{131,94 },{131,96 },{131,99 },{131,101},
    {131,103},{131,105},{131,106},{131,107},{131,108},{131,109},
    {131,110},{131,113},{131,116},{131,119},{131,122},{131,125},
    {131,126},{131,128},{131,129},{131,130},{131,132},{131,134},
    {131,136},{131,137},{131,138},{131,139},{131,140},{131,141},
    {131,143},{131,147},{129,74 },{129,75 }
  };
void JIS_convert::half2full (int *p1,int *p2) {
  int junk;
  int c1     = *p1;
  int maru   = false;
  int nigori = false;
//
//  Merged routine used with the half2full() function.  This procedure
//  makes sure that you will be able to undo the next two characters
//  This allows the undoing of some character translations.  This routine
//  works by checking the number of bytes left in the buffer.  If there 
//  are too few bytes, the contents of the buffer are shifted, and more 
//  data is read into the upper part of the buffer.
//
  *p2 = JIS_EOF;                                      // Make sure this is consistent in case we're at the end of the input.
  if ((position+1 >= size) && reload()) goto Convert; // No second character - jump directly to conversion.
//
//  Look for nigori (ten-ten) or maru characters.
//
  if (type == FILETYPE_SJS) {
    *p2 = get_char();
    if (*p2 == 222) {
      if (IS_NIGORI(*p1)) nigori = true; else unget_char();
    }
    else if (*p2 == 223) {
      if (IS_MARU(*p1)) maru = true; else unget_char();
    }
    else unget_char();
  }
  else {                            // type == FILETYPE_EUC
    junk = get_char();
    if (junk == JIS_SS2) {
      *p2 = get_char();
      if (*p2 == 222) {
        if (IS_NIGORI(*p1)) nigori = true;
          else {
            unget_char();
            unget_char();
          }
      }
      else if (*p2 == 223) {
        if (IS_MARU(*p1)) maru = true;
          else {
            unget_char();
            unget_char();
          }
      }
      else {
        unget_char();
        unget_char();
      }
    }
    else
      unget_char();
  }
Convert:
//
//  Convert to Shift-JIS code
//
  if ((*p1 >= 161) && (*p1 <= 223)) {
    *p1 = mtable[c1-161][0];
    *p2 = mtable[c1-161][1];
  }
//
//  Modify for nigori and maru
//
  if (nigori) {
    if (((*p2 >= 74) && (*p2 <= 103)) || ((*p2 >= 110) && (*p2 <= 122))) (*p2)++;
    else if ((*p1 == 131) && (*p2 == 69)) *p2 = 148;
  }
  else if (maru && (*p2 >= 110) && (*p2 <= 122)) *p2 += 2;
//
//  Convert to JIS character code.
//
  sjis2jis (p1,p2);
  return;
}
#endif SUPPORT_HALFKATA

//--------------------------------
//
//  This attempts to convert certain 3-byte EUC sequences initiated by 0x8f.
//
int convert_EUC_0212 (int ch,int c2)
{
  if (ch == 0xa2) switch (c2) {
    case 0xed: return (0xa9);  // copyright
    case 0xee: return (0xae);  // registered trademark
    case 0xef: return (0x99);  // registered trademark
  }
  else if (ch == 0xa9) switch (c2) {
    case 0xa1: return (0xc6);  // AE
    case 0xa2: return (0xd0);  // eth majuscule
    case 0xac: return (0xd8);  // O stroke
    case 0xad: return (0x8c);  // OE
    case 0xb0: return (0xde);  // thorn majuscule
    case 0xc1: return (0xe6);  // ae
    case 0xc2: return (0xf0);  // eth minuscule
    case 0xcc: return (0xf8);  // o stroke
    case 0xcd: return (0x9c);  // oe
    case 0xce: return (0xdf);  // ss
    case 0xd0: return (0xfe);  // thorn minuscule
  }
  else if (ch == 0xaa) switch (c2) {// majuscule Latin glyphs
    case 0xa1: return (0xc1);  // A acute
    case 0xa2: return (0xc0);  // A grave
    case 0xa3: return (0xc4);  // A umlaut
    case 0xa4: return (0xc2);  // A circumflex
    case 0xa9: return (0xc5);  // A ring
    case 0xaa: return (0xc3);  // A tilde
    case 0xae: return (0xc7);  // C cedilla
    case 0xb1: return (0xc9);  // E acute
    case 0xb2: return (0xc8);  // E grave
    case 0xb3: return (0xcb);  // E umlaut
    case 0xb4: return (0xca);  // E circumflex
    case 0xbf: return (0xcd);  // I acute
    case 0xc0: return (0xcc);  // I grave
    case 0xc1: return (0xcf);  // I umlaut
    case 0xc2: return (0xce);  // I circumflex
    case 0xd0: return (0xd1);  // N tilde
    case 0xd1: return (0xd3);  // O acute
    case 0xd2: return (0xd2);  // O grave
    case 0xd3: return (0xd6);  // O umlaut
    case 0xd4: return (0xd4);  // O circumflex
    case 0xd7: return (0xd4);  // O macron (kludged with circumflex) (ambiguous!)
    case 0xd8: return (0xd5);  // O tilde
    case 0xde: return (0x8a);  // S caron
    case 0xe2: return (0xda);  // U acute
    case 0xe3: return (0xd9);  // U grave
    case 0xe4: return (0xdc);  // U umlaut
    case 0xe5: return (0xdb);  // U circumflex
    case 0xf2: return (0xdd);  // Y acute
    case 0xf3: return (0x9f);  // Y umlaut
    case 0xf6: return (0x8e);  // Z caron
  }
  else if (ch == 0xab) switch (c2) {// minuscule
    case 0xa1: return (0xe1);  // a acute
    case 0xa2: return (0xe0);  // a grave
    case 0xa3: return (0xe4);  // a umlaut
    case 0xa4: return (0xe2);  // a circumflex
    case 0xa7: return (0xe2);  // a macron (kludged with circumflex) (ambiguous!)
    case 0xa9: return (0xe5);  // a ring
    case 0xaa: return (0xe3);  // a tilde
    case 0xae: return (0xe7);  // c cedilla
    case 0xb1: return (0xe9);  // e acute
    case 0xb2: return (0xe8);  // e grave
    case 0xb3: return (0xeb);  // e umlaut
    case 0xb4: return (0xea);  // e circumflex
    case 0xb7: return (0xea);  // e macron (kludged with circumflex) (ambiguous!)
    case 0xbf: return (0xed);  // i acute
    case 0xc0: return (0xec);  // i grave
    case 0xc1: return (0xef);  // i umlaut
    case 0xc2: return (0xee);  // i circumflex
    case 0xd0: return (0xf1);  // n tilde
    case 0xd1: return (0xf3);  // o acute
    case 0xd2: return (0xf2);  // o grave
    case 0xd3: return (0xf6);  // o umlaut
    case 0xd4: return (0xf4);  // o circumflex
    case 0xd7: return (0xf4);  // o macron (kludged with circumflex) (ambiguous!)
    case 0xd8: return (0xf5);  // o tilde
    case 0xde: return (0x9a);  // s caron
    case 0xe2: return (0xfa);  // u acute
    case 0xe3: return (0xf9);  // u grave
    case 0xe4: return (0xfc);  // u umlaut
    case 0xe5: return (0xfb);  // u circumflex
    case 0xe9: return (0xfb);  // u macron (kludged with circumflex) (ambiguous!)
    case 0xf2: return (0xfd);  // y acute
    case 0xf3: return (0xff);  // y umlaut
    case 0xf6: return (0x9e);  // z caron
  }
  return (KANJI_BAD);  // Search for this character in EDICT/ENAMDICT (opening the file directly) to check for missing conversions.
}

//--------------------------------
//
//  This is the reverse of the above.
//  It tries to find a JIS X 0212 encoding for the input.
//
int encode_EUC_0212 (int in,int&o1)
{
  o1 = 0xa2; switch (in) {
    case 0xa9: return (0xed);  // copyright
    case 0xae: return (0xee);  // registered trademark
    case 0x99: return (0xef);  // registered trademark
  }
  o1 = 0xa9; switch (in) {
    case 0xc6: return (0xa1);  // AE
    case 0xd0: return (0xa2);  // eth majuscule
    case 0xd8: return (0xac);  // O stroke
    case 0x8c: return (0xad);  // OE
    case 0xde: return (0xb0);  // thorn majuscule
    case 0xe6: return (0xc1);  // ae
    case 0xf0: return (0xc2);  // eth minuscule
    case 0xf8: return (0xcc);  // o stroke
    case 0x9c: return (0xcd);  // oe
    case 0xdf: return (0xce);  // ss
    case 0xfe: return (0xd0);  // thorn minuscule
  }
  o1 = 0xaa; switch (in) {
    case 0xc1: return (0xa1);  // A acute
    case 0xc0: return (0xa2);  // A grave
    case 0xc4: return (0xa3);  // A umlaut
    case 0xc2: return (0xa4);  // A circumflex
    case 0xc5: return (0xa9);  // A ring
    case 0xc3: return (0xaa);  // A tilde
    case 0xc7: return (0xae);  // C cedilla
    case 0xc9: return (0xb1);  // E acute
    case 0xc8: return (0xb2);  // E grave
    case 0xcb: return (0xb3);  // E umlaut
    case 0xca: return (0xb4);  // E circumflex
    case 0xcd: return (0xbf);  // I acute
    case 0xcc: return (0xc0);  // I grave
    case 0xcf: return (0xc1);  // I umlaut
    case 0xce: return (0xc2);  // I circumflex
    case 0xd1: return (0xd0);  // N tilde
    case 0xd3: return (0xd1);  // O acute
    case 0xd2: return (0xd2);  // O grave
    case 0xd6: return (0xd3);  // O umlaut
    case 0xd4: return (0xd4);  // O circumflex
//  case 0xd4: return (0xd7);  // O macron (kludged with circumflex) (ambiguous!)
    case 0xd5: return (0xd8);  // O tilde
    case 0x8a: return (0xde);  // S caron
    case 0xda: return (0xe2);  // U acute
    case 0xd9: return (0xe3);  // U grave
    case 0xdc: return (0xe4);  // U umlaut
    case 0xdb: return (0xe5);  // U circumflex
    case 0xdd: return (0xf2);  // Y acute
    case 0x9f: return (0xf3);  // Y umlaut
    case 0x8e: return (0xf6);  // Z caron
  }
  o1 = 0xab; switch (in) {
    case 0xe1: return (0xa1);  // a acute
    case 0xe0: return (0xa2);  // a grave
    case 0xe4: return (0xa3);  // a umlaut
    case 0xe2: return (0xa4);  // a circumflex
//  case 0xe2: return (0xa7);  // a macron (kludged with circumflex) (ambiguous!)
    case 0xe5: return (0xa9);  // a ring
    case 0xe3: return (0xaa);  // a tilde
    case 0xe7: return (0xae);  // c cedilla
    case 0xe9: return (0xb1);  // e acute
    case 0xe8: return (0xb2);  // e grave
    case 0xeb: return (0xb3);  // e umlaut
    case 0xea: return (0xb4);  // e circumflex
//  case 0xea: return (0xb7);  // e macron (kludged with circumflex) (ambiguous!)
    case 0xed: return (0xbf);  // i acute
    case 0xec: return (0xc0);  // i grave
    case 0xef: return (0xc1);  // i umlaut
    case 0xee: return (0xc2);  // i circumflex
    case 0xf1: return (0xd0);  // n tilde
    case 0xf3: return (0xd1);  // o acute
    case 0xf2: return (0xd2);  // o grave
    case 0xf6: return (0xd3);  // o umlaut
    case 0xf4: return (0xd4);  // o circumflex
//  case 0xf4: return (0xd7);  // o macron (kludged with circumflex) (ambiguous!)
    case 0xf5: return (0xd8);  // o tilde
    case 0x9a: return (0xde);  // s caron
    case 0xfa: return (0xe2);  // u acute
    case 0xf9: return (0xe3);  // u grave
    case 0xfc: return (0xe4);  // u umlaut
    case 0xfb: return (0xe5);  // u circumflex
//  case 0xfb: return (0xe9);  // u macron (kludged with circumflex) (ambiguous!)
    case 0xfd: return (0xf2);  // y acute
    case 0xff: return (0xf3);  // y umlaut
    case 0x9e: return (0xf6);  // z caron
  }
  return 0;
}

//--------------------------------
//
//  This is the main input driver for reading input streams.  This is
//  called for each character to be read.  This routine gets characters
//  from the input stream and translates them into JIS characters and 
//  returns the values.  When end-of-file is reached, the routine 
//  conintually returns JIS_EOF (-1).
//
//      RETURN -- Kanji character read from stream.
//
//  I got much of this routine from Ken R. Lunde's jconv.c.  The code
//  has been substansially reformated to fit this programingmodel.
//
int JIS_convert::input_char () {
  static char invbase64[128] = { -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,    // 00..0f
                                 -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,    // 10..1f
                                 -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,    // 20..2f
                                 52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,    // 30..3f
                                 -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,    // 40..4f
                                 15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,    // 50..5f
                                 -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,    // 60..6f
                                 41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1 };  // 70..6f
  int ch,c2;
  while (true) {                                    // Loop until we have a character.
    if (type == FILETYPE_UNICODE) set_size(2);      // Read two byte characters
    ch = get_char();
    if (JIS_EOF == ch) return (JIS_EOF);
    switch (type) {
      default:   // FILETYPE_ASCII
           return (ch);
      case FILETYPE_UTF7: {
             int first = false;                                         // This indicates we are not first character in a shifted sequence.
             while (ch != JIS_EOF) {                                    // Continue until EOF or until we have a character to report.
               if (in_twobyte) {                                        // We are in a shifted sequence so continue sequence or exit sequence
                 if ((ch > 0x7f) || (invbase64[ch] < 0)) {              // Not a valid base-64 character so exit shift.
                   in_twobyte = false;
                   if ((ch == UTF7_SHIFTIN) || (ch == UTF7_SHIFTOUT)) { // Exiting shift sequence
                     if (first) return (UTF7_SHIFTIN);                  // If SHIFTIN+SHIFTOUT / SHIFTIN+SHIFTIN -> output shift out.
                   }
                 }
                 else {                                                 // Just base-64 data
                   put_bits (invbase64[ch],6);                          // Add data to bit-buffer
                   first = false;                                            // Not first character
                 }
                 if (bits >= 16) {                                      // If there are 16 bits in buffer, get the data.
                   return (unicode2jis(get_bits(16),KANJI_BAD));
                 }
               }
               else {                                                   // Not in shift sequence.
                 if (ch != UTF7_SHIFTIN) return (ch);                   // Just data so return it.
                   else {                                                 
                     in_twobyte = true;                                 // Start shift sequence
                     first      = true;                                 // This is the first character in a shift
                     bits       = 0;                                    // Clear the bit buffer.
                   }
               }
               ch = get_char();                                         // Get next character.
             }
             return (JIS_EOF); 
           }
      case FILETYPE_JFC:
      case FILETYPE_UTF8:
           if (UTF8_VALUE2 == (ch & UTF8_MASK2)) {
             c2 = get_char();
             ch = ((ch & 0x1f) << 6) | (c2 & 0x3f);
           }
           else if (UTF8_VALUE3 == (ch & UTF8_MASK3)) {
             c2 = get_char();
             ch = ((ch & 0x0f) << 12) | ((c2 & 0x3f) << 6) | (get_char() & 0x3f);
           }
           else if (UTF8_VALUE4 == (ch & UTF8_MASK4)) {
             get_char();
             get_char();
             get_char();
             return (KANJI_BAD);
           }
      case FILETYPE_UNICODE:                        
           if (reverce_bytes) ch = (ch >> 8) | ((ch & 0xff) << 8);  // Do we need to swap the byte order.
           return (unicode2jis(ch,KANJI_BAD));                      // I used to return the UNICODE value, but now we return KANJI_BAD.
      case FILETYPE_JFCEUC:
      case FILETYPE_EUC:
           if (IS_EUC(ch)) {
             if ((c2 = get_char()) < 0) return (KANJI_BAD);
             return (((ch << 8) | c2) & 0x7f7f);    // Kanji code
           }
           else if (ch == EUC_212) {
             if ((ch = get_char()) < 0) return (KANJI_BAD);
             if ((c2 = get_char()) < 0) return (KANJI_BAD);
             return convert_EUC_0212 (ch,c2);
           }
#ifdef SUPPORT_HALFKATA
           else if (ch == JIS_SS2) {                // Half-width katakana
             if ((c2 = get_char()) < 0) return (KANJI_BAD);
             if (IS_HALFKATA(c2)) {
               ch = c2;
               half2full (&ch,&c2);
               return ((ch << 8) | c2);
             }
#else SUPPORT_HALFKATA
           else if (ch == JIS_SS2) {                // JWPce encodes 8-bit ASCII this way on output; not sure if it's standards-compliant though.
             if ((c2 = get_char()) < 0) return (KANJI_BAD);
#endif SUPPORT_HALFKATA
             return (c2 | 0x80);                    // (But, until version 1.64, it couldn't reconstruct its own output for this particular case.)
           }
           return (ch);                             // Ascii fall-through
      case FILETYPE_SJS:
           if (IS_SJIS1(ch)) {                      // Kanji character
             if ((c2 = get_char()) < 0) return (KANJI_BAD);
             if (IS_SJIS2(c2)) sjis2jis (&ch,&c2);
             return ((ch << 8) | c2); 
           }
#ifdef SUPPORT_HALFKATA
           else if (IS_HALFKATA(ch)) {              // Half-width katakana
             half2full (&ch,&c2);
             return ((ch << 8) | c2);
           }
#endif SUPPORT_HALFKATA
           return (ch);                             // Ascii fall through
      case FILETYPE_JIS:
      case FILETYPE_OLD:
      case FILETYPE_NEC:
           if (ch == JIS_ESC) {                     // Start kanji escape sequence
             c2 = get_char();
             if ((c2 == '$') || (c2 == '(')) get_char();
             if ((c2 == 'K') || (c2 == '$')) in_twobyte = true; else in_twobyte = false;
           }
           else if ((ch == JIS_NL) || (ch == JIS_CR)) { // CR/NL stop escape sequence
             in_twobyte = false;
             return (ch);
           }
           else {                                   // kanji and ascii read.
             if (in_twobyte) return ((ch << 8) | get_char()); else return (ch);
           }
    }
  }
}

#define UNIRNG(a,b) (c >= 0x##a && c <= 0x##b)

// This is intended to improve Unicode autodetection by allowing certain unconvertible Unicode characters occasionally found in Japanese text.
static bool excusable (int c) {
  if (c < 0 || c > 0xFFFF) return (false);
  if (UNIRNG(2160,2169) || UNIRNG(2170,2179) || UNIRNG(2460,2473)) return (true);  // Roman numerals and circled numbers
  switch (c) {
    case 0x2027:      // hyphenation point
    case 0x267A:      // (unknown)
    case 0x2764:      // heart
    case 0xFF0D:      // fullwidth hyphen/minus
      return (true);  // Excuse certain Unicode characters which derail clipboard autodetection because they don't map to JIS.
  }
  return (false);
}

//--------------------------------
//
//  Attempt to see if the item connected to the buffer is a Unicode
//  object.
//
//      RETURN -- A zero value indicates this is not UNICODE data.
//                A non-zero value indicates that this is unicode data, but there
//                are two possible return values:
//
//              FILETYPE_UNICODE  -- Normal PC UNICODE.
//              FILETYPE_UNICODER -- Motorola UNIOCDE, the byte order is reverced.
//
int JIS_convert::is_unicode () {
  int ch;
  int eucsjis = true;       // Contains only euc/shift-jis charactrers
  int csize   = charsize;   // Current character size.
  set_size (2);                                     // Use two byte reads.
  ch = get_char();
  if (ch == 0xfeff) { charsize = csize; return (FILETYPE_UNICODE ); }   // This is a unicode file because of the ID 
  if (ch == 0xfffe) { charsize = csize; return (FILETYPE_UNICODER); }   // Unicode reverced byte order flag.
  while (ch != JIS_EOF) {
    if (!unicode2jis(ch,0) && !excusable(ch)) {                         // Does this character lack a JIS equivalent?
      charsize = csize;
      return (false);                                                   // Assume text is not Unicode.
    }
    if ((ch & 0x8080) != 0x8080) eucsjis = false;                       // A non-euc/shift-jis character.
    ch = get_char();
  }
  charsize = csize;                                                     // Back to client chosen character size.
  if (eucsjis) return (false);
  return (FILETYPE_UNICODE);
}

//--------------------------------
//
//  Attempt to see if the item connected to the buffer is in UTF-8 format.
//
//      RETURN -- A zero value indicates this is UTF-8 data.
//
//  Because JIS_EOF is -1, this will cause an EOF condition to fail the tests, since all 
//  of the UTF-8 codes have at least one zero in them.  This checker is nice in that it 
//  can check just the format without having to look at the actual unicode data.
//
#define IS_UTF8(ch)     (0x80 == (ch & 0xc0))   // Test for valid second or third UTF-8 byte

int JIS_convert::is_utf8 () {
  int ch,i,j;
  int ascii_only = true;
  while (JIS_EOF != (ch = get_char())) {
    if      (UTF8_VALUE1 == (ch & UTF8_MASK1)) i = 0;   // Determine number of bytes we have.
    else if (UTF8_VALUE2 == (ch & UTF8_MASK2)) i = 1;
    else if (UTF8_VALUE3 == (ch & UTF8_MASK3)) i = 2;
    else if (UTF8_VALUE4 == (ch & UTF8_MASK4)) i = 3;
    else return (false);                                // This must be smething like 1111 (not valid).
    if (i) ascii_only = false;
    for (j = 0; j < i; j++) {                           // Check following bytes for correct format.
      ch = get_char();
      if (!IS_UTF8(ch)) return (false);
    }
  }
  return (!ascii_only);
}

//--------------------------------
//
//  This is the main output driver for writing output streams.  This is
//  called for each character to be output.  This routien translates
//  the file characters to the alternative file and writes them out
//  to the output stream.
//
//      ch -- Kanji character to output.
//
//  I got much of this routine from Ken R. Lunde's jconv.c.  The code
//  has been substansially reformated to fit this programing model.
//
// ### size/speed improvement is to have put_char mask against 0xff.  This would elimiate this from a number of lines.
void JIS_convert::output_char (int ch) {
  static char base64[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static char mustshift[128] = { 1,1,1,1, 1,1,1,1, 1,0,0,1, 1,0,1,1,    // 00..0f
                                 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    // 10..1f
                                 0,1,1,1, 1,1,1,0, 0,0,1,1, 0,0,0,0,    // 20..2f
                                 0,0,0,0, 0,0,0,0, 0,0,0,1, 1,1,1,0,    // 30..3f
                                 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    // 40..4f
                                 0,0,0,0, 0,0,0,0, 0,0,0,1, 1,1,1,1,    // 50..5f
                                 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    // 60..6f
                                 0,0,0,0, 0,0,0,0, 0,0,0,1, 1,1,1,1 };  // 70..7f
  switch (type) {
//
//  A special node about the UTF-7 writer.  Normally, we would need a flush command to make 
//  sure the bit buffer is correctly flushed, but in this case we don't.  This is because 
//  CR/LF are written out of shift mode, thus the bit-buffer is flushed before these are
//  written.  Since JWPce buts end-of-line at the end of each paragraph, we don't need to 
//  worry about flushing the buffer since it will automatically be flushed!
//
    case FILETYPE_UTF7: {
           int needshift;
           ch = jis2unicode(ch);                                // Covert to UNICODE
           needshift = (ch >= 0x7f) || mustshift[ch];           // Does this character have to be shifted.
           if (needshift && !in_twobyte) {                      // Need shift but are not currently shifted.
             put_char (UTF7_SHIFTIN);                           //   Generate shift
             if (ch == UTF7_SHIFTIN) put_char (UTF7_SHIFTOUT);  //   Special case for SHIFTIN charcter
               else {
                 bits       = 0;                                //   Clear buffer.  This allows us to be sloppy on the end (below)
                 in_twobyte = true;                             //   All other cases, we are now shifted.
               }
           }
           if (in_twobyte) {                                    // In shifted mode.
             if (needshift) put_bits (ch,16);                   //   Character needs shifting so add to bits
               else if (bits) put_bits (0,6);                   //   Going out of shift, so fill buffer for last character
                                                                //     We don't need to fill exactly, because when we shift
                                                                //     again, the buffer will be cleared 
             while (bits >= 6) put_char (base64[get_bits(6)]);  //   Extract as many characters from buffer as possilbe
             if (!needshift) {                                  //   We do not need a shifted charter so take us out of shift
               put_char (UTF7_SHIFTOUT);
               in_twobyte = false;
             }
           }
           if (!needshift) put_char (ch);                       // Put a simple character.
           return;
         }
    case FILETYPE_JFC:
    case FILETYPE_UTF8:
         ch = jis2unicode(ch);                          // Convert to UNICODE
         if      (!(ch & UTF8_WRITE1)) put_char (ch);   // Single bite codes 0xxxxxxx
         else if (!(ch & UTF8_WRITE2)) {                // Double byte codes 110xxxxx 10yyyyyy   
           put_char (UTF8_VALUE2 | (ch >> 6));   
           put_char (UTF8_VALUEC | (ch & 0x3f));
         }
         else {                                         // Three byte codes  1110xxxx 10yyyyyy 10zzzzzz
           put_char (UTF8_VALUE3 | (ch >> 12));
           put_char (UTF8_VALUEC | ((ch >> 6) & 0x3f));
           put_char (UTF8_VALUEC | (ch & 0x3f));
         }
         return;
    case FILETYPE_UNICODE: 
         put_char (jis2unicode(ch));            // Do everything.
         return;
    case FILETYPE_EUC:
         if (ISJIS(ch)) {                       // Output kanji.
           put_char ((ch >> 8) | 0x80);
           put_char ((ch & 0xff) | 0x80);
         }
         else if (ch & 0x80) {                  // Output ascii character with high bit set.
           int e2, e3;
           if (e3 = encode_EUC_0212 (ch,e2)) {  // Try JIS X 0212 first
             put_char (EUC_212);
             put_char (e2);
             put_char (e3);
           } else {
             put_char (JIS_SS2);                // Use an alternative encoding method that couldn't even be reconstructed on input until JWPxp version 1.64!
             put_char (ch & 0x7f);
           }
         }
         else {                                 // Ascii output
           put_char (ch);
         }
         return;
    default:    // FILETYPE_ASCII
    case FILETYPE_SJS:
         if (ISJIS(ch)) {                       // Output kanji
           ch = jis2sjis(ch);
           put_char (ch >> 8);
           put_char (ch & 0xff);
         }
         else {                                 // Output ascii.
           put_char (ch);
         }
         return;
    case FILETYPE_JIS:
    case FILETYPE_OLD:
    case FILETYPE_NEC:
         if ((ch == '\n') || (ch == '\r')) {    // Stop escape sequence and output char.
           put_end ();
           put_char (ch);
         }
         else if (ISJIS(ch)) {                  // Output kanji
           put_start ();
           put_char  (ch >> 8);
           put_char  (ch & 0xff);
         }
         else {                                 // Output ascii
           put_end  ();
           put_char (ch);
         }
         return;
  }
  return;
}

//--------------------------------
//
//  Internal routine that outputs the appropriate JIS escape sequence 
//  to end a two-byte sequence.  The routine is smart enough to do 
//  nothing if we are not within a two-byte sequence.
//
void JIS_convert::put_end () {
  if (!in_twobyte) return;
  in_twobyte = false;
  put_char (JIS_ESC);
  if (type == FILETYPE_NEC) put_char ('H');
    else {
      put_char ('(');
      put_char ('J');
    }

  return;
}

//--------------------------------
//
//  This routine puts a number of bits into the bit-buffer.  Currently, the bit-buffer is 
//  only used by UTF-7 format, but this may change at some time.
//
//      value -- Value to write.
//      count -- Number of bits to write (always the least significant bits).
//
void JIS_convert::put_bits (int value,int count) {
  int mask;
  mask = (1 << count)-1;
  bit_buffer = (bit_buffer << count) | (value & mask);
  bits += count;
  return;
}

//--------------------------------
//
//  Internal routine used in the writing of JIS output streams.  This
//  routine starts an escape sequence that is appropriate for the 
//  current file type.
//
//  The routine is smart enough to know when we are already in an
//  escape sequence, and do nothing.
//  
void JIS_convert::put_start () {
  if (in_twobyte) return;       
  in_twobyte = true;
  put_char (JIS_ESC);
  if (type == FILETYPE_NEC) put_char ('K');
    else {
      put_char ('$');
      if (type == FILETYPE_OLD) put_char ('@'); else put_char ('B');
    }
  return;
}

//--------------------------------
//
//  Setup the type of conversion to be used on the input/output 
//  stream.
//
//      filetype -- Type of conversion to be used.
//
void JIS_convert::set_type (int filetype) {
  type          = (byte) (filetype & FILETYPE_TYPEMASK);
  in_twobyte    = false;
  bits          = 0;
  reverce_bytes = filetype & FILETYPE_REVERCE;
  set_size ((type == FILETYPE_UNICODE) ? 2 : 1);
  return;
}

//--------------------------------
//
//  This is a small routine to insert the UNICODE file ID when the
//  file is written.  The UNICODE id is simply a 0xfeff character
//  at the beginning of the file.
//
void JIS_convert::unicode_write () {
  if ((type != FILETYPE_UNICODE) || (handle == INVALID_HANDLE_VALUE)) return;
  put_char (0xfeff);
  return;
}

// IMPROVE -- Check JIS ending string.  Maybe expand JIS conversion system.


