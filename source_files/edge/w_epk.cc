//----------------------------------------------------------------------------
//  EDGE EPK Support Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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
//----------------------------------------------------------------------------

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "ddf_colormap.h"
#include "ddf_main.h"
#include "ddf_wadfixes.h"
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "l_deh.h"
#include "miniz.h"
#include "r_image.h"
#include "script/compat/lua_compat.h"
#include "snd_types.h"
#include "vm_coal.h"
#include "w_files.h"
#include "w_wad.h"

static std::string known_image_directories[5] = {"flats", "graphics", "skins", "textures", "sprites"};

class PackEntry
{
  public:
    // this name is relative to parent (if any), i.e. no slashes
    std::string name_;

    // only for Folder: the full pathname to file (for FileOpen).
    std::string full_path_;

    // for both types: path relative to pack's "root" directory
    std::string pack_path_;

    // only for ZIP: the index into the archive.
    mz_uint zip_index_;

    PackEntry(const std::string &name, const std::string &path, const std::string &ppath, mz_uint idx)
        : name_(name), full_path_(path), pack_path_(ppath), zip_index_(idx)
    {
    }

    ~PackEntry()
    {
    }

    bool operator==(const std::string &other) const
    {
        return epi::StringCaseCompareASCII(name_, other) == 0;
    }

    bool HasExtension(const char *match) const
    {
        std::string ext = epi::GetExtension(name_);
        return epi::StringCaseCompareASCII(ext, match) == 0;
    }
};

class PackDirectory
{
  public:
    std::string            name_;
    std::vector<PackEntry> entries_;

    PackDirectory(const std::string &name) : name_(name), entries_()
    {
    }

    ~PackDirectory()
    {
    }

    void SortEntries();

    size_t AddEntry(const std::string &name, const std::string &path, const std::string &ppath, mz_uint idx)
    {
        // check if already there
        for (size_t i = 0; i < entries_.size(); i++)
            if (entries_[i] == name)
                return i;

        entries_.push_back(PackEntry(name, path, ppath, idx));
        return entries_.size() - 1;
    }

    int Find(const std::string &name_in) const
    {
        for (int i = 0; i < (int)entries_.size(); i++)
            if (entries_[i] == name_in)
                return i;

        return -1; // not found
    }

    bool operator==(const std::string &other) const
    {
        return epi::StringCaseCompareASCII(name_, other) == 0;
    }
};

class PackFile
{
  public:
    DataFile *parent_;

    bool is_folder_;

    // first entry here is always the top-level (with no name).
    // everything else is from a second-level directory.
    // things in deeper directories are not stored.
    std::vector<PackDirectory> directories_;

    // for faster file lookups
    // stored as filename stems as keys; packpath as values
    std::unordered_multimap<std::string, std::string> search_files_;

    mz_zip_archive *archive_;

  public:
    PackFile(DataFile *par, bool folder) : parent_(par), is_folder_(folder), directories_(), archive_(nullptr)
    {
    }

    ~PackFile()
    {
        if (archive_ != nullptr)
            delete archive_;
    }

    size_t AddDirectory(const std::string &name)
    {
        // check if already there
        for (size_t i = 0; i < directories_.size(); i++)
            if (directories_[i] == name)
                return i;

        directories_.push_back(PackDirectory(name));
        return directories_.size() - 1;
    }

    int FindDirectory(const std::string &name) const
    {
        for (int i = 0; i < (int)directories_.size(); i++)
            if (directories_[i] == name)
                return i;

        return -1; // not found
    }

    void SortEntries();

    epi::File *OpenEntry(size_t dir, size_t index)
    {
        if (is_folder_)
            return OpenFolderEntry(dir, index);
        else
            return OpenZipEntry(dir, index);
    }

    epi::File *OpenEntryByName(const std::string &name)
    {
        if (is_folder_)
            return OpenFolderEntryByName(name);
        else
            return OpenZipEntryByName(name);
    }

    int EntryLength(size_t dir, size_t index)
    {
        epi::File *f = OpenEntry(dir, index);
        if (f == nullptr)
            return 0;

        int length = f->GetLength();
        delete f; // close it

        return length;
    }

    uint8_t *LoadEntry(size_t dir, size_t index, int &length)
    {
        epi::File *f = OpenEntry(dir, index);
        if (f == nullptr)
        {
            length = 0;
            return new uint8_t[1];
        }

        uint8_t *data = f->LoadIntoMemory();
        length        = f->GetLength();

        // close file
        delete f;

        if (data == nullptr)
        {
            length = 0;
            return new uint8_t[1];
        }

        return data;
    }

  private:
    epi::File *OpenFolderEntry(size_t dir, size_t index);
    epi::File *OpenZipEntry(size_t dir, size_t index);

    epi::File *OpenFolderEntryByName(const std::string &name);
    epi::File *OpenZipEntryByName(const std::string &name);
};

int PackFindStem(PackFile *pack, const std::string &name)
{
    return pack->search_files_.count(name);
}

//----------------------------------------------------------------------------

// -AJA- this compares the name in "natural order", which means that
//       "x15" comes after "x1" and "x2" (not between them).
//       more precisely: we treat strings of digits as a single char.
struct ComparePackEntryPredicate
{
    inline bool operator()(const PackEntry &AE, const PackEntry &BE) const
    {
        const std::string &A = AE.name_;
        const std::string &B = BE.name_;

        size_t x = 0;
        size_t y = 0;

        for (;;)
        {
            // reached the end of one/both strings?
            if (x >= A.size() || y >= B.size())
                return x >= A.size() && y < B.size();

            int xc = (int)(unsigned char)A[x++];
            int yc = (int)(unsigned char)B[y++];

            // handle a sequence of digits
            if (epi::IsDigitASCII(xc))
            {
                xc = 200 + (xc - '0');
                while (x < A.size() && epi::IsDigitASCII(A[x]) && xc < 214'000'000)
                    xc = (xc * 10) + (int)(A[x++] - '0');
            }
            if (epi::IsDigitASCII(yc))
            {
                yc = 200 + (yc - '0');
                while (y < B.size() && epi::IsDigitASCII(B[y]) && yc < 214'000'000)
                    yc = (yc * 10) + (int)(B[y++] - '0');
            }

            if (xc != yc)
                return xc < yc;
        }
    }
};

void PackDirectory::SortEntries()
{
    std::sort(entries_.begin(), entries_.end(), ComparePackEntryPredicate());
}

void PackFile::SortEntries()
{
    for (size_t i = 0; i < directories_.size(); i++)
        directories_[i].SortEntries();
}

//----------------------------------------------------------------------------
//  DIRECTORY READING
//----------------------------------------------------------------------------

static void ProcessSubDirectory(PackFile *pack, std::string &fullpath)
{
    std::vector<epi::DirectoryEntry> fsd;

    std::string dirname = epi::GetFilename(fullpath);

    if (!epi::WalkDirectory(fsd, fullpath))
    {
        LogWarning("Failed to read dir: %s\n", fullpath.c_str());
        return;
    }

    size_t d = pack->AddDirectory(dirname);

    for (size_t i = 0; i < fsd.size(); i++)
    {
        if (!fsd[i].is_dir)
        {
            if (epi::GetExtension(fsd[i].name).empty())
            {
                LogWarning("%s has no extension. Bare filenames are not supported for "
                           "mounted directories.\n",
                           fsd[i].name.c_str());
                continue;
            }
            std::string filename = epi::GetFilename(fsd[i].name);
            std::string packpath = epi::MakePathRelative(pack->parent_->name_, fsd[i].name);
            std::string stem     = epi::GetStem(filename);
            epi::StringUpperASCII(stem);
            pack->directories_[d].AddEntry(filename, fsd[i].name, packpath, 0);
            pack->search_files_.insert({stem, packpath});
        }
    }
}

static PackFile *ProcessFolder(DataFile *df)
{
    std::vector<epi::DirectoryEntry> fsd;

    if (!epi::ReadDirectory(fsd, df->name_, "*.*"))
    {
        FatalError("Failed to read dir: %s\n", df->name_.c_str());
    }

    PackFile *pack = new PackFile(df, true);

    // top-level files go in here
    pack->AddDirectory("");

    for (size_t i = 0; i < fsd.size(); i++)
    {
        if (fsd[i].is_dir)
        {
            ProcessSubDirectory(pack, fsd[i].name);
        }
        else
        {
            if (epi::GetExtension(fsd[i].name).empty())
            {
                LogWarning("%s has no extension. Bare filenames are not supported for "
                           "mounted directories.\n",
                           fsd[i].name.c_str());
                continue;
            }
            std::string filename = fsd[i].name;
            std::string packpath = epi::MakePathRelative(df->name_, fsd[i].name);
            std::string stem     = epi::GetStem(filename);
            epi::StringUpperASCII(stem);
            pack->directories_[0].AddEntry(filename, fsd[i].name, packpath, 0);
            pack->search_files_.insert({stem, packpath});
        }
    }

    return pack;
}

epi::File *PackFile::OpenFolderEntry(size_t dir, size_t index)
{
    std::string &filename = directories_[dir].entries_[index].full_path_;

    epi::File *F = epi::FileOpen(filename, epi::kFileAccessRead | epi::kFileAccessBinary);

    // this generally won't happen, file was found during a dir scan
    if (F == nullptr)
        FatalError("Failed to open file: %s\n", filename.c_str());

    return F;
}

epi::File *PackFile::OpenFolderEntryByName(const std::string &name)
{
    std::string fullpath = parent_->name_;
    fullpath.push_back('/');
    fullpath.append(name);

    // NOTE: it is okay here when file does not exist
    return epi::FileOpen(fullpath, epi::kFileAccessRead | epi::kFileAccessBinary);
}

//----------------------------------------------------------------------------
//  ZIP READING
//----------------------------------------------------------------------------

static PackFile *ProcessZip(DataFile *df)
{
    PackFile *pack = new PackFile(df, false);

    pack->archive_ = new mz_zip_archive;

    // this is necessary (but stupid)
    memset(pack->archive_, 0, sizeof(mz_zip_archive));

    if (!mz_zip_reader_init_file(pack->archive_, df->name_.c_str(), 0))
    {
        switch (mz_zip_get_last_error(pack->archive_))
        {
        case MZ_ZIP_FILE_OPEN_FAILED:
        case MZ_ZIP_FILE_READ_FAILED:
        case MZ_ZIP_FILE_SEEK_FAILED:
            FatalError("Failed to open EPK file: %s\n", df->name_.c_str());
            break;
        default:
            FatalError("Not a EPK file (or is corrupted): %s\n", df->name_.c_str());
        }
    }

    // create the top-level directory
    pack->AddDirectory("");

    mz_uint total = mz_zip_reader_get_num_files(pack->archive_);

    for (mz_uint idx = 0; idx < total; idx++)
    {
        // skip directories
        if (mz_zip_reader_is_file_a_directory(pack->archive_, idx))
            continue;

        // get the filename
        char filename[1024];

        mz_zip_reader_get_filename(pack->archive_, idx, filename, sizeof(filename));

        if (epi::GetExtension(filename).empty())
        {
            LogWarning("%s has no extension. Bare EPK filenames are not supported.\n", filename);
            continue;
        }

        std::string packpath = filename;

        // decode into DIR + FILE
        char *p = filename;
        while (*p != 0 && *p != '/' && *p != '\\')
            p++;

        if (p == filename)
            continue;

        size_t dir_idx  = 0;
        char  *basename = filename;

        if (*p != 0)
        {
            *p++ = 0;

            basename = p;
            if (basename[0] == 0)
                continue;

            dir_idx = pack->AddDirectory(filename);
        }
        std::string add_name = basename;
        std::string stem     = epi::GetStem(basename);
        epi::StringUpperASCII(stem);
        pack->directories_[dir_idx].AddEntry(epi::GetFilename(add_name), "", packpath, idx);
        pack->search_files_.insert({stem, packpath});
    }

    return pack;
}

class epk_file_c : public epi::File
{
  private:
    PackFile *pack;

    mz_uint zip_idx;

    mz_uint length = 0;
    mz_uint pos    = 0;

    mz_zip_reader_extract_iter_state *iter = nullptr;

  public:
    epk_file_c(PackFile *_pack, mz_uint _idx) : pack(_pack), zip_idx(_idx)
    {
        // determine length
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(pack->archive_, zip_idx, &stat))
            length = (mz_uint)stat.m_uncomp_size;

        iter = mz_zip_reader_extract_iter_new(pack->archive_, zip_idx, 0);
        EPI_ASSERT(iter);
    }

    ~epk_file_c()
    {
        if (iter != nullptr)
            mz_zip_reader_extract_iter_free(iter);
    }

    int GetLength()
    {
        return (int)length;
    }

    int GetPosition()
    {
        return (int)pos;
    }

    unsigned int Read(void *dest, unsigned int count)
    {
        if (pos >= length)
            return 0;

        // never read more than what GetLength() reports
        if (count > length - pos)
            count = length - pos;

        size_t got = mz_zip_reader_extract_iter_read(iter, dest, count);

        pos += got;

        return got;
    }

    unsigned int Write(const void *src, unsigned int count)
    {
        (void)src;
        (void)count;
        // not implemented
        FatalError("epk_file_c::Write called, but this is not implemented!\n");
        return 0;
    }

    bool Seek(int offset, int seekpoint)
    {
        mz_uint want_pos = pos;

        if (seekpoint == epi::File::kSeekpointStart)
            want_pos = 0;
        if (seekpoint == epi::File::kSeekpointEnd)
            want_pos = length;

        if (offset < 0)
        {
            offset = -offset;
            if ((mz_uint)offset >= want_pos)
                want_pos = 0;
            else
                want_pos -= (mz_uint)offset;
        }
        else
        {
            want_pos += (mz_uint)offset;
        }

        // cannot go beyond the end (except TO very end)
        if (want_pos > length)
            return false;

        if (want_pos == length)
        {
            pos = length;
            return true;
        }

        // to go backwards, we are forced to rewind to beginning
        if (want_pos < pos)
        {
            Rewind();
        }

        // trivial success when already there
        if (want_pos == pos)
            return true;

        SkipForward(want_pos - pos);
        return true;
    }

  private:
    void Rewind()
    {
        mz_zip_reader_extract_iter_free(iter);
        iter = mz_zip_reader_extract_iter_new(pack->archive_, zip_idx, 0);
        EPI_ASSERT(iter);

        pos = 0;
    }

    void SkipForward(unsigned int count)
    {
        uint8_t buffer[1024];

        while (count > 0)
        {
            size_t want = HMM_MIN((size_t)count, sizeof(buffer));
            size_t got  = mz_zip_reader_extract_iter_read(iter, buffer, want);

            // reached end of file?
            if (got == 0)
                break;

            pos += got;
            count -= got;
        }
    }
};

epi::File *PackFile::OpenZipEntry(size_t dir, size_t index)
{
    epk_file_c *F = new epk_file_c(this, directories_[dir].entries_[index].zip_index_);
    return F;
}

epi::File *PackFile::OpenZipEntryByName(const std::string &name)
{
    // this ignores case by default
    int idx = mz_zip_reader_locate_file(archive_, name.c_str(), nullptr, 0);
    if (idx < 0)
        return nullptr;

    epk_file_c *F = new epk_file_c(this, (mz_uint)idx);
    return F;
}

//----------------------------------------------------------------------------
//  GENERAL STUFF
//----------------------------------------------------------------------------

static void ProcessDDFInPack(PackFile *pack)
{
    DataFile *df = pack->parent_;

    std::string bare_filename = epi::GetFilename(df->name_);
    if (bare_filename.empty())
        bare_filename = df->name_;

    for (size_t dir = 0; dir < pack->directories_.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->directories_[dir].entries_.size(); entry++)
        {
            PackEntry &ent = pack->directories_[dir].entries_[entry];

            std::string source = ent.name_;
            source += " in ";
            source += bare_filename;

            // this handles RTS scripts too!
            DDFType type = DDFFilenameToType(ent.name_);

            if (type != kDDFTypeUnknown)
            {
                int            length   = -1;
                const uint8_t *raw_data = pack->LoadEntry(dir, entry, length);

                std::string data((const char *)raw_data);
                delete[] raw_data;

                DDFAddFile(type, data, source);
                continue;
            }

            if (ent.HasExtension(".deh") || ent.HasExtension(".bex"))
            {
                LogPrint("Converting DEH file%s: %s\n", pack->is_folder_ ? "" : " in EPK", ent.name_.c_str());

                int            length = -1;
                const uint8_t *data   = pack->LoadEntry(dir, entry, length);

                ConvertDehacked(data, length, source);
                delete[] data;

                continue;
            }
        }
    }
}

static void ProcessCoalAPIInPack(PackFile *pack)
{
    DataFile *df = pack->parent_;

    std::string bare_filename = epi::GetFilename(df->name_);
    if (bare_filename.empty())
        bare_filename = df->name_;

    std::string source = "coal_api.ec";
    source += " in ";
    source += bare_filename;

    for (size_t dir = 0; dir < pack->directories_.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->directories_[dir].entries_.size(); entry++)
        {
            PackEntry &ent = pack->directories_[dir].entries_[entry];
            if (epi::GetFilename(ent.name_) == "coal_api.ec")
            {
                int            length   = -1;
                const uint8_t *raw_data = pack->LoadEntry(dir, entry, length);
                std::string    data((const char *)raw_data);
                delete[] raw_data;
                CoalAddScript(0, data, source);
                return; // Should only be present once
            }
        }
    }
    FatalError("coal_api.ec not found in edge_defs; unable to initialize COAL!\n");
}

static void ProcessCoalHUDInPack(PackFile *pack)
{
    DataFile *df = pack->parent_;

    std::string bare_filename = epi::GetFilename(df->name_);
    if (bare_filename.empty())
        bare_filename = df->name_;

    std::string source = "coal_hud.ec";
    source += " in ";
    source += bare_filename;

    for (size_t dir = 0; dir < pack->directories_.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->directories_[dir].entries_.size(); entry++)
        {
            PackEntry  &ent    = pack->directories_[dir].entries_[entry];
            std::string ent_fn = epi::GetFilename(ent.name_);
            if (epi::StringCaseCompareASCII(ent_fn, "coal_hud.ec") == 0 ||
                epi::StringCaseCompareASCII(epi::GetStem(ent_fn), "COALHUDS") == 0)
            {
                if (epi::StringPrefixCaseCompareASCII(bare_filename, "edge_defs") != 0)
                {
                    SetCoalDetected(true);
                }

                int            length   = -1;
                const uint8_t *raw_data = pack->LoadEntry(dir, entry, length);
                std::string    data((const char *)raw_data);
                delete[] raw_data;
                CoalAddScript(0, data, source);
                return; // Should only be present once
            }
        }
    }
}

static void ProcessLuaAPIInPack(PackFile *pack)
{
    DataFile *df = pack->parent_;

    std::string bare_filename = epi::GetFilename(df->name_);
    if (bare_filename.empty())
        bare_filename = df->name_;

    std::string source = bare_filename + " => " + "edge_api.lua";

    for (size_t dir = 0; dir < pack->directories_.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->directories_[dir].entries_.size(); entry++)
        {
            PackEntry &ent = pack->directories_[dir].entries_[entry];
            if (epi::GetFilename(ent.name_) == "edge_api.lua")
            {
                int            length   = -1;
                const uint8_t *raw_data = pack->LoadEntry(dir, entry, length);
                std::string    data((const char *)raw_data);
                delete[] raw_data;
                LuaAddScript(data, source);
                return; // Should only be present once
            }
        }
    }
    FatalError("edge_api.lua not found in edge_defs; unable to initialize LUA!\n");
}

static void ProcessLuaHUDInPack(PackFile *pack)
{
    DataFile *df = pack->parent_;

    std::string bare_filename = epi::GetFilename(df->name_);
    if (bare_filename.empty())
        bare_filename = df->name_;

    std::string source = bare_filename + " => " + "edge_hud.lua";

    for (size_t dir = 0; dir < pack->directories_.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->directories_[dir].entries_.size(); entry++)
        {
            PackEntry &ent = pack->directories_[dir].entries_[entry];
            if (epi::StringCaseCompareASCII(epi::GetFilename(ent.name_), "edge_hud.lua") == 0)
            {
                if (epi::StringPrefixCaseCompareASCII(bare_filename, "edge_defs") != 0)
                {
                    LuaSetLuaHUDDetected(true);
                }

                int            length   = -1;
                const uint8_t *raw_data = pack->LoadEntry(dir, entry, length);
                std::string    data((const char *)raw_data);
                delete[] raw_data;
                LuaAddScript(data, source);
                return; // Should only be present once
            }
        }
    }
}

void PackProcessSubstitutions(PackFile *pack, int pack_index)
{
    int d = -1;
    for (const std::string &dir_name : known_image_directories)
    {
        d = pack->FindDirectory(dir_name);
        if (d < 0)
            continue;
        for (size_t i = 0; i < pack->directories_[d].entries_.size(); i++)
        {
            PackEntry &entry = pack->directories_[d].entries_[i];

            // split filename in stem + extension
            std::string stem = epi::GetStem(entry.name_);
            std::string ext  = epi::GetExtension(entry.name_);

            epi::StringLowerASCII(ext);

            if (ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" ||
                ext == ".lmp") // Note: .lmp is assumed to be Doom-format image
            {
                std::string texname;

                epi::TextureNameFromFilename(texname, stem);

                bool add_it = true;

                // Check DDFIMAGE definitions to see if this is replacing a lump
                // type def
                for (ImageDefinition *img : imagedefs)
                {
                    if (img->type_ == kImageDataLump && epi::StringCaseCompareASCII(img->info_, texname) == 0 &&
                        CheckDataFileIndexForName(texname.c_str()) < pack_index)
                    {
                        img->type_ = kImageDataPackage;
                        img->info_ = entry.pack_path_;
                        add_it     = false;
                    }
                }

                // If no DDF just see if a bare lump with the same name comes
                // later
                if (CheckDataFileIndexForName(texname.c_str()) > pack_index)
                    add_it = false;

                if (!add_it)
                    continue;

                LogDebug("- Adding image file in EPK: %s\n", entry.pack_path_.c_str());

                if (dir_name == "textures")
                    AddPackImageSmart(texname.c_str(), kImageSourceTxHi, entry.pack_path_.c_str(), real_textures);
                else if (dir_name == "graphics")
                    AddPackImageSmart(texname.c_str(), kImageSourceGraphic, entry.pack_path_.c_str(), real_graphics);
                else if (dir_name == "flats")
                    AddPackImageSmart(texname.c_str(), kImageSourceFlat, entry.pack_path_.c_str(), real_flats);
                else if (dir_name == "skins") // Not sure about this still
                    AddPackImageSmart(texname.c_str(), kImageSourceSprite, entry.pack_path_.c_str(), real_sprites);
            }
            else
            {
                LogWarning("Unknown image type in EPK: %s\n", entry.name_.c_str());
            }
        }
    }
    // Only sub out sounds and music if they would replace an existing DDF entry
    // This MAY expand to create automatic simple DDFSFX entries if they aren't
    // defined anywhere else
    d = pack->FindDirectory("sounds");
    if (d > 0)
    {
        for (size_t i = 0; i < pack->directories_[d].entries_.size(); i++)
        {
            PackEntry &entry = pack->directories_[d].entries_[i];
            for (SoundEffectDefinition *sfx : sfxdefs)
            {
                // Assume that same stem name is meant to replace an identically
                // named lump entry
                if (!sfx->lump_name_.empty())
                {
                    if (epi::StringCaseCompareASCII(epi::GetStem(entry.name_), sfx->lump_name_) == 0 &&
                        CheckDataFileIndexForName(sfx->lump_name_.c_str()) < pack_index)
                    {
                        sfx->pack_name_ = entry.pack_path_;
                        sfx->lump_name_.clear();
                    }
                }
            }
        }
    }
    d = pack->FindDirectory("music");
    if (d > 0)
    {
        for (size_t i = 0; i < pack->directories_[d].entries_.size(); i++)
        {
            PackEntry &entry = pack->directories_[d].entries_[i];
            for (PlaylistEntry *song : playlist)
            {
                if (epi::GetExtension(song->info_).empty())
                {
                    if (song->infotype_ == kDDFMusicDataLump &&
                        epi::StringCaseCompareASCII(epi::GetStem(entry.name_), song->info_) == 0 &&
                        CheckDataFileIndexForName(song->info_.c_str()) < pack_index)
                    {
                        song->info_     = entry.pack_path_;
                        song->infotype_ = kDDFMusicDataPackage;
                    }
                }
            }
        }
    }
    d = pack->FindDirectory("colormaps");
    if (d > 0)
    {
        for (size_t i = 0; i < pack->directories_[d].entries_.size(); i++)
        {
            PackEntry &entry = pack->directories_[d].entries_[i];

            std::string stem = epi::GetStem(entry.name_);

            bool add_it = true;

            for (Colormap *colm : colormaps)
            {
                if (!colm->lump_name_.empty() &&
                    epi::StringCaseCompareASCII(colm->lump_name_, epi::GetStem(entry.name_)) == 0 &&
                    CheckDataFileIndexForName(colm->lump_name_.c_str()) < pack_index)
                {
                    colm->lump_name_.clear();
                    colm->pack_name_ = entry.pack_path_;
                    add_it           = false;
                }
            }

            if (add_it)
                DDFAddRawColourmap(stem.c_str(), pack->EntryLength(d, i), entry.pack_path_.c_str());
        }
    }
}

void PackProcessHiresSubstitutions(PackFile *pack, int pack_index)
{
    int d = pack->FindDirectory("hires");

    if (d < 0)
        return;

    for (size_t i = 0; i < pack->directories_[d].entries_.size(); i++)
    {
        PackEntry &entry = pack->directories_[d].entries_[i];

        // split filename in stem + extension
        std::string stem = epi::GetStem(entry.name_);
        std::string ext  = epi::GetExtension(entry.name_);

        epi::StringLowerASCII(ext);

        if (ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".lmp") // Note: .lmp is assumed to be Doom-format image
        {
            std::string texname;

            epi::TextureNameFromFilename(texname, stem);

            // See if a bare lump with the same name comes later
            if (CheckDataFileIndexForName(texname.c_str()) > pack_index)
                continue;

            LogDebug("- Adding Hires substitute from EPK: %s\n", entry.pack_path_.c_str());

            const Image *rim = ImageContainerLookup(real_textures, texname.c_str(), -2);
            if (rim && rim->source_type_ != kImageSourceUser)
            {
                AddPackImageSmart(texname.c_str(), kImageSourceTxHi, entry.pack_path_.c_str(), real_textures, rim);
                continue;
            }

            rim = ImageContainerLookup(real_flats, texname.c_str(), -2);
            if (rim && rim->source_type_ != kImageSourceUser)
            {
                AddPackImageSmart(texname.c_str(), kImageSourceTxHi, entry.pack_path_.c_str(), real_flats, rim);
                continue;
            }

            rim = ImageContainerLookup(real_sprites, texname.c_str(), -2);
            if (rim && rim->source_type_ != kImageSourceUser)
            {
                AddPackImageSmart(texname.c_str(), kImageSourceTxHi, entry.pack_path_.c_str(), real_sprites, rim);
                continue;
            }

            // we do it this way to force the original graphic to be loaded
            rim = ImageLookup(texname.c_str(), kImageNamespaceGraphic, kImageLookupExact | kImageLookupNull);

            if (rim && rim->source_type_ != kImageSourceUser)
            {
                AddPackImageSmart(texname.c_str(), kImageSourceTxHi, entry.pack_path_.c_str(), real_graphics, rim);
                continue;
            }

            LogDebug("HIRES replacement '%s' has no counterpart.\n", texname.c_str());

            AddPackImageSmart(texname.c_str(), kImageSourceTxHi, entry.pack_path_.c_str(), real_textures);
        }
        else
        {
            LogWarning("Unknown image type in EPK: %s\n", entry.name_.c_str());
        }
    }
}

bool PackFindFile(PackFile *pack, const std::string &name)
{
    // when file does not exist, this returns false.

    EPI_ASSERT(!name.empty());

    // disallow absolute (real filesystem) paths,
    // although we have to let a leading '/' slide to be caught later
    if (epi::IsPathAbsolute(name) && name[0] != '/')
        return false;

    // do not accept filenames without extensions
    if (epi::GetExtension(name).empty())
        return false;

    // Make a copy in case we need to pop a leading slash
    std::string find_name = name;

    bool root_only = false;

    // Check for root-only search
    if (name[0] == '/')
    {
        find_name = name.substr(1);
        if (find_name == epi::GetFilename(name))
            root_only = true;
    }

    std::string find_stem = epi::GetStem(name);
    epi::StringUpperASCII(find_stem);

    // quick file stem check to see if it's present at all
    if (!PackFindStem(pack, find_stem))
        return false;

    // Specific path given; attempt to find as-is, otherwise return false
    if (find_name != epi::GetFilename(find_name))
    {
        std::string find_comp = find_name;
        auto        results   = pack->search_files_.equal_range(find_stem);
        for (auto file = results.first; file != results.second; ++file)
        {
            if (find_comp == file->second)
                return true;
        }
        return false;
    }
    // Search only the root dir for this filename, return false if not present
    else if (root_only)
    {
        for (auto file : pack->directories_[0].entries_)
        {
            if (epi::StringCaseCompareASCII(file.name_, find_name) == 0)
                return true;
        }
        return false;
    }
    // Only filename given; return first full match from search list, if present
    // Search list is unordered, but realistically identical filename+extensions
    // wouldn't be in the same pack
    else
    {
        auto results = pack->search_files_.equal_range(find_stem);
        for (auto file = results.first; file != results.second; ++file)
        {
            if (epi::StringCaseCompareASCII(find_name, epi::GetFilename(file->second)) == 0)
                return true;
        }
        return false;
    }

    // Fallback
    return false;
}

epi::File *PackOpenFile(PackFile *pack, const std::string &name)
{
    // when file does not exist, this returns nullptr.

    EPI_ASSERT(!name.empty());

    // disallow absolute (real filesystem) paths,
    // although we have to let a leading '/' slide to be caught later
    if (epi::IsPathAbsolute(name) && name[0] != '/')
        return nullptr;

    // do not accept filenames without extensions
    if (epi::GetExtension(name).empty())
        return nullptr;

    // Make a copy in case we need to pop a leading slash
    std::string open_name = name;

    bool root_only = false;

    // Check for root-only search
    if (name[0] == '/')
    {
        open_name = name.substr(1);
        if (epi::GetDirectory(open_name).empty())
            root_only = true;
    }

    std::string open_stem = epi::GetStem(open_name);
    epi::StringUpperASCII(open_stem);

    // quick file stem check to see if it's present at all
    if (!PackFindStem(pack, open_stem))
        return nullptr;

    // Specific path given; attempt to open as-is, otherwise return nullptr
    if (open_name != epi::GetFilename(open_name))
    {
        return pack->OpenEntryByName(open_name);
    }
    // Search only the root dir for this filename, return nullptr if not present
    else if (root_only)
    {
        for (auto file : pack->directories_[0].entries_)
        {
            if (epi::StringCaseCompareASCII(file.pack_path_, open_name) == 0)
                return pack->OpenEntryByName(open_name);
        }
        return nullptr;
    }
    // Only filename given; return first full match from search list, if present
    // Search list is unordered, but realistically identical filename+extensions
    // wouldn't be in the same pack
    else
    {
        auto results = pack->search_files_.equal_range(open_stem);
        for (auto file = results.first; file != results.second; ++file)
        {
            if (epi::StringCaseCompareASCII(open_name, epi::GetFilename(file->second)) == 0)
                return pack->OpenEntryByName(file->second);
        }
        return nullptr;
    }

    // Fallback
    return nullptr;
}

// Like the above, but is in the form of a stem + acceptable extensions
epi::File *PackOpenMatch(PackFile *pack, const std::string &name, const std::vector<std::string> &extensions)
{
    // when file does not exist, this returns nullptr.

    // Nothing to match (may change this to allow a wildcard in the future)
    if (extensions.empty())
        return nullptr;

    std::string open_stem = name;
    epi::StringUpperASCII(open_stem);

    // quick file stem check to see if it's present at all
    if (!PackFindStem(pack, open_stem))
        return nullptr;

    std::string stem_match = open_stem;

    auto results = pack->search_files_.equal_range(open_stem);
    for (auto file = results.first; file != results.second; ++file)
    {
        for (auto ext : extensions)
        {
            epi::ReplaceExtension(stem_match, ext);
            if (epi::StringCaseCompareASCII(stem_match, epi::GetFilename(file->second)) == 0)
                return pack->OpenEntryByName(file->second);
        }
    }

    return nullptr;
}

std::vector<std::string> PackGetSpriteList(PackFile *pack)
{
    std::vector<std::string> found_sprites;

    int d = pack->FindDirectory("sprites");
    if (d > 0)
    {
        for (size_t i = 0; i < pack->directories_[d].entries_.size(); i++)
        {
            PackEntry &entry = pack->directories_[d].entries_[i];

            // split filename in stem + extension
            std::string stem = epi::GetStem(entry.name_);
            std::string ext  = epi::GetExtension(entry.name_);

            epi::StringLowerASCII(ext);

            if (ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" ||
                ext == ".lmp") // Note: .lmp is assumed to be Doom-format image
            {
                std::string texname;
                epi::TextureNameFromFilename(texname, stem);

                bool addme = true;
                // Don't add things already defined in DDFIMAGE
                for (auto img : imagedefs)
                {
                    if (epi::StringCaseCompareASCII(img->name_, texname) == 0)
                    {
                        addme = false;
                        break;
                    }
                }
                if (addme)
                    found_sprites.push_back(entry.pack_path_);
            }
        }
    }

    return found_sprites;
}

static void ProcessWADsInPack(PackFile *pack)
{
    for (size_t d = 0; d < pack->directories_.size(); d++)
    {
        for (size_t i = 0; i < pack->directories_[d].entries_.size(); i++)
        {
            PackEntry &entry = pack->directories_[d].entries_[i];

            if (!entry.HasExtension(".wad"))
                continue;

            epi::File *pack_wad = PackOpenFile(pack, entry.pack_path_);

            if (pack_wad)
            {
                uint8_t      *raw_pack_wad = pack_wad->LoadIntoMemory();
                epi::MemFile *pack_wad_mem = new epi::MemFile(raw_pack_wad, pack_wad->GetLength(), true);
                delete[] raw_pack_wad; // copied on pack_wad_mem creation
                DataFile *pack_wad_df = new DataFile(
                    entry.name_, (pack->parent_->kind_ == kFileKindIFolder || pack->parent_->kind_ == kFileKindIPK)
                                     ? kFileKindIPackWAD
                                     : kFileKindPackWAD);
                pack_wad_df->name_ = entry.name_;
                pack_wad_df->file_ = pack_wad_mem;
                ProcessFile(pack_wad_df);
            }

            delete pack_wad;
        }
    }
}

void PackPopulateOnly(DataFile *df)
{
    if (df->kind_ == kFileKindFolder || df->kind_ == kFileKindEFolder || df->kind_ == kFileKindIFolder)
        df->pack_ = ProcessFolder(df);
    else
        df->pack_ = ProcessZip(df);

    df->pack_->SortEntries();
}

int PackCheckForIwads(DataFile *df)
{
    PackFile *pack = df->pack_;
    for (size_t d = 0; d < pack->directories_.size(); d++)
    {
        for (size_t i = 0; i < pack->directories_[d].entries_.size(); i++)
        {
            PackEntry &entry = pack->directories_[d].entries_[i];

            if (!entry.HasExtension(".wad"))
                continue;

            epi::File *pack_wad = PackOpenFile(pack, entry.pack_path_);

            if (pack_wad)
            {
                int pack_iwad_check = CheckForUniqueGameLumps(pack_wad);
                if (pack_iwad_check >= 0)
                {
                    delete pack_wad;
                    return pack_iwad_check;
                }
            }
            delete pack_wad;
        }
    }
    return -1;
}

void PackProcessAll(DataFile *df, size_t file_index)
{
    if (df->kind_ == kFileKindFolder || df->kind_ == kFileKindEFolder || df->kind_ == kFileKindIFolder)
        df->pack_ = ProcessFolder(df);
    else
        df->pack_ = ProcessZip(df);

    df->pack_->SortEntries();

    // parse the WADFIXES file from edge_defs folder or `edge_defs.epk`
    // immediately
    if ((df->kind_ == kFileKindEFolder || df->kind_ == kFileKindEEPK) && file_index == 0)
    {
        LogPrint("Loading WADFIXES\n");
        epi::File *wadfixes = PackOpenFile(df->pack_, "wadfixes.ddf");
        if (wadfixes)
            DDFReadFixes(wadfixes->ReadText());
        delete wadfixes;
    }

    // Only load some things here; the rest are deferred until
    // after all files loaded so that pack substitutions can work properly
    ProcessDDFInPack(df->pack_);

    // COAL

    // parse COALAPI only from edge_defs folder or `edge_defs.epk`
    if ((df->kind_ == kFileKindEFolder || df->kind_ == kFileKindEEPK) && file_index == 0)
        ProcessCoalAPIInPack(df->pack_);
    ProcessCoalHUDInPack(df->pack_);

    // LUA

    // parse lua api  only from edge_defs folder or `edge_defs.epk`
    if ((df->kind_ == kFileKindEFolder || df->kind_ == kFileKindEEPK) && file_index == 0)
        ProcessLuaAPIInPack(df->pack_);
    ProcessLuaHUDInPack(df->pack_);

    ProcessWADsInPack(df->pack_);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
