//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//  The radical lookup tables used were originally developed by      //
//  Michael Raine and Derc Yamasaki.                                 //
//                                                                   //
//===================================================================//

//===================================================================
//
//  This module implements the kanji search by radical and count dialog
//  box and all fucntions associated with this kanji lookup feature.
//  This modlue also implements the somewhat unrelated function of 
//  JIS code table (see below).
//
//  The kanji lookup is based on indexed radical and count lists.  An 
//  entry is searched by first finding the entry in the index file.  
//  These are basically a sequecial list of structures.  These structures
//  have two fields.  The first entry is the offset into the data file.
//  The second entry is the number of kanji for the entry.  These can 
//  then be read from the data file.
//
//  The selection is done as follows.  The first radical is read in
//  and forms the basis of kanji to be accepted.  
//
//  As each new radical is read in as another list.  All kanji in the 
//  master list that are not in the new radical list are removed 
//  (replaced by 0).  This keeps only all the kanji that are on all
//  lists.
//
//=== begin obsolete ===============================================
//  The elimination by kanji count is performed differently because 
//  the user could select kanji with 2-30 strokes (we limit the stoke 
//  count to 30.  For the counts, we read in the count indexes one at
//  a time and set the high bit 0x8000 on all kanji that in the user
//  radical list that are in the kanji count list.  Then we can go 
//  through the kanji list and eliminate all kanji with the radical not 
//  set.
//=== end obsolete ================================================
//  
//  The previous methode for checking the stroke count makes no sense 
//  at all.  Instead, we search the stroke count, but using the kanji-
//  info data-base to lookup each character's stoke count and elminate
//  characters outside the correct count.
//
//  Note, in the case when the user selected just a stroke count, and 
//  not any radicals, we simply joint the returned kanji lists.
//
//  Size notes:
//
//  The largest count lists are arounte 750 each.  The largest radical
//  list, however, is 1300 (small kuchi).  Since I use the kanji list
//  as a buffer for doing some compares, this puts a limit on the size 
//  of the buffer (must be more than 1300 characters).  This in turn puts 
//  a limit on the size of the scratch buffer used in the search routine.
//
#include "jwpce.h"
#include "jwp_clip.h"
#include "jwp_conf.h"
#include "jwp_edit.h"
#include "jwp_file.h"
#include "jwp_font.h"
#include "jwp_help.h"
#include "jwp_info.h"
#include "jwp_inpt.h"
#include "jwp_klst.h"
#include "jwp_lkup.h"
#include "jwp_misc.h"
#include <commctrl.h>
#include <limits.h>

static LRESULT CALLBACK JWP_jistable_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);

#ifdef WINELIB
  #include <updown.h>
#endif WINELIB

    //===================================================================
//
//  Compile time options.
//

//#define USE_BLOCK_CURSOR    // If defined, cuases the system to use a full
                            //   block cursor for the radical cursor.  If this
                            //   is not defined, an outline cursor defined by
                            //   a bitmap will be used.  This is an outline one
                            //   bit wide.
#ifdef WINCE
#define USE_BLOCK_CURSOR
#endif WINCE

//#define USE_SCROLL_RADICALS // If defined, causes system to restrict the radicals
                            //   window to being smaller than the size used in 
                            //   the dialog box template.  The alternative is a 
                            //   window that expands to be big enough to display
                            //   all of the radicals.
#ifdef WINCE
#define USE_SCROLL_RADICALS
#endif WINCE

#ifdef WINCE                                    // Use different colors for windows CE to get better visibility.
#define COLOR_SELRAD        RGB(200,200,0)      // Color of selected radical
#else  WINCE
#define COLOR_SELRAD        RGB(255,255,0)      // Color of selected radical
#endif WINCE

#define NAME_RADRADICAL     TEXT("radical.dat") // Radical information by radical
#define NAME_RADSTROKE      TEXT("stroke.dat")  // Radical inforamtion by stroke

#define CONTROL_MOVE        5                   // Number of cuttons control+cursor key moves.

                                                // Sizes of button objects in pixals (all are square).
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  #define BUTTON_SIZE       20                  // Size of a button
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  #define BUTTON_SIZE       22                  // Size of a button
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
#define BUTTON_JBMSIZE      16                  // Size of a JIS table character.
#define BUTTON_RBMSIZE      16                  // Size of a radical bitmap
#define BUTTON_SBMSIZE      16                  // Size of a stroke count bitmap.

#define MAX_STROKES 30                          // Maximum number of stokes

#define BUTTON_JBMOFFSET    ((BUTTON_SIZE-BUTTON_JBMSIZE)/2)
#define BUTTON_RBMOFFSET    ((BUTTON_SIZE-BUTTON_RBMSIZE)/2)
#define BUTTON_SBMOFFSET    ((BUTTON_SIZE-BUTTON_SBMSIZE)/2)
                                                
#define MIN_KANJI1  0x3000                      // This specify the range for the kanji in 
#define MAX_KANJI1  0x7400                      //   the kanji-information file.  This is used
#define MIN_KANJI2  0x0021                      //   by a number of kanji lookup routines.
#define MAX_KANJI2  0x007e

//===================================================================
//
//  Class KANJI_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class KANJI_lookup definition.
//
//  This class defines a general kanji lookup class.  This class will manage a kanji 
//  search dialg, and process many of the standard buttons, and actions for the 
//  dialog.  This class is not intended to be used directly, but to be used as a 
//  derived class.
//

typedef class KANJI_lookup : public KANJI_list {
public:
  KANJI_lookup  (void);
  void auto_check      (void);                              // Check for auto processing of the search
  void clear_results   (void);                              // Clear results and set results flags.
  int  command         (WPARAM wParam);                     // Process the standard dialog box commands.    
  int  get_value       (int id,int limit,int &v1,int &v2);  // Get a numerical value from a control.
  void initialize      (HWND hwnd,int allow_auto,int id);   // Intialize the dialog box and class
  int  radlist_winproc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);   // Window proc for handling the radicals list.
  void set_value       (int id,int value);                  // Set a value in a control.

  short     adjusting;                                      // Suppress auto-search when adjusting parameters.
  short     auto_search;                                    // Set true to enable the auto-search
  HWND      dialog;                                         // Pointer to our own dialog box.
  JWP_file *exclude;                                        // Exclude insert to file (used for Reading list).
private:
  int     clear_id;                                         // Id to receive the focus after clearing.
  void         do_search   (int activate);                  // Do the actual search.
  void virtual reset       (void) = 0;                      // Reset the search engine.
  void virtual search      (void) = 0;                      // Execute the actual search.
} KANJI_lookup;

//-------------------------------------------------------------------
//
//  Static data and structures
//

static KANJI_lookup *temp_link = NULL;      // This is a KLUDGE, but I could not think of another
                                            // way to do this without lots of problems.  When a 
                                            // KANJI_lookup object is created, it stores the addess
                                            // of the object at this location.  When the window
                                            // procedure associated with the dialog box starts up
                                            // this pointer is stored in the window pointer to 
                                            // allow the system to access the global structure.  
                                            // 
                                            // This works, but makes the assumtion that you create
                                            // the dialog right after createing the object.

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Window procedure stub for the results list on the Radical Lookup 
//  dialog box.
//
static LRESULT CALLBACK JWP_radlookup_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  KANJI_lookup *lookup;
  if (iMsg == WM_CREATE) SetWindowLong (hwnd,0,(long) temp_link);
  lookup = (class KANJI_lookup *) GetWindowLong(hwnd,0);
  return (lookup->radlist_winproc(hwnd,iMsg,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  Class routines.
//

//--------------------------------
//
//  Constructor.
//
KANJI_lookup::KANJI_lookup (void) : KANJI_list (3000) {
  auto_search = false;      // At minimum suppres auto-search during startup.
  adjusting   = false;      // No we are not adjusting at the moment.
  temp_link   = this;       // This is so the dialog box handler can reach us.
  exclude     = NULL;       // Normally exclude is not used.
  return;
}

//--------------------------------
//
//  This routine is called when the user selects any button that can 
//  effect the result of the search.  If auto-search is active, then 
//  a new search is done.  Otherwise the list is simply cleared.
//
void KANJI_lookup::auto_check () {
  if (adjusting) return;
  if (auto_search) do_search (false); else clear_results ();
  return;
} 

//--------------------------------
//
//  Clear all results parts of the dialog box.
//
void KANJI_lookup::clear_results () {
  EnableWindow (GetDlgItem(dialog,IDC_RLINSERT),false);
  EnableWindow (GetDlgItem(dialog,IDC_RLINFO)  ,false);
  EnableWindow (GetDlgItem(dialog,IDC_RLCOPY)  ,false);
  SetDlgItemText (dialog,IDC_RLMESSAGE,TEXT(""));
  clear ();
  return;
}

//--------------------------------
//
//  Process standard commands from the dialog box.  These include:
//
//      Search
//      Clear
//      Get Info
//      Insert->File
//      Copy->Clipboard
//      Done
//      Auto
//
//      wParam -- contains the command to be processed.
//
//      RETURN -- Return is set to true if the command is processed and false if not.
//
int KANJI_lookup::command (WPARAM wParam) {
  switch (LOWORD(wParam)) {
#ifndef WINCE_POCKETPC
    case IDOK:           
         do_search (true);
         return    (true);
#else   WINCE_POCKETPC
    case IDSEARCH:
         do_search (true);
         return    (true);
    case IDOK:
#endif  WINCE_POCKETPC
    case IDCANCEL:                       // Done button, so exit.
         DestroyWindow (dialog);
         return (true);
    case IDC_RLCLEAR:                    // Clear all buttons and list.
         reset    ();
         SetFocus (GetDlgItem(dialog,clear_id));
         return   (true);
    case IDC_RLINFO:                     // Get info for character.
         get_info (dialog);
         return   (true);;
    case IDC_RLCOPY: {                   // Copy to clipboard
           JWP_file *file = NULL;
           int       i;
           if ((file = file->clip_copy())) {
             if (GetKeyState(VK_SHIFT) >= 0) file->put_string (&list[selected],1);
               else {
                 for (i = 0; i < list_len; i += 2) file->put_string(&list[i],1);
               }
           }
         }
         return (true);
    case IDC_RLINSERT:                    // Insert back into file.
         JWP_file *file; 
         file = file_list.get(exclude);
         file->insert_string (&list[selected],1);   // Allows undo of this specific operation.
         return (true);
    case IDC_RLAUTO:                     // Change auto-search state.
         auto_search = IsDlgButtonChecked(dialog,IDC_RLAUTO);
         jwp_config.cfg.auto_lookup = (byte) auto_search;
         break;;
    default:
         break;
  }
  auto_check ();
  return (false);
}

//--------------------------------
//
//  This routine actually does the setup, then the search, then the cleanup.
//
//      activate -- If set, this will cause the results window to be
//                  activated after the search.
//  
void KANJI_lookup::do_search (int activate) {
//
//  Do the setup.
//
  clear_results ();
//
//  Do the search
//
  search ();
//
//  Do the cleanup.
//
  TCHAR buffer[20];
  if (!list_len ) {                         // Found nothing.
    MessageBeep (MB_ICONASTERISK);
    GET_STRING  (buffer,IDS_RL_NONE);
  }
  else {                                    // have results
    EnableWindow (GetDlgItem(dialog,IDC_RLINSERT),true);
    EnableWindow (GetDlgItem(dialog,IDC_RLINFO)  ,true);
    EnableWindow (GetDlgItem(dialog,IDC_RLCOPY)  ,true);
    select   (0);
    if (activate) SetFocus (GetDlgItem(dialog,IDC_RLRESULT));
    if (list_len < size-1) wsprintf (buffer,TEXT("%d"),(list_len+1)/2);
      else {
        MessageBeep   (MB_ICONASTERISK);    // Have too many results!
        format_string (buffer,IDS_RL_TOOMANY,(list_len+1)/2);
      }
  }
  SetDlgItemText (dialog,IDC_RLMESSAGE,buffer);
  redraw ();
  return;
}

//--------------------------------
//
//  Gets a value from a control.  The special case of a blank or non-numerical value is 
//  automatically mapped to a full range.  The value is also clipped around the exclusion 
//  values.
//
//      id     -- ID to read from.
//      limit  -- Maximum value allowed
//      v1     -- Min range returned (>= 0)
//      v2     -- Max range returned (<= limit)
//
//      RETURN -- value returned in v1 (user value).
//
int KANJI_lookup::get_value (int id,int limit,int &v1,int &v2) {
  int i;
  i = GetDlgItemInt(dialog,id,NULL,false);
  if (i < 0    ) i = 0; 
  if (i > limit) i = limit;
//  set_value (id,i);           // This causes a bad interaction with editing, because the 
  v1 = i;                       //   line you are editing is changing, so I disabled this one.
  v2 = i ? i : limit;
  return (i);
}

//--------------------------------
//
//  Intialize the class structures.
//
//      hwnd       -- Pointer to the dialog window.
//      allow_auto -- Indicates if auto-searching is supported.
//      id         -- ID of control to be activated
//
void KANJI_lookup::initialize (HWND hwnd,int allow_auto,int id) {
  dialog   = hwnd;
  clear_id = id;
  reset ();                                         // Need this before auto is enabled or will fail first time.
  if (allow_auto) {                                 // Take care of the auto-search.
    auto_search = jwp_config.cfg.auto_lookup;
    CheckDlgButton (hwnd,IDC_RLAUTO,auto_search);
  }
  add_dialog (hwnd,true);
  SetFocus   (GetDlgItem(hwnd,id));
  return;
}

//--------------------------------
//
//  This is the window procedure for the results list dialog box.  For
//  the most part, this procedure calls routines from the base class 
//  KANJI_list, to perform operations on the window.
//
int KANJI_lookup::radlist_winproc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;
  int         i;

  switch (iMsg) {
    case WM_CREATE:                 // Create window and adjust size.
         CREATESTRUCT *create;
         create = (CREATESTRUCT *) lParam;
         height = bar_font.height+2*bar_font.vspace+GetSystemMetrics(SM_CYHSCROLL);
         MoveWindow (hwnd,create->x,create->y,create->cx,height+2*WIN_YEDGE,true);
         adjust (window = hwnd);
         ImmAssociateContext (hwnd,NULL);       // Disable the IME for this window
         return (0);
    case WM_GETDLGCODE:             // Want cursor keys for shifting the selected kanji and space for select
         return (DLGC_WANTARROWS | DLGC_WANTCHARS);
    case WM_PAINT:                  // Render
         hdc = BeginPaint (hwnd,&ps);
         SetTextColor     (hdc,GetSysColor(COLOR_WINDOWTEXT));
         SetBkColor       (hdc,GetSysColor(COLOR_WINDOW));
         draw             (hdc);
         EndPaint         (hwnd,&ps);
         return           (0);
    case WM_HSCROLL:                // Process scroll messages.
         do_scroll (wParam);
         return    (0);
#ifndef WINCE
    case WM_MOUSEWHEEL:
         do_wheel (wParam);
         return   (0);
#endif  WINCE
    case WM_KEYDOWN:                // Process keyboard events.
         i = (GetKeyState(VK_CONTROL) < 0) ? CONTROL_MOVE*2 : 2;
         switch (wParam) {
           case VK_GT:
           case VK_RIGHT:
           case VK_F2:
                selected += i;
                break;
           case VK_LT:
           case VK_F3:
           case VK_LEFT:
                selected -= i;
                break;
           case VK_HOME:
                selected = 0;
                break;
           case VK_END:
                selected = list_len;
                break;
           case VK_I:
                get_info (hwnd);
                break;
           case VK_C:
                if (list_len) SendMessage (dialog,WM_COMMAND,IDC_RLCOPY,0);
                break;
           case VK_F23:
                get_info (hwnd);
                return (1);
           default:
                return (0);
         }
         if (selected < 0         ) selected = 0;
         if (selected > list_len-2) selected = list_len-2;
         select (selected);
    case WM_SETFOCUS:   // *** FALL THROUGH ***
SetFocus:
         CreateCaret (hwnd,null,sel.right-sel.left,1);
         SetCaretPos (sel.left,sel.bottom);
         ShowCaret   (hwnd);
         return (0);
    case WM_KILLFOCUS:              // Loosing focus.
         DestroyCaret ();
         return (0);
    case WM_LBUTTONDOWN:            // Mouse event handler
         if ((GetKeyState(VK_MENU) >= 0) && !(wParam & MK_SHIFT)) {     // alt+left -> right mouse button click
           do_mouse (lParam);
           SetFocus (hwnd);
           goto SetFocus;           // KLUDGE -- I dislike goto, but it is efficient
         }          
#ifndef WINCE
//
//  I should be able to combine these, but I have been unable to get that to 
//  work.
//
    case WM_RBUTTONDOWN:            // Get character information
         do_mouse (lParam);
         get_info (hwnd);
         return (1);
    case WM_CONTEXTMENU:            // Get character information
         if (lParam == ~0L) get_info (hwnd);
         return   (1);
#else WINCE
    case WM_RBUTTONDOWN:            // **** FALL THROUGH ****
         do_mouse (lParam);
         get_info (hwnd);
         return (1);
#endif WINCE
    case WM_CHAR:
         if (wParam != ' ') return (0);
    case WM_LBUTTONDBLCLK:          // Double click.
         if (list_len) SendMessage (dialog,WM_COMMAND,IDC_RLINSERT,0);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//--------------------------------
//
//  Sets the value in a control.  The value of zero is mapped to a blank value.
//
//      id    -- ID to write to.
//      value -- Value to write.  Zero is written as a blank.
//
void KANJI_lookup::set_value (int id,int value) {
  adjusting = true;
  if (!value) SetDlgItemText (dialog,id,TEXT("")); else SetDlgItemInt (dialog,id,value,false);
  adjusting = false;
  return;
}

//===================================================================
//
//  Class KANJIRAD_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class KANJIRAD_lookup definition.
//
//  This class is derifed from the base KANJI_lookup class, but adds the ability
//  to process a radicals input indow.  This allows the same processing routines 
//  to be used for the bushu and the radical lookup system.  This class cannot 
//  be used directly, but rather one must dirive a class from it.
//

#define BUTTON_TOTAL        258 // Total number of buttons (radical buttons).
#define BUTTON_COUNT        241 // Total number of radicals.

typedef class KANJIRAD_lookup : public KANJI_lookup {
public:
  KANJIRAD_lookup  (int count,int single);
  ~KANJIRAD_lookup ();
  void do_spinner       (NMUPDOWN *ud,int id);  // Do spiner control.
  int  radicals_winproc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
  void reset_radicals   (void);                 // Reset all radicals
  void set_radical      (int rad);              // Sets all buttons associated with a radical (multi-select only);
  byte    states[BUTTON_COUNT+1];               // State array for radical buttons.
private:
  void   redraw_radicals (void);                // Redraw the radicals selection window.
  void   set_cursor      (HWND hwnd);           // Set cursor in the radicals window.
  void   toggle_button   (void);                // Toggle button state in the radicals window.
  HBITMAP cursor_bitmap;                // Bitmap for caret in radicals window.
  HBITMAP radical_bitmap;               // Radicals bitmaps.
  HBITMAP stroke_bitmap;                // Storke count bitmaps.
  short   cursor;                       // Curosr lcoation in the radical window.
  short   button_x;                     // Number of buttons in the x direction.
  short   button_y;                     // Number of buttons in the y direction.
  short   button_v;                     // Number of vertical buttons in the display.
  short   top;                          // Top visible button array.
  short   last_char;                    // Last character for radicals window.
  short   single;                       // Set to non-zero to allow only a single radical to be selected.
} KANJIRAD_lookup;

//-------------------------------------------------------------------
//
//  Static data and structures
//

//--------------------------------
//
//  This table defines all the buttons in the radical button list.  The numbers have
//  the following meanings.  A value of zero is relplaced with a stroke count number.  
//  The count numbers simply increase linearly thorugh the table, starting with a value
//  of 1.  All other numbers indicate a radical, staring with the value of 1.  This means 
//  that you need to subtract 1 to index into the data file.
//
static byte buttons[BUTTON_TOTAL] = {
    0,  1,  2,  3,  4,  5,  6,  0,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 000..019
   19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,  0, 35, 36, 37,  // 020..039
   38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,  // 040..059
   58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,  0, 75, 76,  // 060..079
   77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96,  // 080..099
   97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,  // 100..119
  117,  0,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,  // 120..139
  136,137,138,139,140,141,142,  0,143,144,145,146,147,148,149,150,151,152,153,154,  // 140..159
  155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,  0,172,173,  // 160..179
  174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,  0,191,192,  // 180..199
  193,194,195,196,197,198,199,200,  0,201,202,203,204,205,206,207,208,209,210,211,  // 200..219
    0,212,213,214,215,216,217,218,219,220,  0,221,222,223,224,225,226,227,  0,228,  // 220..239
  229,230,231,  0,232,233,234,235,  0,236,237,  0,238,  0,239,240,  0,241,          // 240..257
};

//--------------------------------
//
//  This table gives the bushu number for the radical buttons.  Because there are 
//  only 214 bushu and there are 241 buttons, there are a large number of variations.
//  This table is arranged in the order of the radicals.  Note that bushu 23 and 35 
//  do not exist in the button set.  Bushu 23 is automatically mapped to 22 (primarally
//  23 is a classical bushu), 23 and 22 are hard to distinguish.  Bushu 35 is mapped to
//  34 automatically (35 is only used in classical counting).  
//
//  Because this is Japanese, some of the bushu are listed only in alterative form, and 
//  the original Chinese form is not shown, because it is not used).
//
//  The second 4-stroke button does not correspond to any bushu, and is mapped to bushu 255.
//  This will cause the search engines to fail on finding that radical.
//
//  Note that duplication of bushus is different that duplication used in the radical-lookup.
//  The radical lookup sepearates some variations that are not separated in the bushu 
//  listings.
//
static byte bushus[BUTTON_COUNT+1] = {
  0  ,                                                                                      // 0 stroke
  1  ,2  ,3  ,4  ,5  ,  6  ,                                                                // 1 stroke
  7  ,8  ,9  ,9  ,9  ,  10 ,11 ,12 ,13 ,14 ,  15 ,16 ,17 ,18 ,18 ,  19 ,20 ,21 ,22 ,24 ,    // 2 storke
                        25 ,26 ,27 ,28 ,29 ,  162,163,170,
  30 ,31 ,32 ,33 ,34 ,  36 ,37 ,38 ,39 ,40 ,  41 ,42 ,42 ,43 ,44 ,  45 ,46 ,47 ,47 ,48 ,    // 3 stroke
                        49 ,49 ,50 ,51 ,52 ,  53 ,54 ,55 ,56 ,57 ,  58 ,58 ,58 ,59 ,60 ,
                        61 ,64 ,85 ,94 ,140,
  61 ,255,62 ,63 ,64 ,  65 ,66 ,66 ,67 ,68 ,  69 ,70 ,71 ,71 ,72 ,  73 ,74 ,75 ,76 ,77 ,    // 4 stroke
                        78 ,79 ,80 ,80 ,81 ,  82 ,83 ,84 ,85 ,86 ,  86 ,87 ,88 ,89 ,90 ,
                        91 ,92 ,93 ,94 ,96 ,  113,125,130,
  95 ,96 ,97 ,98 ,99 ,  100,101,102,103,104,  105,106,107,108,109,  110,111,112,113,114,    // 5 stroke
                        115,116,117,122,145,
  118,119,120,121,122,  123,124,125,126,127,  128,129,130,131,132,  133,134,135,136,137,    // 6 stroke
                        138,139,140,141,142,  143,144,145,146,
  147,148,149,150,151,  152,153,154,155,156,  157,158,159,160,161,  163,164,165,166,        // 7 stroke
  167,168,169,170,171,  172,173,174,175,210,                                                // 8 stroke
  176,177,178,179,180,  181,182,183,184,185,  186,                                          // 9 stroke
  187,188,189,190,191,  192,193,194,212,                                                    // 10 stroke
  195,196,197,198,199,  200,213,                                                            // 11 stroke
  201,202,203,204,                                                                          // 12 stroke
  205,206,207,208,                                                                          // 13 stroke
  209,210,                                                                                  // 14 stroke
  211,                                                                                      // 15 stroke
  212,213,                                                                                  // 16 stroke
  214,                                                                                      // 17 stroke
};

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Window procedure stub for the radicals button window.
//  
static LRESULT CALLBACK JWP_radicals_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  KANJIRAD_lookup *lookup;
  if (iMsg == WM_CREATE) SetWindowLong (hwnd,0,(long) temp_link);
  lookup = (class KANJIRAD_lookup *) GetWindowLong(hwnd,0);
  return (lookup->radicals_winproc(hwnd,iMsg,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  Class routines.
//

//--------------------------------
//
//  This routine handles the spinner control for the count.  The idea is to improve
//  the sequenc of number used.  The default would be 0, 1, 2 // 29, 30, 0.
//  If radicals are selected with say 11 strokes, this routine will give the following
//  sequence:
//
//      0, 11, 12, .. 29, 30, 0
//
//  1-10 are skipped because they will result in no matches.
//
//      up -- Updown notification structrure.  This is passed as lParam.
//      id -- Id of the edit control.
//
void KANJIRAD_lookup::do_spinner (NMUPDOWN *ud,int id) {
  int i,j,k;
  for (k = j = i = 0; i <= BUTTON_TOTAL; i++) {         // Scann all buttons.
    if      (!buttons[i])        j++;                   // Maker so increment strokes.
    else if (states[buttons[i]]) k += j;                // Selected so change count.
  }
  i  = GetDlgItemInt (dialog,id,NULL,false);            // Get int value.
  i -= ud->iDelta;                                      // Increment.
  if (i < 0          ) i = MAX_STROKES;                 // Check bounds
  if (i > MAX_STROKES) i = 0;
  if (i && (i < k)) i = (ud->iDelta < 0) ? k : 0;       // Check for dead spot.
  SetDlgItemInt (dialog,id,i,false);                    // Put int value.
  return;
}

//--------------------------------
//
//  Constructor.
//
KANJIRAD_lookup::KANJIRAD_lookup (int count,int do_single) {
  single         = do_single;
  radical_bitmap = LoadBitmap(instance,MAKEINTRESOURCE(IDB_RADICALS));
  stroke_bitmap  = LoadBitmap(instance,MAKEINTRESOURCE(IDB_STROKES ));
#ifndef USE_BLOCK_CURSOR
  cursor_bitmap  = LoadBitmap(instance,MAKEINTRESOURCE(IDB_CURSOR  ));
#endif USE_BLOCK_CURSOR
  return;
}

//--------------------------------
//
//  Destructor.
//
KANJIRAD_lookup::~KANJIRAD_lookup () {
  DeleteObject (radical_bitmap);
  DeleteObject (stroke_bitmap);
#ifndef USE_BLOCK_CURSOR
  DeleteObject (cursor_bitmap);
#endif USE_BLOCK_CURSOR
  return;
}

//--------------------------------
//
//  This is the window procedure for the radical buttons window.  This 
//  window contains a number of buttons associated with the radicals
//  that the user can click on.  When the window is active, the caret 
//  is drawn in the window, as a line around the outside of the selected
//  button, or as a solid caret covering the entire button.  The user 
//  can press space to activate a button, and user the cursor keys to
//  move around in the region.
//
int KANJIRAD_lookup::radicals_winproc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;
  RECT        rect;
  int         i,j,x,y;
  switch (iMsg) {
//
//  During creation of the window, we determine the number of buttons 
//  we can put across the window.  We use this value to determine the 
//  number of vertical rows that we will need to setup the window.  We 
//  the adjust the height of the window.  We also need to go and adjust 
//  the height of the dialog box window to match the height of the button
//  window.
//
    case WM_CREATE:
         CREATESTRUCT *create;
         create    = (CREATESTRUCT *) lParam;
         top       = 0;
         last_char = 0;
#ifdef USE_SCROLL_RADICALS
         button_x  = (create->cx-GetSystemMetrics(SM_CXVSCROLL))/BUTTON_SIZE;
         button_y  = (BUTTON_TOTAL/button_x);
         button_v  = create->cy/BUTTON_SIZE;
         if (BUTTON_TOTAL % button_x) button_y++;
         MoveWindow (hwnd,create->x,create->y,button_x*BUTTON_SIZE+2*WIN_XEDGE+GetSystemMetrics(SM_CXVSCROLL),button_v*BUTTON_SIZE+2*WIN_YEDGE,true);
#else  USE_SCROLL_RADICALS
         button_x = create->cx/BUTTON_SIZE;
         button_y = (BUTTON_TOTAL/button_x);
         if (BUTTON_TOTAL % button_x) button_y++;
         button_v = button_y;
         MoveWindow (hwnd,create->x,create->y,button_x*BUTTON_SIZE+2*WIN_XEDGE,button_v*BUTTON_SIZE+2*WIN_YEDGE,true);
         ImmAssociateContext (hwnd,NULL);               // Disable the IME for this window
// ### should correct dialog box size.
#endif USE_SCROLL_RADICALS
         cursor = (button_v/2)*button_x+button_x/2;     // Plop cursor down in the middle of the box.
         set_cursor (hwnd);
         return (0);
    case WM_GETDLGCODE:
         return (DLGC_WANTARROWS | DLGC_WANTCHARS);     // Request arrow keys and chars (to get space).
    case WM_SETFOCUS:
         set_cursor (hwnd); 
         return (0);
    case WM_KILLFOCUS:
         DestroyCaret ();
         return (0);
//
//  Wheel mouse support.
//
#ifndef WINCE
    case WM_MOUSEWHEEL:
         static int delta;                                                  // Accumulation point for deltas.
         int shift,ctrl;
         shift  = SystemParametersInfo(SPI_GETWHEELSCROLLLINES,0,&ctrl,0);  // Scroll amount.
         ctrl   = LOWORD(wParam);                                           // Keys
         delta += (short) HIWORD(wParam);                                   // Delta
//
//  Shift -- skip pages.
//
         if ((ctrl & MK_SHIFT) || (shift == WHEEL_PAGESCROLL)) {
           while (delta > WHEEL_DELTA) {
             cursor += button_v*button_x;
             delta  -= WHEEL_DELTA;
           }
           while (abs(delta) > WHEEL_DELTA) {
             cursor -= button_v*button_x;
             delta  += WHEEL_DELTA;
           }
         }
//
//  Normal -- skip lines.
//
         else {
           while (delta > WHEEL_DELTA) {
             cursor += shift*button_x;
             delta  -= WHEEL_DELTA;
           }
           while (abs(delta) > WHEEL_DELTA) {
             cursor -= shift*button_x;
             delta  += WHEEL_DELTA;
           }
         }
         if (cursor <  0) cursor = 0;   
         if (cursor >= BUTTON_TOTAL) cursor = BUTTON_TOTAL-1;
         set_cursor (hwnd);
         return (0);
//
//  Process scroll messages.
//
#else WINCE
    case WM_VSCROLL:
         switch (LOWORD(wParam)) {
           case SB_LINEDOWN:
                cursor += button_x;
                break;
           case SB_PAGEDOWN:
                cursor += button_v*button_x;
                break;
           case SB_LINEUP:
                cursor -= button_x;
                break;
           case SB_PAGEUP:
                cursor -= button_v*button_x;
                break;
           case SB_THUMBTRACK:          // Tracking.
           case SB_THUMBPOSITION:
                i = HIWORD(wParam);
                cursor += (i-top)*button_x;
                break;
           default:
                return (0);
         }
         if (cursor <  0) cursor = 0;   
         if (cursor >= BUTTON_TOTAL) cursor = BUTTON_TOTAL-1;
         set_cursor (hwnd);
         return (0);
#endif  WINCE
//
//  Character input.  We accept the space character which selects a 
//  radical, and we accept numbers wich move use to the first radical
//  containning a specific stroke count.
//
    case WM_CHAR:
         if (wParam == ' ') {           // This is the space character.
           toggle_button ();
           return (0);
         }
         j = wParam-'0';
         if ((j >= 0) && (j <= 9)) {    // This is a number.
           if (last_char*10+j > 17) i = j; else i = last_char*10+j;
           last_char = j;
           for (j = 0; i; j++) if (!buttons[j]) i--;
           cursor = j;
           set_cursor (hwnd);
         }
         return (0);
//
//  Process keyboard events for special keys, we use these to manuver
//  arround the button set.
//
    case WM_KEYDOWN:
         i = (GetKeyState(VK_CONTROL) < 0);
         j = i ? CONTROL_MOVE : 1;
         switch (wParam) {
           case VK_RIGHT:           // Right 1 or CONTROL_MOVE
                cursor += j;
                break;
           case VK_LEFT:            // Left 1 or CONTROL_MOVE
                cursor -= j;
                break;
           case VK_UP:              // Up 1 or CONTROL_MOVE
                i = cursor % button_x;
                cursor -= j*button_x;
                if (cursor < 0) cursor = i;
                break;
           case VK_DOWN:            // Down 1 or CONTROL_MOVE
                cursor += j*button_x;
                break;
           case VK_HOME:            // Beginning of line or first button
                if (i) cursor = 0;
                cursor = (cursor/button_x)*button_x;
                break;
           case VK_END:             // End of line or last button
                if (i) cursor = BUTTON_TOTAL;
                cursor = (cursor/button_x)*button_x+button_x-1;
                break;
           case VK_PRIOR:           // Top of page
                i = cursor % button_x;
                cursor -= button_v*button_x;
                if (cursor < 0) cursor = i;
                break;
           case VK_NEXT:            // Bottom of page
                cursor += button_v*button_x;
                break;
           case VK_INSERT:
           case VK_V:                                                           // Fake message passed to window 
                if (!single) SendMessage (dialog,WM_COMMAND,IDC_RLCOPYKANJI,0); // to set radicals.
                return (0);
           default:
                return (0);
         }
         if (cursor <  0) cursor = 0;   
         if (cursor >= BUTTON_TOTAL) cursor = BUTTON_TOTAL-1;
         set_cursor (hwnd);
         return (0);
//
//  This is the difficult part, of corse, this is the draw routine.
//  Note that all draws are solid, and cause us to overwirte the entire 
//  button area, this allows us to not have the dredraw function fill 
//  with the background brush.  This has the advantage of supressing 
//  the flashing and actually being faster.
//
    case WM_PAINT:
         int    number;     // Current stroke number counter.
         int    rad;        // Current radical number.
         int    index;      // Current button we are working on.
         HDC    hdcmem;     // for holding the bitmaps.
         HBRUSH brush;      // Yellow brush for selected buttons.

         hdc    = BeginPaint(hwnd,&ps);
         hdcmem = CreateCompatibleDC(hdc);
         brush  = CreateSolidBrush(COLOR_SELRAD);
//       SetMapMode    (hdcmem,GetMapMode(hdc));            // Not necessary
         for (number = index = j = 0; j < button_y; j++) {  // Do each row.
           y = (j-top)*BUTTON_SIZE;
           for (i = 0; i < button_x; i++) {                 // Do each button in a row.
             if (index < BUTTON_TOTAL) {
               if (!buttons[index]) {                       // This is a stroke count number.
                 SelectObject (hdc,GetStockObject(WHITE_BRUSH));
                 Rectangle    (hdc,i*BUTTON_SIZE,y,i*BUTTON_SIZE+BUTTON_SIZE,y+BUTTON_SIZE);
                 SetTextColor (hdc,RGB(255,0,0));
                 SelectObject (hdcmem,stroke_bitmap);
                 BitBlt       (hdc,i*BUTTON_SIZE+BUTTON_SBMOFFSET,y+BUTTON_SBMOFFSET,BUTTON_SBMSIZE,BUTTON_SBMSIZE,hdcmem,0,(number++)*BUTTON_SBMSIZE,SRCAND);
               }
               else {                                       // This is a radical button.
                 rad  = buttons[index];
                 if (states[rad]) brush = (HBRUSH) SelectObject(hdc,brush);
                   else SelectObject (hdc,GetStockObject(LTGRAY_BRUSH));
                 Rectangle    (hdc,i*BUTTON_SIZE,y,i*BUTTON_SIZE+BUTTON_SIZE,y+BUTTON_SIZE);
                 SetTextColor (hdc,RGB(0,0,0));
                 SelectObject (hdcmem,radical_bitmap);
                 BitBlt       (hdc,i*BUTTON_SIZE+BUTTON_RBMOFFSET,y+BUTTON_RBMOFFSET,BUTTON_RBMSIZE,BUTTON_RBMSIZE,hdcmem,0,(rad-1)*BUTTON_RBMSIZE,SRCAND);
                 if (states[rad]) brush = (HBRUSH) SelectObject(hdc,brush);
               }
             }
             else {                                         // These are the blank spaces at the end of the array.
               rect.left   = i*BUTTON_SIZE;
               rect.top    = y;
               rect.right  = rect.left+BUTTON_SIZE;
               rect.bottom = rect.top+BUTTON_SIZE;
               FillRect (hdc,&rect,(HBRUSH) GetStockObject(GRAY_BRUSH));
             }
             index++;
           }
         }
         DeleteObject (brush);
         DeleteDC     (hdcmem);
         EndPaint     (hwnd,&ps);
         return (0);
//
//  Mouse event, implies we need to find out where the user is clicking
//  and toggle that button.
//
    case WM_LBUTTONDOWN:
         x = LOWORD(lParam)/BUTTON_SIZE;
         y = HIWORD(lParam)/BUTTON_SIZE;
         i = (y+top)*button_x+x;
         cursor = i;
         toggle_button ();
         SetFocus      (hwnd);
         set_cursor    (hwnd);
         return (0);
//
//  Double-click event will invoke a search
//
    case WM_LBUTTONDBLCLK: 
         SendMessage (dialog,WM_COMMAND,IDOK,0);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//--------------------------------
//
//  Clear all the radicals and redraw the mindow.
//
void KANJIRAD_lookup::reset_radicals () {
  int i;
  for (i = 0; i <= BUTTON_COUNT; i++) states[i] = false;
  redraw_radicals ();                   // Redraw radical buttons
  return;
}

//--------------------------------
//
//  This rotuine sets the cursor for the radical button window.
//
//      hwnd -- Pointer to the window to set.
//  
void KANJIRAD_lookup::set_cursor (HWND hwnd) {
  int x,y;
  y  = cursor/button_x;
  x  = cursor-button_x*(y);
  if (y >= top+button_v) { top = y-button_v+1; redraw_radicals (); }
  if (y < top) { top = y; redraw_radicals (); }
  y -= top;
#ifdef USE_BLOCK_CURSOR
  CreateCaret (hwnd,NULL,BUTTON_SIZE,BUTTON_SIZE);
#else  USE_BLOCK_CURSOR
  CreateCaret (hwnd,cursor_bitmap,BUTTON_SIZE,BUTTON_SIZE);
#endif USE_BLOCK_CURSOR
  SetCaretPos (x*BUTTON_SIZE,y*BUTTON_SIZE);
  ShowCaret   (hwnd);
#ifdef USE_SCROLL_RADICALS
//
//  Update scroll bar.
//
  scroll_info.nMax   = button_y-1;
  scroll_info.nPage  = button_v;
  scroll_info.nPos   = top;
  SetScrollInfo  (hwnd,SB_VERT,&scroll_info,true);
#endif USE_SCROLL_RADICALS
  return;
}

//--------------------------------
//
//  This routine toggles the state of the radical button indicated 
//  by the radical.  Because of the arrangement of the radical
//  buttons, we may need to activate more than one of the button, because
//  of redundent radicals.
//
//  This routine is designed only for use on the multi-radical window.  This was 
//  seperated out to support the extract all radicals form a character function.
//
//  Generally, redudent radicals are paires, except for one tripplet.
//
#define NUMBER_PAIRS    ((int) (sizeof(radical_pairs)/(2*sizeof(byte))))

void KANJIRAD_lookup::set_radical (int rad) {
  static byte radical_triple[3] = { 65,66,67 };                 // This is the one tripplet.
  static byte radical_pairs[][2] = {                            // These are the pairs.
    {187, 33},{194, 34},{ 52, 53},{ 55, 56},{165, 74},{ 75, 76},//   In this encouding, the reall radical
    { 81, 82},{ 87, 88},{ 98, 97},{119,114},{150,116},{155,117},//   is the first radical.  We do not use this,
    {147,141},{237,200},{239,220},{240,227}                     //   however (see the search rotuine).
  };
  byte state;
  int  i;

  state = states[rad] = !states[rad];               // Get state we are setting to 
  for (i = 0; i < NUMBER_PAIRS; i++) {              // Check pairs
    if (rad == radical_pairs[i][0]) states[radical_pairs[i][1]] = state;
    if (rad == radical_pairs[i][1]) states[radical_pairs[i][0]] = state;
  }
  for (i = 0; i < 3; i++) {                         // Check for the tipplet.
    if (rad == radical_triple[i]) {
      for (i = 0; i < 3; i++) states[radical_triple[i]] = state;
      break;
    }
  }
  redraw_radicals ();                               // Redraw the radicals
  return;
}

//--------------------------------
//
//  This routine toggles the state of the radical button indicated 
//  at the cursor location.  Because of the arrangement of the radical
//  buttons, we may need to activate more than one of the button, because
//  of redundent radicals.
//
//  Generally, redudent radicals are paires, except for one tripplet.
//
void KANJIRAD_lookup::toggle_button () {
  int  rad,i;
//
//  Is this a number.
//
  rad = buttons[cursor];                            // Get the radical.
  if (!rad) return;                                 // This is a number!
//
//  Single radical processing.
//
  if (single) {
    if (states[rad]) reset_radicals ();             // If radicals is selected clear all
      else {
        rad = bushus[rad];
        for (i = 0; i <= BUTTON_COUNT; i++) states[i] = (bushus[i] == rad);
      }
    redraw_radicals ();                             // Redraw the radicals
  }
//
//  Multi-select processing.
//
  else {
    set_radical (rad);
  }
  auto_check ();                                    // Check for auto search or clear the results list.
  return;
}

//--------------------------------
//
//  Forces a redraw of the radical buttons window.  This is called when
//  the state of these buttons changes.
//
void KANJIRAD_lookup::redraw_radicals () {
  InvalidateRect (GetDlgItem(dialog,IDC_RLRADICALS),NULL,false);
  return;
}

//===================================================================
//
//  Class RADSTROKE_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class RADSTROKE_lookup definition.
//
//  This calss is dirived from the KANJI_lookup class, but is not 
//  intended to be used directly.  This class defines a radical/stroke 
//  type lookup.  This basically defines a window in which one can choose
//  a radical by stroke count.  This is used for a form of the bushu-lookup
//  and for a the HS lookup.
//

typedef class RADSTROKE_lookup : public KANJI_lookup {
public:
  RADSTROKE_lookup  (void);                             // Constructor (required).
  ~RADSTROKE_lookup (void);
  int     dlg_lookup     (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
  int     bushu_winproc  (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);

  int     current_radical;                              // This is the current radical, index from 1, 0 is no select
  int     current_stroke;                               // Current radical stroke count;

  byte    (*data)[6];                                   // Data specific to processing the radicals.
  int     has_kanji;                                    // Does this lookup has a kanji item.
  int     max_radstrokes;                               // Maximum number of radical strokes.
  HBITMAP radical_bitmap;                               // Radicals bitmaps.
  byte   *radical_list;                                 // List used for generating non-variation radicals
  byte   *radical_value;                                // Determines the value associated witha radical
  int     get_radical    (void);                        // Get the current radical
private:
  HWND    rad_window;                                   // Window for the radical select control
  HBITMAP cursor_bitmap;                                // Bitmap for caret in radicals window.
  int     buttons;                                      // Number of visible buttons in the x direction
  int     cursor;                                       // Element containning the cursor.
  int     left;                                         // Left most visible element.
  int     last_button;                                  // Number of radical buttons visible
  int     max_radical;                                  // Maximum radical value.

  void    reset          (void);                        // Reset the state of the search.
  int     get_index      (int box);                     // Get index in radical bitmap from view index.
  void    set_cursor     (int show);                    // Set the cursor postition
  void    set_radstroke  (void);                        // Set the radical stroke count.
  void    set_radical    (int value);                   // Set the radical
  void    toggle_button  (void);                        // Set the current selected radical as the default.
  void inline redraw_radicals (void) { InvalidateRect (rad_window,NULL,false); }
  
} RADSTROKE_lookup;

//-------------------------------------------------------------------
//
//  Static data
//
//
//  These access the data array that indicates all the major parameters associated with the radica data.   For each
//  possible number of radical strokes, five numbers are specifiec.  Note that a final number 
//  is used to terminate the array.  The number are:
//
//      0. Number of buttons with varaitions
//      1. Number of buttons without varaitions
//      2. Number of radials with this number of strokes and variations
//      3. Number of radicals with this number of strokes and no variations.
//      4. Index directly into radicals bitmap for this list.  This is used with variations.
//      5. Index into radical list used with no variations to draw the radicals.
//
#define DATA_VARBUTTONS 0   // Number of buttons with variations
#define DATA_NOVBUTTONS 1   // Number of buttons without varaitions
#define DATA_VARNUMBER  2   // Number of radicals with variations
#define DATA_NOVNUMBER  3   // Number of radicals without variations
#define DATA_VARINDEX   4   // Index into radicals used with variations.
#define DATA_NOVINDEX   5   // Index into button list used without variations

#define VARIATION_RADICALS(x)   (data[x][DATA_VARBUTTONS])
#define NOVAR_RADICALS(x)       (data[x][DATA_NOVBUTTONS])
#define VARIATION_NUMBER(x)     (data[x][DATA_VARNUMBER])
#define NOVAR_NUMBER(x)         (data[x][DATA_NOVNUMBER])
#define VARIATION_INDEX(x)      (data[x][DATA_VARINDEX])
#define NOVAR_INDEX(x)          (data[x][DATA_NOVINDEX])   

//--------------------------------
//
//  Used to convert radical numbers to letters and back.  Note the missing l.
//
static char radical_letters[] = "abcdefghijkmnopqrstuvwxyzABCDEFGHIJKLMNO";

//-------------------------------------------------------------------
//
//  static procedures.
//

static LRESULT CALLBACK JWP_bushu_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  RADSTROKE_lookup *lookup;
  if (iMsg == WM_CREATE) SetWindowLong (hwnd,0,(long) temp_link);
  lookup = (class RADSTROKE_lookup *) GetWindowLong(hwnd,0);
  return (lookup->bushu_winproc(hwnd,iMsg,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  begin class RADSTROKE_lookup
//

//--------------------------------
//
//  Class constructor.  Needs to get access to the radical bit map and
//  and the cursor bitmap
//
RADSTROKE_lookup::RADSTROKE_lookup () {
#ifndef USE_BLOCK_CURSOR
  cursor_bitmap  = LoadBitmap(instance,MAKEINTRESOURCE(IDB_CURSOR  ));
#endif USE_BLOCK_CURSOR
  return;
}

//--------------------------------
//
//  Destructor.
//
RADSTROKE_lookup::~RADSTROKE_lookup () {
#ifndef USE_BLOCK_CURSOR
  DeleteObject (cursor_bitmap);
#endif USE_BLOCK_CURSOR
  DeleteObject (radical_bitmap);
  return;
}

//--------------------------------
//
//  Window procedure for the radical selection window strip control
//
int RADSTROKE_lookup::bushu_winproc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;
  int         i,j;

  switch (iMsg) {
//
//  During creation of the window, we determine the number of buttons 
//  we can put across the window.  
//
    case WM_CREATE:                 // Create window and adjust size.
         CREATESTRUCT *create;
         rad_window = hwnd;
         create     = (CREATESTRUCT *) lParam;
         buttons    = create->cx/BUTTON_SIZE;
         left       = 0;
         cursor     = 0;
         MoveWindow (hwnd,create->x,create->y,buttons*BUTTON_SIZE+2*WIN_XEDGE,BUTTON_SIZE+2*WIN_YEDGE+GetSystemMetrics(SM_CYHSCROLL),true);
         ImmAssociateContext (hwnd,NULL);       // Disable the IME for this window
         set_cursor (true);
         return (0);
    case WM_GETDLGCODE:             // Want cursor keys for shifting the selected kanji and space for select
         return (DLGC_WANTARROWS | DLGC_WANTCHARS);
//
//  Take care of activation/deactivateion
//
    case WM_SETFOCUS:
         set_cursor (true); 
         return (0);
    case WM_KILLFOCUS:
         DestroyCaret ();
         return (0);
//
//  Process mouse-wheel messages.
//
#ifndef WINCE
    case WM_MOUSEWHEEL:
         static int delta;                                                  // Accumulation point for deltas.
         int shift,ctrl;
         shift  = SystemParametersInfo(SPI_GETWHEELSCROLLLINES,0,&ctrl,0);  // Scroll amount.
         ctrl   = LOWORD(wParam);                                           // Keys
         delta += (short) HIWORD(wParam);                                   // Delta
//
//  Shift -- skip pages.
//
         if ((ctrl & MK_SHIFT) || (shift == WHEEL_PAGESCROLL)) {
           while (delta > WHEEL_DELTA) {
             cursor -= buttons;
             delta  -= WHEEL_DELTA;
           }
           while (abs(delta) > WHEEL_DELTA) {
             cursor += buttons;
             delta  += WHEEL_DELTA;
           }
         }
//
//  Normal -- skip lines.
//
         else {
           while (delta > WHEEL_DELTA) {
             cursor -= shift;
             delta  -= WHEEL_DELTA;
           }
           while (abs(delta) > WHEEL_DELTA) {
             cursor += shift;
             delta  += WHEEL_DELTA;
           }
         }
         set_cursor (true);
         return (0);
#endif  WINCE
//
//  Process scroll messages.
//
    case WM_HSCROLL:
         switch (LOWORD(wParam)) {
           case SB_LINEDOWN:
                cursor++;
                break;
           case SB_PAGEDOWN:
                cursor += buttons;
                break;
           case SB_LINEUP:
                cursor--;
                break;
           case SB_PAGEUP:
                cursor -= buttons;
                break;
           case SB_THUMBTRACK:          // Tracking.
           case SB_THUMBPOSITION:
                i = HIWORD(wParam);
                cursor += i-left;
                break;
           default:
                return (0);
         }
         set_cursor (true);
         SetFocus   (hwnd);
         return (0);
//
//  This is the difficult part, of corse, this is the draw routine.
//  Note that all draws are solid, and cause us to overwirte the entire 
//  button area, this allows us to not have the dredraw function fill 
//  with the background brush.  This has the advantage of supressing 
//  the flashing and actually being faster.
//
    case WM_PAINT: {
           int    rad;        // Current radical number.
           HDC    hdcmem;     // for holding the bitmaps.
           HBRUSH brush;      // Yellow brush for selected buttons.
           RECT   rect;       // Rectangle that defines the curent button.

           hdc    = BeginPaint(hwnd,&ps);
           hdcmem = CreateCompatibleDC(hdc);
           brush  = CreateSolidBrush(COLOR_SELRAD);
//         SetMapMode    (hdcmem,GetMapMode(hdc));              // Not necessary
           for (i = 0; i < buttons; i++) {                      // Draw all visisble buttons.
             j = i+left;                                        // True index for this array.
             if (j >= last_button) {                            // Blank space at end of list.
               rect.top    = 0;
               rect.bottom = rect.top+BUTTON_SIZE;
               rect.left   = i*BUTTON_SIZE;
               rect.right  = rect.left+BUTTON_SIZE;
               FillRect (hdc,&rect,(HBRUSH) GetStockObject(GRAY_BRUSH));
             }
             else {                                             // Real button.
               rad = get_index(j);                              // Get index into radical array.
               if (current_radical == radical_value[rad]) brush = (HBRUSH) SelectObject(hdc,brush); else SelectObject (hdc,GetStockObject(LTGRAY_BRUSH));
               Rectangle    (hdc,i*BUTTON_SIZE,0,i*BUTTON_SIZE+BUTTON_SIZE,BUTTON_SIZE);
               SetTextColor (hdc,RGB(0,0,0));
               SelectObject (hdcmem,radical_bitmap);
               BitBlt       (hdc,i*BUTTON_SIZE+BUTTON_RBMOFFSET,BUTTON_RBMOFFSET,BUTTON_RBMSIZE,BUTTON_RBMSIZE,hdcmem,0,rad*BUTTON_RBMSIZE,SRCAND);
               if (current_radical == radical_value[rad]) brush = (HBRUSH) SelectObject(hdc,brush);
             }
           }
           DeleteObject (brush);
           DeleteDC     (hdcmem);
           EndPaint     (hwnd,&ps);
         }        
         return (0);
//
//  Keyboard processing.
//
//
//  Character input.  We accept the space character which selects a 
//  radical, and we accept numbers wich move use to the first radical
//  containning a specific stroke count.
//
    case WM_CHAR:
         if (wParam == ' ') toggle_button ();
         set_cursor (true);
         return (0);
//
//  Process keyboard events for special keys, we use these to manuver
//  arround the button set.
//
    case WM_KEYDOWN:
         i = (GetKeyState(VK_CONTROL) < 0);
         j = i ? CONTROL_MOVE : 1;
         switch (wParam) {
           case VK_RIGHT:           // Right 1 or CONTROL_MOVE
                cursor += j;
                break;
           case VK_LEFT:            // Left 1 or CONTROL_MOVE
                cursor -= j;
                break;
           case VK_HOME:            // Beginning of line or first button
                if (i) cursor = 0; else cursor = left;
                break;
           case VK_END:             // End of line or last button
                if (i) cursor = last_button; else cursor = left+buttons-1;
                break;
           default:
                return (0);
         }
         set_cursor (true);
         return (0);
//
//  Mouse event, implies we need to find out where the user is clicking
//  and toggle that button.
//
    case WM_LBUTTONDOWN:
         i = left+(LOWORD(lParam)/BUTTON_SIZE);
         cursor = i;
         toggle_button ();
         SetFocus      (hwnd);
         set_cursor    (true);
         return (0);
//
//  Double-click event will invoke a search
//
    case WM_LBUTTONDBLCLK: 
         SendMessage (dialog,WM_COMMAND,IDOK,0);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//--------------------------------
//
//  This is a piece of a dialog box control rotuine, that handles most of the common things
//  that need to be done.
//
int RADSTROKE_lookup::dlg_lookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  switch (iMsg) {
    case WM_NOTIFY:
         if (wParam == IDC_HSRADSPIN) {
           int       i;
           NMUPDOWN *notify;
           notify = (NMUPDOWN *) lParam;
           i      = get_radical()-notify->iDelta;
           if      (i > max_radical) i = 0;
           else if (i < 0          ) i = max_radical;
           set_radical (i);
         }
         return (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           case IDC_HSVARIANTS:
                jwp_config.cfg.no_variants = !IsDlgButtonChecked(hwnd,IDC_HSVARIANTS);
                last_button                = data[current_stroke][jwp_config.cfg.no_variants];
                max_radical                = jwp_config.cfg.no_variants ? NOVAR_NUMBER(current_stroke) : VARIATION_NUMBER(current_stroke);
                set_cursor      (false);
                redraw_radicals ();
                break;
           case IDC_HSRADSTROKE:
                if (HIWORD(wParam) == EN_CHANGE) set_radstroke ();
                return (true);
           case IDC_HSRADICAL:
           case IDC_HSOTHER:
           case IDC_HSKANJI:
                input_check (hwnd,wParam);
                if (HIWORD(wParam) != EN_CHANGE) return (true);
                break;
           default:
                break;
         }
         return (command(wParam));
  }
  return (false);       // Let the dialog box handler handle the standard
}

//--------------------------------
//
//  Get the index into the radical bitmap for the current radial.  This is based on the
//  viewming mode and other options.
//
//      box -- display box to get the radical index for.  This goes from zero.
//
int RADSTROKE_lookup::get_index (int box) {
  if (jwp_config.cfg.no_variants) return (radical_list[box+NOVAR_INDEX(current_stroke)]);
  return (box+VARIATION_INDEX(current_stroke));
}

//--------------------------------
//
//  This routine gets the current radical selected in the alphabetical list and 
//  converts it to a number form.  
//
//      RETURN -- selected radical.  A value of zero indicates no selected radical.
//
int RADSTROKE_lookup::get_radical () {
  int   i;
  TCHAR buffer[SIZE_BUFFER];
  GetDlgItemText (dialog,IDC_HSRADICAL,buffer,SIZE_BUFFER);
  for (i = 0; i < max_radical; i++) if (radical_letters[i] == buffer[0]) return (i+1);
  return (0);
}

//--------------------------------
//
//  Resets the entire dialog box to it's default state.  This is
//  the Clear Button, it is also used to initialize the dalog box.
//
void RADSTROKE_lookup::reset () {
  if (has_kanji) set_value (IDC_HSKANJI,0);
  set_value     (IDC_HSRADSTROKE,0);
  set_value     (IDC_HSOTHER    ,0);
  set_radstroke ();
  set_radical   (0);
  clear_results ();
  return;
}

//--------------------------------
//
//  This rotuine sets the cursor for the radical button window.
//
//      show -- If non-zero then the cursor will actually be generated.
//  
void RADSTROKE_lookup::set_cursor (int show) {
  if (cursor >= last_button ) cursor = last_button-1;
  if (cursor <  0           ) cursor = 0;
  if (cursor >= left+buttons) { left = cursor-buttons+1; redraw_radicals (); }
  if (cursor <  left        ) { left = cursor;           redraw_radicals (); }
  if (left && (left+buttons >= last_button)) {      // Handles the case when the list is short
    left = last_button-buttons;
    if (left < 0) left = 0;
    redraw_radicals ();
  }
//
//  Update scroll bar.
//
  scroll_info.nMax   = last_button-1;
  scroll_info.nPage  = buttons;
  scroll_info.nPos   = left;
  SetScrollInfo   (rad_window,SB_HORZ,&scroll_info,true);
//
//  Create cursor if necessary.
//
  if (show) {
#ifdef USE_BLOCK_CURSOR
    CreateCaret (rad_window,NULL,BUTTON_SIZE,BUTTON_SIZE);
#else  USE_BLOCK_CURSOR
    CreateCaret (rad_window,cursor_bitmap,BUTTON_SIZE,BUTTON_SIZE);
#endif USE_BLOCK_CURSOR
    SetCaretPos ((cursor-left)*BUTTON_SIZE,0);
    ShowCaret   (rad_window);
  }
  return;
}

//--------------------------------
//
//  Sets the currently selected radical.  This sets the edit box letter, and then
//  sets the radical display to match.
//
//      value -- Radical value to set.  Setting this to zero will deselect all radicals.
//               this will result in a random search later.
//
void RADSTROKE_lookup::set_radical (int value) {
  int j,k;
  TCHAR buffer[10];
  if (value < 0          ) value = 0;                   // Clip value to acceptable range.
  if (value > max_radical) value = max_radical;
  if (!value) buffer[0] = 0;                            // Build text string for edit box and set
    else {
      buffer[0] = radical_letters[value-1];
      buffer[1] = 0;
    }
  if (!value) current_radical = 0;                      // Set selected radical
    else {                                              // Count number of distint radicals to find the current one
      for (k = 1, j = 0; k < value; j++, k++) {
        while (radical_value[get_index(j)] == radical_value[get_index(j+1)]) j++;
      }
      current_radical = radical_value[get_index(j)];    // Set radical value and cursor location
      cursor = j;
    }
  redraw_radicals ();                                   // Redraw radicals list.
  set_cursor      (false);
  SetDlgItemText  (dialog,IDC_HSRADICAL,buffer);        // This is here because this forces a search on auto.
  return;
}

//--------------------------------
//
//  Responds to changes in the number of radical strokes.  This assumes the control is set
//  before calling, and just responds to the changes.
//
void RADSTROKE_lookup::set_radstroke () {
  int i,v1,v2;
  i = get_value(IDC_HSRADSTROKE,max_radstrokes,v1,v2);      // Get number of strokes  
  max_radical     = jwp_config.cfg.no_variants ? NOVAR_NUMBER(i) : VARIATION_NUMBER(i);
  last_button     = data[i][jwp_config.cfg.no_variants];    // Set number of buttons
  current_stroke  = i;                                      // Set current strokes
  set_radical (0);
  return;
}

//--------------------------------
//
//  Routine used to select a radical from the list via the selection window (not the edit-box).
//
void RADSTROKE_lookup::toggle_button () {
  int i,j,k;
  i = get_index(cursor);                                            // Get index for radical selected
  if (!current_stroke) {                                            // This the full set of radicals, so zoom to a sub-list
    for (j = 1; i >= (int) VARIATION_INDEX(j); j++);                //   Find stroke count for radical
    set_value (IDC_HSRADSTROKE,j-1);                                //   Set the stroke count edit box.
    for (j = 0; (j < last_button) && (i != get_index(j)); j++);     //   Adjust the cursor position.
    cursor = j;
  }
  for (k = j = 0; j < last_button; j++, k++) {                      // Find radial number by counting distinct radical 
    if (radical_value[i] == radical_value[get_index(j)]) break;     //    values.
    while (radical_value[get_index(j)] == radical_value[get_index(j+1)]) j++;
  }
  set_radical (k+1);
  return;
}

//===================================================================
//
//  Class BUSHU_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class BUSHU_lookup definition.
//
//  This class is dirived from the KANJI_lookup class and process the 
//  Bushu lookup function.
//

typedef class BUSHU_lookup : public KANJIRAD_lookup {
friend void bushu_lookup (JWP_file *file);
public:
  inline BUSHU_lookup    (int count) : KANJIRAD_lookup (count,true) { return; }     // Constructor (required).
  int  dlg_bushulookup   (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
  BUSHU_lookup           *bushu;
private:                                                // Required procedures:
  void   reset           (void);                        // Reset the search engine.
  void   search          (void);                        // Execute the actual search.
private:
} BUSHU_lookup;

//-------------------------------------------------------------------
//
//  static data and definitions.
//

static BUSHU_lookup *bs_lookup = NULL;  // Global pointer for the skip lookup chart.

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Dialog box procedure stub for the Bushu Lookup dialog box.
//
static BOOL CALLBACK dialog_bushulookup (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (bs_lookup->dlg_bushulookup(hwnd,message,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  begin class BUSHU_lookup
//

//--------------------------------
//
//  Dailog box handler for the radical lookup dailog box.  This does all
//  the control work here.
//
int BUSHU_lookup::dlg_bushulookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  switch (iMsg) {
    case WM_INITDIALOG:                         // Intialize, so chache dialog box pointer.
         CheckDlgButton  (hwnd,IDC_BLNELSON   ,jwp_config.cfg.bushu_nelson   );
         CheckDlgButton  (hwnd,IDC_BLCLASSICAL,jwp_config.cfg.bushu_classical);
         initialize      (hwnd,true,IDC_RLRADICALS);
         POCKETPC_DIALOG (hwnd);
         return (false);
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_BUSHULOOKUP);
         return  (true);
    case WM_DESTROY:
         jwp_config.cfg.bushu_nelson    = IsDlgButtonChecked(hwnd,IDC_BLNELSON   );
         jwp_config.cfg.bushu_classical = IsDlgButtonChecked(hwnd,IDC_BLCLASSICAL);
         remove_dialog (hwnd);
         delete this;
         bs_lookup = NULL;
         return (true);
    case WM_NOTIFY:
         if (wParam == IDC_BLSPIN) do_spinner ((NMUPDOWN *) lParam,IDC_BLSTROKE);
         return (0);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           case IDC_BLNELSON:
                if (!IsDlgButtonChecked(hwnd,IDC_BLNELSON)) CheckDlgButton (hwnd,IDC_BLCLASSICAL,true);
                break;
           case IDC_BLCLASSICAL:
                if (!IsDlgButtonChecked(hwnd,IDC_BLCLASSICAL)) CheckDlgButton (hwnd,IDC_BLNELSON,true);
                break;
           case IDC_BLSTROKE:
                input_check (hwnd,wParam);
                if (HIWORD(wParam) != EN_CHANGE) return (true);
                break;
           default:
                break;
         }
         return (command(wParam));
  }
  return (false);       // Let the dialog box handler handle the standard
}                                                   //   buttons and actions.    

//--------------------------------
//
//  Resets the entire dialog box to it's default state.  This is
//  the Clear Button, it is also used to initialize the dalog box.
//
void BUSHU_lookup::reset () {
  set_value (IDC_BLSTROKE,0);
  adjusting = true;
  adjusting = false;
  clear_results  ();
  reset_radicals ();
  return;
}

//--------------------------------
//
//  This does the actual search.
//
void BUSHU_lookup::search () {
  int i,j,b1,b2,s1,s2,n_bushu,c_bushu,nelson,classical;
  KANJI_info kanji_info;        // Class used to access the kanji information database
//
//  Get search parameters.
//
  get_value (IDC_BLSTROKE,MAX_STROKES,s1,s2);
  nelson    = IsDlgButtonChecked(dialog,IDC_BLNELSON   );
  classical = IsDlgButtonChecked(dialog,IDC_BLCLASSICAL);
  for (b2 = 255, b1 = i = 0; i <= BUTTON_COUNT; i++) {
    if (states[i]) {
      b2 = b1 = bushus[i];
      break;
    }
  }
//
//  Can we get kanji data?
//
  if (kanji_info.open_info(dialog)) return;             // Open info file.
//
//  Do the search.
//
  for (i = MIN_KANJI1; i <= MAX_KANJI1; i += 0x100) {
    for (j = MIN_KANJI2; j <= MAX_KANJI2; j++) {
//
//  Check for valid matching code.
//
      kanji_info.get_info (i|j,INFO_FIXED);             // Get information for this kanji.
      n_bushu = kanji_info.kinfo.bushu;                 // Get the bushu
      c_bushu = kanji_info.kinfo.classical;
      if (n_bushu == 23) n_bushu = 22;                  // Map out the buttons not included
      if (n_bushu == 35) n_bushu = 34;
      if (c_bushu == 23) c_bushu = 22;
      if (c_bushu == 25) c_bushu = 34;
      if (c_bushu ==  0) c_bushu = n_bushu;             // Classical = 0 -> use nelson
      if ((kanji_info.kinfo.strokes >= s1) && (kanji_info.kinfo.strokes <= s2) &&
          ((nelson && (n_bushu >= b1) && (n_bushu <= b2)) || (classical && (c_bushu >= b1) && (c_bushu <= b2)))) {
        put_kanji (i|j);
        put_kanji ('/');
      }
    }
  }
//
//  Cleanup
//
  kanji_info.close_info ();                             // Close the info database.
  return;
}

//-------------------------------------------------------------------
//
//  Stub routine for entry
//

//--------------------------------
//
//  Generailly, I do not like friend fucntions, but I need an entry point,
//  and I wanted all the routines in this file.
//
//      file -- Context from which we were called so we can past text boack
//
void bushu_lookup (JWP_file *file) {
  if (bs_lookup) { SetForegroundWindow (bs_lookup->dialog); return; }
  if (!(bs_lookup = new BUSHU_lookup(3000))) { OutOfMemory (file->window); return; }
  JCreateDialog (IDD_BUSHULOOKUP,file->window,(DLGPROC) dialog_bushulookup);
  return;
}

//===================================================================
//
//  Class BUSHU2_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class BUSHU2_lookup definition.
//
//  This class is dirived from the RADSTROKE_lookup class and process the 
//  SKIP lookup function.
//
//  This search engine uses a non-standard coding.  Unlike almost all 
//  other searches, the value of 0 is needed for the four-corners system.
//  The corner boxes, thus use the value of 10 as a wild card.  
//

typedef class BUSHU2_lookup : public RADSTROKE_lookup {
friend void spahn_lookup (JWP_file *file);
public:
  BUSHU2_lookup  (void);                // Constructor (required).
  int     dlg_bushulookup   (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
private:                            // Required procedures:
  void   search (void);             // Execute the actual search.
} BUSHU2_lookup;

//-------------------------------------------------------------------
//
//  static data and definitions.
//

static BUSHU2_lookup *b2_lookup = NULL; // Global pointer for the skip lookup chart.

#define MAX_BUSHUSTROKES    17          // Number of radical strokes that are possible

//--------------------------------
//
//  This array indicates all the major parameters associated with the radica data.   For each
//  possible number of radical strokes, five numbers are specifiec.  Note that a final number 
//  is used to terminate the array.  The number are:
//
//      0. Number of buttons with varaitions
//      1. Number of buttons without varaitions
//      2. Number of radials with this number of strokes
//      3. Index directly into radicals bitmap for this list.  This is used with variations.
//      4. Index into radical list used with no variations to draw the radicals.
//
static byte b2_data[][6] = {
  { 241,211,   0, 0,    0,  0 },  // 0
  {   6,  6,   6, 6,    0,  0 },  // 1
  {  28, 23,  25,23,    6,  6 },  // 2
  {  40, 30,  35,30,   34, 29 },  // 3 
  {  43, 34,  38,34,   74, 59 },  // 4
  {  25, 23,  25,23,  117, 93 },  // 5
  {  29, 29,  29,29,  142,116 },  // 6
  {  19, 18,  18,18,  171,145 },  // 7
  {  10,  9,  10, 9,  190,163 },  // 8
  {  11, 11,  11,11,  200,172 },  // 9
  {   9,  8,   9, 8,  211,183 },  // 10
  {   7,  6,   7, 6,  220,191 },  // 11
  {   4,  4,   4, 4,  227,197 },  // 12
  {   4,  4,   4, 4,  231,201 },  // 13
  {   2,  2,   2, 2,  235,205 },  // 14
  {   1,  1,   1, 1,  237,207 },  // 15
  {   2,  2,   2, 2,  238,208 },  // 16
  {   1,  1,   1, 1,  240,209 },  // 17
  {   0,  0,   0, 0,  241,210 },  // 18 none
};

//--------------------------------
//
//  Radical list,  The radicals are in order and include the variants.  This hops through 
//  the list just getting the main radicals.  Each line is a different stroke count, starting
//  at two.
//
static byte b2_radlist[] = {
    0,  1,  2,  3,  4,     5,
    6,  7,  8, 11, 12,    13, 14, 15, 16, 17,    18, 19, 21, 22, 23,    24, 25, 26, 27, 28,    29, 30, 31, 
   34, 35, 36, 37, 38,    39, 40, 41, 42, 43,    44, 45, 47, 48, 49,    50, 51, 53, 54, 56,    57, 58, 59, 60, 61,    62, 63, 65, 67, 68,  
   74, 76, 77, 78, 79,    80, 82, 83, 84, 85,    86, 88, 89, 90, 91,    92, 93, 94, 95, 97,    98, 99,100,101,102,   103,105,106,107,108,   109,110,111,112,
  117,118,119,120,121,   122,123,124,125,126,   127,128,129,130,131,   132,133,134,135,136,   137,138,139,
  142,143,144,145,146,   147,148,149,150,151,   152,153,154,155,156,   157,158,159,160,161,   162,163,164,165,166,   167,168,169,170,
  171,172,173,174,175,   176,178,179,180,181,   182,183,184,185,186,   187,188,189,
  190,191,192,193,194,   195,196,197,198,
  200,201,202,203,204,   205,206,207,208,209,   210,
  211,212,213,214,215,   216,217,218,
  220,221,222,223,224,   225,
  227,228,229,230,
  231,232,233,234,
  235,236,
  237,
  238,239,
  240,
};

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Dialog box procedure stub for the four-corner Lookup dialog box.
//
static BOOL CALLBACK dialog_bushu2lookup (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (b2_lookup->dlg_bushulookup(hwnd,message,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  begin class HS_lookup
//

//--------------------------------
//
//  Class constructor.  Needs to get access to the radical bit map and
//  and the cursor bitmap
//
BUSHU2_lookup::BUSHU2_lookup () {
  max_radstrokes = MAX_BUSHUSTROKES;
  data           = b2_data;
  has_kanji      = false;
  radical_list   = b2_radlist;
  radical_value  = &bushus[1];
  radical_bitmap = LoadBitmap(instance,MAKEINTRESOURCE(IDB_RADICALS));
  return;
}

//--------------------------------
//
//  Dailog box handler for the radical lookup dailog box.  This does all
//  the control work here.
//
int BUSHU2_lookup::dlg_bushulookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  switch (iMsg) {
    case WM_INITDIALOG:                         // Intialize, so chache dialog box pointer.
         SendDlgItemMessage (hwnd,IDC_HSRADSRKSPIN,UDM_SETRANGE,0,MAKELONG(MAX_BUSHUSTROKES,0));
         CheckDlgButton     (hwnd,IDC_HSVARIANTS,!jwp_config.cfg.no_variants);
         CheckDlgButton     (hwnd,IDC_BLNELSON   ,jwp_config.cfg.bushu_nelson   );
         CheckDlgButton     (hwnd,IDC_BLCLASSICAL,jwp_config.cfg.bushu_classical);
         initialize         (hwnd,true,IDC_HSRADSTROKE);
         POCKETPC_DIALOG    (hwnd);
         return (false);
    case WM_DESTROY:
         jwp_config.cfg.bushu_nelson    = IsDlgButtonChecked(hwnd,IDC_BLNELSON   );
         jwp_config.cfg.bushu_classical = IsDlgButtonChecked(hwnd,IDC_BLCLASSICAL);
         remove_dialog (hwnd);
         delete this;
         b2_lookup = NULL;
         return (true);
    case WM_NOTIFY:
         if (wParam == IDC_HSOTHERSPIN) {
           NMUPDOWN *ud;
           int       i,j;
           ud = (NMUPDOWN *) lParam;
           i = GetDlgItemInt(hwnd,IDC_HSOTHER    ,NULL,false);
           j = GetDlgItemInt(hwnd,IDC_HSRADSTROKE,NULL,false);
           i -= ud->iDelta;
           if (i < 0          ) i = MAX_STROKES;
           if (i > MAX_STROKES) i = 0;
           if (i && (i < j)) i = (ud->iDelta < 0) ? j : 0;
           SetDlgItemInt (hwnd,IDC_HSOTHER,i,false);
           return (true);
         }
         break;
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_BSLOOKUP);
         return  (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           case IDC_BLNELSON:
                if (!IsDlgButtonChecked(hwnd,IDC_BLNELSON)) CheckDlgButton (hwnd,IDC_BLCLASSICAL,true);
                break;
           case IDC_BLCLASSICAL:
                if (!IsDlgButtonChecked(hwnd,IDC_BLCLASSICAL)) CheckDlgButton (hwnd,IDC_BLNELSON,true);
                break;
           default:
                break;
         }
         break;
  }
  return (dlg_lookup (hwnd,iMsg,wParam,lParam));
}                                                   //   buttons and actions.    

//--------------------------------
//
//  This does the actual search.
//
void BUSHU2_lookup::search () {
  int i,j,b1,b2,s1,s2,n_bushu,c_bushu,nelson,classical;
  KANJI_info kanji_info;        // Class used to access the kanji information database
//
//  Get search parameters.
//
  get_value (IDC_HSOTHER,MAX_STROKES,s1,s2);
  nelson    = IsDlgButtonChecked(dialog,IDC_BLNELSON   );
  classical = IsDlgButtonChecked(dialog,IDC_BLCLASSICAL);
  if (!current_radical) { b1 = 0; b2 = 255; } else b1 = b2 = current_radical;
//
//  Can we get kanji data?
//
  if (kanji_info.open_info(dialog)) return;             // Open info file.
//
//  Do the search.
//
  for (i = MIN_KANJI1; i <= MAX_KANJI1; i += 0x100) {
    for (j = MIN_KANJI2; j <= MAX_KANJI2; j++) {
//
//  Check for valid matching code.
//
      kanji_info.get_info (i|j,INFO_FIXED);             // Get information for this kanji.
      n_bushu = kanji_info.kinfo.bushu;                 // Get the bushu
      c_bushu = kanji_info.kinfo.classical;
      if (n_bushu == 23) n_bushu = 22;                  // Map out the buttons not included
      if (n_bushu == 35) n_bushu = 34;
      if (c_bushu == 23) c_bushu = 22;
      if (c_bushu == 25) c_bushu = 34;
      if (c_bushu ==  0) c_bushu = n_bushu;             // Classical = 0 -> use nelson
      if ((kanji_info.kinfo.strokes >= s1) && (kanji_info.kinfo.strokes <= s2) &&
          ((nelson && (n_bushu >= b1) && (n_bushu <= b2)) || (classical && (c_bushu >= b1) && (c_bushu <= b2)))) {
        put_kanji (i|j);
        put_kanji ('/');
      }
    }
  }
//
//  Cleanup
//
  kanji_info.close_info ();                             // Close the info database.
  return;
}

//-------------------------------------------------------------------
//
//  Stub routine for entry
//

//--------------------------------
//
//  Generailly, I do not like friend fucntions, but I need an entry point,
//  and I wanted all the routines in this file.
//
//      file -- Context from which we were called so we can past text boack
//
void bushu2_lookup (JWP_file *file) {
  if (b2_lookup) { SetForegroundWindow (b2_lookup->dialog); return; }
  if (!(b2_lookup = new BUSHU2_lookup)) { OutOfMemory (file->window); return; }
  JCreateDialog (IDD_BUSHU2LOOKUP,file->window,(DLGPROC) dialog_bushu2lookup);
  return;
}

//===================================================================
//
//  Class FOURCORNER_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class FOURCORNER_lookup definition.
//
//  This class is dirived from the KANJI_lookup class and process the 
//  SKIP lookup function.
//
//  This search engine uses a non-standard coding.  Unlike almost all 
//  other searches, the value of 0 is needed for the four-corners system.
//  The corner boxes, thus use the value of 10 as a wild card.  
//

typedef class FOURCORNER_lookup : public KANJI_lookup {
friend void fourcorner_lookup (JWP_file *file);
public:
  int    dlg_fclookup    (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
private:
  void   reset           (void);                        // Reset the search engine.
  void   search          (void);                        // Execute the actual search.
private:
  int    current;                                       // Indicates the current control for shape selection
  void   check_value     (int id);                      // Check value to see if it is 11.
  void   set_value       (int id,int value);            // Set a value.
  int    test            (int limits[5][2],int main,int index);
} FOURCORNER_lookup;

//-------------------------------------------------------------------
//
//  static data and definitions.
//

static FOURCORNER_lookup *fc_lookup = NULL;     // Global pointer for the skip lookup chart.

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Dialog box procedure stub for the four-corner Lookup dialog box.
//
static BOOL CALLBACK dialog_fclookup (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (fc_lookup->dlg_fclookup(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  This routine controls the FC shape selector control.  This control allows you to
//  choose shapes by using the mouse.
//
#define FC_FULLWIDTH    (40*4)              // Full height of FC buttons
#define FC_FULLHEIGHT   (20*3)                // Full width of FC buttons
#define FC_WIDTH        (FC_FULLWIDTH /4)   // Width of one of the skip type buttons
#define FC_HEIGHT       (FC_FULLHEIGHT/3)   // Height of one of the skip type buttons.

static LRESULT CALLBACK JWP_fcshape_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;
  HDC         hdcmem;       // for holding the bitmaps.
  HBITMAP     bitmap;
  int         i;
  switch (iMsg) {
//
//  During window creation, we have to adjust to the size of the window to match 
//  the size of the bitmap.
//
    case WM_CREATE:
         CREATESTRUCT *create;
         create = (CREATESTRUCT *) lParam;
         MoveWindow (hwnd,create->x,create->y,FC_FULLWIDTH+2*WIN_XEDGE,FC_FULLHEIGHT+2*WIN_YEDGE,true);
         return (0);
//
//  We render the display here.  The main display element is stored as a
//  complete bitmap in the resource, so we just need to render it.
//
    case WM_PAINT:
         hdc    = BeginPaint(hwnd,&ps);
         hdcmem = CreateCompatibleDC(hdc);
//       SetMapMode    (hdcmem,GetMapMode(hdc));                        // Not necessary
         bitmap = LoadBitmap(instance,MAKEINTRESOURCE(IDB_FCSHAPES));
         SelectObject (hdcmem,bitmap);
         BitBlt       (hdc,0,0,FC_FULLWIDTH,FC_FULLHEIGHT,hdcmem,0,0,SRCCOPY);
         DeleteObject (bitmap);
         DeleteDC     (hdcmem);
         EndPaint     (hwnd,&ps);
         return (0);
//
//  Mouse event, causes us to send a message to the parent to set the type.
//
    case WM_LBUTTONDOWN:
         i  =    HIWORD(lParam)/FC_HEIGHT;
         i += 3*(LOWORD(lParam)/FC_WIDTH);
         if (i <= 9) SendMessage(GetParent(hwnd),WM_COMMAND,IDC_FCSHAPES,i);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  begin class FOURCORNER_lookup
//

//--------------------------------
//
//  This routine checks the value in a location after a change.  This is used to generate 
//  the correct input data for the spinners, ie, it adds the blank at the position 10.
//
//      id -- Control to check.
//
void FOURCORNER_lookup::check_value (int id) {
  int i;
  i = GetDlgItemInt(dialog,id,NULL,false);
  if (i == 10) set_value (id,10);
  return;
}

//--------------------------------
//
//  Dailog box handler for the radical lookup dailog box.  This does all
//  the control work here.
//
int FOURCORNER_lookup::dlg_fclookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  int i;
  switch (iMsg) {
    case WM_INITDIALOG:                         // Intialize, so chache dialog box pointer.
         for (i = IDC_FC1SPIN; i <= IDC_FC5SPIN; i += 2) SendDlgItemMessage (hwnd,i,UDM_SETRANGE,0,MAKELONG(10,0));
         initialize      (hwnd,true,IDC_FC1);
         POCKETPC_DIALOG (hwnd);
         return          (false);
    case WM_DESTROY:
         remove_dialog (hwnd);
         delete this;
         fc_lookup = NULL;
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_FCLOOKUP);
         return  (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           case IDC_FCSHAPES:
                set_value (current,lParam);
                current += 2;
                if (current > IDC_FC5) current = IDC_FC1;
                SetFocus (GetDlgItem(hwnd,current));
                break;
           case IDC_FC1:
           case IDC_FC2:
           case IDC_FC3:
           case IDC_FC4:
           case IDC_FC5:
                current = LOWORD(wParam);
                input_check (hwnd,wParam);
                if (HIWORD(wParam) != EN_CHANGE) {
                  check_value (LOWORD(wParam));
                  return (true);
                }
                break;
           default:
                break;
         }
         return (command(wParam));
  }
  return (false);       // Let the dialog box handler handle the standard
}                                                   //   buttons and actions.    

//--------------------------------
//
//  Resets the entire dialog box to it's default state.  This is
//  the Clear Button, it is also used to initialize the dalog box.
//
void FOURCORNER_lookup::reset () {
  int i;
  current = IDC_FC1;
  for (i = IDC_FC1; i <= IDC_FC5; i += 2) set_value (i,10);
  clear_results ();
  return;
}

//--------------------------------
//
//  This does the actual search.
//
void FOURCORNER_lookup::search () {
  int i,j,err,limits[5][2];
  KANJI_info kanji_info;        // Class used to access the kanji information database
//
//  Get search parameters.
//
  for (i = 0; i < 5; i++) {
    j = GetDlgItemInt(dialog,IDC_FC1+2*i,&err,false);
    if (!err) { limits[i][0] = 0; limits[i][1] = 9; } else { limits[i][0] = limits[i][1] = j; }
  }
//
//  Can we get kanji data?
//
  if (kanji_info.open_info(dialog)) return;             // Open info file.
//
//  Do the search.
//
  for (i = MIN_KANJI1; i <= MAX_KANJI1; i += 0x100) {
    for (j = MIN_KANJI2; j <= MAX_KANJI2; j++) {
//
//  Check for valid matching code.
//
      kanji_info.get_info (i|j,INFO_EXTEND);            // Get information for this kanji.
      if (test(limits,kanji_info.extend.fc_main,kanji_info.extend.fc_index)) {
        put_kanji (i|j);
        put_kanji ('/');
      }
      else if (test(limits,kanji_info.fc_main2,kanji_info.extend.fc_index2)) {
        put_kanji (i|j);
        put_kanji ('/');
      }
    }
  }
//
//  Cleanup
//
  kanji_info.close_info ();                             // Close the info database.
  return;
}

//--------------------------------
//
//  Sets a value in one of the corner boxes.  A value of 10 is wild.
//
//      id    -- ID of the box to change
//      value -- Value to set (a value of 10 is set as blank).
//
void FOURCORNER_lookup::set_value (int id,int value) {
  adjusting = true;
  if (value == 10) SetDlgItemText (dialog,id,TEXT("")); else SetDlgItemInt (dialog,id,value,false);
  adjusting = false;
}

//--------------------------------
//
//  This routine test to see if a four-corners code fits in the range selected by the user.
//
//      limits -- Array of limits (corners 1-5)(low-high)
//      main   -- Main part of the number (decimal format).
//      index  -- 5th corner.
//
//      RETURN -- return is true if the value is acceptable.
//
int FOURCORNER_lookup::test (int limits[5][2],int main,int index) {
  int i,j;
  if (main == -1) return (false);
  for (i = 3; i >= 0; i--) {
    j = main%10;
    if ((limits[i][0] > j) || (limits[i][1] < j)) return (false);
    main /= 10;
  }
  if ((limits[4][0] > index) || (limits[4][1] < index)) return (false);
  return (true);
}

//-------------------------------------------------------------------
//
//  Stub routine for entry
//

//--------------------------------
//
//  Generailly, I do not like friend fucntions, but I need an entry point,
//  and I wanted all the routines in this file.
//
//      file -- Context from which we were called so we can past text boack
//
void fourcorner_lookup (JWP_file *file) {
  if (fc_lookup) { SetForegroundWindow (fc_lookup->dialog); return; }
  if (!(fc_lookup = new FOURCORNER_lookup)) { OutOfMemory (file->window); return; }
  JCreateDialog (IDD_FCLOOKUP,file->window,(DLGPROC) dialog_fclookup);
  return;
}

//===================================================================
//
//  Class HS_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class HS_lookup definition.
//
//  This class is dirived from the KANJI_lookup class and process the 
//  SKIP lookup function.
//
//  This search engine uses a non-standard coding.  Unlike almost all 
//  other searches, the value of 0 is needed for the four-corners system.
//  The corner boxes, thus use the value of 10 as a wild card.  
//

typedef class HS_lookup : public RADSTROKE_lookup {
friend void spahn_lookup (JWP_file *file);
public:
  HS_lookup  (void);                // Constructor (required).
  int     dlg_hslookup   (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
private:                            // Required procedures:
  void   search (void);             // Execute the actual search.
} HS_lookup;

//-------------------------------------------------------------------
//
//  static data and definitions.
//

static HS_lookup *hs_lookup = NULL;     // Global pointer for the skip lookup chart.

#define MAX_RADSTROKES  11              // Number of radical strokes that are possible
#define MAX_HSSTROKES   26              // Max number for other stokes
#define MAX_HSKANJI     47              // Maximum number of kanji for other strokes.
#define MAX_HSRADICALS  19              // Maximum radicla value (used only for searching)

//--------------------------------
//
//  This array indicates all the major parameters associated with the radica data.   For each
//  possible number of radical strokes, five numbers are specifiec.  Note that a final number 
//  is used to terminate the array.  The number are:
//
//      0. Number of buttons with varaitions
//      1. Number of buttons without varaitions
//      2. Number of radials with this number of strokes
//      3. Index directly into radicals bitmap for this list.  This is used with variations.
//      4. Index into radical list used with no variations to draw the radicals.
//
static byte hs_data[][6] = {
  { 115,79,   0, 0,  0 ,0                     },   // 0
  {   0, 0,   0, 0,  0 ,0                     },   // 1
  {  30,19,  19, 0,  0 ,0                     },   // 2
  {  27,18,  18,18,  30,19                    },   // 3
  {  22,13,  13,13,  57,19+18                 },   // 4
  {  10, 9,   9, 9,  79,19+18+13              },   // 5
  {   7, 6,   6, 6,  89,19+18+13+9            },   // 6
  {   6, 5,   5, 5,  96,19+18+13+9+6          },   // 7
  {  10, 5,   5, 5,  102,19+18+13+9+6+5       },   // 8
  {   1, 1,   1, 1,  112,19+18+13+9+6+5+5     },   // 9
  {   1, 1,   1, 1,  113,19+18+13+9+6+5+5+1   },   // 10
  {   2, 2,   2, 2,  114,19+18+13+9+6+5+5+1+1 },   // 11
  {   0, 0,   0, 0,  116,0                    },   // 12
};

//--------------------------------
//
//  Radical list,  The radicals are in order and include the variants.  This hops through 
//  the list just getting the main radicals.  Each line is a different stroke count, starting
//  at two.
//
static byte hs_radlist[] = {
    0,  3,  4,  6,  7,    8,11,12,13,14,   15,17,19,20,23,   24,27,28,29,
   30, 34, 36, 38, 40,   41,42,44,45,46,   47,48,49,52,53,   54,55,56,
   57, 58, 59, 60, 62,   64,66,67,68,71,   72,75,77,
   79, 80, 81, 82, 83,   85,86,87,88,
   89, 90, 91, 92, 93,   94,
   96, 97, 98, 99,101,
  102,103,106,107,110,
  112,
  113,
  114,115,
};

//--------------------------------
//
//  Assigns a unique number to each radical, based on the order of the radicals with variants.
//  The value go from 1 to 79.  A value of zero is used to indicate no selected radical.
//
static byte hs_radvalue[] = {
   1, 1, 1, 2, 3,    3, 4, 5, 6, 6,    6, 7, 8, 9,10,   11,11,12,12,13,   14,14,14,15,16,   16,16,17,18,19,
  20,20,20,20,21,   21,22,22,23,23,   24,25,26,26,27,   28,29,30,31,32,   32,32,33,34,35,   36,37,
  38,39,40,41,41,   42,42,43,43,44,   45,46,46,46,47,   48,48,48,49,49,   50,50,
  51,52,53,54,55,   55,56,57,58,59,
  60,61,62,63,64,   65,65,
  66,67,68,69,69,   70,
  71,72,72,72,73,   74,74,74,75,75,
  76,
  77,
  78,79,
};

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Dialog box procedure stub for the four-corner Lookup dialog box.
//
static BOOL CALLBACK dialog_hslookup (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (hs_lookup->dlg_hslookup(hwnd,message,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  begin class HS_lookup
//

//--------------------------------
//
//  Class constructor.  Needs to get access to the radical bit map and
//  and the cursor bitmap
//
HS_lookup::HS_lookup () {
  max_radstrokes = MAX_RADSTROKES;
  data           = hs_data;
  has_kanji      = true;
  radical_list   = hs_radlist;
  radical_value  = hs_radvalue;
  radical_bitmap = LoadBitmap(instance,MAKEINTRESOURCE(IDB_HSRADICALS));
  return;
}

//--------------------------------
//
//  Dailog box handler for the radical lookup dailog box.  This does all
//  the control work here.
//
int HS_lookup::dlg_hslookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  switch (iMsg) {
    case WM_INITDIALOG:                         // Intialize, so chache dialog box pointer.
         SendDlgItemMessage (hwnd,IDC_HSRADSRKSPIN,UDM_SETRANGE,0,MAKELONG(MAX_RADSTROKES,0));
         SendDlgItemMessage (hwnd,IDC_HSOTHERSPIN ,UDM_SETRANGE,0,MAKELONG(MAX_HSSTROKES ,0));
         SendDlgItemMessage (hwnd,IDC_HSKANJISPIN ,UDM_SETRANGE,0,MAKELONG(MAX_HSKANJI   ,0));
         CheckDlgButton     (hwnd,IDC_HSVARIANTS,!jwp_config.cfg.no_variants);
         initialize         (hwnd,true,IDC_HSRADSTROKE);
         POCKETPC_DIALOG    (hwnd);
         return             (false);
    case WM_DESTROY:
         remove_dialog (hwnd);
         delete this;
         hs_lookup = NULL;
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_HSLOOKUP);
         return  (true);
  }
  return (dlg_lookup (hwnd,iMsg,wParam,lParam));
}                                                   //   buttons and actions.    

//--------------------------------
//
//  This does the actual search.
//
void HS_lookup::search () {
  int i,j,s1,s2,r1,r2,o1,o2,k1,k2;
  KANJI_info kanji_info;        // Class used to access the kanji information database
//
//  Get search parameters.
//
  switch (current_stroke) {
    case 0:  s1 = 0; s2 = MAX_RADSTROKES; break;
    case 1:  s1 = s2 = 0; break;
    default: s1 = s2 = current_stroke; break;
  }
  r1 = get_radical();
  if (r1 == 0) r2 = MAX_HSRADICALS; 
    else {
      if (r1 < 'l'-'a'+1) r1--;
      r2 = r1;
    }
  get_value (IDC_HSOTHER,MAX_HSSTROKES,o1,o2);
  get_value (IDC_HSKANJI,MAX_HSKANJI  ,k1,k2);
//
//  Can we get kanji data?
//
  if (kanji_info.open_info(dialog)) return;             // Open info file.
//
//  Do the search.
//
  for (i = MIN_KANJI1; i <= MAX_KANJI1; i += 0x100) {
    for (j = MIN_KANJI2; j <= MAX_KANJI2; j++) {
//
//  Check for valid matching code.
//
      kanji_info.get_info (i|j,INFO_EXTEND);             // Get information for this kanji.
      if (((int) kanji_info.extend.sh_rstroke >= s1) && ((int) kanji_info.extend.sh_rstroke <= s2) &&
          ((int) kanji_info.extend.sh_radical >= r1) && ((int) kanji_info.extend.sh_radical <= r2) &&
          ((int) kanji_info.extend.sh_ostroke >= o1) && ((int) kanji_info.extend.sh_ostroke <= o2) &&
          ((int) kanji_info.extend.sh_index   >= k1) && ((int) kanji_info.extend.sh_index   <= k2)) {
        put_kanji (i|j);
        put_kanji ('/');
      }
    }
  }
//
//  Cleanup
//
  kanji_info.close_info ();                             // Close the info database.
  return;
}

//-------------------------------------------------------------------
//
//  Stub routine for entry
//

//--------------------------------
//
//  Generailly, I do not like friend fucntions, but I need an entry point,
//  and I wanted all the routines in this file.
//
//      file -- Context from which we were called so we can past text boack
//
void spahn_lookup (JWP_file *file) {
  if (hs_lookup) { SetForegroundWindow (hs_lookup->dialog); return; }
  if (!(hs_lookup = new HS_lookup)) { OutOfMemory (file->window); return; }
  JCreateDialog (IDD_HSLOOKUP,file->window,(DLGPROC) dialog_hslookup);
  return;
}

//===================================================================
//
//  Class INDEX_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class INDEX_lookup definition.
//
//  This class is dirived from the KANJI_lookup class and process the 
//  INDEX lookup function (searches for kanji base on dicitionary index).
//

typedef class INDEX_lookup : public KANJI_lookup {
friend void index_lookup (JWP_file *file);
public:
  int  dlg_indexlookup   (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
private:
  int    get_type        (void);                        // Get index type.
  void   reset           (void);                        // Reset the search engine.
  void   search          (void);                        // Execute the actual search.
} INDEX_lookup;

//-------------------------------------------------------------------
//
//  static data and definitions.
//

//
// Number of indexes aviable to the user based on the inforamtion database
//
#define INDEX_FIXED     4                                   // Indexes for minimal database
#define INDEX_EXTRA     6                                   // Indexes for fixed extra data
#define INDEX_ALL       (sizeof(indexes)/sizeof(TCHAR *))   // Indexes for full database

//
// Defintiion of the actual search indexes.
//
                                    // Searches in the base kanji-info file
#define ILTYPE_NELSON       0       // Modern Reader's Japanese-English Character Dictionary, Andrew Nelson
#define ILTYPE_HAIG         1       // The New Nelson Japanese-English Character Dictionary, John Haig
#define ILTYPE_HALPERN      2       // The New Japanese-English Character Dictionary, Jack Halpern
#define ILTYPE_GRADE        3       // Grade Level
                                    // Searches in the extended data set.
#define ILTYPE_MD_LONG      4       // Morohashi Daikanwajiten (full index)
#define ILTYPE_MD_SHORT     5       // Morohashi Daikanwajiten (volume index)
                                    // Searches in the fully expanded data set.
#define ILTYPE_HALPERNKLD   6       // Jack Halpern in his Kanji Learners Dictionary, published by Kodansha in 1999
#define ILTYPE_SH_KANA      7       // Kanji & Kana, Spahn and Hadamitzky
#define ILTYPE_HENSHALL     8       // A Guide To Remembering Japanese Characters, Kenneth G. Henshall
#define ILTYPE_GAKKEN       9       // A New Dictionary  of Kanji Usage, Gakken
#define ILTYPE_HEISIG       10      // Remembering The Kanji, James Heisig
#define ILTYPE_ONEILL       11      // Japanese Names, P. G. O'Neill
#define ILTYPE_ONEILLEK     12      // P.G. O'Neill's Essential Kanji (ISBN 0-8348-0222-8). 
#define ILTYPE_DEROO        13      // Father Joseph De Roo, and published in his book "2001 Kanji"
#define ILTYPE_FREQ         14      // Frequency-of-use ranking, Jack Halpern
#define ILTYPE_READWRITE    15      // "A Guide To Reading and Writing Japanese" edited by Florence Sakade. 
#define ILTYPE_TUTTLECARD   16      // The Tuttle Kanji Cards, compiled by Alexander Kask. 
#define ILTYPE_KANJIWAY     17      // "The Kanji Way to Japanese Language Power" by Dale Crowley. 
#define ILTYPE_KANJICONTEXT 18      // "Kanji in Context" by Nishiguchi and Kono. 
#define ILTYPE_BUSYPEOPLE   19      // "Japanese For Busy People" vols I-III, published by the AJLT. 
#define ILTYPE_COMPACTKANJI 20      // "Kodansha Compact Kanji Guide". 

//--------------------------------
//
//  Text used to describe the indexes.
//
static int indexes[] = { 
  IDS_IL_NELSON,IDS_IL_HAIG,IDS_IL_HALPERN,IDS_IL_GRADE,

  IDS_IL_MOROHASHILONG,IDS_IL_MOROHASHISHORT,

  IDS_IL_HALPERNKLD,IDS_IL_SPAHN    ,IDS_IL_HENSHALL,IDS_IL_GAKKEN      ,IDS_IL_HEISIG    ,IDS_IL_ONEILL      ,IDS_IL_ONEILLEK,IDS_IL_DEROO,IDS_IL_FREQUENCY,
  IDS_IL_READWRITE ,IDS_IL_TUTTLECARD,IDS_IL_KANJIWAY,IDS_IL_KANJICONTEXT,IDS_IL_BUSYPEOPLE,IDS_IL_COMPACTKANJI,
};

static INDEX_lookup *idx_lookup = NULL;     // Global pointer for the skip lookup chart.

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Dialog box procedure stub for the SKIP Lookup dialog box.
//
static BOOL CALLBACK dialog_indexlookup (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (idx_lookup->dlg_indexlookup(hwnd,message,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  begin class INDEX_lookup
//

//--------------------------------
//
//  Dailog box handler for the radical lookup dailog box.  This does all
//  the control work here.
//
int INDEX_lookup::dlg_indexlookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  int i,j;
  switch (iMsg) {
    case WM_INITDIALOG: {                           // Intialize, so chache dialog box pointer.
           KANJI_info kanji_info;                   // We need this to make sure we know what indexes can be used.
           kanji_info.open_info(dialog);;           //   Open info file.
           if      (jwp_config.kanji_flags & KIFLAG_EVAR ) j = INDEX_ALL;
           else if (jwp_config.kanji_flags & KIFLAG_EXTRA) j = INDEX_EXTRA;
           else                                            j = INDEX_FIXED;
           for (i = 0; i < j; i++) SendDlgItemMessage (hwnd,IDC_ILTYPE,CB_ADDSTRING,0,(LPARAM) get_string(indexes[i]));
           if (CB_ERR == SendDlgItemMessage(hwnd,IDC_ILTYPE,CB_SETCURSEL,jwp_config.cfg.index_type,0)) SendDlgItemMessage (hwnd,IDC_ILTYPE,CB_SETCURSEL,0,0);
           initialize      (hwnd,false,IDC_ILINDEX);
           POCKETPC_DIALOG (hwnd);
         }
         return (false);
    case WM_DESTROY:
         remove_dialog (hwnd);
         delete this;
         idx_lookup = NULL;
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_INDEXLOOKUP);
         return  (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
#ifdef WINCE_POCKETPC
           case IDOK:
#endif WINCE_POCKETPC
           case IDCANCEL:
                jwp_config.cfg.index_type = (byte) SendDlgItemMessage(hwnd,IDC_ILTYPE,CB_GETCURSEL,0,0);
                break;
           case IDC_ILTYPE:
                reset ();
                break;
           case IDC_ILVOL:
           case IDC_ILINDEX:
                input_check (hwnd,wParam);
                if (HIWORD(wParam) != EN_CHANGE) return (true);
                break;
           default:
                break;
         }
         return (command(wParam));
  }
  return (false);       // Let the dialog box handler handle the standard
}                                                   //   buttons and actions.    

//--------------------------------
//
//  This routine gets the current search index.  Remember the first character indicates the 
//  search index to be used.
//
int INDEX_lookup::get_type () {
  return (SendDlgItemMessage(dialog,IDC_ILTYPE,CB_GETCURSEL,0,0));
}

//--------------------------------
//
//  Resets the entire dialog box to it's default state.  This is
//  the Clear Button, it is also used to initialize the dalog box.
//
void INDEX_lookup::reset () {
  int i;
  SetDlgItemText (dialog,IDC_ILINDEX,TEXT(""));
  SetDlgItemText (dialog,IDC_ILVOL  ,TEXT(""));
  clear_results  ();
  i = get_type();
  EnableWindow   (GetDlgItem(dialog,IDC_ILVOL),(i == ILTYPE_MD_SHORT) || (i == ILTYPE_BUSYPEOPLE));
  return;
}

//--------------------------------
//
//  This does the actual search.
//
void INDEX_lookup::search () {
  static byte codes[] = { INFO_FIXED,INFO_FIXED,INFO_FIXED,INFO_FIXED, INFO_EXTEND,INFO_EXTEND, INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL,INFO_ALL };
  int code,type,index,vol,i,j;
  KANJI_info kanji_info;        // Class used to access the kanji information database
//
//  Get search parameters.
//
  type  = get_type ();
  code  = codes[type];
  index = GetDlgItemInt(dialog,IDC_ILINDEX,NULL,false);
  vol   = GetDlgItemInt(dialog,IDC_ILVOL  ,NULL,false);
  if (type == ILTYPE_BUSYPEOPLE) index = ((vol << 8) | index);          // Busy people is encoded as upper 8 bits is volume, and lower byte is chapter.
//
//  Can we get kanji data?
//
  if (kanji_info.open_info(dialog)) return;             // Open info file.
//
//  Do the search.
//
  for (i = MIN_KANJI1; i <= MAX_KANJI1; i += 0x100) {
    for (j = MIN_KANJI2; j <= MAX_KANJI2; j++) {
//
//  Check for valid matching code.
//
      kanji_info.get_info (i|j,code);                   // Get information for this kanji.
      switch (type) {
        case ILTYPE_NELSON:          // Modern Reader's Japanese-English Character Dictionary, Andrew Nelson
             if (kanji_info.kinfo.nelson != index) continue;
             break;
        case ILTYPE_HAIG:            // The New Nelson Japanese-English Character Dictionary, John Haig
             if (kanji_info.kinfo.haig != index) continue;
             break;
        case ILTYPE_HALPERN:         // The New Japanese-English Character Dictionary, Jack Halpern
             if (kanji_info.kinfo.halpern != index) continue;
             break;
        case ILTYPE_GRADE:           // Grade Level
             if (kanji_info.kinfo.grade != index) continue;
             break;
        case ILTYPE_MD_LONG:         // Morohashi Daikanwajiten (full index)
             if (kanji_info.extend.md_long != index) continue;
             break;
        case ILTYPE_MD_SHORT:        // Morohashi Daikanwajiten (volume index)
             if (((int) kanji_info.extend.md_short1 != vol) || ((int) kanji_info.extend.md_short2 != index)) continue;
             break;
        case ILTYPE_SH_KANA:         // Kanji & Kana, Spahn and Hadamitzky
             if (kanji_info.sh_kana != index) continue;
             break;
        case ILTYPE_HENSHALL:        // A Guide To Remembering Japanese Characters, Kenneth G. Henshall
             if (kanji_info.henshall != index) continue;
             break;
        case ILTYPE_GAKKEN:          // A New Dictionary  of Kanji Usage, Gakken
             if (kanji_info.gakken != index) continue;
             break;
        case ILTYPE_HEISIG:          // Remembering The Kanji, James Heisig
             if (kanji_info.heisig != index) continue;
             break;
        case ILTYPE_ONEILL:          // Japanese Names, P. G. O'Neill
             if (kanji_info.oneill != index) continue;
             break;
        case ILTYPE_FREQ:            // Frequency-of-use ranking, Jack Halpern
             if (kanji_info.freq != index) continue;
             break;
        case ILTYPE_DEROO:          // Father Joseph De Roo, and published in his book "2001 Kanji"
             if (kanji_info.deroo != index) continue;
             break;
        case ILTYPE_ONEILLEK:       // P.G. O'Neill's Essential Kanji (ISBN 0-8348-0222-8). 
             if (kanji_info.oneill_ek != index) continue;
             break;
        case ILTYPE_HALPERNKLD:     // Jack Halpern in his Kanji Learners Dictionary, published by Kodansha in 1999
             if (kanji_info.halpern_kld != index) continue;
             break;
        case ILTYPE_READWRITE:      // "A Guide To Reading and Writing Japanese" edited by Florence Sakade. 
             if (kanji_info.readwrite != index) continue;
             break;
        case ILTYPE_TUTTLECARD:     // The Tuttle Kanji Cards, compiled by Alexander Kask. 
             if (kanji_info.tuttlecard != index) continue;
             break;
        case ILTYPE_KANJIWAY:       // "The Kanji Way to Japanese Language Power" by Dale Crowley. 
             if (kanji_info.kanjiway != index) continue;
             break;
        case ILTYPE_KANJICONTEXT:   // "Kanji in Context" by Nishiguchi and Kono. 
             if (kanji_info.kanjicontext != index) continue;
             break;
        case ILTYPE_BUSYPEOPLE:     // "Japanese For Busy People" vols I-III, published by the AJLT. 
             if (kanji_info.busypeople != index) continue;
             break;
        case ILTYPE_COMPACTKANJI:   // "Kodansha Compact Kanji Guide". 
             if (kanji_info.kanjiguide != index) continue;
             break;
      }
      put_kanji (i|j);
      put_kanji ('/');
    }
  }
//
//  Cleanup
//
  kanji_info.close_info ();                             // Close the info database.
  return;
}

//-------------------------------------------------------------------
//
//  Stub routine for entry
//

//--------------------------------
//
//  Generailly, I do not like friend fucntions, but I need an entry point,
//  and I wanted all the routines in this file.
//
//      file -- Context from which we were called so we can past text boack
//
void index_lookup (JWP_file *file) {
  if (idx_lookup) { SetForegroundWindow (idx_lookup->dialog); return; }
  if (!(idx_lookup = new INDEX_lookup)) { OutOfMemory (file->window); return; }
  JCreateDialog (IDD_INDEXLOOKUP,file->window,(DLGPROC) dialog_indexlookup);
  return;
}

//===================================================================
//
//  Class RADICAL_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class RADICAL_lookup definition.
//
//  This class is dirived from the KANJI_lookup class and process the 
//  radical lookup function.
//

typedef class RADICAL_lookup : public KANJIRAD_lookup {
friend void radical_lookup (JWP_file *file);            // This is the external entry point!
public:
  inline RADICAL_lookup  (int count) : KANJIRAD_lookup (count,false) { return; }
  int  dlg_radlookup     (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
private:                                                // Required procedures:
  void   reset           (void);                        // Reset the search engine.
  void   search          (void);                        // Execute the actual search.
private:
  int    get_data        (int ch,KANJI *buffer);        // Get information from a dialog, based on a radical or count.
  HANDLE open            (TCHAR *filename,int message); // Open a file, (data or index).
  HANDLE data;                                          // Handle for data file.
} RADICAL_lookup;

//-------------------------------------------------------------------
//
//  static data and definitions.
//

static RADICAL_lookup *rad_lookup = NULL;   // This is a big kludge, we use this to indicate 
                                            //    where the dialog box procedures should call back
                                            //    to.  This should really be changed to a value 
                                            //    passed into the dialog procedure using DailogBoxParam().
                                            //    Then the value could be saved as a window proc.  This 
                                            //    is nice, but, since this can only be called once, this 
                                            //    is okay for now, but a kluge.  ###

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Dialog box procedure stub for the Radical Lookup dialog box.
//
static BOOL CALLBACK dialog_radlookup (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (rad_lookup->dlg_radlookup(hwnd,message,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  begin class RADICAL_lookup
//

//--------------------------------
//
//  Dailog box handler for the radical lookup dailog box.  This does all
//  the control work here.
//
int RADICAL_lookup::dlg_radlookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  switch (iMsg) {
    case WM_INITDIALOG:                         // Intialize, so chache dialog box pointer.
         initialize      (hwnd,true,IDC_RLRADICALS);
         POCKETPC_DIALOG (hwnd);
         return          (false);
    case WM_DESTROY:
         remove_dialog (hwnd);
         delete this;
         rad_lookup = NULL;
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_RADLOOKUP);
         return  (true);
//
//  This really processes the up down buttons.  These could be automatically processed, but 
//  we want these buttons to be smart.  If you select some raticals, say that combine to 11
//  strokes, the smart sequence would be:
//
//          0, 11, 12, 13, ...  30, 0
//
//  The values 1-10 are skipped, because these will always lead to no matches.
//
    case WM_NOTIFY:
         if (wParam == IDC_RLSPIN) do_spinner ((NMUPDOWN *) lParam,IDC_RLSTROKE);
         return (0);
//
//  Process controls.
//
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
//
//  This next command is a fake message passed from the RADKANJI_lookup radicals window
//  procedure.  This message indicate the user has pasted a kanji into the radicals window
//  and wants us to find all the radicals in the kanji.
//
           case IDC_RLCOPYKANJI: {
                  int       i,j,k;
                  KANJI     ch,buffer[3000];
                  JWP_file *paste;
                  if (!(paste = get_paste (hwnd))) { MessageBeep (MB_ICONEXCLAMATION); return (true); }
                  ch = (paste->edit_gettext()[0]);                  // Get character from clipboard
                  reset ();                                         // Clear the search
                  if (INVALID_HANDLE_VALUE == (data = open(NAME_RADRADICAL,IDS_RL_ERRORRADINDEX))) return (true);
                  for (i = 0; i <= BUTTON_COUNT; i++) {             // Scan though all radicals
                    j = get_data (i,buffer);                        // Get kanji for this radical
                    for (k = 0; k < j; k++) {
                      if (buffer[k] == ch) break;                   // Is this kanji in this radical
                    }
                    if (k != j) set_radical (i);                    // activate radical
                  }
                  CloseHandle (data);             // Close files.
                }
                break;
           case IDC_RLPM2:                      // +/-2 so clear +/-1 and >
                if (IsDlgButtonChecked(hwnd,IDC_RLPM2)) {
                  CheckDlgButton (hwnd,IDC_RLPM1,false);
                }
                break;
           case IDC_RLPM1:                      // +/-1 so clear +/-2 and >
                if (IsDlgButtonChecked(hwnd,IDC_RLPM1)) {
                  CheckDlgButton (hwnd,IDC_RLPM2,false);
                }
                break;
           case IDC_RLSTROKE:                   // A number of strokes
                input_check (hwnd,wParam);
                if (HIWORD(wParam) != EN_CHANGE) return (true);
                CheckDlgButton (hwnd,IDC_RLANY,GetDlgItemInt(hwnd,IDC_RLSTROKE,NULL,false) == 0);
                break;
           default:
                break;
         }
         return (command(wParam));
  }
  return (false);       // Let the dialog box handler handle the standard
}                                                   //   buttons and actions.    

//--------------------------------
//
//  Get the kanji information form the curretnly open index/data file 
//  pair.  Depeniding on the index and data file pair open, this will
//  either get a list of all kanji containning a specific radical, or
//  will get a list of all kanji with a specific number of strokes.
//
//      ch     -- Indicate the radical number (one based), or the stroke 
//                count (1 based) or the list to obtain.
//      buffer -- Location to place the kanji read from the list.
//
//      RETURN -- Number of kanji read into the list.
//
int RADICAL_lookup::get_data (int ch,KANJI *buffer) {
  unsigned long done;
  struct radical_data {
    unsigned short offset;
    short count;
  } rad_data;
  ch--;                         // Convert index to zero based as is used in the database.
  SetFilePointer (data,sizeof(radical_data)*ch,NULL,FILE_BEGIN);
  ReadFile       (data,&rad_data,sizeof(radical_data),&done,NULL);
  SetFilePointer (data,rad_data.offset,NULL,FILE_BEGIN);
  ReadFile       (data,buffer,rad_data.count*sizeof(KANJI),&done,NULL);
  return (rad_data.count);
}

//--------------------------------
//
//  Opens a file for use in the radical lookup system.  Basically, this
//  is called for the radical and stroke index and data files.  This
//  routine handles all the error processing associated with the message.
//  The following routine, however, must check the state of the returned
//  handle to see if it is really open.
//
//      filename -- Name of file to open.
//      message  -- ID of description of the file used in case of an error.
//
//      RETURN   -- Handle to the file.
//
HANDLE RADICAL_lookup::open (TCHAR *filename,int message) {
  HANDLE handle;
  handle = jwp_config.open(filename,OPEN_READ,false);
  if (handle == INVALID_HANDLE_VALUE) {
    JMessageBox (dialog,IDS_RL_ERRORTEXT,IDS_RL_ERROR,MB_OK | MB_ICONWARNING,get_string(message),jwp_config.name());
  }
  return (handle);
}

//--------------------------------
//
//  Resets the entire dialog box to it's default state.  This is
//  the Clear Button, it is also used to initialize the dalog box.
//
void RADICAL_lookup::reset () {
  CheckDlgButton (dialog,IDC_RLANY,true);
  CheckDlgButton (dialog,IDC_RLPM1,false);
  CheckDlgButton (dialog,IDC_RLPM2,false);
  set_value      (IDC_RLSTROKE,0);
  reset_radicals ();
  clear_results  ();                    // Clear results bar
  return;
}

//--------------------------------
//
//  This is a the heart of the search routine.  This routine actually 
//  performs the search and puts the results in the buffer.  The routine
//  gets the stoke count information directly, and gets the radical 
//  selections from the states[] array.  The return is place the kanji
//  (in display format) in the display buffer (list) and setting the 
//  list counter (list_len). 
//
//  You can determine the number of matches generated by dividing list_len 
//  by 2, since each entry is just a kanji in this case and a separator.  
//
#define NUMBER_FALSE    (sizeof(false_radicals)/sizeof(byte))

void RADICAL_lookup::search () {
  KANJI kbuffer[3000];      // Buffer for the kanji we want to keep.
  byte  strokes[3000];      // Stroke count for the kanji we keep.
  int cmin,cmax;            // Indicates the min and max counts for the search.
  int first = true;         // Indicates this is the first line to be read.
  int count = 0;            // Number of kanji in the list (some may be 0, indicated rejected).
  int count2;               // Number of kanji in subsequent kanji lists.
  int nocount;              // Indicates that count is unbounded.
  int i,j,k;                
  KANJI_info kanji_info;    // Classe used to access the kanji information database
//
//  Determine the min and max limits for the kanji counts.
//
  if (IsDlgButtonChecked(dialog,IDC_RLANY)) {
    cmin = 0;
    cmax = 30;
  }
  else {
    cmin = cmax = GetDlgItemInt(dialog,IDC_RLSTROKE,NULL,false);
    if (IsDlgButtonChecked(dialog,IDC_RLPM1    )) { cmin -= 1; cmax += 1; }
    if (IsDlgButtonChecked(dialog,IDC_RLPM2    )) { cmin -= 2; cmax += 2; }
  }
  if (cmin < 1 ) cmin = 1;                  // Clip values.
  if (cmax > 30) cmax = 30;
  nocount = ((cmin <= 1) && (cmax >= 30));  // Determine if we have no count values. 
//
//  Begin the search by searching for kanji that match the radicals 
//  selected by the user.
//
  if (INVALID_HANDLE_VALUE == (data = open(NAME_RADRADICAL,IDS_RL_ERRORRADINDEX))) return;
  for (i = 1; i <= BUTTON_COUNT; i++) {                 // Step through the radicals
    if (!states[i]) continue;                           // This radical is not selected
    if (!(count2 = get_data(i,list))) continue;         // This is a false radical (the real radical will come up)
    if (first) {                                        // First radical with real data.
      first = false;
      count = get_data(i,kbuffer);                      // Relad because it is easier than coping
    }
    else {                                              // Second(+) radical, so do a select
      for (j = 0; j < count; j++) {
        if (!kbuffer[j]) continue;                      //   This one's alread been dropped.
        for (k = 0; k < count2; k++) if (list[k] == kbuffer[j]) goto KeepThisOne;
        kbuffer[j] = 0;                                 //    cannot find this one so drop it.
KeepThisOne:;
      }
    }
  }
  CloseHandle (data);                                   // Close files.
//
//  Now we need to do selection based on the stroke count.
//
//  If !first, this means that we have radicals that the user selected.
//  Thus we can have a list of kanji and need to know if they are to be 
//  kept.
//
  if (!first) {
    if (!nocount) {
      if (kanji_info.open_info(dialog)) return;         // Open info file.
      for (i = 0; i < count; i++) {                   
        if (!kbuffer[i]) continue;                      // This one is already gone.
        j = kanji_info.get_stroke (kbuffer[i]);         // Get stroke count for this kanji.
        if ((j < cmin) || (j > cmax)) kbuffer[i] = 0;
          else strokes[i] = (byte) j;                   // Do we reject this kanji based on the stoke count.
      }
      kanji_info.close_info ();                         // Close the info database.
    }
  }
//
//  If we got here, the user has not selected any radicals, but rather 
//  has selected some number of stroke counts, and wants kanji by stroke
//  count.  This is done by concatinating kanji in order of strokes.
//
  else {
    if (INVALID_HANDLE_VALUE != (data = open(NAME_RADSTROKE,IDS_RL_ERRORSTRINDEX))) {
      for (i = cmin; i <= cmax; i++) {
        if (count > size/2) break;                    // This lets us out if the user asks for too much.  
        if (first) {                                  // First list so just read it in.
          count = get_data(i,kbuffer);
          first = false;
        }
        else {                                        // Second or more so concatenate.
          count += get_data(i,kbuffer+count);
        }
      }
    }
    if (count > size) count = size;
    CloseHandle (data);                                 // Close files.
  }
//
//  Now put the the data we have collecte back into the display buffer.
//
  for (i = 0; i < count; i++) {
    if (!kbuffer[i]) continue;      // We can use this if we have a presorted list.
    put_kanji (kbuffer[i]);
    put_kanji ('/');
  }
  return;
}

//-------------------------------------------------------------------
//
//  Stub routine for entry
//

//--------------------------------
//
//  Generailly, I do not like friend fucntions, but I need an entry point,
//  and I wanted all the routines in this file.
//
//      file -- Context from which we were called so we can past text boack
//
void radical_lookup (JWP_file *file) {
  if (rad_lookup) { SetForegroundWindow (rad_lookup->dialog); return; }
  if (!(rad_lookup = new RADICAL_lookup(3000))) { OutOfMemory (file->window); return; }
  JCreateDialog (IDD_RADICALLOOKUP,file->window,(DLGPROC) dialog_radlookup);
  return;
}

//===================================================================
//
//  Class READING_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class READING_lookup definition.
//
//  This class is dirived from the KANJI_lookup class and process the 
//  READING lookup function (searches for kanji base on readings).
//

typedef class READING_lookup : public KANJI_lookup {
friend void reading_lookup (JWP_file *file);
public:
  int  dlg_readinglookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
private:
  int    get_type        (void);                        // Get index type.
  void   reset           (void);                        // Reset the search engine.
  void   search          (void);                        // Execute the actual search.
} READING_lookup;

//-------------------------------------------------------------------
//
//  static data and definitions.
//

//--------------------------------
//
// Defintiion of the actual search indexes.
//
#define RLTYPE_ON       0       // on-yomi
#define RLTYPE_KUN      1       // kun-yomi
#define RLTYPE_KUNON    2       // kun or on
#define RLTYPE_MEANING  3       // meaning
#define RLTYPE_NANORI   4       // nanonri
#define RLTYPE_PINYIN   5       // pinyin
#define RLTYPE_KOREAN   6       // korean

//--------------------------------
//
//  Text used to describe the indexes.
//
#define READINGS_FIXED      ((int) (sizeof(readings)/sizeof(TCHAR *)))

static int readings[] = { IDS_RL_ON,IDS_RL_KUN,IDS_RL_ONKUN,IDS_RL_MEANING };

static READING_lookup *read_lookup = NULL;  // Global pointer for the skip lookup chart.

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Dialog box procedure stub for the READING Lookup dialog box.
//
static BOOL CALLBACK dialog_readinglookup (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (read_lookup->dlg_readinglookup(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  This routine is used to test an ascii string for a match.
//
//      string -- String that is being searched for.
//      buffer -- This is the string being searched for a match.
//      frags  -- If set, this allows fragments of a word to be matched.
//
//      RETURN -- A return value of true indicates a match is found.  A value 
//                of false indicates no match.
//
static int test_ascii (byte *string,byte *buffer,int frags) {
  int i;
  if (!buffer) return (false);                          // No buffer, no match
  do {
    for (i = 0; buffer[i] && string[i] && (tolower(buffer[i]) == (byte) string[i]); i++);
    if (!string[i]) {                                   // Reached end of string, so could be a match.
      if      (frags) return (true);                    // Frags is on this is a machine, otherwise check end chars
      else if (!buffer[i] || (buffer[i] == ',') || (buffer[i] == ' ')) return (true);
    }
    if (frags) buffer++;                                // Match failed so advance to the next possible match
      else {
        while (*buffer && (*buffer != ' ')) buffer++;
        if (*buffer) buffer++;
      }
  } while (*buffer);
  return (false);
}

//--------------------------------
//
//  Tests an kana string for comparison with a key.
//
//      string -- String that is being searched for.
//      buffer -- This is the string being searched for a match.
//      frags  -- If set, this allows fragments of a word to be matched.
//
//      RETURN -- A return value of true indicates a match is found.  A value 
//                of false indicates no match.
//
static int test_kana (byte *string,byte *buffer,int flex) {
  int i;
  int mask = (flex ? 0x7f : 0xff);
  do {
    for (i = 0; buffer[i] && string[i] && ((mask & buffer[i]) == (mask & string[i])); i++);
    if (!string[i]) {
      if (!buffer[i] || (buffer[i] & 0x80)) return (true);
      if (flex && (buffer[i] == 0x1f)) return (true);
    }
  } while (*buffer++ == 0x1f);
  return (false);
}

//-------------------------------------------------------------------
//
//  begin class READING_lookup
//

//--------------------------------
//
//  Dailog box handler for the radical lookup dailog box.  This does all
//  the control work here.
//
//  It is expected that this routine is intialized with a call to CreateDialogParam.
//  The parameter should be a JWP_file object that is used to initizlie the 
//  search string edit box.  This is more effectient that getting it from the file_list,
//  and prevents problems with the order of initlization.
//
int READING_lookup::dlg_readinglookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  int i;
  switch (iMsg) {
    case WM_INITDIALOG:                         // Intialize, so chache dialog box pointer.
         SendDlgItemMessage (hwnd,IDC_RLSPIN,UDM_SETRANGE,0,MAKELONG(MAX_STROKES,0));
         for (i = 0; i < READINGS_FIXED; i++) SendDlgItemMessage (hwnd,IDC_RLTYPE,CB_ADDSTRING,0,(LPARAM) get_string(readings[i]));
         if (jwp_config.kanji_flags & KIFLAG_NANORI) SendDlgItemMessage (hwnd,IDC_RLTYPE,CB_ADDSTRING,0,(LPARAM) get_string(IDS_RL_NANORI));
         if (jwp_config.kanji_flags & KIFLAG_PINYIN) SendDlgItemMessage (hwnd,IDC_RLTYPE,CB_ADDSTRING,0,(LPARAM) get_string(IDS_RL_PINYIN));
         if (jwp_config.kanji_flags & KIFLAG_KOREAN) SendDlgItemMessage (hwnd,IDC_RLTYPE,CB_ADDSTRING,0,(LPARAM) get_string(IDS_RL_KOREAN));
         if (CB_ERR == SendDlgItemMessage(hwnd,IDC_RLTYPE,CB_SETCURSEL,jwp_config.cfg.reading_type,0)) SendDlgItemMessage (hwnd,IDC_RLTYPE,CB_SETCURSEL,0,0);
         CheckDlgButton     (hwnd,IDC_RLKUN ,jwp_config.cfg.reading_kun );
         CheckDlgButton     (hwnd,IDC_RLWORD,jwp_config.cfg.reading_word);
         initialize         (hwnd,false,IDC_RLSTRING);
         SendDlgItemMessage (hwnd,IDC_RLSTRING,JE_LOAD,0,lParam);           // Since intialize will call reset, order is importaint.
         exclude = (JWP_file *) SendDlgItemMessage(hwnd,IDC_RLSTRING,JE_GETJWPFILE,0,0);
         POCKETPC_DIALOG (hwnd);
         return          (false);
    case WM_DESTROY:
         remove_dialog (hwnd);
         delete this;
         read_lookup = NULL;
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_READLOOKUP);
         return  (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
#ifdef WINCE_POCKETPC
           case IDOK:
#endif WINCE_POCKETPC
           case IDCANCEL:
                jwp_config.cfg.reading_type = (byte) SendDlgItemMessage(hwnd,IDC_RLTYPE,CB_GETCURSEL,0,0);
                jwp_config.cfg.reading_kun  =        IsDlgButtonChecked(hwnd,IDC_RLKUN );
                jwp_config.cfg.reading_word =        IsDlgButtonChecked(hwnd,IDC_RLWORD);
                break;
           case IDC_RLSTROKE:
                input_check (hwnd,wParam);
                if (HIWORD(wParam) != EN_CHANGE) return (true);
                break;
           default:
                break;
         }
         return (command(wParam));
  }
  return (false);       // Let the dialog box handler handle the standard
}                                                   //   buttons and actions.    

//--------------------------------
//
//  This routine gets the current search index.  Remember the first character indicates the 
//  search index to be used.
//
int READING_lookup::get_type () {
  int   i;
  TCHAR buffer[SIZE_BUFFER];
  i = SendDlgItemMessage(dialog,IDC_RLTYPE,CB_GETCURSEL,0,0);
  if (i <= RLTYPE_MEANING) return (i);
  SendDlgItemMessage(dialog,IDC_RLTYPE,CB_GETLBTEXT,i,(WPARAM) buffer);
  if (!lstrcmp(buffer,get_string(IDS_RL_NANORI))) return (RLTYPE_NANORI);
  if (!lstrcmp(buffer,get_string(IDS_RL_PINYIN))) return (RLTYPE_PINYIN);
  if (!lstrcmp(buffer,get_string(IDS_RL_KOREAN))) return (RLTYPE_KOREAN);
  return (RLTYPE_ON);
}

//--------------------------------
//
//  Resets the entire dialog box to it's default state.  This is
//  the Clear Button, it is also used to initialize the dalog box.
//
void READING_lookup::reset () {
  SendDlgItemMessage (dialog,IDC_RLSTRING,JE_SETTEXT,0,(LPARAM) NULL);
  clear_results      ();
  return;
}

//--------------------------------
//
//  This does the actual search.
//
void READING_lookup::search () {
  byte      *ptr,*pin,*pout,astring[SIZE_BUFFER];
  KANJI     *kptr;
  int        frags,kun,length,type,s1,s2,i,j,k;
  KANJI_info kanji_info;        // Class used to access the kanji information database
  static byte a_tones[] = { 'a',225,226,224 };
  static byte e_tones[] = { 'e',233,234,232 };
  static byte i_tones[] = { 'i',237,238,236 };
  static byte o_tones[] = { 'o',243,244,242 };
  static byte u_tones[] = { 'u',250,251,249 };
//
//  Get search parameters.
//
  type   = get_type ();
  length = JEGetDlgItemText(dialog,IDC_RLSTRING,&kptr);
  frags  = IsDlgButtonChecked(dialog,IDC_RLWORD);
  kun    = IsDlgButtonChecked(dialog,IDC_RLKUN);
  get_value (IDC_RLSTROKE,MAX_STROKES,s1,s2);
//
//  Convert the strings to working format.
//
//  Ascii based search strings.
//
  if ((type == RLTYPE_PINYIN) || (type == RLTYPE_KOREAN) || (type == RLTYPE_MEANING)) {
    for (ptr = astring, i = 0; i < length; i++) {               // Convert to UTF-8 string
      if (ISJIS(kptr[i])) {
        JMessageBox (dialog,IDS_RL_ERRORKANAFOUND,IDS_RL_ERRORKANA,MB_OK | MB_ICONERROR);
        SetFocus    (GetDlgItem(dialog,IDC_RLSTRING));
        return;
      }
      if (type == RLTYPE_MEANING) jis2utf (ptr,tolower(kptr[i])); else *ptr++ = tolower(kptr[i]);
    }
    *ptr = 0;
    if (type == RLTYPE_PINYIN) {                                // For pinyin expand the tone values.
      for (pin = pout = astring; *pin; pin++, pout++) {
        i = *pin;
        if ((pin[1] >= '0') && (pin[1] <= '9')) {
          j = (*++pin)-'0';
          if (j > 4) j = 0;
          switch (i) {
            case 'a': i = a_tones[j]; break;
            case 'e': i = e_tones[j]; break;
            case 'i': i = i_tones[j]; break;
            case 'o': i = o_tones[j]; break;
            case 'u': i = u_tones[j]; break;
            default:                  break;
          }
        }
        *pout = i;
      }
      *pout = 0;
    }
  }
//
//  Processing for kana based searches.
//
//  This reverces the normal process used in the kanji-info data base.  This means the following
//  mappings:
//
//      -      ->  0x1f
//      (      ->  set the high bit on the next character
//      )      ->  valid only after (, and generates no output.
//      kana   ->  only the lower byte is kept
//      other  ->  error
//
  else {
    int okurigana = false;
    int mask      = 0;
    for (ptr = (byte *) astring, i = 0; i < length; i++) {
      if      ((kptr[i] == KANJI_DASH) || (kptr[i] == KANJI_LONGVOWEL)) *ptr++ = 0x1f;
      else if ((kptr[i] == '(') || (kptr[i] == KANJI_LPARAN) && !okurigana) { okurigana = true; mask = 0x80; }
      else if ((kptr[i] == ')') || (kptr[i] == KANJI_RPARAN) &&  okurigana) { okurigana = false; }
      else if (ISKANA(kptr[i])) { *ptr++ = (kptr[i] & 0x00ff) | mask; mask = 0; }
      else {
        JMessageBox (dialog,IDS_RL_ERRORNONKANA,IDS_RL_ERRORKANA,MB_OK | MB_ICONERROR);
        SetFocus (GetDlgItem(dialog,IDC_RLSTRING));
        return;
      }
    }
    *ptr = 0;
  }
//
//  Can we get kanji data?
//
  if (kanji_info.open_info(dialog)) return;             // Open info file.
//
//  Do the search.
//
  for (i = MIN_KANJI1; i <= MAX_KANJI1; i += 0x100) {
    for (j = MIN_KANJI2; j <= MAX_KANJI2; j++) {
//
//  Check for valid matching code.
//
      kanji_info.get_info (i|j,INFO_STRINGS);       // Get information for this kanji.
      if ((kanji_info.kinfo.strokes < s1) || (kanji_info.kinfo.strokes > s2)) continue;
      switch (type) {
        case RLTYPE_ON:              // on-yomi
             for (ptr = kanji_info.on, k = 0; k < kanji_info.kinfo.on; k++) {
               if (test_kana((byte *) astring,ptr,false)) goto keep_this;
               ptr += strlen((char *) ptr)+1;
             }
             continue;
        case RLTYPE_KUN:             // kun-yomi
             for (ptr = kanji_info.kun, k = 0; k < kanji_info.kinfo.kun; k++) {
               if (test_kana((byte *) astring,ptr,kun)) goto keep_this;
               ptr += strlen((char *) ptr)+1;
             }
             continue;
        case RLTYPE_KUNON:           // kun or on, RELIES ON THE ORDER OF THE ON AND KUN READINGS IN THE DATABASE!
             for (ptr = kanji_info.on, k = 0; k < kanji_info.kinfo.on+kanji_info.kinfo.kun; k++) {
               if (test_kana((byte *) astring,ptr,kun)) goto keep_this;
               ptr += strlen((char *) ptr)+1;
             }
             continue;
        case RLTYPE_MEANING:         // meaning
             for (ptr = kanji_info.imi, k = 0; k < kanji_info.kinfo.imi; k++) {
               if (test_ascii(astring,ptr,frags)) goto keep_this;
               ptr += strlen((char *) ptr)+1;
             }
             continue;
        case RLTYPE_NANORI:          // nanonri
             for (ptr = kanji_info.nan, k = 0; k < kanji_info.kinfo.nan; k++) {
               if (test_kana((byte *) astring,ptr,false)) goto keep_this;
               ptr += strlen((char *) ptr)+1;
             }
             continue;
        case RLTYPE_PINYIN:          // pinyin
             if (!test_ascii(astring,kanji_info.pinyin,false)) continue;
             break;
        case RLTYPE_KOREAN:          // korean
             if (!test_ascii(astring,kanji_info.korean,false)) continue;
             break;
      }
keep_this:;
      put_kanji (i|j);
      put_kanji ('/');
    }
  }
//
//  Cleanup
//
  kanji_info.close_info ();                             // Close the info database.
  return;
}

//-------------------------------------------------------------------
//
//  Stub routine for entry
//

//--------------------------------
//
//  Generailly, I do not like friend fucntions, but I need an entry point,
//  and I wanted all the routines in this file.
//
//      file -- Context from which we were called so we can past text boack
//
void reading_lookup (JWP_file *file) {
  if (read_lookup) { 
    SendDlgItemMessage  (read_lookup->dialog,IDC_RLSTRING,JE_LOAD,0,(LPARAM) file);
    SetForegroundWindow (read_lookup->dialog); 
    return; 
  }
  if (!(read_lookup = new READING_lookup)) { OutOfMemory (file->window); return; }
  JCreateDialog (IDD_READINGLOOKUP,file->window,(DLGPROC) dialog_readinglookup,(LPARAM) file);
  return;
}

//===================================================================
//
//  Class SKIP_lookup
//
//===================================================================

//-------------------------------------------------------------------
//
//  Class SKIP_lookup definition.
//
//  This class is dirived from the KANJI_lookup class and process the 
//  SKIP lookup function.
//

typedef class SKIP_lookup : public KANJI_lookup {
friend void skip_lookup (JWP_file *file);
public:
  int  dlg_skiplookup    (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
private:                                                // Required procedures:
  void   reset           (void);                        // Reset the search engine.
  void   search          (void);                        // Execute the actual search.
} SKIP_lookup;

//-------------------------------------------------------------------
//
//  static data and definitions.
//

#define MAX_TYPE    4                   // Maximum types allowed
#define MAX_SKIP1   20                  // Maximum for first skip value
#define MAX_SKIP2   24                  // Maximum for second skip value

static SKIP_lookup *skp_lookup = NULL;  // Global pointer for the skip lookup chart.

//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Dialog box procedure stub for the SKIP Lookup dialog box.
//
static BOOL CALLBACK dialog_skiplookup (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (skp_lookup->dlg_skiplookup(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  This procedure handles processing for the skip type window.  This window allows 
//  you to choose the skip type by selecting an image, not a number.
//
#define SKIP_FULLWIDTH  118                 // Full width of skip buttons
#define SKIP_FULLHEIGHT 48                  // Full height of skip buttons
#define SKIP_WIDTH      (SKIP_FULLWIDTH /2) // Width of one of the skip type buttons
#define SKIP_HEIGHT     (SKIP_FULLHEIGHT/2) // Height of one of the skip type buttons.

static LRESULT CALLBACK JWP_skiptype_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;
  HDC         hdcmem;       // for holding the bitmaps.
  HBITMAP     bitmap;
  int         i;
  switch (iMsg) {
//
//  During window creation, we have to adjust to the size of the window to match 
//  the size of the bitmap.
//
    case WM_CREATE:
         CREATESTRUCT *create;
         create = (CREATESTRUCT *) lParam;
         MoveWindow (hwnd,create->x,create->y,SKIP_FULLWIDTH+2*WIN_XEDGE,SKIP_FULLHEIGHT+2*WIN_YEDGE,true);
         return (0);
//
//  We render the display here.  The main display element is stored as a
//  complete bitmap in the resource, so we just need to render it.
//
    case WM_PAINT:
         hdc    = BeginPaint(hwnd,&ps);
         hdcmem = CreateCompatibleDC(hdc);
//       SetMapMode    (hdcmem,GetMapMode(hdc));            // Not actually necessary.
         bitmap = LoadBitmap(instance,MAKEINTRESOURCE(IDB_SKIPTYPES));
         SelectObject (hdcmem,bitmap);
         SetBkColor   (hdc,GetSysColor(COLOR_BTNFACE));
         BitBlt       (hdc,0,0,SKIP_FULLWIDTH,SKIP_FULLHEIGHT,hdcmem,0,0,SRCCOPY);
         DeleteObject (bitmap);
         DeleteDC     (hdcmem);
         EndPaint     (hwnd,&ps);
         return (0);
//
//  Mouse event, causes us to send a message to the parent to set the type.
//
    case WM_LBUTTONDOWN:
         i  =    HIWORD(lParam)/SKIP_HEIGHT;
         i += 2*(LOWORD(lParam)/SKIP_WIDTH);
         SendMessage(GetParent(hwnd),WM_COMMAND,IDC_SLSKIPTYPE,i+1);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//-------------------------------------------------------------------
//
//  begin class SKIP_lookup
//

//--------------------------------
//
//  Dailog box handler for the radical lookup dailog box.  This does all
//  the control work here.
//
int SKIP_lookup::dlg_skiplookup (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  switch (iMsg) {
    case WM_INITDIALOG: {                       // Intialize, so chache dialog box pointer.
           KANJI_info kanji_info;               // Class used to access the kanji information database
           kanji_info.open_info  (hwnd);        // We need to open kanji info at the start to see if the miss-codes are included in the info we have.
           kanji_info.close_info ();
           SendDlgItemMessage (hwnd,IDC_SLTYPESPIN,UDM_SETRANGE,0,MAKELONG(MAX_TYPE ,0));
           SendDlgItemMessage (hwnd,IDC_SLS1SPIN  ,UDM_SETRANGE,0,MAKELONG(MAX_SKIP1,0));
           SendDlgItemMessage (hwnd,IDC_SLS2SPIN  ,UDM_SETRANGE,0,MAKELONG(MAX_SKIP2,0));
           CheckDlgButton     (hwnd,IDC_SLMISSCODES,jwp_config.cfg.skip_misscodes);
           if (!(jwp_config.kanji_flags & KIFLAG_XREF)) {
             jwp_config.cfg.skip_misscodes = false;
             EnableWindow (GetDlgItem(hwnd,IDC_SLMISSCODES),false);
           }
           initialize      (hwnd,true,IDC_SLTYPE);
           POCKETPC_DIALOG (hwnd);
         }
         return          (false);
    case WM_DESTROY:
         remove_dialog (hwnd);
         delete this;
         skp_lookup = NULL;
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_SKIPLOOKUP);
         return  (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
#ifdef WINCE_POCKETPC
           case IDOK:
#endif WINCE_POCKETPC
           case IDCANCEL:
                jwp_config.cfg.skip_misscodes = IsDlgButtonChecked(hwnd,IDC_SLMISSCODES);
                break;
           case IDC_SLSKIPTYPE:
                set_value (IDC_SLTYPE,lParam);
                SetFocus  (GetDlgItem(hwnd,IDC_SLS1));
                break;                              
           case IDC_SLTYPE:
           case IDC_SLS1:
           case IDC_SLS2:
                input_check (hwnd,wParam);
                if (HIWORD(wParam) != EN_CHANGE) return (true);
                break;
           default:
                break;
         }
         return (command(wParam));
  }
  return (false);       // Let the dialog box handler handle the standard
}                                                   //   buttons and actions.    

//--------------------------------
//
//  Resets the entire dialog box to it's default state.  This is
//  the Clear Button, it is also used to initialize the dalog box.
//
void SKIP_lookup::reset () {
  set_value (IDC_SLTYPE,0);
  set_value (IDC_SLS1  ,0);
  set_value (IDC_SLS2  ,0);
  adjusting = true;
  CheckDlgButton (dialog,IDC_SLMISSCODES,jwp_config.cfg.skip_misscodes);
  adjusting = false;
  clear_results ();
  return;
}

//--------------------------------
//
//  This does the actual search.
//
void SKIP_lookup::search () {
  byte *ptr;
  int code,i,j,k,t,t1,t2,a,a1,a2,b,b1,b2;
  KANJI_info kanji_info;        // Class used to access the kanji information database
//
//  Get search parameters.
//
  get_value (IDC_SLTYPE,MAX_TYPE ,t1,t2);
  get_value (IDC_SLS1  ,MAX_SKIP1,a1,a2);
  get_value (IDC_SLS2  ,MAX_SKIP2,b1,b2);
  if (IsDlgButtonChecked(dialog,IDC_SLMISSCODES)) code = INFO_ALL; else code = INFO_FIXED;
//
//  Can we get kanji data?
//
  if (kanji_info.open_info(dialog)) return;             // Open info file.
//
//  Do the search.
//
  for (i = MIN_KANJI1; i <= MAX_KANJI1; i += 0x100) {
    for (j = MIN_KANJI2; j <= MAX_KANJI2; j++) {
//
//  Check for valid matching code.
//
      kanji_info.get_info (i|j,code);                   // Get information for this kanji.
      if ((kanji_info.kinfo.skip_t >= t1) && (kanji_info.kinfo.skip_t <= t2) &&
          (kanji_info.kinfo.skip_1 >= a1) && (kanji_info.kinfo.skip_1 <= a2) && 
          (kanji_info.kinfo.skip_2 >= b1) && (kanji_info.kinfo.skip_2 <= b2)) {
        put_kanji (i|j);
        put_kanji ('/');
      }
//
//  Check for miss-codes.
//
      if ((code == INFO_ALL) && kanji_info.xref) {
        for (ptr = kanji_info.xref; *ptr; ptr += 3) {
          if (*ptr == 'z') {
            k = *((ushort *) (ptr+1));
            t = (k >> 10) & 0x7;
            a = (k >> 5 ) & 0x1f;
            b = (k      ) & 0x1f;
            if ((t >= t1) && (t <= t2) &&
                (a >= a1) && (a <= a2) && 
                (b >= b1) && (b <= b2)) {
              put_kanji (i|j);
              put_kanji ('/');
            }
          }
        }
      }
    }
  }
//
//  Cleanup
//
  kanji_info.close_info ();                             // Close the info database.
  return;
}

//-------------------------------------------------------------------
//
//  Stub routine for entry
//

//--------------------------------
//
//  Generailly, I do not like friend fucntions, but I need an entry point,
//  and I wanted all the routines in this file.
//
//      file -- Context from which we were called so we can past text boack
//
void skip_lookup (JWP_file *file) {
  if (skp_lookup) { SetForegroundWindow (skp_lookup->dialog); return; }
  if (!(skp_lookup = new SKIP_lookup)) { OutOfMemory (file->window); return; }
  JCreateDialog (IDD_SKIPLOOKUP,file->window,(DLGPROC) dialog_skiplookup);
  return;
}
































//===================================================================
//
//  Class JIS_table definition.
//

typedef class JIS_table {
public:
  int dlg_jistable     (HWND hwnd,int message,WPARAM wParam,LPARAM lParam);
  int jistable_winproc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam);
  HWND  dialog;         // Window pointer for dialog box.
private:
  KANJI_font *kanji;        // Font used to draw the JIS table.    
  KANJI       current;      // Current active character (JIS code)
  HWND        display;      // Window pointer for character display
  int         kanjibad;     // Cached value of bad kanji (black box).
  int         update;       // Flag indicates doing an update.
  int  get_hex    (int id);                 // Get hex value from an edit box.
  void goto_jis   (int jis,int exclude);    // Move view to a specific JIS code.
  void put_hex    (int id,int hex);         // Put hex value into an edit box.
  int  valid      (int jis);                // Is this a valid JIS code.
  int  valid_page (int page);               // Check to see if there is a valid entry on the page.
} JIS_table;

static JIS_table *jis_ptr = NULL;       // Class poiner to dialog procedures can find the class.
                            
//-------------------------------------------------------------------
//
//  static data and definitions.
//


//-------------------------------------------------------------------
//
//  static procedures.
//

//--------------------------------
//
//  Dialog box procedure stub for JIS table.
//
static BOOL CALLBACK dialog_jistable (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jis_ptr->dlg_jistable(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  Window procedure stub for the character window in the JIS table.
//
static LRESULT CALLBACK JWP_jistable_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  return (jis_ptr->jistable_winproc(hwnd,iMsg,wParam,lParam));
}

//
//  end class JIS_table
//
//===================================================================

//===================================================================
//
//  JIS table rotuines (see stub routine above, and class definition
//                      above).
//
//  These rotuines implement the JIS table routine, which bascially
//  allows the user to brouse the character set in JIS, EUC, shift-JIS,
//  and Unicode character spaces.
//

//-------------------------------------------------------------------
//
//  Dialog box ID's and other static definitions.
//
//      IDC_JTJISCODE     JIS code edit box.
//      IDC_JTEUCCODE     EUC code edit box.
//      IDC_JTSJISCODE    Shift-JIS code edit box.
//      IDC_JTUNICODE     Unicode edito box.
//      IDC_JTINFO        Get info button.
//      IDC_JTINSERT      Insert into file button.
//      IDC_JTTABLE       Kanji list.
//
#if    (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  #define JIS_WIDTH     10      // Number of character per line in the table.
  #define JIS_HEIGHT    10      // Number of lines in the table.
#else  (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
  #define JIS_WIDTH     16      // Number of character per line in the table.
  #define JIS_HEIGHT    6       // Number of lines in the table.
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))

//-------------------------------------------------------------------
//
//  Begin class JIS_table
//

//--------------------------------
//
//  Dialog box procedure for processing the JIS table dialog box.
//
int JIS_table::dlg_jistable (HWND hwnd,int message,WPARAM wParam,LPARAM lParam) {
  int i;
  switch (message) {
    case WM_INITDIALOG:                             // Intialize dialog.
         POCKETPC_DIALOG (hwnd);                    //   Full screen dialog for PocketPC
         add_dialog (dialog = hwnd,true);           //   Cache dialog pointer.
         kanji    = get_jistfont();                 //   Cache font for JIS table
         display  = GetDlgItem(hwnd,IDC_JTTABLE);   //   Cache kanji window for scroll control.
         update   = false;                          //   We are not updating text boxes
         kanjibad = kanji->jis_index(KANJI_BAD);    //   Cache bad kanji index for this font.
         goto_jis (0x2421,0);                       //   Select the start kanji (space).
         return (true);
    case WM_DESTROY:                                // Terminate dialog
         remove_dialog (hwnd);                      //   Unlink from dialog list
         delete this;                               //   Do not need the class object
         jis_ptr = NULL;                            //   Global pointer to NULL so will generate again.
         return (1);
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_JISTABLE);
         return  (true);
    case WM_COMMAND:
         switch (i = LOWORD(wParam)) {
           case IDCANCEL:                           // Done button, so exit.
           case IDOK:
                DestroyWindow (hwnd);
                return (true);
//
//  This sections process chares to the edit boxes.
//
           case IDC_JTJISCODE:
                input_check (hwnd,wParam);
                if (!update) goto_jis(get_hex(IDC_JTJISCODE),IDC_JTJISCODE);
                return (true);
           case IDC_JTEUCCODE:
                input_check (hwnd,wParam);
                if (!update) goto_jis(0x7f7f & get_hex(IDC_JTEUCCODE),IDC_JTEUCCODE);
                return (true);
           case IDC_JTSJISCODE:
                input_check (hwnd,wParam);
                if (!update) goto_jis(sjis2jis(get_hex(IDC_JTSJISCODE)),IDC_JTSJISCODE);
                return (true);
           case IDC_JTUNICODE:
                input_check (hwnd,wParam);
                i = get_hex(IDC_JTUNICODE);
                if (!update) goto_jis(unicode2jis(i,i),IDC_JTUNICODE);
                return (true);
//
//  Info and insert are relativly simple.
//
           case IDC_JTINFO:
                kanji_info (hwnd,current);
                return     (true);
           case IDC_JTCLIP: {                           // Copy to clipboard
                  JWP_file *file = NULL;
                  if (file = file->clip_copy()) {
                    file->put_string (&current,1);
                  }
                }
                return (true);
           case IDC_JTINSERT: 
                JWP_file *file;
                file = file_list.get(NULL);
                file->insert_string (&current,1);       // Allow specific undo of put back.
                return (true);
         }
         break;
  }
  return (false);
}

//--------------------------------
//
//  Reads a hex value from a text edit box and returns the value.
//
//      id     -- Item to read from.
//
//      RETURN -- Hex string converted to numeric value.
//
int JIS_table::get_hex (int id) {
  TCHAR buffer[SIZE_BUFFER];
  int  hex;
  GetDlgItemText (dialog,id,buffer,SIZE_BUFFER);
  sscanf (buffer,TEXT("%x"),&hex);
  return (hex);
}

//--------------------------------
//
//  This key routine implements a change in the cursor location within
//  the caracter set.  This routien will adjust the scroll bar, and 
//  the character values.
//
//      jis     -- New JIS code value.
//      exclude -- This edit box is ecluded from the updating.  This 
//                 needs to be done to prevent an infinite loop on one
//                 hand and to prevent us from trahing the input 
//                 that a user is generating.
//
void JIS_table::goto_jis (int jis,int exclude) {
  current = jis;                            // Update cursor location.
  if (!valid(jis)) return;                  // Not valid so do not change dispaly.
//
//  Update scroll bar and redraw kanji.
//
  scroll_info.nMax   = 0x74-0x21;
  scroll_info.nPage  = 1;
  scroll_info.nPos   = (jis >> 8)-0x21;
  SetScrollInfo  (display,SB_VERT,&scroll_info,true);
  InvalidateRect (display,NULL,false);
//
//  Update edit boxes.  
//
  update  = true;                           // Prevent loop.
  if (exclude != IDC_JTJISCODE ) put_hex (IDC_JTJISCODE ,jis);
  if (exclude != IDC_JTEUCCODE ) put_hex (IDC_JTEUCCODE ,jis | 0x8080);
  if (exclude != IDC_JTSJISCODE) put_hex (IDC_JTSJISCODE,jis2sjis(jis));
  if (exclude != IDC_JTUNICODE ) put_hex (IDC_JTUNICODE ,jis2unicode(jis));
  update  = false;
  return;
}

//--------------------------------
//
//  This routine process messages for the character window in the 
//  JIS table dialog.
//
int JIS_table::jistable_winproc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;          // DC used in WM_PAINT only.
  PAINTSTRUCT ps;           // Paint structure (WM_PAINT only).
  RECT        rect;         // Rect is the entire entire window, then the 16x16 character display box.
  RECT        box;          // This is the entire button box, including borders.
  HPEN        pen;          // Pen used for drawing outlines around the boxes.
  int         i,j,x,y;
  int         index;        // JIS code.

  switch (iMsg) {
//
//  This routine dynaically allocates the size of the array fo buttons.
//  since the buttons, are fixed size, however, this should only cause 
//  the display window to get smaller.
//
    case WM_CREATE:
         CREATESTRUCT *create;
         create = (CREATESTRUCT *) lParam;
         MoveWindow          (hwnd,create->x,create->y,JIS_WIDTH*BUTTON_SIZE+GetSystemMetrics(SM_CXVSCROLL)+2*WIN_XEDGE,JIS_HEIGHT*BUTTON_SIZE+2*WIN_YEDGE,true);
         ImmAssociateContext (hwnd,NULL);       // Disable the IME for this window
         return (0);
    case WM_GETDLGCODE:
         return (DLGC_WANTARROWS | DLGC_WANTCHARS);     // Request arrow keys and chars (to get space).
//
//  This is the difficult part, of corse, this is the draw routine.
//  Note that all draws are solid, and cause us to overwirte the entire 
//  button area, this allows us to not have the dredraw function fill 
//  with the background brush.  This has the advantage of supressing 
//  the flashing and actually being faster.
//
    case WM_PAINT:
         hdc    = BeginPaint(hwnd,&ps);
         index = (current & 0xff00) | 0x20;                 // Intialize JIS value.
         SetTextColor (hdc,GetSysColor(COLOR_WINDOWTEXT));
         SetBkColor   (hdc,GetSysColor(COLOR_WINDOW));
         for (j = 0; j < JIS_HEIGHT; j++) {                 // Do each row.
           y           = j*BUTTON_SIZE;                     // Initialize row based values.
           box .top    = y;
           box .bottom = y+BUTTON_SIZE;
           rect.top    = y+BUTTON_JBMOFFSET;
           rect.bottom = rect.top+BUTTON_JBMSIZE;
           for (i = 0; i < JIS_WIDTH; i++) {                // Do each button in a row.
             box.left   = i*BUTTON_SIZE;                    // Initialize column based values.
             box.right  = box.left+BUTTON_SIZE;
             rect.left  = i*BUTTON_SIZE+BUTTON_JBMOFFSET;
             rect.right = rect.left+BUTTON_JBMSIZE;
             if (valid(index)) {                            // Render button with charcter.
               pen = (HPEN) SelectObject(hdc,CreatePen(PS_SOLID,0,GetSysColor(COLOR_WINDOWTEXT)));
               SelectObject (hdc,GetSysColorBrush(COLOR_WINDOW));
               Rectangle    (hdc,i*BUTTON_SIZE,y,i*BUTTON_SIZE+BUTTON_SIZE,y+BUTTON_SIZE);
               kanji->fill  (hdc,index,&rect);
               DeleteObject (SelectObject(hdc,pen));
             }
             else {                                         // Render invalid button.
               FillRect (hdc,&box,(HBRUSH) GetStockObject(GRAY_BRUSH));
             }
             if (index == current) InvertRect (hdc,&box);   // Invert for cursor.
             index++;                                       // Next character.
           }
         }
         EndPaint (hwnd,&ps);
         return (0);
//
//  Wheel mouse support.
//
#ifndef WINCE
    case WM_MOUSEWHEEL:
         static int delta;                                                  // Accumulation point for deltas.
         delta += (short) HIWORD(wParam);                                   // Delta
         while (delta > WHEEL_DELTA) {
           SendMessage (hwnd,WM_KEYDOWN,VK_PRIOR,0);
           delta -= WHEEL_DELTA;
         }
         while (abs(delta) > WHEEL_DELTA) {
           SendMessage (hwnd,WM_KEYDOWN,VK_NEXT,0);
           delta += WHEEL_DELTA;
         }
         return (0);
#endif  WINCE
//
//  Process scroll messages.
//
//  Mostly we translate these into keyboad messages and allow the 
//  keyboard handler to do our processing for us.
//
    case WM_VSCROLL:
         switch (LOWORD(wParam)) {
           case SB_LINEDOWN:            // Move down one page.
           case SB_PAGEDOWN:
                wParam = VK_NEXT;
                break;
           case SB_LINEUP:              // Move up one page.
           case SB_PAGEUP:
                wParam = VK_PRIOR;
                break;
           case SB_THUMBTRACK:          // Tracking.
           case SB_THUMBPOSITION:
                current = ((HIWORD(wParam)+0x21) << 8) | (current & 0xff);
                goto_jis (current,0);
                return (0);
           default:
                return (0);
         }                                      // **** FALL THROUGH ****
//
//  Process keyboard events for special keys, we use these to manuver
//  arround the button set.
//
    case WM_KEYDOWN:
         i = (GetKeyState(VK_CONTROL) < 0);
         j = i ? CONTROL_MOVE : 1;
         x = current & 0x00ff;
         y = current & 0xff00;
         switch (wParam) {
           case VK_RIGHT:   // Right 1 or CONTROL_MOVE
                x += j;
                break;
           case VK_LEFT:    // Left 1 or CONTROL_MOVE
                for (x -= j; (x > 0x20) && !valid(y | x); x--);
                break;
           case VK_HOME:    // Upper left corner or start of list.
                x = 0x21;
                if (j != 1) y = 0x2100;
                break;
           case VK_END:     // Lower right corner or end of list.
                x = 0x7e;
                if (j != 1) y = 0x7400;
                break;
           case VK_UP:      // Up 1
                for (x -= JIS_WIDTH; (x > 0x20) && !valid(y | x); x -= JIS_WIDTH);
                if (x >= 0x21) break;
                x = (current & 0x000f) | 0x70;      // **** FALL THROUGH ****
           case VK_PRIOR:   // Up page
                if (y == 0x2100) return (0);
                do { y -= 0x100; } while (!valid_page(y) && (y != 0x2100));
                break;
           case VK_DOWN:    // Down 1
                for (x += JIS_WIDTH; (x < 0x7f) && !valid(y | x); x += JIS_WIDTH);
                if (x <= 0x7e) break;
                x = (current & 0x000f) | 0x20;      // **** FALL THROUGH ****
           case VK_NEXT:    // Down page.
                if (y == 0x7400) return (0);
                do { y += 0x100; } while (!valid_page(y) && (y != 0x7400));
                break;
           case VK_C:
                SendMessage (dialog,WM_COMMAND,IDC_JTCLIP,0);
                break;
           case VK_I:
                SendMessage (dialog,WM_COMMAND,IDC_JTINFO,0);
                break;
           default:
                return (0);
         }
         while ((x <= 0x7e) && !valid(y | x)) x++;
         while ((x >= 0x20) && !valid(y | x)) x--;
         goto_jis (y | x,0);
         return (0);
//
//  The only character we accpet is ' ' which indicates insert
//
    case WM_CHAR:
         if (wParam != ' ') return (0);         // **** FALL THROUGH ****
//
//  Double-click event will invoke a search
//
    case WM_LBUTTONDBLCLK: 
         SendMessage (dialog,WM_COMMAND,IDC_JTINSERT,0);
         return (0);
//
//  Mouse event, implies we need to find out where the user is clicking
//  and toggle that button.
//
    case WM_RBUTTONUP:
         ReleaseCapture ();
         return (0);
    case WM_RBUTTONDOWN:
         SetCapture (hwnd);
         wParam = MK_SHIFT;
    case WM_LBUTTONDOWN:
         x = LOWORD(lParam)/BUTTON_SIZE;
         y = HIWORD(lParam)/BUTTON_SIZE;
         goto_jis ((current & 0xff00)+y*JIS_WIDTH+x+0x20,0);
         SetFocus (hwnd);
         if ((GetKeyState(VK_MENU) < 0) || (wParam & MK_SHIFT)) SendMessage (dialog,WM_COMMAND,IDC_JTINFO,0);
         return (0);
//
//  Context menu gets character information
//
#ifndef WINCE
    case WM_CONTEXTMENU:
         SendMessage (dialog,WM_COMMAND,IDC_JTINFO,0);
         return (0);
#endif WINCE
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//--------------------------------
//
//  Puts a hex value into a text item in the dialog box.  This is used
//  to set the JIS, EUC, shift-JIS and Unicode values.
//
//      id  -- Id of item to change.
//      hex -- Numberical value to write.
//
void JIS_table::put_hex (int id,int hex) {
  TCHAR buffer[10];
  wsprintf (buffer,TEXT("%04.4X"),hex);
  SetDlgItemText (dialog,id,buffer);
  return;
}

//--------------------------------
//
//  This is a simple test routine to determine if a JIS value is 
//  valid.  Currently this is based on the actual font loaded, which
//  may be a mistake, but it does control display do what can be 
//  displayed by the font.
//
//      jis    -- JIS valie to test.
//
//      RETRUN -- Non-zero indictes the character is a valid JIS
//                code.
//
int JIS_table::valid (int jis) {
  if (jis == KANJI_BAD) return (true);
  if (kanjibad != kanji->jis_index(jis)) return (true);
  return (false);
}

//--------------------------------
//
//  This is a simple test routine to determine if there is a valid 
//  character on a particular page.  If there is not any valid character,
//  this page is skipped by the table routines.
//
//      page   -- JIS page to test (a page is the upper byte of a JIS code).
//
//      RETRUN -- Non-zero indictes the page contains at least a single valid
//                character.
//
int JIS_table::valid_page (int page) {
  int i;
  for (i = 0x21; i <= 0x7e; i++) {
    if (valid(page|i)) return (true);
  }
  return (false);
}

//-------------------------------------------------------------------
//
//  Exported routines.
//

//--------------------------------
//
//  Entry point for generating the JIS table.
//
//      file -- Context we were called from so we can put things back
//
void jis_table (JWP_file *file) {
  if (jis_ptr) {
    SetForegroundWindow (jis_ptr->dialog);
  }
  else {
    if (!(jis_ptr = new JIS_table)) { OutOfMemory (file->window); return; }
    JCreateDialog (IDD_JISTABLE,file->window,(DLGPROC) dialog_jistable);
  }
  return;
}

//
//  End JIS table
//
//===================================================================

















//-------------------------------------------------------------------
//
//  Other exported procedrues.
//

//--------------------------------
//
//  Initialization procedure.  This basically registers the windows 
//  classes used by the results window and by the radicals buttons
//  windows.
//
int initialize_radlookup (WNDCLASS *wclass) {
  wclass->style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wclass->lpfnWndProc   = JWP_radlookup_proc;               // Results window for radical-lookup
  wclass->hbrBackground = (HBRUSH) (COLOR_WINDOW+1);
  wclass->lpszClassName = TEXT("JWP-KList");
  if (!RegisterClass(wclass)) return (true);
  wclass->style         = CS_HREDRAW | CS_VREDRAW;
  wclass->lpfnWndProc   = JWP_skiptype_proc;                // Radicals window for radicals lookup dialog
  wclass->hbrBackground = (HBRUSH) (COLOR_BTNFACE+1);
  wclass->lpszClassName = TEXT("JWP-SKIP");
  if (!RegisterClass(wclass)) return (true);
  wclass->style         = CS_HREDRAW | CS_VREDRAW;
  wclass->lpfnWndProc   = JWP_fcshape_proc;                 // Radicals window for radicals lookup dialog
  wclass->lpszClassName = TEXT("JWP-FC");
  if (!RegisterClass(wclass)) return (true);
  wclass->style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wclass->lpfnWndProc   = JWP_radicals_proc;                // Radicals window for radicals lookup dialog
  wclass->hbrBackground = (HBRUSH) (COLOR_APPWORKSPACE+1);
  wclass->lpszClassName = TEXT("JWP-Rads");
  if (!RegisterClass(wclass)) return (true);
  wclass->style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wclass->lpfnWndProc   = JWP_bushu_proc;                   // Bushu list for HS lookup
  wclass->lpszClassName = TEXT("JWP-HS");
  if (!RegisterClass(wclass)) return (true);
  wclass->style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wclass->lpfnWndProc   = JWP_jistable_proc;                // JIS table character window.
  wclass->lpszClassName = TEXT("JWP-Table");
  if (!RegisterClass(wclass)) return (true);
  return (false);
}





