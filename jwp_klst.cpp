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
//  The routines here implement most of the functionality of processing
//  window events, and selecting items.  It is left to the derived 
//  classes JWP_conv, and RADICAL_lookup to actually put contents 
//  back into a file, and procedue data.
//
//  Note that these routines assume that the list is actually allocated.
//  before they use.  Thus if the constructor failed, you had better 
//  do something else.
//

#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_file.h"
#include "jwp_font.h"
#include "jwp_info.h"
#include "jwp_klst.h"
#include "jwp_misc.h"

#include <limits.h>

//===================================================================
//
//  Begin Class KANJI_list.
//
//  This class handles processing of kanji-list windows.
//

//--------------------------------
//
//  Constructor.
//
KANJI_list::KANJI_list (int count) {
  window = null;
  size   = 0; 
  if ((list = (KANJI *) malloc(count*sizeof(KANJI)))) size = count;
  clear ();
  return;
}

//--------------------------------
//
//  Destructor.
//
KANJI_list::~KANJI_list () {
  if (list) free (list);
  return;
}

//--------------------------------
//
//  Adjust the window, usually as a responce to a change in window 
//  size.
//
//      hwnd -- Either NULL or a window pointer.  Normally, you would
//              call this will NULL.  A value is only passed in during
//              the window creation routines.
//
//  The height must be set int he derived class, because they allow 
//  for different options.
//
void KANJI_list::adjust (HWND hwnd) {
  RECT rect;
  if (!hwnd) return;
  GetClientRect (hwnd,&rect);
  blank_space = bar_font.hwidth/3;
  y_offset    = bar_font.height-1+bar_font.vspace;
  sel.top     = y_offset-bar_font.height-bar_font.vspace/4;
  sel.bottom  = sel.top+bar_font.vheight+1;
  width       = (short) rect.right;
  set_scroll (true);
  return;
}

//--------------------------------
//
//  Clear the kana->kanji conversion system.
//
void KANJI_list::clear () {
  first    = 0;
  list_len = 0;
  selected = 0;
  set_scroll (true);
  redraw     ();
  return;
}

//--------------------------------
//
//  Not called directly, but rather called from the window proc, this 
//  this routine process the mouse events.  The only events that are 
//  supported is clicking on/near the kanji compounds in the converon
//  bar.
//
//      lParam -- Message, lParam (contains the mouse location).
//
void KANJI_list::do_mouse (LPARAM lParam) {
  int x,old_selected,old_sel_char;
  x = LOWORD(lParam);
  old_selected = selected;
  old_sel_char = sel_char;
  for (selected = first; selected <= last(); selected++) {
    set_scroll (false);
    if ((x >= sel.left) && (x <= sel.right)) {
      select (selected);
      sel_char = (short) (selected+(x-sel.left)/bar_font.hwidth);
      if (list[sel_char] == '/') sel_char--;
      return;
    }
  }
  set_scroll (false);
  selected = old_selected;
  sel_char = old_sel_char;
  return;
}

//--------------------------------
//
//  Processes horizontal scroll bar messages.
//
//      message -- Windows message parameter.
//
void KANJI_list::do_scroll (int message) {
  int i;
  switch (LOWORD(message)) {
    case SB_LINEDOWN:
         next_first ();
         break;
    case SB_LINEUP:
         first = prev(first);
         break;
    case SB_PAGEUP:
         if (first == 0) return;
         i = x_first-width/2;
         if (i < 0) i = 0;
         while (x_first > i) {
           first = prev(first);
           set_scroll (false);
         }
         break;
    case SB_PAGEDOWN:
         i = x_first+width/2;
         while ((x_first < i) && !next_first()) set_scroll (false);
         break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
         GetScrollInfo (window,SB_HORZ,&scroll_info);
         scroll_info.nPos = HIWORD(message);    // This is a kludge because GetScrollInfo does not return the correct data.
         first = 0;
         set_scroll (false);
         while (scroll_info.nPos > x_first) {
           first = next(first);
           set_scroll (false);
         }
         break;
    default:
         return;
  }
  set_scroll (true);
  redraw ();
  return;
}

//--------------------------------
//
//  Processes the wheel-mouse events for the kanji lists. 
//
//      wParam -- The wParam passed in trom the WM_MOUSEWHEEL message.
//
#ifndef WINCE
void KANJI_list::do_wheel (WPARAM wParam) {
  int ctrl,shift,i;
  static int delta;                                                 // Accumulation point for deltas.
  shift  = SystemParametersInfo(SPI_GETWHEELSCROLLLINES,0,&ctrl,0); // Scroll amount.
  ctrl   = LOWORD(wParam);                                          // Keys
  delta += (short) HIWORD(wParam);                                  // Delta
//
//  Shift -- skip pages.
//
  if ((ctrl & MK_SHIFT) || (shift == WHEEL_PAGESCROLL)) {
    while (delta > WHEEL_DELTA) {
      delta -= WHEEL_DELTA;
      if (first == 0) break;
      i = x_first-width/2;
      if (i < 0) i = 0;
      while (x_first > i) {
        first = prev(first);
        set_scroll (false);
      }
    }
    while (abs(delta) > WHEEL_DELTA) {
      delta += WHEEL_DELTA;
      i      = x_first+width/2;
      while ((x_first < i) && !next_first()) set_scroll (false);
    }
  }
//
//  Normal -- skip lines.
//
  else {
    while (delta > WHEEL_DELTA) {
      for (i = 0; i < shift; i++) first = prev(first);
      delta -= WHEEL_DELTA;
    }
    while (abs(delta) > WHEEL_DELTA) {
      for (i = 0; i < shift; i++) next_first ();
      delta += WHEEL_DELTA;
    }
  }
  set_scroll (true);
  redraw ();
  return;
}
#endif  WINCE

//--------------------------------
//
//  Not called directly, but rather called from the window proc, this
//  routine draws the conversion bar.
//
//      hdc -- Display context to render into.
//
void KANJI_list::draw (HDC hdc) {
  int i,x;
  if (!list_len) return;
  x = bar_font.x_offset-x_first;
  for (i = 0; (i < list_len) && (x < width); i++) {
    if (list[i] != '/') bar_font.kanji->draw (hdc,list[i],x,y_offset);
    x = hadvance(x,list[i]);
  }
  InvertRect (hdc,&sel);
  return;
}

//--------------------------------
//
//  This routine is used to get the character information for the selected character.
//  This is used by list handlers the allow the user to determine the meaning of 
//  characters in the list.
//
//      hwnd -- Window assocated wtih the Character information dialog.
//
void KANJI_list::get_info (HWND hwnd) {
  if (!list_len) return;  
  kanji_info (hwnd,list[sel_char]);
  return;
}

//--------------------------------
//
//  Advance a pixal counter down the kanji bar.
//
//      x      -- Previous kanji bar render position.
//      ch     -- Character being rendred.
//  
//      RETURN -- New character render position.
//
int KANJI_list::hadvance (int x,int ch) {
  if (ch == '/') return (x+2*blank_space);
  return (x+bar_font.hwidth);
}

//--------------------------------
//
//  Utiltiy routine to reurn the index of the last conversion in the 
//  list.  Note, this will always be the kana string.
//
//      RETURN -- Index of the last conversion suggested.
//
int KANJI_list::last () {
  return (prev(list_len+1));
}

//--------------------------------
//
//  Utility routine that returns the length of an item beginning at 
//  the indicated location.
//
int KANJI_list::len (int pos) {
  int i;
  for (i = 0; (list[pos] != '/') && (pos < list_len); pos++, i++);
  return (i);
}

//--------------------------------
//
//  Small utility routine to move to next conversion in list.
//
//      pos    -- Starting position.
//
//      RETURN -- Next position.  If starting was the end, the 
//                origininal starting postion will be returned.
//
int KANJI_list::next (int pos) {
  int i;
  for (i = pos; (i < list_len) && (list[i] != '/'); i++);
  if (i == list_len) return (pos);
  return (i+1); 
}

//--------------------------------
//
//  This routine moves the first pointer forward.  The routine will automatically not move the 
//  first pointer if the remainning part of the display line would be less than the width of the 
//  display.  This prevents a blank space on the right side.
//
//      RETURN -- A non-zero value indicates the first position was not changed.
//
int KANJI_list::next_first () {
  if (x_total < x_first+width) return (true);
  first = next(first);
  return (false);
}

//--------------------------------
//
//  Small utility routine to move to previous conversion in list.
//
//      pos    -- Starting position.
//
//      RETURN -- Previous position.  If starting was the beginning,
//                the starting postion will be returned.
//
int KANJI_list::prev (int pos) {
  if (!pos) return (pos);
  for (pos -= 2; (pos >= 0) && (list[pos] != '/'); pos--);
  return (pos+1);
  
}

//--------------------------------
//
//  Puts a character into the kanji list.  This routine checks to make 
//  sure the kanji list has not overflowed before putting the character.
//
//      kanji  -- Chracter to put in the list.
//
//      RETURN -- A non-zero return value indicates an error in 
//                placing the character.
//
int KANJI_list::put_kanji (int kanji) {
  if (list_len >= size) return (true);
  list[list_len++] = kanji;
  return (false);
}

//--------------------------------
//
//  Force a redraw of the window.
//
void KANJI_list::redraw () {
  if (window) InvalidateRect(window,NULL,true);
  return;
}

//--------------------------------
//
//  Selects a kanji replacement string, and implements the replacement.
//  
//      s -- Index of replacement.
//
void KANJI_list::select (int s) {
  selected = s;
  sel_char = s;
//
//  Scroll kanji bar to make current selection visable.
//
  if (first > selected) first = selected;       // (right)
  while (true) {
    set_scroll (false);
    if (sel.right+blank_space < width) break;   // (left).
    for ( ; list[first] != '/'; first++);
    first++;
  }
  set_scroll (true);                            // Set display and redraw
  redraw ();
  return;
}

//--------------------------------
//
//  The name of this routine is somewhat a misnomer.  Yes, it does set
//  the scroll bar, but it also calcualtes all of the parameters 
//  associated with the dispaly of the kanji bar.  This includes 
//  the selected kanji, and other such parameters.
//
//      draw -- Can be set to false to suppress the rendering of 
//              the scroll bar.
//
void KANJI_list::set_scroll (int draw) {
  int i;
  sel.left = bar_font.x_offset;
  x_first  = 0;
  x_total  = 0;
  for (i = 0; i < first; i++) x_first = hadvance(x_first,list[i]);      // pixal location of fist visible kanji.
  for (i = 0; i < list_len; i++) x_total = hadvance(x_total,list[i]);   // pixal length of entire bar.
  for (i = 0; i < selected; i++) sel.left = hadvance(sel.left,list[i]); // Begin and end of selected kanji.
  for (sel.right = sel.left; (i < list_len) && (list[i] != '/'); i++) sel.right = hadvance (sel.right,list[i]);
  sel.left  -= x_first+1;                                               // Adjust to displayable coordinates.
  sel.right -= x_first+1;
 
  if (jwp_config.cfg.kscroll && draw && window) {                       // Actually set the scroll bar.
    scroll_info.nMax  = x_total;
    scroll_info.nPage = width;
    scroll_info.nPos  = x_first;
    SetScrollInfo (window,SB_HORZ,&scroll_info,true);
  }
  return;
}

//
//  End Class KANJI_list.
//
//===================================================================

// ### Need to think about processing list differently, not usinng '/' 
// ###   to separate entriles, but rather setting high bit.


