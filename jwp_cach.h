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
//  This module implements a somewhat generaliezed IO buffering cache.
//  This cache has the advantage of speeding up both reads and writes.
//  Additionally, the generalized cache system allows the same code to 
//  be used for input/output from/to either files or the clipboard.
//
#ifndef jwp_cach_h
#define jwp_cach_h

#define IO_OK   0   // Used to indicate block read is ok.
#define IO_EOF  -1  // Used to indicate end of file form a cache read.

class IO_cache {
friend class JIS_convert;
public:
  int     get_block    (void *data,int size);       // Get block from input stream.
  int     get_char     (void);                      // Get character from input buffer.
  byte   *get_line     (void);                      // Get CR/LF terminated line from buffer.
  long    flush        (void);                      // Finish a write to an output stream.
  void    input_clip   (HGLOBAL clip);              // Initialize to read from clipboard.
  void    input_end    (void);                      // Cleanup routine called at the end of inputs.
  void    input_file   (byte *buffer,int size,HANDLE file);    // Intialize a file input stream.
  void    input_memory (byte *buffer);              // Initilize a memory block as an input stream.
  HGLOBAL output_clip  (long size);                 // Intialize an output stream for the clipboard.
  void    output_count (void);                      // False output stream that counts the characters.
  void    output_file  (byte *buffer,int size,HANDLE file);    // Intialize a file output stream.
  void    put_block    (void *data,int size);       // Put block to output stream.
  void    put_char     (int ch);                    // Put character into output buffer.
  void    rewind       (void);                      // Rewind an input buffer for reuse (used when finding a type).
  void inline set_size (int cs) { charsize = cs; }  // Set character block size.
private:
  int     reload       (void);                      // Reload the input cache (INTERNAL routine).
  void inline unget_char (void) { position--; }     // Un-get a character from the input buffer.

  HANDLE  handle;           // Handle used for input and output file streams.
  HGLOBAL block;            // Golobal block for clipboard work.
  byte   *buffer;           // Pointer to the buffer (both input and output).
  long    count;            // Pointer to count location.
  long    size;             // Size of the buffer (in bytes).
  long    position;         // Current position in the buffer.
  long    bufsize;          // Storage used only for rewind.
  short   error;            // Indidates an error has occured (used only during write).
  short   charsize;         // Size of character to handle UNICODE and CE.
};

typedef IO_cache IO_cache;

#endif jwp_cach_h
