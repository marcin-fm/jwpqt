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
//  This modlue handles the install properties of JWPce.
//
//  Noramlly every time JWPce runs, it checks to see if the file 
//  extensions are associated with JWPce.  If any of the exntensions
//  are not associated then it attempts to do an install.  The 
//  install consists of assigning the extensions to JWPce, and 
//  possibly putting JWPce in the start-menu and/or the desktop.
//  The program also checks to make sure the file associations point 
//  back to this executable.
//

#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_help.h"
#include "jwp_inst.h"
#include "shlobj.h"

#include "jwp_misc.h"

//--------------------------------
//
//  This routine saves a shell link.  A shell link is the name for 
//  a link object, in this case it will be an item in the start menu,
//  or an item on the desktop.
//
//      file  -- System Shell interface file structure already intiailized.
//      place -- Key indicating what system location to place the object.
//               The keys of interest are CSIDL_PROGRAMS (program directory 
//               of the start-menu) and CSIDL_DESKTOP (desktop).
//      dir   -- Subdirectory of the indicated directory to place the
//               object.
//

//--------------------------------
//
//  Windows CE version of the create link.
//
#ifdef WINCE
static void save_link (TCHAR *target,int place,tchar *dir) {
  TCHAR buffer[SIZE_BUFFER];
  get_folder (place,buffer);
  if (dir && *dir) {                                        // Add sub-directory
    lstrcat (buffer,TEXT("\\"));
    lstrcat (buffer,dir);
    CreateDirectory (buffer,NULL);
  }
  lstrcat (buffer,TEXT("\\JWPce.lnk"));                     // This needs to have the extension .lnk to make an actuall link on CE.
  DeleteFile       (buffer);
  SHCreateShortcut (buffer,target);
  return;
}
//
//  Windows 95/NT version of creating a link.
//
#else WINCE
static void save_link (IPersistFile *file,int place,tchar *dir) {
  TCHAR buffer[SIZE_BUFFER];

  get_folder (place,buffer);                                // Get base dir.
  if (dir && *dir) {                                        // Add sub-directory
    lstrcat (buffer,TEXT("\\"));
    lstrcat (buffer,dir);
    CreateDirectory (buffer,NULL);
  }
  lstrcat (buffer,TEXT("\\"));                              // Add name.
  lstrcat (buffer,TEXT("JWPce.lnk"));
#ifdef UNICODE
  file->Save (buffer,true);
#else
  WORD wbuffer[SIZE_BUFFER];
  MultiByteToWideChar (CP_ACP,0,buffer,-1,wbuffer,SIZE_BUFFER);
  file->Save (wbuffer,true);
#endif UNICODE
  return;
}
#endif WINCE

//===================================================================
//
//  Static definitions and data.
//

#define NUMBER_EXT          10  // Number of extensions used possibly used by the program.
#define EXT_JCP             8   // Project extension
#define EXT_JFC             9   // JFC extension.

                                // First install dialog box return codes.
#define INSTALL_ABORT       0   // Abort the install
#define INSTALL_ADVANCED    1   // Go to the advanced install
#define INSTALL_OK          2   // Causes a simple automatic install.

//--------------------------------
//
//  Structure used to pass information to the advanced install dialog box.
//
struct adv_install {
  byte  ext[10];            // Indicates which extensions should be disabled 
                            //   (because they are already associated with JWPce.
  int   start;              // If non-zero causes the program to be placed in the Start Menu.
  int   desktop;            // If non-zero causes the program to be placed on the desktop.
  TCHAR group[SIZE_BUFFER]; // Group to place program in in the start menu.
};

static struct adv_install *adv_install; // Instance.

//===================================================================
//
//  Dialog box procedures.
//

//--------------------------------
//
//  Dialog box function for the advnaced install dialog box.
//
//  On entrance, the adv_isntall structure is filled out.  The ext
//  array indicates which extensions buttons are active (disabled 
//  buttons have already been assigned to JWPce).  On exit the ext
//  field indicates which items should be assigned to JWPce.
//
//      IDC_AISTART     Start-menu check box.
//      IDC_AIGROUP     Start-menu group.
//      IDC_AIDESKTOP   Desktop checkbox.
//      IDC_AIJCE       First extension (the others are in series).
//
static BOOL CALLBACK dialog_advinstall (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  int i;
  switch (message) {
    case WM_INITDIALOG:
         for (i = 0; i < NUMBER_EXT; i++) {
           CheckDlgButton (hwnd,IDC_AIJCE+i,true);
           EnableWindow (GetDlgItem(hwnd,IDC_AIJCE+i),adv_install->ext[i]);
         }
         CheckDlgButton (hwnd,IDC_AISTART  ,adv_install->start);
         CheckDlgButton (hwnd,IDC_AIDESKTOP,adv_install->desktop);
         SetDlgItemText (hwnd,IDC_AIGROUP  ,adv_install->group);
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_INSTALL_ADVANCED);
         return  (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           INPUT_CHECK (IDC_AIGROUP);
           case IDC_AISTART:
                EnableWindow (GetDlgItem(hwnd,IDC_AIGROUP),IsDlgButtonChecked(hwnd,IDC_AISTART));
                return (true);
           case IDCANCEL:
                EndDialog (hwnd,true);
                return    (true);
           case IDOK:
                for (i = 0; i < NUMBER_EXT; i++) adv_install->ext[i] = IsDlgButtonChecked(hwnd,IDC_AIJCE+i);
                adv_install->start   = IsDlgButtonChecked(hwnd,IDC_AISTART);
                adv_install->desktop = IsDlgButtonChecked(hwnd,IDC_AIDESKTOP);
                GetDlgItemText (hwnd,IDC_AIGROUP,adv_install->group,SIZE_BUFFER);
                EndDialog (hwnd,false);
                return    (true);
         }
         break;
  }
  return (false);
}

//--------------------------------
//
//  Dialog box handler for the first install dialog box.  This dialog box 
//  allows the user to select advanced install, automatic install,
//  abort the install, and/or supress further checking for installs.
//
//      ID_IDADVANCED   Advanced install button.
//      ID_IDINSTALL    Check-box to supress further checking.
//
static BOOL CALLBACK dialog_install (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  switch (message) {
    case WM_INITDIALOG:
         CheckDlgButton (hwnd,IDC_IDINSTALL,!jwp_config.cfg.install);
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_INSTALL_MORE);
         return  (true);
    case WM_COMMAND:
         jwp_config.cfg.install = !IsDlgButtonChecked(hwnd,IDC_IDINSTALL);
         switch (LOWORD(wParam)) {
           case IDC_IDADVANCED:
                EndDialog (hwnd,INSTALL_ADVANCED);
                return    (true);
           case IDCANCEL:
                EndDialog (hwnd,INSTALL_ABORT);
                return    (true);
           case IDOK:
                EndDialog (hwnd,INSTALL_OK);
                return    (true);
         }
         break;
  }
  return (false);
}

//===================================================================
//
//  Static routines.
//

//--------------------------------
//
//  This routine gets the value of a key from the registry.
//
//      subkey -- Subkey to get.  All keys retreived here are from the 
//                HKEY_CLASSES_ROOT class.
//      buffer -- Buffer to hold the result.  This should be SIZE_BUFFER
//                in length.
//
//      RETURN -- A non-zero return value indicates an error.
//
static int get_key (tchar *subkey,TCHAR *buffer) {
  HKEY    hkey;
  ulong   size,type;
  size = SIZE_BUFFER;
  if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_CLASSES_ROOT,subkey,0,KEY_QUERY_VALUE,&hkey)) return (true);
  RegQueryValueEx (hkey,NULL,NULL,&type,(byte *) buffer,&size);
  RegCloseKey     (hkey);
  return (false);
}

//--------------------------------
//
//  Sets the value in a registery key.  All keys set by this routine are
//  in the HKEY_CLASSES_ROOT class.
//
//      subkey -- Key location.
//      format -- Format for key value, followed by sprintf parameters.
//
#define ZSTRSIZE(x) (sizeof(TCHAR)*(lstrlen(x)+1))

static void make_key (tchar *subkey,tchar *format,...) {
  HKEY    hkey;
  TCHAR   value[SIZE_BUFFER];
  ulong   type;
  va_list argptr;
  va_start  (argptr,format);
  wvsprintf (value ,format,argptr);
  RegCreateKeyEx (HKEY_CLASSES_ROOT,subkey,null,null,0,KEY_ALL_ACCESS,null,&hkey,&type);
  RegSetValueEx  (hkey,null,null,REG_SZ,(byte *) value,ZSTRSIZE(value));
  RegCloseKey    (hkey);
  return;
}

//===================================================================
//
//  Exported routines.
//

//--------------------------------
//
//  Checks to see if JWPce is installed, and if it is not allows the 
//  user to install it.
//
//      force -- If set, will force JWPce to allow an install.  Can be 
//               used to generate start-menu/desktop items when JWPce 
//               cannot tell if the item already exists.
//
#define EXT_LIMIT   (NUMBER_EXT-2)
#define REG_COMMAND TEXT("JWPce\\Shell\\Open\\Command")
#define REG_JFCFILE TEXT("JFC\\Shell\\Edit\\Command")
#define REG_PROJECT TEXT("JWPce-project\\Shell\\Open\\Command")

void do_install (int force) {
  static tchar* file_ext[] = {TEXT(".jce"),TEXT(".jwp"),TEXT(".euc"),TEXT(".sjs"),TEXT(".jis"),TEXT(".old"),TEXT(".nec"),TEXT(".utf")  /*,TEXT(".jcp"),TEXT(".jfc")*/ };
  int    ok,i;
  TCHAR  buffer[SIZE_BUFFER],command[SIZE_BUFFER],executable[SIZE_BUFFER];
  struct adv_install install;

  if (!force && !jwp_config.cfg.install) return;    // User does not want check.
  ok = !force;
//
//  Initialize values.
//
  adv_install     = &install;
  install.start   = true;
  install.desktop = false;
  GET_STRING (install.group,IDS_INST_GROUP);
  if (!GetModuleFileName(instance,executable,SIZE_BUFFER)) return;
  sprintf (command,TEXT("\"%s\""),executable);
//
//  Check extensions.
//
  for (i = 0; i < EXT_LIMIT; i++) {
    if      (get_key(file_ext[i]   ,buffer)) { install.ext[i] = true; ok = false; }
    else if (!stricmp(TEXT("JWPce"),buffer)) { install.ext[i] = false;            }
    else                                     { install.ext[i] = true; ok = false; }
  }
  if      (get_key(TEXT(".jcp")          ,buffer)) { install.ext[EXT_JCP] = true;  ok = false; }
  else if (!stricmp(TEXT("JWPce-project"),buffer)) { install.ext[EXT_JCP] = false;             }
  else                                             { install.ext[EXT_JCP] = false; ok = false; }
  if      (get_key(TEXT(".jfc")          ,buffer)) { install.ext[EXT_JFC] = true;  ok = false; }
  else if (!stricmp(TEXT("JFC")          ,buffer)) { install.ext[EXT_JFC] = false;             }
  else                                             { install.ext[EXT_JFC] = false; ok = false; }
//
//  Check executable command.
//
  if      (get_key(REG_COMMAND,buffer))               { ok = false;                              }
  else if (strnicmp(command,buffer,lstrlen(command))) { ok = false;                              }
  if      (get_key(REG_PROJECT,buffer))               { ok = false; install.ext[EXT_JCP] = true; }
  else if (strnicmp(command,buffer,lstrlen(command))) { ok = false; install.ext[EXT_JCP] = true; }
  if      (get_key(REG_JFCFILE,buffer))               { ok = false; install.ext[EXT_JFC] = true; }
  else if (strnicmp(command,buffer,lstrlen(command))) { ok = false; install.ext[EXT_JFC] = true; }
//
//  Do we need to do an install.
//  
  if (ok) return;
//
//  Do the dialog boxes.
//
  switch (JDialogBox(IDD_INSTALL,main_window,(DLGPROC) dialog_install)) {
    case INSTALL_ABORT:
         return;
    case INSTALL_ADVANCED:
         if (JDialogBox(IDD_ADVINSTALL,main_window,(DLGPROC) dialog_advinstall)) return;
         break;
    case INSTALL_OK:
         break;
  }
//
//  Install the extensions and the file association.
//
  for (i = 0; i < EXT_LIMIT; i++) {
    if (install.ext[i]) make_key (file_ext[i],TEXT("JWPce"));
  }
  make_key (TEXT("JWPce"             ),get_string(IDS_INST_FILETYPE));
  make_key (TEXT("JWPce\\DefaultIcon"),TEXT("%s,-%d"),executable,IDI_FILEICON);
  make_key (REG_COMMAND               ,TEXT("%s \"%%1\""),command);
//
//  Install the project extension and file association.
//
  if (install.ext[EXT_JCP]) {
    make_key (TEXT(".jcp"),TEXT("JWPce-project"));
    make_key (TEXT("JWPce-project"             ),get_string(IDS_INST_PROJECTTYPE));
    make_key (TEXT("JWPce-project\\DefaultIcon"),TEXT("%s,-%d"),executable,IDI_PROJECTICON);
    make_key (REG_PROJECT                       ,TEXT("%s \"%%1\""),command);
  }
//
//  Install the JFC file extension and association.
//
  if (install.ext[EXT_JFC]) {
    make_key (TEXT(".jfc"),TEXT("JFC"));
    make_key (REG_JFCFILE,TEXT("%s \"%%1\""),command);
  }
//
//  Check for start-menu and/or desktop options.
//
  if (install.start || install.desktop) {
//
//  Create links for Windows CE.
//
#ifdef WINCE
    if (install.start  ) save_link (command,CSIDL_PROGRAMS,install.group);
    if (install.desktop) save_link (command,CSIDL_DESKTOP ,NULL         );
//
//  Create links for Windows 95/NT
//
#else WINCE
    IShellLink   *link; 
    IPersistFile *file; 
    if (S_OK == CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLink,(void **) &link)) {
      link->SetPath        (executable);        // This is what the shortcut points to.
      link->SetDescription ("JWPce");           // This is what will show up in the start-menu.
      if (S_OK == link->QueryInterface(IID_IPersistFile,(void **) &file)) {     // Create file object.
        if (install.start  ) save_link (file,CSIDL_PROGRAMS,install.group);
        if (install.desktop) save_link (file,CSIDL_DESKTOP ,NULL         );
        file->Release ();                       // Done with file.
      }
    }
    link->Release ();                           // Done with link.
#endif WINCE
  }
  return;
}





