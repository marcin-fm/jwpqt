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
#include "jwpce.h"
#include "jwp_cach.h"
#include "jwp_edit.h"
#include "jwp_help.h"
#include "jwp_misc.h"
#include <commctrl.h>   // Needed for tab controls
#include <shellapi.h>
#include <shlobj.h>

//#define FIXED_LOCATIONS   // This generates fixed file loations for older CE machines.

//===================================================================
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

//--------------------------------
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

//--------------------------------
//
//  Deallocate memory associated with the string.
//
void KANJI_string::free () {
  if (kanji) ::free (kanji);
  kanji = NULL;
  return;
}

//--------------------------------
//
//  Get the string value from a Japanese edit box.
//
//      hwnd -- Dialog box containning the edit box.
//      ID   -- ID of the edit box.
//
void KANJI_string::get (HWND hwnd,int id) {
  int    i;
  KANJI *k;
  i = JEGetDlgItemText(hwnd,id,&k);
  free ();
  if (!i) return;
  set (k,i);
  return;
}

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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
//===================================================================

//===================================================================
//
//  Begin class SIZE_window
//
//  This class contains a coloection of data an routines used to manage
//  resizable dialog boxes.  In reallity we only allow resizing on 
//  dialog boxes containning Japnese List constrols.  The idea here is 
//  to be able to see more of the list.  We generally require that the
//  list control be in the lower right hand corner of the dialog.  All
//  the buttons generally stay put, but the list box streaturches to 
//  fill the changing space.  This is done by keeping track of the 
//  dialog default size as well as the default size of the list control
//  this can be used to do all the calculations we need.
//
//  This class also provides a way to store the basic size information
//  in the configuation structure so we can open the dialog where the
//  user wants it again.
//
//  A acception to the constraints is made for the OK and CANCEL
//  controls.  The option of allowing these to be below the 
//  list control is supported.
//
//

//--------------------------------
//
//  Constructor
//
SIZE_window::SIZE_window () {
#ifndef WINCE_POCKETPC
  memset (this,0,sizeof(class SIZE_window));
#endif WINCE_POCKETPC
  return;
}

//--------------------------------
//
//  This routine handles the dynamic controls.  These are controls that are
//  revieled and activated if the dialog box is extended enough to make them
//  visible.
//
void SIZE_window::check_controls () {
#ifndef WINCE_POCKETPC
  int  i;
  HWND hwnd;
  RECT drect,rect;
  if (!first) return;                       // No dynamic controls
  GetWindowRect (dlg,&drect);   
  for (i = first; i <= last; i++) {
    hwnd = GetDlgItem(dlg,i);
    GetWindowRect (hwnd,&rect);
    if ((rect.bottom < drect.bottom-(rect.bottom-rect.top)/2) && (rect.right < drect.right)) {      // Didn't use GetSystemMetrics() for CE devices.
      EnableWindow (hwnd,true);             // Control is visible so actvate it.
      ShowWindow   (hwnd,SW_SHOW);
    }
    else {                                  // Control is not visible so disable it.
      EnableWindow (hwnd,false);
      ShowWindow   (hwnd,SW_HIDE);
    }
  }
#endif WINCE_POCKETPC
  return;
}

//--------------------------------
//
//  Intialize call.  This should be called in the dialog WM_INITDIALOG
//  message and sets the base values.
//
//      hwnd    -- Dialog window.
//      id      -- ID of the list control.
//      init    -- Pointer to a size_window structure.  This canotains the location and
//                 size of the user's last use of the dialog.  These are generally 
//                 stored in the configuration stucture.  If this is NULL, then the 
//                 dialog will open at the default size and location.  The size and 
//                 location will not ve saved.
//      buttons -- If non-zero this indicate the OK and CANCEL buttons are below
//                 the list and must be moved along with the list control.
//      dfirst  -- First dynamic button (Use zero if there are no dynamic buttons).
//      dlast   -- Last dynamic button.  These are controls that are activated as the 
//                 size of the dialog box gets big enough to show them
//
void SIZE_window::wm_init (HWND hwnd,int id,struct size_window *init,int buttons,int dfirst,int dlast) {
#ifndef WINCE_POCKETPC
  RECT rect;
  move_buttons = buttons;                           // Save buttons option.
  dlg          = hwnd;                              // Save dialog
  list         = GetDlgItem(hwnd,id);               // Save list
  save         = init;                              // Save intitializer
  first        = dfirst;                            // Save dynamic buttons
  last         = dlast;
  GetWindowRect (list,&rect);                       // Get list default size.
  lst_xmin = rect.right-rect.left;
  lst_ymin = rect.bottom-rect.top;
  GetWindowRect (dlg,&rect);                        // Get dialog default size.
  dlg_xmin = rect.right-rect.left;
  dlg_ymin = rect.bottom-rect.top;
  GetWindowRect (GetDlgItem(hwnd,IDOK),&rect);      // Get location of OK button.
  check_controls ();                                // Check controls for visible.
  btn_y    = rect.top;                              // Restore window settings.
//
//  Do we need to adjust the dialog.
//
  if (!init) return;                                            // No intialize so cannot setup
  if ((save->sx < dlg_xmin) || (save->sy < dlg_ymin)) return;   // Save is smaller than initialize dailog.
#ifndef WINCE
  if ((save->sx > dlg_xmin) || (save->sy > dlg_ymin)) MoveWindow (hwnd,save->x,save->y,save->sx,save->sy,true);
#else   WINCE
  if ((save->sx > dlg_xmin) || (save->sy > dlg_ymin)) ShowWindow (hwnd,SW_MAXIMIZE);
#endif  WINCE
#endif  WINCE_POCKETPC
  return;
}

//--------------------------------
//
//  This routine keeps track of dialog box moves so we can restore the dialog
//  box position.  This should be called as a responce to a WM_MOVE message.
//
void SIZE_window::wm_move () {
#ifndef WINCE_POCKETPC
  RECT rect;
  if (!save) return;
  GetWindowRect (dlg,&rect);
  save->x = rect.left;
  save->y = rect.top;
#endif  WINCE_POCKETPC
  return;
}

//--------------------------------
//
//  This routine is called whenever the size of the dialog is changed.  This should
//  be called as a responce to a WM_SIZE message.  This will correct the position 
//  and size of the list control as well as move the buttons if necessary.
//  Note that if we change the size of the list control, it will get a WM_SIZE
//  message and reform.
//
//      wParam -- wParam from the WM_SIZE message.  Used to dectect minimize.
//
void SIZE_window::wm_size (int wParam) {
#ifndef WINCE_POCKETPC
#ifndef WINCE
  RECT         rect;
  size_window *size,temp;
  if (wParam == SIZE_MINIMIZED) return;
  GetWindowRect (dlg,&rect);                        // Take care of the window parameters
  if (save) size = save; else size = &temp;
  size->sx = rect.right-rect.left;
  size->sy = rect.bottom-rect.top; 
  size->x  = rect.left;
  size->y  = rect.top;
  check_controls ();
  GetWindowRect  (list,&rect);                      // Take care of the list control
  MoveWindow     (list,rect.left-size->x-GetSystemMetrics(SM_CXFRAME),rect.top-size->y-GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYFRAME),lst_xmin+size->sx-dlg_xmin,lst_ymin+size->sy-dlg_ymin,true);
  if (move_buttons) {                               // Move the OK and CANCEL buttons if necessary.
    HWND button;
    button = GetDlgItem(dlg,IDOK);
    GetWindowRect (button,&rect);
    MoveWindow (button,rect.left-size->x-GetSystemMetrics(SM_CXFRAME),btn_y+size->sy-dlg_ymin-size->y-GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYFRAME),rect.right-rect.left,rect.bottom-rect.top,true);
    button = GetDlgItem(dlg,IDCANCEL);
    GetWindowRect (button,&rect);
    MoveWindow (button,rect.left-size->x-GetSystemMetrics(SM_CXFRAME),btn_y+size->sy-dlg_ymin-size->y-GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYFRAME),rect.right-rect.left,rect.bottom-rect.top,true);
  }
#else  WINCE
  RECT         rect;
  size_window *size,temp = { 0,0,0,0 };
  GetWindowRect (dlg,&rect);                        // Take care of the window parameters
  if (save) size = save; else size = &temp;
  size->sx = rect.right-rect.left;
  size->sy = rect.bottom-rect.top; 
  size->x  = rect.left;
  size->y  = rect.top;
  check_controls ();
  GetWindowRect  (list,&rect);                     // Take care of the list control
  if (wParam == SIZE_MAXIMIZED) {
    MoveWindow (list,rect.left-size->x-GetSystemMetrics(SM_CXDLGFRAME)-1,rect.top-size->y-GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYDLGFRAME)-1,lst_xmin+size->sx-dlg_xmin,lst_ymin+size->sy-dlg_ymin,true);
  }
  else {
    MoveWindow (list,rect.left-size->x-GetSystemMetrics(SM_CXDLGFRAME),rect.top-size->y-GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYDLGFRAME),lst_xmin+size->sx-dlg_xmin,lst_ymin+size->sy-dlg_ymin,true);
  }
#endif WINCE
#endif WINCE_POCKETPC
  return;
}

//--------------------------------
//
//  Called during a sizing operation, this is used to prevent the user
//  from reducing the dialog below the minimum size.  This routine should
//  be called as a responce to a WM_SIZING message.
//
//      rect -- Proposized sizing rectange.  This is actually passed to
//              the WM_SIZING message in lParam.
//
void SIZE_window::wm_sizing (RECT *rect) {
#ifndef WINCE_POCKETPC
  if (rect->right-rect->left < dlg_xmin) rect->right  = rect->left+dlg_xmin;
  if (rect->bottom-rect->top < dlg_ymin) rect->bottom = rect->top +dlg_ymin;
#endif WINCE_POCKETPC
  return;
}

//
//  End class SIZE_window.
//
//===================================================================

//===================================================================
//
//  Static rotuines.
//

//--------------------------------
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
  setup = (TabSetup *) GetWindowLong(hwnd,DWL_USER);    // The user setup, passed in lParam, and saved in the DWL_USER window param
  switch (msg) {
    case WM_INITDIALOG:
         tab   = GetDlgItem(hwnd,IDC_TABCONTROL);       // The tab control.
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
         POCKETPC_DIALOG   (hwnd);
         goto SetPage;
    case WM_HELP:
         do_help (hwnd,setup->pages[setup->page].help);
         return  (true);
    case WM_NOTIFY:
         switch (((LPNMHDR) lParam)->code) {
           case TCN_SELCHANGING:                        // Page is going out of view so hide it.
                tab       = GetDlgItem(hwnd,IDC_TABCONTROL);
                item.mask = TCIF_PARAM;
                TabCtrl_GetItem(tab,TabCtrl_GetCurSel(tab),&item);
                ShowWindow ((HWND) (item.lParam),SW_HIDE);
                return (false);
           case TCN_SELCHANGE:                          // Page is coming into view so show it.
SetPage:;
                tab         = GetDlgItem(hwnd,IDC_TABCONTROL);
                item.mask   = TCIF_PARAM;
                i           = TabCtrl_GetCurSel(tab);
                setup->page = i;
                TabCtrl_GetItem (tab,i,&item);
                ShowWindow (page = ((HWND) (item.lParam)),SW_SHOW);
                BringWindowToTop (page);
                if (msg == WM_INITDIALOG) SetFocus (GetNextDlgTabItem(page,null,false));
                return (false);                
#ifndef WINCE_POCKETPC
           case IDCANCEL:                               // Just exit.
                EndDialog (hwnd,false);
                return    (true);
#endif  WINCE_POCKETPC
         }
         break;
    case WM_COMMAND:
         switch (LOWORD(wParam)) { 
           case IDOK:                                   // Keep the changes, so read all pages.
                tab       = GetDlgItem(hwnd,IDC_TABCONTROL);
                item.mask = TCIF_PARAM;
                for (i = 0; i < setup->count; i++) {
                  TabCtrl_GetItem (tab,i,&item);
                  SendMessage ((HWND) (item.lParam),WM_GETDLGVALUES,0,0);
                }
                EndDialog (hwnd,true);
				return    (true);
           case IDCANCEL:                               // Just exit.
                EndDialog (hwnd,false);
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

//===================================================================
//
//  Exported structures.
//

SCROLLINFO scroll_info = { sizeof(SCROLLINFO),SIF_ALL | SIF_DISABLENOSCROLL,0,0,0,0,0 };    

//===================================================================
//
//  Exported routines.
//

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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

//--------------------------------
//
//  Generates a button dialog.
//
//      HWND   -- Parent window (NULL will use the main window).
//      idd    -- Diolog box id.
//      data   -- Data for the static text item.  A value of NULL should
//                be used if there is not static text item.
//      help   -- Help id for dialog
//
//      RETURN -- The ID of the button selected that termianted the 
//                dialog.
//
int ButtonDialog (HWND hwnd,int idd,tchar *data,int help) {
  if (!hwnd) hwnd = main_window;
  button_data = data;
  button_help = help;
  return (JDialogBox(idd,hwnd,(DLGPROC) dialog_button));
}

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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

//--------------------------------
//
//  Get a system folder.
//
//      id -- Folder ID.
//      buffer -- Buffer for folder path.
//
//      RETURN -- Pointer to buffer.
//
TCHAR *get_folder (int id,TCHAR *buffer) {
#ifndef FIXED_LOCATIONS
  LPITEMIDLIST item  = NULL;
  LPMALLOC     alloc = NULL;
  SHGetMalloc                (&alloc);
  SHGetSpecialFolderLocation (NULL,id,&item);
  SHGetPathFromIDList        (item,buffer); 
  alloc->Free    (item);
  alloc->Release ();
  return (buffer);
#else  FIXED_LOCATIONS
  switch (id) {
    case CSIDL_DESKTOP:
         lstrcpy (buffer,TEXT("\\windows\\desktop"));
         break;
    case CSIDL_PROGRAMS:
         lstrcpy (buffer,TEXT("\\windows\\start menu\\programs"));
         break;
    default:
    case CSIDL_PERSONAL:
         lstrcpy (buffer,TEXT("\\my documents"));
         break;
  }
  return (buffer);
#endif FIXED_LOCATIONS
}

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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

//--------------------------------
//
//  Duplicate a kanji string.  Takes care of the case of the input string being NULL, or 
//  the length being zero.
//
//      string -- String to duplciate.
//      length -- Length of string.
//
//      RETURN -- Dulicated string.  Null indicates an error, or no input string.
//
KANJI *kstrdup (KANJI *string,int length) {
  KANJI *dup;
  if (!string || !length) return (NULL);
  if (!(dup = (KANJI *) malloc(length*sizeof(KANJI)))) return (NULL);
  memcpy (dup,string,length*sizeof(KANJI));
  return (dup);
}

//--------------------------------
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

//--------------------------------
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

//--------------------------------
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

//--------------------------------
//
//  Generate an out of memory error.
//
void OutOfMemory (HWND hwnd) {
  JMessageBox (hwnd,IDS_ERROR_MEMORY,IDS_ERROR,MB_OK | MB_ICONWARNING);
  return;
}

//--------------------------------
//
//  A string is loaded from the string table.  The tab characters are converted to ascii 0.
//  This routine is used to support file requestors.
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
//      id2    -- ID for additional part of the string.  If these are both 
//                present, the two strings will be added together.
//
//      RETURN -- Pointer to static location containning the string.
//
TCHAR *tab_string (int id,int id2) {
  TCHAR *ptr,*p;
  ptr = get_string(id);
  if (id2) LOAD_STRING (ptr+lstrlen(ptr),id2,SIZE_BUFFER-lstrlen(ptr));
  for (p = ptr; *p; p++) {
    if (*p == '\t') *p = 0;
  }
  return (ptr);
}

//--------------------------------
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

//--------------------------------
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

//===================================================================
//
//  PPC and PocketPC routines for halding the list of recent files.
//
//  In Windows or HPC systems, the list is kept in the actual menu, since
//  the screen allows display of the full name.  In PPC and PocketPC 
//  systems only part of the name is shown.  This requires keeping two 
//  lists of files.
//

#if (defined(WINCE_POCKETPC) || defined(WINCE_PPC))

//--------------------------------
//
//  Class that manages the list of recent file names.
//
class RecentList recent_list;   // Class instance.

extern HMENU button_menu;       // This is defined in jwp_flio.cpp and is used only for PocketPCs

//--------------------------------
//
//  Constructor
//
RecentList::RecentList () {
  int i;
  for (i = 0; i < RECENT_MAX; i++) list[i][0] = 0;              // All strings are set to zero length
  return;
}

//--------------------------------
//
//  Insert an item into the list.
//
//      menu -- Menu containning the list so It can be changes.
//      pos  -- Menu position.
//      text -- Text to be inserted.
//
void RecentList::insert (HMENU menu,int pos,TCHAR *text) {
  TCHAR buffer[SIZE_BUFFER];
  int   i;
  for (i = RECENT_MAX-2; i >= pos; i--) lstrcpy (list[i+1],list[i]);
  lstrcpy    (list[pos],text);
  if (lstrlen(text) <= RECENT_LEN) lstrcpy (buffer,text);
    else {
      buffer[0] = text[0];
      buffer[1] = text[1];
      lstrcpy (buffer+2,TEXT(" ..."));
      lstrcat (buffer,text+lstrlen(text)-RECENT_LEN);
    }
  InsertMenu (menu,pos,MF_BYPOSITION,IDM_FILE_FILES_BASE+pos,buffer);
#ifdef WINCE_POCKETPC
  InsertMenu (button_menu,pos,MF_BYPOSITION,IDM_FILE_FILES_BASE+pos,buffer);
#endif WINCE_POCKETPC
  return;
}

//--------------------------------
//
//  Remove item from the list.
//
//      menu -- Pointer to the menu system.
//      pos  -- Item to be removed.
//
void RecentList::remove (HMENU menu,int pos) {
  int i;
  for (i = pos; i <= RECENT_MAX-2; i++) lstrcpy (list[i],list[i+1]);
  list[RECENT_MAX-1][0] = 0;
  RemoveMenu (menu,pos,MF_BYPOSITION);
#ifdef WINCE_POCKETPC
  RemoveMenu (button_menu,pos,MF_BYPOSITION);
#endif WINCE_POCKETPC
  return;
}

//--------------------------------
//
//  Replacemnt for the get_menudata() utility routine that is in jfc_misc.cpp.
//  This is basically the same as the normal get_menudata(), except that 
//  instead of getting the string from the menu, it gets the string from the list.
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
long get_menudata (HMENU menu,int item,int position,TCHAR *buffer) {
  MENUITEMINFO info;
  info.cbSize     = sizeof(info);           // This is required, but not in the docs.
  info.fMask      = MIIM_ID | MIIM_TYPE;    // What we want back.
  info.dwTypeData = buffer;                 // Buffer setup
  info.cch        = SIZE_BUFFER;            // Size of buffer, also required but not in the docs.
  if (!GetMenuItemInfo(menu,item,position,&info)) return (0);
  if (!position) item = item-IDM_FILE_FILES_BASE;
  lstrcpy (buffer,recent_list.get(item));
  return (info.wID);
}

#else  WINCE_POCKETPC

//--------------------------------
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

#endif WINCE_POCKETPC

//
//===================================================================





























#ifdef WINCE
void draw_frame (HDC hdc,HWND hwnd) {
  HPEN   pen;
  HBRUSH brush;
  RECT   rect;
  pen   = (HPEN)   SelectObject(hdc,CreatePen(PS_SOLID,1,GetSysColor(COLOR_WINDOWTEXT)));
  brush = (HBRUSH) SelectObject(hdc,CreateSolidBrush(GetSysColor(COLOR_WINDOW)));
  GetClientRect (hwnd,&rect);
  Rectangle     (hdc,rect.left,rect.top,rect.right,rect.bottom);
  DeleteObject  (SelectObject(hdc,pen));
  DeleteObject  (SelectObject(hdc,brush));
  return;
}
#endif WINCE




