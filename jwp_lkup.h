//===================================================================//
//                                                                   //
//  JWPce Copyright (C) Glenn Rosenthal, 1998-2004, 2005             //
//                                                                   //
//  JWPce is free sotware distributed under the terms of the         //
//  GNU General Public License.                                      //
//                                                                   //
//  The radical lookup tables used were originally developed by      //
//  Michael Raine and Derc Yamasaki.                                 //
//                                                                   //
//===================================================================//

//===================================================================
//
//  This module implements the kanji search by radical and count dialog
//  box and all fucntions associated with this kanji lookup feature.
//
//  The kanji lookup is based on indexed radical and count lists.  An 
//  entry is searched by first finding the entry in the index file.  
//  These are basically a sequecial list of structures.  These structures
//  have two fields.  The first entry is the offset into the data file.
//  The second entry is the number of kanji for the entry.  These can 
//  then be read from the data file.
//
//  The selection is done as follows.  The first radical is read in
//  and forms the basis of kanji to be accepted.  
//
//  As each new radical is read in as another list.  All kanji in the 
//  master list that are not in the new radical list are removed 
//  (replaced by 0).  This keeps only all the kanji that are on all
//  lists.
//
#ifndef jwp_lkup_h
#define jwp_lkup_h

extern int  initialize_radlookup (WNDCLASS *wclass);    // Intializes the radical lookup
extern void bushu_lookup         (JWP_file *from);      // Bushu (single key radical) lookup
extern void bushu2_lookup        (JWP_file *from);      // Bushu lookup compressed.
extern void fourcorner_lookup    (JWP_file *from);      // Four corners lookup
extern void index_lookup         (JWP_file *from);      // Lookup by dictionary index
extern void radical_lookup       (JWP_file *from);      // Actually implements a radical lookup.
extern void reading_lookup       (JWP_file *from);      // Lookup based on dictionary file.
extern void skip_lookup          (JWP_file *from);      // Actually does the skip lookup
extern void spahn_lookup         (JWP_file *from);      // Hadamitzky/Spahn Lookup
extern void jis_table            (JWP_file *from);      // Implements the JIS-table.

#endif jwp_lkup_h
