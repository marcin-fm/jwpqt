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
//  This modlues main function is to deal with the system fonts.  
//  There are three fonts used in the system currently.  These are
//  the kana/kanji font.  The user font used in the main edit window,
//  and the system font used in controls, and in the status bar.  
//  Various useful parameters are caclated buy the font system for use
//  by other routines.  The system currenly allows for changes in the 
//  fonts, but these are relativly limited.  
//
//  The onther function supported in this modlue is the color-kanji
//  feature.  This feature allows the user to generate a list of kanji.
//  When kanji are rendred, the kanji on this list (or off this list)
//  can be rendered in a different color.
//
//  Font system:
//
//  The original JWP fonts come in a number of sizes.  The fonts 
//  soted in the files are filled to a byte, thus a font image that is
//  16x16 requries 32 bytes, and a font that is 24x24 requires 72 bytes.
//  Windows, however, requries that bitmap widths be a multable of 
//  16 bits.  This has major implications when using fonts such as 
//  the 24x24 font.  This font has to be converted to a memory image
//  that is 32 (width)x24 (heigth).  JWPce only rescales fonts that 
//  are cached, thus such a font will always be cached.
//
//  Caches:
//
//  JWPce allows caching of some fonts.  A font that is cahced, uses
//  the bitmaps field points to the cache.  The cache parameter points
//  to an array of index values (not JIS values, but rather internal 
//  font table values).  These are the cached values.  The index field
//  points to the next location to be replaced when a chache miss occres.
//  The index value is cyclic and simply goes around and around.  This
//  means that even kana will get droped from the cache, but this 
//  seems to work well.  JWPce keeps the font file open if the file
//  is cached.  Thus we don't have to reopen it.  (As usuall, the 
//  file is open is share mode.)
//  

#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_edit.h"
#include "jwp_file.h"
#include "jwp_font.h"
#include "jwp_inpt.h"
#include "jwp_misc.h"
#include <winnls.h>
 
//===================================================================
//
//  Compile-time options.
//

#define NAME_KANJI      TEXT("k16x16.f00")  // Name of default system font.
#define NAME_BIGFONT    TEXT("k48x48.f00")  // Name of big kanji font.
#define NAME_MEDFONT    TEXT("k24x24.f00")  // Name of medium kanji font.

#define SIZE_KANJIBUFFER    1026            // Implies a 128x128 max font size.

//===================================================================
//
//  Exported data
//

class JWP_font clip_font;            // Font used for posting bitmaps on the clipboard.
class JWP_font bar_font;             // Font used for kanji bars.
class JWP_font file_font;            // Font used for file information.
class JWP_font sys_font;             // Font used for system data (bushu, JIS table, etc).
class JWP_font edit_font;            // Font used for line edit
class JWP_font list_font;            // Font used for lists.
short sysfont_height;                // Height of the system font (used for toolbars).

//===================================================================
//
//  Static routines.
//
//  Provide various reveice capabilities for routines in this module.
//

//--------------------------------
//
//  Data and definitions assoicated with color-kanji.
//
#define COLORKANJI_NAME TEXT("colkanji.lst")    // File name for storeing color-kanji list.

#define TEXT_COLORKANJI "Color-Kanji List"  // Text for error messages, just to make sure is it the same.

//KANJI *colorkanji_list = NULL;              // Pointer to the actual list.
//short  colorkanji_size = 0;                 // Size of the list in kanji.

//===================================================================
//
//  Static routines.
//

//--------------------------------
//
//  Opens a font or uses the automattic font associated with it.
//
//      cfg  -- Configuration font struct, indicates the font to be open.
//      font -- Font to actually open.
//      def  -- Font to be used for default.
//
static void auto_font (struct cfg_font *cfg,JWP_font *font,JWP_font *def) {
  if (cfg->automatic || !font->open(cfg->name,cfg->size,jwp_config.cfg.cache_displayfont,NULL,false)) font->copy(def);
  return;
}

//--------------------------------
//
//  Determines if a kanji is in a specific set.
//
//      kanji  -- Kanji to search for.
//      set    -- Pointer to list of kanji that is the list.
//      size   -- Number of characterrs in the list.
//
//      RETURN -- A return value of zero indicates that the kanji is
//                not in the list.  Any other return is 1+the index 
//                of the character in the list.
//
static int in_set (int kanji,KANJI *set,int size) {
  int i;
  for (i = 0; i < size; i++) if (kanji == set[i]) return (i+1);
  return (false);
}

//--------------------------------
//
//  This is a faster version of open_font() that is optimized for opening a bitmapped font.
//  This was separated out to save some code space.
//
//      name     -- Name of font to open.
//      cache    -- Indicates if the font is to be cahced.
//      vertical -- Print the font vertically.
//
//      RETURN -- Pointer to newly open bitmapped font or NULL, if the font could not be open.
//
static KANJI_font *open_bitmap (TCHAR *name,int cache,int vertical) {
  BITMAP_KANJI_font *font;
  if (!(font = new BITMAP_KANJI_font)) return (NULL);
  if (font->open(name,cache,vertical)) {
    font->remove ();
    font = NULL;
  }
  return (font);
}

//--------------------------------
//
//  Attempts to open a font based on a name, and size.  This routine autmatically handles
//  bot bitmaped and TrueType fonts.
//
//      name     -- Name of font to open.  If this ends in .f00 it is assumed to be a 
//                  bitmapped font.  Any other ending is a TrueType font.
//      size     -- Size of the font in pixels (used only for TrueType fonts).
//      cache    -- Indictes if the font is to be cached (used only for bitmapped fonts).
//      hdc      -- If included indicates the DC for which the font will be used.  If this is
//                  not included, we autmatically assume it is a screen font.
//      vertical -- If non-zero the font will be vertical.
//
//      RETURN -- Pointer to an open font, or NULL if an error occures.
//
static KANJI_font *open_font (TCHAR *name,int size,int cache,HDC hdc,int vertical) {
  KANJI_font *font;
  if ((lstrlen(name) >= 4) && stricmp(name+lstrlen(name)-4,TEXT(".f00"))) {
    HDC display_hdc = null;
    TRUETYPE_KANJI_font *tt_font;
    if (!hdc) display_hdc = hdc = GetDC(main_window);       // No DC so this is a screen font, make a DC.
    if (!(tt_font = new TRUETYPE_KANJI_font)) return (NULL);// Allocate class
    if (tt_font->open(hdc,name,size,vertical)) {            // Try to open
      tt_font->remove();
      tt_font = NULL;
    }
    font = tt_font;
    if (display_hdc) ReleaseDC (main_window,hdc);           // If we made the dc then get rid of it
  }
  else {
    font = open_bitmap(name,cache,vertical);
  }
  return (font);
}

//===================================================================
//
//  Begin Class KANJI_font.
//
//  This class contains most of the kanji forn manimulation and display 
//  code.
//

#define KANJI_BAD   0x2223      // Black square used for invalid kanji.
#define NO_CHANGE   0xFFFFFFFF  // Indicates no change for the color (This color is invalid in Windows).

static class KANJI_font *big_kanji   = NULL;    // Largest font avaialbe, used for big font in character info.
static class KANJI_font *jist_kanji  = NULL;    // Kanji font for the JIS table
static class KANJI_font *print_kanji = NULL;    // Kanji font for printing

//--------------------------------
//
//  This routine sets the display color based on if a character is in the color-kanji list.
//
//      jis   -- Character to be checked (JIS code).
//      hdc   -- Display context (since this may change the text color).
//      color -- If the color is unchanged, this will return as NO_CHANGE.  If the color is changed,
//               this will return as the original color so that it may be restored.
//
void KANJI_font::find_color (int jis,HDC hdc,COLORREF &color) {
  int i;
  color = NO_CHANGE;
  if (jwp_config.cfg.colorkanji_mode && ISKANJI(jis)) {
    i = color_kanji.in(jis);
    if (jwp_config.cfg.colorkanji_mode == COLORKANJI_MATCH) {
      if ( i) color = SetTextColor(hdc,jwp_config.cfg.colorkanji_color);
    }
    else {
      if (!i) color = SetTextColor(hdc,jwp_config.cfg.colorkanji_color);
    }
  }
  return;
}

//--------------------------------
//
//  This is a general routine to close and deallocate any resource associated with a font.
//  This will work on any supported font type, and will even handle the case of the font
//  already being closed.
//
void KANJI_font::remove () {
  if (!this) return;            // Font is not open
  close ();                     // Close font
  delete this;                  // Delete font class item.
  return;
}

//===================================================================
//
//  Begin Class BITMAP_KANJI_font.
//
//  This class contains the derived class for dealing with bitmapped kanji fonts.  These are
//  the fonts distributed with JWP/JWPce as apposed to TrueType fonts.
//

typedef struct {                // Header structure from font file.
    KANJI facename[20];         // Font family name
    INT16 width, height;        // Width and height in pixels 
    INT16 charsize;             // Number of bytes that make up a character 
    INT16 verticals;            // The number of extra "vertical" characters
    long  offset;               // Byte offset to the beginning of bitmaps 
    INT16 holes;                // Packed JIS coding or 93x93 with holes
    INT16 leading, spacing;     // Horizontal / Vertical gaps
    char extra[6];              // Make it up to 64 bytes  
} FONTHEADER;

//--------------------------------
//
//  Simple constructor that simply clears some of the parameters.
//
BITMAP_KANJI_font::BITMAP_KANJI_font () {
  bitmaps = NULL; 
  cache   = NULL;
  hdcmem  = null;
  file    = INVALID_HANDLE_VALUE;
#ifndef WINCE
  hbitmap = null; 
#endif WINCE
  return;
}

//--------------------------------
//
//  Close a font.
//
void BITMAP_KANJI_font::close () {
  if (bitmaps) free         (bitmaps);
  if (hdcmem ) DeleteDC     (hdcmem);
  if (cache  ) free         (cache);
  if (file != INVALID_HANDLE_VALUE) CloseHandle (file);
  bitmaps  = NULL;
  cache    = NULL;
  hdcmem   = null;
  file     = INVALID_HANDLE_VALUE;
#ifndef WINCE
  if (hbitmap) DeleteObject (hbitmap);
  hbitmap = null;
#endif WINCE
  return;
}

//--------------------------------
//
//  The big one here, this is the kanji rendering engine:
//
//      hdc -- Context to render into.
//      jis -- JIS code to render (will be converted on the fly).
//      x,y -- Location in the context.
//     
void BITMAP_KANJI_font::draw (HDC hdc,int jis,int x,int y) {
  if (vertfont) {                           // Vertical printing this way is only used for 
    RECT rect;                              //   writing to the clipboard.  Thus efficiency 
    rect.top    = y-height+1;               //   is not that important.
    rect.bottom = y+1;
    rect.left   = x;
    rect.right  = rect.left+width;
    fill (hdc,jis,&rect);
  }
  else {
    long     index;
    COLORREF color;
    index  = find_kanji(jis,hdc,color);
#ifdef WINCE
    HBITMAP hbitmap,tbitmap;
    hbitmap = CreateBitmap(width,height,1,1,bitmaps+index);
    tbitmap = (HBITMAP) SelectObject (hdcmem,hbitmap);
    BitBlt       (hdc,x+hshift,y-height,width,height,hdcmem,0,0,SRCCOPY);
    SelectObject (hdcmem,tbitmap);
    DeleteObject (hbitmap);
#else  WINCE
    SetBitmapBits (hbitmap,bmsize,bitmaps+index);
    BitBlt        (hdc,x+hshift,y-height,width,height,hdcmem,0,0,SRCCOPY);
#endif WINCE
    if (color != NO_CHANGE) SetTextColor(hdc,color);
  }
  return;
}

//--------------------------------
//
//  This is a variant on the KANJI_font::draw routine that streatches
//  the font to fill a given rectange.  This is used for the big 
//  character in the character-info dialog box.
//
//      hdc      -- Context to render into.
//      jis      -- JIS code to render (will be converted on the fly).
//      rect     -- Rectangle that the character should fill to.
//
#define SIZE_NOROTATE   (sizeof(no_rotate  )/sizeof(KANJI))
#define SIZE_NEEDOFFSET (sizeof(need_offset)/sizeof(KANJI))
     
void BITMAP_KANJI_font::fill (HDC hdc,int jis,RECT *rect) {
#ifndef WINCE
  char rotate[SIZE_KANJIBUFFER];    // Buffer used to rotate characters.
  static KANJI no_rotate[] = {      // These characters should not be rotated (Japanese quote, etc.)
    0x213b,0x213c,0x2141,0x2142,0x2143,0x2144,0x2145,0x214a,0x214b,
    0x214c,0x214d,0x214e,0x214f,0x2150,0x2151,0x2152,0x2153,0x2154,0x2155,
    0x2156,0x2157,0x2158,0x2159,0x215a,0x215b,0x2161,0x2162,0x2163,0x2164,
    0x2165,0x2166,0x2167,0x222a,0x222b,0x222e,0x2127,
  };                                // These characters need to be moved to the upper right corner 
  static KANJI need_offset[] = {    //   (relative to character) after rotate or setup.
    0x2122,0x2123,0x2124,0x2125,0x2421,0x2423,0x2425,0x2427,0x2429,0x2443,
    0x2463,0x2465,0x2467,0x246e,0x2521,0x2523,0x2525,0x2527,0x2529,0x2543,
    0x2563,0x2565,0x2567,0x256e,0x2575,0x2576,
  };
  static byte mask[] = { 0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01 };
#endif WINCE
  char    *bitmap;                      // The character bitmap
  int      x,y;                         // Character shifts
  long     index;                       // Index (byte) into bitmap arrays
  COLORREF color;                       // Save color for color kanji.
  index  = find_kanji(jis,hdc,color);   // Find the chracter.
  x = y = 0;                            // Character shift is normally zero.
  bitmap = bitmaps+index;               // Get the character bitmap
//
//  Vertical display, so we have to rotate characters and do all sorts 
//  of other stuff.
//
#ifndef WINCE
  if (vertfont) {
    int bytes,i,j,k,n;
    bytes = ((width+15)/16)*2;          // Width of a bitmap line in bytes.
//
//  This character needs to be rotated.
//
    if (!in_set(jis,no_rotate,SIZE_NOROTATE)) {
      memset (rotate,0,bmsize);                 // Zero rotation buffer.
      for (i = 0; i < width; i++) {
        for (j = 0; j < height; j++) {
          k = width-i-1;
          n = bitmap[bytes*j+k/8] & mask[k%8];
          if (n) rotate[bytes*i+j/8] |= mask[j%8];
        }
      }
      bitmap = rotate;                          // Use the rotated bitmap.
    }
//
//  These characters need to be translated.  These characters are 
//  basically shifted to the upper right corner (relative to the 
//  character).
//
    if (in_set(jis,need_offset,SIZE_NEEDOFFSET)) {
      for (i = 0; i < height; i++) {            // Find number of blank lines at the top.
        for (j = 0; j < width; j++) {
          if (!(bitmap[bytes*i+(j/8)] & mask[j%8])) break;
        }
        if (j < width) break;
      }
      y = i;
//    int bottom;
//    for (i = height-1; i >= 0; i--) {         // Find number of blank lines at the bottom.
//      for (j = 0; j < width; j++) {
//        if (!(bitmap[bytes*i+(j/8)] & mask[j%8])) break;
//      }
//      if (j < width) break;
//    }
//    bottom = height-i-1;
      for (j = 0; j < width; j++) {             // Find the number of blank columns on the left
        for (i = 0; i < height; i++) {
          if (!(bitmap[bytes*i+(j/8)] & mask[j%8])) break;
        }
        if (i < height) break;
      }
      x = j;
      for (j = width-1; j >= 0; j--) {          // Find the number of blank columns on the right
        for (i = 0; i < height; i++) {
          if (!(bitmap[bytes*i+(j/8)] & mask[j%8])) break;
        }
        if (i < height) break;
      }
      y -= (width-j-1);
//    x -= bottom;
      x = (x*(rect->right-rect->left))/height;  // Need these tranlations in output coordinates, not input coorciantes
      y = (y*(rect->bottom-rect->top))/width;
    }
  }
#endif WINCE
//
//  Back to general rotuine.
//
//  SetStretchBltMode (hdc,HALFTONE);   //### These do not appear to change the apperance any
//  SetBrushOrgEx     (hdc,0,0,NULL);
#ifdef WINCE
  HBITMAP hbitmap,tbitmap;
  hbitmap = CreateBitmap(width,height,1,1,bitmap);
  tbitmap = (HBITMAP) SelectObject(hdcmem,hbitmap);
  StretchBlt   (hdc,rect->left-x,rect->top-y,rect->right-rect->left,rect->bottom-rect->top,hdcmem,0,0,width,height,SRCCOPY);
  SelectObject (hdcmem,tbitmap);
  DeleteObject (hbitmap);
#else  WINCE
  SetBitmapBits (hbitmap,bmsize,bitmap);
  StretchBlt    (hdc,rect->left-x,rect->top-y,rect->right-rect->left,rect->bottom-rect->top,hdcmem,0,0,width,height,SRCCOPY);
#endif WINCE
  if (color != NO_CHANGE) SetTextColor(hdc,color);
  return;
}

//--------------------------------
//
//  This routine finds a kanji value in the font cache.  If the value 
//  is not already loaded, this rotuine loads it.
//
//      jis    -- Character being requirested.
//      hdc    -- Device context to redner character into
//      color  -- Color setting register, if set to -1 thi is not 
//                using color kanji.  If this is set to any other 
//                color this is the saved version of color fonts.
//
//      RETURN -- Index into font bitmaps for this character.  This 
//                is not the character index, but rather the index 
//                to the actual bitmap.
//
int BITMAP_KANJI_font::find_kanji (int jis,HDC hdc,COLORREF &color) {
  int i;
  unsigned long done;
//
//  Is this character in the color-kanji list.
//
  find_color (jis,hdc,color);
//
//  Now find the character.
//
  jis = jis_index(jis);                     // Convert to intial font code.
  if (!cache) return (jis*bmsize);          // Font is not chached.
//
//  Search cache.
//
  if ((i = in_set(jis+1,(KANJI *) cache,jwp_config.cfg.font_cache))) return (--i*bmsize);
//
//  Not in cache, so we need to load the character.
//
  SetFilePointer (file,sizeof(FONTHEADER)+jis*bmfilesize,NULL,FILE_BEGIN);
//
//  Word aligned font, needs no translation.
//
  if (bmsize == bmfilesize) ReadFile (file,bitmaps+index*bmsize,bmsize,&done,NULL);
//
//  Non-word aligned font (implies byte alighed), needs translation.
//
    else {
      int   j;
      char *ptr,*p,buffer[SIZE_KANJIBUFFER];
      ptr = bitmaps+index*bmsize;                       // Memory font data loation.
      ReadFile (file,buffer,bmfilesize,&done,NULL);     // Read file font data.
      for (p = buffer, i = 0; i < height; i++) {        // Do rows.
        for (j = 0; j < width/8; j++) *ptr++ = *p++;    //   Do columns
        *ptr++ = 0;                                     //   Extra byte per line.
      }
    }
//
//  Update cache and return.
//
  cache[index] = jis+1;
  jis          = index++;
  if (index == jwp_config.cfg.font_cache) index = 0;
  return (jis*bmsize);
}

//--------------------------------
//
//  This routine converts between JIS codes used in the 
//  program and internal codes used in the fonts.  Note
//  the font information is highly compresses, so unused 
//  values have been removed.
//
//  I got this routine from JWP's soruce, and the following is the 
//  notice contained in the JWP source file:
//
//  Copyright (C) Stephen Chung, 1995.  All rights reserved. 
//  The following routine came from KD, the Kanji Driver program  
//  written by Izumi Ozawa.  It was reformatted and slightly      
//  modified.  Many thanks to Izumi for his donation.            
//
//  Note, I have modified the code yet again, but only slightly.
//
int BITMAP_KANJI_font::jis_index (int jis) {
  int hi, lo;

  hi = HIBYTE(jis);   // Removed & 0x007f, because, should be unecessary.
  lo = LOBYTE(jis);   // Removed & 0x007f, because, should be unecessary.

  if ((lo <= 0x20) || (lo >= 0x7f) || (hi <= 0x20) || (hi >= 0x75)) goto BadKanji;
  if ((hi == 0x74) && (lo > 0x24)) goto BadKanji;

  hi = 94 * (hi - 33) + (lo - 33);

  if (holes) return (hi);

  /* Skip the holes */

  if      (hi <= 107)                return (hi);      /* 0..93               $2121-$217e, 0  -93   */
                                                       /* 94..107             $2221-$222e, 94 -107  */
  else if (hi >= 203  && hi <=  212) return (hi-56);   /* 203..212   0 .. 9,  $2330-$2339, 147-156  */
  else if (hi >= 220  && hi <=  245) return (hi-63);   /* 220..245   A .. Z,  $2341-$235a, 157-182  */
  else if (hi >= 252  && hi <=  277) return (hi-69);   /* 252..277   a .. z,  $2361-$237a, 183-208  */
  else if (hi >= 282  && hi <=  364) return (hi-73);   /* 282..364  hiragana, $2421-$2473, 209-291  */
  else if (hi >= 376  && hi <=  461) return (hi-84);   /* 376..461  katakana, $2521-$2576, 292-377  */
  else if (hi >= 470  && hi <=  493) return (hi-92);   /* 470..493   GREEK,   $2621-$2638, 378-401  */
  else if (hi >= 502  && hi <=  525) return (hi-100);  /* 502..525   greek,   $2641-$2658, 402-425  */
  else if (hi >= 564  && hi <=  596) return (hi-138);  /* 564..596   RUSSIAN, $2721-$2741, 426-458  */
  else if (hi >= 612  && hi <=  644) return (hi-153);  /* 612..644   russian, $2751-$2771, 459-491  */
                                                       /*  n/a       linedraw,  n/a      , 492-523  */
  else if (hi >= 1410 && hi <= 4374) return (hi-886);  /* 1410..4374 kanji-1, $3021-$4f53, 524-3488 */
  else if (hi >= 4418 && hi <= 7805) return (hi-928);  /* 4418..7805 kanji-2, $5021-$7424, 3490-6877*/

    /* No such kanji */
BadKanji:
  return (jis_index(KANJI_BAD));
}

//--------------------------------
//
//  This routine opens a font up for operations.
//
//      name     -- Name of font to open.
//      docache  -- If non-zero forces font chaching.  
//      vertical -- Print fonts vetically
//
//      RETURN   -- A non-zero value indicates an error.
//
//  Not that even if no font caching is requested, JWPce may overide
//  this request if the font requested has a non-word alighed strucure.
//  Then to perform the translations, JWPce will implement caching
//  anyway.
//
int BITMAP_KANJI_font::open (TCHAR *name,int docache,int vertical) {
  int number;
  FONTHEADER header;
  unsigned long done;
  if (INVALID_HANDLE_VALUE == (file = jwp_config.open(name,OPEN_READ,false))) return (true);
  if (!ReadFile(file,&header,sizeof(header),&done,NULL)) return (true);
  truetype   = false;
  vertfont   = vertical;
  width      = header.width;
  height     = header.height;
  bmsize     = header.height*sizeof(short)*((header.width/8+1)/sizeof(short));
  bmfilesize = header.charsize;
  holes      = header.holes;
  leading    = header.leading;
  spacing    = header.spacing/2;                // ### this was modified because character spacing was too big
  hshift     = spacing/2;
  number     = (GetFileSize(file,NULL)-sizeof(header))/bmsize;
  if (bmsize != bmfilesize) docache = true;     // Because of font adjustment, odd font sizes must be cached.
//
//  Cached fonts, allocate tables, and intialize values.
//
  if (docache) {
    cache   = (KANJI *) calloc(jwp_config.cfg.font_cache,sizeof(short));
    bitmaps = (char  *) calloc(jwp_config.cfg.font_cache,bmsize);
    if (!cache || !bitmaps) return (true);
    index = 0;
  }
//
//  Non-cached fonts.  Read entire font table, and clear some values.
//
  else {
    if (!(bitmaps = (char *) calloc(number,bmsize)) || !ReadFile(file,bitmaps,number*bmsize,&done,NULL)) return (true);
    CloseHandle (file);
    file = INVALID_HANDLE_VALUE;
  }
//
//  This stuff was orriginally located in the draw, routine.  This allowed
//  draw to respond to different hdc's.  I moved the standard display
//  stuff here to save on operating speed.
//
//  This stuff builds a memory HDC that is compatable with the screen.
//  Bitmaps are first transfered here then blit into the main display.
//
#ifndef WINCE
  if (!(hbitmap = CreateBitmap(width,height,1,1,bitmaps))) return (true);
#endif WINCE
  HDC hdc;
  hdc    = GetDC(main_window);
  hdcmem = CreateCompatibleDC(hdc);
#ifndef WINCE
  SelectObject (hdcmem,hbitmap);
//SetMapMode   (hdcmem,GetMapMode(hdc));        // Not necessary
#endif WINCE
  ReleaseDC    (main_window,hdc);
  return (false);
}

//
//  End Class BITMAP_KANJI_font
//
//===================================================================

//===================================================================
//
//  Begin Class TRUETYPE_KANJI_font.
//
//  This class contains the derived class for dealing with TrueType kanji fonts.
//

#ifndef WINCE   // Windows CE TrueType font support is directly through the font 
                // rotuines, not through the glyph indexing.  This works, but does 
                // not allow vertical character replacement.

#define X   ((char *) &x)

//--------------------------------
//
//  Swap byte order within a long int.
//
//      x      -- Integer.
//
//      RETURN -- The swapped integer
//
static ulong swap_long (ulong x) {
  char p;
  p    = X[0];
  X[0] = X[3];
  X[3] = p;
  p    = X[1];
  X[1] = X[2];
  X[2] = p;
  return (x);
}

//--------------------------------
//
//  Swap byte order within a short int.
//
//      x      -- Integer.
//
//      RETURN -- The swapped integer
//
static ushort swap_short (ushort x) {
  char p;
  p    = X[0];
  X[0] = X[1];
  X[1] = p;
  return (x);
}

#endif WINCE

//
//  Simple class constructor to make sure the object is initialized correctly.
//
TRUETYPE_KANJI_font::TRUETYPE_KANJI_font (void) {
  font = null;
#ifndef WINCE                       // Win CE does not use glyph addressing
  cmap = NULL;
  gsub = NULL;
#endif WINCE
  return;
}

//--------------------------------
//
//  Close a font.
//
void TRUETYPE_KANJI_font::close () {
  if (font) DeleteObject (font);
  font = null;
#ifndef WINCE                       // Win CE does not use glyph addressing
  if (cmap) free         (cmap);
  if (gsub) free         (gsub);
  cmap = NULL;
  gsub = NULL;
#endif WINCE
  return;
}

//--------------------------------
//
//  Basic render routine for TrueType fonts.
//
//      hdc -- Display contect to render to.
//      jis -- Character to render
//      x,y -- Location in the context.
//
void TRUETYPE_KANJI_font::draw (HDC hdc,int jis,int x,int y) {
  COLORREF color;
  ushort   glyph;

  find_color (jis,hdc,color);                       // Get color.
  SetBkMode (hdc,TRANSPARENT);                      // Make background transparent
  font = (HFONT) SelectObject(hdc,font);            // Change fonts
#ifndef WINCE
  glyph = jis_index(jis);                           // Find glyph index
  ExtTextOut (hdc,x+hshift,y+vshift,ETO_GLYPH_INDEX,NULL,(TCHAR *) &glyph,1,NULL);
#else WINCE
  glyph = jis2unicode(jis);
  ExtTextOut (hdc,x+hshift,y+vshift,0,NULL,(TCHAR *) &glyph,1,NULL);
#endif WINCE
  font = (HFONT) SelectObject(hdc,font);            // Restore font
  if (color != NO_CHANGE) SetTextColor(hdc,color);  // Restroe color
  return;
}

//--------------------------------
//
//  This is a variant on the KANJI_font::draw routine that streatches
//  the font to fill a given rectange.  This is used for the big 
//  character in the character-info dialog box.
//
//      hdc      -- Context to render into.
//      jis      -- JIS code to render (will be converted on the fly).
//      rect     -- Rectangle that the character should fill to.
//
//  Because TrueType fonts are generate rotated for vertical printing, the vertical flag is not
//  used, and this routine simply prints the character in the center of the selected box.
//
void TRUETYPE_KANJI_font::fill (HDC hdc,int jis,RECT *rect) {
  draw (hdc,jis,rect->left+(rect->right-rect->left+1-width)/2-hshift,rect->bottom-(rect->bottom-rect->top+1-height)/2);
  return;
}

//--------------------------------
//
//  Convert JIS character code into an glyph index for the font.
//
//      jis    -- JIS code to be processed.
//
//      RETURN -- Glyph index to be used in rendering the character.  If the glyph is not
//                present, the routine will automatically return the default blank character.
//
int TRUETYPE_KANJI_font::jis_index (int jis) {
#ifndef WINCE
  int i;
  jis = jis2unicode(jis);                                       // Convert to UNICODE
  for (i = 0; (i < count) && (jis > end[i]); i++);              // Find segment containning charcter
  if (jis < start[i]) return (jis_index(KANJI_BAD));            // Character is out of segment (does not exist).
  if (!offset[i]) jis += delta[i];                              // Displacement character
    else  jis = *(offset[i]/2 + (jis - start[i]) + &offset[i]); // Get value from glyph array.
  if (!jis) return (jis_index(KANJI_BAD));                      // No not a good character.
  if (gsub) {                                                   // If this is here we are printing vertical.
    for (i = 0; (i < vcount) && (from[i] < jis); i++);          // Find the glyph in the substitution list.
    if (from[i] == jis) jis = to[i];                            // Do we need to stubstitue.
  }
  return (jis);                                                 // Return value.
#else WINCE
  jis = jis2unicode(jis);                                       // No glyph addressing, so assume all valid unicode
  if (!jis) return (jis2unicode(KANJI_BAD));                    //   is in the font.
  return (jis);
#endif WINCE
}

//--------------------------------
//
//  This routine opens a font up for operations.
//
//      hdc      -- Display context for the font (screen or printer).
//      name     -- Name of font to open.
//      size     -- Indicates pixal size of the characters generated by the font.  This is 
//                  ignored for bitmap fonts, but actually determines the font size for TrueType
//                  fonts.
//      vertical -- Opens a font for vertical printing.
//
//      RETURN   -- A non-zero value indicates an error.
//
#define CMAP (*((ulong *) "cmap"))      // Character map tag
#define GSUB (*((ulong *) "GSUB"))      // Glyph substitution tag
#define VERT (*((ulong *) "vert"))      // Vertical printing tag

//
//  Macro used for addressing within the TrueType font structure.  This calucates and address from
//  the base of one structure and casts the result to the final type.  Note the offset can only
//  be short, since the swap is done here.
//
#define ADDRESS(t,b,o)    (struct t *) (((char *) b)+swap_short(o))
                                                                    
int TRUETYPE_KANJI_font::open (HDC hdc,tchar *name,int size,int vertical) {
//
//  Open the kanji font.
//
  LOGFONT    lf;
  memset (&lf,0,sizeof(lf));
  lstrcpy (lf.lfFaceName,name);
  lf.lfHeight     = -size;
  lf.lfCharSet    = SHIFTJIS_CHARSET;
  lf.lfEscapement = lf.lfOrientation = (vertical ? 900 : 0);
  if (!(font = CreateFontIndirect(&lf))) return (true);
  truetype = true;
  vertfont = vertical;
//
//  Access the font thrugh the DC
//
  TEXTMETRIC tm;
  font = (HFONT) SelectObject(hdc,font);
//
//  Get the metric information from the font.
//
  GetTextMetrics (hdc,&tm);
//width   = (short) tm.tmAveCharWidth;
//height  = (short) tm.tmAscent;
//leading = (short) tm.tmExternalLeading;
  height  = size; 
  width   = (short) tm.tmMaxCharWidth;
  if (width > height) width = height;
// HACK -- Need a better system for getting kanji width.  Currently assuming height=width.  This was added to support full UNICODE FONTS.
  leading = height/8; 
  spacing = width/12;
  hshift  = spacing/2-1;            // This -1 seems to give better screen display and should not show up on the printer.
  vshift  = (short) -height;
//
//  Read the cmap inforamtion from the font.  
//
//  The system must have a way to do this.  This should not have to be done here, but I 
//  could not find another way to do this.
//
#ifndef WINCE               // Windows CE does not use glyph addressing
  struct cmap_header {      // cmap header structure
    ushort version;         //   file version number (not used).
    ushort number;          //   Number of encodings
  };
  struct table {            // Encoding sub-tables 
    ushort platform_id;     //   platform id (we want 3 == PC)
    ushort encoding_id;     //   encoding id (we want 1 == UNICODE)
    ulong  offset;          //   offset from start of cmap to the actual sub-table
  } *tables;
  struct subtable {         // A partial version of a cmap sub-table
    ushort format;          //   format (should be 4)
    ushort length;          //   length of sub-table
    ushort version;         //   version (not used)
    ushort segCountX2;      //   twice the number of segments in the sub table 
    ushort searchRange;     //   junk
    ushort entrySelector;   //   junk
    ushort rangeShift;      //   junk
    ushort endCount;        //   start of end list table 
  } *sub;                   // -- Rest of table is calculated on the fly, since this table cannot be represented with a C++ structure.
  int  i;
  long j;

  j = GetFontData (hdc,CMAP,0,NULL,0);                              // Size of the cmap data
  if (!(cmap = (char *) malloc(j))) return (true);                  // Make a memory block for this
  if (GDI_ERROR == GetFontData(hdc,CMAP,0,cmap,j)) return (true);   // Read the cmap.
  j      = swap_short(((struct cmap_header *) cmap)->number);       // Get number of sub-tables.
  tables = (struct table *) (cmap+sizeof(struct cmap_header));      // Get sub-tables
  for (i = 0; i < j; i++) {                                         // Find our sub-table (3,1)
    if ((swap_short(tables[i].platform_id) == 3) && (swap_short(tables[i].encoding_id) == 1)) break;
  }
  if (i >= j) return (true);                                        // Could not find sub-table
  sub = (struct subtable *) (cmap+swap_long(tables[i].offset));     // Find the actual sub-table
  if (swap_short(sub->format) != 4) return (true);                  // Check, is the table format 4.
  count  = swap_short(sub->segCountX2)/2;                           // Get count (number of segments)
  end    = &sub->endCount;                                          // Get address of segment tables
  start  = end+count+1;
  delta  = start+count;
  offset = delta+count;
  glyph  = offset+count;                                            // Get address of the glyph array
  j = swap_short(sub->length)/2;                                    // Swap bytes in the sub-table
  for (i = 0; i < j; i++) ((ushort *) sub)[i] = swap_short(((ushort *) sub)[i]);
//
//  If the font is to be printed vertical we will need to change the offsets and change some of 
//  the gylphs for vertical printing.
//
  if (vertical) {
    #include <pshpack1.h>               // Necessary to get the structure alignement correct!
    struct gsub_header {                // Header structure for the GSUB 
      long   version;                   // version (not used)    
      ushort script_list;               // offset to script list (not actually used)
      ushort feature_list;              // offset to the feature list (this is checked)
      ushort lookup_list;               // offset to the lookup list.
    } *header;
    struct feature_element {            // List element for featchers stored in the GSUB
      ulong  tag;                       // Tag.
      ushort offset;                    // Offset in the feature list.
    };
    struct feature_list {               // Features stored in this GSUB
      ushort count;                     // Number of features in the GSUB
      struct feature_element elements;  // List of actual feature elements.
    } *feature_list;
    struct feature {                    // Description of an actual feature parameters
      ushort param;                     // parameters (not used).
      ushort count;                     // Number of indexes (assumed to be 1).
      ushort index;                     // Actual index to feature.
    } *feature;
    struct lookup_list {                // Contains offset to lookup lists
      ushort count;                     // Number of lookup lists (not used, we get index from feature)
      ushort offset;                    // Offset to list
    } *lookup_list;
    struct lookup {                     // Describes a lookup table and process.
      ushort type;                      // lookup type (assumed to be 1 = substitution).
      ushort flags;                     // not used.
      ushort count;                     // Number of lists in the set (assumed to be 1)
      ushort offset;                    // Offset to the first part of the list.
    } *lookup;
    struct sub_table {                  // This describes the replacment glyph list or substituiton table
      ushort format;                    // format (assumed to be 2 = output glyphs)
      ushort offset;                    // offset to the the from (coverage table).
      ushort count;                     // number of entires.
      ushort glyphs;                    // glyph list (fisrt glyph)
    } *sub_table;
    struct coverage {                   // Table indicates what glyphs are to be replaced with substitions
      ushort format;                    // format (assumed to be 1)
      ushort count;                     // number of entries (assumed to match sub-table)
      ushort glyphs;                    // glyph list (first glyph)                       
    } *coverage;
    #include <poppack.h>                // Restore the structure alignment

    j = GetFontData (hdc,GSUB,0,NULL,0);                                // Get isze of the gsub
    if (!(gsub = (char *) malloc(j))) return (true);                    // Allocat space
    GetFontData (hdc,GSUB,0,gsub,j);                                    // Get the gsub.
    header       = (struct gsub_header *) gsub;                         // This is the header location.
    feature_list = ADDRESS(feature_list,gsub,header->feature_list);     // Find the feature list.
    j = swap_short(feature_list->count);                                // Try to find the VERT section
    for (i = 0; (i < j) && (VERT != (&feature_list->elements)[i].tag); i++); 
    if (i >= j) return (true);                                          // No vert then we cannot use this font
    feature     = ADDRESS(feature,feature_list,(&feature_list->elements)[i].offset);        // Get feature for 'vert'
    lookup_list = ADDRESS(lookup_list,gsub,header->lookup_list);                            // This is the base of the lookup tables.
    lookup      = ADDRESS(lookup,lookup_list,(&lookup_list->offset)[swap_short(feature->index)]);   // Select lookup table we need
    sub_table   = ADDRESS(sub_table,lookup,lookup->offset);             // Glyphs that are the substitues.
    coverage    = ADDRESS(coverage,sub_table,sub_table->offset);        // Glyphs that are to be replaced.
    vcount      = swap_short(coverage->count);                          // These are the reall working values.
    from        = &coverage->glyphs;
    to          = &sub_table->glyphs;
    for (i = 0; i < vcount; i++) {                                      // Need to swap the byte order
      from[i] = swap_short(from[i]);
      to  [i] = swap_short(to  [i]);
    }
//
//  Correct the character placement for veritcal printing
//  
    vshift = 0;
  }
#endif WINCE            // Windows CE does not use glyph addressing
//
//  Clean-up and restore the default font.
//
  font = (HFONT) SelectObject(hdc,font);
  return (false);
}

//
//  End Class TRUETYPE_KANJI_font
//
//===================================================================

//===================================================================
//
//  Begin Class JWP_font.
//
//  This class contains information ralted to fonts used by the program,
//  there sises, and other features.  Much of the information contained
//  in this class is simply mainted such that other parts of the 
//  program do not need to recalculate these values.
//

//--------------------------------
//
//  Close the font class and deallocate all resources.
//
void JWP_font::close () {
  if (!duplicate) {
    kanji->remove ();
    if (ascii) DeleteObject (ascii);
  }
  ascii = NULL;
  kanji = NULL;
  return;
}

//--------------------------------
//
//  Generates a copy of the font.  This is used because the system font and the 
//  display font are speparate.  If the user wants to use the same font for each
//  we just copy the font.  Thus we have only one open copy of the font at a
//  time.  
//
//  This routine just duplicates the JFC_font structure and sets the duplicate flag.
//
//      font -- Font to be copied
//
void JWP_font::copy (class JWP_font *font) {
  *this     = *font;
  duplicate = true;
  return;
}

//--------------------------------
//
//  Calculate horizontal advance for a character.
//
//      x      -- Inital horizontal position.
//      ch     -- Character.
//
//      RETURN -- New hornizontal poisiton.
//
int JWP_font::hadvance (int x,int ch) {
  if (ISJIS(ch)) return (x+hwidth);
  if (ch != '\t') return (x+widths[ch]);
  return (((x-x_offset)/hwidth+1)*hwidth+x_offset);
}

//--------------------------------
//
//  This routine opens font system.  This includes a kanji font and an ASCII
//  font of matched size.  All metrics and dimensions are intialized.
//
//      name   -- Name of font.  If the font ends in ".f00" this is assumed
//                to be a bitmapped font.  Otherwise it is assumed to be a 
//                TrueType font.
//      size   -- Height in pixels of the font.
//      cache  -- Non-zero for a chached font.  (Only used for bitmapped fonts.)
//      hdc    -- Display context.  (A value of NULL will assume the main display.)
//      vert   -- Vertical version of the font.
//
//      RETURN -- Non-zero value indicates success.
//
int JWP_font::open (TCHAR *name,int size,int cache,HDC hdc,int vert) {
//
//  Close any open font.
//
  close ();
  duplicate = false;
//
//  Try to open the kanji font.
//
  if (!(kanji = open_font(name,size,cache,hdc,vert))) return (false);
  height   = kanji->height;
  rheight  = kanji->height+1;
  vspace   = height/4;
  hspace   = kanji->width/4;
  hwidth   = kanji->width +  kanji->spacing;
  vheight  = kanji->height+2*kanji->leading;    // This needs to be corrected to get correct line spacing.
  cheight  = vheight-kanji->leading;            // Height of caret (cursor).
  lheight  = vheight;                           // Height of line in a Japanese list box
  loffset  = lheight-height-1;                  // Offset for rendering a line in a list box (for speed this is calcuated here but used only in one place).
  x_offset = hspace;                            // Parameters used JFC_file class for display.
  y_offset = height+vspace;
//
//  Now open the ASCII font
//
  LOGFONT    lf;
  HFONT      tfont;
  int        i;
  memset  (&lf,0,sizeof(lf));
  lstrcpy (lf.lfFaceName,jwp_config.cfg.ascii_font.name);
  lf.lfHeight = -height;
  if (!(ascii = CreateFontIndirect(&lf))) return (false);
//
//  Get font metrics
//  
  hdc = GetDC (main_window);
  tfont = (HFONT) SelectObject (hdc,ascii);
  for (i = 0; i < 256; i++) {                   // Get width occupied by each ASCII
    widths[i] = (short) ascii_width(hdc,i);
  }
  SelectObject (hdc,tfont);
  ReleaseDC (main_window,hdc);
  return (true);
}

//--------------------------------
//
//  Opens an ascii font with a size that matches the kanji font.
//
//      face -- Font face name to be open.
//
HFONT JWP_font::open_ascii (tchar *face) {
  LOGFONT    lf;
  memset  (&lf,0,sizeof(lf));
  lstrcpy (lf.lfFaceName,face);
  lf.lfHeight = -height;
  return  (CreateFontIndirect(&lf));
}

//
//  End Class JWP_font
//
//===================================================================

//===================================================================
//
//  These routines handle the color-kanji list.
//

class COLOR_kanji color_kanji;
static HWND adddel_dialog = NULL;

//--------------------------------
//
//  Convert from index number to JIS
//
//      ch     -- Index number for kanji.
//
//      RETURN -- JIS kanji value.
//
static int index2jis (int ch) {
  int i,j;
  i = ch/94;
  j = ch-(i*94);
  return (0x3021+((i << 8) | j));
}

//--------------------------------
//
//  Convert JIS number in kanji index.
//
//      ch     -- JIS character.
//
//      RETURN -- Index number.
//
static int jis2index (int ch) {
  ch = ch-((int) 0x3021);
  if (ch < 0) return (-1);
  ch = HIBYTE(ch)*94+LOBYTE(ch);
  if (ch > MAX_KANJI) return (-1);
  return (ch);
}

//--------------------------------
//
//  Dialog box procedure for the Add & Delete kanji dialog.
//
static int adddel_proc (HWND hwnd,int msg,WPARAM wParam,LPARAM lParam) {
  int    i,length,start;
  KANJI *kanji;
  switch (msg) {
    case WM_INITDIALOG: 
         add_dialog (hwnd,true);
         adddel_dialog = hwnd;
         SendDlgItemMessage (hwnd,IDC_AKKANJI,JE_LOAD,0,(LPARAM) jwp_file);
         return (true);
    case WM_DESTROY:
         remove_dialog (hwnd);
         adddel_dialog = NULL;
         return (0);
    case WM_COMMAND:        
         switch (LOWORD(wParam)) { 
           case IDOK:           // Add
           case IDSEARCH:       // Message from Jedit box.
           case IDC_AKDELETE:   // Delete
                start  = color_kanji.count();
                length = JEGetDlgItemText(hwnd,IDC_AKKANJI,&kanji);
                if (LOWORD(wParam) == IDC_AKDELETE) {
                  for (i = 0; i < length; i++) color_kanji.remove (kanji[i]);
                  length = color_kanji.count();
                  JMessageBox (main_window,IDS_CK_REMOVE,IDS_CK_TITLE,MB_OK,length,start-length);
                }
                else {
                  for (i = 0; i < length; i++) color_kanji.add (kanji[i]);
                  length = color_kanji.count();
                  JMessageBox (main_window,IDS_CK_APPEND,IDS_CK_TITLE,MB_OK,length,length-start);
                }
                color_kanji.write ();
                DestroyWindow     (hwnd);
                return (0);
           case IDCANCEL:
                DestroyWindow (hwnd);
                return        (0);
         }
  }
  return (false);
}

//--------------------------------
//
//  Add a kanji to the list.  The routine automatically takes care of 
//  duplicated kanji and such.
//
//      ch     -- Character to add.
//
//      return -- Non-zero value indicates the character was valid and
//                could be added.
//
int COLOR_kanji::add (int ch) {
  int i;
  i = jis2index(ch);
  if (i < 0) return (false);
  data[i] = true;
  return (true);
}

//--------------------------------
//
//  Clear the color kanji list.
//
void COLOR_kanji::clear () { 
  memset (&data,0,sizeof(data));
  return;
}

//--------------------------------
//
//  Count number of kanji in the list.
//
int COLOR_kanji::count () {
  int i,j;
  for (i = j = 0; i <= MAX_KANJI; i++) j += data[i];
  return (j);
}

//--------------------------------
//
//  This routine implements the add & delete kanji dialog.
//
void COLOR_kanji::do_adddel () {
  if (adddel_dialog) SetForegroundWindow (adddel_dialog); else JCreateDialog (IDD_ADDCOLORKANJI,main_window,(DLGPROC) adddel_proc);
  return;
}

//--------------------------------
//
//  Determine if a character is in the color-kanji list.
//
//      ch     -- Character to check.
//
//      RETURN -- Non-zero value indicates the kanji is in the list.
//
int COLOR_kanji::in (int ch) {
  int i;
  i = jis2index(ch);
  if (i == -1) return (false);
  return (data[i]);
}

//--------------------------------
//
//  This routine addes the color kanji to the current file.  This is used to 
//  edit the color kanji list.
//
void COLOR_kanji::put () {
  int   i;
  KANJI k;
  for (i = 0; i <= MAX_KANJI; i++) {
    if (data[i]) {
      k = index2jis(i);
      jwp_file->put_string(&k,1);
    }
  }
  return;
}

//--------------------------------
//
//  Read the color kanji list from the standard file.
//
void COLOR_kanji::read () {
  KANJI  kanji;
  HANDLE handle;
  unsigned long done;
  if (INVALID_HANDLE_VALUE != (handle = jwp_config.open(COLORKANJI_NAME,OPEN_READ,true))) {
    while (true) {
      ReadFile (handle,&kanji,sizeof(KANJI),&done,NULL);
      if (done != sizeof(kanji)) break;
      kanji = ((kanji & 0xff00) >> 8) | ((kanji & 0x00ff) << 8);    // Swap byte order to get EUC.
      add (kanji & 0x7f7f);
    }
  }
  CloseHandle (handle);
}

//--------------------------------
//
//  Remove a kanji to the list.  The routine automatically takes care of 
//  duplicated kanji and such.
//
//      ch     -- Character to remove.
//
//      return -- Non-zero value indicates the character was valid and
//                could be removed.
//
int COLOR_kanji::remove (int ch) {
  int i;
  i = jis2index(ch);
  if (i < 0) return (false);
  data[i] = false;
  return (true);
}

//--------------------------------
//
//  Write the color kanji list to the standard file.
//
//      RETURN -- A non-zero return indicates an error writing.
//
int COLOR_kanji::write () {
  int    i;
  KANJI  kanji;
  HANDLE handle;
  unsigned long done;
  if (INVALID_HANDLE_VALUE == (handle = jwp_config.open(COLORKANJI_NAME,OPEN_NEW,true))) {
    QUIET_ERROR ErrorMessage (true,IDS_CK_ERROR,jwp_config.name());
    return (true);
  }
  for (i = 0; i <= MAX_KANJI; i++) {
    if (data[i]) {
      kanji = index2jis(i) | 0x8080;
      kanji = ((kanji & 0xff00) >> 8) | ((kanji & 0x00ff) << 8);    // Swap byte order to get EUC.
      WriteFile(handle,&kanji,sizeof(kanji),&done,NULL);
    }
  }
  CloseHandle (handle);
  return      (false);
}

//--------------------------------
//
//  This routine implements the Utilities/Make Kanji List
//
void JWP_file::do_kanjilist () {
  Paragraph *para;
  int        i,start;
  start = color_kanji.count();
  for (para = first; para; para = para->next) {
    for (i = 0; i < para->length; i++) if (ISKANJI(para->text[i])) color_kanji.add(para->text[i]);
  }
  redraw_all ();
  JMessageBox (main_window,start ? IDS_CK_APPEND : IDS_CK_TEXT,IDS_CK_TITLE,MB_OK,color_kanji.count(),color_kanji.count()-start);
  color_kanji.write ();
  return;
}

//
//  End Class COLOR_kanji
//
//===================================================================

//===================================================================
//
//  Exported routines.
//

//--------------------------------
//
//  Render an ASCII or extneded ASCII character using the currently selected font.
//
//      hdc -- Display context for display.  This must be provied.
//      x,y -- Location in the display of the lower-left corner of the object.
//      ch  -- Character to render.
//  
void ascii_draw (HDC hdc,int x,int y,int ch) {
  static TCHAR text[2] = { 0,0 };
#ifdef WINCE
  text[0] = (TCHAR) jis2unicode(ch);
#else  WINCE
  text[0] = (TCHAR) ch;
#endif WINCE  
  TextOut  (hdc,x,y,text,1);
  return;
}

//--------------------------------
//
//  Determine the pixel width of an ASCII or extended ASCII character.
//
//      hdc    -- Display context for display.  This must be provied.
//      ch     -- Character.  Shold only be ASCII or extended ASCII.
//      
//      RETURN -- Width ofcharacter in pixels using the current display
//  
int ascii_width (HDC hdc,int ch) {
  static TCHAR text[2] = { 0,0 };
  SIZE   s;
#ifdef WINCE
  text[0] = (TCHAR) jis2unicode(ch);
#else  WINCE
  text[0] = (TCHAR) ch;
#endif WINCE  
  GetTextExtentPoint32 (hdc,text,1,&s);
  return (s.cx);
}

//--------------------------------
//
//  This routine deallocates all kanji fonts.  This is called when we are about to exit and 
//  when the user changes the font base, since this will require re-evaluating the font 
//  choics.
//
void free_fonts () {
  sys_font .close ();
  list_font.close ();
  edit_font.close ();
  file_font.close ();
  clip_font.close ();
  bar_font .close ();
  big_kanji  ->remove ();
  jist_kanji ->remove ();
  print_kanji->remove ();
  big_kanji   = NULL;
  jist_kanji  = NULL;
  print_kanji = NULL;
  return;
}

//--------------------------------
//
//  This routine gets the bigest kanji font avilable.  This is ued 
//  for rendering the large kanji in the characrter info dialog.
//
//      rect   -- Rectangle space.  This is used to determine the font size generated 
//                for TrueType fonts.  This is unused if the main font is not a TrueType 
//                font.
//
//      RETURN -- Pointer to the kanji font.  If necessary, this 
//                rotuine will open a new font, but if it cannot find
//                a font to open, it will return the main system font.
//
KANJI_font *get_bigfont (RECT *rect) {
  int i,j;
  j = rect->right-rect->left;
  i = rect->bottom-rect->top;
  if (j < i) i = j;
  if (big_kanji) return (big_kanji);                            // Already have a big font.
  if (!jwp_config.cfg.big_font.automatic) {                     // User has a chosen font.
    if (big_kanji = open_font(jwp_config.cfg.big_font.name,i,true,NULL,false)) return (big_kanji);
  }
  if (file_font.kanji->truetype) {                              // Try to use user's font face.
    if ((big_kanji = open_font(jwp_config.cfg.file_font.name,i,true,NULL,false))) return (big_kanji);
  }
  if (sys_font.kanji->truetype) {                               // Try to use system font face.
    if ((big_kanji = open_font(jwp_config.cfg.sys_font.name,i,true,NULL,false))) return (big_kanji);
  }
  if ((big_kanji = open_bitmap(NAME_BIGFONT,true,false))) return (big_kanji);   // Try 48x48
  if ((big_kanji = open_bitmap(NAME_MEDFONT,true,false))) return (big_kanji);   // Try 24x24
  return (sys_font.kanji);
}

//--------------------------------
//
//  This routine selects a font for use in the JIS Table.  Since the table is based on a 16x16
//  font we will attempt to generate a 16x16 bit version of the current font.
//
//      RETURN -- Pointer to a font to use.  A valid font is always returned.
//
KANJI_font *get_jistfont () {
  if (jist_kanji) return (jist_kanji);                          // Already have a font
  if (!jwp_config.cfg.jis_font.automatic) {                     // User has a chosen font.
    if (jist_kanji = open_font(jwp_config.cfg.jis_font.name,16,true,NULL,false)) return (jist_kanji);
  }
#if 0                                                           // Removed these to default to 
  if (file_font.kanji->height == 16) return (file_font.kanji);  // Try the user's chosen font.
  if (file_font.kanji->truetype) {                              // Try to use user's font face.
    if ((jist_kanji = open_font(jwp_config.cfg.file_font.name,16,true,NULL,false))) return (jist_kanji);
  }
#endif
  if (sys_font.kanji->height == 16) return (sys_font.kanji);    // Try to use the system font.
  if (sys_font.kanji->truetype) {                               // Try to use system font face.
    if ((jist_kanji = open_font(jwp_config.cfg.sys_font.name,16,true,NULL,false))) return (jist_kanji);
  }
  return (sys_font.kanji);
}

//--------------------------------
//
//  This rotuine gets a font that is sutable for printing.  Noramlly, the printer font is 
//  closed every time and reopened.  This allows us to deal with changes made by the user,
//  particuarly if he/she is using a TrueType font and prints at a different printer 
//  resoultion or size, the font will have to change.  For bitmap fonts, this is simply 
//  extra work, but since we are printing it does not really matter.
//
//      hdc      -- Display context for printer.  This must be provied.
//      vertical -- If non-zero this indicates that you want a vertical font (generally for 
//                  printing.  This is actually ignoed by bitmapped fonts, but is used 
//                  by TrueType fonts, where vertical and horizontal fonts actually have 
//                  different characters.
//      
//      RETURN   -- Pointer to font for pritning.
//  
KANJI_font *get_printfont (HDC hdc,int vertical) {
  int size;
  print_kanji->remove ();
  print_kanji = NULL;
  size = (jwp_config.cfg.print_font.size*GetDeviceCaps(hdc,LOGPIXELSY))/720;
  if (!jwp_config.cfg.print_font.automatic) {
    if ((print_kanji = open_font(jwp_config.cfg.print_font.name,size,true,hdc,vertical))) return (print_kanji);
  }
  if (file_font.kanji->truetype) {
    if ((print_kanji = open_font(jwp_config.cfg.file_font.name,size,true,hdc,vertical))) return (print_kanji);
  }
  if ((print_kanji = open_bitmap(NAME_BIGFONT,true,vertical))) return (print_kanji);    // Try 48x48
  if ((print_kanji = open_bitmap(NAME_MEDFONT,true,vertical))) return (print_kanji);    // Try 24x24
  if (file_font.kanji->truetype) {
    if ((print_kanji = open_bitmap(NAME_KANJI,true,vertical))) return (print_kanji);    // Try 16x16;
  }
  return (file_font.kanji);                                                 // Use the standard font.
}

//--------------------------------
//
//  Initialize the fonts structures.
//
int initialize_fonts () {
  HDC        hdc;
  TEXTMETRIC tm;
//
//  May be a reinitalize, so we just close some optional fonts.  When
//  they are used again they will be open with the correct parameters.
//
  free_fonts ();
//
//  Open the system font
//
  if (!sys_font.open(jwp_config.cfg.sys_font.name,jwp_config.cfg.sys_font.size,jwp_config.cfg.cache_displayfont,NULL,false)) {
    if (!(sys_font.open(NAME_KANJI,16,jwp_config.cfg.cache_displayfont,NULL,false))) return (true);    // If we cannot open the user's font
  }                                                                                             //   try the default bitmapped font
//
//  Generate the display font.
//
  auto_font (&jwp_config.cfg.edit_font,&edit_font,& sys_font);
  auto_font (&jwp_config.cfg.list_font,&list_font,&edit_font);
  auto_font (&jwp_config.cfg. bar_font,& bar_font,&edit_font);
  auto_font (&jwp_config.cfg.file_font,&file_font,& sys_font);
  if (jwp_config.cfg.clip_font.automatic || !clip_font.open(jwp_config.cfg.clip_font.name,jwp_config.cfg.clip_font.size,true,NULL,jwp_config.cfg.clip_font.vertical)) clip_font.copy(&file_font);
//
//  Get system font height.
//
  hdc = GetDC (main_window);
  GetTextMetrics (hdc,&tm);                     // Get system font info for later.
  ReleaseDC (main_window,hdc);
  sysfont_height = (short) tm.tmHeight;
//
//  Read in color-kanji list.
//
  color_kanji.read ();
  return (false);
}


// ### May want to implement a binary search on the color-kanji list.












