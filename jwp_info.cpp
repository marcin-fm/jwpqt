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
//  Structure of the KINFO files:
//
//  The following indicates the structure of the files written by this
//  utility.  The actual infirmaton content of the files can differ, but
//  the structure is identical.
//
//      MAGIC    -- 4 byte binary number used to verify the file type.
//      FLAGS    -- Flags indicating what is written into the file:
//
//                  KIFLAG_PINYIN (0x0001) -- Has pin yin data.
//                  KIFLAG_KOREAN (0x0002) -- Has Korean data.
//                  KIFLAG_NANORI (0x0004) -- Has nanori data.
//                  KIFLAG_EXTRA  (0x0008) -- Has extended data filed
//                  KIFLAG_EVAR   (0x0010) -- Has extended variable data
//                  KIFLAG_XREF   (0x0020) -- Has cross refference data
//
//      COUNT    -- Number of entries written into the file.
//      MAXJIS   -- Highest JIS code included in the file.
//
//      FIXED    -- A fixed file part.  This contains a sequency of 
//                  6355 kinfo structures representing each kanji.
//                  The structual fields are:
//                    
//                  offset      -- Points the VARIABLE part for this kanij.
//                  imi,nan,    -- Indicate the number entries of each
//                    on,kun,      type contained in the variable part.
//                    pinyin,      As data is removed to make th efile 
//                    korean,      smaller, these values are reduced (or
//                    extra        set to zero) and the correspoding 
//                                 entries are removed from the file.
//                                          
//      VARIABLE -- The variable part consists of a number of null 
//                  termianted strings writtent sequecially into the 
//                  file.  The above counters indicate the number 
//                  of each type of string.  The order of the strings
//                  is as follows:
//
//                  Korean   -- Korean data if pressent.
//                  PinYin   -- Ascii with extended charactrs to get
//                              accents.
//                  meanings -- Ascii.
//                  on-yomi  -- Compressed katakana.  The first byte is 
//                              removed, to save space.  The special
//                              code 0x1f indicates a katakana -.
//                  kun-yomi -- Compressed hiragana.  The first byte
//                              is removed, to save space.  The special
//                              code 0x1f indicates a katakana -, and 
//                              the special code 0x20 indicates the 
//                              beginning of the okurigana.
//                  nanori   -- Compressed hiragana.  Special cdoes 
//                              above are supported but not used.
//                  Extra    -- Extended field if pressent (see below for description)
//
//      EXTRA -- The extra is a binary field.  This must be the last field, because it
//               is possible to get embbedded zeros in this field.  The layout of the 
//               field is a fixed length binary structrue followed by a sequence of 
//               one-byte codes followed by the data for that argument.  The data format 
//               varies according to the idividual code field.  Much of this data 
//               is not pressent in all entries, so this type of coding takes less space.  
//               Additionally, this allows the design to change depending on what is to 
//               be added later.  
//
//              F<short>        -- Frequency count.
//              I<short>        -- Spahn and Hadamitzky Kana & Kanji
//              E<short>        -- "A Guide To Remembering Japanese Characters" by Kenneth G. Henshall
//              K<short>        -- Gakken Kanji Dictionary ("A  New Dictionary  of Kanji Usage")
//              L<short>        -- "Remembering The Kanji" by James Heisig
//              O<short>        -- "Japanese Names", by P.G. O'Neill
//              Q<short>        -- Four-corners secondary code <code><resolution is stroed in the struture>
//
//  The organization of the extra field, will always include all entries before the cross-reference
//  entries are added.  This allows the reader to mark the frist cross-reference entry and 
//  decode from there forward.  Note the cross-reference codes are all lower case, which makes
//  them easy to identifiy.
//
//              h<short>        -- Halpern dictionary cross-refernece entries.
//              n<short>        -- Nelson dictionary cross-reference entries.
//              o<short>        -- "Japanese Names", by P.G. O'Neill cross-refernece
//              k<short>        -- JIS 0208 Cross-reference
//              j<short>        -- JIS 0212 Cross-reference
//              z<short>        -- SKIP code missclassification/cross-reference 
//                                  Coded as (<error><<13) | (<type> << 10) | (<1> << 5)) | (<2>)
//
//      DROPPED ENTRY (only one of these):
//              i<short>        -- Spahn and Hadamitzky Kana & Kanji cross-reference 
//                                 coded as (rstorke << 11) | (ostroke<<6)
//
//  The first part of the file conists of a number of kinfo 
//  structures that are arranged simply in a packed order of the 
//  kanji in the JIS table.  Note that the structure is packed, and 
//  data elements are allingned on word boundaries.  This gives a total
//  structure size of 14 bytes, which is unusual for higher end systems.
//
//  The on/kun lines are stored in a compressed format, that is simply 
//  the last byte of the caracter.  Thus they must be combained with 
//  BASE_HIRAGANA, or BASE_KATAKANA to form an actual character.  Within
//  the on lines, the special character 0x1f is used to indicate a 
//  katakana -.  Within the kun lines, the special character 0x1f is 
//  used to indicate a katakana -, and the special character 0x20 is 
//  used to indicate the start of an okurigana.  This translates to 
//  a Japanese (.  Note that the closing ) is not contained within the 
//  info file, and must be added by the programmer.  (Note the 
//  implementation I use does not distinguish between the on/kun lists,
//  and uses the same interpreter for both.  Since, however, the on-yomi
//  should not contain the code 0x20, this is not a problem.)
//
//  Understanding the big buffer:
//
//  Because the information contained in the second part of the kanji-
//  info file is free form (i.e. varies in length), this information 
//  is broken up by having it in lines.  Unfortunately this presents 
//  some major problems.  First, windows file system does not deal 
//  with line delminated files (ther is no read-line).  Normally, one
//  might use the c++ or c file systems to read this type of data.  
//  In this case I do not want to load either of these systems, to keep
//  the code size small and fast.  
//
//  To get around this problem, I use brute force.  I read a big buffer
//  full of data starting at the location idnciated by the kinfo.
//  offset field and break this up in memory.  This has two plusses.
//  it is really fast, and it is really easy.
//
//  Most of this message is old, the buffer space required is small 
//  now that I am using my own database files.  The databese building
//  utility will actually tell you how long the buffer needs to be.
//

#include "jwpce.h"
#include "jwp_clip.h"
#include "jwp_conf.h"
#include "jwp_edit.h"
#include "jwp_file.h"
#include "jwp_font.h"
#include "jwp_help.h"
#include "jwp_info.h"
#include "jwp_inpt.h"
#include "jwp_misc.h"

//===================================================================
//
//  Compile time options.
//
#define NAME_KANJIINFO  TEXT("kanjinfo.dat")    // Name of the main kanji-info file.

#define KINFO_MAGIC     0x34A5B4D4      // This value is used to confirm that 
                                        //   this is the correct verison of the 
                                        //   KANJINFO.DAT file.

#define KANJIINFO_OFFSET    (2*sizeof(ulong)+2*sizeof(ushort))  //  Offset of first entry in the file.

#define SIZE_KANJI      (6355+10)       // Number of kanji alloacted by the 
                                        //   counting procedrue.  This is the 
                                        //   Number of different kanji we can 
                                        //   handle.  JWPce fonts have 6355 kanji,
                                        //   plus we alloow 10 odd ones.

#define USE_ASCII_OKURIGANA             // If defined causes the system to 
                                        //   use ascii ( and ) to surround
                                        //   okurigana, instead of using 
                                        //   Japanese versions.  This looks
                                        //   better to me.

#define PROCESS_WIDE_CHARACTERS         // If set, this cuases the kanji-info
                                        //   dialog box to do extra processing
                                        //   for wide characters, that do not 
                                        //   correctly fit in the character box.
                                        //   This only effects ascii characters.
                                        //   This will cause the system to check
                                        //   if characters are too wide (height
                                        //   is set so it is always correct), and
                                        //   if so, the system will open a smaller
                                        //   font for that character.

#define COUNT_SEPARATOR KANJI_SLASH     // Character used to separtate on-yomi and 
                                        //   kun-yomi reading in the count kanji
                                        //   dialog box.

#if (!defined(WINCE))                       // Windows version.

  #define INFO_MAXLINES   27                // Maxinum number of displayable items for info dialog.
  #define INFO_FIRSTINDEX 0                 // First index into the aray of info dialog items.
  #define INFO_FIRST      IDC_KILABEL13     // First dynamic control for info dialog.
  #define INFO_LAST       IDC_KIITEM21      // Last dynamic control for info dialog.
  #define MORE_MAXLINES   13                // Maximum number of items for more info dialog
  #define MORE_FIRSTINDEX 13                // First index into items array for more info dialog.
  #define MORE_FIRST      0                 // First dynamic control for more info dialog.
  #define MORE_LAST       0                 // Last dynamic control for more info dialog.
  #define COLOR_BUSHU     COLOR_BTNFACE     // Bushu background color.

#elif (!(defined(WINCE_PPC) || defined(WINCE_POCKETPC)))    // HPC version

  #define INFO_MAXLINES   9                 // Maxinum number of displayable items for info dialog.
  #define INFO_FIRSTINDEX 0                 // First index into the aray of info dialog items.
  #define INFO_FIRST      IDC_KILABEL9      // First dynamic control for info dialog.
  #define INFO_LAST       IDC_KIITEM10      // Last dynamic control for info dialog.
  #define MORE_MAXLINES   17                // Maximum number of items for more info dialog
  #define MORE_FIRSTINDEX 9                 // First index into items array for more info dialog.
  #define MORE_FIRST      0                 // First dynamic control for more info dialog.
  #define MORE_LAST       0                 // Last dynamic control for more info dialog.
  #define COLOR_BUSHU     COLOR_BTNFACE     // Bushu background color

#else                                       // PPC/PocketPC version        

  #define INFO_MAXLINES   6                 // Maxinum number of displayable items for info dialog.
  #define INFO_FIRSTINDEX 0                 // First index into the aray of info dialog items.
  #define INFO_FIRST      0                 // First dynamic control for info dialog.
  #define INFO_LAST       0                 // Last dynamic control for info dialog.
  #define MORE_MAXLINES   12                // Maximum number of items for more info dialog
  #define MORE_FIRSTINDEX 6                 // First index into items array for more info dialog.
  #define MORE_FIRST      0                 // First dynamic control for more info dialog.
  #define MORE_LAST       0                 // Last dynamic control for more info dialog.
  #define XREF_MAXLINES   8                 // Maximum number of items for more info dialog
  #define XREF_FIRSTINDEX 18                // First index into items array for more info dialog.
  #define XREF_FIRST      0                 // First dynamic control for more info dialog.
  #define XREF_LAST       0                 // Last dynamic control for more info dialog.
  #define COLOR_BUSHU     COLOR_WINDOW      // Bushu background color

#endif

//===================================================================
//
//  static data and definitions.
//
//  These are various static data used in some of the routines.
//

static KANJI_info *single_info = NULL;          // Pointer to info object used when only a single info is used.
static SIZE_window info_size;                   // Class used to allow dynamic sizing on the info dialog.
static SIZE_window more_size;                   // Class used to allow dymanic sizing of the more kanji-info dialog.
static SIZE_window count_size;                  // Class used to allow dynamic sizing of the count kanji dialog.

//--------------------------------
//
//  These are the label names for items in the info box.
//  
//  I did not count on them being in order because this makes it easier to add a new one.
//
static short info_codes[] = { IDS_KI_BLANK,
                              IDS_KI_ITEMTYPE,IDS_KI_ITEMJISCODE,IDS_KI_ITEMSHIFTJIS,IDS_KI_ITEMUNICODE,IDS_KI_ITEMSTROKES,IDS_KI_ITEMGRADE,IDS_KI_ITEMHELSON,IDS_KI_ITEMHALPERN,IDS_KI_ITEMSPAHN,IDS_KI_ITEMFOURCORNERS,
                              IDS_KI_ITEMMOROHASHI,IDS_KI_ITEMPINYIN,IDS_KI_ITEMKOREAN,IDS_KI_ITEMFREQUENCY,IDS_KI_ITEMHENSHALL,IDS_KI_ITEMGAKKEN,IDS_KI_ITEMHEISIG,IDS_KI_ITEMONEILL,IDS_KI_ITEMDEROO,IDS_KI_ITEMKANJILEARN,
                              IDS_KI_READWRITE,IDS_KI_TUTTLECARD,IDS_KI_KANJIWAY,IDS_KI_KANJICONTEXT,IDS_KI_BUSYPEOPLE,IDS_KI_COMPACTGUIDE,
                            };

//--------------------------------
//
//  This table converts bushu numbers into their JIS character codes.
//  The index into the table is the bushu-1.
//
static KANJI bushu_symbols[] = {
    0x306c, 0x2143, 0x5026, 0x254e, 0x3235,     /* 01 - 05 */
    0x502d, 0x4673, 0x5035, 0x3f4d, 0x5139,     /* 06 - 10 */
    0x467e, 0x482c, 0x5144, 0x514c, 0x5152,     /* 11 - 15 */
    0x515c, 0x5161, 0x4561, 0x4e4f, 0x5231,     /* 16 - 20 */
    0x5238, 0x5239, 0x523e, 0x3d3d, 0x4b4e,     /* 21 - 25 */
    0x5247, 0x524c, 0x5253, 0x4b74, 0x387d,     /* 26 - 30 */
    0x5378, 0x455a, 0x3b4e, 0x5469, 0x546a,     /* 31 - 35 */
    0x4d3c, 0x4267, 0x3d77, 0x3b52, 0x555f,     /* 36 - 40 */
    0x4023, 0x3e2e, 0x5577, 0x5579, 0x5625,     /* 41 - 45 */
    0x3b33, 0x406e, 0x3929, 0x384a, 0x3652,     /* 46 - 50 */
    0x3433, 0x5676, 0x5678, 0x572e, 0x5730,     /* 51 - 55 */
    0x5735, 0x355d, 0x2568, 0x5744, 0x5746,     /* 56 - 60 */
    0x3f34, 0x5879, 0x384d, 0x3c6a, 0x3b59,     /* 61 - 65 */
    0x5a3d, 0x4a38, 0x454d, 0x3654, 0x4a7d,     /* 66 - 70 */
    0x5a5b, 0x467c, 0x5b29, 0x376e, 0x4c5a,     /* 71 - 75 */
    0x3767, 0x3b5f, 0x5d46, 0x5d55, 0x5d59,     /* 76 - 80 */
    0x4866, 0x4c53, 0x3b61, 0x5d63, 0x3f65,     /* 81 - 85 */
    0x3250, 0x445e, 0x4963, 0x602b, 0x602d,     /* 86 - 90 */
    0x4a52, 0x3267, 0x356d, 0x3824, 0x383c,     /* 91 - 95 */
    0x364c, 0x313b, 0x3424, 0x3445, 0x4038,     /* 96 - 100 */
    0x4d51, 0x4544, 0x4925, 0x4942, 0x6222,     /* 101 - 105 [ 104 questionable ] */
    0x4772, 0x4869, 0x3b2e, 0x4c5c, 0x4c37,     /* 106 - 110 */
    0x4c70, 0x4050, 0x3c28, 0x633b, 0x3253,     /* 111 - 115 [ 114 questionable ] */
    0x376a, 0x4e29, 0x435d, 0x4a46, 0x3b65,     /* 116 - 120 */
    0x344c, 0x6626, 0x4d53, 0x3129, 0x4f37,     /* 121 - 125 */
    0x3c29, 0x6650, 0x3c2a, 0x6666, 0x4679,     /* 126 - 130 */
    0x3f43, 0x3c2b, 0x3b6a, 0x3131, 0x4065,     /* 131 - 135 */
    0x4124, 0x3d2e, 0x3a31, 0x3f27, 0x6767,     /* 136 - 140 */
    0x6948, 0x436e, 0x376c, 0x3954, 0x3061,     /* 141 - 145 */
    0x403e, 0x382b, 0x3351, 0x3840, 0x432b,     /* 146 - 150 */
    0x4626, 0x6c35, 0x6c38, 0x332d, 0x4056,     /* 151 - 155 */
    0x4176, 0x422d, 0x3f48, 0x3c56, 0x3f49,     /* 156 - 160 */
    0x4324, 0x6d68, 0x4d38, 0x4653, 0x4850,     /* 161 - 165 [ 162 questionable ] */
    0x4e24, 0x3662, 0x4439, 0x4c67, 0x496c,     /* 166 - 170 */
    0x7030, 0x7032, 0x312b, 0x4044, 0x4873,     /* 171 - 175 */
    0x4c4c, 0x3357, 0x706a, 0x706c, 0x323b,     /* 176 - 180 */
    0x4a47, 0x4977, 0x4874, 0x3f29, 0x3c73,     /* 181 - 185 */
    0x3961, 0x474f, 0x397c, 0x3962, 0x7175,     /* 186 - 190 */
    0x7228, 0x722e, 0x722f, 0x3534, 0x357b,     /* 191 - 195 */
    0x443b, 0x7343, 0x3c2f, 0x734e, 0x4b63,     /* 196 - 200 */
    0x322b, 0x3550, 0x3975, 0x7363, 0x7366,     /* 201 - 205 */
    0x4524, 0x385d, 0x414d, 0x4921, 0x736e,     /* 206 - 210 */
    0x736f, 0x4e36, 0x737d, 0x737e,             /* 211 - 214 */
};

//===================================================================
//
//  static routines.
//
static BOOL CALLBACK dialog_kanjiinfo  (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
static BOOL CALLBACK dialog_moreinfo   (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
static BOOL CALLBACK dialog_xrefinfo   (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);

static byte *put_line    (EUC_buffer *line,int base,byte *ptr,int add);             // Format an output line for the kanji-info list-box.
static byte *put_reading (EUC_buffer *line,int base,byte *ptr,int index,int last);  // Special rotuine used to place readings.
static void  put_string  (EUC_buffer *line,tchar *string);                          // Add a string to the list.
static byte *skip_line   (byte *ptr);                                               // Skip to next line in the buffer.

//--------------------------------
//
//  Stub routine for kanji-info dialog box.
//
//  This routine has to save the parameter passed during the intialization.
//  This parameter is the pointer to the KANJI_info object associated
//  with the dialog.  The routine then always recalls the saved 
//  parameter and uses it to call the object's dialog box procedure.
//
static BOOL CALLBACK dialog_kanjiinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  KANJI_info *info;
  if (message == WM_INITDIALOG) SetWindowLong (hwnd,GWL_USERDATA,lParam);
  info = (KANJI_info *) GetWindowLong(hwnd,GWL_USERDATA);
  return (info->dlg_kanjiinfo(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  Stub routine for the more-info dialog box.
//
//  This routine has to save the parameter passed during the intialization.
//  This parameter is the pointer to the KANJI_info object associated
//  with the dialog.  The routine then always recalls the saved 
//  parameter and uses it to call the object's dialog box procedure.
//
static BOOL CALLBACK dialog_moreinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  KANJI_info *info;
  if (message == WM_INITDIALOG) SetWindowLong (hwnd,GWL_USERDATA,lParam);
  info = (KANJI_info *) GetWindowLong(hwnd,GWL_USERDATA);
  return (info->dlg_moreinfo(hwnd,message,wParam,lParam));
}

//--------------------------------
//
//  Stub routine for the xref-info dialog box.
//
//  This routine has to save the parameter passed during the intialization.
//  This parameter is the pointer to the KANJI_info object associated
//  with the dialog.  The routine then always recalls the saved 
//  parameter and uses it to call the object's dialog box procedure.
//
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
static BOOL CALLBACK dialog_xrefinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  KANJI_info *info;
  if (message == WM_INITDIALOG) SetWindowLong (hwnd,GWL_USERDATA,lParam);
  info = (KANJI_info *) GetWindowLong(hwnd,GWL_USERDATA);
  return (info->dlg_xrefinfo(hwnd,message,wParam,lParam));
}
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))

//--------------------------------
//
//  This utility rotuine formats a string for the list-box containned
//  in the kanji-info dialog box.  This routine is used for on-yomi, 
//  kun-yomi, meanings, and titles.
//
//      line   -- Pointer to EUC_buffer class object associated with the 
//                list box.
//      base   -- Character to base to use.  This determines the type of 
//                line being formatted:
//
//          0             -- Meaning line.
//          BASE_KATAKANA -- on-yomi.
//          BASE_HIRAGANA -- kun-yomi.
//
//      ptr    -- Pointer to current location in buffer.  To optimize the 
//                file operations, all the line information in the knaji-
//                info file is read in one block and decoded in memory. 
//                On entry this pointer points to the beginning of the 
//                line.  The RETRUN value points the beginning of the 
//                next line.
//      add    -- If set to true, causes this to be added to the current
//                line instead of repolacing the current line.
//
//      RETURN -- Points to the beginning of the next line in the buffer.
//
//  The following special characters are supported:
//
//      0x1f (all bases)         -- produces a katakana -.
//      high-bit set (all bases) -- produces a kanji (, indicating the 
//                                  beginning of a origana.  The closing 
//                                  ) will be added by the formating 
//                                  routine automatically.
//
#ifdef USE_ASCII_OKURIGANA
  #define PARAN_LEFT    '('
  #define PARAN_RIGHT   ')'
#else  USE_ASCII_OKURIGANA
  #define PARAN_LEFT    KANJI_LPARAN
  #define PARAN_RIGHT   KANJI_RPARAN
#endif USE_ASCII_OKURIGANA

static byte *put_line (EUC_buffer *line,int base,byte *ptr,int add) {
  int okurigana = false;
  if (!add) line->clear ();
  if (!base) {                      // If not base then this is a menaing field so the data is EUC.
    while (*ptr) line->put_char (utf2jis(ptr));
  }
  else {                            // If we have a base then this is a single byte EUC type encoding.
    while (*ptr) {
      if (*ptr == 0x1f) line->put_char (KANJI_DASH);
        else {
          if (*ptr & 0x80) { line->put_char(PARAN_LEFT); okurigana = true; }
          line->put_char (base | (*ptr & 0x7F));
        }
      ptr++;
    }
  }
  if (okurigana) line->put_char (PARAN_RIGHT);
  if (!add) line->flush (-1);
  return (ptr+1);
}

//--------------------------------
//
//  This is a wrapper for the rotuine put_line() that does special processing for 
//  putting the reading fields for kanji.   This routine handles the compressed/
//  non-compressed display of the character data.
//
//      line   -- Pointer to EUC_buffer class object associated with the 
//                list box.
//      base   -- Character to base to use.  This determines the type of 
//                line being formatted:
//
//          0             -- Meaning line.
//          BASE_KATAKANA -- on-yomi.
//          BASE_HIRAGANA -- kun-yomi.
//
//      ptr    -- Pointer to current location in buffer.  To optimize the 
//                file operations, all the line information in the knaji-
//                info file is read in one block and decoded in memory. 
//                On entry this pointer points to the beginning of the 
//                line.  The RETRUN value points the beginning of the 
//                next line.
//      index  -- Current index into the reading type being processed.
//      last   -- Last index in reading type being processed.
//
//      RETURN -- Points to the beginning of the next line in the buffer.
//
static byte *put_reading (EUC_buffer *line,int base,byte *ptr,int index,int last) {
  if (!jwp_config.cfg.info_compress) return (put_line(line,base,ptr,false));    // Uncompressed line, just pass through.
  if (!index) line->clear ();                       // Start of a compressed block.
    else {          
      if (base) line->put_char (KANJI_CAMA);        // Continuing a compressed block
        else {
          line->put_char (',');
          line->put_char (' ');
        }
    }
  ptr = put_line(line,base,ptr,true);               // Output actual line
  if (index+1 == last) line->flush (-1);            // End of compressed block, flush the converter
  return (ptr);
}

//--------------------------------
//
//  This utility rotuine formats a string for the list-box containned
//  in the kanji-info dialog box.  This is usd only for simple lines.
//
//      line   -- Pointer to EUC_buffer class object associated with the 
//                list box.
//      ptr    -- Pointer to string to be formatted into the text. 
//
static void put_string (EUC_buffer *line,tchar *ptr) {
  line->clear ();
  while (*ptr) line->put_char (*ptr++);
  line->flush (-1);
  return;
}

//--------------------------------
//
//  Small utility routine used to skip some strings in the line buffer
//  during processing.  Basically this is used to skip to the meaning 
//  strings first.
//
//      ptr    -- Pointer to current location in buffer.  To optimize the 
//                file operations, all the line information in the knaji-
//                info file is read in one block and decoded in memory. 
//                On entry this pointer points to the beginning of the 
//                line.  The RETRUN value points the beginning of the 
//                next line.
//
//      RETURN -- Points to the beginning of the next line in the buffer.
//
static byte *skip_line (byte *ptr) {
  ptr += strlen((char *) ptr)+1;
  return (ptr);
}

//===================================================================
//
//  Window procedures
//
//  These are specilized windows procedures used in the Kanji Info 
//  dialog box.  These replace windows static displays that were previously 
//  used in JWPce, and were changed to allow support for Windows CE.
//
//

//--------------------------------
//
//  Window procedure for window class used to display the bushu character
//  This class simply accepts a single bit of data that determins the 
//  bushu character.  Setting this character to zero will suppress display
//  of the bushu.
//
static LRESULT CALLBACK JWP_bushu_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC           hdc;
  CREATESTRUCT *create;
  PAINTSTRUCT   ps;
  int           bushu;

  switch (iMsg) {
//
//  Here we primarally need to adjust the size of the window.
//
    case WM_CREATE:                                 
         create = (CREATESTRUCT *) lParam;
         MoveWindow (hwnd,create->x,create->y,sys_font.hwidth,sys_font.height,true);
         lParam = 0;
//
//  This is the set command that sets the color we are to display.
//
    case WMU_SETWINDOWVALUE:        // *** FALL THROUGH *** 
         SetWindowLong  (hwnd,0,lParam);
         InvalidateRect (hwnd,NULL,true);
         return (0);
//
//  This does the actual redraw.
//
    case WM_PAINT:
         hdc   = BeginPaint (hwnd,&ps);
         bushu = GetWindowLong(hwnd,0);
         SetBkColor   (hdc,GetSysColor(COLOR_BUSHU));
         SetTextColor (hdc,GetSysColor(COLOR_WINDOWTEXT));
         if ((bushu >= 1) && (bushu <= 214)) {  
           sys_font.kanji->draw (hdc,bushu_symbols[bushu-1],0,sys_font.kanji->height);
         }
         EndPaint (hwnd,&ps);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//--------------------------------
//
//  Dialog box procedure for the display window that pops up so you can
//  get a better look at the kanji in the PPC version.  This dialog box
//  should be created with a call to DialogBoxParam, so the kanji to 
//  be displayed in the dialog can be passed to the program!
//
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
static byte kanji_view = false; // This variable is used to keep this 
                                // dialog from being created more than
                                // once.  When the dialog is entered,
                                // this is set to true.  This will prevent
                                // the kanji view from launching the 
                                // dialog again.  When the dialog is 
                                // termianted, this is cleared.

static BOOL CALLBACK dialog_kanjiview (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  switch (message) {
    case WM_INITDIALOG:
         kanji_view = true;     // Block second creation.
         SendDlgItemMessage (hwnd,IDC_KIBIGKANJI,WMU_SETWINDOWVALUE,0,lParam);
         return (false);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_CHARINFO);
         return  (true);
//
//  Process push buttons.
//
    case WM_COMMAND:    
         switch (LOWORD(wParam)) {
           case IDOK:
           case IDCANCEL:
                kanji_view = false;     // Allow dialog to be generated again.
                EndDialog (hwnd,false);
                return (true);
         }
         break;
  }
  return (false);
}
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))

//--------------------------------
//
//  Window producedure for the control to draw the big kanji character.
//  This is a relativly simple rendering routine.
//
static LRESULT CALLBACK JWP_kanji_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC           hdc;        // Display context
  CREATESTRUCT *create;     // Create window structure
  PAINTSTRUCT   ps;         // Update paint structure
  LOGFONT       lf;         // Logical-Font structure used to make large 
                            //   version of the ascii font for the big-char box.
  RECT          rect,full;  // Rectangle
  KANJI_font   *big;        // Large kanji font for window.
  HFONT         font;       // Font for ascii font.
  int           kanji;      // The character
  int           width;      // Display width of character

  switch (iMsg) {
//
//  Here we primarally need to adjust the size of the window.
//
    case WM_CREATE:                                 
         create = (CREATESTRUCT *) lParam;
         lParam = 0;
//
//  This is the set command that sets the color we are to display.
//
    case WMU_SETWINDOWVALUE:        // *** FALL THROUGH *** 
         SetWindowLong  (hwnd,0,lParam);
         InvalidateRect (hwnd,NULL,true);
         return (0);
//
//  For PPC version, we allow clicking on the small kanji view to 
//  make a large kanji view.
//
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
    case WM_LBUTTONDOWN:
         kanji = GetWindowLong(hwnd,0);
         if (kanji && !kanji_view) JDialogBox (IDD_KANJIVIEW,hwnd,(DLGPROC) dialog_kanjiview,kanji);
         return (0);
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
//
//  Double click
//
    case WM_LBUTTONDBLCLK:
         SendMessage (GetParent(hwnd),WM_COMMAND,GetWindowLong(hwnd,GWL_ID),0);
         return (0);
//
//  This does the actual redraw.
//
    case WM_PAINT:
         hdc   = BeginPaint (hwnd,&ps);
         kanji = GetWindowLong(hwnd,0);
         SetBkColor    (hdc,GetSysColor(COLOR_WINDOW));
         SetTextColor  (hdc,GetSysColor(COLOR_WINDOWTEXT));
         GetClientRect (hwnd,&full);
         rect.right  = full.right -sys_font.hspace;
         rect.left   = full.left  +sys_font.hspace;
         rect.top    = full.top   +sys_font.vspace;
         rect.bottom = full.bottom-sys_font.vspace;
         if (kanji) {  
//
//  Render kanji characters.
//
           if (ISJIS(kanji)) {
             big = get_bigfont(&full);
             big->fill (hdc,kanji,&rect);    // Kanji characters are easy!
           }
//
//  Ascii characters involve making a very big font, and fitting the 
//  font into the box.  If the PROCESS_WIDE_CHARCTERS is on, we may 
//  have to do the whole thing twice.  Ther first time, we generate 
//  the font, we see if the character is too wide.  If it is then we
//  propotinally scale down the font, and make a other attempt.
//
           else {
             memset (&lf,0,sizeof(lf));
             lstrcpy (lf.lfFaceName,jwp_config.cfg.ascii_font.name);
             lf.lfHeight = rect.top-rect.bottom;
             if (!(font = CreateFontIndirect(&lf))) return (true);
             font = (HFONT) SelectObject (hdc,font);
             SetBkMode (hdc,TRANSPARENT);
             width = ascii_width(hdc,kanji);
#ifdef PROCESS_WIDE_CHARACTERS
             if (width > rect.right-rect.left) {    // Character is too wide.
               SelectObject (hdc,font);
               DeleteObject (font);
               lf.lfHeight = ((rect.top-rect.bottom)*(rect.right-rect.left))/width;
               if (!(font = CreateFontIndirect (&lf))) return (true);
               font  = (HFONT) SelectObject (hdc,font);
               width = ascii_width(hdc,kanji);
             }
#endif PROCESS_WIDE_CHARACTERS
             ascii_draw   (hdc,rect.left+(rect.right-rect.left-width)/2,rect.top,kanji);
             SelectObject (hdc,font);
             DeleteObject (font);
           }
         }
         EndPaint (hwnd,&ps);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//--------------------------------
//
//  This routine registers the two classes that are used by the kanji
//  info dialog.
//
//      wclass -- Partially initialize window class structure.
//      
//      RETURN -- Non-zero indicates an error.
// 
int initialize_info (WNDCLASS *wclass) {
//
//  Register window classes
//
  if (wclass) {
    wclass->style         = CS_HREDRAW | CS_VREDRAW;                // Bushu character window
    wclass->hbrBackground = (HBRUSH) (COLOR_BUSHU+1);
    wclass->lpfnWndProc   = JWP_bushu_proc;
    wclass->lpszClassName = TEXT("JWP-Bushu");
    if (!RegisterClass(wclass)) return (true);
    wclass->style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;   // Large kanji window.
    wclass->hbrBackground = (HBRUSH) (COLOR_WINDOW+1);  
    wclass->lpfnWndProc   = JWP_kanji_proc;
    wclass->lpszClassName = TEXT("JWP-Kanji");
    if (!RegisterClass(wclass)) return (true);
  }
//
//  Get startup info form the kanji information database.
//
  ulong done;
  KANJI_info info;
  if (info.open_info(null)) return (false);
  ReadFile (info.handle,&jwp_config.kanji_flags,sizeof(long),&done,NULL);
//  ReadFile (handle,&count   ,sizeof(short),&done,NULL);     // Get the number of kanji in database (NOT USED AT THIS TIME)
//  ReadFile (handle,&last_jis,sizeof(short),&done,NULL);     // Last kanji in the database. (NOT USED AT THIS TIME)
  info.close_info ();
  return (false);
}

//===================================================================
//
//  Small service rotuines used to display a particular kind of kanji 
//  information data.
//

//--------------------------------
//
//  Formats a four-corners entry.  This handles convertin the entry to a black if the
//  -1 code is in the main field.
//
//      buffer -- Location to store the formatted entry.
//      main   -- Main entry (4 courners).
//      index  -- 5th corner.
//
//      RETURN -- Pointer to buffer.
//
static TCHAR *format_fc (TCHAR *buffer,int main,int index) {
  if (main == -1) buffer[0] = 0; else wsprintf (buffer,TEXT("%04d.%d"),main,index);
  return (buffer);
}

//--------------------------------
//
//  Formats an integer entry.  This is really designed to be used with format_two(), 
//  otherwise it is better to use SetDlgItemInt().
//
//      buffer -- Buffer to format the integer.
//      value  -- Value to write.
//
//      RETURN -- pointer to buffer.
//
static TCHAR *format_int (TCHAR *buffer,int value) {
  buffer[0] = 0;
  if (value) wsprintf (buffer,TEXT("%d"),value);
  return   (buffer);
}

//--------------------------------
//
//  Displays a double entry info entry.  These are like Halpern and ONeill, that have
//  two reffereces.  A tab character is autmatically inserted between the two entries.
//
//      hwnd    -- Dialog box.
//      id      -- ID to write to.
//      buffer1 -- Pointer to entry for first element.
//      buffer2 -- Pointer to entry for second element
//
static void format_two (HWND hwnd,int id,TCHAR *buffer1,TCHAR *buffer2) {
  TCHAR buffer[80];
  wsprintf       (buffer,TEXT("%s\t%s"),buffer1,buffer2);
  SetDlgItemText (hwnd,id,buffer);
  return;
}

//--------------------------------
//
//  Displays an ASCII coded string.  For CE machines, this must be converted 
//  to UNICODE before the display.  For NT/98/95 machines, this is simply 
//  a macro.
//
//      hwnd   -- Dialog box pointer.
//      id     -- Location to display the text.
//      string -- Data to display.
//
#ifdef WINCE
static void info_string (HWND hwnd,int id,byte *string) {
  TCHAR text[SIZE_BUFFER];
  int   i;
  for (i = 0; string[i]; i++) text[i] = ascii2unicode(string[i]);
  text[i] = 0;
  SetDlgItemText (hwnd,id,text);
  return;
}
#else WINCE
  #define info_string(w,i,s) SetDlgItemText(w,i,(char *) s);
#endif WINCE

//===================================================================
//
//  begin class KANJI_info
//
//  This class implements the kanji-info lookup.
//

static byte *info_cache = NULL;     // This location holds the cached kanji information. 

//--------------------------------
//
//  Close the resources
//
void KANJI_info::close_info () {
  if (handle != INVALID_HANDLE_VALUE) CloseHandle (handle);
  handle = INVALID_HANDLE_VALUE;
  return;
}

//--------------------------------
//
//  This is the main routine.  This basically is the dialog procedure
//  for the kanji-info dialog box.
//
int KANJI_info::dlg_kanjiinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  switch (message) {
//
//  This is the main part of the routine.  Since this is just an 
//  informational dialog box, the main part is in making the dialog 
//  box.
//
    case WM_INITDIALOG: 
         dialog = hwnd;
         info_size.wm_init (hwnd,IDC_KILIST,&jwp_config.cfg.size_info,false,INFO_FIRST,INFO_LAST);
         add_dialog        (hwnd,true);
         init_dialog       (hwnd);
         POCKETPC_DIALOG   (hwnd);
         return            (false);
//
//  Changing the size
//
#ifndef WINCE
    case WM_SIZING:
         info_size.wm_sizing ((RECT *) lParam);
         return              (0);
#endif  WINCE
    case WM_SIZE:
         info_size.wm_size (wParam);
         return            (0);
//
//  Dialog is being destroyed, we need to delete the class for it.
//
    case WM_DESTROY:
         if (this == single_info) single_info = NULL;
         remove_dialog (hwnd);
         delete this;
         return (true);           
//
//  User is using single mode and wants to change the kanji.
//
    case WMU_SETINFOKANJI:
         ch = (KANJI) lParam;
         InvalidateRect (hwnd,NULL,true);
         init_dialog    (hwnd);
         return         (true);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_CHARINFO);
         return  (true);
//
//  Process push buttons.
//
    case WM_COMMAND:    
         switch (LOWORD(wParam)) {
           case IDOK:
           case IDCANCEL:
                DestroyWindow (hwnd);   
                return (true);
           case IDC_KIFROMCLIP: 
                JWP_file *paste;
                if (!(paste = get_paste (hwnd))) return (true);
                ch = paste->edit_gettext()[0];
                InvalidateRect (hwnd,NULL,true);
                init_dialog    (hwnd);
                return (true);
           case IDC_KIMORE:
                JDialogBox (IDD_MOREINFO,hwnd,(DLGPROC) dialog_moreinfo,(LONG) this);
                return (true);
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           case IDC_KIXREF:
                JDialogBox (IDD_XREFINFO,hwnd,(DLGPROC) dialog_xrefinfo,(LONG) this);
                return (true);
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           case IDC_KIBIGKANJI:
                JWP_file *file;
                file = file_list.get(NULL);
                file->insert_string (&ch,1);
                return (true);
           case IDC_KILIST:
           case IDC_KIINSERT:
                SendDlgItemMessage (hwnd,IDC_KILIST,JL_INSERTTOFILE,0,0);
                return (true);
         }
         break;
  }
  return (false);
}

//--------------------------------
//
//  This is the dialog box handler for the More Kanji Info dialog.  There are
//  two copies of this routine.  One for PPC machines and one for all other 
//  routines (the gneral rotuine has some variation between CE and non-CE 
//  machines.
//
int KANJI_info::dlg_moreinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  int i;
  switch (message) {
//
//  This is the main part of the routine.  Since this is just an 
//  informational dialog box, the main part is in making the dialog 
//  box.
//
    case WM_INITDIALOG: 
         more_size.wm_init (hwnd,IDC_MIXREF,&jwp_config.cfg.size_more,false,MORE_FIRST,MORE_LAST);
#if (!(defined(WINCE_PPC) || defined(WINCE_POCKETPC)))
         format_xref       (hwnd);
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
         for (i = 0; i < MORE_MAXLINES; i++) format_line (hwnd,i,jwp_config.cfg.kanji_info[i+MORE_FIRSTINDEX]);
         POCKETPC_DIALOG (hwnd);
         return          (false);
//
//  Size message processing.
//
#ifndef WINCE
    case WM_SIZING:
         more_size.wm_sizing ((RECT *) lParam);
         return (0);
#endif  WINCE
    case WM_SIZE:
         more_size.wm_size (wParam);
         return (0);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_CHARINFO);
         return  (true);
//
//  Process push buttons.
//
    case WM_COMMAND:    
         switch (LOWORD(wParam)) {
           case IDOK:
           case IDCANCEL:
                EndDialog (hwnd,false);
                return (true);
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           case IDC_MINEXT:
                JDialogBox (IDD_XREFINFO,hwnd,(DLGPROC) dialog_xrefinfo,(LONG) this);
                return (true);
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
           case IDC_MIXREF:
           case IDC_MIINSERT:
                SendDlgItemMessage (hwnd,IDC_MIXREF,JL_INSERTTOFILE,0,0);
                return (true);
         }
         break;
  }
  return (false);
}

//--------------------------------
//
//  This is the dialog box handler for the Cross-Reference dialog.  This dialog
//  is only used on PPC machines, and only because there is not sufficient 
//  room for these items in any of the other dailog pages.
//
#if (defined(WINCE_PPC) || defined(WINCE_POCKETPC))
int KANJI_info::dlg_xrefinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  int   i;
  switch (message) {
//
//  This is the main part of the routine.  Since this is just an 
//  informational dialog box, the main part is in making the dialog 
//  box.
//
    case WM_INITDIALOG: 
         more_size.wm_init (hwnd,IDC_MIXREF,&jwp_config.cfg.size_more,false,XREF_FIRST,XREF_LAST);
         format_xref       (hwnd);
         for (i = 0; i < XREF_MAXLINES; i++) format_line (hwnd,i,jwp_config.cfg.kanji_info[i+XREF_FIRSTINDEX]);
         POCKETPC_DIALOG (hwnd);
         return          (false);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_CHARINFO);
         return  (true);
//
//  Process push buttons.
//
    case WM_COMMAND:    
         switch (LOWORD(wParam)) {
           case IDOK:
           case IDCANCEL:
                EndDialog (hwnd,false);
                return (true);
           case IDC_MIINSERT:
           case IDC_MIXREF:
                SendDlgItemMessage (hwnd,IDC_MIXREF,JL_INSERTTOFILE,0,0);
                return (true);
         }
         break;
  }
  return (false);
}
#endif (defined(WINCE_PPC) || defined(WINCE_POCKETPC))

//--------------------------------
//
//  This si the big one.  This routine generates all the line entries except
//  for bushu.
//
//      hwnd -- Pointer to dialog box.
//      line -- Line number indexed from 0 to display the information.
//      code -- Information to be displayed on that line.
//
//  Acess is based on sequential numbers for the dialog box controls.  The labels are 
//  first followed by the item.  Thus incrementing by two goes to the next item.
//
#define CHARTYPE_UNKNOWN    0       // Character types.
#define CHARTYPE_ASCII      1
#define CHARTYPE_OEM        2
#define CHARTYPE_JSYMBOL    3
#define CHARTYPE_JASCII     4
#define CHARTYPE_HIRAGANA   5
#define CHARTYPE_KATAKANA   6
#define CHARTYPE_GREEK      7
#define CHARTYPE_RUSSIAN    8
#define CHARTYPE_RESERVED   9
#define CHARTYPE_KANJI1     10
#define CHARTYPE_KANJI2     11

#define IDC_KIBASE IDC_KILABEL0     // Used as base for accessing dialog box controls.

void KANJI_info::format_line (HWND hwnd,int line,int code) {
  TCHAR      buffer2[30];               // Buffer for accumulating pin-yin data and ASCII value of JIS code
  TCHAR      buffer1[30];
  static short types[] = { IDS_KI_TYPEUNKNOWN,IDS_KI_TYPEASCII,IDS_KI_TYPEEXTENDED,IDS_KI_TYPESYMBOL,IDS_KI_TYPEJASCII,IDS_KI_TYPEHIRAGANA,IDS_KI_TYPEKATAKANA,IDS_KI_TYPEGREEK,IDS_KI_TYPERUSSIAN,IDS_KI_TYPERESERVED,IDS_KI_TYPEKANJI1,IDS_KI_TYPEKANJI2 };

  int i;
//
//  Do the label
//
  line = IDC_KIBASE+2*line;
  if (code && (code <= INFO_MAXITEM)) wsprintf (buffer1,TEXT("%s:"),get_string(info_codes[code])); else buffer1[0] = 0;  
  SetDlgItemText (hwnd,line,buffer1);
//
//  Do the item
//
  SetDlgItemText (hwnd,++line,TEXT(""));
  if ((ch < 0x3000) && (code > INFO_UNICODE)) return;
//  if (!kinfo.extra && (code >= INFO_SPAHN)) return;
  switch (code) {
    case INFO_TYPE:
         i = HIBYTE(ch);
         if       (ch == 0)                   i = CHARTYPE_UNKNOWN;
         else if  (ch <= 127)                 i = CHARTYPE_ASCII;
         else if  (ch <= 255)                 i = CHARTYPE_OEM;
         else if ((i == 0x21) || (i == 0x22)) i = CHARTYPE_JSYMBOL;
         else if  (i == 0x23)                 i = CHARTYPE_JASCII;
         else if  (i == 0x24)                 i = CHARTYPE_HIRAGANA;
         else if  (i == 0x25)                 i = CHARTYPE_KATAKANA;
         else if  (i == 0x26)                 i = CHARTYPE_GREEK;
         else if  (i == 0x27)                 i = CHARTYPE_RUSSIAN;
         else if ((i >= 0x28) && (i <= 0x2f)) i = CHARTYPE_RESERVED;
         else if ((i >= 0x30) && (i <= 0x4f)) i = CHARTYPE_KANJI1;
         else if ((i >= 0x50) && (i <= 0x74)) i = CHARTYPE_KANJI2;
         else                                 i = CHARTYPE_UNKNOWN;
         SetDlgItemText (hwnd,line,get_string(types[i]));
         break;
    case INFO_JIS:
         if (ch <= 0x00ff) { buffer2[0] = (byte) ch; buffer2[1] = 0; }
           else { buffer2[0] = HIBYTE(ch); buffer2[1] = LOBYTE(ch); buffer2[2] = 0; }
         wsprintf       (buffer1,TEXT("%X (%X) [%s]"),ch,ch | 0x8080,buffer2);
         SetDlgItemText (hwnd,line,buffer1);
         break;
    case INFO_SHIFTJIS:
         wsprintf       (buffer1,TEXT("%X"),(ch < 0x80) ? ch : jis2sjis(ch));
         SetDlgItemText (hwnd,line,buffer1);
         break;
    case INFO_UNICODE:
         wsprintf       (buffer1,TEXT("%X"),jis2unicode(ch));
         SetDlgItemText (hwnd,line,buffer1);
         break;
    case INFO_STROKE:
         SetDlgItemInt (hwnd,line,kinfo.strokes,true);
         break;
    case INFO_GRADE:
         if (kinfo.grade) SetDlgItemInt (hwnd,line,kinfo.grade,false);
         break;
    case INFO_NELSON:
         format_two (hwnd,line,format_int(buffer1,kinfo.nelson),format_int(buffer2,kinfo.haig));
         break;
    case INFO_HALPERN:
         if (kinfo.skip_t) wsprintf (buffer2,TEXT("%d-%d-%d"),kinfo.skip_t,kinfo.skip_1,kinfo.skip_2); else buffer2[0] = 0;
         format_two (hwnd,line,format_int(buffer1,kinfo.halpern),buffer2);
         break;
    case INFO_SPAHN:
         if (extend.sh_rstroke || extend.sh_ostroke) wsprintf (buffer1,TEXT("%d%c%d.%d"),extend.sh_rstroke,extend.sh_radical+'a',extend.sh_ostroke,extend.sh_index); else buffer1[0] = 0;
         format_two (hwnd,line,buffer1,format_int(buffer2,sh_kana));
         break;
    case INFO_FOURCORNERS:
         format_two (hwnd,line,format_fc(buffer1,extend.fc_main,extend.fc_index),format_fc(buffer2,fc_main2,extend.fc_index2));
         break;
    case INFO_MOROHASHI:
         if (!extend.md_long) buffer1[0] = 0;
           else {
             if      (extend.md_x) wsprintf (buffer1,TEXT("%dX"),extend.md_long);
             else if (extend.md_p) wsprintf (buffer1,TEXT("%dP"),extend.md_long);
             else                  wsprintf (buffer1,TEXT("%d") ,extend.md_long);
           }
         if (extend.md_short1) wsprintf (buffer2,TEXT("%d.%04d"),extend.md_short1,extend.md_short2); else buffer2[0] = 0;
         format_two (hwnd,line,buffer1,buffer2);
         break;
    case INFO_PINYIN:
         if (pinyin) info_string (hwnd,line,pinyin);
         break;
    case INFO_KOREAN:
         if (korean) info_string (hwnd,line,korean);
         break;
    case INFO_FREQUENCY:
         if (freq) SetDlgItemInt (hwnd,line,freq,false);
         break;
    case INFO_HENSHALL:
         if (henshall) SetDlgItemInt (hwnd,line,henshall,false);
         break;
    case INFO_GAKKEN:
         if (gakken) SetDlgItemInt (hwnd,line,gakken,false);
         break;
    case INFO_HEISIG:   
         if (heisig) SetDlgItemInt (hwnd,line,heisig,false);
         break;
    case INFO_ONEILL:
         format_two (hwnd,line,format_int(buffer1,oneill),format_int(buffer2,oneill_ek));
         break;
    case INFO_DEROO:
         if (deroo) SetDlgItemInt (hwnd,line,deroo,false);
         break;
    case INFO_KANJILEARN:
         if (halpern_kld) SetDlgItemInt (hwnd,line,halpern_kld,false);
         break;
    case INFO_READWRITE:
         if (readwrite) SetDlgItemInt (hwnd,line,readwrite,false);
         break;
    case INFO_TUTTLECARDS:
         if (tuttlecard) SetDlgItemInt (hwnd,line,tuttlecard,false);
         break;         
    case INFO_KANJIWAY:
         if (kanjiway) SetDlgItemInt (hwnd,line,kanjiway,false);
         break;
    case INFO_KANJICONTEXT:
         if (kanjicontext) SetDlgItemInt (hwnd,line,kanjicontext,false);
         break;         
    case INFO_COMPACTGUIDE:
         if (kanjiguide) SetDlgItemInt (hwnd,line,kanjiguide,false);
         break;
    case INFO_BUSYPEOPLE:
         if (busypeople) {
           format_string  (buffer1,IDS_KI_BUSYFORMAT,busypeople >> 8,busypeople & 0x00ff);
           SetDlgItemText (hwnd,line,buffer1);
         }         
         break;
    case INFO_BLANK:
    default:
         break;
  }
  return;
}

//--------------------------------
//
//  This routine generates the cross-reference table.  This is only
//  separated here to make things clearer.  This routine is only used 
//  in one place, but the place is different for the different versions of
//  JWPce, so it is easier to put this here.
//
//      hwnd -- Pointer to dialog box.
//
void KANJI_info::format_xref (HWND hwnd) {
  static short errors[] = { IDS_KI_SKIPPOSITION,IDS_KI_SKIPSTROKE,IDS_KI_SKIPBOTH,IDS_KI_SKIPBREEN };
  int        i;
  byte      *ptr;
  TCHAR      buffer[SIZE_BUFFER];
  EUC_buffer line;                                  // EUC line buffer used to set strings in the list-box.
  if (kinfo.extra) {;
    line.initialize (GetDlgItem(hwnd,IDC_MIXREF));    // Initialize buffer for output.
    for (ptr = xref; xref && *ptr; ptr += 3) {
      i = (((ushort) ptr[2]) << 8) | ((ushort) ptr[1]);
      switch (*ptr) {
        case 'n':
             format_string (buffer,IDS_MI_NELSON,i);
             break;
        case 'h':
             format_string (buffer,IDS_MI_HALPERN,i);
             break;
        case 'o':
             format_string (buffer,IDS_MI_ONEILL,i);
             break;
        case 'k':
             format_string (buffer,IDS_MI_JIS0208,i);
             break;
        case 'j':
             format_string (buffer,IDS_MI_JIS0212,i);
             break;
        case 'z':
             format_string (buffer,IDS_MI_SKIP,((i >> 10) & 0x7),((i >> 5) & 0x1f),(i & 0x1f),get_string(errors[(i >> 13)-1]));
             break;
        case 'd':
             format_string (buffer,IDS_MI_DEROO,i);
             break;
      }
#ifdef WINCE
      for (i = 0; buffer[i]; i++) ((byte *) buffer)[i] = (char) buffer[i]; 
      ((byte *) buffer)[i] = 0;
#endif WINCE        
      put_line (&line,0,(byte *) buffer,false);
    }
  }
  SetFocus (GetDlgItem(hwnd,IDC_MIXREF));
  return;
}

//--------------------------------
//
//  Get the kanji-info for a particular kanji.
//
//      ch     -- Character to get info for.
//      amount -- Amount of information to get:
//
//              INFO_FIXED   -- Just the fixed inforamtion.
//              INFO_STRINGS -- Fixed information and readings/meanings
//              INFO_EXTEND  -- Above and extended inforamtion that is fixed size.
//              INFO_ALL     -- Get all information
//
void KANJI_info::get_info (int ch,int amount) {
  unsigned long done;
  byte         *ptr;
  int           i;
//
//  Get fixed data elements.
//
  if (ch > last_jis) {                          // User asked for an invalid character
    memset (&kinfo,0,sizeof(kinfo));
    return;
  }
  ch = (ch & 0x7f7f)-0x3021;
  ch = HIBYTE(ch)*94+LOBYTE(ch);
  if (info_cache) memcpy (&kinfo,info_cache+KANJIINFO_OFFSET+ch*sizeof(kinfo),sizeof(kinfo));
    else {
      SetFilePointer (handle,KANJIINFO_OFFSET+ch*sizeof(kinfo),NULL,FILE_BEGIN);
      ReadFile (handle,&kinfo,sizeof(kinfo),&done,NULL);
    }
  if (amount == INFO_FIXED) return;
//
//  Get the basic data sets, meanings, etc.
//
  if (info_cache) ptr = info_cache+kinfo.offset;
    else {
      SetFilePointer (handle,kinfo.offset,NULL,FILE_BEGIN);
      ReadFile (handle,buffer,SIZE_INFOBUFFER,&done,NULL);
      ptr = buffer;
    }
  if (!kinfo.korean) korean = NULL; else ptr = skip_line(korean = ptr);
  if (!kinfo.pinyin) pinyin = NULL; else ptr = skip_line(pinyin = ptr);
  for (imi = ptr, i = 0; i < kinfo.imi; i++) ptr = skip_line(ptr);
  for (on  = ptr, i = 0; i < kinfo.on ; i++) ptr = skip_line(ptr);
  for (kun = ptr, i = 0; i < kinfo.kun; i++) ptr = skip_line(ptr);
  nan = ptr;
  if (amount == INFO_STRINGS) return;
//
//  Process the extended data.
//
  if (!kinfo.extra) {
    memset (&extend,0,sizeof(struct extend));
    extend.fc_main = ~0UL;                                          // -1 in a portable way
  }
  else {
    for (i = 0; i < kinfo.nan; i++) ptr = skip_line(ptr);           // Skip the nanori
    memcpy (&extend,ptr,sizeof(struct extend));                     // Fixed part of the extended data.
    if (amount == INFO_EXTEND) return;
    freq = sh_kana = henshall = gakken = heisig = oneill = deroo = halpern_kld = oneill_ek = 0;     // Zero out the data fields.
    busypeople = kanjiway = kanjiguide = kanjicontext = readwrite = tuttlecard = 0;
    fc_main2 = -1;
    xref     = NULL;
    for (ptr += sizeof(struct extend); *ptr; ptr += 3) {            // Process the data list.
      if (islower(*ptr)) {                                          // Lowercase entry indicates start
        xref = ptr;                                                 //   of the cross-reference entries
        break;                                                      //   processing of these is delayed
      }                                                             //   until later.
      i = (((ushort) ptr[2]) << 8) | ptr[1];                        // Get interger parameter.
      switch (*ptr) {                                               // Common entries 
        case 'B': busypeople   = i; break;      // "Japanese For Busy People" vols I-III, published by the AJLT. The codes are the volume.chapter.
        case 'C': kanjiway     = i; break;      // "The Kanji Way to Japanese Language Power" by Dale Crowley.
        case 'D': deroo        = i; break;      // Father Joseph De Roo, and published in his book "2001 Kanji" (Bojinsha). 
        case 'E': henshall     = i; break;
        case 'F': freq         = i; break;      // Frequency of use
        case 'G': kanjiguide   = i; break;      // "Kodansha Compact Kanji Guide".
        case 'H': halpern_kld  = i; break;      // Jack Halpern in his Kanji Learners Dictionary, published by Kodansha in 1999. The numbers have been provided by Mr Halpern.
        case 'I': sh_kana      = i; break;
        case 'J': kanjicontext = i; break;      // "Kanji in Context" by Nishiguchi and Kono.
        case 'K': gakken       = i; break;      // Gakken Kanji Dictionary ("A New Dictionary of Kanji Usage"). 
        case 'L': heisig       = i; break;      // "Remembering The Kanji" by James Heisig.
        case 'N': oneill_ek    = i; break;      // P.G. O'Neill's Essential Kanji (ISBN 0-8348-0222-8).
        case 'O': oneill       = i; break;      // "Japanese Names", by P.G. O'Neill. (Weatherhill, 1972)
        case 'Q': fc_main2     = i; break;      // "Four Corner" (second code).
        case 'S': readwrite    = i; break;      // "A Guide To Reading and Writing Japanese" edited by Florence Sakade.
        case 'T': tuttlecard   = i; break;      // Tuttle Kanji Cards, compiled by Alexander Kask.
        default:
             break;
      }
    }
  }
  return;
}

//--------------------------------
//
//  This routine simply gets the stroke count for a particular kanji.
//  This information is used by a number of other routines (especially
//  the kanji lookup routines), and thus it makes sense to have a special
//  routine.
//
//      ch -- Character to get the stroke count for (JIS)
//
//      RETURN -- Stroke count (zero is returned if the character is not valid).
//
int KANJI_info::get_stroke (int ch) {
  get_info (ch,INFO_FIXED);
  return (kinfo.strokes);
}

//--------------------------------
//
//  This is the handler for the WM_INITDIALOG message for the main Kanji Info 
//  dialog.  This has been seperatred out from the dialog handler to allow for
//  easier addaptation between the different platforms.  
//
//      hwnd -- Pointer to dialog window.
//
void KANJI_info::init_dialog (HWND hwnd) {
  EUC_buffer line;                      // EUC line buffer used to set strings in the list-box.
  TCHAR      buffer2[20];               // Buffer for accumulating pin-yin data and ASCII value of JIS code
  byte      *ptr;                       // Pointer to current location within the data buffer.
  int        i;

  if (ch > 0x00ff) ch &= 0x7f7f;        // We accept EUC characters.
//
//  Blank out all old data.
//
  SetDlgItemText     (hwnd,IDC_KIBUSHU        ,TEXT(""));
  SendDlgItemMessage (hwnd,IDC_KILIST         ,JL_RESET          ,0,0);
  SendDlgItemMessage (hwnd,IDC_KIBIGKANJI     ,WMU_SETWINDOWVALUE,0,ch);
  SendDlgItemMessage (hwnd,IDC_KIBUSHUCHAR    ,WMU_SETWINDOWVALUE,0,0);
  SendDlgItemMessage (hwnd,IDC_KICLASSICALCHAR,WMU_SETWINDOWVALUE,0,0);
//
//  Check for valid character
//
  EnableWindow(GetDlgItem(hwnd,IDC_KIMORE),ch >= 0x3000);
  memset (&kinfo,0,sizeof(kinfo));
//
//  Display romaji for kana
//  
  if (ISKANA(ch) && ((ch & 0xff) >= 0x21) && ((ch & 0xff) <= 0x76)) {
    static byte special[3][4] = { "vu","+ka","+ke" };
    line.initialize (GetDlgItem(hwnd,IDC_KILIST));
    for (i = 0; compound_kana[i].kana[0]; i++) {
      if (!compound_kana[i].kana[1] && ((ch & 0xff) == compound_kana[i].kana[0])) put_reading (&line,0,(byte *) compound_kana[i].string,0,1);
    }
    if ((ch & 0xff) <= 0x73) put_reading (&line,0,(byte *) direct_kana[(ch & 0xff)-0x21],0,1); else put_reading (&line,0,special[(ch & 0xff)-0x74],0,1);
  }
//
//  Deal with kanji.
//  
  else if (ISKANJI(ch) && ((ch & 0xff) >= 0x21) && ((ch & 0xff) <= 0x7e)) {
//
//  Open and read kanji information, an display the easy items.
//        
    if (open_info(hwnd)) return;
    get_info (ch,INFO_ALL);
    close_info ();
    SendDlgItemMessage (hwnd,IDC_KIBUSHUCHAR,WMU_SETWINDOWVALUE,0,kinfo.bushu);
    if (!kinfo.classical) SetDlgItemInt (hwnd,IDC_KIBUSHU,kinfo.bushu,true);
      else {
        wsprintf (buffer2,TEXT("%d (%d)"),kinfo.bushu,kinfo.classical);
        SetDlgItemText     (hwnd,IDC_KIBUSHU,buffer2);
        SendDlgItemMessage (hwnd,IDC_KICLASSICALCHAR,WMU_SETWINDOWVALUE,0,kinfo.classical);
      }       
//
//  Read all of the line type entries all in one big block.  This is 
//  very brut-force, but it does save time/space, and everything else.
//
    line.initialize (GetDlgItem(hwnd,IDC_KILIST));    // Initialize buffer for output.
//
//  Process list elements.
//
    if (jwp_config.cfg.info_titles)              line.put_label  (IDS_KI_LISTMEANINGS);
    for (ptr = imi, i = 0; i < kinfo.imi; i++)  ptr = put_reading(&line,0            ,ptr,i,kinfo.imi);
    if (jwp_config.cfg.info_titles && kinfo.on ) line.put_label  (IDS_KI_LISTONYOMI);
    for (ptr = on,  i = 0; i < kinfo.on; i++)   ptr = put_reading(&line,BASE_KATAKANA,ptr,i,kinfo.on);
    if (jwp_config.cfg.info_titles && kinfo.kun) line.put_label  (IDS_KI_LISTKUNYOMI);
    for (ptr = kun, i = 0; i < kinfo.kun; i++)  ptr = put_reading(&line,BASE_HIRAGANA,ptr,i,kinfo.kun);
    if (jwp_config.cfg.info_titles && kinfo.nan) line.put_label  (IDS_KI_LISTNANORI);
    for (ptr = nan, i = 0; i < kinfo.nan; i++)  ptr = put_reading(&line,BASE_HIRAGANA,ptr,i,kinfo.nan);
//
//  Activate the list so the user can use the cursor keys to move 
//  through the list.  ESC and or ENTER will still exit the dialog.
//
    SetFocus (GetDlgItem(hwnd,IDC_KILIST));
  }
//
//  Render the info items.
//
  for (i = 0; i <= INFO_MAXLINES; i++) format_line (hwnd,i,jwp_config.cfg.kanji_info[i+INFO_FIRSTINDEX]);
  return;
}

//--------------------------------
//
//  Open the kanji-info file.
//
//  The file is open, and a number of spot checks are preformed to make
//  sure that this is the correct version of the file.
//
//      hwnd -- Window of the caller (used to generate error messages).
//              Passing a value of NULL will diswable the messages.
//
//      RETURN -- Non-zero return indicates an error in opening the file.
//  
struct info_header {
  long  magic;
  long  flags;
  short count;
  short last_jis;
};

#define INFO_CACHE  ((struct info_header *) info_cache)

int KANJI_info::open_info (HWND hwnd) {
  unsigned long done,magic,size;
//
//  If we have a cached information file we do not need to open it.
//
  if (info_cache) {
    magic                  = INFO_CACHE->magic;
    jwp_config.kanji_flags = INFO_CACHE->flags;
    count                  = INFO_CACHE->count;
    last_jis               = INFO_CACHE->last_jis;
    return (false);
  }
//
//  Open the file
//
  handle = jwp_config.open(NAME_KANJIINFO,OPEN_READ,false); // Open file and check.
  if (INVALID_HANDLE_VALUE == handle) {
    if (hwnd) {
      JMessageBox (hwnd,IDS_KI_ERROROPEN,IDS_KI_ERRORTITLE,MB_OK | MB_ICONEXCLAMATION,jwp_config.name());
    }
    return (true);
  }
//
//  If we are going to cache the file then read the file.
//
  if (jwp_config.cfg.cache_info && hwnd) {  // hwnd check is to revent load on intialization.
    size = GetFileSize(handle,&done);
    if (!(info_cache = (byte *) malloc(size))) {
      OutOfMemory (hwnd);
      close_info  ();
      return      (true);
    }
    ReadFile (handle,info_cache,size,&done,NULL);
    magic                  = INFO_CACHE->magic;
    jwp_config.kanji_flags = INFO_CACHE->flags;
    count                  = INFO_CACHE->count;
    last_jis               = INFO_CACHE->last_jis;
  }
//
//  Not cached then check the file information.
//
  else {
    ReadFile (handle,&magic   ,sizeof(long ),&done,NULL);     // Check for database id (magic)
    ReadFile (handle,&jwp_config.kanji_flags,sizeof(long),&done,NULL);
    ReadFile (handle,&count   ,sizeof(short),&done,NULL);     // Get the number of kanji in database (NOT USED AT THIS TIME)
    ReadFile (handle,&last_jis,sizeof(short),&done,NULL);     // Last kanji in the database. (NOT USED AT THIS TIME)
  }
//
//  Check the magic inforamtion.
//
  if (magic != KINFO_MAGIC) {
    close_info ();
    handle = INVALID_HANDLE_VALUE;
    if (hwnd) {
      JMessageBox (hwnd,IDS_KI_ERRORVERSION,IDS_KI_ERRORTITLE,MB_OK | MB_ICONEXCLAMATION,jwp_config.name());
    }
    return (true);
  }
  return (false);
}

//
//  End Class KANJI_info.
//
//===================================================================

//===================================================================
//
//  Configure Character Information dialog box.
//

//--------------------------------
//
//  dialog box handler to configure the character information dialog.
//

//
//  Macros to make things clearer.
//
#define GETSEL(x)    ((int) SendDlgItemMessage(hwnd,IDC_ICITEM0+(x),CB_GETCURSEL,0,0))      // Get CB select
#define SETSEL(x,y)  SendDlgItemMessage (hwnd,IDC_ICITEM0+(x),CB_SETCURSEL,y,0)             // Set CB select

int dialog_infoconfig (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  int i,j,k;
  switch (message) {
    case WM_INITDIALOG:
         for (i = 0; i < INFO_MAXITEM; i++) {
           for (j = 0; j <= INFO_MAXITEM; j++) SendDlgItemMessage (hwnd,IDC_ICITEM0+i,CB_ADDSTRING,0,(LPARAM) get_string(info_codes[j]));
           SETSEL (i,cfg->kanji_info[i]);
         }
         POCKETPC_DIALOG (hwnd);
         return          (true);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_CONFIGINFO);
         return  (true);
//
//  Process push buttons.
//
    case WM_COMMAND:    
         switch (LOWORD(wParam)) {
//
//  This handles changes to the list.  If the user changes an item to blank we don't have to do anything.  If the user, however
//  changes an item to anything else then we have some checking to do.  If the user has created a duplicate item then we need to 
//  fill that item in with one of the missing items.
//
//  First step is to find a missing item.  We do this by looking for items 1..INFO_MAXITEM in the list boxes.  If we cannot find 
//  a missing item then we are done.  Note we do not check for zero becasue we allow any number of these.
//
//  Second step is to check the item that got set.  If this got set to blank then we don't have to do anything.  Actually this 
//  should get picked up in the first test.  This was done second only to save a variable.
//
//  Third, we search to find a duplicated item.  If there is no duplicated item then we don't have anything to do either.
//
//  Finally we replace the duplicated item with the missing item.
//          
           default: 
                if (HIWORD(wParam) != CBN_SELCHANGE) return (0);
                wParam = LOWORD(wParam);
//
//  Find a missing item
//
                for (i = 1; i <= INFO_MAXITEM; i++) {
                  for (j = 0; (j < INFO_MAXITEM) && (i != GETSEL(j)); j++); 
                  if (j == INFO_MAXITEM) break;
                }
                if (i > INFO_MAXITEM) return (0);                      // No missing items so done.
//
//  Get the item that got set.  There should be two of these.
//
                j = SendDlgItemMessage(hwnd,wParam,CB_GETCURSEL,0,0);
                if (j == INFO_BLANK) return (0);                        // Setting to blank so don't do anything.
//
//  Find second item, and correct.
//
                for (k = 0; (k < INFO_MAXITEM) && ((((int) wParam) == IDC_ICITEM0+k) || (j != GETSEL(k))); k++);
                if (k != INFO_MAXITEM) SETSEL (k,i);                    // Only maka change if there is a duplicate item.
                return (0);
           case IDC_ICDEFAULT:
                for (i = 0; i < INFO_MAXITEM; i++) SETSEL(i,default_config.kanji_info[i]);
                return (0);
           case IDOK:
                for (i = 0; i < INFO_MAXITEM; i++) cfg->kanji_info[i] = GETSEL(i);
           case IDCANCEL:
                EndDialog (hwnd,false);
                return (0);
         }
         break;
  }
  return (false);
}

//===================================================================
//
//  begin count-kanji feacture.
//
//  This collection of routines and data strucrues implements the 
//  cunt-kanji feature.
//

//-------------------------------------------------------------------
//
//  static data and definitions.
//
//  These are various static data used in some of the routines.
//

typedef struct kanji_c {        // Kanji counting structure.
  KANJI kanji;                  // Kanji (JIS value)
  short count;                  // Count (frequency)
} kanji_c;

class KANJI_count {            // Struture to be passed to dialog box procedure.
public:
  int     all;                  // Count all files.
  int     hiragana;             // Number of hiragana.
  int     katakana;             // Number of katakana.
  int     ascii;                // Number of ascii.
  int     jascii;               // Number of jascii;
  int     kanji_list;           // Number of kanji on list.
  int     kanji_nolist;         // Number of kanji not on list.
  int     kanjis_list;          // Number of different kanji on list
  int     kanjis_nolist;        // Number of different kanjies not on list.
  int     other;                // Number of other characters.
  int     total;                // Total number of characters.
  HWND    dialog;               // Dialog pointer so we can find it.
  kanji_c list[SIZE_KANJI];     // Array of kanji count structures for each kanji.
  void inline clear (void) { total = kanji_list = kanji_nolist = kanjis_list = kanjis_nolist = hiragana = katakana = ascii = jascii = other = 0; }
  void count (int ch);
};

typedef class KANJI_count KANJI_count;

KANJI_count *kanji_count = NULL;    // Static structure instance that is used to 
                                    //   get data into and out of the dialog box
                                    //   procedure.

//--------------------------------
//
//  Do the actual count.
//
void KANJI_count::count (int ch) {
  int i;
  total++;
  if (ISHIRAGANA(ch)) { hiragana++; return; }
  if (ISKATAKANA(ch) || (ch == KANJI_LONGVOWEL)) { katakana++; return; }
  if (ISASCII(ch)) { ascii++; return; }
  if (ISJASCII(ch)) { jascii++; return; };
  if (ISKANJI(ch)) {
    for (i = 0; list[i].kanji && (list[i].kanji != ch); i++);
    kanji_count->list[i].kanji = ch;
    kanji_count->list[i].count++;
    return;
  }
  other++;
  return;
}

//-------------------------------------------------------------------
//
//  Static routines.
//

static void  count_block (EUC_buffer *line,int count,byte *ptr,int base);   // Format a blockfor the count-kanji dialog.
static BOOL CALLBACK dialog_kanjicount (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
static int __cdecl kanji_compare (const void *elem1, const void *elem2 );   // Stub for quick-sort routine.

//--------------------------------
//
//  Small stub routine to output a block of informaton in the count 
//  kanji dialog.  This may be information such as on-yomi, kun-yomi, 
//  etc.
//
//      line  -- EUC_buffer class object to output data through.
//      count -- Number of lines to ouutput.
//      ptr   -- Pointer to the first line (compressed data).
//      base  -- Base indicates the type of data.  Three passes are 
//               used:
//
//          BASE_KATAKANA -- Indicates data is katakana (on-yomi).
//          BASE_HIRAGANA -- Indicates data is hiragana (kun-yomi).
//          0             -- Ascii data (imi).
//
static void count_block (EUC_buffer *line,int count,byte *ptr,int base) {
  int i;
  if (!count) return;
  line->put_char (KANJI_SPACE);
  for (i = 0; i < count; i++) {
    if (i) {
      if (!base) line->put_string (TEXT(", "));
        else line->put_char (COUNT_SEPARATOR);
    }
    ptr = put_line(line,base,ptr,true);
  }
  return;
}

//--------------------------------
//
//  This is the dialog box handler for the count-kanji dialog box.  
//  For the count-kanji function, this is the big one.
//
//      IDC_CKALL           All checkbox.
//      IDC_CKEXCLUDE       Exclude kanji-list.
//      IDC_CKINCLUDE       Only include kanji-list.
//      IDC_CKFREQUENCY     Output frequency
//      IDC_CKONYOMI        Output on-yomi
//      IDC_CKKUNYOMI       Output kun-yomi
//      IDC_CKMEANING       Output meaning
//      IDC_CKINFO          Get Info button.
//      IDC_CKINSERT        Insert into file button.
//      IDC_CKLIST          The actual list.
//      IDC_CKNUMBER        Number in the list.
//
static BOOL CALLBACK dialog_kanjicount (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  switch (message) {
//
//  Simple dialog setup.
//
    case WM_INITDIALOG: 
         int i;
         count_size.wm_init (hwnd,IDC_CKLIST,&jwp_config.cfg.size_oount,false,0,0);
         add_dialog (kanji_count->dialog = hwnd,true);
         SetDlgItemText (hwnd,IDC_CKNUMBER,TEXT(""));
         for (i = IDC_CKFREQUENCY; i <= IDC_CKMEANING; i++) CheckDlgButton (hwnd,i,true);
         POCKETPC_DIALOG (hwnd);
         return          (true);
//
//  Shut down routine
//
    case WM_DESTROY:
         remove_dialog (hwnd);
         delete kanji_count;
         kanji_count = NULL;
         return (true);
//
//  Sizing messages
//
#ifndef WINCE
    case WM_SIZING:
         count_size.wm_sizing ((RECT *) lParam);
         return (0);
#endif  WINCE
    case WM_SIZE:
         count_size.wm_size (wParam);
         return (0);
    case WM_MOVE:
         count_size.wm_move ();
         return (0);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_KANJI_COUNTKANJI);
         return  (true);
    case WM_COMMAND:    
         switch (LOWORD(wParam)) {
//
//  Mutually exclusive checkboxes.
//
           case IDC_CKEXCLUDE:
                if (IsDlgButtonChecked(hwnd,IDC_CKEXCLUDE)) CheckDlgButton (hwnd,IDC_CKINCLUDE,false);
                return (true);
           case IDC_CKINCLUDE:
                if (IsDlgButtonChecked(hwnd,IDC_CKINCLUDE)) CheckDlgButton (hwnd,IDC_CKEXCLUDE,false);
                return (true);
//
//  Standard insert into file.
//
           case IDC_CKLIST:
           case IDC_CKINSERT:
                SendDlgItemMessage (hwnd,IDC_CKLIST,JL_INSERTTOFILE,0,(LPARAM) jwp_file);
                return (true);
//
//  Get information on one of the kanji.
//
           case IDC_CKINFO: {
                  KANJI *kanji;
                  int i;        
                  i = JL_GetBegin(hwnd,IDC_CKLIST,&kanji);
                  if (!i) MessageBeep (MB_ICONASTERISK); else kanji_info (hwnd,*kanji);
                }
                return (true);
//
//  This is the real work.  Count!
//
#ifndef WINCE_POCKETPC
           case IDOK:
#endif  WINCE_POCKETPC                
           case IDSEARCH: {
                  KANJI_info kanji_info;        // Used to get character infomration (readings, etc).
                  EUC_buffer line;              // Used to put data into the list box.
                  int        frequency,onyomi;  // List catagories.
                  int        kunyomi,meaning;
                  TCHAR      text[20];          // Formating buffer.
                  int        i;
//
//  Setup and count.
//
                  SendDlgItemMessage (hwnd,IDC_CKLIST,JL_RESET,0,0);
                  memset (kanji_count,0,sizeof(KANJI_count));
                  kanji_count->clear ();
                  kanji_count->all = IsDlgButtonChecked(hwnd,IDC_CKALL);
                  jwp_file->kanjicount ();
//
//  Generate the summory stats
//
                  for (i = 0; i < SIZE_KANJI; i++) {
                    if (!kanji_count->list[i].count) continue;
                    if (color_kanji.in(kanji_count->list[i].kanji)) {
                      kanji_count->kanji_list += kanji_count->list[i].count;
                      kanji_count->kanjis_list++;
                    }
                    else {
                      kanji_count->kanji_nolist += kanji_count->list[i].count; 
                      kanji_count->kanjis_nolist++;
                    }
                  }
//
//  Exclude kanji on list.
//
                  if (IsDlgButtonChecked(hwnd,IDC_CKEXCLUDE)) {
                    for (i = 0; i < SIZE_KANJI; i++) {
                      if (color_kanji.in(kanji_count->list[i].kanji)) kanji_count->list[i].count = 0; 
                    }
                  }
//
//  Include kanji only on list.
//
                  if (IsDlgButtonChecked(hwnd,IDC_CKINCLUDE)) {
                    for (i = 0; i < SIZE_KANJI; i++) {
                      if (!color_kanji.in(kanji_count->list[i].kanji)) kanji_count->list[i].count = 0;
                    }
                  }
//
//  Sort the list and setup totals display.
//
                  qsort (kanji_count->list,SIZE_KANJI,sizeof(kanji_c),kanji_compare);
                  for (i = 0; (i < SIZE_KANJI) && kanji_count->list[i].count; i++);
                  SetDlgItemInt (hwnd,IDC_CKNUMBER,i,false);
                  if (i) SetFocus (GetDlgItem(hwnd,IDC_CKLIST));  // Actiave list.
//
//  Setup display.
//
                  frequency = IsDlgButtonChecked(hwnd,IDC_CKFREQUENCY);
                  onyomi    = IsDlgButtonChecked(hwnd,IDC_CKONYOMI   );
                  kunyomi   = IsDlgButtonChecked(hwnd,IDC_CKKUNYOMI  );
                  meaning   = IsDlgButtonChecked(hwnd,IDC_CKMEANING  );
                  line.initialize (GetDlgItem(hwnd,IDC_CKLIST));  // Initialize buffer for output.
//
//  Process each kanji.
//
                  kanji_info.open_info (hwnd);
                  for (i = 0; i < SIZE_KANJI; i++) {
                    if (!kanji_count->list[i].count) break;     // First kanji with zero count is done.
                    kanji_info.get_info (kanji_count->list[i].kanji,INFO_STRINGS);
                    line.clear ();                              // Start output.
                    line.put_char (kanji_count->list[i].kanji); // Put kanji.
                    if (frequency) {                            // Put frequncy
                      line.put_char (KANJI_SPACE);
                      wsprintf (text,TEXT("%d"),kanji_count->list[i].count);
                      line.put_string (text);
                    }                                           // Put readings/meaning.
                    if (onyomi ) count_block (&line,kanji_info.kinfo.on ,kanji_info.on ,BASE_KATAKANA);
                    if (kunyomi) count_block (&line,kanji_info.kinfo.kun,kanji_info.kun,BASE_HIRAGANA);
                    if (meaning) count_block (&line,kanji_info.kinfo.imi,kanji_info.imi,0            );
                    line.flush (-1);
                  }
                  kanji_info.close_info ();
                }
//
//  Display the summary information
//
                JMessageBox (hwnd,IDS_CK_SUMMARY,IDS_CK_SUMMARYTITLE,MB_OK,
                                  kanji_count->total,
                                  kanji_count->kanji_list+kanji_count->kanji_nolist,kanji_count->kanji_list,kanji_count->kanji_nolist,
                                  kanji_count->kanjis_list+kanji_count->kanjis_nolist,kanji_count->kanjis_list,kanji_count->kanjis_nolist,
                                  kanji_count->hiragana+kanji_count->katakana,kanji_count->hiragana,kanji_count->katakana,
                                  kanji_count->ascii+kanji_count->jascii,kanji_count->ascii,kanji_count->jascii,
                                  kanji_count->other);
                return (true);
//
//  Standard exit
//
#ifdef WINCE_POCKETPC
           case IDOK:
#endif WINCE_POCKETPC                
           case IDCANCEL:
                DestroyWindow (hwnd);
                return (true);
         }
         break;
  }
  return (false);
}

//--------------------------------
//
//  Stub rotuine used to compare two kanji count values.  This is called
//  indirectly through the qsort routine.
//
//  Condition is backward so that the list is orderd by most common.
//
static int __cdecl kanji_compare (const void *elem1, const void *elem2 ) {
  return (((kanji_c *) elem2)->count-((kanji_c *) elem1)->count);
}

//-------------------------------------------------------------------
//
//  Class JWP_file.
//

//--------------------------------
//
//  This is a service routine called by the count-kanji dialog box to
//  do the actual counting.  This is done in a class JWP_file routine 
//  to get clear access to the file structures without class violations.
//
void JWP_file::kanjicount () {
  JWP_file *file;
  Paragraph *para;
  int i;
  file = this;
  do {
    for (para = file->first; para; para = para->next) {
      for (i = 0; i < para->length; i++) kanji_count->count (para->text[i]);
    }
    if (!kanji_count->all) return;      // If not counting all files, then exit.
    file = file->next;                  // Move to next file.
  } while (file != this);
  return;
}

//
//  End Count-kanji feature.
//
//===================================================================

//===================================================================
//
//  Exported routines.
//

//--------------------------------
//
//  This is a stub calling rotuine that does the kanji-count feature.
//  Prinarally, this routine invokes the count-kanji dialog box after
//  allocating the global strucrue necessary.
//
void do_kanjicount () {
  if (kanji_count) {                            // If dialog is open simply switch to it.
    SetForegroundWindow (kanji_count->dialog); 
    return; 
  }                                             // Otherwise make a new dialog
  if (!(kanji_count = new KANJI_count)) { OutOfMemory (main_window); return; }
  JCreateDialog (IDD_KANJICOUNT,main_window,(DLGPROC) dialog_kanjicount);
  return;
}

//--------------------------------
//
//  Deallocate memory resource associated with kanji information.
//
void free_info () {
  if (info_cache) free (info_cache);
  info_cache = NULL;
  return;
}

//--------------------------------
//
//  Entry point for changing the character info configuration.
//
//      hwnd -- Parent window.
//
void info_config (HWND hwnd) {
  JDialogBox (IDD_INFOCONFIG,hwnd,(DLGPROC) dialog_infoconfig,0);
  return;
}

//--------------------------------
//
//  This is the main entry point for the client.  This routine is called
//  with the character to get the information from.
//
//      hwnd  -- Parent window pointer.  This is necessary, because 
//               we may be called from a dialog box which is different 
//               than the root file, thus we cannot use the window 
//               field in the from JWP_file class.
//      kanji -- Character to find info for.
//
void kanji_info (HWND hwnd,int kanji) {
  if (jwp_config.cfg.info_onlyone && single_info) {
    SendMessage (single_info->dialog,WMU_SETINFOKANJI,0,kanji);
  }
  else {
    KANJI_info *info;
    if (!(info = new KANJI_info)) { OutOfMemory (hwnd); return; }
    info->ch = kanji;
    JCreateDialog (IDD_KANJIINFO,hwnd,(DLGPROC) dialog_kanjiinfo,(LONG) info);
    if (jwp_config.cfg.info_onlyone) single_info = info;
  }
  return;
}


//%%% The 4-corners, radicial lists (horizontal), HS-list, and the square radicals list are using fixed color systems still.





