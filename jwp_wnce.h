//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//===================================================================//
//
//  This modlue contains a number of special definitalis used by the 
//  Windows CE versions of the code.  Most of these defintiions, are
//  to replace missing system rotuines with some other routines.
//
#ifndef jwp_wnce_h
#define jwp_wnce_h

#if (defined(WINCE_POCKETPC) || defined(WINCE_PPC))
  #include <Aygshell.h>
#endif WINCE_POCKETPC

//===================================================================
//
//  Machine specifc definitions.
//
//  The defintions used to determine the target platform are WINCE_HPC, and WINCE_PPC.
//  These are used as follows:
//
//      WINCE_HPC      -- Windows CE HPC
//      WINCE_PPC      -- Indicates only PPC machines.
//      WINCE_POCKETPC -- Windows CE PocketPC specific machine (not PPC)
//
//  This block of codes makes several other defitniions to make the code easier:
//
//      WINCE     -- Any Windows CE device
//      WINCE_PPC -- A Windows CE PPC
//      WINCE_HPC -- A Windows CE HPC
//
#if (defined(WINCE_PPC) || defined(WINCE_HPC) || defined(WINCE_POCKETPC))
//  #define WINCE
#endif

//-------------------------------------------------------------------
//
//  Special PPC only routines.
//
#if (defined(WINCE_POCKETPC) || (defined(WINCE_PPC) && !defined(__ARM__)))
  #define PPC_INPUT_PANEL
#endif

#ifdef WINCE_POCKETPC

  extern void full_screen   (HWND hwnd);
  extern void input_check   (HWND hwnd,int wParam);                 // Check input panel conditions when selecting editbox
  extern void input_panel   (HWND hwnd,int state);                  // Used to enable and disable the input panel on PPC machines.

  #define INPUT_STATUS()
  #define INPUT_RESTORE()
  #define SIP_ON()               input_panel (hwnd,true);
  #define SIP_OFF()              input_panel (hwnd,false);
  #define POCKETPC_DIALOG(hwnd)  full_screen (hwnd);
  #define INPUT_CHECK(id) case id: input_check(hwnd,wParam); break; // Make or destroy input panel for edit box.

#elif  (defined(WINCE_PPC) && !defined(__ARM__))

  extern void input_status  (void);
  extern void input_restore (HWND hwnd);
  extern void input_check   (HWND hwnd,int wParam);                 // Check input panel conditions when selecting editbox
  extern void input_panel   (HWND hwnd,int state);                  // Used to enable and disable the input panel on PPC machines.

  #define INPUT_STATUS()         input_status();
  #define INPUT_RESTORE()        input_restore(hwnd);
  #define SIP_ON()               input_panel (hwnd,true);
  #define SIP_OFF()              input_panel (hwnd,false);
  #define POCKETPC_DIALOG(hwnd)  
  #define INPUT_CHECK(id) case id: input_check(hwnd,wParam); break; // Make or destroy input panel for edit box.

#else

  #define INPUT_STATUS()
  #define INPUT_RESTORE()
  #define SIP_ON()               
  #define SIP_OFF()              
  #define POCKETPC_DIALOG(hwnd)  
  #define input_check(hwnd,wParam)
  #define input_panel(x,y)
  #define input_status()
  #define INPUT_CHECK(id)

#endif

//===================================================================
//
//  Main definitions
//

#ifdef WINCE    // The rest of this file is only used for Windows CE machines.

#include <Commctrl.h>
#include <Commdlg.h>

//
//  #define replaced C++ runtime routines.
//
#ifndef WINCE_POCKETPC
#define isalnum(x)      iswalnum(x)
#define isalpha(x)      iswalpha(x)
#define islower(x)      iswlower(x)
#define isprint(x)      iswprint(x)
#define isspace(x)      iswspace(x)
#define isupper(x)      iswupper(x)
#endif  WINCE_POCKETPC
//%%%
#define sprintf         swprintf
#define sscanf          swscanf
#define strdup(x)       _wcsdup(x)
#define stricmp(x,y)    _wcsicmp(x,y)
#define strnicmp(x,y,n) _wcsnicmp(x,y,n)
#define toupper(x)      towupper(x)

//
//  C++ runtime routines replaced by internal routines.
//
#ifndef WINCE_POCKETPC
extern void *calloc (long s1,long s2);    // Allocate and zero a memory block.
#endif  WINCE_POCKETPC

//
//  Windows routines replaced by #defines.
//
#define GHND                            LHND
#define HDROP							void *


#ifndef WINCE_POCKETPC
#define GlobalAlloc(x,y)                LocalAlloc(x,y)
#endif  WINCE_POCKETPC
#define GlobalLock(x)                   LocalLock(x)
#define GlobalSize(x)                   LocalSize(x)
#define GlobalUnlock(x)                 LocalUnlock(x)
#define TextOut(hdc,x,y,str,len)        ExtTextOut(hdc,x,y,0,NULL,str,len,NULL)
#define CheckDlgButton(hwnd,id,val)     SendDlgItemMessage (hwnd,id,BM_SETCHECK,val,0)
#define IsDlgButtonChecked(hwnd,id)     (BOOL) SendDlgItemMessage (hwnd,id,BM_GETCHECK,0,0)

//
//  IME rotuines that are mapped out
//
#define ImmAssociateContext(a,b)

//
//  Windows routines replaced by Internal rotuines.
//
extern void GetCurrentDirectory (int size,TCHAR *buffer);
extern void GetFullPathName     (const TCHAR *name,int,TCHAR *buffer,TCHAR **ptr);
extern void InvertRect          (HDC hdc,RECT *rect);

//
//  Special Windows CE only routines.
//
extern TCHAR *currentdir;                                       // Current directoy
extern void set_currentdir      (TCHAR *path,int filename);

#define PS_DOT  PS_DASH                                         // CE does not support dot brushes, just solid burshes.

#endif WINCE

//
//  Metric used for detecting mouse/pen being held down
//
#ifdef WINCE
  #define DOUBLE_X  3
  #define DOUBLE_Y  3
#else  WINCE
  #define DOUBLE_X  GetSystemMetrics(SM_CXDOUBLECLK)
  #define DOUBLE_Y  GetSystemMetrics(SM_CYDOUBLECLK)
#endif WINCE



//
//  Macros used for Windows CE pen fucntions.
//
//  These definitions were to enable or disable the system for enabling the popup menu when
//  using PocketPC.  I decided to en able this all the time, since it didn't seem to 
//  provide a problem.
//
#if 0
#ifdef xxWINCE
  #define MOUSE_MOVE(hwnd,lParam,mouse_x,mouse_y)
  #define MOUSE_TIMER(hwnd,selecting,mouse_x,mouse_y,proc)
  #define MOUSE_LBUTTONUP(hwnd,selecting)
  #define MOUSE_LBUTTONDOWN(hwnd,lParam,mouse_x,mouse_y)
#else  WINCE
  #define MOUSE_MOVE(hwnd,lParam,mouse_x,mouse_y)          { if ((abs(LOWORD(lParam)-mouse_x) > DOUBLE_X) || (abs(HIWORD(lParam)-mouse_y) > DOUBLE_Y)) KillTimer (hwnd,TIMER_MOUSEHOLD); }
  #define MOUSE_TIMER(hwnd,selecting,mouse_x,mouse_y,proc) { selecting = false; KillTimer (hwnd,TIMER_MOUSEHOLD); ReleaseCapture (); proc (mouse_x,mouse_y); }
  #define MOUSE_LBUTTONUP(hwnd,selecting)                  { KillTimer (hwnd,TIMER_MOUSEHOLD); selecting = false; }
  #define MOUSE_LBUTTONDOWN(hwnd,lParam,mouse_x,mouse_y)   { mouse_x = LOWORD(lParam); mouse_y = HIWORD(lParam); SetTimer (hwnd,TIMER_MOUSEHOLD,GetDoubleClickTime(),NULL); }
#endif WINCE
#endif

#endif jwp_wnce_h
