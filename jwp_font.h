//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//===================================================================//

//===================================================================//
//
//  This modlues main function is to deal with the system fonts.  
//  There are three fonts used in the system currently.  These are
//  the kana/kanji font.  The user font used in the main edit window,
//  and the system font used in controls, and in the status bar.  
//  Various useful parameters are caclated buy the font system for use
//  by other routines.  The system currenly allows for changes in the 
//  fonts, buf these are relativly limited.  
//
//  Since in the current version, printing is not supported, only the 
//  16x16 kanji font is loaded.  When printing, or higher reslution
//  screens are used, support for additional font can easily be added.
//
#ifndef jwp_font_h
#define jwp_font_h

//#include "jwp_prnt.h"               // We need this for the definition of the print structures

//===================================================================//
//
//  Exported definitons.
//
#ifndef SHIFTJIS_CHARSET            // This constaint is only defined in Win CE 2.11 and 
  #define SHIFTJIS_CHARSET 0x80     //   up.  We support TrueType fonts for all versions, but
#endif  SHIFTJIS_CHARSET            //   must check for Japanese fonts.

typedef unsigned short KANJI;       // Type for a kanji/kana character.

//#define ONLY_16                     // Allows only the F16X16.F00 kanji font.  This allows some optimizations.

//-------------------------------------------------------------------
//
//  class KANJI_font.
//
//  This class contains information related direclty to a specific
//  kanji font.
//

//--------------------------------
//
//  This is a virtual font class.  This class cannot be used directly, but must be used
//  to define a veritual class for the type of font that you want.  Because the font 
//  parameters differ so radically, different open versions for each font type are used.
//  The current routine open_bitmap() and open_font() provide uniformed ways to open the
//  fonts.
//
class KANJI_font {
public:
  short width,height;       // height and witdth of font
  short leading;            // Vertial gaps between lines
  short spacing;            // Horizontal gaps between characters.
  byte  truetype;           // Indicates this is a TrueType font.
  byte  vertfont;           // Vertical font.
  void  virtual draw       (HDC hdc,int jis,int x,int y) = 0;   // Render character
  void  virtual fill       (HDC hdc,int jis,RECT *rect) = 0;    // Used to make big kanji.
  void          find_color (int jis,HDC hdc,COLORREF &color);   // Find a kanji.  Load the kanji from disk if necessary
  int   virtual jis_index  (int jis) = 0;                       // Convert jis code to font code.
  void          remove     (void);                              // Deallocate this font and clean up.
private:
  void  virtual close      (void) = 0;                          // Close the font
};

//--------------------------------
//  
//  This dirived class processes bitmaped fonts
//
class BITMAP_KANJI_font : public KANJI_font {
public:
  BITMAP_KANJI_font (void);
  void  draw        (HDC hdc,int jis,int x,int y);          // Render character
  void  fill        (HDC hdc,int jis,RECT *rect);           // Used to make big kanji.
  int   jis_index   (int jis);                              // Convert jis code to font code.
  int   open        (TCHAR *name,int cache,int vertical);   // Open the font.
private:
  void  close       (void);                                 // Closes a font.
  int   find_kanji  (int index,HDC hdc,COLORREF &color);    // Find a kanji.  Load the kanji from disk if necessary

  short   holes;                // Indicates font has holes.
  short   hshift;               // Shift used in redering characters.
  short   bmsize;               // Size of bitmap in memory bytes (per character).
  short   bmfilesize;           // Size of bitmap in file bytes (per character).
  char   *bitmaps;              // Font's bitmap data
  KANJI  *cache;                // Font cache.
  short   index;                // Next bitmap to replace in the font cache.
  HDC     hdcmem;               // Memory HDC used for rendering the bitmaps.
  HANDLE  file;                 // Pointer to file (used only for cached fonts).
#ifndef WINCE                   // CE does not support SetBitmapBits so cannot use this.
  HBITMAP hbitmap;				// Bitmap for bit operations
#endif  WINCE
};

//--------------------------------
//
//  This dirived class processes TrueType fonts.
//
class TRUETYPE_KANJI_font : public KANJI_font {
public:
  TRUETYPE_KANJI_font (void);
  void  draw          (HDC hdc,int jis,int x,int y);                // Render character
  void  fill          (HDC hdc,int jis,RECT *rect);                 // Used to make big kanji.
  int   jis_index     (int jis);                                    // Convert jis code to font code.
  int   open          (HDC hdc,tchar *name,int cache,int vertical); // Open the font.
private:
  void  close         (void);                                       // Closes a font.
  HFONT   font;                 // TrueType font structure.
  short   hshift;               // Shift used in redering characters.
  short   vshift;               // Vertical shift for rendering characters
#ifndef WINCE                   // Windows CE does not use gylph addressing for TrueType fonts
  char   *cmap;                 // Copy of the cmap structure for this font.
  char   *gsub;                 // GSUB structre for this font, used for viertical glyph replacemnt
  ushort  count;                // Number of entries in the cmap subtable.
  ushort *start;                // Start table for ranges                                
  ushort *end;                  // End table for ranges
  ushort *delta;                // Delta table for displacements
  ushort *offset;               // Offest table for entriesinto the glyph table
  ushort *glyph;                // Glyph table.
  ushort  vcount;               // Number of vertical replacement glyphs
  ushort *from;                 // Glphys to be replaced.
  ushort *to;                   // replacement glyphs.
#endif WINCE
};

//-------------------------------------------------------------------
//
//  This is the main class for font work.  Each JFC_font object contains all
//  the information necessary to render text in a particular font.  All spacings
//  and other settings are contained in the class as well as the ASCII font 
//  metrics.  
//
//  Each JFC_font object really constists of a kanji font and an ASCII font pair,
//  plus all the support information.  
//
//  JFC_font objects can contain dupicates of open fonts.  This makes things more
//  efficient.  They are flaged by the duplicate member.
//
class JWP_font {
public:
  short height;                             // Kanji font height
  short hwidth;                             // Kanji nominal character-character spcaking.
  short rheight;                            // Top of rectange around kanji font (used a lot in redraw, and selecting).
  short vheight;                            // Kanji nominal line-line spacing.
  short lheight;                            // Height used in list boxes.
  short loffset;                            // Offset for rending line of text in list box.  (Only used once, but pre-calculated for speed)
  short cheight;                            // Height of caret (cursor)
  short vspace;                             // Vertical extra space measure (framing around window).
  short hspace;                             // Horizontal extra space meauser (framing around window).
  short x_offset;                           // Horizontal offset for rendering text (leaves a small borer on left side). [Parameter used by the JFC_file class]
  short y_offset;                           // Vertical offset for rendering of text from top of screen. [Parameter used by the JFC_file class]
  short vertical;                           // Vertical printing font.
  byte  duplicate;                          // Indicates that the font is actually a duplicate of another font, so don't deallocate.
  HFONT       ascii;                        // Ascii font.
  KANJI_font *kanji;                        // Kanji font.

  void  close      (void);                  // Close the font.
  void  copy       (class JWP_font *font);  // Duplicates a particular font.
  int   hadvance   (int x,int ch);          // Advance a cursor horizontaly by a single character.
  int   open       (TCHAR *name,int size,int cache,HDC hdc,int vert); // Open and initialize the font.
  HFONT open_ascii (tchar *face);           // Open an ascii font.

  inline JWP_font (void) { ascii = NULL; kanji = NULL; duplicate = false; }
private: 
  short widths[256];                        // Holds the char widths in pixels, for the pirmary ASCII.
};

//-------------------------------------------------------------------
//
//  class COLOR_kanji.
//
//  Manages the color kanji list.
//
//  The basics of the color kanji list is an array containning one byte for
//  each kanji.  If the byte is set the kanji is in the list.  If it is not 
//  set the kanji is not in the list.
//

#define MAX_KANJI   6450                            // Max number of kanji we allow in the list.

class COLOR_kanji {
public:
  inline COLOR_kanji (void) { clear(); return; }    // Initialize.
  int  add       (int ch);                          // Add a character to the list.
  void clear     (void);                            // Clear the list.
  int  count     (void);                            // Count the number of kanji in the list.
  void do_adddel (void);                            // Does the add & remove kanji from list dialog
  int  in        (int ch);                          // Is a character in the list.
  void put       (void);                            // Add the color kanji into the current file.
  void read      (void);                            // Read list from a file.
  int  remove    (int ch);                          // Remove a character from the color kanji list.
  int  write     (void);                            // Write the list to a file.
private:
  byte data[MAX_KANJI+10];                          // One byte for each character.
};

//-------------------------------------------------------------------
//
//  Exported data
//

extern class JWP_font clip_font;            // Font used for posting bitmaps on the clipboard.
extern class JWP_font bar_font;             // Font used for kanji bars.
extern class JWP_font file_font;            // Font used for file information.
extern class JWP_font sys_font;             // Font used for system data (bushu, JIS table, etc).
extern class JWP_font edit_font;            // Font used for line edit
extern class JWP_font list_font;            // Font used for lists.
extern short sysfont_height;                // Height of the system font (used for toolbars).

extern class COLOR_kanji color_kanji;

//-------------------------------------------------------------------
//
//  Exported routines.
//
extern void        ascii_draw       (HDC hdc,int x,int y,int ch);   // Draw an ASCII (or extended ASCII character)
extern int         ascii_width      (HDC hdc,int ch);               // Get width of ASCII character.
extern void        free_fonts       (void);                         // Cleanup routine to close all open fonts.
extern KANJI_font *get_bigfont      (RECT *rect);                   // Routine to get the big font.
extern KANJI_font *get_jistfont     (void);                         // Get font for use in the JIS table
extern KANJI_font *get_printfont    (HDC hdc,int vertical);         // Routine to get a printer font of a given height.
extern int         initialize_fonts (void);                         // Setup the fonts system and intiailize.

#endif jwp_font_h

