//-------------------------------------------------------------------//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998,1999,2000.             //
//  All rights reserved.                                             //
//                                                                   //   
//  These routines are intended to interface with EDICT, which is    //
//  a Japanese/English Dictionary developed and copyrighted by       //
//  James William Breen.                                             //
//                                                                   //   
//-------------------------------------------------------------------//

//-------------------------------------------------------------------
//
//  This module implements the dictionary search and mantinance routines.
//  These handle all interactions with the dictionaries and with searching.
//
//  The main dicitonary procedure involves searching Jim Breen's Japanese-
//  English Dictionary based on the index file used by JWP.  
//
//  Understanding the index file:
//
//  The dictionary is index by a string based on a EUC character 
//  combination.  The index is based on ascii characters of three 
//  or more chracters, kana strings of 2 or more kana, and single 
//  kanji.  All of the indexs are mixxed togetther and sorted in
//  numberical order, allowing use of a binary search.  Each index
//  points to a match string location in the dictionary file.  Note 
//  that these matches may actually be in the middle of a dictionary
//  entry.
//
//  The index has some odd characteristics.  Kanji and ascii are 
//  indexed wherever they occure in the text, but kara strings are only
//  indexed when they begin a word!
//
//  A search is conducted by performing a binary search on the index.  
//  For each index value we get the line from the actual dictionary and
//  compare with our key.  When we find the first match that matches 
//  our key, we have the beginning of our actuall data.  We then containue
//  getting the next index, and then the next data from the dicitionary,
//  until we get to the point when the lines gotten do not match the key.
//
//  Understanding dicitioanry entries:
//
//  Each entry inthe dictionary is a EUC line of text, terminated by a
//  cr-lf pair. (I do not count on this actually being a cr-lf, and can 
//  handle any comination [cr, lf, lf-cr, cr-lf].).  The format of each
//  line depends on weather or not there is kanji for the word in 
//  in quetion:
//
//      <kanji> [<kana>] /<defintion>/<definition>/ ...
//
//              or if there is no kanji:
//
//      <kana> /<definition>/<definition>
//
//  The index seach will give us the location of what we are searching 
//  for within a line of text (definititely not necessarily at the 
//  actual beginning of the line).  To handle this problem i use a 
//  centered buffer approche (see below).  Generally, we backup to the 
//  beginning of the line.  
//
//  Each of the <definition>s contains a complete definition of the 
//  word.  Since words are offten used to mean different things, these
//  defintions can vary a lot.  Within each definition, however, there
//  may be key string.  This string:
//
//          (<key>,<key>,...)
//
//  Tells us soemthing about the type of defintiion.  For example, the 
//  string (pl,pn,giv,fem) tells us that this entry is a place name, and
//  a female given name.  
//
//  I allow selective searching based on the type of entry.  The main 
//  desire of this is to allow filtering of personal names, and place 
//  names.  For example searching on the kanji NI (two) you get 738 
//  entries.  Removing place names and personal names, reduces this to
//  277 entries.  
//
//  Filtering keys correctly took a lot of work.  See the routine 
//  JWP_dict::search_dict() for a detailed descritption of the 
//  filtering process.
//
//  The dictionary flags:
//
//  The dictionary search rotoutines use flags that are stored in 
//  the global configuration.  These flags can be preserved over a 
//  instances of JWPce:
//
//      dict_auto     -- Causes automatic searching of the dictionary.
//      dict_compress -- Causes results display to be use a compressed 
//                       format.  This allows more lines to be visible,
//                       but makes them harder to read.
//
//  The rest of the flags are stored as bit field in the dict_bits
//  area.  These bits correspond to the rejected fields of the dict_keys
//  and are unrolled into those fields when the dictionary is open and 
//  put back when the dictionay dialog is closed.
//
//  The dominate keys are:
//
//      DICTKEY_END    -- Results must match the end of a word.
//      DICTKEY_BEGIN  -- Reuslts must match the beginning of a word.
//      DICTKEY_NAMES  -- Rejects all personal names.
//      DICTKEY_PLACES -- Rejects all place names.
//
//  User dictionary:
//
//  The user dictionary is kept in the same format as EDICT.  When the 
//  dictionary is loaded a '\n' character is placed in front of the 
//  actual dictionary.  This allows the search backward in the file to
//  correctly process the first entry in the file.  This is handled in 
//  in the main dictionaries by filling the dictionary buffer with '\n'
//  characters before loading short dictionary entries.
//
//  The dictionaries file:
//
//  The dicitonaries to be searched are stored in a file called "dict.cfg".
//  This file is actually an EUC file with UNIX end of line markers.  
//  Each line indicates a dictionary loaded in the system, and how to
//  perform a search on the dictionary.  The line format is a fllows:
//
//      <S| ><I| ><N| ><O| ><file><TAB><description><\n>
//
//  Only the description allows EUC code, all of the rest of the line
//  is simple ascii.  Additionally, tabs should not be inlcuded in the 
//  description, because they will mess up the list editor.
//
//  The SINO flags are as follows:
//
//      S -- Search this dictionary (space is disabled).
//      I -- Dicitionary has a index file <file>.jdx.
//      N -- Dicitionary contains names.
//      O -- Dicitionary contains only names.
//
#include "jwpce.h"
#include "jwp_clip.h"
#include "jwp_conf.h"
#include "jwp_conv.h"
#include "jwp_dict.h"
#include "jwp_edit.h"
#include "jwp_file.h"
#include "jwp_help.h"
#include "jwp_inpt.h"
#include "jwp_misc.h"

//#define DICT_DEBUG    // Used for debugging the NT dictionary problem -- REMOVE WHEN DONE!

void do_help (HWND hwnd,int id);
//===================================================================
//
//  Compile time options.
//
#if 1                       // If this is zero, dictionaries that are active (will
  #define HL(x) (!(x))      //   be searched) are highlighted in hightlight color.
#else                       //   If this is set to zero, dictionaries that are not
  #define HL(x) (x)         //   searched will be highligted.
#endif

#define DROP_AUTOADD        // Definining this value changes the way dictioanries 
                            //   that are droped on the dictionaries dialog are 
                            //   processed.  If this value is defined, the dictionaries
                            //   are automatically added to the list, without opening
                            //   the edit dictionary dialog.  If this is not defined 
                            //   the edit dictionary dialog is open for each file 
                            //   dropped.  Note, when an error occures, the edit
                            //   dictionary dialog will still be open.

#define MAX_KEY_LENGTH  100 // Determines max number of character we allow 
                            //   a search on. If the user search string is 
                            //   too long, we truncate to this level.

//-------------------------------------------------------------------
//
//  Compile time options.
//
#define NAME_MAINDICT       TEXT("edict")       // Name of dictionary file.
#define NAME_NAMEDICT       TEXT("enamdict")    // Name of the name dictionary.
#define NAME_USERDICT       TEXT("user.dct")    // Name of user dictionary file.
#define NAME_CLASSICAL      TEXT("classical")   // Name of classical dictionary.
#define NAME_DICTIONARIES   TEXT("jwpce.dic")   // Name of dictionaries file.

#define SIZE_LINE       512                     // Offest into the buffer for the search point.
#define SIZE_DICTBUFFER 1024                    // Size of the buffer read from the dicitonary.

#define HIRAGANA_A      0x2422      // Hiragana values used in the advanced search.
#define HIRAGANA_I      0x2424      //   These values are simply taken from the 
#define HIRAGANA_U      0x2426      //   JIS table and are included here because 
#define HIRAGANA_KU     0x242f      //   it is easy to do so.
#define HIRAGANA_GU     0x2430
#define HIRAGANA__TU    0x2443
#define HIRAGANA_TSU    0x2444
#define HIRAGANA_NU     0x244c
#define HIRAGANA_FU     0x2455
#define HIRAGANA_BU     0x2456
#define HIRAGANA_MU     0x2460
#define HIRAGANA_RU     0x246b
#define HIRAGANA_WA     0x246f
#define HIRAGANA_N      0x2473

//-------------------------------------------------------------------
//
//  Definitions used with the diconary tracking feature.  
//
//  This is a special research version of the program that can track 
//  dictonary requests for a research project.  This is not something 
//  most people should even bother to look at.
//
#ifdef DICTIONARY_TRACKING

#define NAME_TRACKING       "track.def"     // Name of traking definition file.

static char track_file[SIZE_BUFFER] = {0};  // Name of tracking file.
static char track_log [SIZE_BUFFER] = {0};  // Name of file user must edit to enable tracking.

static int track_set = false;               // Status flag, true when data is ready to write.
static KANJI track_buffer[4*SIZE_BUFFER];   // Buffer to hold user line from tracking.


//
//  Initialization procedrue, this checks for the tracking definition 
//  file and if it can find it opens the file and setsup the tradking 
//  information.
//
static void TRACKING_INIT () {
  static int init = false;                  // Prevents init from being called twice.
  char *ptr;
  FILE *file;
  if (init) return;
  init = true;
  if (!(file = fopen(jwp_config.name(NAME_TRACKING,OPEN_READ,false),"r"))) return;
  fgets  (track_log,SIZE_BUFFER,file);      // Get log file.
  fgets  (track_file,SIZE_BUFFER,file);     // Get working file name.
  for (ptr = track_log ; *ptr; ptr++) if (ISCRLF(*ptr)) *ptr = 0;
  for (ptr = track_file; *ptr; ptr++) if (ISCRLF(*ptr)) *ptr = 0;
  fclose (file);
  return;
}

//
//  This is the record end of the tracking routine.  This will recored
//  the data if the confitions are correct.
//
//      matches -- Indicates the number of matches recorded on the 
//                 last dictionary search.
//
static void TRACKING_LOG (int matches) {
  int i;
  FILE *file;
  KANJI *kptr;
  if (matches || !track_set) return;            // Search was success or no data ready
  track_set = false;                            // Clear the data, we are done with it
  for (i = 0; i < 20; i++) {                    // Make up to 20 attempts to open the log file.
    if (file = fopen(track_log,"ab")) break;
  }
  if (!file) return;                            // Still not file, then abort.
  for (kptr = track_buffer; *kptr; kptr++) {    // Write the string to the buffer.
    if (!ISJIS(*kptr)) fputc (*kptr,file);
      else {
        fputc (0x80 | (*kptr >> 8  ),file);
        fputc (0x80 | (*kptr & 0xff),file);
      }
  }
  fputc ('\r',file);
  fputc ('\n',file);
  fclose (file);                                // Close the file.
  return;
}

//
//  Called fromthe dictionary routine, this is just a stub that calls
//  the JWP_file class routine.  Since this is a special case, I did 
//  not code in an exception to the class routines.
//
//  The function of this routine is to setup the user's data in the 
//  tracking buffer.  If the data is valid for the log file TRACKING_LOG
//  will later record it in the file.
//
//      file -- File being edited, this tells us who to track.
//
static void TRACKING_SET (JWP_file *file) {
  track_set = file->dictionary_track (track_buffer);
  return;
}

int JWP_file::dictionary_track (KANJI *buffer) {
  int i;
  if (stricmp(track_file,name)) return (false);         // Wrong file type
  if (!sel.type) return (false);                        // No mark.
  if (sel.pos1.para != sel.pos2.para) return (false);   // Select cannot be pasted.
  all_abs ();
  for (i = sel.pos1.pos-5; i < sel.pos1.pos; i++) {
    if (i < 0) *buffer++ = 0x2179; else *buffer++ = sel.pos1.para->text[i];
  }
  *buffer++ = '\t';
  for (; i < sel.pos2.pos; i++) {
    *buffer++ = sel.pos1.para->text[i];
  }
  *buffer++ = '\t';
  for (; i < sel.pos2.pos+5; i++) {
    if (i >= sel.pos1.para->length) *buffer++ = 0x217a;
    else                            *buffer++ = sel.pos1.para->text[i];
  }
  *buffer++ = 0;
  all_rel ();
  return (true);
}

#else
  #define TRACKING_INIT()           // Stub routines to blank out the tracking when not used.
  #define TRACKING_SET(x)
  #define TRACKING_LOG(x)
#endif DICTONARY_TRACKING

//-------------------------------------------------------------------
//
//  static data and definitions.
//
//  Most of these have to do with the dict_key structure, and a the dict_keys
//  variable.  These simply keep track of conditions for filtering a dictionary
//  entry.  The dict_keys array has two false entries in the front.  These 
//  entries are for use in the Dictionary Options dialog.
//

#if 0 // ### I really wanted to make the JWP_dict class allocated on the fly, 
      //       when I tried to do this, however, I found a bug in MSVC++, and
      //       could not get around it by disabling the optimiziation, so I have
      //       gone back to the static allocation.  This worked in Debug mode,
      //       but not in Release mode.
      
static JWP_dict *jwp_dict = NULL;   // Static instance of the dictionary
                                    //   class.  This is used to hold the
                                    //   current instance so dialog box 
                                    //   procedure can access it.  Additonally,
                                    //   if this value is non-NULL, you are 
                                    //   alread in the dictionary.
#endif

//
//  These variables were moved here from the class because the class item
//  is now allocated on the fly, thus to allow these variables to be 
//  used over more than one instance.  Using this method, the these 
//  files can be loaded the first time the dictionary is used and remain
//  in memory until the system is shutdown.
//
static byte *user_dict    = NULL;   // This is the user dictionary.  This is the 
                                    //   user's own private dictionary.
static byte *dictionaries = NULL;   // This is the dictonaries file.  This file
                                    //   contains information about the suplimental
                                    //   dictionaries that are to be searched.

#define NUMBER_DICTKEYS ((int) (sizeof(dict_keys)/sizeof(struct dict_key)))

struct dict_key {       // Structure for keeping track of filtering conditions.
  char   key[5];        // Key as is containned in a directory entry.
  byte   reject;        // Non-zero if this key is to be filtered out.
  short  text;          // Text description of the key for the options dialog.
};

//
//  These are various search elemetns that you can optionally search on.
//  This a keys specified in the dictionary.  Commeted out thins are 
//  believed (by me) to not be worth it.
//  
//
struct dict_key dict_keys[] = {
  { "/({[",0,IDS_DICT_BEGIN                                                    },
  { "/({[",0,IDS_DICT_END                                                      },
  { "s"   ,0,IDS_DICT_PERSONALNAMES                                            },
  { "p"   ,0,IDS_DICT_PLACENAMES                                               }, 
  { "vulg",0,IDS_DICT_VULGAR                                                   },
  { "X"   ,0,IDS_DICT_X                                                        },
  { "col" ,0,IDS_DICT_COLLOQUIAL                                               },
  { "m-sl",0,IDS_DICT_MANGA                                                    },
  { "sl"  ,0,IDS_DICT_SLANG                                                    },
  { "MA"  ,0,IDS_DICT_MARIAL                                                   },
  { "id"  ,0,IDS_DICT_IDIOMATIC                                                },
  { "arch",0,IDS_DICT_ARCHAIC                                                  },
  { "obs" ,0,IDS_DICT_OBSOLETE                                                 },
  { "obsc",0,IDS_DICT_OBSCURE                                                  },
  { "ok"  ,0,IDS_DICT_KANA                                                     },
  { "abbr",0,IDS_DICT_ABBREVIATION                                             },
  { "fam" ,0,IDS_DICT_FAMILIAR                                                 },
  { "pol" ,0,IDS_DICT_POLITE                                                   },
  { "hum" ,0,IDS_DICT_HUMBLE                                                   },
  { "hon" ,0,IDS_DICT_HONORIFIC                                                },
//{ "an"  ,0,TEXT("adjectival nouns or quasi-adjectives (keiyoudoshi)")        },
//{ "a-no",0,TEXT("nouns which may take the genitive case particle no")        },
//{ "vs"  ,0,TEXT("nouns or participles which take the auxillary verb suru")   },
//{ "vt"  ,0,TEXT("transitive verbs")                                          },
//{ "vi"  ,0,TEXT("intransitive verbs")                                        },
//{ "I"   ,0,TEXT("Type I (godan) verbs (only when type is not implicit)")     },
//{ "IV"  ,0,TEXT("Type IV (irregular) verbs, such as gosaru")                 }, 
  { "fem" ,0,IDS_DICT_FEMALE                                                   },
  { "male",0,IDS_DICT_MALE                                                     },
  { "pref",0,IDS_DICT_PREFIX                                                   },
  { "suf" ,0,IDS_DICT_SUFFIX                                                   },
//{ "uk"  ,0,TEXT("words usually written in kana alone")                       },
//{ "uK"  ,0,TEXT("words usually written in kanji alone")                      },
  { "oK"  ,0,IDS_DICT_KANJI                                                    },
//{ "ik"  ,0,TEXT("words containing irregular kana usage")                     },
//{ "iK"  ,0,TEXT("words containing irregular kanji usage")                    },
//{ "io"  ,0,TEXT("words words containing irregular okurigana usage")          },
};

//-------------------------------------------------------------------
//
//  static routines.
//
static int   dict_comp   (byte *key,byte *ptr,int n);   // Compare a key against a dictionary entry.
static byte *format_line (EUC_buffer *line,byte *data,int user,int classical);  // Format a dictionary line for display.
static byte *load_dict   (tchar *name);                 // Make memory image of a dictionary file.
static int   test_key    (byte *ptr,char *key);         // Test a string against being a dictionary type key.

//
//  Dialog procedure for the dictionary.  This is bascially a stub that 
//  calls JWP_dict::dlg_dictionary.
//
static BOOL CALLBACK dialog_dictionary (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_dict.dlg_dictionary(hwnd,message,wParam,lParam));
}

//
//  Dialog procedure for the dictionary options.  This is bascially a stub that 
//  calls JWP_dict::dlg_dictoptions.
//
static BOOL CALLBACK dialog_dictoptions (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_dict.dlg_dictoptions(hwnd,message,LOWORD(wParam)));
}

//
//  Copare a dictionary entry against a key.
//
//      key    -- Key to be compared agianst (ascii should be in lower case,
//                and kana should be converted hiragana!).
//      ptr    -- Pointer to data from the dictionary file.
//      n      -- Length limit.  A value of 0 can be used to unlimit the 
//                comparison (go unitl NULL is reached).
//
//      RETURN -- Zero indicates the key and data match.  A non-zero value
//                indicates they don't match.  Positive indicates key is 
//                later than the data.  Netgitive indicates oposite.
//
static int dict_comp (byte *key,byte *ptr,int n) {
  int  i;
  byte c1,c2;
//  BOOL f1 = false;                              // Flags indicate the first byte of a JIS character code
  BOOL f2 = false;                   

  for (i = 0; ; i++) {
    if (n && (i >= n)) return (0);              // If using limted compare, we can stop when we readch the full length of the string.
    c1 = key[i];
    c2 = ptr[i];
//  if (('A' <= c1) && (c1 <= 'Z')) c1 += 32;   // Key is alraedy in lower case.      
    if (('A' <= c2) && (c2 <= 'Z')) c2 += 32;
//    if (!(c1 & 0x80)) f1 = false;             // Code no longer used because the key is converted to
//      else {                                  //   hiragana in advance!
//        f1 = !f1;
//        if (f1 && (c1 == 0xa5)) c1 = 0xa4;
//    }
    if (!(c2 & 0x80)) f2 = false;
      else {
        f2 = !f2;
        if (f2 && (c2 == 0xa5)) c2 = 0xa4;      // Katakana -> hiragana
      }
    if (c1 != c2) return (c1 - c2);
    if (c1 == 0) return (0);
  }
  return (c1 - c2);
}

//
//  This routine fromats a dictionary line for display.  This is used
//  by the user dictionary editor and the main dictonary routines.
//
//      line      -- EUC_buffer object used to access the list box.
//      data      -- Poiner into the dictionary line (not the beginning 
//                   of the line).
//      user      -- Set to true for the user dictionary editor.  This 
//                   allows the slash characters to remain in the line.
//      classical -- Indicates this is the classical dictionary.  This 
//                   allows EUC characters in the meaning field.  The 
//                   consequence of this is you cannot have extended
//                   ascii in this field.
//
//      RETURN -- The return value is a pointer to the next dictionary 
//                line element.  This is used in the user dictionary to 
//                advance through the dictionary.  If an error occures 
//                in parsing the dictionary, a value of NULL is returned 
//                to indicate an error.
//
static byte *format_line (EUC_buffer *line,byte *data,int user,int classical) {
  int ch,i;
  int first_line = true;
  line->clear ();                               // Intialize line buffer.
  for (i = 0; i < SIZE_LINE; i++) {             // Limit string length.
    if (!*data || ISCRLF(data[1])) break;       // End of line so exit, or error condition (past end of buffer)
    if ((*data & 0x80) && (classical || first_line)) {  // Output kanji/kana character.
      ch = *data++;
      line->put_char ((ch << 8) | *data);
    }
    else if (first_line) {                      // First line has special characters.
      switch (*data) {
        case '[':
             line->put_char (jwp_config.cfg.dict_compress ? '[' : KANJI_LBRACKET);
             break;
        case ']':
             line->put_char (jwp_config.cfg.dict_compress ? ']' : KANJI_RBRACKET);
             break;
        case ' ':
             line->put_char ('\t');
             break;
        case '/':                               // First '/' indicates end of first line, just text after here.
             first_line = false;
             if (!jwp_config.cfg.dict_compress) line->flush (-1);
             break;
        default:
             line->put_char (*data);
             break;
      }
    }
    else if (!user && (*data == '/')) {         // After first line just output, but change '/' into ', '.
      line->put_char (',');
      line->put_char (' ');
    }
    else line->put_char (*data);
    data++;
  }
  line->flush (-1);                             // Flush last line.
  data += 2;                                    // Calculate location of next line.
  if (ISCRLF(*data)) data++;
  return (data);
}

//
//  Utility routine to load a dictionary file from disk if the file
//  is available.  The routine does not deal with errors.
//
//      name -- Name of file to load.
//
static byte *load_dict (tchar *name) {
  int    i;
  byte  *dict;
  HANDLE handle;
  unsigned long done;

  handle = CreateFile(name,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null);
  if (handle == INVALID_HANDLE_VALUE) return (NULL);
  i = GetFileSize(handle,NULL);
  if ((dict = (byte *) calloc(1,i+5))) {
    dict[0] = '\n';                          // Make it look like there is an earlier 
    ReadFile(handle,dict+1,i,&done,NULL);    //   entry (so we can search to beginning)
  }
  CloseHandle (handle);
  return (dict);
}

//
//  This is a utility routien used by the entry type filtering in the 
//  JWP_dict::search_dict() routine.  This routine attempts to see if 
//  the string pointed to matches a articular dictionary type key.  The
//  conditions for such a match are to match the charcters in the key, 
//  and be followed by a ',' or a ')'.
//
//  Note the first character of the input string is skiped, because it
//  is assumed that this is either '(', or ','.
//
//      ptr -- String to be tested.
//      key -- Key to test against (null termianted ascii string).
//
static int test_key (byte *ptr,char *key) {
  int len = strlen(key);
  if (strncmp((char *) ++ptr,key,len)) return (0);
  if ((ptr[len] == ',') || (ptr[len] == ')')) return (len+1);
  return (0);
}

//-------------------------------------------------------------------
//
//  Exported routines.
//

//
//  This is a rotuine called during clean up to deallocate memory helded
//  by the dictionary routines.
//
void free_dictionary () {
  if (dictionaries) free (dictionaries);    // Dicitonaries files
  if (user_dict) free (user_dict);          // User dictionary.
  return;
}

//-------------------------------------------------------------------
//
//  Begin user dictionary edit routines.
//
//  These routines handler the user dictionary and are basically 
//  operations supporting the EDIT_userdict class, which is dirived 
//  from the EDIT_list class.
//

//```````````````````````````````````````````````````````````````````
//
//  Class definition.
//

class EDIT_userdict : public EDIT_list {
public:
  int   dlg_edituser(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
  byte *get_data    (void);                     // Get data from the list into working format.
  void  put_data    (byte *data,tchar *name);   // Put data into the dialog box for initialize.
private:
  int   edit        (void);                     // Edit/Add entry procedure.
};

static class EDIT_userdict *edit_userdict = NULL;   // Pointer to class instance so dialog procedure can find us.

//```````````````````````````````````````````````````````````````````
//
//  Static routines.
//

//
//  Dialog box stub for the main dialog, simply calles the class rotuine.
//
static int dialog_edituser (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (edit_userdict->dlg_edituser(hwnd,message,wParam,lParam));
}

//
//  Dialog procedure for editing the user dictionary.  This a stub that 
//  calls JWP_dict::dlg_userdict
//
static BOOL CALLBACK dialog_userdict (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_dict.dlg_userdict(hwnd,message,wParam,lParam));
}

//```````````````````````````````````````````````````````````````````
//
//  class EDIT_userdict.
//

//
//  Dialog box procedure for editing an entry in the user dictionary.
//
//      IDC_DEKANA        Kana edit box (required)
//      IDC_DEKANJI       Kanji edit box (optional)
//      IDC_DEMEANING     Meaning edit box (required)
//
//      IDC_DDSTRING      Search string from dictionary dialog box (used to initialize user dict)
//
int EDIT_userdict::dlg_edituser (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  int lkana,lkanji;
  KANJI *kkana,*kkanji;
  TCHAR *ptr,buffer[SIZE_BUFFER];
  int i,j;

  switch (message) {
//
//  Initialize the dialog box.  Read the data from the kbuffer, and 
//  setup the dialog parameters.
//
    case WM_INITDIALOG:
         if (length) {          // this is an EDIT, so initialize the editor.
           for (i = 0; kbuffer[i] != '\t'; i++);
           i++;         // kana only.
           if ((kbuffer[i] != KANJI_LBRACKET) && (kbuffer[i] != '[')) {
             SendDlgItemMessage (hwnd,IDC_DEKANA,JE_SETTEXT,i-1,(LPARAM) kbuffer);
           }
           else {       // Kana and kanji.
             SendDlgItemMessage (hwnd,IDC_DEKANJI,JE_SETTEXT,i-1,(LPARAM) kbuffer);
             for (i++, j = 0; kbuffer[i+j] != '\t'; j++);
             SendDlgItemMessage (hwnd,IDC_DEKANA,JE_SETTEXT,j-1,(LPARAM) (kbuffer+i));
             i += j+1;
           }            // Setup meaning.
           for (j = 0; i < length; i++) buffer[j++] = (char) kbuffer[i];
           buffer[j] = 0;
           SetDlgItemText (hwnd,IDC_DEMEANING,buffer);
         }                      
         else {                 // This is an ADD so initialize from the Dicitionary Dailog Box.
           lkana = JE_GetText(jwp_dict.dialog,IDC_DDSTRING,&kkana);
           for (i = 0; i < lkana; i++) if (ISKANJI(kkana[i])) break;
           SendDlgItemMessage (hwnd,(i == lkana) ? IDC_DEKANA : IDC_DEKANJI,JE_SETTEXT,lkana,(LPARAM) kkana);
         }
         return (true);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_USEREDIT);
         return  (true);
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           INPUT_CHECK (IDC_DEMEANING);
//
//  User wants to keep this conversion, so see what is up.
//
           case IDOK:
//
//  Get strings and make sure they are not empty.
//
                lkana  = JE_GetText(hwnd,IDC_DEKANA ,&kkana);
                lkanji = JE_GetText(hwnd,IDC_DEKANJI,&kkanji);
                for (j = false, i = 0; i < lkana; i++) {    // Check for error caracters.
                  if (kkana[i] == ' ') break;
                  if (!ISKANA(kkana[i]) && (kkana[i] != KANJI_LONGVOWEL) && (kkana[i] != KANJI_TILDE)) j = true;
                }
                if (!lkana || (i != lkana)) {               // Kana string is empty or cotnains space -> ERROR!
                  JMessageBox (hwnd,IDS_DE_ERRORKANA,IDS_DE_ERROR,MB_OK | MB_ICONERROR);
                  SetFocus   (GetDlgItem(hwnd,IDC_DEKANA));
                  return (0);
                }                                           // Kana string contains non-kana chracters -> WARNING!
                if (j && (IDNO == JMessageBox(hwnd,IDS_DE_ERRORNONKANA,IDS_DE_ERROR,MB_YESNO | MB_ICONWARNING))) {
                  SetFocus (GetDlgItem(hwnd,IDC_DEKANA));
                  return   (0);
                }
//
//  Build buffer string.
//
                length = 0;
                if (!lkanji) {              // Just kana
                  for (i = 0; i < lkana; i++) kbuffer[length++] = kkana[i];
                }
                else {                      // Kana and kanji.
                  for (i = 0; i < lkanji; i++) kbuffer[length++] = kkanji[i];
                  kbuffer[length++] = '\t';
                  kbuffer[length++] = jwp_config.cfg.dict_compress ? '[' : KANJI_LBRACKET;
                  for (i = 0; i < lkana; i++) kbuffer[length++] = kkana[i];
                  kbuffer[length++] = jwp_config.cfg.dict_compress ? ']' : KANJI_RBRACKET;
                }
                kbuffer[length++] = '\t';
//
//  Setup line break so when we put back the data it will format correctly.
//
                line_break = jwp_config.cfg.dict_compress ? 0 : length;
//
//  Get meaning and format.
//
                GetDlgItemText (hwnd,IDC_DEMEANING,buffer,SIZE_BUFFER);
                if (!lstrlen(buffer)) {      // Make sure the meaning is not empty.
                  JMessageBox (hwnd,IDS_DE_ERRORMEANING,IDS_DE_ERROR,MB_OK | MB_ICONWARNING);
                  SetFocus   (GetDlgItem(hwnd,IDC_DEMEANING));
                  return (0);
                }
#ifdef WINCE                                                                    // This was modified to support extended
                for (ptr = buffer; *ptr; ptr++) kbuffer[length++] = *ptr;       //   ascii in the dictionary searches.
#else  WINCE                                                                            
                for (ptr = buffer; *ptr; ptr++) kbuffer[length++] = *((byte *) ptr);
#endif WINCE
                EndDialog (hwnd,true);
                return (0);
//
//  Didn't want to do it after all.
//
           case IDCANCEL:
                EndDialog (hwnd,false);
                return (0);
         }
  }
  return (false);
}

//
//  Required virtual function to edit an entry.  This just invokes the 
//  edit dialog box.  All of the real work is there.
//
int EDIT_userdict::edit () {
  return (JDialogBox(IDD_DICTUSEREDIT,dialog,(DLGPROC) dialog_edituser));
}

//
//  This routine gets the data from the from the list and converts it
//  into the format necessary for a dictionary file.  
//
//      RETURN -- Memory allocated copy of the buffer.  This is simply
//                the disk image of the string converted into a long string.
//
byte *EDIT_userdict::get_data () {
  int i,index;
  byte *data,*ptr;
                                                
  if (!(data = (byte *) calloc(1,size()))) { OutOfMemory (dialog); return (data); }
  ptr    = data;
  index  = 0;
  *ptr++ = '\n';                                // This is so the search will work correctly.
  while (index < count()) {                     // Whey there are items keep processing.
    get_buffer (index);                         // Get buffer.
//
//  Put first string (typically this is kanji, but could be kana).
//
    for (i = 0; kbuffer[i] != '\t'; i++) {      
      *ptr++ = (kbuffer[i] >> 8) | 0x80;
      *ptr++ = (kbuffer[i] & 0x00ff) | 0x80;
    }
    *ptr++ = ' ';                               // Separator.
    i++;
//
//  If have a selcond string we need to translate that.
//
    if ((kbuffer[i] == KANJI_LBRACKET) || (kbuffer[i] == '[')) {
      *ptr++ = '[';
      for (i++; (kbuffer[i] != KANJI_RBRACKET) && (kbuffer[i] != ']'); i++) {
        *ptr++ = (kbuffer[i] >> 8) | 0x80;
        *ptr++ = (kbuffer[i] & 0x00ff) | 0x80;
      }
      *ptr++ = ']';
      *ptr++ = ' ';
      i += 2;
    }
//
//  Put meanings, and terminating characters.
//
    *ptr++ = '/';
    for (; i < length; i++) *ptr++ = (byte) kbuffer[i];
    *ptr++ = '/';
//  *ptr++ = '\r';                              // UNIX termination is OK!
    *ptr++ = '\n';
    index  = next_item(index);                  // Move to next item.
  };
  return (data);
}

//
//  Required virtual fucntion.  This function takes a data block (from a file
//  or whatever) and puts it into the list box.
//
//      data -- Pointer to the data buffer.
//      name -- Name of data file being read.
//
void EDIT_userdict::put_data (byte *data,tchar *name) {
  if (*data == '\n') data++;        // This is used to skip extra '\n' at beggining of user dictionary.
  while (*data) {
    data = format_line (this,data,true,false);
  }
  return;
}

//```````````````````````````````````````````````````````````````````
//
//  class JWP_dict
//

//
//  Dialog box procedure used to edit the list.
//
#define RET     *((int *) lParam)

int JWP_dict::dlg_userdict (HWND hwnd,int msg,WPARAM wParam,LPARAM lParam) {
  switch (msg) {
    case WM_INITDIALOG:
         add_dialog (user_dialog = hwnd,false);
         if (!(edit_userdict = new EDIT_userdict())) return (false);
         edit_userdict->init (hwnd,user_dict,IDS_DE_FILETYPE);
         return (true);
    case WM_DESTROY:
         if (edit_userdict->changed) {
           if (IDYES == JMessageBox(hwnd,IDS_DU_CHANGEDTEXT,IDS_DU_CLOSE,MB_ICONWARNING | MB_YESNO)) save_user ();
         }
         SetFocus (GetParent(hwnd));
         user_dialog = null;
         remove_dialog (hwnd);
         delete edit_userdict;
         return (true);
//
//  Process message generated by system shut-down.
//
    case WMU_OKTODESTROY:
         if (!edit_userdict->changed) { RET = true; return (0); }
         switch (JMessageBox(hwnd,IDS_DU_CHANGEDTEXT,IDS_DU_CLOSE,MB_ICONQUESTION | MB_YESNOCANCEL)) {
           case IDNO:     RET = true;  break;
           case IDCANCEL: RET = false; break;
           case IDYES:    RET = !save_user(); break;
         }
         if (RET) edit_userdict->changed = false;
         return (0);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_USERDICT);
         return  (true);
#ifndef WINCE
    case WM_DROPFILES:                          // Drag & drop import dictionary
         edit_userdict->do_drop ((HDROP) wParam);
         return  (0);
#endif  WINCE
    case WM_COMMAND:
         switch (LOWORD(wParam)) {              // These events belong to the edit-list class.
           case IDC_EDITLIST:   
           case IDC_EDITLISTADD:
           case IDC_EDITLISTEDIT:
           case IDC_EDITLISTUP:
           case IDC_EDITLISTDOWN:
           case IDC_EDITLISTDELETE:
           case IDC_EDITLISTIMPORT:
           case IDC_EDITLISTINSERT:
                edit_userdict->do_event (wParam);
                return (0);
//
//  User wants to keep the list so output to a buffer.
//
           case IDOK:                           // User wants to keep changes.
                if (edit_userdict->changed && save_user()) return (0);
           case IDCANCEL:       // **** FALL THORUGH ****
                edit_userdict->changed = false; // Prevents a request to save the data.
                DestroyWindow (hwnd);
                return (0);
         }
         break;
  }
  return (false);
}

//
//  Small utility routine to save the user dictionary.  This was seperated out so 
//  the user dictionary could be saved in you close the parent window and the suer 
//  dictionary gets clobered.  This allows the main handler to ask you if you want
//  to save your user dicdtionary.
//
//      RETURN -- A non-zero value indicates a failure to save the dictionary.
//
int JWP_dict::save_user () {
  HANDLE file;
  unsigned long done;
  if (user_dict) free (user_dict);          // Remove old conversions.
  user_dict = edit_userdict->get_data ();   // Make current conversions active oones.
  if (INVALID_HANDLE_VALUE == (file = jwp_config.open(NAME_USERDICT,OPEN_NEW,true))) {
    QUIET_ERROR {
      edit_userdict->error (IDS_DE_ERROROPEN,jwp_config.name());
      return (true);                    
    }
  }                                       // Write conversions to disk.
  WriteFile   (file,user_dict+1,strlen((char *) user_dict+1),&done,NULL);
  CloseHandle (file);
  return      (false);
}

//
//  End User Dictionary Edit
//
//-------------------------------------------------------------------

//-------------------------------------------------------------------
//
//  Begin dictonaries list edit controls
//
//  These routines handle editing and chaning the dictionaries list.
//  The opperations are handled by a EDIT_dictionaries class, which 
//  is dirived from the EDIT_list class.
//

//```````````````````````````````````````````````````````````````````
//
//  Class definition.
//

class EDIT_dictionaries : public EDIT_list {
public:
  char *drop;           // Set to point to a string buffer when processing 
                        //   files droped onto the main dictionaries 
                        //   dailog.  This si a KLUDGE, used so we can reuse
                        //   the code already in the edit dictioary routine.
                        //   This will cause the dialog to setup an entry, but 
                        //   never display the dialog, but simply return the 
                        //   values as if the user had clicked yes.
  int   toggle;         // Set to true when invoiding edit command.  This 
                        //   is a KLUDGE used to implement the TOGGLE button.
                        //   This is done by setting this flag and invoking 
                        //   the edit command.
  int   dlg_editdict(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
  byte *get_data    (void);                     // Get data from the list into working format.
  void  put_data    (byte *data,tchar *name);   // Put data into the dialog box for initialize.
private:
//
//  Variables used to disassemble a line from the dictioanries database.
//
  int   descript;       // Index of the beginning of the description 
  int   indexed;        // This dictionary is indexed.
  int   names;          // This dicitonary contains names
  int   only;           // This dictionary contains only names
  int   path;           // Index of the start of the path information.
  int   search;         // Search this dictionary.
  void  add_string   (tchar *string);           // Add's a string to kbuffer.
  int   edit         (void);                    // Edit/Add entry procedure.
  void  parse_buffer (void);                    // Parse a kbuffer line.
  int   setup_file   (HWND hwnd,TCHAR *name);   // Setup dialog for a dictionary files.
  void inline add_string (int id) { add_string(get_string(id)); };   // Add's a string to kbuffer from the string table
};

static class EDIT_dictionaries *edit_dictionaries = NULL;   // Pointer to class instance so dialog procedure can find us.

//```````````````````````````````````````````````````````````````````
//
//  Static routines.
//

//
//  Dialog box stub for the main dialog, simply calles the class rotuine.
//
static int dialog_editdictionaries (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (edit_dictionaries->dlg_editdict(hwnd,message,wParam,lParam));
}

//
//  Stub procedure for calling ditionaries dialog box procedue
//
static BOOL CALLBACK dialog_dictionaries (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  return (jwp_dict.dlg_dictionaries (hwnd,message,wParam,lParam));
}

//```````````````````````````````````````````````````````````````````
//
//  class EDIT_dictionaries.
//

//
//  Small utilty routine used to add an ASCII string to the kbuffer.
//
//      string -- STring to add.
//
void EDIT_dictionaries::add_string (tchar *string) {
  while (*string) kbuffer[length++] = *string++;
  return;
}

//
//  Dialog box handler for dialog box to edit a selection in the 
//  dictionaries list.
//
//      IDC_DSENABLE      Seach check box
//      IDC_DSNAMES       Names check box
//      IDC_DSINDEX       Indexed check box
//      IDC_DSONLY        Only names check box
//      IDC_DSDESCRIPTION Descrition edit box.
//      IDC_DSFILE        Files edit box
//      IDC_DSBROWSE      Browse button.
//
int EDIT_dictionaries::dlg_editdict (HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
  KANJI *desc;
  TCHAR buffer[SIZE_BUFFER];
  int i,j;

  switch (message) {
//
//  Initialize the dialog box.  Read the data from the kbuffer, and 
//  setup the dialog parameters.
//
    case WM_INITDIALOG:
         if (length) {                              // This is an EDIT, so we have data.
           parse_buffer ();
           CheckDlgButton (hwnd,IDC_DSENABLE,search );
           CheckDlgButton (hwnd,IDC_DSNAMES ,names  );
           CheckDlgButton (hwnd,IDC_DSINDEX ,indexed);
           CheckDlgButton (hwnd,IDC_DSONLY  ,only   );
           for (i = path; i < length; i++) buffer[i-path] = (char) kbuffer[i];
           buffer[i-path] = 0;
           SetDlgItemText (hwnd,IDC_DSFILE,buffer);
           SendDlgItemMessage (hwnd,IDC_DSDESCRIPTION,JE_SETTEXT,descript,(LPARAM) kbuffer);
         }
         else {                                     // This is an ADD, so clear data.
           CheckDlgButton (hwnd,IDC_DSENABLE,true);
           CheckDlgButton (hwnd,IDC_DSNAMES ,true);
         }
#ifndef WINCE
         if (drop) {                                // User droped files onto the dictioanries 
           if (setup_file(hwnd,drop)) {             //   dialog.  At minimum, we want to 
             SetDlgItemText (hwnd,IDC_DSFILE,drop); //   intialize to the file the user 
             return (true);                         //   droped.  May want to complete the additon
           }                                        //   by jumping to the OK button handler.
#ifdef DROP_AUTOADD                                 // 
           goto DropFileExit;                       // If an error occures, the dialog will always 
#endif DROP_AUTOADD                                 //   be created.
         }
#endif WINCE
         return (true);
#ifndef WINCE
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_SUPPLEMENTAL);
         return  (true);
    case WM_DROPFILES:
         if (DragQueryFile((HDROP) wParam,0,buffer,SIZE_BUFFER) > 0) {
           setup_file (hwnd,buffer);
         }
         DragFinish ((HDROP) wParam);
         return (0);
#endif WINCE
    case WM_COMMAND:
         switch (LOWORD(wParam)) {
           INPUT_CHECK (IDC_DSFILE);
//
//  Get file name from requester
//
           case IDC_DSBROWSE: {
                  TCHAR filter[SIZE_BUFFER];
                  OPENFILENAME  ofn;
                  memset (&ofn,0,sizeof(ofn));
                  ofn.lStructSize       = sizeof(ofn);
                  ofn.hwndOwner         = hwnd;
                  ofn.hInstance         = instance;
                  ofn.lpstrFilter       = format_string(filter,IDS_DS_FILETYPE,0,0);    // Never could get it to do non-extension files.
                  ofn.nFilterIndex      = 1;
                  ofn.lpstrFile         = buffer;
                  ofn.nMaxFile          = SIZE_BUFFER;
                  ofn.Flags             = OFN_FILEMUSTEXIST  | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY  | OFN_EXPLORER;
#ifdef WINCE
                  ofn.lpstrInitialDir   = currentdir;           // Use Windows CE current directory
#endif WINCE
                  GetDlgItemText (hwnd,IDC_DSFILE,buffer,SIZE_BUFFER);
                  if (!GetOpenFileName(&ofn)) return (true);    // User canclled!
#ifdef WINCE
                  set_currentdir (buffer,true);                 // Set Windows CE current directory
#endif WINCE
//
//  Setup dialog box with a particular file in mind.
//  
                  setup_file (hwnd,buffer);
                }
                return (0);
//
//  These items are linked to each other.
//
           case IDC_DSNAMES:
                if (!IsDlgButtonChecked(hwnd,IDC_DSNAMES)) CheckDlgButton (hwnd,IDC_DSONLY,false);
                return (0);
           case IDC_DSONLY:
                if (IsDlgButtonChecked(hwnd,IDC_DSONLY)) CheckDlgButton (hwnd,IDC_DSNAMES,true);
                return (0);
//
//  User wants to keep this conversion, so see what is up.
//
#if (!defined(WINCE) && defined(DROP_AUTOADD))
DropFileExit:;
#endif
           case IDOK: {
                  TCHAR text[SIZE_BUFFER];
//
//  Get strings and make sure they are not empty.
//
                  j = JE_GetText(hwnd,IDC_DSDESCRIPTION,&desc);
                  GetDlgItemText (hwnd,IDC_DSFILE,buffer,SIZE_BUFFER);
                  if (!lstrlen(buffer) || !j) {
                    JMessageBox (hwnd,IDS_DS_ERROREMPTY,IDS_DS_ERROR,MB_OK | MB_ICONWARNING);
                    SetFocus (GetDlgItem(hwnd,j ? IDC_DSDESCRIPTION : IDC_DSFILE));
                    return (0);
                  }
//
//  Build buffer string.
//                                      // Process descritpion and convert '\t'->KANJI_SPACE so formatting is right.
                  for (length = 0; length < j; length++) kbuffer[length] = (desc[length] != '\t') ? desc[length] : KANJI_SPACE;
                  kbuffer[length++] = '\t';
                  line_break = length;
                  if (           IsDlgButtonChecked(hwnd,IDC_DSENABLE )) add_string (IDS_DS_SEARCH  ); else add_string (IDS_DS_BYPASS);
                  if ((indexed = IsDlgButtonChecked(hwnd,IDC_DSINDEX ))) add_string (IDS_DS_INDEX   );
                  if (           IsDlgButtonChecked(hwnd,IDC_DSNAMES  )) add_string (IDS_DS_NAME    );
                  if (           IsDlgButtonChecked(hwnd,IDC_DSONLY   )) add_string (IDS_DS_NAMEONLY);
                  highlight (HL(IsDlgButtonChecked(hwnd,IDC_DSENABLE)));      // Highlight selected items.
                  kbuffer[length++] = KANJI_SPACE;
                  add_string (buffer);
//
//  Check to see if file exits.  We will allow instalation of dictionaries
//  that don't exist, but we want to warn about it.
//
                  lstrcpy (text,buffer);
                  lstrcat (text,TEXT(".jdx"));
                  if (!FileExists(buffer) || (indexed && !FileExists(text))) {
                    if (IDNO == JMessageBox(hwnd,IDS_DS_DOESNOTEXIST,IDS_AREYOUSURE,MB_YESNO | MB_ICONWARNING,buffer)) return (true);
                  }
                  EndDialog (hwnd,true);
                }
                return (0);
//
//  Didn't want to do it after all.
//
           case IDCANCEL:
                EndDialog (hwnd,false);
                return (0);
         }
  }
  return (false);
}

//
//  Required virtual function to edit an entry.  This just invokes the 
//  edit dialog box.  All of the real work is there.
//
int EDIT_dictionaries::edit () {
//
//  This is a KLUDGE.  To implement the TOGGLE button, we set the 
//  toggle flag then invoke the editor command.  This will simply change
//  the state and return the value.  This will do all the real work 
//  necessary to do the actual work.
//
//  This routine had to be changed in version 1.34.  The new routine copies
//  the kbuffer to a temp location.  This allows the size of the SEARCH/BYPASS
//  strings to be different.  This was necessary for suport of multable 
//  languages in JWPce.
//
  if (toggle) {
    int   i,j;
    tchar *ptr;
    KANJI  ktemp[SIZE_BUFFER];
    for (i = 0; i < SIZE_BUFFER; i++) ktemp[i] = kbuffer[i];                // Copy kbuffer.
    for (i = 0; kbuffer[i] != '\t'; i++);                                   // Skip to end of name
    line_break = ++i;                                                       // Save line break position.
    ptr = get_string(IDS_DS_SEARCH);                                        // Get search string.
    for (j = 0; ptr[j] && (ptr[j] == kbuffer[i+j]); j++);                   // Determine if this is a SEARCHED or not.
    if (!ptr[j]) ptr = get_string(IDS_DS_BYPASS);                           // Was search so now bypassed
    highlight (HL(ptr[j]));                                                 // Set the highlighting.
    for (j = i; ktemp[j] != ' '; j++);                                      // Skip the SEARCH/BYPASS in the old string
    while (*ptr) kbuffer[i++] = *ptr++;                                     // Set new SEARCH/BYPASS
    for (j++; (kbuffer[i] = ktemp[j]); i++,j++);                            // Copy the test of the string.
    return (true);
  }
//
//  Normal edit operation.
//
  return (JDialogBox(IDD_DICTEDITDICTIONARIES,dialog,(DLGPROC) dialog_editdictionaries));
}

//
//  This routine gets the data from the from the list and converts it
//  into the format necessary for a dictionary file.  
//
//      RETURN -- Memory allocated copy of the buffer.  This is simply
//                the disk image of the string converted into a long string.
//
byte *EDIT_dictionaries::get_data () {
  int   i,index;
  byte *data,*ptr;

  if (!(data = (byte *) calloc(1,size()))) { OutOfMemory (dialog); return (data); }
  ptr    = data;
  index  = 0;
  while (index < count()) {                         // While there are items keep processing.
    get_buffer   (index);                           // Get buffer.
    parse_buffer ();                                // Decode buffer.
    *ptr++ = search  ? 'S' : ' ';                   // Put flags.
    *ptr++ = indexed ? 'I' : ' ';
    *ptr++ = names   ? 'N' : ' ';
    *ptr++ = only    ? 'O' : ' ';                   // Put path infomration.
    for (i = path; i < length; i++) *ptr++ = (byte) kbuffer[i];
    *ptr++ = '\t';
    for (i = 0; kbuffer[i] != '\t'; i++) {          // Put description.
      if (!(ISJIS(kbuffer[i]))) *ptr++ = (byte) kbuffer[i];
        else {
          *ptr++ = (kbuffer[i] >> 8) | 0x80;
          *ptr++ = (kbuffer[i] & 0x00ff) | 0x80;
        }
    }
    *ptr++ = '\n';                                  // End of line.
    index = next_item (index);                      // Advance to next element.
  }
  return (data);
}

//
//  This is a utilty routine used to parse the information in the 
//  kbuffer.  This is used to decode data during writing of the 
//  actual dictionary file, and to decode the contents of the line
//  during entry to the item editor.
//
//  The format of the string containned in the kbuffer is:
//
//      <description><TAB><flags><KANJI_SPACE><filename>
//
//  The flags field is anumber of flag entry separated by spaces.
//  The flags are text representations, and only the first letter 
//  is of significance:
//
//      SEARCH  -- Dicitonary is to be searched.
//      INDEXED -- Dictionary is indexed.
//      NAMES   -- Dictioanry contains names.
//      ONLY    -- Dictionary contains names only.
//
//      DISABLE -- This flag is only for the user's benifit.  This 
//                 is included when the SEARCH flag is not included,
//                 but is ignored by all routines.  Only the prssence
//                 or apssence of the SEARCH flag is significant.
//
void EDIT_dictionaries::parse_buffer () {
  int i;
  indexed = names = only = search = false;
  for (i = 0; kbuffer[i] != '\t'; i++);         // Find end of descripton.
  descript = i;
  i++;
  while (kbuffer[i] != KANJI_SPACE) {           // Process flags.
    switch (kbuffer[i]) {
      case 'S':
           search = true;
           break;
      case 'N':
           names = true;
           break;
      case 'O':
           only = true;
           break;
      case 'I':
           indexed = true;
           break;
      default:
           break;
    }
    while ((kbuffer[i] != ' ') && (kbuffer[i] != KANJI_SPACE)) i++;
    if (kbuffer[i] == ' ') i++;
  }
  path = i+1;                                   // What reamins is the file path.
  return;
}

//
//  This routine puts data into the edit-list.
//
//      data -- Pointer to the data to be put into the list.  In this 
//              case this is a pointer to a memory image of the 
//              dict.cfg file.
//      name -- Name of imported data file (not used).
//
void EDIT_dictionaries::put_data (byte *data,tchar *name) {
  TCHAR file[SIZE_BUFFER];
  int  i;
  while (*data) {
    search = indexed = names = only = false;
    if (*data++ == 'S') search  = true;                 // Get flags.
    if (*data++ == 'I') indexed = true;
    if (*data++ == 'N') names   = true;
    if (*data++ == 'O') only    = true;
    highlight (HL(search));                             // Highlight searched dictionaries.
    clear     ();
    for (i = 0; *data != '\t'; i++) file[i] = *data++;  // Get file 
    file[i] = 0;
    for (data++; *data != '\n'; data++) {               // Get description
      if (!(*data & 0x80)) put_char (*data);            //   and put into list.
        else {
          i = *data++;
          put_char (((i << 8) | *data) & 0x7f7f);
        }
    }
    data++;
    put_char ('\t');                                    // Put divider
    flush    (-1);                                      // Break line.
    if (search ) put_string (get_string(IDS_DS_SEARCH  )); else put_string (get_string(IDS_DS_BYPASS));
    if (indexed) put_string (get_string(IDS_DS_INDEX   ));
    if (names  ) put_string (get_string(IDS_DS_NAME    ));            // Put flags.
    if (only   ) put_string (get_string(IDS_DS_NAMEONLY));
    put_char   (KANJI_SPACE);                           // Put divider
    put_string (file);                                  // Put file name.
    flush      (-1);                                    // Done.
  }
  return;
}

//
//  This rotuine intializes the edit a dictionary dialog box from a 
//  file name. This will set the file name, potentially load the 
//  first line of the file to initialie the comment field, check for 
//  an index file and set the index file values.
//
//      hwnd   -- Dialog window handle.
//      name   -- Name of file to initlize to.
//
//      RETURN -- A non-zero value indicates an error.
//
int EDIT_dictionaries::setup_file (HWND hwnd,TCHAR *name) {
  int    i,j;
  HANDLE file;
  unsigned long done;
  char  *ptr,temp[SIZE_BUFFER];
  KANJI *kptr;
//
//  Open file and get the first line for the description.
//
  file = CreateFile (name,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null);
  if (file != INVALID_HANDLE_VALUE) {
    ReadFile(file,temp,SIZE_BUFFER,&done,NULL);
    CloseHandle (file);
    length = 0;
    for (i = 0  ; (i < SIZE_BUFFER) && (temp[i] != '/'); i++);
    for (j = i+1; (j < SIZE_BUFFER) && (temp[j] != '/'); j++) kbuffer[length++] = temp[j];
    if (j >= SIZE_BUFFER) {
      JMessageBox (hwnd,IDS_DS_ERRORNOTDICT,IDS_DS_ERROR,MB_OK | MB_ICONERROR,name);
      return (true);
    }
    ptr = temp+i+1;
    i = JE_GetText(hwnd,IDC_DSDESCRIPTION,&kptr);
    if (i && (IDYES == JMessageBox(hwnd,IDS_DS_ALREADYTEXT,IDS_DS_ALREADYTITLE,MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2))) i = 0;
    if (!i) SendDlgItemMessage (hwnd,IDC_DSDESCRIPTION,JE_SETTEXT,length,(LPARAM) kbuffer);
  }
//
//  Item is okay, so set file name.
//
  SetDlgItemText (hwnd,IDC_DSFILE,name);
//
//  Check for an index file.
//
  i = lstrlen(name);
  lstrcat (name,TEXT(".jdx"));
  CheckDlgButton (hwnd,IDC_DSINDEX,FileExists(name));
  name[i] = 0;
  return (false);
}

//```````````````````````````````````````````````````````````````````
//
//  class JWP_dict routines.
//
//
//  Dialog box handler for the dictionaries dialog box.
//
//      IDC_DCEDICT       Edict check box.
//      IDC_DCNAMDICT     Namdict check box.
//      IDC_DCUSERDICT    User dictionary check box.
//      IDC_DCTOGGLE      Toggle searched, non-searched.
//      IDC_DCQUIET       Quiet processing of NAMDICT errors.
//
int JWP_dict::dlg_dictionaries (HWND hwnd,int message,WPARAM wParam,LPARAM lParam) {
  switch (message) {
    case WM_INITDIALOG:
         CheckDlgButton (hwnd,IDC_DCEDICT   ,jwp_config.cfg.dict_edict  );
         CheckDlgButton (hwnd,IDC_DCNAMDICT ,jwp_config.cfg.dict_namdict);
         CheckDlgButton (hwnd,IDC_DCUSERDICT,jwp_config.cfg.dict_user   );
         CheckDlgButton (hwnd,IDC_DCQUIET   ,jwp_config.cfg.dict_quiet  );
         if (!(edit_dictionaries = new EDIT_dictionaries())) return (false);
         edit_dictionaries->toggle = false;
         edit_dictionaries->drop   = false;
         edit_dictionaries->init (hwnd,dictionaries,NULL);
         EnableWindow (GetDlgItem(hwnd,IDC_DCTOGGLE),edit_dictionaries->count());
         return (true);
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_DICTIONARIES);
         return  (true);
//
//  This is somewhat a KLUDGE here.  To get the dictonaries processed
//  correctly, we load each fiel name into the edit_dictionaires::drop
//  parameter.  This will cause the dialog to indialties as if that 
//  file were a dictionary file.  Depending on the settings of the 
//  DROP_AUTOADD flag, this can cause cause the dialog to shutdown 
//  without even opening, and return the processed values.  This lets 
//  use use all the tools in the edit dictionary dialog, without having
//  to duplicate them.  This all supports multable file drop.
//
#ifndef WINCE
    case WM_DROPFILES: {
           int  i;
           char buffer[SIZE_BUFFER];
           edit_dictionaries->drop = buffer;
           for (i = 0; DragQueryFile((HDROP) wParam,i,buffer,SIZE_BUFFER) > 0; i++) {
             edit_dictionaries->do_event (IDC_EDITLISTADD);
           }
           edit_dictionaries->drop = NULL;
           DragFinish ((HDROP) wParam);
         }
         return (0);
#endif WINCE
    case WM_COMMAND:
         switch (LOWORD(wParam)) {              
           case IDC_EDITLIST:                   // These events belong to the edit-
           case IDC_EDITLISTADD:                //   list class.
           case IDC_EDITLISTEDIT:
           case IDC_EDITLISTUP:
           case IDC_EDITLISTDOWN:
           case IDC_EDITLISTDELETE:
           case IDC_EDITLISTIMPORT:
           case IDC_EDITLISTINSERT:
                edit_dictionaries->do_event (wParam);
                EnableWindow (GetDlgItem(hwnd,IDC_DCTOGGLE),edit_dictionaries->count());
                return (0);
//
//  Toggle the selected dictionary.  This is a KLUDGE, we use the edit
//  function capability to do this.
//
           case IDC_DCTOGGLE: 
                edit_dictionaries->toggle = true;
                edit_dictionaries->do_event (IDC_EDITLISTEDIT);
                edit_dictionaries->toggle = false;
                return (0);
//
//  User wants to keep the list so output to a buffer.
//
           case IDOK:                                   // User wants to keep changes.
                HANDLE file;
                unsigned long done;
                jwp_config.cfg.dict_edict   = IsDlgButtonChecked (hwnd,IDC_DCEDICT   );
                jwp_config.cfg.dict_namdict = IsDlgButtonChecked (hwnd,IDC_DCNAMDICT );
                jwp_config.cfg.dict_user    = IsDlgButtonChecked (hwnd,IDC_DCUSERDICT);
                jwp_config.cfg.dict_quiet   = IsDlgButtonChecked (hwnd,IDC_DCQUIET   );
                if (dictionaries) free (dictionaries);          // Remove old conversions.
                dictionaries = edit_dictionaries->get_data ();  // Make current conversions active oones.
                if (INVALID_HANDLE_VALUE == (file = jwp_config.open(NAME_DICTIONARIES,OPEN_NEW,true))) {
                  QUIET_ERROR {
                    edit_dictionaries->error (IDS_DS_ERROROPEN,jwp_config.name());
                    return (0);                    
                  }
                }                                   // Write conversions to disk.
                WriteFile(file,dictionaries,strlen((char *) dictionaries),&done,NULL);
                CloseHandle (file);
           case IDCANCEL:               // **** FALL THORUGH ****
                delete edit_dictionaries;
                EndDialog (hwnd,false);
                return (0);
         }
         break;
  }
  return (false);
}

//
//  Edit dictionaries edit box.
//
//-------------------------------------------------------------------

//-------------------------------------------------------------------
//
//  begin class JWP_dict
//
//  This class implements the dictionary search.
//

JWP_dict jwp_dict;          // Class instance.

//
//  ID's for the maind dictionary dialog
//
//      IDC_DDSTRING   Search string (edit-control)
//      IDC_DDRESULT   Results (list-box)
//      IDC_DDBEGIN    Match only at beiginning (check-box).
//      IDC_DDEND      Match only at end (check-box)
//      IDC_DDNONAME   No names (check-box)
//      IDC_DDADVANCED Activates advanced searching (check-box)
//      IDC_DDSEARCH   Do search (button)
//      IDC_DDINSERT   Insert into file (button)
//      IDC_DDUSER     Edit user dictionary.
//      IDC_DDOPTIONS  Options (button)
//      IDC_DDDONE     Done (button)
//      IDC_DDSTATUS   Status (text-message).
//

//
//  This routine is called when an entry in the dictionary has been 
//  found.  This rotuine will check the entry against the filters, 
//  and if the entry passess all filters, the entry will be put into
//  the list.
//
//      ptr    -- Pointer to the actual entry location in the dictionary
//                buffer.
//      length -- Length of user's search key.
//
#define NUMBER_DICTNAME ((int) (sizeof(names)/sizeof(char [6])))

#define EUC_CAMA    (0x2221 | 0x8080)       // EUC mach characters.  Note the switched byte order
#define EUC_SLASH   (0x3f21 | 0x8080)       //   to reflect the order of bytes in the file.

#define IS_BEGIN(c)     (((c) == '[') || ((c) == ' ') || ((c) == '/') || ISCRLF(c))     // Valid being of entry conditions
#define IS_END_N(c)     (((c) == ']') || ((c) == ' ') || ((c) == '/'))                  // Normal end of entry conditions
#define EUC_MATCH(p,x)  (((p)[0] | (((int) (p)[1])<<8)) == (x))                         // Odd methode necessary for MIPS processor/compiler error
#define IS_END(p)       (IS_END_N(*(p)) || (classical_part && (EUC_MATCH(p,EUC_CAMA) || EUC_MATCH(p,EUC_SLASH))))   // Test for valid end character

void JWP_dict::check_entry (byte *ptr,int length) {
  int   i;
  byte *p,*p2,*p3;      // Scratch pointers.
//
//  Check entry for valid begin/end requirements.
//
  if ((!classical_part && dict_keys[DICTKEY_BEGIN].reject && !IS_BEGIN(ptr[-1])) || (dict_keys[DICTKEY_END].reject && !IS_END(ptr+length))) { 
    rejected++;                                 // Check for match at beginning and ending of the search.
    message (NULL); 
    return;
  }
//
//  At this point, we have a an accepted entry, that matches the user's
//  beginning/ending of the line conditions.  
//
//  Now we begin to format the line, and have to check for excluded 
//  entry times.
//
  for ( ; !ISCRLF(*ptr); ptr--);                // Backup in the buffer to the beginning of the line.
  ptr++;
//
//  This next nasty little section of code handles removing entries, 
//  based on the entry type.  This is complicated because each entry 
//  can have multable definitions.  If the user has selected No Personal
//  Names, then we need to remove an entry that is a personal name only
//  in it's entirety.  If, however, the entry is both a place name and
//  a personal name, we need to simply remove the personal name type
//  id from the enrtry.
//
//  Just for review, a dictionary line looks like:
//
//      <kanji> [<kana>] /definition/definition/    ...
//                  or
//      <kana> /definition/definition/  ...
//
//  Within each definition there can be several type identifiers, 
//  placed in paraentheses.  For exapmle: (pl,pn,giv,fem), indicates a 
//  place name, and a female given name.
//
#if 0   //****  OBSOLETE  *****  OBSOLETE  *****  OBSOLETE  *****  OBSOLETE  *****  OBSOLETE  *****  OBSOLETE  *****
        //
        //  This is the old filter used with the version of EDICT that
        //  was destributed with JWP 1.31.  I have kept this arround for 
        //  centamental resons.  It was difficult to prefect so I am keeping
        //  it.  Who knows, maybe I will need it some time
  if (filter) {
    static char names[][6] = { "sur","giv","male","fem" }   // These are extensions added to the pn (personal name) type
                                                            //   generally they follow the pn field.  JWPce does not  
                                                            //   do filtering based on the individual type of name, but
                                                            //   rather on the entire class of names, thus these will be
                                                            //   rejected if pn is rejected.
    int j;                  // Used to scan the extended keys.
    int len;                // Length of key we are working with.
    int len2;               // Length of extra key.  These are attached to the 'pn' key.
    int check = false;      // Set to non-zero when the entry is modified, 
                            //   will force an evaluation of the entry later.
    p = ptr;                                                // pointer p will advance through the file.
    while (!ISCRLF(*p)) {                                        
      if (*p != '(') { p++; continue; }                     // We have manybe found a type key, if not off to next charcter.
      while (*p != ')') {                                   // Until we reach the end of the type key we need to keep examining each key.
        for (i=DICTKEY_START; i < NUMBER_DICTKEYS; i++) {   // See if we know what the key is.
          if ((len = test_key(p,dict_keys[i].key))) break;
        }
        if (!len) { p++; break; }                           // This is a cheat.  We dont' reconize the key so we will 
                                                            //   assume that this is just a note, and by exiting the 
                                                            //   inside key loop and advancing the pointer, we will 
                                                            //   actually be in the state of the looking for '(' again.
        if (i == DICTKEY_NAMES) {                           // If the key we found was 'pn', we need to find any
          do {                                              //   additional specifiers associated with the 'pn', i.e. 
            for (j = 0; j < NUMBER_DICTNAME; j++) {         //   fem, giv, male, sur, etc.
              if ((len2 = test_key(p+len,names[j]))) break;
            }
            len += len2;                                    // We skip the addons to 'pn' by simply increasing the length 
          } while (len2);                                   //   of the 'pn' key.
        }
        if (!dict_keys[i].reject) { p += len; continue; }   // If this is not a rejected key, simply advance to the end of
                                                            //   the key, and continue.  We are still in a key (<key>,<key>).
        if ((*p == '(') && (p[len] == ')')) {               // We have a key we want to reject, but the key started with '(',
          for (p2 = p+len; *p2 != '/'; p2++);               //   and ended with ')'.  This means that we really want to 
          while (*p != '/') p--;                            //   reject this entrie definition.  (Note, other keys may 
          p3 = p;                                           //   have been removed to get us here.)  To remove an entire 
          while (!ISCRLF(*p2)) *p3++ = *p2++;               //   definition, we find the beginning '/' and the ending '/', 
          *p3 = '\n';                                       //   then copy all characters from end to begining.  After this
          check = true;                                     //   we leave the character pointer at where the beginning of this
          break;                                            //   definition was, and exit the in-definiition loop.
        }                                                 
        p2 = p;                                             // Remember we were rejecting this key.  In this case, there are
        if (*p2 == '(') p2++;                               //   either previous or following keys to be removed, thus we want 
        while (!ISCRLF(p2[len])) { p2[0] = p2[len]; p2++; } //   to simply remove the key.  We do this by copying from end of key
        p2[0] = '\n';                                       //   to beginning of key.  (Note, we preserve '(' if it was the 
      }                                                     //   beginning of the sequence.)
    }                                                     
    if (check) {                                            // Modifiec entry, so do we still want it.  We count the '/'s.  If 
      for (i = 0, p = ptr; !ISCRLF(*p); p++) if (*p == '/') i++;  //   there are two or more we keep the enry.  One or less we have
      if (i <= 1) { rejected++; message (NULL); return; }   //   removed all definitions, so we reject it.
// #*#*# Could add a filetered counter here.
    }
  }
#endif  //****  OBSOLETE  *****  OBSOLETE  *****  OBSOLETE  *****  OBSOLETE  *****  OBSOLETE  *****  OBSOLETE  *****
  if (filter) {
    static char names[][2] = { "u","g","f","m" };           // These are extensions added to the (personal name) type
                                                            //   generally they follow the pn field.  JWPce does not  
                                                            //   do filtering based on the individual type of name, but
                                                            //   rather on the entire class of names, thus these will be
                                                            //   rejected if pn is rejected.
    int j;                  // Used to scan the extended keys.
    int len;                // Length of key we are working with.
    int check = false;      // Set to non-zero when the entry is modified, 
                            //   will force an evaluation of the entry later.
    p = ptr;                                                // pointer p will advance through the file.
    while (!ISCRLF(*p)) {                                        
      if (*p != '(') { p++; continue; }                     // We have manybe found a type key, if not off to next charcter.
      while (*p != ')') {                                   // Until we reach the end of the type key we need to keep examining each key.
        for (i=DICTKEY_START; i < NUMBER_DICTKEYS; i++) {   // See if we know what the key is.
          if (len = test_key(p,dict_keys[i].key)) break;
        }
        if (!len) {                                         // Special case, because we do not destinguish by name types
          i = DICTKEY_NAMES;                                //   we have to reject all name types when the user chooses
          for (j = 0; j < NUMBER_DICTNAME; j++) {           //   to reject names.  Thus we let the standard array take 
            if (len = test_key(p,names[j])) break;          //   care of the first term (s=surname), and then remove all 
          }                                                 //   of the other name terms here.
        }
        if (!len) { p++; break; }                           // This is a cheat.  We dont' reconize the key so we will 
                                                            //   assume that this is just a note, and by exiting the 
                                                            //   inside key loop and advancing the pointer, we will 
                                                            //   actually be in the state of the looking for '(' again.
        if (!dict_keys[i].reject) { p += len; continue; }   // If this is not a rejected key, simply advance to the end of
                                                            //   the key, and continue.  We are still in a key (<key>,<key>).
        if ((*p == '(') && (p[len] == ')')) {               // We have a key we want to reject, but the key started with '(',
          for (p2 = p+len; *p2 != '/'; p2++);               //   and ended with ')'.  This means that we really want to 
          while (*p != '/') p--;                            //   reject this entrie definition.  (Note, other keys may 
          p3 = p;                                           //   have been removed to get us here.)  To remove an entire 
          while (!ISCRLF(*p2)) *p3++ = *p2++;               //   definition, we find the beginning '/' and the ending '/', 
          *p3 = '\n';                                       //   then copy all characters from end to begining.  After this
          check = true;                                     //   we leave the character pointer at where the beginning of this
          break;                                            //   definition was, and exit the in-definiition loop.
        }                                                 
        p2 = p;                                             // Remember we were rejecting this key.  In this case, there are
        if (*p2 == '(') p2++;                               //   either previous or following keys to be removed, thus we want 
        while (!ISCRLF(p2[len])) { p2[0] = p2[len]; p2++; } //   to simply remove the key.  We do this by copying from end of key
        p2[0] = '\n';                                       //   to beginning of key.  (Note, we preserve '(' if it was the 
      }                                                     //   beginning of the sequence.)
    }                                                     
    if (check) {                                            // Modifiec entry, so do we still want it.  We count the '/'s.  If 
      for (i = 0, p = ptr; !ISCRLF(*p); p++) if (*p == '/') i++;  //   there are two or more we keep the enry.  One or less we have
      if (i <= 1) { rejected++; message (NULL); return; }   //   removed all definitions, so we reject it.
// ### Could add a filetered counter here.
    }
  }

//
//  Hooray! We have an entry that we actually want to keep.  This means 
//  we have to format the line and generate the output.  Most of the 
//  work in generating the output string is done by the class EUC_buffer.
//  (see top of file for class information).
//                                  
  matches++;                                // Change count and display
  message     (NULL);                       // Change count
  format_line (this,ptr,false,classical);   // Output line.
  return;
}

//
//  This is the dialog box procedure for the main dictionary dialog.
//  This is where all commands are interpreted.
//
int JWP_dict::dlg_dictionary (HWND hwnd,int msg,WPARAM wParam,LPARAM lParam) {
  int i;
  static byte enable_clip;      // This is used to prevent errors when using the clipboard
                                //   tracking.  This suppresses clipboard searching until
                                //   after the first search command is received.
  static byte auto_search;      // This is set during cureation of the dialog box, and 
                                //   then checked during the first WM_PAINT message to 
                                //   see if the user has met the conditions for an auto
                                //   seach.  Using this methode, we are able to process
                                //   an auto-seach after the dialog box has become visible.
                                //   Especially if the user selecteds a long search, this 
                                //   is much nicer, because they will be able to abort the 
                                //   search, and/or see what is going on.  As oposed to 
                                //   simply waiting a long time for the search to occure.
#ifndef WINCE
  static HWND clipview;         // Previous clipboard viewer.
#endif WINCE

  switch (msg) {
//
//  Intialize the dialog.  Intialize some varialbes, and setup the 
//  controls.
//
    case WM_INITDIALOG: 
         active = true;
         add_dialog (hwnd,true);
         for (i = 0; i < NUMBER_DICTKEYS; i++) dict_keys[i].reject = ((jwp_config.cfg.dict_bits & (0x1L << i)) != 0);
         dialog  = hwnd;
         initialize (GetDlgItem(hwnd,IDC_DDRESULT));    // Intialize the base class EUC_buffer.
         set_checkboxes ();
         wParam = SendDlgItemMessage (hwnd,IDC_DDSTRING,JE_LOAD,0,(LPARAM) jwp_file);
         SendDlgItemMessage (hwnd,IDC_DDRESULT,JL_SETEXCLUDE,0,SendDlgItemMessage(hwnd,IDC_DDSTRING,JE_GETJWPFILE,0,0));
         if (wParam && jwp_config.cfg.dict_auto) auto_search = true;
//
//  Setup the clipboard tracking
//
#ifndef WINCE
         enable_clip = false;
         clipview    = SetClipboardViewer (hwnd);       // Setup clipboard tracking
#endif WINCE
//
//  Read user dictionary, if it hasent been loaded.
//  Read dictionaries file if it hasen't been loaded.
//  
         if (!user_dict) user_dict = load_dict(jwp_config.name(NAME_USERDICT,OPEN_READ,true));
         if (!dictionaries) dictionaries = load_image(jwp_config.name(NAME_DICTIONARIES,OPEN_READ,true));
         TRACKING_INIT();                               // Initialize dicitonary tracking.
         TRACKING_SET (file);
         return   (true);
//
//  Save the state of the dictonary keys.
//
    case WM_DESTROY:
         active = false;
         get_checkboxes ();
         jwp_config.cfg.dict_bits = 0;
         for (i = 0; i < NUMBER_DICTKEYS; i++) {
           if (dict_keys[i].reject) jwp_config.cfg.dict_bits |= (0x1L << i);
         }
#ifndef WINCE
         ChangeClipboardChain (hwnd,clipview);          // Terminate clipboard tracking
#endif WINCE
         remove_dialog (hwnd);
         return (0);
//
//  Clipboard tracking rotuines
//
#ifndef WINCE
    case WM_DRAWCLIPBOARD:
         if (clipview) SendMessage (clipview,msg,wParam,lParam);
         if (jwp_config.cfg.dict_watchclip && enable_clip && !jwp_clipboard) {
           JWP_file *string;
           string = (JWP_file *) SendDlgItemMessage(hwnd,IDC_DDSTRING,JE_GETJWPFILE,0,0);
           if (string->edit_clip()) SendMessage (hwnd,WM_COMMAND,IDOK,0);
         }
         return (0);
    case WM_CHANGECBCHAIN:
         if (clipview == (HWND) wParam) clipview = (HWND) lParam; else SendMessage (clipview,msg,wParam,lParam);
         return (0);
#endif WINCE
//
//  Process help messages
//
    case WM_HELP:
         do_help (hwnd,IDH_DICT_GENERAL);
         return  (true);
//
//  We don't really process the WM_PAINT message, as a mater of fact, we 
//  return 0 to idnicate that we did not process them.  We do, however, 
//  watch for the first one to see if the user is doing an auto_search.
//
//  ### There should be a better way to do this but i have not found one yet.
//
    case WM_PAINT:
         enable_clip = true;
         if (auto_search) {
           auto_search = false;
           SendMessage (hwnd,WM_COMMAND,IDOK,0);
         }
         return (false);
    case WM_COMMAND:        
         switch (LOWORD(wParam)) { 
//
//  User has intializted a search.
//
           case IDOK:           // Search
                search_dict  ();
                TRACKING_LOG (matches);
                return (0);
//
//  The user has clicked on the Advanced checkbox.
//
           case IDC_DDNONAME:
                dict_keys[DICTKEY_PLACES].reject = dict_keys[DICTKEY_NAMES ].reject = !IsDlgButtonChecked(hwnd,IDC_DDNONAME);
                CheckDlgButton (hwnd,IDC_DDNONAME,dict_keys[DICTKEY_PLACES].reject);
                return (0);
//
//  If user double clicks in the result window, we want to paste the 
//  results back into whoever called us.
//
//  This is the main paste back.
//
           case IDC_DDRESULT:
           case IDC_DDINSERT:
                SendDlgItemMessage (hwnd,IDC_DDRESULT,JL_INSERTTOFILE,0,0);
                return (0);
//
//  User wants to edit the user dictionary.
//
           case IDC_DDUSER:
                user_dictionary ();
                return (0);
//
//  User wants to edit the options.
//
           case IDC_DDOPTIONS:
                if (is_searching()) return (0);
                JDialogBox (IDD_DICTOPTIONS,hwnd,(DLGPROC) dialog_dictoptions);
                return (0);
//
//  We may be done.  First check to see if we are searching.  If so,
//  abort the sarch, but do not exit.
//                
           case IDCANCEL:
                if (state) {                            // Check to see if we are search, if so
                  state = DICTSTATE_ABORT;              //   abort search but don't exit.
                  return (1);
                }
                if (user_dialog) {                      // Check to see if the user dictionary is
                  SendMessage(user_dialog,WMU_OKTODESTROY,0,(long) &i); // open.  If so see what to
                  if (!i) return (0);                                   // do about closing/saving/etc.
                }
                DestroyWindow (hwnd);
                return    (0);
         }
  }
  return (false);
}

//
//  This is the dialog box handler for the Dictionary Options dialog box.
//
//      hwnd    -- Pointer to our dialog box
//      msg     -- What windows wants.
//      command -- This is the command part of WM_COMMAND messages.  This
//                 has been extracted in the stub routine.
//
//      IDC_DOEXCLUDE          The main exclusion list.
//      IDC_DOCOMPRESS         The compress check-box.
//      IDC_DOAUTO             The auto-search check-box
//      IDC_DODICTIONARIES     The dictionaries button.
//
int JWP_dict::dlg_dictoptions (HWND hwnd,int msg,int command) {
  int i,j;
  switch (msg) {
    case WM_INITDIALOG:
         get_checkboxes ();
         CheckDlgButton (hwnd,IDC_DOCOMPRESS ,jwp_config.cfg.dict_compress );
         CheckDlgButton (hwnd,IDC_DOAUTO     ,jwp_config.cfg.dict_auto     );
         CheckDlgButton (hwnd,IDC_DOADVSEARCH,jwp_config.cfg.dict_advanced );
         CheckDlgButton (hwnd,IDC_DOADVALWAYS,jwp_config.cfg.dict_always   );
         CheckDlgButton (hwnd,IDC_DOADVALL   ,jwp_config.cfg.dict_showall  );
         CheckDlgButton (hwnd,IDC_DOADVI     ,jwp_config.cfg.dict_iadj     );
         CheckDlgButton (hwnd,IDC_DOCLIPBOARD,jwp_config.cfg.dict_watchclip);
         CheckDlgButton (hwnd,IDC_DOCLASSICAL,jwp_config.cfg.dict_classical);
         for (i = 0; i < NUMBER_DICTKEYS; i++) {
           SendDlgItemMessage (hwnd,IDC_DOEXCLUDE,LB_ADDSTRING,0,(long) get_string(dict_keys[i].text));           
           SendDlgItemMessage (hwnd,IDC_DOEXCLUDE,LB_SETSEL,dict_keys[i].reject,i);
         }
         SendDlgItemMessage (hwnd,IDC_DOEXCLUDE,LB_SETTOPINDEX,0,0);   // Need to set this or windows scrolls the list-box
         for (i = IDC_DOADVI; i <= IDC_DOADVALL; i++) EnableWindow(GetDlgItem(hwnd,i),jwp_config.cfg.dict_advanced);
         return (true);
    case WM_HELP:
         do_help (hwnd,IDH_DICT_OPTIONS);
         return  (true);
    case WM_COMMAND:
         switch (command) {
           case IDC_DOADVSEARCH:
                j = IsDlgButtonChecked(hwnd,IDC_DOADVSEARCH);
                for (i = IDC_DOADVI; i <= IDC_DOADVALL; i++) EnableWindow(GetDlgItem(hwnd,i),j);
                return (0);
           case IDC_DODICTIONARIES:
                JDialogBox (IDD_DICTIONARIES,hwnd,(DLGPROC) dialog_dictionaries); 
                return (0);
           case IDC_DOUSERDICT:
                user_dictionary ();
                return (0);
           case IDOK:
                jwp_config.cfg.dict_compress  = IsDlgButtonChecked(hwnd,IDC_DOCOMPRESS );
                jwp_config.cfg.dict_auto      = IsDlgButtonChecked(hwnd,IDC_DOAUTO     );
                jwp_config.cfg.dict_advanced  = IsDlgButtonChecked(hwnd,IDC_DOADVSEARCH);
                jwp_config.cfg.dict_always    = IsDlgButtonChecked(hwnd,IDC_DOADVALWAYS);
                jwp_config.cfg.dict_showall   = IsDlgButtonChecked(hwnd,IDC_DOADVALL   );
                jwp_config.cfg.dict_iadj      = IsDlgButtonChecked(hwnd,IDC_DOADVI     );
                jwp_config.cfg.dict_watchclip = IsDlgButtonChecked(hwnd,IDC_DOCLIPBOARD);
                jwp_config.cfg.dict_classical = IsDlgButtonChecked(hwnd,IDC_DOCLASSICAL);
                for (i = 0; i < NUMBER_DICTKEYS; i++) {
                  dict_keys[i].reject = (byte) SendDlgItemMessage(hwnd,IDC_DOEXCLUDE,LB_GETSEL,i,0);
                }
                set_checkboxes ();
           case IDCANCEL:   // *** FALL THROUGH ***
                EndDialog (hwnd,true);                  // Return value does not matter, because we don't use it.
                return (0);
         }
  }
  return (false);
}

//
//  This routine actually does a search.  The argumetns inciate what 
//  to be searched for.  In a simple search, this will be called once,
//  For an advanced search, this can be called many different times as
//  the search arguments is modified and many different endings are 
//  added.
//
//      search -- The string to be searched for.
//      length -- Length of the search string.
//
//      RETURN -- Indicates the search status:
//
//          DICTSEARCH_MIXED -- Search contains ascii and kana.
//          DICTSEARCH_SHORT -- Too few characters to search for.
//          DICTSEARCH_OKAY  -- Search is OK (this does not mean any
//                              matches were found).
//
int JWP_dict::do_search (KANJI *search,int length) {
  int ascii,count,kanji,kana,i;
  byte key[2*MAX_KEY_LENGTH+4];
//
//  Build the key
//
  ascii = count = kanji = kana = 0;
  for (i = 0; i < length; i++) {                // Build a key.
    if (ISASCII(search[i])) {
      ascii++;
      key[count++] = tolower(search[i]);
    }
    else {
      if (ISKANA(search[i])) kana++; else kanji++;
      key[count++] = HIBYTE(search[i]) | 0x80;
      key[count++] = LOBYTE(search[i]) | 0x80;
    }
  }
  key[count] = 0;
//
//  Check key for validity.
//
  if (ascii && (kana || kanji)) return (DICTSEARCH_MIXED);
  if ((ascii && (ascii < 3)) || (!ascii && !kanji && (kana < 2))) return (DICTSEARCH_SHORT);
//
//  Check for classical particles and jodoushi.
//
  classical_part = jwp_config.cfg.dict_classical && (search[0] == KANJI_LONGVOWEL);
//
//  This is the actuall search block.  This will search all dicitonaries
//  that are appropriate.
//
  int    indexed;
  byte  *ptr,*dict;
  TCHAR  buffer[SIZE_BUFFER];
//
//  If this is a classical dictionary search we want to search the 
//  classical dictionary first.
//
  if (jwp_config.cfg.dict_classical) {
    classical = true;                                   // This is a bit of a KLUDGE, this lets the formatter 
    highlight (true);                                   //   know this is this is a classical dictionary.
    if (!(dict = load_dict(jwp_config.name(NAME_CLASSICAL,OPEN_READ,false)))) error (IDS_DD_CLASSICAL,jwp_config.name());
      else {
        search_memory (key,count,(byte *) dict);        // Non-indexed dictioanry
        free (dict);                                    // Cleanup
      }
    classical = false;
  }
//
//  Next we will setup for the the search loop.  This loop gets each
//  dictionayr entry, checks to see if it still matches the key, then
//  formats the entry for display.
//
//  Search EDICT and ENAMDICT
//
  highlight (false);
  if (jwp_config.cfg.dict_edict) search_index (key,count,jwp_config.name(NAME_MAINDICT,OPEN_READ,false));
  if (state == DICTSTATE_ABORT) return (DICTSEARCH_ABORT);
  if (!jwp_config.cfg.dict_quiet || FileExists(jwp_config.name(NAME_NAMEDICT,OPEN_READ,false))) {
    if (jwp_config.cfg.dict_namdict && !nonames) search_index (key,count,jwp_config.name(NAME_NAMEDICT,OPEN_READ,false));
    if (state == DICTSTATE_ABORT) return (DICTSEARCH_ABORT);
  }
//
//  Do the suplimental dictionaries.
//
  if ((ptr = dictionaries)) {
    while (*ptr) {
      while (*ptr++ == 'S') {                   // Use a while so we can break out.
        if (*ptr++ == 'I') indexed = true; else indexed = false;
#if 1
        ptr++;                                  // Name field is currently unused (time for filtering seems small)
#else
//      if (*ptr++ == 'N') names   = true;      // ### unused at the moment.
#endif
        if ((*ptr++ == 'O') && nonames) break;  // Skip name only dictionaries if names are blocked.
        for (i = 0; ptr[i] != '\t'; i++) buffer[i] = ptr[i];
        buffer[i] = 0;                          // Got dictionary name.
        if (indexed) search_index (key,count,buffer);           // Indexed dictionary search.
          else {                                                // Load dictionary
            if (!(dict = load_dict(buffer))) error (IDS_DD_CANNOTOPEN,buffer);
              else {
                search_memory (key,count,(byte *) dict);        // Non-indexed dictioanry
                free (dict);                                    // Cleanup
              }
          }

      }
      while (*ptr != '\n') ptr++;                   // Skip to next dictionary.
      ptr++;
      if (state == DICTSTATE_ABORT) return (DICTSEARCH_ABORT);  // Did user abort.
    }
  }
//
//  Search user dictionary, and abort point.
//
  highlight (true);
  if (user_dict && jwp_config.cfg.dict_user) search_memory (key,count,user_dict);
//
//  Done!
//
  return (DICTSEARCH_OKAY);
}

//
//  Generate an error message.
//
//      error  -- Set non-zero for an error and zero for a warning.
//      format -- ID for a printf style format string.
//      ...    -- Arguments for printf string.
//
void JWP_dict::error (int format,...) {
  TCHAR buffer[SIZE_WORKING],string[SIZE_WORKING];
  va_list argptr;
  va_start   (argptr,format);
  GET_STRING (string,format);
  wvsprintf  (buffer,string,argptr);
  MessageBox (dialog,buffer,get_string(IDS_DS_ERROR),MB_OK | MB_ICONWARNING);
  return;
}

//
//  Reads the state of the dictionary dialog box's four check-boxes.
//
void JWP_dict::get_checkboxes () {
  dict_keys[DICTKEY_END   ].reject = IsDlgButtonChecked(dialog,IDC_DDEND     );
  dict_keys[DICTKEY_BEGIN ].reject = IsDlgButtonChecked(dialog,IDC_DDBEGIN   );
  jwp_config.cfg.dict_advanced     = IsDlgButtonChecked(dialog,IDC_DDADVANCED);
  return;
}

//
//  This routine reads a line from the diction based on an index into 
//  the index file.
//
//      index  -- File handle for index file (should already be open).
//      dict   -- File handle for dictionary file (should already be open).
//      loc    -- Index into dictionary file to get data for.
//      buffer -- Buffer to read line into.  Note that this routine 
//                actually reads data into the middle of the buffer.
//                This is so you can back up in the buffer.  For 
//
//  From search_dict() routine:
//
//  This routine uses buffer and buf, to allow us to backward scan in 
//  the file.  This works as follows:
//
//  When a read from the dictionary takes place, a block of size SIZE_DICTBUFFER
//  bytes is read.  The cener of this block is the locationactually 
//  requested.  This gives us the capability to backup to the beginning 
//  of the dictonary entry.  
//
//  The pointer buf, points into the buffer and points the actual point
//  in the line that was being requested.
//
void JWP_dict::get_line (HANDLE index,HANDLE dict,int loc,byte *buffer) {
  long offset;
  unsigned long done;
  SetFilePointer (index,(loc+1)*sizeof(long),NULL,FILE_BEGIN);
  ReadFile (index,&offset,sizeof(long),&done,NULL);
  if (offset-SIZE_LINE < 0) {                       // Special case for near the beginning of the file.
    memset         (buffer,'\n',SIZE_LINE);         // This is required to prevent search errors.
    SetFilePointer (dict,0,NULL,FILE_BEGIN);
    ReadFile       (dict,buffer+SIZE_LINE-offset,SIZE_DICTBUFFER-SIZE_LINE+offset,&done,NULL);
  }
  else {                                            // General case.
    SetFilePointer (dict,offset-SIZE_LINE,NULL,FILE_BEGIN);
    ReadFile       (dict,buffer,SIZE_DICTBUFFER,&done,NULL);
  }
  return; 
}

//
//  Simple routine to tell if we are in a search, and make a beep if so.
//
int JWP_dict::is_searching () {
  if (!state) return (false);
  MessageBeep (MB_ICONASTERISK); 
  return (true);  
}

//
//  Displays a message in the upper right corner of the dictionary 
//  dialog box.  This message generally intdicates the state of the 
//  current search.
//
//      message -- Pointer to a printf type format string for the 
//                 message.  Passing the value of NULL will cause the 
//                 standard matches/rejected message to be displayed.
//
void JWP_dict::message (tchar *format,...) {
  TCHAR buffer[256];
  va_list argptr;
  va_start (argptr,format);
  if (format) wvsprintf (buffer,format,argptr);
    else format_string(buffer,rejected ? IDS_DD_MATCHREJECT : IDS_DD_MATCH,matches,rejected);
  SetDlgItemText (dialog,IDC_DDSTATUS,buffer);
  return;
}

//
//  This is the client entry point into the dictionary system.  This is
//  called from a client, and sets some values, then simply enters the
//  dictionary search dialog box.  The major functions are all handled 
//  through the dialog box procedure.
//
//      file -- Context we are comming from.
//
void JWP_dict::search (JWP_file *file) {
//
//  If the dictionary is active then we want to transfer control to the current
//  dictionary dialog.  If the user is comming form any place other than the 
//  edit box in the dictionary then we want to reload the ditionary and maybe
//  do an automatic search.
//
  if (active) { 
    if (file->window != GetDlgItem(dialog,IDC_DDSTRING)) {
      if (jwp_config.cfg.dict_auto && SendDlgItemMessage(dialog,IDC_DDSTRING,JE_LOAD,0,(LPARAM) file)) {
        SendMessage (dialog,WM_COMMAND,IDOK,0);
      }
    }
    SetForegroundWindow (dialog); 
    return; 
  }
//
//  No dictionary, so generate the dialog box.
//
  jwp_conv.clear (); 
  JCreateDialog (IDD_DICTIONARY,file->window,(DLGPROC) dialog_dictionary);
  return;
}

//
//  This is the search routine for indexed dictionaries.
//
//      key    -- Key to search for.
//      length -- Length of the key
//      name   -- Name of the dictionary (".jdx" is added to get the name of the index);
//
void JWP_dict::search_index (byte *key,int length,tchar *name) {
  MSG        msg;                       // Message structure used to keep dialog box active.
  HANDLE     dict,index;                // Handles for main dictionary file and for index file.
  long       top,bottom,middle,cut;     // Parameter for binary search.
  int        diff;                      // Difference in key comparisons.
  byte       buffer[SIZE_DICTBUFFER];   // Main buffer for reading in data from the dictionary.
  byte      *buf;                       // Pointer to the location in buffer when the user requested data is.
  TCHAR     *index_name;
//
//  This routine uses buffer and buf, to allow us to backward scan in 
//  the file.  This works as follows:
//
//  When a read from the dictionary takes place, a block of size SIZE_DICTBUFFER
//  bytes is read.  The cener of this block is the locationactually 
//  requested.  This gives us the capability to backup to the beginning 
//  of the dictonary entry.  
//
//  The pointer buf, points into the buffer and points the actual point
//  in the line that was being requested.
//
  buf = buffer+SIZE_LINE-1;
//
//  Open dictionary and index.
//
  index_name = (TCHAR *) buffer;
  lstrcpy (index_name,name);
  lstrcat (index_name,TEXT(".jdx"));
  dict  = CreateFile (      name,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null);
  index = CreateFile (index_name,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,null);
  if ((dict == INVALID_HANDLE_VALUE) || (index == INVALID_HANDLE_VALUE)) {
    CloseHandle (dict);
    CloseHandle (index);
    error (IDS_DD_DICTINDEX,name);
    return;
  }
//
//  This section performs a binary search, looking for the beginning
//  of the index region dealing with the string that the user has entered.
//
  top    = 0;
  bottom = (GetFileSize(index,NULL)/sizeof(long))-1;
  while (true) {
    middle=(top+bottom)/2;
    get_line (index,dict,middle,buffer);
    diff = dict_comp (key,buf,0);
    if (!diff) { cut = middle; break; }
    if (top >= bottom-1) { cut = bottom; break; }
    if (diff > 0) top = middle; else bottom = middle;
  }
//
//  The actual loop.
//
  for (;; cut++) {
//
//  This allows the other controls in the dialog box to function 
//  during the search.  
//
    while (PeekMessage(&msg,null,0,0,PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (state == DICTSTATE_ABORT) break;    // Dicitionary dialog box is asking us to abort.
//
//  Get the next line from the dictionary.  
//
    get_line (index,dict,cut,buffer);
    if (dict_comp(key,buf,length)) break;   // No longer match.  Search is done, let's get out of here.
    check_entry (buf,length);
  }
//
//  Clean up and exit.
//
  CloseHandle (dict);   
  CloseHandle (index);
  return;
}

//
//  This is the main search engine.  This is where you go when you
//  click SEARCH.  This handles getting the parameters from the dialog
//  box.  This routine could have been embedded (and it once was) in 
//  the dialog box handler, however, that is just too hard to deal with,
//  so I sepated it.
//
//  Special macros used to modify the search string then actually do the 
//  search.  These are simply defined to simply the writing of the code.
//  
//      SEARCH_END -- Changes the end character and then does a search.
//      SEARCH_ADD -- Adds a character to the end of the stirng and searches.
//
#define SEARCH_END(x) { ch[0] = x; if ((abort = do_search(search,length  ))) break; }
#define SEARCH_ADD(x) { ch[1] = x; if ((abort = do_search(search,length+1))) break; }

void JWP_dict::search_dict () {
  int    ascii,i,length;
  KANJI *search_ptr;                // Pointer to edit-box version of the search string (NO NOT CHANGE!)
  KANJI  search[MAX_KEY_LENGTH+1];  // Copy of the edit-box search string that can be modified.
  int    first = true;              // Indicates first pass in a best fit search.
  int    abort;                     // Indicates the abort condition from the last search.
  char  *ptr;                       // Holds romaji for kana character at end of search string.
//
//  Intitlaize the search parameters.
//
  if (is_searching()) return;                   // Already searching.
  get_checkboxes ();                            // Get all the settings.
  length = JE_GetText(dialog,IDC_DDSTRING,&search_ptr);
  if (length > MAX_KEY_LENGTH) length = MAX_KEY_LENGTH;     // Truncate user string if necessary.
  while (length && ISSPACE(search_ptr[length-1])) length--;
  SendDlgItemMessage (dialog,IDC_DDRESULT,JL_RESET,0,0);
//
//  Pre-pocess key.  This converts the key to hiragana for faster searching,
//  and checks various parameters in the key for later use!
//
  ascii = false;
  for (i = 0; i < length; i++) {
    search[i] = search_ptr[i];
    if      (ISASCII   (search[i])) ascii = true;
    else if (ISKATAKANA(search[i])) search[i] = BASE_HIRAGANA | (search[i] & 0xff);
  }
//
//  Initialize the search system.
//
  matches = rejected = 0;
  state   = DICTSTATE_SEARCH;
  message (get_string(IDS_DD_SEARCHING));
//
//  Setup search flags out here so if we are doing an advanced search,
//  we don't have to evaluate these again and again.
//
  filter = false;
  for (i = DICTKEY_START; i < NUMBER_DICTKEYS; i++) if (dict_keys[i].reject) { filter = true; break; }
  nonames = dict_keys[DICTKEY_NAMES].reject && dict_keys[DICTKEY_PLACES].reject;
//
//  Do the search.
//

  switch (abort = do_search(search,length)) {
    case DICTSEARCH_SHORT:
         error (IDS_DD_ERRORLENGTH); 
         state = DICTSTATE_IDLE;
         return;
    case DICTSEARCH_MIXED:
         error (IDS_DD_ERRORASCIIKANA); 
         state = DICTSTATE_IDLE;
         return;
    case DICTSEARCH_ABORT:
         break;
  }
//''''''''''''''''''''''''''''''''''''''''''
//
//  Adaptive search engine
//
  KANJI *ch;
  i = 0;
  if (!abort && !ascii && jwp_config.cfg.dict_advanced) {
    while (!matches || (first && jwp_config.cfg.dict_always) || jwp_config.cfg.dict_showall) {
      if (first) first = false;
        else {
          if ((abort = do_search(search,length))) break;
        }
//
//  Get the last character in the search string.  If the character is a
//  kana then we have some processing to do.  If the last character is not 
//  a kana then there are less possible matches that we can do.
//
      ch = &search[length-1];
      if (!ISKANA(*ch)) {
        SEARCH_ADD (HIRAGANA_RU);                               // Could be an ichidan doushi.
        if (jwp_config.cfg.dict_iadj) SEARCH_ADD (HIRAGANA_I);  // Could be an i-adjative.
      }
//
//  Processisng for words ending in kana.
//
      else {
        if (*ch == HIRAGANA_I  ) {              // te/ta-forms for ku and gu verbs.
          SEARCH_END (HIRAGANA_KU);
          SEARCH_END (HIRAGANA_GU);
          *ch = HIRAGANA_I;                     // Need to restore the character for later!
        }

        if (*ch == HIRAGANA__TU) {              // te/ta-forms for u, tsu, and ru verbs.
          SEARCH_END (HIRAGANA_U);
          SEARCH_END (HIRAGANA_TSU);
          SEARCH_END (HIRAGANA_RU);
        }
        else if (*ch == HIRAGANA_N) {           // te/ta-forms for fu, bu, nu, mu verbs.
          SEARCH_END (HIRAGANA_NU);
          SEARCH_END (HIRAGANA_BU);
          SEARCH_END (HIRAGANA_MU);
        }
        else {
//
//  Get romaji value.
//
          if (*ch == HIRAGANA_WA) *ch = HIRAGANA_A;
          ptr = kana_to_ascii(*ch);
          i = strlen(ptr)-1;
//
//  Try imperative of ichidan doushi.
//
          if ((ptr[i] == 'i') || (ptr[i] == 'e')) SEARCH_ADD (HIRAGANA_RU);
//
//  Try treating as a godan doushi.
//
          if (ptr[i] != 'u') {
            if ((*ch = (!i ? HIRAGANA_U : godan_kana(*ptr)))) {
              if ((abort = do_search(search,length))) break;
            }
          }
//
//  Try to make an i-adjative.
//
//  ### This could be improved. ###
//
          if (jwp_config.cfg.dict_iadj) SEARCH_END (HIRAGANA_I);
        }
//
//  Shorten the string.
//
      }
      length--;
    }
  }
//
//  The is the cleanup steps.
//
  if (abort == DICTSEARCH_ABORT) message (get_string(rejected ? IDS_DD_ABORTREJECT : IDS_DD_ABORT),matches,rejected);
  if (matches) {                                // Handle results.
    SendDlgItemMessage (dialog,IDC_DDRESULT,JL_SETSEL,true,0);
    SetFocus (GetDlgItem(dialog,IDC_DDRESULT)); 
  }
  else {
    if (!rejected) message (get_string(IDS_DD_NOMATCH));
    MessageBeep (MB_ICONASTERISK);
  }
  state = DICTSTATE_IDLE;
  return;
}

//
//  Performs a dictionary search based on a memory dictionary.  The
//  dictionary should be loaded before calling this routine.
//
//      key    -- Key to be searched for.
//      length -- Length of the key.
//      dict   -- Pointer to the dictionary memory image (has a fake
//                '\n' in the first slot).
//
void JWP_dict::search_memory (byte *key,int length,byte *dict) {
  int   i;
  byte *ptr,*p,*p2,buffer[SIZE_BUFFER];

  for (ptr = dict+1; *ptr; ptr++) {
    if (dict_comp(key,ptr,length)) {            // No match.
      if (*ptr >= 0x80) ptr++;                  // For kana/kanji characters skip two bytes.
      continue;
    }
    for (i = 0, p = ptr; (i < SIZE_LINE-9) && !ISCRLF(*ptr); ptr--, i++); 
                                                // Find beginning of entry.
    buffer[0] = '\n';                           // Build duplicate entry.
    p2 = buffer+(p-ptr);                        // Calculate same relative place for end/begin 
    for (i = 1, ptr++; (i < SIZE_BUFFER-2) && !ISCRLF(*ptr); ptr++) buffer[i++] = *ptr;     
                                                // Diplicate entry (use ptr so skip rest of entry)
    buffer[i] = '\n';                           // Terminate enry.
    check_entry (p2,length);                    // Process entry.
  }
  return;
}

//
//  Set the state of the four check-boxes in the main dictionary dialog.
//
void JWP_dict::set_checkboxes () {
  int i;
  if      ( dict_keys[DICTKEY_PLACES].reject &&  dict_keys[DICTKEY_NAMES].reject) i = BST_CHECKED;
  else if (!dict_keys[DICTKEY_PLACES].reject && !dict_keys[DICTKEY_NAMES].reject) i = BST_UNCHECKED;
  else                                                                            i = BST_INDETERMINATE;
  CheckDlgButton (dialog,IDC_DDNONAME  ,i);
  CheckDlgButton (dialog,IDC_DDADVANCED,jwp_config.cfg.dict_advanced);
  CheckDlgButton (dialog,IDC_DDBEGIN   ,dict_keys[DICTKEY_BEGIN].reject);
  CheckDlgButton (dialog,IDC_DDEND     ,dict_keys[DICTKEY_END  ].reject);
  return;
}

//
//  This routine launches the user dictionary.  This will either bring the current 
//  user-dicitonary function to the front, or will create a new one.
//
void JWP_dict::user_dictionary () {
  if (user_dialog) SetForegroundWindow (user_dialog);
    else JCreateDialog (IDD_DICTUSER,dialog,(DLGPROC) dialog_userdict); 
  return;
}

//
//  End Class JWP_dict.
//
//-------------------------------------------------------------------


// ### Should make dictionary class dynamically allocated

// ### Think about improvements based on changing the rejection keys.


//
//  Small utlity routine used to copy the clipboard contets to the current edit box.
//  This is currently only used in the dictionary routines, for clipboard tracking.
//  The commands are done directly to prevent activating the cursor routines in the 
//  edit box in case the edit box is not selected.
//
int JWP_file::edit_clip () {
  sel.type      = SELECT_EDIT;      // Select all text
  sel.pos1.para = first;
  sel.pos1.line = first->first;
  sel.pos1.pos  = 0;
  sel.pos2.para = last;
  sel.pos2.line = last->first;
  sel.pos2.pos  = last->length;
  sel.pos2.rel ();
  clip_paste (false);               // Paste.  This will remove all previous text
  return (true);
}


