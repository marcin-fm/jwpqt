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
#ifndef jwp_file_h
#define jwp_file_h

#include "jwp_jisc.h"
#include "jwp_misc.h"
#include "jwp_para.h"
#include "jwp_prnt.h"

//===================================================================
//
//  class Position
//
//  Class defines a position within a file and a number of routines
//  that can operate on the class.  Positions are used for the cursor,
//  Upper left corner of the display (view_top), and selected regions
//  of the text.
//
typedef class Position {                        // Position wintin the file
friend class JWP_file;
friend class Paragraph;
friend class JWP_search;
private:
  class  Paragraph *para;                       // Pointer to paragraph
  struct Line      *line;                       // Pointer to line
  short             pos;                        // Offset into line
  long              x,y;                        // Absolute offests of position.
  void inline abs       (void) { pos += line->first; line = para->first; }  // Convert to absolute position.
  int  inline abs_pos   (void) { return (pos+line->first); }                // Get absolute cursor position    
  void        advance   (int count);            // Move cursor forward or backward by count.
  void        align     (JWP_file *file,int x_pos,int mouse = FALSE);  // Align cursor to a specific pixal location.
  int         move_up   (void);                 // Move cursor up through the file.
  int         move_down (void);                 // Move cursor down through the file.
  void        rel       (void);                 // Convert to relative position.
                                                // Inline functions get_char() get character at cursor.  Others 
                                                //   return locational flags, b=begin, e=end, l=line, p=paragraph, f=file
  int  inline get_char  (void) { return ((pos >= para->length) ? 0 : para->text[line->first+pos]); }
  int  inline bof       (void) { return (!pos && (line == para->first) && !para->prev); }
  int  inline eof       (void) { return ((pos >= line->length) && (line == para->last) && !para->next); }
  int  inline bol       (void) { return (!pos); }
  int  inline eol       (void) { return (pos >= line->length); }
  int  inline bop       (void) { return (!line->prev && !pos); }
  int  inline eop       (void) { return (!line->next && (pos >= line->length)); }
} Position;

#include "jwp_undo.h"

//===================================================================
//
//  struct Selection.
//
//  Defines the variables associated with the slected region.  This 
//  did not actually have to be separated, but it makes is easier to 
//  tell what is what.
//
                                // Selection types
#define SELECT_NONE     0       // No selection
#define SELECT_EDIT     1       // Standard window edit selection.
#define SELECT_KANJI    2       // Kanji conversion selection.
#define SELECT_CONVERT  3       // Conversion in process.

                                // Selection fixed codes.
#define SELECT_FIX1     0       // Fixed point is pos1
#define SELECT_FIX2     1       // Fixed point is pos2

typedef struct Selection {      // Information for a selection.
  class Position pos1,pos2;     // Positions for selection
  byte           type;          // Type marker for selection
  byte           fixed;         // Indicates which point of the mark is fixed.
} Selection;

//===================================================================
//
//  Begin Class JWP_view.
//
//  Class defines a basic file fiew window.  This is generally associated
//  with a list of JWP_file class, whcih contain the actual files being
//  editied.
//

//===================================================================
//
//  Begin Class JWP_File.
//
//  This is the basic class that describes a file and all of it's parts.
//
                                // Size of some arrays.
#define NUMBER_SUMMARY      5   // Number of lines in the summary.
#define NUMBER_HEADERS      4   // Number of different headers.
#define NUMBER_POSITIONS    3   // Number of positions per header.

#define SUMMARY_TITLE       0   // Summary names.
#define SUMMARY_SUBJECT     1
#define SUMMARY_AUTHOR      2
#define SUMMARY_KEYWORDS    3
#define SUMMARY_COMMENT     4
                                // Flags for JWP_file::find_pos()
#define POS_CURSOR          0   // Standard cursor position
#define POS_VIEW            1   // View point position
#define POS_VERT            2   // Cursor vertical only position
#define POS_VVERT           3   // View point position vertical only

                                // Character codes for JWP_file::put_char()
#define CHAR_CONT           0   // Continue current state.
#define CHAR_START          1   // Start kana->kanji conversion
#define CHAR_STOP           2   // Stop kana->kanji conversion

                                // File reading errors
#define FILEERR_OK          0   // No errror
#define FILEERR_FATAL       1   // Fatal file error, cannot be recovered from.
#define FILEERR_ERROR       2   // File has an error but is recoverable

#define SEL_MOUSE           2   // Special flag indicating selection is with a mouse.

typedef class JWP_file {
friend class Position;
friend class Paragraph;
friend int close_file (int exit_ok);
friend class JWP_search;
friend class UNDO_type;
friend class UNDO_newpara;
friend class UNDO_delpara;
public:
  class  JWP_file  *next,*prev;                         // Links to other files
//JWP_file (int format_width);                          // Defined in clipboard support functions
//JWP_file (HWND hwnd);                                 // Defined in edit-box support routines.
//JWP_file (tchar *name,int type);                      // Defined in file io support rotuines
 ~JWP_file (void);
  HWND  window;
  void        do_key         (int key,int ctrl,int shift);  //Process key. 
  void        do_mouse       (int iMsg,WPARAM wParam,LPARAM lParam);    // Process mouse.
  void        do_menu        (int wParam);              // Execute a memue command
  void        popup_menu     (int x,int y);             // Generate a popup menu.
  void        draw_all       (HDC hdc,RECT *bound);     // Main draw routine.
  void        h_scroll       (int message);             // Process horizontal scroll messages.
  void        v_scroll       (int message);             // Process vertical scroll messages       
  void        left           (void);                    // Move sursor to the left witht or whitout shift.
  void        right          (void);                    // Move cursor to the right with or whithout shift.
  byte        filetype;                                 // File type (disk type).
//
//  Formatting commands.
//
  void        adjust         (void);                    // Adjust file for major changes.
  void inline caret_off      (void) { DestroyCaret(); } // Disable the display caret.
  void        caret_on       (void);                    // Enable the display caret.
  void        change         (void);                    // Mark file as changed
  void        redraw_all     (void);                    // Redraw entire file.
  void        redraw_from    (Paragraph *para,Line *line);          // Redraw from this paragraph forward
  void        redraw_para    (Paragraph *para,Line *line);          // Redraw this paragraph
  void        redraw_range   (int pos1,Paragraph *para2,int pos2);  // Redraw range.
  void        reformat       (void);                    // Reforamt the file.
  void        title          (void);                    // Set current display title.
  void        sysname        (int id);                  // Used to name special files that belong to the editor.
//
//  Routines for support of the the history buffer.
//
public:
  class JWP_history *history;                           // Pointer to the history buffer if there is one.
//
//  Undo/redo manipulation rotuines.
//
public:
  void        do_undo        (void);                    // Implements the Edit/Undo command.
  void        do_redo        (void);                    // Implements the Edit/Redo command.
  void        undo_end       (void);                    // End an undo chain.
  void        undo_init      (int old_length);          // Allocates or reallocates an undo buffer for this file
  void        undo_para      (int id,Paragraph *para = NULL);   // Key routine that saves changes to a single paragraph.
  void        undo_start     (void);                    // Start an undo chain.
  void        undo_undo      (UNDO_para *para);         // Fake rotuine used when restoring data from 
private:
  byte        undo_accum;                               // Non-zero means that undo is accumulating characters
  UNDO_action **undo;                                   // List of undo lists.
  Paragraph  *number_to_para (int number);              // Convert number to paragraph.
  int         para_to_number (Paragraph *para);         // Convert paragraph to a number.
  void        redo_clear     (void);                    // Clear the redo buffer.
  void        undo_clear     (int code = UNDO_NONE);    // Clear state of accumulated undo if state does not match code.
  void        undo_delpara   (Paragraph *para);         // Add a paragraph deletion undo action to the current chain.
  void        undo_menu      (void);                    // Update the state of the Edit/Undo and Edit/Redo menu items.
  void        undo_new       (void);                    // Generates a new undo chain.
  void        undo_newpara   (Paragraph *para);         // Add a paragraph additon undo action to the current chain.
  void        undo_pop       (UNDO_action **list);      // Undo an action and pop it from a list.
  void        undo_push      (UNDO_action *action);     // Push and undo action into the current chain.
  void        undo_type      (void);                    // Save undo information for type/name change.
//
//  Paragraph manipulation rotuines.
//
public:
  int do_formatpara (HWND hwnd,UINT messag,int wParam); // Process formmat paragraph dialog box.
private:
  void        set_page       (Paragraph *para,int page);// Make a paragraph as a page break.
  void        del_paragraph  (Paragraph *para);         // Delete a paragraph.
  int         new_paragraph  (Paragraph *para);         // Generate a new paragraph after the indicated one.
//
//  Entry points for ascii->kana converter and kana->kanji converter.
//
public:
  void        do_char        (int ch);                  // Process keys
  void        put_char       (int ch,int code);         // Put character into file from ascii->kana convert.
  void        ime_char       (int ch,int unicode);      // Inserts a character from the IME.
//
//  Support for Color-kanji (located in jwp_font.cpp)
//
public:
  void        kanjicount     (void);                    // Does the count part of a kanji count.
  void        do_kanjilist   (void);                    // Generate a kanji list from the current file.
//
//  File IO support routines.
//
public:
  JWP_file (tchar *name,int type,int recent=true);      // Generall file constructor used for unnamed and named files
  void        activate       (void);                    // Activate this file.
  int         close          (int exit_ok);             // Close this file.
  void        delete_file    (void);                    // Delete disk file associated with the current file.
  long        export_file    (JIS_convert *cvrt);       // Write file to any non-JWP format
  void        revert         (void);                    // Revert to last loaded file.
  int         save           (tchar *name);             // Save file.
  int         save_as        (void);                    // Save with requestor
  long        write_jwp_file (IO_cache *cache);         // Write JWP file.
private:
  int         import_file    (JIS_convert *cvrt);       // Import file from any non-JWP format
  void        page_setup     (void);                    // Page setup dialog
  void        print          (int setup);               // Print and print-setup.
  void        print_heads    (HDC hdc,int head,int y,PrintContext *pc,int *widths,int page);
  int         project_save   (tchar *name);             // Save the project
  void        project_read   (tchar *name);             // Restore the project
  int         read_jwp_file  (IO_cache *cache);         // Read JWP file.
//
//  Support routine for SAVECHECK dialog box.
//
public:
  TCHAR inline *get_name     (void) { return (name); }              // Get the file name
//
//  Support routine for DIALOG BOXES in general.
//
public:
  void          put_string   (KANJI *kanji,int length);             // Used by dialog boxes, to insert back into file.
  void          insert_string(KANJI *kanji,int length);             // Insert a string from a dialog box into a file.with undo
//
//  These fucntions are here to support edit control boxes.
//
public:
  JWP_file (HWND hwnd);                                             // Constructor used for JWP_file object at the heart of the edit control.
  int           edit_copy    (JWP_file *file);                      // Initalize an edit control from a JWP_file.
  void  inline  edit_set     (KANJI *text,int length);              // Set edit control text.
  int   inline  edit_getlen  (void) { return (first->length); }     // Get length of edit control
  KANJI inline *edit_gettext (void) { return (first->text); }       // Get edit control text.
  int           edit_clip    (void);                                // Replace edit control with clipboard contents
//
//  Support routines for clipboard functions
//
public:
  JWP_file (int format_width);                          // This construct is really for the clipboard functions.
  JWP_file   *clip_copy        (void);                  // Copy slected region to clipboard.
  HGLOBAL     export_bitmap    (void);                  // Export clipboard data to bitmap.
  int         import_clip      (void);                  // Read clipboard data into this file.
private:
  void        clip_paste       (int errors);            // Paste from clipboard.
  void        clip_cut         (void);                  // Cut text to clipboard.
//
//  Support for kana->kanji conversion routines.
//
public:
  void        put_kanji        (KANJI *kanji,int length);   // Put kanji into file from kana->kanji convert.
  void        convert          (int direction);         // Dirve conversion commands.
//
//  Selection controls.
//
public:
  void        selection_clear  (void);                  // Clear the selection.
  void        selection_delete (void);                  // Delete the selection.
  struct Selection  sel;                                // Selection for this file
private:
  int         in_selection     (Position *loc);         // Tests if a position in in the selection.
  void        selection        (int shift);             // General processing routine for making selection my shift-cursor motion.

public:
  void        view_check       (void);                  // Adjust horizontal/vertical scroll to show cursor.
  void        edit_menu        (void);                  // Dtermines the state of elements of the edit-menu.
  byte        changed;                                  // File is changed.
private:
  void        all_abs          (void);                  // Convert all points to abs.
  void        all_rel          (void);                  // Convert all points to relative.
  void inline clear_cursor     (void) { x_cursor = 0; } // Clear stored vertical cursor.
  void        draw_line        (HDC hdc,Paragraph *para,Line *line,int y,int xmin,int xmax,class JWP_font *font);
  void        find_pos         (Position *loc,int view = POS_CURSOR);   // Find the pixal location of a point..
  void        set_scroll       (void);                  // Adjust position of scroll bars.
  class  Paragraph *first,*last;                        // Paragraphs for the file.
  class  Position   cursor;                             // Current cursor position
  class  Position   view_top;                           // Indicates top-left corner of display.  Control screen scrolling.
  short             x_cursor;                           // X cursor location used in scroll ups and downs (stores pixal locaton at start of up/down).
  short             char_pagewidth;                     // Width of page in characters.
  long              total_length;                       // Total length of the file (int pixls)
  short             height;                             // Display height.
  short             width;                              // Display width.
  short             char_width;                         // Width of display area in kanji characters.
  short             hscroll;                            // Horizontal scroll in characters width 
  short             vscroll;                            // Vertical scroll distance in in pixals (page up/down)
  short             ime_y;                              // Vertical offset used for IME composition window.
  TCHAR            *name;                               // Disk file name
  PrintSetup        page;                               // Page setup.
  byte              no_first;                           // No headers & footers on first page.
  byte              odd_even;                           // Separate odd and even headers and footers.
  KANJI_string      summary[NUMBER_SUMMARY];            // Summary data for file.
  KANJI_string      headers[NUMBER_HEADERS][NUMBER_POSITIONS];  // Header information for this file.
} JWP_file;

#define JWP_FONT    ((filetype == FILETYPE_EDIT) ? edit_font : file_font)

extern class JWP_file *jwp_file;    // Pointer to double linked ring of files.

//===================================================================
//
//  Class to keep track of current files list.  This is the list used
//  to determine where to "Insert to File".
//

typedef struct FILE_node {                  // Node structure for list.
  struct FILE_node *next;                   // Pointer to next node
  class  JWP_file  *file;                   // Pointer to this file
} FILE_node;

typedef class FILE_list {
public:
  inline FILE_list (void) { list = NULL; }  // Intialize the list
  void      add    (JWP_file *file);        // Add a file to the list.
  JWP_file *get    (JWP_file *exclude);     // Get current file from list.
  void      remove (JWP_file *file);        // Remove a file from the list.
private:
  FILE_node *list;                          // List of nodes.
} FILE_list;

extern FILE_list file_list;                 // Actual class instance.

//===================================================================
//
//  Key code for use with do_key().
//

#ifndef WINELIB         // WINELIB has these defined in winuser.h (man would that be helpful!).
#define VK_4    0x34
#define VK_A    0x41
#define VK_B    0x42
#define VK_C    0x43
#define VK_D    0x44
#define VK_E    0x45
#define VK_F    0x46
#define VK_G    0x47
#define VK_H    0x48
#define VK_I    0x49
#define VK_J    0x4a
#define VK_K    0x4b
#define VK_L    0x4c
#define VK_M    0x4d
#define VK_N    0x4e
#define VK_O    0x4f
#define VK_P    0x50
#define VK_Q    0x51
#define VK_R    0x52
#define VK_S    0x53
#define VK_T    0x54
#define VK_U    0x55
#define VK_V    0x56
#define VK_W    0x57
#define VK_X    0x58
#define VK_Y    0x59
#define VK_Z    0x5a
#endif WINELIB

#define VK_LT   0xbc
#define VK_GT   0xbe


#endif jwp_file_h


