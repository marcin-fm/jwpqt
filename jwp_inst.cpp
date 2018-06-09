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
#include "jwp_misc.h"
#include "shlobj.h"

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
  lstrcat (buffer,TEXT("JWPxp.lnk"));
#ifdef UNICODE
  file->Save (buffer,true);
#else
  WCHAR wbuffer[SIZE_BUFFER];
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
#define EXT_JPR             8   // JWPxp project extension (was JCP).
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
         EnableWindow   (GetDlgItem(hwnd,IDC_AIGROUP),IsDlgButtonChecked(hwnd,IDC_AISTART));
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

static HKEY hk_classes = 0;

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
  if (ERROR_SUCCESS != RegOpenKeyEx(hk_classes?hk_classes:HKEY_CLASSES_ROOT,subkey,0,KEY_QUERY_VALUE,&hkey)) return (true);
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

static bool notified;

static void make_key (tchar *subkey,tchar *format,...) {
  HKEY    hkey = 0;
  TCHAR   value[SIZE_BUFFER];
  LSTATUS err = 0;
  va_list argptr;
  va_start  (argptr,format);
  wvsprintf (value ,format,argptr);
  err |= RegCreateKeyEx (hk_classes?hk_classes:HKEY_CLASSES_ROOT,subkey,null,null,0,KEY_ALL_ACCESS,null,&hkey,null);    // Access classes at (HKLM|HKCU)/Software/Classes instead of the composite HKCR (which may not work for unprivileged users).
  err |= RegSetValueEx  (hkey,null,null,REG_SZ,(byte *) value,ZSTRSIZE(value));
  err |= RegCloseKey    (hkey);
  if (err && !notified) {
#ifdef _DEBUG
    MPRINTF (TEXT("Error creating/modifying registry key.\n\n%s\n\n%s"),subkey,value);
#else
    ErrorMessage (true,IDS_REG_ERROR_WRITE);
    notified = true;                                // Don't report subsequent errors on this installation attempt.
#endif
  }
  return;
}

#define EXT_LIMIT   (NUMBER_EXT-2)
#define REG_COMMAND TEXT("JWPxp\\Shell\\Open\\Command")
#define REG_JFCFILE TEXT("JFC\\Shell\\Edit\\Command")
#define REG_PROJECT TEXT("JWPxp-project\\Shell\\Open\\Command")

#define SUBKEY_CLASSES TEXT("Software\\Classes")

static tchar* file_ext[] = {TEXT(".jce"),TEXT(".jwp"),TEXT(".euc"),TEXT(".sjs"),TEXT(".jis"),TEXT(".old"),TEXT(".nec"),TEXT(".utf")  /*,TEXT(".jpr"),TEXT(".jfc")*/ };

//--------------------------------
//
//  Check extension assignments in registry.
//
//      install -- adv_install structure, the ext field will be filled in.
//
//      RETURN -- A non-zero return value indicates everything is already installed.
//
static int check_extensions (struct adv_install &install) {
  int    ok,i;
  TCHAR  buffer[SIZE_BUFFER],command[SIZE_BUFFER],executable[SIZE_BUFFER];
//
//  Initialize values.
//
  ok = true;
  GetModuleFileName(instance,executable,SIZE_BUFFER);
  sprintf (command,TEXT("\"%s\""),executable);
//
//  Check extensions.
//
  for (i = 0; i < EXT_LIMIT; i++) {
    if      (get_key(file_ext[i]   ,buffer)) { install.ext[i] = true; ok = false; }
    else if (!stricmp(TEXT("JWPxp"),buffer)) { install.ext[i] = false;            }
    else                                     { install.ext[i] = true; ok = false; }
  }
  if      (get_key(TEXT(".jpr")          ,buffer)) { install.ext[EXT_JPR] = true;  ok = false; }
  else if (!stricmp(TEXT("JWPxp-project"),buffer)) { install.ext[EXT_JPR] = false;             }
  else                                             { install.ext[EXT_JPR] = false; ok = false; }
  if      (get_key(TEXT(".jfc")          ,buffer)) { install.ext[EXT_JFC] = true;  ok = false; }
  else if (!stricmp(TEXT("JFC")          ,buffer)) { install.ext[EXT_JFC] = false;             }
  else                                             { install.ext[EXT_JFC] = false; ok = false; }
//
//  Check executable command.
//
  if      (get_key(REG_COMMAND,buffer))               { ok = false;                              }
  else if (strnicmp(command,buffer,lstrlen(command))) { ok = false;                              }
  if      (get_key(REG_PROJECT,buffer))               { ok = false; install.ext[EXT_JPR] = true; }
  else if (strnicmp(command,buffer,lstrlen(command))) { ok = false; install.ext[EXT_JPR] = true; }
  if      (get_key(REG_JFCFILE,buffer))               { ok = false; install.ext[EXT_JFC] = true; }
  else if (strnicmp(command,buffer,lstrlen(command))) { ok = false; install.ext[EXT_JFC] = true; }

  return ok;
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

void do_install (int force) {
  int    ok,i,iresult,forall;
  TCHAR  command[SIZE_BUFFER],executable[SIZE_BUFFER];
  struct adv_install install;

  if (!force && !jwp_config.cfg.install) return;    // User does not want check.
  ok = !force;
//
//  Initialize values.
//
  adv_install     = &install;                       // This is safe as adv_install is currently only referenced at this level of scope or lower.
  install.start   = true;
  install.desktop = false;
  GET_STRING (install.group,IDS_INST_GROUP);
  if (!GetModuleFileName(instance,executable,SIZE_BUFFER)) return;
  sprintf (command,TEXT("\"%s\""),executable);
  forall     = 0;                                   // Do not install links for all users.
  hk_classes = 0;                                   // This will cause several registry functions to default to using HKCR.
//
//  Do we need to do an install.
//  
  if (check_extensions (install) && ok) return;     // Side effect: determines which extensions need to be installed.
//
//  Do the dialog boxes.
//
  if (INSTALL_ABORT == (iresult = JDialogBox(IDD_INSTALL,main_window,(DLGPROC) dialog_install))) return;
//
//  This section takes care of installations for all users / current user.
//  Without this, unprivileged users may not be able to install any extensions.
//
#if !defined(WINCE) && WINVER >= _WIN32_WINNT_WIN2K
  if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE,SUBKEY_CLASSES,0,KEY_CREATE_SUB_KEY,&hk_classes)) hk_classes = 0;                                                        // Check for HKLM accessibility.
  if (hk_classes) {                                                                                                                                                             // HKLM accessible?
    if (IDCANCEL == (i = MessageBox (main_window,TEXT("Install for all users?"),TEXT("You Are Privileged"),MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2))) goto cleanup;    // Query user.
    if (i == IDNO) {                                                                                                                                                            // Wants to install for current user only.
      RegCloseKey (hk_classes);                                                                                                                                                 // Close key, then attempt to reopen it in HKCU.
      hk_classes = 0;
    }
    else forall = true;                                                                                                                                                         // Also install links for all users.
  }
  if (!hk_classes && (ERROR_SUCCESS != RegOpenKeyEx (HKEY_CURRENT_USER,SUBKEY_CLASSES,0,KEY_CREATE_SUB_KEY,&hk_classes))) hk_classes = 0;                                       // If this fails, errors may occur down the line, which will get reported at that time.
  if (hk_classes) check_extensions (install);                                                                                                                                   // Recheck extensions relative to one of the Software/Classes keys.
#endif
//
//  Advanced installation requested?
//
  if (INSTALL_ADVANCED == iresult) {
    install.start = install.desktop = false;                                                // Assume "advanced" users won't want links.
    if (JDialogBox(IDD_ADVINSTALL,main_window,(DLGPROC) dialog_advinstall)) goto cleanup;   // Present advanced installation options dialog.
  }
//
//  Install the extensions and the file association.
//
  notified = false;                                                                         // Used by make_key() to avoid reporting subsequent errors.
  for (i = 0; i < EXT_LIMIT; i++) {
    if (install.ext[i]) make_key (file_ext[i],TEXT("JWPxp"));
  }
  make_key (TEXT("JWPxp"             ),get_string(IDS_INST_FILETYPE));
  make_key (TEXT("JWPxp\\DefaultIcon"),TEXT("%s,-%d"),executable,IDI_FILEICON);
  make_key (REG_COMMAND               ,TEXT("%s \"%%1\""),command);
//
//  Install the project extension and file association.
//
  if (install.ext[EXT_JPR]) {
    make_key (TEXT(".jpr"),TEXT("JWPxp-project"));
    make_key (TEXT("JWPxp-project"             ),get_string(IDS_INST_PROJECTTYPE));
    make_key (TEXT("JWPxp-project\\DefaultIcon"),TEXT("%s,-%d"),executable,IDI_PROJECTICON);
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
//  Finished modifying registry.
//
  if (hk_classes) RegCloseKey (hk_classes);
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
      link->SetDescription (TEXT("JWPxp"));     // This is what will show up in the Start menu.
      if (S_OK == link->QueryInterface(IID_IPersistFile,(void **) &file)) {     // Create file object.
        if (install.start  ) save_link (file,forall?CSIDL_COMMON_PROGRAMS:        CSIDL_PROGRAMS,install.group);
        if (install.desktop) save_link (file,forall?CSIDL_COMMON_DESKTOPDIRECTORY:CSIDL_DESKTOP ,NULL         );
        file->Release ();                       // Done with file.
      }
    }
    link->Release ();                           // Done with link.
#endif WINCE
  }
cleanup:
  if (hk_classes) RegCloseKey (hk_classes);
  hk_classes = 0;
  return;
}





