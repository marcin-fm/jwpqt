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
//  This modlue contains the paragraph class and some support routines.
//  This was all moved here from the the modlues jwp_file.cpp and 
//  jwpce.cpp to keep it collected.
//
#ifndef jwp_para_h
#define jwp_para_h

//--------------------------------
//
//  Struct line.
//
//  Spefies a line break within a paragraph.
//
typedef struct Line {                           // Individual line
  int          first;                           // Offset of fist kanji for line.
  int          length;                          // Length of line
  struct Line *next,*prev;                      // Next and previous lines
} Line;

//--------------------------------
//
//  Class Paragraph.
//
//  Class defines a paragraph (basic unit of editor) and the operations
//  that can be perfomred on it.
//

typedef class Paragraph {                       // Paragraph element
friend class Position;
friend class JWP_file;
friend class JWP_search;
public:
  Paragraph ();                                 // Unquied constructor.
 ~Paragraph ();                                 // Destructor.
  void        add_char   (int ch);              // Fast add a character used only during file reads
  void        del_char   (JWP_file *file,Line *line,int pos);  // Delete character from the paragraph.
  void        ins_string (JWP_file *file,Line *line,int pos,KANJI *string,int len);  // Insert character
  void        rep_char   (JWP_file *file,Line *line,int pos,int ch);
private:
  int         alloc      (void);                // include buffer allocation.
  int         add_to_clip(int start,int end);   // Copy part of this paragraph to the clipboard.
  int         copy_para  (Paragraph *para);     // Copy entire paragraph to this paragraph.
  void        copy_format(Paragraph *para);     // Copy formating infomation form aother paragraph.
  void        format     (JWP_file *file,Line *line,int redraw);    // Pargaraph format.
  int         line_start (Line *line);          // x-pixal location of the start of a line.
  Line       *new_line   (void);                // Add line pointer to end of file.
  int         set_text   (KANJI *data,int len); // Set entire text buffer.
  class Paragraph *next,*prev;                  // Pointers to next & previous paragraph.
  struct Line     *first,*last;                 // First and last line.
  KANJI           *text;                        // Actual text for paragraph (all of it).
  int              size;                        // Allocation length of pharagrph buffer.
  int              length;                      // Number of characters in text line.
  short            spacing;                     // Line spaceing in units of 100.
  byte             page_break;                  // Indicates this is a page break.
  byte             indent_left,indent_right;    // Margin settings in KANJI units.
  signed char      indent_first;                // Offset for first line of paragraph.
} Paragraph;

#endif jwp_para_h
