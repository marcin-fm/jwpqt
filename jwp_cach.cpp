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
//  Reading data:
//
//  First establish an input stream.  This is done by calling a routine
//  like input_file() or input_clip(), etc.  Then you perform you IO
//  by calling routines like get_char() and get_block().  When finished,
//  you can call the routine input_end().  Calling the termination 
//  routine is manditory for clipboard reads, and optional for all others.
//  
//
//  Wrinting a data:
//
//  First establish an output stream.  This is done by calling a 
//  routine like output_file(), output_clip(), or output_count().
//  You then repeadly call put_char() and put_block, to write data.
//  When finished, you must call flush().  When you call flush(), you
//  will get an error code for the entire write, unless you are doing
//  a count, in which case you will get a byte count.
//
#include "jwpce.h"
#include "jwp_cach.h"
#include "jwp_inpt.h"       // For the definition of IS_CRLF

//===================================================================
//
//  Begin Class IO_cache.
//
//  This class implements a io buffer system used for reading and  
//  wirting files.
//  

//--------------------------------
//
//  Definition that allows the cache buffer to be accessed as an array
//  of unsigned short integers.  The offest is stored in byte values, 
//  which allows switching between access modes freely.
//
#define BUFFER(x)   (((unsigned short *)(buffer+x))[0])

//--------------------------------
//
//  Flush the buffer to a file and preform any end of write cleanup.
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
long IO_cache::flush () {
  int err;
  unsigned long done;
  if (!buffer) return (count);
  if (handle == INVALID_HANDLE_VALUE) { GlobalUnlock (block); return (error); }
  err = !WriteFile (handle,buffer,position,&done,NULL);
  position = 0;
  if (err) error = true;
  return (error);
}

//--------------------------------
//
//  Transfers a block out of an input buffer.  This can also be used 
//  to skip forward through a cache by setting the data parameter to 
//  NULL.
//
//      data   -- Pointer to location where to trnasfer the data to.  
//                If this is NULL, data will not be transfered, but
//                the cache pointer will still be adnvanced.  This 
//                effectivly results in a skip through the cache.
//      bsize  -- Size to transfer in bytes.
//
//      RETURN -- A non-zero return indicates an error or EOF condition.
//
int IO_cache::get_block (void *data,int bsize) {
  if ((position+bsize >= size) && reload()) return (IO_EOF);
  if (position+bsize >= size) return (IO_EOF);
  if (data) memcpy (data,buffer+position,bsize);
  position += bsize;
  return (IO_OK);
}

//--------------------------------
//
//  Internal routine that reads a character form the input buffer.  If 
//  necessary, the buffer is refreshed from the data source.
//
//      RETURN -- Next character from the buffer.
//
int IO_cache::get_char () {
  int i,j;
  if ((position+2*charsize > size) && reload()) {
    position += charsize;   // Need to always change the posotion, in case we need to 
    return (IO_EOF);        //   unget the character, we need the position to be right.
  }
  if (charsize > 1) {
    j = i = BUFFER(position);       // We can cheet of UNICODE, and take the first zero
  }                                 //   character as EOF.
  else {
    i = buffer[position];           // On single byte reads we must look for two zeros in
    j = buffer[position+1];         //   a row.  This allows us to read JWP's stupid zero
  }                                 //   flag for extended ascii (bad choice of values)!
  position += charsize;
  if (!i && !j) return (IO_EOF);
  return (i);
}

//--------------------------------
//
//  This routine reads a line from the buffer.  Note, this routine
//  does not extract the line from the cache, but rather returns a 
//  pointer into the cache.  The cache pointer is advanced to past
//  the end of the line.  This means if you are using a memory or 
//  clipboard cache, and you modify the contents of the returned
//  pointer you can destroy your data.
//
//      RETURN -- Pointer to line, or a value of NULL if the cache is
//                at the end.
//
byte *IO_cache::get_line () {
  int i,pos;
  int load = false;     // This indicates we have tried a reload, we only want to do this once.
  while (true) {
    if (position >= size) return (NULL);                        // Cache is empty!
    pos = position;                                             // Save position for return
    for (i = position; (i < size) && !ISCRLF(buffer[i]); i++);  // Find CR or LF
    if ((i < size) && (buffer[i] == '\r')) i++;                 // Skip CR
    if ((i < size) && (buffer[i] == '\n')) i++;                 // Skip 
    if (load || (i < size)) break;                              // Either have line or buffer has been reloaded
    load = true;                                                // Reload the buffer 
    reload ();
  }
  position = i;                 // Set buffer pointer
  return (buffer+pos);          // Calcualte return value.
}
//### may want to reivise this to not check cr&lf since in the long run this will not be used.

//--------------------------------
//
//  Setup an input stream from the clipboard.  Basicaly, this is simply
//  an input stream entirally confined within a single block of memory.
//
//      hglobal -- Block of memory.  Generally, this is obtained from
//                 GetClipboardData().
//
void IO_cache::input_clip (HGLOBAL hglobal) {
  charsize   = 1;
  block      = hglobal;
  handle     = INVALID_HANDLE_VALUE;
  buffer     = (byte *) GlobalLock(block);
  position   = 0;
  size       = GlobalSize(block);
  return;
}

//--------------------------------
//
//  Small stub routine to cleanup after a write.  This is really only
//  necassary after a clipboard write.  When used after a file write,
//  it puts the file pointer back at the beiggning.  This can be used.
//  after checking the file type.
//
void IO_cache::input_end () {
  if (block) GlobalUnlock (block);
  return;
}

//--------------------------------
//
//  Sets the cache to use a file as input.
//
//      buf      -- Buffer for input buffering.
//      bsize    -- Size of buffer.
//      file     -- Handle to file (must be open and at the correct position).
//
void IO_cache::input_file (byte *buf,int bsize,HANDLE file) {
  unsigned long done;
  charsize   = 1;
  block      = null;
  handle     = file;
  buffer     = buf;
  position   = 0;
  ReadFile (handle,buffer,bsize,&done,NULL);    // Load buffer.
  size       = done;
  bufsize    = bsize;
  return;
}

//--------------------------------
//
//  Sets the cache to use a memory block as input.
//
//      memory -- Pointer to null terminated memory block to be 
//                used as an input source.  This is bascially identical
//                to using the clipboard, but uses a simple memory 
//                block.
//
void IO_cache::input_memory (byte *memory) {
  charsize   = 1;
  block      = null;
  handle     = INVALID_HANDLE_VALUE;
  buffer     = memory;
  position   = 0;
  size       = strlen((char *) buffer);
  return;
}

//--------------------------------
//
//  Setup an output stream to the clipboard.
//
//      asize  -- Size of clipboard write.  This will be increased by 
//                one, internally, to allow for a terminating NULL.
//
//      RETURN -- Pointer to allocated clipboard global memory block.
//                This can be passed to SetClipboardData(), after 
//                flush() has been called.
//
HGLOBAL IO_cache::output_clip (long asize) {
  charsize   = 1;
  error      = false;       // Clear error flag.
  handle     = INVALID_HANDLE_VALUE;
  block      = GlobalAlloc(GHND,asize+1);
  buffer     = (byte *) GlobalLock(block);
  size       = asize+1;
  position   = 0;
  return (block);
}

//--------------------------------
//
//  Setup an output stream for a specialized output when no data is 
//  wirtten, but rather the number of bytes output is counted.  
//  Generally, this is used to determine the size of a data block for
//  the clipboard.
//
void IO_cache::output_count () {
  charsize   = 1;
  error      = false;       // Clear error flag.
  handle     = null;
  buffer     = NULL;
  block      = null;
  count      = 0;
  return;
} 

//--------------------------------
//
//  Sets the convert to use a file as output.  This routine also clears
//  the system error flag.
//
//      buf      -- Buffer for input buffering.
//      bsize    -- Size of buffer.
//      file     -- Handle to file (must be open and at the correct position).
//
void IO_cache::output_file (byte *buf,int bsize,HANDLE file) {
  charsize   = 1;
  error      = false;       // Clear error flag.
  handle     = file;
  buffer     = buf;
  block      = null;
  size       = bsize;
  position   = 0;
  return;
}

//--------------------------------
//
//  Put a block of data into the buffer.
//
//      data  -- Data to put into buffer.
//      bsize -- Size of the data in bytes.
//
void IO_cache::put_block (void *data,int bsize) {
  if (!buffer) { count += bsize; return; }
  if (position+bsize >= size) flush ();
  memcpy (buffer+position,data,bsize);
  position += bsize;
  return;
}

//--------------------------------
//
//  Internal rotuine to put a character to an output stream.  If 
//  necessary, the output stream is flushed to the output device.
//
//      ch -- Character to put.
//
void IO_cache::put_char (int ch) {
  if (!buffer) { count += charsize; return; }
  if (position+charsize >= size) flush();
  if (charsize > 1) BUFFER(position) = ch; else buffer[position] = ch;
  position += charsize;
  return;
}

//--------------------------------
//
//  Reloads the cache.  This is used for read caches.  This routine 
//  will move anything remainning in the cache buffer to the beginning
//  of the buffer and then fill the rest of the buffer with new data.
//
//      RETURN -- A non-zero return indicates an error in reloading
//                or the end of the file has been reached.
//
int IO_cache::reload () {
  unsigned long done;
  if (handle == INVALID_HANDLE_VALUE) {                      // Cannot reload a memory cache.
    if (position >= size) return (true);
    return (false);
  }
  memmove  (buffer,buffer+position,size-position);
  ReadFile (handle,buffer+size-position,position,&done,NULL);
  size     = size-position+done;
  position = 0;
  return (!size);
}

//--------------------------------
//
//  This routine rewinds an imput buffer to the beginning.  This si 
//  used to reset the buffer back to the beginning to be used over
//  again.  This only functions on imput caches.
//
void IO_cache::rewind () {
  int csize = charsize;                         // Save character size
  input_end ();
  if (handle != INVALID_HANDLE_VALUE) {
    SetFilePointer (handle,0,NULL,FILE_BEGIN);  // Rewind file
    input_file     (buffer,bufsize,handle);
  }
  else if (block) {
    input_clip (block);                         // Rewind clipboard
  }
  else {
    position = 0;                               // Rewind memory block.
  }
  charsize = csize;                             // Restore character size
  return;
}

//
//  End Class IO_cache.
//
//===================================================================


