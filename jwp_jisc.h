//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//  The code do do conversion between ECU, JIS, and Shift-JIS        //
//  was taken from jconv.c which is copyright by Ken R. Lunde,       //
//  Adobe Systems Incorporated.  Full copyright notice is cpp file.  //
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
#ifndef jwp_jisc_h
#define jwp_jisc_h

#include "jwp_cach.h"   // Needed because the JIS_convert class is base on the IO_cache class.

                                    // File types
#define FILETYPE_UNNAMED    0x00    // Unnamed file (from New).
#define FILETYPE_AUTODETECT 0x01    // Auto-detect file type (load only).
#define FILETYPE_JTYPES     0x02    // Japanese file types (load only).
#define FILETYPE_ASCII      0x03    // Ascii file (load only).
#define FILETYPE_NORMAL     0x04    // Normal JPWce file type.
#define FILETYPE_JWP        0x05    // Normal .JWP file type.
#define FILETYPE_EUC        0x06    // EUC file type.
#define FILETYPE_SJS        0x07    // Shift-JIS file type.
#define FILETYPE_JIS        0x08    // New JIS file type.
#define FILETYPE_OLD        0x09    // Old JIS file type.
#define FILETYPE_NEC        0x0a    // NEC JIS file type.
#define FILETYPE_UNICODE    0x0b    // Unicode file type.
#define FILETYPE_UNICODER   0x2b    // Unicode file type with reverced byte order.
#define FILETYPE_UTF7       0x0c    // UTF-7 file type.
#define FILETYPE_UTF8       0x0d    // UTF-8 file type.
#define FILETYPE_JFC        0x0e    // JFC file type (UTF-8 format)
#define FILETYPE_JFCEUC     0x2e    // JFC file type (EUC format)
#define FILETYPE_PROJECT    0x0f    // Project file
#define FILETYPE_REVERCE    0x20    // File type has erverce byte order.  Only used for 16-bit formats.
#define FILETYPE_EDIT       0x40    // Edit control type.
#define FILETYPE_WORK       0x41    // Working file type (clipboard, etc.)
#define FILETYPE_WORKMASK   0x40    // Working file mask.
#define FILETYPE_TYPEMASK   0x0f    // Mask to get actual working types for files.
#define IS_WORKFILE(x)  ((x) & FILETYPE_WORKMASK)   // Test for working type file

#define UNICODE_BAD         0xffff  // Unicode value representing an error.

                                    // UTF-8 encouding values. 
#define UTF8_VALUE1     0x00        // Value for set bits for single byte UTF-8 Code.
#define UTF8_MASK1      0x80        // Mask (i.e. bits not set by the standard) 0xxxxxxx
#define UTF8_WRITE1     0xff80      // Mask of bits we cannot allow if we are going to write one byte code
#define UTF8_VALUE2     0xc0        // Two byte codes
#define UTF8_MASK2      0xe0        // 110xxxxx 10yyyyyy
#define UTF8_WRITE2     0xf800      // Mask of mits we cannot allow if we are going to write two byte code
#define UTF8_VALUE3     0xe0        // Three byte codes    
#define UTF8_MASK3      0xf0        // 1110xxxx 10yyyyyy 10zzzzzz
#define UTF8_VALUE4     0xf0        // Four byte values
#define UTF8_MASK4      0xf8        // 11110xxx ----    (These values are not supported by JWPce).    
#define UTF8_VALUEC     0x80        // Continueation byte (10xxxxxx).
#define UTF8_MASKC      0xc0

                                    // Character markers.
#define JIS_EOF             -1      // All other characters are internal

class JIS_convert : public IO_cache {
public:
  int     find_type     (void);                 // Determine type of an input stream.
  int     is_unicode    (void);                 // Tests to see if object is a Unicode file.
  int     is_utf8       (void);                 // Tests for a UTF8 file.
  int     input_char    (void);                 // Read next kanji from the input stream
  void    output_char   (int ch);               // Output a character to the buffer.
  void    set_type      (int type);             // Setup for a conversion.
  void    unicode_write (void);                 // Write's UNICODE ID if necessary.
private:
  void    half2full     (int *p1,int *p2);      // Convert half width character to full width character.
  void    put_end       (void);                 // End a JIS escape sequence
  void    put_start     (void);                 // Start a JIS escape sequence
  void    put_bits      (int value,int count);  // Put bits in the bit buffer.
  int     get_bits      (int count);            // Get bits from bit buffer.
  short   type;             // File type being written or read.
  byte    in_twobyte;       // In two byte JIS escape sequence.
  byte    reverce_bytes;    // Need to reverce data byte order (UNICODE only).
  ulong   bit_buffer;       // Bit buffer for UTF-7 format.
  short   bits;             // Number of bits in the buffer.
};

typedef class JIS_convert JIS_convert;

//===================================================================
//
//  Exported routines.
//
extern int  ascii2unicode  (int ch);            // Convert extended ASCII to UNICODE
extern int  jis2sjis       (int ch);            // Convert JIS code into Shift-JIS code.
extern int  jis2unicode    (int ch);            // Convert JIS code into Unicode.
extern int  sjis2jis       (int ch);            // Convert Shift-JIS code to JIS code.
extern int  unicode2jis    (int ch,int bad);    // Convert Unicode to JIS code.   
//
//  Some of functions have been blocked out because they are not yet needed.  They are for when the 
//  code is converted to UNICODE.
//
extern void jis2utf        (byte *&ptr,int ch); // Convert JIS code to UTF-8
extern int  utf2jis        (byte *&ptr);        // Convert UTF-8 to JIS code.
//extern void unicode2utf    (byte *&ptr,int ch); // Convert UNICODE to UTF-8
//extern int  utf2unicode    (byte *&ptr);        // Convert UTF-8 to UNICODE.

extern void initialize_cp  (void);              // Initialize the conversion routines (adjust for code page)

#endif jwp_jisc_h
