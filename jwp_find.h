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
//  This module implements the search capabilities for the editor.
//  Fundimentally, this is done through the JWP_search class.  This 
//  class is a friend class of JWP_file because it needs access to 
//  the internal strcutres of the file for scanning.
//

#ifndef jwp_find_h
#define jwp_find_h

class JWP_search {
public:
  JWP_search ();
  inline ~JWP_search () { clear (TRUE); }
  int  dlg_search  (HWND hwnd,UINT message,int command,int rep,class JWP_list *list);    // Process search and replace dialoge boxes
  void do_next     (class JWP_list *list);          // Implement the Edit/Find Next function.
  void do_replace  (void);                          // Implement the Edit/Replace function.
  void do_search   (class JWP_list *list);          // Implement the Edit/Search function
  void not_found   (HWND window,int replace_all);   // Error message for not found.
  int  test        (KANJI *string);                 // Test to see if string matches the search pattern.
private:
  void   clear     (int rep);                       // Clears the search engine and releases the memory.
  int    changes;                                   // Counter for number of changes.
  short  is_replace;                                // Set to non-zero to do a replace as oposed to search.
  short  replace_length;                            // Length of the replace string.
  KANJI *replace;                                   // Repalce kanji string.
  KANJI *search;                                    // Search kanji string, working string. 
                                                    //   (jascii->ascii and case conversions have been done.)
                                                    //   This parameter being NULL, implies that this is 
                                                    //   a fixed case (fast) search.
  KANJI *search_input;                              // Search kanji string entered by the user (saved so we can restore it next time them open the dialoge box.)
  short  search_length;                             // Length of the search string.
  HWND   search_dlg;                                // Pointer to search dialog.
  HWND   replace_dlg;                               // Pointer to replace dialog.
};

typedef JWP_search JWP_search;

extern JWP_search jwp_search;                       // Instance of class.

extern JWP_history search_history;                  // History buffer for seach.
extern JWP_history replace_history;                 // History buffer for replace.

#endif jwp_find_h
