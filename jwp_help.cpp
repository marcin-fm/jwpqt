//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//===================================================================//

#include "jwpce.h"
#include "jwp_conf.h"
#include "jwp_help.h"

//--------------------------------
//
//  Process help based on ID number.  Id numbers come from Windows 95
//  type help.  For Windows CE they are converted in URL's and sent
//  to Pocket Internet Exporer for processing.
//
//      hwnd -- Window that called help, so system cant return to 
//              this window.
//      id   -- Help ID to find.  An ID of zero will bring the 
//              help contents.
//
//  Due to a bug in Pocket Internet Explorer (2.0 SP 1), full URL's 
//  are not accepted, thus help can only get you to the correct 
//  chapter of help, and not to the actual section.  The part of the 
//  URL after # is not processed by PIE.
//
#define SIZE_HELP   (sizeof(help)/sizeof(struct helpinfo))  // Number of help ID used in Windows CE.

void do_help (HWND hwnd,int id) {
#ifdef WINCE
  struct helpinfo {     // Used to translate ID to URL.
    ushort id;          // ID number
    tchar *file;        // Main part of file name "m_<file>.html"
    tchar *section;     // Section within the file "#<section>".  (NULL = top of file.)
  };
  static struct helpinfo help[] = {
    { IDH_INSTALL_ADVANCED  ,TEXT("install")  ,TEXT("MORE")         },
    { IDH_INSTALL_MORE      ,TEXT("install")  ,TEXT("MORE")         },
    { IDH_INTERFACE_JLIST   ,TEXT("interface"),TEXT("EDITLIST")     },
    { IDH_INTERFACE_COLOR   ,TEXT("interface"),TEXT("COLOR")        },
    { IDH_TEXT_USERKANJI    ,TEXT("text")     ,TEXT("USER")         },
    { IDH_EDIT_PARAGRAPH    ,TEXT("edit")     ,TEXT("PARA")         },
    { IDH_EDIT_SEARCH       ,TEXT("edit")     ,TEXT("SEARCH")       }, 
    { IDH_KANJI_CHARINFO    ,TEXT("kanji")    ,TEXT("CHARINFO")     },
    { IDH_KANJI_RADLOOKUP   ,TEXT("kanji")    ,TEXT("RADLOOKUP")    },
    { IDH_KANJI_BUSHULOOKUP ,TEXT("kanji")    ,TEXT("BUSHULOOKUP")  },
    { IDH_KANJI_BSLOOKUP    ,TEXT("kanji")    ,TEXT("BSLOOKUP")     },
    { IDH_KANJI_SKIPLOOKUP  ,TEXT("kanji")    ,TEXT("SKIPLOOKUP")   },
    { IDH_KANJI_HSLOOKUP    ,TEXT("kanji")    ,TEXT("HSLOOKUP")     },
    { IDH_KANJI_FCLOOKUP    ,TEXT("kanji")    ,TEXT("FCLOOKUP")     },
    { IDH_KANJI_READLOOKUP  ,TEXT("kanji")    ,TEXT("READLOOKUP")   },
    { IDH_KANJI_INDEXLOOKUP ,TEXT("kanji")    ,TEXT("INDEXLOOKUP")  },
    { IDH_KANJI_JISTABLE    ,TEXT("kanji")    ,TEXT("JISTABLE")     },
    { IDH_KANJI_COUNTKANJI  ,TEXT("kanji")    ,TEXT("COUNT")        },
    { IDH_DICT_GENERAL      ,TEXT("dict")     ,NULL                 },
    { IDH_DICT_USERDICT     ,TEXT("dict")     ,TEXT("USER")         },
    { IDH_DICT_OPTIONS      ,TEXT("dict")     ,TEXT("OPTIONS")      },
    { IDH_DICT_DICTIONARIES ,TEXT("dict")     ,TEXT("DICTIONARIES") },
    { IDH_FILE_DUPLICATE    ,TEXT("files")    ,TEXT("DUPLICATE")    }, 
    { IDH_FILE_CLOSE        ,TEXT("files")    ,TEXT("CLOSE")        },
    { IDH_FILE_CHANGE       ,TEXT("files")    ,TEXT("CURRENT")      },
    { IDH_PRINT_MARGINS     ,TEXT("print")    ,TEXT("MARGINS")      },
    { IDH_PRINT_HEADERS     ,TEXT("print")    ,TEXT("HEADERS")      },
    { IDH_PRINT_SUMMARY     ,TEXT("print")    ,TEXT("SUMMARY")      },
    { IDH_OPTIONS_GENERAL   ,TEXT("opts")     ,TEXT("GENERAL")      }, 
    { IDH_OPTIONS_DISPLAY   ,TEXT("opts")     ,TEXT("DISPLAY")      },
    { IDH_OPTIONS_FONT      ,TEXT("opts")     ,TEXT("FONT")         },  
    { IDH_OPTIONS_FILE      ,TEXT("opts")     ,TEXT("FILE")         },
    { IDH_OPTIONS_MISC      ,TEXT("opts")     ,TEXT("MISC")         },
    { IDH_OPTIONS_ADVANCED  ,TEXT("opts")     ,TEXT("ADVANCED")     },
  };
  int i;
  TCHAR buffer[SIZE_BUFFER];
  for (i = 0; (i < SIZE_HELP) && (help[i].id != id); i++);  // Find ID
  if (i == SIZE_HELP) id = 0;                               // Not found, use ID 0.
  lstrcpy (buffer,TEXT("help\\m_"));        
  if (!id) {                                                // ID zero, just go to contents.
    lstrcat (buffer,TEXT("main.html"));
  }
  else {                                                    // Actual ID so build URL.
    lstrcat (buffer,help[i].file);
    lstrcat (buffer,TEXT(".html"));
    if (help[i].section) {
      lstrcat (buffer,TEXT("#"));
      lstrcat (buffer,help[i].section);
    }
  }
  CreateProcess (TEXT("iexplore"),jwp_config.name(buffer,OPEN_READ,false),NULL,NULL,0,0,NULL,NULL,NULL,NULL);
#else
  WinHelp (hwnd,jwp_config.name(TEXT("jwpce.hlp"),OPEN_READ,false),id ? HELP_CONTEXT : HELP_FINDER,id);
#endif WINCE
  return;
}

