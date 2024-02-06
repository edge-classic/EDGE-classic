//------------------------------------------------------------------------
//  WAD Reading / Writing
//------------------------------------------------------------------------
//
//  AJ-BSP  Copyright (C) 2001-2023  Andrew Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __AJBSP_WAD_H__
#define __AJBSP_WAD_H__

// EPI
#include "file.h"

namespace ajbsp
{

class WadFile;

enum MapFormat
{
    kMapFormatInvalid = 0,
    kMapFormatDoom,
    kMapFormatHexen,
    kMapFormatUDMF
};

class Lump
{
    friend class WadFile;

  private:
    WadFile *parent_;

    const char *name_;

    int lump_start_;
    int lump_length_;

    // constructor is private
    Lump(WadFile *par, const char *name, int start, int len);
    Lump(WadFile *par, const struct RawWadEntry *entry);

    void MakeEntry(struct RawWadEntry *entry);

  public:
    ~Lump();

    const char *Name() const
    {
        return name_;
    }
    int Length() const
    {
        return lump_length_;
    }

    // do not call this directly, use WadFile::RenameLump()
    void Rename(const char *new_name);

    // attempt to seek to a position within the lump (default is
    // the beginning).  Returns true if OK, false on error.
    bool Seek(int offset);

    // read some data from the lump, returning true if OK.
    bool Read(void *data, size_t len);

    // read a line of text, returns true if OK, false on EOF
    bool GetLine(char *buffer, size_t buf_size);

    // write some data to the lump.  Only the lump which had just
    // been created with WadFile::AddLump() or RecreateLump() can be
    // written to.
    bool Write(const void *data, int len);

    // write some text to the lump
    void Printf(const char *msg, ...);

    // mark the lump as finished (after writing data to it).
    bool Finish();

    // predicate for std::sort()
    class OffsetComparisonPredicate
    {
      public:
        inline bool operator()(const Lump *A, const Lump *B) const
        {
            return A->lump_start_ < B->lump_start_;
        }
    };

  private:
    // deliberately don't implement these
    Lump(const Lump &other);
    Lump &operator=(const Lump &other);
};

//------------------------------------------------------------------------

class WadFile
{
    friend class Lump;

  private:
    std::string filename_;

    char mode_; // mode value passed to ::Open()

    FILE *fp_;

    epi::MemFile *memory_fp_;

    char kind_; // 'P' for PWAD, 'I' for IWAD

    // zero means "currently unknown", which only occurs after a
    // call to BeginWrite() and before any call to AddLump() or
    // the finalizing EndWrite().
    int total_size_;

    std::vector<Lump *> directory_;

    int dir_start_;
    int dir_count_;

    // these are lump indices (into 'directory' vector)
    std::vector<int> levels_;
    std::vector<int> patches_;
    std::vector<int> sprites_;
    std::vector<int> flats_;
    std::vector<int> tx_tex_;

    bool begun_write_;
    int  begun_max_size_;

    // when >= 0, the next added lump is placed _before_ this
    int insert_point_;

    // constructor is private
    WadFile(std::string name, char mode, FILE *fp, epi::MemFile *memory_fp);

  public:
    ~WadFile();

    // open a wad file.
    //
    // mode is similar to the fopen() function:
    //   'r' opens the wad for reading ONLY
    //   'a' opens the wad for appending (read and write)
    //   'w' opens the wad for writing (i.e. create it)
    //
    // Note: if 'a' is used and the file is read-only, it will be
    //       silently opened in 'r' mode instead.
    //
    static WadFile *Open(std::string filename, char mode = 'a');

    static WadFile *OpenMem(std::string filename, uint8_t *raw_wad, int raw_length);

    bool IsReadOnly() const
    {
        return mode_ == 'r';
    }

    int TotalSize() const
    {
        return total_size_;
    }

    int NumLumps() const
    {
        return (int)directory_.size();
    }
    Lump *GetLump(int index);
    Lump *FindLump(const char *name);
    int   FindLumpNumber(const char *name);

    Lump *FindLumpInNamespace(const char *name, char group);

    int LevelCount() const
    {
        return (int)levels_.size();
    }
    int LevelHeader(int lev_num);
    int LevelLastLump(int lev_num);

    // these return a level number (0 .. count-1)
    int LevelFind(const char *name);
    int LevelFindByNumber(int number);
    int LevelFindFirst();

    // returns a lump index, -1 if not found
    int LevelLookupLump(int lev_num, const char *name);

    MapFormat LevelFormat(int lev_num);

    void SortLevels();

    // all changes to the wad must occur between calls to BeginWrite()
    // and EndWrite() methods.  the on-disk wad directory may be trashed
    // during this period, it will be re-written by EndWrite().
    void BeginWrite();
    void EndWrite();

    // change name of a lump (can be a level marker too)
    void RenameLump(int index, const char *new_name);

    // remove the given lump(s)
    // this will change index numbers on existing lumps
    // (previous results of FindLumpNum or LevelHeader are invalidated).
    void RemoveLumps(int index, int count = 1);

    // this removes the level marker PLUS all associated level lumps
    // which follow it.
    void RemoveLevel(int lev_num);

    // removes any GL-Nodes lumps that are associated with the given level.
    void RemoveGLNodes(int lev_num);

    // removes any ZNODES lump from a UDMF level.
    void RemoveZNodes(int lev_num);

    // insert a new lump.
    // The second form is for a level marker.
    // The 'max_size' parameter (if >= 0) specifies the most data
    // you will write into the lump -- writing more will corrupt
    // something else in the WAD.
    Lump *AddLump(const char *name, int max_size = -1);
    Lump *AddLevel(const char *name, int max_size = -1, int *lev_num = NULL);

    // setup lump to write new data to it.
    // the old contents are lost.
    void RecreateLump(Lump *lump, int max_size = -1);

    // set the insertion point -- the next lump will be added *before*
    // this index, and it will be incremented so that a sequence of
    // AddLump() calls produces lumps in the same order.
    //
    // passing a negative value or invalid index will reset the
    // insertion point -- future lumps get added at the END.
    // RemoveLumps(), RemoveLevel() and EndWrite() also reset it.
    void InsertPoint(int index = -1);

  private:
    static WadFile *Create(std::string filename, char mode);

    // read the existing directory.
    void ReadDirectory();

    void DetectLevels();
    void ProcessNamespaces();

    // look at all the lumps and determine the lowest offset from
    // start of file where we can write new data.  The directory itself
    // is ignored for this.
    int HighWaterMark();

    // look at all lumps in directory and determine the lowest offset
    // where a lump of the given length will fit.  Returns same as
    // HighWaterMark() when no largest gaps exist.  The directory itself
    // is ignored since it will be re-written at EndWrite().
    int FindFreeSpace(int length);

    // find a place (possibly at end of WAD) where we can write some
    // data of max_size (-1 means unlimited), and seek to that spot
    // (possibly writing some padding zeros -- the difference should
    // be no more than a few bytes).  Returns new position.
    int PositionForWrite(int max_size = -1);

    bool FinishLump(int final_size);
    int  WritePadding(int count);

    // write the new directory, updating the dir_xxx variables
    // (including the CRC).
    void WriteDirectory();

    void FixGroup(std::vector<int> &group, int index, int num_added, int num_removed);

  private:
    // deliberately don't implement these
    WadFile(const WadFile &other);
    WadFile &operator=(const WadFile &other);

  private:
    // predicate for sorting the levels[] vector
    class LevelNameComparisonPredicate
    {
      private:
        WadFile *wad_;

      public:
        LevelNameComparisonPredicate(WadFile *w) : wad_(w)
        {
        }

        inline bool operator()(const int A, const int B) const
        {
            const Lump *L1 = wad_->directory_[A];
            const Lump *L2 = wad_->directory_[B];

            return (strcmp(L1->Name(), L2->Name()) < 0);
        }
    };
};

} // namespace ajbsp

#endif /* __AJBSP_WAD_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
