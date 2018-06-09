
#define CF_NO_EXPORT  (1 << 7)  // This flag is for marking alternate configuration item names that can be imported (for compatibility) but won't be exported. The preferred form should be the one actually written out.
#define CF_NO_DEFAULT (1 << 6)  // Indicates that the inline default value should not be used.
#define CF_HEX        (1 << 5)  // Output integers in hexadecimal format.

enum member_t {
MT_UNDEFINED,
MT_BIN,     // Generic
MT_BOOL,    // Boolean
MT_INT,     // Integer
MT_STR,     // String
MT_REMARK,  // Informative
};

typedef struct {
  char*autoname;
  void*addr;
  void*def_p;
  short size;
  byte flags;
  byte type;
  char*name;
  union { long def_i; bool def_b; };
} CONFIG_TEMPLATE;

#define CFG_TARGET(field) jwp_config.cfg.field
#define DEF_TARGET(field) default_config.field

#define CITMPL(field,flags,type) #field, &CFG_TARGET(field), &DEF_TARGET(field), sizeof(CFG_TARGET(field)), flags, type,  STR_PREPEND

/*
#define _CIG(field,flags) #field, &CFG_TARGET(field), &DEF_TARGET(field), sizeof(CFG_TARGET(field)), flags, MT_BIN,  STR_PREPEND
#define _CII(field,flags) #field, &CFG_TARGET(field), &DEF_TARGET(field), sizeof(CFG_TARGET(field)), flags, MT_INT,  STR_PREPEND
#define _CIB(field,flags) #field, &CFG_TARGET(field), &DEF_TARGET(field), sizeof(CFG_TARGET(field)), flags, MT_BOOL, STR_PREPEND
#define _CIT(field,flags) #field, &CFG_TARGET(field), &DEF_TARGET(field), sizeof(CFG_TARGET(field)), flags, MT_STR,  STR_PREPEND
*/

#define _CIG(m,f) CITMPL(m,f,MT_BIN)
#define _CII(m,f) CITMPL(m,f,MT_INT)
#define _CIB(m,f) CITMPL(m,f,MT_BOOL)
#define _CIT(m,f) CITMPL(m,f,MT_STR)

#define CIG(m) _CIG(m,CF_NO_DEFAULT)
#define CIT(m) _CIG(m,CF_NO_DEFAULT)
#define CII(m) _CII(m,0)
#define CIH(m) _CII(m,CF_HEX)
#define CIB(m) _CIB(m,0)


// Font structure
#define CISF(structure,str)                                   \
{ _CIT(structure.name, CF_NO_DEFAULT) str ## ".Font" },       \
{ _CII(structure.size, CF_NO_DEFAULT) str ## ".Size" },       \
{  CIB(structure.automatic)           str ## ".Auto", true }, \
{  CIB(structure.vertical)            str ## ".Vert", false },

// Window dimensions
#define CISW(structure,str)             \
{  CII(structure.x)  str ## ".X", 0 },  \
{  CII(structure.y)  str ## ".Y", 0 },  \
{  CII(structure.sx) str ## ".W", 0 },  \
{  CII(structure.sy) str ## ".H", 0 },

// Textual
#define REMARK(str) { 0, 0, 0, 0, 0, MT_REMARK, str },
#define BLANK()   REMARK("")
#define HEAD(str) REMARK("\n# " ## str)
#define REM(str)  REMARK("# " ## str)



#define STR_PREPEND ""

CONFIG_TEMPLATE config_template[] = {

HEAD("Window and Dialog Dimensions")
{ CII(x)  "Window.X" }, //   int   x,y,xs,ys;              // Dimensions of previous session's main window.
{ CII(y)  "Window.Y" },
{ CII(xs) "Window.W" },
{ CII(ys) "Window.H" },
BLANK()
CISW(size_info,  "CharInfo")    //  struct size_window size_info; // Size of info dialog.
CISW(size_dict,  "Dictionary")  //  struct size_window size_dict; // Size of dictionary window.
CISW(size_count, "KanjiCount")  //  struct size_window size_count;// Size of count kanji window.
CISW(size_more,  "MoreInfo")    //  struct size_window size_more; // Size of more info dialog.
CISW(size_cnvrt, "UserConv")    //  struct size_window size_cnvrt;// Size of user kana->kanji conversions window.
CISW(size_user,  "UserDict")    //  struct size_window size_user; // Size of user dictionary window;

HEAD("Fonts")
CISF(ascii_font, "ASCII")   //  struct cfg_font ascii_font;   // Ascii system font.
CISF(sys_font,   "System")  //  struct cfg_font sys_font;     // System font used for text and a few other places.
CISF(list_font,  "List")    //  struct cfg_font list_font;    // Font for lists.
CISF(edit_font,  "Edit")    //  struct cfg_font edit_font;    // Font used for Japanese edit constrols.
CISF(bar_font,   "KanjiBar")//  struct cfg_font bar_font;     // Font used for kanji bars.
CISF(file_font,  "File")    //  struct cfg_font file_font;    // Font used for editing files.
CISF(big_font,   "Big")     //  struct cfg_font big_font;     // Font used for big text.
CISF(jis_font,   "Table")   //  struct cfg_font jis_font;     // Font used for JIS table.
CISF(clip_font,  "Bitmap")  //  struct cfg_font clip_font;    // Font used for clipboard bitmapps.
CISF(print_font, "Print")   //  struct cfg_font print_font;   // Font used for printing.
//{ CISF(extra_font) "" },  //  struct cfg_font extra_font;   // Extra font for later
//{ CISF(extra_font2) "" }, //  struct cfg_font extra_font2;  // Another extra font.
BLANK()
{ CIB(all_fonts) "ShowAllFonts" },  //  byte  all_fonts;              // Show all fonts in the font selector
{ CIB(cache_displayfont) "BufferDisplayFont", false },  //  byte  cache_displayfont;      // Should we buffer the display font or load the whole file?
{ CII(font_cache) "FontCacheSizeInCharacters", 400 },   //  short font_cache;             // Size of font cache in characters. Does not affect TrueType fonts.

HEAD("Printing")
{ CII(head_left)     "Printing_HeaderPos_Left", 0 },      //  short head_left;              // Position of headers to the left of margins
{ CII(head_right)    "Printing_HeaderPos_Right", 0 },     //  short head_right;             // Position of headers to the right of margins.
{ CII(head_top)      "Printing_HeaderPos_Top", 100 },     //  short head_top;               // Position of header lines above margins.
{ CII(head_bottom)   "Printing_HeaderPos_Bottom", 100 },  //  short head_bottom;            // Position of header lines below margins.
{ CIB(print_justify) "Printing_Justify_ASCII", true },    //  byte  print_justify;          // Justify ASCII text.
BLANK()
{ CIG(page)          "Printing_DefaultLayout" },    //  PrintSetup page;              // Default page layout (used for printing).
{ CIG(date_format)   "Printing_Formatting_Date" },  //    KANJI date_format[SIZE_DATE]; // Date format string.
{ CIG(time_format)   "Printing_Formatting_Time" },  //    KANJI time_format[SIZE_DATE]; // Time format string.
{ CIG(am_format)     "Printing_Formatting_AM" },    //    KANJI am_format[SIZE_AMPM];   // AM format string.
{ CIG(pm_format)     "Printing_Formatting_PM" },    //    KANJI pm_format[SIZE_AMPM];   // PM format string.

HEAD("Arrays")
{ CIG(kanji_info)   "CharInfo_Fields" },  //  byte  kanji_info[60];         // Character Information dialog items
{ CIG(buttons)      "ToolbarButtons" },   //  byte  buttons[100];           // Buttons for the button bar.REM("Do not change this directly!")
{ CII(button_count) "ToolbarButtonCount", 39 }, //  byte  button_count;           // Number of buttons in the toolbar

HEAD("Colors")
{ CIG(info_color)       "Color_Highlight", }, //  COLORREF info_color;          // Color used for titles in kanji-info box.
{ CIG(colorkanji_color) "Color_KanjiList" },  //  COLORREF colorkanji_color;    // Color to be used with color-kanji.
{ CIG(rarekanji_color)  "Color_RareKanji" },  //  COLORREF rarekanji_color;     // Color to be used with "rare" kanji.

HEAD("Color Kanji")
{ CII(colorkanji_mode)   "ColorKanji_Mode", COLORKANJI_OFF }, //  byte  colorkanji_mode;        // Determines the way color-fonts are suported.
{ CIB(colorkanji_bitmap) "ColorKanji_Clipboard" },  //  byte  colorkanji_bitmap;      // Support color kanji in bitmap clipboard format
{ CIB(colorkanji_print)  "ColorKanji_Printing" },   //  byte  colorkanji_print;       // Support color kanji in printing.

HEAD("Uncommon Kanji")
{ CIB(colorize_rare)     "ColorizeRareKanji" },         //  byte  colorize_rare;          // Render rare kanji in a separate color. Only applies if not already affected by the kanji list.
{ CIB(mark_rare_kanji)   "MarkRareKanjiInKanjiBars" },  //  byte  mark_rare_kanji;        // Add a special mark to rare kanji displayed in kanji bars.

HEAD("Tuning")
{ CII(convert_size) "ConversionChoicesStored", 200 }, //  short convert_size;           // Number of entries in user conversion table.
{ CII(dict_buffer) "DictionaryBuffer_Size", 20000 },  //  int   dict_buffer;            // Size of dictionary buffer.
{ CII(history_size) "HistoryBuffers_NumChars", 300 }, //  short history_size;           // Size of history buffers (in characters). Each history type gets its own buffer of this size.
{ CII(undo_number) "MaximumUndoLevels", 50 },   //  short undo_number;            // Number of levels of undo to keep.
{ CII(alloc) "ParagraphMemory_BlockSize", 40 }, //  short alloc;                  // Allocation unit size for lines/paragraphs (in KANJI, 2 bytes apiece).

HEAD("Startup")
{ CIB(install) "CheckInstalled", true },  //  byte  install;                // If set causes check for installed version and file extensions.
{ CIB(maximize) "MaximizeWindow" }, //  byte  maximize;               // Maximize the main window.
{ CIB(usedims) "RestoreWindow" },   //  byte  usedims;                // Use last saved dimensions for main window.
{ CIB(reload_files) "ReloadPreviousFiles", true },  //  byte  reload_files;           // Reload files loaded when we exited.

HEAD("Exit")
{ CIB(close_does_file) "CloseButton_Closes_File", true }, //  byte  close_does_file;        // Window close control, closes just current file.
{ CIB(confirm_exit) "LastFileConfirmExit", true },  //  byte  confirm_exit;           // Requires confirmation before exiting the program when closing the last file. No effect when closing the entire program.
{ CIB(save_exit)    "SaveSettingsOnExit", true },   //  byte  save_exit;              // Save configuration on exit.
{ CIB(save_history) "Save_Histories", true },       //  byte  save_history;           // Save dictionary/search/replace histories on exit.
{ CIB(save_recent)  "Save_RecentFiles", true },     //  byte  save_recent;            // Save recent file list on exit.

HEAD("Basic Operation")
{ CIB(auto_scroll)  "AutoScroll", true }, //  byte  auto_scroll;            // Enables or disables the auto-scroll feature.
{ CII(scroll_speed) "AutoScroll_Speed", 100 },  //  short scroll_speed;           // Determines the auto-scroll speed (lower is faster).
{ CII(double_open) "DoubleOpenBehavior", DOUBLE_PROMPT }, //  byte  double_open;            // Determine the action in the case of a double open.
//{ CII(ime_mode) "", IME_OFF },  //  byte  ime_mode;               // Determines JWPce's interaction with the Microsoft IME. Unused.
//{ CIB(page_mode_file) "" },     //  byte  page_mode_file;         // Uses page scrolling for the file (PPC only)
//{ CIB(page_mode_list) "" },     //  byte  page_mode_list;         // Uses page scrolling for lists (PPC only)
//{ CII(dir_handling_todo) "", 0 }, //  byte  dir_handling_todo;      // Defines, in part, how the initial directory for the Open dialog is determined.
//  Insert to file flags
{ CIB(paste_newpara) "InsertOnSeparateLines", true }, //  byte  paste_newpara;          // Causes text inserted from a Japanese list box to be inserted into separate lines (actually paragraphs) for each entry. Named "Insert on New Lines" in options.
{ CIB(backup_files) "KeepBackupCopyWhenSaving" },     //  byte  backup_files;           // Save last version of a file as a backup.
{ CII(code_page) "TranslationCodePage", CODEPAGE_AUTO },  //  short code_page;              // Code page used for Unicode translations.

HEAD("Display")
{ CIB(vscroll) "ScrollBar_Vertical", true },  //  byte  vscroll,hscroll;        // Vertical and horizontal scroll bar.
{ CIB(hscroll) "ScrollBar_Horizontal", true },
{ CIB(kscroll) "ScrollBar_KanjiBar", true },  //  byte  kscroll;                // Activate scroll bar on bar.
{ CIB(toolbar)  "Show_Toolbar", true },       //  byte  toolbar;                // Disable the toolbar.
{ CIB(status)   "Show_StatusBar", true },     //  byte  status;                 // Display status bar.
{ CIB(kanjibar) "Show_KanjiBar", true },      //  byte  kanjibar;               // Display the kanji bar.
{ CIB(kanjibar_top) "KanjiBarAtTop" },        //  byte  kanjibar_top;           // Places the kanji bar at the top of the screen
{ CII(width_mode) "LineWidth_Mode", WIDTH_DYNAMIC },  //  byte  width_mode;             // Determines how the width of the display is calculated.
{ CII(char_width) "LineWidth_Fixed", 35 },  //  short char_width;             // Line width (in Kanji) used for the fixed-width display option.
{ CIB(relax_punctuation) "RelaxMargins_Punctuation", true },  //  byte  relax_punctuation;      // Allow relaxed punctuation.
{ CIB(relax_smallkana)   "RelaxMargins_SmallKana", true },    //  byte  relax_smallkana;        // Allow relaxed small kana.
{ CIB(units_cm) "MetricUnits" },  //  byte  units_cm;               // CM units (or inches).

HEAD("Clipboard")
{ CII(clip_write)     "Clipboard_Format_Export", FILETYPE_SJS },  //  byte  clip_write;             // Clipboard write type.
{ CII(clip_read)      "Clipboard_Format_Import", FILETYPE_AUTODETECT }, //  byte  clip_read;              // Clipboard read type.
{ CIB(no_BITMAP)      "Clipboard_Omit_Bitmap" },  //  byte  no_BITMAP;              // Suppress BITMAP clipboard format
{ CIB(no_UNICODETEXT) "Clipboard_Omit_Unicode" }, //  byte  no_UNICODETEXT;         // Suppress UNICODETEXT clipboard format

HEAD("Search")
{ CIB(search_all)    "Search_AllFiles" }, //  byte  search_all;             // Search: All files.
{ CIB(search_nocase) "Search_CaseInsensitive", true },  //  byte  search_nocase;          // Search: Ignore case
{ CIB(keep_find)     "Search_KeepDialogsOpen", true },  //  byte  keep_find;              // Causes the Search/Replace dialog to remain open during searches.
{ CIB(search_jascii) "Search_WidthInsensitive", true }, //  byte  search_jascii;          // Search: JASCII=ASCII. This could be expanded to include (not-yet-supported) half-width katakana as well.
{ CIB(search_wrap)   "Search_WrapAround" }, //  byte  search_wrap;            // Search: Wrap at end of file. 
//{ CIB(search_back) "" },  //  byte  search_back;            // There is no point in saving this since it is cleared every time the dialog opens.
//{ CIB(search_noconfirm) "" }, //  byte  search_noconfirm;       // There is no point in saving this since it is cleared every time the dialog opens.

HEAD("Character Info")
{ CIB(info_compress) "CharInfo_Compact" },  //  byte  info_compress;          // Compress Character information.
{ CIB(info_titles)   "CharInfo_ShowHeadings", true }, //  byte  info_titles;            // Puts titles in the kanji-info list box.
{ CIB(info_onlyone)  "CharInfo_SingleDialog" },   //  byte  info_onlyone;           // Allows only one info version to open.
{ CIB(cache_info) "Cache_KanjiInfoFile", true },  //  byte  cache_info;             // Cache kanji information file on first use.

HEAD("Kanji Lookup")
{ CIB(auto_lookup) "AutoSearch_KanjiLookup", true },  //  byte  auto_lookup;            // Should we do auto-lookups in the radical lookup dialog.
{ CIB(bushu_nelson)    "Bushu_MatchNelson", true },   //  byte  bushu_nelson;           // Search for Nelson bushu
{ CIB(bushu_classical) "Bushu_MatchClassical", true },  //  byte  bushu_classical;        // Search for classicla bushu
{ CIB(reading_kun)  "FlexibleKunReadings", true },  //  byte  reading_kun;            // Allow flexable kun readings.
{ CIB(reading_word) "MatchPartialMeanings" },       //  byte  reading_word;           // Allow partial-word matches for meanings in Reading lookup.
{ CIB(skip_misscodes) "Match_SKIP_Miscodings" },    //  byte  skip_misscodes;         // Search for skip miss-codes.
{ CIB(rare_last) "RareKanjiLast", true },           //  byte  rare_last;              // List rare kanji at the end. Only needed for the radical lookup.
{ CIB(no_variants) "ReduceRadicalChoices" },        //  byte  no_variants;            // Hides variant/equivalent radicals from radical selection bar.
BLANK()
{ CII(index_type)   "IndexType", 0 }, //  byte  index_type;             // Index type for index search.
{ CII(reading_type) "ReadingType", RLTYPE_KUNON },  //  byte  reading_type;           // Reading type for reading search

#undef  STR_PREPEND
#define STR_PREPEND "Dict_"
HEAD("Dictionary")
{ CIB(dict_auto) "AutoSearch", true },      //  byte  dict_auto;              // Automatically attempt a search if the user has selected text.
{ CIB(dict_advanced) "AdvancedSearches" },  //  byte  dict_advanced;          // Use addaptive dictionary search.
{ CIB(dict_always) "AlwaysAdvancedSearch", true },  //  byte  dict_always;            // Always do an advanced search even if there were matches for the base string.
{ CIB(dict_showall) "Adv_KeepSearching" },  //  byte  dict_showall;           // Keep searching in an advanced search until all possibilities are exhausted.
{ CIB(dict_iadj)    "Adv_Try_I_Adjectives", true }, //  byte  dict_iadj;              // Process i-adjitives.
{ CIB(dict_advmark) "Adv_SeparatorMark", true },    //  byte  dict_advmark;           // Separate advanced search entries.
{ CIB(dict_classical) "ClassicalSupport" }, //  byte  dict_classical;         // Classical dictionary search
{ CIB(dict_compress) "Compact" }, //  byte  dict_compress;          // Displays diconary search results in compressed form.
{ CIB(dict_watchclip) "MonitorClipboard" }, //  byte  dict_watchclip;         // Watch clipboard when dictionary is open
{ CIB(dict_primaryfirst) "PriorityEntriesFirst", true },  //  byte  dict_primaryfirst;      // Move primary entries to the front the dictionary display.
{ CIB(dict_primark)      "PrioritySeparator", true }, //  byte  dict_primark;      // Demarcates priority and non-priority entries if dict_primaryfirst is enabled.
{ CIB(dict_jascii2ascii) "JASCII_to_ASCII" },     //  byte  dict_jascii2ascii;      // Treat JASCII as ascii;
{ CIB(dict_fullascii) "ASCII_MatchFullEntry" },   //  byte  dict_fullascii;         // Causes first/last to select complete entry for ascii 
{ CIH(dict_bits) "ExclusionFilters", DICTBITS },  //  long  dict_bits;              // Stores the state of all dictionary bits in one place.
//{ CIH(dict_bits_ex) "", 0 },  //  long  dict_bits_ex;           // Additional dictionary bit flags.
{ CIB(dict_link_adv_noname) "Link_Adv_NoNames" }, //  byte  dict_link_adv_noname;   // Link the Advanced and No Names checkboxes.
{ CIB(dict_contingent) "ContingentSearches" },    //  byte  dict_contingent;        // Enable "contingent" searches.
//{ CIB(dict_sort_always_todo) "" },    //  byte  dict_sort_always_todo;  // Always run a sort after a completed dictionary search.
//{ CII(dict_sort_method_todo) "", 0 }, //  char  dict_sort_method_todo;  // Default dictionary sorting method. Negative for inverse.
#undef  STR_PREPEND
#define STR_PREPEND ""

};
