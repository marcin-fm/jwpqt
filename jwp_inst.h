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
//  This modlue handles the install properties of JWPce.
//
//  Noramlly every time JWPce runs, it checks to see if the file 
//  extensions are associated with JWPce.  If any of the exntensions
//  are not associated then it attempts to do an install.  The 
//  install consists of assigning the extensions to JWPce, and 
//  possibly putting JWPce in the start-menu and/or the desktop.
//  The program also checks to make sure the file associations point 
//  back to this executable.
//

#ifndef jwp_inst_h
#define jwp_inst_h

extern void do_install (int force);

#endif jwp_inst_h
