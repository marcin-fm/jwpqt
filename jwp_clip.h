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
//  This modlue implement most of the clipboard functionaly of the 
//  system.  The clipbard has no class associated witht it, but rather
//  makes use of stuff distrubuted in a number of classes, particuarly
//  Paragraph, and JWP_file.
//

#ifndef jwp_clip_h
#define jwp_clip_h

#include "jwp_file.h"

extern void      clear_clipboard (void);                    // Destroy internal clipbaord data.
extern void      do_clipboard    (int iMsg,WPARAM wParam);  // Processes system messages associated with the clipboard.
extern void      free_paste      (JWP_file *paste);         // Free paste gotten with get_paste().
extern JWP_file *get_paste       (HWND hwnd);               // Get object to paste    

extern JWP_file *jwp_clipboard;

#endif jwp_clip_h
