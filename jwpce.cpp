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
//  Main executable module for JWPce.
//

#include <ctype.h>

#include "jwpce.h"
#include "jwp_clip.h"
#include "jwp_conf.h"
#include "jwp_conv.h"
#include "jwp_dict.h"
#include "jwp_edit.h"
#include "jwp_file.h"
#include "jwp_find.h"
#include "jwp_font.h"
#include "jwp_help.h"
#include "jwp_info.h"
#include "jwp_inpt.h"
#include "jwp_inst.h"
#include "jwp_flio.h"
#include "jwp_jisc.h"
#include "jwp_lkup.h"
#include "jwp_misc.h"
#include "jwp_prnt.h"
#include "jwp_stat.h"
#include "jwp_wnce.h"
#include "shlobj.h"

#ifndef WINCE
//x=0;
#endif

#include <commctrl.h> 
#include <commdlg.h>
#include <limits.h>

#ifdef WINCE
static void do_commandbar (void);
#endif WINCE

#ifdef WINCE_POCKETPC
HWND menu_bar;
HWND button_bar;
HMENU hmenu2;
SHACTIVATEINFO activateinfo;
#endif WINCE_POCKETPC

//
//  Window procedure for the window.
//
static LRESULT CALLBACK JWP_mode_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);

//===================================================================
//
//  Options.
//

//
//  USE_MYCHOOSECOLOR -- If defined, this cuases the system to use
//                       my ChooseColor dialog and not the system one.
//                       This was intended primarally for use in 
//                       Windows CE where the system Choose Color 
//                       dialog is not available.
//
//#define USE_MYCHOOSECOLOR
#ifdef WINCE
  #define USE_MYCHOOSECOLOR
#endif WINCE

//
//  These codes define the values required for a language DLL.  These values are combined
//  the version ID to build a full value.  These letter codes indicate the type of 
//  platform that the DLL is intended to support.
//
#if  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  #define LANGUAGE_CODE 'P'
#elif defined(WINCE_HPC)
  #define LANGUAGE_CODE 'H'
#else
  #define LANGUAGE_CODE 'W'     
#endif

//===================================================================
//
//  Debugging routines.  These either create a message box or send a
//  message to the debugger.
//

#ifdef DEBUG_ROUTINES

//--------------------------------
//
//  Send a message to the debugger.  This routine is only active in debug mode.
//
//      format -- printf type argument string.
//
//  The macro DPRINTF can be used to generate a debugging call that will 
//  automatically be compiled out.
//
void dprintf (TCHAR *format,...) {
  TCHAR buffer[SIZE_BUFFER];
  va_list args;
  va_start          (args,format);
  wvsprintf         (buffer,format,args);
  OutputDebugString (buffer);
  va_end            (args);
  return;
}

//--------------------------------
//
//  Display a message in a message box..  This routine is only active in debug mode.
//
//      format -- printf type argument string.
//
//  The macro MPRINTF can be used to generate a debugging call that will 
//  automatically be compiled out.
//
void mprintf (TCHAR *format,...) {
  TCHAR buffer[SIZE_BUFFER];
  va_list args;
  va_start   (args,format);
  wvsprintf  (buffer,format,args);
  MessageBox (main_window,buffer,TEXT("Debugging Message!"),MB_OK);
  va_end     (args);
  return;
}

#endif DEBUG_ROUTINES

//===================================================================
//
//  Static data.
//

static HACCEL haccel     = null;    // The accelerator table for the menu.

#ifdef WINCE
static HWND  command_bar = null;    // The Windows CE command bar / Windows tool bar
static TCHAR *tip_data   = NULL;    // Data use for the tool-tips
#endif WINCE
#ifdef WINCE_PPC
static HWND  button_bar  = null;    // Button bar for PPC's
#endif WINCE_PPC

//--------------------------------
//
//  This is the default configuation stored as a static structure.
//  This is harder to maintain than a set of assignments, but it is 
//  smaller, and gives us the correct arguments to default intialization,
//  and to reset to default configuration as the user is ruingin.  
//  
//  See the section on the options for a description of processing of 
//  this structure.
//
#define DICTBITS    (DICTBIT_NAMES | DICTBIT_PLACES | DICTBIT_BEGIN)

struct cfg default_config = {
  CONFIG_MAGIC,                             //  long  magic;                  // Identifies this as a JWPce config file.
  sizeof(struct cfg),                       //  long  size;                   // Size of structure
  {                                         //  PrintSetup page;              // Default printer setup.
    1.0,1.0,1.0,1.0,                        //  float left,right,top,bottom;  // Margins in inches.
    false,                                  //  byte  vertical;               // vertical printing
    false,                                  //  byte  landscape;              // Landscape page
  },
  { TEXT("Arial")     ,0  ,true,false },    //  struct cfg_font ascii_font;   // Ascii system font.
  { TEXT("k16x16.f00"),16 ,true,false },    //  struct cfg_font sys_font;     // System font used for text and a few other places.
  { TEXT("k16x16.f00"),16 ,true,false },    //  struct cfg_font list_font;    // Font for lists.
  { TEXT("k16x16.f00"),16 ,true,false },    //  struct cfg_font edit_font;    // Font used for Japanese edit constrols.
  { TEXT("k16x16.f00"),16 ,true,false },    //  struct cfg_font bar_font;     // Font used for kanji bars.
  { TEXT("k16x16.f00"),16 ,true,false },    //  struct cfg_font file_font;    // Font used for editing files.
  { TEXT("k48x48.f00"),48 ,true,false },    //  struct cfg_font big_font;     // Font used for big text.
  { TEXT("k16x16.f00"),16 ,true,false },    //  struct cfg_font jis_font;     // Font used for JIS table.
  { TEXT("k16x16.f00"),16 ,true,false },    //  struct cfg_font clip_font;    // Font used for clipboard bitmapps.
  { TEXT("k48x48.f00"),120,true,false },    //  struct cfg_font print_font;   // Font used for big text.
  { TEXT("k16x16.f00"),16 ,true,false },    //  struct cfg_font extra_font;   // Extra font for later
  { TEXT("k16x16.f00"),16 ,true,false },    //  struct cfg_font extra_font2;  // Another extra font.
  { 0,0,0,0 },                              //  struct size_window size_dict; // Size of dictionary window.
  { 0,0,0,0 },                              //  struct size_window size_user; // Size of user dictionary window;
  { 0,0,0,0 },                              //  struct size_window size_oount;// Size of count kanji window.
  { 0,0,0,0 },                              //  struct size_window size_cnvrt;// Size of user kana->kanji conversions window.
  { 0,0,0,0 },                              //  struct size_window size_info; // Size of info dialog.
  { 0,0,0,0 },                              //  struct size_window size_more; // Size of more info dialog.
  { 0,0,0,0 },                              //  struct size_window size_fill; // Unused size structure for later.
  0,0,0,0,                                  //  int   x,y,xs,ys;              // Dimensions of last saved configuration.
  20000,                                    //  int   dict_buffer;            // Size of dictionary buffer.
  RGB(255,0,0),                             //  COLORREF info_color;          // Color used for titles in kanji-info box.
  RGB(0,0,255),                             //  COLORREF colorkanji_color;    // Color to be used with color-kanji.
  DICTBITS,                                 //  long  dict_bits;              // Stores the state of all dictionary bits in one place.
  40,                                       //  short alloc;                  // Allocation size for lines.
  200,                                      //  short convert_size;           // Number of entires in user conversion table.
  35,                                       //  short char_width;             // Character width for formatinning.
  50,                                       //  short undo_number;            // Number of levels of undo to keep.
  400,                                      //  short font_cache;             // Size of font cache in characters.
  0,                                        //  short head_left;              // Position of headers to the left of margins
  0,                                        //  short head_right;             // Position of headers to the right of margins.
  100,                                      //  short head_top;               // Position of header lines above margins.
  100,                                      //  short head_bottom;            // Position of header lines below margins.
  100,                                      //  short scroll_speed;           // Determines the scroll speed.
  300,                                      //  short history_size;           // Size of history buffer (in characters).
  { '&','y','/','&','M','/','&','D',0 },    //  KANJI date_format[SIZE_DATE]; // Date format string.
  { '&','h',':','&','N',' ','&','A',0 },    //  KANJI time_format[SIZE_DATE]; // Time format string.
  { 'A','M',0 },                            //  KANJI am_format[SIZE_AMPM];   // AM format string.
  { 'P','M',0 },                            //  KAMJI pm_format[SIZE_AMPM];   // PM format string.
  {                                         //  byte  buttons[100];           // Buttons for the button bar.
    0,BUTTON_FILENEW,BUTTON_FILEOPEN,BUTTON_FILESAVE,0,BUTTON_FILEPRINT,
    0,BUTTON_CUT,BUTTON_COPY,BUTTON_PASTE,0,BUTTON_UNDO,BUTTON_REDO,
    0,BUTTON_SEARCH,BUTTON_REPLACE,BUTTON_NEXT,
    0,BUTTON_KANJI,BUTTON_ASCII,BUTTON_JASCII,BUTTON_CONVERT,
    0,BUTTON_GETINFO,BUTTON_JISTABLE,BUTTON_DICTIONARY,BUTTON_COUNTKANJI,
    0,BUTTON_RADLOOKUP,BUTTON_BUSHULOOKUP,BUTTON_BSLOOKUP,BUTTON_SKIPLOOKUP,BUTTON_HSLOOKUP,BUTTON_FCLOOKUP,BUTTON_READLOOKUP,BUTTON_INDEXLOOKUP,
    0,BUTTON_PAGELAYOUT,BUTTON_OPTIONS,0,0
  },
#if (!defined(WINCE_PPC) && !defined(WINCE_POCKETPC))
  {                                         //  byte  kanji_info[60];         // Character Information dialog items
    INFO_TYPE,INFO_JIS,INFO_SHIFTJIS,INFO_UNICODE,INFO_STROKE,INFO_GRADE,INFO_NELSON,INFO_HALPERN,INFO_SPAHN,INFO_FOURCORNERS,INFO_MOROHASHI,INFO_PINYIN,INFO_KOREAN,
    INFO_FREQUENCY,INFO_HENSHALL,INFO_GAKKEN,INFO_HEISIG,INFO_ONEILL,INFO_DEROO,INFO_KANJILEARN,
    21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60
#else   WINCE_PPC
  {                                         //  byte  kanji_info[60];         // Character Information dialog items
    INFO_JIS,INFO_STROKE,INFO_GRADE,INFO_NELSON,INFO_HALPERN,INFO_SPAHN,
    INFO_TYPE,INFO_SHIFTJIS,INFO_UNICODE,INFO_FOURCORNERS,INFO_MOROHASHI,INFO_PINYIN,INFO_KOREAN,INFO_FREQUENCY,INFO_HENSHALL,INFO_ONEILL,INFO_GAKKEN,INFO_HEISIG,
    INFO_DEROO,INFO_KANJILEARN,
    21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60
#endif WINCE_PPC
  },
//
//  Dictionary flags
//
  false,                                    //  byte  dict_compress;          // Displays diconary search results in compressed form.
  true,                                     //  byte  dict_auto;              // Automatically attempt a search if the user has selected text.
  false,                                    //  byte  dict_advanced;          // Use addaptive dictionary search.
  true,                                     //  byte  dict_iadj;              // Process i-adjitives.
  true,                                     //  byte  dict_always;            // Even if choices are found do an addpative search.
  false,                                    //  byte  dict_showall;           // Show all possible choices in an addaptive search.
  false,                                    //  byte  dict_advmark;           // Separate advanced search entries.
  false,                                    //  byte  dict_watchclip;         // Watch clipboard when dictionary is open
  false,                                    //  byte  dcit_classical;         // Classical dictionary search
  true,                                     //  byte  dict_primaryfirst;      // Move primary entries to the front the dictionary display.
  false,                                    //  byte  dict_fullascii;         // Causes first/last to select complete entry for ascii 
  true,                                     //  byte  dict_jascii2ascii;      // Treat JASCII as ascii;
//
//  Startup flags
//
  true,                                     //  byte  install;                // If set causes check for installed version and file extensions.
  false,                                    //  byte  maximize;               // Maximaize the file.
  false,                                    //  byte  usedims;                // Use last saved dimensions.
  true,                                     //  byte  save_exit;              // Save configuration on exit.     
  true,                                     //  byte  reload_files;           // Reload files loaded when we exited.
//
//  Display flags
//
  true,true,                                //  byte  vscroll,hscroll;        // Vertical and horizontal scroll bar.
  true,                                     //  byte  kscroll;                // Activate scroll bar on bar.
  false,                                    //  byte  kanjibar_top;           // Places the kanji bar at the top of the screen
  true,                                     //  byte  status;                 // Display status bar.
  true,                                     //  byte  toolbar;                // Enable the toolbar.
  39,                                       //  byte  button_count;           // Number of buttons in the toolbar
//
//  Basic operations flags
//
  true,                                     //  byte  confirm_exit;           // Require confirmation of exit on closing last file.
  true,                                     //  byte  close_does_file;        // Window close control, closes just current file.
  true,                                     //  byte  backup_files;           // Save last version of a file as a backup.
  DOUBLE_PROMPT,                            //  byte  double_open;            // Determine the action in the case of a double open.
  IME_OFF,                                  //  byte  ime_mode;               // Determines JWPce's interaction with the Microsoft IME
  false,                                    //  byte  delete_conversion;      // Causes the delete key to delete current kanji conversion instead of text to right (old action).
  true,                                     //  byte  auto_scroll;            // Enables or disables the auto-scroll feature.
  false,                                    //  byte  page_mode_file;         // Uses page scrolling for the file (PPC only)
  false,                                    //  byte  page_mode_list;         // Uses page scrolling for lists (PPC only)
//
//  Clipboard flags
//
  FILETYPE_SJS     ,                        //  byte  clip_write;             // Clipboard write type.
  FILETYPE_AUTODETECT,                      //  byte  clip_read;              // Clipboard read type.
  false,                                    //  byte  no_BITMAP;              // Suppress BITMAP clipboard format
  false,                                    //  byte  no_UNICODETEXT;         // Suppress UNICODETEXT clipboard format
//
//  Search flags
//
  false,                                    //  byte  search_nocase;          // Search: Ignore case
  false,                                    //  byte  search_jascii;          // Search: JASCII=ascii
  false,                                    //  byte  search_back;            // Search: Move backward
  false,                                    //  byte  search_wrap;            // Search: Wrap at end of file.
  false,                                    //  byte  search_all;             // Search: All files.
  false,                                    //  byte  search_noconfirm;       // Repalce: Without confirmation.
  true,                                     //  byte  keep_find;              // Causes the Search/Replace dialog to remain open during searches.
//
//  Insert to file flags
//
  true,                                     //  byte  paste_newpara;          // When pasting back in the file insert extra lines into new paragraph.
//
//  Formatting and printing
//
  true,                                     //  byte  relax_pucntuation;      // Allow relaxed punctuation.
  true,                                     //  byte  relax_smallkana;        // Allow relaxed small kana.
  WIDTH_DYNAMIC,                            //  byte  width_mode;             // Determines how the width of the display is calculated.
  true,                                     //  byte  print_justify;          // Justfication for ascii text during printing.
  false,                                    //  byte  units_cm;               // CM units (or inches).
//
//  Info and Color Kanji flags
//
  false,                                    //  byte  info_compress;          // Compress Character information.
  true,                                     //  byte  info_titles;            // Puts titles in the kanji-info list box.
  false,                                    //  byte  info_onlyone;           // Allows only one info version to open.
  false,                                    //  byte  cache_info;             // Fill for later exapnsion
  COLORKANJI_NOMATCH,                       //  byte  colorkanji_mode;        // Determines the way color-fonts are suported.
  false,                                    //  byte  colorkanji_bitmap;      // Support color kanji in bitmap clipboard format
  false,                                    //  byte  colorkanji_print;       // Support color kanji in printing.
//
//  Kanji lookup flags
//
  true,                                     //  byte  auto_lookup;            // Should we do auto-lookups in the radical lookup dialog.
  false,                                    //  byte  skip_misscodes;         // Search for skip miss-codes.
  true,                                     //  byte  bushu_nelson;           // Search for Nelson bushu
  true,                                     //  byte  bushu_classical;        // Search for classicla bushu
  0,                                        //  byte  index_type;             // Index type for index search.
  0,                                        //  byte  reading_type;           // Reading type for reading search
  true,                                     //  byte  reading_kun;            // Allow flexable kun readings.
  false,                                    //  byte  reading_word            // Allow flexable word matching.
  false,                                    //  byte  no_variants;            // Suppresses showing of variants in radical lookups.
//
//  Font flags
//
  true,                                     //  byte  cache_displayfont;      // Should we cache or not cache the display font.
  false,                                    //  byte  all_fonts;              // Show all fonts in the font selector
//
//  Fill
//
  false,                                    //  byte  nokanjibar;             // Diables the kanji bar.
  CODEPAGE_AUTO,                            //  short code_page;              // Code page used for translations
//
//  Fill
//
  0,                                        //  short fill1
  { 0 }                                                                       // Filler for later expansion.
};

//===================================================================
//
//  Exported data.
//

HMENU     hmenu    = null;      // The main menu.
HMENU     popup    = null;      // The popup menu.
HINSTANCE instance = null;      // Our instance.
HINSTANCE language = null;      // Language processor instance.

//===================================================================
//
//  Static procedures.
//

static BOOL CALLBACK dialog_formatpara (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
static BOOL CALLBACK dialog_userconv   (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);

static TCHAR *get_parameter (TCHAR *buffer,TCHAR *ptr); // Utility rotuine to extract a parameter from a line.
static int    open_files    (tchar *command);           // Intialization rotuine to open all files.
static void   terminate     (int    format,...);        // Bail-out routine used if cannot intialize.

//--------------------------------
//
//  Stub rotuine used to pass-off to the JWP_file class. for handling 
//  the format paragraph dialog box.
//
static BOOL CALLBACK dialog_formatpara (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_file->do_formatpara(hwnd,message,wParam));
}

//--------------------------------
//
//  Stub rotuine used to pass-off to the JWP_conv class. for handling 
//  the user-conversion dialog box.
//
static BOOL CALLBACK dialog_userconv (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_conv.dlg_userconv(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  This routine extracts a parameter from a text line.  A parameter is 
//  defined as a quoted string, or a space deliminated argument.
//
//      buffer -- Buffer to copy parameter to (must be big enough!).
//      ptr    -- Pointer into string being processed.
//
//      RETURN -- Pointer into string being processed at the next 
//                parameter, or pointer to the end of the string.  
//                Generally, this becomes ptr, next time around.
//
static TCHAR *get_parameter (TCHAR *buffer,TCHAR *ptr) {
  TCHAR *p2;
  if (*ptr == '"') {
    lstrcpy (buffer,++ptr);
    for (p2 = buffer; *p2 && (*p2 != '"'); p2++);
  }
  else {
    lstrcpy (buffer,ptr);
    for (p2 = buffer; *p2 && !isspace(*p2); p2++);
  }
  if (!*p2) return (NULL);
  *p2  = 0;
  ptr += (p2-buffer);
  for (ptr++; *ptr && isspace(*ptr); ptr++);
  if (!*ptr) return (NULL);
  return (ptr);
}

//--------------------------------
//
//  This routine is called only once, during startup to open all files
//  the user has requested.  This includes files from the command line, 
//  and previously loaded files, if that option is set.
//
//  The reason this is separite from the main windows procedure is so 
//  the stack space required does not get allocated on that routine's 
//  stack, since such space would remain dead and used for the remainder
//  of the program (in CE space, the two buffers use 2k).
//
//      command -- Command line passed from the user.
//
//      RETURN  -- A non-zero return value indicates a load error,
//                 and no files could be open.  This is probobly a 
//                 shortage of memory.
//
static int open_files (TCHAR *command) {
  int    i;
  TCHAR *ptr,*p2,name[SIZE_BUFFER],buffer[SIZE_BUFFER];
//
//  Process recent files, and reload files.
//
//  When the main configuration was loaded, the extneded contents of 
//  the config file (stuff after the fixed structure) were loaded into
//  the JWP_config::load variable, so we can access them then destroy 
//  them.
//
  if (jwp_config.load) {
    for (ptr = jwp_config.load, i = 0; i < 9; i++, ptr += lstrlen(ptr)+1) {
      if (*ptr) recent_files (ptr);
    }
    if (jwp_config.cfg.reload_files) {
#ifdef WINCE
      set_currentdir (ptr,false);
#else  WINCE
      if (*ptr) SetCurrentDirectory (ptr);
#endif WINCE
      ptr += lstrlen(ptr)+1;
      for (; *ptr; ptr += lstrlen(ptr)+1) {
        if (!file_is_open(ptr) && FileExists(ptr)) new JWP_file(ptr,FILETYPE_AUTODETECT,false);
      }
    }
    free (jwp_config.load);
    jwp_config.load = NULL;
  }
//
//  Process command line arguments.
//
  ptr = command;
#ifndef WINCE                           // Windows CE version does not support the network options.
  if (ptr) {                            // Skip network support option
    p2 = get_parameter(buffer,ptr);
    if ((buffer[0] == '+') || (buffer[0] == '-')) ptr = p2;
  }
#endif  WINCE
  while (ptr && *ptr) {
    ptr = get_parameter(buffer,ptr);
    GetFullPathName (buffer,SIZE_BUFFER,name,&p2);
    new JWP_file(name,FILETYPE_AUTODETECT);
  } 
//
//  If no loaded files open first file.
//
  if (jwp_file) return (false);
  return (!new JWP_file(NULL,FILETYPE_UNNAMED));
}

//--------------------------------
//
//  If things work correctly, this rotuine should never get called.
//  This is a panic stop.  It is only used until the system is started.
//  If any module returns a failure during startup this is called and
//  we stop.
//
//  The configuration is not saved here because we don't want to trash
//  the user's configuration on a failture to stratup.
//
//      format... -- printf type format string ID containning the termination
//                   message.
//
static void terminate (int format,...) {
  TCHAR buffer[SIZE_BUFFER],string[SIZE_BUFFER];
  va_list argptr;
  va_start   (argptr,format);
  if (format) {
    GET_STRING (string,format);
    wvsprintf  (buffer,string,argptr);
    MessageBox (null,buffer,get_string(IDS_ERROR_TERMINAL),MB_OK | MB_ICONERROR);
  }
  if (main_window) DestroyWindow (main_window);
  ExitThread (format ? 0 : 1);
}

//===================================================================
//
//  This is a small window class for rendering the color samples used
//  in the configuration dialog box.
//

//--------------------------------
//
//  Window procedure for a color window.  This window simply displays a color.
//
static LRESULT CALLBACK JWP_color_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;
  RECT        rect;
  HBRUSH      brush;

  switch (iMsg) {
//
//  On mouse down, we send our color to the parent.
//
    case WM_LBUTTONDOWN:
         SendMessage (GetParent(hwnd),WMU_COLORCHANGE,0,GetWindowLong(hwnd,0));
         return (0);
//
//  This is the set command that sets the color we are to display.
//
    case WMU_SETWINDOWVALUE:
         SetWindowLong  (hwnd,0,lParam);
         InvalidateRect (hwnd,NULL,false);
         return (0);
//
//  This does the actual redraw.
//
    case WM_PAINT:
         hdc = BeginPaint (hwnd,&ps);
         brush = CreateSolidBrush(GetWindowLong(hwnd,0));
         GetClientRect (hwnd,&rect);
         FillRect      (hdc,&rect,brush);
         DeleteObject  (brush);
         EndPaint      (hwnd,&ps);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//
//  End color control.
//
//===================================================================

//===================================================================
//
//  Input mode control routines, variables and procedures.
//

//--------------------------------
//
//  This structure defines a list of windows to update when the input
//  mode is changed.
//
typedef struct ModeNode {
  struct ModeNode *next;                // Pointer to next list element.
  HWND             hwnd;                // Pointer to window to update.
} ModeNode;

static ModeNode *mode_list = NULL;      // List of windows to update when the mode changes.
                                        //   This contains one entry ofr each dialog box with
                                        //   a mode change button.

//--------------------------------
//
//  This routine changes the current editor mode.  Most of this is simply
//  display changes, the actual mode changes is simply done by setting 
//  the mode flag in jwp_config.
//
//      mode -- Generally this indicates on of the basci modes, MODE_KANJI,
//              MODE_ASCII, or MODE_JASCII.  Two special modes are 
//              accepted, however:
//
//          MODE_CYCLE    -- Cycles through the modes.  This is used for
//                           clicking on the mode indicator on the status
//                           line.
//          MODE_TOGGLE   -- Toggles between MODE_KANJI and MODE_ASCII, 
//                           This implements the F4 key.
//          MODE_NOCHANGE -- Does not change the mode, but does update all the 
//                           settings.  This is used for toolbar manipulations
//          
void set_mode (int mode) {
  ModeNode *node;
  int i;
  if (mode == jwp_config.mode) return;
  switch (mode) {
    case MODE_TOGGLE:
         mode = (jwp_config.mode != MODE_KANJI) ? MODE_KANJI : MODE_ASCII;
         break;
    case MODE_CYCLE:
         mode = (jwp_config.mode+1)%3;
         break;
    case MODE_NOCHANGE:
         mode = jwp_config.mode;
         break;
    default:
         break;
  }
  jwp_config.mode = mode;           // Update the actuall mode.
  jwp_conv.clear ();                // Also redraw the status bar
//
//  Update mode buttons in the edit boxes.  
//  Also update the selected mode in the menu.
//
  for (i = MODE_KANJI; i <= MODE_JASCII; i++) CheckMenuItem (hmenu,IDM_EDIT_MODE_KANJI+i,(i == mode) ? MF_CHECKED : MF_UNCHECKED);
  for (node = mode_list; node; node = node->next) InvalidateRect (node->hwnd,NULL,true);
#ifndef WINCE
  jwp_tool.check (IDM_EDIT_MODE_KANJI ,mode == MODE_KANJI);
  jwp_tool.check (IDM_EDIT_MODE_ASCII ,mode == MODE_ASCII);
  jwp_tool.check (IDM_EDIT_MODE_JASCII,mode == MODE_JASCII);
#endif  WINCE
  return;
}

//--------------------------------
//
//  Window procedeure for the mode buttons located in the dialog boxes.
//  This button will link itself into the list of buttons to be updated
//  when the mode is changed.  Selecting this button will cuase a mode
//  cycle to appear which will in tern change the button face.  When 
//  the dialog box is closed, the control will recreive a WM_DESTROY 
//  message and the button will unlink itself from the update list.
//
static LRESULT CALLBACK JWP_mode_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;
  RECT        rect;
  ModeNode   *node,*tnode;
  static tchar modes[3] = { 'K','A','J' };  // Static values for the button face.

  switch (iMsg) {
//
//  Create, we need to allocate a ModeNode structure and link it into
//  the list of update positions.  If we cannot allocate the structure,
//  we do not link.  The button will change the mode, but not update, but
//  the system is probobly in serious memory trouble anyway.
//
    case WM_CREATE:
         if ((node = (ModeNode *) malloc(sizeof(ModeNode)))) {
           node->hwnd = hwnd;
           node->next = mode_list;
           mode_list  = node;
         }
         return (0);
//
//  Destroy message, means we are going away.  Thus we unlink ourselves
//  from the update list and end.  If we are not in the list, we just end.
//
    case WM_DESTROY:
         if (!mode_list) return (0);        // No list.
         if (mode_list->hwnd == hwnd) {     // First element in list.
           tnode     = mode_list;
           mode_list = mode_list->next;
           free (tnode);
         }
         else {                             // Search list for node.
           for (node = mode_list; node->next && (node->next->hwnd != hwnd); node = node->next);
           if (node->next) {                // Found it!
             tnode      = node->next;
             node->next = tnode->next;
             free (tnode);
           }
         }
         return (0);
//
//  Paint message means we need to generate the display.  We will always
//  base the display on the value in jwp_config.mode.  This means the 
//  display will always be right, and the update function need only 
//  invalidate our window rectrange.
//
    case WM_PAINT:
         hdc = BeginPaint (hwnd,&ps);
         GetClientRect (hwnd,&rect);
         SetBkMode     (hdc,TRANSPARENT);
         DrawText      (hdc,&modes[jwp_config.mode],1,&rect,DT_CENTER | DT_VCENTER);
         EndPaint      (hwnd,&ps);
         return (0);
//
//  Finally, a mouse click does a mode change which will change the 
//  mode setting and actually invalidate our window rectangle and thus
//  update our display.
//
    case WM_LBUTTONDOWN:
         set_mode (MODE_CYCLE);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//
//  End Mode control routines.
//
//===================================================================

//===================================================================
//
//  This group of routines keeps track of non-modal dialog boxes so we can 
//  direct messages to these dialog boxes.  As a new non-modal dialog is created,
//  it must register with a call to add_dialog().  This will allow the main window
//  loop to direct messages to it.  When the dialog box terminates, it must call
//  remove_dialog().
//
//  The parameter closeable indicates that JWPce can safely close the dialog box.
//  This is used during the exit procedures.
//

//--------------------------------
//
//  Structrue is a linked list that keeps track of all open dialog boxes
//
typedef struct DIALOG_node {
  struct DIALOG_node *next;
  HWND                hwnd;
  short               closeable;
} DIALOG_node;

static DIALOG_node *dialog_list = NULL;     // List instance

//--------------------------------
//
//  Add a dialog box to the list of open non-modal dialog boxes.
//
//      hwnd      -- Pointer to dialog box window.
//      closeable -- A true value indicates that this dialog can be 
//                   closed.  This is used primarally for the exit.
//
void add_dialog (HWND hwnd,int _closeable) {
  DIALOG_node *node;
  if (!(node = new DIALOG_node)) { OutOfMemory (hwnd); return; }
  node->hwnd      = hwnd;
  node->next      = dialog_list;
  node->closeable = _closeable;
  dialog_list     = node;
  return;
}

//--------------------------------
//
//  Remove a dialog from the list.  This is called as a dialog box is destroyed to remove it
//  from the list of non-modal dialog boxes.
//
void remove_dialog (HWND hwnd) {
  DIALOG_node *node,*temp;
  if (!dialog_list) return;                             // No dialogs to look at.
  if (dialog_list->hwnd == hwnd) {                      // First list element.
    temp        = dialog_list;
    dialog_list = temp->next;
    delete temp;
  }
  else {                                                // Find dialog box and remove
    for (node = dialog_list; node->next && (node->next->hwnd != hwnd); node = node->next);
    if (node->next) {
      temp       = node->next;
      node->next = temp->next;
      delete temp;
    }
  }
  return;
}

//
//  End dialog list.
//
//===================================================================

//===================================================================
//
//  Routines for processing the view or file child window.
//
//  Class defines a basic file view window.  This is generally associated
//  with a list of JWP_file class, whcih contain the actual files being
//  editied.
//

HWND file_window = null;                    // Window pointer to edit-window.

//--------------------------------
//
//  Window proc assoicated with the view window.
//
static void adjust_view (HWND hwnd);
static int  initialize_view (WNDCLASS *wclass);
static LRESULT CALLBACK JWP_view_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);

static LRESULT CALLBACK JWP_view_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;
  static short delta = 1;       // This is a KLUDGE used to get around the fact
                                //   that mouse_event will not generate an event
                                //   if the mouse does not move so we generate
                                //   events that move one micky right and left 
                                //   alternately, so the average is no motion.
  switch (iMsg) {
    case WM_SETFOCUS:
         SetFocus (main_window);
         return (0);
    case WM_CREATE:
         adjust_view (hwnd);
         return (0);
    case WM_PAINT:
         hdc = BeginPaint (hwnd,&ps);
         SetBkColor   (hdc,GetSysColor(COLOR_WINDOW));
         SetTextColor (hdc,GetSysColor(COLOR_WINDOWTEXT));
         if (jwp_file) jwp_file->draw_all (hdc,&ps.rcPaint);
         EndPaint (hwnd,&ps);
         return (0);
    case WM_VSCROLL:
         jwp_file->v_scroll(wParam); 
         return (0);
    case WM_HSCROLL:
         jwp_file->h_scroll(wParam);
         return (0);
    case WM_TIMER:
         if (wParam == TIMER_MOUSEHOLD) jwp_file->do_mouse (iMsg,wParam,lParam);
           else {
             KillTimer    (hwnd,TIMER_AUTOSCROLL);
             mouse_event  (MOUSEEVENTF_MOVE,delta,0,0,0);   // Fake mouse event so window keeps scrolling
             if (delta == 1) delta = -1; else delta = 1;    // Toggle mouse direction so no net motion occures.
           }
         return (0);
    case WM_LBUTTONUP:
    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDBLCLK: 
    case WM_RBUTTONDOWN:
         jwp_file->do_mouse (iMsg,wParam,lParam);
         return (0);
    case WM_COMMAND:            // Menu commands in the popup menu.
         jwp_file->do_menu (wParam);
         return (0);
    default:
         break;
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//--------------------------------
//
//  Adjust the window (prinrally in responce to changes in the window 
//  size.  
//
//      hwnd -- Pointer to the window.  Normally you can pass NULL, and 
//              the class will use the actual window.  This argument
//              is used when being called before the window is created.
//
static void adjust_view (HWND hwnd) {
  RECT rect;
  int  offset;
  if (!hwnd) return;
  offset = (jwp_config.cfg.kanjibar_top ? jwp_conv.height-1 : 0);
#ifdef WINCE
  int i;
  i = CommandBar_Height(command_bar);
  GetClientRect (main_window,&rect);
  MoveWindow    (hwnd,-1,i+offset,rect.right+2,rect.bottom-jwp_conv.height-jwp_stat.height+3-i,true);
#else  WINCE
  ShowScrollBar (hwnd,SB_HORZ,jwp_config.cfg.hscroll);
  ShowScrollBar (hwnd,SB_VERT,jwp_config.cfg.vscroll);
  GetClientRect (main_window,&rect);
  MoveWindow    (hwnd,-1,jwp_config.commandbar_height+offset,rect.right+2,rect.bottom-jwp_conv.height-jwp_stat.height+3-jwp_config.commandbar_height,true);
#endif WINCE
  jwp_file->adjust ();
  return;
}

//--------------------------------
//
//  Initializes the class, and actually enerates the window.
//
static int initialize_view (WNDCLASS *wclass) {
  int style;
  if (wclass) {
    wclass->style         = CS_HREDRAW | CS_VREDRAW;                // Mode button class for dialog boxes.
    wclass->hbrBackground = (HBRUSH) (HBRUSH) (COLOR_BTNFACE+1);
    wclass->lpfnWndProc   = JWP_mode_proc;
    wclass->lpszClassName = TEXT("JWP-Mode");
    if (!RegisterClass(wclass)) return (true);
    wclass->style         = CS_HREDRAW | CS_VREDRAW;                // Color button class for dialog boxes.
    wclass->lpfnWndProc   = JWP_color_proc;                         
    wclass->lpszClassName = TEXT("JWP-Color");
    if (!RegisterClass(wclass)) return (true);
    wclass->style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;   // View class (main editor window)
    wclass->hbrBackground = (HBRUSH) (COLOR_WINDOW+1);
    wclass->lpfnWndProc   = JWP_view_proc;
    wclass->lpszClassName = TEXT("JWP-View");
    if (!RegisterClass(wclass)) return (true);
  }
  style = WS_CHILD | WS_VISIBLE | WS_BORDER;                        // Create view window.
  if (jwp_config.cfg.hscroll) style |= WS_HSCROLL;
  if (jwp_config.cfg.vscroll) style |= WS_VSCROLL;
  if (!(file_window = CreateWindow(TEXT("JWP-View"),NULL,style,0,0,0,0,main_window,(HMENU) 3,instance,NULL))) return (true);
  ShowWindow (file_window,SW_SHOW);
  return (false);
}

//
//  End child window (view) routines.
//
//===================================================================

//===================================================================
//
//  Begin Class JWP_config.
//
//  Simple class to handle reading and writing the intialization file
//
//  The structure of the configuation file is as follows:
//
//      binary struct cfg -- Contains most of the options.  Note the 
//                           magic field indicates the config file version.
//                           As options are added this field is incremented
//                           and JWPce will reject old configurations.
//      diconary history  -- An integer (count filed), followed by a 
//      search histor        buffer, for each of these.
//      reaplce history
//      recent files      -- Next are 9 null terminated strings.  These
//                           are file names for the recent files list.
//                           Even if there are less than 9 files in the 
//                           list, 9 file names will be here.  The non-
//                           existant names will be written as zeros.
//      working directory -- Stored working directory.
//      load files        -- The remainder of the list is a number of 
//                           load files.  These are null termianted 
//                           strings of files to load on startup.
//

class JWP_config jwp_config;
#define NAME_CONFIG TEXT("jwpce.cfg")

//--------------------------------
//
//  Constructor.
//
JWP_config::JWP_config () {
  TCHAR buf[SIZE_BUFFER];
  ok              = false;      // Disk configuration is not valid.
  mode            = MODE_ASCII; // Editing mode (false mode value for intialization).
  insert          = true;       // Insert mode.
  global_effect   = false;      // Changes are local to paragraph.
//
//  Windows CE startup, locate the executalbe file to find the config
//  file location.
//
#ifdef WINCE
  GetModuleFileName (instance,buf,SIZE_BUFFER);
  GetFullPathName (buf,SIZE_BUFFER,buffer,&ptr);
  nptr       = 0;
  nbuffer[0] = 0;
//
//  Windows NT/95 startup.  This startup loads the command line and 
//  reads the first command parameter to determine if the Network 
//  option is being loaded.
//
#else WINCE
#ifndef WINELIB
  TCHAR *p;
  p = get_parameter (buf,GetCommandLine());
  GetFullPathName (buf,sizeof(buffer),buffer,&ptr);
//
//  Setup for network support.  This allows a number of the user 
//  configuration files to be located in a different location.  The 
//  test for setup of a network configuration is the first command 
//  line parameter nees to be '+' or '-' followed by a directory.
//  This directory becomes the location JWPce will user for 
//  configuration files.  The + character inciates normal error 
//  reporting.  The - character indicates suppressed error reporting.
//
  nbuffer[0]   = 0;
  nptr         = NULL;
  quiet_errors = false;
  if (p) {                                      // No command arguments, so not network support
    get_parameter (buf,p);                      // Get first command argument.
    if ((buf[0] == '+') || (buf[0] == '-')) {   // Is first chare + or -?
      if (buf[0] == '-') quiet_errors = true;   // Quiet errors
      add_part      (buf,TEXT("x"));            // Build directory.
      GetFullPathName (buf+1,sizeof(nbuffer),nbuffer,&nptr);
    }
  }
//
//  UNIX startup
//
#else WINELIB
//
//  GetCommandLine() can't be done in a global constructor under
//  WineLib because Wine won't have initialised yet. Instead we
//  supply a static mapping and arrange for it to correspond to
//  the right directories for UNIX.
//
  nbuffer[0]   = 0;                     // ${HOME} is mapped to f:
  strcpy (nbuffer, "f:\\.jwpce\\");
  nptr = nbuffer + strlen (nbuffer);
  buffer[0]   = 0;                      // System files are mapped to c:
  strcpy (buffer, "c:\\");
  ptr = buffer + strlen(buffer);
#endif WINELIB
#endif WINCE
  return;
}

//--------------------------------
//
//  Geneate a file name for operations.
//
//      file   -- Name of file.
//      mode   -- Open mode (OPEN_READ, OPEN_WRITE, OPEN_NEW, or OPEN_APPEND).
//      net    -- Determines if this is a networkable file.  If so, 
//                the name returned will be the user directory name.
//  
//      RETURN -- Pointer to buffer containning full path to file.
//
//  Network suport is actually added primarally through this name routine.
//  This rotuien determines what files JWPce will open for all but editor
//  files.  The basic opteration invovles determining what mode the 
//  file needs to be open in and weather it is a user file or a system file.
//
//  This routine has been expanded to correctly process names containning a
//  path.  Such names are not expanded, but used directly.  This was added to
//  support the new dictionary routines.
//
TCHAR *JWP_config::name (TCHAR *file,int mode,int net) {
#ifdef WINCE
  if (file[0] == '\\') return (file);
#else  WINCE
  if (file[1] == ':') return (file);
#endif WINCE
  lstrcpy (ptr,file);                               // Single user/system file names
  if (!net || !nptr) return (last_name = buffer);   // Not user file or not netwrok running.
  lstrcpy (nptr,file);                              // Network located file.    
  if ((mode != OPEN_READ) || FileExists(nbuffer)) return (last_name = nbuffer); // Write/Append mode or user file exits
  return (last_name = buffer);                      // Fall back to single user/system file.
}

//--------------------------------
//
//  Open a file located in the program directory.
//
//      filename -- Base name of file to open.
//      mode     -- Open mode (OPEN_READ or OPEN_WRITE or OPEN_APPEND).
//      net    -- Determines if this is a networkable file.  If so, 
//                the name returned will be the user directory name.
//
//      RETURN   -- HANDLE for a file.
//
HANDLE JWP_config::open (TCHAR *filename,int mode,int net) {
  TCHAR *ptr;
  HANDLE handle;
#ifndef WINCE
  UINT   error_mode;
  error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);    // Diable errors incase we are using a floppy
#endif  WINCE
  ptr = name(filename,mode,net);
  if      (mode == OPEN_READ  ) handle = OPENREAD  (ptr);
  else if (mode == OPEN_APPEND) handle = OPENAPPEND(ptr);
  else if (mode == OPEN_NEW   ) handle = OPENNEW   (ptr);
  else                          handle = OPENWRITE (ptr);
#ifndef WINCE
  SetErrorMode (error_mode);                            // Re-enable error handling.
#endif  WINCE
  return (handle);
}

//--------------------------------
//
//  Read configuration file.
//
//      RETURN -- Non-zero indicates an allocation error.
//
int JWP_config::read () {
  int    i,err = false;
  HANDLE hfile;
  unsigned long done;
  hfile = open(NAME_CONFIG,OPEN_READ,true); 
  if (INVALID_HANDLE_VALUE != hfile) {
    if (!ReadFile(hfile,&cfg,sizeof(cfg),&done,NULL) || (cfg.magic != CONFIG_MAGIC)) CloseHandle (hfile);
      else {
        ok   = true;
        dict_history   .read (hfile);                   // Read will do allocation if necessary
        search_history .read (hfile);
        replace_history.read (hfile);
        i    = GetFileSize(hfile,NULL)-3*HISTORY_SIZE-sizeof(cfg);
        load = (TCHAR *) calloc(i+24,sizeof(TCHAR));    // Enough extra characters to handle UNICODE
        ReadFile(hfile,load,i,&done,NULL);
        CloseHandle (hfile);
        goto AdjustConfig; 
      }
  }
  cfg  = default_config;
  load = NULL;
  err  = true;
  search_history .alloc (jwp_config.cfg.history_size);
  replace_history.alloc (jwp_config.cfg.history_size);
  dict_history   .alloc (jwp_config.cfg.history_size);
  ErrorMessage (false,IDS_START_CONFIGLOAD,name());
AdjustConfig:
#ifndef WINCE
  if (!cfg.usedims) cfg.x = cfg.y = cfg.xs = cfg.ys = CW_USEDEFAULT;
#endif  WINCE
  return (err);
}

//--------------------------------
//
//  This routine is called to initialize the set values to a new 
//  configuration.  All the updating that is necessary to perform the 
//  update is performed.  This should not be used for initializing 
//  the configuration, but rather for initalizaing the configuration 
//  after a change has been made.
//
//      new_config -- New configuration.
//
void JWP_config::set (struct cfg *new_config) {
  int old_undo;
  JWP_file *file;
//
//  Save some paraemters then transfer the whole config structure.
//
  old_undo = cfg.undo_number;                   // Save old undo
  jwp_conv.realloc (new_config->convert_size);  // Reallocate user conversions.
  cfg = *new_config;                            // Do assign.
//
//  If new font delete old font and reinitialize.
//
  initialize_cp    ();
  initialize_fonts ();
//
//  Setup the display and all the other features.
//
//  This 
//
#ifdef WINCE
  DestroyWindow (jwp_conv.window);      // Destory the windows so we can rebuild them
  DestroyWindow (jwp_stat.window);
  DestroyWindow (file_window);
  jwp_stat.initialize (NULL);           // Regenerate the status bar.
  jwp_conv.initialize (NULL);           // Regenerate the kana->kanji converter bar.
  initialize_view     (NULL);           // Regenerate the editor/view window.
  file = jwp_file;                      // We now need to reset the window pointers in 
  do {                                  //   all the open files.  Any Japanese edit 
    file->window = file_window;         //   controls will generate their windows on 
    file = file->next;                  //   the fly, so we don't need to deal with 
  } while (file != jwp_file);           //   them.
#else  WINCE
  DestroyWindow       (jwp_conv.window);
  jwp_tool.process    (false);
  jwp_conv.initialize (NULL);
  jwp_stat.initialize (NULL);
#endif WINCE
  jwp_stat.adjust (null);
  jwp_conv.adjust (null);
  jwp_stat.adjust (null);
  adjust_view (file_window);
  initialize_printer (&jwp_config.cfg.page);
//
//  Reallocate undo buffers for all the files.
//
  if (cfg.undo_number != old_undo) {
    file = jwp_file;
    do {
      file->undo_init (old_undo);
      file = file->next;
    } while (file != jwp_file);
  }
  file = jwp_file;
//
//  Reformat files.
//
  do { 
    file->reformat();
    file = file->next;
  } while (file != jwp_file);
//
//  Handle changes in the kanji info caching
//
  if (!cfg.cache_info) free_info ();
//
//  Reallocate the history buffers if necessary.
//
  search_history .alloc (jwp_config.cfg.history_size);
  replace_history.alloc (jwp_config.cfg.history_size);
  dict_history   .alloc (jwp_config.cfg.history_size);
//
//  Activate top file.
//
  jwp_file->activate();
  return;
}

//--------------------------------
//
//  Write configuration file.
//
//  This routine writes the configuration structure to the config file.
//  It does not write the file names, write_files() does that.
//
void JWP_config::write () {
  HANDLE          hfile;
  int             err;
  unsigned long   done;
  if (!ptr) return;
#ifndef WINCE
  if (main_window) {
    WINDOWPLACEMENT placement;
    placement.length = sizeof(placement);
    GetWindowPlacement (main_window,&placement);
    cfg.x  = placement.rcNormalPosition.left;
    cfg.y  = placement.rcNormalPosition.top;
    cfg.xs = placement.rcNormalPosition.right -placement.rcNormalPosition.left;
    cfg.ys = placement.rcNormalPosition.bottom-placement.rcNormalPosition.top;
    cfg.maximize = (byte) ((placement.showCmd == SW_MAXIMIZE) || ((placement.showCmd == SW_SHOWMINIMIZED) && (placement.flags == WPF_RESTORETOMAXIMIZED)));
  }
#endif WINCE
  hfile = open(NAME_CONFIG,OPEN_WRITE,true); 
  if (INVALID_HANDLE_VALUE != hfile) {
    err = !WriteFile(hfile,&cfg,sizeof(cfg),&done,NULL);
    ok = true;
    dict_history.write    (hfile);
    search_history.write  (hfile);
    replace_history.write (hfile);
    CloseHandle  (hfile);
    if (!err) return;
  }
  QUIET_ERROR ErrorMessage (true,IDS_START_CONFIGSAVE,name());
  return;
}

//--------------------------------
//
//  Write file names to the configuration file.
//
//  This routine writes the file names to the confifugation file.
//  Basically this skips to the end of the fixed data structure then
//  writes 9 recent files, and then writes the names of the currently 
//  open files.
//
#define STRINGSIZE(x)   (sizeof(TCHAR)*(x))

void JWP_config::write_files () {
  HANDLE    hfile;
  int       i;
  TCHAR    *ptr,buffer[SIZE_BUFFER];
  JWP_file *file;
  unsigned long done;
//
//  Open file.  Cannot use JWP_config::open() because we want to maintain
//  the contents of the file and open() destroys the previous file.
//
  hfile = open(NAME_CONFIG,OPEN_APPEND,true); 
  if (INVALID_HANDLE_VALUE == hfile) {
    QUIET_ERROR ErrorMessage (true,IDS_START_CONFIGWRITE,name());
    return;
  }
  SetFilePointer (hfile,sizeof(cfg)+3*HISTORY_SIZE,NULL,FILE_BEGIN);    // Move to end of fixed strucutre.
  SetEndOfFile   (hfile);                                               // Truncate file here.
//
//  Write the recent files list if we were not given a name.  This means this is the main
//  configuration and not a user project.
//
  for (i = 8; i >= 0; i--) {                            // Write recent files.
    if (!get_menudata(hmenu,IDM_FILE_FILES_BASE+i,false,buffer)) buffer[3] = 0; 
    WriteFile (hfile,buffer+3,STRINGSIZE(lstrlen(buffer+3)+1),&done,NULL);
  }
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
  return;
}

//
//  End Class JWP_config.
//
//===================================================================

//===================================================================
//
//  These routines handle the processing of the Utilities/Options...
//  dialog box.
//
//  These routines manipulate an object of type struct cfg (same as the 
//  core of the JWP_config class, and as the configuation file.  These 
//  actually manipulate one pointed to by the global variable cfg.  If 
//  the user aborts the Options setting, the structure is discarded.  
//  If the user keeps the changes, then JWP_config::set() is called 
//  to set the configuration.  JWP_config::set() is also used to 
//  implement a reset to default configuration.
//

       struct cfg *cfg;             // Config structure modified by these routines.
static HFONT font;                  // Indicates new font chosen.

//--------------------------------
//
//  This is the dialog box handler for my ChooseColor dialog box.  
//  This dialog box was gnerated specifically for use on Windows CE
//  machines where the system one does not exist.
//
#ifdef USE_MYCHOOSECOLOR

static BOOL CALLBACK dialog_choosecolor (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  static COLORREF colors[] = { RGB(255,000,000),RGB(000,255,000),RGB(000,000,255),  // Color array for non-grey colors.
                               RGB(255,255,000),RGB(255,000,255),RGB(000,255,255),
                               RGB(180,000,000),RGB(000,180,000),RGB(000,000,180),
                               RGB(180,180,000),RGB(180,000,180),RGB(000,180,180),
                             };
  int i,j;
  switch (msg) {
//
//  Generate the dialog box.
//
    case WM_INITDIALOG:
         SetWindowLong (hwnd,DWL_USER,lParam);                      // Save input parameter for abort.
         for (i = IDC_CCBOX1 ; i <= IDC_CCBOX12; i++) SendDlgItemMessage (hwnd,i,WMU_SETWINDOWVALUE,0,colors[i-IDC_CCBOX1]);
         for (i = IDC_CCBOX13; i <= IDC_CCBOX24; i++) { 
           j = (255*(i-IDC_CCBOX13))/(IDC_CCBOX24-IDC_CCBOX13);     // Setup grey-scale colors.
           SendDlgItemMessage (hwnd,i,WMU_SETWINDOWVALUE,0,RGB(j,j,j));
         }
//
//  Respond to color change (called on initialize also).  In this case 
//  one of the color boxes has been clicked and we want to set the color.
//
    case WMU_COLORCHANGE:       // **** FALL THROUGH **** 
         SendDlgItemMessage (hwnd,IDC_CCSAMPLE,WMU_SETWINDOWVALUE,0,lParam);
         SetDlgItemInt (hwnd,IDC_CCRED  ,GetRValue(lParam),false);
         SetDlgItemInt (hwnd,IDC_CCGREEN,GetGValue(lParam),false);
         SetDlgItemInt (hwnd,IDC_CCBLUE ,GetBValue(lParam),false);
         return (true);
//
//  Help 
//
    case WM_HELP:
         do_help (hwnd,IDH_INTERFACE_COLOR);
         return  (true);
//
//  Main controls
//
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           case IDC_CCRED:                  // The edit boxes, simply change the color.
           case IDC_CCGREEN:
           case IDC_CCBLUE: {
                  int r,g,b;
                  input_check (hwnd,wParam);
                  r = GetDlgItemInt(hwnd,IDC_CCRED  ,NULL,false);
                  g = GetDlgItemInt(hwnd,IDC_CCGREEN,NULL,false);
                  b = GetDlgItemInt(hwnd,IDC_CCBLUE ,NULL,false);
                  SendDlgItemMessage (hwnd,IDC_CCSAMPLE,WMU_SETWINDOWVALUE,0,RGB(r,g,b));
                }
                return (true);
           case IDCANCEL:                   // Cancle returns the default color, passed in as an argument.
                EndDialog (hwnd,GetWindowLong(hwnd,DWL_USER));
                return (true);
           case IDOK:                       // OK returns the current color in the sample box.
                EndDialog (hwnd,GetWindowLong(GetDlgItem(hwnd,IDC_CCSAMPLE),0));
                return (true);
         }
         break;
  }
  return (false);
}

#endif USE_MYCHOOSECOLOR

//--------------------------------
//
//  Internal routine used to get the color of something.
//
//      hwnd   -- Parent dialog window.
//      color  -- Default (current) color selection.
//
//      RETURN -- Return value is the newly choses color, or the 
//                old color if the user aborted.
//
static COLORREF get_color (HWND hwnd,COLORREF color) {
// ### If we make custom colors static and intitialize it, we can have 
// ###   custom colors carried over multible invocations.
#ifdef USE_MYCHOOSECOLOR
  return (JDialogBox(IDD_CHOOSECOLOR,hwnd,(DLGPROC) dialog_choosecolor,(LPARAM) color));
#else  USE_MYCHOOSECOLOR
  COLORREF    custom_colors[16];
  CHOOSECOLOR choose;
  memset (&choose,0,sizeof(choose));
  choose.lStructSize  = sizeof(choose);
  choose.hwndOwner    = hwnd;
  choose.hInstance    = null;					// Was, but V++ 6.0 has problems with this instance;
  choose.rgbResult    = color;
  choose.lpCustColors = custom_colors;
  choose.Flags        = CC_ANYCOLOR | CC_RGBINIT | CC_SOLIDCOLOR;
  InvalidateRect (hwnd,NULL,false);
  if (ChooseColor (&choose)) return (choose.rgbResult); else return (color);
  return (color);
#endif USE_MYCHOOSECOLOR
}

//--------------------------------
//
//  Dialog box procedure for the general page.
//
static BOOL CALLBACK options_general (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG:
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         CheckDlgButton (hwnd,IDC_OGPAGEFILE    ,cfg->page_mode_file);
         CheckDlgButton (hwnd,IDC_OGPAGELIST    ,cfg->page_mode_list);
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         CheckDlgButton (hwnd,IDC_OGRESTOREPOS  ,cfg->usedims);
         CheckDlgButton (hwnd,IDC_OGRELOADFILES ,cfg->reload_files);
         CheckDlgButton (hwnd,IDC_OGCONFIRMEXIT ,cfg->confirm_exit);
         CheckDlgButton (hwnd,IDC_OGSAVESETTINGS,cfg->save_exit);
         CheckDlgButton (hwnd,IDC_OGWIDTHDYNAMIC+cfg->width_mode,true);
         CheckDlgButton (hwnd,cfg->close_does_file ? IDC_OGCLOSEFILE  : IDC_OGCLOSEPROGRAM,true);
         CheckDlgButton (hwnd,cfg->units_cm ? IDC_OGCM : IDC_OGINCHES,true);
         SetDlgItemInt  (hwnd,IDC_OGWIDTH,cfg->char_width ,false);
         EnableWindow (GetDlgItem(hwnd,IDC_OGWIDTH),cfg->width_mode == WIDTH_FIXED);
         return (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           INPUT_CHECK (IDC_OGWIDTH);
           case IDCANCEL:                       // Allow ESC abort
                DestroyWindow (hwnd);
                return (true);
           case IDC_OGWIDTHFIXED:
           case IDC_OGWIDTHDYNAMIC:
           case IDC_OGWIDTHPRINTER:
                EnableWindow (GetDlgItem(hwnd,IDC_OGWIDTH),IsDlgButtonChecked(hwnd,IDC_OGWIDTHFIXED));
                return (true);
           case IDC_OGSAVESETTINGS:
                EnableWindow (GetDlgItem(hwnd,IDC_OGRESTOREPOS),IsDlgButtonChecked(hwnd,IDC_OGSAVESETTINGS));
                return (true);
         }
         break;
    case WM_GETDLGVALUES:
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         cfg->page_mode_file  = IsDlgButtonChecked(hwnd,IDC_OGPAGEFILE    );
         cfg->page_mode_list  = IsDlgButtonChecked(hwnd,IDC_OGPAGELIST    );
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         cfg->usedims         = IsDlgButtonChecked(hwnd,IDC_OGRESTOREPOS  );
         cfg->reload_files    = IsDlgButtonChecked(hwnd,IDC_OGRELOADFILES );
         cfg->confirm_exit    = IsDlgButtonChecked(hwnd,IDC_OGCONFIRMEXIT );
         cfg->save_exit       = IsDlgButtonChecked(hwnd,IDC_OGSAVESETTINGS);
         cfg->close_does_file = IsDlgButtonChecked(hwnd,IDC_OGCLOSEFILE   );
         cfg->units_cm        = IsDlgButtonChecked(hwnd,IDC_OGCM          );
         cfg->char_width      = get_int(hwnd,IDC_OGWIDTH,5,1000,cfg->char_width);
         if      (IsDlgButtonChecked(hwnd,IDC_OGWIDTHDYNAMIC)) cfg->width_mode = WIDTH_DYNAMIC;
         else if (IsDlgButtonChecked(hwnd,IDC_OGWIDTHFIXED  )) cfg->width_mode = WIDTH_FIXED;
         else                                                  cfg->width_mode = WIDTH_PRINTER; 
         return (true);
  }
  return (false);
}

//--------------------------------
//
//  Dialog box Procedure for the Display page.
//
static BOOL CALLBACK options_display (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG:
         CheckDlgButton (hwnd,IDC_ODVSCROLL    , cfg->vscroll);
         CheckDlgButton (hwnd,IDC_ODHSCROLL    , cfg->hscroll);
         CheckDlgButton (hwnd,IDC_ODKSCROLL    , cfg->kscroll);
         CheckDlgButton (hwnd,IDC_ODSTATUSBAR  , cfg->status);
         CheckDlgButton (hwnd,IDC_ODKBARTOP    , cfg->kanjibar_top);
         CheckDlgButton (hwnd,IDC_ODAUTOSCROLL , cfg->auto_scroll);
         CheckDlgButton (hwnd,IDC_ODKANJIBAR   ,!cfg->nokanjibar);
         SetDlgItemInt  (hwnd,IDC_ODSCROLLSPEED, cfg->scroll_speed,false);
#ifndef WINCE
         CheckDlgButton (hwnd,IDC_ODTOOLBAR    ,cfg->toolbar);
#endif  WINCE
         SendDlgItemMessage (hwnd,IDC_ODHIGHLIGHTBOX,WMU_SETWINDOWVALUE,0,cfg->info_color);
         EnableWindow (GetDlgItem(hwnd,IDC_ODSCROLLSPEED),IsDlgButtonChecked(hwnd,IDC_ODAUTOSCROLL));
         return (true);
    case WMU_COLORCHANGE:                       // User clicked the color box, so select a new color.
         wParam = IDC_ODHIGHLIGHT;
    case WM_COMMAND:        // **** FALL THROUGH ****
         switch (LOWORD(wParam)) {
           INPUT_CHECK (IDC_ODSCROLLSPEED);
           case IDC_ODAUTOSCROLL:
                EnableWindow (GetDlgItem(hwnd,IDC_ODSCROLLSPEED),IsDlgButtonChecked(hwnd,IDC_ODAUTOSCROLL));
                break;
           case IDC_ODHIGHLIGHT:
                cfg->info_color = get_color(hwnd,cfg->info_color);
                SendDlgItemMessage (hwnd,IDC_ODHIGHLIGHTBOX,WMU_SETWINDOWVALUE,0,cfg->info_color);
                break;
         }
         break;
    case WM_GETDLGVALUES:
         cfg->vscroll      = IsDlgButtonChecked(hwnd,IDC_ODVSCROLL   );
         cfg->hscroll      = IsDlgButtonChecked(hwnd,IDC_ODHSCROLL   );
         cfg->kscroll      = IsDlgButtonChecked(hwnd,IDC_ODKSCROLL   );
         cfg->status       = IsDlgButtonChecked(hwnd,IDC_ODSTATUSBAR );
         cfg->kanjibar_top = IsDlgButtonChecked(hwnd,IDC_ODKBARTOP   );
         cfg->auto_scroll  = IsDlgButtonChecked(hwnd,IDC_ODAUTOSCROLL);
         cfg->nokanjibar   =!IsDlgButtonChecked(hwnd,IDC_ODKANJIBAR  );
         cfg->scroll_speed = get_int(hwnd,IDC_ODSCROLLSPEED,0,10000,cfg->scroll_speed);
#ifndef WINCE
         cfg->toolbar      = IsDlgButtonChecked(hwnd,IDC_ODTOOLBAR   );
#endif  WINCE
         return (true);
  }
  return (false);
}

//--------------------------------
//
//  Dialog box procedure for the Fonts page.
//
                                        // Indexes for font types
#define FONT_NONE       -1              // No font being worked with (used for initialization)
#define FONT_JIS        0               // Font used for JIS table.
#define FONT_SYSTEM     1               // System font
#define FONT_EDIT       2               // Font used for Japanese edit controls.
#define FONT_LIST       3               // Font used for Japanese list boxes.
#define FONT_BAR        4               // Font used for kanji bars.
#define FONT_FILE       5               // Font used for the editor.
#define FONT_CLIP       6               // Font used for bitmaps on the clipboard
#define FONT_PRINT      7               // Font used for printing.
#define FONT_BIG        8               // Big font.

static short current_font = FONT_NONE;  // Indicates the current font the user is working on.

#define USE_FONTLISTBOX                 // Uses a font list box instead of the font selector.  I like this
                                        //   better.

static void change_font   (HWND hwnd);
static void check_height  (HWND hwnd);
static void select_string (HWND hwnd,int id,tchar *string);

//--------------------------------
//
//  This routine handles a change in the font the user is setting.  This is also
//  used to set the data and to read the data for the current font.  
//
//  First this routine reads the data for the out-going font type.  Then sets the
//  data for the next font type.  
//
//  If you call this routine without changing the font type, you will just read the 
//  values and then put them back into place.
//
//  This routine is used to initialize.  The current_font (out-going font) is set 
//  to FONT_NONE so no data is read, but the new font is initilize.  Simiuarly, the 
//  routien is used to read the data before the dialog is closed.
//  
static void change_font (HWND hwnd) {
  int   i;
  struct cfg_font *font;
//
//  User is changing the font so lets save the old font information.
//
//  We read all information, including the invalid information.  This is safe on the 
//  read, because we just ignore the fields we don't want.
//
  if (current_font != FONT_NONE) {
    switch (current_font) {
      case FONT_JIS   : font = &cfg->jis_font;   break;
      case FONT_SYSTEM: font = &cfg->sys_font;   break;
      case FONT_EDIT  : font = &cfg->edit_font;  break;
      case FONT_LIST  : font = &cfg->list_font;  break;
      case FONT_BAR   : font = &cfg->bar_font;   break;
      case FONT_FILE  : font = &cfg->file_font;  break;
      case FONT_CLIP  : font = &cfg->clip_font;  break;
      case FONT_BIG   : font = &cfg->big_font;   break;
      case FONT_PRINT : font = &cfg->print_font; break;
    }
    if (current_font == FONT_PRINT) font->size = get_float(hwnd,IDC_OFHEIGHT,2,1600,font->size,10,NULL);
      else font->size = get_int(hwnd,IDC_OFHEIGHT,4,1000,font->size);
    font->automatic = IsDlgButtonChecked(hwnd,IDC_OFAUTO);
    font->vertical  = IsDlgButtonChecked(hwnd,IDC_OFVERTICAL);
    i               = SendDlgItemMessage(hwnd,IDC_OFFONTNAME,CB_GETCURSEL,0,0);
    if (CB_ERR != i) SendDlgItemMessage (hwnd,IDC_OFFONTNAME,CB_GETLBTEXT,i,(LPARAM) font->name);
  }
//  
//  Now setup the new font information.
//
//  Here we need to do each case seperately so we can blank out the fields we dont 
//  want displayed.
//
  switch (current_font = (short) SendDlgItemMessage(hwnd,IDC_OFFONT,CB_GETCURSEL,0,0)) {
    case FONT_JIS:
         EnableWindow   (GetDlgItem(hwnd,IDC_OFAUTO  ),true);
         CheckDlgButton (hwnd,IDC_OFAUTO    ,cfg->jis_font.automatic);
         select_string  (hwnd,IDC_OFFONTNAME,cfg->jis_font.name);
         SetDlgItemText (hwnd,IDC_OFHEIGHT  ,TEXT(""));
         break;         
    case FONT_SYSTEM:
         EnableWindow   (GetDlgItem(hwnd,IDC_OFAUTO  ),false);
         CheckDlgButton (hwnd,IDC_OFAUTO    ,false);
         select_string  (hwnd,IDC_OFFONTNAME,cfg->sys_font.name);
         SetDlgItemInt  (hwnd,IDC_OFHEIGHT  ,cfg->sys_font.size,false);
         break;
    case FONT_EDIT:
         EnableWindow   (GetDlgItem(hwnd,IDC_OFAUTO  ),true);
         CheckDlgButton (hwnd,IDC_OFAUTO    ,cfg->edit_font.automatic);
         select_string  (hwnd,IDC_OFFONTNAME,cfg->edit_font.name);
         SetDlgItemInt  (hwnd,IDC_OFHEIGHT  ,cfg->edit_font.size,false);
         break;
    case FONT_LIST:
         EnableWindow   (GetDlgItem(hwnd,IDC_OFAUTO  ),true);
         CheckDlgButton (hwnd,IDC_OFAUTO    ,cfg->list_font.automatic);
         select_string  (hwnd,IDC_OFFONTNAME,cfg->list_font.name);
         SetDlgItemInt  (hwnd,IDC_OFHEIGHT  ,cfg->list_font.size,false);
         break;
    case FONT_BAR:
         EnableWindow   (GetDlgItem(hwnd,IDC_OFAUTO  ),true);
         CheckDlgButton (hwnd,IDC_OFAUTO    ,cfg->bar_font.automatic);
         select_string  (hwnd,IDC_OFFONTNAME,cfg->bar_font.name);
         SetDlgItemInt  (hwnd,IDC_OFHEIGHT  ,cfg->bar_font.size,false);
         break;
    case FONT_FILE:
         EnableWindow   (GetDlgItem(hwnd,IDC_OFAUTO  ),true);
         CheckDlgButton (hwnd,IDC_OFAUTO    ,cfg->file_font.automatic);
         select_string  (hwnd,IDC_OFFONTNAME,cfg->file_font.name);
         SetDlgItemInt  (hwnd,IDC_OFHEIGHT  ,cfg->file_font.size,false);
         break;
    case FONT_CLIP:
         EnableWindow   (GetDlgItem(hwnd,IDC_OFAUTO  ),true);
         CheckDlgButton (hwnd,IDC_OFAUTO    ,cfg->clip_font.automatic);
         select_string  (hwnd,IDC_OFFONTNAME,cfg->clip_font.name);
         SetDlgItemInt  (hwnd,IDC_OFHEIGHT  ,cfg->clip_font.size,false);
         break;
    case FONT_PRINT:
         EnableWindow   (GetDlgItem(hwnd,IDC_OFAUTO  ),true);
         CheckDlgButton (hwnd,IDC_OFAUTO    ,cfg->print_font.automatic);
         select_string  (hwnd,IDC_OFFONTNAME,cfg->print_font.name);
         put_float      (hwnd,IDC_OFHEIGHT  ,cfg->print_font.size,10);
         break;
    case FONT_BIG:
         EnableWindow   (GetDlgItem(hwnd,IDC_OFAUTO  ),true);
         CheckDlgButton (hwnd,IDC_OFAUTO    ,cfg->big_font.automatic);
         select_string  (hwnd,IDC_OFFONTNAME,cfg->big_font.name     );
         SetDlgItemText (hwnd,IDC_OFHEIGHT  ,TEXT(""));
         break;
  }
  if (current_font == FONT_CLIP) {
    CheckDlgButton (hwnd,IDC_OFVERTICAL,cfg->clip_font.vertical);
    EnableWindow   (GetDlgItem(hwnd,IDC_OFVERTICAL),true);
  }
  else {
    CheckDlgButton (hwnd,IDC_OFVERTICAL,false);
    EnableWindow   (GetDlgItem(hwnd,IDC_OFVERTICAL),false);
  }
  EnableWindow   (GetDlgItem(hwnd,IDC_OFFONTNAME),!IsDlgButtonChecked(hwnd,IDC_OFAUTO));
  SetDlgItemText (hwnd,IDC_OFHLABEL,get_string((current_font == FONT_PRINT) ? IDS_OF_POINTS : IDS_OF_PIXELS));
  check_height   (hwnd);
  return;
}

//--------------------------------
//
//  Small utility routine to check the state of the height box.  If the current display font
//  is not a TrueType font then this will disable the window, otherwise it will enable the 
//  window.
//
static void check_height (HWND hwnd) {
  int   i;
  TCHAR buffer[SIZE_BUFFER];
  i = SendDlgItemMessage(hwnd,IDC_OFFONT,CB_GETCURSEL,0,0);
  SendDlgItemMessage (hwnd,IDC_OFFONTNAME,CB_GETLBTEXT,SendDlgItemMessage(hwnd,IDC_OFFONTNAME,CB_GETCURSEL,0,0),(LPARAM) buffer);
  EnableWindow (GetDlgItem(hwnd,IDC_OFHEIGHT),(FONT_PRINT == i) || (lstrlen(buffer) >= 4) && stricmp(buffer+lstrlen(buffer)-4,TEXT(".f00")) && ((FONT_BIG != i) && (FONT_JIS != i) && !IsDlgButtonChecked(hwnd,IDC_OFAUTO)));
  return;
}

//--------------------------------
//
//  Utility routine used to select only TrueType fonts for the font list.  In the case of 
//  PPC machines, any font is accepted because the PPC's don't support TT fonts.
//
//      lParam -- Passed indirectly via call to EnumFontFamilies() contains a pointer to the
//                dialog window so that messages can be sent to the controls
//
static int CALLBACK enum_fonts (ENUMLOGFONT *lpelf,NEWTEXTMETRIC *lpntm,int FontType,LPARAM lParam) {
#ifdef WINCE                // Some Windows CE systems don't have TT fonts, so include all fonts.
  SendDlgItemMessage ((HWND) lParam,IDC_OFASCIIFONT,CB_ADDSTRING,0,(LPARAM) lpelf->elfLogFont.lfFaceName);
#else  WINCE
  if (FontType == TRUETYPE_FONTTYPE) {
    SendDlgItemMessage ((HWND) lParam,IDC_OFASCIIFONT,CB_ADDSTRING,0,(LPARAM) lpelf->elfLogFont.lfFaceName);
  }
#endif WINCE
  return (true);
} 
  
//--------------------------------
//
//  Utility routine used to locate all Japanese TrueType fonts, so they can be included in 
//  the print lists.
//
//      lParam -- Passed indirectly via call to EnumFontFamilies() contains a pointer to the
//                dialog window so that messages can be sent to the controls
//
static int CALLBACK enum_jfonts (ENUMLOGFONT *lpelf,NEWTEXTMETRIC *lpntm,int FontType,LPARAM lParam) {
  if (FontType != TRUETYPE_FONTTYPE) return (true);
  if (!cfg->all_fonts && (lpelf->elfLogFont.lfCharSet != SHIFTJIS_CHARSET)) return (true);
  SendDlgItemMessage ((HWND) lParam,IDC_OFFONTNAME,CB_ADDSTRING,0,(LPARAM) lpelf->elfLogFont.lfFaceName);
  return (true);
}  

//--------------------------------
//
//  Internal routine used to select an exect string from the font
//  list, this is necessary because the font selection can be wrong, 
//  Ariel Bold is listed in front of Ariel.
//
static void select_string (HWND hwnd,int id,tchar *string) {
  int i;
  i = SendDlgItemMessage(hwnd,id,CB_FINDSTRINGEXACT,0,(LPARAM) string);
  SendDlgItemMessage (hwnd,id,CB_SETCURSEL,i,0);
  return;
}

//--------------------------------
//
//  Internal routine to setup the fonts in the Japanese and printer font 
//  lists.  This routine was sepearated out so it can be called again if 
//  the user changes the state of the Show All Fonts button.
//
//      hwnd -- Pointer to dialog window.
//
static void setup_jfonts (HWND hwnd) {
  HDC             hdc;
  HANDLE          handle;
  WIN32_FIND_DATA data;
//
//  Reset lists to get an empty list.
//
  SendDlgItemMessage (hwnd,IDC_OFFONTNAME,CB_RESETCONTENT,0,0);
  handle = FindFirstFile(jwp_config.name(TEXT("*.f00"),OPEN_READ,false),&data);
  if (handle != INVALID_HANDLE_VALUE) {
    do {
      SendDlgItemMessage (hwnd,IDC_OFFONTNAME,CB_ADDSTRING,0,(LPARAM) data.cFileName);
    } while (FindNextFile(handle,&data));
    FindClose (handle);
  }
//
//  Process TrueType fonts
//
  hdc = GetDC(hwnd);
  EnumFontFamilies (hdc,NULL,(FONTENUMPROC) enum_jfonts,(LPARAM) hwnd);
  ReleaseDC        (hwnd,hdc);
  change_font      (hwnd);
  return;
}

#define NUMBER_FONTS    (sizeof(fonts)/sizeof(short))
static BOOL CALLBACK options_font (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  static short fonts[] = { IDS_OF_JIS,IDS_OF_SYSTEM,IDS_OF_EDIT,IDS_OF_LIST,IDS_OF_BAR,IDS_OF_FILE,IDS_OF_CLIP,IDS_OF_PRINT,IDS_OF_BIG };
  switch (msg) {
    case WM_INITDIALOG: {
           SetDlgItemText     (hwnd,IDC_OFASCIINAME ,cfg->ascii_font.name);
           SendDlgItemMessage (hwnd,IDC_OFASCIINAME ,WM_SETFONT,(WPARAM) font,MAKELPARAM(true,0));
           CheckDlgButton     (hwnd,IDC_OFRELAXPUNCT,cfg->relax_punctuation);
           CheckDlgButton     (hwnd,IDC_OFRELAXSMALL,cfg->relax_smallkana);
           CheckDlgButton     (hwnd,IDC_OFSHOWALL   ,cfg->all_fonts);
//
//  Set list display font.
//
           SendDlgItemMessage (hwnd,IDC_OFASCIIFONT,WM_SETFONT,(WPARAM) GetStockObject(SYSTEM_FONT),MAKELPARAM(true,0));
           SendDlgItemMessage (hwnd,IDC_OFFONTNAME ,WM_SETFONT,(WPARAM) GetStockObject(SYSTEM_FONT),MAKELPARAM(true,0));
//
//  Setup the font type list
//
           int i;
           SendDlgItemMessage (hwnd,IDC_OFFONT,CB_RESETCONTENT,0,0);
           for (i = 0; i < NUMBER_FONTS; i++) SendDlgItemMessage (hwnd,IDC_OFFONT,CB_ADDSTRING,0,(LPARAM) get_string(fonts[i]));
           SendDlgItemMessage (hwnd,IDC_OFFONT,CB_SETCURSEL,FONT_FILE,0);
//
//  Setup the font list.
//
           HDC hdc;
           hdc          = GetDC(hwnd);
           current_font = FONT_NONE;
           EnumFontFamilies  (hdc,NULL,(FONTENUMPROC) enum_fonts ,(LPARAM) hwnd);
           ReleaseDC         (hwnd,hdc);
           select_string     (hwnd,IDC_OFASCIIFONT  ,cfg->ascii_font.name);
           setup_jfonts      (hwnd);
         }
         return (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           INPUT_CHECK (IDC_OFHEIGHT);
//
//  Change to the font.
//
           case IDC_OFASCIIFONT:
                SendDlgItemMessage (hwnd,IDC_OFASCIIFONT,CB_GETLBTEXT,SendDlgItemMessage(hwnd,IDC_OFASCIIFONT,CB_GETCURSEL,0,0),(LPARAM) cfg->ascii_font.name);
                DeleteObject (font);
                font = edit_font.open_ascii (cfg->ascii_font.name);
                SetDlgItemText     (hwnd,IDC_OFASCIINAME,cfg->ascii_font.name);
                SendDlgItemMessage (hwnd,IDC_OFASCIINAME,WM_SETFONT,(WPARAM) font,MAKELPARAM(true,0));
                return (true);
//
//  Working with the kanji fonts
//
           case IDC_OFAUTO:
           case IDC_OFFONT:
                change_font (hwnd);
                return (true);
           case IDC_OFFONTNAME:  
                check_height (hwnd);
                return (true);
//
//  Changes to the show all buttons will require that we save the current settings,
//  then reinitialize the fonts lists.
//                
           case IDC_OFSHOWALL:
                cfg->all_fonts = IsDlgButtonChecked(hwnd,IDC_OFSHOWALL);
                setup_jfonts (hwnd);
                return (true);
         }
         break;
    case WM_GETDLGVALUES:
         cfg->relax_punctuation = IsDlgButtonChecked(hwnd,IDC_OFRELAXPUNCT);
         cfg->relax_smallkana   = IsDlgButtonChecked(hwnd,IDC_OFRELAXSMALL);
         change_font (hwnd);                        // This forces a read of the font settings.
         return (true);
  }
  return (false);
}

//--------------------------------
//
//  Dialog box handler for File/Clipboard page.
//
//      IDC_OCSAVEBACKUP  Make backup
//      IDC_OCCLIPIMPORT  Clipboard import format.
//      IDC_OCCLIPEXPORT  Clipboard export format.
//      IDC_OCDUPOPEN     Open duplicate files.
//      IDC_OCDUPCHANGE   Change to open file.
//      IDC_OCDUPPROMPT   Ask the user.
//
#define NUMBER_CLIPS    ((int) (sizeof(clip_formats)/sizeof(int)))      // Number of clipboard formats.

static BOOL CALLBACK options_file (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  static int clip_formats[] = { IDS_OC_AUTO,IDS_OC_EUC,IDS_OC_SHIFTJIS,IDS_OC_NEWJIS,IDS_OC_OLDJIS,IDS_OC_NECJIS,IDS_OC_UNICODE,IDS_OC_UTF7,IDS_OC_UTF8 };
  int i;
  HWND himport,hexport;
  switch (msg) {
    case WM_INITDIALOG:
         CheckDlgButton (hwnd,IDC_OCSAVEBACKUP,cfg->backup_files);
         CheckDlgButton (hwnd,IDC_OCDUPOPEN+cfg->double_open,true);
         himport = GetDlgItem(hwnd,IDC_OCCLIPIMPORT);
         hexport = GetDlgItem(hwnd,IDC_OCCLIPEXPORT);
         for (i = 0; i < NUMBER_CLIPS; i++) {
           SendMessage (himport,CB_ADDSTRING,0,(LPARAM) get_string(clip_formats[i]));
           if (i) SendMessage (hexport,CB_ADDSTRING,0,(LPARAM) get_string(clip_formats[i]));
         } 
         i = (cfg->clip_read == FILETYPE_AUTODETECT) ? 0 : cfg->clip_read-(FILETYPE_EUC-1);
         SendMessage (himport,CB_SETCURSEL,i,0);
         SendMessage (hexport,CB_SETCURSEL,cfg->clip_write-FILETYPE_EUC,0);
#ifndef WINCE
         CheckDlgButton (hwnd,IDC_OCNOBITMAP ,cfg->no_BITMAP);
         CheckDlgButton (hwnd,IDC_OCNOUNICODE,cfg->no_UNICODETEXT);
#endif  WINCE
         return (true);
    case WM_GETDLGVALUES:
         cfg->backup_files = IsDlgButtonChecked(hwnd,IDC_OCSAVEBACKUP);
         i = SendDlgItemMessage(hwnd,IDC_OCCLIPIMPORT,CB_GETCURSEL,0,0);
         cfg->clip_read  = !i ? FILETYPE_AUTODETECT : i+(FILETYPE_EUC-1);
         cfg->clip_write = (byte) (FILETYPE_EUC+SendDlgItemMessage(hwnd,IDC_OCCLIPEXPORT,CB_GETCURSEL,0,0));
         if      (IsDlgButtonChecked(hwnd,IDC_OCDUPOPEN  )) cfg->double_open = DOUBLE_OPEN;
         else if (IsDlgButtonChecked(hwnd,IDC_OCDUPCHANGE)) cfg->double_open = DOUBLE_CHANGE;
         else                                               cfg->double_open = DOUBLE_PROMPT;  
#ifndef WINCE
         cfg->no_BITMAP      = IsDlgButtonChecked(hwnd,IDC_OCNOBITMAP);
         cfg->no_UNICODETEXT = IsDlgButtonChecked(hwnd,IDC_OCNOUNICODE);
#endif  WINCE
         return (true);
  }
  return (false);
}

//--------------------------------
//
//  Dialog box handler for Misc page.
//
static BOOL CALLBACK options_misc (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  int i;
  switch (msg) {
    case WM_INITDIALOG:
         CheckDlgButton (hwnd,IDC_OMINFOTITLES  ,cfg->info_titles);
         CheckDlgButton (hwnd,IDC_OMCOLORKANJI  ,cfg->colorkanji_mode);
         CheckDlgButton (hwnd,(cfg->colorkanji_mode == COLORKANJI_MATCH) ? IDC_OMINLIST : IDC_OMNOTINLIST,true);
         CheckDlgButton (hwnd,IDC_OMCOLORBITMAP ,cfg->colorkanji_bitmap);
         CheckDlgButton (hwnd,IDC_OMCOLORPRINT  ,cfg->colorkanji_print);
         CheckDlgButton (hwnd,IDC_OMINSERTLINE  ,cfg->paste_newpara);
         CheckDlgButton (hwnd,IDC_OMINFOCOMPRESS,cfg->info_compress);
         CheckDlgButton (hwnd,IDC_OMSINGLEINFO  ,cfg->info_onlyone);
         SendMessage (hwnd,WM_COMMAND,IDC_OMCOLORKANJI,0);
         SendDlgItemMessage (hwnd,IDC_OMKANJIBOX,WMU_SETWINDOWVALUE,0,cfg->colorkanji_color);
         return (true);
    case WMU_COLORCHANGE:                       // User clicked the color box, so select a new color.
         wParam = IDC_OMKANJICOLOR;
    case WM_COMMAND:    // **** FALL THROUGH ****
         switch (LOWORD(wParam)) {
           case IDC_OMCOLORKANJI:
                for (i = IDC_OMINLIST; i <= IDC_OMCOLORPRINT; i++) EnableWindow (GetDlgItem(hwnd,i),IsDlgButtonChecked(hwnd,IDC_OMCOLORKANJI));
                return (true);
           case IDC_OMINFOSETUP:
                info_config (hwnd);
                return (true);
           case IDC_OMKANJICOLOR:
                cfg->colorkanji_color = get_color (hwnd,cfg->colorkanji_color);
                SendDlgItemMessage (hwnd,IDC_OMKANJIBOX,WMU_SETWINDOWVALUE,0,cfg->colorkanji_color);
                return (true);
         }
         break;
    case WM_GETDLGVALUES:
         cfg->paste_newpara     = IsDlgButtonChecked(hwnd,IDC_OMINSERTLINE);
         cfg->info_titles       = IsDlgButtonChecked(hwnd,IDC_OMINFOTITLES);
         cfg->colorkanji_bitmap = IsDlgButtonChecked(hwnd,IDC_OMCOLORBITMAP);
         cfg->colorkanji_print  = IsDlgButtonChecked(hwnd,IDC_OMCOLORPRINT);
         cfg->info_compress     = IsDlgButtonChecked(hwnd,IDC_OMINFOCOMPRESS);
         cfg->info_onlyone      = IsDlgButtonChecked(hwnd,IDC_OMSINGLEINFO);
         if (!IsDlgButtonChecked(hwnd,IDC_OMCOLORKANJI)) cfg->colorkanji_mode = COLORKANJI_OFF;
           else cfg->colorkanji_mode = IsDlgButtonChecked(hwnd,IDC_OMINLIST) ? COLORKANJI_MATCH : COLORKANJI_NOMATCH;
         return (true);
  }
  return (false);
}

//--------------------------------
//
//  Dialog handler for Advanced page.
//
//      IDC_OAALLOCSIZE       Allocation size for lines.
//      IDC_OACONVERTSIZE     Size for user selection table for kana->kanji convert.
//      IDC_OAUNDOLEVELS      Undo levels.
//      IDC_OAFONTCACHE       Font cache size.
//      IDC_OACACHEDISPLAY    Cache display font.
//
#define PAGES (sizeof(page_names)/sizeof(short))

static BOOL CALLBACK options_advanced (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  static short code_pages[] = { CODEPAGE_AUTO,CODEPAGE_EASTEUROPE,CODEPAGE_CYRILLIC,CODEPAGE_USA,CODEPAGE_GREEK,CODEPAGE_TURKISH,CODEPAGE_HEBREW,CODEPAGE_ARABIC,CODEPAGE_BALTIC,CODEPAGE_VIETNAMESE };
  static short page_names[] = { IDS_CP_AUTO  ,IDS_CP_EASTEUROPE  ,IDS_CP_CYRILLIC  ,IDS_CP_USA  ,IDS_CP_GREEK  ,IDS_CP_TURKISH  ,IDS_CP_HEBREW  ,IDS_CP_ARABIC  ,IDS_CP_BALTIC  ,IDS_CP_VIETNAMESE   };
  int i;
  switch (msg) {
    case WM_INITDIALOG:
         for (i = 0; i < PAGES; i++) SendDlgItemMessage (hwnd,IDC_OACODEPAGE,CB_ADDSTRING,0,(LPARAM) get_string(page_names[i]));
         for (i = 0; (i < PAGES) && (code_pages[i] != cfg->code_page); i++);
         if (i == PAGES) i = 0;
         SendDlgItemMessage (hwnd,IDC_OACODEPAGE,CB_SETCURSEL,i,0);
         SetDlgItemInt      (hwnd,IDC_OAALLOCSIZE       ,cfg->alloc       ,false);
         SetDlgItemInt      (hwnd,IDC_OACONVERTSIZE     ,cfg->convert_size,false);
         SetDlgItemInt      (hwnd,IDC_OAUNDOLEVELS      ,cfg->undo_number ,false);
         SetDlgItemInt      (hwnd,IDC_OAFONTCACHE       ,cfg->font_cache  ,false);
         SetDlgItemInt      (hwnd,IDC_OAHISTORY         ,cfg->history_size,false);
         SetDlgItemInt      (hwnd,IDC_OABUFFER          ,cfg->dict_buffer ,false);
         CheckDlgButton     (hwnd,IDC_OACACHEDISPLAY    ,cfg->cache_displayfont);
         CheckDlgButton     (hwnd,IDC_OACACHEINFO       ,cfg->cache_info);
         CheckDlgButton     (hwnd,IDC_OASEARCHOPEN      ,cfg->keep_find);
         return (true);
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
    case WM_COMMAND: 
         INPUT_CHECK (IDC_OAALLOCSIZE);
         INPUT_CHECK (IDC_OACONVERTSIZE);
         INPUT_CHECK (IDC_OAUNDOLEVELS);
         INPUT_CHECK (IDC_OAFONTCACHE);
         INPUT_CHECK (IDC_OAHISTORY);
         INPUT_CHECK (IDC_OABUFFER);
         break;
#endif (defined(WINCE_PPC) || (defined(WINCE_POCKETPC))
    case WM_GETDLGVALUES:
         cfg->alloc             = get_int(hwnd,IDC_OAALLOCSIZE  ,16  ,1024   ,cfg->alloc       );
         cfg->convert_size      = get_int(hwnd,IDC_OACONVERTSIZE,10  ,2000   ,cfg->convert_size);
         cfg->undo_number       = get_int(hwnd,IDC_OAUNDOLEVELS , 3  ,1000   ,cfg->undo_number );
         cfg->font_cache        = get_int(hwnd,IDC_OAFONTCACHE  ,100 ,7000   ,cfg->font_cache  );
         cfg->history_size      = get_int(hwnd,IDC_OAHISTORY    ,0   ,30000  ,cfg->history_size);
         cfg->dict_buffer       = get_int(hwnd,IDC_OABUFFER     ,2048,INT_MAX,cfg->dict_buffer );
         cfg->cache_displayfont = IsDlgButtonChecked(hwnd,IDC_OACACHEDISPLAY);
         cfg->cache_info        = IsDlgButtonChecked(hwnd,IDC_OACACHEINFO);
         cfg->keep_find         = IsDlgButtonChecked(hwnd,IDC_OASEARCHOPEN);
         cfg->code_page         = code_pages[SendDlgItemMessage(hwnd,IDC_OACODEPAGE,CB_GETCURSEL,0,0)];
         return (true);
  }
  return (false);
}

//--------------------------------
//
//  Main dirver for the options dialog.
//
void do_options () {
  int        i;
  struct cfg new_config;
//
//  Static data for the page dialog.
//  
  static TabPage pages[] = {
    { IDS_OPTS_GENERAL ,IDD_OPTGENERAL ,(DLGPROC) options_general ,IDH_OPTIONS_GENERAL  },
    { IDS_OPTS_DISPLAY ,IDD_OPTDISPLAY ,(DLGPROC) options_display ,IDH_OPTIONS_DISPLAY  },
    { IDS_OPTS_FONT    ,IDD_OPTFONT    ,(DLGPROC) options_font    ,IDH_OPTIONS_FONT     },
    { IDS_OPTS_FILECLIP,IDD_OPTFILE    ,(DLGPROC) options_file    ,IDH_OPTIONS_FILE     },
    { IDS_OPTS_MISC    ,IDD_OPTMISC    ,(DLGPROC) options_misc    ,IDH_OPTIONS_MISC     },
    { IDS_OPTS_ADVANCED,IDD_OPTADVANCED,(DLGPROC) options_advanced,IDH_OPTIONS_ADVANCED },
  };
  static TabSetup setup = { pages,sizeof(pages)/sizeof(TabPage),0,(DLGPROC) NULL };
//
//  Intialize the variables and call the dialog box.
//
  new_config = jwp_config.cfg;
  cfg        = &new_config;
  font       = edit_font.open_ascii(jwp_config.cfg.ascii_font.name);
  i = TabDialog(IDD_OPTIONS,&setup);
  DeleteObject (font);
//
//  Change the parameters.
//
  if (i) jwp_config.set (&new_config);
  return;
}

//
//  End Options Dialog handlers.
//
//===================================================================

//===================================================================
//
//  Main windows rotuine.  WinMain and the main window procedures.
//

#define WINCLASS_MAIN   TEXT("JWPce-Main")  // Main window class.

HWND main_window = null;                    // THE WINDOW!

static LRESULT CALLBACK winproc_main (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);

#ifdef WINELIB
extern "C" { 
  int WINAPI WinMain (HINSTANCE hInstance,HINSTANCE hPrevInstance,TCHAR *szCmdLine,int iCmdShow); 
}
#endif WINELIB

//--------------------------------
//
//  Windows main routine.
//
int WINAPI WinMain (HINSTANCE hInstance,HINSTANCE hPrevInstance,TCHAR *szCmdLine,int iCmdShow) {
  MSG      msg;
  WNDCLASS wndclass,*wclass;
  DIALOG_node       *node;
//
//  For PPC's check to see if another version of JWPce is running.
//
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  HANDLE mutex;
  mutex = CreateMutex(NULL,false,TEXT("JWPce-Mutex"));
  if (ERROR_ALREADY_EXISTS == GetLastError()) {
    if (main_window = FindWindow(WINCLASS_MAIN,NULL)) {
      CloseHandle (mutex);
      SetForegroundWindow (main_window);
    }
    return (0);
  }
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
//
//  Activate the COM libararies
//
#ifdef WINCE
  if (S_OK != CoInitializeEx(NULL,COINIT_MULTITHREADED)) terminate (IDS_TERM_COM);
#else  wince
  if (S_OK != CoInitialize(NULL)) terminate (IDS_TERM_COM);
#endif WINCE
//
//  Support for international versions.  Open the language DLL and see if we can read it.
//
  instance = hInstance;
  if (!(language = LoadLibrary(jwp_config.name(TEXT("JWPce_lang.dll"),OPEN_READ,false)))) language = instance;
    else {
      TCHAR *ptr = get_string(IDS_LANG_ID);
      if ((ptr[0] != LANGUAGE_CODE) || strnicmp(ptr+1,VERSION_STRING,4)) {
        FreeLibrary (language);
        language = instance;
        JMessageBox (NULL,IDS_LANG_ERRORTEXT,IDS_LANG_ERRORTITLE,MB_OK | MB_ICONERROR,jwp_config.name());
      }
    }  
//
//  Do some initialization.
//
  InitCommonControls ();
#ifndef WINCE
  if (!(hmenu  = LoadMenu(language,MAKEINTRESOURCE(IDM_MAINMENU)))) terminate (IDS_TERM_MENU);
#endif  WINCE
  if (!(popup  = LoadMenu(language,MAKEINTRESOURCE(IDM_POPUP   )))) terminate (IDS_TERM_POPUP);
  if (!(haccel = LoadAccelerators(hInstance,MAKEINTRESOURCE(IDA_MAINACCEL)))) terminate (IDS_TERM_ACCEL);
  if (!hPrevInstance) {
    wclass = &wndclass;
    memset (&wndclass,0,sizeof(wndclass));
//  wndclass.style         = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc   = winproc_main;
//  wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 4;                     // All windows carry 4 extra bytes for our data.
    wndclass.hInstance     = hInstance;
    wndclass.hIcon         = LoadIcon (hInstance,MAKEINTRESOURCE(IDI_MAINICON));
    wndclass.hbrBackground = (HBRUSH) (COLOR_MENU+1);
//  wndclass.lpszMenuName  = NULL;
    wndclass.lpszClassName = WINCLASS_MAIN;
#ifndef WINCE
    wndclass.hCursor       = LoadCursor (null,IDC_ARROW);
#endif  WINCE
    if (!RegisterClass(&wndclass)) terminate (IDS_TERM_REGCLASS);
  }
  else {
    wclass = NULL;
  }
//
//  Setup default values for the current directoy.  This will get 
//  overwritten when the user changes things.
//
  TCHAR buffer[SIZE_BUFFER];
#if   defined(WINELIB)
  SetCurrentDirectory (TEXT("f:\\"));                           // Fixed location for WINELIB
#elif defined (WINCE)
  set_currentdir      (get_folder(CSIDL_PERSONAL,buffer),false);
#else 
  SetCurrentDirectory (get_folder(CSIDL_PERSONAL,buffer));
#endif
//
//  Read configuration.
//
  if (jwp_config.read()) jwp_config.write ();
//
//  Intialize main window.
//
#if   defined(WINCE_POCKETPC)
  main_window = CreateWindowEx(0,WINCLASS_MAIN,WINCLASS_MAIN,WS_VISIBLE,0,0,CW_USEDEFAULT,CW_USEDEFAULT,NULL,NULL,instance,NULL);
  ShowWindow   (main_window,SW_SHOW);
  UpdateWindow (main_window);
  do_commandbar   ();
  memset (&activateinfo,0,sizeof(activateinfo));
  activateinfo.cbSize = sizeof(activateinfo);
  input_panel (main_window,true);
  SetForegroundWindow (main_window);                // Necessary because CE does not restore JWPce to top after configuration message!
int working;

#elif defined(WINCE)
  if (!(main_window = CreateWindowEx(WS_EX_CONTEXTHELP,WINCLASS_MAIN,TEXT("JWPce-Main Window"),WS_VISIBLE,0,0,CW_USEDEFAULT,CW_USEDEFAULT,null,null,hInstance,null))) terminate (IDS_TERM_WINDOW);
  ShowWindow          (main_window,SW_SHOW);
  UpdateWindow        (main_window);
  do_commandbar       ();
  SetForegroundWindow (main_window);                // Necessary because CE does not restore JWPce to top after configuration message!
#else  WINCE
  if (!(main_window = CreateWindowEx(WS_EX_ACCEPTFILES | WS_EX_CONTEXTHELP,WINCLASS_MAIN,TEXT("JWPce-Main Window"),WS_OVERLAPPEDWINDOW,jwp_config.cfg.x,jwp_config.cfg.y,jwp_config.cfg.xs,jwp_config.cfg.ys,null,null,hInstance,null))) terminate (IDS_TERM_WINDOW);
  if ((iCmdShow != SW_SHOWMINIMIZED) && jwp_config.cfg.maximize && jwp_config.cfg.usedims) iCmdShow = SW_SHOWMAXIMIZED;
  ShowWindow       (main_window,iCmdShow);
  UpdateWindow     (main_window);
  SetMenu          (main_window,hmenu);
  jwp_tool.process (false);
#endif WINCE
  do_install   (false);
//
//  Intialize everybody else.
//
  do_clipboard  (WM_CREATE,0);
  initialize_cp ();
  if (initialize_fonts())            terminate (IDS_TERM_INITIALIZE,get_string(IDS_TERM_FONTS));
  if (jwp_stat.initialize(wclass))   terminate (IDS_TERM_INITIALIZE,get_string(IDS_TERM_STATUS));
  if (jwp_conv.initialize(wclass))   terminate (IDS_TERM_INITIALIZE,get_string(IDS_TERM_KANAKANJI));
  if (initialize_edit(wclass))       terminate (IDS_TERM_INITIALIZE,get_string(IDS_TERM_CONTROLS));
  if (initialize_view(wclass))       terminate (IDS_TERM_INITIALIZE,get_string(IDS_TERM_FILEWINDOW));
  if (initialize_info(wclass))       terminate (IDS_TERM_INITIALIZE,get_string(IDS_TERM_INFO));
  if (initialize_radlookup(wclass))  terminate (IDS_TERM_INITIALIZE,get_string(IDS_TERM_LOOKUP));
  initialize_printer (&jwp_config.cfg.page);
//
//  Readjust the status line in case the user puts the status bar on top
//  of the kanji bar.
//
  jwp_stat.adjust (null);
//
//  Open files for the user.
//
  if (open_files(szCmdLine)) terminate (IDS_TERM_INITIALIZE,get_string(IDS_TERM_STARTUP));
  set_mode (MODE_KANJI);
//
//  Main message loop.
//
  while (GetMessage(&msg,null,0,0)) {
    for (node = dialog_list; node && !IsDialogMessage(node->hwnd,&msg); node = node->next);
    if (!node) {
      if (!TranslateAccelerator(main_window,haccel,&msg)) {
        TranslateMessage (&msg);
        DispatchMessage  (&msg);
      }
    }
  }
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))      // Cleanup for PPC task blocking.
  if (mutex) CloseHandle (mutex);
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  CoUninitialize ();                // Cleanup COM interface.
  return (msg.wParam);
}

//--------------------------------
//
//  Main window procedure.  Menus, junk, etc.
//

static LRESULT CALLBACK winproc_main (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  int ctrl,shift;
  static int block = false;
  switch (iMsg) {
//
//  Standard, when the user moves, or changes size we adjust the windows.
//
    case WM_SIZE:
    case WM_MOVE:
#ifdef WINCE
         jwp_stat.adjust (null);
         jwp_conv.adjust (null);
         adjust_view (file_window);
#else WINCE
         if (!IsIconic(hwnd)) {         // Don't mess with window when it is minimized!
           jwp_stat.adjust (null);
           jwp_conv.adjust (null);
           adjust_view     (file_window);
           jwp_tool.adjust ();
         }
#endif WINCE
         return (0);
//
//  Noramlly this could be reached by Alt+F4.  We have, however, 
//  "accelerated" Alt+F4 to the IDM_EXIT command, thus we can only 
//  get here by clicking the close button.
//
    case WM_CLOSE:
		 if	     (GetKeyState(VK_MENU   ) < 0) shift = false;   // Alt click -> force close program
		 else if (GetKeyState(VK_CONTROL) < 0) shift = true;    // ctrl click -> force close file
		 else                                  shift = jwp_config.cfg.close_does_file;
         SendMessage (hwnd,WM_COMMAND,shift ? IDM_FILE_CLOSE : IDM_FILE_EXIT,0);
         return (0);
//
//  Destroy means we are about to die, so clean up.
//
    case WM_DESTROY:
#ifndef WINCE
         jwp_tool.process (true);       // Close the command bar window.
#endif WINCE
         jwp_config.done ();            // Save user configuration
         jwp_conv.done   ();            // Save user kana->kanji selections.
#ifdef WINCE_PPC
         CommandBar_Destroy (button_bar);
#endif WINCE_PPC
#ifdef WINCE
         CommandBar_Destroy (command_bar);
         if (tip_data) free (tip_data);
#else  WINCE
//       DestroyMenu     (hmenu);       // Not necessary, the window will destroy the menu.
#endif WINCE
         DestroyMenu     (popup);       // Free lots of memory.
//       DeleteObject    (haccel);      // Not necssary to delete, will be freed when window closes.
         free_dictionary ();            // Free Dictionary allocation.
         free_info       ();            // Free information buffer
         clear_clipboard ();
         free_fonts      ();            // Close open fonts
         if (language && (language != instance)) FreeLibrary (language);
         PostQuitMessage (0);           // saiyanara (I hate romaji!!!)
         return (0);
//
//  Handle the caret (man is Windows strange -- the operating system should do this).
//
    case WM_SETFOCUS:
         INPUT_RESTORE ();
         file_list.add (NULL);
         if (jwp_file) jwp_file->caret_on ();
         return (0);
    case WM_KILLFOCUS:
         INPUT_STATUS ();
         if (jwp_file) jwp_file->caret_off ();
         return (0);
//
//  Char message is gnerated for most ascii.  This is generaly pass on 
//  to the input routines or the ascii->kana input rotuines, etc.
//
//  Odd, but tab also shows up here.  We have to blcok tab, otherwise
//  it shows up both there and on the WM_KEYDOWN message.  Also ^I casues
//  a tab to be sent.
//
    case WM_CHAR:
         if (wParam != '\t') jwp_file->do_char (wParam);
         return (0);
//
//  These are all the special keys (home, F5, Ctrl+K, etc.)  These 
//  are passed to the event handler in jwp_file.cpp.
//
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
int dont_really_like_the_blockout_but_it_works;
    case WM_KEYUP:
         if (wParam == VK_F23) { block = false; jwp_file->do_key (wParam,false,false); }
         return (0);
    case WM_KEYDOWN:
         if (wParam == VK_F23) { block = true; return (0); }
         if (block) return (0);
         shift = (GetKeyState(VK_SHIFT)   < 0);
         ctrl  = (GetKeyState(VK_CONTROL) < 0);
         jwp_file->do_key (wParam,ctrl,shift);
         return (0);
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
    case WM_KEYDOWN:
         shift = (GetKeyState(VK_SHIFT)   < 0);
         ctrl  = (GetKeyState(VK_CONTROL) < 0);
         jwp_file->do_key (wParam,ctrl,shift);
         return (0);
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
//
//  Wheel mouse support.
//
#ifndef WINCE
    case WM_MOUSEWHEEL:
         static int delta;                                                  // Accumulation point for deltas.
         int i;
         shift  = SystemParametersInfo(SPI_GETWHEELSCROLLLINES,0,&ctrl,0);  // Scroll amount.
         ctrl   = LOWORD(wParam);                                           // Keys
         delta += (short) HIWORD(wParam);                                   // Delta
//
//  Control -- flip through open files.
//
         if (ctrl & MK_CONTROL) {
           if      (delta      > WHEEL_DELTA) { delta = 0; jwp_file->next->activate (); } 
           else if (abs(delta) > WHEEL_DELTA) { delta = 0; jwp_file->prev->activate (); }
         }
//
//  Shift -- skip pages.
//
         else if ((ctrl & MK_SHIFT) || (shift == WHEEL_PAGESCROLL)) {
           while (delta > WHEEL_DELTA) {
             jwp_file->v_scroll (SB_PAGEUP);
             delta -= WHEEL_DELTA;
           }
           while (abs(delta) > WHEEL_DELTA) {
             jwp_file->v_scroll (SB_PAGEDOWN);
             delta += WHEEL_DELTA;
           }
         }
//
//  Normal -- skip lines.
//
         else {
           while (delta > WHEEL_DELTA) {
             for (i = 0; i < shift; i++) jwp_file->v_scroll (SB_LINEUP);
             delta -= WHEEL_DELTA;
           }
           while (abs(delta) > WHEEL_DELTA) {
             for (i = 0; i < shift; i++) jwp_file->v_scroll (SB_LINEDOWN);
             delta += WHEEL_DELTA;
           }
         }
         return (0);
#endif  WINCE
//
//  Clipboard support.
//
    case WM_DESTROYCLIPBOARD:
         clear_clipboard ();
         return (0);
    case WM_RENDERALLFORMATS:
    case WM_RENDERFORMAT:
         do_clipboard (iMsg,wParam);
         return (0);
//
//  IME support.
//
#ifndef WINCE
    case WM_IME_CHAR:
         jwp_file->ime_char (wParam,IsWindowUnicode(hwnd));
         return (0);
#endif WINCE
//
//  Support for drag and drop:
//
#ifndef WINCE
    case WM_DROPFILES:
         do_drop ((HDROP) wParam);
         return (0);
#endif WINCE
//
//  If we are a PPC we need to check for changes in the input pannel.  We use this to change
//  the size of the display if the input panel is open or closed.
//
#if defined(WINCE_POCKETPC)
    case WM_CREATE:
         int     cx,cy,dy;
         SIPINFO si;
         memset (&si,0,sizeof(si));
         si.cbSize = sizeof(si);
         SHSipInfo (SPI_GETSIPINFO,0,&si,false);
         cx = si.rcVisibleDesktop.right - si.rcVisibleDesktop.left;
         cy = si.rcVisibleDesktop.bottom - si.rcVisibleDesktop.top;
         dy = GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYFIXEDFRAME);
         if (!(si.fdwFlags & SIPF_ON) || ((si.fdwFlags & SIPF_ON) && !(si.fdwFlags & SIPF_DOCKED)))
         cy -= dy;
         SetWindowPos (hwnd,NULL,0,dy,cx,cy,SWP_NOZORDER);
         return (0);
    case WM_SETTINGCHANGE:
         SHHandleWMSettingChange (hwnd,wParam,lParam,&activateinfo);
         return (0);
    case WM_ACTIVATE:
         SHHandleWMActivate (hwnd,wParam,lParam,&activateinfo,0);
         return (0);
#elif defined(WINCE_PPC)
    case WM_CREATE:
         wParam = SPI_SETSIPINFO;
    case WM_SETTINGCHANGE: 
         if (SPI_SETSIPINFO == wParam) {
           SIPINFO si;
           memset (&si,0,sizeof(si));
           si.cbSize = sizeof(si);
           if (!SHSipInfo(SPI_GETSIPINFO,0,&si,0)) return (0);
           MoveWindow (hwnd,si.rcVisibleDesktop.left,si.rcVisibleDesktop.top,si.rcVisibleDesktop.right-si.rcVisibleDesktop.left+1,si.rcVisibleDesktop.bottom-si.rcVisibleDesktop.top+1,true);
           return (0);
         }
#else WINCE / WIN
    case WM_CREATE:
         return (0);
#endif WINCE_PPC
//
//  Messages associated with the Windows toolbar (not Windows CE)
//
#ifndef WINCE
    case WM_NOTIFY: {
           NMHDR *pnmh = (NMHDR *) lParam;
           switch (pnmh->code) {
             case TBN_CUSTHELP:
                  do_help (hwnd,IDH_OPTIONS_TOOLBAR);
                  return (true);
             case TBN_RESET:
                  jwp_tool.reset ();
                  return (true);  
             case TBN_QUERYINSERT:
             case TBN_QUERYDELETE:
                  return (true);
             case TBN_GETBUTTONINFO: 
                  return (jwp_tool.info(pnmh));
             case TTN_NEEDTEXT: 
                  return (jwp_tool.tip(pnmh));
           }
         }
         break;
#endif WINCE
//
//  The menu commands, lots of them.  Most of these are staight forward,
//  calling some direct rotuine that dose the work.  Or duplicating 
//  keyboard input to do the work.  This is a little tricky.
//
//  There are two ways to get things done here.  Most every command 
//  has a keyboard input and a menu imput (often more than one keyboard 
//  input).  To syncronize these things, one can use accelerators to 
//  cause the keyboard to trip a menu command, or one can simply have 
//  the menu command call the keyboard handler for the event.  Both 
//  systems are use here intensionally.  
//
//  To JWPce a Japanese edit control is simply a single paragarph, single
//  line file containning a long line.  This allows almost all the 
//  code to be duplicated for the main editor and the edit boxes.  The 
//  main differentce is the dispach routine is different (the window
//  procedure).  
//
//  Both window procedures use the same handlers for the WM_CHAR and 
//  WM_KEYDOWN (slight difference on the second) messages.  Thus 
//  functions that are valid on an edit control are implemented 
//  as a responce to a keyboard event, and the menu command simply 
//  generates the keyboard event (i.e. paste, copy, cut, convert).  
//
//  Functions, however, that are not valid on an edit control are 
//  are implemented as a menu command and a keyboard option is 
//  implemented as an accelerator (i.e. open, close, save, etc.).
//
//  There are also some fucntions implemented as both.  For example, 
//  the mode changes are implemented directly in the menu and the 
//  keyboard, because to cause the menu to invode the keyboard would 
//  have taken more code than invoding the new_mode rotuine directly.
//
    case WM_COMMAND:
         jwp_file->do_menu (wParam);
         return (0);
    case WM_HELP:
         do_help (main_window,0);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//
//  End Main Window routine.
//
//===================================================================





//--------------------------------
//
//  Menu processor routine, does all actual menu commands, which can
//  only come from the main window anyway.
//
void JWP_file::do_menu (int wParam) {
  int       i;
  JWP_file *file;
  switch (LOWORD(wParam)) {
// --------------------------------------------- File menu
    case IDM_FILE_NEW:
         jwp_conv.clear ();
         new JWP_file (NULL,FILETYPE_UNNAMED);
         break;
    case IDM_FILE_OPEN:
         jwp_conv.clear ();
         do_fileopen ();
         break;
    case IDM_FILE_REVERT:
         revert ();
         break;
    case IDM_FILE_CLOSE:
         jwp_conv.clear     ();
         close              (false);
         jwp_file->activate ();         // Can't just use activate(), because we just closed!
         break;
    case IDM_FILE_CLOSEALL:
         jwp_conv.clear ();
         while (jwp_file->next != jwp_file) {                  
           if (jwp_file->close(false)) break;
         }
         if (new JWP_file(NULL,FILETYPE_UNNAMED)) jwp_file->next->close (false);
         break;
    case IDM_FILE_SAVE:
         jwp_conv.clear ();
         save           (NULL);
         break;
    case IDM_FILE_SAVEAS:
         jwp_conv.clear ();
         save_as        ();
         break;
    case IDM_FILE_SAVEALL:
         jwp_conv.clear ();
         file = jwp_file;
         do {
           jwp_file->save (NULL);
           jwp_file = jwp_file->next;
         } while (jwp_file != file);
         break;
    case IDM_FILE_DELETE: 
         delete_file ();
         break;
    case IDM_FILE_PRINT:
         print (false);
         break;
    case IDM_FILE_PRINTERSETUP:
         print (true);
         break;
    case IDM_FILE_EXIT:
         while (dialog_list) {                  // Close all dialogs open, and check if this is OK
           if (!dialog_list->closeable) {       // Can't just close this one, we have to shut it down!
             int i;
             SendMessage(dialog_list->hwnd,WMU_OKTODESTROY,0,(long) &i);
             if (!i) return;
           }
           DestroyWindow (dialog_list->hwnd);
         }                                      // This is necessary to prevent a system crash with the modeless dialogs
         jwp_conv.clear ();
         jwp_config.write_files ();             // Save recent/loaded files.
         while (jwp_file) {                  
           if (jwp_file->close(true)) break;    // Close all files and abort.
         }
         break;
// --------------------------------------------- Edit menu
    case IDM_EDIT_UNDO:
         do_key (VK_Z,true,false);
         break;
    case IDM_EDIT_REDO:
         do_key (VK_Y,true,false);
         break;
    case IDM_EDIT_MODE_ASCII:
         set_mode (MODE_ASCII);
         break;
    case IDM_EDIT_MODE_KANJI:
         set_mode (MODE_KANJI);
         break;
    case IDM_EDIT_MODE_JASCII:
         set_mode (MODE_JASCII);
         break;
    case IDM_EDIT_MODE_TOGGLE:              // This menu item does not have an actual menu 
         set_mode (MODE_TOGGLE);            //   listing, but is rather just a place for an
         break;                             //   accelerator key listing.
    case IDM_EDIT_CUT:
         do_key (VK_X,true,false);
         break;
    case IDM_EDIT_COPY:
         do_key (VK_C,true,false);
         break;
    case IDM_EDIT_PASTE:
         do_key (VK_V,true,false);
         break;
    case IDM_EDIT_SELECTALL:
         do_key (VK_A,true,true);
         break;
    case IDM_EDIT_INSERTPAGEBREAK:
         do_key (VK_RETURN,true,false);
         break;
    case IDM_EDIT_SEARCH:
         jwp_search.do_search (NULL);
         break;
    case IDM_EDIT_REPLACE:
         jwp_search.do_replace ();
         break;
    case IDM_EDIT_REVERSESEARCH:
         jwp_config.cfg.search_back = !jwp_config.cfg.search_back;
    case IDM_EDIT_FINDNEXT:         // *** FALL THROUGH ***
         jwp_search.do_next (NULL);
         break;
// --------------------------------------------- Kanji menu
    case IDM_KANJI_CONVERT:
         do_key (VK_F2,false,false);
         break;
    case IDM_KANJI_RADICALLOOKUP:
         do_key (VK_L,true,false);
         break;
    case IDM_KANJI_BUSHULOOKUP:
         do_key (VK_L,true,true);
         break;
    case IDM_KANJI_BUSHU2LOOKUP:
         do_key (VK_B,true,true);
         break;
    case IDM_KANJI_SKIPLOOKUP:
         do_key (VK_S,true,true);
         break;
    case IDM_KANJI_HSLOOKUP:
         do_key (VK_H,true,false);
         break;
    case IDM_KANJI_FCLOOKUP:
         do_key (VK_4,true,false);
         break;
    case IDM_KANJI_READINGLOOKUP:
         do_key (VK_R,true,true);
         break;
    case IDM_KANJI_INDEXLOOKUP:
         do_key (VK_I,true,true);
         break;
    case IDM_KANJI_GETINFO:
         do_key (VK_I,true,false);
         break;
    case IDM_KANJI_JISTABLE:
         do_key (VK_T,true,false);
         break;
    case IDM_KANJI_COUNTKANJI:
         do_kanjicount ();
         break;
    case IDM_KANJI_MAKEKANJILIST:
         color_kanji.clear ();
    case IDM_KANJI_APPENDKANJILIST: // *** FALL THROUGH ***
         do_kanjilist ();
         break;
    case IDM_KANJI_OPENKANJILIST:
         jwp_conv.clear ();
         new JWP_file (NULL,FILETYPE_UNNAMED);
         color_kanji.put ();
         jwp_file->sysname (IDS_CK_VIEWNAME);
         break;
    case IDM_KANJI_ADDSUBKANJILIST:
         color_kanji.do_adddel ();
         break;
    case IDM_KANJI_CLEARKANJILIST:
         color_kanji.clear ();
         color_kanji.write ();
         activate ();
         break;
// --------------------------------------------- Utilities menu
    case IDM_UTILITIES_FORMATFILE:
         jwp_config.global_effect = true;
    case IDM_UTILITIES_FORMATPARAGRAPH:
         JDialogBox (IDD_FORMATPARA,main_window,(DLGPROC) dialog_formatpara);
         jwp_config.global_effect = false;
         break;
    case IDM_UTILITIES_PAGELAYOUT:
         page_setup ();
         break;
    case IDM_UTILITIES_DICTIONARY:
         do_key (VK_D,true,false);
         break;
#if 0
case IDM_UTILTIIES_VOCABLIST:
break;
#endif
    case IDM_UTILITIES_USERCONVERSION:
         if (jwp_conv.dialog) SetForegroundWindow (jwp_conv.dialog);
           else JCreateDialog (IDD_CONVERTUSER,main_window,(DLGPROC) dialog_userconv);
         break;
    case IDM_UTILITIES_OPTIONS:
         do_options ();
         break;
    case IDM_UTILITIES_CHARINFO:
         cfg = &jwp_config.cfg;
         info_config (main_window);
         break;
    case IDM_UTILITIES_DEFAULTOPTIONS:
         jwp_config.set (&default_config);
         break;
    case IDM_UTILITIES_SAVESETTINGS:
         jwp_config.write ();
         break;
    case IDM_UTILITIES_INSTALL:
         do_install (true);
         break;
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
    case IDM_UTILITIES_TOGGLE:              // False menu event.  This is generated by the toggle button
         do_commandbar ();                  //   on the PPC's button bar.  This casues the menu bar and 
         break;                             //   button bar to toggle.
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
#ifndef WINCE
    case IDM_UTILITIES_CUSTOMIZE:           // Customize the toolbar
         jwp_tool.customize ();
         break;
#endif WINCE
// --------------------------------------------- Window menu
    case IDM_WINDOW_NEXTFILE:
         next->activate ();
         break;
    case IDM_WINDOW_PREVIOUSFILE:
         prev->activate ();
         break;
    case IDM_WINDOW_FILES:
         do_files (0);
         break;
// --------------------------------------------- Help menu
    case IDM_HELP_MAININDEX:
         do_help (main_window,0);
         break;
    case IDM_HELP_ABOUTJWPCE: {
           TCHAR text[SIZE_BUFFER],title[SIZE_BUFFER];
           format_string (text ,IDS_ABOUT_TEXT ,VERSION_STRING);
           format_string (title,IDS_ABOUT_TITLE,VERSION_STRING);
           MessageBox (main_window,text,title,MB_OK);
         }
         break;
// --------------------------------------------- Recent Files/Open Files menus
    default:
         i = LOWORD(wParam);
         if ((i >= IDM_WINDOW_FILES_BASE+1) && (i <= IDM_WINDOW_FILES_BASE+9)) do_files  (i-IDM_WINDOW_FILES_BASE+1);
         if ((i >= IDM_FILE_FILES_BASE)     && (i <= IDM_FILE_FILES_BASE  +9)) do_recent (i);
         break;
  }
  return;
}

//--------------------------------
//
//  Windows CE HPC toolbar routine.  This rotuine sets up the command bar for HPCs.
//
#ifdef WINCE_HPC

#define NUMBER_TIPS 14          // Number of added buttons.  I have added a spare one just in case.

void do_commandbar () {
  static TBBUTTON buttons[] = {
    { 0           ,0                       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_SEP     ,0,-1},
    { STD_FILENEW ,IDM_FILE_NEW            ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { STD_FILEOPEN,IDM_FILE_OPEN           ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { STD_FILESAVE,IDM_FILE_SAVE           ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},

    { 0           ,0                       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_SEP     ,0,-1},
    { STD_CUT     ,IDM_EDIT_CUT            ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { STD_COPY    ,IDM_EDIT_COPY           ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { STD_PASTE   ,IDM_EDIT_PASTE          ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { 15          ,IDM_KANJI_CONVERT       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { STD_FIND    ,IDM_EDIT_SEARCH         ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { STD_UNDO    ,IDM_EDIT_UNDO           ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},

    { 0           ,0                       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_SEP     ,0,-1},
    { 16          ,IDM_KANJI_GETINFO       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1}, // info
    { 17          ,IDM_UTILITIES_DICTIONARY,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1}, // dictionary
    { 18          ,IDM_KANJI_RADICALLOOKUP ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1}, // radical lookup
    { 19          ,IDM_KANJI_SKIPLOOKUP    ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1}, // SKIP lookup
  };

  static tchar *tips[NUMBER_TIPS+1] = { TEXT(""),NULL };        // Tool tips array, filled out at startup (CE requires static).
//
//  Setup the tool tips.
//
  TCHAR *ptr;
  int    i = 1;                                                         // Set this here because we use it below
  ptr = get_string(IDS_TOOL_TIPS);                                      // Get tool tips coded data.
  if (tip_data = (TCHAR *) malloc(sizeof(TCHAR)*(1+lstrlen(ptr)))) {    // Allocate storage.  If not possible we just forget the tips.
    lstrcpy (tip_data,ptr);                                             // Copy tips to the data array.
    for (ptr = tip_data; i <= NUMBER_TIPS; i++) {                       // Move through array.
      tips[i] = ptr;                                                    // Save pointer
      while (*ptr && (*ptr != '\t')) ptr++;                             // Find next tab (marks end of tip)
      if (!*ptr) break;                                                 // Off the end of the 
      *ptr = 0;                                                         // Change tab to end of string
      ptr++;                                                            // On to next tip.
    }
  }
//
//  Build command bar.
//
  command_bar = CommandBar_Create(instance,main_window,1);
  CommandBar_InsertMenubar (command_bar,language,IDM_MAINMENU,0);
  CommandBar_AddBitmap     (command_bar,HINST_COMMCTRL,IDB_STD_SMALL_COLOR,15,0,0);
  CommandBar_AddBitmap     (command_bar,instance,IDB_HPCTOOLS,5,0,0);
  CommandBar_AddButtons    (command_bar,sizeof(buttons)/sizeof(TBBUTTON),buttons);
  CommandBar_AddToolTips   (command_bar,i+1,tips);
  CommandBar_AddAdornments (command_bar,CMDBAR_HELP,0);
  hmenu = CommandBar_GetMenu(command_bar,0);
  jwp_config.commandbar_height = CommandBar_Height(command_bar);
}

#endif WINCE_HPC


//--------------------------------
//
//  Windows CE PPC toolbar routine.  This rotuine sets up the command bar for PPCs.
//
#ifdef WINCE_PPC

#define NUMBER_TIPS 8           // Number of added buttons.  I have added a spare one just in case.

void do_commandbar () {
  static short is_command = false;          // Indicates if we are showing the command bar or the tool bar
  static short tip_count  = 1;              // Needs to be static because we need this after the tips are setup.
  static TBBUTTON buttons[] = {
    { 15          ,IDM_UTILITIES_TOGGLE    ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { 0           ,0                       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_SEP     ,0,-1},
    { STD_FILESAVE,IDM_FILE_SAVE           ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { 0           ,0                       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_SEP     ,0,-1},
    { 16          ,IDM_KANJI_CONVERT       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { STD_UNDO    ,IDM_EDIT_UNDO           ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
    { 0           ,0                       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_SEP     ,0,-1},
    { 17          ,IDM_KANJI_GETINFO       ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1}, // info
    { 18          ,IDM_UTILITIES_DICTIONARY,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1}, // dictionary
    { 19          ,IDM_KANJI_RADICALLOOKUP ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1}, // radical lookup
  };
  static TBBUTTON xbuttons[] = {
    { 0           ,IDM_UTILITIES_TOGGLE    ,TBSTATE_ENABLED,TBSTYLE_BUTTON | TBSTYLE_AUTOSIZE,0,-1},
  };

  static tchar *tips[NUMBER_TIPS+1] = { TEXT(""),NULL };        // Tool tips array, filled out at startup (CE requires static).
//
//  Setup the tool tips.
//
  if (!tip_data) {
    TCHAR *ptr;
    ptr = get_string(IDS_TOOL_TIPS);                                // Get tool tips coded data.
    if (tip_data = (TCHAR *) malloc(sizeof(TCHAR)*lstrlen(ptr))) {  // Allocate storage.  If not possible we just forget the tips.
      lstrcpy (tip_data,ptr);                                       // Copy tips to the data array.
      for (ptr = tip_data; tip_count <= NUMBER_TIPS; tip_count++) { // Move through array.
        tips[tip_count] = ptr;                                      // Save pointer
        while (*ptr && (*ptr != '\t')) ptr++;                       // Find next tab (marks end of tip)
        if (!*ptr) break;                                           // Off the end of the 
        *ptr = 0;                                                   // Change tab to end of string
        ptr++;                                                      // On to next tip.
      }
    }
  }
//
//  Build command bar.
//
  if (!command_bar) {
    command_bar = CommandBar_Create(instance,main_window,1);
    CommandBar_InsertMenubar  (command_bar,language,IDM_MAINMENU,0);
//    CommandBar_AddBitmap      (button_bar,HINST_COMMCTRL,IDB_STD_SMALL_COLOR,15,0,0);
    CommandBar_AddBitmap      (command_bar,instance,IDB_PPCTOOLS,1,0,0);
    CommandBar_AddButtons     (command_bar,1,xbuttons);
    CommandBar_AddToolTips    (command_bar,2,tips);
    CommandBar_AddAdornments  (command_bar,CMDBAR_HELP,0);
    hmenu = CommandBar_GetMenu(command_bar,0);
  }
  if (!button_bar) {
    button_bar = CommandBar_Create(instance,main_window,2);
    CommandBar_AddBitmap     (button_bar,HINST_COMMCTRL,IDB_STD_SMALL_COLOR,15,0,0);
    CommandBar_AddBitmap     (button_bar,instance,IDB_PPCTOOLS,5,0,0);
    CommandBar_AddButtons    (button_bar,sizeof(buttons)/sizeof(TBBUTTON),buttons);
    CommandBar_AddToolTips   (button_bar,tip_count-1,&tips[1]);
    CommandBar_AddAdornments (button_bar,CMDBAR_HELP,0);
  }
  is_command = !is_command;
  CommandBar_Show (command_bar, is_command);
  CommandBar_Show (button_bar ,!is_command);
  jwp_config.commandbar_height = CommandBar_Height(command_bar);
  return;
}

#endif WINCE_PPC

//--------------------------------
//
//  Windows CE PocketPC toolbar routine.  This rotuine sets up the command bar for PPCs.
//
#ifdef WINCE_POCKETPC

void do_commandbar () {
  static short is_command = false;
  static short created    = false;
  if (!created) {
    SHMENUBARINFO mbi;
    memset (&mbi,0,sizeof(mbi));
    mbi.cbSize     = sizeof(mbi);
    mbi.hwndParent = main_window;
    mbi.hInstRes   = language;
    mbi.nToolBarId = IDR_MAINMENU;
    if (SHCreateMenuBar(&mbi)) menu_bar = mbi.hwndMB;
    hmenu = (HMENU) SendMessage (menu_bar,SHCMBM_GETMENU,0,0);
    CommandBar_AddBitmap (menu_bar,HINST_COMMCTRL,IDB_STD_SMALL_COLOR,15,0,0);
    CommandBar_AddBitmap (menu_bar,instance,IDB_PPCTOOLS,5,0,0);
    memset (&mbi,0,sizeof(mbi));
    mbi.cbSize     = sizeof(mbi);
    mbi.hwndParent = main_window;
    mbi.hInstRes   = language;
    mbi.nToolBarId = IDR_BUTTONBAR;
    if (SHCreateMenuBar(&mbi)) button_bar = mbi.hwndMB;
    hmenu2 = (HMENU) SendMessage (button_bar,SHCMBM_GETMENU,0,0);
    CommandBar_AddBitmap (button_bar,HINST_COMMCTRL,IDB_STD_SMALL_COLOR,15,0,0);
    CommandBar_AddBitmap (button_bar,instance,IDB_PPCTOOLS,5,0,0);
    created = true;

int possibly_move_this_back_over_by_CreateWindow;
  }
  is_command = !is_command;
  CommandBar_Show (menu_bar  , is_command);
  CommandBar_Show (button_bar,!is_command);
  return;
}

#endif WINCE_POCKETPC



