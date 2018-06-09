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
//  This modlue is central to the program.  In this module most of the 
//  basic manipulations on a file are implemented.  This modlue includes 
//  complete implementations of the Paragraph class, Position class, 
//  JWP_view class and a large part of the  JWP_file class.
//

#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_conv.h"
#include "jwp_dict.h"
#include "jwp_file.h"
#include "jwp_find.h"
#include "jwp_font.h"
#include "jwp_info.h"
#include "jwp_inpt.h"
#include "jwp_lkup.h"
#include "jwp_misc.h"
#include "jwp_para.h"
#include "jwp_stat.h"

//===================================================================
//
//  Compile time options.
//

//#define USE_REDRAW_LINE_BLANKING    // Defining this value cuases JWPce to blank each line
                                    //   just before the line is redrawn, instead of simply
                                    //   blanking the entire region when the InvalidateRect
                                    //   command is given.  This is actually slightly slower
                                    //   then preblanking, but make sthe screen flash much
                                    //   less noticable.

#ifdef USE_REDRAW_LINE_BLANKING     // Redraw blanking flags passed to InvalidateRect().
  #define REDRAW_BLANK  false
#else
  #define REDRAW_BLANK  true
#endif USE_REDRAW_LINE_BLANKING

//===================================================================
//
//  Begin Class FILE_list
//

//--------------------------------
//
//  This class is used to manage a list of files the user has selected.  
//  (Remember, an edit-box is a file to JWPce.)  This list keeps track 
//  of the files the user has slected.  The top file on the list will 
//  be used as the target for the "Insert to File" operations.  
//
//  The list is managed as follows:
//
//      Main edit windows are represented by NULL.  Selecting the main
//      window causes the entire list to be cleared, ie the main window
//      is now the target.
//
//      Selecting an edit-window adds it to the list.  It also removes
//      any other occurences of the window from the list.
//
//  When a window is closed, it should be removed from the list.  (This is
//  usually done in the WM_DESTROY message, or the class deconstructor.)
//

FILE_list file_list;            // The global list instance.

//--------------------------------
//
//  Add a file to the list.  
//
//      file -- File to be added.  Remember that main edit windows add a 
//              NULL value.  This clears the list.
//
void FILE_list::add (JWP_file *file) {
  FILE_node *node;
//
//  file is NULL.  This is a special code for the user has selected the 
//  main window.  The entire list gets cleared.
//
  if (!file) {
    while (list) remove (list->file);
    return;
  }
//
//  Selected file is the top file on the list, do nothing.
//
  if (list && (list->file == file)) return;
//
//  Remove the file from the list, just in case it exists in the list somewhere
//  and insert the file at the top of the list.
//
  remove (file);
  if (node = new FILE_node) {
    node->file = file;
    node->next = list;
    list       = node;
  }
  return;
}

//--------------------------------
//
//  This routine gets the target for the "Insert to File" operatitons.
//  Normally this is the top file in the list, but if the list is empty,
//  the current main editor window file will be returned.
//
//      exclude -- Exclude this file from the return.  If this file is 
//                 the top file, the next file will be recovered.
//
//      RETURN  -- Target file for an "Insert to File" operation.
//
JWP_file *FILE_list::get (JWP_file *exclude) {
  if (list && (list->file != exclude)) return (list->file);
  if (list && list->next) return (list->next->file);
  return (jwp_file);
}

//--------------------------------
//
//  Remove a file from the list.  The rotuine does handle the case where
//  you try to remove a file not in the list.
//
//      file -- File to be removed.
//
void FILE_list::remove (JWP_file *file) {
  FILE_node *node,*temp;
  if (!list) return;                        // No list, man that was easy!
  if (list->file == file) {                 // Top element in list.
    node = list;
    list = node->next;
    delete node;
  }
  else {                                    // Search list.
    for (node = list; node->next && (node->next->file != file); node = node->next);
    if (node->next) {
      temp       = node->next;
      node->next = temp->next;
      delete temp;
    }
  }
  return;
}

//
//  End Class FILE_list.
//
//===================================================================

//===================================================================
//
//  Begin Class Position
//
//  Class defines a position within a file and a number of routines
//  that can operate on the class.  Positions are used for the cursor,
//  Upper left corner of the display (view_top), and selected regions
//  of the text.
//

//--------------------------------
//
//  Advance this position by a number of characters (could be a negative
//  value).  
//
//      count -- Number of characters to move the cursor.
//  
void Position::advance (int count) {
  abs ();
  pos += count;
  rel ();
  return;
}

//--------------------------------
//
//  Align cursor when moving up or down.  This consists of finding the 
//  the cursor location on the current line that is ass close as possible
//  to the stored cursor location, then adjusting the location of the 
//  cursor to match this location.
//
//      file  -- File to align with.
//      x_pos -- Position to align with.
//      mouse -- Set to non-zero for mouse movement.  This causes
//               truncation, not rounding on the x locations.
//
void Position::align (JWP_file *file,int x_pos,int mouse) {
  int i,x,x2;
  x2 = x = para->line_start(line,&file_font); 
  for (i = 0; i < line->length; i++)  {
    x2 = file_font.hadvance(x,para->text[line->first+i]);
    if (x2 >= x_pos) break;
    x = x2;
  }
  x  -= x_pos;
  if (!mouse) {
    x2 -= x_pos;
    if (x2 < 0) x2 = -x2;
    if (x  < 0) x  = -x;
    if (x2 < x) i++;
  }
  pos = i;
  return;
}

//--------------------------------
//
//  Move indicated position down (toward end of file).
//
//      RETURN -- Nonzero return indicates end-of-file.
//
int Position::move_down () {
  if (line->next) { line = line->next; return (false); }
  if (para->next) { para = para->next; line = para->first; return (false); }
  return (true);
}

//--------------------------------
//
//  Move indicated position up (toward top of file).
//
//      RETURN -- Nonzero return indicates top-of-file.
//
int Position::move_up () {
  if (line->prev) { line = line->prev; return (false); }
  if (para->prev) { para = para->prev; line = para->last; return (false); }
  return (true);
}

//--------------------------------
//
//  Move indicated position left.
//
//      RETURN -- Nonzero return indicates top-of-file.
//
int Position::move_left () {
  if       (bof()) return (true);
  if      (!bol()) pos--;
  else if (line->prev) { line = line->prev; pos = line->length; }
  else if (para->prev) { para = para->prev; line = para->last; pos = line->length; }
  return (false);
}

//--------------------------------
//
//  Move indicated position right.
//
//      RETURN -- Nonzero return indicates end-of-file.
//
int Position::move_right () {
  if       (eof()) return (true);
  if      (!eol()) pos++;
  else if (line->next) { line = line->next; pos = 0; }
  else if (para->next) { para = para->next; line = para->first; pos = 0; }
  return (false);
}

//--------------------------------
//
//  Convert position from absolute position (where line parameter of 
//  position is not used, but rather cursor is stored relative from 
//  the beginning of the paragraph), to relaative position (where line 
//  is used, and the curosr position is stored relative to the beginning 
//  of the line.
//
void Position::rel () {
  for (line = para->first; line && (line->first+line->length < pos); line = line->next);
  line = line ? line : para->last;
  pos -= line->first;
  return;
}

//--------------------------------
//
//  Retrieves.
//
//      RETURN -- The character to the left of the current position or zero if at the beginning of the line.
//
int Position::get_left () {
  Position temp;
  if (bol()) return (0);
  temp = *this;
  temp.move_left();
  return temp.get_char();
}

//
//  End Class Position.
//
//===================================================================

//===================================================================
//
//  Begin Class JWP_File.
//
//  This is the basic class that describes a file and all of it's parts.
//

#define CARETWIDTH_INSERT       2   // Insert cursor width
#define CARETWIDTH_OVERWRITE    4   // Overwrite cursor width

class JWP_file *jwp_file = NULL;

//--------------------------------
//
//  File descructor.
//
JWP_file::~JWP_file () {
  Paragraph *p;
  while ((p = first)) { first = p->next; delete p; }
  if (name) free (name);
  undo_free (undo,jwp_config.cfg.undo_number);
  return;
}

//--------------------------------
//
//  Adjust file data after major changes.
//
void JWP_file::adjust () {
  RECT  rect;
  int   c_width = 0;                                // Added for LINUX version compiler (not really necessary)
  if (!this || !first || !window) return;

  GetClientRect (window,&rect);
  width      = (short) (rect.right);
  height     = (short) (rect.bottom);
  char_width = (width-JWP_FONT.x_offset)/JWP_FONT.hwidth;
  hscroll    = char_width/4;
  vscroll    = height-2*JWP_FONT.vheight-JWP_FONT.y_offset;
  if (hscroll < 1) hscroll = 1;
//
//  Determine how the page margins are setup.
//
  if (filetype != FILETYPE_EDIT) {
    switch (jwp_config.cfg.width_mode) {
      case WIDTH_DYNAMIC: c_width = char_width;                break;
      case WIDTH_FIXED  : c_width = jwp_config.cfg.char_width; break;
      case WIDTH_PRINTER: c_width = page.print_width();        break;
    }
    if (char_pagewidth != c_width) {
      char_pagewidth = c_width;
      reformat ();
    }
  }
//
//  Recalculate positions.
//
  find_pos (&view_top,POS_VIEW);
  find_pos (&cursor);
  if (sel.type) {
    find_pos (&sel.pos1);
    find_pos (&sel.pos2);
  }
//
//  Check visibility and move caret.
//
  view_check ();
  if (window == GetActiveWindow()) caret_on ();
  edit_menu  ();
  redraw_all ();
  return;
}

//--------------------------------
//
//  Small utility routine to convert the cursor and selection points
//  to absolute coordinates.
//
void JWP_file::all_abs () {
  cursor.abs ();
  if (sel.type) {
    sel.pos1.abs ();
    sel.pos2.abs ();
  }
  return;
}

//--------------------------------
//
//  Small utility routine to convert the cursor and selection points
//  to relative coordinates.
//
void JWP_file::all_rel () {
  cursor.rel ();
  if (sel.type) {
    sel.pos1.rel ();
    sel.pos2.rel ();
  }
  return; 
}

//--------------------------------
//
//  Enables the display of the caret.
//
void JWP_file::caret_on () {
  int width,x,y;
  x = cursor.x-view_top.x;
  if (jwp_config.insert) width = CARETWIDTH_INSERT; else width = CARETWIDTH_OVERWRITE;
  CreateCaret (window,null,width,JWP_FONT.cheight);
  SetCaretPos (x-width/2,y = (cursor.y-view_top.y-JWP_FONT.rheight));
  ShowCaret   (window);
//
//  Deal with the IME if pressent
//
#ifndef WINCE
#if 0
  HIMC    imc;
  if (imc = ImmGetContext(window)) {
    LOGFONT         lf;
    COMPOSITIONFORM cf;
    memset (&lf,0,sizeof(lf));
    lf.lfHeight       = -JWP_FONT.height;
    lf.lfCharSet      = SHIFTJIS_CHARSET;
    cf.dwStyle        = CFS_FORCE_POSITION;
    cf.ptCurrentPos.x = x;
    cf.ptCurrentPos.y = y+ime_y;
    ImmSetCompositionWindow (imc,&cf);
    ImmSetCompositionFont   (imc,&lf);
    ImmReleaseContext       (window,imc);
  }
#endif
#endif WINCE
  return;
}

//--------------------------------
//
//  Clean up after composition ends or is canceled.
//
void JWP_file::ime_stop (HWND hwnd) {
#ifndef WINCE
  caret_on ();     // The caret won't get turned back on if composition was canceled, otherwise this would be unnecessary.
#endif WINCE
  return;
}

//--------------------------------
//
//  Adjust the IME's composition window/font used for a given window.
//
void JWP_file::ime_start (HWND hwnd) {
#ifndef WINCE
  COMPOSITIONFORM cf;
  LOGFONT         lf;
  HIMC            imc;
  int x,y;
  if (!(imc = ImmGetContext(hwnd))) return;
  caret_off ();                               // This doesn't work if something else turns it back on during composition, e.g. when switching windows.
  x = cursor.x-view_top.x;
  y = cursor.y-view_top.y-JWP_FONT.rheight;
//
//  The above calculations are taken from the caret positioning logic.
//  The following adjustments should make the first IME character keep its exact position before and after conversion, regardless of font size, when using TrueType fonts.
//  They also work well with the fonts selected by the IME to replace our bitmap fonts.
//
  x += JWP_FONT.kanji->hshift;
  y += hwnd==main_window? 2:1;
//
//  Request a composition font with attributes similar to the one in use.
//  For TrueType fonts, try to get a font with the same face name.
//
  memset (&cf,0,sizeof(cf));
  memset (&lf,0,sizeof(lf));
  lf.lfHeight       = JWP_FONT.rheight;       // This gives pretty good results for bitmap fonts. The IME font is necessarily smaller.
  lf.lfCharSet      = SHIFTJIS_CHARSET;
  if (JWP_FONT.kanji->truetype) {
    lf.lfHeight     = -JWP_FONT.height;       // Same method used to open TrueType fonts.
    lstrcpy (lf.lfFaceName,(filetype == FILETYPE_EDIT? jwp_config.cfg.edit_font:jwp_config.cfg.file_font).name);
  }
//
//  Adjust composition window.
//  Special handling is needed for the main window since the cursor is part of the "view" child window but said window forcibly sets focus to the main window and thus the IME composition window's coordinates are relative to the main window.
//
  cf.dwStyle        = CFS_FORCE_POSITION;
  cf.ptCurrentPos.x = x;
  cf.ptCurrentPos.y = y;
  if (hwnd==main_window) cf.ptCurrentPos.y += jwp_config.commandbar_height + (jwp_config.cfg.kanjibar_top? jwp_conv.height-1:0);
//
//  Submit changes and clean up.
//
  ImmSetCompositionWindow (imc,&cf);
  ImmSetCompositionFont   (imc,&lf);
  ImmReleaseContext       (hwnd,imc);
#endif WINCE
  return;
}

//--------------------------------
//
//  Mark file as changed and update title bar.
//
void JWP_file::change () {
  if (changed) return;
  changed = true;
  title ();
  return;
}

//--------------------------------
//
//  Cut text to clipboard.
//
void JWP_file::clip_cut () {
  if (sel.type == SELECT_CONVERT || sel.type == SELECT_KANJI) {
   if (sel.type == SELECT_KANJI) convert (CONVERT_RIGHT);       // If an inline conversion is pending, try to convert it to the default candidate.
   jwp_conv.clear ();                                           // Clear out any conversions taking place.
  }
  clip_copy        ();
  undo_start       ();
  selection_delete ();
  undo_end         ();
  return;
}

//--------------------------------
//
//  A small utility routine to move the cursor to the left.  This was 
//  separated out because it is used in more than one location.
//
void JWP_file::left () {
  if      (!cursor.bol()) cursor.pos--;
  else if (cursor.line->prev) { cursor.line = cursor.line->prev; cursor.pos = cursor.line->length; }
  else if (cursor.para->prev) { cursor.para = cursor.para->prev; cursor.line = cursor.para->last; cursor.pos = cursor.line->length; }
  return;
}

void JWP_file::right () {
  if      (!cursor.eol()) cursor.pos++;
  else if (cursor.line->next) { cursor.line = cursor.line->next; cursor.pos = 0; }
  else if (cursor.para->next) { cursor.para = cursor.para->next; cursor.line = cursor.para->first; cursor.pos = 0; }
  return;
}

//--------------------------------
//
//  This routine processes all general keyboard messages from the user
//  and from the menu systems.  Actually, this routine processes virtal 
//  keys, which are things like the arrows, home, etc.
//
//      key   -- Keyboard vertial key code.
//      ctrl  -- Non-zero if the control key is also held down.
//      shift -- Non-zero if the shift key is also held down.
//
void JWP_file::do_key (int key,int ctrl,int shift) {
  int i,j,k;
  switch (key) {
//
//  CAPS LOCK -- Output any pending ambiguous kana since toggling the caps lock normally implies we've finished entering the previous text.
//
    case VK_CAPITAL:
         if (ctrl || shift) return;
       //k = GetKeyState (VK_CAPITAL) & 1;                // Get new state of toggle (not currently used).
         if (jwp_config.mode == MODE_KANJI && kana_convert.pending_ambiguous ()) {
           kana_convert.clear ();                         // Output any pending kana. This is mostly intended for situations where one might be typing a group of katakana ending in AIUEON while utilizing the caps lock.
         }
         shift = false;                                   // This is preemptive in case Shift-Caps is supported later for some reason.
         break;
//
//  BRACE -- <ctrl>  -- Attempt to match brace at cursor position or find brace (if any) on current line.
//          +<shift> -- Select the region navigated, if any. Not all that useful but consistent with the rest of the program.
//
    case VK_OEM_4:            //  '[{' for US
    case VK_OEM_6:            //  ']}' for US
         if (!ctrl) return;
         jwp_conv.clear (true);
         clear_cursor   ();
         undo_clear     ();
         Position pos;
         int c,type,bcnt;
         //
         //  Check if cursor is on a brace or a single character to the right of one.
         //  I am assuming that 'cursor' normally stays in relative mode.
         //
         type = 0;
         pos = cursor;
         c = pos.get_char ();
         if (c == '{' || c == '}') type = c - '|';
         else if (!pos.bol()) {
           pos.advance (-1);
           c = pos.get_char ();
           if (c == '{' || c == '}') type = c - '|';
         }
         //
         //  If cursor is on a brace, try to find a match.
         //
         if (type > 0) {
           for (bcnt = 1; bcnt;) {
             if (pos.move_left()) return;                 // BOF -- No matching brace found.
             c = pos.get_char ();
             if (c == '{' || c == '}') bcnt += c - '|';
           }
         }
         if (type < 0) {
           for (bcnt = 1; bcnt;) {
             if (pos.move_right()) return;                // EOF -- No matching brace found.
             c = pos.get_char ();
             if (c == '{' || c == '}') bcnt -= c - '|';
           }
         }
         //
         //  Set cursor on matched brace, then update and return.
         //
         if (type) {
           selection (shift);     // Start selection (if shift held). Will be finished after the break statement.
           cursor = pos;          // Set cursor.
           redraw_all ();         // This is how Ctrl-Home/End behave. do_next() does jwp_file->adjust ();
           break;                 // Finish processing any selection and update a few things.
         }
         //
         //  No starting brace to match, so search for a brace on current line instead.
         //  We don't rock back and forth because it's harder to do safely than it should be.
         //
         for (pos = cursor;;) {   // Search backwards.
           c = pos.get_char ();
           if (c == '{' || c == '}') { type = c - '|'; break; }
           if (pos.bol()) break;
           pos.advance (-1);
         }
         //  Yeah, yeah, this logic is a bit goofy, but it looks cleaner.
         if (!type) pos = cursor;
         while (!type) {          // Search forwards.
           c = pos.get_char ();
           if (c == '{' || c == '}') { type = c - '|'; break; }
           if (pos.eol()) break;
           pos.advance (1);
         }
         if (!type) return;       // Nothing found.
         //
         //  Snap cursor to brace.
         //
         selection (shift);
         cursor = pos;
         break;                   // Finish processing any selection and update a few things.
//
//  HOME -- <plain> -- Beginning of line.
//          <ctrl>  -- Beginning of the file.
//          <shift> -- Select a region.
//
    case VK_HOME:
         jwp_conv.clear ();
         selection      (shift);
         clear_cursor   ();
         undo_clear     ();
         if (ctrl) {
           cursor.para = view_top.para = first;
           cursor.line = view_top.line = first->first;
           view_top.y = 0;
           redraw_all ();
         }
         cursor.pos = 0;
         break;
//
//  END -- <plain> -- End of line.
//         <ctrl>  -- End of the file.
//         <shift> -- Select a region.
//
    case VK_END:
         jwp_conv.clear ();
         selection      (shift);
         clear_cursor   ();
         undo_clear     ();
         if (ctrl) {
           cursor.para = view_top.para = last;
           cursor.line = view_top.line = cursor.para->last;
           view_top.y  = total_length;              // Will actually force reverse positioning.
         }
         cursor.pos = cursor.line->length;
         break;
//
//  LEFT -- <plain> -- Move the cursor to the left.
//          <ctrl>  -- Word to the left.
//          <shift> -- Select a region.
//
    case VK_LEFT:
         jwp_conv.clear ();
         clear_cursor   ();
         undo_clear     ();
         selection      (shift);
         if (!ctrl) left ();
           else {
#ifndef ORIGINAL_BEHAVIOR
             for (k = cursor.bol(); !cursor.bof();) {                 // Skip past any whitespace including intervening blank lines.
               left ();
               if (!k && cursor.bol() || !same_class(cursor.get_char(),' ')) break;
             }
             if (!k && cursor.bol() || cursor.bof()) break;           // Stop at beginning of line unless we started there.
             for (i = k = cursor.get_char(); !cursor.bof(); i = j) {  // Find start of word. See the VK_RIGHT handler for more detail on how this works.
               left                 ();
               j = cursor.get_char  ();
               if (k == KANJI_LONGVOWEL) k = j;
               if (!same_class (i,j)) break;
               if (!same_class (k,j)) break;
             }
             if (!cursor.bof() || same_class(j,' ')) right ();
#else
             if (!cursor.bof()) left ();
             while (true) {
               i = char_class(cursor.get_char());
               if (cursor.bof() || ((i != CLASS_JUNK) && (i != CLASS_SPACE))) break;
               left ();
             }
             while ((i == char_class(cursor.get_char())) && !cursor.bof()) left ();
             if (!cursor.bof()) right ();
#endif
           }
         break;
//
//  RIGHT -- <plain> -- Cursor to the right.
//           <ctrl>  -- Word to the right.
//           <shift> -- Select a region.
//
    case VK_RIGHT:
         jwp_conv.clear ();
         clear_cursor   ();
         undo_clear     ();
         selection      (shift);
         if (!ctrl) right ();
           else {
#ifndef ORIGINAL_BEHAVIOR
             for (i = k = cursor.get_char(); !cursor.eof(); i = j) {
               right                ();
               j = cursor.get_char  ();
               if (k == KANJI_LONGVOWEL) k = j;                                   // Try to find a less ambiguous starting character.
               if (!same_class (i,j) && (char_class (j) != CLASS_SPACE)) break;   // Stop on a class change except if we'd end up on whitespace.
               if (!same_class (k,j) && (char_class (j) != CLASS_SPACE)) break;   // This fixes e.g. katakana ending with a long vowel followed by hiragana.
             }
#else
             i = char_class(cursor.get_char());           
             if (i == CLASS_SPACE) i = CLASS_JUNK;
             while (true) {
               if (cursor.eof()) break;
               j = char_class(cursor.get_char());
               if (j == CLASS_SPACE) i = CLASS_JUNK;      // This ensures that the next non-space/non-junk character will satisfy all conditions.
               if ((j != i) && (j != CLASS_SPACE)) break;
               right ();
             }
#endif
           }
         break;
//
//  BACKSPACE -- <in ascii->kana convert> Abort conversion.
//               <in kana->kanji start>   Delete character from conversion.
//               <in selection process>   Delete selection.
//               <plain>                  Delete previous character.
//               <shift>                  Cut selection to clipboard or delete to beginning of line.
//
    case VK_BACK:
         int _shift;
         _shift = shift;                            // A local copy makes the logic simpler.
         shift = false;                             // Prevent problems later down the line.
         if (kana_convert.erase()) return;          // Don't treat discarded input as a deletion event since the file was not affected. Helps keep inline kanji conversion chains intact.
         clear_cursor ();
         undo_clear   (UNDO_DEL);
         if (sel.type == SELECT_KANJI && !_shift) { // This is a special case and needs to be resolved first.
           cursor.para->del_char (this,cursor.line,--cursor.pos);
           all_abs ();
           sel.pos2.pos--;
           if (sel.pos1.pos == sel.pos2.pos) selection_clear ();    
           all_rel ();
           find_pos (&sel.pos2);
           break;
         }
//
// Shift-key logic begins here.
//
         if (sel.type == SELECT_NONE && _shift) {   // Delete to beginning of line on Shift-Backspace if nothing selected.
DeleteToStart:
           if (cursor.bol()) return;
           selection (true);
           clear_cursor ();
           undo_para (UNDO_DEL);
           cursor.pos = 0;                          // Go to beginning of line.
           selection (true);
           selection_delete ();
           break;
         }
         if (sel.type && _shift) {                  // Copy selected text if shift is held (the same way Delete works).
           if (sel.type == SELECT_CONVERT && cursor.pos != sel.pos2.pos) {
             jwp_conv.clear ();                     // Clear out any conversions taking place.
             selection_clear ();
             goto DeleteToStart;                    // Special case if the cursor is not located immediately after the conversion in question.
           }
           clip_cut ();                             // The new clip_cut() takes care of in-progress kanji conversions for us.
           break;
         }
//
// General cases.
//
         if (sel.type == SELECT_CONVERT) { selection_clear (); jwp_conv.clear (); }     // Commit conversion and deselect. The final character in the result will be deleted presently.
         if (sel.type) {
           undo_start       ();
           selection_delete ();
           undo_end         ();
           break;
         }
//
// Do a basic backspace operation.
// Nothing is selected by this point, and shift has been forced off.
//
         if (cursor.bof()) return;
         if (cursor.bol()) {                        // This mess fixes undoing a backspace resulting in a line-join.
           undo_start ();
           undo_para (UNDO_DEL);
           left ();
           ASSERT (cursor.eop());
           goto BS_Kludge;
         }
         undo_para (UNDO_DEL);                      // This keeps the cursor in the correct position when undoing.
         left ();
//
//  DELETE -- <in kana->kanji start>   Delete character from conversion.
//            <in selection process>   Delete selection.
//            <plain>                  Delete character.
//            <shift>                  Cut to clipboard or delete to end of line if nothing selected.
//
    case VK_DELETE:     // *** FALL THROUGH *** //
         if (kana_convert.erase()) return;          // See comment at VK_BACK.
         clear_cursor ();
         undo_clear   (UNDO_DEL);
//
// Shift-key logic begins here.
//
         if (shift && sel.type == SELECT_NONE) {    // Delete to end of line on Shift-Delete if nothing selected.
           if (cursor.eol()) return;
           selection (true);
           clear_cursor ();
           undo_para (UNDO_DEL);
           cursor.pos = cursor.line->length;        // Go to end of line.
           selection (true);
           selection_delete ();
           shift = false;
           break;
         }
         if (shift) {
           shift = false;       // Prevent problems later down the line.
           clip_cut ();
           break;
         }
         if (sel.type == SELECT_CONVERT) { selection_clear (); jwp_conv.clear (); }
         if (sel.type) {
           undo_start       ();
           selection_delete ();
           undo_end         ();
           break;
         }
         if (cursor.eof()) return;
         if (cursor.eop()) {
           undo_start (); BS_Kludge:
           if (!cursor.para->length && cursor.para->next->page_break) {
             cursor.para = cursor.para->next;       // Special case when next paragraph is a page break.
             cursor.line = cursor.para->first;
             cursor.pos  = 0;
             del_paragraph (cursor.para->prev);
             undo_end      ();
             break;
           }
           undo_para     (UNDO_QUE);                // Moving this line here fixes the bug when you undo a line-joining operation.
           cursor.para->page_break = false;
           cursor.para->ins_string (this,cursor.para->last,cursor.para->last->length,cursor.para->next->text,cursor.para->next->length);
           del_paragraph (cursor.para->next);
           undo_end      ();
           break;
         }
         undo_para (UNDO_DEL);
         i = cursor.abs_pos();                                  // Since the delete char will reformat, we need to
         cursor.para->del_char (this,cursor.line,cursor.pos);   //   save cursor position and other information.  We
         cursor.pos  = i;                                       //   want to call del_char with relative coordinates
         cursor.line = cursor.para->first;                      //   for screen redraw efficiency.
         cursor.rel (); 
         break;
//
//  INSERT -- <shift> -- Paste from clipboard.
//            <ctrl>  -- Copy to clipboard.
//            <plain> -- Toggle insert mode.
//
    case VK_INSERT:
         if (shift) {
           clip_paste (false);  // Changed to false to suppress the error message.
           shift = false;
           break;
         }
         if (ctrl) {
           clip_copy ();
           return;
         }
         jwp_config.insert = jwp_config.insert ? false : true;
         jwp_stat.redraw();
         break;
//
//  4 -- <ctrl> -- Four corner lookup
//
    case VK_4:
         if (ctrl) fourcorner_lookup (this);
         return;
//
//  A -- <shift & ctrl> -- Select entire paragraph (Select All).  Used by the edit control routines.
//       <ctrl>         -- Ascii mode.
//
    case VK_A:              
         if (ctrl && shift) {
           jwp_conv.clear ();
           if (not_empty()) {           // Disallow selection if file is completely empty.
             sel.type = SELECT_EDIT;
             sel.pos1.para = first;
             sel.pos1.line = first->first;
             sel.pos1.pos  = 0;
             sel.pos2.para = last;
             sel.pos2.line = last->last;
             sel.pos2.pos  = last->last->length;
             find_pos (&sel.pos1);
             find_pos (&sel.pos2);
           }
           edit_menu  ();
           redraw_all ();
           return;
         }
         if (ctrl) set_mode (MODE_ASCII);
         return;
//
//  B -- <ctrl> -- Search for kanji by bushu
//
    case VK_B:
         if (ctrl && shift) bushu2_lookup (this);
         return;
//
//  C -- <ctrl> -- Copy to clipboard.
//
    case VK_C:
         if (ctrl) clip_copy ();
         return;
//
//  F6 -- Dictionary.
//
    case VK_F6:
         ctrl = true;
//
//  D -- <ctrl> -- Dictionary.
//
    case VK_D:
         if (ctrl) {
           jwp_dict.search (this);
         }
         return;
//
//  H -- <ctrl> -- Hadamitzky/Spahn Lookup
//
    case VK_H:
         if (ctrl) {
           spahn_lookup (this);
         }
         return;
//
//  I -- <ctrl> -- Character Info.
//
    case VK_I:
         if (ctrl && shift) {
           index_lookup (this);
           return;
         }
         if (ctrl) {
           all_abs ();
           if (sel.type && (sel.pos1.pos < sel.pos1.para->length)) i = sel.pos1.para->text[sel.pos1.pos];
           else if (cursor.pos < cursor.para->length) i = cursor.para->text[cursor.pos];
           else i = 0;
           all_rel ();
           kanji_info (window,i);
           break;
         }
         return;
//
//  J -- <ctrl> -- JASCII mode
//
    case VK_J:              // JASCII mode
         if (ctrl) set_mode (MODE_JASCII);
         return;
//
//  K -- <ctrl> -- Kanji mode.
//
    case VK_K:              // Kanji mode
         if (ctrl) set_mode (MODE_KANJI);
         return;
//
//  F5 -- Radical Lookup
//  L -- <ctrl> -- Radical Lookup.
//
    case VK_L: 
         if (!ctrl) return;
    case VK_F5:
         if (shift) bushu_lookup (this); else radical_lookup (this);
         return;
//
//  M -- <ctrl> -- Toggle Japanese IME. Note that ^M also means "determine all" in MS IME Standard.
//
    case VK_M:
         if (!ctrl) return;
#ifndef WINCE
         if (winver >= 0x0500 && !GetSystemMetrics(SM_IMMENABLED)) return;      // Not very effective in practice. It only seems to indicate if "East Asian languages" are installed. I stepped through it and it doesn't seem to be a very expensive operation though.
         jwp_conv.clear   ();
         undo_clear       ();                                                   // Break the chain here, since errors and confusion may abound.
         ImmSimulateHotKey(main_window,IME_JHOTKEY_CLOSE_OPEN);                 // Toggle Japanese IME, if available. The return value does not seem to reflect the result of the attempted IME activation.
#endif
         return;
//
//  R -- <ctrl+shift> -- Reading lookup
//
    case VK_R:
         if (ctrl && shift) reading_lookup (this);
         return;
//
//  S -- <ctrl+shift> -- SKIP lookup
//
    case VK_S:
         if (ctrl && shift) skip_lookup (this);
         return;
// 
//  T -- <ctrl> -- JIS table
//  
    case VK_T:
         if (ctrl) jis_table (this);
         return;
//
//  V -- <ctrl> -- Paste from clipboard
//
    case VK_V:
         if (ctrl) clip_paste (false);  // Changed to false to suppress the error message.
         shift = false;                 // Fixes selection errors.
         break;
//
//  W -- <ctrl> -- Select word.
//
    case VK_W:              // Select word
         if (!ctrl) return;
         clear_cursor   ();
         jwp_conv.clear ();
#ifndef ORIGINAL_BEHAVIOR
         if (shift) {
           do_key (VK_HOME,false,false);
           do_key (VK_END,false,true);
           return;
         }
         j = 0;
         if (sel.type && sel.pos2.pos == cursor.pos) j = 1;                       // If preceding is selected, treat it like a separate word.
         else if (cursor.eol () && !cursor.bol()) do_key (VK_LEFT,false,false);   // This will cause it to select leftwards at end of line.
         if (same_class(k=cursor.get_char(),' ')) do_key (VK_RIGHT,true,false);   // Skip whitespace to start of next word.
         else if (!j && !cursor.bol()) {            // This could be more compact but the comments and logic are a bit clearer this way.
           if (!same_class(cursor.get_left(),k)) ;  // Peek at preceding. If types differs then cursor must be at a word boundary already.
           else do_key (VK_LEFT,true,false);        // Move cursor back to beginning of word.
         }
         selection (false);                         // Cancel previous selection, if any.
         do_key (VK_RIGHT,true,true);               // Highlight up to the next word. Safe to call at EOF or whatever.
         while (!cursor.bol() && char_class(cursor.get_left()) == CLASS_SPACE) do_key (VK_LEFT,false,true);
         return;
#else
         while (true) {                     // Find beginning of word.
           i = char_class(cursor.get_char());
           if (cursor.bof() || ((i != CLASS_JUNK) && (i != CLASS_SPACE))) break;
           do_key (VK_LEFT,false,false);
         }                                  
         while ((i == char_class(cursor.get_char())) && !cursor.bof()) do_key (VK_LEFT,false,false); 
         if (!cursor.bof()) do_key (VK_RIGHT,false,false);
         if (i == CLASS_SPACE) i = CLASS_JUNK;
         while (true) {                     // find the end of the word.
           if (cursor.eof()) break;
           if (i != char_class(cursor.get_char())) break;
           do_key (VK_RIGHT,false,true);
         }
#endif
         break;
//
//  X -- <ctrl> -- Cut to clipboard.
//
    case VK_X:
         shift = false;   // Fixes selection errors.
         if (ctrl) clip_cut ();
         break;
//
//  Y -- <ctrl> -- Redo.
//
    case VK_Y:
Redo:    if (ctrl) do_redo ();
         return;
//
//  Z -- <ctrl>       -- Undo.
//       <ctrl+shift> -- Redo.
//
    case VK_Z:
         if (ctrl && shift) goto Redo;
         if (ctrl) do_undo ();
         return;
//
//  Space and > Conversion to the left with control
//
    case VK_GT:
         if (!ctrl) return;
//
//  F2 -- Forward kana->kanji convert.
//
    case VK_F2:
         if (shift) {
FinalizeConversion:                                                   // Shared by F3/LT. The direction (CONVERT_RIGHT) only matters if a conversion is in progress (SELECT_CONVERT).
           if (sel.type != SELECT_CONVERT) convert (CONVERT_RIGHT);   // Unless a conversion is already in progress, try to convert to the default candidate.
           jwp_conv.clear  ();                                        // End any in-progress conversion, accepting whichever kanji is selected at the moment.
           selection_clear ();                                        // Deselect any text affected by the conversion process.
           return;
         }
         convert (CONVERT_RIGHT);
         return;
//
//  < Convertion to the left
//
    case VK_LT:
         if (!ctrl) return;
//
//  F3 -- Backward kana->kanji convert.
//
    case VK_F3:
         //if (key == VK_F3 && !shift && !ctrl && TODO) goto DoNext;  // Force F3 to be an alternate search key.
         if (key == VK_F3 && !shift && !ctrl && (!sel.type || sel.type == SELECT_EDIT)) do {
           int leng,sleng;
           KANJI *kstr;
           if (kana_convert.pending_AIUEO ()) break;        // User probably intended to forcibly convert A/I/U/E/O to a kanji.
           leng = jwp_search.search_leng ();                // Check if a search string exists and record its length.
           if (sel.type == SELECT_NONE && !leng) break;     // No search string so ignore this command. For this conditional, we could do_next() anyway, which would open up the search dialog, but that would encourage using F3 for all searches instead of the proper F8 key.
           if (sel.type == SELECT_EDIT) {                   // Text is selected so check if it matches the most recent search string, in which case the user probably intended to repeat the search.
             sleng = get_selected_str (&kstr);
             if (!sleng) goto DoNext;                       // Not sure if this can actually happen but might as well check for a selection of length zero.
             for (i = 0; i < sleng; i++) {
               if (!ISKANA(kstr[i])) goto DoNext;           // If the selection contains any non-kana (i.e. it's absolutely inconvertible) then assume user intended to search.
             }
             if (get_selected_str (&kstr) != leng) break;   // Require selection to be the same length as the search string, since test() doesn't (and can't) check the length of its input.
             if (!jwp_search.test (kstr)) break;            // If selection doesn't match search string (accounting for case, etc.), assume user intended an explicit conversion.
           }
DoNext:    jwp_search.do_next (NULL);                       // We have high confidence that this F3 invocation was intended to repeat a search, not convert to kanji.
           return;
         } while (0);                                       // End of F3 overloading.
         if (shift) goto FinalizeConversion;
         convert (CONVERT_LEFT);
         return;
//
//  F4 or Ctrl-^ -- Toggle input mode (kanji--ascii)
//
    case VK_6:
         if (!ctrl) return;
    case VK_F4:
         set_mode (MODE_TOGGLE);
         return;
//
//  Application key, and shift-F10 -- Cause the same as a right mouse click at the cursor location.
//
//  F23 -- This is really a message from Windows CE on the palm computer.
//         This is a select button which popus up the popup menu.
//
    case VK_F23:
    case VK_APPS:
         popup_menu (cursor.x-view_top.x,cursor.y-view_top.y);
         return;
//
//  RETURN -- <ctrl>  -- Insert page break.
//            <shift> -- Insert soft return. [This is false.]
//            <plain> -- Insert paragraph.
//
    case VK_RETURN:
         shift = false;                             // Prevents selection complications.
         if (sel.type == SELECT_KANJI) convert (CONVERT_RIGHT);
         jwp_conv.clear  ();
         undo_start      ();
         clear_cursor    ();                        // This needs to be after undo_start to keep the correct cursor position when undoing.
         if (sel.type == SELECT_EDIT) selection_delete(); else selection_clear ();
//
//      Page break.
//
         if (ctrl) {
           if (cursor.para->next && !cursor.para->length && !cursor.para->page_break) {
             set_page (cursor.para,true);           // Blank paragrah, so simply turn it 
             do_key   (VK_RIGHT,false,false);       //   into a page break.
             undo_end ();
             break;
           }
           if (cursor.bop()) {
             do_key   (VK_RETURN,false,false);      // At beggining of paragraph, add new 
             set_page (cursor.para->prev,true);     //   paragraph and make it a page break.
             undo_end ();
             break;
           }
           if (cursor.eop()) {                      // End of paragraph, add new paragraph
             do_key   (VK_RETURN,false,false);      //   and make it a page break.
             set_page (cursor.para,true);
             if (cursor.para == last) new_paragraph (cursor.para);
             do_key   (VK_RIGHT,false,false);
             undo_end ();
             break;
           }
           do_key (VK_RETURN,false,false);          // Mid-paragraph process
           if (!new_paragraph(cursor.para->prev)) set_page (cursor.para->prev,true);
           undo_end ();
           break;
         }
//
//      Insert new paragraph.
//
         if (new_paragraph(cursor.para)) { undo_end (); break; }
         if (cursor.para->page_break) {             // This is a page break so simply
           set_page (cursor.para,false);            //   insert a new paragraph and move
           set_page (cursor.para->next,true);       //   down to it.
           do_key   (VK_RIGHT,false,false);
           undo_end ();
           break;
         }
         if (!cursor.eop()) {                       // If we were not at the end of the 
           undo_para  (UNDO_QUE);                   //   paragraph, we need to correct this
           i = cursor.abs_pos();                    //   this paragraph.
           cursor.para->next->ins_string (this,cursor.para->next->first,0,cursor.para->text+i,cursor.para->length-i);
           cursor.para->length = i;
         }
         undo_para (UNDO_QUE);                          // This line fixes the cursor position when undoing a linefeed at the end of a line.
         cursor.para->format (this,cursor.line,true);   // Correct next paragraph.
         do_key (VK_RIGHT,false,false);         
         cursor.para->format (this,NULL,true);
         undo_end ();
         break;
//
//  UP -- <ctrl>  -- Scroll upward, moving the cursor if necessary to keep it visible.
//  (old) <ctrl>  -- kana->kanji convertion forward (right) or scroll upward.
//        <plain> -- Up one line.
//        <shift> -- Extend selection.
//        SPECIAL -- On PPC/PocketPC up during a kanji conversion is taken as convert.
//
    case VK_UP:
         /* This is the old behavior, which has been removed since it was interfering with the new Ctrl-Up/Down scrolling.
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         if ((sel.type == SELECT_KANJI) || (sel.type == SELECT_CONVERT)) {
           convert (CONVERT_RIGHT);
           break;
         }
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         if (ctrl && !sel.type) {
           shift = false;
           v_scroll (SB_LINEUP);
           return;
         }
         if (ctrl) {
           shift = false;
           convert (CONVERT_RIGHT);
           break;
         }
         */
         if (ctrl) {
           shift = false;
           v_scroll (SB_LINEUP);
           //find_pos (&cursor);                        // This could be redundant, but it's what view_check() does initially.
           if (cursor.y-view_top.y <= height) return;   // Return if cursor did not drop off the bottom (conditional based on view_check).
         }
         if (jwp_config.cfg.page_mode_file) goto PageUp;
         jwp_conv.clear ();
         undo_clear     ();
         selection      (shift);
         if (!x_cursor) x_cursor = (short) cursor.x;
         if (cursor.move_up()) break;               // The selection will be disabled if necessary.
         cursor.align (this,x_cursor);
         break;
//
//  DOWN -- <ctrl>  -- Scroll downward, moving the cursor if necessary to keep it visible.
//    (old) <ctrl>  -- kana->kanji convertion backward (left) or scroll downward.
//          <plain> -- Down one line.
//          <shift> -- Extend selection.
//          SPECIAL -- On PPC/PocketPC down during a kanji conversion is taken as convert.
//
    case VK_DOWN:
         /* See commented-out section above for explanation.
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         if ((sel.type == SELECT_KANJI) || (sel.type == SELECT_CONVERT)) {
           convert (CONVERT_RIGHT);
           break;
         }
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         if (ctrl && !sel.type) {
           shift = false;
           v_scroll (SB_LINEDOWN);
           return;
         }
         if (ctrl) {
           shift = false;
           convert (CONVERT_LEFT);
           break;
         }
         */
         if (ctrl) {
           shift = false;
           v_scroll (SB_LINEDOWN);
           if ((cursor.y > view_top.y+JWP_FONT.vspace) && (total_length > view_top.y+height-JWP_FONT.vheight)) return;  // See VK_UP for comments and caveats.
         }
         if (jwp_config.cfg.page_mode_file) goto PageDown;
         jwp_conv.clear ();
         undo_clear     ();
         selection      (shift);
         if (!x_cursor) x_cursor = (short) cursor.x;
         if (cursor.move_down()) break;
         cursor.align (this,x_cursor);
         break;
//
//  PAGEUP -- <ctrl>  -- Previous file (not implemented here).
//            <plain> -- Up one screen full.
//            <shift> -- Extend selection.
//
    case VK_PRIOR:
PageUp:;
         jwp_conv.clear ();
         undo_clear     ();
         selection      (shift);
         if (!x_cursor) x_cursor = (short) cursor.x;
         i = cursor.y-vscroll;
         while (!cursor.move_up()) {
           find_pos (&cursor,POS_VERT);
           if (cursor.y <= i) break;        
         }
         cursor.align (this,x_cursor);
         break;
//
//  PAGEDOWN -- <ctrl>  -- Next file (not implemented here).
//              <plain> -- Down one screen full.
//              <shift> -- Extend selection.
//
    case VK_NEXT:  // page down
PageDown:;
         jwp_conv.clear ();
         undo_clear     ();
         selection      (shift);
         if (!x_cursor) x_cursor = (short) cursor.x;
         i = cursor.y+vscroll;
         while (!cursor.move_down()) {
           find_pos (&cursor,POS_VERT);
           if (cursor.y >= i) break;
         }
         cursor.align (this,x_cursor);
         break;
//
//  TAB -- Implements a tab.  The Tab normally would be processed 
//         through the do_char() rotuine.  This had to be intercepted
//         because ctrl-I was being intercepted.
//
    case VK_TAB:
         if (shift) return;
         if (jwp_config.mode == MODE_KANJI) {
           if (sel.type == SELECT_KANJI) jwp_conv.clear ();   // Start the conversion since tabs are obviously not part of it.
           else kana_convert.clear ();                        // Output any pending kana since jwp_conv.clear() is now being called conditionally.
         }
         do_char        ('\t');
         break;
    default:
         return;
  }
  if (shift) selection (true);
  view_check ();
  return;
}

//--------------------------------
//
//  Process all mouse commands in the main window.  These primarally 
//  consist of move cursor, select region, and select word.
//
//      iMsg   -- Windows message to process.
//      wParam -- Contains the keyboard flags for the mouse event.
//      lParam -- Contains the mouse location.
//

void JWP_file::do_mouse (int iMsg,WPARAM wParam,LPARAM lParam) {
  static short      mouse_x,mouse_y;    // Location of mouse click.
  static Paragraph *last_para = NULL;   // These remember the last selection 
  static Line      *last_line = NULL;   //   render position.  This is used    
  static short      last_pos  = 0;      //   to prevent unnecessary rendring.
  static short      selecting = false;  // Set to true when doing a selection
  Paragraph *para = NULL;               // Added because LINUX compier is woried about these not
  Line      *line = NULL;               //   being set.
  Position   mouse;
  int        i,insel,y,x_pos,y_pos;

  switch (iMsg) {
    case WM_TIMER:
         selecting = false; 
         last_para = NULL;        // Remove the extraneous selection.
         selection_clear ();      // Holding LMB is only intended for opening the menu and is useless for selections.
         KillTimer       (window,TIMER_MOUSEHOLD); 
         ReleaseCapture  (); 
         popup_menu      (mouse_x,mouse_y);
         return;
    case WM_LBUTTONUP:
         KillTimer       (window,TIMER_MOUSEHOLD);
         ReleaseCapture  ();
         if (!selecting) return;
         break;
    case WM_LBUTTONDOWN:
         if (wParam & MK_SHIFT) { iMsg = WMU_KANJIINFO; break; }      // Shift+LMB -> kanji information.
         mouse_x = LOWORD(lParam); 
         mouse_y = HIWORD(lParam); 
         SetTimer        (window,TIMER_MOUSEHOLD,GetDoubleClickTime(),NULL);
         SetCapture      (window);
         if (wParam & MK_CONTROL)           iMsg = WM_LBUTTONDBLCLK;  // Ctrl+LMB -> left button double click.
         else if (GetKeyState(VK_MENU) < 0) iMsg = WM_RBUTTONDOWN;    // Alt+LMB -> right button click.
         break;
    case WM_MOUSEMOVE:
         if ((abs(LOWORD(lParam)-mouse_x) > DOUBLE_X) || (abs(HIWORD(lParam)-mouse_y) > DOUBLE_Y)) KillTimer (window,TIMER_MOUSEHOLD);
         if (!selecting) return;
         break;
  }
//
//  Set focus to this window.  Translate messages as needed.
//
  SetFocus (window);
//
//  Find the y location of the mosue click.
//
  x_pos = (short) LOWORD(lParam);                       // Decode mouse position.
  y_pos = (short) HIWORD(lParam);
  y     = JWP_FONT.y_offset;
  line  = view_top.line;                                // Intialize to the top of the display.
  for (para = view_top.para; para; para = para->next) { // Find vertical position
    if (!line) line = para->first;
    for (; line; line = line->next) {
      if (y >= y_pos) goto FoundYPosition;
      y += (para->spacing*JWP_FONT.vheight)/100;
    }
  }
  if (!line) { para = last; line = para->last; }        // Vertical is past EOF
FoundYPosition:
//
//  Set location of the mouse click in file coordinates.
//
  mouse.para = para;
  mouse.line = line;
  mouse.align (this,x_pos+view_top.x,!selecting);
//
//  Detemine if we need to move the cursor.  If this is a right mouse
//  button event within a marked region we don't move the cursor,
//  otherwise we need to move the cursor to where the click was.
//
  insel = false;
  if (iMsg != WM_LBUTTONUP) {                           // Don't move the mouse if this is cursor up.
    if ((iMsg != WM_RBUTTONDOWN) || !(insel = in_selection(&mouse))) {
      cursor.para = mouse.para;     // Instead of doing a full structure copy we copy the position 
      cursor.line = mouse.line;     //   elements.  This preserves the x,y location in case the cursor did
      cursor.pos  = mouse.pos;      //   not move.  This saves a call to find_pos() later.
    }
  }
  if ((iMsg == WM_RBUTTONDOWN) && !insel) selection_clear ();
//
//  Now se have to process the individual actions.
//
  switch (iMsg) {
//
//  Alt-button, gives rise to get character info.
//
    case WMU_KANJIINFO: KanjiInfo:
         do_key (VK_I,true,false);
         iMsg = WM_RBUTTONDOWN;                 // Trick the motion routines into not generating a selection.
         break;
//
//  Right button click.
//
    case WM_RBUTTONDOWN:                        // Right button.  Bring up popup menu and let it send messages back to our window.
         if (wParam & MK_SHIFT) goto KanjiInfo;
         popup_menu (x_pos,y_pos);
         return;
//
//  Double click -> select a word.
//
    case WM_LBUTTONDBLCLK:
         do_key (VK_W,true,false);
         return;
//
//  Left button click.  Clear old slelction, and start a new selection.
//
    case WM_LBUTTONDOWN:
         selection_clear ();
         selecting = true;                      // Indicates that we are in a mouse select.
         last_para = NULL;                      // This will force updating of the cursor location.
         break;
//
//  Releaseing the left button.
//
    case WM_LBUTTONUP:
         selecting = false;                     // Disable the selection draw.
         if (sel.type && (sel.pos1.para == sel.pos2.para) && (sel.pos1.line == sel.pos2.line) && (sel.pos1.pos == sel.pos2.pos)) {
           iMsg      = WM_RBUTTONDOWN;          // If select is zero width block selection generation, ie. just move cursor
           last_para = NULL;                    // This will force the cursor movement (thus redraw).
           selection_clear ();
         }
         break;
//
//  Just update the mouse position.
//
    case WM_MOUSEMOVE:
         break;
  }
//
//  This section updates the cursor location.
//
  if ((last_para != para) || (last_line != line) || (last_pos != cursor.pos)) {
    last_para = cursor.para;                    // Que new position and render
    last_line = cursor.line;
    last_pos  = cursor.pos;
    view_check ();
    selection  ((iMsg != WM_RBUTTONDOWN) && not_empty() ? SEL_MOUSE : false);  // Block generation of select if right button or no text.
  }
//
//  Auto-scroll handler.  When currsor is close enough to the edge 
//  we generate move up or move down commands necessary to scroll the 
//  list.
//
  if (!jwp_config.cfg.auto_scroll) return;
  if     ((HIWORD(lParam) < JWP_FONT.height/3) && (view_top.line != first->first)) i = SB_LINEUP;
  else if (HIWORD(lParam) > height-JWP_FONT.height/3) {
    Position pos;                               // This section makes sure we do not 
    pos.para = last;                            //   over-scroll the display.  This is 
    pos.line = last->last;                      //   such a mess becuase the difficulties of
    pos.pos  = 0;                               //   the variable length paragraphs.
    find_pos (&pos);
    if (pos.y <= view_top.y+height-JWP_FONT.vheight) return;
    i = SB_LINEDOWN;
  }
  else return;                                  // No auto-scroll so exit.
  SendMessage  (window,WM_VSCROLL,i,0);         // Scroll list.
  UpdateWindow (window);                        // Force window redraw
  SetTimer     (window,TIMER_AUTOSCROLL,jwp_config.cfg.scroll_speed,NULL);
  return;
}

//--------------------------------
//
//  Draw all lines in the file.
//
//      hdc   -- Conext to render into.
//      bound -- Bounding rectangle for rendering.
//
void JWP_file::draw_all (HDC hdc,RECT *bound) {
  int        y;
  HFONT      font;
  Paragraph *para;
  JWP_font  *kfont = &JWP_FONT;
  Line      *line  = view_top.line;
  y            = kfont->y_offset;
  bound->left -= kfont->x_offset;
  font = (HFONT) SelectObject (hdc,JWP_FONT.ascii);
  SetBkMode (hdc,TRANSPARENT);
  for (para = view_top.para; para; para = para->next) {
    if (!line) line = para->first;
    for (; line; line = line->next) {
      if (y >= bound->top) draw_line (hdc,para,line,y,bound->left,bound->right,kfont);
      if (y > bound->bottom) return;
      y += (para->spacing*kfont->vheight)/100;
    }
  }
#ifdef USE_REDRAW_LINE_BLANKING
  RECT rect;
  rect.left   = 0;
  rect.right  = width;
  rect.top    = y-jwp_font.rheight;
  rect.bottom = height;
  if (filetype == FILETYPE_EDIT) { rect.left++; rect.right--; rect.bottom--; };
  BackFillRect (hdc,&rect);
#endif USE_REDRAW_LINE_BLANKING
  SelectObject (hdc,font);
  return;
}

//--------------------------------
//
//  The core of the screen redering.  This routine renders an actuall 
//  line with all details.
//
//      hdc       -- Display context to render into.
//      para      -- Paragraph containning line.
//      line      -- Line to be rendred.
//      y         -- Screen pixal location for line.
//      xmin,xmax -- Restricts rendering to the indicated range (sort of).
//
void JWP_file::draw_line (HDC hdc,Paragraph *para,Line *line,int y,int xmin,int xmax,class JWP_font *font) {
  KANJI  ch;
  RECT   rect;
  int    i,j,x;
#ifdef USE_REDRAW_LINE_BLANKING
  rect.left   = (filetype == FILETYPE_EDIT) ? 1 : 0;
  rect.right  = width-1;
  rect.top    = y-jwp_font.rheight;
  rect.bottom = rect.top + (para->spacing*jwp_font.vheight)/100;
  BackFillRect (hdc,&rect);
#endif USE_REDRAW_LINE_BLANKING
//
//  This is a hard page break, so draw a bar.
//
  if (para->page_break) {
    rect.left   = font->x_offset;
    rect.right  = xmax;
    rect.top    = y-(5*font->height)/8;
    rect.bottom = rect.top+font->height/4;
    FillRect (hdc,&rect,(HBRUSH) GetStockObject(LTGRAY_BRUSH));
    y += view_top.y;                                        // Page break in selection 
    if (!sel.type || (y >= sel.pos2.y) || (y < sel.pos1.y)) return; // selection ends at page break -> page break is not included
    rect.top    = y-font->height-view_top.y;
    rect.bottom = rect.top+font->vheight;
    InvertRect (hdc,&rect);
    return;
  }
//
//  Draw a real line of text.
//
  x = para->line_start(line,font)-view_top.x;       // Calculate this line's starting horizontal offset.
  font->hadvance(x,0,HADV_START);
  for (i = 0; i < line->length; i++) {              // Determine start character
    j = font->hadvance(x,para->text[line->first+i],HADV_PEEK);  // Calculate tentative x position.
    if (j >= xmin) break;
        font->hadvance(x,para->text[line->first+i],HADV_CONT);  // Commit internal x position.
    x = j;
  }
  for (; (i < line->length) && (x <= xmax); i++) {  // draw characters for line
    ch = para->text[line->first+i];
    if (ISJIS(ch)) {                                //   JIS character
      font->kanji->draw (hdc,ch,x,y);  
    }
    else if (ch != '\t') {                          //   ASCII character
      ascii_draw (hdc,x,y-font->height,ch);
    }
    x = font->hadvance (x,ch,HADV_CONT);            //   Advance position
    if (x > xmax) break;
  }
//
//  If text is slected render inversion.
//
  if (!sel.type) return;
  if ((sel.pos1.para == sel.pos2.para) && (sel.pos1.line == sel.pos2.line) && (sel.pos1.pos == sel.pos2.pos)) return; // Ignore null selection.
  y += view_top.y;
  if ((y > sel.pos2.y) || (y < sel.pos1.y)) return;
  if (sel.pos1.y == sel.pos2.y) {
    rect.left  = sel.pos1.x-view_top.x;
    rect.right = sel.pos2.x-view_top.x;
  }
  else if (y == sel.pos1.y) {
    rect.left  = sel.pos1.x-view_top.x;
    rect.right = x;
  }
  else if (y == sel.pos2.y) {
    rect.left  = font->x_offset;
    rect.right = sel.pos2.x-view_top.x;
  }
  else {
    rect.left  = font->x_offset;
    rect.right = x;
  }
  rect.top    = y-font->height-view_top.y-1;        // ### just added -1.  If I like this move it back to ajdust routines.
  rect.bottom = rect.top+font->vheight;
  rect.left--;
  InvertRect (hdc,&rect);
  return;
}

//--------------------------------
//
//  Find the location of a point within the file.
//
//  Fill in the x & y data values with the absolute locations within the 
//  file.
//
//      loc  -- Location.
//      code -- Indicates type of anaysis to perform:
//
//          POS_CURSOR -- Normal analysis for a cursor position.
//          POS_VIEW   -- Perform fixed space (kanji based) analysis
//                        used for the view point.
//          POS_VERT   -- Only vertical analysis (assumes horizontal 
//                        does not change).
//          POS_VVERT  -- Vertical mdotion for view top point only.
//
void JWP_file::find_pos (Position *loc,int code) {
  int        i;
  Paragraph *para;
  Line      *line = NULL;
  loc->y = JWP_FONT.y_offset;
  for (para = first; para; para = para->next) {
    for (line = para->first; line && (line != loc->line); line = line->next) loc->y += (para->spacing*JWP_FONT.vheight)/100;
    if (line == loc->line) break;
  }
  if (code == POS_VVERT) {
    loc->y -= JWP_FONT.y_offset;
    return;
  }
  if (code == POS_VERT) return;
  if (code == POS_CURSOR) {
    loc->x = loc->para->line_start(loc->line,&JWP_FONT);
    JWP_FONT.hadvance(loc->x,0,HADV_START);
    for (i = 0; (i < loc->pos) && (i < loc->line->length); i++) loc->x = JWP_FONT.hadvance(loc->x,para->text[line->first+i],HADV_CONT);
  }
  else {    // VIEW
    loc->x  = JWP_FONT.hwidth*loc->pos;
    loc->y -= JWP_FONT.y_offset;
  }
  return;
}

//--------------------------------
//
//  Processes horizontal scroll bar messages.
//
//      message -- Windows message parameter.
//
void JWP_file::h_scroll (int message) {
  switch (LOWORD(message)) {
    case SB_LINEUP:
         view_top.pos--;
         break;
    case SB_LINEDOWN:
         view_top.pos++;
         break;
    case SB_PAGEUP:
         view_top.pos -= 2*hscroll;
         break;
    case SB_PAGEDOWN:
         view_top.pos += 2*hscroll;
         break;
    case SB_THUMBTRACK:
  //case SB_THUMBPOSITION:                        // Removing this eliminates a redundant redraw without any negative impacts, as far as I can tell.
         GetScrollInfo (window,SB_HORZ,&scroll_info);
         //scroll_info.nPos = HIWORD(message);    // ### This is a kludge because GetScrollInfo does not return the correct data.
         if (SB_THUMBTRACK == LOWORD(message)) scroll_info.nPos = scroll_info.nTrackPos;    // This replaces the above kludge.
         view_top.pos = scroll_info.nPos;
         break;
    default:
         return;
  }
  if (view_top.pos+char_width > char_pagewidth) view_top.pos = char_pagewidth-char_width;
  if (view_top.pos < 0) view_top.pos = 0;
  find_pos (&view_top,POS_VIEW);
  set_scroll ();
  redraw_all ();
  caret_on   ();
  return;
}

//--------------------------------
//
//  Process character from the IME.
//
//      ch      -- Character.
//      unicode -- Non-zero indicates UNICODE 
//
void JWP_file::ime_char (int ch,int unicode) {
  if (ch == '\t') return;
#ifdef WINCE
  put_char (ch,CHAR_STOP);
#else  WINCE
  if      (ch < 0x100) put_char (ch,CHAR_STOP);
  else if (unicode)    put_char (unicode2jis(ch,KANJI_BAD),CHAR_STOP);
  else                 put_char (sjis2jis   (ch)          ,CHAR_STOP);
#endif WINCE
  return;
}

//--------------------------------
//
//  This routine determines if the indicated location is within the 
//  selected region.
//
//      loc    -- Location to be tested.
//
//      RETURN -- A non-zero return value indicates the point is 
//                within the selection.  A value of zero indicates
//                the point is not in the selected region.
//
int JWP_file::in_selection (Position *loc) {
  if (!sel.type) return (false);            // No selection cannot do anything.
  find_pos (loc);                           // Convert coordinates for everything.
  all_abs  ();
  loc->abs ();
  if (sel.pos1.para == sel.pos2.para) {     // Selection all in one paragraph
    if ((sel.pos1.para == loc->para) && (sel.pos1.pos <= loc->pos) && (sel.pos2.pos >= loc->pos)) return (true);
  }
  else if (sel.pos1.para == loc->para) {    // Extended selection, check start paragraph
    if (sel.pos1.pos <= loc->pos) return (true);
  }
  else if (sel.pos2.para == loc->para) {    // Extneded selection, check ending paragraph
    if (sel.pos2.pos >= loc->pos) return (true);
  }                                         // Extneded selection, check central region.
  else if ((sel.pos1.y <= loc->y) && (sel.pos2.y >= loc->y)) return (true);
  loc->rel ();                              // Convert corrdinates back.
  all_rel  ();
  return   (false);
}

//--------------------------------
//
//  Utility fucntion used to insert from a dialog into a file with undo.
//
//      kanji  -- Kanji string to be insrted.
//      length -- Length of string.
//
void JWP_file::insert_string (KANJI *kanji,int length) {
  selection_clear ();                           // Fixes a few bugs such as an insertion after a right-to-left selection.
  undo_para  (UNDO_ANY);                        // Allow specific undo of put back.
  put_string (kanji,length);
  view_check ();
  return;
}

//--------------------------------
//
//  Generate a popup menu.
//
//      x,y -- Location for menu, relative to the window.
//
void JWP_file::popup_menu (int x,int y) {
  int   i;
  HMENU pmenu;
  RECT  rect;
  pmenu = GetSubMenu(popup,0);
  i     = MF_BYCOMMAND | (sel.type ? MF_ENABLED : MF_GRAYED);
  EnableMenuItem (pmenu,IDM_EDIT_COPY,i);
  EnableMenuItem (pmenu,IDM_EDIT_CUT ,i);
  EnableMenuItem (pmenu,IDM_EDIT_UNDO,MF_BYCOMMAND | (undo && undo[0]) ? MF_ENABLED : MF_GRAYED);
  EnableMenuItem (pmenu,IDM_EDIT_REDO,MF_BYCOMMAND | (redo && redo[0]) ? MF_ENABLED : MF_GRAYED);
  for (i = MODE_KANJI; i <= MODE_JASCII; i++) CheckMenuItem (pmenu,IDM_EDIT_MODE_KANJI+i,(i == jwp_config.mode) ? MF_CHECKED : MF_UNCHECKED);
  GetWindowRect  (window,&rect);
  x += rect.left;
  y += rect.top;
  view_check ();                                // These allow us to move the cursor before the
  caret_on   ();                                //   menu appears.  This tends to look nicer.
#ifdef WINCE
  TrackPopupMenu (pmenu,TPM_LEFTALIGN | TPM_TOPALIGN,x,y,0,window,NULL);
#else  WINCE
  TrackPopupMenu (pmenu,TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,x,y,0,window,NULL);
#endif WINCE
  return;
}

//--------------------------------
//
//  This routine is used to insert a string at the current cursor location.
//  This is not used for editing fucntions, but rather for dialog boxes
//  to put text back into the main file.  For example, the Character
//  Information dialog box uses this to put information back into 
//  the file.
//
//      string -- String to be inserted.
//      length -- Length of the string.
//
void JWP_file::put_string (KANJI *string,int length) {
  int pos;
  pos = cursor.abs_pos();                   
  cursor.para->ins_string (this,cursor.line,cursor.pos,string,length);
  cursor.line = cursor.para->first;             // Because of the reformat we need to make 
  cursor.pos  = pos;                            //   sure this is a real abosolute cursor 
  cursor.advance (length);                      //   location.
  return;
}

//--------------------------------
//
//  Redraw the entire file.
//
void JWP_file::redraw_all () {
  InvalidateRect (window,NULL,REDRAW_BLANK);
  return;
}

//--------------------------------
//
//  Redraw from the beginning of the specified paragraph to the end of 
//  the file.  This is used when lines are added or removed from the
//  file.  
//
//      para -- Paragraph to redraw from.
//      line -- Line to start the redraw on (NULL indicates beginning 
//              of paragraph.
//
void JWP_file::redraw_from (Paragraph *para,Line *line) {
  RECT     rect;
  Position pos;
  if (filetype == FILETYPE_WORK) return;            // This routine is called during 
                                                    //   clipboard generation.  This 
                                                    //   blocks the resultant redraw.
  pos.para = para;                                  // Find position of the top of the paragraph.
  pos.line = line ? line : para->first;
  find_pos (&pos,POS_VERT);
  rect.left   = 0;                                  // Build redraw rectangle
  rect.right  = width;
  rect.top    = pos.y-view_top.y-JWP_FONT.rheight;
  rect.bottom = height;
  InvalidateRect (window,&rect,REDRAW_BLANK);       // Redraw
  return;
}

//--------------------------------
//
//  Redraw the contents of the paragraph.
//
//      para -- Paragraph to redraw.
//      line -- Line to start the redraw on (NULL indicates beginning 
//              of paragraph.
//
void JWP_file::redraw_para (Paragraph *para,Line *line) {
  RECT rect;
  Position pos1,pos2;
  if (!window) return;                          // Suppress redraw for working files.
  pos1.para = para;                             // Get first and last position.
  pos1.line = line ? line : para->first;
  pos2.para = para;
  pos2.line = para->last;
  find_pos (&pos1,POS_VERT);                
  find_pos (&pos2,POS_VERT);
  rect.left   = 0;                              // Build redraw rectangle
  rect.right  = width;
  rect.top    = pos1.y-view_top.y-JWP_FONT.rheight;
  rect.bottom = pos2.y-view_top.y-JWP_FONT.rheight+(para->spacing*JWP_FONT.vheight)/100;
  InvalidateRect (window,&rect,REDRAW_BLANK);   // Do redraw.
  return;
}

//--------------------------------
//
//  Redraw a range of the screen based on the pixal locations (measured
//  from the top of the file).
//
//      pos1  -- Start location.
//      para2 -- Paragraph object containning the location pos2 
//               (used for paragraph line spacing).
//      pos2  -- End position
//  
void JWP_file::redraw_range (int pos1,Paragraph *para2,int pos2) {
  RECT rect;
  rect.left   = 0;                              // Build redraw rectangle
  rect.right  = width;
  rect.top    = pos1-view_top.y-JWP_FONT.rheight;
  rect.bottom = pos2-view_top.y-JWP_FONT.rheight+(para2->spacing*JWP_FONT.vheight)/100;
  InvalidateRect (window,&rect,REDRAW_BLANK);   // Redraw
  return;
}

//--------------------------------
//
//  This routine forces a reformatting of all paragraphs in the file.
//
void JWP_file::reformat () {
  Paragraph *para;
  all_abs ();
  cursor.rel ();
  for (para = first; para; para = para->next) para->format(this,NULL,false);
  cursor.abs ();
  all_rel ();
  return;
}

//--------------------------------
//
//  Main selection driver routine.  This is called before a cursor 
//  movement, and after.  This will cause the selection to be 
//  generated and mantained as the cursor is moved.
//
//      shift -- Inidcates the state of the shift.
//
//  Setting shift to SEL_MOUSE, is used when processing mosue messages.
//  This generates some different options.
//
void JWP_file::selection (int shift) {
  Position pos,last;
//
//  No shift, so just clear the current selection.
//
  if (!shift) {
    selection_clear ();
    return;
  }
//
//  If coming off of a kanji convert clear the selection and set the 
//  new position.
//
  if (sel.type == SELECT_CONVERT) selection_clear ();
//
//  No selection currently, so start a selection.
//
  if (!sel.type) {
    sel.type  = SELECT_EDIT;
    sel.fixed = SELECT_FIX1;
    sel.pos1  = cursor;
    sel.pos2  = cursor;    
    edit_menu ();
    return;
  }
//
//  Extending a selection.
//
  find_pos (&cursor);                   // Analize cursor position.
  if (sel.fixed == SELECT_FIX1) {       // Save non-fixed point, and replace.
    last     = sel.pos2;
    sel.pos2 = cursor; 
  }
  else {
    last     = sel.pos1;
    sel.pos1 = cursor;
  }                                     // Start and end or at same position, so clear selection.
  if ((sel.pos1.line == sel.pos2.line) && (sel.pos1.pos == sel.pos2.pos) && (shift != SEL_MOUSE)) { selection_clear(); goto Redraw; }
  if ((sel.pos1.y > sel.pos2.y) || ((sel.pos1.y == sel.pos2.y) && (sel.pos1.x > sel.pos2.x))) {
    pos      = sel.pos1;                // Determine if fixpoint comes first or second.
    sel.pos1 = sel.pos2;                //   If necessary swap order of points.
    sel.pos2 = pos;
    if (sel.fixed == SELECT_FIX1) sel.fixed = SELECT_FIX2; else sel.fixed = SELECT_FIX1;
  }
//
//  Redraw changed part of selection.
//
Redraw: // Fixes Shift+VK_NEXT followed by Shift+VK_PRIOR, and vice versa.
  if (last.y > cursor.y) redraw_range (cursor.y,last.para,last.y);
    else redraw_range (last.y,cursor.para,cursor.y);
  return;
}

//--------------------------------
//
//  This routine clears the selection, and is safe the call under 
//  any circumstances.
//
void JWP_file::selection_clear () {
  if (!sel.type) return;
  sel.type = SELECT_NONE;
  edit_menu ();
  redraw_range (sel.pos1.y,sel.pos2.para,sel.pos2.y);
  return;
}

//--------------------------------
//
//  Delete selected region.
//
void JWP_file::selection_delete () {
  int i,delta;
  if (!sel.type) return;
  if ((sel.pos1.line == sel.pos2.line) && (sel.pos1.pos == sel.pos2.pos)) return;   // Null selection. This isn't supposed to happen but it can due to bugs.
  undo_para (UNDO_QUE,sel.pos1.para);
  all_abs ();
  if (sel.pos1.para == sel.pos2.para) {     // Are makeing the explicid assumtion that cursor is in the smae paragraph.
    delta = sel.pos2.pos-sel.pos1.pos;      //   and that the cursor is after or in the selected region.
    for (i = sel.pos2.pos; i < sel.pos2.para->length; i++) sel.pos1.para->text[i-delta] = sel.pos2.para->text[i];
    sel.pos1.para->length -= delta;
    cursor.pos            -= delta;
    if (cursor.pos < sel.pos1.pos) cursor.pos = sel.pos1.pos;
  }
  else {
    cursor = sel.pos1;
    sel.pos1.para->length = sel.pos1.pos;
    while (sel.pos1.para->next != sel.pos2.para) del_paragraph (sel.pos1.para->next);
    sel.pos1.para->ins_string (this,sel.pos1.para->first,sel.pos1.pos,sel.pos2.para->text+sel.pos2.pos,sel.pos2.para->length-sel.pos2.pos);
    del_paragraph (sel.pos2.para);
  }
  cursor.rel      ();
  selection_clear ();
  change          ();
  sel.pos1.para->format (this,sel.pos1.line,true);
  return;
}

//--------------------------------
//
//  Setup the scroll bars and handle changing them for the current
//  cursor location.
//
void JWP_file::set_scroll () {
  if (IS_WORKFILE(filetype)) return;
  if (jwp_config.cfg.vscroll) {
    scroll_info.nMax   = total_length;
    scroll_info.nPage  = height-JWP_FONT.vheight;
    scroll_info.nPos   = view_top.y;
    SetScrollInfo (window,SB_VERT,&scroll_info,true);
  }
  if (jwp_config.cfg.hscroll) {
    scroll_info.nMax  = char_pagewidth-1;
    scroll_info.nPage = char_width;
    scroll_info.nPos  = view_top.pos;
    SetScrollInfo (window,SB_HORZ,&scroll_info,true);
  }
  return;
}

//--------------------------------
//
//  This routine names a file as a specific system file.  Typically these names are 
//  ivalid file anmes and cannot actually be saved.  This is used for things such as
//  viewing the color kanji list.
//
//      id -- String table ID for the name of the file.  This currently only allows
//            fixed file names.
//
void JWP_file::sysname (int id) {
  changed = false;
  name = strdup(get_string(id));
  title ();
  return;
}

//--------------------------------
//
//  Set title bar for view.
//
void JWP_file::title () {
  TCHAR buffer[512];
  if (!this || (filetype & FILETYPE_WORKMASK)) return;
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  TCHAR *ptr1,*ptr2;
  for (ptr1 = ptr2 = name; *ptr1; ptr1++) {
    if (*ptr1 == '\\') ptr2 = ptr1;
  }
  wsprintf (buffer,TEXT("%c %s"),changed ? '*' : ' ',(ptr2 == name) ? ptr2 : ptr2+1);
  SetWindowText (main_window,buffer);
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  wsprintf (buffer,TEXT("%s %c %s"),VERSION_NAME,changed ? '*' : '-',name);
  SetWindowText (main_window,buffer);
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  return;
}

//--------------------------------
//
//  Processes vertical scroll bar messages.
//
//      message -- Windows message parameter.
//
void JWP_file::v_scroll (int message) {
  int i;
  switch (LOWORD(message)) {
    case SB_LINEUP:
         if (view_top.move_up ()) return;                               // If we couldn't scroll any further, skip redraw, etc.
         break;
    case SB_LINEDOWN:
         if (view_top.move_down ()) return;                             // If we couldn't scroll any further, skip redraw, etc.
         break;
    case SB_PAGEUP:
         i = view_top.y-vscroll;
         while (!view_top.move_up()) {
           find_pos (&view_top,POS_VVERT);
           if (view_top.y <= i) break;
         }         
         break;
    case SB_PAGEDOWN:
         i = view_top.y+vscroll;
         while (!view_top.move_down()) {
           find_pos (&view_top,POS_VVERT);
           if (view_top.y >= i) break;
         }
         break;
    case SB_THUMBTRACK:
  //case SB_THUMBPOSITION:                                              // Removed as being effectively redundant.
         GetScrollInfo (window,SB_VERT,&scroll_info);
         i = scroll_info.nTrackPos;
         if (view_top.y > i) {                                          // Fast positioning for the cursor motion.
           while ((i < view_top.y) && !view_top.move_up()) view_top.y -= (view_top.para->spacing*JWP_FONT.vheight)/100;
         }
         while ((i > view_top.y) && !view_top.move_down()) view_top.y += (view_top.para->spacing*JWP_FONT.vheight)/100;
         break;
    default:
         return;
  }
  find_pos   (&view_top,POS_VVERT);
  set_scroll ();
  redraw_all ();
  caret_on   ();
  return;
}

//--------------------------------
//
//  This routine checks the display to make sure that the cursor
//  is visible in the display window.  If necessary, this routine
//  will scroll the display to make the cursor visible.
//
void JWP_file::view_check () {
  int abort;
  Position pos;
  if (filetype == FILETYPE_WORK) return;    // Working file so don't pay attention to visibility
  pos.para = last;
  pos.line = last->last;
  pos.pos  = last->last->length;            // Position of last character in file (pos.eof() will now return true)
  find_pos (&pos);
  find_pos (&cursor);
  total_length = pos.y;

#ifndef ORIGINAL_BEHAVIOR
//
//  If necessary, move the view through the file until the cursor is vertically on the screen.
//  This speeds up the following operations for large files. There is still some inefficiency
//  in the next section because find_pos keeps searching from the start of the file every time,
//  and it is getting called in each of the loops used to fine-tune the view.
//
//  The comparison value here is two screens. A little less than this can cause scrolling problems
//  in the downward direction or similar issues when using Page Down. The upward direction seems
//  fine with just "height * 1" (one screen) so more investigation is needed.
//
  int cy = cursor.y;
  if (abs (view_top.y - cy) > height*2) { // More than two screens away?
    if (cy < view_top.y) {                // Fast positioning for the cursor motion. Code taken from SB_THUMBPOSITION in v_scroll.
      while ((cy < view_top.y) && !view_top.move_up()) view_top.y -= (view_top.para->spacing*JWP_FONT.vheight)/100;
    }
    while ((cy > view_top.y) && !view_top.move_down()) view_top.y += (view_top.para->spacing*JWP_FONT.vheight)/100;
    find_pos (&view_top,POS_VVERT);       // Update view. This is usually redundant, but there are some cases where it might be necessary (see the extended comment below).
    redraw_all ();                        // Entire screen will need to be redrawn.
#ifdef _DEBUG
/*
    Position old = view_top;              // This section is to validate commenting-out find_pos() a few lines back. So far, the only known case where you get different results is when selecting and then deleting several screens of text (usually four, although adjusting the scroll bar after selecting the text can affect it).
    find_pos (&view_top,POS_VVERT);
    if (memcmp(&view_top,&old,sizeof(Position))) ALERT ();
*/
#endif
  }
#endif

//
//  The next limit performes two checks.  Frist, make sure the cursor 
//  is visible on the screen, not off the top, second, make sure we
//  don't have a huge blank space at the end of the display.  These 
//  means that if you delete the end of the file, this check will 
//  cause the display to scroll back to display a full screen worth 
//  of the file.
//
  while ((cursor.y <= view_top.y+JWP_FONT.vspace) || (total_length <= view_top.y+height-JWP_FONT.vheight)) {  // Cursor is off the top
    abort = view_top.move_up ();
    find_pos (&view_top,POS_VIEW);
    if (abort) break;
    redraw_all ();
  }
  while (cursor.y-view_top.y > height) {            // Cursor is off the bottom
    if (view_top.move_down()) break;
    find_pos (&view_top,POS_VIEW);
    redraw_all ();
  }
  if (filetype == FILETYPE_EDIT) {                  // Special case for edit control, scroll to keep end of line in window
    while (view_top.pos && (pos.x <= view_top.x+width-JWP_FONT.hwidth)) {
      view_top.pos--;
      find_pos (&view_top,POS_VIEW);
      redraw_all ();
    }
  }
  while (cursor.x < view_top.x) {                   // Cursor is off the left
    view_top.pos -= hscroll;
    if (view_top.pos < 0) view_top.pos = 0;
    find_pos (&view_top,POS_VIEW);
    redraw_all ();
  }
  while (cursor.x-view_top.x > width) {             // Cursor is off the right
    view_top.pos += hscroll;
    find_pos (&view_top,POS_VIEW);
    redraw_all ();
  }
  set_scroll ();
  caret_on   ();
  return;
}

//--------------------------------
//
//  This routine determines if the file is empty (no text anywhere).
//
bool JWP_file::is_empty () {
  if (first && first->next) return (false); // Implies two or more paragraphs.
  if (first->length) return (false);        // Non-blank first paragraph.
  return (true);
}

bool JWP_file::not_empty () {
  return (!is_empty());
}

KANJI JWP_file::get_selected_ch ()
{
  int length;
  KANJI k=0;
  if (!sel.type) return (0);
  all_abs ();
  if (sel.pos1.para == sel.pos2.para) length = sel.pos2.pos; else length = sel.pos1.para->length;
  if ((length -= sel.pos1.pos) > 0) k = *(sel.pos1.para->text+sel.pos1.pos);
  all_rel ();
  return (k);
}

int JWP_file::get_selected_str (KANJI **kstr)
{
  int length;
  *kstr = 0;
  if (!sel.type) return (0);
  all_abs ();
  if (sel.pos1.para == sel.pos2.para) length = sel.pos2.pos; else length = sel.pos1.para->length;
  if ((length -= sel.pos1.pos) > 0) *kstr = sel.pos1.para->text+sel.pos1.pos; else length = 0;
  all_rel ();
  return (length);
}

//
//  End Class JWP_File.
//
//===================================================================

// ### Still not really happy with the screen dredraw on Windows CE.



