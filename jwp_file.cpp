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
  x2 = x = para->line_start(line); 
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
//
//  Recalculate the IME position
//
#ifndef WINCE
    ime_y = GetSystemMetrics(SM_CYSIZE)+GetSystemMetrics(SM_CYMENU);
#endif WINCE
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
  int i,j;
  switch (key) {
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
             selection (shift);
             if (!cursor.bof()) do_key (VK_LEFT,false,shift);
             while (true) {
               i = char_class(cursor.get_char());
               if (cursor.bof() || ((i != CLASS_JUNK) && (i != CLASS_SPACE))) break;
               left ();
             }
             while ((i == char_class(cursor.get_char())) && !cursor.bof()) left ();
             if (!cursor.bof()) do_key (VK_RIGHT,false,shift);
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
             i = char_class(cursor.get_char());           
             if (i == CLASS_SPACE) i = CLASS_JUNK;
             while (true) {
               if (cursor.eof()) break;
               j = char_class(cursor.get_char());
               if (j == CLASS_SPACE) i = CLASS_JUNK;
               if ((j != i) && (j != CLASS_SPACE)) break;
               right ();
             }
           }
         break;
//
//  BACKSPACE -- <in ascii->kana convert> Abort conversion.
//               <in kana->kanji start>   Delete character from conversion.
//               <in selection process>   Delete selection.
//               <plain>                  Delete previous character.
//
    case VK_BACK:
         clear_cursor ();
         undo_clear   (UNDO_DEL);
         if (kana_convert.erase()) { undo_clear(); return; }
         if (sel.type == SELECT_CONVERT) { selection_clear (); jwp_conv.clear (); }
         if (sel.type == SELECT_KANJI) {
           cursor.para->del_char (this,cursor.line,--cursor.pos);
           all_abs ();
           sel.pos2.pos--;
           if (sel.pos1.pos == sel.pos2.pos) selection_clear ();    
           all_rel ();
           find_pos (&sel.pos2);
           break;
         }
         if (sel.type) {
           undo_start       ();
           selection_delete ();
           undo_end         ();
           break;
         }
         if (cursor.bof()) return;
         left ();
//
//  DELETE -- <in kana->kanji start>   Delete character from conversion.
//            <in selection process>   Delete selection.
//            <plain>                  Delete character.
//            <shift>                  Cut to clipboard.
//
    case VK_DELETE:     // *** FALL THROUGH *** //
         clear_cursor ();
         undo_clear   (UNDO_DEL);
         if (kana_convert.erase()) { undo_clear(); return; }
         if (shift) {
           shift = false;       // Prevent problems later down the line.
           clip_cut ();
           break;
         }
         if (sel.type == SELECT_CONVERT) selection_clear ();
         if (sel.type) {
           undo_start       ();
           selection_delete ();
           undo_end         ();
           break;
         }
         if (cursor.eof()) return;
         if (cursor.eop()) {
           undo_start ();
           if (!cursor.para->length && cursor.para->next->page_break) {
             cursor.para = cursor.para->next;       // Special case when next paragraph is a page break.
             cursor.line = cursor.para->first;
             cursor.pos  = 0;
             del_paragraph (cursor.para->prev);
             undo_end      ();
             break;
           }
           cursor.para->page_break = false;
           cursor.para->ins_string (this,cursor.para->last,cursor.para->last->length,cursor.para->next->text,cursor.para->next->length);
           undo_para     (UNDO_QUE);
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
         shift = false;
         break;
//
//  INSERT -- <shift> -- Paste from clipboard.
//            <ctrl>  -- Copy to clipboard.
//            <plain> -- Toggle insert mode.
//
    case VK_INSERT:
         if (shift) {
           clip_paste (true);
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
//  A -- <shift & ctrl> -- Select entire paragraph.  Used by the edit control routines.
//       <ctrl>         -- Ascii mode.
//
    case VK_A:              
         if (ctrl && shift) {
           jwp_conv.clear ();
           if (first->text) {
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
         break;
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
           undo_clear ();
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
           undo_clear ();
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
         undo_clear ();
         if (shift) bushu_lookup (this); else radical_lookup (this);
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
         if (ctrl) clip_paste (true);
         break;
//
//  W -- <ctrl> -- Select word.
//
    case VK_W:              // Select word
         if (!ctrl) return;
         clear_cursor   ();
         jwp_conv.clear ();
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
           if (i == CLASS_SPACE) i = CLASS_JUNK;
           if (i != char_class(cursor.get_char())) break;
           do_key (VK_RIGHT,false,true);
         }
         break;
//
//  X -- <ctrl> -- Cut to clipboard.
//
    case VK_X:
         if (ctrl) clip_cut ();
         break;
//
//  Y -- <ctrl> -- Redo.
//
    case VK_Y:
         if (ctrl) do_redo ();
         break;
//
//  Z -- <ctrl> -- Undo.
//
    case VK_Z:
         if (ctrl) do_undo ();
         break;
//
//  Space and > Conversion to the left with control
//
    case VK_GT:
         if (!ctrl) return;
//
//  F2 -- Forward kana->kanji convert.
//
    case VK_F2:
         convert (CONVERT_RIGHT);
         break;
//
//  < Convertion to the left
//
    case VK_LT:
         if (!ctrl) return;
//
//  F3 -- Forward kana->kanji convert.
//
    case VK_F3:
         convert (CONVERT_LEFT);
         break;
//
//  F4 -- Toggle input mode (kanji--ascii)
//
    case '6':
         if (!ctrl) break;
    case VK_F4:
         set_mode (MODE_TOGGLE);
         break;
//
//  Application key, and shift-F10 -- Cause the same as a right mouse click at the cursor location.
//
    case VK_APPS:
         popup_menu (cursor.x-view_top.x,cursor.y-view_top.y);
         return;
//
//  F23 -- This is really a message from Windows CE on the palm commputer
//         This is a select button which popus up the popup menu.
//
    case VK_F23:
         popup_menu (cursor.x-view_top.x,cursor.y-view_top.y);
         return;
//
//  RETURN -- <ctrl>  -- Insert page break.
//            <shift> -- Insert soft return.
//            <plain> -- Insert paraagraph.
//
    case VK_RETURN:
         if (sel.type == SELECT_KANJI) convert (CONVERT_RIGHT);
// IMPROVE -- Would be nice if this did not lose the conversion when pressing enter.  That is, if we could delay the conversion for a bit.         
         clear_cursor    ();
         jwp_conv.clear  ();
         undo_start      ();
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
         cursor.para->format (this,cursor.line,true);   // Correct next paragraph.
         do_key (VK_RIGHT,false,false);         
         cursor.para->format (this,NULL,true);
         undo_end ();
         break;
//
//  UP -- <ctrl>  -- kana->kanji convertion forward (right).
//        <plain> -- Up one line.
//        <shift> -- Extend selection.
//        SPECIAL -- On PPC/PocketPC up during a kanji conversion is taken as convert.
//
    case VK_UP:
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         if ((sel.type == SELECT_KANJI) || (sel.type == SELECT_CONVERT)) {
           convert (CONVERT_RIGHT);
           break;
         }
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         if (ctrl) {
           convert (CONVERT_RIGHT);
           break;
         }
         if (jwp_config.cfg.page_mode_file) goto PageUp;
         jwp_conv.clear ();
         undo_clear     ();
         selection      (shift);
         if (!x_cursor) x_cursor = (short) cursor.x;
         if (cursor.move_up()) return;
         cursor.align (this,x_cursor);
         break;
//
//  DOWN -- <ctrl>  -- kana->kanji convertion backward (left).
//          <plain> -- Down one line.
//          <shift> -- Extend selection.
//          SPECIAL -- On PPC/PocketPC down during a kanji conversion is taken as convert.
//
    case VK_DOWN:
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         if ((sel.type == SELECT_KANJI) || (sel.type == SELECT_CONVERT)) {
           convert (CONVERT_RIGHT);
           break;
         }
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         if (ctrl) {
           convert (CONVERT_LEFT);
           break;
         }
         if (jwp_config.cfg.page_mode_file) goto PageDown;
         jwp_conv.clear ();
         undo_clear     ();
         selection      (shift);
         if (!x_cursor) x_cursor = (short) cursor.x;
         if (cursor.move_down()) return;
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
         jwp_conv.clear ();
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
         if (wParam & MK_SHIFT) { iMsg = WMU_KANJIINFO; break; }        // shift+left -> right button click.
         mouse_x = LOWORD(lParam); 
         mouse_y = HIWORD(lParam); 
         SetTimer        (window,TIMER_MOUSEHOLD,GetDoubleClickTime(),NULL);
         SetCapture      (window);
         if (wParam & MK_CONTROL)      iMsg = WM_LBUTTONDBLCLK;     // clrt+left -> left button double click.
         if (GetKeyState(VK_MENU) < 0) iMsg = WM_RBUTTONDOWN;       // alt+left -> kanji information.
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
//	Alt-button, gives rise to get character info.
//
    case WMU_KANJIINFO:
         do_key (VK_I,true,false);
		 iMsg = WM_RBUTTONDOWN;			// Trick the motion routines into not generating a selection.
		 break;		 
//
//  Right button click.
//
    case WM_RBUTTONDOWN:                // Right button.  Bring up popup menu and let it send messages back to our window.
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
    selection  ((iMsg != WM_RBUTTONDOWN) ? SEL_MOUSE : false);  // Block generation of select if right button
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
  x = para->line_start(line)-view_top.x; 
  for (i = 0; i < line->length; i++) {              // Determine start character
    j = font->hadvance(x,para->text[line->first+i]);
    if (j >= xmin) break;
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
    x = font->hadvance (x,ch);                      //   Advance position
    if (x > xmax) break;
  }
//
//  If text is slected render inversion.
//
  if (!sel.type) return;
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
    loc->x = loc->para->line_start(loc->line);
    for (i = 0; (i < loc->pos) && (i < loc->line->length); i++) loc->x = JWP_FONT.hadvance(loc->x,para->text[line->first+i]);
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
    case SB_THUMBPOSITION:
         GetScrollInfo (window,SB_HORZ,&scroll_info);
         scroll_info.nPos = HIWORD(message);    // ### This is a kludge because GetScrollInfo does not return the correct data.
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
  if ((sel.pos1.line == sel.pos2.line) && (sel.pos1.pos == sel.pos2.pos) && (shift != SEL_MOUSE)) { selection_clear(); return; }
  if ((sel.pos1.y > sel.pos2.y) || ((sel.pos1.y == sel.pos2.y) && (sel.pos1.x > sel.pos2.x))) {
    pos      = sel.pos1;                // Determine if fixpoint comes first or second.
    sel.pos1 = sel.pos2;                //   If necessary swap order of points.
    sel.pos2 = pos;
    if (sel.fixed == SELECT_FIX1) sel.fixed = SELECT_FIX2; else sel.fixed = SELECT_FIX1;
  }
//
//  Redraw changed part of selection.
//
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
         view_top.move_up ();
         break;
    case SB_LINEDOWN:
         view_top.move_down ();
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
    case SB_THUMBPOSITION:
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
  pos.pos  = last->last->length;
  find_pos (&pos);
  find_pos (&cursor);
  total_length = pos.y;
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

//
//  End Class JWP_File.
//
//===================================================================

// ### Still not really happy with the screen dredraw on Windows CE.



