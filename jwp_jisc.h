//-------------------------------------------------------------------//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998,1999,2000.             //
//  All rights reserved.                                             //
//                                                                   //   
//  The code do do conversion between ECU, JIS, and Shift-JIS        //
//  was taken from jconv.c which is copyright by Ken R. Lunde,       //
//  Adobe Systems Incorporated.  Full copyright notice is cpp file.  //
//                                                                   //
//-------------------------------------------------------------------//

//-------------------------------------------------------------------
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

                                // Character markers.
#define JIS_EOF             -1  // All other characters are internal

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

//-------------------------------------------------------------------
//
//  Exported routines.
//
extern int  jis2sjis       (int);   // Convert JIS code into Shift-JIS code.
extern int  jis2unicode    (int);   // Convert JIS code into Unicode.
extern int  sjis2jis       (int);   // Convert Shift-JIS code to JIS code.
extern int  unicode2jis    (int);   // Convert Unicode to JIS code.   

extern void initialize_cp  (void);  // Initialize the conversion routines (adjust for code page)

#endif jwp_jisc_h
