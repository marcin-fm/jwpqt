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
//  This modlue manages the status bar.  
//

#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_conv.h"
#include "jwp_file.h"
#include "jwp_font.h"
#include "jwp_stat.h"

#include <commctrl.h> 

//===================================================================
//
//  class JWP_stat.
//
//  This class manages the status bar.
//

class JWP_stat jwp_stat;                        // Class instance.

static LRESULT CALLBACK JWP_stat_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);

//--------------------------------
//
//  Window procedure.
//
static LRESULT CALLBACK JWP_stat_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;

  switch (iMsg) {
    case WM_CREATE:
         jwp_stat.adjust (hwnd);
         return (0);
    case WM_PAINT:
         hdc = BeginPaint (hwnd,&ps);
         jwp_stat.draw(hdc);
         EndPaint (hwnd,&ps);
         return (0);
    case WM_LBUTTONDOWN:
         jwp_stat.do_mouse (lParam);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//--------------------------------
//
//  Adjust the size of the window (is a responce to a size change).
//
//      hwnd -- this parameter is NULL in all cases, except when the
//              the window is first open.  In that case we need our
//              window passed in to us.
//
void JWP_stat::adjust (HWND hwnd) {
  RECT   rect;
  HDC    hdc;
  SIZE   size;
  tchar *ptr;
  int  i;
  if (!hwnd) hwnd = window;                     // No window pointer use the window var.
  if (!hwnd) return;                            // No window still -> no status bar.
  height  = (3*sysfont_height)/2;               // Setup window size and move it.
  GetClientRect (main_window,&rect);
  MoveWindow    (hwnd,-1,rect.bottom-height+1,rect.right+2,height,true);
//
//  Setup internal size parameters in the window.
//
  offset   = (1*sysfont_height)/5;              // Vertical offset for text.
  hdc      = GetDC (hwnd);
  ptr      = get_string (IDS_STAT_OVR);
  GetTextExtentPoint32 (hdc,ptr,lstrlen(ptr),&size);
  i        = size.cx;
  x_insert = (short) (rect.right-i);            // Location for INS/OVR
  GetTextExtentPoint32 (hdc,ptr,lstrlen(ptr)-1,&size);
  x_imouse = (short) (x_insert+size.cx);
  ptr      = get_string (IDS_STAT_JASCII);
  GetTextExtentPoint32 (hdc,ptr,lstrlen(ptr),&size);
  x_mode   = (short) (x_insert-size.cx-i/2);    // Location for mode 
  x_mmouse = (short) (x_mode+size.cx);
  x_text   = (short) (i/3);                     // Location for text message
  x_type   = (short) (x_mode-i/2);              // Location for file type (right side, not left).
  ReleaseDC (hwnd,hdc);
  return;
}

//--------------------------------
//
//  Render the status bar.  This routine is not called directly, but
//  rather is called from the window procedure.
//
//      hdc -- Display context.
//
void JWP_stat::draw (HDC hdc) {
  int    i;
  tchar *ptr;
  SIZE   size;
  static short modes[] = { IDS_STAT_KANJI,IDS_STAT_ASCII,IDS_STAT_JASCII };
  static short types[] = { IDS_STAT_UNTYPED,0,0,IDS_STAT_TYPEASCII,IDS_STAT_TYPEJWPCE,IDS_STAT_TYPEJWP,IDS_STAT_TYPEEUC,IDS_STAT_TYPESJIS,IDS_STAT_TYPENEW,IDS_STAT_TYPEOLD,IDS_STAT_TYPENEC,IDS_STAT_TYPEUNICODE,IDS_STAT_TYPEUTF7,IDS_STAT_TYPEUTF8,IDS_STAT_TYPEJFC };
 
  SetBkMode (hdc,TRANSPARENT);
  TextOut   (hdc,x_insert,offset,get_string(jwp_config.insert ? IDS_STAT_INS : IDS_STAT_OVR),3);
  TextOut   (hdc,x_mode  ,offset,get_string(modes[jwp_config.mode]),5);
#ifdef WINCE
  TCHAR buffer2[40];
  for (i = 0; buffer[i]; i++) buffer2[i] = buffer[i];
  TextOut   (hdc,x_text  ,offset,buffer2,i);
#else
  TextOut   (hdc,x_text  ,offset,buffer,lstrlen(buffer));
#endif WINCE
  if (jwp_file) {
    ptr = get_string(types[jwp_file->filetype]);
    i   = lstrlen(ptr);
    GetTextExtentPoint32 (hdc,ptr,i,&size);
    TextOut (hdc,x_type-size.cx,offset,ptr,i);
  }
  return;
}

//--------------------------------
//
//  Not called directly, but rather called from the window proc, this 
//  this routine process the mouse events.  The only events that are 
//  supported is clicking on/near the status symobls, you can cycle 
//  through the INS/OVR and the input mode.
//
//      lParam -- Message, lParam (contains the mouse location).
//
void JWP_stat::do_mouse (LPARAM lParam) {
  int x,y;
  x = LOWORD(lParam);
  y = HIWORD(lParam);
  if ((y < height/6) || (y >= (5*height)/6)) return;
  if      ((x >= x_insert) && (x <= x_imouse)) jwp_file->do_key (VK_INSERT,false,false);
  else if ((x >= x_mode  ) && (x <= x_mmouse)) set_mode (MODE_CYCLE);
  return;
}

//--------------------------------
//
//  Intialize the class and setup.
//
//      wclass -- class tructure passed in with intialization parameters.
//                if this is NULL we do not need to register class.
//
int JWP_stat::initialize (WNDCLASS *wclass) {
  if (wclass) {                                 // Register class.
    wclass->style         = CS_HREDRAW | CS_VREDRAW;
    wclass->lpfnWndProc   = JWP_stat_proc;
    wclass->hbrBackground = (HBRUSH) (COLOR_BTNFACE+1);
    wclass->lpszClassName = TEXT("JWP-Stat");
    if (!RegisterClass(wclass)) return (true);
  }
  if (!jwp_config.cfg.status && window) {       // This is actually invoided it the user
    DestroyWindow (window);                     //   changes the configuration to delete
  }                                             //   an existing status bar.
  if (!jwp_config.cfg.status) {                 // No status bar.
    window  = null;
    height  = 1;                                // Needs to be 1 not zero to get correct math in adjust routines
    return (false);
  }
  if (window) return (true);                    // Alread have a window.
  if (!(window = CreateWindow(TEXT("JWP-Stat"),NULL,WS_CHILD | WS_VISIBLE | WS_BORDER,0,0,0,0,main_window,(HMENU) 3,instance,NULL))) return (true);
  ShowWindow (window,SW_SHOW);                  // Make window.
  return (false);
}

//--------------------------------
//
//  Redraw the screen.  This simply checks to see if there is a window
//  then ivalidates the window.
//
void JWP_stat::redraw () {
  if (window) InvalidateRect(window,NULL,true); 
  return;
}

//--------------------------------
//
//  Updage the text part of the display, which requires a redraw.
//
//      text -- Text to be part of the display
//
void JWP_stat::update (char *text) {
  strcpy (buffer,text); 
  redraw (); 
  return;
}

//
//  End class JWP_stat
//
//===================================================================

//===================================================================
//
//  Begin class JWP_tool.
//
//  This class processes the toolbar used in Windows (not CE). 
//
//  Key to understanding the toolbar is the strage of tool buttons.  Each
//  button is assigned a number (the value of zero is used for separators).
//  The buttons are stored in an array buttons[] in the cfg structre of the 
//  JWP_config class.  The field button_count indicates the number of 
//  buttons.
//
//  If the button_count is zero, the button array is initialize to the
//  default values.  This allows older systems to automatically pick up the 
//  default button bar.
//
//  Whenever the button bar is created, it is created by adding the buttons
//  from the buttons array.  Whenever it is removed, the buttons are read out
//  and stored back in the array.
//
//  The array can currently store 100 buttons (counting seperators).
//
//  The button numbers are stored in the dwData field of the button structrue.
//  This is also used to access the tips for the button (addjusted by a 
//  value of 1).
//
#ifndef WINCE       // Toolbar is only used for Windows, Wincows CE uses a command bar.

#define NT_KANJI        15      // These are the bitmap offsets for my custom buttons.
#define NT_ASCII        16      //   They start at 15, because the standard button array   
#define NT_JASCII       17      //   is added first, this imports 15 buttons.
#define NT_CONVERT      18      //
#define NT_GETINFO      19      // PS I hate making icons!
#define NT_RADLOOKUP    20
#define NT_BUSHULOOKUP  21
#define NT_BSLOOKUP     22
#define NT_SKIPLOOKUP   23
#define NT_HSLOOKUP     24
#define NT_FCLOOKUP     25
#define NT_READLOOKUP   26
#define NT_INDEXLOOKUP  27
#define NT_JISTABLE     28
#define NT_COUNTKANJI   29
#define NT_COLORKANJI   30
#define NT_FORMATFILE   31
#define NT_PARAGRAPH    32
#define NT_PAGELAYOUT   33
#define NT_DICTIONARY   34
#define NT_KANCONVERT   35
#define NT_NEXT         36
#define NT_REVERSE      37

#define NT_TOTAL        23      // Total number of buttons in my custom bitmap

//--------------------------------
//
//  This is the default button list.
//
static byte button_list[] = {
  0,BUTTON_FILENEW,BUTTON_FILEOPEN,BUTTON_FILESAVE,
  0,BUTTON_FILEPRINT,
  0,BUTTON_CUT,BUTTON_COPY,BUTTON_PASTE,
  0,BUTTON_UNDO,BUTTON_REDO,
  0,BUTTON_SEARCH,BUTTON_REPLACE,BUTTON_NEXT,
  0,BUTTON_KANJI,BUTTON_ASCII,BUTTON_JASCII,BUTTON_CONVERT,
  0,BUTTON_GETINFO,BUTTON_JISTABLE,BUTTON_DICTIONARY,BUTTON_COUNTKANJI,
  0,BUTTON_RADLOOKUP,BUTTON_BUSHULOOKUP,BUTTON_BSLOOKUP,BUTTON_SKIPLOOKUP,BUTTON_HSLOOKUP,BUTTON_FCLOOKUP,BUTTON_READLOOKUP,BUTTON_INDEXLOOKUP,
  0,BUTTON_PAGELAYOUT,BUTTON_OPTIONS,
};

//--------------------------------
//
//  All of the user's possible buttons, arranged by order in the menus.  This is used for 
//  data, and this order is used in the Customize feature.
//
//  The auto-sizing of the buttons did not work on all systems.
//
static TBBUTTON buttons[] = {
  { 0             ,0                            ,TBSTATE_ENABLED,TBSTYLE_BUTTON   | TBSTYLE_SEP       ,0,0,BUTTON_SEPARATOR   ,-1},
  { STD_FILENEW   ,IDM_FILE_NEW                 ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_FILENEW     ,-1},
  { STD_FILEOPEN  ,IDM_FILE_OPEN                ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_FILEOPEN    ,-1},
  { STD_FILESAVE  ,IDM_FILE_SAVE                ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_FILESAVE    ,-1},
  { STD_DELETE    ,IDM_FILE_DELETE              ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_FILEDELETE  ,-1},
  { STD_PRINT     ,IDM_FILE_PRINT               ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_FILEPRINT   ,-1},

  { STD_UNDO      ,IDM_EDIT_UNDO                ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_UNDO        ,-1},
  { STD_REDOW     ,IDM_EDIT_REDO                ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_REDO        ,-1},
  { NT_KANJI      ,IDM_EDIT_MODE_KANJI          ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_KANJI       ,-1},
  { NT_ASCII      ,IDM_EDIT_MODE_ASCII          ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_ASCII       ,-1},
  { NT_JASCII     ,IDM_EDIT_MODE_JASCII         ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_JASCII      ,-1},
  { STD_CUT       ,IDM_EDIT_CUT                 ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_CUT         ,-1},
  { STD_COPY      ,IDM_EDIT_COPY                ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_COPY        ,-1},
  { STD_PASTE     ,IDM_EDIT_PASTE               ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_PASTE       ,-1},
  { STD_FIND      ,IDM_EDIT_SEARCH              ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_SEARCH      ,-1},
  { STD_REPLACE   ,IDM_EDIT_REPLACE             ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_REPLACE     ,-1},
  { NT_NEXT       ,IDM_EDIT_FINDNEXT            ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_NEXT        ,-1},
  { NT_REVERSE    ,IDM_EDIT_REVERSESEARCH       ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_REVERSE     ,-1},

  { NT_CONVERT    ,IDM_KANJI_CONVERT            ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_CONVERT     ,-1},
  { NT_GETINFO    ,IDM_KANJI_GETINFO            ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_GETINFO     ,-1},
  { NT_RADLOOKUP  ,IDM_KANJI_RADICALLOOKUP      ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_RADLOOKUP   ,-1},
  { NT_BUSHULOOKUP,IDM_KANJI_BUSHULOOKUP        ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_BUSHULOOKUP ,-1},
  { NT_BSLOOKUP   ,IDM_KANJI_BUSHU2LOOKUP       ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_BSLOOKUP    ,-1},
  { NT_SKIPLOOKUP ,IDM_KANJI_SKIPLOOKUP         ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_SKIPLOOKUP  ,-1},
  { NT_HSLOOKUP   ,IDM_KANJI_HSLOOKUP           ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_HSLOOKUP    ,-1},
  { NT_FCLOOKUP   ,IDM_KANJI_FCLOOKUP           ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_FCLOOKUP    ,-1},
  { NT_READLOOKUP ,IDM_KANJI_READINGLOOKUP      ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_READLOOKUP  ,-1},
  { NT_INDEXLOOKUP,IDM_KANJI_INDEXLOOKUP        ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_INDEXLOOKUP ,-1},
  { NT_JISTABLE   ,IDM_KANJI_JISTABLE           ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_JISTABLE    ,-1},
  { NT_COUNTKANJI ,IDM_KANJI_COUNTKANJI         ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_COUNTKANJI  ,-1},
  { NT_COLORKANJI ,IDM_KANJI_MAKEKANJILIST      ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_COLORKANJI  ,-1},

  { NT_FORMATFILE ,IDM_UTILITIES_FORMATFILE     ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_FORMATFILE  ,-1},
  { NT_PARAGRAPH  ,IDM_UTILITIES_FORMATPARAGRAPH,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_PARAGRAPH   ,-1},
  { NT_PAGELAYOUT ,IDM_UTILITIES_PAGELAYOUT     ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_PAGELAYOUT  ,-1},
  { NT_DICTIONARY ,IDM_UTILITIES_DICTIONARY     ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_DICTIONARY  ,-1},
  { NT_KANCONVERT ,IDM_UTILITIES_USERCONVERSION ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_KANJICONVERT,-1},
  { STD_PROPERTIES,IDM_UTILITIES_OPTIONS        ,TBSTATE_ENABLED,TBSTYLE_BUTTON /*| TBSTYLE_AUTOSIZE*/,0,0,BUTTON_OPTIONS     ,-1},
};
                            // Tool tips (button descriptiosn).  Note the addressing is     
static short tips[] = {     //   off by one, because there is no spot for the sepeartor.       
  IDS_TOOL_NEW,IDS_TOOL_OPEN,IDS_TOOL_SAVE,IDS_TOOL_DELETE,IDS_TOOL_PRINT,

  IDS_TOOL_UNDO  ,IDS_TOOL_REDO   ,
  IDS_TOOL_KANJI ,IDS_TOOL_ASCII  ,IDS_TOOL_JASCII,
  IDS_TOOL_CUT   ,IDS_TOOL_COPY   ,IDS_TOOL_PASTE ,
  IDS_TOOL_SEARCH,IDS_TOOL_REPLACE,IDS_TOOL_NEXT  ,IDS_TOOL_REVERSE,

  IDS_TOOL_CONVERT ,IDS_TOOL_INFO      ,
  IDS_TOOL_RADICAL ,IDS_TOOL_BUSHU     ,IDS_TOOL_BS       ,IDS_TOOL_SKIP,IDS_TOOL_HS,IDS_TOOL_FOURCORNER,IDS_TOOL_READING,IDS_TOOL_INDEX,
  IDS_TOOL_JISTABLE,IDS_TOOL_COUNTKANJI,IDS_TOOL_COLORKANJI,

  IDS_TOOL_FORMATFILE,IDS_TOOL_FORMATPARA,IDS_TOOL_LAYOUT,IDS_TOOL_DICTIONARY,IDS_TOOL_CONVERSIONS,IDS_TOOL_OPTIONS
};

class JWP_tool jwp_tool;        // The actual class instance    

//--------------------------------
//
//  Adjust the size of the button bar.  This is a responce to a WM_SIZE message from the 
//  system.  This should have been inline, but the TB_AUTOSIZE is not defined everywhere,
//  so this is cleaner.
//
void JWP_tool::adjust () {
  if (window) SendMessage (window,TB_AUTOSIZE,0,0);
  return;
}

//--------------------------------
//
//  Controls the check state of a button in the button bar.  Note indexing is via command
//  id, not button index.  This is because the user may move the buttons then we don't know
//  where they are.  This routine will find the button if it exists.
//
//      id    -- Command used for the button (IDM_???).
//      state -- true = checked (selected) and false = unchecked (not-selected).
//
//  Note the routine handles the case of the toolbar being or not being there, and the case
//  of the button being there or not being there.
//
void JWP_tool::check (int id,int state) {
  if (!window) return;
  SendMessage (window,TB_CHECKBUTTON,id,MAKELONG(state,0));
  return;
}

//--------------------------------
//
//  Responds to the Utilities/Cutomize... menu item.  This could have been inlined, but 
//  the defintiion of TB_CUSTOMIZE cuases problems.
//
void JWP_tool::customize () {
#ifndef WINELIB
  SendMessage (window,TB_CUSTOMIZE,0,0);
#endif WINELIB
  return;
}

//--------------------------------
//
//  Controls the activation state of a button in the button bar.  Note indexing is via command
//  id, not button index.  This is because the user may move the buttons then we don't know
//  where they are.  This routine will find the button if it exists.
//
//      id    -- Command used for the button (IDM_???).
//      state -- true = enabled and false = disabled.
//
//  Note the routine handles the case of the toolbar being or not being there, and the case
//  of the button being there or not being there.
//
void JWP_tool::enable (int id,int state) {
  if (!window) return;
  SendMessage (window,TB_ENABLEBUTTON,id,MAKELONG(state,0));
  return;
}

//--------------------------------
//
//  This handles the TB_GETINFO WM_NOTIFY message.  These are issed by the system to find
//  all the buttons that can be placed in the tool bar when the user is customizing the 
//  toolbar.  We keep responging true until we run out of buttons then we return a false.
//  We bassically feed the system the buttons in the buttons[] array above.
//
//      pnmh   -- Pointer ot notification message.
//
//      RETURN -- true indicates we added a button, false indicates the arry is done
//
int JWP_tool::info (NMHDR *pnmh) {
  TBNOTIFY *bn = (TBNOTIFY *) pnmh;
  if (bn->iItem+1 >= ((int) ((sizeof(buttons)/sizeof(TBBUTTON))))) return (false);
  LOAD_STRING (bn->pszText,tips[bn->iItem],bn->cchText);
//
//  This code will remove the shortcut keys from the tools, but actually I think it works 
//  out better with the short-cuts in there.
//
#if 0   
  TCHAR *ptr;
  for (ptr = bn->pszText; *ptr; ptr++) if (*ptr == '(') *ptr = 0; 
#endif
  memcpy     (&bn->tbButton,&buttons[bn->iItem+1],sizeof(TBBUTTON));
  return (true);
}

//--------------------------------
//
//  For the toolbar, this is the big one.  This both makes and destroys the toolbar.  This 
//  routine also save the current user settings when the tool bar is destoryed and recreates
//  them when the toolbar is generated.  
//
//      exit -- This flag indicates that we are exiting (or at least are sure we want to), 
//              and thus want to destroy the tool bar no mater what.
//
//  Note, this routine contains an automatically initialization for the toolbar.  If I ever
//  change the configuration file system, this should be removed as being redundent.
//  
void JWP_tool::process (int exit) {
  int         i;  
  RECT        rect;
  TBBUTTON    button;
  TBADDBITMAP custom_bitmaps;
//
//  This handles opeing the toolbar.
//
  if (jwp_config.cfg.toolbar && !window) {
    window               = CreateToolbarEx(main_window,WS_CHILD | WS_VISIBLE | CCS_TOP | CCS_ADJUSTABLE | TBSTYLE_TOOLTIPS,IDB_TOOLBAR,16,HINST_COMMCTRL,IDB_STD_SMALL_COLOR,NULL,0,24,22,16,16,sizeof(TBBUTTON));
    custom_bitmaps.hInst = instance;
    custom_bitmaps.nID   = IDB_TOOLBAR;
    SendMessage (window,TB_ADDBITMAP ,NT_TOTAL,(LPARAM) &custom_bitmaps);
    for (i = 0; i < jwp_config.cfg.button_count; i++) {                 // Add buttons to the toolbar
      SendMessage (window,TB_ADDBUTTONS,1,(LPARAM) &buttons[jwp_config.cfg.buttons[i]]); 
    }
    GetWindowRect (window,&rect);                                       // Get button size.
    jwp_config.commandbar_height = (short) (rect.bottom-rect.top+1);
    EnableMenuItem (hmenu,IDM_UTILITIES_CUSTOMIZE,MF_ENABLED);
    if (jwp_file) jwp_file->edit_menu ();
    set_mode (MODE_NOCHANGE);
  }
//
//  Whenever we close the toolbar, we save the button configuration.
//  The buttons are saved by saving their user data field which contains
//  the button number.  The value of zero is used as a separator.
//
  if ((exit || !jwp_config.cfg.toolbar) && window) {
    for (i = 0; i < 100; i++) {                                 // Allow a maximum of 100 buttons
      if (!SendMessage(window,TB_GETBUTTON,i,(LPARAM) &button)) break;
      jwp_config.cfg.buttons[i] = (byte) button.dwData;
    }
    jwp_config.cfg.button_count = i;                            // Store number of buttons
    DestroyWindow (window);
    jwp_config.commandbar_height = 0;
    window                       = null;
    EnableMenuItem (hmenu,IDM_UTILITIES_CUSTOMIZE,MF_GRAYED);
  }
  return;
}

//--------------------------------
//
//  This resets the buttons to the default button list.
//
void JWP_tool::reset () {
  int i,j;
  j = SendMessage(window,TB_BUTTONCOUNT,0,0);                           // Number of buttons
  for (i = 0; i < j; i++) SendMessage(window,TB_DELETEBUTTON,0,0);      // Remove all buttons
  for (i = 0; i < ((int) sizeof(button_list)); i++) {                           // Add buttons to the toolbar
    SendMessage (window,TB_ADDBUTTONS,1,(LPARAM) &buttons[button_list[i]]); 
  }
  return;
}

//--------------------------------
//
//  This handles the TTN_NEEDTEXT WM_NOTIFY message.  This is basically a request from the
//  system to get the tooltip for the buttons.
//
//      pnmh   -- Pointer ot notification message.
//
//      RETURN -- true indicates we added a button, false indicates the arry is done
//
int JWP_tool::tip (NMHDR *pnmh) {
  int          i;
  TBBUTTON     button;
  TOOLTIPTEXT *tt = (TOOLTIPTEXT *) pnmh;
  i = SendMessage(window,TB_COMMANDTOINDEX,tt->hdr.idFrom,0);   // Convert from command ID to button index
  if (SendMessage(window,TB_GETBUTTON,i,(LPARAM) &button)) {    // Get button structure
    GET_STRING (tt->szText,tips[button.dwData-1]);             // Copy the text based on our button ID number
  }
  else {
    lstrcpy (tt->szText,TEXT("wakaranai!"));                    // What is this?
  }
  return (true);
}

#endif WINCE

