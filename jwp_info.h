//-------------------------------------------------------------------//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998,1999,2000.             //
//  All rights reserved.                                             //
//                                                                   //   
//  The database read by JWPce is dirived directly from KANJIDIC     //
//  database dirived by Jim Breen.  Please see the _cpright.txt file //
//  for additional information.                                      //
//                                                                   //
//-------------------------------------------------------------------//

//-------------------------------------------------------------------
//
//  This modlue implements various informational routines.  The 
//  principle of these are the character-info (kanji info) and the 
//  kanji-table.  These routines primrally provide information 
//  related to Japanese characters.
//

#ifndef jwp_info_h
#define jwp_info_h

#include "jwp_file.h"

#define SIZE_INFOBUFFER 512             // Size of buffer for reading info lines.

//
//  Fixed structure for data file.
//
//  This is the structure representing the fixed part of the data file
//  that JWPce is expecting.
//
#include <pshpack2.h>   // Structure to be packed at 2 byte boundaries.
                        //   this results is a smaller data size.

struct kinfo {
  ushort bushu      :8;     // Radical number (from Nelson).
  ushort strokes    :5;     // Stroke count.
  ushort on         :3;     // Number of on-yomi

  ushort grade      :4;     // Grade that Japanese learn the kanji
  ushort imi        :4;     // Number of meanings
  ushort skip_t     :3;     // Skip code: type
  ushort skip_1     :5;     //            first number

  ushort skip_2     :5;     //            second number
  ushort pinyin     :1;     // Number of PinYin lines (1 or zero).
  ushort kun        :5;     // Number of kun-yomi
  ushort nan        :5;     // Number of nanori.

  ushort extra      :1;     // Entry has extended data.
  ushort halpern    :15;    // Halpern refference number

  ushort korean     :1;     // Number of Korean lines (1 or zero)
  ushort nelson     :15;    // Nelson reference number

  ushort haig;              // Index into New Nelson dicitonary (edited by Haig)

  ulong  classical  :8;     // Classical bushu (if different from Nelson's)
  ulong  offset     :24;    // File offset to variable part of the data.
};

struct extend {
  ushort md_long;           // Morohashi Daikanwajiten long entry

  ulong  md_short1  :4;     // Morohashi Daikanwajiten short (volume)
  ulong  md_short2  :13;    // Morohashi Daikanwajiten short (kanji)
  ulong  sh_rstroke :5;     // Spahn & Hadamitzky Dictionary radical stroke count
  ulong  sh_radical :5;     // Spahn & Hadamitzky Dictionary radical (a latter)
  ulong  sh_ostroke :5;     // Spahn & Hadamitzky Dictionary other stroke count

  ulong  sh_index   :6;     // Spahn & Hadamitzky Dictionary index of kanji with radical and stroke count
  ulong  fc_main    :14;    // Four corners main entry
  ulong  fc_index   :4;     // Four corners extra entry.
  ulong  fc_index2  :4;     // Four corners extra entry (for second code)
  ulong  md_p       :1;     // Morohashi Daikanwajiten (original code 'P')
  ulong  md_x       :1;     // Morohashi Daikanwajiten (close code 'X')
};

#include <poppack.h>

//
//  SKIP Miss-clasification / cross-reference codes:
//
#define SKIP_POSITION   1       // Position error.
#define SKIP_STROKE     2       // Stroke error.
#define SKIP_BOTH       3       // Both position and stroke error.
#define SKIP_BREEN      4       // Disagreement between Jim Breen and Jack Halpern about the code.

//
//  Amount codes passed to get_info().  These determine the amount of information processing
//  that is done.
//
#define INFO_FIXED      0       // Get just the fixed information.
#define INFO_STRINGS    1       // Get fixed information and the readings/meaning information.
#define INFO_EXTEND     2       // Get fixed extended information.
#define INFO_ALL        3       // Get all information.

//
//  Flags for the flag field.  These indicate what fields are included 
//  in the file.
//
#define KIFLAG_PINYIN   0x0001  // Has pin yin data.
#define KIFLAG_KOREAN   0x0002  // Has Korean data.
#define KIFLAG_NANORI   0x0004  // Has nanori data.
#define KIFLAG_EXTRA    0x0008  // Has extended data filed
#define KIFLAG_EVAR     0x0010  // Has extended variable data
#define KIFLAG_XREF     0x0020  // Has cross refference data

//
//  Number of kanji in the database.  This is actually written in the newer databases,
//  buf for the moment, I will continue using a hard-coded value.
//
#define KIMAX_KANJI     6355    // Eventually this should be read from the file.

//
//  KANJI_info class handles acess to the kanji-info file, and generation
//  of the character info dalog box.
//
class KANJI_info {
friend int  initialize_info (WNDCLASS *wclass);
friend void kanji_info (HWND hwnd,int kanji);
public:
  inline KANJI_info (void) { handle = INVALID_HANDLE_VALUE; return; }
  struct kinfo     kinfo;                       // This is the main information block.
  byte             buffer[SIZE_INFOBUFFER];     // Buffer used for holding the extended information    
  struct extend    extend;                      // Pointer to extended data structure or a NULL.
  byte            *pinyin;                      // Pointer to pinyin data or a NULL.
  byte            *korean;                      // Pointer to korean data or a NULL.
  byte            *on;                          // Pointer to first on-yomi entry.
  byte            *kun;                         // Pointer to first kun-yomi entry.
  byte            *imi;                         // Pointer to first imi (meaning) entry.
  byte            *nan;                         // Pointer to first nanori entry.
  byte            *xref;                        // Start of the cross-reference entries.
  short            freq;                        // Character frequency
  short            sh_kana;                     // Spahn and Hadamitzky Kana & Kanji
  short            henshall;                    // "A Guide To Remembering Japanese Characters" by Kenneth G. Henshall
  short            gakken;                      // Gakken Kanji Dictionary ("A  New Dictionary  of Kanji Usage")
  short            heisig;                      // "Remembering The Kanji" by James Heisig
  short            oneill;                      // "Japanese Names", by P.G. O'Neill
  short            fc_main2;                    // Four-corners secondary code <code><resolution is stroed in the struture>
//  short            count;                       // Number of kanji in the database. (NOT USED AT THIS TIME)
//  KANJI            last_jis;                    // Last JIS value in the database.  (NOT USED AT THIS TIME)

  int  dlg_kanjiinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);  // Dialog box procedure
  int  dlg_moreinfo  (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);  // Dialog box procedure
  int  dlg_xrefinfo  (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);  // Dialog box procedure
  int  open_info     (HWND hwnd);                   // Open the kanji-info file.
  void get_info      (int ch,int amount);           // Get kanji info record for a character.
  int  get_stroke    (int ch);                      // Get stroke count for character (used by the kanji search rotuines)
  void close_info    (void);                        // Close info
private:
  void setup_char  (HWND hwnd);                     // Setup dialog box to display a character.
  void init_dialog (HWND hwnd);                     // Intialize main character info dialog.
  KANJI     ch;                                     // Kanji we are working with.
  HANDLE    handle;                                 // Hanlde for accessing the kanji-info file.
};

typedef KANJI_info KANJI_info;

extern void do_kanjicount   (void);                 // Implements the count kanji feature.
extern void free_info       (void);                 // Free memory allocated for the kanji info.
extern int  initialize_info (WNDCLASS *wclass);     // Register classes needed by the info routines.
extern void kanji_info      (HWND hwnd,int kanji);  // Get kanji finformation for a character

#endif jwp_info_h


