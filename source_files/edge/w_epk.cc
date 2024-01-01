//----------------------------------------------------------------------------
//  EDGE EPK Support Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2023  The EDGE Team.
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

#include "i_defs.h"
#include "l_deh.h"
#include "r_image.h"
#include "w_files.h"
#include "w_wad.h"
#include "vm_coal.h"
#include "script/compat/lua_compat.h"

// EPI
#include "epi.h"
#include "file.h"
#include "file_memory.h"
#include "filesystem.h"
#include "path.h"
#include "sound_types.h"
#include "str_util.h"

// DDF
#include "main.h"
#include "colormap.h"
#include "wadfixes.h"

#include <unordered_map>
#include <vector>
#include <algorithm>

// ZIP support
#include "miniz.h"

static std::string image_dirs[5] = {"flats", "graphics", "skins", "textures", "sprites"};

class pack_entry_c
{
  public:
    // this name is relative to parent (if any), i.e. no slashes
    std::string name;

    // only for Folder: the full pathname to file (for FS_Open).
    std::string fullpath;

    // for both types: path relative to pack's "root" directory
    std::string packpath;

    // only for ZIP: the index into the archive.
    mz_uint zip_idx;

    pack_entry_c(const std::string &_name, const std::string &_path, const std::string &_ppath, mz_uint _idx)
        : name(_name), fullpath(_path), packpath(_ppath), zip_idx(_idx)
    {
    }

    ~pack_entry_c()
    {
    }

    bool operator==(const std::string &other) const
    {
        return epi::case_cmp(name, other.c_str()) == 0;
    }

    bool HasExtension(const char *match) const
    {
        std::string ext = epi::PATH_GetExtension(name).string();
        return epi::case_cmp(ext, match) == 0;
    }
};

class pack_dir_c
{
  public:
    std::string               name;
    std::vector<pack_entry_c> entries;

    pack_dir_c(const std::string &_name) : name(_name), entries()
    {
    }

    ~pack_dir_c()
    {
    }

    void SortEntries();

    size_t AddEntry(const std::string &_name, const std::string &_path, const std::string &_ppath, mz_uint _idx)
    {
        // check if already there
        for (size_t i = 0; i < entries.size(); i++)
            if (entries[i] == _name)
                return i;

        entries.push_back(pack_entry_c(_name, _path, _ppath, _idx));
        return entries.size() - 1;
    }

    int Find(const std::string &name_in) const
    {
        for (int i = 0; i < (int)entries.size(); i++)
            if (entries[i] == name_in)
                return i;

        return -1; // not found
    }

    bool operator==(const std::string &other) const
    {
        return epi::case_cmp(name, other) == 0;
    }
};

class pack_file_c
{
  public:
    data_file_c *parent;

    bool is_folder;

    // first entry here is always the top-level (with no name).
    // everything else is from a second-level directory.
    // things in deeper directories are not stored.
    std::vector<pack_dir_c> dirs;

    // for faster file lookups
    // stored as filename stems as keys; packpath as values
    std::unordered_multimap<std::string, std::string> search_files;

    mz_zip_archive *arch;

  public:
    pack_file_c(data_file_c *_par, bool _folder, bool _zip) : parent(_par), is_folder(_folder), dirs(), arch(NULL)
    {
    }

    ~pack_file_c()
    {
        if (arch != NULL)
            delete arch;
    }

    size_t AddDir(const std::string &name)
    {
        // check if already there
        for (size_t i = 0; i < dirs.size(); i++)
            if (dirs[i] == name)
                return i;

        dirs.push_back(pack_dir_c(name));
        return dirs.size() - 1;
    }

    int FindDir(const std::string &name) const
    {
        for (int i = 0; i < (int)dirs.size(); i++)
            if (dirs[i] == name)
                return i;

        return -1; // not found
    }

    void ProcessSubDir(const std::string &dirname);
    void SortEntries();

    epi::file_c *OpenEntry(size_t dir, size_t index)
    {
        if (is_folder)
            return OpenEntry_Folder(dir, index);
        else
            return OpenEntry_Zip(dir, index);
    }

    epi::file_c *OpenFileByName(const std::string &name)
    {
        if (is_folder)
            return OpenFile_Folder(name);
        else
            return OpenFile_Zip(name);
    }

    int EntryLength(size_t dir, size_t index)
    {
        epi::file_c *f = OpenEntry(dir, index);
        if (f == NULL)
            return 0;

        int length = f->GetLength();
        delete f; // close it

        return length;
    }

    byte *LoadEntry(size_t dir, size_t index, int &length)
    {
        epi::file_c *f = OpenEntry(dir, index);
        if (f == NULL)
        {
            length = 0;
            return new byte[1];
        }

        byte *data = f->LoadIntoMemory();
        length     = f->GetLength();

        // close file
        delete f;

        if (data == NULL)
        {
            length = 0;
            return new byte[1];
        }

        return data;
    }

  private:
    epi::file_c *OpenEntry_Folder(size_t dir, size_t index);
    epi::file_c *OpenEntry_Zip(size_t dir, size_t index);

    epi::file_c *OpenFile_Folder(const std::string &name);
    epi::file_c *OpenFile_Zip(const std::string &name);
};

int Pack_FindStem(pack_file_c *pack, const std::string &name)
{
    return pack->search_files.count(name);
}

//----------------------------------------------------------------------------

// -AJA- this compares the name in "natural order", which means that
//       "x15" comes after "x1" and "x2" (not between them).
//       more precisely: we treat strings of digits as a single char.
struct Compare_packentry_pred
{
    inline bool operator()(const pack_entry_c &AE, const pack_entry_c &BE) const
    {
        const std::string &A = AE.name;
        const std::string &B = BE.name;

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
            if (isdigit(xc))
            {
                xc = 200 + (xc - '0');
                while (x < A.size() && isdigit(A[x]) && xc < 214'000'000)
                    xc = (xc * 10) + (int)(A[x++] - '0');
            }
            if (isdigit(yc))
            {
                yc = 200 + (yc - '0');
                while (y < B.size() && isdigit(B[y]) && yc < 214'000'000)
                    yc = (yc * 10) + (int)(B[y++] - '0');
            }

            if (xc != yc)
                return xc < yc;
        }
    }
};

void pack_dir_c::SortEntries()
{
    std::sort(entries.begin(), entries.end(), Compare_packentry_pred());
}

void pack_file_c::SortEntries()
{
    for (size_t i = 0; i < dirs.size(); i++)
        dirs[i].SortEntries();
}

//----------------------------------------------------------------------------
//  DIRECTORY READING
//----------------------------------------------------------------------------

void ProcessSubDir(pack_file_c *pack, const std::string &fullpath)
{
    std::vector<epi::dir_entry_c> fsd;

    std::string dirname = epi::PATH_GetFilename(fullpath).string();

    if (!epi::FS_ReadDirRecursive(fsd, fullpath, "*.*"))
    {
        I_Warning("Failed to read dir: %s\n", fullpath.c_str());
        return;
    }

    size_t d = pack->AddDir(dirname);

    for (size_t i = 0; i < fsd.size(); i++)
    {
        if (!fsd[i].is_dir)
        {
            if (epi::PATH_GetExtension(fsd[i].name).empty())
            {
                I_Warning("%s has no extension. Bare filenames are not supported for mounted directories.\n",
                          fsd[i].name.u8string().c_str());
                continue;
            }
            std::string filename = epi::PATH_GetFilename(fsd[i].name).string();
            epi::str_upper(filename);
            std::string packpath = fsd[i].name.lexically_relative(pack->parent->name).string();
            pack->dirs[d].AddEntry(filename, fsd[i].name.string(), packpath, 0);
            pack->search_files.insert({epi::PATH_GetBasename(filename).string(), packpath});
        }
    }
}

static pack_file_c *ProcessFolder(data_file_c *df)
{
    std::vector<epi::dir_entry_c> fsd;

    if (!epi::FS_ReadDir(fsd, df->name, "*.*"))
    {
        I_Error("Failed to read dir: %s\n", df->name.u8string().c_str());
    }

    pack_file_c *pack = new pack_file_c(df, true, false);

    // top-level files go in here
    pack->AddDir("");

    for (size_t i = 0; i < fsd.size(); i++)
    {
        if (fsd[i].is_dir)
        {
            ProcessSubDir(pack, fsd[i].name.string());
        }
        else
        {
            if (epi::PATH_GetExtension(fsd[i].name).empty())
            {
                I_Warning("%s has no extension. Bare filenames are not supported for mounted directories.\n",
                          fsd[i].name.u8string().c_str());
                continue;
            }
            std::string filename = epi::PATH_GetFilename(fsd[i].name).string();
            epi::str_upper(filename);
            std::string packpath = fsd[i].name.lexically_relative(df->name).string();
            pack->dirs[0].AddEntry(filename, fsd[i].name.string(), packpath, 0);
            pack->search_files.insert({epi::PATH_GetBasename(filename).string(), packpath});
        }
    }

    return pack;
}

epi::file_c *pack_file_c::OpenEntry_Folder(size_t dir, size_t index)
{
    const std::string &filename = dirs[dir].entries[index].fullpath;

    epi::file_c *F = epi::FS_Open(filename.c_str(), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);

    // this generally won't happen, file was found during a dir scan
    if (F == NULL)
        I_Error("Failed to open file: %s\n", filename.c_str());

    return F;
}

epi::file_c *pack_file_c::OpenFile_Folder(const std::string &name)
{
    std::filesystem::path fullpath = epi::PATH_Join(parent->name, name);

    // NOTE: it is okay here when file does not exist
    return epi::FS_Open(fullpath, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
}

//----------------------------------------------------------------------------
//  ZIP READING
//----------------------------------------------------------------------------

static pack_file_c *ProcessZip(data_file_c *df)
{
    pack_file_c *pack = new pack_file_c(df, false, true);

    pack->arch = new mz_zip_archive;

    // this is necessary (but stupid)
    memset(pack->arch, 0, sizeof(mz_zip_archive));

    if (!mz_zip_reader_init_file(pack->arch, df->name.string().c_str(), 0))
    {
        switch (mz_zip_get_last_error(pack->arch))
        {
        case MZ_ZIP_FILE_OPEN_FAILED:
        case MZ_ZIP_FILE_READ_FAILED:
        case MZ_ZIP_FILE_SEEK_FAILED:
            I_Error("Failed to open EPK file: %s\n", df->name.u8string().c_str());
            break;
        default:
            I_Error("Not a EPK file (or is corrupted): %s\n", df->name.u8string().c_str());
        }
    }

    // create the top-level directory
    pack->AddDir("");

    mz_uint total = mz_zip_reader_get_num_files(pack->arch);

    for (mz_uint idx = 0; idx < total; idx++)
    {
        // skip directories
        if (mz_zip_reader_is_file_a_directory(pack->arch, idx))
            continue;

        // get the filename
        char filename[1024];

        mz_zip_reader_get_filename(pack->arch, idx, filename, sizeof(filename));

        if (epi::PATH_GetExtension(filename).empty())
        {
            I_Warning("%s has no extension. Bare EPK filenames are not supported.\n", filename);
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

            dir_idx = pack->AddDir(filename);
        }
        std::string add_name = basename;
        epi::str_upper(add_name);
        pack->dirs[dir_idx].AddEntry(epi::PATH_GetFilename(add_name).string(), "", packpath, idx);
        pack->search_files.insert({epi::PATH_GetBasename(add_name).string(), packpath});
    }

    return pack;
}

class epk_file_c : public epi::file_c
{
  private:
    pack_file_c *pack;

    mz_uint zip_idx;

    mz_uint length = 0;
    mz_uint pos    = 0;

    mz_zip_reader_extract_iter_state *iter = NULL;

  public:
    epk_file_c(pack_file_c *_pack, mz_uint _idx) : pack(_pack), zip_idx(_idx)
    {
        // determine length
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(pack->arch, zip_idx, &stat))
            length = (mz_uint)stat.m_uncomp_size;

        iter = mz_zip_reader_extract_iter_new(pack->arch, zip_idx, 0);
        SYS_ASSERT(iter);
    }

    ~epk_file_c()
    {
        if (iter != NULL)
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
        // not implemented
        return count;
    }

    bool Seek(int offset, int seekpoint)
    {
        mz_uint want_pos = pos;

        if (seekpoint == epi::file_c::SEEKPOINT_START)
            want_pos = 0;
        if (seekpoint == epi::file_c::SEEKPOINT_END)
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
        iter = mz_zip_reader_extract_iter_new(pack->arch, zip_idx, 0);
        SYS_ASSERT(iter);

        pos = 0;
    }

    void SkipForward(unsigned int count)
    {
        byte buffer[1024];

        while (count > 0)
        {
            size_t want = std::min((size_t)count, sizeof(buffer));
            size_t got  = mz_zip_reader_extract_iter_read(iter, buffer, want);

            // reached end of file?
            if (got == 0)
                break;

            pos += got;
            count -= got;
        }
    }
};

epi::file_c *pack_file_c::OpenEntry_Zip(size_t dir, size_t index)
{
    epk_file_c *F = new epk_file_c(this, dirs[dir].entries[index].zip_idx);
    return F;
}

epi::file_c *pack_file_c::OpenFile_Zip(const std::string &name)
{
    // this ignores case by default
    int idx = mz_zip_reader_locate_file(arch, name.c_str(), NULL, 0);
    if (idx < 0)
        return NULL;

    epk_file_c *F = new epk_file_c(this, (mz_uint)idx);
    return F;
}

//----------------------------------------------------------------------------
//  GENERAL STUFF
//----------------------------------------------------------------------------

static void ProcessDDFInPack(pack_file_c *pack)
{
    data_file_c *df = pack->parent;

    std::string bare_filename = epi::PATH_GetFilename(df->name).string();
    if (bare_filename.empty())
        bare_filename = df->name.string();

    for (size_t dir = 0; dir < pack->dirs.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->dirs[dir].entries.size(); entry++)
        {
            pack_entry_c &ent = pack->dirs[dir].entries[entry];

            std::string source = ent.name;
            source += " in ";
            source += bare_filename;

            // this handles RTS scripts too!
            ddf_type_e type = DDF_FilenameToType(ent.name);

            if (type != DDF_UNKNOWN)
            {
                int         length   = -1;
                const byte *raw_data = pack->LoadEntry(dir, entry, length);

                std::string data((const char *)raw_data);
                delete[] raw_data;

                DDF_AddFile(type, data, source);
                continue;
            }

            if (ent.HasExtension(".deh") || ent.HasExtension(".bex"))
            {
                I_Printf("Converting DEH file%s: %s\n", pack->is_folder ? "" : " in EPK", ent.name.c_str());

                int         length = -1;
                const byte *data   = pack->LoadEntry(dir, entry, length);

                DEH_Convert(data, length, source);
                delete[] data;

                continue;
            }
        }
    }
}

static void ProcessCoalAPIInPack(pack_file_c *pack)
{
    data_file_c *df = pack->parent;

    std::string bare_filename = epi::PATH_GetFilename(df->name).string();
    if (bare_filename.empty())
        bare_filename = df->name.string();

    std::string source = "coal_api.ec";
    source += " in ";
    source += bare_filename;

    for (size_t dir = 0; dir < pack->dirs.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->dirs[dir].entries.size(); entry++)
        {
            pack_entry_c &ent = pack->dirs[dir].entries[entry];
            if (epi::PATH_GetFilename(ent.name) == "COAL_API.EC" || epi::PATH_GetBasename(ent.name) == "COALAPI")
            {
                int         length   = -1;
                const byte *raw_data = pack->LoadEntry(dir, entry, length);
                std::string data((const char *)raw_data);
                delete[] raw_data;
                VM_AddScript(0, data, source);
                return; // Should only be present once
            }
        }
    }
    I_Error("coal_api.ec not found in edge_defs; unable to initialize COAL!\n");
}

static void ProcessCoalHUDInPack(pack_file_c *pack)
{
    data_file_c *df = pack->parent;

    std::string bare_filename = epi::PATH_GetFilename(df->name).string();
    if (bare_filename.empty())
        bare_filename = df->name.string();

    std::string source = "coal_hud.ec";
    source += " in ";
    source += bare_filename;

    for (size_t dir = 0; dir < pack->dirs.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->dirs[dir].entries.size(); entry++)
        {
            pack_entry_c &ent = pack->dirs[dir].entries[entry];
            if (epi::PATH_GetFilename(ent.name) == "COAL_HUD.EC" || epi::PATH_GetBasename(ent.name) == "COALHUDS")
            {
                if (epi::prefix_case_cmp(bare_filename, "edge_defs") != 0)
                {
                    VM_SetCoalDetected(true);
                }

                int         length   = -1;
                const byte *raw_data = pack->LoadEntry(dir, entry, length);
                std::string data((const char *)raw_data);
                delete[] raw_data;
                VM_AddScript(0, data, source);
                return; // Should only be present once
            }
        }
    }
}

static void ProcessLuaAPIInPack(pack_file_c *pack)
{
    data_file_c *df = pack->parent;

    std::string bare_filename = epi::PATH_GetFilename(df->name).string();
    if (bare_filename.empty())
        bare_filename = df->name.string();

    std::string source = bare_filename + " => " + "edge_api.lua";

    for (size_t dir = 0; dir < pack->dirs.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->dirs[dir].entries.size(); entry++)
        {
            pack_entry_c &ent = pack->dirs[dir].entries[entry];
            if (epi::PATH_GetFilename(ent.name) == "EDGE_API.LUA")
            {
                int         length   = -1;
                const byte *raw_data = pack->LoadEntry(dir, entry, length);
                std::string data((const char *)raw_data);
                delete[] raw_data;
                LUA_AddScript(data, source);
                return; // Should only be present once
            }
        }
    }
    I_Error("edge_api.lua not found in edge_defs; unable to initialize LUA!\n");
}

static void ProcessLuaHUDInPack(pack_file_c *pack)
{
    data_file_c *df = pack->parent;

    std::string bare_filename = epi::PATH_GetFilename(df->name).string();
    if (bare_filename.empty())
        bare_filename = df->name.string();

    std::string source = bare_filename + " => " + "edge_hud.lua";

    for (size_t dir = 0; dir < pack->dirs.size(); dir++)
    {
        for (size_t entry = 0; entry < pack->dirs[dir].entries.size(); entry++)
        {
            pack_entry_c &ent = pack->dirs[dir].entries[entry];
            if (epi::PATH_GetFilename(ent.name) == "EDGE_HUD.LUA")
            {
                if (epi::prefix_case_cmp(bare_filename, "edge_defs") != 0)
                {
                    LUA_SetLuaHudDetected(true);
                }

                int         length   = -1;
                const byte *raw_data = pack->LoadEntry(dir, entry, length);
                std::string data((const char *)raw_data);
                delete[] raw_data;
                LUA_AddScript(data, source);
                return; // Should only be present once
            }
        }
    }
}

void Pack_ProcessSubstitutions(pack_file_c *pack, int pack_index)
{
    int d = -1;
    for (auto dir_name : image_dirs)
    {
        d = pack->FindDir(dir_name);
        if (d < 0)
            continue;
        for (size_t i = 0; i < pack->dirs[d].entries.size(); i++)
        {
            pack_entry_c &entry = pack->dirs[d].entries[i];

            // split filename in stem + extension
            std::string stem = epi::PATH_GetBasename(entry.name).string();
            std::string ext  = epi::PATH_GetExtension(entry.name).string();

            epi::str_lower(ext);

            if (ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" ||
                ext == ".lmp") // Note: .lmp is assumed to be Doom-format image
            {
                std::string texname;

                epi::STR_TextureNameFromFilename(texname, stem);

                bool add_it = true;

                // Check DDFIMAGE definitions to see if this is replacing a lump type def
                for (int j = 0; j < imagedefs.GetSize(); j++)
                {
                    imagedef_c *img = imagedefs[j];
                    if (img->type == IMGDT_Lump && epi::case_cmp(img->info, texname) == 0 &&
                        W_CheckFileNumForName(texname.c_str()) < pack_index)
                    {
                        img->type = IMGDT_Package;
                        img->info = entry.packpath;
                        add_it    = false;
                    }
                }

                // If no DDF just see if a bare lump with the same name comes later
                if (W_CheckFileNumForName(texname.c_str()) > pack_index)
                    add_it = false;

                if (!add_it)
                    continue;

                I_Debugf("- Adding image file in EPK: %s\n", entry.packpath.c_str());

                if (dir_name == "textures")
                    AddImage_SmartPack(texname.c_str(), IMSRC_TX_HI, entry.packpath.c_str(), real_textures);
                else if (dir_name == "graphics")
                    AddImage_SmartPack(texname.c_str(), IMSRC_Graphic, entry.packpath.c_str(), real_graphics);
                else if (dir_name == "flats")
                    AddImage_SmartPack(texname.c_str(), IMSRC_Flat, entry.packpath.c_str(), real_flats);
                else if (dir_name == "skins") // Not sure about this still
                    AddImage_SmartPack(texname.c_str(), IMSRC_Sprite, entry.packpath.c_str(), real_sprites);
            }
            else
            {
                I_Warning("Unknown image type in EPK: %s\n", entry.name.c_str());
            }
        }
    }
    // Only sub out sounds and music if they would replace an existing DDF entry
    // This MAY expand to create automatic simple DDFSFX entries if they aren't defined anywhere else
    d = pack->FindDir("sounds");
    if (d > 0)
    {
        for (size_t i = 0; i < pack->dirs[d].entries.size(); i++)
        {
            pack_entry_c &entry = pack->dirs[d].entries[i];
            for (int j = 0; j < sfxdefs.GetSize(); j++)
            {
                sfxdef_c *sfx = sfxdefs[j];
                // Assume that same stem name is meant to replace an identically named lump entry
                if (!sfx->lump_name.empty())
                {
                    if (epi::case_cmp(epi::PATH_GetBasename(entry.name).string(), sfx->lump_name) == 0 &&
                        W_CheckFileNumForName(sfx->lump_name.c_str()) < pack_index)
                    {
                        sfx->pack_name = entry.packpath;
                        sfx->lump_name.clear();
                    }
                }
            }
        }
    }
    d = pack->FindDir("music");
    if (d > 0)
    {
        for (size_t i = 0; i < pack->dirs[d].entries.size(); i++)
        {
            pack_entry_c &entry = pack->dirs[d].entries[i];
            for (int j = 0; j < playlist.GetSize(); j++)
            {
                pl_entry_c *song = playlist[j];
                if (epi::PATH_GetExtension(song->info).empty())
                {
                    if (song->infotype == MUSINF_LUMP &&
                        epi::case_cmp(epi::PATH_GetBasename(entry.name).string(), song->info) == 0 &&
                        W_CheckFileNumForName(song->info.c_str()) < pack_index)
                    {
                        song->info     = entry.packpath;
                        song->infotype = MUSINF_PACKAGE;
                    }
                }
            }
        }
    }
    d = pack->FindDir("colormaps");
    if (d > 0)
    {
        for (size_t i = 0; i < pack->dirs[d].entries.size(); i++)
        {
            pack_entry_c &entry = pack->dirs[d].entries[i];

            std::string stem = epi::PATH_GetBasename(entry.name).string();

            bool add_it = true;

            for (auto colm : colourmaps)
            {
                if (!colm->lump_name.empty() &&
                    epi::case_cmp(colm->lump_name, epi::PATH_GetBasename(entry.name).string()) == 0 &&
                    W_CheckFileNumForName(colm->lump_name.c_str()) < pack_index)
                {
                    colm->lump_name.clear();
                    colm->pack_name = entry.packpath;
                    add_it          = false;
                }
            }

            if (add_it)
                DDF_AddRawColourmap(stem.c_str(), pack->EntryLength(d, i), entry.packpath.c_str());
        }
    }
}

void Pack_ProcessHiresSubstitutions(pack_file_c *pack, int pack_index)
{
    int d = pack->FindDir("hires");

    if (d < 0)
        return;

    for (size_t i = 0; i < pack->dirs[d].entries.size(); i++)
    {
        pack_entry_c &entry = pack->dirs[d].entries[i];

        // split filename in stem + extension
        std::string stem = epi::PATH_GetBasename(entry.name).string();
        std::string ext  = epi::PATH_GetExtension(entry.name).string();

        epi::str_lower(ext);

        if (ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".lmp") // Note: .lmp is assumed to be Doom-format image
        {
            std::string texname;

            epi::STR_TextureNameFromFilename(texname, stem);

            // See if a bare lump with the same name comes later
            if (W_CheckFileNumForName(texname.c_str()) > pack_index)
                continue;

            I_Debugf("- Adding Hires substitute from EPK: %s\n", entry.packpath.c_str());

            const image_c *rim = W_ImageDoLookup(real_textures, texname.c_str(), -2);
            if (rim && rim->source_type != IMSRC_User)
            {
                AddImage_SmartPack(texname.c_str(), IMSRC_TX_HI, entry.packpath.c_str(), real_textures, rim);
                continue;
            }

            rim = W_ImageDoLookup(real_flats, texname.c_str(), -2);
            if (rim && rim->source_type != IMSRC_User)
            {
                AddImage_SmartPack(texname.c_str(), IMSRC_TX_HI, entry.packpath.c_str(), real_flats, rim);
                continue;
            }

            rim = W_ImageDoLookup(real_sprites, texname.c_str(), -2);
            if (rim && rim->source_type != IMSRC_User)
            {
                AddImage_SmartPack(texname.c_str(), IMSRC_TX_HI, entry.packpath.c_str(), real_sprites, rim);
                continue;
            }

            // we do it this way to force the original graphic to be loaded
            rim = W_ImageLookup(texname.c_str(), INS_Graphic, ILF_Exact | ILF_Null);

            if (rim && rim->source_type != IMSRC_User)
            {
                AddImage_SmartPack(texname.c_str(), IMSRC_TX_HI, entry.packpath.c_str(), real_graphics, rim);
                continue;
            }

            I_Warning("HIRES replacement '%s' has no counterpart.\n", texname.c_str());

            AddImage_SmartPack(texname.c_str(), IMSRC_TX_HI, entry.packpath.c_str(), real_textures);
        }
        else
        {
            I_Warning("Unknown image type in EPK: %s\n", entry.name.c_str());
        }
    }
}

bool Pack_FindFile(pack_file_c *pack, const std::string &name)
{
    // when file does not exist, this returns false.

    SYS_ASSERT(!name.empty());

    // disallow absolute (real filesystem) paths,
    // although we have to let a leading '/' slide to be caught later
    if (epi::PATH_IsAbsolute(name) && name[0] != '/')
        return false;

    // do not accept filenames without extensions
    if (epi::PATH_GetExtension(name).empty())
        return false;

    // Make a copy in case we need to pop a leading slash
    std::string find_name = name;

    bool root_only = false;

    // Check for root-only search
    if (name[0] == '/')
    {
        find_name = name.substr(1);
        if (find_name == epi::PATH_GetFilename(name).string())
            root_only = true;
    }

    std::string find_stem = epi::PATH_GetBasename(name).string();
    epi::str_upper(find_stem);

    // quick file stem check to see if it's present at all
    if (!Pack_FindStem(pack, find_stem))
        return false;

    // Specific path given; attempt to find as-is, otherwise return false
    if (find_name != epi::PATH_GetFilename(find_name).string())
    {
        std::filesystem::path find_comp = find_name;
        auto results = pack->search_files.equal_range(find_stem);
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
        for (auto file : pack->dirs[0].entries)
        {
            if (epi::case_cmp(file.name, find_name) == 0)
                return true;
        }
        return false;
    }
    // Only filename given; return first full match from search list, if present
    // Search list is unordered, but realistically identical filename+extensions wouldn't be in the same pack
    else
    {
        auto results = pack->search_files.equal_range(find_stem);
        for (auto file = results.first; file != results.second; ++file)
        {
            if (epi::case_cmp(find_name, epi::PATH_GetFilename(file->second).string()) == 0)
                return true;
        }
        return false;
    }

    // Fallback
    return false;
}

epi::file_c *Pack_OpenFile(pack_file_c *pack, const std::string &name)
{
    // when file does not exist, this returns NULL.

    SYS_ASSERT(!name.empty());

    // disallow absolute (real filesystem) paths,
    // although we have to let a leading '/' slide to be caught later
    if (epi::PATH_IsAbsolute(name) && name[0] != '/')
        return nullptr;

    // do not accept filenames without extensions
    if (epi::PATH_GetExtension(name).empty())
        return nullptr;

    // Make a copy in case we need to pop a leading slash
    std::string open_name = name;

    bool root_only = false;

    // Check for root-only search
    if (name[0] == '/')
    {
        open_name = name.substr(1);
        if (open_name == epi::PATH_GetFilename(open_name).string())
            root_only = true;
    }

    std::string open_stem = epi::PATH_GetBasename(open_name).string();
    epi::str_upper(open_stem);

    // quick file stem check to see if it's present at all
    if (!Pack_FindStem(pack, open_stem))
        return nullptr;

    // Specific path given; attempt to open as-is, otherwise return NULL
    if (open_name != epi::PATH_GetFilename(open_name).string())
    {
        return pack->OpenFileByName(open_name);
    }
    // Search only the root dir for this filename, return NULL if not present
    else if (root_only)
    {
        for (auto file : pack->dirs[0].entries)
        {
            if (epi::case_cmp(file.name, open_name) == 0)
                return pack->OpenFileByName(open_name);
        }
        return nullptr;
    }
    // Only filename given; return first full match from search list, if present
    // Search list is unordered, but realistically identical filename+extensions wouldn't be in the same pack
    else
    {
        auto results = pack->search_files.equal_range(open_stem);
        for (auto file = results.first; file != results.second; ++file)
        {
            if (epi::case_cmp(open_name, epi::PATH_GetFilename(file->second).string()) == 0)
                return pack->OpenFileByName(file->second);
        }
        return nullptr;
    }

    // Fallback
    return nullptr;
}

// Like the above, but is in the form of a stem + acceptable extensions
epi::file_c *Pack_OpenMatch(pack_file_c *pack, const std::string &name, const std::vector<std::string> &extensions)
{
    // when file does not exist, this returns NULL.

    // Nothing to match (may change this to allow a wildcard in the future)
    if (extensions.empty())
        return NULL;

    std::string open_stem = name;
    epi::str_upper(open_stem);

    // quick file stem check to see if it's present at all
    if (!Pack_FindStem(pack, open_stem))
        return NULL;

    std::filesystem::path stem_match = open_stem;

    auto results = pack->search_files.equal_range(open_stem);
    for (auto file = results.first; file != results.second; ++file)
    {
        for (auto ext : extensions)
        {
            stem_match.replace_extension(ext);
            if (epi::case_cmp(stem_match.string(), epi::PATH_GetFilename(file->second).string()) == 0)
                return pack->OpenFileByName(file->second);
        }
    }

    return NULL;
}

std::vector<std::string> Pack_GetSpriteList(pack_file_c *pack)
{
    std::vector<std::string> found_sprites;

    int d = pack->FindDir("sprites");
    if (d > 0)
    {
        for (size_t i = 0; i < pack->dirs[d].entries.size(); i++)
        {
            pack_entry_c &entry = pack->dirs[d].entries[i];

            // split filename in stem + extension
            std::string stem = epi::PATH_GetBasename(entry.name).string();
            std::string ext  = epi::PATH_GetExtension(entry.name).string();

            epi::str_lower(ext);

            if (ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" ||
                ext == ".lmp") // Note: .lmp is assumed to be Doom-format image
            {

                std::string texname;
                epi::STR_TextureNameFromFilename(texname, stem);

                // Don't add things already defined in DDFIMAGE
                for (int j = 0; j < imagedefs.GetSize(); j++)
                {
                    imagedef_c *img = imagedefs[j];
                    if (epi::case_cmp(img->name, texname) == 0)
                        continue;
                }

                found_sprites.push_back(entry.packpath);
            }
        }
    }

    return found_sprites;
}

static void ProcessWADsInPack(pack_file_c *pack)
{
    for (size_t d = 0; d < pack->dirs.size(); d++)
    {
        for (size_t i = 0; i < pack->dirs[d].entries.size(); i++)
        {
            pack_entry_c &entry = pack->dirs[d].entries[i];

            if (!entry.HasExtension(".wad"))
                continue;

            epi::file_c *pack_wad = Pack_OpenFile(pack, entry.packpath);

            if (pack_wad)
            {
                byte            *raw_pack_wad = pack_wad->LoadIntoMemory();
                epi::mem_file_c *pack_wad_mem = new epi::mem_file_c(raw_pack_wad, pack_wad->GetLength(), true);
                delete[] raw_pack_wad; // copied on pack_wad_mem creation
                data_file_c *pack_wad_df = new data_file_c(
                    entry.name, (pack->parent->kind == FLKIND_IFolder || pack->parent->kind == FLKIND_IPK)
                                    ? FLKIND_IPackWAD
                                    : FLKIND_PackWAD);
                pack_wad_df->name = entry.name;
                pack_wad_df->file = pack_wad_mem;
                ProcessFile(pack_wad_df);
            }

            delete pack_wad;
        }
    }
}

void Pack_PopulateOnly(data_file_c *df)
{
    if (df->kind == FLKIND_Folder || df->kind == FLKIND_EFolder || df->kind == FLKIND_IFolder)
        df->pack = ProcessFolder(df);
    else
        df->pack = ProcessZip(df);

    df->pack->SortEntries();
}

int Pack_CheckForIWADs(data_file_c *df)
{
    pack_file_c *pack        = df->pack;
    for (size_t d = 0; d < pack->dirs.size(); d++)
    {
        for (size_t i = 0; i < pack->dirs[d].entries.size(); i++)
        {
            pack_entry_c &entry = pack->dirs[d].entries[i];

            if (!entry.HasExtension(".wad"))
                continue;

            epi::file_c *pack_wad = Pack_OpenFile(pack, entry.packpath);

            if (pack_wad)
            {
                int pack_iwad_check  = W_CheckForUniqueLumps(pack_wad);
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

void Pack_ProcessAll(data_file_c *df, size_t file_index)
{
    if (df->kind == FLKIND_Folder || df->kind == FLKIND_EFolder || df->kind == FLKIND_IFolder)
        df->pack = ProcessFolder(df);
    else
        df->pack = ProcessZip(df);

    df->pack->SortEntries();

    // parse the WADFIXES file from edge_defs folder or `edge_defs.epk` immediately
    if ((df->kind == FLKIND_EFolder || df->kind == FLKIND_EEPK) && file_index == 0)
    {
        I_Printf("Loading WADFIXES\n");
        epi::file_c *wadfixes = Pack_OpenFile(df->pack, "wadfixes.ddf");
        if (wadfixes)
            DDF_ReadFixes(wadfixes->ReadText());
        delete wadfixes;
    }

    // Only load some things here; the rest are deferred until
    // after all files loaded so that pack substitutions can work properly
    ProcessDDFInPack(df->pack);

    // COAL

    // parse COALAPI only from edge_defs folder or `edge_defs.epk`
    if ((df->kind == FLKIND_EFolder || df->kind == FLKIND_EEPK) && file_index == 0)
        ProcessCoalAPIInPack(df->pack);
    ProcessCoalHUDInPack(df->pack);

    // LUA

    // parse lua api  only from edge_defs folder or `edge_defs.epk`
    if ((df->kind == FLKIND_EFolder || df->kind == FLKIND_EEPK) && file_index == 0)
        ProcessLuaAPIInPack(df->pack);
    ProcessLuaHUDInPack(df->pack);

    ProcessWADsInPack(df->pack);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
