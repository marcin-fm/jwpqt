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
//  This modlue is a collection of micelaeous routines not placed 
//  in any other module.
//
#ifndef jwp_misc_h
#define jwp_misc_h

#include "jwp_cach.h"

//--------------------------------
//
//  Structure used to save the size and location of a dialog.
//
struct size_window {
  int x,y;                          // Window position.
  int sx,sy;                        // Window size.
};

//--------------------------------
//
//  Class used to process size changes in dialogs.
//
class SIZE_window {
public:
  SIZE_window    (void);                                                    // Contstructor
  void wm_init   (HWND hwnd,int id,struct size_window *init,int buttons,int dfirst,int dlast);   // Called in WM_INITDIALOG
  void wm_move   (void);                                                    // Called from WM_MOVE
  void wm_size   (int wParam);                                              // Called from WM_SIZE
  void wm_sizing (RECT *rect);                                              // Called from WM_SIZING
private:
  void   check_controls  (void);    // Check the dynamic controls.
  short  first,last;                // First and last dynamic buttons.
  byte   move_buttons;              // Indicates IDOK and IDCANCEL controls need to be moved.
  struct size_window *save;         // Pointer to saved file location (could be NULL)
  int    dlg_xmin,dlg_ymin;         // Minimum dialog size (also default)
  int    lst_xmin,lst_ymin;         // Minimum list control size.
  int    btn_y;                     // Default location of the control buttons
  HWND   list;                      // Pointer to list.     
  HWND   dlg;                       // Pointer to dialog.
};

//--------------------------------
//
//  This class defines a kanji string and the actions that can be done 
//  to it.  The base storage is simply a pointer to a NULL terminated 
//  kanji list.  The pointer is NULL when the string contains no data.
//
typedef class KANJI_string {
public:
  KANJI *kanji;
  inline  KANJI_string() { kanji = NULL; }      // Contstructor
  inline ~KANJI_string() { free(); }            // Destructor.
  inline  void set (class KANJI_string *ks) { set (ks->kanji); }
  void   copy     (KANJI *string,int limit);    // Copy string to buffer with limit
  void   free     (void);                       // Deallocate resources.
  void   get      (HWND hwnd,int id);           // Get from edit box.
  int    length   (void);                       // Get length
  void   put      (HWND hwnd,int id);           // Put in edit box.
  int    read     (IO_cache *cache);            // Read from file
  void   set      (KANJI *k,int len = -1);      // Set to specific value.
  int    write    (IO_cache *cache);            // Write to file.
  void   transfer (class KANJI_string *ks);     // Transfer string without reallocating.
} KANJI_string;

//--------------------------------
//
//  Scroll info
//
extern SCROLLINFO scroll_info;      // Scroll Info structure used by allmost
                                    //   all scroll bars in the system.
//--------------------------------
//
//  Error & Message Rotuines..
//
extern void ErrorMessage (int error,int format,...);        // General Error Message.
extern void OutOfMemory  (HWND hwnd);                       // Out of Memory error.
extern int  YesNo        (int format,...);                  // Put up a simple yes-no dialog box.
extern int  ButtonDialog (HWND hwnd,int idd,tchar *data,int help);          // Generates a dialog box that terminates at the first button.
extern HWND JCreateDialog(int id,HWND hwnd,DLGPROC proc,long param=0);      // Version of system routine to generate a non-modal dialog
extern int  JDialogBox   (int id,HWND hwnd,DLGPROC proc,long param=0);      // Version of system routien to generate a dialog box.
extern int  JMessageBox  (HWND hwnd,int text,int caption,UINT type,...);    // Extened version of system message routine.

//--------------------------------
//
//  Memory allocation.
//
extern KANJI *kstrdup (KANJI *string,int length);           // Duplicate a kanji string in memory.

//--------------------------------
//
//  Tab dialog box controler
//
#define WM_GETDLGVALUES (WM_USER+1) // Message sent to pages to read values out.

typedef struct TabPage {            // Defines a page of a Tab Dialog box.
  short   text;                     // Label for page.
  int     id;                       // ID of page template.
  DLGPROC procedure;                // Procedure for this page.
  ushort  help;                     // Help id
} TabPage;

typedef struct TabSetup {           // Defines a tab dialog box.
  TabPage *pages;                   // Pointer to array of TabPage structures.
  short    count;                   // Number of pages in the above array.
  short    page;                    // Current page.
  DLGPROC  procedure;               // Dialog proc for user buttons on the main page.
} TabSetup;

extern int TabDialog (int id,TabSetup *setup);

//--------------------------------
//
//  Debugging Routine.
//
extern void do_nothing      (void);                     // For searching for optimizer bugs.
                                                        //   This routine is not actually anywhere, 
                                                        //   so if you need it you need to define it.
//--------------------------------
//
//  File name routines.
//
extern TCHAR *add_part   (TCHAR *buffer,TCHAR *part);   // Add part to a file name (used with open-files)
extern TCHAR *get_folder (int id,TCHAR *buffer);        // Get a system folder path.
extern int    FileExists (tchar *name);                 // Check to see if a file exists

//--------------------------------
//
//  File IO routines.
//
extern byte *load_image (tchar *name);                  // Generates a null terminate memory image of a file.

//--------------------------------
//
//  Dialog box Routines.
//
extern int  get_int     (HWND hwnd,int id,int min_val,int max_val,int def);   // Get int value from edit control.
extern int  get_float   (HWND hwnd,int id,float min_val,float max_value,float def,int scale,float *value);   // Get a float value from buffer
extern void put_float   (HWND hwnd,int id,float value,int scale);             // Put float value into a dialog box.

//--------------------------------
//
//  Menu control routines.
//
extern long get_menudata (HMENU menu,int item,int position,TCHAR *buffer);  // Get information from recent files menu

#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
//--------------------------------
//
//  Class that manages the list of recent file names.
//
#define RECENT_MAX  10                  // Number of items in recent files list (must be 1 more then the actual values)
#define RECENT_LEN  28                  // Maximum length of recent file name

typedef TCHAR RecentLine[256];          // Single line of the list.

class RecentList {
public:
  RecentList::RecentList (void);                                // Constructor
  inline TCHAR *get    (int pos) { return (list[pos]); }        // Get an item
  void          insert (HMENU menu,int pos,TCHAR *text);        // Insert a name into the list.
  void          remove (HMENU menu,int pos);                    // Remove a name from the list.
private:
  RecentLine list[RECENT_MAX];                                  // Actual list.
};

extern class RecentList recent_list;    // Class instance.

  #define JInsertMenu(menu,pos,flags,item,text)    recent_list.insert(menu,pos,text)  
  #define JRemoveMenu(menu,pos,flags)              recent_list.remove(menu,pos)
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  #define JInsertMenu(menu,pos,flags,item,text)    InsertMenu(menu,pos,flags,item,text)
  #define JRemoveMenu(menu,pos,flags)              RemoveMenu(menu,pos,flags)
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))


//--------------------------------
//
//  String table manipulation tools.
//
#define GET_STRING(b,id)        LoadString(language,id,b,(sizeof(b)/sizeof(TCHAR)))
#define LOAD_STRING(b,id,s)     LoadString(language,id,b,s)

extern TCHAR *get_string    (int id);                   // Get string and return in a pointer to a static buffer.
extern TCHAR *format_string (TCHAR *buffer,int id,...); // Foramt a string based on an
extern TCHAR *tab_string    (int id,int id2 = 0);       // Get a string an replace tab characters with ascii 0 (used for requestors).

//--------------------------------
//
//  Numerical tools
//
#define NINT(x) ((int) ((x)+0.5))   // Round float to integer.

//-------------------------------------------------------------------
//
//  Graphics IO routins.
//
//      Fills a rectangle with the current background color.
//      This used to be done with direct calls to FillRect,
//      with color arguments, however, Windows CE does not 
//      correctly support these calls, thus I replaced the 
//      calls with this routine.
//
#ifdef WINCE 
  #define BackFillRect(hdc,rect)    { HBRUSH brush; brush = (HBRUSH) SelectObject(hdc,CreateSolidBrush(GetSysColor(COLOR_WINDOW))); FillRect (hdc,rect,brush); DeleteObject(SelectObject(hdc,brush)); }
#else
  #define BackFillRect(hdc,rect)    FillRect (hdc,rect,(HBRUSH) (COLOR_WINDOW+1));
#endif WINCE

#endif jwp_misc_h

