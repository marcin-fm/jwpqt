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
//  This modlule implements most of the file IO options.  (See jwp_clip.cpp
//  for clipboard IO.)  Most of the routines contained in this module 
//  are defined in the class JWP_file.
//

#include "jwpce.h"
#include "jwp_cach.h"
#include "jwp_conf.h"
#include "jwp_conv.h"
#include "jwp_file.h"
#include "jwp_flio.h"
#include "jwp_help.h"
#include "jwp_inpt.h"
#include "jwp_jisc.h"
#include "jwp_misc.h"
#include "jwp_stat.h"

#ifdef WINCE
//  typedef long time_t;  // Time routines are not available in Windows CE
#else  WINCE
  #include "time.h"     // Only included because JWP Undo structure contains time_t
#endif WINCE

//===================================================================
//
//  Compile-time code generation options
//

//
//  If defined, allows program to actually read strings from JWP files.
//  Otherwise these strings are simply skipped.  These inlcude the 
//  summary strings, header strings, and data whithin undo blocks within
//  the file.
//

//--------------------------------
//
//  Compile time options.
//
#define SIZE_JISBUFFER  2000    // Size for buffer used in file import and export.

//===================================================================
//
//  Static data and definitions.
//

//
//  filters holds paired strings that are used for the file open/save 
//  dialoge box.  These are in the format that windows wants them.  
//  The array file_exts, points in the the filters objects, to pick up 
//  the extension parts of the filters, for building file names.
//
  
#if 0
//--------------------------------
//
//  Filters for open file only.  These are not used, but are a useful reference
//
"Auto-Detect   (*.*)\0*.*\0"
"Japanese File Types\0*.jce;*.jwp;*.euc;*.sjs;*.jis;*.old;*.nec;*.utf;*.jfc;*.jcp;*.txt\0"
"Ascii   (*.txt)\0*.txt\0"
//
//  Filters used for both reading and saving.
//
"Normal   (*.jce)\0*.jce\0"
"JWP   (*.jwp)\0*.jwp\0"
"EUC   (*.euc)\0*.euc\0"
"Shift-JIS   (*.sjs)\0*.sjs\0"
"New JIS   (*.jis)\0*.jis\0"
"Old JIS   (*.old)\0*.old\0"
"NEC JIS   (*.nec)\0*.nec\0"
"Unicode   (*.*)\0*.*\0"
"UTF-7   (*.utf/*.txt)\0*.txt;*.utf\0"
"UTF-8   (*.utf/*.txt)\0*.txt;*.utf\0"
"JFC   (*.jfc)\0*.jfc\0"
"JWPce Project (*.jcp)\0*.jcp\0"
"\0\0"
#endif

//--------------------------------
//
//  File extensions.
//
static tchar file_exts[][5] = { TEXT(".jce"),       // Normal
                                TEXT(".jwp"),       // JWP
                                TEXT(".euc"),       // EUC
                                TEXT(".sjs"),       // Shift-JIS
                                TEXT(".jis"),       // New JIS
                                TEXT(".old"),       // Old JIS
                                TEXT(".nec"),       // NEC JIS
                                TEXT(".txt"),       // UNICODE
                                TEXT(".utf"),       // UTF-7
                                TEXT(".utf"),       // UTF-8
                                TEXT(".jfc"),       // JFC
                                TEXT(".jcp"),       // JWPce Project
                              };

//-------------------------------------------------------------------
//
//  Data structures taken from JWP.  These structures respresent data
//  stored in the files, and thus have to be used.
//
#define JWP_MAGIC       0x42022667L // Used to identify JWP files.
#define JWPce_PROJECT   0x832CD37AL // ID for JWPce project files.

static char versions[3][6] = { "J1.20","B2","B1" }; // ID's for various JWP format files.

#define HIGH_BIT_ESCAPE     0       // Used in JWP internal file format to escape ascii codes with high bit set.
                                    // ### this is going to cause clipbaord problems,
                                    // ### becasue of the null-byte detector in the end of
                                    // ### clipboard.  I might need to think about 
                                    // ### how to fix this.

#define FILEVERSION_130     0       // Version ID for V1.3 and 1.31
#define FILEVERSION_110     1       // Version ID for V1.1
#define FILEVERSION_101     2       // Version ID for V1

#include <pshpack1.h>               // JWP file formats are aligned on single byte
                                    //   boundaries.  This seems very strange to me,
                                    //   but we need to duplicate this configuration.
typedef struct {                    // JWP native file format header.
  unsigned long  magic;             // Just to make sure is a JWP file.
  char           version[6];        // File version as a string.
  short          paragraphs;        // Number of paragraphs.
  float          margins[4];        // Margin settings.
  byte           landscape:1;       // Print oriantation.
  byte           summary:1;         // Is there a file summary.
  byte           headers:1;         // Are there print headers.
  byte           lrheader:1;        // Separate right and left headers?
  byte           nofirstpage:1;     // Supress header & footers on first page.
  byte           vertical:1;        // Vertical printing.
  short          undo;              // Number of undo levels in file.
  char           extra[97];         // Padding to 128 bytes (don't ask me why).
} FILE_HEADER;

typedef struct {                    // JWP native file undo structure.
  short  para1,start;               //   JWPce does not support undo data
  short  para2,stop;                //   within files, thus I do not care 
  time_t time;                      //   about these, except for the action 
  short  action;                    //   field which contains information
  void  *data;                      //   abound the next block of data.
  void  *next,*prev;                //   I.E. if action is non-zero this 
} UNDO_HEADER;                      //   is followed by a PARAGRAPH structure.

typedef struct {                    // JWP native file paragraph structure, used
  byte  leftindent;                 //   with undo data stored in the file.  JWPce
  byte  rightindent;                //   does not support undo data wihtin a file,
  byte  firstindent;                //   thus the contents of this structure are 
  byte  pagebreak:1;                //   of no importance, we just need the size.
  short reserved;
  short spacing;
  short spacemulti;
  void *lines;
  void *lastline;
  void *text;
  short textsize;
  void *prev,*next;
} PARAGRAPH;

typedef struct {                    // JWP native file pharagraph header from version 1.
  short textsize;                   // Number of kanji in text (not characters, because of EUC coding).
  short firstindent;                // Additional indent for first line.
  short leftindent;                 // Indent for normal line.
  short rightindent;                // Indent for right edge.
} OLD_PARA_HEADER;

typedef struct {                    // JWP native file pharagraph header used after version 1.
  short textsize;                   // Number of kanji in text (not characters, because of EUC coding).
  short spacemulti;                 // Determines line spaceing (100 is normal single line).
  byte  firstindent;                // Additional indent for first line.
  byte  leftindent;                 // Indent for normal line.
  byte  rightindent;                // Indent for right edge.
  byte  reserved;                   // I have no idea
  byte  pagebreak:1;                // Indicates that his paragraph is a page break
  byte  extra[7];                   // More padding.
} PARA_HEADER;
#include <poppack.h>

//===================================================================
//
//  Static windows procedures.
//

//--------------------------------
//
//  This is the dialog box procedure for the Window/Files... command.
//  This simply generates a list of all open files and allows the user
//  to choose one.
//
//  Return from the dialog box is zero if the user cnacled the file 
//  change, and a number if the user wants us to change files.  The 
//  numbering is 1 is the current file, 2 is the next (forward), etc.
//
//      IDC_FDFILES   The files list.
//
static int files_help;
#define MAX_FILENAME    36

static BOOL CALLBACK dialog_files (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  JWP_file *file;
  switch (message) {
    case WM_INITDIALOG: 
         file = jwp_file;
         do {
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           TCHAR *ptr,*last,buffer[SIZE_BUFFER];
           int    i;
           if (lstrlen(file->get_name()) <= MAX_FILENAME) SendDlgItemMessage (hwnd,IDC_FDFILES,LB_ADDSTRING,0,(LPARAM) file->get_name());
             else {
               last = ptr = file->get_name();
               lstrcpy (buffer,TEXT("..."));
               for (i = 0; ptr[i]; i++) {
                 if (ptr[i] == '\\') {
                   last = ptr+i;
                   if (lstrlen(ptr+i) <= MAX_FILENAME) break;
                 }
               }
               if (ptr[i]) lstrcat(buffer,ptr+i); else lstrcat (buffer,last);
               SendDlgItemMessage (hwnd,IDC_FDFILES,LB_ADDSTRING,0,(LPARAM) buffer);
             }
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           SendDlgItemMessage (hwnd,IDC_FDFILES,LB_ADDSTRING,0,(LPARAM) file->get_name());
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           file = file->next;
         } while (file != jwp_file);
         SendDlgItemMessage (hwnd,IDC_FDFILES,LB_SETCURSEL,0,0);
         return (true);
    case WM_HELP:
         do_help (hwnd,files_help);
         return  (true);
    case WM_COMMAND:    
         switch (LOWORD(wParam)) {
           case IDC_FDFILES:
                if (HIWORD(wParam) != LBN_DBLCLK) return (true);
           case IDOK:           // **** FALL THROUGH ****
                EndDialog (hwnd,1+SendDlgItemMessage(hwnd,IDC_FDFILES,LB_GETCURSEL,0,0));
                return (true);
           case IDCANCEL:
                EndDialog (hwnd,0);
                return (true);
         }
         break;
  }
  return (false);
}

//===================================================================
//
//  Static utility routines.
//
static void open_file      (tchar *name,int type);              // Open a new file checking to see if it is open.
static KANJI *read_kstring (IO_cache *cache,KANJI *buffer);     // Read a kanji string.
static int    skip_kstring (IO_cache *cache);                   // Skip through a kanji string.

//--------------------------------
//
//  Utiltiy routine used to open files.  This has to do with the fact 
//  that muli-select files return differently from single select files.
//  This routine checks to see if the file is open already and does 
//  the correct action based on the configuration.
//
//      name -- Name of file to open.
//      type -- Type of the file.
//
//      IDOK            Change to duplicate file.
//      ID_DFOPENNEW    Open duplicate
//      ID_DFREPLACE    Replace older version with new version
//      IDCANCEL        Cancel load.
//
static void open_file (tchar *name,int type) {
  int       i;
  JWP_file *file;
  if ((file = file_is_open(name))) {
    if ((i = jwp_config.cfg.double_open) == DOUBLE_PROMPT) {    // User wanted us to ask them.
      switch (ButtonDialog(null,IDD_DUPLICATE,name,IDH_FILE_DUPLICATE)) {
        case IDOK:                  // Change to file.
             i = DOUBLE_CHANGE;
             break;
        case IDC_DFOPEN:
//           i = DOUBLE_OPEN;       // Do not need to reset for below.
             break;
        case IDCANCEL:
             return;
        case IDC_DFREPLACE:
             file->activate ();
             file->revert   ();
             return;
      }
    }
    if (i == DOUBLE_CHANGE) {   // Act on user choice.  Choice to open will fall through.
      file->activate ();
      return;
    }
  }
  new JWP_file (name,type);
  return;
}

//--------------------------------
//
//  This routine skips the contents of a JWP kanji string.  This is 
//  used to skip data in the undo portion of the file.
//
//      cache  -- Cache to be processed.
//
//      RETURN -- zero value indicates and error
//
static int skip_kstring (IO_cache *cache) {
  short len;
  if (cache->get_block(&len,sizeof(len))) return (false);
  if (cache->get_block(NULL,len*sizeof(KANJI))) return (false);
  return (true);       
}

//===================================================================
//
//  Exported routines.
//

//--------------------------------
//
//  This routine generates the choose file dialgo box as well as adjusting
//  the curretn file.  This is used for implementation os some commands,
//  as well as to deal with the pop-up menu for the list box.
//
//      hwnd   -- Window that will own the dialog box if generrated.
//      index  -- Index from the current file to generate.  The basic 
//                values used are -1 (previous file), +1 (next file), and
//                0 (generate a dialog box).
//      help   -- Help index for file.
//
//      RETURN -- Pointer to file selected by the user.
//
JWP_file *choose_file (HWND hwnd,int index,int help) {
  int i;
  JWP_file *file;
  files_help = help;
  if (!index) {
    index = JDialogBox(IDD_FILES,hwnd,(DLGPROC) dialog_files);
    if (!index) return (NULL);
  }
  for (file = jwp_file, i = 1; i < index; i++) file = file->next;
  return (file);
}

//--------------------------------
//
//  This routine implements the drag and drop feature.
//
//      drop -- Pointer to the drag-and-drop handled passed to us
//              with the WM_DROPFILES message.
//
void do_drop (HDROP drop) {
#ifndef WINCE                   // Windows CE does not support file drag and drop
  int  i;
  char buffer[SIZE_BUFFER];
  for (i = 0; DragQueryFile(drop,i,buffer,SIZE_BUFFER) > 0; i++) {
    open_file (buffer,FILETYPE_AUTODETECT);
  }
  DragFinish (drop);
  return;
#endif WINCE
}

//--------------------------------
//
//  This routine handles the File/Open... menu item.
//
#define SIZE_OPENBUFFER 1000

void do_fileopen () {
  OPENFILENAME ofn;
  TCHAR buffer[SIZE_OPENBUFFER];
  int  type;
  int  first = true;
  memset (&ofn  ,0,sizeof(ofn));
  memset (buffer,0,sizeof(buffer));         // This is not necessary for windows, but WINELIB needs it.
  ofn.lStructSize       = sizeof(ofn);
  ofn.hwndOwner         = main_window;
  ofn.hInstance         = instance;
  ofn.lpstrFilter       = tab_string(IDS_FILE_FILTERSOPEN,IDS_FILE_FILTERSGENERAL);
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  ofn.nFilterIndex      = FILETYPE_NORMAL;
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  ofn.nFilterIndex      = FILETYPE_JTYPES;
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  ofn.lpstrFile         = buffer;
  ofn.nMaxFile          = SIZE_OPENBUFFER;
#ifdef WINCE
  ofn.Flags             = OFN_CREATEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY  | OFN_EXPLORER;
  ofn.lpstrInitialDir   = currentdir;   // Use Current directory for windows CE              
#else WINCE
  ofn.Flags             = OFN_CREATEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY  | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
#endif WINCE
  if (!GetOpenFileName(&ofn)) return;                       // User canclled!
  type = ofn.nFilterIndex;
  if (type == FILETYPE_JTYPES) type = FILETYPE_AUTODETECT;  // JTYPES is really a special form of auto-detect.
#ifdef WINCE
  set_currentdir (buffer,true);         // Set windows CE current directory.
  open_file (buffer,type);              // Windows CE does not support multi-select
#else  WINCE
  TCHAR *ptr,name[SIZE_BUFFER];         // Multi-select reader for regular files.
  ptr = buffer+lstrlen(buffer)+1;
  while (*ptr || first) {
    first = false;
    lstrcpy (name,buffer);
    ptr = add_part (name,ptr);
    open_file (name,type);
  }
#endif WINCE
  return;
}

//--------------------------------
//
//  This routine actually implements the Window/Files... command, and 
//  The individual files names in the Window menu.  If the argument is
//  zero, the user will be asked for a file.  Any other agument causes
//  the file to change.  The basic arguments are 1 is the current file,
//  increasing numbers are files counted forward.
//
//      index -- File to change to (forward, 1 is this file, 0 is 
//               a dailog box).
//
void do_files (int index) {
  JWP_file *file;
  file = choose_file(main_window,index,IDH_FILE_CHANGE);
  if (file) file->activate ();
  return;
}

//--------------------------------
//
//  This routine opens a file associated with the recent files menu.
//
//      id -- Menu id for the item.
//
void do_recent (int id) {
  TCHAR buffer[SIZE_BUFFER];
  get_menudata (hmenu,id,false,buffer);
  open_file (buffer+3,FILETYPE_AUTODETECT);
  return;
}

//--------------------------------
//
//  Determines if a file is already open.
//
//      name   -- Name of file to check for.
//
//      RETURN -- If the file is open the return value will be a pointer
//                to the JWP_file class object associated with the file.
//                If the file is not open the return value will be NULL.
//
JWP_file *file_is_open (tchar *name) {
  JWP_file *file;
  if (!jwp_file) return (NULL);
  file = jwp_file;
  do {
    if (!stricmp(name,file->get_name())) return (file);
    file = file->next;
  } while (file != jwp_file);
  return (NULL);
}

//--------------------------------
//
//  This routine processes files into the recent files list.  This 
//  routine is called whenever a named file is open or closed.
//
HMENU button_menu;

void recent_files (tchar *name) {
#ifdef WINCE_POCKETPC
  int   i,j;
  TCHAR buffer[SIZE_BUFFER];
  HMENU menu;
  menu        = GetSubMenu(GetSubMenu(hmenu ,MENU_FILE),MENU_RECENT);     // Get recent files sub-menu
  button_menu = GetSubMenu(GetSubMenu(hmenu2,MENU_FILE),MENU_RECENT);     // Get recent files sub-menu
  wsprintf (buffer,TEXT("&1 %s"),name);                             // Make menu item
  JInsertMenu (menu ,0,MF_BYPOSITION,IDM_FILE_FILES_BASE,buffer);   // Insert at the top of the list.
  for (i = 1; (j = get_menudata(menu,i,true,buffer)); i++) {        // Check the rest of the list.
    JRemoveMenu (menu ,i,MF_BYPOSITION);                            // Dump item.
    if ((j == IDM_FILE_FILES_NONE) || (i >= 9) || !stricmp(buffer+3,name)) { i--; continue; }
    buffer[1] = '1'+i;                                              // If here we will re-add item.
    JInsertMenu (menu ,i,MF_BYPOSITION,IDM_FILE_FILES_BASE+i,buffer);
  }
#else  WINCE_POCKETPC
  int   i,j;
  TCHAR buffer[SIZE_BUFFER];
  HMENU menu;
  menu = GetSubMenu(GetSubMenu(hmenu,MENU_FILE),MENU_RECENT);       // Get recent files sub-menu
  wsprintf (buffer,TEXT("&1 %s"),name);                             // Make menu item
  JInsertMenu (menu,0,MF_BYPOSITION,IDM_FILE_FILES_BASE,buffer);    // Insert at the top of the list.
  for (i = 1; (j = get_menudata(menu,i,true,buffer)); i++) {        // Check the rest of the list.
    JRemoveMenu (menu,i,MF_BYPOSITION);                             // Dump item.
    if ((j == IDM_FILE_FILES_NONE) || (i >= 9) || !stricmp(buffer+3,name)) { i--; continue; }
    buffer[1] = '1'+i;                                              // If here we will re-add item.
    JInsertMenu (menu,i,MF_BYPOSITION,IDM_FILE_FILES_BASE+i,buffer);
  }
#endif WINCE_POCKETPC
  return;
}

//===================================================================
//
//  Begin class JWP_file:
//
//  This modlue contains the file manipulation routines wthin the class
//  JWP_file.
//

//--------------------------------
//
//  File constructor.
//
//      filename -- Name for file.  If this is NULL, an unnamed file
//                  will be generated.
//      type     -- File type.  This should never be FILETYPE_ASCII.
//                  Additionally, if the 
//                  name is NULL, this should be FILETYPE_UNNAMED.
//
JWP_file::JWP_file (tchar *filename,int type,int recent) {
  byte          buffer[SIZE_JISBUFFER];             // Buffer for importing files.
  unsigned long done;                               // Placeholder for bytes read from a file.
  int           exit = false;                       // Non-zero value indicates read error.
  HANDLE        file;                               // Handle for file to read from.
  unsigned long magic;                              // Holds magic value from file (used to identify JWP files).
  TCHAR        *ext;                                // Points to file name extension, used to check extension.
  TCHAR         temp[40];                           // Scratch for building unnamed file names.
  int           type_guess = FILETYPE_AUTODETECT;   // Used to resove some abiguities in auto-file types.
  static short  unnamed_count = 0;                  // Counter for unnamed files.
  JIS_convert   convert;                            // Class instance used for JIS/EUC file reads
  IO_cache      cache;                              // IO_cache class instance used to read JWP format files.
  memset (this,0,sizeof(JWP_file));                 // Zero out the entire memory structure.
//
//  Setup file name and some of the file extensions.
//
  if (!filename) {
    format_string (temp,IDS_FILE_UNNAMED,++unnamed_count);  // Generate name and setup.
    filename = temp;
  }
//first = last = NULL;                      // Zeroed structure so this is not required.
//sel.type     = SELECT_NONE;               // Zeroed structure so this is not required.
  filetype     = FILETYPE_WORK;             // This suppresses file redraw until the file is done loading.
                                            //   The actual redraw will be done by the activate at the end
                                            //   of the file load.
  name         = strdup(filename);
  window       = file_window;
//
//  Setup formatting parameters
//
  page = jwp_config.cfg.page;
//
//  Setup the undo buffer.
//
  undo_init (0);
//
//  Attempt to open the file.
//
  file = OPENREAD(filename);
  if ((type == FILETYPE_UNNAMED) || (file == INVALID_HANDLE_VALUE)) {
    if (type == FILETYPE_AUTODETECT) type = FILETYPE_NORMAL;
    if (new_paragraph (NULL)) { OutOfMemory(window); exit = true; }
  }
//
//  Make sure we know what type of file we are dealing with.
//
  else {
    if (type == FILETYPE_AUTODETECT) {                      // Auto-detect, check for JWP/JWPce/Project files
      ext = name+lstrlen(name)-4;                           // Get file extension.
      ReadFile (file,&magic,sizeof(magic),&done,NULL);
      if (magic == JWP_MAGIC) {
        if (!stricmp(ext,TEXT(".jwp"))) type = FILETYPE_JWP; else type = FILETYPE_NORMAL;
      }
      else if (magic == CONFIG_MAGIC) type = FILETYPE_PROJECT;      // This is a project file
      else if (!stricmp(ext,TEXT(".jfc"))) type       = FILETYPE_JFC; 
      else if (!stricmp(ext,TEXT(".euc"))) type       = FILETYPE_EUC;
      else if (!stricmp(ext,TEXT(".sjs"))) type       = FILETYPE_SJS;
      else if (!stricmp(ext,TEXT(".jis"))) type       = FILETYPE_JIS;
      else if (!stricmp(ext,TEXT(".old"))) type       = FILETYPE_OLD;
      else if (!stricmp(ext,TEXT(".nec"))) type       = FILETYPE_NEC;
      else if (!stricmp(ext,TEXT(".utf"))) type_guess = FILETYPE_UTF8;
      SetFilePointer (file,0,NULL,FILE_BEGIN);                      // Rewind file for read.
    }
    if (type == FILETYPE_AUTODETECT) {                      // Auto-detect check for JIS/EUC/Shift-JIS files.
      convert.input_file (buffer,sizeof(buffer),file);
      convert.set_type (FILETYPE_AUTODETECT);
      type = convert.find_type();
      switch (type_guess) {                                 // Resove type ambiguities.
        case FILETYPE_UTF8:                                 // UTF8 and UTF7 have the same extensions (default to UTF8).
             if (type != FILETYPE_UTF7) type = FILETYPE_UTF8;
             break;
      }
      SetFilePointer (file,0,NULL,FILE_BEGIN);
    }           
    if (type == FILETYPE_JFC) {                             // Need to deal with EUC JFC files types (boy was that a mistake!)
      convert.input_file (buffer,sizeof(buffer),file);
      convert.set_type (FILETYPE_AUTODETECT);
      if (!convert.is_utf8()) type = FILETYPE_JFCEUC;               // If it's not UTF-8 it should be EUC
      SetFilePointer (file,0,NULL,FILE_BEGIN);
    }
//
//  Do actual load.
//
    if ((type == FILETYPE_NORMAL) || (type == FILETYPE_JWP)) {
      cache.input_file (buffer,sizeof(buffer),file);      
      exit = read_jwp_file(&cache);
    }
    else {                                                  // This will read project files, but it is not a reall problem.
      convert.input_file (buffer,sizeof(buffer),file);   
      convert.set_type (type);
      exit = import_file(&convert);
      if (type == FILETYPE_JFCEUC) type = FILETYPE_JFC;     // We only read EUC format, always write in UTF-8
    }
  }
//
//  Close handle, and clean up if there is an error.
//
  CloseHandle (file);
  switch (exit) {
    case FILEERR_OK:        // File is OK
         break;
    case FILEERR_FATAL:     // File has a fatal error
         JMessageBox (main_window,IDS_FILE_CORRUPTED,IDS_FILE_ERROR,MB_OK | MB_ICONERROR,filename);
         delete this;
         return;
    case FILEERR_ERROR:     // File has some kind of error but some data may be readable.
         if (IDYES == JMessageBox(main_window,IDS_FILE_DAMMAGED,IDS_FILE_ERROR,MB_YESNO | MB_ICONERROR,filename)) break;
         delete this;
         return;
  }
//
//  Setup display, and fix up the rest of the dislay.  
//
  char_pagewidth = jwp_config.cfg.char_width ? jwp_config.cfg.char_width : 0;
  cursor.para    = view_top.para = first;
  cursor.line    = view_top.line = first->first;            
//cursor.pos     = view_top.pos  = 0;       // Zeroed structure so this is not required.
  changed        = false;
  filetype       = type;
  if (!jwp_file) next = prev = this;                    // Link into system lists
    else {
      prev = jwp_file;
      next = jwp_file->next;
      jwp_file->next->prev = this;
      jwp_file->next       = this;
    }
  jwp_file = this;
  if (type == FILETYPE_PROJECT) {
    project_read (filename);
    return;
  }
  if (jwp_config.cfg.char_width) reformat ();           // Adjust will not format fixed dimensions.
  activate ();
  if ((filetype != FILETYPE_UNNAMED) && recent) recent_files (name);
  return;
}

//--------------------------------
//
//  This is a utitlity routine called whenever the visible file is
//  changed.  It performs some simple system changes and changes the 
//  displayed file.
//
//  On some occasions, this routine is called simply to update the 
//  display of the current file, without changing the file.
//
void JWP_file::activate () {
  JWP_file *file;
  int i,j;
  TCHAR buffer[100],*name;
  HMENU window_menu;
//
//  These are general updates of the screen and other systems.
//
  if (!this) return;
  jwp_conv.clear ();
  if (filetype != FILETYPE_EDIT) jwp_file = this;
  title     ();
  adjust    ();
  edit_menu ();
  EnableMenuItem (hmenu,IDM_FILE_REVERT,(filetype == FILETYPE_UNNAMED) ? MF_GRAYED : MF_ENABLED);
  EnableMenuItem (hmenu,IDM_FILE_DELETE,(filetype == FILETYPE_UNNAMED) ? MF_GRAYED : MF_ENABLED);
  jwp_stat.redraw ();
  redo_clear ();
//
//  Update the file names list in the Window menu.
//
  i           = 0;
  file        = jwp_file;
  window_menu = GetSubMenu(hmenu,MENU_WINDOW);
  while (DeleteMenu(window_menu,MENU_WINDOW_FILE_BASE,MF_BYPOSITION));
  do {
    name = file->get_name();
    for (j = lstrlen(name); j && (name[j-1] != '\\'); j--);
    wsprintf (buffer,TEXT("&%d  %s"),i+1,name+j);
    AppendMenu (window_menu,MF_STRING,IDM_WINDOW_FILES_BASE+i,buffer);
    file = file->next;
    if (8 == i++) break;
  } while (file != jwp_file);
  DrawMenuBar (main_window);
  return;
}

//--------------------------------
//
//  Close a file.  Generally, this routine needs some wrapper around 
//  it before closing, to make sure everything is only.  In particular,
//  the next file should be activated.
//
//      exit_ok -- If non-zero says that it is okay to exit the program 
//                 when the last file is closed.  If this is zero and 
//                 the last file is closed, a dialog may open and 
//                 ask if it is okay to exit the program.
//
//      RETURN  -- A non-zero value indicates that the file could not
//                 be closed.  Generally, this is a user intervention 
//                 issue.
//
//      IDC_SCSAVE -- Save file and continue.
//
int JWP_file::close (int exit_ok) {
  class JWP_file *file;
  if (changed) {
    switch (ButtonDialog(null,IDD_SAVECHECK,jwp_file->get_name(),IDH_FILE_CLOSE)) {
      case IDCANCEL:
           return (true);
      case IDOK:
           if (save(NULL)) return (true);
      case IDC_SCNO:
           break;
    }
  }
  if (next == this) file = NULL; else file = prev;  // Closing las file.
  next->prev = prev;                                // Link file lists.
  prev->next = next;
  jwp_file = file;
  delete this;                                      // Free resources.
  if (!file) {                                      // Exit or not.
    if (exit_ok || !jwp_config.cfg.confirm_exit || YesNo(IDS_FILE_LAST)) { if (!exit_ok) jwp_config.write_files (); DestroyWindow (main_window); }
      else {
        if (!new JWP_file(NULL,FILETYPE_UNNAMED)) { ErrorMessage (true,IDS_FILE_CREATEFATAL); DestroyWindow (main_window); }
      }
  }
  jwp_file->activate ();
  return (false);
}

//--------------------------------
//
//  Deletes the disk file associated with the current file.  This routine
//  was added to provide a way to remove files on Windows CE PPC machines,
//  that don't have a file manager.
//
void JWP_file::delete_file () {
  if (IDYES == JMessageBox(main_window,IDS_FILE_DELETETEXT,IDS_FILE_DELETETITLE,MB_ICONWARNING | MB_YESNO,name)) {
    DeleteFile (name);
    change     ();
  }
  return;
}

//--------------------------------
//
//  Exports the current file to the indicated JIS_convert structrue.
//
//      convert -- Initialized JIS_convert structure.  This allows
//                 us to export to files, clipboard, or other locations
//
long JWP_file::export_file (JIS_convert *convert) {
  int        i;
  Paragraph *para;
  convert->unicode_write ();                        // Do we need to write a UNICODE ID
  for (para = first; para; para = para->next) {
    for (i = 0; i < para->length; i++) convert->output_char (para->text[i]);
    convert->output_char ('\r');
    convert->output_char ('\n');
  }
  return (convert->flush());
// ### need to suport export line options.
// ###      tab->fixed space
// ###      preserve line breaks
// ###      fixed line length
}

//--------------------------------
//
//  This routine imports a file from a location indicated by the 
//  JIS_convert structrue.  
//
//      convert -- Indicates where to import the file from.  This 
//                 allows importing files from many different sources.
//
int JWP_file::import_file (JIS_convert *convert) {
// ###    if (type != FILETYPE_NORMAL) CONVERM TYPE CHANGE.
// ### set make paragraphs?
// ### do I want a statatus bar.
  int last_ch,ch = 0;
  int line_pending = false;
  if (new_paragraph(last)) return (FILEERR_FATAL);
  while (true) {
    last_ch = ch;
    ch = convert->input_char ();
    if (ch == 0xfeff) continue;         // This character is the UNICODE id 
                                        //   marker (this should only skip 
                                        //   the first occurance, but this 
                                        //   skips all of them).
    if (ch == JIS_EOF) break;
    if (((last_ch == '\r') && (ch == '\n')) || ((last_ch == '\n') && (ch == '\r'))) { ch = last_ch; continue; }
    if (line_pending) {
      line_pending = false;
      if (new_paragraph(last)) return (FILEERR_ERROR);
    }
    if ((ch == '\n') || (ch == '\r')) {
      line_pending = true;
      continue;
    }
    last->add_char (ch);
  }
  return (FILEERR_OK);
// ### need to suport inport options
// ###      align kanji
// ###      make paragraphs
// ###      ### indent is new paragraph ??? ###
}

//--------------------------------
//
//  Read a project file.  
//
//  The basic format of the project files is:
//
//          cfg struct          -- Binary structure with main configuration.
//          current directory   -- written as a Null termianted string.
//          list of files       -- Written as NULL terminated strings
//
//      name   -- Name of file to read.
//      
void JWP_file::project_read (tchar *name) {
  int    i;
  HANDLE hfile;
  unsigned long done;
  TCHAR *load,*ptr;
  struct cfg cfg;
//
//  Check what the user wants to do with the open files.
//
  switch (JMessageBox(main_window,IDS_FILE_PROJTEXT,IDS_FILE_PROJTITLE,MB_ICONQUESTION | MB_YESNOCANCEL)) {
    case IDYES:
         while (next != this) this->next->close (false);
         break;
    case IDNO:
         break;
    case IDCANCEL:
         close (false);
         return;
         break;
  }
  recent_files (name);          // Add the project ot the recent files list.
//
//  Open the project file.
//
  hfile = OPENREAD(name); 
  if (INVALID_HANDLE_VALUE == hfile) {
    ErrorMessage (true,IDS_FILE_PROJERROR,name);
    return;
  }
//
//  Read and check the configuration part.
//
  if (!ReadFile(hfile,&cfg,sizeof(cfg),&done,NULL) || (cfg.magic != CONFIG_MAGIC)) {
    ErrorMessage (true,IDS_FILE_PROJCORRUP,name);
    CloseHandle  (hfile);
    return;
  }
//
//  Get the file list and current directory.
//
  i = GetFileSize(hfile,NULL);
  if (!(ptr = load = (TCHAR *) calloc(i+24,1))) {
    CloseHandle (hfile);
    OutOfMemory (window);
    return;
  }
//
//  Cleanup the files.
//
  ReadFile    (hfile,ptr,i,&done,NULL);
  CloseHandle (hfile);
//
//  Actuall apply all the information
//
#ifdef WINCE                                // Set directory
  set_currentdir (ptr,false);
#else  WINCE
  if (*ptr) SetCurrentDirectory (ptr);
#endif WINCE
  ptr += lstrlen(ptr)+1;
  for (; *ptr; ptr += lstrlen(ptr)+1) {     // Open files.
    if (FileExists(ptr)) new JWP_file(ptr,FILETYPE_AUTODETECT,false);
  }
  jwp_config.set (&cfg);                    // Apply configuration
//
//  Final cleanup
//
  free   (load);    // Deallocate file name block.
  close  (false);   // Close this file (actually is the .jcp file).
  return;
}

//--------------------------------
//
//  Save the current configuration as a project file.  
//
//  The basic format of the project files is:
//
//          cfg struct          -- Binary structure with main configuration.
//          current directory   -- written as a Null termianted string.
//          list of files       -- Written as NULL terminated strings
//
//      name   -- Name of file to write.
//      
//      RETURN -- A non-zero value indicates an error.
//
#define STRINGSIZE(x)   (sizeof(TCHAR)*(x))

int JWP_file::project_save (tchar *name) {
  HANDLE        hfile;
  unsigned long done;
  TCHAR    *ptr,buffer[SIZE_BUFFER];
  JWP_file *file;
//
//  Open the file and write the configuration structure.
//
  hfile = OPENWRITE(name); 
  if (INVALID_HANDLE_VALUE == hfile) {
    ErrorMessage (true,IDS_FILE_PROJSAVE,name);
    return (true);
  }
  WriteFile (hfile,&jwp_config.cfg,sizeof(cfg),&done,NULL);
//
//  Write the current directory
//
  GetCurrentDirectory (SIZE_BUFFER,buffer);             // Save current directory.
  WriteFile (hfile,buffer,STRINGSIZE(lstrlen(buffer)+1),&done,NULL);
//
//  Write the currently loaded files.  This has to be done in the order from the current 
//  file around, but with the current file wirtten last so that the order of the files 
//  loaded back in will be the current order.  We are trying to restore the enrionement,
//  right?
//
  if ((file = jwp_file)) {
    file = file->next;
    while (true) {
      ptr = file->get_name ();
      if (file->filetype != FILETYPE_UNNAMED) WriteFile (hfile,ptr,STRINGSIZE(lstrlen(ptr)+1),&done,NULL);
      if (file == jwp_file) break;
      file = file->next;
    } 
  }
  CloseHandle (hfile);
  return (false);
}

//--------------------------------
//
//  Read a JWP format file.  This routine is normally not called 
//  directly, but rather is called through the constructor for a JWP_file.
//  item.
//
//      cache  -- Initialized IO_cache class to perform read.
//
//      RETURN -- Non-zero value indicates and error and the load was
//                aborted.
//
#define NUMBER_VERSIONS     3   // Number of different JWP file versions.

int JWP_file::read_jwp_file (IO_cache *cache) {
// ### Do i want to implement something like the slider bar.
  FILE_HEADER     file_header;      // File main header
  UNDO_HEADER     undo_header;      // Undo structure header.
  PARAGRAPH       paragraph;        // Data within an undo structure.
  PARA_HEADER     para_header;      // Paragraph header for v1.1+
  OLD_PARA_HEADER old_para_header;  // Paragraph header for v1
  int             version;          // version.
  int             ch,i,j;
//
//  Read file header.
//
  if (cache->get_block(&file_header,sizeof(file_header))) return (FILEERR_FATAL);
  for (version = 0; version < NUMBER_VERSIONS; version++) {
    if (!strcmp(file_header.version,versions[version])) break;
  }
  if ((version == NUMBER_VERSIONS) || (file_header.magic != JWP_MAGIC)) { ErrorMessage (true,IDS_FILE_ERRORJWPCE,name); return (FILEERR_FATAL); }
//
//  Setup formatting parameters.
//
  page.left      = file_header.margins[0];
  page.right     = file_header.margins[1];
  page.top       = file_header.margins[2];
  page.bottom    = file_header.margins[3];
  page.vertical  = file_header.vertical;
  page.landscape = file_header.landscape;
  odd_even       = file_header.lrheader;
  no_first       = file_header.nofirstpage;
//
//  Read summary lines if in file.
//
  if (file_header.summary) {
    for (i = 0; i < NUMBER_SUMMARY; i++) if (summary[i].read(cache)) return (FILEERR_FATAL);
  }
//
//  Read header lines within the file.
//  
  if (file_header.headers) {
    for (i = 0; i < NUMBER_HEADERS; i++) {
      for (j = 0; j < NUMBER_POSITIONS; j++) {
        if (headers[i][j].read(cache)) return (FILEERR_FATAL);
      }
    }
  }
//
//  Read undo information within the file.
//
  for (i = 0; i < file_header.undo; i++) {
    if (cache->get_block(&undo_header,sizeof(undo_header))) return (FILEERR_FATAL);
    if (undo_header.action >= 1) {
      do {                      // Read chain of pharagraph structures.
        if (cache->get_block(&paragraph,sizeof(paragraph))) return (FILEERR_FATAL);
        if (!skip_kstring(cache)) return (FILEERR_FATAL);
      } while (paragraph.prev);
    }  
    if (!undo_header.next) break;
  }
//
//  Read paragraphs within the file.
//
  for (i = 0; i < file_header.paragraphs; i++) {
    switch (version) {
      case FILEVERSION_130:
      case FILEVERSION_110:
           if (cache->get_block(&para_header,sizeof(para_header))) return (FILEERR_ERROR);
           break;
      case FILEVERSION_101:
           if (cache->get_block(&old_para_header,sizeof(old_para_header))) return (FILEERR_ERROR);
           para_header.textsize    = old_para_header.textsize;
           para_header.firstindent = (byte) old_para_header.firstindent;
           para_header.leftindent  = (byte) old_para_header.leftindent;
           para_header.rightindent = (byte) old_para_header.rightindent;
           para_header.spacemulti  = 100;
           para_header.pagebreak   = false;
           break;
    }
    if (new_paragraph(last)) { OutOfMemory (window); return (FILEERR_ERROR); }
    last->indent_first  = para_header.firstindent;
    last->indent_left   = para_header.leftindent;
    last->indent_right  = para_header.rightindent;
    last->spacing       = para_header.spacemulti;
    last->length        = para_header.textsize-1;
    last->first->length = para_header.textsize-1;
    if (para_header.pagebreak) last->page_break = true;
    while (last->size < para_header.textsize) if (last->alloc()) { last->length = 0; return (FILEERR_ERROR); }
    for (j = 0; j < para_header.textsize; j++) {                // Read paragraph text.
      if (IO_EOF == (ch = cache->get_char())) { last->length = j; return (FILEERR_ERROR); }
      if      (ch & 0x80)             { ch = ((ch << 8) | cache->get_char()) & 0x7f7f; }
      else if (ch == HIGH_BIT_ESCAPE) { ch = cache->get_char() | 0x80; }
      last->text[j] = ch;
    }
  }
  return (FILEERR_OK);
}

//--------------------------------
//
//  Implements the rever menue option.  This basically reloads the 
//  last saved version of the file.
//
void JWP_file::revert () {
  class JWP_file *new_file;
  if (changed && !YesNo(IDS_FILE_REVERT,name)) return;
  if (!(new_file = new JWP_file(name,filetype))) return;
  activate ();
  changed = false;
  close (false);  
  new_file->activate ();
  return;
}

//--------------------------------
//
//  Save the current file.
//
//      filename -- File name to be used in saving the file.  If this 
//                  name is not-NULL, this will be the file name used 
//                  to save the file.  If this parameter is NULL the
//                  file name stored in the file structure will be 
//                  used.  If the file type is FILETYPE_UNNAMED, then
//                  the routine will default to calling the JWP_file::
//                  save_as() routine.
//
//      RETURN   -- A non-zero return value indicates the file was not
//.                 saved.  This could be because the user cancelled 
//                  the save operation.
//
//  Note, the type of file saved, is determined by the JWP_file::filetype
//  parameter.  This means that JWP_file::save_as() must pass the 
//  file type via the JWP_file::filetype paraemter.
//
#define TEMP_FILE_STRING    TEXT("_@@##$$_")
#define BACKUP_FILE_STRING  TEXT("_BAK")

int JWP_file::save (tchar *filename) {
  int    exit;
  HANDLE file;
  TCHAR  buffer[512];
  byte        buf[SIZE_JISBUFFER];      // Buffer used for file io.
  IO_cache    cache;                    // IO_cache class for writing JWP files.
  JIS_convert convert;                  // JIS_convert class for exporting file types.
//
//  Determine filename, and temporary file name.  When files are saved,
//  they are first written to a temporary file, then renamed to replace
//  the original file.  This allows a save not to overwrite a file.
//
  if (!filename) {
    if (filetype != FILETYPE_UNNAMED) filename = name;
      else return (save_as());
  }
  else {
    if (name) free (name);                      // New filename becomes the file name.
    name = strdup (filename);
    title ();
  }
  recent_files (filename);
  lstrcpy (buffer,filename);
  lstrcat (buffer,TEMP_FILE_STRING);
//
//  Attempt to open temporary file and write file.
//
  if (INVALID_HANDLE_VALUE == (file = OPENWRITE(buffer))) { ErrorMessage (true,IDS_FILE_ERRORTEMP,filename); return (true); }
  if ((filetype == FILETYPE_NORMAL) || (filetype == FILETYPE_JWP)) {
    cache.output_file (buf,sizeof(buf),file);
    exit = write_jwp_file(&cache); 
  }
  else {
    convert.output_file (buf,sizeof(buf),file);
    convert.set_type (filetype);
    exit = export_file(&convert);
  }
  CloseHandle (file);
  if (exit) {
    DeleteFile   (buffer);                      // Write error -> delete temp file.
    ErrorMessage (true,IDS_FILE_ERRORWRITE,filename);
    return       (true);
  }
//
//  Finish up after a sucessful write.
//
  changed = false;
  title ();
  jwp_stat.redraw ();
  if (jwp_config.cfg.backup_files) {            // Save old verison of file for backup.
    lstrcpy    (buffer,filename);
    lstrcat    (buffer,BACKUP_FILE_STRING);
    DeleteFile (buffer);
    MoveFile   (filename,buffer);
    lstrcpy    (buffer,filename);
    lstrcat    (buffer,TEMP_FILE_STRING);
  }
  DeleteFile (filename);                        // Replace old file.
  MoveFile   (buffer,filename);
  return     (false);
}

//--------------------------------
//
//  Process the File/Save As... menu item or the File/Save menu item
//  when the file has no name.
//
//      RETURN -- A non-zero return value indicates a failture to 
//                save the file.  This can be because the user 
//                canclled the save.
//
int JWP_file::save_as () {
  OPENFILENAME ofn;
  HANDLE       file;
  int  i;
  TCHAR buffer[SIZE_BUFFER];

  if ((filetype == FILETYPE_UNNAMED) || (filetype == FILETYPE_ASCII)) i = FILETYPE_NORMAL-3; else i = filetype-3;

  memset (&ofn,0,sizeof(ofn));  
  ofn.lStructSize       = sizeof(ofn);
  ofn.hwndOwner         = main_window;
  ofn.hInstance         = instance;
  ofn.lpstrFilter       = tab_string(IDS_FILE_FILTERSGENERAL);
  ofn.nFilterIndex      = i;
  ofn.lpstrFile         = buffer;
  ofn.nMaxFile          = SIZE_BUFFER;
  ofn.Flags             = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
#ifdef WINCE
  ofn.lpstrInitialDir   = currentdir;   // Use Windows CE current directory
#endif WINCE
  if (filetype) lstrcpy (buffer,name); else buffer[0] = 0;
  for (i = lstrlen(buffer)-1; (i > 0) && (buffer[i] != '.') && (buffer[i] != '/') && (buffer[i] != ':'); i--);
  if (buffer[i] == '.') buffer[i] = 0;
  if (!GetSaveFileName(&ofn)) return (true);
#ifdef WINCE
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  TCHAR temp[SIZE_BUFFER];                          // This is a big Kludge!
  if ((buffer[0] == '\\') && (buffer[1] == '\\')) { // This appears to be a problem only on the PPC's
    lstrcpy (temp,&buffer[1]);
    lstrcpy (buffer,temp);
  }
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  set_currentdir (buffer,true);         // Wave windows CE current directory
#endif WINCE
//
//  Add file extensions
//  Check for exisitng file.
//  Set the file type and save.
//
  for (i = lstrlen(buffer); (i > 0) && (buffer[i] != '\\') && (buffer[i] != '.'); i--);
  if (buffer[i] != '.') lstrcat (buffer,file_exts[ofn.nFilterIndex-1]);
  if (buffer[lstrlen(buffer)-1] == '.') lstrcat (buffer,&file_exts[ofn.nFilterIndex-1][1]);
  if (INVALID_HANDLE_VALUE != (file = OPENREAD(buffer))) {
    CloseHandle (file);
    if (!YesNo(IDS_FILE_OVERWRITE,buffer)) return (true);
  } 
//
//  The user is actually requesting a save project!
//
  i = ofn.nFilterIndex+3;
  if (i == FILETYPE_PROJECT) return (project_save(buffer));
//
//  This is all other files saves.
//
  undo_type ();
  filetype = (byte) i;
  return (save(buffer));  
}

//--------------------------------
//
//  Write a file in JWP file format (version J1.20).
//
//      cache  -- IO_cache structure initialize to the correct location.
//
//      RETURN -- A non-zero value indicates an error writing the 
//                file.
//
long JWP_file::write_jwp_file (IO_cache *cache) {
  FILE_HEADER file_header;
  PARA_HEADER para_header;
  int         i,j;
  KANJI       ch;
  Paragraph  *para;
// ### Do i want to implement something like the slider bar.
//
//  Setup and write file header.  The blanking of the data structrues
//  is required to allow JWP to read the file.  I do not actually see
//  this in the code, but I have not searched for it.  Because the 
//  data structures are intialized to zero, much of the actual structre
//  intialization does not need to be done.
//
  memset (&file_header,0,sizeof(FILE_HEADER));  
  for (para = first, i = 0; para; para = para->next) i++;       // Cound paragraphs
  file_header.paragraphs  = i;
  file_header.magic       = JWP_MAGIC;                          // Setup Id
  strcpy (file_header.version,versions[0]);
  file_header.margins[0]  = page.left;
  file_header.margins[1]  = page.right;
  file_header.margins[2]  = page.top;
  file_header.margins[3]  = page.bottom;
  file_header.vertical    = page.vertical;
  file_header.landscape   = page.landscape;
  file_header.lrheader    = odd_even;
  file_header.nofirstpage = no_first;
//
//  Check for summary and header information
//
  for (i = 0; i < NUMBER_SUMMARY; i++) if (summary[i].kanji) file_header.summary = true;
  for (i = 0; i < NUMBER_HEADERS; i++) {
    for (j = 0; j < NUMBER_POSITIONS; j++) if (headers[i][j].kanji) file_header.headers = true;
  }
  cache->put_block (&file_header,sizeof(file_header));          // Write file header.
//
//  Write summary and header information.
//
  if (file_header.summary) {
    for (i = 0; i < NUMBER_SUMMARY; i++) summary[i].write (cache);
  }
  if (file_header.headers) {
    for (i = 0; i < NUMBER_HEADERS; i++) {
      for (j = 0; j < NUMBER_POSITIONS; j++) headers[i][j].write (cache);
    }
  }
//
//  Write each paragraph.
//
  memset (&para_header,0,sizeof(PARA_HEADER));
  for (para = first; para; para = para->next) {
    para_header.textsize    = para->length+1;           // JWP counts trailing '\n'
    para_header.spacemulti  = para->spacing;
    para_header.firstindent = para->indent_first;
    para_header.leftindent  = para->indent_left;
    para_header.rightindent = para->indent_right;
    para_header.pagebreak   = para->page_break;
    cache->put_block (&para_header,sizeof(para_header));
    for (i = 0; i < para->length; i++) {
      ch = para->text[i];
      if (ISJIS(ch)) {
        cache->put_char ((ch >> 8) | 0x80);
        cache->put_char ((ch & 0xff) | 0x80);
      }
      else if ((ch & 0x0080) || !ch) {
        cache->put_char (HIGH_BIT_ESCAPE);
        cache->put_char (ch & 0x7f);
      }
      else {
        cache->put_char (ch);
      }
    }
    cache->put_char ('\n');
  }
  return (cache->flush());
}

//
//  End Class JWP_file.
//
//===================================================================


















