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
//  This modlue implement most of the clipboard functionaly of the 
//  system.  The clipbard has no class associated witht it, but rather
//  makes use of stuff distrubuted in a number of classes, particuarly
//  Paragraph, and JWP_file.
//
//  The clipboard operation uses a delayed render for the clipboard.  
//  This means that JWPce does not actually place data on the clipboard
//  until some other program request the data.
//
//  Internally data to be placed on the clipboard is stored in a 
//  JWP_file class object pointed to by jwp_clipboard.  Data in this 
//  object is not formatted, but does contain formatting inforamtion 
//  in the paragraph structures.
//
//  When an internal paste occures (whithin this instance), data is 
//  taken directly out of the jwp_clipboard configuration.  When a 
//  post into another program occures, the data is rendered into the 
//  clipboard and imported by the other program.  
//
//  JWPce supports four clipboard formats CF_TEXT, CF_OEMTEST 
//  (identical from our point of view), private format clip_jwpce, and
//  CF_BITMAP.  
//
//  The private format allows different instances of JWPce to share 
//  data without any loss of information.
//

#include "jwpce.h"
#include "jwp_clip.h"
#include "jwp_conf.h"
#include "jwp_file.h"
#include "jwp_font.h"
#include "jwp_jisc.h"
#include "jwp_misc.h"
#include "jwp_stat.h"

//===================================================================
//
//  Special compile time option.
//
//  CLIPBOARD_FORMAT -- Determines extra clipboard formats that are 
//                      supported.  The values CLIPBOARD_FORMAT_BITMAP
//                      CLIPBOARD_FORMAT_NOBITMAP are supported.
//
#ifndef CLIPBOARD_FORMAT        // Make sure this is defined as something.
  #pragma message("ERROR: 'CLIPBOARD_FORMAT' is not defined")
  #if (==0)     // Make an error 
  #endif
#endif  CLIPBOARD_FORMAT

//===================================================================
//
//  Compile-time parameters.
//

#define CLIPBOARD_JWPCE TEXT("JWPce-Clip")  // Name of private clipbard format.

//===================================================================
//
//  Static data.
//

static UINT             clip_jwpce;         // Id of JWPce's private clipboard format.

//===================================================================
//
//  Exported data.
//

JWP_file *jwp_clipboard = NULL;         // Internal representation of the clipboard.

//===================================================================
//
//  Exported routines.
//

//--------------------------------
//
//  Clear the contents of the clipboard image.
//
void clear_clipboard () {
  if (!jwp_clipboard) return;
  delete jwp_clipboard;
  jwp_clipboard = NULL;
  return;
}

//--------------------------------
//
//  This routine processes clipbard messages from the system or other 
//  parts of the program.
//
//      iMsg   -- System message.  Generally these are staight-forward 
//                messages.  The exception is the WM_CREATE message. 
//                This is not a clipboard message and does not get 
//                set here as a responce to the system, but rather is
//                set here to intialize the program.
//      wParam -- The wParam associated with the system message.
//
//  Processing of the WM_DESTROYCLIPBAORD message has been moved back
//  to the main message loop, since this simply calls a global rotoutine.
//
void do_clipboard (int iMsg,WPARAM wParam) {
  switch (iMsg) {
//
//  Generally received from windows when we shut down, this is a 
//  request to render all possible formats.  We process this by calling
//  ourself with each of the possible formats.
//
    case WM_RENDERALLFORMATS:
         OpenClipboard  (main_window);
         do_clipboard   (WM_RENDERFORMAT,clip_jwpce);
#ifndef WINCE
         do_clipboard   (WM_RENDERFORMAT,CF_TEXT);
         do_clipboard   (WM_RENDERFORMAT,CF_OEMTEXT);
#endif  WINCE
         if (!jwp_config.cfg.no_UNICODETEXT) do_clipboard   (WM_RENDERFORMAT,CF_UNICODETEXT);
#if (CLIPBOARD_FORMAT == CLIPBOARD_FORMAT_BITMAP)
         if (!jwp_config.cfg.no_BITMAP)      do_clipboard   (WM_RENDERFORMAT,CF_BITMAP);
#endif CLIPBOARD_FORMAT
         CloseClipboard ();
         return;
//
//  Request from the system to render a specific format so someone 
//  else can paste it.
//
    case WM_RENDERFORMAT:
         long        size;
         HGLOBAL     block;
         IO_cache    cache;
         JIS_convert convert;
         if (!jwp_clipboard) return;
         switch (wParam) {
           default:             //  clip_id  (our private format [a JWP file writen to clipboard])
                cache.output_count ();
                size  = jwp_clipboard->write_jwp_file(&cache);
                block = cache.output_clip (size);
                jwp_clipboard->write_jwp_file (&cache);
                break;
#ifdef WINCE                        // Windows CE only supports UNICODE format (JIS formats are in UNICODE format)
           case CF_UNICODETEXT:     // UNICODE format is supported though JIS_convert.
                convert.output_count ();
                convert.set_type (jwp_config.cfg.clip_write);
                convert.set_size (2);                           // Always using UNICODE format
                size  = jwp_clipboard->export_file(&convert);
                block = convert.output_clip (size);
                convert.set_type (jwp_config.cfg.clip_write);
                convert.set_size (2);                           // Always using UNICODE format
                jwp_clipboard->export_file (&convert);
                break;
#else WINCE                         // Windows NT/95 support all text formats.
           case CF_TEXT:            // Text formats, base on JIS_convert
           case CF_OEMTEXT:
           case CF_UNICODETEXT:     // UNICODE format is supported though JIS_convert.
                convert.output_count ();
                convert.set_type ((wParam == CF_UNICODETEXT) ? FILETYPE_UNICODE : jwp_config.cfg.clip_write);
                size  = jwp_clipboard->export_file(&convert);
                block = convert.output_clip (size);
                convert.set_type ((wParam == CF_UNICODETEXT) ? FILETYPE_UNICODE : jwp_config.cfg.clip_write);
                jwp_clipboard->export_file (&convert);
                break;
#endif WINDCE
#if (CLIPBOARD_FORMAT == CLIPBOARD_FORMAT_BITMAP)
           case CF_BITMAP:          // BITMAP format for import into Word, etc.
                block = jwp_clipboard->export_bitmap();
                break;
#endif CLIPBOARD_FORMAT
         } 
         SetClipboardData (wParam,block);
         return;
//
//  Not a request from the system, but rather a message from JWPce during
//  startup, this tells us to register our private clipboard class.
//
    case WM_CREATE:
         clip_jwpce = RegisterClipboardFormat(CLIPBOARD_JWPCE);
         return;
  }
  return;
}

//--------------------------------
//
//  Correctly release the data structure allocated with get_paste.
//
//      paste -- JWP_file object allocated with get_paste().
//
void free_paste (JWP_file *paste) {
  if (paste != jwp_clipboard) delete paste;
  return;
}

//--------------------------------
//
//  This routine gets the clipboard data (from whatever the soruce is) and
//  converts it into a JWP_file class object.  A pointer to the object is
//  returned.  Note that if you call this you must delete the object returned.
//
//  In the case that the clipbard data cannot be read, an error message 
//  will be generated.
//
//      hwnd   -- Pointer to client's window, in case of an error.
//                passing a value of NULL will supress any error messages.
//
//      RETURN -- Pointer to clipboard data.  
//  
JWP_file *get_paste (HWND hwnd) {
  JWP_file *paste;
  if (jwp_clipboard) paste = jwp_clipboard;         // Internal data.
    else {
      if ((paste = new JWP_file(0))) {              // External data.
        OpenClipboard (main_window);
        if (paste->import_clip()) { delete paste; paste = NULL; }
        CloseClipboard ();
      }
    } 
  if (!paste && hwnd) JMessageBox (hwnd,IDS_ERROR_CLIPTEXT,IDS_ERROR_CLIPTITLE,MB_OK | MB_ICONEXCLAMATION);
  return (paste);
}

//===================================================================
//
//  Begin Class Paragraph.
//
//  Part of the implementation of the class paragraph.  This routine
//  is specialized to clipboard fucntions and thus has been moved 
//  to this modlue.
//

//--------------------------------
//
//  Transfer's this paragraph to the end of the clipboard.
//
//      start  -- Starting location within the paragraph.
//      end    -- Ending location wihtin the paragraph (-1 indicates end of paragraph)
//
//      RETURN -- A non-zero value indicates a memory allocation error.
//
int Paragraph::add_to_clip (int start,int end) {
  if (end == -1) end = length;
  if (jwp_clipboard->new_paragraph(jwp_clipboard->last)) return (true);
  jwp_clipboard->last->copy_format (this);
  if (start) jwp_clipboard->last->indent_first = 0;     // Needed to make sence when we export the clip.
  return (jwp_clipboard->last->set_text(text+start,end-start));
}

//
//  End Class Paragraph.
//
//===================================================================

//===================================================================
//
//  Begin Class JWP_File.
//
//  Parts of the class JWP_file associated with the clipboard.
//

//--------------------------------
//
//  Minimalistic file constructor used for jwp_clipboard and temp 
//  JWP_file objects.
//
//      format_width -- This is the width used to format the file 
//                      if formatting is requried.  Generally formatting
//                      is only used to support the bitmap clipboard 
//                      format.
//
JWP_file::JWP_file (int format_width) {
  memset (this,0,sizeof(JWP_file));     // Clear everything.
  filetype       = FILETYPE_WORK;       // This file type blocks redraw & title commands.
//first          = last = NULL;
//name           = NULL;
//undo           = NULL;
//sel.type       = SELECT_NONE;
  char_pagewidth = format_width;        // Used in geneating the bitmap clipboard format.
  return;
}

//--------------------------------
//
//  This routine implements a paste from the clipboard to the current 
//  cursor location.
//
//  Clipboard data is first obtained in a JWP_file object called paste.
//  If the data is internal, this object will simply point to 
//  jwp_clipboard.  If the data is external, this object will contain 
//  a duplicate of the system clipboard image.
//
//      errors -- If non-zero errors will be reported.
//
void JWP_file::clip_paste (int errors) {
  JWP_file *paste;
  Paragraph *para;
  undo_start ();
  if (sel.type == SELECT_EDIT) selection_delete ();             // Pasting into exising selection.
  selection_clear ();
  if (!(paste = get_paste (errors ? window : null))) return;    // Get data to paste.
//
//  Single paragraph paste.
//
  if (paste->first == paste->last) {
    undo_para  (UNDO_QUE);
    put_string (paste->first->text,paste->first->length);
  }
//
//  Extended object paste.
//
  else {
    do_key    (VK_RETURN,false,false);
    do_key    (VK_LEFT,false,false);
    undo_para (UNDO_QUE);
    cursor.para->ins_string (this,cursor.line,cursor.line->length,paste->first->text,paste->first->length);
    for (para = paste->first->next; para != paste->last; para = para->next) {
      if (new_paragraph(cursor.para)) { undo_end(); return; }
      cursor.para = cursor.para->next;
      if (cursor.para->copy_para(para)) { undo_end(); return; }
      cursor.para->format (this,NULL,true);
    }
    cursor.para = cursor.para->next;
    cursor.line = cursor.para->first;
    cursor.pos  = 0;
    if (paste->filetype == FILETYPE_NORMAL) cursor.para->copy_format (paste->last);
    put_string (paste->last->text,paste->last->length);
  }
//
//  Memory clean up.
//
  undo_end   ();
  free_paste (paste);
  return;
}

//--------------------------------
//
//  Implements a copy to the clipbaord format he current file.  Here, 
//  data is copped to the internal cliboard and windows is notifiled 
//  that the data is ready when it is requested.
//
//  If no data is slected when this routine is called, the current 
//  contents ofthe clipbard will be erased.
//
//      RETURN -- Return is a pointer to the cliboard.  This is used
//                by some controls to write directly into the cliboard
//
JWP_file *JWP_file::clip_copy () {
  Paragraph *para;
  OpenClipboard   (main_window);
  EmptyClipboard  ();
  clear_clipboard ();             
//
//  This section is invoided when doing a cut/copy from a list.  This
//  generates a blank clipboard file with a cursor location and a single
//  paragraph.  This is compatable with the insert function used by 
//  the list managers.
//
  if (!this) {
    if ((jwp_clipboard = new JWP_file(jwp_file->char_pagewidth))) {
      jwp_clipboard->new_paragraph(NULL);
      jwp_clipboard->cursor.para = jwp_clipboard->first;
      jwp_clipboard->cursor.line = jwp_clipboard->first->first;            
    }
  }
//
//  We get here if the user has selected text.  This will generate a 
//  blank clipboard file only if the user has makred text.
//
  else if (sel.type) {
    if (!(jwp_clipboard = new JWP_file(char_pagewidth))) OutOfMemory (window); 
    else {
      all_abs ();
      if (sel.pos1.para == sel.pos2.para) {
        if (sel.pos1.para->add_to_clip (sel.pos1.pos,sel.pos2.pos)) goto CleanUp;
        jwp_clipboard->first->indent_first = 0;         // Want to block indent on export.
      }
      else {
        if (sel.pos1.para->add_to_clip(sel.pos1.pos,-1)) goto CleanUp;
        for (para = sel.pos1.para->next; para != sel.pos2.para; para = para->next) {
          if (para->add_to_clip(0,-1)) goto CleanUp;
        }
        if (sel.pos2.para->add_to_clip(0,sel.pos2.pos)) goto CleanUp;
      }
      all_rel ();
    }
  }
//
//  Export clipboard formats.
//
  SetClipboardData (clip_jwpce    ,null);
  if (!jwp_config.cfg.no_UNICODETEXT) SetClipboardData (CF_UNICODETEXT,null);
#ifndef WINCE
  SetClipboardData (CF_TEXT       ,null);
  SetClipboardData (CF_OEMTEXT    ,null);
#endif  WINCE
#if (CLIPBOARD_FORMAT == CLIPBOARD_FORMAT_BITMAP)
  if (!jwp_config.cfg.no_BITMAP) SetClipboardData (CF_BITMAP     ,null);
#endif CLIPBOARD_FORMAT
CleanUp:
  CloseClipboard ();
  return (jwp_clipboard);
}

//--------------------------------
//
//  This little routine controls the activation of the Edit/Cut and Edit/Copy
//  Menu items.  Bascially, they are enabled when there is a selected text.
//
void JWP_file::edit_menu () {
  if (filetype == FILETYPE_EDIT) return;
  EnableMenuItem (hmenu,IDM_EDIT_COPY,sel.type ? MF_ENABLED : MF_GRAYED);
  EnableMenuItem (hmenu,IDM_EDIT_CUT ,sel.type ? MF_ENABLED : MF_GRAYED);
#ifndef WINCE
  jwp_tool.enable (IDM_EDIT_COPY,sel.type ? true : false);
  jwp_tool.enable (IDM_EDIT_CUT ,sel.type ? true : false);
#endif  WINCE
  return;
}

//--------------------------------
//
//  This routine exports the clipbard data as a bitmap format.  This
//  allows Japanese text to be imported into a standard word processor
//  such as Word or WordPerfect. 
//
//      RETURN -- Global memory pointer to the allocated bitmap.
//
#if (CLIPBOARD_FORMAT == CLIPBOARD_FORMAT_BITMAP)

HGLOBAL JWP_file::export_bitmap () {
  int        x,y,i,j;
  Paragraph *para;          // Current paragraph
  Line      *line;          // Current line.
  HDC        hdc;           // Display Context for main window.
  HDC        hdcmem;        // Display Context for bitmap in memory.
  HBITMAP    hbitmap;       // Actuall bitmap
  RECT       rect;          // Bonding rectangle for the blitmap
  HFONT      font;          // Holds context font so we can restore.
//
//  Need to setup the parameters in the clipboard so we can use the 
//  standard routines.
//
  cursor.para    = view_top.para = first;
  cursor.line    = view_top.line = first->first;            
  cursor.pos     = view_top.pos  = 0;
  adjust ();
//
//  Determine the heigh and width of the bitmap required.
//
  x = y = 0;
  for (para = first; para; para = para->next) {
    para->format (this,NULL,false); // Data placed in the clipboard is unformatted!
    for (line = para->first; line; line = line->next) {
      y += clip_font.vheight;
      for (j = clip_font.x_offset, i = 0; i < line->length; i++) j = clip_font.hadvance(j,para->text[line->first+i]);
      j -= clip_font.x_offset;
      if (j > x) x = j;
    }
  }
  y -= 2*clip_font.kanji->leading;                  // Remove extra vertical space allocated for each line.
//
//  Make memory bitmap
//
  hdc     = GetDC(main_window);
  hdcmem  = CreateCompatibleDC(hdc);
  if (!jwp_config.cfg.colorkanji_bitmap || !jwp_config.cfg.colorkanji_mode) {
    hbitmap = CreateBitmap(x,y,1,1,NULL);           // No color kanji so just make a monocrome
  }                                                 //   bitmap.  
  else {                                        
    hbitmap = CreateCompatibleBitmap (hdc,x,y);     // Color mode so make color bitmap.
  }
  font    = (HFONT) SelectObject (hdcmem,clip_font.ascii);
  SelectObject(hdcmem,hbitmap);
//
//  Fill background with white.
//
  rect.left   = 0;
  rect.top    = 0;
  rect.right  = x;
  rect.bottom = y;
  FillRect (hdcmem,&rect,(HBRUSH) GetStockObject(WHITE_BRUSH));
//
//  Render bitmap.
//
  y                  = clip_font.height;        // Vertical start location.
  x                  = clip_font.x_offset;      // Save horizonal border
  clip_font.x_offset = 0;                       // Set to zero for rending
  for (para = first; para; para = para->next) {
    for (line = para->first; line; line = line->next) {
      draw_line (hdcmem,para,line,y,rect.left,rect.right,&clip_font);
      y += clip_font.vheight;
    }
  }
  clip_font.x_offset = x;                       // Restore default x border.
//
//  Clean up and exit.
//
  SelectObject (hdcmem,font);
  DeleteDC     (hdcmem);
  ReleaseDC    (main_window,hdc);
  return       (hbitmap);
}
#endif CLIPBOARD_FORMAT

//--------------------------------
//
//  This routine actually goes about the process of reading from the 
//  clipboard.  This is used when data is coming form another instance
//  of JWPce, or from another program entirally.
//
//      RETURN -- Non-zero return indicates an error.
//
int JWP_file::import_clip () {
  int         type;
  HGLOBAL     block;
  IO_cache    cache;
  JIS_convert convert;
//
//  Attempt to get private data format.
//
  if ((block = GetClipboardData(clip_jwpce))) {
    cache.input_clip (block);
    read_jwp_file    (&cache);
    cache.input_end  ();
    return (false);
  }
//
//  Windows CE Clipboard function only reads UNICODETEXT The actual 
//  text read may be encouded as any of the types, but we are assuming
//  that the clipboard data is basically unicode format, even though for 
//  JIS codes only ascii values will be present.
//
#ifdef WINCE
  if (block = GetClipboardData(CF_UNICODETEXT)) {
    convert.input_clip (block);
    convert.set_type   (jwp_config.cfg.clip_read);
    convert.set_size   (2);                                 // Always using UNICODE format
    if (jwp_config.cfg.clip_read == FILETYPE_AUTODETECT) {  // Import format is autodetect, so see what we have.
      type = convert.find_type ();
      convert.rewind     ();
      convert.set_type   (type);
      convert.set_size   (2);                               // Always using UNICODE format
    }
    import_file       (&convert);
    convert.input_end ();
    return (false);
  }
//
//  Attempt to read unicode data.
//
#else WINCE
  if (((jwp_config.cfg.clip_read == FILETYPE_UNICODE   ) || 
       (jwp_config.cfg.clip_read == FILETYPE_AUTODETECT)) && 
       (block = GetClipboardData(CF_UNICODETEXT))) {
    convert.input_clip (block);
    convert.set_type   (jwp_config.cfg.clip_read);
    if ((jwp_config.cfg.clip_read == FILETYPE_UNICODE) || convert.is_unicode()) {
      convert.rewind     ();
      convert.set_type   (FILETYPE_UNICODE);
      import_file        (&convert);
      convert.input_end  ();
      return             (false);
    }
  }
//
//  Attempt to read one of the text formats.  This implies data is EUC/JIS/Shift-JIS/UTF/etc.
//
  if ((block = GetClipboardData(CF_TEXT)) || (block = GetClipboardData(CF_OEMTEXT))) {
    convert.input_clip (block);
    convert.set_type   (jwp_config.cfg.clip_read);
    if (jwp_config.cfg.clip_read == FILETYPE_AUTODETECT) {  // Import format is autodetect, so see what we have.
      type = convert.find_type ();
      convert.rewind     ();
      convert.set_type   (type);
    }
    import_file       (&convert);
    convert.input_end ();
    return (false);
  }
#endif WINCE
//
//  Error!
//
  return (true);        // Could not find an import format that we understand!
}

//
//  End Class JWP_file.
//
//===================================================================


