//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//  The database read by JWPce is dirived directly from KANJIDIC     //
//  database dirived by Jim Breen.  Please see the _cpright.txt file //
//  for additional information.                                      //
//                                                                   //
//===================================================================//

//===================================================================
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

//--------------------------------
//
//  Fixed kanji info structure.
//
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

//--------------------------------
//
//  Extended data structrue.
//
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
//  Identifiers for kanji info dialog items.
//
#define INFO_BLANK              0
#define INFO_TYPE               1
#define INFO_JIS                2
#define INFO_SHIFTJIS           3
#define INFO_UNICODE            4
#define INFO_STROKE             5
#define INFO_GRADE              6
#define INFO_NELSON             7
#define INFO_HALPERN            8
#define INFO_SPAHN              9
#define INFO_FOURCORNERS        10
#define INFO_MOROHASHI          11
#define INFO_PINYIN             12
#define INFO_KOREAN             13

#define INFO_FREQUENCY          14
#define INFO_HENSHALL           15
#define INFO_GAKKEN             16
#define INFO_HEISIG             17
#define INFO_ONEILL             18
#define INFO_DEROO              19
#define INFO_KANJILEARN         20
#define INFO_READWRITE          21
#define INFO_TUTTLECARDS        22
#define INFO_KANJIWAY           23
#define INFO_KANJICONTEXT       24
#define INFO_BUSYPEOPLE         25
#define INFO_COMPACTGUIDE       26

#define INFO_MAXITEM INFO_COMPACTGUIDE  // Last info-item code.

//--------------------------------
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
  short            oneill_ek;                   // P.G. O'Neill's Essential Kanji (ISBN 0-8348-0222-8). 
  short            halpern_kld;                 // Jack Halpern in his Kanji Learners Dictionary, published by Kodansha in 1999
  short            deroo;                       // Father Joseph De Roo, and published in his book "2001 Kanji"
  short            count;                       // Number of kanji in the database. (NOT USED AT THIS TIME)
  short            readwrite;                   // "A Guide To Reading and Writing Japanese" edited by Florence Sakade. 
  short            tuttlecard;                  // Tuttle Kanji Cards, compiled by Alexander Kask. 
  short            kanjiway;                    // "The Kanji Way to Japanese Language Power" by Dale Crowley. 
  short            kanjicontext;                // "Kanji in Context" by Nishiguchi and Kono. 
  short            kanjiguide;                  // "Kodansha Compact Kanji Guide". 
  short            busypeople;                  // "Japanese For Busy People" vols I-III, published by the AJLT. The codes are the volume.chapter. 
  KANJI            last_jis;                    // Last JIS value in the database.

  int  dlg_kanjiinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);  // Dialog box procedure
  int  dlg_moreinfo  (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);  // Dialog box procedure
  int  dlg_xrefinfo  (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);  // Dialog box procedure
  int  open_info     (HWND hwnd);                   // Open the kanji-info file.
  void get_info      (int ch,int amount);           // Get kanji info record for a character.
  int  get_stroke    (int ch);                      // Get stroke count for character (used by the kanji search rotuines)
  void close_info    (void);                        // Close info
private:
  void format_line (HWND hwnd,int line,int code);   // Format and place an object in the display.
  void setup_char  (HWND hwnd);                     // Setup dialog box to display a character.
  void init_dialog (HWND hwnd);                     // Intialize main character info dialog.
  void format_xref (HWND hwnd);                     // Format the xref.
  KANJI     ch;                                     // Kanji we are working with.
  HANDLE    handle;                                 // Hanlde for accessing the kanji-info file.
  HWND      dialog;                                 // Dialog pointer.
};

typedef KANJI_info KANJI_info;

extern void do_kanjicount   (void);                 // Implements the count kanji feature.
extern void free_info       (void);                 // Free memory allocated for the kanji info.
extern void info_config     (HWND hwnd);            // Open configure kanji-info dialog.
extern int  initialize_info (WNDCLASS *wclass);     // Register classes needed by the info routines.
extern void kanji_info      (HWND hwnd,int kanji);  // Get kanji finformation for a character

#endif jwp_info_h


