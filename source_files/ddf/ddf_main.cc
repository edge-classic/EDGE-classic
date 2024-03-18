//----------------------------------------------------------------------------
//  EDGE Data Definition Files Code (Main)
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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

#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "ddf_anim.h"
#include "ddf_colormap.h"
#include "ddf_font.h"
#include "ddf_image.h"
#include "ddf_local.h"
#include "ddf_style.h"
#include "ddf_switch.h"
// EPI
#include "epi.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "sokol_color.h"

// EDGE
#include "p_action.h"

void ReadTriggerScript(const std::string &_data, const std::string &source);

enum DDFReadStatus
{
    kDdfReadStatusInvalid = 0,
    kDdfReadStatusWaitingTag,
    kDdfReadStatusReadingTag,
    kDdfReadStatusWaitingNewDefinition,
    kDdfReadStatusReadingNewDefinition,
    kDdfReadStatusReadingCommand,
    kDdfReadStatusReadingData,
    kDdfReadStatusReadingRemark,
    kDdfReadStatusReadingString
};

enum DDFReadCharReturn
{
    kDdfReadCharReturnNothing,
    kDdfReadCharReturnCommand,
    kDdfReadCharReturnProperty,
    kDdfReadCharReturnDefinitionStart,
    kDdfReadCharReturnDefinitionStop,
    kDdfReadCharReturnRemarkStart,
    kDdfReadCharReturnRemarkStop,
    kDdfReadCharReturnSeparator,
    kDdfReadCharReturnStringStart,
    kDdfReadCharReturnStringStop,
    kDdfReadCharReturnGroupStart,
    kDdfReadCharReturnGroupStop,
    kDdfReadCharReturnTagStart,
    kDdfReadCharReturnTagStop,
    kDdfReadCharReturnTerminator,
    kDdfReadCharReturnOK
};

#define DDF_DEBUG_READ 0

bool strict_errors = false;
bool lax_errors    = false;
bool no_warnings   = false;

//
// DdfError
//
// -AJA- 1999/10/27: written.
//
int         cur_ddf_line_num;
std::string cur_ddf_filename;
std::string cur_ddf_entryname;
std::string cur_ddf_linedata;

void DdfError(const char *err, ...)
{
    va_list argptr;
    char    buffer[2048];
    char   *pos;

    buffer[2047] = 0;

    // put actual error message on first line
    va_start(argptr, err);
    vsprintf(buffer, err, argptr);
    va_end(argptr);

    pos = buffer + strlen(buffer);

    if (cur_ddf_filename != "")
    {
        sprintf(pos, "Error occurred near line %d of %s\n", cur_ddf_line_num, cur_ddf_filename.c_str());
        pos += strlen(pos);
    }

    if (cur_ddf_entryname != "")
    {
        sprintf(pos, "Error occurred in entry: %s\n", cur_ddf_entryname.c_str());
        pos += strlen(pos);
    }

    if (cur_ddf_linedata != "")
    {
        sprintf(pos, "Line contents: %s\n", cur_ddf_linedata.c_str());
        pos += strlen(pos);
    }

    // check for buffer overflow
    if (buffer[2047] != 0)
        FatalError("Buffer overflow in DdfError\n");

    // add a blank line for readability under DOS/Linux.
    LogPrint("\n");

    FatalError("%s", buffer);
}

void DdfWarning(const char *err, ...)
{
    va_list argptr;
    char    buffer[1024];

    if (no_warnings)
        return;

    va_start(argptr, err);
    vsprintf(buffer, err, argptr);
    va_end(argptr);

    LogWarning("%s", buffer);

    if (!cur_ddf_filename.empty())
    {
        LogPrint("  problem occurred near line %d of %s\n", cur_ddf_line_num, cur_ddf_filename.c_str());
    }

    if (!cur_ddf_entryname.empty())
    {
        LogPrint("  problem occurred in entry: %s\n", cur_ddf_entryname.c_str());
    }

    if (!cur_ddf_linedata.empty())
    {
        LogPrint("  with line contents: %s\n", cur_ddf_linedata.c_str());
    }
}

void DdfDebug(const char *err, ...)
{
    va_list argptr;
    char    buffer[1024];

    if (no_warnings)
        return;

    va_start(argptr, err);
    vsprintf(buffer, err, argptr);
    va_end(argptr);

    LogDebug("%s", buffer);

    if (!cur_ddf_filename.empty())
    {
        LogDebug("  problem occurred near line %d of %s\n", cur_ddf_line_num, cur_ddf_filename.c_str());
    }

    if (!cur_ddf_entryname.empty())
    {
        LogDebug("  problem occurred in entry: %s\n", cur_ddf_entryname.c_str());
    }

    if (!cur_ddf_linedata.empty())
    {
        LogDebug("  with line contents: %s\n", cur_ddf_linedata.c_str());
    }
}

void DdfWarnError(const char *err, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, err);
    vsprintf(buffer, err, argptr);
    va_end(argptr);

    if (strict_errors)
        DdfError("%s", buffer);
    else
        DdfWarning("%s", buffer);
}

void DdfInit()
{
    DdfStateInit();
    DdfSFXInit();
    DdfColmapInit();
    DdfImageInit();
    DdfFontInit();
    DdfStyleInit();
    DdfAttackInit();
    DdfWeaponInit();
    DdfMobjInit();
    DdfLinedefInit();
    DdfSectorInit();
    DdfSwitchInit();
    DdfAnimInit();
    DdfGameInit();
    DdfLevelInit();
    DdfMusicPlaylistInit();
    DdfFlatInit();
    DdfFixInit();
    DdfMovieInit();
}

class define_c
{
  public:
    std::string name;
    std::string value;

  public:
    define_c() : name(), value()
    {
    }

    define_c(const char *N, const char *V) : name(N), value(V)
    {
    }

    define_c(const std::string &N, const std::string &V) : name(N), value(V)
    {
    }

    ~define_c()
    {
    }
};

// defines are very rare, hence no need for fast lookup.
// a std::vector is nice and simple.
static std::vector<define_c> all_defines;

void DdfMainAddDefine(const char *name, const char *value)
{
    all_defines.push_back(define_c(name, value));
}

void DdfMainAddDefine(const std::string &name, const std::string &value)
{
    all_defines.push_back(define_c(name, value));
}

const char *DdfMainGetDefine(const char *name)
{
    // search backwards, to allow redefinitions to work
    for (int i = (int)all_defines.size() - 1; i >= 0; i--)
        if (epi::StringCaseCompareASCII(all_defines[i].name, name) == 0)
            return all_defines[i].value.c_str();

    // undefined, so use the token as-is
    return name;
}

void DdfMainFreeDefines()
{
    all_defines.clear();
}

//
// DdfMainCleanup
//
// This goes through the information loaded via DDF and matchs any
// info stored as references.
//
void DdfCleanUp()
{
    DdfLanguageCleanUp();
    DdfImageCleanUp();
    DdfFontCleanUp();
    DdfStyleCleanUp();
    DdfMobjCleanUp();
    DdfAttackCleanUp();
    DdfStateCleanUp();
    DdfLinedefCleanUp();
    DdfSFXCleanUp();
    DdfColmapCleanUp();
    DdfWeaponCleanUp();
    DdfSectorCleanUp();
    DdfSwitchCleanUp();
    DdfAnimCleanUp();
    DdfGameCleanUp();
    DdfLevelCleanUp();
    DdfMusicPlaylistCleanUp();
    DdfFlatCleanUp();
    DdfFixCleanUp();
    DdfMovieCleanUp();
}

static const char *tag_conversion_table[] = {
    "ANIMATIONS", "DDFANIM",  "ATTACKS", "DDFATK",  "COLOURMAPS", "DDFCOLM",  "FLATS",     "DDFFLAT",
    "FIXES",      "WADFIXES", "FONTS",   "DDFFONT", "GAMES",      "DDFGAME",  "IMAGES",    "DDFIMAGE",
    "LANGUAGES",  "DDFLANG",  "LEVELS",  "DDFLEVL", "LINES",      "DDFLINE",  "PLAYLISTS", "DDFPLAY",
    "SECTORS",    "DDFSECT",  "SOUNDS",  "DDFSFX",  "STYLES",     "DDFSTYLE", "SWITCHES",  "DDFSWTH",
    "THINGS",     "DDFTHING", "WEAPONS", "DDFWEAP", "MOVIES",     "DDFMOVIE",

    nullptr,      nullptr};

void DdfGetLumpNameForFile(const char *filename, char *lumpname)
{
    FILE *fp = fopen(filename, "r");

    if (!fp)
        FatalError("Couldn't open DDF file: %s\n", filename);

    bool in_comment = false;

    for (;;)
    {
        int ch = fgetc(fp);

        if (ch == EOF || ferror(fp))
            break;

        if (ch == '/' || ch == '#') // skip directives too
        {
            in_comment = true;
            continue;
        }

        if (in_comment)
        {
            if (ch == '\n' || ch == '\r')
                in_comment = false;
            continue;
        }

        if (ch == '[')
            break;

        if (ch != '<')
            continue;

        // found start of <XYZ> tag, read it in

        char tag_buf[40];
        int  len = 0;

        for (;;)
        {
            ch = fgetc(fp);

            if (ch == EOF || ferror(fp) || ch == '>')
                break;

            tag_buf[len++] = epi::ToUpperASCII(ch);

            if (len + 2 >= (int)sizeof(tag_buf))
                break;
        }

        tag_buf[len] = 0;

        if (len > 0)
        {
            for (int i = 0; tag_conversion_table[i]; i += 2)
            {
                if (strcmp(tag_buf, tag_conversion_table[i]) == 0)
                {
                    strcpy(lumpname, tag_conversion_table[i + 1]);
                    fclose(fp);

                    return; // SUCCESS!
                }
            }

            fclose(fp);
            FatalError("Unknown marker <%s> in DDF file: %s\n", tag_buf, filename);
        }
        break;
    }

    fclose(fp);
    FatalError("Missing <..> marker in DDF file: %s\n", filename);
}

//
// Description of the DDF Parser:
//
// The DDF Parser is a simple reader that is very limited in error checking,
// however it can adapt to most tasks, as is required for the variety of stuff
// need to be loaded in order to configure the EDGE Engine.
//
// The parser will read an ascii file, character by character an interpret each
// depending in which mode it is in; Unless an error is encountered or a called
// procedure stops the parser, it will read everything until EOF is encountered.
//
// When the parser function is called, a pointer to a DDFReadInfo is passed and
// contains all the info needed, it contains:
//
// * filename              - filename to be read, returns error if nullptr
// * DdfMainCheckName     - function called when a def has been just been
// started
// * DdfMainCheckCmd      - function called when we need to check a command
// * DdfMainCreateEntry   - function called when a def has been completed
// * DdfMainFinishingCode - function called when EOF is read
// * currentcmdlist        - Current list of commands
//
// Also when commands are referenced, they use currentcmdlist, which is a
// pointer to a list of entries, the entries are formatted like this:
//
// * name - name of command
// * routine - function called to interpret info
// * numeric - void pointer to an value (possibly used by routine)
//
// name is compared with the read command, to see if it matchs.
// routine called to interpret info, if command name matches read command.
// numeric is used if a numeric value needs to be changed, by routine.
//
// The different parser modes are:
//  kDdfReadStatusWaitingNewDefinition
//  kDdfReadStatusReadingNewDefinition
//  kDdfReadStatusReadingCommand
//  kDdfReadStatusReadingData
//  kDdfReadStatusReadingRemark
//  kDdfReadStatusReadingString
//
// 'kDdfReadStatusWaitingNewDefinition' is only set at the start of the code, At
// this point every character with the exception of DEFSTART is ignored. When
// DEFSTART is encounted, the parser will switch to
// kDdfReadStatusReadingNewDefinition. DEFSTART the parser will only switches
// modes and sets firstgo to false.
//
// 'kDdfReadStatusReadingNewDefinition' reads all alphanumeric characters and
// the '_' character - which substitudes for a space character (whitespace is
// ignored) - until DEFSTOP is read. DEFSTOP passes the read string to
// DdfMainCheckName and then clears the string. Mode
// kDdfReadStatusReadingCommand is now set. All read stuff is passed to char
// *buffer.
//
// 'kDdfReadStatusReadingCommand' picks out all the alphabetic characters and
// passes them to buffer as soon as COMMANDREAD is encountered; DdfMainReadCmd
// looks through for a matching command, if none is found a fatal error is
// returned. If a matching command is found, this function returns a command
// reference number to command ref and sets the mode to
// kDdfReadStatusReadingData. if DEFSTART is encountered the procedure will
// clear the buffer, run DdfMainCreateEntry (called this as it reflects that in
// Items & Scenery if starts a new mobj type, in truth it can do anything
// procedure wise) and then switch mode to kDdfReadStatusReadingNewDefinition.
//
// 'kDdfReadStatusReadingData' passes alphanumeric characters, plus a few other
// characters that are also needed. It continues to feed buffer until a
// SEPARATOR or a TERMINATOR is found. The difference between SEPARATOR and
// TERMINATOR is that a TERMINATOR refs the cmdlist to find the routine to use
// and then sets the mode to kDdfReadStatusReadingCommand, whereas SEPARATOR
// refs the cmdlist to find the routine and a looks for more data on the same
// command. This is how the multiple states and specials are defined.
//
// 'kDdfReadStatusReadingRemark' does not process any chars except REMARKSTOP,
// everything else is ignored. This mode is only set when REMARKSTART is found,
// when this happens the current mode is held in formerstatus, which is restored
// when REMARKSTOP is found.
//
// 'kDdfReadStatusReadingString' is set when the parser is going through data
// (kDdfReadStatusReadingData) and encounters STRINGSTART and only stops on a
// STRINGSTOP. When kDdfReadStatusReadingString, everything that is an ASCII
// char is read (which the exception of STRINGSTOP) and passed to the buffer.
// REMARKS are ignored in when kDdfReadStatusReadingString and the case is take
// notice of here.
//
// The maximum size of BUFFER is set in the BUFFERSIZE define.
//
// DdfMainReadFile & DdfMainProcessChar handle the main processing of the
// file, all the procedures in the other DDF files (which the exceptions of the
// Inits) are called directly or indirectly. DdfMainReadFile handles to
// opening, closing and calling of procedures, DdfMainProcessChar makes sense
// from the character read from the file.
//

//
// DdfMainProcessChar
//
// 1998/08/10 Added String reading code.
//
static DDFReadCharReturn DdfMainProcessChar(char character, std::string &token, int status)
{
    // int len;

    // -ACB- 1998/08/11 Used for detecting formatting in a string
    static bool formatchar = false;

    // With the exception of kDdfReadStatusReadingString, whitespace is ignored.
    if (status != kDdfReadStatusReadingString)
    {
        if (epi::IsSpaceASCII(character))
            return kDdfReadCharReturnNothing;
    }
    else // check for formatting char in a string
    {
        if (!formatchar && character == '\\')
        {
            formatchar = true;
            return kDdfReadCharReturnNothing;
        }
    }

    // -AJA- 1999/09/26: Handle unmatched '}' better.
    if (status != kDdfReadStatusReadingString && character == '{')
        return kDdfReadCharReturnRemarkStart;

    if (status == kDdfReadStatusReadingRemark && character == '}')
        return kDdfReadCharReturnRemarkStop;

    if (status != kDdfReadStatusReadingString && character == '}')
        DdfError("DDF: Encountered '}' without previous '{'.\n");

    switch (status)
    {
    case kDdfReadStatusReadingRemark:
        return kDdfReadCharReturnNothing;

        // -ES- 2000/02/29 Added tag check.
    case kDdfReadStatusWaitingTag:
        if (character == '<')
            return kDdfReadCharReturnTagStart;
        else
            DdfError("DDF: File must start with a tag!\n");
        break;

    case kDdfReadStatusReadingTag:
        if (character == '>')
            return kDdfReadCharReturnTagStop;
        else
        {
            token += (character);
            return kDdfReadCharReturnOK;
        }

    case kDdfReadStatusWaitingNewDefinition:
        if (character == '[')
            return kDdfReadCharReturnDefinitionStart;
        else
            return kDdfReadCharReturnNothing;

    case kDdfReadStatusReadingNewDefinition:
        if (character == ']')
        {
            return kDdfReadCharReturnDefinitionStop;
        }
        else if ((epi::IsAlphanumericASCII(character)) || (character == '_') || (character == ':') ||
                 (character == '+'))
        {
            token += epi::ToUpperASCII(character);
            return kDdfReadCharReturnOK;
        }
        return kDdfReadCharReturnNothing;

    case kDdfReadStatusReadingCommand:
        if (character == '=')
        {
            return kDdfReadCharReturnCommand;
        }
        else if (character == ';')
        {
            return kDdfReadCharReturnProperty;
        }
        else if (character == '[')
        {
            return kDdfReadCharReturnDefinitionStart;
        }
        else if (epi::IsAlphanumericASCII(character) || character == '_' || character == '(' || character == ')' ||
                 character == '.')
        {
            token += epi::ToUpperASCII(character);
            return kDdfReadCharReturnOK;
        }
        return kDdfReadCharReturnNothing;

        // -ACB- 1998/08/10 Check for string start
    case kDdfReadStatusReadingData:
        if (character == '\"')
            return kDdfReadCharReturnStringStart;

        if (character == ';')
            return kDdfReadCharReturnTerminator;

        if (character == ',')
            return kDdfReadCharReturnSeparator;

        if (character == '(')
        {
            token += (character);
            return kDdfReadCharReturnGroupStart;
        }

        if (character == ')')
        {
            token += (character);
            return kDdfReadCharReturnGroupStop;
        }

        // Sprite Data - more than a few exceptions....
        if (epi::IsAlphanumericASCII(character) || character == '_' || character == '-' || character == ':' ||
            character == '.' || character == '[' || character == ']' || character == '\\' || character == '!' ||
            character == '#' || character == '%' || character == '+' || character == '@' || character == '?')
        {
            token += epi::ToUpperASCII(character);
            return kDdfReadCharReturnOK;
        }
        else if (epi::IsPrintASCII(character))
            DdfWarnError("DDF: Illegal character '%c' found.\n", character);

        break;

    case kDdfReadStatusReadingString: // -ACB- 1998/08/10 New string
                                      // handling
        // -KM- 1999/01/29 Fixed nasty bug where \" would be recognised as
        //  string end over quote mark.  One of the level text used this.
        if (formatchar)
        {
            // -ACB- 1998/08/11 Formatting check: Carriage-return.
            if (character == 'n')
            {
                token += ('\n');
                formatchar = false;
                return kDdfReadCharReturnOK;
            }
            else if (character == '\"') // -KM- 1998/10/29 Also recognise quote
            {
                token += ('\"');
                formatchar = false;
                return kDdfReadCharReturnOK;
            }
            else if (character == '\\') // -ACB- 1999/11/24 Double
                                        // backslash means directory
            {
                token += ('\\');
                formatchar = false;
                return kDdfReadCharReturnOK;
            }
            else // -ACB- 1999/11/24 Any other characters are treated in
                 // the norm
            {
                token += (character);
                formatchar = false;
                return kDdfReadCharReturnOK;
            }
        }
        else if (character == '\"')
        {
            return kDdfReadCharReturnStringStop;
        }
        else if (character == '\n')
        {
            cur_ddf_line_num--;
            DdfWarnError("Unclosed string detected.\n");

            cur_ddf_line_num++;
            return kDdfReadCharReturnNothing;
        }
        // -KM- 1998/10/29 Removed ascii check, allow foreign characters (?)
        // -ES- HEY! Swedish is not foreign!
        else
        {
            token += (character);
            return kDdfReadCharReturnOK;
        }

    default: // doh!
        FatalError("DdfMainProcessChar: INTERNAL ERROR: "
                   "Bad status value %d !\n",
                   status);
        break;
    }

    return kDdfReadCharReturnNothing;
}

//
// DdfMainReadFile
//
// -ACB- 1998/08/10 Added the string reading code
// -ACB- 1998/09/28 DdfReadFunction Localised here
// -AJA- 1999/10/02 Recursive { } comments.
// -ES- 2000/02/29 Added
//
void DdfMainReadFile(DDFReadInfo *readinfo, const std::string &data)
{
    std::string token;
    std::string current_cmd;

    char *name  = nullptr;
    char *value = nullptr;

    int current_index = 0;
    int entry_count   = 0;

#if (DDF_DEBUG_READ)
    char charcount = 0;
#endif

    int status       = kDdfReadStatusWaitingTag;
    int formerstatus = kDdfReadCharReturnNothing;

    int  comment_level = 0;
    int  bracket_level = 0;
    bool firstgo       = true;

    cur_ddf_line_num = 1;
    cur_ddf_filename = std::string(readinfo->lumpname);
    cur_ddf_entryname.clear();

    // WISH: don't make this copy, parse directly from the string
    char *memfile = new char[data.size() + 1];
    data.copy(memfile, std::string::npos);
    memfile[data.size()] = 0;

    char *memfileptr = memfile;
    int   memsize    = (int)data.size();

    // -ACB- 1998/09/12 Copy file to memory: Read until end. Speed optimisation.
    while (memfileptr < &memfile[memsize])
    {
        // -KM- 1998/12/16 Added #define command to ddf files.
        if (epi::StringPrefixCaseCompareASCII(std::string_view(memfileptr, 7), "#DEFINE") == 0)
        {
            bool line = false;

            memfileptr += 8;
            name = memfileptr;

            while (*memfileptr != ' ' && memfileptr < &memfile[memsize])
                memfileptr++;

            if (memfileptr < &memfile[memsize])
            {
                *memfileptr++ = 0;
                value         = memfileptr;
            }
            else
            {
                DdfError("#DEFINE '%s' as what?!\n", name);
            }

            // FIXME handle comments, stop at "//"

            while (memfileptr < &memfile[memsize])
            {
                if (*memfileptr == '\r')
                    *memfileptr = ' ';
                if (*memfileptr == '\\')
                    line = true;
                if (*memfileptr == '\n' && !line)
                    break;
                memfileptr++;
            }

            if (*memfileptr == '\n')
                cur_ddf_line_num++;

            *memfileptr++ = 0;

            DdfMainAddDefine(name, value);

            token.clear();
            continue;
        }

        // -AJA- 1999/10/27: Not the greatest place for it, but detect //
        //       comments here and ignore them.  Ow the pain of long
        //       identifier names...  Ow the pain of &memfile[size] :-)

        if (comment_level == 0 && status != kDdfReadStatusReadingString && memfileptr + 1 < &memfile[memsize] &&
            memfileptr[0] == '/' && memfileptr[1] == '/')
        {
            while (memfileptr < &memfile[memsize] && *memfileptr != '\n')
                memfileptr++;

            if (memfileptr >= &memfile[memsize])
                break;
        }

        char character = *memfileptr++;

        if (character == '\n')
        {
            int l_len;

            cur_ddf_line_num++;

            // -AJA- 2000/03/21: determine linedata.  Ouch.
            for (l_len = 0;
                 &memfileptr[l_len] < &memfile[memsize] && memfileptr[l_len] != '\n' && memfileptr[l_len] != '\r';
                 l_len++)
            {
            }

            cur_ddf_linedata = std::string(memfileptr, l_len);

            // -AJA- 2001/05/21: handle directives (lines beginning with #).
            // This code is more hackitude -- to be fixed when the whole
            // parsing code gets the overhaul it needs.

            if (epi::StringPrefixCaseCompareASCII(std::string_view(memfileptr, 9), "#CLEARALL") == 0)
            {
                if (!firstgo)
                    DdfError("#CLEARALL cannot be used inside an entry !\n");

                (*readinfo->clear_all)();

                memfileptr += l_len;
                continue;
            }

            if (epi::StringPrefixCaseCompareASCII(std::string_view(memfileptr, 8), "#VERSION") == 0)
            {
                // just ignore it
                memfileptr += l_len;
                continue;
            }
        }

        int response = DdfMainProcessChar(character, token, status);

        switch (response)
        {
        case kDdfReadCharReturnRemarkStart:
            if (comment_level == 0)
            {
                formerstatus = status;
                status       = kDdfReadStatusReadingRemark;
            }
            comment_level++;
            break;

        case kDdfReadCharReturnRemarkStop:
            comment_level--;
            if (comment_level == 0)
            {
                status = formerstatus;
            }
            break;

        case kDdfReadCharReturnCommand:
            if (!token.empty())
                current_cmd = token.c_str();
            else
                current_cmd.clear();

            EPI_ASSERT(current_index == 0);

            token.clear();
            status = kDdfReadStatusReadingData;
            break;

        case kDdfReadCharReturnTagStart:
            status = kDdfReadStatusReadingTag;
            break;

        case kDdfReadCharReturnTagStop:
            if (epi::StringCaseCompareASCII(token, readinfo->tag) != 0)
                DdfError("Start tag <%s> expected, found <%s>!\n", readinfo->tag, token.c_str());

            status = kDdfReadStatusWaitingNewDefinition;
            token.clear();
            break;

        case kDdfReadCharReturnDefinitionStart:
            if (bracket_level > 0)
                DdfError("Unclosed () brackets detected.\n");

            entry_count++;

            if (firstgo)
            {
                firstgo = false;
                status  = kDdfReadStatusReadingNewDefinition;
            }
            else
            {
                cur_ddf_linedata.clear();

                // finish off previous entry
                (*readinfo->finish_entry)();

                token.clear();

                status = kDdfReadStatusReadingNewDefinition;

                cur_ddf_entryname.clear();
            }
            break;

        case kDdfReadCharReturnDefinitionStop:
            cur_ddf_entryname = epi::StringFormat("[%s]", token.c_str());

            // -AJA- 2009/07/27: extend an existing entry
            if (token[0] == '+' && token[1] == '+')
                (*readinfo->start_entry)(token.c_str() + 2, true);
            else
                (*readinfo->start_entry)(token.c_str(), false);

            token.clear();
            status = kDdfReadStatusReadingCommand;
            break;

            // -AJA- 2000/10/02: support for () brackets
        case kDdfReadCharReturnGroupStart:
            if (status == kDdfReadStatusReadingData || status == kDdfReadStatusReadingCommand)
                bracket_level++;
            break;

        case kDdfReadCharReturnGroupStop:
            if (status == kDdfReadStatusReadingData || status == kDdfReadStatusReadingCommand)
            {
                bracket_level--;
                if (bracket_level < 0)
                    DdfError("Unexpected `)' bracket.\n");
            }
            break;

        case kDdfReadCharReturnSeparator:
            if (bracket_level > 0)
            {
                token += (',');
                break;
            }

            if (current_cmd.empty())
                DdfError("Unexpected comma `,'.\n");

            if (firstgo)
                DdfWarnError("Command %s used outside of any entry\n", current_cmd.c_str());
            else
            {
                (*readinfo->parse_field)(current_cmd.c_str(), DdfMainGetDefine(token.c_str()), current_index, false);
                current_index++;
            }

            token.clear();
            break;

            // -ACB- 1998/08/10 String Handling
        case kDdfReadCharReturnStringStart:
            status = kDdfReadStatusReadingString;
            break;

            // -ACB- 1998/08/10 String Handling
        case kDdfReadCharReturnStringStop:
            status = kDdfReadStatusReadingData;
            break;

        case kDdfReadCharReturnTerminator:
            if (current_cmd.empty())
                DdfError("Unexpected semicolon `;'.\n");

            if (bracket_level > 0)
                DdfError("Missing ')' bracket in ddf command.\n");

            (*readinfo->parse_field)(current_cmd.c_str(), DdfMainGetDefine(token.c_str()), current_index, true);
            current_index = 0;

            token.clear();
            status = kDdfReadStatusReadingCommand;
            break;

        case kDdfReadCharReturnProperty:
            DdfWarnError("Badly formed command: Unexpected semicolon `;'\n");
            break;

        case kDdfReadCharReturnNothing:
            break;

        case kDdfReadCharReturnOK:
#if (DDF_DEBUG_READ)
            charcount++;
            LogDebug("%c", character);
            if (charcount == 75)
            {
                charcount = 0;
                LogDebug("\n");
            }
#endif
            break;

        default:
            break;
        }
    }

    current_cmd.clear();
    cur_ddf_linedata.clear();

    // -AJA- 1999/10/21: check for unclosed comments
    if (comment_level > 0)
        DdfError("Unclosed comments detected.\n");

    if (bracket_level > 0)
        DdfError("Unclosed () brackets detected.\n");

    if (status == kDdfReadStatusReadingTag)
        DdfError("Unclosed <> brackets detected.\n");

    if (status == kDdfReadStatusReadingNewDefinition)
        DdfError("Unclosed [] brackets detected.\n");

    if (status == kDdfReadStatusReadingData || status == kDdfReadStatusReadingString)
        DdfWarnError("Unfinished DDF command on last line.\n");

    // if firstgo is true, nothing was defined
    if (!firstgo)
        (*readinfo->finish_entry)();

    delete[] memfile;

    cur_ddf_entryname.clear();
    cur_ddf_filename.clear();

    DdfMainFreeDefines();
}

//
// DdfMainGetNumeric
//
// Get numeric value directly from the file
//
void DdfMainGetNumeric(const char *info, void *storage)
{
    int *dest = (int *)storage;

    EPI_ASSERT(info && storage);

    if (epi::IsAlphaASCII(info[0]))
    {
        DdfWarnError("Bad numeric value: %s\n", info);
        return;
    }

    // -KM- 1999/01/29 strtol accepts hex and decimal.
    *dest = strtol(info, nullptr, 0); // straight conversion - no messin'
}

//
// DdfMainGetBoolean
//
// Get true/false from the file
//
// -KM- 1998/09/01 Gets a true/false value
//
void DdfMainGetBoolean(const char *info, void *storage)
{
    bool *dest = (bool *)storage;

    EPI_ASSERT(info && storage);

    if ((epi::StringCaseCompareASCII(info, "TRUE") == 0) || (epi::StringCaseCompareASCII(info, "1") == 0))
    {
        *dest = true;
        return;
    }

    if ((epi::StringCaseCompareASCII(info, "FALSE") == 0) || (epi::StringCaseCompareASCII(info, "0") == 0))
    {
        *dest = false;
        return;
    }

    DdfError("Bad boolean value: %s\n", info);
}

//
// DdfMainGetString
//
// Get String value directly from the file
//
void DdfMainGetString(const char *info, void *storage)
{
    std::string *dest = (std::string *)storage;

    EPI_ASSERT(info && storage);

    *dest = info;
}

//
// DdfMainParseField
//
// Check if the command exists, and call the parser function if it
// does (and return true), otherwise return false.
//
bool DdfMainParseField(const DDFCommandList *commands, const char *field, const char *contents, uint8_t *obj_base)
{
    EPI_ASSERT(obj_base);

    for (int i = 0; commands[i].name; i++)
    {
        const char *name = commands[i].name;

        if (name[0] == '!')
            name++;

        // handle subfields
        if (name[0] == '*')
        {
            name++;

            int len = strlen(name);
            EPI_ASSERT(len > 0);

            if (strncmp(field, name, len) == 0 && field[len] == '.' && epi::IsAlphanumericASCII(field[len + 1]))
            {
                // recursively parse the sub-field
                return DdfMainParseField(commands[i].sub_comms, field + len + 1, contents,
                                          obj_base + commands[i].offset);
            }

            continue;
        }

        if (DdfCompareName(field, name) != 0)
            continue;

        // found it, so call parse routine
        EPI_ASSERT(commands[i].parse_command);

        (*commands[i].parse_command)(contents, obj_base + commands[i].offset);

        return true;
    }

    return false;
}

void DdfMainGetLumpName(const char *info, void *storage)
{
    // Gets the string and checks the length is valid for a lump.

    EPI_ASSERT(info && storage);

    std::string *LN = (std::string *)storage;

    if (strlen(info) > 8)
        DdfDebug("Name %s too long for a lump; this is acceptable if referring to a "
                  "pack file or other special value.\n",
                  info);

    (*LN) = info;
}

void DdfMainRefAttack(const char *info, void *storage)
{
    AttackDefinition **dest = (AttackDefinition **)storage;

    EPI_ASSERT(info && storage);

    *dest = (AttackDefinition *)atkdefs.Lookup(info);
    if (*dest == nullptr)
        DdfWarnError("Unknown Attack: %s\n", info);
}

int DdfMainLookupDirector(const MapObjectDefinition *info, const char *ref)
{
    const char *p = strchr(ref, ':');

    int len = p ? (p - ref) : strlen(ref);

    if (len <= 0)
        DdfError("Bad Director `%s' : Nothing after divide\n", ref);

    std::string director(ref, len);

    int state  = DdfStateFindLabel(info->state_grp_, director.c_str());
    int offset = p ? HMM_MAX(0, atoi(p + 1) - 1) : 0;

    // FIXME: check for overflow
    return state + offset;
}

void DdfMainGetFloat(const char *info, void *storage)
{
    float *dest = (float *)storage;

    EPI_ASSERT(info && storage);

    if (strchr(info, '%') != nullptr)
    {
        DdfMainGetPercentAny(info, storage);
        return;
    }

    if (sscanf(info, "%f", dest) != 1)
        DdfError("Bad floating point value: %s\n", info);
}

// -AJA- 1999/09/11: Added DdfMainGetAngle and DdfMainGetSlope.

void DdfMainGetAngle(const char *info, void *storage)
{
    EPI_ASSERT(info && storage);

    BAMAngle *dest = (BAMAngle *)storage;

    float val;

    if (sscanf(info, "%f", &val) != 1)
        DdfError("Bad angle value: %s\n", info);

    *dest = epi::BAMFromDegrees(val);
}

void DdfMainGetSlope(const char *info, void *storage)
{
    float  val;
    float *dest = (float *)storage;

    EPI_ASSERT(info && storage);

    if (sscanf(info, "%f", &val) != 1)
        DdfError("Bad slope value: %s\n", info);

    if (val > +89.5f)
        val = +89.5f;
    if (val < -89.5f)
        val = -89.5f;

    *dest = tan(val * HMM_PI / 180.0);
}

static void DoGetFloat(const char *info, void *storage)
{
    float *dest = (float *)storage;

    EPI_ASSERT(info && storage);

    if (sscanf(info, "%f", dest) != 1)
        DdfError("Bad floating point value: %s\n", info);
}

//
// DdfMainGetPercent
//
// Reads percentages (0%..100%).
//
void DdfMainGetPercent(const char *info, void *storage)
{
    float *dest = (float *)storage;
    char   s[101];
    char  *p;
    float  f;

    // check that the string is valid
    epi::CStringCopyMax(s, info, 100);
    for (p = s; epi::IsDigitASCII(*p) || *p == '.'; p++)
    { /* do nothing */
    }

    // the number must be followed by %
    if (*p != '%')
    {
        DdfWarnError("Bad percent value '%s': Should be a number followed by %%\n", info);
        // -AJA- 2001/01/27: backwards compatibility
        DoGetFloat(s, &f);
        *dest = HMM_MAX(0, HMM_MIN(1, f));
        return;
    }

    *p = 0;

    DoGetFloat(s, &f);
    if (f < 0.0f || f > 100.0f)
        DdfError("Bad percent value '%s': Must be between 0%% and 100%%\n", s);

    *dest = f / 100.0f;
}

//
// DdfMainGetPercentAny
//
// Like the above routine, but allows percentages outside of the
// 0-100% range (which is useful in same instances).
//
void DdfMainGetPercentAny(const char *info, void *storage)
{
    float *dest = (float *)storage;
    char   s[101];
    char  *p;
    float  f;

    // check that the string is valid
    epi::CStringCopyMax(s, info, 100);
    for (p = s; epi::IsDigitASCII(*p) || *p == '.'; p++)
    { /* do nothing */
    }

    // the number must be followed by %
    if (*p != '%')
    {
        DdfWarnError("Bad percent value '%s': Should be a number followed by %%\n", info);
        // -AJA- 2001/01/27: backwards compatibility
        DoGetFloat(s, dest);
        return;
    }

    *p = 0;

    DoGetFloat(s, &f);

    *dest = f / 100.0f;
}

// -KM- 1998/09/27 You can end a number with T to specify tics; ie 35T
// means 35 tics while 3.5 means 3.5 seconds.

void DdfMainGetTime(const char *info, void *storage)
{
    float val;
    int  *dest = (int *)storage;

    EPI_ASSERT(info && storage);

    // -ES- 1999/09/14 MAXT means that time should be maximal.
    if (epi::StringCaseCompareASCII(info, "maxt") == 0)
    {
        *dest = INT_MAX; // -ACB- 1999/09/22 Standards, Please.
        return;
    }

    if (strchr(info, 'T'))
    {
        DdfMainGetNumeric(info, storage);
        return;
    }

    if (sscanf(info, "%f", &val) != 1)
        DdfError("Bad time value: %s\n", info);

    *dest = (int)(val * (float)kTicRate);
}

//
// DdfDummyFunction
//
void DdfDummyFunction(const char *info, void *storage)
{
    /* does nothing */
    (void)info;
    (void)storage;
}

//
// DdfMainGetColourmap
//
void DdfMainGetColourmap(const char *info, void *storage)
{
    const Colormap **result = (const Colormap **)storage;

    *result = colormaps.Lookup(info);
    if (*result == nullptr)
        DdfError("DdfMainGetColourmap: No such colourmap '%s'\n", info);
}

//
// DdfMainGetRGB
//
void DdfMainGetRGB(const char *info, void *storage)
{
    RGBAColor *result = (RGBAColor *)storage;
    int        r      = 0;
    int        g      = 0;
    int        b      = 0;

    EPI_ASSERT(info && storage);

    if (DdfCompareName(info, "NONE") == 0)
    {
        *result = kRGBANoValue;
        return;
    }

    if (sscanf(info, " #%2x%2x%2x ", &r, &g, &b) != 3)
        DdfError("Bad RGB colour value: %s\n", info);

    *result = epi::MakeRGBA((uint8_t)r, (uint8_t)g, (uint8_t)b);

    // silently change if matches the "none specified" value
    if (*result == kRGBANoValue)
        *result ^= 0x00010100;
}

//
// DdfMainGetWhenAppear
//
// Syntax:  [ '!' ]  [ SKILL ]  ':'  [ NETMODE ]
//
// SKILL = digit { ':' digit }  |  digit '-' digit.
// NETMODE = 'sp'  |  'coop'  |  'dm'.
//
// When no skill was specified, it's as though all were specified.
// Same for the netmode.
//
// -AJA- 2004/10/28: Dodgy-est crap ever, now with ranges and negation.
//
void DdfMainGetWhenAppear(const char *info, void *storage)
{
    AppearsFlag *result = (AppearsFlag *)storage;

    *result = kAppearsWhenNone;

    bool negate = (info[0] == '!');

    const char *range = strstr(info, "-");

    if (range)
    {
        if (range <= info || range[+1] == 0 || range[-1] < '1' || range[-1] > '5' || range[+1] < '1' ||
            range[+1] > '5' || range[-1] > range[+1])
        {
            DdfError("Bad range in WHEN_APPEAR value: %s\n", info);
            return;
        }

        for (char sk = '1'; sk <= '5'; sk++)
            if (range[-1] <= sk && sk <= range[+1])
                *result = (AppearsFlag)(*result | (kAppearsWhenSkillLevel1 << (sk - '1')));
    }
    else
    {
        if (strstr(info, "1"))
            *result = (AppearsFlag)(*result | kAppearsWhenSkillLevel1);

        if (strstr(info, "2"))
            *result = (AppearsFlag)(*result | kAppearsWhenSkillLevel2);

        if (strstr(info, "3"))
            *result = (AppearsFlag)(*result | kAppearsWhenSkillLevel3);

        if (strstr(info, "4"))
            *result = (AppearsFlag)(*result | kAppearsWhenSkillLevel4);

        if (strstr(info, "5"))
            *result = (AppearsFlag)(*result | kAppearsWhenSkillLevel5);
    }

    if (strstr(info, "SP") || strstr(info, "sp"))
        *result = (AppearsFlag)(*result | kAppearsWhenSingle);

    if (strstr(info, "COOP") || strstr(info, "coop"))
        *result = (AppearsFlag)(*result | kAppearsWhenCoop);

    if (strstr(info, "DM") || strstr(info, "dm"))
        *result = (AppearsFlag)(*result | kAppearsWhenDeathMatch);

    // allow more human readable strings...

    if (negate)
        *result = (AppearsFlag)(*result ^ (kAppearsWhenSkillBits | kAppearsWhenNetBits));

    if ((*result & kAppearsWhenSkillBits) == 0)
        *result = (AppearsFlag)(*result | kAppearsWhenSkillBits);

    if ((*result & kAppearsWhenNetBits) == 0)
        *result = (AppearsFlag)(*result | kAppearsWhenNetBits);
}

//
// DdfMainGetBitSet
//
void DdfMainGetBitSet(const char *info, void *storage)
{
    BitSet *result = (BitSet *)storage;
    int     start, end;

    EPI_ASSERT(info && storage);

    // allow a numeric value
    if (sscanf(info, " %i ", result) == 1)
        return;

    *result = 0;

    for (; *info; info++)
    {
        if (*info < 'A' || *info > 'Z')
            continue;

        start = end = (*info) - 'A';

        // handle ranges
        if (info[1] == '-' && 'A' <= info[2] && info[2] <= 'Z' && info[2] >= info[0])
        {
            end = info[2] - 'A';
        }

        for (; start <= end; start++)
            (*result) |= (1 << start);
    }
}

static int FindSpecialFlag(const char *prefix, const char *name, const DDFSpecialFlags *flag_set)
{
    int  i;
    char try_name[512];

    for (i = 0; flag_set[i].name; i++)
    {
        const char *current = flag_set[i].name;

        if (current[0] == '!')
            current++;

        sprintf(try_name, "%s%s", prefix, current);

        if (DdfCompareName(name, try_name) == 0)
            return i;
    }

    return -1;
}

DDFCheckFlagResult DdfMainCheckSpecialFlag(const char *name, const DDFSpecialFlags *flag_set, int *flag_value,
                                            bool allow_prefixes, bool allow_user)
{
    int index;
    int negate = 0;
    int user   = 0;

    // try plain name...
    index = FindSpecialFlag("", name, flag_set);

    if (allow_prefixes)
    {
        // try name with ENABLE_ prefix...
        if (index == -1)
        {
            index = FindSpecialFlag("ENABLE_", name, flag_set);
        }

        // try name with NO_ prefix...
        if (index == -1)
        {
            negate = 1;
            index  = FindSpecialFlag("NO_", name, flag_set);
        }

        // try name with NOT_ prefix...
        if (index == -1)
        {
            negate = 1;
            index  = FindSpecialFlag("NOT_", name, flag_set);
        }

        // try name with DISABLE_ prefix...
        if (index == -1)
        {
            negate = 1;
            index  = FindSpecialFlag("DISABLE_", name, flag_set);
        }

        // try name with USER_ prefix...
        if (index == -1 && allow_user)
        {
            user   = 1;
            negate = 0;
            index  = FindSpecialFlag("USER_", name, flag_set);
        }
    }

    if (index < 0)
        return kDdfCheckFlagUnknown;

    (*flag_value) = flag_set[index].flags;

    if (flag_set[index].negative)
        negate = !negate;

    if (user)
        return kDdfCheckFlagUser;

    if (negate)
        return kDdfCheckFlagNegative;

    return kDdfCheckFlagPositive;
}

//
// DdfDecodeBrackets
//
// Decode a keyword followed by something in () brackets.  Buf_len gives
// the maximum size of the output buffers.  The outer keyword is required
// to be non-empty, though the inside can be empty.  Returns false if
// cannot be parsed (e.g. no brackets).  Handles strings.
//
bool DdfMainDecodeBrackets(const char *info, char *outer, char *inner, int buf_len)
{
    const char *pos = info;

    while (*pos && *pos != '(')
        pos++;

    if (*pos == 0 || pos == info)
        return false;

    if (pos - info >= buf_len) // overflow
        return false;

    strncpy(outer, info, pos - info);
    outer[pos - info] = 0;

    pos++; // skip the '('

    info = pos;

    bool in_string = false;

    while (*pos && (in_string || *pos != ')'))
    {
        // handle escaped quotes
        if (pos[0] == '\\' && pos[1] == '"')
        {
            pos += 2;
            continue;
        }

        if (*pos == '"')
            in_string = !in_string;

        pos++;
    }

    if (*pos == 0)
        return false;

    if (pos - info >= buf_len) // overflow
        return false;

    strncpy(inner, info, pos - info);
    inner[pos - info] = 0;

    return true;
}

//
// DdfMainDecodeList
//
// Find the dividing character.  Returns nullptr if not found.
// Handles strings and brackets unless simple is true.
//
const char *DdfMainDecodeList(const char *info, char divider, bool simple)
{
    int  brackets  = 0;
    bool in_string = false;

    const char *pos = info;

    for (;;)
    {
        if (*pos == 0)
            break;

        if (brackets == 0 && !in_string && *pos == divider)
            return pos;

        // handle escaped quotes
        if (!simple)
        {
            if (pos[0] == '\\' && pos[1] == '"')
            {
                pos += 2;
                continue;
            }

            if (*pos == '"')
                in_string = !in_string;

            if (!in_string && *pos == '(')
                brackets++;

            if (!in_string && *pos == ')')
            {
                brackets--;
                if (brackets < 0)
                    DdfError("Too many ')' found: %s\n", info);
            }
        }

        pos++;
    }

    if (in_string)
        DdfError("Unterminated string found: %s\n", info);

    if (brackets != 0)
        DdfError("Unclosed brackets found: %s\n", info);

    return nullptr;
}

// DDF OBJECTS

// ---> mobj_strref class

const MapObjectDefinition *MobjStringReference::GetRef()
{
    if (def_)
        return def_;

    def_ = mobjtypes.Lookup(name_.c_str());

    return def_;
}

// ---> damage class

//
// DamageClass Constructor
//
DamageClass::DamageClass()
{
}

//
// DamageClass Copy constructor
//
DamageClass::DamageClass(DamageClass &rhs)
{
    Copy(rhs);
}

//
// DamageClass Destructor
//
DamageClass::~DamageClass()
{
}

//
// DamageClass::Copy
//
void DamageClass::Copy(DamageClass &src)
{
    nominal_    = src.nominal_;
    linear_max_ = src.linear_max_;
    error_      = src.error_;
    delay_      = src.delay_;

    obituary_ = src.obituary_;
    pain_     = src.pain_;
    death_    = src.death_;
    overkill_ = src.overkill_;

    no_armour_           = src.no_armour_;
    damage_flash_colour_ = src.damage_flash_colour_;

    bypass_all_ = src.bypass_all_;
    instakill_  = src.instakill_;
    if (src.damage_unless_)
    {
        damage_unless_  = new Benefit;
        *damage_unless_ = *src.damage_unless_;
    }
    if (src.damage_if_)
    {
        damage_if_  = new Benefit;
        *damage_if_ = *src.damage_if_;
    }
    grounded_monsters_ = src.grounded_monsters_;
    all_players_       = src.all_players_;
}

//
// DamageClass::Default
//
void DamageClass::Default(DamageClassDefault def)
{
    obituary_.clear();

    switch (def)
    {
    case kDamageClassDefaultMobjChoke: {
        nominal_             = 6.0f;
        linear_max_          = 14.0f;
        error_               = -1.0f;
        delay_               = 2 * kTicRate;
        obituary_            = "OB_DROWN";
        no_armour_           = true;
        bypass_all_          = false;
        instakill_           = false;
        damage_unless_       = nullptr;
        damage_if_           = nullptr;
        grounded_monsters_   = false;
        damage_flash_colour_ = SG_RED_RGBA32;
        all_players_         = false;
        break;
    }

    case kDamageClassDefaultSector: {
        nominal_             = 0.0f;
        linear_max_          = -1.0f;
        error_               = -1.0f;
        delay_               = 31;
        no_armour_           = false;
        bypass_all_          = false;
        instakill_           = false;
        damage_unless_       = nullptr;
        damage_if_           = nullptr;
        grounded_monsters_   = false;
        damage_flash_colour_ = SG_RED_RGBA32;
        all_players_         = false;
        break;
    }

    case kDamageClassDefaultAttack:
    case kDamageClassDefaultMobj:
    default: {
        nominal_             = 0.0f;
        linear_max_          = -1.0f;
        error_               = -1.0f;
        delay_               = 0;
        no_armour_           = false;
        bypass_all_          = false;
        instakill_           = false;
        damage_unless_       = nullptr;
        damage_if_           = nullptr;
        grounded_monsters_   = false;
        damage_flash_colour_ = SG_RED_RGBA32;
        all_players_         = false;
        break;
    }
    }

    pain_.Default();
    death_.Default();
    overkill_.Default();
}

//
// DamageClass assignment operator
//
DamageClass &DamageClass::operator=(DamageClass &rhs)
{
    if (&rhs != this)
        Copy(rhs);

    return *this;
}

// ---> label offset class

//
// LabelOffset Constructor
//
LabelOffset::LabelOffset()
{
    offset_ = 0;
}

//
// LabelOffset Copy constructor
//
LabelOffset::LabelOffset(LabelOffset &rhs)
{
    Copy(rhs);
}

//
// LabelOffset Destructor
//
LabelOffset::~LabelOffset()
{
}

//
// LabelOffset::Copy
//
void LabelOffset::Copy(LabelOffset &src)
{
    label_  = src.label_;
    offset_ = src.offset_;
}

//
// LabelOffset::Default
//
void LabelOffset::Default()
{
    label_.clear();
    offset_ = 0;
}

//
// LabelOffset assignment operator
//
LabelOffset &LabelOffset::operator=(LabelOffset &rhs)
{
    if (&rhs != this)
        Copy(rhs);

    return *this;
}

// ---> dlight_info class

DynamicLightDefinition::DynamicLightDefinition()
{
    Default();
}

DynamicLightDefinition::DynamicLightDefinition(DynamicLightDefinition &rhs)
{
    Copy(rhs);
}

void DynamicLightDefinition::Copy(DynamicLightDefinition &src)
{
    type_   = src.type_;
    shape_  = src.shape_;
    radius_ = src.radius_;
    colour_ = src.colour_;
    height_ = src.height_;
    leaky_  = src.leaky_;

    cache_data_ = nullptr;
}

void DynamicLightDefinition::Default()
{
    type_   = kDynamicLightTypeNone;
    radius_ = 32;
    colour_ = SG_WHITE_RGBA32;
    height_ = 0.5f;
    leaky_  = false;
    shape_  = "DLIGHT_EXP";

    cache_data_ = nullptr;
}

DynamicLightDefinition &DynamicLightDefinition::operator=(DynamicLightDefinition &rhs)
{
    if (this == &rhs)
        return *this;

    Copy(rhs);

    return *this;
}

// ---> weakness_info class

WeaknessDefinition::WeaknessDefinition()
{
    Default();
}

WeaknessDefinition::WeaknessDefinition(WeaknessDefinition &rhs)
{
    Copy(rhs);
}

void WeaknessDefinition::Copy(WeaknessDefinition &src)
{
    height_[0] = src.height_[0];
    height_[1] = src.height_[1];
    angle_[0]  = src.angle_[0];
    angle_[1]  = src.angle_[1];

    classes_    = src.classes_;
    multiply_   = src.multiply_;
    painchance_ = src.painchance_;
}

void WeaknessDefinition::Default()
{
    height_[0] = 0.0f;
    height_[1] = 1.0f;

    angle_[0] = kBAMAngle0;
    angle_[1] = kBAMAngle360;

    classes_    = 0;
    multiply_   = 2.5;
    painchance_ = -1; // disabled
}

WeaknessDefinition &WeaknessDefinition::operator=(WeaknessDefinition &rhs)
{
    if (this == &rhs)
        return *this;

    Copy(rhs);

    return *this;
}

//----------------------------------------------------------------------------

static std::vector<DdfFile> unread_ddf;

struct ddf_reader_t
{
    DdfType     type;
    const char *lump_name;
    const char *pack_name;
    const char *print_name;
    void (*func)(const std::string &data);
};

// -KM- 1999/01/31 Order is important, Languages are loaded before sfx, etc...
static ddf_reader_t ddf_readers[kTotalDdfTypes] = {
    {kDdfTypeLanguage, "DDFLANG", "language.ldf", "Languages", DdfReadLangs},
    {kDdfTypeSFX, "DDFSFX", "sounds.ddf", "Sounds", DdfReadSFX},
    {kDdfTypeColourMap, "DDFCOLM", "colmap.ddf", "ColourMaps", DdfReadColourMaps},
    {kDdfTypeImage, "DDFIMAGE", "images.ddf", "Images", DdfReadImages},
    {kDdfTypeFont, "DDFFONT", "fonts.ddf", "Fonts", DdfReadFonts},
    {kDdfTypeStyle, "DDFSTYLE", "styles.ddf", "Styles", DdfReadStyles},
    {kDdfTypeAttack, "DDFATK", "attacks.ddf", "Attacks", DdfReadAtks},
    {kDdfTypeWeapon, "DDFWEAP", "weapons.ddf", "Weapons", DdfReadWeapons},
    {kDdfTypeThing, "DDFTHING", "things.ddf", "Things", DdfReadThings},

    {kDdfTypePlaylist, "DDFPLAY", "playlist.ddf", "Playlists", DdfReadMusicPlaylist},
    {kDdfTypeLine, "DDFLINE", "lines.ddf", "Lines", DdfReadLines},
    {kDdfTypeSector, "DDFSECT", "sectors.ddf", "Sectors", DdfReadSectors},
    {kDdfTypeSwitch, "DDFSWTH", "switch.ddf", "Switches", DdfReadSwitch},
    {kDdfTypeAnim, "DDFANIM", "anims.ddf", "Anims", DdfReadAnims},
    {kDdfTypeGame, "DDFGAME", "games.ddf", "Games", DdfReadGames},
    {kDdfTypeLevel, "DDFLEVL", "levels.ddf", "Levels", DdfReadLevels},
    {kDdfTypeFlat, "DDFFLAT", "flats.ddf", "Flats", DdfReadFlat},
    {kDdfTypeMovie, "DDFMOVIE", "movies.ddf", "Movies", DdfReadMovies},

    // RTS scripts are handled differently
    {kDdfTypeRadScript, "RSCRIPT", "rscript.rts", "RadTrig", nullptr}};

DdfType DdfLumpToType(const std::string &name)
{
    std::string up_name(name);
    epi::StringUpperASCII(up_name);

    for (size_t i = 0; i < kTotalDdfTypes; i++)
        if (up_name == ddf_readers[i].lump_name)
            return ddf_readers[i].type;

    return kDdfTypeUnknown;
}

DdfType DdfFilenameToType(const std::string &path)
{
    std::string check = epi::GetExtension(path);

    if (epi::StringCaseCompareASCII(check, ".rts") == 0)
        return kDdfTypeRadScript;

    check = epi::GetFilename(path);

    std::string stem = epi::GetStem(check);

    for (size_t i = 0; i < kTotalDdfTypes; i++)
        if (epi::StringCaseCompareASCII(check, ddf_readers[i].pack_name) == 0 ||
            epi::StringCaseCompareASCII(stem, ddf_readers[i].lump_name) == 0)
            return ddf_readers[i].type;

    return kDdfTypeUnknown;
}

void DdfAddFile(DdfType type, std::string &data, const std::string &source)
{
    unread_ddf.push_back({type, source, ""});

    // transfer the caller's data
    unread_ddf.back().data.swap(data);
}

void DdfAddCollection(std::vector<DdfFile> &col, const std::string &source)
{
    for (DdfFile &it : col)
        DdfAddFile(it.type, it.data, source);
}

void DdfDumpFile(const std::string &data)
{
    LogDebug("\n");

    // we need to break it into lines
    std::string line;

    size_t pos = 0;

    while (pos < data.size())
    {
        line += data[pos];
        pos += 1;

        if (data[pos] == '\n')
        {
            LogDebug("%s", line.c_str());
            line.clear();
        }
    }

    if (line.size() > 0)
        LogDebug("%s", line.c_str());
}

void DdfDumpCollection(const std::vector<DdfFile> &col)
{
    for (const DdfFile &it : col)
        DdfDumpFile(it.data);
}

static void DdfParseUnreadFile(size_t d)
{
    for (DdfFile &it : unread_ddf)
    {
        if (it.type == ddf_readers[d].type)
        {
            LogPrint("Parsing %s from: %s\n", ddf_readers[d].lump_name, it.source.c_str());

            if (it.type == kDdfTypeRadScript)
            {
                ReadTriggerScript(it.data, it.source);
            }
            else
            {
                // FIXME store `source` in cur_ddf_filename (or so)

                (*ddf_readers[d].func)(it.data);
            }

            // can free the memory now
            it.data.clear();
        }
    }
}

void DdfParseEverything()
{
    // -AJA- Since DDF files have dependencies between them, it makes most
    //       sense to load all lumps of a certain type together, for example
    //       all DDFSFX lumps before all the DDFTHING lumps.

    for (size_t d = 0; d < kTotalDdfTypes; d++)
        DdfParseUnreadFile(d);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
