//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//===================================================================//

#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_file.h"
#include "jwp_inpt.h"
#include "jwp_font.h"
#include "jwp_help.h"
#include "jwp_misc.h"
#include "jwp_prnt.h"

//===================================================================
//
//  These routines handle the processing of the Utilities/Page Layout...
//  dialog box.
//
//  These routines manipulate an object of type struct FileSetup.
//  This structure contains all of the parameters that can be set 
//  in this dialog box.  If the changes are accpeted, the values 
//  in this structure become the values for this file (and global 
//  defintions), if not, they a simply dellocated.
//

//-------------------------------------------------------------------
//
//  Global definitions.
//

//--------------------------------
//
//  Structure containning things that can be modified using this 
//  dialog box.
//
typedef struct FileSetup {
  PageSetup file;                                           // This file margins
  PageSetup def;                                            // Default margins
  KANJI_string headers[NUMBER_HEADERS][NUMBER_POSITIONS];   // Headers for this file
  KANJI_string summary[NUMBER_SUMMARY];                     // Summary for this file
  KANJI_string date;                                        // Date format
  KANJI_string time;                                        // Time format
  KANJI_string am;                                          // AM format string.
  KANJI_string pm;                                          // PM format string.
  byte   odd_even;                                          // Separate odd & even headers (this file)
  byte   no_first;                                          // No header on first page (this file)
  byte   justify;                                           // Justify ASCII text.
  short  headleft,headright,headtop,headbottom;             // Header location parameters
//
//  Working variables.
//
  short  head;          // Which header set is currently being edited
} FileSetup;

static FileSetup *filesetup;                // Pointer to data structure so dialog 
                                            //   routines can get to it.

//-------------------------------------------------------------------
//
//  Advanced page routines.
//
//      IDC_LAPOSHEADLEFT     Header position locators
//      IDC_LAPOSHEADRIGHT     
//      IDC_LAPOSHEADTOP       
//      IDC_LAPOSHEADBOTTOM    
//      IDC_LACENTERSHORT     Center short text.
//      IDC_LAJUSTIFYLONG     Justify long text.
//
#ifndef WINCE

static BOOL CALLBACK setup_advanced (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  int i;
  switch (msg) {
    case WM_INITDIALOG:
         for (i = 0; i < 4; i++) put_float (hwnd,IDC_LAPOSHEADLEFT+i,(&filesetup->headleft)[i],100);
         CheckDlgButton (hwnd,IDC_LAJUSTIFY,filesetup->justify);
         return (true);
    case WM_GETDLGVALUES:
         for (i = 0; i < 4; i++) (&filesetup->headleft)[i] = get_float(hwnd,IDC_LAPOSHEADLEFT+i,0,10,(&filesetup->headleft)[i],100,NULL);
         filesetup->justify = IsDlgButtonChecked(hwnd,IDC_LAJUSTIFY);
         return (true);
  }
  return (false);
}
#endif WINCE

//--------------------------------
//
//  Date & Time page routines.
//
//      IDC_LDDATE    Date format string
//      IDC_LDTIME    Time format string
//      IDC_LDAM      AM format string
//      IDC_LDPM      PM format string
//
#ifndef WINCE

static BOOL CALLBACK setup_datetime (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG:
         filesetup->time.put (hwnd,IDC_LDTIME);
         filesetup->date.put (hwnd,IDC_LDDATE);
         filesetup->am  .put (hwnd,IDC_LDAM);
         filesetup->pm  .put (hwnd,IDC_LDPM);
         return (true);
    case WM_GETDLGVALUES:
         filesetup->time.get (hwnd,IDC_LDTIME);
         filesetup->date.get (hwnd,IDC_LDDATE);
         filesetup->am  .get (hwnd,IDC_LDAM);
         filesetup->pm  .get (hwnd,IDC_LDPM);
         return (true);
  }
  return (false);
}
#endif WINCE

//-------------------------------------------------------------------
//
//  Header/Footer page routines.
//
//      IDC_LHNOFIRST     No header/footer on the first page.
//      IDC_LHODDEVEN     Separate even and odd headers/footers
//      IDC_LHEDIT        What set of headers/footers are we editing.
//      IDC_LHHEADLEFT    Left header/footer
//      IDC_LHHEADCENTER  Center header/footer
//      IDC_LHHEADRIGHT   Right header/footer
//

//--------------------------------
//
//  This rutine is called to change the curretn header/footer set being
//  edited.
//
//      hwnd    -- Dialog box pointer.
//      newhead -- New header or footer to edit.
//
static void change_head (HWND hwnd,int newhead) {
  int i,j;
  i = newhead;
  if (!IsDlgButtonChecked(hwnd,IDC_LHODDEVEN)) i >>= 1;     // Convert to list box settings.
  if (filesetup->head == newhead) return;                   // No change, so exit.
  if (filesetup->head != -1) {                              // Old setting was unset, so don't save
    for (j = 0; j < NUMBER_POSITIONS; j++) {
      filesetup->headers[filesetup->head][j].get (hwnd,IDC_LHHEADLEFT+j);
    }
  }
//
//  When setting -1 option (ie just reading the data) we need to 
//  leave other settings unchanged so that the page is still intact,
//  thus we don't want to set anything here.
//
  if (newhead != -1) {                                      // New setting is unset so don't load (shutdown case).
    SendDlgItemMessage (hwnd,IDC_LHEDIT,CB_SETCURSEL,i,0);  // Set list box.  Usually we end up setting this 
                                                            //   to what it already is set to.  During intialize,
                                                            //   however, we use this to set the list box.
    filesetup->head = newhead;                              // Set new 
    for (j = NUMBER_POSITIONS-1; j >= 0; j--) filesetup->headers[newhead][j].put (hwnd,IDC_LHHEADLEFT+j);
  }
  SetFocus (GetDlgItem(hwnd,IDC_LHHEADLEFT));               // Set focus to first edit box.
  return;
}

//--------------------------------
//
//  This routine handles changes from separate odd-even headers/footers,
//  and normal.
//
//      hwnd -- Pointer to dialog box.
//
static void setup_oddeven (HWND hwnd) {
  int i,j;
  static short oddeven[] = { IDS_LH_HEADODD,IDS_LH_HEADEVEN,IDS_LH_FOOTODD,IDS_LH_FOOTEVEN };
  static short both   [] = { IDS_LH_HEADER,IDS_LH_FOOTER };

  SendDlgItemMessage (hwnd,IDC_LHEDIT,CB_RESETCONTENT,0,0); // Clear list.
  j = filesetup->head;                                      // Get current header setting.
  if (j < 0) j = 0;                                         // Take care of startup condition.
  if (IsDlgButtonChecked(hwnd,IDC_LHODDEVEN)) {             // Going to separte odd & even.
    for (i = 0; i < ((int) (sizeof(oddeven)/sizeof(short))); i++) {
      SendDlgItemMessage (hwnd,IDC_LHEDIT,CB_ADDSTRING,0,(LPARAM) get_string(oddeven[i]));
    }
  }
  else {                                                    // Going to normal.
    for (i = 0; i < ((int) (sizeof(both)/sizeof(short))); i++) {
      SendDlgItemMessage (hwnd,IDC_LHEDIT,CB_ADDSTRING,0,(LPARAM) get_string(both[i]));
    }
    j &= 0x2;                                               // Make sure we have a vaild set of headers selected.
  }
  change_head (hwnd,-1);                                    // Force readout of headers
  filesetup->head = -1;                                     // Force change!
  change_head (hwnd,j);                                     // Change header settings.
  return;
}

//--------------------------------
//
//  Dialog box procedure for this page.
//
static BOOL CALLBACK setup_header (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  int i;
  switch (msg) {
    case WM_INITDIALOG:
         CheckDlgButton (hwnd,IDC_LHNOFIRST,filesetup->no_first);
         CheckDlgButton (hwnd,IDC_LHODDEVEN,filesetup->odd_even);
         filesetup->head = -1;              // Indicates intialization.
         setup_oddeven (hwnd);
         return (true);
    case WM_COMMAND: 
         switch (LOWORD(wParam)) {
           case IDC_LHODDEVEN:              // User changed even/odd
                setup_oddeven (hwnd);
                return (true);
           case IDC_LHEDIT:                 // User changed selected set of headers
                i = SendDlgItemMessage(hwnd,IDC_LHEDIT,CB_GETCURSEL,0,0);
                if (i < 0) i = 0;
                if (!IsDlgButtonChecked(hwnd,IDC_LHODDEVEN)) i <<= 1;  // Take care of even/odd or not.
                change_head (hwnd,i);
                return (true);
         }
         return (true);
    case WM_GETDLGVALUES:
         filesetup->no_first = IsDlgButtonChecked(hwnd,IDC_LHNOFIRST);
         filesetup->odd_even = IsDlgButtonChecked(hwnd,IDC_LHODDEVEN);
         change_head (hwnd,-1);      // Change to -1 does a readout of the current headers/footers
         return (true);
  }
  return (false);
}

//-------------------------------------------------------------------
//
//  Margins page routines.
//
//      IDC_LMFLEFT           File left margin
//      IDC_LMFRIGHT          File right margin
//      IDC_LMFTOP            File top margin
//      IDC_LMFBOTTOM         File bottom margin
//      IDC_LMFVERTICAL       File veritcal print
//      IDC_LMFLANDSCAPE      File landscape
//      IDC_LMFROMDEFAULT     Set from default button
//      IDC_LMFUNITS          File units
//      IDC_LMFWIDTH          File width
//      IDC_LMDLEFT           Default left margin
//      IDC_LMDRIGHT          Default right margin
//      IDC_LMDTOP            Default top margin
//      IDC_LMDBOTTOM         Default bottom margin
//      IDC_LMDVERTICAL       Default vertical print
//      IDC_LMDLANDSCAPE      Default landscape 
//      IDC_LMFROMFILE        Set from this file.
//      IDC_LMDUNITS          Default units
//      IDC_LMDWIDTH          Default width
//
#define OFFSET_FILE     0                           // Offset for file variables
#define OFFSET_DEFAULT  (IDC_LMDLEFT-IDC_LMFLEFT)   // Offest for default variables.

//--------------------------------
//
//  Gets a dimension value from an edit box.  If necessary, this routine
//  corrects for the units.
//
//      hwnd    -- Dailog box.
//      id      -- ID of the edit box.
//      *value  -- On entry this is the old value which will be used
//                 as a default.  On exit this is the value from the 
//                 edit box.
//
static void get_dim (HWND hwnd,int id,float *value) {
  float temp;
  get_float (hwnd,id,0,10,*value,1,&temp);
  if (jwp_config.cfg.units_cm) temp = (float) (temp/2.54);
  *value = temp;
  return;
}

//--------------------------------
//
//  Gets all the setup parameters for a page.
//
//      hwnd   -- Dialog box pointer.
//      page   -- Pointer to PageSetup struct to be fill in.
//      offset -- Determines if we are reading parameters for the 
//                current file or the default page.
//  
static void get_page (HWND hwnd,PageSetup *page,int offset) {
  get_dim (hwnd,offset+IDC_LMFLEFT  ,&page->left  );
  get_dim (hwnd,offset+IDC_LMFRIGHT ,&page->right );
  get_dim (hwnd,offset+IDC_LMFTOP   ,&page->top   );
  get_dim (hwnd,offset+IDC_LMFBOTTOM,&page->bottom);
  page->vertical  = IsDlgButtonChecked(hwnd,offset+IDC_LMFVERTICAL );
  page->landscape = IsDlgButtonChecked(hwnd,offset+IDC_LMFLANDSCAPE);
  return;
}

//--------------------------------
//
//  Puts a margin dimension into the edit box.  This rotuine corrects 
//  for the units if necessary.
//
//      hwnd  -- Dialog box.
//      id    -- ID for edit box.
//      value -- Value (in inches).
//
static void put_dim (HWND hwnd,int id,float value) {
  if (jwp_config.cfg.units_cm) value = (float) (value*2.54);
  put_float (hwnd,id,value,1);
  return;
}

//--------------------------------
//
//  Put a page setup into the dailog box.
//
//      hwnd   -- Dailog box.
//      page   -- Pointer to PageSetup struct of page to put into the
//                dialog box.
//      offest -- Determines if this is the default page setup or the 
//                current file setup.
//
static void put_page (HWND hwnd,PageSetup *page,int offset) {
  CheckDlgButton (hwnd,offset+IDC_LMFVERTICAL ,page->vertical);
  CheckDlgButton (hwnd,offset+IDC_LMFLANDSCAPE,page->landscape);
  put_dim (hwnd,offset+IDC_LMFLEFT  ,page->left  );
  put_dim (hwnd,offset+IDC_LMFRIGHT ,page->right );
  put_dim (hwnd,offset+IDC_LMFTOP   ,page->top   );
  put_dim (hwnd,offset+IDC_LMFBOTTOM,page->bottom);
  return;
}

//--------------------------------
//
//  Dailog box procedure for the margins page.
//
static BOOL CALLBACK setup_margins (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  static byte setup;
  switch (msg) {
    case WM_INITDIALOG:
         setup = true;                                      // Block updates
         put_page (hwnd,&filesetup->file,OFFSET_FILE   );
         put_page (hwnd,&filesetup->def ,OFFSET_DEFAULT);
         SetDlgItemText (hwnd,IDC_LMFUNITS,get_string(jwp_config.cfg.units_cm ? IDS_LM_CM : IDS_LM_INCH));
         SetDlgItemText (hwnd,IDC_LMDUNITS,get_string(jwp_config.cfg.units_cm ? IDS_LM_CM : IDS_LM_INCH));
         setup = false;                                     // Activate updates
         return (true);
    case WM_COMMAND: 
         switch (LOWORD(wParam)) {
           INPUT_CHECK (IDC_LMFLEFT);
           INPUT_CHECK (IDC_LMFRIGHT);
           INPUT_CHECK (IDC_LMFTOP);
           INPUT_CHECK (IDC_LMFBOTTOM);
           INPUT_CHECK (IDC_LMDLEFT);
           INPUT_CHECK (IDC_LMDRIGHT);
           INPUT_CHECK (IDC_LMDTOP);
           INPUT_CHECK (IDC_LMDBOTTOM);
           case IDC_LMFROMFILE:                 // Load default from file.
                setup = true;
                get_page (hwnd,&filesetup->file,OFFSET_FILE);
                put_page (hwnd,&filesetup->file,OFFSET_DEFAULT);
                setup = false;
                break;
           case IDC_LMFROMDEFAULT:              // Load file from default.
                setup = true;
                get_page (hwnd,&filesetup->file,OFFSET_DEFAULT);
                put_page (hwnd,&filesetup->file,OFFSET_FILE   );
                setup = false;
                break;
         }                                      
//
//  This recalculates the page widths as the user changes values.
//
         if (!setup) {
           get_page (hwnd,&filesetup->file,OFFSET_FILE);
           get_page (hwnd,&filesetup->def ,OFFSET_DEFAULT);
           SetDlgItemInt(hwnd,IDC_LMFWIDTH,filesetup->file.print_width(),true);
           SetDlgItemInt(hwnd,IDC_LMDWIDTH,filesetup->def .print_width(),true);
         }
         return (true);
    case WM_GETDLGVALUES:
         get_page (hwnd,&filesetup->file,OFFSET_FILE);
         get_page (hwnd,&filesetup->def ,OFFSET_DEFAULT);
         return (true);
  }
  return (false);
}

//-------------------------------------------------------------------
//
//  Summary page routines.
//
//      IDC_LSTITLE       Title
//      IDC_LSSUBJECT     Subject
//      IDC_LSAUTHOR      Author
//      IDC_LSKEYWORDS    Keywords
//      IDC_LSNOTES       Comment.
//
static BOOL CALLBACK setup_summary (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  int i;
  switch (msg) {
    case WM_INITDIALOG:     // Load is backward to get the correct cursor placement.
         for (i = NUMBER_SUMMARY-1; i >= 0; i--) filesetup->summary[i].put(hwnd,IDC_LSTITLE+i);
         return (true);
    case WM_GETDLGVALUES:
         for (i = 0; i < NUMBER_SUMMARY; i++) filesetup->summary[i].get(hwnd,IDC_LSTITLE+i);
         return (true);
  }
  return (false);
}

//-------------------------------------------------------------------
//
//  This routine processes the page layout command.
//
void JWP_file::page_setup () {
  FileSetup setup;
  filesetup = &setup;
  int i,j;
//
//  Static data for the page dialog.
//  
  static TabPage pages[] = {
    { IDS_LAYOUT_MARGINS ,IDD_LAYOUTMARGINS ,(DLGPROC) setup_margins ,IDH_PRINT_MARGINS  },
    { IDS_LAYOUT_HEADFOOT,IDD_LAYOUTHEADER  ,(DLGPROC) setup_header  ,IDH_PRINT_HEADERS  },
    { IDS_LAYOUT_SUMMARY ,IDD_LAYOUTSUMMARY ,(DLGPROC) setup_summary ,IDH_PRINT_SUMMARY  },
#ifndef WINCE
    { IDS_LAYOUT_DATETIME,IDD_LAYOUTDATE    ,(DLGPROC) setup_datetime,IDH_PRINT_DATETIME },
    { IDS_LAYOUT_ADVANCED,IDD_LAYOUTADVANCED,(DLGPROC) setup_advanced,IDH_PRINT_ADVANCED },
#endif WINCE
  };
  static TabSetup layout = { pages,sizeof(pages)/sizeof(TabPage),0,(DLGPROC) NULL };
//
//  Setup configuration variable.
//
  setup.file     = page;
  setup.def      = jwp_config.cfg.page;
  setup.no_first = no_first;
  setup.odd_even = odd_even;
  setup.justify  = jwp_config.cfg.print_justify;
  setup.date.set (jwp_config.cfg.date_format);
  setup.time.set (jwp_config.cfg.time_format);
  setup.am  .set (jwp_config.cfg.am_format);
  setup.pm  .set (jwp_config.cfg.pm_format);
  memcpy (&setup.headleft,&jwp_config.cfg.head_left,4*sizeof(short));
  for (i = 0; i < NUMBER_SUMMARY; i++) setup.summary[i].set(&summary[i]);
  for (i = 0; i < NUMBER_HEADERS; i++) {
    for (j = 0; j < NUMBER_POSITIONS; j++) setup.headers[i][j].set(&headers[i][j]);
  }
//
//  Do it.
//
  i = TabDialog(IDD_LAYOUT,&layout);
//
//  Now apply the changes.
//
  if (i) {
    change ();                                          // File has changed.
    page                         = setup.file;
    jwp_config.cfg.page          = setup.def;
    no_first                     = setup.no_first;
    odd_even                     = setup.odd_even;
    jwp_config.cfg.print_justify = setup.justify;
    setup.date.copy (jwp_config.cfg.date_format,SIZE_DATE);
    setup.time.copy (jwp_config.cfg.time_format,SIZE_DATE);
    setup.am  .copy (jwp_config.cfg.am_format  ,SIZE_AMPM);
    setup.pm  .copy (jwp_config.cfg.pm_format  ,SIZE_AMPM);
    memcpy (&jwp_config.cfg.head_left,&setup.headleft,4*sizeof(short));
    for (i = 0; i < NUMBER_SUMMARY; i++) summary[i].transfer(&setup.summary[i]);
    for (i = 0; i < NUMBER_HEADERS; i++) {
      for (j = 0; j < NUMBER_POSITIONS; j++) headers[i][j].transfer(&setup.headers[i][j]);
    }
    activate ();
  }
  return;
}

//
//  End of routines to handle the processing of the Utilities/Page Layout...
//  dialog box.
//
//===================================================================

//===================================================================
//
//  Begin class TEMP_string.
//
//  This class implements a temporyary working string with a limited 
//  character length.  Text and characters can be added to the string
//  without exceeding the length automatically.
//
#ifndef WINCE

typedef class TEMP_string {
public:
  KANJI buffer[SIZE_BUFFER];        // Buffer containning string.
  int   size;                       // Max size buffer is allowd
  int   length;                     // Current length of buffer.
  
  TEMP_string       (int limit);                    // Constructor
  void add_char     (int ch);                       // Add a character to the string.
  void add_int      (int value,int fill = false);   // Add an integer value to the string.
  void add_kanji    (KANJI *kanji);                 // Add a NULL termiante kanji string to the string
  void add_pattern  (KANJI *pattern);               // Add a date or time pattern to the string.
  void add_string   (tchar *string);                // Add an ascii string to the string.
  void inline clear (void) { length = 0; }          // Clear contents of string.
} TEMP_string;

//--------------------------------
//
//  Constructor, initializes the string as having no characters, and 
//  sets the maximum number of characters possible.
//
//      limit -- Maximum number of characters, should be no more than 
//               SIZE_BUFFER.
//
TEMP_string::TEMP_string (int limit) {
  size = limit;
  clear ();
  return;
}

//--------------------------------
//
//  The core routine, this routine adds a character if the string is
//  not filled.  This routine is called by all other add routines 
//  eventually.
//
//      ch -- Character to add (JIS or ASCII).
//
void TEMP_string::add_char (int ch) {
  if (length >= size) return;
  buffer[length++] = ch;
  return;
}

//--------------------------------
//
//  Add an integer value to the string.  This routine simply formats 
//  the integer value into a temporary string and then calles add_string().
//
//      value -- Value to add.
//      fill  -- Optional parameter, if set to true, the added number
//               will be filled upto two places with leading zeros.
//
void TEMP_string::add_int (int value,int fill) {
  TCHAR temp[100];
  wsprintf (temp,fill ? TEXT("%02d") : TEXT("%d"),value);
  add_string (temp);
  return;
}

//--------------------------------
//
//  Adds a null terminated kanji string to the string.  
//
//      kanji -- Kanji string to add, can be NULL.
//
void TEMP_string::add_kanji (KANJI *kanji) {
  if (!kanji) return;
  while (*kanji) add_char (*kanji++);
  return;
}

//--------------------------------
//
//  This is a very specific routine that adds a special code pattern 
//  to the string.  This is used to add the time and/or date to the 
//  string.  
//
//      pattern -- null terminated pattern string, can be NULL.  In 
//                 this string, & followed by a character has special
//                 values:
//
//          a/A -- AM or PM string.
//          d/D -- Day of the month
//          H   -- Hours in 24 hour format.
//          h   -- Hours in 12 hour format.
//          n/N -- Minutes.
//          m/M -- Month.
//          y   -- Year in two-digit code.
//          Y   -- Year in four-digit number.
//          &   -- &
//
//          ??? -- &?
//
void TEMP_string::add_pattern (KANJI *pattern) {
  SYSTEMTIME time;
  if (!pattern) return;
  GetLocalTime (&time);
  for (; *pattern; pattern++) {
    if (*pattern != '&') add_char (*pattern);
      else {
        switch (*(++pattern)) {
          case 'a':         // AM or PM string.
          case 'A':
               add_kanji ((time.wHour > 12) ? jwp_config.cfg.pm_format : jwp_config.cfg.am_format);
               break;
          case 'd':         // Day of month
          case 'D':
               add_int (time.wDay);
               break;
          case 'H':         // Hour 24-hour format
               add_int (time.wHour);
               break;
          case 'h':         // Hour 12-hour format
               add_int ((time.wHour > 12) ? time.wHour-12 : time.wHour);
               break;
          case 'm':         // Month of year
          case 'M':
               add_int (time.wMonth);
               break;
          case 'n':         // Minuts
          case 'N':
               add_int (time.wMinute,true);
               break;
          case 'Y':         // Year 4-digit
               add_int (time.wYear);
               break;
          case 'y':         // Year 2-digit
               add_int (time.wYear % 100,true);
               break;
          case '&':         // & character
               add_char ('&');
               break;
          default:          // The user is confused!
               add_char ('&');
               add_char (*pattern);
               break;
        }
      }
  }
}

//--------------------------------
//
//  Add an ASCII stirng.
//
//      string -- ASCII string, can be NULL.
//
void TEMP_string::add_string (tchar *string) {
  if (!string) return;
  while (*string) add_char (*string++);
  return;
}

#endif WINCE
//
//  End class TEMP_string.
//
//===================================================================

//===================================================================
//
//  Begin class PrintContext.
//
//  The print context is a structure and class that defines the context
//  of the printer.
//

PrintContext print_context;     // Global print context defines the 
                                // currently selected printer characteristics.

//--------------------------------
//
//  This rotuine initializes a print context, calculating all the 
//  parameters used in printing.
//
//      hdc -- Display context (or Information Context) for the pritner.
//      ps  -- Pointer to a PageSetup object the contains user page
//             defintion parameters.
//
void PrintContext::setup (HDC hdc,class PageSetup *ps) {
//
//  Since we don't support printing on the Windows CE routines, we 
//  simply include a default page setup routine that is used to provide
//  a page width.  Currently page breaks are not indicated within the 
//  text, thus we only need horizontal dimensions.  For setup perposes,
//  we set page width at 8.5", and set the resolution at 300 dots 
//  per inch.
//
#ifndef WINCE
  int top,bottom,left,right;
  xdpi       = GetDeviceCaps(hdc,LOGPIXELSX);           // Resolution
  ydpi       = GetDeviceCaps(hdc,LOGPIXELSY);            
  xmax       = GetDeviceCaps(hdc,PHYSICALWIDTH);        // Physical width
  ymax       = GetDeviceCaps(hdc,PHYSICALHEIGHT);
  top        = NINT(ydpi*ps->top);                      // Margins in printer coordiantes
  bottom     = NINT(ydpi*ps->bottom);
  left       = NINT(xdpi*ps->left);
  right      = NINT(xdpi*ps->right);
  font       = get_printfont(hdc,ps->vertical);         // We will need a kanji font.
  width      = xmax-left-right;                         // Actual printed sizes
  height     = ymax-top -bottom;                        // Kanji font dimensions, with space around characters
  kwidth     = ((jwp_config.cfg.print_font.size)*xdpi*(font->width +font->spacing))/720/font->width;
  kheight    = ((jwp_config.cfg.print_font.size)*ydpi*(font->height+font->leading))/720/font->height;
  fwidth     = ((jwp_config.cfg.print_font.size)*xdpi)/720; // Kanji font dimensions, raw character size
  fheight    = ((jwp_config.cfg.print_font.size)*ydpi)/720;
  xoffset    = left-GetDeviceCaps(hdc,PHYSICALOFFSETX); // Corrections for non-printable area and margins
  yoffset    = top -GetDeviceCaps(hdc,PHYSICALOFFSETY);
  xshift     = (kwidth-fwidth)/2;                       // Kanji character centering.
  char_width = width/kwidth;                            // Number of kanji across page.
#endif WINCE
  return;
}

//
//  End class PrintContext.
//
//===================================================================

//===================================================================
//
//  Begin class PageSetup.
//
//  The PageSetup discribes the page that the user wants, this includes
//  landscape/portrait, vertical print, and margins settings.
//

//--------------------------------
//
//  This routine calculates the kanji width of a page useing the global
//  PrintContext this page setup.
//
//      RETURN -- Width of the page in kanji.
//
int PageSetup::print_width () {
  int i;
  if (landscape) {      // Need to swap all calculations in the case of landscape.
    i = (print_context.kwidth*print_context.ydpi)/print_context.xdpi;
    return ((print_context.ymax-NINT(print_context.ydpi*(left+right)))/i);
  }
  return ((print_context.xmax-NINT(print_context.xdpi*(left+right)))/print_context.kwidth);
}

//
//  End class PrintContext.
//
//===================================================================



#ifndef WINCE
static HANDLE hDevMode  = null;
static HANDLE hDevNames = null;
#endif WINCE




//--------------------------------
//
//  This routine changes or gets the setting on the landscape or 
//  portrait mode for the printer.  Generally the printer context is
//  kept in protrait mode and calculations are converted as needed.
//  When printing, however, the context is generated idrectoy in the 
//  print orientation that will be used.
//
//      mode   -- New mode:
//
//          true  -- landscape
//          false -- portrait
//          -1    -- Do not change mode, just get old mode.
//
//      RETURN -- Old mode.
//
static int getset_landscape (int mode) {
#ifdef WINCE
  static int landscape = false;
  int oldmode = landscape;
  if (mode != -1) landscape = mode;
  return (oldmode);
#else WINCE
  DEVMODE *devmode;
  int oldmode;
  devmode = (DEVMODE *) GlobalLock(hDevMode);
#ifdef WINELIB
  oldmode = (devmode->u1.s1.dmOrientation == DMORIENT_LANDSCAPE);
  if (mode != -1) devmode->u1.s1.dmOrientation = mode ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
#else WINELIB
  oldmode = (devmode->dmOrientation == DMORIENT_LANDSCAPE);
  if (mode != -1) devmode->dmOrientation = mode ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
#endif WINELIB
  GlobalUnlock (hDevMode);
  return (oldmode);
#endif WINCE
} 

//--------------------------------
//
//  Create an Inforamtion Context for the printer specified by the 
//  user.  This will always start as the default printer, with the 
//  default settings, but once the user has changed the printer this 
//  will containue to follow the printer settings.
//
//      RETURN -- An information context to the current printer.
//
#ifndef WINCE
static HDC open_printer () {
  HDC hdc;
  DEVMODE  *devmode;
  DEVNAMES *devnames;
  if (!hDevMode || !hDevNames) {
    JMessageBox (main_window,IDS_PRINT_NOPRINTERS,IDS_PRINT_ERROR,MB_OK);
    return      (null);
  }
  devmode  = (DEVMODE  *) GlobalLock(hDevMode);
  devnames = (DEVNAMES *) GlobalLock(hDevNames);
  hdc = CreateIC (((char *) devnames)+devnames->wDriverOffset,((char *) devnames)+devnames->wDeviceOffset,
                  ((char *) devnames)+devnames->wOutputOffset,devmode);
  GlobalUnlock (hDevMode);
  GlobalUnlock (hDevNames);
  return (hdc);
}
#endif WINCE






//--------------------------------
//
//  Initialzie the printer.
//
void initialize_printer (PageSetup *ps) {
#ifdef WINCE
  print_context.setup (NULL,ps);
#else WINCE
  HDC hdc;

  print_context.char_width = 0;
  PRINTDLG printdlg;
  if (!hDevMode) {
    memset (&printdlg,0,sizeof(printdlg));
    printdlg.lStructSize = sizeof(printdlg);
    printdlg.Flags       = PD_USEDEVMODECOPIESANDCOLLATE | PD_RETURNDEFAULT;
    if (!PrintDlg(&printdlg)) return;
    hDevMode  = printdlg.hDevMode;
    hDevNames = printdlg.hDevNames;
  }
  getset_landscape (false);

  if (!(hdc = open_printer())) return;
  print_context.setup (hdc,ps);
  DeleteDC (hdc);
  return;
#endif WINCE
}




//===================================================================
//
//  Begin printer block.
//
//  These fucntions and data actually perform the printting, not just
//  the setup or general routines.
//

//-------------------------------------------------------------------
//
//  Static data.
//
#ifndef WINCE

static short print_abort;           // Set to non-zero to abort printing.
static HWND  print_dialog = null;   // Pointer to print dialog so we can do page count.

//-------------------------------------------------------------------
//
//  Static procedures.
//
static int print_line (HDC hdc,KANJI *kanji,int length,int xx,int yy,PrintContext *pc,int *widths,int vertical,int end,int noprint);   

//--------------------------------
//
//  Printer's abort procedure.  This is basically a directoy copy of 
//  the standard windows procedures.
//
static BOOL CALLBACK AbortProc (HDC hdc,int icode) {
  MSG msg;
  while (!print_abort && PeekMessage(&msg,null,0,0,PM_REMOVE)) {
    if (!print_dialog || !IsDialogMessage(print_dialog,&msg)) {
      TranslateMessage (&msg);
      DispatchMessage  (&msg);
    }
  }
  return (!print_abort);
}

//--------------------------------
//
//  This is the dialog box procedure for the print status dialog box.
//  This simply allows the user to cancle the prining and provides a 
//  status display.
//
//      IDC_PSNAME     Id of the file name.
//      IDC_PSMESSAGE  Id of the message (generally page number).
//
static BOOL CALLBACK printing_dialog (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG:
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_PRINT_PRINTING);
         return  (true);
    case WM_COMMAND:        // Only one button, so we can only abort.
         SetDlgItemText (hwnd,IDC_PSMESSAGE,get_string(IDS_PRINT_ABORT));
         print_abort = true;
         return (true);
  }
  return (false);
}
#endif WINCE

//-------------------------------------------------------------------
//
//  JWP_file class rotuines.
//

//--------------------------------
//
//  This is is the main print rotuine.  This handles both the print 
//  command and the printer setup command.
//
//  This routine does both setup and printing.  For printing, we 
//  allow the following print boundaries to be used.  We can print by 
//  page limits.  If the user has a selected region, the pages indicated
//  by the user's selection will be setup in the page limits.  
//
//  The user can also print just the selcted region.  This will print
//  simply the selected region.  In this case the page limits will not
//  be set correclty and the header/footer lines may be wrong.
//
//  When printing by page limits, the program actually starts at page 
//  1 and prints trought the file, but all actual printing is suppresed
//  until we pass the first page.  After we pass the last page printing
//  is aborted.
//
//  When printing a selection we start directly at the beginning of 
//  the selection, and abort after the last line is output.
//
//      setup -- If this flag is non-zero this routine does a printer
//               setup instead of a print command.
//
void JWP_file::print (int setup) {
#ifndef WINCE
  int i,j;
  Paragraph   *para;            // For scanning through paragraphs
  DOCINFO      docinfo;         // Windows document info structure
  Line        *line;            // For scanning through lines.
  Line        *line_start;      // Start line for printing selections
  Line        *line_end;        // End line for printing selections.
  int          pagebreak;       // Flag indicating need to make a page break.
  int          page_count;      // Current printing page.
  int          x,y=0;
  int          colorkanji;      // Holds color kanji setting to restore after printing.
  int          error = true;    // Cleared when system exits print loop correctly.
  PRINTDLG     printdlg;        // Structre for print dialog.
  PrintContext pc;              // JWPce printer context structure for a real print job.
  TCHAR        buffer[100];     // Message buffer.
//
//  Abort if there are no printers.
//
  if (!hDevMode || !hDevNames) {
    JMessageBox (main_window,IDS_PRINT_NOPRINTERS,IDS_PRINT_ERROR,MB_OK);
    return;
  }
//
//  Setup printer dialog box structure.
//
  memset (&printdlg,0,sizeof(printdlg));
  printdlg.lStructSize = sizeof(printdlg);
  printdlg.hDevMode    = hDevMode;
  printdlg.hDevNames   = hDevNames;
  printdlg.hwndOwner   = main_window;
  printdlg.hInstance   = instance;
  printdlg.Flags       = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE;
  if (!sel.type) printdlg.Flags |= PD_NOSELECTION; else printdlg.Flags |= PD_PAGENUMS;
  if (setup    ) printdlg.Flags |= PD_PRINTSETUP;
//
//  Determine the page limits for the print job.
//
  if (!setup) {
    HDC hdc;                                            // Generate a new HDC and intialize the 
    if (!(hdc = open_printer())) return;                //   print structures so we can count the 
    pc.setup (hdc,&page);                               //   number of pages.
    DeleteDC (hdc);                                     
    char_pagewidth = page.print_width();                // Format the file to the printer printer 
    reformat ();
    para       = first;                                 // Move thorugh file as in printting to 
    line       = para->first;                           //   count pages and locate the selected region
    pagebreak  = true;                                  // Forces a page on the first output.
    page_count = 0;                                     // Reset page count.
    while (true) {                                      // Print line.s
      if (pagebreak) {                                  // Do page breaks
        pagebreak = false;                              //   clear page flag
        y = 0;                                          //   reset vertical position after page.
        page_count++;                                   //   updage page counter.
      }
      if (line == sel.pos1.line) printdlg.nFromPage = page_count;   // Pickup the pages containning the selection.
      if (line == sel.pos2.line) printdlg.nToPage   = page_count;
      y += (pc.kheight*para->spacing)/100;              // Update verical position.
      if (y+pc.kheight > pc.height) pagebreak = true;   // Check for page.
      line = line->next;                                // Next line!
      while (!line) {                                   // Do we need to go to next paragraph.
        para = para->next;                              // Advance to next line.
        if (!para) goto CountDone;                      // End of the file.
        line = para->first;
        if (para->page_break) { pagebreak = true; line = NULL; }
      }
    }
CountDone:;
    printdlg.nMinPage  = 1;                             // Setup page limits.
    printdlg.nMaxPage  = page_count;
    if (!sel.type) {                                    // No selection, then set print limtis
      printdlg.nFromPage = 1;                           //   at the extreams of the file.
      printdlg.nToPage   = page_count;
    }
  }
//
//  Generate Printer setup dialog box.
//
  getset_landscape (page.landscape);                    // Set landscape/portait mode.
  if (!PrintDlg(&printdlg)) return;                     // Generate dialog box.
  if (!(printdlg.Flags & PD_PAGENUMS)) {                // If selection, set page limits to the
    printdlg.nFromPage = 1;                             //   selection points.
    printdlg.nToPage   = 0x7fff;
  }
//
//  We have a printer setup, and how have a display context so lets 
//  setup the print.
//
  hDevMode  = printdlg.hDevMode;        // Save settings the user selected for printer
  hDevNames = printdlg.hDevNames;
  pc.setup (printdlg.hDC,&page);        // Setup the print context.
//
//  Did the user change to landscape mode.
//
  i = page.landscape;
  page.landscape = getset_landscape (false);
  if (i != page.landscape) change ();
//
//  Adjust the global print context to match the user's
//
  initialize_printer (&page);
  pc.setup (printdlg.hDC,&page);
//
//  If this this a setup box, we are done and can exit.
//
  if (setup) {
    DeleteDC (printdlg.hDC);
    activate ();
    return;
  }
//
//  User may have changed the paper size, etc, so reformat the document
//  to match the paper size the user chose.
//
  char_pagewidth = page.print_width();
  reformat ();
//
//  Build a printer version of the display font.
//
  HFONT      font;              // Our printer font.
  HFONT      oldfont;           // Original context font for end.
  LOGFONT    lf;                // Used to make the font.
  ABC        abc[256];          // Used to get font width.
  int        widths[256];       // Calculated font widths in device units.
  memset (&lf,0,sizeof(lf));    // Create the font.
  strcpy (lf.lfFaceName,jwp_config.cfg.ascii_font.name);
  lf.lfHeight = -pc.fheight;
  if ((font = CreateFontIndirect(&lf))) {	            // Did we get the font!
//  
//  Allocated the font so lets get some data on the font.
//
    oldfont = (HFONT) SelectObject(printdlg.hDC,font);	// Save old font for end.
    GetCharABCWidths (printdlg.hDC,0,255,abc);			// Get character widths.
    for (i = 0; i < 255; i++) widths[i] = (short) (abc[i].abcA+abc[i].abcB+abc[i].abcC);
//
//  Setup print job name, and handle case of print to file.
//
    DEVNAMES  *devnames;          
    memset (&docinfo,0,sizeof(docinfo));
    docinfo.cbSize      = sizeof(docinfo);
    docinfo.lpszDocName = name;							// Use file name as print job name.
    devnames = (DEVNAMES *) GlobalLock(hDevNames);
    docinfo.lpszOutput = ((char *) devnames)+devnames->wOutputOffset;
//###    if (printdlg.Flags & PD_PRINTTOFILE) docinfo.lpszOutput = "FILE:";  // Do print to file
//### Not really happy that we get an old style dailog box for the FILE: printer.
//
//
//===========================================
//
//  This is the real printting loop.
//
    EnableWindow (main_window,false);                   // Dissalbe input to us.
    colorkanji   = jwp_config.cfg.colorkanji_mode;      // Save old color-kanji mdoe
    if (!jwp_config.cfg.colorkanji_print) jwp_config.cfg.colorkanji_mode = COLORKANJI_OFF;  
    print_abort  = false;
    print_dialog = JCreateDialog(IDD_PRINTING,main_window,(DLGPROC) printing_dialog);
    SetDlgItemText (print_dialog,IDC_PSNAME,name);
    SetAbortProc (printdlg.hDC,(ABORTPROC) AbortProc);  // Setup abort proc.
    if (StartDoc(printdlg.hDC,&docinfo) > 0) {          // Start the document.
      if (printdlg.Flags & PD_SELECTION) {              // User wants to print selection.
        para       = sel.pos1.para;                 
        line       = sel.pos1.line;
        line_start = sel.pos1.line;                     // Setup limits used by program.
        line_end   = sel.pos2.line;
      }
      else {                                            // User wants to print pages or text.
        para       = first;                             //   Thus we start at top and suppress
        line       = para->first;                       //   printing until we get to the pages.
        line_start = NULL;                              // Disable selection limits.
        line_end   = NULL;
      }
      pagebreak  = true;                                // Forces a page on the first output.
      page_count = 0;                                   // Curent page is zero.
      while (true) {                                    // Print line.s
//
//  New page block!
//
#define HEAD_TOP    (((100+jwp_config.cfg.head_top   )*pc.kheight)/100)
#define HEAD_BOTTOM (((    jwp_config.cfg.head_bottom)*pc.kheight)/100)

        if (pagebreak) {                                // Do page breaks
          pagebreak = false;
          y = 0;                                        //   reset vertical position after page.
          SetDlgItemText (print_dialog,IDC_PSMESSAGE,format_string(buffer,IDS_PRINT_PAGE,++page_count));
          if (page_count >= printdlg.nFromPage) {       //   suppress printing if not in range.
            if (StartPage(printdlg.hDC) < 0) goto CleanUp; 
            SelectObject (printdlg.hDC,font);
            if (!no_first || (page_count != 1)) {
              if (odd_even && !(page_count & 0x1)) j = 1; else j = 0;
              for (i = 0; i < NUMBER_POSITIONS; i++) {    
                print_heads (printdlg.hDC,j  ,pc.yoffset          -HEAD_TOP   ,&pc,widths,page_count);
                print_heads (printdlg.hDC,j+2,pc.yoffset+pc.height+HEAD_BOTTOM,&pc,widths,page_count);
              }
            }
          }
        }
//
//  Print line
//
        if (page_count >= printdlg.nFromPage) {         //   If we are in valid page area print the lines.
          KANJI *ptr;
          if (line == para->first) x = para->indent_first; else x = para->indent_left;
          x = pc.kwidth*x+pc.xoffset;                   //   process indents for line.
          ptr = para->text+line->first;                 //   printed text.
          i   = line->length;                           //   length
          if (line == line_end) i = sel.pos2.pos;       //   truncate last line of selection.
          if (line == line_start) {                     //   shift in for first line of selection.
            ptr += sel.pos1.pos;
            i   -= sel.pos1.pos;
          }
          print_line (printdlg.hDC,ptr,i,x,y+pc.yoffset,&pc,widths,page.vertical,(line == para->last) || (line == line_end),false);
          if (line == line_end) goto PrintDone;         //   end of selection we are done!
        }
//
//  Check for abort.
//
        AbortProc (printdlg.hDC,0);                     // We have to check our selves.
        if (print_abort) {                              // Have an abort so get out of here
          error = false;                                // User aborted the print so exit whiout error.
          AbortDoc (printdlg.hDC);
          goto CleanUp;
        }
//
//  Move to next line.
//
        y += (pc.kheight*para->spacing)/100;            // Update verical position.
        if (y+pc.kheight > pc.height) pagebreak = true; // Check for page.
        line = line->next;                              // Next line!
        while (!line) {                                 // Do we need to go to next paragraph.
          para = para->next;
          if (!para) goto PrintDone;                    // End of file.
          line = para->first;
          if (para->page_break) { pagebreak = true; line = NULL; }
        }
//
//  End of page
//
        if (pagebreak && (page_count >= printdlg.nToPage)) goto PrintDone;     // End of print region.
        if (pagebreak && (page_count >= printdlg.nFromPage) && (EndPage(printdlg.hDC) < 0)) goto CleanUp;

      }
//
//  Non-error exit.
//
PrintDone:;
      if (EndPage(printdlg.hDC) < 0) goto CleanUp;
      EndDoc (printdlg.hDC);
      error = false;
    }
//
//  Deallocate fonts and other things.
//
CleanUp:;
    GlobalUnlock  (hDevNames);                      // Unlock the output device
    jwp_config.cfg.colorkanji_mode = colorkanji;    // Restore color-kanji.
    SelectObject  (printdlg.hDC,oldfont);           // Restore font
    DeleteObject  (font);                           // Deallocate font.
    EnableWindow  (main_window,true);               // Enable the main window.
    DestroyWindow (print_dialog);                   // Get rid of the print dialog.
    print_dialog = null;                    
  }
  if (error) ErrorMessage (true,IDS_PRINT_ERRORTEXT,name);
  DeleteDC (printdlg.hDC);                  // Done with printer DC
  activate ();                              // Reformat document back to how it belongs.
  return;
#endif WINCE
}

//--------------------------------
//
//  Prints the header or futter lines. for this page.
//
//      hdc    -- Display Context for the printer.
//      head   -- Header or footer set to print.
//      y      -- Page height of the header or footer.
//      pc     -- Print context that contains printer configuration.
//      widths -- Character widths table (passed through to other rotuines).
//      page   -- Page number (for &P code).
//  
#ifndef WINCE
void JWP_file::print_heads (HDC hdc,int head,int y,PrintContext *pc,int *widths,int page_count) {
  int   just,width,x;
  KANJI *kanji;
  TEMP_string string(SIZE_BUFFER);
  for (just = 0; just < NUMBER_POSITIONS; just++) {     // Loop over the three positions.
    kanji = headers[head][just].kanji;                  // Get header string.
    if (!kanji) continue;                               // No string so skip
    string.clear ();                                    // Clear contents of string.
    for (; *kanji; kanji++) {                           // Move thorugh string.
      if (*kanji != '&') string.add_char (*kanji);      //   Simple character add
        else {                                          //   & Codes
          switch (toupper(*++kanji)) {
            case 'A':       // Author
                 string.add_kanji (summary[SUMMARY_AUTHOR].kanji);
                 break;
            case 'C':       // Comment
                 string.add_kanji (summary[SUMMARY_COMMENT].kanji);
                 break;
            case 'D':       // Date
                 string.add_pattern (jwp_config.cfg.date_format);
                 break;
            case 'F':       // File Name and Path
                 string.add_string (name);
                 break;
            case 'K':       // Keywords
                 string.add_kanji (summary[SUMMARY_KEYWORDS].kanji);
                 break;
            case 'L':       // Title
                 string.add_kanji (summary[SUMMARY_TITLE].kanji);
                 break;
            case 'N':       // File Name
                 int i;
                 for (i = lstrlen(name)-1; i >= 0; i--) if (name[i] == '\\') break;
                 string.add_string (name+i+1);
                 break;
            case 'S':       // Subject
                 string.add_kanji (summary[SUMMARY_SUBJECT].kanji);
                 break;
            case 'T':       // Time
                 string.add_pattern (jwp_config.cfg.time_format);
                 break;  
            case 'P':       // Page
                 string.add_int (page_count);
                 break;
            case '&':       // &
                 string.add_char ('&');
                 break;
            default:        // User error
                 string.add_char ('&');
                 string.add_char (*kanji);
                 break;
          }
        }
    }
//
//  Adjust position and print.
//
#define HEAD_LEFT   ((pc->kwidth*(jwp_config.cfg.head_left))/100)
#define HEAD_WIDTH  ((pc->kwidth*(jwp_config.cfg.head_left+jwp_config.cfg.head_right))/100)
    width = print_line (hdc,string.buffer,string.length,0,y,pc,widths,page.vertical,true,true);
    x = pc->xoffset-HEAD_LEFT+((pc->char_width*pc->kwidth+HEAD_WIDTH-width)*just)/2;
    print_line (hdc,string.buffer,string.length,x,y,pc,widths,page.vertical,true,false);
  }
  return;
}
#endif WINCE







//--------------------------------
//
//  This routine prints a line of text at a specified location on the 
//  page.  This is the basis of all the print operations.
//
//      hdc      -- Display Context for the printer.
//      kanji    -- Pointer to line to print.
//      length   -- Length of the line.
//      xx,yy    -- Location relative the printable region of the page
//                  of the upper left corner of the text.  These form 
//                  the basis of all locations.
//      pc       -- Print context for this print job.
//      widths   -- Character widths table.
//      vertical -- Non-zero for vertical printing
//      end      -- Indicates this is the last line in the paragraph, and thus has special formatting.
//      noprint  -- Suppresses the printing (used for determining the length of a line).
//
//      RETURN   -- Width of the line in printer pixels.
//
#ifndef WINCE
static int print_line (HDC hdc,KANJI *kanji,int length,int xx,int yy,PrintContext *pc,int *widths,int vertical,int end,int noprint) {
  int ch,i,x;
  RECT rect;
  rect.top    = yy;                             // Setup for line output
  rect.bottom = rect.top+pc->fheight;           //   vertical parameters for line.
  for (x = i = 0; i < length; i++) {            //   process characters
    ch = kanji[i];
    if (ISJIS(ch)) {                            //   JIS character
      if (!noprint) {
        rect.left  = x+xx+pc->xshift;
        rect.right = rect.left+pc->fwidth;
        pc->font->fill (hdc,ch,&rect);  
      }
      x += pc->kwidth;
    }
    else if (ch != '\t') {                      //   ASCII character
//
//  Determine the amount of space that will be added at the end of
//  the ASCII text, to bring the line to the next kanji location.
//
      int j,spaces,x2;
      x2     = x;
      spaces = 2;                               // Two adjustment sapces (begin and end)
      for (j = i; !ISJIS(kanji[j]) && ('\t' != kanji[j]) && (j < length); j++) {
        x2 += widths[kanji[j]];      
        if (kanji[j] == ' ') spaces++;
      }
      x2 = (x2/pc->kwidth+1)*pc->kwidth-x2;
//
//  If ASCII text end on kanji whitout tab, then no adjustments (x2 = 0)
//  If justification is disabled set x2 = 0;
//  If ASCII text ends at the end of the line, and we have one word, always center.
//
      if (((j != length) || end) && (kanji[j] != '\t')) x2 = 0;
      if (!jwp_config.cfg.print_justify) x2 = 0;
      if ((j == length) && (spaces != 2)) spaces--;
//
//  Printing loop for ascii.
//
#define ADD_SPACE(i)    (((x2*(i+1))/spaces)-((x2*i)/spaces))
      j  = 1;
      x += ADD_SPACE(0);                        // Initial space.
      while ((i < length) && !ISJIS(kanji[i]) && ('\t' != kanji[i])) {
        ch = kanji[i];
        if (!noprint) {
          ascii_draw (hdc,x+xx,yy,ch);         // Output character
        }
        x += widths[ch];                        // Advance location
        if (ch == ' ') {
          x += ADD_SPACE(j);                    // Extra advance for space character
          j++;
        }
        i++;                                    // Next character
      }
      i--;                                      // Backup one character for main loop.
    }
//
//  TAB character
//
    else {                                      //   TAB character.
      x = (x/pc->kwidth+1)*pc->kwidth;
    }
  }
  return (x);
}
#endif

//--------------------------------
//
//  Constructor
//
PrintContext::PrintContext () {
//
//  Since we don't support printing on the Windows CE routines, we 
//  simply include a default page setup routine that is used to provide
//  a page width.  Currently page breaks are not indicated within the 
//  text, thus we only need horizontal dimensions.  For setup perposes,
//  we set page width at 8.5", and set the resolution at 300 dots 
//  per inch.
//
  int left,right;
  xdpi       = 300;                                     // Resolution
//ydpi       = 300;
  xmax       = 3*850;                                   // Physical width
  ymax       = 3*1100;
//top        = ydpi;                                    // Margins in printer coordiantes
//bottom     = ydpi;
  left       = xdpi;
  right      = xdpi;
  font       = file_font.kanji;                         // We will need a kanji font.
  width      = xmax-left-right;                         // Actual printed sizes
//height     = ymax-top -bottom;                        // Kanji font dimensions, with space around characters
  kwidth     = (120*xdpi*(16+2))/720/16;
//kheight    = (120*ydpi*(16+2*2))/720/16;
//fwidth     = (120*xdpi)/720;                          // Kanji font dimensions, raw character size
//fheight    = (120*ydpi)/720;
//xoffset    = left;                                    // Corrections for non-printable area and margins
//yoffset    = top;
//xshift     = (kwidth-fwidth)/2;                       // Kanji character centering.
  char_width = width/kwidth;                            // Number of kanji across page.
  return;
}
