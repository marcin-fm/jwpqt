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
//  This modlule implements most of the file IO options.  (See jwp_clip.cpp
//  for clipboard IO.)  Most of the routines contained in this module 
//  are defined in the class JWP_file.
//

#ifndef jwp_flio_h
#define jwp_flio_h

extern JWP_file *choose_file  (HWND hwnd,int idnex,int help);   // Generates a choose file dialog box.
extern void      do_drop      (HDROP drop);                     // Implements drag and drop for the main window.
extern void      do_fileopen  (void);                           // Implements the file/open... menu command
extern void      do_files     (int index);                      // Implements the window/Files... menu command
extern void      do_recent    (int id);                         // Open file from recent files menu.
extern JWP_file *file_is_open (tchar *name);                    // Check to see if the named file is open.
extern void      recent_files (tchar *name);                    // Handles updating the recent files menu.

#endif jwp_flio_h
