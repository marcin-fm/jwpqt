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
//  This modlule implements a knaji list window.  This is bascially 
//  the same window used for the kana->kanji conversion and for the 
//  results of a kadical lookup conversion.  This is bascially a 
//  horizontal strip of kanji/(kana) compounds seperated by spaces.
//  The routines here implement omost of the functionality of processing
//  window events, and selecting items.  It is left to the derived 
//  classes JWP_conv, and RADICAL_lookup to actually put contents 
//  back into a file, and procedue data.
//

#ifndef jwp_klst_h
#define jwp_klst_h

#include "jwp_file.h"

#ifdef WINELIB
  #define _virt_    virtual
#else  WINELIB
  #define _vert_    
#endif WINELIB



//===================================================================
//
//  Parameters for routines.
//
                                // Parameters for KANJI_list::do_select() and JWP_file::convert().
#define KANJILIST_RIGHT   0     // Shift conversion to right.
#define KANJILIST_LEFT    1     // Shift conversion to left.

class KANJI_list {
public:
  KANJI_list (int count = 1000);
  _vert_ ~KANJI_list (void);
  void  adjust       (HWND hwnd);               // Adjust display parameters.
  void  clear        (void);                    // Clear conversion system
  void  do_mouse     (LPARAM lParam);           // Process mouse messages.
  void  do_scroll    (int message);             // Process scroll bar messages.
  void  do_wheel     (WPARAM wParam);           // Do wheel-mouse events.
  void  draw         (HDC hdc);                 // Render conversion bar.
  void  get_info     (HWND hwnd);               // Gets info for the selected character.
  int   last         (void);                    // Returns index of last compound.
  int   len          (int pos);                 // Returns the length of an kanji string.
  int   next         (int pos);                 // Get index of next kanji compound.
  int   put_kanji    (int kanji);               // Put character into the buffer.
  int   prev         (int pos);                 // Get index of previous kanji compound.
  void  redraw       (void);                    // Force redaw of the kanji bar.
  void  set_scroll   (int draw);                // Calculate scroll and display parameters.
  virtual void select(int s);                   // Select a kanji conversion and insert into file.

  JWP_file    *file;                            // Context for replacement. (used only for convert routines)
//
//  Window display parameters.
//
  HWND         window;          // Window pointer.
  short        height;          // Height of the conversion bar.
  short        width;           // display width (pixal).
  short        blank_space;     // Amount of horizontal space to separate entries in the kanji list.
  short        y_offset;        // Y offset for rendering kanji in the bar.
//
//  List parameters.
//
  KANJI       *list;            // The list.
  short        size;            // Size of the list
  short        list_len;        // Number of characters in the list.
  short        first;           // Index of first kanji to be shown.
  short        selected;        // Index to selected kanki.
  short        sel_char;        // Index of character user clicked on.  This is used for kanji info.
  int          x_total;         // X pixal length of the entire list.
  int          x_first;         // X pixal location of first.
  RECT         sel;             // Specifies the location of the invert box for the slected conversion.
private:
  int  hadvance      (int x,int ch);            // Advance to next character
  int  next_first    (void);                    // Advance the first marker.
};

typedef class KANJI_list KANJI_list;

#endif jwp_klst_h
