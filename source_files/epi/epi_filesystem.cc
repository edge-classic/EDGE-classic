//----------------------------------------------------------------------------
//  EDGE Filesystem Class
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2024 The EDGE Team.
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

#include "epi_filesystem.h"

#include "epi.h"
#include "epi_file.h"
#include "epi_sdl.h"
#include "epi_str_compare.h"
#include "epi_windows.h"
#ifdef EDGE_WEB
#include <emscripten.h>
#endif
#ifndef _WIN32
#include <dirent.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#ifdef __MINGW32__
#include <sys/stat.h>
#endif

namespace epi
{

#ifdef _WIN32                                    // Windows API
static inline bool IsDirectorySeparator(const char c)
{
    return (c == '\\' || c == '/' || c == ':'); // Kester added ':'
}
bool IsPathAbsolute(std::string_view path)
{
    EPI_ASSERT(!path.empty());

    // Check for Drive letter, colon and slash...
    if (path.size() > 2 && path[1] == ':' && (path[2] == '\\' || path[2] == '/') && IsAlphaASCII(path[0]))
    {
        return true;
    }

    // Check for share name...
    if (path.size() > 1 && path[0] == '\\' && path[1] == '\\')
        return true;

    return false;
}
static const wchar_t *FlagsToANSIMode(int flags)
{
    // Must have some value in flags
    if (flags == 0)
        return nullptr;

    // Check for any invalid combinations
    if ((flags & kFileAccessWrite) && (flags & kFileAccessAppend))
        return nullptr;

    if (flags & kFileAccessRead)
    {
        if (flags & kFileAccessWrite)
            return L"wb+";
        else if (flags & kFileAccessAppend)
            return L"ab+";
        else
            return L"rb";
    }
    else
    {
        if (flags & kFileAccessWrite)
            return L"wb";
        else if (flags & kFileAccessAppend)
            return L"ab";
        else
            return nullptr; // Invalid
    }
}
FILE *FileOpenRaw(std::string_view name, unsigned int flags)
{
    const wchar_t *mode = FlagsToANSIMode(flags);
    if (!mode)
        return nullptr;
    std::wstring wname = epi::UTF8ToWString(name);
    return _wfopen(wname.c_str(), mode);
}
bool FileDelete(std::string_view name)
{
    EPI_ASSERT(!name.empty());
    std::wstring wname = epi::UTF8ToWString(name);
    return _wremove(wname.c_str()) == 0;
}
bool IsDirectory(std::string_view dir)
{
    EPI_ASSERT(!dir.empty());
    std::wstring wide_dir = epi::UTF8ToWString(dir);
#ifdef __MINGW32__
    struct stat dircheck;
    if (wstat(wide_dir.c_str(), &dircheck) != 0)
        return false;
#else
    struct _stat dircheck;
    if (_wstat(wide_dir.c_str(), &dircheck) != 0)
        return false;
#endif
    return (dircheck.st_mode & _S_IFDIR);
}
static std::string CurrentDirectoryGet()
{
    std::string    directory;
    const wchar_t *dir = _wgetcwd(nullptr, 0);
    if (dir)
        directory = epi::WStringToUTF8(dir);
    return directory; // can be empty
}
bool CurrentDirectorySet(std::string_view dir)
{
    EPI_ASSERT(!dir.empty());
    std::wstring wdir = epi::UTF8ToWString(dir);
    return _wchdir(wdir.c_str()) == 0;
}
bool MakeDirectory(std::string_view dir)
{
    EPI_ASSERT(!dir.empty());
    std::wstring wdirectory = epi::UTF8ToWString(dir);
    return _wmkdir(wdirectory.c_str()) == 0;
}
bool FileExists(std::string_view name)
{
    EPI_ASSERT(!name.empty());
    std::wstring wname = epi::UTF8ToWString(name);
    return _waccess(wname.c_str(), 0) == 0;
}
bool TestFileAccess(std::string_view name)
{
    // The codebase only seems to use this to test read access, so we
    // shouldn't need to pass any modes as a parameter
    EPI_ASSERT(!name.empty());
    std::wstring wname = epi::UTF8ToWString(name);
    if (_waccess(wname.c_str(), 4) == 0)      // Read-only
        return true;
    else if (_waccess(wname.c_str(), 6) == 0) // Read/write
        return true;
    else
        return false;
}
bool ReadDirectory(std::vector<DirectoryEntry> &fsd, std::string &dir, const char *mask)
{
    if (dir.empty() || !FileExists(dir) || !mask)
        return false;

    std::string prev_dir = CurrentDirectoryGet();

    if (prev_dir.empty()) // Something goofed up, don't make it worse
        return false;

    if (!CurrentDirectorySet(dir))
        return false;

    std::wstring     fmask = epi::UTF8ToWString(mask);
    WIN32_FIND_DATAW fdataw;
    HANDLE           fhandle = FindFirstFileW(fmask.c_str(), &fdataw);

    if (fhandle == INVALID_HANDLE_VALUE)
    {
        CurrentDirectorySet(prev_dir);
        return false;
    }

    fsd.clear();

    do
    {
        std::string filename = epi::WStringToUTF8(fdataw.cFileName);
        if (filename == "." || filename == "..")
        {
            // skip the funky "." and ".." dirs
        }
        else
        {
            epi::DirectoryEntry new_entry;
            new_entry.name   = dir;
            new_entry.is_dir = (fdataw.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
            new_entry.size   = new_entry.is_dir ? 0 : fdataw.nFileSizeLow;
            new_entry.name.push_back('/');
            new_entry.name.append(filename);
            fsd.push_back(new_entry);
        }
    } while (FindNextFileW(fhandle, &fdataw));

    FindClose(fhandle);
    CurrentDirectorySet(prev_dir);
    return true;
}
bool WalkDirectory(std::vector<DirectoryEntry> &fsd, std::string &dir)
{
    if (dir.empty() || !FileExists(dir))
        return false;

    std::string prev_dir = CurrentDirectoryGet();

    if (prev_dir.empty()) // Something goofed up, don't make it worse
        return false;

    if (!CurrentDirectorySet(dir))
        return false;

    WIN32_FIND_DATAW fdataw;
    HANDLE           fhandle = FindFirstFileW(L"*.*", &fdataw);

    if (fhandle == INVALID_HANDLE_VALUE)
    {
        CurrentDirectorySet(prev_dir);
        return false;
    }

    do
    {
        std::string filename = epi::WStringToUTF8(fdataw.cFileName);
        if (filename == "." || filename == "..")
        {
            // skip the funky "." and ".." dirs
        }
        else if (fdataw.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            std::string subdir = dir;
            subdir.push_back('/');
            subdir.append(filename);
            if (!WalkDirectory(fsd, subdir))
                return false;
        }
        else
        {
            epi::DirectoryEntry new_entry;
            new_entry.name   = dir;
            new_entry.is_dir = false;
            new_entry.size   = fdataw.nFileSizeLow;
            new_entry.name.push_back('/');
            new_entry.name.append(filename);
            fsd.push_back(new_entry);
        }
    } while (FindNextFileW(fhandle, &fdataw));

    FindClose(fhandle);
    CurrentDirectorySet(prev_dir);
    return true;
}
#else // POSIX API
static inline bool IsDirectorySeparator(const char c)
{
    return (c == '\\' || c == '/');
}
bool IsPathAbsolute(std::string_view path)
{
    EPI_ASSERT(!path.empty());

    if (IsDirectorySeparator(path[0]))
        return true;
    else
        return false;
}
static const char *FlagsToANSIMode(int flags)
{
    // Must have some value in flags
    if (flags == 0)
        return nullptr;

    // Check for any invalid combinations
    if ((flags & kFileAccessWrite) && (flags & kFileAccessAppend))
        return nullptr;

    if (flags & kFileAccessRead)
    {
        if (flags & kFileAccessWrite)
            return "wb+";
        else if (flags & kFileAccessAppend)
            return "ab+";
        else
            return "rb";
    }
    else
    {
        if (flags & kFileAccessWrite)
            return "wb";
        else if (flags & kFileAccessAppend)
            return "ab";
        else
            return nullptr; // Invalid
    }
}
FILE *FileOpenRaw(std::string_view name, unsigned int flags)
{
    const char *mode = FlagsToANSIMode(flags);
    if (!mode)
        return nullptr;
    EPI_ASSERT(!name.empty());
    return fopen(std::string(name).c_str(), mode);
}
bool FileDelete(std::string_view name)
{
    EPI_ASSERT(!name.empty());
    return remove(std::string(name).c_str()) == 0;
}
bool IsDirectory(std::string_view dir)
{
    EPI_ASSERT(!dir.empty());
    struct stat dircheck;
    if (stat(std::string(dir).c_str(), &dircheck) == -1)
        return false;
    return S_ISDIR(dircheck.st_mode);
}
static std::string CurrentDirectoryGet()
{
    std::string directory;
    const char *dir = getcwd(nullptr, 0);
    if (dir)
        directory = dir;
    return directory;
}
bool CurrentDirectorySet(std::string_view dir)
{
    EPI_ASSERT(!dir.empty());
    return chdir(std::string(dir).c_str()) == 0;
}
bool MakeDirectory(std::string_view dir)
{
    EPI_ASSERT(!dir.empty());
    return (mkdir(std::string(dir).c_str(), 0664) == 0);
}
bool FileExists(std::string_view name)
{
    EPI_ASSERT(!name.empty());
    return access(std::string(name).c_str(), F_OK) == 0;
}
bool TestFileAccess(std::string_view name)
{
    // The codebase only seems to use this to test read access, so we
    // shouldn't need to pass any modes as a parameter
    EPI_ASSERT(!name.empty());
    return access(std::string(name).c_str(), R_OK) == 0;
}
bool ReadDirectory(std::vector<DirectoryEntry> &fsd, std::string &dir, const char *mask)
{
    if (dir.empty() || !FileExists(dir) || !mask)
        return false;

    std::string prev_dir = CurrentDirectoryGet();

    std::string mask_ext = epi::GetExtension(mask); // Allows us to retain "*.*" style syntax

    if (prev_dir.empty())                           // Something goofed up, don't make it worse
        return false;

    if (!CurrentDirectorySet(dir))
        return false;

    DIR *handle = opendir(dir.c_str());
    if (!handle)
        return false;

    fsd.clear();

    for (;;)
    {
        struct dirent *fdata = readdir(handle);
        if (!fdata)
            break;

        if (strlen(fdata->d_name) == 0)
            continue;

        std::string filename = fdata->d_name;

        // skip the funky "." and ".." dirs
        if (filename == "." || filename == "..")
            continue;

        // I though fnmatch should handle this, but ran into case sensitivity
        // issues when using WSL for some reason - Dasho
        if (mask_ext != ".*" && epi::StringCaseCompareASCII(mask_ext, epi::GetExtension(filename)) != 0)
            continue;

        struct stat finfo;

        if (stat(filename.c_str(), &finfo) != 0)
            continue;

        epi::DirectoryEntry new_entry;
        new_entry.name   = dir;
        new_entry.is_dir = S_ISDIR(finfo.st_mode) ? true : false;
        new_entry.size   = finfo.st_size;
        new_entry.name.push_back('/');
        new_entry.name.append(filename);
        fsd.push_back(new_entry);
    }

    CurrentDirectorySet(prev_dir);
    closedir(handle);
    return true;
}
static std::vector<DirectoryEntry> *nftw_fsd = nullptr;
static int WalkDirectoryCallback(const char *filename, const struct stat *stat_pointer, int file_flags, FTW *walk_pointer)
{
    EPI_ASSERT(nftw_fsd);
    if (file_flags == FTW_F) // The documentation for nftw refers to these as flags but I think it can only be one of them - Dasho
    {
        epi::DirectoryEntry new_entry;
        new_entry.name   = filename;
        new_entry.is_dir = false;
        new_entry.size   = stat_pointer->st_size;
        nftw_fsd->push_back(new_entry);
    }
    return 0;
}
bool WalkDirectory(std::vector<DirectoryEntry> &fsd, std::string &dir)
{
    if (dir.empty() || !FileExists(dir))
        return false;

    nftw_fsd = &fsd;

    int result = nftw(dir.c_str(), WalkDirectoryCallback, 10, (FTW_DEPTH|FTW_MOUNT|FTW_PHYS));

    nftw_fsd = nullptr;

    return (result == 0);
}
#endif

// Universal Functions

std::string GetStem(std::string_view path)
{
    EPI_ASSERT(!path.empty());
    // back up until a slash or the start
    for (int p = path.size() - 1; p > 1; p--)
    {
        if (IsDirectorySeparator(path[p - 1]))
        {
            path.remove_prefix(p);
            break;
        }
    }
    // back up until a dot
    for (int p = path.size() - 2; p >= 0; p--)
    {
        const char ch = path[p];
        if (IsDirectorySeparator(ch))
            break;

        if (ch == '.')
        {
            // handle filenames that being with a dot
            // (un*x style hidden files)
            if (p == 0 || IsDirectorySeparator(path[p - 1]))
                break;

            path.remove_suffix(path.size() - p);
            break;
        }
    }
    std::string filename(path);
    return filename;
}

std::string GetFilename(std::string_view path)
{
    EPI_ASSERT(!path.empty());
    // back up until a slash or the start
    for (int p = path.size() - 1; p > 0; p--)
    {
        if (IsDirectorySeparator(path[p - 1]))
        {
            path.remove_prefix(p);
            break;
        }
    }
    std::string filename(path);
    return filename;
}

// This should only be for EPK entry use; essentially it strips the parent
// path from the child path assuming the parent is actually in the child path
std::string MakePathRelative(std::string_view parent, std::string_view child)
{
    EPI_ASSERT(!parent.empty() && !child.empty() && child.size() > parent.size());
    size_t parent_check = child.find(parent);
    if (parent_check != std::string_view::npos)
    {
        child.remove_prefix(parent.size());
        if (IsDirectorySeparator(child[0]))
            child.remove_prefix(1);
    }
    std::string relpath(child);
    return relpath;
}

std::string SanitizePath(std::string_view path)
{
    std::string sani_path;
    for (const char ch : path)
    {
        if (ch == '\\')
            sani_path.push_back('/');
        else
            sani_path.push_back(ch);
    }
    return sani_path;
}

std::string PathAppend(std::string_view parent, std::string_view child)
{
    EPI_ASSERT(!parent.empty() && !child.empty());

    if (IsDirectorySeparator(parent.back()))
        parent.remove_suffix(1);

    std::string new_path(parent);

    new_path.push_back('/');

    if (IsDirectorySeparator(child[0]))
        child.remove_prefix(1);

    new_path.append(child);

    return new_path;
}

std::string PathAppendIfNotAbsolute(std::string_view parent, std::string_view child)
{
    EPI_ASSERT(!parent.empty() && !child.empty());

    std::string new_path;

    if (IsPathAbsolute(child))
        new_path = child;
    else
    {
        if (IsDirectorySeparator(parent.back()))
            parent.remove_suffix(1);
        new_path = parent;
        new_path.push_back('/');
        if (IsDirectorySeparator(child[0]))
            child.remove_prefix(1);
        new_path.append(child);
    }

    return new_path;
}

std::string GetDirectory(std::string_view path)
{
    EPI_ASSERT(!path.empty());
    std::string directory;
    // back up until a slash or the start
    for (int p = path.size() - 1; p >= 0; p--)
    {
        if (IsDirectorySeparator(path[p]))
        {
            directory = path.substr(0, p);
            break;
        }
    }

    return directory; // nothing
}

std::string GetExtension(std::string_view path)
{
    EPI_ASSERT(!path.empty());
    std::string extension;
    // back up until a dot
    for (int p = path.size() - 1; p >= 0; p--)
    {
        const char ch = path[p];
        if (IsDirectorySeparator(ch))
            break;

        if (ch == '.')
        {
            // handle filenames that being with a dot
            // (un*x style hidden files)
            if (p == 0 || IsDirectorySeparator(path[p - 1]))
                break;

            extension = path.substr(p);
            break;
        }
    }
    return extension; // can be empty
}

void ReplaceExtension(std::string &path, std::string_view ext)
{
    EPI_ASSERT(!path.empty() && !ext.empty());
    int extpos = -1;
    // back up until a dot
    for (int p = path.size() - 1; p >= 0; p--)
    {
        char &ch = path[p];
        if (IsDirectorySeparator(ch))
            break;

        if (ch == '.')
        {
            // handle filenames that being with a dot
            // (un*x style hidden files)
            if (p == 0 || IsDirectorySeparator(path[p - 1]))
                break;

            extpos = p;
            break;
        }
    }
    if (extpos == -1) // No extension found, add it
        path.append(ext);
    else
    {
        while (path.size() > extpos)
        {
            path.pop_back();
        }
        path.append(ext);
    }
}

File *FileOpen(std::string_view name, unsigned int flags)
{
    EPI_ASSERT(!name.empty());
    FILE *fp = FileOpenRaw(name, flags);
    if (!fp)
        return nullptr;
    return new ANSIFile(fp);
}

bool OpenDirectory(const std::string &src)
{
    // A result of 0 is 'success', but that only means SDL was able to launch
    // some kind of process to attempt to handle the path. -1 is the only result
    // that is guaranteed to be an 'error'
    if (SDL_OpenURL(StringFormat("file:///%s", src.c_str()).c_str()) == -1)
    {
        LogWarning("OpenDirectory failed to open requested path %s\nError: %s\n", src.c_str(), SDL_GetError());
        return false;
    }
    return true;
}

bool FileCopy(std::string_view src, std::string_view dest)
{
    EPI_ASSERT(!src.empty() && !dest.empty());

    if (!epi::TestFileAccess(src))
        return false;

    if (epi::FileExists(dest))
    {
        // overwrite dest if it exists
        if (!epi::FileDelete(dest))
            return false;
    }

    File *srcfile = epi::FileOpen(src, kFileAccessRead | kFileAccessBinary);

    if (!srcfile)
        return false;

    File *destfile = epi::FileOpen(dest, kFileAccessWrite | kFileAccessBinary);

    if (!destfile)
    {
        delete srcfile;
        return false;
    }

    int srcsize = srcfile->GetLength();

    uint8_t *srcdata = srcfile->LoadIntoMemory();

    int copied = destfile->Write(srcdata, srcsize);

    delete[] srcdata;
    delete destfile;
    delete srcfile;

    if (copied != srcsize)
    {
        epi::FileDelete(dest);
        return false;
    }
    else
        return true;
}

// Emscripten-specific
#ifdef EDGE_WEB
void SyncFilesystem(bool populate)
{
    EM_ASM_(
        {
            if (Module.edgePreSyncFS)
            {
                Module.edgePreSyncFS();
            }
            FS.syncfs(
                $0, function(err) {
                    if (Module.edgePostSyncFS)
                    {
                        Module.edgePostSyncFS();
                    }
                });
        },
        populate);
}
#else
void SyncFilesystem(bool populate)
{
    (void)populate;
}
#endif

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
