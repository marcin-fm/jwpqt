//-------------------------------------------------------------------//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998,1999,2000.             //
//  All rights reserved.                                             //
//                                                                   //   
//  These routines are intended to interface with EDICT, which is    //
//  a Japanese/English Dictionary developed and copyrighted by       //
//  James William Breen.                                             //
//                                                                   //   
//-------------------------------------------------------------------//

//-------------------------------------------------------------------
//
//  This module implements the dictionary search and mantinance routines.
//  These handle all interactions with the dictionaries and with searching.
//

#ifndef jwp_dict_h
#define jwp_dict_h
#include "jwp_file.h"
#include "jwp_edit.h"

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

class JWP_dict : EUC_buffer {
friend class EDIT_userdict;
public:
  inline JWP_dict () { classical = active = false; user_dialog = null; }
  void search          (JWP_file *file);                                    // Entry point
  int  dlg_dictionaries(HWND hwnd,int message,WPARAM wParam,LPARAM lParam); // Main dialog procedure.
  int  dlg_dictionary  (HWND hwnd,int message,WPARAM wParam,LPARAM lParam); // Main dialog procedure.
  int  dlg_dictoptions (HWND hwnd,int msg,int command);                     // Dicitonary options dialog procedure.
  int  dlg_userdict    (HWND hwnd,int message,WPARAM wParam,LPARAM lParam); // User dictionary dialog procedure.
private:
  void check_entry    (byte *data,int length);                  // Check an entry for includsion
  int  do_search      (KANJI *search,int length);               // Actually executat a search for a specific pattern, seith a set of parameters.
  void error          (int format,...);                         // Version of the main error handler adjusted for this dialog box.
  void get_checkboxes (void);                                   // Get the state of the 4 check-boxes.
  void get_line       (HANDLE index,HANDLE dict,int loc,byte *buffer);  // Get a line from the dictionary file.
  int  is_searching   (void);                                   // Determine if we are in a search.
  void message        (tchar *foramt,...);                      // Set the message in the dialog window.
  void search_dict    (void);                                   // Actually search.
  void search_index   (byte *key,int length,tchar *name);       // Search indexed dictionary.
  void search_memory  (byte *key,int length,byte *dict);        // Search memory dictionary.
  void set_checkboxes (void);                                   // Set the state of the 4 check-boxes in the main dialog box.
  void user_dictionary(void);                                   // Edit the user dictionary.
  int  save_user      (void);                                   // Save user dictionary.

  HWND      dialog;             // The dialog window, used so various rotuines can access the dialog box.
  HWND      user_dialog;        // Dialog pointer for the user dictionary.
  short     matches;            // Number of valid matches.
  short     rejected;           // Number of fully rejected entries.
  byte      state;              // Current state (see flags above).
  byte      filter;             // Set to true when entries need to be filtered.
  byte      nonames;            // Names are to be filtered out of the search (skips some dictionaries);
  byte      active;             // Determine dictionary is active.
  byte      classical_part;     // Determines if this this a classical paticle (or jodoushi).
  byte      classical;          // Indicates this is a search of the classical dictionary.
};

typedef class JWP_dict JWP_dict;

extern JWP_dict jwp_dict;       // Class instance.

extern void free_dictionary (void); // Deallocate dictionary objects.

#endif jwp_dict_h

