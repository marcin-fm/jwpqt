//-------------------------------------------------------------------//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998,1999,2000.             //
//  All rights reserved.                                             //
//                                                                   //
//-------------------------------------------------------------------//

//-------------------------------------------------------------------
//
//  This modlue is a collection of micelaeous routines not placed 
//  in any other module.
//
#ifndef jwp_misc_h
#define jwp_misc_h

#include "jwp_cach.h"
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

//
//  Scroll info
//
extern SCROLLINFO scroll_info;      // Scroll Info structure used by allmost
                                    //   all scroll bars in the system.
//
//  Error & Message Rotuines..
//
extern void ErrorMessage (int error,int format,...);        // General Error Message.
extern void OutOfMemory  (HWND hwnd);                       // Out of Memory error.
extern int  YesNo        (int format,...);                  // Put up a simple yes-no dialog box.
extern int  ButtonDialog (int idd,tchar *data,int help);    // Generates a dialog box that terminates at the first button.
extern HWND JCreateDialog(int id,HWND hwnd,DLGPROC proc,long param=0);      // Version of system routine to generate a non-modal dialog
extern int  JDialogBox   (int id,HWND hwnd,DLGPROC proc,long param=0);      // Version of system routien to generate a dialog box.
extern int  JMessageBox  (HWND hwnd,int text,int caption,UINT type,...);    // Extened version of system message routine.

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

//
//  Debugging Routine.
//
extern void do_nothing      (void);                     // For searching for optimizer bugs.
                                                        //   This routine is not actually anywhere, 
                                                        //   so if you need it you need to define it.
//
//  File name routines.
//
extern TCHAR *add_part   (TCHAR *buffer,TCHAR *part);   // Add part to a file name (used with open-files)
extern int    FileExists (tchar *name);                 // Check to see if a file exists

//
//  File IO routines.
//
extern byte *load_image (tchar *name);                  // Generates a null terminate memory image of a file.

//
//  Dialog box Routines.
//
extern int  get_int     (HWND hwnd,int id,int min_val,int max_val,int def);   // Get int value from edit control.
extern int  get_float   (HWND hwnd,int id,float min_val,float max_value,float def,int scale,float *value);   // Get a float value from buffer
extern void put_float   (HWND hwnd,int id,float value,int scale);             // Put float value into a dialog box.

//
//  Menu control routines.
//
extern long get_menudata (HMENU menu,int item,int position,TCHAR *buffer);  // Get information from recent files menu

//
//  String table manipulation tools.
//
#define GET_STRING(b,id)        LoadString(language,id,b,(sizeof(b)/sizeof(TCHAR)))
#define LOAD_STRING(b,id,s)     LoadString(language,id,b,s)

extern TCHAR *get_string    (int id);                   // Get string and return in a pointer to a static buffer.
extern TCHAR *format_string (TCHAR *buffer,int id,...); // Foramt a string based on an

//
//  Numerical tools
//
#define NINT(x) ((int) ((x)+0.5))   // Round float to integer.

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
  #define BackFillRect(hdc,rect)    FillRect (hdc,rect,GetStockObject(WHITE_BRUSH));
#else
  #define BackFillRect(hdc,rect)    FillRect (hdc,rect,(HBRUSH) (COLOR_WINDOW+1));
#endif WINCE


#endif jwp_misc_h

