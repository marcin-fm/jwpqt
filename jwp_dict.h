//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//  These routines are intended to interface with EDICT, which is    //
//  a Japanese/English Dictionary developed and copyrighted by       //
//  James William Breen.                                             //
//                                                                   //   
//===================================================================//

//===================================================================
//
//  This module implements the dictionary search and mantinance routines.
//  These handle all interactions with the dictionaries and with searching.
//

#ifndef jwp_dict_h
#define jwp_dict_h
#include "jwp_file.h"
#include "jwp_edit.h"

//--------------------------------
//
//  This class is used to handle the dictionary list. 
//
//  This is a virtual class.  The actual working class routines specify a specific search
//  routine used to search this dictionary.
//
#define DICT_EUC        1           // Dictionary has EUC encoding (normal EDICT format)
#define DICT_UTF8       2           // Dictionary has UTF-8 encoding.
#define DICT_MIXED      3           // Dictionary has mixed encoding.  Kana and kanji are EUC.  Meaning is in extended ascii.

#define DICT_NONAMES    0           // Dictioanry has no names.
#define DICT_NAMES      1           // Dictionary has names.
#define DICT_NAMESONLY  2           // Dictionary has only names.

#define DICT_NORMAL     0           // Dictionary is normal.
#define DICT_CLASSICAL  1           // Classicial dictionary (special Jodoushi handing)
#define DICT_USER       2           // User dictionary.

#define DICT_MEMORY     0           // Memory based dictionary
#define DICT_INDEX      1           // Index based dictionary.
#define DICT_BUFFER     2           // Buffer based dictionary

class Dictionary {
friend class JWP_dict;
friend void free_dictionary (void);
friend Dictionary *new_dictionary (TCHAR *name,TCHAR *file,TCHAR *flags);
public:
  ~Dictionary          (void);                                      // Destructor
  void   addlist       (HWND list,int index);                       // Add this dictionary to the Dictionaries list.
  void   close         (void);                                      // Close this dictionary (does not deallocate resources).
  void   get_firstline (HWND hwnd,int doname);                      // Get the first line of citioanry (and name) for Dictionary properties dialog.
  int    get_info      (HWND hwnd);                                 // Get basic info for Dictionary properities dialog (encoding, index)
  void   get_line      (int loc,byte *buffer);                      // Get a line from the dictionary file.
  TCHAR *get_name      (void);                                      // Return the name of the file associated with this dictionary.
  void   init          (TCHAR *_name,TCHAR *_file,TCHAR *flags);    // Constructor
  int    open          (void);                                      // Open the dictionary.
  void   write         (HANDLE file);                               // Write this dictionary to disk version of dictionaries list.
  int    read_sample   (byte *buffer,int size);                     // Read a fixed number of bytes from the start of a dictionary.
//
//  Virtural routines.
//
  void virtual search (KANJI *key,int length) = 0;                  // Actually search the dictionary.
//
//  Data
//
  class Dictionary *next;           // link to next dictionary.
  HANDLE            dict,index;     // Handles for main dictionary file and for index file.
  long              index_max;      // Maximum index in the file used for the binary search.
  byte             *memory;         // Memory image for dictionary.
  TCHAR            *name;           // User assigned dictionary name.
  TCHAR            *filename;       // Actual file
  byte              format;         // Dictioanry format
  byte              idx;            // Index type
  byte              names;          // Does dictionary have names.
  byte              special;        // Do we search this dictionary.
  byte              searched;       // Special flags for dictionary.
  byte              buffered;       // Search type.
  byte              keep;           // Keep in memory or keep open.
  byte              quiet;          // Quiet handling of errors.
};

//--------------------------------
//
//  Derived dictionary classes.
// 
//  This specify the specific search rotuine for the type of dictionary.
//
class EUC_IDX_Dictionary : private Dictionary {
private:
  void search (KANJI *key,int length);
};

class EUC_MEM_Dictionary : private Dictionary {
private:
  void search (KANJI *key,int length);
};

class MIX_IDX_Dictionary : private Dictionary {
private:
  void search (KANJI *key,int length);
};

class MIX_MEM_Dictionary : private Dictionary {
private:
  void search (KANJI *key,int length);
};

class UTF_IDX_Dictionary : private Dictionary {
private:
  void search (KANJI *key,int length);
};

class UTF_MEM_Dictionary : private Dictionary {
private:
  void search (KANJI *key,int length);
};

//--------------------------------
//
//  Dictonary working class.
//
                                    // Dictionary state flags used so the dialog can remaine live during a search.
#define DICTSTATE_IDLE      0       // Nothing is going on.
#define DICTSTATE_SEARCH    1       // A search is in progress.
#define DICTSTATE_ABORT     2       // We have requested an abort of the current search.

                                    // Error conditions for dictionary search.  These indicate valid search arguments.
#define DICTSEARCH_OKAY     0       // Search string is OK.
#define DICTSEARCH_SHORT    1       // Search string is too short.
#define DICTSEARCH_MIXED    2       // Search string contains mixed ascii and Japanese.
#define DICTSEARCH_ABORT    10      // User aborted the search!

                                    // Specific entries in the dict_keys array.
#define DICTKEY_START       2       // First dict key for most searches.  The earlier ones are there only for the options dialog box
#define DICTKEY_BEGIN       0       // Requires matching at the beginning of words.
#define DICTKEY_END         1       // Requires matching at the end of words.
#define DICTKEY_NAMES       2       // Rejects personal names.
#define DICTKEY_PLACES      3       // Rejects place names.

#define DICTBIT_BEGIN   (0x1L << DICTKEY_BEGIN )    // These are bit values used to 
#define DICTBIT_END     (0x1L << DICTKEY_END   )    //   store various dictiony flags
#define DICTBIT_NAMES   (0x1L << DICTKEY_NAMES )    //   in the configuration file.
#define DICTBIT_PLACES  (0x1L << DICTKEY_PLACES)

#define MAX_KEY_LENGTH  100 // Determines max number of character we allow 
                            //   a search on. If the user search string is 
                            //   too long, we truncate to this level.

class JWP_dict : EUC_buffer {
friend class EDIT_userdict;
public:
  inline JWP_dict () { memset (this,0,sizeof(JWP_dict)); return; }
  void search          (JWP_file *file);                                        // Entry point
  int  dlg_dictionaries(HWND hwnd,int message,WPARAM wParam,LPARAM lParam);     // Main dialog procedure.
  int  dlg_dictionary  (HWND hwnd,int message,WPARAM wParam,LPARAM lParam);     // Main dialog procedure.
  int  dlg_dictoptions (HWND hwnd,int msg,int command);                         // Dicitonary options dialog procedure.
  int  dlg_editdict    (HWND hwnd,int message,WPARAM wParam,LPARAM lParam);     // Edit a dictionary entry in the dictionary list.
  int  dlg_userdict    (HWND hwnd,int message,WPARAM wParam,LPARAM lParam);     // User dictionary dialog procedure.
  int  check_abort     (void);                                                  // Processes the messages in the que and check for an abort.
  void euc_check       (byte *data);                                            // Check an entry for includsion
  void utf_check       (byte *data,int length);                                 // Check an entry for includsion
private:
  int  add_dict           (Dictionary *&base,TCHAR *name,TCHAR *file,TCHAR *flags,int startup);   // Add a dictionary to the end of the search list.
  void check_primary      (byte *ptr);                              // Check for prinary entry and move to forward part of list if this is active.
  void def_dictionaries   (Dictionary *&base);                      // Setup the default dictionaries.
  int  do_search          (void);                                   // Actually executat a search for a specific pattern, seith a set of parameters.
  void error              (int format,...);                         // Version of the main error handler adjusted for this dialog box.
  int  euc_endbegin       (byte *first,byte *last);                 // Check ending and beginning.
  int  filter_entry       (byte *ptr);                              // Check an entry agains user selected filters.
  void find_user          (void);                                   // Find and activate the user ditionary in the dictionary list.
  void get_checkboxes     (void);                                   // Get the state of the 4 check-boxes.
  int  get_last           (void);                                   // Get last character in search
  int  is_searching       (void);                                   // Determine if we are in a search.
  void load               (void);                                   // Load dictionaries file
  void message            (tchar *foramt,...);                      // Set the message in the dialog window.
  int  put_last           (KANJI kanji);                            // Add kanji to the end of the search key.
  int  search_add         (KANJI kanji);                            // Search dictionary with added ending.
  int  search_end         (KANJI kanji);                            // Search dictionary with new ending.
  void search_dict        (void);                                   // Actually search.
  void set_checkboxes     (void);                                   // Set the state of the 4 check-boxes in the main dialog box.
  void user_dictionary    (HWND hwnd);                              // Edit the user dictionary.
  int  utf_endbegin       (byte *first,byte *last);                 // Check ending and beginning.
  int  save_user          (void);                                   // Save user dictionary.
  int  write_dictionaries (void);                                   // Write dictionaries file.
//
//  Data
//
  class     Dictionary *user;                   // User dictionary.
  class     Dictionary *dic;                    // Scratch dictionary pointer.  Also used to pass information to dialog box to edit dictionaries.
  class     Dictionary *dict;                   // Current searched dictionary.  We don't use dic, because there could be conflicts in non-modal dialogs.
  TCHAR    *drop;                               // Used to pass drag and drop infirmation to a dialog.
  HWND      dialog;                             // The dialog window, used so various rotuines can access the dialog box.
  HWND      user_dialog;                        // Dialog pointer for the user dictionary.
  int       matches;                            // Number of valid matches.
  int       rejected;                           // Number of fully rejected entries.
  int       primary;                            // Pointer to primary location.
  byte      state;                              // Current state (see flags above).
  byte      filter;                             // Set to true when entries need to be filtered.
  byte      nonames;                            // Names are to be filtered out of the search (skips some dictionaries);
  byte      active;                             // Determine dictionary is active.
  byte      classical_part;                     // Determines if this this a classical paticle (or jodoushi).
  byte      clipsearch;                         // Indicates search is from clipboard.  This silences a number of the error messages.
  byte      pattern;                            // Indicates a pattern search.  
  byte      ascii_search;                       // Indicates search is ASCII.  This uses differetn end/begin match characters.
  KANJI    *search_ptr;                         // Pointer to string to be searched for.
  KANJI    *prefix_ptr;                         // Pointer to pattern prefix.
  KANJI    *postfix_ptr;                        // Pointer to pattern postfix.
  int       search_len;                         // Length of search string.
  int       prefix_len;                         // Length of prefix (zero for no prefix)
  int       postfix_len;                        // Length of postfix (zero for no postfix).
  int       euc_length;                         // Length of search string in EUC characters.
};

typedef class JWP_dict JWP_dict;

extern JWP_dict jwp_dict;           // Class instance.

extern JWP_history dict_history;    // History buffer for dictionary.

extern void free_dictionary (void); // Deallocate dictionary objects.

#endif jwp_dict_h

