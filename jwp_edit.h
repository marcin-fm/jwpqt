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

#ifndef jwp_edit_h
#define jwp_edit_h
#include "jwp_file.h"

//===================================================================
//
//  Edge of controls dimensions
//
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  #define WIN_XEDGE     1
  #define WIN_YEDGE     1
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  #define WIN_XEDGE     GetSystemMetrics(SM_CXEDGE)
  #define WIN_YEDGE     GetSystemMetrics(SM_CYEDGE)
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))

//===================================================================
//
//  Japanese edit control messages and macros.
//

#define JE_SETTEXT      WMU_SETTEXT             // Set the text in the message box.
#define JE_GETTEXT      WMU_GETTEXT             // Get the text from the message box (USE THE MACRO)
#define JE_GETJWPFILE   WMU_GETWINDOWVALUE      // Get the JWP_file object associated with an edit control
#define JE_LOAD         WMU_LOADCONTROL         // Load the edit control from the indicated file (reads selected text).
#define JE_SETHIST      WMU_SETHISTORY          // Set the history value for this edit control.
#define JE_HISTORYLIST  WMU_HISTORYLIST         // Generate a history list for this edit control.

//--------------------------------
//
//  Inline function to get text from a message box, allos type checking 
//  and other nice things to work well.
//
//      hwnd   -- Dialog box window.
//      id     -- Edit box id.
//      kanji  -- On exit this pointer points to the edit boxes text.
//
//      RETURN -- Return value is the length of the text buffer.
//
int inline JEGetDlgItemText  (HWND hwnd,int id,KANJI **kanji) { 
#if 0
  JWP_file *file;
  file   = (JWP_file *) SendDlgItemMessage(hwnd,id,JE_GETJWPFILE,0,0);
  *kanji = file->edit_gettext();
  return (file->edit_getlen());
#else
  return(SendDlgItemMessage(hwnd,id,JE_GETTEXT ,0,(LPARAM) kanji)); 
#endif
}

//===================================================================
//
//  begin class JWP_list
//
//  This class implements a Japanese list box control with lots of 
//  trimmings.
//

#define JL_RESET        WMU_RESETLIST           // Clear contents of the list box.
#define JL_SETSEL       WMU_SETSELECT           // Set current selection.
#define JL_GETTEXT      WMU_GETTEXT             // Get text and length of a viven line.
#define JL_GETBEGIN     WMU_JLBC_BEGIN          // Get beginning of extended block containning a specific line.
#define JL_GETJWPLIST   WMU_GETWINDOWVALUE      // Returns the JWP_list object associated witht the list.
#define JL_INSERTTOFILE WMU_JLBC_INSERT         // Cause an insert to a specific file.
#define JL_SETEXCLUDE   WMU_SETEXCLUDE          // Set exclusion file

int inline JL_GetText  (HWND hwnd,int id,int line,KANJI **kanji) { return(SendDlgItemMessage(hwnd,id,JL_GETTEXT,line,(LPARAM) kanji)); }
int inline JL_GetBegin (HWND hwnd,int id,KANJI **kanji) { return(SendDlgItemMessage(hwnd,id,JL_GETBEGIN,0,(LPARAM) kanji)); }

//--------------------------------
//
//  This class describes a single line of tghe list
//
typedef class LIST_line {
public:
  void   clear (void);                  // Disposes of all memory associated with this item, and clears flags
  void   alloc (int len,KANJI *line);   // Allocates data for the list, based on an argument.
  short  selected;                      // Non-zero indicates the item is selected.
  short  length;                        // Length of the item.
  KANJI *text;                          // Actuall data.
} LIST_line;

//--------------------------------
//
//  This class describes a data element from a Japanese list control.
//
typedef class JWP_list {
public:
  int         count;            // Number of lines in the list.
  int         width;            // Width of the list.
  int         current;          // Current element in the list.
  int         single;           // Single-select managed list.
  int         xmax;             // List wrap point.  Was in EUC_buffer, but is here for resaizable lists.
//
//  Constructor/destructor
//
  JWP_list  (HWND hwnd);
  ~JWP_list (void);
  HWND inline parent (void) { return (GetParent(window)); }  // Get out parent
//
//  Block manimpulation commands
//
  int         del_block     (int line);                     // Delete a block
  int         begin         (int line);                     // Find beginning of this block
  int         move_block    (int from,int to);              // Move a block.
  int         next          (int line);                     // Find next block
  void        find          (int search_length);            // Search for string in the list.
//
//  General manipulation commands
//
  void        add_line      (int len,KANJI *text);          // Put a line into the file.
  int         get_buffer    (KANJI *buffer,int index);      // Extract an extended data object.
  int         get_text      (int line,KANJI **text);        // Get a line's text and length.
  void        insert        (int newline,JWP_file *file = NULL);    // Insert selected text into a JWP_file structure.
  void        move          (int pos,int shift);            // Move the cursor.
  void        sort          (int (*proc)(KANJI *buf1,KANJI *buf2));
  int         win_proc      (HWND hwnd,int msg,WPARAM wParam,LPARAM lParam);  // Window procedure for the dialog
private: 
  void        adjust        (void);                         // Adjust the list box after the size has changed.
  void        clip_copy     (void);                         // Copy selected items to the clipboard.
  void        del_line      (int line);                     // Delete a line.
  void        draw_line     (HDC hdc,int line);             // Render a specific line
  LIST_line  *get_line      (int line);                     // Find data associated with line
  int         get_char      (LPARAM lParam,int *pos,int average);    // Find character under the cursor.
  void        popup_menu    (int x,int y);                  // Do popup menu.
  void        que_line      (int line);                     // Que a line to be redrawn
  void        reset         (void);                         // Clear contents fo the list
  void inline redraw        (void) { InvalidateRect (window,NULL,false); }
  void        select        (int line,int onoff);           // Set selected state or clear it.
  void        scroll        (void);                         // Set scroll bar.

  int         alloc;                // Number of lines allocated for the list.
  int         focus;                // List has the focus
  int         height;               // Height of a line in the list in pixels
  int         in_select;            // Non-zero when user is selecting with the mouse.
  int         last_x;               // Last x position selected, used to supress extra redraws.
  int         last_y;               // Last line selected for mouse selection (to suppress flashing).
  int         lines;                // Visible lines in the display window.
  int         sel_fixed;            // Fixed point for selecting text (this is a line number).
  int         sel_pos1,sel_pos2;    // Cursor location for single line select.
  int         sel_x1,sel_x2;        // Pixal location of single line selection (sel_x1 = 0 disables selection).
  int         sel_posf,sel_xf;      // Fixed point (cursor/pixal) for selected text (this is the first mouse click location of a drag).
  int         select_count;         // Number of selected lines.
  int         selecting;            // Indicates the process of selecting text
  int         top;                  // Top element in the list.
  KANJI       last_char;            // Character under last mouse click (used for character info).
  HWND        window;               // Window containning the actual list.
  JWP_file   *exclude;              // Exclude this file from insert operations.
  struct LIST_list *lists,*last;    // List and pointer to last list.
} JWP_list;

//
//  End Class JWP_list
//
//===================================================================

//===================================================================
//
//  begin class EUC_buffer
//
//  This class implements a simple buffer used to break dictionary 
//  strings into lines, for the display.  
//
#define EUC_HIGHLIGHT   0x01    // Special character for highlight in in list box.  This is
                                //   big KLUDGE, see jwp_edit.cpp for details.  This basically
                                //   indicates that this line is to be in the highlite color.

typedef class EUC_buffer {
public:
  short     x;                                  // Current horizontal position of vertual cursor.
//  short     xmax;                               // Maximum acceptable horizontal cursor position.
  HWND      window;                             // Window of list box control to write the string to.
  JWP_list *list;                               // Pointer to JWP_list object
  void  clear      (void);                      // Clears the state of the buffer.
  void  flush      (int pos);                   // Writes the contents of the buffer, up to a specific character.  The rest of the characters are readded to the buffer to form a new line.
  void  inline highlight (int x) {hilight = x;} // Set highlight value.
  void  initialize (HWND hwnd);                 // Initialize the buffer.
  void  put_char   (int ch);                    // Put a character to the buffer.
  void  put_kanji  (KANJI *kanji,int length);   // Put kanji string to buffer.
  void  put_string (tchar *text);               // Put a string to the buffer.
  void  put_label  (int id);                    // Put a marker label into the list.
private:
  KANJI buffer[256];                            // Buffer to hold the string being built.
  short count;                                  // Current position in the buffer.
  short hilight;                                // Set to non-zero for highlighted entry.
} EUC_buffer;

//
//  End Class EUC_buffer.
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

//--------------------------------
//
//  This class describes a data element from a Japanese list control.
//
//                                    
//  Dialog box IDs for edit-lists.
//
//      IDC_EDITLIST        List box
//      IDC_EDITLISTADD     Add button 
//      IDC_EDITLISTEDIT    Edit button.
//      IDC_EDITLISTDELETE  Delete button.
//      IDC_EDITLISTUP      Up button.
//      IDC_EDITLISTDOWN    Down button.
//      IDC_EDITLISTIMPORT  Import button.
//      IDC_EDITLISTINSERT  Insert button.
//

typedef class EDIT_list : public EUC_buffer {
public:
  HWND      dialog;             // Pointer to our dialog box.
  int       length;             // Length of the kbuffer (used, not size).
  short     line_break;         // Line break used in formatting lines.
  short     changed;            // Indicates if the list is changed.
  JWP_file *file;               // Where we got called from.  
  KANJI kbuffer[SIZE_BUFFER];   // Kanji buffer for transfering the contents to the edit dialog.

  int  inline  count        (void)            { return (list->count); }
  int  inline  get_buffer   (int index)       { return (length = list->get_buffer(kbuffer,index)); }
  int  inline  next_item    (int index)       { return (list->next(index)); }
  void         do_drop      (HDROP drop);				                            // Process drag and drop import list.
  void         do_event     (int event);                                            // Event handler for the buttons.
  void         error        (int format,...);                                       // Generate  an error message.
  void         init         (HWND hwnd,byte *data,int import);                      // Initializes the instance of the class.
  void virtual put_data     (byte *data,tchar *name) = 0;                           // Puts data into the structure (used in startup and import).
  long         size         (void);                                                 // Esitmates the size of the entire contents of the list.
private:
  TCHAR        import[64];                                                          // Import string for specific file type.
  void         import_file  (tchar *name);                                          // Actually import a file.
  int  virtual edit         (void) = 0;                                             // Virtual function called to add/edit a logical item.
  int  inline  move_item    (int from,int to) { return (list->move_block(from,to)); }
  int  inline  begin_item   (int index)       { return (list->begin(index)); }
  int  inline  delete_item  (int index)       { return (list->del_block(index)); }
  void inline  move         (int index)       { list->move (index,false); }
} EDIT_list;

//
//  End Class EDIT_list.
//
//===================================================================

//===================================================================
//
//  JWP_history clasz
//

#define HISTORY_SIZE    (jwp_config.cfg.history_size*sizeof(KANJI)+sizeof(int))     // Size of a single history buffer written to a file.

//--------------------------------
//
//  This class maintains a history buffer for an edit control.
//
//  The actual class implementations are in the jwp_find.cpp and jwp_dict.cpp routines.
//  The class is defined here, because it is accessed only from the edit control.
//
typedef class JWP_history {
public:
  JWP_history      (int newid);
  ~JWP_history     (void);
  void add         (KANJI *string,int length);                              // Add line to history
  int  alloc       (int newsize);                                           // Allocate history buffer.
  int  dlg_history (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);    // Dilaog box hander for history list.
  void down        (JWP_file *file);                                        // Handles the down key-stroke
  void list        (JWP_file *file);                                        // Creates history list.
  int  read        (HANDLE hfile);                                          // Writes history to a file.
  void up          (JWP_file *file);                                        // Handles the up key-stroke
  int  write       (HANDLE hfile);                                          // Writes the history to a file.
private:
  KANJI *buffer;                            // Pointer to storage buffer.
  int    count;                             // Number of elements in the history.
  int    last;                              // Last line sent to the user.
  short *pointers;                          // Array of pointers (pointers[count] indicates last used buffer position.
  int    ptrmax;                            // Maximum number of pointers.
  int    size;                              // Size of buffer space in charactrs.
  int    text_id;                           // Text ID used for title of list box.
  int    find   (KANJI *string,int length); // Search for an element in the history./
  void   remove (int index);                // Remvoe an entry.
} JWP_history;

//
//  JWP_history clasz
//
//===================================================================

extern int  initialize_edit (WNDCLASS *wclass);         // Intialize the edit class


#endif jwp_edit_h



