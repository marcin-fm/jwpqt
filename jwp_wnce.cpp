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
#ifdef WINCE
#include "jwpce.h"

#if (defined(WINCE_PPC) && !defined(_ARM_))
  #include <Aygshell.h>
#endif 

//-------------------------------------------------------------------
//
//  Exported Data.
//
TCHAR *currentdir = NULL;   // Our own version fo the current directory.

//
//  Repalcement for c library routine that allocates and clears memory.
//
//      s1,s2  -- Multiplied together, this is the size of the memory
//                block being allocated.
//  
//      RETURN -- Zeroed block fo memory or NULL.
//
void *calloc (long s1,long s2) {
  void *mem;
  if (mem = malloc(s1*s2)) memset (mem,0,s1*s2);
  return (mem);
}
 
//
//  Fake stub routine for the system GetCurrentDirectory.  This 
//  simply gets our own internal current_directory value.
//
//      size   -- Size of buffer.
//      buffer -- Buffer to hold current direcotry.
//
void GetCurrentDirectory (int size,TCHAR *buffer) {
  if (!currentdir) {
    buffer[0] = 0;
    return;
  }
  lstrcpy (buffer,currentdir);
  return;
}

//
//  Get the full path from the relative path.  Since Windows CE always
//  works with the full path, there is actually very little to do 
//  here.
//
//      name   -- Partial or full path that is to be expanded.
//      size   -- Size of buffer.
//      buffer -- Location to put the path built.
//      ptr    -- Pointer to the beginning of the name part of the full
//                path.
//
void GetFullPathName (const TCHAR *name,int size,TCHAR *buffer,TCHAR **ptr) {
  int i;
  lstrcpy (buffer,name);
  for (i = lstrlen(buffer)-1; (i >= 0) && (buffer[i] != '\\') && (buffer[i] != ':'); i--);
  *ptr = buffer+i+1;
  return;
}

//
//  Replacement for the system routine that inverts a rectangle region.
//
//      hdc  -- Display context to render into.
//      rect -- Rectangle that specifies the coordinates fo the 
//              region to inver.
//
//
void InvertRect (HDC hdc,RECT *rect) {
  BitBlt (hdc,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,NULL,0,0,DSTINVERT);
  return;
}

//
//  Save the current directory in our buffer.  This allows us to provide
//  a current directory even though Windows CE has no such concept.
//
//      path     -- To to be used in setting the current directory.
//      filename -- If non-zero the path specified is a file name and 
//                  the path part must be extracted, otherwise this is 
//                  assumed to be a directory and no further processing
//                  is required.
//
void set_currentdir (TCHAR *path,int filename) {
  int i;
  if (currentdir) free (currentdir);
  currentdir = strdup(path);
  if (!filename) return;
  for (i = lstrlen(currentdir)-1; (i > 0) && (currentdir[i] != '\\') && (currentdir[i] != ':'); i--);
  currentdir[i] = 0;
  return;
}

//-------------------------------------------------------------------
//
//  Routines specific to PPC's
//
#ifdef PPC_INPUT_PANEL

static short input_state = false;   // Memory location for stored input panel state.

//
//  This routine enables and disables the input panel when you activate a dialog box item.
//
//      notify -- Notification from the system.
//
void input_check (int wParam) {
  switch (HIWORD(wParam)) {
    case EN_SETFOCUS:  input_panel (true);  break;
    case EN_KILLFOCUS: input_panel (false); break;
    break; 
  }
  return;
}

//
//  This routine controls the input panel.  This allows the program to open and close the
//  input panel.
//
//      state -- Set to non-zero to turn on the panel.
//
void input_panel (int state) {
  SIPINFO si;                                       // Intialize the SIPINFO structure
  memset (&si,0,sizeof(si));
  si.cbSize = sizeof(si);
  if (!SHSipInfo(SPI_GETSIPINFO,0,&si,0)) return;   // Get the current SIPINFO so we know the states
  si.fdwFlags &= ~SIPF_ON;                          // Clear the flag that controls the input panel
  if (state) si.fdwFlags |= SIPF_ON;                // Set flag if we want to turn it on.
  SHSipInfo (SPI_SETSIPINFO,0,&si,0);               // Set the input panel
  return;
}

//
//  Restore the input panel to a saved state.  This is actually not a direct restore, but 
//  rather clever.  If the panel was open we will open it.  If it was closed we will do 
//  nothing.  Generally this has a very intiuative feel.
//
void input_restore () {
  if (input_state) input_panel (true);
  return;
}

//
//  This rotuine stores the state of the input so it can potentially be restore at a later 
//  time.
//
void input_status () {
  SIPINFO si;                                       // Intialize the SIPINFO structure
  memset (&si,0,sizeof(si));
  si.cbSize = sizeof(si);
  if (!SHSipInfo(SPI_GETSIPINFO,0,&si,0)) return;   // Get the current SIPINFO so we know the states
  input_state = (short) (si.fdwFlags & SIPF_ON);    // Get input panel state.
  return;
}

#endif PPC_INPUT_PANEL

#endif WINCE
