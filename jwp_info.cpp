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
//  Structure of the KINFO files:
//
//  The following indicates the structure of the files written by this
//  utility.  The actual infirmaton content of the files can differ, but
//  the structure is identical.
//
//      MAGIC    -- 4 byte binary number used to verify the file type.
//      FLAGS    -- Flags indicating what is written into the file:
//      COUNT    -- Number of entries written into the file.
//      MAXJIS   -- Highest JIS code included in the file.
//
//                  KIFLAG_PINYIN (0x0001) -- Has pin yin data.
//                  KIFLAG_KOREAN (0x0002) -- Has Korean data.
//                  KIFLAG_NANORI (0x0004) -- Has nanori data.
//                  KIFLAG_EXTRA  (0x0008) -- Has extended data filed
//                  KIFLAG_EVAR   (0x0010) -- Has extended variable data
//                  KIFLAG_XREF   (0x0020) -- Has cross refference data
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

//-------------------------------------------------------------------
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

//-------------------------------------------------------------------
//
//  static data and definitions.
//
//  These are various static data used in some of the routines.
//

//static KANJI_info *kanji_info_ptr;  // Static class pointer used for the dialog
                                    //   this allows the class generate to be 
                                    //   passed to a dialog box.

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

//-------------------------------------------------------------------
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

//
//  Stub routine for the xref-info dialog box.
//
//  This routine has to save the parameter passed during the intialization.
//  This parameter is the pointer to the KANJI_info object associated
//  with the dialog.  The routine then always recalls the saved 
//  parameter and uses it to call the object's dialog box procedure.
//
#ifdef WINCE_PPC
static BOOL CALLBACK dialog_xrefinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  KANJI_info *info;
  if (message == WM_INITDIALOG) SetWindowLong (hwnd,GWL_USERDATA,lParam);
  info = (KANJI_info *) GetWindowLong(hwnd,GWL_USERDATA);
  return (info->dlg_xrefinfo(hwnd,message,wParam,lParam));
}
#endif WINCE_PPC

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
  while (*ptr) {
    if (*ptr == 0x1f) line->put_char (KANJI_DASH);
      else {
        if (*ptr & 0x80) { line->put_char(PARAN_LEFT); okurigana = true; }
        line->put_char (base | (*ptr & 0x7F));
      }
    ptr++;
  }
  if (okurigana) line->put_char (PARAN_RIGHT);
  if (!add) line->flush (-1);
  return (ptr+1);
}

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

//-------------------------------------------------------------------
//
//  Window procedures
//
//  These are specilized windows procedures used in the Kanji Info 
//  dialog box.  These replace windows static displays that were previously 
//  used in JWPce, and were changed to allow support for Windows CE.
//
//

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
         MoveWindow (hwnd,create->x,create->y,jwp_font.hwidth,jwp_font.height,true);
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
         if ((bushu >= 1) && (bushu <= 214)) {  
           kanji->draw (hdc,bushu_symbols[bushu-1],0,kanji->height);
         }
         EndPaint (hwnd,&ps);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

//
//  Dialog box procedure for the display window that pops up so you can
//  get a better look at the kanji in the PPC version.  This dialog box
//  should be created with a call to DialogBoxParam, so the kanji to 
//  be displayed in the dialog can be passed to the program!
//
#ifdef WINCE_PPC
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
#endif WINCE_PPC

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
  SIZE          size;       // Used to get the size of the ascii characters.
  TCHAR         text[2];    // Temp buffer to display ascii characters.
  int           kanji;      // The character

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
#ifdef WINCE_PPC
    case WM_LBUTTONDOWN:
         kanji = GetWindowLong(hwnd,0);
         if (kanji && !kanji_view) JDialogBox (IDD_KANJIVIEW,hwnd,(DLGPROC) dialog_kanjiview,kanji);
         return (0);
#endif WINCE_PPC
//
//  This does the actual redraw.
//
    case WM_PAINT:
         hdc   = BeginPaint (hwnd,&ps);
         kanji = GetWindowLong(hwnd,0);
         GetClientRect (hwnd,&full);
         rect.right  = full.right -jwp_font.hspace;
         rect.left   = full.left  +jwp_font.hspace;
         rect.top    = full.top   +jwp_font.vspace;
         rect.bottom = full.bottom-jwp_font.vspace;
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
             lstrcpy (lf.lfFaceName,jwp_config.cfg.font);
             lf.lfHeight = rect.top-rect.bottom;
             if (!(font = CreateFontIndirect(&lf))) return (true);
             font = (HFONT) SelectObject (hdc,font);
             text[0] = (TCHAR) kanji;
             text[1] = 0;
             SetBkMode (hdc,TRANSPARENT);
             GetTextExtentPoint32 (hdc,text,1,&size);
#ifdef PROCESS_WIDE_CHARACTERS
             if (size.cx > rect.right-rect.left) {    // Character is too wide.
               SelectObject (hdc,font);
               DeleteObject (font);
               lf.lfHeight = ((rect.top-rect.bottom)*(rect.right-rect.left))/size.cx;
               if (!(font = CreateFontIndirect (&lf))) return (true);
               font = (HFONT) SelectObject (hdc,font);
               GetTextExtentPoint32 (hdc,text,1,&size);
             }
#endif PROCESS_WIDE_CHARACTERS
             TextOut (hdc,rect.left+(rect.right-rect.left-size.cx)/2,rect.top,text,1);
             SelectObject (hdc,font);
             DeleteObject (font);
           }
         }
         EndPaint (hwnd,&ps);
         return (0);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}

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
    wclass->style         = CS_HREDRAW | CS_VREDRAW;    // Bushu character window
    wclass->hbrBackground = (HBRUSH) (COLOR_MENU+1);
    wclass->lpfnWndProc   = JWP_bushu_proc;
    wclass->lpszClassName = TEXT("JWP-Bushu");
    if (!RegisterClass(wclass)) return (true);
    wclass->hbrBackground = (HBRUSH) (COLOR_WINDOW+1);  // Large kanji window.
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

//-------------------------------------------------------------------
//
//  Small service rotuines used to display a particular kind of kanji 
//  information data.
//

//
//  Displays four-corner data, and well as checking to see if the data
//  needs to be displayed. 
//
//      hwnd  -- Dialog box pointer.
//      id    -- ID for location to display.
//      main  -- Main part of the data (set to -1 to indicate non-data).
//      index -- Last digiti that resolveds the differences between diffrerent 
//               entrys with the same main value.
//
static void info_fourcorner (HWND hwnd,int id,int main,int index) {
  TCHAR buffer[40];
  if (main == -1) return;
  wsprintf (buffer,TEXT("%04d.%d"),main,index);
  SetDlgItemText (hwnd,id,buffer);
  return;
}

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
  MultiByteToWideChar (CP_ACP,0,(char *) string,-1,text,SIZE_BUFFER);
  SetDlgItemText      (hwnd,id,text);
  return;
}
#else WINCE
  #define info_string(w,i,s) SetDlgItemText(w,i,(char *) s);
#endif WINCE

//-------------------------------------------------------------------
//
//  begin class KANJI_info
//
//  This class implements the kanji-info lookup.
//

static byte *info_cache = NULL;     // This location holds the cached kanji information. 

//
//  Close the resources
//
void KANJI_info::close_info () {
  if (handle != INVALID_HANDLE_VALUE) CloseHandle (handle);
  handle = INVALID_HANDLE_VALUE;
  return;
}

//
//  This is the main routine.  This basically is the dialog procedure
//  for the kanji-info dialog box.
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
//
//      IDC_KIINSERT      Insert button
//      IDC_KIFROMCLIP    Get character from clipboard.
//      IDC_KILIST        List box
//      IDC_KITYPE        Character type
//      IDC_KIJISCODE     JIS code
//      IDC_KISTROKES     Scroke count
//      IDC_KIBUSHU       bushu (radical) by number
//      IDC_KIGRADE       Character grade
//      IDC_KINELSON      Nelson reference number
//      IDC_KIHALPERN     halpern reference number
//      IDC_KIUNICODE     Unicode value
//      IDC_KISKIP        SKIP code (NTC dictionary)
//      IDC_KIPINYIN      PinYin translation
//      IDC_KISHIFTJIS    Shift-JIS code
//      IDC_KIBIGKANJI    Big kanji window.
//      IDC_KIBUSHUCHAR   Bushu character
//
int KANJI_info::dlg_kanjiinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  switch (message) {
//
//  This is the main part of the routine.  Since this is just an 
//  informational dialog box, the main part is in making the dialog 
//  box.
//
    case WM_INITDIALOG: 
         add_dialog  (hwnd,true);
         init_dialog (hwnd);
         return (false);
//
//  Dialog is being destroyed, we need to delete the class for it.
//
    case WM_DESTROY:
         remove_dialog (hwnd);
         delete this;
         return (true);           
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
#ifdef WINCE_PPC
           case IDC_KIXREF:
                JDialogBox (IDD_XREFINFO,hwnd,(DLGPROC) dialog_xrefinfo,(LONG) this);
                return (true);
#endif WINCE_PPC
           case IDC_KILIST:
           case IDC_KIINSERT:
                SendDlgItemMessage (hwnd,IDC_KILIST,JL_INSERTTOFILE,0,0);
                return (true);
         }
         break;
  }
  return (false);
}

//
//  This is the dialog box handler for the More Kanji Info dialog.  There are
//  two copies of this routine.  One for PPC machines and one for all other 
//  routines (the gneral rotuine has some variation between CE and non-CE 
//  machines.
//
#ifdef WINCE_PPC

//
//  Dialog box handler for PPC machines for the More Kanji Info dialog.
//
int KANJI_info::dlg_moreinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  static short types[] = { IDS_KI_TYPEUNKNOWN,IDS_KI_TYPEASCII,IDS_KI_TYPEEXTENDED,IDS_KI_TYPESYMBOL,IDS_KI_TYPEJASCII,IDS_KI_TYPEHIRAGANA,IDS_KI_TYPEKATAKANA,IDS_KI_TYPEGREEK,IDS_KI_TYPERUSSIAN,IDS_KI_TYPERESERVED,IDS_KI_TYPEKANJI1,IDS_KI_TYPEKANJI2 };
  TCHAR buffer[SIZE_BUFFER];
  int   i;
  switch (message) {
//
//  This is the main part of the routine.  Since this is just an 
//  informational dialog box, the main part is in making the dialog 
//  box.
//
    case WM_INITDIALOG: 
         SetFocus (GetDlgItem(hwnd,IDC_MINEXT));
//
//  Do JIS value
// 
//
//  Utility routine to generate the character pattern geneated by varius
//  Japanese character encodings.  This is used to see what will show 
//  up when viewing a file.
//
         wsprintf (buffer,TEXT("%X"),jis2sjis(ch));
         SetDlgItemText (hwnd,IDC_KISHIFTJIS,buffer);
         wsprintf (buffer,TEXT("%X"),jis2unicode(ch));
         SetDlgItemText (hwnd,IDC_KIUNICODE,buffer);
//
//  Do character type.
//
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
         SetDlgItemText (hwnd,IDC_KITYPE,get_string(types[i]));
         if (i < CHARTYPE_KANJI1) return (false);   // Not kanji we have have no more info.
//
//  Process PinYin.
//
         if (korean) info_string (hwnd,IDC_KIKOREAN,korean);
         if (pinyin) info_string (hwnd,IDC_KIPINYIN,pinyin);
//
//  Process the entended data entries in the list.
//
         if (kinfo.extra) {
           info_fourcorner (hwnd,IDC_KIFC ,extend.fc_main,extend.fc_index);
           info_fourcorner (hwnd,IDC_KIFC2,fc_main2      ,extend.fc_index2);
           if (freq    ) SetDlgItemInt (hwnd,IDC_MIFREQ    ,freq    ,false);
           if (henshall) SetDlgItemInt (hwnd,IDC_MIHENSHALL,henshall,false);
           if (gakken  ) SetDlgItemInt (hwnd,IDC_MIGAKKEN  ,gakken  ,false);
           if (heisig  ) SetDlgItemInt (hwnd,IDC_MIHEISIG  ,heisig  ,false);
           if (oneill  ) SetDlgItemInt (hwnd,IDC_MIONEILL  ,oneill  ,false);
           if (extend.md_short1) {
             wsprintf (buffer,TEXT("%d.%04d"),extend.md_short1,extend.md_short2);
             SetDlgItemText (hwnd,IDC_KIMDSHORT,buffer);
           }
           if (extend.md_long) {
             if      (extend.md_x) wsprintf (buffer,TEXT("%dX"),extend.md_long);
             else if (extend.md_p) wsprintf (buffer,TEXT("%dP"),extend.md_long);
             else                  wsprintf (buffer,TEXT("%d") ,extend.md_long);
             SetDlgItemText (hwnd,IDC_KIMDLONG,buffer);
           }
         }
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
                EndDialog (hwnd,false);
                return (true);
           case IDC_MINEXT:
                JDialogBox (IDD_XREFINFO,hwnd,(DLGPROC) dialog_xrefinfo,(LONG) this);
                return (true);
         }
         break;
  }
  return (false);
}

#else WINCE_PPC

//
//  General dialog box handler for the More Kanji Info dialog.
//
int KANJI_info::dlg_moreinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  static short errors[] = { IDS_KI_SKIPPOSITION,IDS_KI_SKIPSTROKE,IDS_KI_SKIPBOTH,IDS_KI_SKIPBREEN };
  switch (message) {
//
//  This is the main part of the routine.  Since this is just an 
//  informational dialog box, the main part is in making the dialog 
//  box.
//
    case WM_INITDIALOG: 
#ifdef WINCE
         info_fourcorner (hwnd,IDC_KIFC ,extend.fc_main,extend.fc_index);
         info_fourcorner (hwnd,IDC_KIFC2,fc_main2      ,extend.fc_index2);
         if (korean) info_string (hwnd,IDC_KIKOREAN,korean);
         if (pinyin) info_string (hwnd,IDC_KIPINYIN,pinyin);
#endif WINCE
         if (kinfo.extra) {
           byte *ptr;
           int   i;
           TCHAR buffer[SIZE_BUFFER];
           EUC_buffer line;                     // EUC line buffer used to set strings in the list-box.
           if (freq    ) SetDlgItemInt (hwnd,IDC_MIFREQ    ,freq    ,false);
           if (henshall) SetDlgItemInt (hwnd,IDC_MIHENSHALL,henshall,false);
           if (gakken  ) SetDlgItemInt (hwnd,IDC_MIGAKKEN  ,gakken  ,false);
           if (heisig  ) SetDlgItemInt (hwnd,IDC_MIHEISIG  ,heisig  ,false);
           if (oneill  ) SetDlgItemInt (hwnd,IDC_MIONEILL  ,oneill  ,false);
           line.initialize (GetDlgItem(hwnd,IDC_MIXREF));   // Initialize buffer for output.
           for (ptr = xref; xref && *ptr; ptr += 3) {
             i = *((ushort *) (ptr+1));
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
             }
#ifdef WINCE
             for (i = 0; buffer[i]; i++) ((byte *) buffer)[i] = (char) buffer[i]; 
             ((byte *) buffer)[i] = 0;
#endif WINCE        
             put_line (&line,0,(byte *) buffer,false);
           }
#ifdef WINCE
           if (extend.md_short1) {
             wsprintf (buffer,TEXT("%d.%04d"),extend.md_short1,extend.md_short2);
             SetDlgItemText (hwnd,IDC_KIMDSHORT,buffer);
           }
           if (extend.md_long) {
             if      (extend.md_x) wsprintf (buffer,TEXT("%dX"),extend.md_long);
             else if (extend.md_p) wsprintf (buffer,TEXT("%dP"),extend.md_long);
             else                  wsprintf (buffer,TEXT("%d") ,extend.md_long);
             SetDlgItemText (hwnd,IDC_KIMDLONG,buffer);
           }
#endif WINCE
         }
         SetFocus (GetDlgItem(hwnd,IDC_MIXREF));
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

#endif WINCE_PPC

//
//  This is the dialog box handler for the Cross-Reference dialog.  This dialog
//  is only used on PPC machines, and only because there is not sufficient 
//  room for these items in any of the other dailog pages.
//
#ifdef  WINCE_PPC
int KANJI_info::dlg_xrefinfo (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  static short errors[] = { IDS_KI_SKIPPOSITION,IDS_KI_SKIPSTROKE,IDS_KI_SKIPBOTH,IDS_KI_SKIPBREEN };
  switch (message) {
//
//  This is the main part of the routine.  Since this is just an 
//  informational dialog box, the main part is in making the dialog 
//  box.
//
    case WM_INITDIALOG: 
         if (kinfo.extra) {
           byte *ptr;
           int   i;
           TCHAR buffer[SIZE_BUFFER];
           EUC_buffer line;                     // EUC line buffer used to set strings in the list-box.
           line.initialize (GetDlgItem(hwnd,IDC_MIXREF));   // Initialize buffer for output.
           for (ptr = xref; xref && *ptr; ptr += 3) {
             i = *((ushort *) (ptr+1));
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
             }
             for (i = 0; buffer[i]; i++) ((byte *) buffer)[i] = (char) buffer[i]; 
             ((byte *) buffer)[i] = 0;
             put_line (&line,0,(byte *) buffer,false);
           }
         }
         SetFocus (GetDlgItem(hwnd,IDC_MIXREF));
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
#endif WINCE_PPC

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
  ch = (ch & 0x7f7f)-0x3021;
  ch = HIBYTE(ch)*94+LOBYTE(ch);
  if (ch > KIMAX_KANJI) {                       // User asked for an invalid character
    memset (&kinfo,0,sizeof(kinfo));
    return;
  }
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
    freq = sh_kana = henshall = gakken = heisig = oneill = 0;       // Zero out the data fields.
    fc_main2 = -1;
    xref     = NULL;
    for (ptr += sizeof(struct extend); *ptr; ptr += 3) {            // Process the data list.
      if (islower(*ptr)) {                                          // Lowercase entry indicates start
        xref = ptr;                                                 //   of the cross-reference entries
        break;                                                      //   processing of these is delayed
      }                                                             //   until later.
      i = (((ushort) ptr[2]) << 8) | ptr[1];                        // Get interger parameter.
      switch (*ptr) {                                               // Common entries 
        case 'F': freq     = i; break;
        case 'I': sh_kana  = i; break;
        case 'E': henshall = i; break;
        case 'K': gakken   = i; break;
        case 'L': heisig   = i; break;
        case 'O': oneill   = i; break;
        case 'Q': fc_main2 = i; break;
        default:
             break;
      }
    }
  }
  return;
}

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

//
//  This is the handler for the WM_INITDIALOG message for the main Kanji Info 
//  dialog.  This has been seperatred out from the dialog handler to allow for
//  easier addaptation between the different platforms.  There are two main 
//  versions (PPC and non-PPC) machines (the non-PPC version has some CE/non-CE
//  variation.
//
#ifdef WINCE_PPC

//
//  PPC handler for WM_INITDIALOG for the Kanji Info dialog
//
void KANJI_info::init_dialog (HWND hwnd) {
  TCHAR      buffer[SIZE_BUFFER];       // Text buffer for building lines.
  EUC_buffer line;                      // EUC line buffer used to set strings in the list-box.
  TCHAR      buffer2[20];               // Buffer for accumulating pin-yin data and ASCII value of JIS code
  byte      *ptr;                       // Pointer to current location within the data buffer.
  int        i;
  if (ch > 0x00ff) ch &= 0x7f7f;        // We accept EUC characters.
//
//  Blank out all old data.
//
  SetDlgItemText (hwnd,IDC_KIJISCODE,TEXT(""));
  for (i = IDC_KISTROKES; i <= IDC_KISHKANA; i++) SetDlgItemText (hwnd,i,TEXT(""));
  SendDlgItemMessage (hwnd,IDC_KILIST,JL_RESET,0,0);
//
//  Do JIS value
// 
//
//  Utility routine to generate the character pattern geneated by varius
//  Japanese character encodings.  This is used to see what will show 
//  up when viewing a file.
//
  if (ch <= 0x00ff) { buffer2[0] = (byte) ch; buffer2[1] = 0; }
    else { buffer2[0] = HIBYTE(ch); buffer2[1] = LOBYTE(ch); buffer2[2] = 0; }
  wsprintf (buffer,TEXT("%X (%X) [%s]"),ch,ch | 0x8080,buffer2);
  SetDlgItemText (hwnd,IDC_KIJISCODE,buffer);
  SendDlgItemMessage (hwnd,IDC_KIBIGKANJI     ,WMU_SETWINDOWVALUE,0,ch);
  SendDlgItemMessage (hwnd,IDC_KIBUSHUCHAR    ,WMU_SETWINDOWVALUE,0,0);
  SendDlgItemMessage (hwnd,IDC_KICLASSICALCHAR,WMU_SETWINDOWVALUE,0,0);
//
//  Do character type.
//
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
  EnableWindow(GetDlgItem(hwnd,IDC_KIMORE),i >= CHARTYPE_KANJI1);
  if (i < CHARTYPE_KANJI1) return;          // Not kanji we have have no more info.
  memset (&kinfo,0,sizeof(kinfo));
  if (((ch & 0xff) < 0x21) || ((ch & 0xff) > 0x7e)) return;
//
//  Open and read kanji information, an display the easy items.
//        
  if (open_info(hwnd)) return;
  get_info (ch,INFO_ALL);
  close_info ();
  SendDlgItemMessage (hwnd,IDC_KIBUSHUCHAR,WMU_SETWINDOWVALUE,0,kinfo.bushu);
  SetDlgItemInt (hwnd,IDC_KISTROKES,kinfo.strokes,true);
  if (!kinfo.classical) SetDlgItemInt (hwnd,IDC_KIBUSHU,kinfo.bushu,true);
    else {
      wsprintf (buffer2,TEXT("%d (%d)"),kinfo.bushu,kinfo.classical);
      SetDlgItemText     (hwnd,IDC_KIBUSHU,buffer2);
      SendDlgItemMessage (hwnd,IDC_KICLASSICALCHAR,WMU_SETWINDOWVALUE,0,kinfo.classical);
    }       
  if (kinfo.grade  ) SetDlgItemInt  (hwnd,IDC_KIGRADE  ,kinfo.grade  ,false);
  if (kinfo.nelson ) SetDlgItemInt  (hwnd,IDC_KINELSON ,kinfo.nelson ,false);
  if (kinfo.halpern) SetDlgItemInt  (hwnd,IDC_KIHALPERN,kinfo.halpern,false);
  if (kinfo.haig   ) SetDlgItemInt  (hwnd,IDC_KIHAIG   ,kinfo.haig   ,false);
  wsprintf (buffer2,TEXT("%d-%d-%d"),kinfo.skip_t,kinfo.skip_1,kinfo.skip_2);
  if (kinfo.skip_t ) SetDlgItemText (hwnd,IDC_KISKIP,buffer2);
//
//  Read all of the line type entries all in one big block.  This is 
//  very brut-force, but it does save time/space, and everything else.
//
  line.initialize (GetDlgItem(hwnd,IDC_KILIST));    // Initialize buffer for output.
//
//  Process list elements.
//
  if (jwp_config.cfg.info_titles) put_line (&line,0,(byte *) "\x01\x1fmeanings\x1f",false);
  for (ptr = imi, i = 0; i < kinfo.imi; i++) ptr = put_reading(&line,0,ptr,i,kinfo.imi);
  if (jwp_config.cfg.info_titles && kinfo.on ) put_line (&line,0,(byte *) "\x01\x1fon-yomi\x1f",false);
  for (ptr = on,  i = 0; i < kinfo.on; i++) ptr = put_reading(&line,BASE_KATAKANA,ptr,i,kinfo.on);
  if (jwp_config.cfg.info_titles && kinfo.kun) put_line (&line,0,(byte *) "\x01\x1fkun-yomi\x1f",false);
  for (ptr = kun, i = 0; i < kinfo.kun; i++) ptr = put_reading(&line,BASE_HIRAGANA,ptr,i,kinfo.kun);
  if (jwp_config.cfg.info_titles && kinfo.nan) put_line (&line,0,(byte *) "\x01\x1fnanori\x1f",false);
  for (ptr = nan, i = 0; i < kinfo.nan; i++) ptr = put_reading(&line,BASE_HIRAGANA,ptr,i,kinfo.nan);
//
//  Process the entended data entries in the list.
//
  if (kinfo.extra) {
    if (extend.sh_rstroke || extend.sh_ostroke) {
      wsprintf (buffer,TEXT("%d%c%d.%d"),extend.sh_rstroke,extend.sh_radical+'a',extend.sh_ostroke,extend.sh_index);
      SetDlgItemText (hwnd,IDC_KISHDICT,buffer);
    }
    if (sh_kana) SetDlgItemInt (hwnd,IDC_KISHKANA,sh_kana,false);
  }
//
//  Activate the list so the user can use the cursor keys to move 
//  through the list.  ESC and or ENTER will still exit the dialog.
//
  SetFocus (GetDlgItem(hwnd,IDC_KILIST));
  return;
}

#else WINCE_PPC

//
//  Non-PPC handler for the WM_INITDIALOG for the Kanji Info dialog.
//  
void KANJI_info::init_dialog (HWND hwnd) {
  static short types[] = { IDS_KI_TYPEUNKNOWN,IDS_KI_TYPEASCII,IDS_KI_TYPEEXTENDED,IDS_KI_TYPESYMBOL,IDS_KI_TYPEJASCII,IDS_KI_TYPEHIRAGANA,IDS_KI_TYPEKATAKANA,IDS_KI_TYPEGREEK,IDS_KI_TYPERUSSIAN,IDS_KI_TYPERESERVED,IDS_KI_TYPEKANJI1,IDS_KI_TYPEKANJI2 };
  TCHAR      buffer[SIZE_BUFFER];       // Text buffer for building lines.
  EUC_buffer line;                      // EUC line buffer used to set strings in the list-box.
  TCHAR      buffer2[20];               // Buffer for accumulating pin-yin data and ASCII value of JIS code
  byte      *ptr;                       // Pointer to current location within the data buffer.
  int        i;

  if (ch > 0x00ff) ch &= 0x7f7f;        // We accept EUC characters.
//
//  Blank out all old data.
//
#ifndef WINCE
  for (i = IDC_KITYPE; i <= IDC_KIKOREAN; i++) SetDlgItemText (hwnd,i,TEXT(""));
#else   WINCE
  for (i = IDC_KITYPE; i <= IDC_KISHKANA; i++) SetDlgItemText (hwnd,i,TEXT(""));
#endif  WINCE
  SendDlgItemMessage (hwnd,IDC_KILIST,JL_RESET,0,0);
//
//  Do JIS value
// 
//
//  Utility routine to generate the character pattern geneated by varius
//  Japanese character encodings.  This is used to see what will show 
//  up when viewing a file.
//
  if (ch <= 0x00ff) { buffer2[0] = (byte) ch; buffer2[1] = 0; }
    else { buffer2[0] = HIBYTE(ch); buffer2[1] = LOBYTE(ch); buffer2[2] = 0; }
  wsprintf (buffer,TEXT("%X (%X) [%s]"),ch,ch | 0x8080,buffer2);
  SetDlgItemText (hwnd,IDC_KIJISCODE,buffer);
  wsprintf (buffer,TEXT("%X"),jis2sjis(ch));
  SetDlgItemText (hwnd,IDC_KISHIFTJIS,buffer);
  wsprintf (buffer,TEXT("%X"),jis2unicode(ch));
  SetDlgItemText (hwnd,IDC_KIUNICODE,buffer);
  SendDlgItemMessage (hwnd,IDC_KIBIGKANJI     ,WMU_SETWINDOWVALUE,0,ch);
  SendDlgItemMessage (hwnd,IDC_KIBUSHUCHAR    ,WMU_SETWINDOWVALUE,0,0);
  SendDlgItemMessage (hwnd,IDC_KICLASSICALCHAR,WMU_SETWINDOWVALUE,0,0);
//
//  Do character type.
//
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
  SetDlgItemText (hwnd,IDC_KITYPE,get_string(types[i]));
  EnableWindow(GetDlgItem(hwnd,IDC_KIMORE),i >= CHARTYPE_KANJI1);
  if (i < CHARTYPE_KANJI1) return;          // Not kanji we have have no more info.
  memset (&kinfo,0,sizeof(kinfo));
  if (((ch & 0xff) < 0x21) || ((ch & 0xff) > 0x7e)) return;
//
//  Open and read kanji information, an display the easy items.
//        
  if (open_info(hwnd)) return;
  get_info (ch,INFO_ALL);
  close_info ();
  SendDlgItemMessage (hwnd,IDC_KIBUSHUCHAR,WMU_SETWINDOWVALUE,0,kinfo.bushu);
  SetDlgItemInt (hwnd,IDC_KISTROKES,kinfo.strokes,true);
  if (!kinfo.classical) SetDlgItemInt (hwnd,IDC_KIBUSHU,kinfo.bushu,true);
    else {
      wsprintf (buffer2,TEXT("%d (%d)"),kinfo.bushu,kinfo.classical);
      SetDlgItemText     (hwnd,IDC_KIBUSHU,buffer2);
      SendDlgItemMessage (hwnd,IDC_KICLASSICALCHAR,WMU_SETWINDOWVALUE,0,kinfo.classical);
    }       
  if (kinfo.grade  ) SetDlgItemInt  (hwnd,IDC_KIGRADE  ,kinfo.grade  ,false);
  if (kinfo.nelson ) SetDlgItemInt  (hwnd,IDC_KINELSON ,kinfo.nelson ,false);
  if (kinfo.halpern) SetDlgItemInt  (hwnd,IDC_KIHALPERN,kinfo.halpern,false);
  if (kinfo.haig   ) SetDlgItemInt  (hwnd,IDC_KIHAIG   ,kinfo.haig   ,false);
  wsprintf (buffer2,TEXT("%d-%d-%d"),kinfo.skip_t,kinfo.skip_1,kinfo.skip_2);
  if (kinfo.skip_t ) SetDlgItemText (hwnd,IDC_KISKIP,buffer2);
//
//  Process PinYin.
//
#ifndef WINCE
  if (korean) info_string (hwnd,IDC_KIKOREAN,korean);
  if (pinyin) info_string (hwnd,IDC_KIPINYIN,pinyin);
#endif WINCE
//
//  Read all of the line type entries all in one big block.  This is 
//  very brut-force, but it does save time/space, and everything else.
//
  line.initialize (GetDlgItem(hwnd,IDC_KILIST));    // Initialize buffer for output.
//
//  Process list elements.
//
  if (jwp_config.cfg.info_titles) put_line (&line,0,(byte *) "\x01\x1fmeanings\x1f",false);
  for (ptr = imi, i = 0; i < kinfo.imi; i++) ptr = put_reading(&line,0,ptr,i,kinfo.imi);
  if (jwp_config.cfg.info_titles && kinfo.on ) put_line (&line,0,(byte *) "\x01\x1fon-yomi\x1f",false);
  for (ptr = on,  i = 0; i < kinfo.on; i++) ptr = put_reading(&line,BASE_KATAKANA,ptr,i,kinfo.on);
  if (jwp_config.cfg.info_titles && kinfo.kun) put_line (&line,0,(byte *) "\x01\x1fkun-yomi\x1f",false);
  for (ptr = kun, i = 0; i < kinfo.kun; i++) ptr = put_reading(&line,BASE_HIRAGANA,ptr,i,kinfo.kun);
  if (jwp_config.cfg.info_titles && kinfo.nan) put_line (&line,0,(byte *) "\x01\x1fnanori\x1f",false);
  for (ptr = nan, i = 0; i < kinfo.nan; i++) ptr = put_reading(&line,BASE_HIRAGANA,ptr,i,kinfo.nan);
//
//  Process the entended data entries in the list.
//
  if (kinfo.extra) {
    if (extend.sh_rstroke || extend.sh_ostroke) {
      wsprintf (buffer,TEXT("%d%c%d.%d"),extend.sh_rstroke,extend.sh_radical+'a',extend.sh_ostroke,extend.sh_index);
      SetDlgItemText (hwnd,IDC_KISHDICT,buffer);
    }
    if (sh_kana) SetDlgItemInt (hwnd,IDC_KISHKANA,sh_kana,false);
#ifndef WINCE
    info_fourcorner (hwnd,IDC_KIFC ,extend.fc_main,extend.fc_index);
    info_fourcorner (hwnd,IDC_KIFC2,fc_main2      ,extend.fc_index2);
    if (extend.md_long) {
      if      (extend.md_x) wsprintf (buffer,TEXT("%dX"),extend.md_long);
      else if (extend.md_p) wsprintf (buffer,TEXT("%dP"),extend.md_long);
      else                  wsprintf (buffer,TEXT("%d") ,extend.md_long);
      SetDlgItemText (hwnd,IDC_KIMDLONG,buffer);
    }
    if (extend.md_short1) {
      wsprintf (buffer,TEXT("%d.%04d"),extend.md_short1,extend.md_short2);
      SetDlgItemText (hwnd,IDC_KIMDSHORT,buffer);
    }
#endif WINCE
  }
//
//  Activate the list so the user can use the cursor keys to move 
//  through the list.  ESC and or ENTER will still exit the dialog.
//
  SetFocus (GetDlgItem(hwnd,IDC_KILIST));
  return;
}

#endif WINCE_PPC

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
int KANJI_info::open_info (HWND hwnd) {
  unsigned long done,magic,size;
//
//  If we have a cached information file we do not need to open it.
//
  if (info_cache) return (false);
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
    magic = *((long *) info_cache);
  }
//
//  Not cached then check the file information.
//
  else {
    ReadFile (handle,&magic   ,sizeof(long ),&done,NULL);     // Check for database id (magic)
//    ReadFile (handle,&jwp_config.kanji_flags,sizeof(long),&done,NULL);
//    ReadFile (handle,&count   ,sizeof(short),&done,NULL);     // Get the number of kanji in database (NOT USED AT THIS TIME)
//    ReadFile (handle,&last_jis,sizeof(short),&done,NULL);     // Last kanji in the database. (NOT USED AT THIS TIME)
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
//-------------------------------------------------------------------

//-------------------------------------------------------------------
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

//-------------------------------------------------------------------
//
//  Static routines.
//

static void  count_block (EUC_buffer *line,int count,byte *ptr,int base);   // Format a blockfor the count-kanji dialog.
static BOOL CALLBACK dialog_kanjicount (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
static int __cdecl kanji_compare (const void *elem1, const void *elem2 );   // Stub for quick-sort routine.

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
         add_dialog (kanji_count->dialog = hwnd,true);
         SetDlgItemText (hwnd,IDC_CKNUMBER,TEXT(""));
         for (i = IDC_CKFREQUENCY; i <= IDC_CKMEANING; i++) CheckDlgButton (hwnd,i,true);
         return (true);
//
//  Shut down routine
//
    case WM_DESTROY:
         remove_dialog (hwnd);
         delete kanji_count;
         kanji_count = NULL;
         return (true);
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
           case IDOK: {
                  KANJI_info kanji_info;        // Used to get character infomration (readings, etc).
                  EUC_buffer line;              // Used to put data into the list box.
                  int        frequency,onyomi;  // List catagories.
                  int        kunyomi,meaning;
                  TCHAR      text[20];          // Formating buffer.
                  int        i,j;
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
                    for (j = 0; j < colorkanji_size; j++) {
                      if (colorkanji_list[j] == kanji_count->list[i].kanji) break;
                    }
                    if (j == colorkanji_size) {
                      kanji_count->kanji_nolist += kanji_count->list[i].count; 
                      kanji_count->kanjis_nolist++;
                    }
                    else {
                      kanji_count->kanji_list += kanji_count->list[i].count;
                      kanji_count->kanjis_list++;
                    }
                  }
//
//  Exclude kanji on list.
//
                  if (IsDlgButtonChecked(hwnd,IDC_CKEXCLUDE)) {
                    for (i = 0; i < SIZE_KANJI; i++) {
                      for (j = 0; j < colorkanji_size; j++) {
                        if (colorkanji_list[j] == kanji_count->list[i].kanji) { kanji_count->list[i].count = 0; break; }
                      }
                    }
                  }
//
//  Include kanji only on list.
//
                  if (IsDlgButtonChecked(hwnd,IDC_CKINCLUDE)) {
                    for (i = 0; i < SIZE_KANJI; i++) {
                      for (j = 0; j < colorkanji_size; j++) {
                        if (colorkanji_list[j] == kanji_count->list[i].kanji) break;
                      }
                      if (j == colorkanji_size) kanji_count->list[i].count = 0;
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
           case IDCANCEL:
                DestroyWindow (hwnd);
                return (true);
         }
         break;
  }
  return (false);
}

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
//-------------------------------------------------------------------

//-------------------------------------------------------------------
//
//  Exported routines.
//

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

//
//  Deallocate memory resource associated with kanji information.
//
void free_info () {
  if (info_cache) free (info_cache);
  info_cache = NULL;
  return;
}

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
extern void createdialogparam (int id,HWND hwnd,DLGPROC proc,long param);

void kanji_info (HWND hwnd,int kanji) {
  KANJI_info *info;
  if (!(info = new KANJI_info)) { OutOfMemory (hwnd); return; }
  info->ch = kanji;
  JCreateDialog (IDD_KANJIINFO,hwnd,(DLGPROC) dialog_kanjiinfo,(LONG) info);
  return;
}

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