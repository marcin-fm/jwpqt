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
//  Search string:
//
//  The search string (what we are looking for) is stored in two places.
//  The first location is JWP_search::search_input.  This is exactly 
//  what the user input.  The second location is the JWP_search::search
//  string.  The second string is a processed string, which may have 
//  ascii characters converted to lower case, and jascii may have been 
//  been converted to ascii, depeneding on the state of the flags.
//
//  If JWP_search::search is null, this indicates that search_nocase and
//  search_jascii are both null, or that no characters are converted.  
//  For example the user entered a hiragana string, we can use the faster
//  search engine, without transforming characters.
//
//  Search operations are goverered by six flags located in the 
//  jwp_config.cfg structure:
//
//      search_jascii    -- Causes the search to consider ascii values to 
//                          be equivalent to the equivalent jascii codes.
//                          this flag, or the search_nocase flag will 
//                          force the seach algarithum to use a slower 
//                          search methode.
//      search_nocase    -- Causes the search engine to ignore the case 
//                          of ascii and jascii characters.  See the note
//                          about the search engine in the search_jascii
//                          flag.
//      search_back      -- Causes the search to operate backward through
//                          the file.  Generally, this flag is cleared 
//                          whenever you enter the search box, instead of
//                          preserving the state.  See CLEAR_BACKWARD 
//                          compile option.
//      search_wrap      -- Wrap causes the seach engine, when reaching 
//                          the end of a file to start over at the top of
//                          the file.  This flag is disabled when 
//                          search_all is enabled.  This is bacause search_all
//                          essentually does what search_wrap does.
//      search_all       -- When the end of one file is reached, the search
//                          progresses to the next open file.  
//      search_noconfirm -- Only valid on replace operations, this flag
//                          causes replacements to proceed without user
//                          intervention.  Generally, this flag is 
//                          cleared on entry to the dialoge also.  See
//                          teh CLEAR_NOCONFIRM compile option.
//

//===================================================================
//
//  Compile time options.
//

#define CLEAR_BACKWARD      // Defining this causes the Backward flag
                            //   to always be cleared in search and repalce
                            //   dialogue boxes.
#define CLEAR_NOCONFIRM     // Defining this causes the No Confirm flag
                            //   to always be cleared in repalce dialogue
                            //   boxes.

#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_edit.h"
#include "jwp_find.h"
#include "jwp_help.h"
#include "jwp_inpt.h"
#include "jwp_misc.h"

//===================================================================
//
//  Exported ata.
//

JWP_history search_history (IDS_HIST_SEARCH);       // Search hsitory buffer.
JWP_history replace_history(IDS_HIST_REPLACE);      // Replace hsitory buffer.

//===================================================================
//
//  Static internal routines.  
//

//--------------------------------
//
//  This is a stub dialog box handling routine for the replace dialoge
//  box.  This routine simply calls the class memeber function.
//
static BOOL CALLBACK dialog_replace (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_search.dlg_search(hwnd,message,LOWORD(wParam),true,NULL));
}

//--------------------------------
//
//  This is a stub dialog box handling routine for the replace dialoge
//  box.  This routine simply calls the class memeber function.
//
static BOOL CALLBACK dialog_search (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  JWP_list *list;
  if (message == WM_INITDIALOG) SetWindowLong (hwnd,GWL_USERDATA,lParam);
  list = (JWP_list *) GetWindowLong(hwnd,GWL_USERDATA);
  return (jwp_search.dlg_search(hwnd,message,LOWORD(wParam),false,list));
}

//--------------------------------
//
//  This routine reduces a character.  This is used to translate a 
//  character through the filters JASCII = ASCII and ignore case.
//
//      ch     -- Input character.
//  
//      RETURN -- Translated character.  Often the character translation
//                is itself.
//
static int search_char (int ch) {
  if (ch >= BASE_HIRAGANA) return (ch);
  if (jwp_config.cfg.search_jascii && ISJASCII(ch)) ch = jascii_to_ascii(ch);
  if (jwp_config.cfg.search_nocase && ISASCII (ch)) ch = tolower(ch);
  return (ch); 
}

//===================================================================
//
//  Begin Class JWP_search.
//
//  This class defines a search environment.
//

JWP_search jwp_search;  // Class instance.

//--------------------------------
//
//  Constructor.
//
JWP_search::JWP_search () {
  search_length = replace_length   = 0;
  search_input  = search = replace = NULL;
  search_dlg    = replace_dlg      = null;
  return;
}

//--------------------------------
//
//  Deallocate resrouces associated with a search.
//
void JWP_search::clear (int rep) {
  if (search_input) free (search_input);
  if (search      ) free (search);
  search = search_input = NULL;
  if (rep) {
    if (replace) free (replace);
    replace = NULL;
  }
  return;
}

//--------------------------------
//
//  This is actually the dialog box function for both search and replace
//  dialoge boxes.
//
//      hwnd    -- Window pointer for the dialoge box.
//      message -- Message from windows.
//      command -- Pre-processed command from windows.
//      rep     -- Non-zero indicates this is a replace dialoge.
//      list    -- If non-null, this indicates a search of an edit list, and this is the pointer to the list.
//  
int JWP_search::dlg_search (HWND hwnd,UINT message,int command,int rep,class JWP_list *list) {
  switch (message) {
//
//  Dialoge initialize, setup the buttons, and the edit-controls.
//
    case WM_INITDIALOG: 
         if (rep) replace_dlg = hwnd; else search_dlg = hwnd;
         add_dialog (hwnd,true);
         CheckDlgButton (hwnd,IDC_SRCASE    ,jwp_config.cfg.search_nocase);
         CheckDlgButton (hwnd,IDC_SRJASCII  ,jwp_config.cfg.search_jascii);
         CheckDlgButton (hwnd,IDC_SRWRAP    ,jwp_config.cfg.search_wrap);
         CheckDlgButton (hwnd,IDC_SRALLFILES,jwp_config.cfg.search_all);
         SendDlgItemMessage (hwnd,IDC_SRSEARCH,JE_SETTEXT,search_length   ,(LONG) search_input);
         SendDlgItemMessage (hwnd,IDC_SRSEARCH,JE_SETHIST,IDC_SRHISTSEARCH,(LONG) &search_history);
         if (!list) SendDlgItemMessage (hwnd,IDC_SRSEARCH,JE_LOAD   ,0,(LONG) jwp_file);
         EnableWindow   (GetDlgItem(hwnd,IDC_SRALLFILES),!list);

#ifndef CLEAR_BACKWARD
         CheckDlgButton (hwnd,IDC_SRBACKWARD,jwp_config.cfg.search_back);
#endif  CLEAR_BACKWARD
         if (rep) {
#ifndef CLEARN_NOCONFIRM
           CheckDlgButton (hwnd,IDC_SRNOCONFIRM,jwp_config.cfg.search_noconfirm);
#endif  CLEARN_NOCONFIRM
           SendDlgItemMessage (hwnd,IDC_SRREPLACE,JE_SETTEXT,replace_length   ,(LONG) replace);
           SendDlgItemMessage (hwnd,IDC_SRREPLACE,JE_SETHIST,IDC_SRHISTREPLACE,(LONG) &replace_history);
         }
         dlg_search (hwnd,WM_COMMAND,IDC_SRALLFILES,rep,list);
         return (true);
//
//  Since this is non-modal we need to cleanup on exit.
//
    case WM_DESTROY:
         if (rep) replace_dlg = null; else search_dlg = null;
         remove_dialog (hwnd);
         return (0);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_EDIT_SEARCH);
         return  (true);
//
//  Commands coming back from the controls.
//
    case WM_COMMAND:
         switch (command) { 
           case IDC_SRALLFILES:
                EnableWindow (GetDlgItem(hwnd,IDC_SRWRAP),!IsDlgButtonChecked(hwnd,IDC_SRALLFILES));  // Disable the wrap button when all-files is selected.
                return (true);
//
//  History buffers
//
           case IDC_SRHISTSEARCH:
                SendDlgItemMessage (hwnd,IDC_SRSEARCH,JE_HISTORYLIST,0,0);
                return (0);
           case IDC_SRHISTREPLACE:
                SendDlgItemMessage (hwnd,IDC_SRREPLACE,JE_HISTORYLIST,0,0);
                return (0);
//
//  User has selected the ok.
//
           case IDOK:
           case IDSEARCH:
                int       i;
                clear (rep);
                is_replace = rep;
                jwp_config.cfg.search_nocase = IsDlgButtonChecked(hwnd,IDC_SRCASE);
                jwp_config.cfg.search_jascii = IsDlgButtonChecked(hwnd,IDC_SRJASCII);
                jwp_config.cfg.search_back   = IsDlgButtonChecked(hwnd,IDC_SRBACKWARD);
                jwp_config.cfg.search_wrap   = IsDlgButtonChecked(hwnd,IDC_SRWRAP);
                jwp_config.cfg.search_all    = IsDlgButtonChecked(hwnd,IDC_SRALLFILES);
//
//  Read the seach string, and if search_jascii or search_nocase flags
//  have been selected then build alternitive search string.
//
                search_length = JEGetDlgItemText(hwnd,IDC_SRSEARCH,&search_input);
                search_input  = kstrdup(search_input,search_length);  
                if (!search_input) { OutOfMemory (hwnd); return (true); }
                if (jwp_config.cfg.search_jascii || jwp_config.cfg.search_nocase) {
                  if (!(search = kstrdup(search_input,search_length))) OutOfMemory (hwnd); 
                    else {
                      for (i = 0; i < search_length; i++) search[i] = search_char(search[i]);
                    }
                }
//
//  Simular procedure to get the replace string.
//                
                if (rep) {              
                  jwp_config.cfg.search_noconfirm = IsDlgButtonChecked(hwnd,IDC_SRNOCONFIRM);
                  replace_length                  = JEGetDlgItemText(hwnd,IDC_SRREPLACE,&replace);
                  replace                         = kstrdup(replace,replace_length);
                }
                if (list) list->find (search_length); else SendMessage (main_window,WM_COMMAND,IDM_EDIT_FINDNEXT,0);
                if (!jwp_config.cfg.keep_find) DestroyWindow (hwnd); else SetFocus (hwnd);
                return        (true);
           case IDCANCEL:
                DestroyWindow (hwnd);
                return        (true);
         }
  }
  return (false);
}

//--------------------------------
//
//  This monster routine really does all the search and replace actual
//  work.
//  
#define CONFIRM_NO      IDC_RCNOREP   // No, do not replace, but continue on.
#define CONFIRM_YES     IDOK          // Yes, do replace, and continue on.
#define CONFIRM_ABORT   IDCANCEL      // No, do not replace and abort search
#define CONFIRM_ALL     IDC_RCALL     // Yes, do raplace, and all further searches.

void JWP_search::do_next (class JWP_list *list) {
  int code = CONFIRM_NO;    // Code indicates confirmation from the user.  The special value
                            //   CONFIRM_ALL can be set to force search into search_noconfirm
                            //   mode.
  JWP_file *file;           // These two specify the present location of the search within
  Position  pos;            //   both all the files, and within the current file.
  JWP_file *start_file;     // These two specify the staring location of the search within 
  Position  start_pos;      //   both all the files, and within the current file.  This is used
                            //   to determine when the search has failed (we are back at the 
                            //   same position (for wrap searches, and all file searches).
//
//  Check for a valid search at all.
//
  if (!search_input) {                      // If no search then do find.
    do_search (list);
    return;
  }
//
//  Check for a list search
//
  if (list) {                               // If this is a list then use the list find.
    list->find (search_length);
    return;
  }
//
//  Intialize the start and active positions.
//
  changes    = 0;                           // Initialize the changes.
  start_file = file = jwp_file;
  file->clear_cursor ();                    // Take out the vertical adjust memory.
  pos = file->cursor;
  pos.abs ();
  start_pos = pos;
//
//  This while looop suports replacements.  That is contained wihtin 
//  the while loop is the sarch for the next item, and the replace part.
//  For simple searches, the loop will only be executed once.
//
  while (true) {                            // REPLACE LOOP
//
//  Backwards search block.
//
    if (jwp_config.cfg.search_back) {
      if (code != CONFIRM_ALL) pos.pos--;   // Move one so we don't file the location we are starting from (last search).
      while (pos.para) {                    // PARA LOOP: Search through all paragraphs.
        while (pos.pos >= 0) {              // POS LOOP: Search through all positions in a paragraph.
          if ((start_file == file) && (start_pos.para == pos.para) && (start_pos.pos == pos.pos)) goto NotFound;
          if ((pos.pos+search_length <= pos.para->length) && test(pos.para->text+pos.pos)) goto FoundMatch;
          pos.pos--;
        }
  //
  // Advance to next paragrah.
  //
  // If, the next paragraph is NULL, then we are at the end of the file.
  // Here, we may have the option of moving to the ntext file, or moving to the bottom of the current file.
  //
        pos.para = pos.para->prev;         
        if (!pos.para) {
          if      (jwp_config.cfg.search_all ) { file = file->prev; pos.para = file->last; }
          else if (jwp_config.cfg.search_wrap) { pos.para = file->last; }
        }
        if (pos.para) pos.pos = pos.para->length;
      }
    }                                       // PARA LOOP
//
//  Forward search block (simular to above).
//
//  Note that we cannot skip the end of the strings, because if we do 
//  so the wrap and all-files options will not find their end conditions.
//  Because we may not come back to the same location if the user 
//  starts the search at the end of a paragraph.
//
    else {
      if (code != CONFIRM_ALL) pos.pos++;
      while (pos.para) {
        while (pos.pos <= pos.para->length) {
          if ((start_file == file) && (start_pos.para == pos.para) && (start_pos.pos == pos.pos)) goto NotFound;
          if ((pos.pos+search_length <= pos.para->length) && test(pos.para->text+pos.pos)) goto FoundMatch;
          pos.pos++;
        }
        pos.para = pos.para->next;
        if (!pos.para) {
          if      (jwp_config.cfg.search_all ) { file = file->next; pos.para = file->first; }
          else if (jwp_config.cfg.search_wrap) { pos.para = file->first; }
        }
        pos.pos = 0;
      }
    }
//
//  Not found!  Put up a message and exit search routine.
//
NotFound:
    not_found (main_window,code == CONFIRM_ALL);
    break;
//
//  Found.  Switch to the new file, and mark the selected text.
//
FoundMatch:
    if (file != jwp_file) file->activate ();
    jwp_file->cursor = jwp_file->sel.pos1 = jwp_file->sel.pos2 = pos;
    jwp_file->sel.type      = SELECT_EDIT;
    jwp_file->sel.pos2.pos += search_length;
//
//  Shift back to relative coordinates so we don't screw up the undo 
//  processors.
//
    jwp_file->all_rel ();
//
//  This is a simple search, so clean up, generate display and exit.
//    
    if (!is_replace) break;
//
//  We got here, so we are doing a replace.
//                                                              // Setting confirm all here has the advantage on a 
                                                                //   preset replace all of diabling the error condition
                                                                //   for not-found only after the first search.  Thus if
                                                                //   nothing is found, an error is generated, but if at
                                                                //   least one change is found no errors are generated.
    if (jwp_config.cfg.search_noconfirm) code = CONFIRM_ALL;    // In search_noconfirm mode.
      else if (code != CONFIRM_ALL) {                           // User already selected do all.
        jwp_file->adjust ();                                    // Refresh display and see what the user wants.
        code = ButtonDialog(replace_dlg,IDD_REPLACECONFIRM,NULL,IDH_EDIT_SEARCH);
        if (code == CONFIRM_ABORT) return;                      // User wants out!
      }
    if (code != CONFIRM_NO) {                                   // User wants to skip the replace this time (NO).
      if ((start_file == file) && (start_pos.para == pos.para) && (pos.pos <= start_pos.pos)) {
        start_pos.pos += replace_length-search_length;          // When we replace, must adjust the start position, to make sure
      }                                                         //   we don't go into an endless loop.
      jwp_file->undo_start ();
      jwp_file->selection_delete ();
      jwp_file->sel.pos1.para->ins_string (jwp_file,jwp_file->sel.pos1.line,jwp_file->sel.pos1.pos,replace,replace_length);
      jwp_file->undo_end ();
      changes++;
      if (code == CONFIRM_ALL) {
        if (jwp_config.cfg.search_back) pos.pos -= search_length; else pos.pos += replace_length;
      }
    }
  }     // REPLACE LOOP
  jwp_file->adjust ();
  return;
}

//--------------------------------
//
//  Processes the Edit/Replace command.  This basically generates the 
//  dialoge box. and then if the user did not cancle, does a do_next()
//  to actually do the command.
//
void JWP_search::do_replace () {
  if (replace_dlg) SetForegroundWindow (replace_dlg);
    else JCreateDialog(IDD_REPLACE,main_window,(DLGPROC) dialog_replace);
  return;
}

//--------------------------------
//
//  Processes the Edit/Search command.  This basically generates the 
//  dialoge box. and then if the user did not cancle, does a do_next()
//  to actually do the command.
//
void JWP_search::do_search (class JWP_list *list) {
  if (search_dlg && !list) SetForegroundWindow (search_dlg);
    else JCreateDialog(IDD_SEARCH,list ? list->parent() : main_window,(DLGPROC) dialog_search,(long) list);
  return;
}

//--------------------------------
//
//  Standardized message handler for not found error message.  This handles
//  mapping the error message boack to an old dialog box or to the searched
//  object (file or list).
//
//      window      -- Window to report the error message if the dialog box is
//                     not open.
//      replace_all -- Indicates is a replace of all so we use different messages.
//
void JWP_search::not_found (HWND window,int replace_all) {
  HWND hwnd;
  hwnd = is_replace ? replace_dlg : search_dlg;
  if (!hwnd) hwnd = window;
  if (replace_all && changes) JMessageBox (hwnd,IDS_SR_CHANGESMADE,IDS_SR_CHANGES,MB_OK | MB_ICONINFORMATION,changes);
    else JMessageBox (hwnd,IDS_SR_NOTFOUND,IDS_WARNING,MB_OK | MB_ICONWARNING);
  return;
}

//--------------------------------
//
//  Test if the current search string matches the current file 
//  location.
//
//      string -- String to be tested
//
//      RETURN -- A non-zero return value indicates a match.
//
int JWP_search::test (KANJI *string) {
  int i;
  if (search) {
    for (i = 0; i < search_length; i++) if (search_char(string[i]) != search[i]) return (false);
  }
  else {
    for (i = 0; i < search_length; i++) if (string[i] != search_input[i]) return (false);
  }
  return (true);
}

//
//  End Class JWP_search
//
//===================================================================













