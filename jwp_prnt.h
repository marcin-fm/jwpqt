//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//===================================================================//

#ifndef jwp_prnt_h
#define jwp_prnt_h
#include "jwp_font.h"

//
//  This structure describes the setup of a page.  This includes margins,
//  printing options (vertical, and landscape).  There is such a class
//  assocated with each file, and a general one that is defined as 
//  used for the default settings.
//
typedef class PageSetup {
public:
  float left,right,top,bottom;  // Margins in inches.
  byte  vertical;               // vertical printing
  byte  landscape;              // Landscape page
  int print_width (void);       // Calculate width of printed page in kanji.
} PrintSetup;

//
//  This class describes the context in which printing will occure.
//  The class contains basic elements such as the size of the page,
//  resolution, offset positions, and various other parameters.
//
typedef class PrintContext {
public:
  int   xmax,ymax;          // Physical dimensions of the printer.
  short xdpi,ydpi;          // Printer resolution.
  int   height,width;       // Printable page area (with margins included)
  short kwidth,kheight;     // Character sizes with space around the chracter
  short fwidth,fheight;     // Pixal size of the character font (not space)
  int   xoffset,yoffset;    // Offsets to get the render potion of the page.
  short xshift;             // Shift for rendering kanji characters.
  short char_width;         // Width of the page in characters.
  KANJI_font *font;         // Kanji font for rednering.

  void setup   (HDC hdc,PageSetup *ps);
  PrintContext (void);       // Constructor
} PrintContext;

extern PrintContext print_context;              // Global print context (default settings)

extern void initialize_printer (PageSetup *ps); // Intialize printing parameters.

#endif jwp_prnt_h
