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
//  This moldue contains routines associated with the maninupation of 
//  paragraphs.  Most of the routines are the class paragraph, but some
//  of the rotuines are from the class JWP_file.
//
//  This modlue contains the paragraph class and some support routines.
//  This was all moved here from the the modlues jwp_file.cpp and 
//  jwpce.cpp to keep it collected.
//
#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_file.h"
#include "jwp_font.h"
#include "jwp_help.h"
#include "jwp_inpt.h"
#include "jwp_misc.h"
#include "jwp_para.h"

//===================================================================
//
//  Static routines.
//

//--------------------------------
//
//  Determines if the next character can be relaxed.  A relaxed 
//  character is a character that is actually displayed out of the 
//  margin range to the left.
//
//      ch        -- Character to be examined.
//      relaxable -- Pointer to a flag that indicates if characters
//                   can be relaxed.  This is always set to false 
//                   when a character is relaxed.  This is because 
//                   at most one character per line can be relaxed.
//
//      RETURN    -- Non-zero return value indicates character can be
//                   relaxed.
//
static int RelaxChar (int ch,int *relaxable) {
  int hi,lo;
  if (!*relaxable) return (false);      // Already must have relaxed a character on this line.
  hi = ch >> 8;
  lo = ch & 0xff;
  switch (hi) {
    case 0x24:  // Hiragana
    case 0x25:  // Katakana
         if (!jwp_config.cfg.relax_smallkana) break;    // Relax small characters?
         switch (lo) {  // Small characters
           case 0x21:   // a
           case 0x23:   // i
           case 0x25:   // u
           case 0x27:   // e
           case 0x29:   // o
           case 0x63:   // ya
           case 0x65:   // yu
           case 0x67:   // yo
           case 0x43:   // tsu
                *relaxable = false;
                break;
         }
         break;
    case 0x21:  // Special characters.          
         if (!jwp_config.cfg.relax_punctuation) break;  // Relax punctuation?
         switch (lo) {
           case 0x22:   // Japanese ,
           case 0x23:   // Japnaese . (maru)
           case 0x57:   // Japanese close quote
           case 0x59:   // Japanese close double quote
           case 0x4b:   // Japanese close )
                *relaxable = false;
                break;
         }
  }
  return (!*relaxable);
}

//-------------------------------------------------------------------
//
//  Begin Class Paragraph.
//
//  Class defines a paragraph (basic unit of editor) and the operations
//  that can be perfomred on it.  (Also see module jwp_clip.cpp for 
//  an additional routine.
//

//--------------------------------
//
//  Constructor
//
Paragraph::Paragraph () {
  memset (this,0,sizeof(Paragraph));
  first = last  = new Line;                 // Line for this paragraph.
  first->length = first->first = 0;         // Setup line structure
  first->next   = first->prev  = NULL;
//next = prev   = NULL;                     // Links to other paragraphs
//indent_left   = indent_right = indent_first = 0;
//page_break    = false;
  spacing       = 100;                      
//size = length = 0;                        // No data in paragraph.
//text          = NULL;
  return;
}

//--------------------------------
//
//  Destructor
//
Paragraph::~Paragraph () {
  Line *l;
  while ((l = first)) { first = l->next; delete l; }
  if (text) free (text);
  return;
}

//--------------------------------
//
//  Insert a character into the end of a pararaph.  This routine is 
//  indented primarally for the import_file routines that read JIS,
//  EUC, etc. files.
//
//      ch   -- KANJI code to insert in (can be ASCII).
//
void Paragraph::add_char (int ch) {
  if ((length+1 > size) && alloc()) return;
  text[length++] = ch;
  return;
}

//--------------------------------
//
//  Reallocate the paragraph buffer to a bigger size.
//
int Paragraph::alloc () {
  KANJI *buffer;
  if (!(buffer = (KANJI *) malloc((size+jwp_config.cfg.alloc)*sizeof(KANJI)))) { OutOfMemory (main_window); return (true); }
  if (text) {
    memcpy (buffer,text,size*sizeof(KANJI));
    free   (text);
  }
  text  = buffer;
  size += jwp_config.cfg.alloc;
  return (false);
}

//--------------------------------
//
//  Copies the format parameters from one paragraph to this one.
//
//      para -- Paragraph to copy from.
//
void Paragraph::copy_format (Paragraph *para) {
  spacing      = para->spacing;
  page_break   = para->page_break;
  indent_left  = para->indent_left;
  indent_right = para->indent_right;
  indent_first = para->indent_first;
  return;
}

//--------------------------------
//
//  Copy a paragraph to this one.  This includes formatting and data.
//
//      para   -- Paragraph to copy.
//  
//      RETURN -- A non-zero value indicates a memory allocation error.
//
int Paragraph::copy_para (Paragraph *para) {
  copy_format (para);
  return (set_text (para->text,para->length));
}

//--------------------------------
//
//  Delete a character from the paragraph.
//  
//      file -- File for reformating.
//      line -- Line to delete from.
//      pos  -- Position to delete from.
//
void Paragraph::del_char (JWP_file *file,Line *line,int pos) {
  int i;
  for (i = line->first+pos; i < length-1; i++) text[i] = text[i+1];
  length--;
  format (file,line,true); 
  file->change ();
  return;
}

//--------------------------------
//
//  Format a pargraph.
//
//      file   -- File containning paragraph.
//      line   -- Line to redraw from, sort of!  This is generally the 
//                line where the change was made.  The acutal redraw 
//                will start on the previous line (or at the top of the
//                file.  This has to due with the fact that as one types 
//                the reforatting change change the length of the line 
//                above, but never more than one line above (lots of lines 
//                below, thus we always redraw to the end of the paragraph).
//                  
//                Passing NULL or the top line of paragraph will cause 
//                the redraw to start at the top of the paragraph.
//      redraw -- If non-zero the paragraph will be redraw.  This is 
//                so certain operations can be carried out without 
//                redraws, and followed by one big redraw at the end.
//
void Paragraph::format (JWP_file *file,Line *rline,int redraw) {
  int   i,j=0,x2,x3=0;                  // Assignments are to make some compilers happy
  KANJI ch;
  Line *l;
  int   xmax      = file_font.x_offset+file_font.hwidth*(file->char_pagewidth-indent_right-indent_left-indent_first);
  int   x         = file_font.x_offset; // Horzontal position counter.
  Line *line      = first;              // Line pointer.
  int   word      = false;              // Set to true when in an ASCII word.
  int   changed   = false;              // Indicates number of lines has changed.
  int   relaxable = true;               // Allow relaxed margin?
  line->first     = 0;                  // Setup first line.
  if (rline) rline = rline->prev;       // Setup rline here, because we may delete the 
                                        //   line containning the edit point during the 
                                        //   reformat.
  for (i = 0; i < length; i++) {
    ch = text[i];
    x2 = file_font.hadvance(x,ch);
    if ((x2 > xmax) && !ISSPACE(ch) && !RelaxChar(ch,&relaxable) && (x3 != file_font.x_offset)) { // Past end of buffer, so output a line!
      line->length = j-line->first+1;           // Calculate length of line.
      if (line->next) line = line->next;        // Get next line.
        else {
          line    = new_line();
          changed = true;                       // Number of lines changed.
        }
      relaxable   = true;                       // New line so we can relax a character again.
      line->first = j+1;
      xmax        = file_font.x_offset+file_font.hwidth*(file->char_pagewidth-indent_right-indent_left);
      x2         -= x3;
      x2         += file_font.x_offset;
    }
    if (ISJIS(ch) || ISSPACE(ch)) {         // JIS or SPACE or can be split anywhere            
      word = false;                 
      j    = i;
      x3   = x2;
    }
    else if (!word) {                       // ASCII characters (words, lots of work!)
      word = true;
      j    = i-1;
      x3   = x;
    }
    x = x2;
  }
  line->length = i-line->first;             // Finish out last line.
//
//  Trim any extra lines and fix up end a paragraph.
//
  file->cursor.abs ();                      // Need to make sure we don't delete the cursor.
  while (line->next) {                      // Trim extra lines.
    if (file->view_top.line == line->next) file->view_top.line = line;  // Make sure we don't delete line with view pointer
    l          = line->next;
    line->next = l->next;
    changed    = true;
    delete l;
  }
  file->cursor.rel ();
  last = line;
//
//  Redraw either paragraph or the rest of the file, depending on if 
//  the number of lines in the file has changed.
//
  if (redraw) {
    if (changed) file->redraw_from (this,rline); else file->redraw_para (this,rline);
  }
  return;
}

//--------------------------------
//
//  Insert a string into a paragraph.
//
//      file -- File to insert into. 
//      line -- Line to insert into.
//      pos  -- Position into that line.
//      ch   -- KANJI code to insert in (can be ASCII).
//
void Paragraph::ins_string (JWP_file *file,Line *line,int pos,KANJI *string,int len) {
  int i,j;
  if (len <= 0) return;
  if (page_break) { MessageBeep (MB_ICONASTERISK); return; }    // Cannot insert into page break.
  while (len+length > size) {
    if (alloc()) return;
  }
  length += len;
  for (i = length-1; i >= line->first+pos+len; i--) text[i] = text[i-len];
  for (j = 0; j < len; j++) text[line->first+pos+j] = string[j];
  format (file,line,true); 
  file->change ();
  return;
}

//--------------------------------
//
//  Calculate pixel indent for the current line.  Based on scrolling, 
//  paragrpah parameters, etc.
//
//      line   -- Line to be considered.
//
//      RETURN -- Pixel location.  Note value may be netative if 
//                display is scrolled.
//
int Paragraph::line_start (Line *line) {
  int x;
  x = file_font.x_offset+file_font.hwidth*indent_left;
  if (first == line) x += file_font.hwidth*indent_first;
  return (x);
}

//--------------------------------
//
//  Generate a new line at the end of the paragraph (used by the 
//  formatting and reading routines.
//
Line *Paragraph::new_line () {
  Line *line;
  if (!(line = new Line)) return (NULL);
  line->length = 0;
  line->next   = NULL;
  line->prev   = last;
  last->next   = line;
  last         = line;
  return    (line);
}

//--------------------------------
//
//  Repalce a character within the current paragraph.
//
//      file -- File containning the paragraph.
//      line -- Line of where to replace the character.
//      pos  -- Position within the line where to repalce the character.
//      ch   -- Character to replace.
//
void Paragraph::rep_char (JWP_file *file,Line *line,int pos,int ch) {
  text[line->first+pos] = ch;
  format (file,line,true);    
  file->change ();
  return;
}

//--------------------------------
//
//  Set entire text buffer for a pargaph.  This is generally used 
//  when importing text or setting a text buffer.  
//
//      data   -- Data to set the text buffer to.
//      len    -- Length of text buffer.
//
//      RETURN -- Non-zero if there is a memory buffer.
//
int Paragraph::set_text (KANJI *data,int len) {
// ### may want to change the allocation to allocate all at once.
  while (size <= len) if (alloc()) return (true);
  memcpy (text,data,len*sizeof(KANJI));
  length = len;
  return (false);
}

//
//  End Class Paragraph.
//
//===================================================================

//===================================================================
//
//  Begin Class JWP_File.
//
//  Paragraph manipulation routines form JWP_file class.
//

//--------------------------------
//
//  Delete specifiecd paragraph and relink lists.
//
//      para -- Paragraph to be delted.
//
void JWP_file::del_paragraph (Paragraph *para) {
  redraw_from  (para,NULL);
  undo_delpara (para);
  while (view_top.para == para) view_top.move_up(); // Make sure we don't delete our view point. (Note: first paragraph cannot be deleted).
  if (first == para) first = para->next;
  if (last  == para) last  = para->prev;
  if (para->prev) para->prev->next = para->next;
  if (para->next) para->next->prev = para->prev;
  if (cursor.para == para) {                        // Make sure we don't delete the cursor.
    if (para->next) cursor.para = para->next; else cursor.para = para->prev;
    cursor.line = cursor.para->first;
    cursor.pos  = 0;
  }
  change ();
  return;
}

//--------------------------------
//
//  Processes paragraph format dialog box.
//
int JWP_file::do_formatpara (HWND hwnd,UINT message,int wParam) {
  switch (message) {
//
//  Intialize the dialog by setting all of the edit boxes.
//
    case WM_INITDIALOG: 
         SetDlgItemInt (hwnd,IDC_FPLEFT   ,cursor.para->indent_left ,true);
         SetDlgItemInt (hwnd,IDC_FPRIGHT  ,cursor.para->indent_right,true);
         SetDlgItemInt (hwnd,IDC_FPFIRST  ,cursor.para->indent_first,true);
         put_float     (hwnd,IDC_FPSPACING,cursor.para->spacing,100);
         if (jwp_config.global_effect) SetWindowText (hwnd,get_string(IDS_FP_FORMATFILE));
         return (true);
//
//  Help
//
    case WM_HELP:
         do_help (hwnd,IDH_EDIT_PARAGRAPH);
         return  (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) { 
           INPUT_CHECK (IDC_FPLEFT);
           INPUT_CHECK (IDC_FPRIGHT);
           INPUT_CHECK (IDC_FPFIRST);
           INPUT_CHECK (IDC_FPSPACING);
//
//  OK, means reformat the pargarphs.
//
           case IDOK:
                int        left,right,frst,spacing;
                Paragraph *start,*end,*para;
//
//  Get the formatting parameters.
//
                left    = get_int(hwnd,IDC_FPLEFT ,0   ,255,cursor.para->indent_left );
                right   = get_int(hwnd,IDC_FPRIGHT,0   ,255,cursor.para->indent_right);
                frst    = get_int(hwnd,IDC_FPFIRST,-127,127,cursor.para->indent_first);
                spacing = get_float (hwnd,IDC_FPSPACING,1,10,cursor.para->spacing,100,NULL);
                if ((frst < -left) || (left+right >= jwp_file->char_pagewidth) || (left+right+frst >= jwp_file->char_pagewidth)) {
                  JMessageBox (hwnd,IDS_FP_INVALIDTEXT,IDS_FP_INVALIDTITLE,MB_OK | MB_ICONERROR);
                  return     (true);
                }
//
//  Set first and last paragraph to be formatted.
//
                if (jwp_config.global_effect) { start = first; end = last; }
                else if (sel.type) { start = sel.pos1.para; end = sel.pos2.para; }
                else { start = end = cursor.para; }
//
//  Foramt paragraphs.
//
                undo_start ();
                for (para = start; true; para = para->next) {
                  undo_para (UNDO_QUE,para);
                  para->indent_left  = left;
                  para->indent_right = right;
                  para->indent_first = frst;
                  para->spacing      = spacing;
                  para->format (this,NULL,true);
                  if (para == end) break;
                } 
                undo_end ();
//
//  Correct display and exit.
//
                change    ();
                adjust    ();
                EndDialog (hwnd,true);
                return    (true);
//
//  Cancel means exit without doing anything.
//
           case IDCANCEL:
                EndDialog (hwnd,false);
                return    (true);
         }
  }
  return (false);
}

//--------------------------------
//
//  Generate a new paragraph following the indicated paragraph.
//
//      para   -- New paragraph will be placed in file list after this one.
//                An argument of NULL is used to generate the first
//                paragraph in a file.
//
//      RETURN -- Non-zero return indicates failure to allocate.
//
int JWP_file::new_paragraph (Paragraph *para) {
  Paragraph *p;
  if (!(p = new Paragraph)) return (true);
  if (para) {
    p->prev    = para;
    p->next    = para->next;
    if (para->next) para->next->prev = p;
    para->next = p;  
    if (para == last) last = p;
    p->copy_format (para);
    p->page_break = false;
  }
  else {
    first = last = p;
  }
  undo_newpara (p);
  change       ();
  redraw_from  (p,NULL);
  return       (false);
}

//--------------------------------
//
//  Turns a paragraph into a hard page break.
//
//      para -- Paragraph to turn
//      page -- A true value cuases this to become a page break, and a 
//              false value causes it to become a normal paragraph.
//
void JWP_file::set_page (Paragraph *para,int page) {
  undo_para (UNDO_QUE,para);
  para->page_break = page;
  para->length     = 0;
  redraw_para (para,NULL);
  return;
}

//
//  End Class JWP_File.
//
//===================================================================




