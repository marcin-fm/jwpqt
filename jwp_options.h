//-------------------------------------------------------------------//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998,1999,2000.             //
//  All rights reserved.                                             //
//                                                                   //
//-------------------------------------------------------------------//

//-------------------------------------------------------------------
//
//  This modlue contains all of the major compile time otpions for the
//  program.  These are not adjustments of the size of various buffers,
//  but rather enable or disable major freatures of the program.
//
#ifndef jwp_opts_h
#define jwp_opts_h

//===================================================================
//
//  Compile-time code generation options
//

//
//------  jwp_clip  ------
//
//  CLIPBOARD_FORAMT -- Determines what optional clipboard foramts 
//                      are supported.
//
//      CLIPBOARD_FORMAT_BITMAP   -- Supports the bitmap cliboard foramt.
//      CLIPBOARD_FORMAT_NOBITMAP -- Does not support the bitmap
//                                   foramt.
#define CLIPBOARD_FORMAT_BITMAP     0xAF35B601
#define CLIBOOARD_FORAMT_NOBITMAP   0xAF35B602

#if    defined(WINCE)
  #define CLIPBOARD_FORMAT CLIPBOARD_FORMAT_NOBITMAP
#elif !defined(CLIPBOARD_FORMAT)
  #define CLIPBOARD_FORMAT CLIPBOARD_FORMAT_BITMAP
#endif 

//
//------  jwp_conv  ------
//
//  CONVERT_ACCESS -- This specifies how JWPce accesses the WNN kana->
//                    kanji conversion dictionary.  There are two
//                    possible settings for this parameter:
//
//      CONVERT_ACCESS_MEMORY   -- The dictionary is loaded into memory 
//                                 during startup and is accessed from 
//                                 memory.  This is fast, but requires
//                                 more memory.
//      CONVERT_ACCESS_FILE     -- The dictionary is loaded as needed 
//                                 In this setting, the dictioary is 
//                                 open when JWPce intializes and the 
//                                 file remtains locked until JWPce 
//                                 terminates.  Note that access to the 
//                                 file is shared so this should not be 
//                                 a problem.
//
#define CONVERT_ACCESS_MEMORY   0xAF34B601
#define CONVERT_ACCESS_FILE     0xAF34B602

#ifndef CONVERT_ACCESS
  #define CONVERT_ACCESS CONVERT_ACCESS_FILE
#endif  CONVERT_ACCESS

//
//------  jwp_jisc  ------
//
//  SUPORT_HALFKATA -- If defined allows suport for half width katakana
//                     strings used in Shift-JIS and EUC encodings.  If 
//                     not defined half-width katakana sequences will not
//                     be read.  These sequences do not appear to be used
//                     much.
//
#define SUPORT_HALFKATA

//
//------  jwp_dict  ------
//
//  DICTONARY_TRACKING -- If defined, this causes special code used for
//                        research to become active, this code will 
//                        record quieries to the dictonary and determine
//                        and their success or failure.  This is designed
//                        as a research tool for Professor M. O. Douglass
//                        of UCLA, and not as a general use tool.  Not
//                        defining this value removes all trace of the 
//                        research code for the compilation.
//
#ifdef WINCE
  #ifdef DICTONARY_TRACKING
    #undef DICTIONARY_TRACKING
  #endif DICTONARY_TRACKING
#else  WINCE
//#define DICTIONARY_TRACKING
#endif WINCE

#ifdef DICTIONARY_TRACKING      
  #define VERSION_SPECIAL "r"   // Define version special constaint so name is different
#else  DICTIONARY_TRACKING
  #define VERSION_SPECIAL 
#endif DICTIONARY_TRACKING

#endif jwp_opts_h
