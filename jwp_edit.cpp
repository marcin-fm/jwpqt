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
//  This module implements japanese versions of the windows controls.
//  In particular, this module implements a japanese edit control, and
//  a list handlier for doing list boxes in japanese.  A number of 
//  service and utility routines for processing data into and out of 
//  these items.
//
//  class EUC_buffer:
//
//  This class chops data into display elements that can fit into 
//  a list box.  This is used by the dictionary routine, and by the 
//  kanji-information routiens.
//
//  Color KLUDGE:
//
//  I wanted to allow the program to change the color of labels in the 
//  character-info box.  This did not work, in a simple way because 
//  windows keeps our strings.  To get arround this, we set the first
//  character of a line to the value 0x01, this indicates a color 
//  code.  This is a bit of a KLUDGE, but it works.
//
//  EDIT_list:
//
//  This class provides list managment for an ediable list.  This class
//  provides the basic operations that are needed for maintainning these
//  types of lists.  This includes calling specifized routines that 
//  perform various operations in the system.  This class is not used
//  directly, but rather you dirive a class from it that provides 
//  services necessary for a specific list.
//
//  JWP_list:
//
//  The JWP_lsit class manages a Japanese list-box control.  Much as 
//  with the edit class, this class is associated with each lsit box
//  that is generated.  This class services all list box requiest.
//  This class provides only a simplified message interface to the 
//  list box.  If, however, more access and features are required, 
//  the JL_GETJWPLIST message can be sent to get a pointer to the 
//  underline class.  Then the class routines can be called directly,
//  which much more power.
//
//  The base class provides an odd mix of features.  There are basic 
//  simple class manipulation routines, but there are also blcok 
//  manipulation rotuines.  The block routines are designed to provide 
//  service for the EDIT_list class.  This class operates in detail
//  with the JWP_list class, but cannot be dirvied because of how the 
//  objects are generated.  Thus the division of labor between the 
//  two classes is complex, which each class attempting to provide the 
//  most efficient features.
//
//  Memory is allocated in a JWP_list object in a series of blocks.  
//  each block contains a fixed number of lines, and a pointer to the 
//  next block.  This means that to get to a specific line requires
//  that you move through all of the blocks starting at the first, until
//  you have the block that contains the line you are interested in.
//  this system is not the most efficient in terms of access, but 
//  allows the allocation of a arbitary size or list, but generally 
//  does not require more than a single block for most common lists, so
//  is very efficent in these case.  In the case of a lone list, it 
//  prevents the requirement of having to constantly reallocate the 
//  base set of pointers over and over, which is hard on memory.
//
#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_conv.h"
#include "jwp_edit.h"
#include "jwp_file.h"
#include "jwp_find.h"
#include "jwp_flio.h"   // Needed to get definition of choose_file.
#include "jwp_font.h"
#include "jwp_help.h"   // Needed for help ID for choose_file call
#include "jwp_info.h"
#include "jwp_inpt.h"
#include "jwp_misc.h"

#include <limits.h>

  static JWP_file *last_insert = NULL;              // The last file that the user inserted into, for popup menu.
//===================================================================
//
//  Compile-time options
//

//#define FOLLOW_BY_SPACE               // If defined, causes multi-line selections, when 
                                        //   pasted back into a file, to use a kanji-space
                                        //   character (0x2121) to separate enrities.  The 
                                        //   default is to use tabs.


//===================================================================
//
//  Static data
//
static SIZE_window hist_size;           // Class used for size changes in the history window.

//===================================================================
//
//  Static routines.
//
static LRESULT CALLBACK JWP_edit_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);

//--------------------------------
//
//  This is the window proc for the japanes edit box.  This is the big
//  deal.  
//
//  In the implementation of the edit-box routine, I let the strings, 
//  be stored by windows.  The strings are converted to EUC format so
//  windows can handle them.  I only interpret them for display perposes.
//  
//  This routine stores a pointer to the JWP_file object associated 
//  with the edit box in the window structure at offset 0.  Basically 
//  we treat the edit control as a file containning only one paragraph, 
//  with a very long line buffer.
//
//  Special messages:
//
//      JE_SETTEXT    -- Sets the text buffer of the edit box.  
//                       wParam -- Length,  lParam -- Pointer to kanji string.
//      JE_GETJWPFILE -- Returns pointer JWP_file class object.
//      JE_GETLINE    -- Returns pointer to text buffer (i.e. text in 
//                       the first paragraph).
//      JE_GETLENGTH  -- Returns the length of the text buffer.
//      JE_LOAD       -- Unusuall interpritation of this, this causes a
//                       the edit-box to initialize by coping the selected
//                       region from another JWP_file object.
//                       lParam -- Pointer to JWP_file object 
//
static LRESULT CALLBACK JWP_edit_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC           hdc;
  PAINTSTRUCT   ps;
  CREATESTRUCT *create;
  JWP_file     *file;
  static int    block = false;
  file = (JWP_file *) GetWindowLong(hwnd,0);        // Get our JWP_fiel class object.
  switch (iMsg) {
//
//  Creation, we need to create the JWP_file object, and adjust the size
//  of the window to refelect the actual size of the kanji font.
//
    case WM_CREATE:                                 
         create = (CREATESTRUCT *) lParam;
         MoveWindow (hwnd,create->x,create->y,create->cx,edit_font.height+2*edit_font.vspace+2*WIN_YEDGE,true);
         file = new JWP_file (hwnd);
         SetWindowLong (hwnd,0,(long) file);    // Save JWP_file object for edit box.
         return (0);
    case WM_DESTROY:                            // Destroy the JWP_file object, and remove properties.
         file_list.remove (file);               // Appears we don't loose the focus before being killed.
         delete file;
         return (0);
    case WM_SETFOCUS:                           // Set the focus.
         SIP_ON         ();
         file->do_key   (VK_A,true,true);
         file->caret_on ();
         file_list.add  (file);
         return (0);
    case WM_KILLFOCUS:                          // Kill the focus.
         SIP_OFF         ();
         jwp_conv.clear  ();
         file->caret_off ();
         return (0);
    case WM_PAINT:                              // Render.
         RECT rect;
         hdc = BeginPaint (hwnd,&ps);
         GetClientRect    (hwnd,&rect);
         FillRect         (hdc,&rect,GetSysColorBrush(COLOR_WINDOW));
         SetBkColor       (hdc,GetSysColor(COLOR_WINDOW));
         SetTextColor     (hdc,GetSysColor(COLOR_WINDOWTEXT));
         file->draw_all   (hdc,&ps.rcPaint);
         EndPaint         (hwnd,&ps);
         return (0);
    case WM_TIMER:
         if (wParam != TIMER_MOUSEHOLD) return (0);
    case WM_LBUTTONUP:                          // Mouse button & move 
    case WM_LBUTTONDOWN: 
    case WM_MOUSEMOVE:          //*** FALL THOUGHT! ***
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
         file->do_mouse (iMsg,wParam,lParam);
         return (0);
    case WM_CHAR:                               // Character input.
         if (wParam == '\t') return (0);
         file->do_char (wParam);
         return (0);
#ifndef WINCE
    case WM_IME_CHAR:                           // IME support.
         file->ime_char (wParam,IsWindowUnicode(hwnd));
         return (0);
#endif WINCE
    case WM_GETDLGCODE:                         // We need to get input from windows.
         return (DLGC_WANTARROWS | DLGC_WANTALLKEYS | DLGC_WANTCHARS);


#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
    case WM_KEYUP:
         if (wParam == VK_F23) { block = false; jwp_file->do_key (wParam,false,false); }
         return (0);
    case WM_KEYDOWN:                            // Vitural keys.
int dont_really_like_the_blockout_but_it_works;
         if (wParam == VK_F23) { block = true; return (0); }
         if (block) return (0);
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
    case WM_KEYDOWN:                            // Vitural keys.
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         int shift,ctrl;
         shift = (GetKeyState(VK_SHIFT)   < 0);
         ctrl  = (GetKeyState(VK_CONTROL) < 0);
         switch (wParam) {
           case VK_TAB:                         // Tab has speciall meanning.
                if (ctrl) break;                // Ctrl+TAB is always a tab (lets us put tabs into Japanese edit boxes).
                SetFocus (GetNextDlgTabItem(GetParent(hwnd),hwnd,shift));   // Move to next last item.
                return (0);
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           case VK_UP:                          // For PPC/PocketPC, the up down are used for convert not the history.  This is a more useful formulation.
                file->do_key (VK_F2,false,false);
                return (0);
           case VK_DOWN:
                file->do_key (VK_F3,false,false);
                return (0);
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           case VK_UP:                          // Remove this inputs.
                if (ctrl) break;
                if (file->history) file->history->up (file);
                return (0);
           case VK_DOWN:
                if (ctrl) break;
                if (file->history) file->history->down (file);
                return (0);
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           case VK_PRIOR:
           case VK_NEXT:
                return (0);
           case VK_RETURN:                      // Return has special meaning (invoke dialog event)
                if (file->sel.type == SELECT_KANJI) file->convert (CONVERT_RIGHT);
                jwp_conv.clear ();
                SendMessage (GetParent(hwnd),WM_COMMAND,IDSEARCH,0L);       // Used to be IDOK, but was changed to work with PocketPC, which requires IDOK to close some dialogs.
                return (0);
           case VK_ESCAPE:                      // Escape has special meaning (abort dialog)
                jwp_conv.clear ();
                SendMessage (GetParent(hwnd),WM_COMMAND,IDCANCEL,0L);
                return (0);
           case VK_F23:
                file->do_mouse (WM_RBUTTONDOWN,0,0xffffffff);
                return (0);
         }
         file->do_key (wParam,ctrl,shift);      // Default key processor.
         return (0);
    case JE_SETTEXT:                            // This message sets the buffer contents.
         file->edit_set ((KANJI *) lParam,wParam);
         return (0);
    case JE_GETJWPFILE:                         // This entry point returns the JWP_file object.
         return ((long) file);
    case JE_GETTEXT:                            // Returns the text buffer and file length.
         KANJI *kanji;
         int    length;
         kanji  = file->edit_gettext();
         length = file->edit_getlen();
         if (file->history) file->history->add (kanji,length);
         *((KANJI **) lParam) = kanji;
         return (length);
    case JE_SETHIST:                            // Set a history pointer for this edit control.  Also alows use to set attached history button.
         file->history = (JWP_history *) lParam;
         if (wParam) {                                  
           HWND button;                                 // If we have a list button, we will move that button to the edge of the edit control,
           RECT butrect,jecrect,dlgrect;                //   and set the height to match that of the edit control.
           button = GetDlgItem(GetParent(hwnd),wParam);
           GetWindowRect (button,&butrect);
           GetWindowRect (hwnd  ,&jecrect);
           GetWindowRect (GetParent(hwnd),&dlgrect);
#ifdef WINCE_POCKETPC
           MoveWindow    (button,jecrect.right-dlgrect.left-WIN_XEDGE-1,butrect.top-dlgrect.top-GetSystemMetrics(SM_CYCAPTION)-WIN_YEDGE,butrect.right-butrect.left+1,jecrect.bottom-jecrect.top,true);
#else  WINCE_POCKETPC
           MoveWindow    (button,jecrect.right-dlgrect.left-WIN_XEDGE-1,butrect.top-dlgrect.top-GetSystemMetrics(SM_CYCAPTION)-WIN_YEDGE-1,butrect.right-butrect.left+1,jecrect.bottom-jecrect.top,true);
#endif WINCE_POCKETPC
         }
         return (0);
    case JE_HISTORYLIST:                        // User hit the history button.
         if (file->history) file->history->list (file);
         SetFocus (hwnd);
         return   (0);
//
//  These are the menu commands that can be sent from the popup command.
//
    case WM_COMMAND:
         file->do_menu (wParam);
         return (0);
//
//  This is an unusual interpretation of this message, I use this to 
//  request that the edit box initialize it's contents from the selected
//  region of a specifc file.
//
    case JE_LOAD:                              
         return (file->edit_copy ((JWP_file *) lParam));
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//--------------------------------
//
//  This is the window proc for the special Japanese edit controls
//  used in Property Pages.  
//
//  This routine just has pecial handers for the ESC and ENTER key.
//  All other processing is passed to the standard routine, JWP_edit_proc.
//  
//
static LRESULT CALLBACK JWP_page_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  if (iMsg == WM_KEYDOWN) {
    if ((wParam == VK_RETURN) || (wParam == VK_ESCAPE)) hwnd = GetParent(hwnd);
  }
  return (JWP_edit_proc(hwnd,iMsg,wParam,lParam));
}

//===================================================================
//
//  begin class EUC_buffer
//
//  This class implements a simple buffer used to break dictionary 
//  strings into lines, for the display.  
//

//--------------------------------
//
//  Clear the buffer.  This sets the x poisiton and character count to zero.
//
void EUC_buffer::clear () {
  x     = 0;
  count = 0;
  if (hilight) put_char (EUC_HIGHLIGHT);    // Highlight is generated on the clear command (called 
                                            //   by flush.  This means you need to set the highlight
                                            //   before calling clear.
  return;
}

//--------------------------------
//
//  Writes the contents of the buffer to the list box control, up to the 
//  specified character.  The remainder of the characters are retained in 
//  the buffer for the next line.
//
//      pos -- Position to flush buffer to.  If this value is -1 the 
//             entire contents ofthe buffer will be flushed.
//
void EUC_buffer::flush (int pos) {
  int  i,j;
  if (pos == -1) pos = count;               // Flush entire buffer.
  i = count;                                // Save lenth of the buffer;
  list->add_line (pos,buffer);              // Write to list-box
  clear    ();                              // Clear buffer.
  put_char ('\t');                          // Indent new line.
  for (j = pos; j < i; j++) put_char (buffer[j]);
  return;
}

//--------------------------------
//
//  Intialize the buffer system.  This mostly involves caching the list-box
//  window pointer, and determinining the widht of the list box.
//
//      hwnd -- List box window handle.
//
void EUC_buffer::initialize (HWND hwnd) {
  list = (JWP_list *) SendMessage(hwnd,JL_GETJWPLIST,0,0);
  highlight (false);
  return;
}

//--------------------------------
//
//  This is the main routine.  This places a character in the buffer.  
//  If necessary the contents of the buffer are flushed to make room 
//  for the new character.
//
//      ch -- Character to place in the buffer.
//
void EUC_buffer::put_char (int ch) {
  int i;
//
//  If this is a visable (non-space) character and will put us past 
//  the max length marker, then we need to break the line.
//
//  If it is simply a JIS character we can break the line just before 
//  this.  If it is an ascii character, we have to backup until we get 
//  to a space.
//
  if (ch != EUC_HIGHLIGHT) x = list_font.hadvance(x,ch); // KLUDGE: See top of file, if the character code is 0x01 this is a color shift.
  if (!ISSPACE(ch) && (x >= list->xmax)) {
    if (ISJIS(ch)) flush (count);
      else {
        for (i = count-1; (i >= 0) && !ISJIS(buffer[i]) && !isspace(buffer[i]); i--);
        i++;
        if (i) flush (i);           // This handles the case when the user has selected a very
      }                             //   large font and a single word of the text will not fit 
  }                                 //   on a line.  This allows the word to extend into the right 
//                                  //   margin.  This is not perfect, but probobly the best.
//  Put the character, converting to EUC code.
//
  if (ISJIS(ch)) ch &= 0x7f7f;
  buffer[count++] = ch;
  return;
}

//--------------------------------
//
//  Put an entire kanji string into an EUC_buffer.
//
//      kanji  -- Kanji string.
//      length -- Number of characters in the string.
//
void EUC_buffer::put_kanji (KANJI *kanji,int length) {
  int i;
  for (i = 0; i < length; i++) put_char (kanji[i]);
  return;
}

//--------------------------------
//
//  Generates a label in a list.  The format of the label is:
//      
//      -<text>-
//
//  and is highlighted.
//
//      id -- String table ID
//
void EUC_buffer::put_label (int id) {
  highlight  (true);
  clear      ();
  put_char   (KANJI_DASH);
  put_string (get_string(id));
  put_char   (KANJI_DASH);
  highlight  (false);
  flush      (-1);
  return;
}

//--------------------------------
//
//  Puts an entire string into an EUC_buffer.  The string is restricted 
//  to a processing an ascii string.
//
//      string -- String to put into the buffer.
//
void EUC_buffer::put_string (tchar *string) {
  while (*string) put_char (*string++);
  return;
}

//
//  End Class EUC_buffer
//
//===================================================================

//===================================================================
//
//  Begin Class EDIT_list.
//
//  This class is designed as a base class for dirived classes.  It 
//  cannot be used directly as a stand-alone classe (because of some
//  pure virtural functions).  This class handles the processing 
//  associated with an editalbe list.  
//
//  An editable list is a listbox control and a number of support 
//  controls to allow the items in the listbox to be edited, moved,
//  deleted, etc.  
//
//  Generally, these items are generated within a dialog box and 
//  have to have very specific IDs (see header file).  The following 
//  controls are supported:
//
//      ID_EDITLIST       -- The actual list box.  Allows only a single
//                           selected item.
//      ID_EDITLISTADD    -- ADD button.  Adds a new item following 
//                           the currently selected item.
//      ID_EDITLISTEDIT   -- EDIT button.  Edits the currently 
//                           selected item.
//      ID_EDITLISTDELETE -- DELETE button.  Deletes the currently 
//                           selected item.
//      ID_EDITLISTUP     -- UP button. Moves the currently selected 
//                           item up one in the list.
//      ID_EDITLISTDOWN   -- DOWN button.  Moves the currently selected
//                           item down in the list one item.
//
//  The editlist controls allow logical items in the list to extend
//  over any entries in the list box (first line starts directly, 
//  all following lines have a TAB as the first character).  The allows
//  editing dicitionary entries and other things.  The currently 
//  selected item does not reffer to the selected line, but the entire
//  item containning the the slelected line.
//

//--------------------------------
//
//  Routine to implement dragging onto the list.  This basically 
//  will import a file into the list, by dragging the file.
//
//  If you want this to work, you must set dialog to have the 
//  extended style AcceptFiles.
//
void EDIT_list::do_drop (HDROP drop) {
#ifndef WINCE                           // Windos Ce does not support file drag and drop!
  int  i;           
  char buffer[SIZE_BUFFER];
  for (i = 0; DragQueryFile(drop,i,buffer,SIZE_BUFFER) > 0; i++) {
    import_file (buffer);
  }
  DragFinish (drop);
#endif WINCE
  return;
}

//--------------------------------
//
//  This is the event handler called from the dialog box's window 
//  procedure.  This will process events having to do with mantainning
//  the list.
//
//      id -- Id of the button activated.
//
#define SIZE_OPENBUFFER     2000                    // Size of buffer for holding names
#define STRING_IMPORTERROR  "File Import Error!"    // Text stirng for error messages.

void EDIT_list::do_event (int id) {
  int i,j;
  j = list->current;
  switch (LOWORD(id)) {
//
//  Big one's EDIT and ADD.
//
    case IDC_EDITLIST:                                      // User clicked the list.
         if (HIWORD(id)) break;                             // If double click process as an edit message.
         id = IDC_EDITLISTEDIT; 
    case IDC_EDITLISTEDIT:      // **** FALL THROUGH ****
    case IDC_EDITLISTADD:
         if (LOWORD(id)==IDC_EDITLISTEDIT) {                // If edit get data.
           j = begin_item(j);
           get_buffer (j);
         }
         else {                                                       
           length = 0;                                      // Else zero buffer and get index for new line.
           j      = next_item(j);
         }
         if (edit()) {                                      // User liked the edit so put the item.
           changed = true;
           if (id == IDC_EDITLISTEDIT) j = delete_item(j);  // if Edit delete old 
           if (j < 0) j = 0;
           i = count();
//
//  Routine use to format a line that has been edited by the user.
//  This routine will reformat the line and put it back in the list.
//  This routine does one of two things.  If the the line_break 
//  parameter is set to zero (default for most lists), this routine
//  will simply put the line in the buffer.  If the line-break 
//  parameter is set, that parameter determines when a line-break
//  should be placed in the line.  This is used to duplicate the 
//  dictionary formatting.
//
           clear     ();                                    // Put corrected data back.
           if (line_break) {
             put_kanji (kbuffer,line_break);
             flush     (-1);
           }
           put_kanji  (kbuffer+line_break,length-line_break);
           flush      (-1);
           move_item  (i,j);                                // Move the item.
         }
         break;
    case IDC_EDITLISTDELETE:                // DELETE.
         changed = true;
         move (delete_item(j));
         break;
    case IDC_EDITLISTUP:                    // UP
         changed = true;
         j = begin_item(j);
         move_item (j,begin_item(j-1));
         break;                             // DOWN
    case IDC_EDITLISTDOWN:
         changed = true;
         move_item (j,next_item(next_item(j)));
         break;
    case IDC_EDITLISTNEXT:                  // NEXT
         jwp_search.do_next (list);
         break;
    case IDC_EDITLISTFIND:                  // FIND
         jwp_search.do_search (list);
         break;    
    case IDC_EDITLISTINSERT:                // INSERT into file.
         list->insert (false);
         return;                            // No reason to go on.   
//
//  Import a file event.
//
    case IDC_EDITLISTIMPORT: {              // IMPORT 
           OPENFILENAME  ofn;
           TCHAR *ptr,name[SIZE_BUFFER],buffer[SIZE_OPENBUFFER];
//
//  Setup file requestor.
//
           memset     (&ofn,0,sizeof(ofn));               
           memset     (name,0,sizeof(name));
           GET_STRING (name,IDS_LIST_IMPORT);
           lstrcat    (name,import);
           for (ptr = name; *ptr; ptr++) if (*ptr == '\t') *ptr = 0;
           ofn.lStructSize       = sizeof(ofn);
           ofn.hwndOwner         = dialog;
           ofn.hInstance         = instance;
           ofn.lpstrFilter       = name;
           ofn.nFilterIndex      = 2;
           ofn.lpstrFile         = buffer;
           ofn.nMaxFile          = SIZE_OPENBUFFER;
#ifdef WINCE
           ofn.Flags             = OFN_FILEMUSTEXIST  | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY  | OFN_EXPLORER;
           ofn.lpstrInitialDir   = currentdir;  // Use Windows CE current directory
#else WINCE
           ofn.Flags             = OFN_FILEMUSTEXIST  | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY  | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
#endif WINCE
           buffer[0] = 0;
           if (!GetOpenFileName(&ofn)) return;  // User canclled!
//
//  Process file names.
//
#ifdef WINCE
           set_currentdir (buffer,true);        // Set Windows CE Current directory
           import_file (buffer);                // Windows CE does not support multi select
#else  WINCE
           int   first;
           ptr = buffer+lstrlen(buffer)+1;      // Windows NT/95 does support multi-select
           first = true;
           while (*ptr || first) {
             first = false;
             lstrcpy (name,buffer);
             ptr = add_part(name,ptr);
             import_file (name);
           }
#endif WINCE
         }
         break;
  }
//
//  Activate and deactivate manipulation buttons.
//
  i = count();
  j = begin_item(list->current);

  EnableWindow (GetDlgItem(dialog,IDC_EDITLISTEDIT  ),i);
  EnableWindow (GetDlgItem(dialog,IDC_EDITLISTDELETE),i);
  EnableWindow (GetDlgItem(dialog,IDC_EDITLISTUP    ),j > 0);
  EnableWindow (GetDlgItem(dialog,IDC_EDITLISTDOWN  ),next_item(j) < i);
  return;
}

//--------------------------------
//
//  Generates an error message.
//
//      format -- Printf format string to be used in message.
//
void EDIT_list::error (int format,...) {
  TCHAR buffer[SIZE_BUFFER],string[SIZE_BUFFER];
  va_list argptr;
  va_start   (argptr,format);
  GET_STRING (string,format);
  wvsprintf  (buffer,string,argptr);
  MessageBox (dialog,buffer,get_string(IDS_ERROR),MB_OK | MB_ICONWARNING);
  return;
}

//--------------------------------
//
//  This is simply a utility rotuien that imports a file.  This was 
//  separated out so that both the drag&drop and the import button 
//  could make use of the same rotuine.
//
//      name -- Name of file to import.
//
void EDIT_list::import_file (tchar *name) {
  byte *user;
  if (!(user = load_image(name))) {     // Load file info memory
    error (IDS_LIST_IMPORTERROR,name);
    return;
  }  
  changed = true;                       // Mark list as changed.
  put_data (user,name);                 // Parse into list.
  free (user);                          // Free memory
  return;
}

//--------------------------------
//
//  Initializes the class.  This routine must be called before any 
//  other class rountines can be safely called.
//
//      hwnd        -- Dialog box window pointer.
//      data        -- Pointer to user data (passed to dirved class).  
//                     This is data used to initialize the list.  A value of 
//                     NULL will suppress the intialization.
//      import_id   -- Text id used in the open file dialog for 
//                     importing this type of object.  The format of 
//                     this string should be:
//
//                      <description>\t*.<extension>
//
//                      The routine will format this for use in a open
//                      file dialog.
//
void EDIT_list::init (HWND hwnd,byte *data,int import_id) {
  dialog     = hwnd;
  line_break = 0;                       // This allows classes that don't need line_break to ignore it.
  list       = (JWP_list *) SendDlgItemMessage(hwnd,IDC_EDITLIST,JL_GETJWPLIST,0,0);
  changed    = false;
  GET_STRING (import,import_id);
  initialize (GetDlgItem(hwnd,IDC_EDITLIST));   // Intialize the EUC_buffer class.
  if (data) put_data (data,NULL);
  list->single = true;
  move     (0);
  do_event (0);                         // Fake call into event to set buttons
  return;
}

//--------------------------------
//
//  Estimates the size of all items in the list.  This is used to 
//  allocate space for the items.  Return is in number of bytes
//  required and assumes all characters are kanji (two bytes each),
//  and some extra characters will be required.
//
//      RETURN -- Estimated size of object (over estimated).
//
long EDIT_list::size () {
  int    i,j;
  KANJI *kptr;
  long   size = 6;
  j = count ();
  for (i = 0; i < j; i++) size += 6+2*list->get_text(i,&kptr);  // 2 assumes all characters are kanji
  return (size);
}

//
//  End Class EDIT_list.
//
//===================================================================

//===================================================================
//
//  begin class JWP_file
//
//  These routines are parts of the class JWP_file that are used with
//  the list-box and edit functions.
//

//--------------------------------
//
//  This is a construct used to construct a JWP_file object for a edit-box.
//  The edit-box essentially edits a file that is one paragraph, all in a 
//  narrow display window.
//
//      hwnd -- Window to attach the file. to .
//
JWP_file::JWP_file (HWND hwnd) {
  memset (this,0,sizeof(JWP_file));
  filetype       = FILETYPE_EDIT;
  char_pagewidth = 20000;
  window         = hwnd;
//sel.type       = SELECT_NONE;
//name           = NULL;
//first = last   = NULL;
  ime_y          = GetSystemMetrics(SM_CYEDGE);
  new_paragraph (NULL);
  cursor.para    = view_top.para = first;
  cursor.line    = view_top.line = first->first;            
//cursor.pos     = view_top.pos  = 0;
  undo_init (0);
  activate ();
  return;
}

//--------------------------------
//
//  This routine copies the selected region from one JWP_file object to
//  another.  This is really indended to be used in the japanese edit 
//  control to copy the selected region out of the context file into 
//  the edit control's file.
//
//      file   -- Place to copy from.
//
//      RETURN -- Returns the length of the text added.  Generally, the
//                length is not used, but if this is zero, nothing was
//                transfered.
//
int JWP_file::edit_copy (JWP_file *file) {
  int length;
  if (!file->sel.type) return (0);
  file->all_abs ();
  if (file->sel.pos1.para == file->sel.pos2.para) length = file->sel.pos2.pos; else length = file->sel.pos1.para->length;
  length -= file->sel.pos1.pos;
  edit_set (file->sel.pos1.para->text+file->sel.pos1.pos,length);
  file->all_rel ();
  return (length);
}

//--------------------------------
//
//  This routine is used to set the text in a JWP_file object used within
//  a japanese edit control.  Primallraly, this blanks the first paragraph
//  and then replaces it with the text.  This is used to intialize the 
//  edit control, from a message, or from a the clipboard.
//
//      kanji  -- Text to set the edit control to.
//      length -- Length of the string to set.
//
void JWP_file::edit_set (KANJI *kanji,int length) {
  first->length        = 0;
  cursor.pos           = 0;
  first->first->length = 0;
  selection_clear ();
  put_string      (kanji,length);
  redraw_all      ();
  view_check      ();
  changed = false;                  // This works with the history buffer.
  return;
}

//
//  End Class JWP_file.
//
//===================================================================

//===================================================================
//
//  begin LIST_line
//
//  This class provides some basic operations on a line containned in
//  a Japanese List box control.
//

//--------------------------------
//
//  Allocate memory and copy a string into the buffer.
//
//      len  -- Length of string to copy.
//      line -- Text to copy.
//
void LIST_line::alloc (int len,KANJI *line) {
  int i;
  clear ();                     // Clear any old value.
  if (!len) return;
  if ((text = (KANJI *) malloc(i = sizeof(KANJI)*len))) {
    memcpy (text,line,i);
    length = len;
  }
  return;
}

//--------------------------------
//
//  Deallocate all resources used by the line and crear all flags.
//
void LIST_line::clear () {
  if (text) free (text);
  length = 0;
  text   = NULL;
  return;
}

//
//  End Class LIST_line.
//
//===================================================================

//===================================================================
//
//  Exported list hander routines and strctures
//

//--------------------------------
//
//  This structure describes the internal memory format used by the 
//  list manager.
//
#define LIST_BLOCK  1000                // Number of lines in each block

typedef struct LIST_list {
  class  LIST_line  lines[LIST_BLOCK];  // Actual data block
  struct LIST_list *next;               // Pointer to next block.
} LIST_list;

//--------------------------------
//
//  Window procedure for Japanese list boxes.
//
static LRESULT CALLBACK JWP_list_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  CREATESTRUCT *create;
  JWP_list     *list;
  list = (JWP_list *) GetWindowLong(hwnd,0);        // Get our JWP_List class object.
  switch (iMsg) {
//
//  Creation, we need to create the JWP_list object, and adjust the size
//  of the window to refelect the displayable area in the window.
//
    case WM_CREATE:                                 
         create = (CREATESTRUCT *) lParam;
         list = new JWP_list (hwnd);
         SetWindowLong (hwnd,0,(long) list);        // Save JWP_list object for list box.
         ImmAssociateContext (hwnd,NULL);           // Disable the IME for this window
         break;
    case WM_DESTROY:                                // Destroy the JWP_list object.
         delete list;
         return (0);
    case WM_GETDLGCODE:                             // We need to get input from windows.
         return (DLGC_WANTARROWS | DLGC_WANTALLKEYS | DLGC_WANTCHARS);
  }
  return (list->win_proc(hwnd,iMsg,wParam,lParam)); // Call the class windows procedure.
}

//===================================================================
//
//  begin JWP_list class
//
//  This class provides basic control over Japanese list boxes and 
//  managed lists.
//

//--------------------------------
//
//  Constuctor for Japanese list object.
//
//      hwnd -- Window containning list.
//
JWP_list::JWP_list (HWND hwnd) {
  memset (this,0,sizeof(JWP_list));     // Zero out structure which sets most variables.
  window     = hwnd;                    // Save window pointer.
  height     = list_font.lheight;       // Height of a display line.
  exclude    = NULL;                    // This field indicates a file excluded from insert operations.
  return;                               //   This is used to support dialogs with Japanese list controls.
}

//--------------------------------
//
//  Deconstructor.
//
JWP_list::~JWP_list () {
  int i;
  LIST_list *list;
  while (lists) {                       // Dispose of all lists.
    list  = lists;
    lists = list->next;
    for (i = 0; i < LIST_BLOCK; i++) list->lines[i].clear ();
    free (list);
  }
  return;
}

//--------------------------------
//
//  Put a line into the list.  If necessary the list will be extended 
//  so the line can be added.
//
//      loc  -- Line location to place this.
//      len  -- Length of the line in characters.
//      text -- Pointer to kanji text for the line.
//
void JWP_list::add_line (int len,KANJI *text) {
  LIST_line *line;
  int loc = count;
//
//  We need to allocate a new block!
//
  if (loc >= alloc) {
    LIST_list *list;
    list = (LIST_list *) calloc(1,sizeof(LIST_list));
    if (!list) return;                          // Allocation failed.
    if (!last) lists = last = list;
      else {
        last->next = list;
        last       = list;
      }
    alloc += LIST_BLOCK;                        // Correct number of allocated lines.
  }
  count++;                                      // Correct line count.
  line = get_line(loc);                         // Get the line
  line->alloc (len,text);                       // Assign string to it.
  que_line    (loc);                            // Redraw line
  scroll      ();                               // Reset scroll bar.
  return;
}

//--------------------------------
//
//  This is called when the size of the control is changd or during creation of
//  the control to adjust the size of the control.
//
void JWP_list::adjust () {
  int  i;
  RECT dialog,rect;
//
//  Basic parameters.
//
  if (!this) return;
  GetClientRect (window,&rect);
  width  = rect.right-rect.left;                                                    // Width of display arrea.
  xmax   = width-list_font.hwidth;
  GetWindowRect (window,&rect);
  GetWindowRect (GetParent(window),&dialog);
  lines  = (rect.bottom-rect.top-2*GetSystemMetrics(SM_CYEDGE))/height;             // Calculate viewable lines.
  i      = lines*height+2*GetSystemMetrics(SM_CYEDGE);
//
//  If window is not an exact number of lines change the window size.  Note, that if
//  we change the size we will come back here again so we don't do anything else.
//
  if (i != rect.bottom-rect.top) {
#ifndef WINCE
    MoveWindow (window,rect.left-dialog.left-GetSystemMetrics(SM_CXFRAME),rect.top-dialog.top-GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYFRAME),rect.right-rect.left,i,true);
#else   WINCE
    MoveWindow (window,rect.left-dialog.left-GetSystemMetrics(SM_CXEDGE ),rect.top-dialog.top-GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYEDGE ),rect.right-rect.left,i,true);
#endif  WINCE
  }
//
//  If the window is the correct size, we can redisplay the data into the new window.
//  If we adjusted the window above, windows will send another WM_SIZE message, and
//  that time we will have the correct window size.  This prevents doing a lot of 
//  work twice.
//
    else {
      KANJI     *kptr;
      EUC_buffer line;
      LIST_list *old,*temp;
      int        j,len,old_top,old_current,old_count,new_top,new_current,highlight;
      old_count    = count;                                                     // Save old parameters
      old_current  = new_current = begin(current);
      old_top      = new_top     = begin(top);
      old          = lists;                                                     // Save old lists
      lists = last = NULL;                                                      // Reinitialize to a blank list.
      count = top  = current = alloc = 0;
      highlight    = false;
      if (old_count) {
        line.initialize (window);                                               // Setup EUC_buffer.
        for (i = 0, temp = old; temp; temp = temp->next) {
          for (j = 0; (i < old_count) && (j < LIST_BLOCK); j++) {
            kptr = temp->lines[j].text;                                         // Get each line.
            len  = temp->lines[j].length;
            if (*kptr == EUC_HIGHLIGHT) {                                       // If line starts with highlight marker we skip marker and set highlight.
              kptr++; 
              len--; 
              highlight = true;
            }
            if (*kptr != '\t') {                                                // If line does not begin with a tab, and is not first line, dump buffer,
              if (i) line.flush     (-1);                                       //   reset highlight, and start a new line.
              line.highlight (highlight); 
              line.clear     (); 
              highlight = false;             
            }
            if (*kptr == '\t') line.put_kanji(kptr+1,len-1); else line.put_kanji(kptr,len);   // Output line
            if (i == old_top    ) new_top     = count;                          // If this line was top, or current save new location.
            if (i == old_current) new_current = count;
            i++;
          }
          line.flush (-1);                                                      // Flush out last buffer.
        }
      }
//
//  Dispose of old memory.
//
      while (old) {
        for (i = 0; i < LIST_BLOCK; i++) {
          if (old->lines[i].text) free (old->lines[i].text);
        }
        temp = old;
        old  = old->next;
        free (temp);
      }
//
//  Reset display parameters.  Remember a redraw is already qued.
//
      top     = new_top;
      current = new_current;
      if (top+lines-1 > count) top = count-lines+1;
      if (top > current) top = current;
      if (top+lines-1 < current) top = current-lines+1;
      if (top < 0) top = 0;
      scroll ();
    }
  return;
}

//--------------------------------
//
//  From an index into a block, this routine returns the index of the 
//  first line in the block.
//
//      line   -- Starting line for the search.
//
//      RETURN -- Index of the first line in the block.  
//  
int JWP_list::begin (int line) {
  int    len;
  KANJI *kptr;
  while (true) {
    if (line <= 0) return (0);
    len = get_text(line,&kptr);
    if (*kptr != '\t') break;
    line--;
  }
  return (line);
}

//--------------------------------
//
//  Copy the context of the selection to the clipboard.
//
void JWP_list::clip_copy () {
  JWP_file *file = NULL;
  if ((file = file->clip_copy())) insert (false,file);
  return;
}

//--------------------------------
//
//  Deletes all lines associated with a block
//
//      line -- Line wihin the block to be deleted (not necessarly 
//              the first line.
//
//      RETURN -- Pointer to the first block of the line.
//
int JWP_list::del_block (int line) {
  int i,j;
  line = begin(line);
  i    = next(line)-line;
  for (j = 0; j < i; j++) del_line (line);
  return (line);
}

//--------------------------------
//
//  Deletes the indicated line from the list.
//
//      line -- Line to delete.
//
void JWP_list::del_line (int line) {
  LIST_line *data,*data2;
  if (!(data = get_line(line))) return;         // Line does not exist
  if (data->selected) select_count--;           // Keep track of number of selected lines.
  data->clear();                                // Clear data for line.
  que_line (line);                              // Que draw.
  while (true) {                                // Shuffle other lines up
    data2 = get_line(++line);
    que_line (line);
    if (!data2) {
      data->text = NULL;
      data->clear ();
      break;
    }
    *data = *data2;
    data  = data2;
  }
  count--;                                      // Decrement count.
  if (current >= count) move (current-1,false); // Process move.
  return;
}

//--------------------------------
//
//  This is the main rederming routine.
//
//      hdc  -- Display context to hender to.
//      line -- Line to be rendred.
//
#define NO_CHANGE   0xFFFFFFFF

void JWP_list::draw_line (HDC hdc,int line) {
  RECT       rect;
  KANJI      ch;
  LIST_line *text;
  COLORREF   oldcolor = NO_CHANGE;
  int        i,x,y;
//
//  Generate the background rectangle.
//
  rect.top    = (line-top)*height;
  rect.bottom = rect.top+height;
  rect.left   = 0;
  rect.right  = width;
  if (focus && (line == current)) {
    HPEN   pen; 
    pen = (HPEN) SelectObject(hdc,CreatePen(PS_SOLID,0,GetSysColor(COLOR_WINDOWTEXT)));
    SelectObject (hdc,GetSysColorBrush(COLOR_WINDOW));
    SetBkMode    (hdc,OPAQUE);
    Rectangle    (hdc,rect.left,rect.top,rect.right,rect.bottom);
    DeleteObject (SelectObject(hdc,pen));
    SetBkMode    (hdc,TRANSPARENT);
  }
  else {
    FillRect (hdc,&rect,GetSysColorBrush(COLOR_WINDOW));
  }
//
//  Render text
//
  text = get_line(line);                    // Get line
  if (!text || !text->text) return;         // No text so exit.
  x = list_font.x_offset;                   // Setup intial position.
  y = rect.bottom-list_font.loffset;        // Only use of loffset.
  i = 0;
//
//  Chech for highlite line.
//
  if (text->text[0] == EUC_HIGHLIGHT) {
    oldcolor = SetTextColor(hdc,jwp_config.cfg.info_color);
    i++;
  }                                                                
//
//  Render loop
//
  for (; i < text->length; i++) {
    ch = text->text[i];
    if (ISJIS(ch)) list_font.kanji->draw(hdc,ch,x,y);
    else if (ch != '\t') {
      ascii_draw (hdc,x,y-list_font.height,ch);
    }
    x = list_font.hadvance(x,ch);
  }
//
//  Render selection indicator.
//
  if (text->selected) {         
    if (sel_x1) { rect.left = sel_x1; rect.right = sel_x2; }    // This line has a single line select.
    InvertRect(hdc,&rect);        // Select rectangle
  }
//
//  Cleanup
//
  if (oldcolor != NO_CHANGE) SetTextColor(hdc,oldcolor);        // Restore text color
  return;
}

//--------------------------------
//
//  This routine implements a find command in the list.  This command is based on finding a 
//  string within the block, not the individual string.
//
//      search_length -- Length of the search string.
//
void JWP_list::find (int search_length) {
  int   i,index,length,start;
  KANJI kbuffer[SIZE_BUFFER];
  start = index = begin(current);                                               // Get starting position.
  while (true) {                                                                // Continue search until resolved.
    if (jwp_config.cfg.search_back) {                                           // Backup a line.
      if (index == 0) {
        if (jwp_config.cfg.search_wrap) index = count; else goto NotFound;
      }
      index = begin(index-1);
    }
    else {                                                                      // Go forward a line.
      index = next(index);
      if (index > count) {
        if (jwp_config.cfg.search_wrap) index = 0; else goto NotFound;
      }
    }
    if (index == start) goto NotFound;                                          // Looped back to beginning point -> not found.
    length = get_buffer (kbuffer,index);                                        // Get block.
    for (kbuffer, i = 0; i <= length-search_length; i++) {                      // Check for string.
      if (jwp_search.test(&kbuffer[i])) goto Found;
    }
  }
Found:;                                                                         // Found
  move (next(index)-1,false);                                                   // This premove makes sure the entire thing should be visible.
  move (index,true);                                                            // This true causes the block to be highlighted.
  return;  
NotFound:;                                                                      // Not found.
  jwp_search.not_found (window,false);
  return;
}

//--------------------------------
//
//  Extracts a complete logical item from the list.
//
//      buffer -- Location to store the extracted string.  
//      index  -- Index into item to be extracted.
//
//      RETURN -- Length of the string extracted.
//
//  The extracted string is placed in kbuffer, and the length parameter
//  is set to indicate the length ofthe extraction.  The buffer is also
//  null terminated so both methods can be used.
//
int JWP_list::get_buffer (KANJI *buffer,int index) {
  KANJI *kptr;
  int    i,len,length;
  index  = begin(index);
  length = 0;
  while (true) {
    if (!(len = get_text(index++,&kptr))) break;
    if (length && (*kptr != '\t')) break;   
    if (*kptr == '\t') { kptr++; len--; }
    for (i = 0; i < len; i++) buffer[length++] = *kptr++;
  } 
  buffer[length] = 0;
  return (length);
}

//--------------------------------
//
//  This routine deterimes the character that is located under the 
//  cursor.  The cusor position is passed in the form it is received
//  from Windows, ie with both parameters within the lParam.
//
//  The result is a character stored in the objects last_char field.
//  this caracter can be used if the user select character information.
//
//      lParam  -- Cursor position as passed from Windows.
//      pos     -- Address for return of the actual cursor index of the character.
//      average -- If set to true will select the character by average or center
//                 position, instead of the true character containning the cursor.
//                 Generally we start with this set to false, then change to 
//                 true when dragging a selection.
//
//      RETURN  -- The pixal location of the beginning of the character.
//
int JWP_list::get_char (LPARAM lParam,int *pos,int average) {
  KANJI *kptr;
  int    i,j,k,x,x1;
  i  = LOWORD(lParam);
  j  = get_text((HIWORD(lParam)-1)/height+top,&kptr);
  x  = list_font.x_offset;
  x1 = 0;                               // Make some sticky compilers happy
  for (k = 0; (k < j) && (x < i); k++) x = list_font.hadvance(x1 = x,kptr[k]);
  if (x < i) {                          // Click is past the end of the line.
    last_char = 0; 
    *pos      = j; 
    return (x); 
  }
  if (k <= 0) {                         // Click is to left of the first character
    last_char = kptr[0];
    *pos      = 0;
    return (list_font.x_offset);
  }
  if (average && ((x-i) < (i-x1))) {    // Click is in character, but average is on so take right character
    last_char = kptr[k];
    *pos      = k;
    return (x);
  }
  last_char = kptr[k-1];                // Click is in line, so take character to left.
  *pos      = k-1;
  return (x1);
}

//--------------------------------
//
//  Get the LIST_line structure associated with a line.  
//
//      line   -- Line number.
//
//      RETURN -- Pointer to line structure, or NULL if the line does 
//                not exist.
//
LIST_line *JWP_list::get_line (int line) {
  LIST_list *list;
  if (line >= count) return (NULL);
  for (list = lists; line >= LIST_BLOCK; list = list->next) line -= LIST_BLOCK;
  return (&list->lines[line]);
}

//--------------------------------
//
//  This is the major routine clients use to get text from the list
//  box.  This routine retrieves the list's own memory pointer to 
//  the line and the length.  You must not modify the return memory, 
//  nor can you rely on this memory being valid after the list box is
//  closed.
//
//      line   -- Line to returneve data fro.
//      text   -- Pointer to a pointer.  On exit the pointer will point 
//                to the text assoicated with the indicated line.
//      
//      RETURN -- Return is the length of the line.
//
//  If the line requisted is beyond the end of the list the text
//  pointer will point to a static NULL location, and the length will
//  be zero.
//
//  Note, that this routine automatically strips out things like the 
//  hidden code for highlighted line.
//
int JWP_list::get_text (int line,KANJI **text) {
  static KANJI none = 0;
  LIST_line *data;
  KANJI     *kptr;
  int        i;
  if (!(data = get_line(line))) { *text = &none; return (0); }
  i    = data->length;
  kptr = data->text;
  if (*kptr == EUC_HIGHLIGHT) { kptr++; i--; }
  *text = kptr;
  return (i);
}

//--------------------------------
//
//  Inserts all selected lines into the indicated file.
//
//      newline -- Forces new lines after each line inserted.
//      file    -- Indicates the file to insert to.  If this is NULL the default file
//                 determined by the file insertion list will be used.
//
void JWP_list::insert (int newline,JWP_file *file) {
  KANJI     *text;
  LIST_line *line;
  if (!file) file = file_list.get(exclude);
  int i,j,s1,s2;
#ifdef FOLLOW_BY_SPACE      // This option is a slight change in the paste-back formating.
  KANJI space[2] = { KANJI_SPACE };
#else  FOLLOW_BY_SPACE
  KANJI space[1] =  { '\t' };
#endif FOLLOW_BY_SPACE
  int new_para   = (file->filetype == FILETYPE_EDIT) ? false : jwp_config.cfg.paste_newpara;
  int first      = true;                                        // Inidcates first line.
  int need_space = false;                                       // This indicates that we need to add a spapce before adding 
                                                                //   the next line.  If the previous line ends with a space or
                                                                //   the next line begins with a space we do not need to add
                                                                //   one. 
  if (!select_count) { MessageBeep (MB_ICONASTERISK); return; } // Nothing found so make a warning
  file->selection_clear();                                      // Clear selection so it dosen't get wipped out
  file->undo_start ();                                          // Allow this to be undone
  file->undo_para  (UNDO_QUE);  
  s1 = sel_pos1;                                                // These can get modified so we need to work
  s2 = sel_pos2;                                                //   with copies.
  for (i = 0; i < count; i++) {                                 // Move through all possible lies.
    line = get_line(i);
    if (line->selected) {                                       // Is the line selected?
      j = get_text(i,&text);                                    // Get the line text.
      if (*text == '\t') { text++; j--; s1--; s2--; }           // Text begins with '\t', so skip that
        else if (!first && new_para) {                          // If not tab, do we need a new paragraph?
          need_space = false;
          file->do_key (VK_RETURN,false,false);                 // Do new paragraph
        }
      first = false;                                            // Can't be first line anymore.
      if (*text == ' ') need_space = false;                     // Text begins with space so we don't need another
      if (need_space) file->put_string (space,1);               // We need a space, so put one.
      if (!sel_x1) file->put_string (text,j);                   // Put the actual text.
        else file->put_string (text+s1,s2-s1);                  // Put single line selection.
      need_space = (!ISJIS(text[j-1]) && (text[j-1] != ' '));   // If line does not end with space we will need on on the next line.
    }                                                           //   Or if we don't end on a kana
  }
  if (newline && new_para) file->do_key (VK_RETURN,false,false);    // Insert end CR when pasting to other file.
  file->undo_end   ();                                  // Done with the undo.
  file->view_check ();
  return;
}

//--------------------------------
//
//  This is a major routine that is called any time the current position
//  in the list is changed.  
//
//      pos   -- New current position.
//      shift -- Set to non-zero if the shift key is held down, or the 
//               selected region is being extended.
//
void JWP_list::move (int pos,int shift) {
  int        i,j,old;
  KANJI     *kptr;
  LIST_list *list;
  
  old     = current;                        // Save old location for later
  current = pos;;                           // Calculate new location.
  if (current >= count) current = count-1;  // Clip locations to actual data.
  if (current < 0) current = 0;             //   Order is importaint when no data is there
//
//  No shift, so clear all selections and make a single selection.
//
  if (!shift) {
    selecting = false;                      // Not doing a selection.
    for (j = 0, list = lists; list; list = list->next) {
      for (i = 0; i < LIST_BLOCK; i++) {
        if (list->lines[i].selected) {
          list->lines[i].selected = false;
          que_line (j+i);                   // If selection changed que for draw.
        }
      }
      j += LIST_BLOCK;
    }
    select_count = 0;
    sel_x1 = sel_xf = 0;                    // Clear selection for a single line
    select (current,true);
  }
//
//  We are doing an extended select, so pay attetion.
//
  else {
    if (!selecting) {                       // Starting a selection so save data.
      selecting = true;
      sel_fixed = old;
    }
    i = old;                                // Re-evaluate lines between old and new position.
    while (true) {
      if (((sel_fixed <= i) && (current >= i)) || 
          ((sel_fixed >= i) && (current <= i))) j = true; else j = false;
      select (i,j);
      if (i == current) break;
      if (old < current) i++; else i--;
    }
  }
//
//  Make sure the current line is visible.
//
  if ((current < top) || (current >= top+lines)) {      // Scroll required
    if (current < top) top = current; else top = current+1-lines;
    redraw ();
  }
  else {
    que_line (old);                         // Simple line redraw.
    que_line (current);
  }
  scroll ();                                // Adjust scroll bar
  last_char = get_text(current,&kptr) ? kptr[0] : 0;        // Get first character for info.
  if ((old != current) && single) SendMessage (GetParent(window),WM_COMMAND,MAKELONG(GetWindowLong(window,GWL_ID),true),0);
  return;
}

//--------------------------------
//
//  Move a block of lines from one location ot another.  The from and 
//  to parameters do not have to be at the beginning of blocks.
//
//      from   -- Where to move lines from.
//      to     -- Where to move lines to.
//
//      RETURN -- Returns the location where the line actually ended up.
//
int JWP_list::move_block (int from,int to) {
  LIST_line temp,*line1,*line2;
  int done,i,j;
  from = begin(from);           // Move to the beginning of the blocks.
  to   = begin(to);
  done = next(from);            // Get the end location of the from block for the ending.
//
//  Que lines that will be affected, and need to be redraw.
//
  if (to < from) { i = to; j = done; } else { i = from; j = next(to); }
  while (i < j) que_line (i++);
//
//  Moving to lowere lines.
//
  if (to < from) {
    for (; from < done; from++, to++) {
      line1 = get_line(from);
      temp  = *line1;
      for (i = from-1; i >= to; i--) {
        line2 =  get_line(i);
       *line1 = *line2;
        line1 =  line2;
      }
      *line1 = temp;
    }
  }
//
//  Moving to upper lines.
//  
  else {
    for (done -= from; done; done--) {
      line1 = get_line(from);
      temp  = *line1;
      for (i = from+1; i < to; i++) {
        line2 =  get_line(i);
       *line1 = *line2;
        line1 = line2;
      }
      *line1 = temp;
    }
  } 
//
//  Move cursor to the newly selected line.
//
  move (i = begin(to-1),false);
  return (i);
}

//--------------------------------
//
//  From a given line find the beginning of the next block.
//
//      line   -- Line to start the search in.
//      
//      RETURN -- Index of the first line of the next block.
//
int JWP_list::next (int line) {
  KANJI *kptr;
  int    len;
  if (line >= count) return (count+1);
  do {
    len = get_text(++line,&kptr);
  } while (len && (*kptr == '\t'));
  return (line);
}

//--------------------------------
//
//  Routine to handle the popup menu.
//
//      x,y -- Location of the menu (within the list window).
//
void JWP_list::popup_menu (int x,int y) {
  int       i,j;
  HMENU     pmenu;
  RECT      rect;
  JWP_file *tfile;
  TCHAR     buffer[SIZE_BUFFER];
  SetFocus (window);
  pmenu = GetSubMenu(popup,1);
  DeleteMenu (pmenu,IDM_LIST_INSERTTO,MF_BYCOMMAND);
  if (last_insert) {                                // Check for last insert file is still valid.
    tfile = jwp_file;
    do {
      if (tfile == last_insert) break;
      tfile = tfile->next;
    } while (tfile != jwp_file);
    if (tfile != last_insert) last_insert = NULL;
  }
  if (last_insert) {                                // Last insert is still valid so build menu item
    AppendMenu (pmenu,MF_STRING,IDM_LIST_INSERTTO,format_string(buffer,IDS_LIST_POPUP,last_insert->get_name()));
  }                                                 // Enable/disable menu items.
  if (select_count) j = MF_BYCOMMAND | MF_ENABLED; else j = MF_BYCOMMAND | MF_GRAYED;
  for (i = IDM_LIST_COPY; i <= IDM_LIST_INSERTTO; i++) EnableMenuItem (pmenu,i,j);
  EnableMenuItem (pmenu,IDM_LIST_REPLACETOFILE,(select_count && file_list.get(exclude)->sel.type) ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED));
  GetWindowRect  (window,&rect);                    // Generate popup

  x += rect.left;
  y += rect.top;
#ifdef WINCE
  TrackPopupMenu (pmenu,TPM_LEFTALIGN | TPM_TOPALIGN,x,y,0,window,NULL);
#else  WINCE
  TrackPopupMenu (pmenu,TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,x,y,0,window,NULL);
#endif WINCE
  return;
}

//--------------------------------
//
//  Marks the rectangle associated with a given line as invlaid.
//
//      line -- Line to be marked invalid.
//
void JWP_list::que_line (int line) {
  RECT rect;
  rect.top    = (line-top)*height;
  rect.bottom = rect.top+height;
  rect.left   = 0;
  rect.right  = width;
  InvalidateRect (window,&rect,false);
  return;
}

//--------------------------------
//  
//  Resets the contents of the list.  All items are reumoved and the 
//  list is redrawn.
//
void JWP_list::reset () {
  LIST_list *list;
  int        i;
  for (list = lists; list; list = list->next) {
    for (i = 0; i < LIST_BLOCK; i++) {
      list->lines[i].selected = false;
      list->lines[i].clear();
    }
  }
  count = top = current = 0;
  selecting       = false;              // Not doing a select
  select_count    = 0;                  // No selected lines (no lines)
  sel_xf = sel_x1 = 0;                  // No single line select.
  redraw ();
  return;
}

//--------------------------------
//
//  Sets the scroll bar position for the list.
//
void JWP_list::scroll () {
  scroll_info.nMax   = count-1;
  scroll_info.nPage  = lines;
  scroll_info.nPos   = top;
  SetScrollInfo (window,SB_VERT,&scroll_info,true);
  return;
}

//--------------------------------
//
//  Sort the list.
//
//      proc -- Procedure for comparison.
//
void JWP_list::sort (int (*proc)(KANJI *buf1,KANJI *buf2)) {
  KANJI buf1[SIZE_BUFFER],buf2[SIZE_BUFFER];
  int i,j,low;
  for (i = 0; i < count; i = next(i)) {
    get_buffer (buf1,low = i);
    for (j = next(i); j < count; j = next(j)) {
      get_buffer (buf2,j);
      if ((*proc)(buf1,buf2)) {
        low = j;
        get_buffer (buf1,low = j);
      }
    } 
    if (low != i) move_block (low,i);
  }
  SetFocus (window);
  move     (0,false);
  redraw   ();
  return;
}

//--------------------------------
//
//  Change (ie set) the selection state of a given line.
//
//      line  -- Line to change the selection state of.
//      onoff -- New state.
//
void JWP_list::select (int line,int onoff) {
  LIST_line *data;
  if (!(data = get_line(line))) return;
  if ((data->selected && !onoff) || (!data->selected && onoff)) {
    if (onoff) select_count++; else select_count--;
    data->selected = onoff;
    que_line (line);
  }
  return;
}

//--------------------------------
//
//  Window procedure for Japanese list-box control
//
#define BOTTOM  (count-lines)     // Value of top when at bottom of display.

int JWP_list::win_proc (HWND hwnd,int msg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  HFONT       font;
  PAINTSTRUCT ps;
  int         i,j;
  static short delta = 1;               // This is a KLUDGE used to get around the fact
                                        //   that mouse_event will not generate an event
                                        //   if the mouse does not move so we generate
                                        //   events that move one micky right and left 
                                        //   alternately, so the average is no motion.
  static short mouse_x,mouse_y;         // Last mouse position.
  switch (msg) {
//```````````````````````````````````````````````````````````````````
//
//  General window messages (create, destroy, paint).
//

//
//  Crate only generates the scroll bar
//
    case WM_CREATE:
         adjust ();
         return (0);
//
//  Size changing
//
    case WM_SIZE: 
         adjust ();
         return (0);
//
//  Set and Kill focus simply change state and redraw.
//
    case WM_SETFOCUS:
         focus = true;
         que_line (current);
         break;
    case WM_KILLFOCUS:
         focus = false;
         que_line (current);
         break;
//
//  Paint renders all lines.
//
    case WM_PAINT:
         hdc = BeginPaint (hwnd,&ps);
         font = (HFONT) SelectObject (hdc,list_font.ascii);
         SetBkColor   (hdc,GetSysColor(COLOR_WINDOW));
         SetTextColor (hdc,GetSysColor(COLOR_WINDOWTEXT));
         SetBkMode    (hdc,TRANSPARENT);
         for (i = 0; i < lines; i++) draw_line (hdc,i+top);
         SelectObject (hdc,font);
         EndPaint (hwnd,&ps);
         return (0);
//```````````````````````````````````````````````````````````````````
//
//  Keyboard events.
//
    case WM_KEYDOWN:
         int shift,ctrl;
         shift = (GetKeyState(VK_SHIFT)   < 0);
         ctrl  = (GetKeyState(VK_CONTROL) < 0);
         switch (wParam) {
           case VK_TAB:                         // Tab -> Next or previous control
                SetFocus (GetNextDlgTabItem(GetParent(hwnd),hwnd,shift));   // Move to next last item.
                return (0);
           case VK_RETURN:                      // Return has special meaning (invoke dialog event)
                SendMessage (GetParent(hwnd),WM_COMMAND,IDOK,0L);
                return (0);
           case VK_ESCAPE:                      // Escape has special meaning (abort dialog)
                SendMessage (GetParent(hwnd),WM_COMMAND,IDCANCEL,0L);
                return (0);
           case VK_HOME:                        // Home -> top of list
                move (0,shift);
                return (0);
           case VK_END:                         // End -> Bottom of list
                move (count,shift);
                return (0);
           case VK_UP:                          // Up -> Up one line
                sel_x1 = 0;
                if (ctrl) SendMessage (GetParent(hwnd),WM_COMMAND,IDC_EDITLISTUP,0); 
                else if (jwp_config.cfg.page_mode_list) move (current+1-lines,shift);
                else move (current-1,shift);
                return (0);
           case VK_DOWN:                        // Donw -> Down one line
                sel_x1 = 0;
                if (ctrl) SendMessage (GetParent(hwnd),WM_COMMAND,IDC_EDITLISTDOWN,0); 
                else if (jwp_config.cfg.page_mode_list) move (current+lines-1,shift);
                else move (current+1,shift);
                return (0);
           case VK_PRIOR:                       // Page up -> Up one page
                move (current+1-lines,shift);
                return (0);
           case VK_NEXT:                        // Page down -> Down one page
                move (current+lines-1,shift);
                return (0);
           case VK_DELETE:                      // Delete -> Delete entry (single select list only)
                SendMessage (GetParent(hwnd),WM_COMMAND,IDC_EDITLISTDELETE,0);
                return (0);
           case VK_SPACE:                       // Space -> Add entry (single select list only)
                SendMessage (GetParent(hwnd),WM_COMMAND,IDC_EDITLISTEDIT,0);
                return (0);
           case VK_INSERT:                      // Insert -> Add entry (single select list only)
                if (ctrl) clip_copy ();         //           Ctrl insert does a copy to clipboard
                  else SendMessage (GetParent(hwnd),WM_COMMAND,IDC_EDITLISTADD,0);
                return (0);
           case VK_F23:
                SendMessage (hwnd,WM_RBUTTONDOWN,0,0xffffffff);
                return (0);
           case VK_A:                           // ctrl+shift+A -> Select all.
                if (ctrl) {
SelectAll:;
                  i = top;
                  j = current;   
                  move (0,false);
                  move (count-1,true);
                  top     = i;
                  current = j;
                  scroll ();
                  redraw ();
                  return (0);
                }
                break;
           case VK_C:                           // ctrl+Insert/ctrl+C -> Copy to clipboard
                if (ctrl) {
                  clip_copy ();
                  return    (0);
                }
                break;
           case VK_I:
                if (ctrl) kanji_info (hwnd,last_char);
                return (0);
           case VK_F:
           case VK_S:
                if (!ctrl) break;
           case VK_F8:
                jwp_search.do_search (this);
                return               (0);
           case VK_N:
                if (!ctrl) break;
           case VK_F9:
                jwp_search.do_next (this);
                return             (0);
           case VK_RIGHT:
           case VK_LEFT:
                SendMessage (GetParent(hwnd),WMU_EDITFROMLIST,wParam,lParam);
                return      (0);
           default:
                break;
         }
         return (0);
//```````````````````````````````````````````````````````````````````
//
//  Character messages.  These are used only by the dictionary to go on to the next search.
//
    case WM_CHAR:
         ctrl  = (GetKeyState(VK_CONTROL) < 0);
         if (ctrl || (wParam == '\t')) return (0);      // Let WM_KEYDOWN handle this one.
         SendMessage (GetParent(hwnd),WMU_CHARFROMLIST,wParam,lParam);
         return      (0);
#ifndef WINCE
    case WM_IME_CHAR:                                   // IME support.
         SendMessage (GetParent(hwnd),WMU_IMEFROMLIST,wParam,lParam);
         return (0);
#endif WINCE
//```````````````````````````````````````````````````````````````````
//
//  Control messages.
//
//  The following are requests for action on the dialog box via messages.
//
    case JL_RESET:                          // Reset contexts of list.
         reset  ();
         return (0);
    case JL_GETBEGIN:                       // Get first line of this block.
         wParam = begin(current);
    case JL_GETTEXT:    // *** FALL THORUGH // Get text information and return.
         return (get_text(wParam,(KANJI **) lParam));
    case JL_INSERTTOFILE:                   // Insert selected lines into file
         insert (false);
         return (0);
    case JL_GETJWPLIST:                     // Get the JWP_list object.
         return ((long) this);
    case JL_SETSEL:                         // Change the select state of a line.
         select (lParam,wParam);
         return (0);
    case JL_SETEXCLUDE:                     // Set the exclusion file used for insert to file.
         exclude = (JWP_file *) lParam;
         return (0);
//```````````````````````````````````````````````````````````````````
//
//  Timer events.
//
    case WM_TIMER:
         if (wParam == TIMER_AUTOSCROLL) {
           KillTimer    (hwnd,TIMER_AUTOSCROLL);          // Auto-scroll timer.
           mouse_event  (MOUSEEVENTF_MOVE,delta,0,0,0);   // Fake mouse event so window keeps scrolling
           if (delta == 1) delta = -1; else delta = 1;    // Toggle mouse direction so no net motion occures.
         }
         else {
           selecting = false; 
           KillTimer      (hwnd,TIMER_MOUSEHOLD); 
           ReleaseCapture (); 
           popup_menu     (mouse_x,mouse_y);
         }
         return (0);
//```````````````````````````````````````````````````````````````````
//
//  Mouse events.
//
//  Left button up indicates the user is done with the selection.
//
    case WM_LBUTTONUP:
         in_select = false;
         KillTimer      (hwnd,TIMER_MOUSEHOLD);
         ReleaseCapture ();
         return         (0);        
//
//  Left mouse double click -> Send message to parent
//  
    case WM_LBUTTONDBLCLK: 
         SendMessage (GetParent(hwnd),WM_COMMAND,GetWindowLong(hwnd,GWL_ID),0);
         return      (0);
//
//  Mouse moves only count if we are in a mouse select.  If we are, we
//  treat mouse moves simply as if the user shift clicked.
//
    case WM_MOUSEMOVE:
         if ((abs(LOWORD(lParam)-mouse_x) > DOUBLE_X) || (abs(HIWORD(lParam)-mouse_y) > DOUBLE_Y)) KillTimer (hwnd,TIMER_MOUSEHOLD);
         if (!in_select) return (0);
         wParam = MK_SHIFT;             
//
//  Left mouse button.
//
    case WM_LBUTTONDOWN:            // **** FALL THROUGH ****
         if (msg == WM_LBUTTONDOWN) {
           mouse_x = (short) LOWORD(lParam); 
           mouse_y = (short) HIWORD(lParam); 
           SetTimer   (hwnd,TIMER_MOUSEHOLD,GetDoubleClickTime(),NULL);
           SetCapture (hwnd);
         }
//
//  Set focus to this window.  Translate messages as needed.
//
         SetFocus (hwnd);
         if (GetKeyState(VK_MENU) >= 0) {       // alt+left -> right mouse button click
//
//  Find the y location of the mosue click.
//
           i = (HIWORD(lParam)-1)/height+top;
           if (!in_select || (i != last_y)) {           // Mouse selelect that did not really move (suppress flashing).
             last_y    = i;                             // Save last position for next time.
             in_select = true;                          // This could be a mouse select.
//
//  Take care of control click
//
             if (wParam & MK_CONTROL) {
               LIST_line *line;
               if (!(line = get_line(i))) return (0);
               que_line (current);
               que_line (i);
               select   (i,!line->selected);
               sel_fixed = i;
               current   = i;
               return   (0);
             }
//
//  This is a basic left click (or shift-left click
//
             move (i,wParam & MK_SHIFT);
           }
//
//  Single line selction section.
//
//  Special characteristics:
//
//      sel_xf = 0 --> Have not set the fixed point, ie. starting a select.
//      sel_x1 = 0 --> Select is disabled because same start point/end point
//                     or because the number of lies is greater than 1.
//
           if (1 != select_count) sel_x1 = 0;       // More than one selected line, so cannot have single line select.
             else {
               j = get_char(lParam,&i,sel_xf);      // Get cusor poisition information.
               if (!sel_xf) {                       // No fixed point so set it.
                 sel_xf   = j;
                 sel_posf = i;
                 last_x   = i;                      // Provide for quick exit.
               }
               if (i != last_x) {                   // No motion so exit.
                 if (i > sel_posf) {                // Set points.  One is just the fixed point,
                   sel_pos1 = sel_posf;             //   and the other is just the new point.
                   sel_pos2 = i;                    //   sel_x1 always preceeds sel_x2.
                   sel_x1   = sel_xf;
                   sel_x2   = j;
                 }
                 else {
                   sel_pos2 = sel_posf;
                   sel_pos1 = i;
                   sel_x2   = sel_xf;
                   sel_x1   = j;
                 }
                 if (sel_x1 == sel_x2) sel_x1 = 0;  // Same start and end point so disable single line.
               } 
               que_line ((HIWORD(lParam)-1)/height+top);
             }
//
//  Auto-scroll handler.  When currsor is close enough to the edge 
//  we generate move up or move down commands necessary to scroll the 
//  list.
//
           if (!jwp_config.cfg.auto_scroll) return (0);
           if      ((HIWORD(lParam) <  height/3)              && (top > 0     )) i = SB_LINEUP;
           else if ((HIWORD(lParam) >= height*lines-height/3) && (top < BOTTOM)) i = SB_LINEDOWN;
           else return (0);             // No auto-scroll so exit.

           win_proc     (hwnd,WM_VSCROLL,i,0);          // Scroll list.
           UpdateWindow (hwnd);                         // Force window redraw
           SetTimer     (hwnd,TIMER_AUTOSCROLL,jwp_config.cfg.scroll_speed,NULL);
           KillTimer    (hwnd,TIMER_MOUSEHOLD);
           return       (0);
         }
//
//  Right mouse button invokes popup menu.
//
#ifndef WINCE
    case WM_CONTEXTMENU:
         if (lParam != -1) return (0);                  // Want only keyboard events here.
         popup_menu (width/2,current*height+(height*3)/4);
         return (0);
#endif WINCE
    case WM_RBUTTONDOWN: 
         i = LOWORD(lParam);
         get_char (lParam,&i,false);
         i = LOWORD(lParam);
         j = HIWORD(lParam);
         if (wParam & MK_SHIFT) kanji_info (hwnd,last_char); else popup_menu (i,j);
         return (0);
//```````````````````````````````````````````````````````````````````
//
//  Wheel mouse support.
//
#ifndef WINCE
    case WM_MOUSEWHEEL:
         static int delta;                                                  // Accumulation point for deltas.
         shift  = SystemParametersInfo(SPI_GETWHEELSCROLLLINES,0,&ctrl,0);  // Scroll amount.
         ctrl   = LOWORD(wParam);                                           // Keys
         delta += (short) HIWORD(wParam);                                   // Delta
         i      = 0;                                                        // Default shift.
//
//  Shift -- skip pages.
//
         if ((ctrl & MK_SHIFT) || (shift == WHEEL_PAGESCROLL)) {
           while (delta >= WHEEL_DELTA) {
             i      = 1-lines;
             delta -= WHEEL_DELTA;
           }
           while (abs(delta) >= WHEEL_DELTA) {
             i      = lines-1;
             delta += WHEEL_DELTA;
           }
         }
//
//  Normal -- skip lines.
//
         else {
           while (delta >= WHEEL_DELTA) {
             i     = -shift;
             delta -= WHEEL_DELTA;
           }
           while (abs(delta) >= WHEEL_DELTA) {
             i     =  shift;
             delta += WHEEL_DELTA;
           }
         }
         j = top;
         top += i;
         if (top > BOTTOM) top = BOTTOM;
         if (top < 0) top = 0;
         if (j == top) return (0);
         scroll ();
         redraw ();
         return (0);
#endif  WINCE
//```````````````````````````````````````````````````````````````````
//
//  Scroll bar messages.
//
    case WM_VSCROLL:
         switch (LOWORD(wParam)) {
           case SB_LINEUP:
                i = -1;
                break;
           case SB_LINEDOWN:
                i = 1;
                break;
           case SB_PAGEUP:
                i = 1-lines;
                break;
           case SB_PAGEDOWN:
                i = lines-1;
                break;
           case SB_THUMBTRACK:
           case SB_THUMBPOSITION:
                GetScrollInfo (window,SB_VERT,&scroll_info);
                i = scroll_info.nTrackPos-top;
                break;
           default:
                return (0);
         }
         j = top;
         top += i;
         if (top > BOTTOM) top = BOTTOM;
         if (top < 0) top = 0;
         if (j == top) return (0);
         scroll ();
         redraw ();
         return (0);
//```````````````````````````````````````````````````````````````````
//
//  Menu messages.  
//
//  These are associated witht he pop-up menu, and the commands that 
//  can be invoked this way.
//
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           case IDM_LIST_INSERTTOFILE:          // Standard insert to file
                insert (false);
                return (0);
           case IDM_LIST_REPLACETOFILE:         // Repalce to file.
                file_list.get(exclude)->selection_delete ();
                insert (false);
                return (0);
           case IDM_LIST_INSERTTONEWFILE: {      // Insert to a new file
                  JWP_file *tfile;
                  tfile = jwp_file;            
                  last_insert = new JWP_file(NULL,FILETYPE_UNNAMED);
                  insert (true,last_insert);
                  tfile->activate ();
                }
                return (0);
           case IDM_LIST_INSERTTO:              // Insert to last file inserted to.
                insert (true,last_insert);
                jwp_file->title();              // If the insrt file changed state changed the title will be redrawn.
                return (0);
           case IDM_LIST_INSERTTOANYFILE: {     // Insert to any file
                  JWP_file *tfile;
                  if (!(tfile = choose_file(hwnd,0,IDH_INTERFACE_JLIST))) return (0);
                  insert (true,last_insert = tfile);
                }
                jwp_file->title();              // If the insrt file changed state changed the title will be redrawn.
                return (0);
           case IDM_LIST_GETINFO:               // Get character information.
                kanji_info (hwnd,last_char);
                return     (0);
           case IDM_LIST_COPY:                  // Copy to clipboard.
                clip_copy ();
                break;
           case IDM_EDIT_SEARCH:
                jwp_search.do_search (this);
                break;
           case IDM_EDIT_FINDNEXT:
                jwp_search.do_next (this);
                break;
           case IDM_EDIT_SELECTALL:
                goto SelectAll;
           default:
                break;

         }
         break;
  }
  return (DefWindowProc(hwnd,msg,wParam,lParam));
}

//
//  End Class JWP_list
//
//===================================================================

//===================================================================
//
//  Begin class JWP_history
//
//  This class maintains a history buffer for a given Japanese edit control.  A basic history 
//  consists of a buffer used to store strings and an array of pointers.  The user allocates a 
//  fixed size block for pointers and data.  10% of the block is used for pointers and the rest
//  is used for line storage. 
//
//  The pointers are indexes into the storage buffer.  The length of an entry is obtained by sutracting
//  the difference between this pointer and the next pointer up.  This means we always allocate an
//  extra pointer.  This last pointer always points to the first unused buffer in the array.
//

#define BUFFER(x)                       (buffer+pointers[x])                            // Pointer to buffer for this index.
#define LENGTH(x)                       (pointers[x+1]-pointers[x])                     // Length of an entry by index.
#define KANJIMOVE(dest,source,length)   memmove(dest,source,(length)*(sizeof(KANJI)))   // Move a array of kanji.

//--------------------------------
//
//  Stub routine for the history dialog.
//
static int dialog_history (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  JWP_history *hist;
  if (message == WM_INITDIALOG) SetWindowLong (hwnd,GWL_USERDATA,lParam);
  hist = (JWP_history *) GetWindowLong(hwnd,GWL_USERDATA);
  return (hist->dlg_history(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  Class constructor.
//
JWP_history::JWP_history (int id) {
  memset (this,0,sizeof(class JWP_history));
  text_id = id;
  return;
}

//--------------------------------
//
//  Class destructor
//
JWP_history::~JWP_history () {
  alloc (0);
  return;
}

//--------------------------------
//
//  Add a line to the history pointer.
//
//  Note, we use a smart-add.  If the energy is already in the history, it is not placed in the history again, but
//  rather the previous entry is moved to the top of the history.  This allows more elements to be stored in the 
//  same history space.
//
//      string -- String to be added.
//      length -- Length of the string.
//
void JWP_history::add (KANJI *string,int length) {
  int i;
  if  (!buffer || !length) return;                                          // No string so don't do anyting.
  if  (length > size) length = size-1;                                      // Handle case when user sends us a very long string.
  i = find(string,length);                                                  // Is this in the list already.
  if      (i ==  0) return;                                                 // first entry
  else if (i != -1) remove (i);                                             // Some other entry.
  while ((count == ptrmax) || (pointers[count]+length >= size)) count--;    // Create space for the new string.
  KANJIMOVE (buffer+length,buffer,size-length);                             // Move the old data.
  for (i = ptrmax; i > 0; i--) pointers[i] = pointers[i-1]+length;
  KANJIMOVE (buffer,string,length);                                         // Copy the new data.
  count++;
  return;
}

//--------------------------------
//
//  Change the size of the allocated buffer.
//
//      newsize -- New buffer size in number of kanji characters.
//
//      RETURN  -- A non-zero value indicates an allocation error.
//
int JWP_history::alloc (int newsize) {
  int    i;
  KANJI *newbuf;
  short *newptr;
  if (size == newsize) return (false);                                      // Same size as before so do nothing.
  if (!newsize) {                                                           // Used for the destructor.
    newbuf      = NULL;
    newptr      = NULL;
    count       = 0;
    ptrmax      = 0;
  }
//
//  Going to allocate a new buffer.   We will divide the buffer into a pointer space (about 10%) and a data space (about 90%).  
//  We will move over as many of the old pointers and data as we could in the past.
//
  else {
    if (!(newptr = (short *) calloc(newsize,sizeof(KANJI)))) return (true);
    ptrmax  = (newsize/10)+1;                                   // Size of pointer space.
    count   = min(count,ptrmax);                                // Is this smaller than previous
    newbuf  = (KANJI *) (newptr+ptrmax+1);                      // Find buffer space.
    newsize = newsize-ptrmax-1;                                 // Correct size.
    i       = min(size,newsize);                                // Move as much of the buffer as possible.
    if (pointers) {
      KANJIMOVE (newptr,pointers,count+1);                                    
      KANJIMOVE (newbuf,buffer,i);
    }
    for (i = 0; (i < count) && (pointers[i+1] < newsize); i++); // Correct the count.
    count = i;
  }
  if (pointers) free (pointers);
  pointers = newptr;
  buffer   = newbuf;
  size     = newsize;
  return (false);
}

//--------------------------------
//
//  Dialog box handler for the listory list.  Not really much to do.
//
int JWP_history::dlg_history (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  int        i;
  JWP_list  *list;
  switch (message) {
    case WM_INITDIALOG:
         hist_size.wm_init (hwnd,IDC_HLLIST,NULL,true,0,0);
         SetWindowText (hwnd,get_string(text_id));
         list = (JWP_list *) SendDlgItemMessage(hwnd,IDC_HLLIST,JL_GETJWPLIST,0,0);
         for (i = 0; i < count; i++) list->add_line (LENGTH(i),BUFFER(i));
         SetFocus (GetDlgItem(hwnd,IDC_HLLIST));
         return (false);
    case WM_HELP:
         do_help (hwnd,IDH_INTERFACE_JEDIT);
         return  (true);
#ifndef WINCE
    case WM_SIZING:
         hist_size.wm_sizing ((RECT *) lParam);
         return (0);
#endif  WINCE
    case WM_SIZE:
         hist_size.wm_size (wParam);
         return (0);
    case WM_COMMAND:
         switch (LOWORD(wParam)) { 
           case IDC_HLLIST:
           case IDOK:
                list = (JWP_list *) SendDlgItemMessage(hwnd,IDC_HLLIST,JL_GETJWPLIST,0,0);
                EndDialog (hwnd,list->current);
                return    (0);
           case IDCANCEL:
                EndDialog (hwnd,-1);
                return    (0);
         }
  }
  return (false);
}

//--------------------------------
//
//  This routine handles pressing the donw button in the history buffer.  This has the effect of scrolling forward
//  If at the end of the list a blank line will be generated.  If you pres this again, the list window will be open.
//
//      file -- Should point to JWP_file class for the Japanese edit constrol.
//
void JWP_history::down (JWP_file *file) {
  if (!buffer) return;
  if (file->changed) { list (file); return; }
  last--;
  if (last >= 0) file->edit_set (BUFFER(last),LENGTH(last));
    else {
      file->edit_set (buffer,0);
      file->change   ();
    }
  return;
}

//--------------------------------
//
//  Find a specific entry in the buffer.
//
//      string -- String to be added.
//      length -- Length of the string.
//
//      RETURN -- Index of mathc (requries exact match), or -1 if it cannot be found.
//
int JWP_history::find (KANJI *string,int length) {
  int i,j;
  if (!length) return (-1);
  for (i = 0; i < count; i++) {
    for (j = 0; (j < LENGTH(i)) && (string[j] == BUFFER(i)[j]); j++);
    if (j == LENGTH(i)) return (i);
  }
  return (-1);
}

//--------------------------------
//
//  Main entry point for generating the history list.
//
//      file -- Pointer to the JWP_file class object for the Japanese edit control.
//  
void JWP_history::list (JWP_file *file) {
  int i;
  i = JDialogBox (IDD_HISTORY,file->window,(DLGPROC) dialog_history,(LPARAM) this);
  if (i != -1) {
    file->edit_set (BUFFER(i),LENGTH(i));
    file->change   ();
  }
  return;
} 

//--------------------------------
//
//  Read the history from a file.  The history is written into the file as first and integer.  This is a the 
//  count field.  Then the entire buffer is written.
//
//      hfile  -- File to read from.
//
//      RETURN -- Non-zero value indicates and error.  Not used.
//
int JWP_history::read (HANDLE hfile) {
  unsigned long done;
  alloc (jwp_config.cfg.history_size);
  ReadFile (hfile,&count,sizeof(int),&done,NULL);
  if (pointers) ReadFile (hfile,pointers,jwp_config.cfg.history_size*sizeof(KANJI),&done,NULL);
    else SetFilePointer (hfile,jwp_config.cfg.history_size*sizeof(KANJI),NULL,FILE_CURRENT);
  return (false);
}

//--------------------------------
//
//  Remove an entry by index number.
//
//      index -- Index to be removed.
//
void JWP_history::remove (int index) {
  int i,length;
  length = LENGTH(index);
  KANJIMOVE (BUFFER(index),BUFFER(index+1),size-pointers[index+1]);
  for (i = index; i < count; i++) pointers[i] = pointers[i+1]-length;
  count--;
  return;
}

//--------------------------------
//
//  This routine handles pressing the up button in the history buffer.  This has the effect of scrolling backward
//  through the history until the last item in the buffer is found.
//
//      file -- Should point to JWP_file class for the Japanese edit constrol.
//
void JWP_history::up (JWP_file *file) {
  if (!buffer) return;                                                      // No history so exit.
  if      (!file->changed) last++;                                          // Line not changed so move back one.
  else if (0 == find(file->edit_gettext(),file->edit_getlen())) last = 1;   // Current line is the same as top of buffer, so skip back one.
  else {                                                                    // Push this line.
    add (file->edit_gettext(),file->edit_getlen());
    last = 0; 
  }
  if (last >= count) last = count-1;                                        // Copy history to edit control.
  file->edit_set (BUFFER(last),LENGTH(last));
  return;
}

//--------------------------------
//
//  Write the history to the file.  The history is written into the file as first and integer.  This is a the 
//  count field.  Then the entire buffer is written.
//
//      hfile  -- File to read from.
//
//      RETURN -- Non-zero value indicates and error.  Not used.
//
int JWP_history::write (HANDLE hfile) {
  unsigned long done,zero = 0;
  int i;
  WriteFile (hfile,&count,sizeof(int),&done,NULL);
  if (pointers) WriteFile (hfile,pointers,jwp_config.cfg.history_size*sizeof(KANJI),&done,NULL);
    else {
      for (i = 0; i < jwp_config.cfg.history_size; i++) WriteFile (hfile,&zero,2,&done,NULL);
    }
  return (false);
}

//
//  End class JWP_history
//
//===================================================================

//===================================================================
//
//  Exported routines
//

//--------------------------------
//
//  This small routine registers the window class used for the japanese
//  edit-box procedure, and Japanese list box procedures.
//
int initialize_edit (WNDCLASS *wclass) {
  wclass->hbrBackground = (HBRUSH) (COLOR_WINDOW+1);
  wclass->style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wclass->lpfnWndProc   = JWP_edit_proc;
  wclass->lpszClassName = TEXT("JWP-Edit");
  if (!RegisterClass(wclass)) return (true);
  wclass->style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wclass->lpfnWndProc   = JWP_page_proc;        // Special Japanese edit control used in prop-pages.
  wclass->lpszClassName = TEXT("JWP-Page");
  if (!RegisterClass(wclass)) return (true);
  wclass->style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wclass->lpfnWndProc   = JWP_list_proc;
  wclass->lpszClassName = TEXT("JWP-List");
  if (!RegisterClass(wclass)) return (true);
  return (false);
}






