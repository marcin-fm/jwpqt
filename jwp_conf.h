//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//===================================================================//

#ifndef jwp_conf_h
#define jwp_conf_h
#include "jwp_prnt.h"
#include "jwp_misc.h"

#ifdef WINCE
#define QUIET_ERROR     if (true)                       // No quiet errors for Windows CE
#else  WINCE
#define QUIET_ERROR     if (!jwp_config.quiet_errors)   // Block error message if quiet erros has been selected.
#endif WINCE

#define SIZE_NAME       40
#define SIZE_DATE       20
#define SIZE_AMPM       10

#define OPENREAD(f)     CreateFile (f,GENERIC_READ ,FILE_SHARE_READ,null,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null)
#define OPENWRITE(f)    CreateFile (f,GENERIC_WRITE,0              ,null,OPEN_ALWAYS  ,FILE_ATTRIBUTE_NORMAL,null)
#define OPENAPPEND(f)   CreateFile (f,GENERIC_WRITE,0              ,null,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null)
#define OPENNEW(f)      CreateFile (f,GENERIC_WRITE,0              ,null,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,null)

#define OPEN_READ   0
#define OPEN_WRITE  1
#define OPEN_APPEND 2
#define OPEN_NEW    3

#define COLORKANJI_OFF      0   // Disables the kanji font coloring.
#define COLORKANJI_MATCH    1   // Colors kanji that match the list.
#define COLORKANJI_NOMATCH  2   // Colors kanji that don't match the list.

#define DOUBLE_OPEN         0   // Open the file as requested.
#define DOUBLE_CHANGE       1   // Change to the already open file.
#define DOUBLE_PROMPT       2   // Prompt the user.

#define WIDTH_DYNAMIC       0   // Use dynamic width (adjust to screen width).
#define WIDTH_FIXED         1   // Use fixed with mode.
#define WIDTH_PRINTER       2   // Use printer width mode.

#define IME_OFF             0   // IME is disabled, only JWPce's input system is used.
#define IME_MIXED           1   // Mode uses the IME, but ASCII text is pass through JWPce's input processor
#define IME_ON              2   // Replace JWPce's input sytem with the IME.
#define IME_FULL            3   // Allows extended IME support.

#define CODEPAGE_AUTO       0       // Automatically determine the code page.
#define CODEPAGE_EASTEUROPE 1250    // Easter Europ code page
#define CODEPAGE_CYRILLIC   1251    // Cyrillic code page
#define CODEPAGE_USA        1252    // USA and Western Europe code page
#define CODEPAGE_GREEK      1253    // Greek code page
#define CODEPAGE_TURKISH    1254    // Turkish code page
#define CODEPAGE_HEBREW     1255    // Hebrew code page
#define CODEPAGE_ARABIC     1256    // Arabic code page
#define CODEPAGE_BALTIC     1257    // Baltic code page
#define CODEPAGE_VIETNAMESE 1258    // Vietnamese code page

#ifdef WINCE
  #define CONFIG_MAGIC  0xCE72C536  // Magic ID for JWP config files.
#else  WINCE
  #define CONFIG_MAGIC  0xBE72C536  // Magic ID for JWP config files.
#endif WINCE

//--------------------------------
//
//  Structure for storing font information.
//
struct cfg_font {
  TCHAR name[SIZE_NAME];        // Name of font
  short size;                   // Size of font
  byte  automatic;              // Font is automatically sized.
  byte  vertical;               // Extra byte for who knows what.
};

//--------------------------------
//
//  Configuration for storing in a file.
//
//  Configuration changes for next major revisition:
//
//      1)  Change nokanjibar to kanjibar and move to display section.
//
struct cfg {
  ulong magic;                  // Identifies this as a JWPce config file.
  long  size;                   // Size of structrue.
  PrintSetup page;              // Default printer setup.
  struct cfg_font ascii_font;   // Ascii system font.
  struct cfg_font sys_font;     // System font used for text and a few other places.
  struct cfg_font list_font;    // Font for lists.
  struct cfg_font edit_font;    // Font used for Japanese edit constrols.
  struct cfg_font bar_font;     // Font used for kanji bars.
  struct cfg_font file_font;    // Font used for editing files.
  struct cfg_font big_font;     // Font used for big text.
  struct cfg_font jis_font;     // Font used for JIS table.
  struct cfg_font clip_font;    // Font used for clipboard bitmapps.
  struct cfg_font print_font;   // Font used for printing.
  struct cfg_font extra_font;   // Extra font for later
  struct cfg_font extra_font2;  // Another extra font.
  struct size_window size_dict; // Size of dictionary window.
  struct size_window size_user; // Size of user dictionary window;
  struct size_window size_oount;// Size of count kanji window.
  struct size_window size_cnvrt;// Size of user kana->kanji conversions window.
  struct size_window size_info; // Size of info dialog.
  struct size_window size_more; // Size of more info dialog.
  struct size_window size_fill; // Unused size structure for later
  int   x,y,xs,ys;              // Dimensions of last saved configuration.
  int   dict_buffer;            // Size of dictionary buffer.
  COLORREF info_color;          // Color used for titles in kanji-info box.
  COLORREF colorkanji_color;    // Color to be used with color-kanji.
  long  dict_bits;              // Stores the state of all dictionary bits in one place.
  short alloc;                  // Allocation size for lines.
  short convert_size;           // Number of entires in user conversion table.
  short char_width;             // Character width for formatinning.
  short undo_number;            // Number of levels of undo to keep.
  short font_cache;             // Size of font cache in characters.
  short head_left;              // Position of headers to the left of margins
  short head_right;             // Position of headers to the right of margins.
  short head_top;               // Position of header lines above margins.
  short head_bottom;            // Position of header lines below margins.
  short scroll_speed;           // Determines the scroll speed.
  short history_size;           // Size of history buffer (in characters).
  KANJI date_format[SIZE_DATE]; // Date format string.
  KANJI time_format[SIZE_DATE]; // Time format string.
  KANJI am_format[SIZE_AMPM];   // AM format string.
  KANJI pm_format[SIZE_AMPM];   // PM format string.
  byte  buttons[100];           // Buttons for the button bar.
  byte  kanji_info[60];         // Character Information dialog items
//
//  Dictionary flags.
//
  byte  dict_compress;          // Displays diconary search results in compressed form.
  byte  dict_auto;              // Automatically attempt a search if the user has selected text.
  byte  dict_advanced;          // Use addaptive dictionary search.
  byte  dict_iadj;              // Process i-adjitives.
  byte  dict_always;            // Even if choices are found do an addpative search.
  byte  dict_showall;           // Show all possible choices in an addaptive search.
  byte  dict_advmark;           // Separate advanced search entries.
  byte  dict_watchclip;         // Watch clipboard when dictionary is open
  byte  dict_classical;         // Classical dictionary search
  byte  dict_primaryfirst;      // Move primary entries to the front the dictionary display.
  byte  dict_fullascii;         // Causes first/last to select complete entry for ascii 
  byte  dict_jascii2ascii;      // Treat JASCII as ascii;
//
//  Startup flags
//
  byte  install;                // If set causes check for installed version and file extensions.
  byte  maximize;               // Maximaize the file.
  byte  usedims;                // Use last saved dimensions.
  byte  save_exit;              // Save configuration on exit.     
  byte  reload_files;           // Reload files loaded when we exited.
//
//  Display flags
//
  byte  vscroll,hscroll;        // Vertical and horizontal scroll bar.
  byte  kscroll;                // Activate scroll bar on bar.
  byte  kanjibar_top;           // Places the kanji bar at the top of the screen
  byte  status;                 // Display status bar.
  byte  toolbar;                // Disable the toolbar.
  byte  button_count;           // Number of buttons in the toolbar
//
//  Basic operations flags
//
  byte  confirm_exit;           // Require confirmation of exit on closing last file.
  byte  close_does_file;        // Window close control, closes just current file.
  byte  backup_files;           // Save last version of a file as a backup.
  byte  double_open;            // Determine the action in the case of a double open.
  byte  ime_mode;               // Determines JWPce's interaction with the Microsoft IME
  byte  fill_unused;            // Causes the delete key to delete current kanji conversion instead of text to right (old action).
  byte  auto_scroll;            // Enables or disables the auto-scroll feature.
  byte  page_mode_file;         // Uses page scrolling for the file (PPC only)
  byte  page_mode_list;         // Uses page scrolling for lists (PPC only)
//
//  Clipboard flags
//
  byte  clip_write;             // Clipboard write type.
  byte  clip_read;              // Clipboard read type.
  byte  no_BITMAP;              // Suppress BITMAP clipboard format
  byte  no_UNICODETEXT;         // Suppress UNICODETEXT clipboard format
//
//  Search flags
//
  byte  search_nocase;          // Search: Ignore case
  byte  search_jascii;          // Search: JASCII=ascii
  byte  search_back;            // Search: Move backward (check jwp_find.cpp to see if this is active)
  byte  search_wrap;            // Search: Wrap at end of file. 
  byte  search_all;             // Search: All files.
  byte  search_noconfirm;       // Repalce: Without confirmation. (check jwp_find.cpp to see if this is active)
  byte  keep_find;              // Causes the Search/Replace dialog to remain open during searches.
//
//  Insert to file flags
//
  byte  paste_newpara;          // When pasting back in the file insert extra lines into new paragraph.
//
//  Formatting and printing
//
  byte  relax_punctuation;      // Allow relaxed punctuation.
  byte  relax_smallkana;        // Allow relaxed small kana.
  byte  width_mode;             // Determines how the width of the display is calculated.
  byte  print_justify;          // Justify ASCII text.
  byte  units_cm;               // CM units (or inches).
//
//  Info and Color Kanji flags
//
  byte  info_compress;          // Compress Character information.
  byte  info_titles;            // Puts titles in the kanji-info list box.
  byte  info_onlyone;           // Allows only one info version to open.
  byte  cache_info;             // Fill for later exapnsion
  byte  colorkanji_mode;        // Determines the way color-fonts are suported.
  byte  colorkanji_bitmap;      // Support color kanji in bitmap clipboard format
  byte  colorkanji_print;       // Support color kanji in printing.
//
//  Kanji lookup flags
//
  byte  auto_lookup;            // Should we do auto-lookups in the radical lookup dialog.
  byte  skip_misscodes;         // Search for skip miss-codes.
  byte  bushu_nelson;           // Search for Nelson bushu
  byte  bushu_classical;        // Search for classicla bushu
  byte  index_type;             // Index type for index search.
  byte  reading_type;           // Reading type for reading search
  byte  reading_kun;            // Allow flexable kun readings.
  byte  reading_word;           // Allow flexable word matching.
  byte  no_variants;            // Suppresses showing of variants in radical lookups.
//
//  Font flags
//
  byte  cache_displayfont;      // Should we cache or not cache the display font.
  byte  all_fonts;              // Show all fonts in the font selector
//
//  Added
//
  byte  nokanjibar;             // Diables the kanji bar.
  short code_page;              // Code page used for translations
//
//  Fill
//
  short fill1;
  byte  fill[252];              // Fill for later expansion.
};

//--------------------------------
//
//  Configuration class
//
class JWP_config {
public:
  struct cfg cfg;
  TCHAR *load;              // Pointer to the data on previously loaded files.
  byte  insert;             // Insert mode;
  byte  mode;               // Input mode;
  byte  global_effect;      // Turns local (paragraph) base effects into global (file) base effects.
  byte  quiet_errors;       // Flag for quiet error processing.
  short commandbar_height;  // Height of command bar
  long  kanji_flags;        // Flags indicating what is in the kanji info database.
private:
  byte   ok;                // Disk based configuration is valid.
  TCHAR  buffer[256];       // Name of configuration file.
  TCHAR  nbuffer[256];      // Buffer for network names
  TCHAR *ptr;               // Pointer to end of buffer base.
  TCHAR *nptr;              // Pointer to end of network name
  TCHAR *last_name;         // Pointer to last name generated.
public:
  JWP_config         (void);                                        // Constructor.
  void  done         (void) { if (!ok || cfg.save_exit) write(); }  // Called during exit to write configuration.
  HANDLE open        (TCHAR *name,int mode,int net);                // Open a file based on configurations settings.
  TCHAR *name        (TCHAR *file,int mode,int net);                // Generate file name.
  TCHAR inline *name (void) { return (last_name); }                 // Get last name generated by program.
  int   read         (void);                                        // Reader routine.
  void  set          (struct cfg *new_config);                      // Set a configuration.
  void  write        (void);                                        // Write configuration file
  void  write_files  (void);                                        // Write files information to the configuation file.
};

extern class JWP_config jwp_config;

#endif jwp_conf_h
