//-------------------------------------------------------------------//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998,1999,2000.             //
//  All rights reserved.                                             //
//                                                                   //
//-------------------------------------------------------------------//
//
//  This modlue contains a number of special definitalis used by the 
//  Windows CE versions of the code.  Most of these defintiions, are
//  to replace missing system rotuines with some other routines.
//
#ifndef jwp_wnce_h
#define jwp_wnce_h

//-------------------------------------------------------------------
//
//  Machine specifc definitions.
//
//  The defintions used to determine the target platform are WINCE_HPC, and WINCE_PPC.
//  These are used as follows:
//
//      WINCE_HPC -- Windows CE HPC
//      WINCE_PPC -- Indicates only PPC machines.
//
//  This block of codes makes several other defitniions to make the code easier:
//
//      WINCE     -- Any Windows CE device
//      WINCE_PPC -- A Windows CE PPC
//      WINCE_HPC -- A Windows CE HPC
//
#if (defined(WINCE_PPC) || defined(WINCE_HPC))
//  #define WINCE
#endif

//-------------------------------------------------------------------
//
//  Main definitions
//

#ifdef WINCE    // The rest of this file is only used for Windows CE machines.

#include <Commctrl.h>
#include <Commdlg.h>

//
//  #define replaced C++ runtime routines.
//
#define isalnum(x)      iswalnum(x)
#define isalpha(x)      iswalpha(x)
#define islower(x)      iswlower(x)
#define isprint(x)      iswprint(x)
#define isspace(x)      iswspace(x)
#define isupper(x)      iswupper(x)
#define sprintf         swprintf
#define sscanf          swscanf
#define strdup(x)       _wcsdup(x)
#define stricmp(x,y)    _wcsicmp(x,y)
#define strnicmp(x,y,n) _wcsnicmp(x,y,n)
#define toupper(x)      towupper(x)

//
//  C++ runtime routines replaced by internal routines.
//
extern void *calloc (long s1,long s2);    // Allocate and zero a memory block.

//
//  Windows routines replaced by #defines.
//
#define GHND                            LHND
#define HDROP							void *

#define CheckDlgButton(hwnd,id,val)     SendDlgItemMessage (hwnd,id,BM_SETCHECK,val,0)
#define GlobalAlloc(x,y)                LocalAlloc(x,y)
#define GlobalLock(x)                   LocalLock(x)
#define GlobalSize(x)                   LocalSize(x)
#define GlobalUnlock(x)                 LocalUnlock(x)
#define IsDlgButtonChecked(hwnd,id)     (BOOL) SendDlgItemMessage (hwnd,id,BM_GETCHECK,0,0)
#define TextOut(hdc,x,y,str,len)        ExtTextOut(hdc,x,y,0,NULL,str,len,NULL)

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
extern TCHAR *currentdir;                                   // Current directoy
extern void set_currentdir      (TCHAR *path,int filename);

#endif WINCE

//
//  Special PPC only routines.
//
#if    (defined(WINCE_PPC) && !defined(_ARM_))
  #define PPC_INPUT_PANEL
#endif (defined(WINCE_PPC) && !defined(_ARM_))

#ifdef PPC_INPUT_PANEL
  extern void input_check   (int wParam);                       // Check input panel conditions when selecting editbox
  extern void input_panel   (int state);                        // Used to enable and disable the input panel on PPC machines.
  extern void input_restore (void);                             // Restore a saved input panel state.
  extern void input_status  (void);                             // Get input panel state for later.
  #define INPUT_CHECK(id) case id: input_check(wParam); break;  // Make or destroy input panel for edit box.
#else
  #define input_check(wParam)
  #define input_panel(x)
  #define input_restore()
  #define input_status()
  #define INPUT_CHECK(id)
#endif PPC_INPUT_PANEL




#endif jwp_wnce_h
