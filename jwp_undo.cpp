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
//  This modlule implements the undo system.  This system is quite 
//  complex, because of the constraints of a reasonable system.  In 
//  particular, we want the user to be able to undo editing changes, but
//  we want a resonable overhead for the system, and we want thoes 
//  changes to be undoable in a resonalbe increment.  Keeping track of
//  every character typed, and every change is easy, but somewhat 
//  inconvenient for unding a change.  Saving changes too infrequently
//  is also a problem, in that the user cannot undo changes easilly, 
//  without unding a lot of other stuff.  We get arround this in a 
//  number of different ways.  Prinarally we save changes a key intervals
//  and whenever the user moves the cursor.  This means that a string 
//  of delete commands will be saved together, and a string of typing
//  may also be saved together, but if the user types some, deletes some,
//  etc, this generates a resonalbe system.
//
//  UNDO_action:
//
//  Undo data is stored in a number of actions.  These actions describe
//  what was done to the file to put it into a new state.  The basic 
//  action types are defined by a virtual class tructure and are 
//  defined as follows:
//
//          UNDO_para    -- Stores changes to a single paragraph, this 
//                          includes editing, deleteing, reformating, etc.
//          UNDO_newpara -- Generated whenever a new paragraph is
//                          generated so it can be deleted.
//          UNDO_delpara -- Generated whenever a paragreaph is deleted.
//          UNDO_type    -- Special structure generated only when file
//                          type or name is changed (save as...).
//
//  Each undo action object contains the data and information necessary
//  to reverce the change.  Note that actions the user considers as atomic
//  may generated many undo actions.  An example of this is slececting 
//  several paragraphs and reformatting them at once.  Each paragraph 
//  generates an UNDO_para class object.
//
//  Each type of UNDO_action, basically consists of two specific 
//  rotuines:
//
//          UNDO_???::undo()      -- Which undoes the changes, and geneates 
//                                   redo information on the fly
//          JWP_file::undo_??? () -- Constructor that makes the object,
//                                   and pushes it onto the undo chain.
//
//  Lists and chains:
//
//  As stated above an interal atomic undo action may not correspond 
//  with a user's view of an atomic action.  We conpensate for this by
//  building undo chains.  These chains are a singly linked list containning
//  a number of undo_actions that form a user atomic action.  Atomic 
//  actions are added to the chain at the beginning, and other actions
//  are pushed using the JWP_file::undo_push() command.  A new chain
//  is generated (by throwing away the oldest chanin) by calling 
//  JWP_file::undo_new().  Also note that some undo higher functions
//  have the ability to call undo_new() internally (specifically 
//  JWP_file::undo_para, and JWP_file::undo_type()). A chain can be 
//  disposed by by calling the static routine free_chain().
//
//  The entire undo buffer (and redo buffer) is a list (or array) of
//  undo chains.  The first element (0) is the newest and the higher 
//  numbers get older.
//
//  More Notes on the UNDO_Action derived classes:
//
//  Each of these classes contains a vitural function undo(), that is 
//  called to undo the action of the class.  This function should 
//  push sufficient data to redo the action on the redo stack.  
//  Some objects push essetially the same object on the redo stack 
//  (UNDO_para, UNDO_type), and some push the oposite (UNDO_delpara,
//  UNDO_newpara).
//
//  Methodes of storing undo infomation.
//
//  Three methodes of generating undo data are used.  These modes have 
//  diffent uses in different places:
//
//  Direct call [JWP_file::undo_para()/JWP_file::undo_type()]:
//
//  These are direct call, and are executed just before the file is 
//  changed.  These directly generate an undo chain that is expected 
//  to contain only a single item.
//
//  Accumulated [JWP_file::undo_clear()/JWP_file::undo_para()]:
//
//  This type of call is generated as follows before a particular type 
//  of change undo_para() is called with a type argument.  If the current
//  type is not allready on record, a new undo chain (containning a single
//  entry) is generated, and the type is set.  If the type matches the 
//  current accumulation type then nothing will happen.  
//
//  What this does is when you start to entry characters, an image of 
//  the current paragraph is stored and the undo accumulation type is 
//  set to UNDO_TYPE (typing).  As you continue to type characters no
//  more undo records will be generated, but when you change to deleting,
//  or move the cursor the undo type is reset, so if you go back to 
//  typing another undo image will be saved.  This causes undo iformation
//  to be stored in nice increments.
//
//  The routine undo_clear is called at several points to reset the 
//  undo type if it is not a specific type.  Generally, this is used on
//  cursor movement routines to reintialize the undo type.
//
//  Note that generating any other type of undo record will clear the 
//  undo accumulation type.
//
//  Passive [JWP_file::undo_start()/undo_end()/undo_newpara()/undo_delpara()/undo_para()]:
//
//  This type of undo record is generated for a number of complex 
//  commands that extend over more than one paragraph.  In this case
//  a flag (queing) is set to true.  This flag causes certain types 
//  of changes to be accumulated in an undo record.  undo_end is then
//  called at the end to indicate that we are finished.
//
//  Note that when in accumulation mode the id argument passed to 
//  undo_para() are automacially changed to UNDO_QUE, to cause quing.
//
//  Additionally, note that calls to undo_start()/undo_end() are nested,
//  this allows the undo system to deal with calling a routine that 
//  also saves undo information without a problem.  Note that the system
//  is soft on the exit, and allows undo_end() to be called an arbitary
//  number of times (this is used some place in the code).
//
//-------------------
//
//  What actually goes on during an undo and a redo operation:
//
//  The system keeps nominally two lists.  The undo lists (which is 
//  attached to the file) and a static redo lists (in this module).
//  When an action is being undone (or redone), the system uses the 
//  same old undo information accumulation routines to store the 
//  redo or undo information.  How does this actually work:
//
//  When the user starts an undo, several things happen: 
//
//    1. Any selected regon is cleared (otherwiese this is a mess).  
//    2. If it does not exist a redo list is allocated.  If the list
//       does exist, this indicates we are in the second or deeper
//       undo request.
//    3. A pasive undo accumulation is started [undo_start()]
//       This will accumulate the information for the redo.
//    4. A special (static) flag called undoing is set.  This flag is 
//       improtaint in that all attempts to save undo infomration are
//       stored in the redo list instead of the undo list.  Additionally,
//       this flag indicates we should not throw away the redo information,
//       which is usally tossed whenever a new undo data is stored.
//    5. The top undo chain is undone one step at a time.
//    6. The chain is then disposed of and the list is shifted.
//    7. All the special variables and states are reset.
//
//  When the user starts a redo, the sactions are very simular, except:
//
//      We don't need to allocate the redo list.
//      The special variable set is redoing, not undoing.
//
//  The redo list is disposed of whenever the user first generates any
//  new undo information (except from a redo -- see above), or when the
//  user switches to a new file.
//

#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_file.h"
#include "jwp_stat.h"

//===================================================================
//
//  Static data and definitions.
//

       UNDO_action **redo = NULL;   // Redo buffer, used to hold actions in case we want to redo.
static byte queing        = false;  // Causes accumulation of some types of changes.
static byte undoing       = false;  // Set during an actual undo phase.
static byte redoing       = false;  // Set during an actual redo phase.

//===================================================================
//
//  Static routines
//

//--------------------------------
//
//  Deallocates a chain of undo actions that have been linked.
//
//      action -- Pointer to the head of the chain (may be NULL).
//
static void free_chain (UNDO_action *action) {
  UNDO_action *act;
  while (action) {
    act    = action;
    action = action->next;
    delete act;
  }
  return;
}

//===================================================================
//
//  Exported routines.
//

//--------------------------------
//
//  Allocates (or reallocates) and undo list to a new length.
//
//      old        -- Old list (can be NULL).
//      old_length -- Length of old list.
//
//      RETURN     -- Pointer to a new list.  The length of the new list 
//                    is set by jwp_config.cfg.undo_number.
//
UNDO_action **undo_alloc (UNDO_action **old,int old_length) {
  int i,length;
  UNDO_action **undo;
  if (!old) old_length = 0;
  length = min(jwp_config.cfg.undo_number,old_length);
  if ((undo = (UNDO_action **) calloc(jwp_config.cfg.undo_number,sizeof(UNDO_action *)))) {
    for (i = 0; i < length; i++) { 
      undo[i] = old[i];
      old[i]  = NULL;
    }
  }
  undo_free (old,old_length);
  return (undo);
}

//--------------------------------
//
//  Deallocate an undo list.
//
//      list   -- Pointer to the list (can be NULL).
//      length -- Lenght of the list.
//
void undo_free (UNDO_action **list,int length) {
  int i;
  if (!list) return;
  for (i = 0; i < length; i++) free_chain (list[i]);
  free (list);
  return;
}

//===================================================================
//
//  Begin class UNDO_action:
//
//  This is the base class for the undo actions.  There is not much 
//  to it.
//

//--------------------------------
//
//  Destructor.
//
UNDO_action::~UNDO_action () {
  if (data) free (data);
  return;
}

//--------------------------------
//
//  End Class UNDO_action
//
//===================================================================

//===================================================================
//
//  Begin class UNDO_delpara:
//
//  This class contains undo information for a delete paragraph.  
//  This type of object is generated by JWP_file::del_paragrah, and 
//  is fundamentally an UNDO_para object, except the treatment of 
//  the object is somewhat different in the undo() function generates
//  a paragaph before seting the data for it.  The constructor rotuine,
//  JWP_file::undo_delpara() is also much simpler and expects to be 
//  called from inside an undo_start()/undo_end() pair.
//
//  This class was revised.  The base class UNDO_delpara, only contains
//  the paragraph number of the paragraph that was deleted.  The
//  constructore JWP_file::undo_delpara(), however, pushes two items, 
//  on the chain, the first is this that will make a paragraph.  This 
//  will then be followed by an UNDO_para object.
//

//--------------------------------
//
//  This routine simply restores the deleted paragraph.  Remember, that
//  the gnerator placed a paragrah type object next in the chain, that 
//  will contain the actual contents of the pargraph.
//
//      file -- File from which we are undoing.
// 
void UNDO_delpara::undo (JWP_file *file) {
  file->new_paragraph (file->number_to_para(number-1));
  return;
}

//--------------------------------
//
//  This is the constructor for a class UNDO_delpara object.  This 
//  constructor calls the same core as the UNDO_para constructor to
//  setup the class.  The rotuine actually pushes onto the stack two 
//  items, a UNDO_para object forllowed by an UNDO_delpara object.  
//  When these are reversed,they will cause the paragraph to be 
//  generated then filled.
//
//      para -- Pointer to paragraph that is going to be deleted.
//
void JWP_file::undo_delpara (Paragraph *para) {
  UNDO_delpara *undo_delpara;

  if (!undo || !queing) return;
  if (!(undo_delpara = new UNDO_delpara)) return;
  undo_delpara->data   = NULL;
  undo_delpara->number = para_to_number (para);
  undo_para (UNDO_QUE,para);
  undo_push (undo_delpara);
  return;
}

//
//  End Class UNDO_delpara.
//
//===================================================================

//===================================================================
//
//  Begin class UNDO_newpara:
//
//  This action is generated whenever the user geneates a new paragrah,
//  as in ctrl+return.
//

//--------------------------------
//
//  Actual undo fucntion.  This basically deletes the paragraph.
//
//      file -- File to put undo back into.
//
void UNDO_newpara::undo (JWP_file *file) {
  Paragraph *para;
  para = file->number_to_para (number);
  file->del_paragraph (para);
  return;
}

//--------------------------------
//
//  Generator function.  This is called with a pointer to a new 
//  pargraph (after the paragraph has been generated).
//
//      para -- Newly created paragraph.
//
void JWP_file::undo_newpara (Paragraph *para) {
  UNDO_newpara *undo_newpara;
  if (!undo || !queing || !(undo_newpara = new UNDO_newpara)) return;
  undo_newpara->data   = NULL;
  undo_newpara->number = para_to_number (para);
  undo_push (undo_newpara);
  return;
}

//
//  End Class UNDO_newpara.
//
//===================================================================

//===================================================================
//
//  Begin class UNDO_para:
//
//  This class contains undo information that replaces a specific 
//  paragraph with one that has been saved.  All typing, simple editing,
//  and most other changes fit within this class.
//

//--------------------------------
//
//  This is a class stub that calls a function in JWP_file class.  We 
//  do this because we need to get out of this class and into the file
//  class (what a pisser).
//
void UNDO_para::undo (JWP_file *file) {
  file->undo_undo (this);
  return;
}

//--------------------------------
//
//  Main rotuine of the paragraph saving block.  This routine if actually
//  invoked will save an image of the current curor paragraph that can be 
//  undone later.
//
//  Because we allow the user to not simply save every change, but rather 
//  to save changes in blocks (i.e. typed characters until the cursor is 
//  moved, etc.) we need to determine if we need to make a change.  Most
//  of these blocked changes come through this routine.  To handle this,
//  we pass an id to this routine.  This can determine if we are going to 
//  save new data, or to allow the changes to accumulat.  This is done 
//  as follows:  If the ID is the same as the current accumulation id 
//  (stored in JWP_file::undo_accum), then we just accumulate the change
//  and do not create a new undo entry.  If the id is different, we save 
//  the data and go on.
//
//  There are a number of special id's:
//
//          UNDO_NONE -- We are not accumulating (this should not be 
//                       passed to this routine in general).
//          UNDO_ANY  -- Force creation of an undo action.
//          UNDO_PUSH -- Force creation of an undo action, but push it on the list.
//
//      id -- ID (see above).
//
void JWP_file::undo_para (int id,Paragraph *para) {
  int i;
  Line      *line;
  UNDO_para *undo_para;

  if (!undo || (undo_accum == id)) return;          // No undo buffer, or same id so do nothing.
  if (id == UNDO_QUE) {                             // quing check.
    if (!queing) return;
    id = UNDO_ANY;
  }
  if (!(undo_para = new UNDO_para)) return;         // Allocate undo object.
  undo_accum = (id == UNDO_ANY) ? UNDO_NONE : id;   // Set accumulation parameter
  if (!para) para = cursor.para;                    // Get current paragraph.
  for (i = 0, line = cursor.para->first; line != cursor.line; i++, line = line->next);
  undo_para->number       = para_to_number(para);   // Setup undo structure.
  undo_para->length       = para->length;
  undo_para->page_break   = para->page_break;
  undo_para->indent_left  = para->indent_left;
  undo_para->indent_right = para->indent_right;
  undo_para->indent_first = para->indent_first;
  undo_para->spacing      = para->spacing;
  undo_para->cursor_para  = para_to_number(cursor.para);
  undo_para->cursor_line  = i;
  undo_para->cursor_pos   = cursor.pos;
  undo_para->data         = (KANJI *) malloc (i = para->length*sizeof(KANJI));
  if (!undo_para->data && i) { delete undo_para; return; }
  memcpy    (undo_para->data,para->text,i);         // copy the paragraph contents.
  if (!queing) undo_new ();                         // Push undo data
  undo_push (undo_para);    
  return;
}

//--------------------------------
//
//  This rotuine actually undoes changes to a single paragaraph.  This 
//  is the prinary undo funcion that actually does work.
//
void JWP_file::undo_undo (UNDO_para *undo) {
  int        i;
  Line      *line;
  Paragraph *para;
  para               = number_to_para (undo->number);   // Get paragraph number
  undo_para (UNDO_QUE,para);                            // Setup undo
  para->page_break   = undo->page_break;                // Get undo format data
  para->indent_left  = undo->indent_left;
  para->indent_right = undo->indent_right;
  para->indent_first = undo->indent_first;
  para->spacing      = undo->spacing;
  para->set_text ((KANJI *) undo->data,undo->length);   // Get text
  para->format   (this,NULL,true);                      // Format the paragraph
  cursor.para = number_to_para (undo->cursor_para);     // Restore cursor location
  for (i = 0, line = cursor.para->first; i != undo->cursor_line; i++, line = line->next);
  cursor.line = line;
  cursor.pos  = undo->cursor_pos;
  return;
}

//
//  End Class UNDO_para
//
//===================================================================

//===================================================================
//
//  Begin class UNDO_type:
//
//  This class contains undo information for file name and type changes.
//  Currently, this cann only be generated from within the save-as.
//  command processor.
//

//--------------------------------
//
//  The actual undo for this class of action.  Note that this must be
//  a friend class for this to work out.
//
void UNDO_type::undo (JWP_file *file) {
  file->undo_type ();                   // Undo the type change we are about to do (undo)
  file->filetype = type;                // Restore type.
  if (file->name) free (file->name);    // Dispose current name.
  file->name = (TCHAR *) data;          // Restore name
  data = NULL;                          // Clear this so we don't deallcoate the name field.
  file->title ();                       // Do title and status bar.
  jwp_stat.redraw ();
  return;
}

//--------------------------------
//
//  Geneates an UNDO_type object in the undo list
//
void JWP_file::undo_type () {
  UNDO_type *undo_type;
  if (!undo) return;                        // No undo buffer
  if (!(undo_type = new UNDO_type)) return; // Allocate undo object
  undo_clear ();                            // Setup new chanin
  undo_type->type = filetype;               // Save file type and name.
  if (!(undo_type->data = strdup(name))) { delete undo_type; return; }
  if (!queing) undo_new ();                 // Push the data.
  undo_push (undo_type);
  return;
}

//
//  End Class UNDO_type
//
//===================================================================

//===================================================================
//
//  Begin class JWP_file:
//

//--------------------------------
//
//  This routine implements the Edit/Redo command.
//
void JWP_file::do_redo () {
//
//  Check for valid redo list list not necessary because of menu selection.
//  undo_menu will disable the menu item, and thus the keyboard shortcut.
//
  if (!redo) return;
  redoing = true;
  undo_pop (redo);
  redoing = false;
  return;
}

//--------------------------------
//
//  This is the user entry point.  This implements the Edit/Undo command.
//
void JWP_file::do_undo () {
//
//  Check for valid undo list list not necessary because of menu selection.
//  undo_menu will disable the menu item, and thus the keyboard shortcut.
//
  undoing = true;                               // We are in an actuall undo.
  if (!redo) redo = undo_alloc(NULL,0);         // Make a redo list if not one.
  undo_pop (undo);                              // Call the real working routine.
  undoing = false;                              // Out of the undo
  return;
}

//--------------------------------
//
//  Converts a paragarph number to an actuall paragraph pointer.  Use 
//  of paragraph number in necessary in the undo system, becuase if we 
//  used pointers, as the file was edited, the pointers would changes,
//  and we would be unable to undo te changes.
//
//      number -- Paragraph number, counted from the start of the file (0).
//
//      RETURN -- Pointer to the indicated paragraph.
//
Paragraph *JWP_file::number_to_para (int number) {
  int i;
  Paragraph *para;
  for (i = 0, para = first; i < number; i++, para = para->next);
  return (para);
}

//--------------------------------
//
//  Converts an actual paragarph painter to a paragraph number.  Use 
//  of paragraph number in necessary in the undo system, becuase if we 
//  used pointers, as the file was edited, the pointers would changes,
//  and we would be unable to undo te changes.
//
//      number -- Pointer to the indicated paragraph.
//
//      RETURN -- Paragraph number, counted from the start of the file (0).
//
int JWP_file::para_to_number (Paragraph *para) {
  int i;
  Paragraph *p;
  for (i = 0, p = first; p != para; p = p->next, i++);
  return (i);
}

//--------------------------------
//
//  Clears the contents of the redo list.  This is actually blocked if 
//  you are in the state of redoing, or undoing.
//
void JWP_file::redo_clear () {
  if (redoing || undoing) return;
  undo_free (redo,jwp_config.cfg.undo_number);
  redo = NULL;
  undo_menu ();
  return;
}

//--------------------------------
//
//  This routine cleas the undo_accum flag, which indicates the type 
//  of changes currently being accumulated.  If the flag matches the 
//  imput ID then it is not cleared (continue accumulating) this type 
//  of change.  The ID value UNDO_NONE may be used to clear the 
//  value of the flage in all cases.
//
void JWP_file::undo_clear (int id) {
  if (undo_accum != id) undo_accum = UNDO_NONE; 
  return;
}

//--------------------------------
//
//  Ends a level of passive undo envent quing.  Note that calls to 
//  undo_start()/undo_end() are nested, and thus should be  matched.
//  the undo_end() routine is relaxed, in that it can be called more 
//  times than necessary without generating an error.  Additonally, 
//  there is a nesting limit of 255 levels (but it should never get 
//  deeper than 2 or 3).
//
void JWP_file::undo_end () {
  if (queing) queing--;
  return;
}

//--------------------------------
//
//  Intialize the undo buffer based on an old length.
//
//      old_length -- Old length of buffer (may be zero).
//                    
//  New length is determined from the settings in jwp_config.
//
void JWP_file::undo_init (int old_length) {
  if (!old_length) undo = NULL;
  undo = undo_alloc(undo,old_length);
  return;
}

//--------------------------------
//
//  Updates the state of the Edit/Undo and Edit/Redo menu itmes to 
//  reflect the state of the buffers.
//
// ### This routine is actually call too often and if performence is 
// ###   determined to becomen importaint, we can thin out the calls 
// ###   to this routine.
//
void JWP_file::undo_menu () {
  EnableMenuItem (hmenu,IDM_EDIT_UNDO,(undo && undo[0]) ? MF_ENABLED : MF_GRAYED);
  EnableMenuItem (hmenu,IDM_EDIT_REDO,(redo && redo[0]) ? MF_ENABLED : MF_GRAYED);
  return;
}

//--------------------------------
//
//  Generates a new undo chain.  Depending on weather we are accumulating
//  undo data or doing an undo, the new chain is generated in the 
//  undo list or in the redo list.
//  
void JWP_file::undo_new () {
  int           i;
  UNDO_action **ptr;
  redo_clear ();
  ptr = undoing ? redo : undo;
  if (!ptr) return;
  free_chain (ptr[jwp_config.cfg.undo_number-1]);
  for (i = jwp_config.cfg.undo_number-2; i >= 0; i--) ptr[i+1] = ptr[i];
  ptr[0] = NULL;
  return;  
}

//--------------------------------
//
//  This is the reall workhorse of the undo/redo actions.  This actually 
//  perofrmes the undo or redo action.  Setup for this rotuine must be 
//  performed by the do_undo() or do_redo() rotuines.
//
//      list -- Indicates the list to be processed (JWP_file::undo for
//              and undo and redo for a redo).
//
void JWP_file::undo_pop (UNDO_action **list) {
  int          i;
  UNDO_action *action;
  selection_clear ();                           // Clear selected text because won't make sense after.
  undo_start      ();                           // Accumulate redo infomration.
  for (action = list[0]; action; action = action->next) action->undo (this);    // Undo a chain!
  undo_end        ();                           // End accumulate
  free_chain (list[0]);                         // Dispose of used undo chain.
  for (i = 1; i < jwp_config.cfg.undo_number; i++) list[i-1] = list[i];
  list[jwp_config.cfg.undo_number-1] = NULL;
  undo_menu  ();                                // Update undo menu.
  adjust     ();                                // Make sure the display is ok.
  view_check ();
  redraw_all ();
  return;
}

//--------------------------------
//
//  Add an action to the current undo chain   Depending on the current
//  state (in an undo, in a redo, etc.) the action canbe added to the 
//  undo or redo lists.
//
//      action -- Action to be added.
//
void JWP_file::undo_push (UNDO_action *action) {
  UNDO_action **ptr;
  ptr = undoing ? redo : undo;
  action->next = ptr[0];
  ptr[0]       = action;
  undo_menu ();
  return;
}

//--------------------------------
//
//  Start passive quing of undo events.  Note that calls to undo_start()
//  are nested, and must be followed by a matching call to undo_end().
//
void JWP_file::undo_start () {
  if (!undo || queing++) return;
  undo_accum = UNDO_NONE;
  undo_new ();
  return;
}

//
//  End Class JWP_file
//
//===================================================================



