//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//===================================================================//

#define _WIN32_WINNT    0x0401                  // Necessary to get wheel-mouse defintions.

#include <windows.h>

#ifndef jwp_main_h
#define jwp_main_h

#include "jwp_wnce.h"                           // Windows CE stuff
#include "jwp_options.h"                        // Sets some major compile time options.
#include "resource.h"                           // Get the resource parameters for everyone
#ifndef WINCE
  #include <stdio.h>                            // Definition of sscanf() is here, but CE does not have this file
#endif

#ifdef WINELIB                                  // Defintions for UNIX WINELIB
  #include <stdlib.h>
  #include <ctype.h>
  #include <tchar.h>
  #include <malloc.h>
  #include <shlguid.h>
  #include <commdlg.h>
  #define stricmp strcasecmp
  #define CC_ANYCOLOR 0x100
  #define CC_SOLIDCOLOR 0x80
  #define CF_UNICODETEXT 99
#endif WINELIB

//===================================================================
//
//  Degugging stuff
//

//#define DEBUG_ROUTINES

#if (defined(_DEBUG) && !defined(DEBUG_ROUTINES)) 
#define DEBUG_ROUTINES
#endif _DEBUG && !DEBUG_ROUTINES

#ifdef DEBUG_ROUTINES
  extern void dprintf (TCHAR *format,...);
  extern void mprintf (TCHAR *format,...);
  #define DPRINTF   dprintf
  #define MPRINTF   mprintf
#else  DEBUG_ROUTINES  
  #define DPRINTF   (true ? (void) 0 : wsprintf )
  #define MPRINTF   (true ? (void) 0 : wsprintf )
#endif DEBUG_ROUTINES

//===================================================================
//
//  General definitions.
//

#define SIZE_BUFFER     512                     // Size of general working character buffer.
                                                // Version special is defined in options and includes special flags.
#define SIZE_WORKING    256                     // Size of a working string (smaller than a buffer).
#ifndef VERSION_SPECIAL
  #define VERSION_SPECIAL                       // Define the special ID if not already.  Used for special versions.
#endif  VERSION_SPECIAL
#define VERSION_STRING  TEXT("1.50") VERSION_SPECIAL  // Version ID number
#define VERSION_NAME    TEXT("JWPce ") VERSION_STRING // Version name.

                                // Input modes
#define MODE_KANJI      0       // Kanji (kana too)
#define MODE_ASCII      1       // ASCII
#define MODE_JASCII     2       // Japanese ASCII.
#define MODE_TOGGLE     10      // Toggles between ASCII and kanji.
#define MODE_CYCLE      11      // Cycles through all modes.
#define MODE_NOCHANGE   13      // Does the updage associated with a mode change but does not change the mode

                                // Windows globals.
extern HWND      main_window;   // Main window (parent).
extern HWND      file_window;   // Ediging window (main child).
extern HINSTANCE instance;      // Our instance.
extern HINSTANCE language;      // Language processor instance.
extern HMENU     hmenu;         // Our menu.
extern HMENU     popup;         // Our popup menu.
#ifdef WINCE_POCKETPC
extern HMENU     hmenu2;        // File menu in the button bar.
#endif WINCE_POCKETPC

typedef short           INT16;  // Various sizes (mostly used in relation to JWP structures).
typedef unsigned short  UINT16;
typedef UINT16          KANJI;
typedef unsigned short  ushort;
typedef unsigned long   ulong;
typedef const TCHAR     tchar;

#define null            0       // Used to set handles to zero, clarifies memory refferences.

//
//  Menu defintiions, in order (need to be used in both jwpce.cpp, and jwp_file.cpp).
//
#if  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  #define MENU_FILE                         0       // Menu number for File menu.
  #define MENU_RECENT                       10      // Menu number for recent files list.
#elif defined(WINCE_HPC)
  #define MENU_FILE                         0       // Menu number for File menu.
  #define MENU_RECENT                       9       // Menu number for recent files list.
#else
  #define MENU_FILE                         0       // Menu number for File menu.
  #define MENU_RECENT                       13      // Menu number for recent files list.
#endif TARGET_TYPE

#define MENU_WINDOW                         4       // Menu number for the Window number
#define MENU_WINDOW_FILE_BASE               4       // Menu index for the first file name in the Windows menu.

//
//  Custom window message values.
//
#define WMU_SETWINDOWVALUE  (WM_USER+1)     // Used to set control value for all working windows.
#define WMU_GETWINDOWVALUE  (WM_USER+2)     // Used to get structrues behind windows controls.
#define WMU_COLORCHANGE     (WM_USER+3)     // Message from color box control indicating it is selected.
#define WMU_KANJIINFO       (WM_USER+4)     // Internal message used to requrest kanji information in mouse handler.
#define WMU_LOADCONTROL     (WM_USER+5)     // Request control to load selected data
#define WMU_JLBC_BEGIN      (WM_USER+6)     // Japanese list box control find beginning of entry.
#define WMU_JLBC_INSERT     (WM_USER+7)     // Insert data from list box to file.
#define WMU_SETTEXT         (WM_USER+8)     // Set text in a Japanese edit control
#define WMU_GETTEXT         (WM_USER+9)     // Get text from a Japanese edit/list control
#define WMU_RESETLIST       (WM_USER+10)    // Reset a Japanese List control
#define WMU_SETSELECT       (WM_USER+11)    // Set selection in Japanese List control
#define WMU_SETEXCLUDE      (WM_USER+12)    // Set exclusion file used for inserts
#define WMU_OKTODESTROY     (WM_USER+13)    // Message sent to non-modal dialog boxes before they are destroyed
#define WMU_CHARFROMLIST    (WM_USER+14)    // User is typing into the list box.
#define WMU_EDITFROMLIST    (WM_USER+15)    // User has enetered edit commands into the list box.
#define WMU_IMEFROMLIST     (WM_USER+16)    // User is typing text via the IME into the list box.
#define WMU_SETHISTORY      (WM_USER+17)    // Set the history used with a Japanese edit control
#define WMU_HISTORYLIST     (WM_USER+18)    // Generate a history list for this list.
#define WMU_SETINFOKANJI    (WM_USER+19)    // Set the character used for the kanji info (used in single window system)

//
//  Timers used.
//
#define TIMER_AUTOSCROLL    1               // Timer used for auto-scroll
#define TIMER_MOUSEHOLD     2               // Timer used for mouse hold events.

//===================================================================
//
//  Configuration defintiions
//
extern struct cfg *cfg;                     // Pointer to configuration used 
extern struct cfg  default_config;          // Default config stucture definines the default condition.

//===================================================================
//
//  Exported routines.
//
extern void add_dialog    (HWND hwnd,int closeable);    // Add a modeless dialog to the system list
extern void remove_dialog (HWND hwnd);                  // Remove a mdoeless dialog from the system list.
extern void set_mode      (int mode);                   // Sets the current edit mode.


#endif jwp_main_h

