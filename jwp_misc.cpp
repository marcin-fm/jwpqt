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
#include "jwpce.h"
#include "jwp_cach.h"
#include "jwp_edit.h"
#include "jwp_help.h"
#include "jwp_misc.h"
#include <commctrl.h>   // Needed for tab controls

//-------------------------------------------------------------------
//
//  Begin class KANJI_string.
//
//  This class is a collection of fucntions deisgined to process 
//  strings that are kanji based.  These objects are hevally used 
//  in headers/footers, summaries, and printing.
//
//  The initernal data represended by the string, is simply a NULL
//  terminated list of JIS codes and ASCII codes.
//

//
//  Copies the contents of the string buffer into a fixed length 
//  buffer, that is NULL terminated.
//
//      string -- Buffer location to copy into.
//      limit  -- Size of the fixed buffer.  The number of characters
//                copied is limited to one less than the buffer 
//                length.  A Null character will be added to the 
//                end of the buffer.
//
void KANJI_string::copy (KANJI *string,int limit) {
  int i;
  memset (string,0,limit*sizeof(KANJI));
  i = length();
  if (i >= limit) i = limit-1;
  memcpy (string,kanji,i*sizeof(KANJI));
  return;
}

//
//  Deallocate memory associated with the string.
//
void KANJI_string::free () {
  if (kanji) ::free (kanji);
  kanji = NULL;
  return;
}

//
//  Get the string value from a Japanese edit box.
//
//      hwnd -- Dialog box containning the edit box.
//      ID   -- ID of the edit box.
//
void KANJI_string::get (HWND hwnd,int id) {
  int    i;
  KANJI *k;
  i = JE_GetText(hwnd,id,&k);
  free ();
  if (!i) return;
  set (k,i);
  return;
}

//
//  Get the length of the string.
//
//      RETURN -- Length of the string in characters.
//  
int KANJI_string::length () {
  int i;
  if (!kanji) return (0);
  for (i = 0; kanji[i]; i++);
  return (i);
}

//
//  Put the string into a Japanese edit-box.
//
//      hwnd -- Pointer to dialog box containning the Japanese edit box.
//      id   -- ID of the Japanese edit box.
//
void KANJI_string::put (HWND hwnd,int id) {
  SendDlgItemMessage (hwnd,id,JE_SETTEXT,length(),(LPARAM) kanji);
  return;
}

//
//  Read a string from a file.
//
//      cache  -- IO_cache object that describes where to read the 
//                object from.
//
//      RETURN -- Non-zero return value indicates an error.
//
int KANJI_string::read (IO_cache *cache) {
  short len;
  free ();
  if (cache->get_block(&len,sizeof(len))) return (true);
  if (len <= 0) return (false);
  if (!(kanji = (KANJI *) calloc(len+1,sizeof(KANJI)))) return (true);
  if (cache->get_block(kanji,len*sizeof(KANJI))) return (true);
  return (false);
}

//
//  Set the string value (i.e. copy a string into this object).
//
//      kstring -- Pointer to a kanji string.
//      len     -- Length of kstring, in characters.  A value of 
//                 -1 can be used to copy up to a NULL terminating 
//                 character.
//
void KANJI_string::set (KANJI *kstring,int len) {
  if (!kstring) return;
  if (len == -1) {
    for (len = 0; kstring[len]; len++);
  }
  if (!(kanji = (KANJI *) calloc(len+1,sizeof(KANJI)))) return;
  memcpy (kanji,kstring,len*sizeof(KANJI));
  return;
}

//
//  This routine transfers the actual string from one KANJI_string 
//  object to another without allocating the string.  This is faster 
//  and easier on the system memory.
//
//      ks -- KANJI_string object that should be transfered to this 
//            KANJI_string.
//
void KANJI_string::transfer(KANJI_string *ks) {
  free ();
  kanji = ks->kanji;
  ks->kanji = NULL;
  return;
}

//
//  Write the string to a file.  The format of the string in the file
//  is a count of the number of bytes followed by the bytes.  The 
//  trailing NULL is not written.
//
//      cache  -- IO_cache object which indicates where the object 
//                will be written.
//
//      RETURN -- A non-zero value indicates an error.
//
int KANJI_string::write (IO_cache *cache) {
  short len;
  len = length();
  cache->put_block(&len,sizeof(len));
  if (len <= 0) return (false);
  cache->put_block(kanji,len*sizeof(KANJI));
  return (false);
}

//
//  End class KANJI_string.
//
//-------------------------------------------------------------------

//-------------------------------------------------------------------
//
//  Static rotuines.
//

//
//  Main dialog box handler for tabed-dialog boxes.  There is actually a second user
//  handler tha may be called to process messages not processed here.
//
static BOOL CALLBACK tab_dialog (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  HWND      page,tab;           // Window pointer for page and tab control.
  TabSetup *setup;              // Pointer to setup structure.
  TC_ITEM   item;               // Item structure to access data in the tab control.
  RECT      dlg,rect;           // Rectangles for the dialog box and the tab control (and it's window)
  int       i;              
  tab   = GetDlgItem(hwnd,IDC_TABCONTROL);              // The tab control.
  setup = (TabSetup *) GetWindowLong(hwnd,DWL_USER);    // The user setup, passed in lParam, and saved in the DWL_USER window param
  switch (msg) {
    case WM_INITDIALOG:
         setup = (TabSetup *) lParam;                   // Get the real setup.
         SetWindowLong (hwnd,DWL_USER,lParam);          // Save it for later
         GetWindowRect (hwnd,&dlg);                     // Get working space in tab control
#ifndef WINELIB
         dlg.top    += GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYDLGFRAME);
         dlg.bottom += GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYDLGFRAME);
#endif WINELIB
//       dlg.left   += GetSystemMetrics(SM_CXDLGFRAME);
//       dlg.right  += GetSystemMetrics(SM_CXDLGFRAME);
         memset (&item,0,sizeof(item));
         item.mask = TCIF_TEXT | TCIF_PARAM;
         for (i = 0; i < setup->count; i++) {           // Create each page.
           page = JCreateDialog (setup->pages[i].id,hwnd,setup->pages[i].procedure);
           item.pszText = get_string(setup->pages[i].text);     // Add page to tab control
           item.lParam  = (LPARAM) page;
           TabCtrl_InsertItem (tab,i,&item);
           GetWindowRect      (tab,&rect);              // Center page in tab control window.
           TabCtrl_AdjustRect (tab,false,&rect);
           MoveWindow         (page,rect.left-dlg.left,rect.top-dlg.top,rect.right-rect.left,rect.bottom-rect.top,true);
           ShowWindow         (page,SW_HIDE);        
         }
         TabCtrl_SetCurSel (tab,setup->page);           // Select last page used.
         goto SetPage;
    case WM_HELP:
         do_help (hwnd,setup->pages[setup->page].help);
         return  (true);
    case WM_NOTIFY:
         switch (((LPNMHDR) lParam)->code) {
           case TCN_SELCHANGING:                        // Page is going out of view so hide it.
                item.mask = TCIF_PARAM;
                TabCtrl_GetItem(tab,TabCtrl_GetCurSel(tab),&item);
                ShowWindow ((HWND) (item.lParam),SW_HIDE);
                return (false);
           case TCN_SELCHANGE:                          // Page is coming into view so show it.
SetPage:;
                item.mask   = TCIF_PARAM;
                i           = TabCtrl_GetCurSel(tab);
                setup->page = i;
                TabCtrl_GetItem (tab,i,&item);
                ShowWindow (page = ((HWND) (item.lParam)),SW_SHOW);
                BringWindowToTop (page);
                if (msg == WM_INITDIALOG) SetFocus (GetNextDlgTabItem(page,null,false));
                return (false);                
         }
         break;
    case WM_COMMAND:
         switch (LOWORD(wParam)) { 
           case IDOK:                                   // Keep the changes, so read all pages.
                item.mask = TCIF_PARAM;
                for (i = 0; i < setup->count; i++) {
                  TabCtrl_GetItem (tab,i,&item);
                  SendMessage ((HWND) (item.lParam),WM_GETDLGVALUES,0,0);
                }
           case IDCANCEL:                               // Just exit.
                EndDialog (hwnd,LOWORD(wParam == IDOK));
                return    (true);
           case IDC_TABHELP:
                do_help (hwnd,setup->pages[setup->page].help);
                return  (true);
         }
         break;
  }
//
//  Call user procedure.
//
  if (setup && setup->procedure) return ((*setup->procedure)(hwnd,msg,wParam,lParam));
  return (false);
}

//-------------------------------------------------------------------
//
//  Exported structures.
//

SCROLLINFO scroll_info = { sizeof(SCROLLINFO),SIF_ALL | SIF_DISABLENOSCROLL,0,0,0,0,0 };    

//-------------------------------------------------------------------
//
//  Exported routines.
//

//
//  This small utility routine is actually designed for use with the 
//  file requestors.  It's fucntion is to add a part the a file name
//  (adjusting slashes and whatever else need be adjusted), and advance
//  the pointer to the next file in the list.
//
//  A special case occures when *part is null.  No file name is modified,
//  and the value of part is returned.  This should only occure when the 
//  user has only selected a single file.
//
//      buffer -- Buffer used to build file name.
//      part   -- Part ot add to the buffer string.
//
//      RETURN -- On exit the buffer contains the completed file name.
//                and the return value contains the pointer to the next
//                element in the file list.
//
TCHAR *add_part (TCHAR *buffer,TCHAR *part) {
  if (!*part) return (part);
  if (buffer[lstrlen(buffer)-1] != '\\') lstrcat (buffer,TEXT("\\"));
  lstrcat (buffer,part);
  return (part+lstrlen(part)+1);
}

//
//  Group of routines to process button dialog boxes.  A button 
//  dialog box is any dialog box that contains only active buttons.
//  Additionally, a single static text item is supported.  The retun
//  value from this type of dialog is the ID value of the button 
//  selected.  Any button will terminate the dialog.
//
//      IDC_DATA    // ID for static text item.
//
static tchar *button_data;  // Static location to pass data to the 
                            //   dialog box procedure for the static 
                            //   text item.  A value of NULL will 
                            //   suppress the text.
static int    button_help;  // Static location used to hold help ID for
                            //   button dialog.

//
//  Dialog box procedure.
//
static BOOL CALLBACK dialog_button (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  switch (message) {
    case WM_INITDIALOG: 
         if (button_data) SetDlgItemText (hwnd,IDC_DATA,button_data);
         return (true);
    case WM_HELP:
         do_help (hwnd,button_help);
         return  (true);
    case WM_COMMAND:
         EndDialog (hwnd,LOWORD(wParam));
         return    (true);
  }
  return (false);
}

//
//  Generates a button dialog.
//
//      idd    -- Diolog box id.
//      data   -- Data for the static text item.  A value of NULL should
//                be used if there is not static text item.
//      help   -- Help id for dialog
//
//      RETURN -- The ID of the button selected that termianted the 
//                dialog.
//
int ButtonDialog (int idd,tchar *data,int help) {
  button_data = data;
  button_help = help;
  return (JDialogBox(idd,main_window,(DLGPROC) dialog_button));
}

//
//  Generate an error message.
//
//      error  -- Set non-zero for an error and zero for a warning.
//      format -- printf style format string id.
//      ...    -- Arguments for printf string.
//
void ErrorMessage (int error,int format,...) {
  TCHAR buffer[SIZE_BUFFER],string[SIZE_BUFFER];
  va_list argptr;
  va_start   (argptr,format);
  GET_STRING (string,format);
  wvsprintf  (buffer,string,argptr);
  if (error) MessageBox (main_window,buffer,get_string(IDS_ERROR),MB_OK | MB_ICONWARNING);
    else MessageBox (main_window,buffer,get_string(IDS_WARNING),MB_OK | MB_ICONINFORMATION);
  return;
}

//
//  Check to see if a file exists.
//
//      name   -- Name of file to check.
//
//      RETURN -- A non-zero return value indicates that the file 
//                exists and can be open.
//
int FileExists (tchar *name) {
  HANDLE file;
#ifndef WINCE
  UINT   error_mode;
  error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);    // Diable errors incase we are using a floppy
#endif  WINCE
  file = CreateFile(name,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null);
#ifndef WINCE
  SetErrorMode (error_mode);                            // Re-enable error handling.
#endif  WINCE
  if (file == INVALID_HANDLE_VALUE) return (false);
  CloseHandle (file);
  return (true);
}

//
//  This routine formats a string based on an ID from the resource table.
//
//      buffer -- Buffer to hold the formatted string.
//      id     -- ID of printf tyep format pattern.
//      
//      RETURN -- Pointer to buffer.
//
TCHAR *format_string (TCHAR *buffer,int id,...) {
  TCHAR format[SIZE_BUFFER];
  va_list argptr;
  va_start  (argptr,id);
  LoadString (language,id,format,SIZE_BUFFER);
  wvsprintf (buffer,format,argptr);
  return (buffer);
}

//
//  This rotuine is used to read a float value from an edit-box.
//
//      hwnd    -- Window containning the edit box (dialog).
//      id      -- ID of edit box.
//      min_val -- Minimum acceptable value.
//      max_val -- Maximum acceptable value.
//      def     -- Default value (used when errors occure)
//      scale   -- This value is multiplied by the eidt-box value 
//                 before returning.  This is used for processing
//                 values for line spacing and font point size.
//      address -- If non-zero the value will be written to this
//                 address.
//
//      RETURN  -- Return value is the nearest integer to the value
//                 the user entered.
//
int get_float (HWND hwnd,int id,float min_val,float max_val,float def,int scale,float *address) {
  float value;
  TCHAR buffer[SIZE_BUFFER];
  GetDlgItemText(hwnd,id,buffer,SIZE_BUFFER);
  if (1 != sscanf(buffer,TEXT("%g"),&value)) value = def;
    else {
#ifdef WINCE_PPC                                            // This is a big KLUDGE
      if (((int) value) < ((int) min_val)) value = min_val; // Apparently MS C++ 6.00 will not let you use < or >
      if (((int) value) > ((int) max_val)) value = max_val; //   with float numbers!
#else
      if (value < min_val) value = min_val;
      if (value > max_val) value = max_val;
#endif WINCE_PPC
      value *= scale;
    }
  if (address) *address = value;
  return ((int) (value+0.5));
}

//
//  Gets an interger value from a dialog box edit control with 
//  bounds checking, and a default value to use if no value can 
//  be extracted.
//
//      hwnd    -- Dialog box window pointer.
//      id      -- ID of edit control.
//      min_val -- Minimum acceptable value.
//      max_val -- Maximum acceptable value.
//      def     -- Default value (used if user clears the control, etc.).
//
//      RETURN  -- Return value is the inteteger.
//
int get_int (HWND hwnd,int id,int min_val,int max_val,int def) {
  int  i,err;
  i = GetDlgItemInt (hwnd,id,&err,true);
  if (!err) return (def);
  if (i < min_val) i = min_val;
  if (i > max_val) i = max_val;
  return (i);
}

//
//  This routine gets information about a specific menu item.  The 
//  information returned includes the ID and the text of the item.
//
//      menu     -- Menu or sub-menu to look in.
//      item     -- Menu item or position (from zero).
//      position -- If this is non-zero then item indicates a psotion,
//                  otherwise this idicates a menu item by id.
//      buffer   -- Location to return the text of the menu item.  
//                  This buffer should be atleast SIZE_BUFFER long to
//                  avoid an error.
//
//      RETURN   -- Return value is the ID of the menu item, or zero
//                  if this is not a valid menu item.
//
//  This routine replaced the old GetMenuItemText, and GetMenuItemID,
//  which are not supported under windows CE.
//  
long get_menudata (HMENU menu,int item,int position,TCHAR *buffer) {
  MENUITEMINFO info;
  info.cbSize     = sizeof(info);           // This is required, but not in the docs.
  info.fMask      = MIIM_ID | MIIM_TYPE;    // What we want back.
  info.dwTypeData = buffer;                 // Buffer setup
  info.cch        = SIZE_BUFFER;            // Size of buffer, also required but not in the docs.
  if (!GetMenuItemInfo(menu,item,position,&info)) return (0);
  return (info.wID);
}

//
//  Gets a string from the system resource and returns the string.
//
//  Care must be used in calling this routine since the routine returns the string in 
//  a static data space.  First this means there are two rules to follow:
//
//      1. The length of the string that can be recovered is limited by the the
//         static buffer.  Tis currently SIZE_BUFFER.  The filter strings are long!
//      2. There is only one static buffer so care must be taken in the use of 
//         the string.
//
//      id     -- ID of string to recover.
//
//      RETURN -- Pointer to static location containning the string.
//
TCHAR *get_string (int id) {
  static TCHAR buffer[SIZE_BUFFER];
  LoadString (language,id,buffer,SIZE_BUFFER);
  return (buffer);
}

//
//  This is a replacment for the system routines CreateDialogParam/CreateDialog.  This 
//  routien supports reading the templete from one source and the dialog from another.
//  This is necessary to allow custom controls defined in JWPce to be used in dialog 
//  boxes generated from templets in a DLL.  
//
//      id     -- ID for dialog box.
//      hwnd   -- Parent window.
//      proc   -- Dialog procedure.
//      param  -- Parameter (optional), passed to dialog procedure.
//
//      RETURN -- Pointer to dialog window generated.
//
HWND JCreateDialog (int id,HWND hwnd,DLGPROC proc,long param) {
  HRSRC   handle;
  HGLOBAL resource; 
  void   *data;
  handle   = FindResource(language,MAKEINTRESOURCE(id),RT_DIALOG);
  resource = LoadResource(language,handle);
  data     = LockResource(resource);
  return (CreateDialogIndirectParam(instance,(LPCDLGTEMPLATE) data,hwnd,proc,param));
}

//
//  This is a replacment for the system routines DialogBoxParam/DialogBox.  This 
//  routien supports reading the templete from one source and the dialog from another.
//  This is necessary to allow custom controls defined in JWPce to be used in dialog 
//  boxes generated from templets in a DLL.  
//
//      id     -- ID for dialog box.
//      hwnd   -- Parent window.
//      proc   -- Dialog procedure.
//      param  -- Parameter (optional), passed to dialog procedure.
//
//      RETURN -- Pointer to dialog window generated.
//
int JDialogBox (int id,HWND hwnd,DLGPROC proc,long param) {
  HRSRC   handle;
  HGLOBAL resource; 
  void   *data;
  handle   = FindResource(language,MAKEINTRESOURCE(id),RT_DIALOG);
  resource = LoadResource(language,handle);
  data     = LockResource(resource);
  return (DialogBoxIndirectParam(instance,(LPCDLGTEMPLATE) data,hwnd,proc,param));
}

//
//  This routine loads a null termianted image of a file into memroy.
//  This is used by various routines in the system.
//
//      name   -- Name of file to load from.
//
//      RETURN -- Pointer to allocated memory block, or NULL in the 
//                case of an error.
//
byte *load_image (tchar *name) {
  byte  *image;
  HANDLE handle;
  unsigned long i,done;
  if (INVALID_HANDLE_VALUE == (handle = CreateFile(name,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null))) return (NULL);
  i = GetFileSize(handle,NULL);
  if ((image = (byte *) calloc(1,i+6))) ReadFile(handle,image,i,&done,NULL);
  CloseHandle (handle);
  return (image);
}

//
//  This is an enhanced version of the system routine.
//
//      hwnd    -- Parent window for dialog box.
//      text    -- ID for text used in the message box.  This is indicates a printf style 
//                 format string.
//      caption -- ID for message box title.
//      type    -- Type for message box, all system types are supported.
//      ...     -- Parameters for use with the text string.
//
//      RETURN  -- MessageBox() return value.
//
int JMessageBox (HWND hwnd,int text,int caption,UINT type,...) {
  TCHAR buffer[SIZE_BUFFER],message[SIZE_BUFFER],title[SIZE_WORKING];
  va_list argptr;
  va_start   (argptr,type);
  GET_STRING (message,text);
  wvsprintf  (buffer,message,argptr);
  GET_STRING (title,caption);
  return (MessageBox(hwnd,buffer,title,type));
}

//
//  This routine is used to write a float value into an edit box.
//
//      hwnd  -- Window containning the edit box (dialog).
//      id    -- ID of edit box.
//      value -- Value to be written.
//      scale -- Scalling value.  The value written is divided by 
//               this value before being placed in the edit box.  
//               This is used to handle line spacing, and font point
//               size, both of which are turned into integers.
//  
void put_float (HWND hwnd,int id,float value,int scale) {
  TCHAR buffer[100];
  sprintf (buffer,TEXT("%g"),value/scale);
  SetDlgItemText (hwnd,id,buffer);
  return;
}

//
//  Generate an out of memory error.
//
void OutOfMemory (HWND hwnd) {
  JMessageBox (hwnd,IDS_ERROR_MEMORY,IDS_ERROR,MB_OK | MB_ICONWARNING);
  return;
}

//
//  Small stub rotuine for initializing a tabed dialog box.
//
//      ID     -- ID for the main window.
//      setup  -- A fully initialized setup structure, that describes each page and it's 
//                dialog box handler.
//
//      RETURN -- Unless the client main routine returns something, a true value indicates 
//                the user wants the values, and a false indicates he/she aborted the 
//                dialog box.
//
int TabDialog (int id,TabSetup *setup) {
  return (JDialogBox(id,main_window,(DLGPROC) tab_dialog,(LONG) setup));
}

//
//  Generate a simple Yes-No dialog box, and get input from the user.
//
//      format -- printf style format string passed to the dialog box.
//
//      RETURN -- A non-zero return value indicates yes.
//
int YesNo (int format,...) {
  TCHAR buffer[SIZE_WORKING],string[SIZE_WORKING];
  va_list argptr;
  va_start   (argptr,format);
  GET_STRING (string,format);
  wvsprintf  (buffer,string,argptr);
  return (IDYES == MessageBox(main_window,buffer,get_string(IDS_AREYOUSURE),MB_YESNO | MB_ICONQUESTION));
}






