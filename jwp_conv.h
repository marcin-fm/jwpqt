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
//  This modlue hanles conversion of kana to kanji.
//
//  How the kana->kanji conversion works.  The system is stored in two
//  files.  The first file is a index file used to find the correct set
//  of conversions quickly (wnn.idx).  The second part is the actuall
//  conversion file (wnn.dct).
//
//  Note in working with the dicitonary files, all kana, kanji, and 
//  derived values have the upper bit (0x80) set on each byte.  This
//  does int interfere with the values, and allows text prosesing of 
//  the rest of the string without any difficulty.
//

#ifndef jwp_conv_h
#define jwp_conv_h

#include "jwp_file.h"
#include "jwp_klst.h"

//===================================================================
//
//  Potentially adjustuable parameters.
//

#define SIZE_SELKEY     6       // Max size of keys in the user conversion dictionary.
#define SIZE_MAXKEY     20      // We will not even attempt to convert kana strings this long.

//===================================================================
//
//  Parameters for routines.
//
                                        // Parameters for JWP_conv::do_select() and JWP_file::convert().
#define CONVERT_RIGHT   KANJILIST_RIGHT // Shift conversion to right.
#define CONVERT_LEFT    KANJILIST_LEFT  // Shift conversion to left.
#define CONVERT_ATTEMPT 2               // Not a shift command, but an attempt to do an automatic conversion.
#define CONVERT_FORCE   0               // Force conversion.

//===================================================================
//
//  KANJI_index structre:
//
//  This describes the entries within the main dictionary index file.
//

#define INDEXLEVEL      3       // Length of kana keys in the main dictionary index file.

typedef struct KANJI_index {    // Describes dictionary index file entries.
  byte key[INDEXLEVEL+1];       // kana key.
  long offset;                  // Offest into the data file.
} KANJI_index;

//===================================================================
//
//  KANJI_sel structure:
//
//  This structure describes an entry in the user conversion dictionary.
//

typedef struct KANJI_sel {
  byte  key[SIZE_SELKEY];       // Kana key (may be full ength.
  short select;                 // Index into list that user has selected.
} KANJI_sel;

//===================================================================
//
//  JWP_conv class
//
//  Describes the enrite kana->kanji conversion system.
//

class JWP_conv : public KANJI_list {
public:
  JWP_conv (int count);
 ~JWP_conv (void);
  void  adjust       (HWND hwnd);               // Adjust display parameters.
  int   attempt      (KANJI *kana,int length);  // Process auto kana->kanji conversion (determine if conve is possible).
  void  clear        (void);                    // Clear conversion system
  int   convert      (KANJI *kana,int length);  // Do an actuall conversion (no external output).
  int   dlg_userconv (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);  // Edit user kana->kanji conversons.
  void  do_convert   (KANJI *kana,int length);  // Attempt forced conversion (F2, F2, etc)
  void  do_select    (int direction);           // Change selected kanji right or left
  void  done         (void);                    // We are exiting so save parameters.
  int   initialize   (WNDCLASS *wclass);        // Intialize conversion system.
  void  realloc      (int size);                // Reallocate the user selection buffer.
  HWND  dialog;                                 // Pointer to the dialog for user entries.
private:
  int  binary_search (byte *key);               // Perform binary search of main dict index.
  void  filter       (void);                    // Remove duplicates form the list.
  KANJI_sel *find_choice (void);                // Find conversion ins user selection dictionary.
  int  index_compare (byte *key,int id);        // Index compare (used for binary search).
  int  save_user     (void);                    // Save the user conversions list.
  void search_dict   (byte *key,int endkana,IO_cache *cache,int sorted);    // Actuall search engine.
  void search        (byte *key,int endkana);   // Search main dictionary.
  void select        (int s);                   // Select a kanji conversion and insert into file.
//
//  Dictionary parameters;
//
#if (CONVERT_ACCESS == CONVERT_ACCESS_MEMORY)
  byte        *dict;                    // Main dictonary data.
#else 
  HANDLE       dict;                    // Main dictionary file.
#endif
  KANJI_index *index;                   // Main dictonary index.
  short        size;                    // Number of entries in main index.
  KANJI_sel   *user_sel;                // User selection cache.
  byte        *user;                    // User dictionary data.
//
//  Misc.
//
  byte         sel_key[SIZE_SELKEY];    // Key cached for selection cache rotuines.
  byte         usersel_changed;         // Has user select cache changed.
  byte         is_more;                 // Set to 0 when cannot add more kana and get a valid string.
};

extern class JWP_conv jwp_conv;

extern int godan_kana (int ascii);      // Get godan kana character associated ascii character

#endif jwp_conv_h
