//-------------------------------------------------------------------//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998,1999,2000.             //
//  All rights reserved.                                             //
//                                                                   //
//-------------------------------------------------------------------//

#ifndef jwp_conf_h
#define jwp_conf_h
#include "jwp_prnt.h"

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

#define EXPORT_PARAGRAPH    0   // Exported lines are long lines containning a paragraph.
#define EXPORT_FORMAT       1   // Export lines as formated in the file.
#define EXPORT_FIXED        2   // Export lines with a fixed format length.

#define WIDTH_DYNAMIC       0   // Use dynamic width (adjust to screen width).
#define WIDTH_FIXED         1   // Use fixed with mode.
#define WIDTH_PRINTER       2   // Use printer width mode.

#define IME_OFF             0   // IME is disabled, only JWPce's input system is used.
#define IME_MIXED           1   // Mode uses the IME, but ASCII text is pass through JWPce's input processor
#define IME_ON              2   // Replace JWPce's input sytem with the IME.
#define IME_FULL            3   // Allows extended IME support.

#ifdef WINCE
  #define CONFIG_MAGIC  0xCE72C522  // Magic ID for JWP config files.
#else  WINCE
  #define CONFIG_MAGIC  0xBE72C522  // Magic ID for JWP config files.
#endif WINCE

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//
//  Things to be fixed if I ever change the config file format again.
//
//  1. Toolbar configuration stroed in JWP_tool::process() should be removed.
//
//  2. Reorder parameters to make sense.
//
//  3. Unused fields:
//
//      export_crlf
//      export_lines
//      export_length
//      dict_nobeep
//      dict_excludeme
//
//  4. Reverse no_toolbar -> toolbar.
//
//  5. Get rid of double exclusion code in both bushu-lookups (handles if not classical
//     and not Nelson).  With new configuration this would not be possible.
//
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


struct cfg {
  ulong magic;                  // Identifies this as a JWPce config file.
  long  dict_bits;              // Stores the state of all dictionary bits in one place.
  short size;                   // Size of structrue.
  byte  dict_compress;          // Displays diconary search results in compressed form.
  byte  dict_auto;              // Automatically attempt a search if the user has selected text.
  byte  dict_edict;             // Search EDICT dictionary.
  byte  dict_namdict;           // Search NAMDICT dictionary.
  byte  dict_user;              // Search user dictionary.
  byte  dict_quiet;             // Quiety handle error in NAMDICT.
  byte  dict_advanced;          // Use addaptive dictionary search.
  byte  dict_iadj;              // Process i-adjitives.
  byte  dict_always;            // Even if choices are found do an addpative search.
  byte  dict_showall;           // Show all possible choices in an addaptive search.
  byte  install;                // If set causes check for installed version and file extensions.
  byte  maximize;               // Maximaize the file.
  byte  usedims;                // Use last saved dimensions.
  byte  save_exit;              // Save configuration on exit.     
  byte  reload_files;           // Reload files loaded when we exited.
  byte  vscroll,hscroll;        // Vertical and horizontal scroll bar.
  byte  kscroll;                // Activate scroll bar on bar.
  byte  kanjibar_top;           // Places the kanji bar at the top of the screen
  byte  status;                 // Display status bar.
  byte  confirm_exit;           // Require confirmation of exit on closing last file.
  byte  close_does_file;        // Window close control, closes just current file.
  byte  backup_files;           // Save last version of a file as a backup.
  byte  double_open;            // Determine the action in the case of a double open.
  byte  clip_write;             // Clipboard write type.
  byte  clip_read;              // Clipboard read type.
  byte  export_crlf;            // Export files with cr-fl pair (DOS format). NOT CURRENTLY USED!
  byte  export_lines;           // How exported files should be processed. NOT CURRENTLY USED!
  byte  search_nocase;          // Search: Ignore case
  byte  search_jascii;          // Search: JASCII=ascii
  byte  search_back;            // Search: Move backward (check jwp_find.cpp to see if this is active)
  byte  search_wrap;            // Search: Wrap at end of file. 
  byte  search_all;             // Search: All files.
  byte  search_noconfirm;       // Repalce: Without confirmation. (check jwp_find.cpp to see if this is active)
  byte  paste_newpara;          // When pasting back in the file insert extra lines into new paragraph.
  byte  relax_punctuation;      // Allow relaxed punctuation.
  byte  relax_smallkana;        // Allow relaxed small kana.
  byte  width_mode;             // Determines how the width of the display is calculated.
  byte  print_autofont;         // Auto font select for printing.
  byte  print_justify;          // Justify ASCII text.
  byte  units_cm;               // CM units (or inches).
  byte  info_titles;            // Puts titles in the kanji-info list box.
  byte  colorkanji_mode;        // Determines the way color-fonts are suported.
  byte  colorkanji_bitmap;      // Support color kanji in bitmap clipboard format
  byte  colorkanji_print;       // Support color kanji in printing.
  byte  cache_displayfont;      // Should we cache or not cache the display font.
  byte  auto_lookup;            // Should we do auto-lookups in the radical lookup dialog.
  COLORREF info_color;          // Color used for titles in kanji-info box.
  COLORREF colorkanji_color;    // Color to be used with color-kanji.
  short alloc;                  // Allocation size for lines.
  int   x,y,xs,ys;              // Dimensions of last saved configuration.
  TCHAR font[SIZE_NAME];        // Name of roman font.
  TCHAR print[SIZE_NAME];       // Name of print font.
  TCHAR display[SIZE_NAME];     // Name of display font.
  short font_size;              // Size of font used for rending TrueType fonts
  short convert_size;           // Number of entires in user conversion table.
  short char_width;             // Character width for formatinning.
  short undo_number;            // Number of levels of undo to keep.
  short export_length;          // Length of fixed export lines. NOT CURRENTLY USED!
  short font_cache;             // Size of font cache in characters.
  short print_size;             // Point size of the font to use for printing.
  short head_left;              // Position of headers to the left of margins
  short head_right;             // Position of headers to the right of margins.
  short head_top;               // Position of header lines above margins.
  short head_bottom;            // Position of header lines below margins.
  PrintSetup page;              // Default printer setup.
  KANJI date_format[SIZE_DATE]; // Date format string.
  KANJI time_format[SIZE_DATE]; // Time format string.
  KANJI am_format[SIZE_AMPM];   // AM format string.
  KANJI pm_format[SIZE_AMPM];   // PM format string.
  byte  skip_misscodes;         // Search for skip miss-codes.
  byte  bushu_nelson;           // Search for Nelson bushu
  byte  bushu_classical;        // Search for classicla bushu
  byte  index_type;             // Index type for index search.
  byte  reading_type;           // Reading type for reading search
  byte  reading_kun;            // Allow flexable kun readings.
  byte  reading_word;           // Allow flexable word matching.
  byte  dict_watchclip;         // Watch clipboard when dictionary is open
  byte  dict_excludeme;         // Exclude me from clibboard tracking. NOT CURRENTLY USED!
  byte  dict_classical;         // Classical dictionary search
  byte  dict_nobeep;            // Makes the dictionary searches quiet. NOT CURRENTLY USED!
  byte  no_variants;            // Suppresses showing of variants in radical lookups.
  byte  all_fonts;              // Show all fonts in the font selector
  byte  no_toolbar;             // Disable the toolbar.
  byte  button_count;           // Number of buttons in the toolbar
  byte  cache_info;             // Fill for later exapnsion
  byte  buttons[100];           // Buttons for the button bar.
  byte  no_BITMAP;              // Suppress BITMAP clipboard format
  byte  no_UNICODETEXT;         // Suppress UNICODETEXT clipboard format
  byte  info_compress;          // Compress Character information.
  byte  ime_mode;               // Determines JWPce's interaction with the Microsoft IME

  byte  fill[136];              // Fill for later expansion.
};

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
  HANDLE open        (tchar *name,int mode,int net);                // Open a file based on configurations settings.
  TCHAR *name        (tchar *file,int mode,int net);                // Generate file name.
  TCHAR inline *name (void) { return (last_name); }                 // Get last name generated by program.
  int   read         (void);                                        // Reader routine.
  void  set          (struct cfg *new_config);                      // Set a configuration.
  void  write        (void);                                        // Write configuration file
  void  write_files  (void);                                        // Write files information to the configuation file.
};

extern class JWP_config jwp_config;

#endif jwp_conf_h
