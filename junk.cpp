static void BackFillRect (HDC hdc,RECT *rect);          // Fills a rectangle with the current background color.
                                                        //   This used to be done with direct calls to FillRect,
                                                        //   with color arguments, however, Windows CE does not 
                                                        //   correctly support these calls, thus I replaced the 
                                                        //   calls with this routine.

//
//  This is a small routine to fill a rectange with the windows current
//  background color.  This was previously done by making direct calls 
//  to FillRect() with color arguments.  Windows CE, however, does not
//  correctly interpret these calls, thus a change in the rotuines was
//  required.
//
//      hdc  -- Display context to render into.
//      rect -- Rectangle to be filled.
//
static void BackFillRect (HDC hdc,RECT *rect) {
  HBRUSH brush;
  brush = CreateSolidBrush(GetBkColor(hdc));
  FillRect (hdc,rect,brush);
  DeleteObject (brush);
  return;
}

void JWP_file::redraw_line (Paragraph *para,Line *line,int down) {
  RECT     rect;
  Position pos;
  pos.para = para;
  pos.line = line;
  find_pos (&pos,POS_VERT);
  rect.left   = 0;
  rect.right  = width;
  rect.top    = pos.y-view_top.y-jwp_font.rheight;
  rect.bottom = down ? 10000 : (rect.top+(para->spacing*jwp_font.vheight)/100);
  InvalidateRect (window,&rect,REDRAW_BLANK);
  return;
}

#if 0
void JWP_file::redraw_range (Paragraph *para1,Line *line1,Paragraph *para2,Line *line2) {
  RECT rect;
  Position pos1,pos2;
  pos1.para = para1;
  pos1.line = line1;
  pos2.para = para2;
  pos2.line = line2;
  find_pos (&pos1,POS_VERT);
  find_pos (&pos2,POS_VERT);
  rect.left   = 0;
  rect.right  = width;
  rect.top    = pos1.y-view_top.y-jwp_font.rheight;
  rect.bottom = pos2.y-view_top.y-jwp_font.rheight+(para2->spacing*jwp_font.vheight)/100;
  InvalidateRect (window,&rect,REDRAW_BLANK);
  return;
}
#endif

static int euc_to_jis (KANJI *jis,byte *euc);   // Convert EUC->JIS codes
//
//  This utiltiy routine converts an ECU string into a JIS encoded kanji 
//  string.  This is used prinarally to read data from list-boxes.
//
// ### Currently this routine is only used in one place, so we may want 
// ###   to embled in in the JWP_file::put_euc() routine.
//
//      jis    -- JIS encoded return buffer.
//      euc    -- NULL terminated euc encoded strings.
//
//      RETURN -- Length of the resulting JIS string.
//
static int euc_to_jis (KANJI *jis,byte *euc) {
  int ch,count = 0;
  for (; *euc; euc++) {
    if (!(*euc & 0x80)) jis[count++] = *euc;
      else {
        ch = *euc++;
        jis[count++] = ((ch << 8) | *euc) & 0x7f7f;
      }
  }
  return (count);
}


//
//  This routine is used to insert EUC encoded strings.  Primarally 
//  these are used in list boxes, but they also have other uses.
//  Passing a parameter of NULL as the string, this will cause a new
//  paragraph to be generated.  This is used by the string procesisng
//  routines to insert strings into different lines.
//
//      euc -- Poitner to EUC string.
//
void JWP_file::put_euc (byte *euc) {
  KANJI buffer[SIZE_BUFFER];
  int   length;
  selection_clear ();
  if (!euc) {                               // NULL -> New paragraph.
    if (new_paragraph(cursor.para)) return;
    cursor.para = cursor.para->next;
    cursor.line = cursor.para->first;
    cursor.pos  = 0;
  }
  else {                                    // Standard insert.
    length = euc_to_jis(buffer,euc);
    put_string (buffer,length);
  }
  return;
}


//
//  This routine is used by all selection type list boxes to get the 
//  contents of the list box and insert them into the current file.
//  This is used by all the routines that process list-boxes.
//
//  The main job here is to move through the list and copy over each 
//  selected item one at a time.
//
//      file -- JWP_file object we are going to put data into.
//      list -- Pointer to the widnow containning the list.
//
void list_insert (JWP_file *file,HWND list) {
  byte buffer[SIZE_BUFFER],*ptr;
  int i,j;
#ifdef FOLLOW_BY_SPACE      // This option is a slight change in the paste-back formating.
  byte space[3] = { 0x80|0x21,0x80|0x21,0x00 };
#else
  byte space[2] = "\t";
#endif FOLLOW_BY_SPACE
  int new_para   = (file->filetype == FILETYPE_EDIT) ? false : jwp_config.cfg.paste_newpara;
  int need_space = false;       // This indicates that we need to add a spapce before adding 
                                //   the next line.  If the previous line ends with a space or
                                //   the next line begins with a space we do not need to add
                                //   one. 
  j = SendMessage(list,JL_GETCOUNT,0,0);                        // Get number of items is list.
  for (i = 0; i < j; i++) if (SendMessage(list,JL_GETSEL,i,0)) break;
  if (i == j) { MessageBeep (MB_ICONASTERISK); return; }        // Nothing selected.
  file->undo_start ();                                          // Allow this to be undone
  file->undo_para  (UNDO_QUE);  
  for (i = 0; i < j; i++) {
    if (SendMessage(list,JL_GETSEL,i,0)) {                      // If selected need to process it.
      SendMessage(list,JL_GETLINE,i,(long) buffer);             // Get line.
      ptr = buffer;
      if (buffer[0] == EUC_HIGHLIGHT) ptr++;                    // KLUDGE: first byte is 1 this is a color code so skip it.
      if (*ptr == '\t') ptr++;                                  // If line begins with tab remove it.
        else {                                                  // We have line not beginning with a tab.  This means that 
          if (need_space && new_para) {                         //   it is the beginning of an entry.  If the user wants 
            file->put_euc (NULL);                               //   entries on different lines then we need to generate 
            need_space = false;                                 //   paragraphs for it.  If a new para we clear the need_space.
          }                                                     //   The entrance check for need_space makes sure that we 
        }                                                       //   do not make a new paragraph on the first line.
      if (*ptr == ' ') need_space = false;                      // If line begins with space we don't need one
      if (need_space) file->put_euc(space);                     // Need a space so put the space string.
      file->put_euc(ptr);                                       // Put line.
      need_space = (ptr[strlen((char *) ptr)-1] != ' ');        // If line does not end with space we will need on on the next line.
    }
  }
  file->undo_end ();
  return;
}



//
//  This routine implements the windows messages associated with user
//  rendering of a list box.  I have implemented this is a very simple
//  way.
//
//  Normally, this is called at the end of your dailog box procedure 
//  to process the four messages indicated.
//
LONG list_proc (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  RECT rect;
//
//  I do not support sorting of these list items, since they may contain
//  kanji.  Sorting by character number would make no sense, we would 
//  need to sort on reading, but that is very difficult, so we just 
//  don't sort (so we actually do not sort, but rather let the default
//  window proc return zero).
//
  switch (message) {
#if 0
    case WM_COMPAREITEM:
         COMPAREITEMSTRUCT *comp;
         int  i;
         byte buffer2[SIZE_BUFFER];

         comp = (COMPAREITEMSTRUCT *) lParam;
         SendMessage(comp->hwndItem,LB_GETTEXT,comp->itemID1,(long) buffer);
         SendMessage(comp->hwndItem,LB_GETTEXT,comp->itemID2,(long) buffer2);
         for (i = 0; buffer[i] && buffer2[i]; i++) {
           if (buffer[i] != buffer2[i]) break;
         }
         return (buffer[i]=buffer2[i]);
#endif
//
//  We have to do not have to do any work when an item is deleted.
//
    case WM_DELETEITEM:
         return (true);
//
//  This is the big one, the renderign routine.
//
    case WM_DRAWITEM: {
           static TCHAR temp[2] = {0,0};        // Buffer for processing ascii sctrings
           DRAWITEMSTRUCT *draw;                // Indicates what to draw.
           COLORREF        oldcolor;            // Saves the color so we can restore it after changing.
           HFONT           tfont;               // Device context old font (we will change to the user font).
           byte            buffer[SIZE_BUFFER]; // Line buffer.
           byte           *ptr;                 // Current location for rendering int buffer.
           int             x,y;                 // Cursor location for draw.
           KANJI           ch;                  // Used to construct a kanji cahracter.
//
//  Setup rendering parameters.
//         
           draw = (DRAWITEMSTRUCT *) lParam;          
           if (draw->itemID == -1) return (true);
           SetBkMode (draw->hDC,TRANSPARENT);
           tfont = SelectObject (draw->hDC,jwp_font.font);
//
//  Render background (if we have focus, we want this to have a border).
//
           if (draw->itemState & ODS_FOCUS) {
             Rectangle (draw->hDC,draw->rcItem.left,draw->rcItem.top,draw->rcItem.right,draw->rcItem.bottom);
           }
           else {
             FillRect (draw->hDC,&draw->rcItem,(HBRUSH) (COLOR_WINDOW+1));
           }
//
//  Render actual text
//
           SendMessage(draw->hwndItem,LB_GETTEXT,draw->itemID,(long) buffer);
           x = jwp_font.x_offset;
           y = draw->rcItem.top-(jwp_font.vheight-jwp_font.height)/2;
           ptr = buffer;                                                    // This is a real KLUDGE, we put a value of 
           if (*ptr != EUC_HIGHLIGHT) oldcolor = -1;                        //   1 in the front of a line in order to 
             else {                                                         //   change the color of the line.  This 
               oldcolor = SetTextColor(draw->hDC,jwp_config.cfg.info_color);//   works, and allows the  system to work  
               ptr++;                                                       //   correctly.  
             }                                                                
           for (; *ptr; ptr++) {
             ch = *ptr;
             if (*ptr & 0x80) {
               ch = ((ch << 8) | *(++ptr)) & 0x7f7f;
               kanji.draw (draw->hDC,ch,x,y+jwp_font.vheight);
             }
             else if (ch != '\t') {
               temp[0] = (char) ch;
               TextOut (draw->hDC,x,y,temp,1);
             }
             x = jwp_font.hadvance(x,ch);
           }
           if (oldcolor!=-1) SetTextColor(draw->hDC,oldcolor);  // Finish for KLUDGE, restore the color.
//
//  If object is selected invert the rectangle. and restore the system font.
//
           if (draw->itemState & ODS_SELECTED) InvertRect (draw->hDC,&draw->rcItem);
           SelectObject (draw->hDC,tfont);
         }
         return (true);
//
//  We treat the objects as fixed size.
//
    case WM_MEASUREITEM:
         MEASUREITEMSTRUCT *measure;

         measure = (MEASUREITEMSTRUCT *) lParam;
         GetClientRect (GetDlgItem(hwnd,wParam),&rect);
         measure->itemHeight = jwp_font.vheight;
         measure->itemWidth  = rect.right;
         return (true);
  }
  return (true);
}




extern void static_rect (HWND hwnd,int id,RECT *rect,int dialog);             // Get coordiantes of static window.
//
//  This routine retreves the bounding rectangle for a static window 
//  contained within a dialog box.  This is used in the options dialog
//  and in the character-info dialog.
//
//      hwnd   -- Dialog box window pointer.
//      id     -- ID for the static control within the window.
//      rect   -- On exit this will contain the static window coordinates
//                relative to the dialog box window.
//      dialog -- Non-zer if this is a real dialog box.  This is because
//                the dialog boxes used in the options dialog are 
//                different.
//
//  There should be a better way to do this, but beats me.
//
void static_rect (HWND hwnd,int id,RECT *rect,int dialog) {
  RECT dlg;
  GetWindowRect (GetDlgItem(hwnd,id),rect);
  GetWindowRect (hwnd,&dlg);
  rect->left   -= dlg.left;
  rect->right  -= dlg.left;
  rect->top    -= dlg.top;
  rect->bottom -= dlg.top;
  if (dialog) {
    rect->top    -= GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYDLGFRAME);
    rect->bottom -= GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYDLGFRAME);
    rect->left   -= GetSystemMetrics(SM_CXDLGFRAME);
    rect->right  -= GetSystemMetrics(SM_CXDLGFRAME);
  }
  return;
}



#if 0
//
//  Setup first pages.
//
  static TabPage pages[] = {
    { TEXT("General"          ),IDD_OPTGENERAL ,(DLGPROC) options_general  },
    { TEXT("Display"          ),IDD_OPTDISPLAY ,(DLGPROC) options_display  },
    { TEXT("Font/Format/Print"),IDD_OPTFONT    ,(DLGPROC) options_font     },
    { TEXT("File/Clipboard"   ),IDD_OPTFILE    ,(DLGPROC) options_file     },
    { TEXT("Misc"             ),IDD_OPTMISC    ,(DLGPROC) options_misc     },
    { TEXT("Advanced"         ),IDD_OPTADVANCED,(DLGPROC) options_advanced },
  };
  static TabSetup setup = { pages,sizeof(pages)/sizeof(TabPage),0,NULL };
  int i;

  struct cfg new_config;
//
//  Setup configuration variable.
//
  new_config = jwp_config.cfg;
  cfg        = &new_config;
  font       = jwp_font.open_ascii(jwp_config.cfg.font);
//
//  Do the actual dialog
//
  i = TabDialog(IDD_TEST,&setup);
  DeleteObject (font);
  if (i) return;
//
//  Now apply the changes.
//
  jwp_config.set (cfg);
#else
static BOOL CALLBACK tab_dialog (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  HWND      page,tab;       // Window pointer for page and tab control.
  TabSetup *setup;          // Pointer to setup structure.
  TC_ITEM   item;           // Item structure to access data in the tab control.
  RECT      dlg,rect;       // Rectangles for the dialog box and the tab control (and it's window)
  int       i;              
  tab = GetDlgItem(hwnd,IDC_TABCONTROL);
  switch (msg) {
    case WM_INITDIALOG:
         setup = (TabSetup *) lParam;
         SetWindowLong (hwnd,DWL_USER,lParam);
         GetWindowRect (hwnd,&dlg);
         dlg.top    += GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYDLGFRAME);
         dlg.bottom += GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYDLGFRAME);
//       dlg.left   += GetSystemMetrics(SM_CXDLGFRAME);
//       dlg.right  += GetSystemMetrics(SM_CXDLGFRAME);
         memset (&item,0,sizeof(item));
         item.mask = TCIF_TEXT | TCIF_PARAM;
         for (i = 0; i < setup->count; i++) {
           page = CreateDialog (instance,MAKEINTRESOURCE(setup->pages[i].id),hwnd,setup->pages[i].procedure);
           item.pszText = setup->pages[i].text;
           item.lParam  = (LPARAM) page;
           TabCtrl_InsertItem (tab,i,&item);
           GetWindowRect      (tab,&rect);
           TabCtrl_AdjustRect (tab,false,&rect);
           MoveWindow         (page,rect.left-dlg.left,rect.top-dlg.top,rect.right-rect.left,rect.bottom-rect.top,true);
           ShowWindow         (page,SW_HIDE);        
         }
         TabCtrl_SetCurSel (tab,setup->page);
         goto SetPage;
    case WM_NOTIFY:
         switch (((LPNMHDR) lParam)->code) {
           case TCN_SELCHANGING:
                item.mask = TCIF_PARAM;
                TabCtrl_GetItem(tab,TabCtrl_GetCurSel(tab),&item);
                ShowWindow ((HWND) (item.lParam),SW_HIDE);
                break;
           case TCN_SELCHANGE:
SetPage:;
                item.mask   = TCIF_PARAM;
                setup       = (TabSetup *) GetWindowLong(hwnd,DWL_USER);
                i           = TabCtrl_GetCurSel(tab);
                setup->page = i;
                TabCtrl_GetItem (tab,i,&item);
                ShowWindow (page = ((HWND) (item.lParam)),SW_SHOW);
                SetFocus   (GetNextDlgTabItem(page,NULL,false));
                break;                
         }
         return (false);
    case WM_COMMAND:
         switch (LOWORD(wParam)) { 
           case IDOK:
                item.mask = TCIF_PARAM;
                setup     = (TabSetup *) GetWindowLong(hwnd,DWL_USER);
                for (i = 0; i < setup->count; i++) {
                  TabCtrl_GetItem (tab,i,&item);
                  SendMessage ((HWND) (item.lParam),WM_GETDLGVALUES,0,0);
                }
           case IDCANCEL:
                EndDialog (hwnd,LOWORD(wParam != IDOK));
                return    (true);
         }
  }
  return (false);
}

int TabDialog (int id,TabSetup *setup) {
  return (DialogBoxParam (instance,MAKEINTRESOURCE(id),main_window,(DLGPROC) tab_dialog,(LONG) setup));
  return (0);
}


#if 0
static int print_width (KANJI *kanji,int length,PrintContext *pc,int *widths) {
  int ch,i,x;

  for (x = i = 0; i < length; i++) {
    ch = kanji[i];
    if (ISJIS(ch)) x += pc->kwidth;
    else if (ch != '\t') x += widths[ch];
    else x = (x/pc->kwidth+1)*pc->kwidth;
  }
  return (x);
}
#endif


static int kanji_len (KANJI *kstring) {
  int i;
  if (!kstring) return (0);
  for (i = 0; kstring[i]; i++);
  return (i);
}

static KANJI *kanji_dup (KANJI *kstring,int len = -1) {
  KANJI *dup;
  if (!kstring) return (NULL);
  if (len == -1) len = kanji_len(kstring);
  if (!(dup = (KANJI *) calloc(len+1,sizeof(KANJI)))) return (NULL);
  CopyMemory (dup,kstring,len*sizeof(KANJI));
  return (dup);
}

static void put_kstring (HWND hwnd,int id,KANJI *kanji) {
  int i;
  i = kanji_len(kanji);
  SendDlgItemMessage (hwnd,id,EM_REPLACESEL,i,(LPARAM) kanji);
  return;
}

static void get_kstring (HWND hwnd,int id,KANJI **kanji) {
  int    i;
  KANJI *k;
  i =           SendDlgItemMessage(hwnd,id,EM_LINELENGTH,0,0);
  k = (KANJI *) SendDlgItemMessage(hwnd,id,EM_GETLINE   ,0,0);
  if (*kanji) free (*kanji);
  *kanji = NULL;
  if (!i) return;
  *kanji = kanji_dup(k,i);
  return;
}



#if 0
//
//  Set configuration value to default.
//
void JWP_config::reset () {
  config
  cfg.magic           = CONFIG_MAGIC;       // Set magic value for write.
  cfg.install         = true;               // Process install (check program & extensions)
  cfg.maximize        = false;              // Auto-maximiaze the window.
  cfg.usedims         = false;              // Use stored widnow dimensions or let widnows size window.
  cfg.save_exit       = true;               // Save configuration on exit.
  cfg.reload_files    = true;               // Reload files next time.
  cfg.status          = true;               // Status bar.
  cfg.statabove       = false;              // Status bar goes below kanji bar.
  cfg.alloc           = 40;                 // Allocation unit size for lines (in KANJI).
  cfg.convert_size    = 200;                // Size of user conversion dicitionary (in entries).
  cfg.dynamic_width   = true;               // Use dynamic width;
  cfg.char_width      = 35;                 // Width of text in kanji.  Zero results in auto size.
  cfg.vscroll         = true;               // Text window vertical scroll bar.
  cfg.hscroll         = true;               // Text window horizontal scroll bar.
  cfg.kscroll         = true;               // Kanji bar scroll bar.
  cfg.confirm_exit    = true;               // Require confirmation of exit on closing last file.
  cfg.close_does_file = true;               // Window close control, closes just current file.
  cfg.backup_files    = true;               // Save old copy of files as a backup.
  cfg.clip_write      = FILETYPE_EUC;       // Clipboard write type.
  cfg.clip_read       = FILETYPE_AUTODETECT;// Clipboard read type.
  cfg.search_nocase   = false;              // Search without case.
  cfg.search_jascii   = false;              // Search JASCII = ASCII
  cfg.search_back     = false;              // Search forward.
  cfg.search_wrap     = false;              // Search wrap at the end of the file.
  cfg.search_all      = false;              // Search all loaded files.
  cfg.search_noconfirm= false;              // Replace without confirmation.
  cfg.dict_bits       = DICTBIT_NAMES | DICTBIT_BEGIN;  // Default dictionary settings.
  cfg.dict_compress   = false;              // Do not compress dictionary entries.
  cfg.dict_auto       = true;               // Automatically search if the user has slected text.
  cfg.info_titles     = true;               // Put titles in the kanji-info list box.
  cfg.info_color      = RGB(255,0,0);       // Color used for titles in kanji-info box.
  cfg.undo_number     = 50;                 // Number of undo levels to keep.
  cfg.radical_info    = false;              // Double click on a kanji will input kanji into the file (apposed to get info).
  cfg.colorkanji_mode = COLORKANJI_NOMATCH; // Color kanji that match the list.
  cfg.colorkanji_color= RGB(0,0,255);       // Color to use in coloring kanji
  strcpy (cfg.font,"Arial");                // ASCII font.
  return;
}
#endif


#if 0
int dialog_int (HWND hwnd,int id,int def) {
  char *ptr,buffer[40];
  GetDlgItemText (hwnd,id,buffer,sizeof(buffer));
  for (ptr = buffer; isspace(*ptr); ptr++);
  if (!*ptr) return (def);
  return (atoi(ptr));
}
#endif


void radical_lookup (JWP_file *from) {
#if 0
  HRSRC resource;
  long size;
  HGLOBAL global;
  LPDLGTEMPLATE dialog;
  void *resmem;
#endif
//  DialogBox (instance,"RADLOOKUP",from->window,(DLGPROC) dialog_radlookup);
#if 0
  resource = LoadResource(NULL,"RADLOOKUP");
  size = SizeofResource(NULL,resource)+100;
  global = GlobalAlloc(GHND,size);
  dialog = (LPDLGTEMPLATE) GlobalLock(global);
  resmem = LockResource(resource);
  memcpy (dialog,resmem,size);
  DialogBoxIndirect (instance,dialog,from->window,(DLGPROC) dialog_radlookup);
  GlobalUnlock (global);
  GlobalFree (global);
#endif
#if 0
  struct rt {
    DLGTEMPLATE head;
    short menu;
    short wclass;
    short title[16];
  } rt;
  rt.head.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT;
  rt.head.dwExtendedStyle = 0;
  rt.head.cdit            = 0;
  rt.head.x               = 0;
  rt.head.y               = 0;
  rt.head.cx              = 100;
  rt.head.cy              = 100;
  rt.menu                 = 0x0000;
  rt.wclass               = 0x0000;
  int i;
  for (i = 0; i < 16; i++) rt.title[i] = "Radical Lookup"[i];
#endif
  byte buffer[2000];
  DLGTEMPLATE head;
  DialogTemplate rt;
  DLGITEMTEMPLATE item;

  head.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT;
  head.dwExtendedStyle = 0;
  head.x               = 0;
  head.y               = 0;
  head.cx              = 100;
  head.cy              = 100;
  rt.initialize (buffer,&head);
  rt.add_short  (0);
  rt.add_short  (0);
  rt.add_string ("Radical Lookup");
  rt.add_short  (8);
  rt.add_string ("MS Sans Serif");
  item.style           = 0;
  item.dwExtendedStyle = 0;
  item.x               = 3;
  item.y               = 6;
  item.cx              = 39;
  item.cy              = 18;
  item.id              = 1100;
  rt.add_item (&item);
  rt.add_short (0xffff);
  rt.add_short (0x0080);
  rt.add_string ("test");
  rt.add_short  (0);

  DialogBoxIndirect (instance,(DLGTEMPLATE *) buffer,from->window,(DLGPROC) dialog_radlookup);
#if 0
  HGLOBAL global;
  LPDLGTEMPLATE dialog;
  global = GlobalAlloc(GHND,2000);
  dialog = (LPDLGTEMPLATE) GlobalLock(global);
  memcpy (dialog,buffer,2000);

  DialogBoxIndirect (instance,(DLGTEMPLATE *) dialog,from->window,(DLGPROC) dialog_radlookup);
  GlobalUnlock (global);
  GlobalFree (global);
#endif
  return;
}


#define DialogTemplateHead  ((DLGTEMPLATE *) buffer)

typedef class DialogTemplate {
public:
  void add_string (char *string);
  void add_block (void *data,int size);
  void add_short (int value);
  void add_item  (DLGITEMTEMPLATE *item);
  void initialize (byte *buf,DLGTEMPLATE *head);
private:
  byte *buffer;
  byte *ptr;
} DialogTemplate;

void DialogTemplate::add_short (int value) {
  short s;
  s = value;
  add_block (&s,sizeof(short));
  return;
}

void DialogTemplate::add_string (char *string) {
  do { add_short (*string); } while (*string++);
  return;
}

void DialogTemplate::add_block (void *data,int size) {
  memcpy (ptr,data,size);
  ptr += size;
  return;
}

void DialogTemplate::initialize (byte *buf,DLGTEMPLATE *head) {
  ptr = buffer = buf;
  add_block (head,sizeof(DLGTEMPLATE));
  DialogTemplateHead->cdit = 0;
  return;
}

void DialogTemplate::add_item (DLGITEMTEMPLATE *item) {
  add_block (item,sizeof(DLGITEMTEMPLATE));
  DialogTemplateHead->cdit++;
  return;
}

#if 0
CreateFont
SYSTEM_FONT
LoadResource
DialogBoxIndirect
#endif


// This was a window proc for handling the kanji character in the kanji-info, as was
//   orrigianlly used in JWP.  I converted this display to static.

#define PROP_KANJI  "JWP-Kanji"

  wclass->style         = CS_HREDRAW | CS_VREDRAW;
  wclass->lpfnWndProc   = JWP_bigkanji_proc;
  wclass->lpszClassName = "JWP-BigK";
  if (!RegisterClass(wclass)) return (TRUE);

static LRESULT CALLBACK JWP_bigkanji_proc (HWND hwnd,UINT iMsg,WPARAM wParam,LPARAM lParam) {
  HDC         hdc;
  PAINTSTRUCT ps;
  KANJI ch;
  RECT rect;
  
  switch (iMsg) {
    case WM_CREATE:
         SetProp (hwnd,PROP_KANJI,0);
         return (0);
    case WM_DESTROY: 
         RemoveProp (hwnd,PROP_KANJI);
         return (0);
    case WM_USER:
         SetProp (hwnd,PROP_KANJI,(HANDLE) wParam);
         InvalidateRect (hwnd,NULL,TRUE);
         UpdateWindow (hwnd);
         return (0);
    case WM_PAINT:
         ch = (KANJI) GetProp(hwnd,PROP_KANJI);
         hdc = BeginPaint (hwnd,&ps);
         GetClientRect (hwnd,&rect);
         Rectangle (hdc,0,0,rect.right,rect.bottom);
         rect.right  = rect.right -jwp_font.hspace;
         rect.left   = rect.left  +jwp_font.hspace;
         rect.top    = rect.top   +jwp_font.vspace;
         rect.bottom = rect.bottom-jwp_font.vspace;
         if (ch) kanji.fill (hdc,ch,&rect);
         EndPaint (hwnd,&ps);
         return (0);
    case WM_GETDLGCODE:
         return (DLGC_STATIC);
  }
  return (DefWindowProc(hwnd,iMsg,wParam,lParam));
}





#if 0
#define DICTKEY_NOMATCH     0
#define DICTKEY_REJECT     -1
#define DICTKEY_CONTINUE   -2
#define DICTKEY_FINISHED   -3

static int test_key (char *ptr,int key) {
  byte ch,*p;
  int  len;
  len = strlen(dict_keys[key].key);
  if (strncmp(ptr+1,dict_keys[key].key,len)) return (DICTKEY_NOMATCH);
  ch = ptr[len+1];
  if ((ch != ',') && (ch != ')')) return (DICTKEY_NOMATCH);
  if (!dict_keys[key].reject) return ((ch == ')') ? DICTKEY_FINISHED : len);
#if 0
  if ((ch == ',') && (key[0] == 'p') && (key[1] == 'n')) {
    while (TRUE) {
      for (j = 0; j <
    }
  }
#endif
  if ((ch == ')') && (*ptr == '(')) return (DICTKEY_REJECT);
  len++;
  if (*ptr == '(') ptr++;
  for ( ; ptr[len]; ptr++) ptr[0] = ptr[len];
  ptr[0] = 0;
  return ((ch == ')') ? DICTKEY_FINISHED : DICTKEY_CONTINUE);
}
#endif



#if 0
{
byte temp[256];
    byte *p,*p2,*p3;
    int i,j,processed;
    processed = FALSE;
    for (p = ptr; *p; p++) {
      if (*p == '(') {
        while (*p && (*p != ')') && (*p != '/')) {
          for (p++, i = 0; p[i] && (p[i] != '/') && (p[i] != ',') && (p[i] != ')'); i++) temp[i] = p[i];
          temp[i] = 0;
          for (j = 0; j < NUMBER_DICTKEYS; j++) {
            if (dict_keys[j].reject && !strncmp((char *) temp,dict_keys[j].key,strlen(dict_keys[j].key))) {
              for (p2 = p+i; *p2 && (*p2 != '/'); p2++);
              while (*p != '/') p--;
              for (p3 = p; *p3; ) *++p3 = *++p2;
//              while (*p2) *p++ = *p2++;
              *p = 0;
              i = 0;
              processed = TRUE;
              break;
rejected++;
message (NULL);
goto RejectLine;
            }
          }
          p += i;
        }
      }
    }
#endif     
       
#if 0
        for (i = 0; p[i] && (p[i] != '/') && (p[i] != ',') && (p[i] !
        p++;
        for (i = 0; i < NUMBER_DICTKEYS; i++) {
          if (dict_keys[i].reject && !strncmp((char *) p,dict_keys[i].key,strlen(dict_keys[i].key))) { 
            for (p2 = p; *p2 != '/'; p2++);
            while (*p != '/') p--;
            while (*p2) *p++ = *p2++;
            *p = 0;
            for (i = 0, p2 = ptr; *p2; p2++) if (*p2 == '/') i++;
//            if (i > 1) break;
            rejected++; 
            message (NULL);
            goto RejectLine;
          }
        }
      }
    }
TestJump:;
}
#endif

#if 0
        /* Censor the entry */

        if (!CensorEntry(target)) {
            nr_rejected++;
            continue;
        }

#endif

#if 0
{
byte *p;
int i,j;
    for (p = ptr; *p; p++) {
      if (*p == '(') {
RestartList:
        for (i = 0; i < NUMBER_DICTKEYS; i++) {
          if (!dict_keys[i].reject) continue;
          switch (j = test_key((char *) p,i)) {
            case DICTKEY_NOMATCH:
                 break;
            case DICTKEY_REJECT: {
                   byte *p2,*p3;
                   for (p2 = p; *p2 != '/'; p2++);
                   while (*p != '/') p--;
                   for (p3 = p; *p2; ) *p3++ = *p2++;
                   *p3 = 0;
                 }
            default:
                 p += j-1;
            case DICTKEY_FINISHED:
                 goto NextCharacter;
            case DICTKEY_CONTINUE:
                 goto RestartList;
          }
        }
      }
NextCharacter:;
    }

}
#endif



#if 0
RECT rect;

int x,xmax,i,j,ch,first_line;
    GetClientRect (GetDlgItem(dialog,ID_RESULT),&rect);
    x = 0;
    i = 0;
    j = 0;
    first_line = TRUE;
    xmax = rect.right;
    byte buffer2[256];

    while (TRUE) {
      ch = ptr[i];
      if (first_line || (ch & 0x80)) {
        if (ch & 0x80) ch2 = ((ch << 8) | ptr[i]) & 0x7f7f;
        else if (ch == '[') ch2 = BRACE_LEFT;
        else if (ch == ']') ch2 = BRACE_RIGHT;
        else if (ch == ' ') ch2 = '\t';
        if ((x = jwp_font.hadvance(x,ch2)) >= xmax) {
          buffer2[j] = 0;
          SendDlgItemMessage (dialog,ID_RESULT,LB_ADDSTRING,0,(long) buffer2);
          j = 0;
          x = jwp_font.hadvance(0,'\t');
          buffer2[j++] = '\t';
          continue;
        }
        if (ch2 == '\t') buffer2[j++] = '\t';
          else {
            buffer2[j++] = (ch2 >> 8) | 

        buffer2[j++] = ptr[i++];
        buffer2[j++] = ptr[i++];
      }
      else if ((ch == '\n') || (ch == '\r')) {
        buffer2[j] = 0;
        SendDlgItemMessage (dialog,ID_RESULT,LB_ADDSTRING,0,(long) buffer2);
        break;
      }
      else if (first_line) {
        switch (ch):
          case ' ':
               buffer2[j++] = '\t';
               x = jwp_font.hadvance(x,'\t');
               break;
          case '[':
          case ']':
               ch = (ch = '[') ? BRACE_LEFT : BRACE_RIGHT;

      }
      else {
        buffer2[j++] = ptr[i++];
      }
    }
#endif    


#if 0
    while (TRUE) {
      ch = ptr[i];
      if (ch & 0x80) {
        j = i;
        ch = ((ch << 8) | (ptr[i+1])) & 0x7f7f;
        if (jwp_font.hadvance(x,ch) > xmax) {
          ptr = put_line (ptr,i);
          x = i = 0;
          continue;
        }
        i += 2;
      }
      else if ((ch == '\r') || (ch == '\n') || !ch) {
        put_line (ptr,i);
        break;
      }
      else if (!past_kanji) {
        switch (ch) {
          case ' ':
               ch = ptr[i] = '\t';
               break;
          case '[':
                
        }
        if (ch == ' ') ptr[i] = '\t
      }
      else {
        switch (ch) {
          case '\t':
               
          case ' ':
          case '/':
          default:
               i++;
        }
      }

    }
#endif


//
//  Calculate horizontal advance for a character.
//
//      x      -- Inital horizontal position.
//      ch     -- Character.
//
//      RETURN -- New hornizontal poisiton.
//
int JWP_file::hadvance (int x,int ch) {
  if (ISJIS(ch)) return (x+jwp_font.hwidth);
  if (ch != '\t') return (x+jwp_font.width(ch));
  return (((x-jwp_font.x_offset)/jwp_font.hwidth+1)*jwp_font.hwidth+jwp_font.x_offset);
}


//
//  Destructor
//
JWP_config::~JWP_config () {
  if (colorkanji_list) free (colorkanji_list);
  colorkanji_list = NULL;
  return;
}


//
//  Generate an error message dealing with configuration files.
//
//      err     -- Set to TRUE for an error and FALSE for a warning.
//      code    -- Base text of message:
//          
//              ERROR_LOAD -- Cannot load a file.
//              ERROR_SAVE -- Cannot save a file.
//
//      message -- Describes the file type in question.
//
//  Messages like:
//
//                  Unable to load <message>
//                       <file name>
//
//  are generated by this routine.
//
void JWP_config::error (int err,int code,char *message) {
  static char *messages[] = {
    "Unable to load ",
    "Unable to save ",
  };
  char temp[512];
  strcpy (temp,messages[code]);
  strcat (temp,message);
  strcat (temp,"\n");
  strcat (temp,buffer);
  ErrorMessage (err,temp);
// ### Need to fix ErrorMessage not to use sprintf
  return;
}



void position_dialog (HWND hwnd) {
  RECT rect;
  GetWindowRect (hwnd,&rect);
  OffsetRect    (&rect,-rect.left,-rect.top);
}
#if 0
void CenterDialogBox (HWND hwnd)
{
    RECT rect;

    GetWindowRect(hwnd, &rect);
    OffsetRect(&rect, -rect.left, -rect.top);
    MoveWindow(hwnd, ((GetSystemMetrics(SM_CXSCREEN) - rect.right ) / 2 + 4) & ~7,
                     (GetSystemMetrics(SM_CYSCREEN) - rect.bottom) / 2,
                     rect.right, rect.bottom, FALSE);
}
#endif





//
//  Simple utility routine ot read a character from a file.
//
//      RETURN -- Character read.
//
static int read_char (HANDLE file) {
  unsigned long done;
  byte c;
  ReadFile (file,&c,sizeof(c),&done,NULL);
  return (c);
}

HGLOBAL JWP_file::clip_export () {
  long        size;
  HGLOBAL     block;
  JIS_convert convert;
  convert.output_count ();
  convert.set_type (jwp_config.cfg.clip_write);
  size  = export_file(&convert);
  block = convert.output_clip (size);
  convert.set_type (jwp_config.cfg.clip_write);
  export_file (&convert);
  return (block);
}

HGLOBAL JWP_file::clip_private () {
  long     size;
  HGLOBAL  block;
  IO_cache cache;
  cache.output_count ();
  size  = write_jwp_file(&cache);
  block = cache.output_clip (size);
  write_jwp_file (&cache);
  return (block);
}



//
//  Write a block to a file.  This utility routine is used by the 
//  save file options.
//
//      file   -- Handle for file to write to.
//      data   -- Pointer to data.
//      szie   -- Number of bytes to write.
//
//      RETURN -- A non-zero value indicates an error writing to 
//                the file.
//  
static int write_file (HANDLE file,void *data,int size) {
  int err;
  unsigned long done;
  err = !WriteFile(file,data,size,&done,NULL);
  if (err || (done != (unsigned long) size)) return (TRUE);
  return (FALSE);
}

//
//  Write a single byte to a file.  This is simply a front end to the 
//  write_file() routine.
//
//      file   -- File to write to.
//      ch     -- Character to write.
//
//      RETURN -- A non-zero return value indicates an error writing
//                to the file.
//
static int write_byte (HANDLE file,int ch) {
  byte c;
  c = ch;
  return (write_file(file,&c,sizeof(byte)));
}



#if 0
//
//  Flush the buffer to a file.
//
//  Generally, the client will call this routine after writing a file 
//  to dump the buffer contents to the output stream.  Additionally, the
//  error condition (for the entrie write) is returned at this stage.
//
//  This routine is also called internally whenever the buffer is full.
//
//      RETURN -- A non-zero value indicates an error occured some time
//                during the file write.
//
long JIS_convert::flush () {
  int err;
  unsigned long done;
  if (!buffer) return (count);
  if (handle == INVALID_HANDLE_VALUE) { GlobalUnlock (block); return (error); }
  err = !WriteFile (handle,buffer,position,&done,NULL);
  position = 0;
  if (err/* || (position != (int) done)*/) error = TRUE;
  return (error);
}

//
//  Internal rotuine to put a character to an output stream.  If 
//  necessary, the output stream is flushed to the output device.
//
//      ch -- Character to put.
//
void JIS_convert::put_char (int ch) {
  if (!buffer) { count++; return; }
  buffer[position++] = ch;
  if (position == size) flush ();
  return;
}

//
//  Internal routine that reads a character form the input buffer.  If 
//  necessary, the buffer is refreshed from the data source.
//
//      RETURN -- Next character from the buffer.
//
int JIS_convert::get_char () {
  unsigned long done;
  if (!size) return (JIS_EOF);
  if (position >= size) {
    if (block) size = 0;
      else {
        ReadFile (handle,buffer,size,&done,NULL);
        position = 0;
        size     = done;
      }
    if (!size) return (JIS_EOF);
  }
  if (!buffer[position]) return (JIS_EOF);
  return (buffer[position++]);
}

HGLOBAL JIS_convert::output_clip (long asize,int filetype) {
  error      = FALSE;       // Clear error flag.
  handle     = INVALID_HANDLE_VALUE;
  block      = GlobalAlloc(GHND,asize+1);
  buffer     = (byte *) GlobalLock(block);
  size       = asize+1;
  type       = filetype;
  position   = 0;
  in_twobyte = FALSE;
  return (block);
}

void JIS_convert::output_count (int filetype) {
  error      = FALSE;       // Clear error flag.
  handle     = NULL;
  buffer     = NULL;
  block      = NULL;
  count      = 0;
  type       = filetype;
  in_twobyte = FALSE;
}

//
//  Sets the convert to use a file as input.
//
//      buf      -- Buffer for input buffering.
//      bsize    -- Size of buffer.
//      file     -- Handle to file (must be open and at the correct position).
//      filetype -- Type of file being read.  If using the find_type this value 
//                  is not used.
//
void JIS_convert::input_file (byte *buf,int bsize,HANDLE file,int filetype) {
  unsigned long done;
  block      = NULL;
  handle     = file;
  buffer     = buf;
  type       = filetype;
  position   = 0;
  in_twobyte = FALSE;
  ReadFile (handle,buffer,bsize,&done,NULL);    // Load buffer.
  size       = done;
  return;
}

void JIS_convert::input_clip (HGLOBAL hglobal,int filetype) {
  block      = hglobal;
  handle     = INVALID_HANDLE_VALUE;
  buffer     = (byte *) GlobalLock(block);
  type       = filetype;
  position   = 0;
  in_twobyte = FALSE;
  size       = GlobalSize(block);
  return;
}

//
//  Sets the convert to use a file as output.  This routine also clears
//  the system error flag.
//
//      buf      -- Buffer for input buffering.
//      bsize    -- Size of buffer.
//      file     -- Handle to file (must be open and at the correct position).
//      filetype -- Type of file being written.
//
void JIS_convert::output_file (byte *buf,int bsize,HANDLE file,int filetype) {
  error      = FALSE;       // Clear error flag.
  handle     = file;
  buffer     = buf;
  block      = NULL;
  size       = bsize;
  type       = filetype;
  position   = 0;
  in_twobyte = FALSE;
  return;
}


#endif

#if 0
//
//  Insert a character into a paragraph.
//
//      line -- Line to insert into.
//      pos  -- Position into that line.
//      ch   -- KANJI code to insert in (can be ASCII).
//
void Paragraph::ins_char (Line *line,int pos,int ch) {
  int i;
  if (length+1 > size) {
    if (alloc()) return;
  }
  for (i = length; i > line->first+pos; i--) text[i] = text[i-1];
  text[i] = ch;
  length++;
  if (format()) jwp_view.redraw(); else redraw();
  return;
}
#endif

//  Old version of ctrl-right (repalced by version using do_key).
#if 0
           abs_position (&cursor);
           i = char_class(get_char());           
           if (i == CLASS_SPACE) i = CLASS_JUNK;
           while (TRUE) {
             for (; cursor.pos < cursor.para->length; cursor.pos++) {
               j = char_class(get_char());
               if (j == CLASS_SPACE) i = CLASS_JUNK;
               if ((j != i) && (j != CLASS_SPACE)) {
                 rel_position (&cursor);
                 break;
               }
             }
             i = CLASS_JUNK;
             if (!cursor.para->next) break;
             cursor.para = cursor.para->next;
             cursor.pos  = 0;
           }
#endif

#if 0   // Unused class function to move down a number of lines.
  void   line_down (Position *pos,int count,int view);   // Move a position down a line.
void JWP_file::line_down (Position *pos,int count,int view) {
  int i;
  for (i = 0; i < count; i++) {
    if (pos->line->next) { pos->line = pos->line->next; continue; }
    if (pos->para->next) { pos->para = pos->para->next; pos->line = pos->para->first; continue; }
    break;
  }
  find_pos (pos,view);
  return; 
}

#endif

#if 0   // Top and bottom from vertical scroll.  These do not appear to be used.
    case SB_TOP:
         view_top.para = first;
         view_top.line = first->first;
         view_top.pos  = 0;
         find_pos (&view_top,POS_VIEW);
         break;
    case SB_BOTTOM:
         Position pos;
         pos.para = last;
         pos.line = last->last;
         find_pos (&pos,POS_VVERT);
         i = pos.y-height+jwp_font.vheight*2;
         while (!move_down(&view_top)) {
           find_pos (&view_top,POS_VVERT);
           if (view_top.y >= i) break;
         }
         break;
#endif

static char direct_kana[SIZE_DIRECT][4] = {
  {"+a" },{"a" },{"+i"},{"i" },{"+u" },{"u" },{"+e" },{"e"  },{"+o" },{"o" },
  {"ka" },{"ga"},{"ki"},{"gi"},{"ku" },{"gu"},{"ke" },{"ge" },{"ko" },{"go"},
  {"sa" },{"za"},{"si"},{"zi"},{"su" },{"zu"},{"se" },{"ze" },{"so" },{"zo"},
  {"ta" },{"da"},{"ti"},{"di"},{"+tu"},{"tu"},{"du" },{"te" },{"de" },{"to"},
  {"do" },{"na"},{"ni"},{"nu"},{"ne" },{"no"},{"ha" },{"ba" },{"pa" },{"hi"},
  {"bi" },{"pi"},{"hu"},{"bu"},{"pu" },{"he"},{"be" },{"pe" },{"ho" },{"bo"},
  {"po" },{"ma"},{"mi"},{"mu"},{"me" },{"mo"},{"+ya"},{"ya" },{"+yu"},{"yu"},
  {"+yo"},{"yo"},{"ra"},{"ri"},{"ru" },{"re"},{"ro" },{"+wa"},{"wa" },{"wi"},
  {"we" },{"wo"},{"n'"}
};

#if 0
struct script_element {
  ulong tag;
  ushort offset;
} *script_element;
struct script_list {
  ushort count;
  struct script_element element;
} *script_list;
script_list = (struct script_list *) (temp+swap_short(gsub_header->script_list));
j = swap_short(script_list->count);
script_element = &script_list->element;
#define KANA (*((ulong *) "kana"))
for (i = 0; (i < j) && (KANA == script_element[i].tag); i++); 
if (i >= j) return (true);

struct lang_element {
  ulong tag;
  ushort offset;
} *lang_element;
struct script {
//  ushort default_offset;
  ushort count;
  struct lang_element elements;  
} *script;
script = (struct script *)(((char *) script_list)+swap_short(script_element[i].offset));
j = swap_short(script->count);
#define JAN (*((ulong *) "JAN"))
lang_element = &script->elements;
for (i = 0; (i < j) && (JAN == lang_element[i].tag); i++); 
if (i >= j) return (true);

struct language {
  ushort offset;
  ushort rquired;
  ushort count;
  ushort feature;
} *language;
language = (struct language *) (((char *) script)+swap_short(lang_element[i].offset));
j = swap_short(language->count);
#endif

//
//  Process recent files, and reload files.
//
//  When the main configuration was loaded, the extneded contents of 
//  the config file (stuff after the fixed structure) were loaded into
//  the JWP_config::load variable, so we can access them then destroy 
//  them.
//
//      list -- Indicates wheather these files are to be listed in the 
//              recent files list.  This also controls processing of the
//              recent files list.
//
void JWP_config::open_files (int list) {
  if (!load) return;
  for (ptr = jwp_config.load, i = 0; i < 9; i++, ptr += lstrlen(ptr)+1) {
    if (*ptr && list) recent_files (ptr);
  }
    if (jwp_config.cfg.reload_files) {
#ifdef WINCE
      set_currentdir (ptr,false);
#else
      if (*ptr) SetCurrentDirectory (ptr);
#endif
      ptr += lstrlen(ptr)+1;
      for (; *ptr; ptr += lstrlen(ptr)+1) {
        if (!file_is_open(ptr) && FileExists(ptr)) new JWP_file(ptr,FILETYPE_AUTODETECT);
      }
    }
    free (jwp_config.load);
    jwp_config.load = NULL;
  }


