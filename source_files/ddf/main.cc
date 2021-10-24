//----------------------------------------------------------------------------
//  EDGE Data Definition Files Code (Main)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
//           
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#include "local.h"

#include <limits.h>
#include <vector>

#include "path.h"
#include "str_format.h"

#include "colormap.h"

#include "p_action.h"


// FIXME: unwanted link to engine code (switch to epi::angle_c)
extern float M_Tan(angle_t ang)  GCCATTR((const));


#define DEBUG_DDFREAD  0

static int engine_version;
static std::string ddf_where;

bool strict_errors = false;
bool lax_errors = false;
bool no_warnings = false;


//
// DDF_Error
//
// -AJA- 1999/10/27: written.
//
int cur_ddf_line_num;
std::string cur_ddf_filename;
std::string cur_ddf_entryname;
std::string cur_ddf_linedata;

void DDF_Error(const char *err, ...)
{
	va_list argptr;
	char buffer[2048];
	char *pos;

	buffer[2047] = 0;

	// put actual error message on first line
	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);
 
	pos = buffer + strlen(buffer);

	if (!cur_ddf_filename.empty())
	{
		sprintf(pos, "Error occurred near line %d of %s\n", 
				cur_ddf_line_num, cur_ddf_filename.c_str());
				
		pos += strlen(pos);
	}

	if (!cur_ddf_entryname.empty())
	{
		sprintf(pos, "Error occurred in entry: %s\n", 
				cur_ddf_entryname.c_str());
				
		pos += strlen(pos);
	}

	if (!cur_ddf_linedata.empty())
	{
		sprintf(pos, "Line contents: %s\n", 
				cur_ddf_linedata.c_str());
				
		pos += strlen(pos);
	}

	// check for buffer overflow
	if (buffer[2047] != 0)
		I_Error("Buffer overflow in DDF_Error\n");
  
	// add a blank line for readability under DOS/Linux.
	I_Printf("\n");
 
	I_Error("%s", buffer);
}

void DDF_Warning(const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	if (no_warnings)
		return;

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	I_Warning("%s", buffer);

	if (!cur_ddf_filename.empty())
	{
		I_Printf("  problem occurred near line %d of %s\n", 
				  cur_ddf_line_num, cur_ddf_filename.c_str());
	}

	if (!cur_ddf_entryname.empty())
	{
		I_Printf("  problem occurred in entry: %s\n", 
				  cur_ddf_entryname.c_str());
	}
	
	if (!cur_ddf_linedata.empty())
	{
		I_Printf("  with line contents: %s\n", 
				  cur_ddf_linedata.c_str());
	}

///	I_Printf("\n");
}

void DDF_WarnError(const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	if (strict_errors)
		DDF_Error("%s", buffer);
	else
		DDF_Warning("%s", buffer);
}


void DDF_Init(int _engine_ver)
{
	engine_version = _engine_ver;

	DDF_StateInit();
	DDF_LanguageInit();
	DDF_SFXInit();
	DDF_ColmapInit();
	DDF_ImageInit();
	DDF_FontInit();
	DDF_StyleInit();
	DDF_AttackInit(); 
	DDF_WeaponInit();
	DDF_MobjInit();
	DDF_LinedefInit();
	DDF_SectorInit();
	DDF_SwitchInit();
	DDF_AnimInit();
	DDF_GameInit();
	DDF_LevelInit();
	DDF_MusicPlaylistInit();
}

void DDF_SetWhere(const std::string& dir)
{
	ddf_where = dir;
}


class define_c
{
public:
	// Note: these pointers only point inside the currently
	//       loaded memfile.  Hence there is no need to
	//       explicitly free them.
	char *name;
	char *value;

public:
	define_c() : name(NULL), value(NULL)
	{ }

	define_c(char *_N, char *_V) : name(_N), value(_V)
	{ }

	~define_c()
	{ }
};

// -AJA- 1999/09/12: Made these static.  The variable `defines' was
//       clashing with the one in rad_trig.c.  Ugh.

static std::vector<define_c> defines;

static void DDF_MainAddDefine(char *name, char *value)
{
	for (int i = 0; i < (int)defines.size(); i++)
	{
		if (stricmp(defines[i].name, name) == 0)
		{
			DDF_Error("Redefinition of '%s'\n", name);
			return;
		}
	}

	defines.push_back(define_c(name, value));
}

static const char *DDF_MainGetDefine(const char *name)
{
	for (int i = 0; i < (int)defines.size(); i++)
	{
		if (stricmp(defines[i].name, name) == 0)
			return defines[i].value;
	}
	return name; // un-defined.
}

//
// DDF_MainCleanup
//
// This goes through the information loaded via DDF and matchs any
// info stored as references.
//
void DDF_CleanUp(void)
{
	DDF_LanguageCleanUp();
	DDF_ImageCleanUp();
	DDF_FontCleanUp();
	DDF_StyleCleanUp();
	DDF_MobjCleanUp();
	DDF_AttackCleanUp();
	DDF_StateCleanUp();
	DDF_LinedefCleanUp();
	DDF_SFXCleanUp();
	DDF_ColmapCleanUp();
	DDF_WeaponCleanUp();
	DDF_SectorCleanUp();
	DDF_SwitchCleanUp();
	DDF_AnimCleanUp();
	DDF_GameCleanUp();
	DDF_LevelCleanUp();
	DDF_MusicPlaylistCleanUp();
}

static const char *tag_conversion_table[] =
{
    "ANIMATIONS",  "DDFANIM",
    "ATTACKS",     "DDFATK",
    "COLOURMAPS",  "DDFCOLM",
    "FONTS",       "DDFFONT",
    "GAMES",       "DDFGAME",
    "IMAGES",      "DDFIMAGE",
    "LANGUAGES",   "DDFLANG",
    "LEVELS",      "DDFLEVL",
    "LINES",       "DDFLINE",
    "PLAYLISTS",   "DDFPLAY",
    "SECTORS",     "DDFSECT",
    "SOUNDS",      "DDFSFX",
    "STYLES",      "DDFSTYLE",
    "SWITCHES",    "DDFSWTH",
    "THINGS",      "DDFTHING",
    "WEAPONS",     "DDFWEAP",

	NULL, NULL
};

void DDF_GetLumpNameForFile(const char *filename, char *lumpname)
{
	FILE *fp = fopen(filename, "r");
	
	if (!fp)
		I_Error("Couldn't open DDF file: %s\n", filename);

	bool in_comment = false;

	for (;;)
	{
		int ch = fgetc(fp);

		if (ch == EOF || ferror(fp))
			break;

		if (ch == '/' || ch == '#')  // skip directives too
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
		int len = 0;

		for (;;)
		{
			ch = fgetc(fp);

			if (ch == EOF || ferror(fp) || ch == '>')
				break;

			tag_buf[len++] = toupper(ch);

			if (len+2 >= (int)sizeof(tag_buf))
				break;
		}

		tag_buf[len] = 0;

		if (len > 0)
		{
			for (int i = 0; tag_conversion_table[i]; i += 2)
			{
				if (strcmp(tag_buf, tag_conversion_table[i]) == 0)
				{
					strcpy(lumpname, tag_conversion_table[i+1]);
					fclose(fp);

					return;  // SUCCESS!
				}
			}

			fclose(fp);
			I_Error("Unknown marker <%s> in DDF file: %s\n", tag_buf, filename);
		}
		break;
	}

	fclose(fp);
	I_Error("Missing <..> marker in DDF file: %s\n", filename);
}

// -KM- 1998/12/16 This loads the ddf file into memory for parsing.
// -AJA- Returns NULL if no such file exists (with a warning).

static void *DDF_MainCacheFile(readinfo_t * readinfo)
{
	FILE *file;
	char *memfile;
	size_t size;

	if (!readinfo->filename)
		I_Error("DDF_MainReadFile: No file to read\n");

	std::string filename(epi::PATH_Join(ddf_where.c_str(), readinfo->filename));

	file = fopen(filename.c_str(), "rb");
	if (file == NULL)
	{
		I_Warning("DDF_MainReadFile: Unable to open: '%s'\n", filename.c_str());
		return NULL;
	}

#if (DEBUG_DDFREAD)
	I_Debugf("\nDDF Parser Output:\n");
#endif

	// get to the end of the file
	fseek(file, 0, SEEK_END);

	// get the size
	size = ftell(file);

	// reset to beginning
	fseek(file, 0, SEEK_SET);

	// malloc the size
	memfile = new char[size + 1];

	fread(memfile, sizeof(char), size, file);
	memfile[size] = 0;

	// close the file
	fclose(file);

	readinfo->memsize = size;
	return (void *)memfile;
}

static void DDF_ParseVersion(const char *str, int len)
{
	if (len <= 0 || ! isspace(*str))
		DDF_Error("Badly formed #VERSION directive.\n");
	
	for (; isspace(*str) && len > 0; str++, len--)
	{ }

	if (len < 4 || ! isdigit(str[0]) || str[1] != '.' ||
		! isdigit(str[2]) || ! isdigit(str[3]))
	{
		DDF_Error("Badly formed #VERSION directive.\n");
	}

	int ddf_version = ((str[0] - '0') * 100) |
	                  ((str[2] - '0') *  10) |
				       (str[3] - '0');

	if (ddf_version < 123)
		DDF_Error("Illegal #VERSION number: %s\n", str);

	/*
	if (ddf_version > engine_version)
		DDF_Error("This version of EDGE cannot handle this DDF.\n");
	
	*/
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
// When the parser function is called, a pointer to a readinfo_t is passed and
// contains all the info needed, it contains:
//
// * filename              - filename to be read, returns error if NULL
// * DDF_MainCheckName     - function called when a def has been just been started
// * DDF_MainCheckCmd      - function called when we need to check a command
// * DDF_MainCreateEntry   - function called when a def has been completed
// * DDF_MainFinishingCode - function called when EOF is read
// * currentcmdlist        - Current list of commands
//
// Also when commands are referenced, they use currentcmdlist, which is a pointer
// to a list of entries, the entries are formatted like this:
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
//  waiting_newdef
//  reading_newdef
//  reading_command
//  reading_data
//  reading_remark
//  reading_string
//
// 'waiting_newdef' is only set at the start of the code, At this point every
// character with the exception of DEFSTART is ignored. When DEFSTART is
// encounted, the parser will switch to reading_newdef. DEFSTART the parser
// will only switches modes and sets firstgo to false.
//
// 'reading_newdef' reads all alphanumeric characters and the '_' character - which
// substitudes for a space character (whitespace is ignored) - until DEFSTOP is read.
// DEFSTOP passes the read string to DDF_MainCheckName and then clears the string.
// Mode reading_command is now set. All read stuff is passed to char *buffer.
//
// 'reading_command' picks out all the alphabetic characters and passes them to
// buffer as soon as COMMANDREAD is encountered; DDF_MainReadCmd looks through
// for a matching command, if none is found a fatal error is returned. If a matching
// command is found, this function returns a command reference number to command ref
// and sets the mode to reading_data. if DEFSTART is encountered the procedure will
// clear the buffer, run DDF_MainCreateEntry (called this as it reflects that in Items
// & Scenery if starts a new mobj type, in truth it can do anything procedure wise) and
// then switch mode to reading_newdef.
//
// 'reading_data' passes alphanumeric characters, plus a few other characters that
// are also needed. It continues to feed buffer until a SEPARATOR or a TERMINATOR is
// found. The difference between SEPARATOR and TERMINATOR is that a TERMINATOR refs
// the cmdlist to find the routine to use and then sets the mode to reading_command,
// whereas SEPARATOR refs the cmdlist to find the routine and a looks for more data
// on the same command. This is how the multiple states and specials are defined.
//
// 'reading_remark' does not process any chars except REMARKSTOP, everything else is
// ignored. This mode is only set when REMARKSTART is found, when this happens the
// current mode is held in formerstatus, which is restored when REMARKSTOP is found.
//
// 'reading_string' is set when the parser is going through data (reading_data) and
// encounters STRINGSTART and only stops on a STRINGSTOP. When reading_string,
// everything that is an ASCII char is read (which the exception of STRINGSTOP) and
// passed to the buffer. REMARKS are ignored in when reading_string and the case is
// take notice of here.
//
// The maximum size of BUFFER is set in the BUFFERSIZE define.
//
// DDF_MainReadFile & DDF_MainProcessChar handle the main processing of the file, all
// the procedures in the other DDF files (which the exceptions of the Inits) are
// called directly or indirectly. DDF_MainReadFile handles to opening, closing and
// calling of procedures, DDF_MainProcessChar makes sense from the character read
// from the file.
//

//
// DDF_MainProcessChar
//
// 1998/08/10 Added String reading code.
//
static readchar_t DDF_MainProcessChar(char character, std::string& token, int status)
{
	//int len;

	// -ACB- 1998/08/11 Used for detecting formatting in a string
	static bool formatchar = false;

	// With the exception of reading_string, whitespace is ignored.
	if (status != reading_string)
	{
		if (isspace(character))
			return nothing;
	}
	else  // check for formatting char in a string
	{
		if (!formatchar && character == '\\')
		{
			formatchar = true;
			return nothing;
		}
	}

	// -AJA- 1999/09/26: Handle unmatched '}' better.
	if (status != reading_string && character == '{')
		return remark_start;
  
	if (status == reading_remark && character == '}')
		return remark_stop;

	if (status != reading_string && character == '}')
		DDF_Error("DDF: Encountered '}' without previous '{'.\n");

	switch (status)
	{
		case reading_remark:
			return nothing;

			// -ES- 2000/02/29 Added tag check.
		case waiting_tag:
			if (character == '<')
				return tag_start;
			else
				DDF_Error("DDF: File must start with a tag!\n");
			break;

		case reading_tag:
			if (character == '>')
				return tag_stop;
			else
			{
				token += (character);
				return ok_char;
			}

		case waiting_newdef:
			if (character == '[')
				return def_start;
			else
				return nothing;

		case reading_newdef:
			if (character == ']')
			{
				return def_stop;
			}
			else if ((isalnum(character)) || (character == '_') ||
					 (character == ':')   || (character == '+'))
			{
				token += toupper(character);
				return ok_char;
			}
			return nothing;

		case reading_command:
			if (character == '=')
			{
				return command_read;
			}
			else if (character == ';')
			{
				return property_read;
			}
			else if (character == '[')
			{
				return def_start;
			}
			else if (isalnum(character) || character == '_' ||
					 character == '(' || character == ')' ||
					 character == '.')
			{
				token += toupper(character);
				return ok_char;
			}
			return nothing;

			// -ACB- 1998/08/10 Check for string start
		case reading_data:
			if (character == '\"')
				return string_start;
      
			if (character == ';')
				return terminator;
      
			if (character == ',')
				return separator;
      
			if (character == '(')
			{
				token += (character);
				return group_start;
			}
      
			if (character == ')')
			{
				token += (character);
				return group_stop;
			}
      
			// Sprite Data - more than a few exceptions....
			if (isalnum(character) || character == '_' || character == '-' ||
				character == ':' || character == '.'  || character == '[' ||
				character == ']' || character == '\\' || character == '!' ||
				character == '#' || character == '%'  || character == '+' ||
				character == '@' || character == '?')
			{
				token += toupper(character);
				return ok_char;
			}
			else if (isprint(character))
				DDF_WarnError("DDF: Illegal character '%c' found.\n", character);

			break;

		case reading_string:  // -ACB- 1998/08/10 New string handling
			// -KM- 1999/01/29 Fixed nasty bug where \" would be recognised as
			//  string end over quote mark.  One of the level text used this.
			if (formatchar)
			{
				// -ACB- 1998/08/11 Formatting check: Carriage-return.
				if (character == 'n')
				{
					token += ('\n');
					formatchar = false;
					return ok_char;
				}
				else if (character == '\"')    // -KM- 1998/10/29 Also recognise quote
				{
					token += ('\"');
					formatchar = false;
					return ok_char;
				}
				else if (character == '\\') // -ACB- 1999/11/24 Double backslash means directory
				{
					token += ('\\');
					formatchar = false;
					return ok_char;
				}
				else // -ACB- 1999/11/24 Any other characters are treated in the norm
				{
					token += (character);
					formatchar = false;
					return ok_char;
				}

			}
			else if (character == '\"')
			{
				return string_stop;
			}
			else if (character == '\n')
			{
				cur_ddf_line_num--;
				DDF_WarnError("Unclosed string detected.\n");

				cur_ddf_line_num++;
				return nothing;
			}
			// -KM- 1998/10/29 Removed ascii check, allow foreign characters (?)
			// -ES- HEY! Swedish is not foreign!
			else
			{
				token += (character);
				return ok_char;
			}

		default:  // doh!
			I_Error("DDF_MainProcessChar: INTERNAL ERROR: "
					"Bad status value %d !\n", status);
			break;
	}

	return nothing;
}

//
// DDF_MainReadFile
//
// -ACB- 1998/08/10 Added the string reading code
// -ACB- 1998/09/28 DDF_ReadFunction Localised here
// -AJA- 1999/10/02 Recursive { } comments.
// -ES- 2000/02/29 Added
//
bool DDF_MainReadFile(readinfo_t * readinfo)
{
	std::string token;
	std::string current_cmd;

	char *name;
	char *value = NULL;
	char character;
	char *memfile;
	char *memfileptr;
	int status, formerstatus;
	int response;
	int size;
	int comment_level;
	int bracket_level;
	bool firstgo;
	
	int current_index = 0;
	int entry_count = 0;
  
#if (DEBUG_DDFREAD)
	char charcount = 0;
#endif

	status = waiting_tag;
	formerstatus = readstatus_invalid;
	comment_level = 0;
	bracket_level = 0;
	firstgo = true;

	cur_ddf_line_num = 1;

	if (!readinfo->memfile && !readinfo->filename)
		I_Error("DDF_MainReadFile: No file to read\n");

	if (!readinfo->memfile)
	{
		readinfo->memfile = (char*)DDF_MainCacheFile(readinfo);

		// no file ?  No worries, we'll get it from edge.wad...
		if (!readinfo->memfile)
			return false;
      
		cur_ddf_filename = std::string(readinfo->filename);
	}
	else
	{
		cur_ddf_filename = std::string(readinfo->lumpname);
	}

	memfileptr = memfile = readinfo->memfile;
	size = readinfo->memsize;

	// -ACB- 1998/09/12 Copy file to memory: Read until end. Speed optimisation.
	while (memfileptr < &memfile[size])
	{
		// -KM- 1998/12/16 Added #define command to ddf files.
		if (!strnicmp(memfileptr, "#DEFINE", 7))
		{
			bool line = false;

			memfileptr += 8;
			name = memfileptr;

			while (*memfileptr != ' ' && memfileptr < &memfile[size])
				memfileptr++;

			if (memfileptr < &memfile[size])
			{
				*memfileptr++ = 0;
				value = memfileptr;
			}
			else
			{
				DDF_Error("#DEFINE '%s' as what?!\n", name);
			}

			while (memfileptr < &memfile[size])
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

			DDF_MainAddDefine(name, value);

			token.clear();
			continue;
		}

		// -AJA- 1999/10/27: Not the greatest place for it, but detect //
		//       comments here and ignore them.  Ow the pain of long
		//       identifier names...  Ow the pain of &memfile[size] :-)
    
		if (comment_level == 0 && status != reading_string &&
			memfileptr+1 < &memfile[size] &&
			memfileptr[0] == '/' && memfileptr[1] == '/')
		{
			while (memfileptr < &memfile[size] && *memfileptr != '\n')
				memfileptr++;

			if (memfileptr >= &memfile[size])
				break;
		}
    
		character = *memfileptr++;

		if (character == '\n')
		{
			int l_len;

			cur_ddf_line_num++;

			// -AJA- 2000/03/21: determine linedata.  Ouch.
			for (l_len=0; &memfileptr[l_len] < &memfile[size] &&
					 memfileptr[l_len] != '\n' && memfileptr[l_len] != '\r'; l_len++)
			{ }


			cur_ddf_linedata = std::string(memfileptr, l_len);

			// -AJA- 2001/05/21: handle directives (lines beginning with #).
			// This code is more hackitude -- to be fixed when the whole
			// parsing code gets the overhaul it needs.
      
			if (strnicmp(memfileptr, "#CLEARALL", 9) == 0)
			{
				if (! firstgo)
					DDF_Error("#CLEARALL cannot be used inside an entry !\n");

				(* readinfo->clear_all)();

				memfileptr += l_len;
				continue;
			}

			if (strnicmp(memfileptr, "#VERSION", 8) == 0)
			{
				if (! firstgo)
					DDF_Error("#VERSION cannot be used inside an entry !\n");

				DDF_ParseVersion(memfileptr + 8, l_len - 8);

				memfileptr += l_len;
				continue;
			}
		}

		response = DDF_MainProcessChar(character, token, status);

		switch (response)
		{
			case remark_start:
				if (comment_level == 0)
				{
					formerstatus = status;
					status = reading_remark;
				}
				comment_level++;
				break;

			case remark_stop:
				comment_level--;
				if (comment_level == 0)
				{
					status = formerstatus;
				}
				break;

			case command_read:
				if (! token.empty())
					current_cmd = token.c_str();
				else
					current_cmd.clear();

				SYS_ASSERT(current_index == 0);

				token.clear();
				status = reading_data;
				break;

			case tag_start:
				status = reading_tag;
				break;

			case tag_stop:
				if (stricmp(token.c_str(), readinfo->tag) != 0)
					DDF_Error("Start tag <%s> expected, found <%s>!\n", 
							  readinfo->tag, token.c_str());

				status = waiting_newdef;
				token.clear();
				break;

			case def_start:
				if (bracket_level > 0)
					DDF_Error("Unclosed () brackets detected.\n");
         
				entry_count++;

				if (firstgo)
				{
					firstgo = false;
					status = reading_newdef;
				}
				else
				{
					cur_ddf_linedata.clear();

					// finish off previous entry
					(* readinfo->finish_entry)();

					token.clear();
					
					status = reading_newdef;

					cur_ddf_entryname.clear();
				}
				break;

			case def_stop:
				cur_ddf_entryname = epi::STR_Format("[%s]", token.c_str());

				// -AJA- 2009/07/27: extend an existing entry
				if (token[0] == '+' && token[1] == '+')
					(* readinfo->start_entry)(token.c_str()+2, true);
				else
					(* readinfo->start_entry)(token.c_str(), false);

				token.clear();
				status = reading_command;
				break;

				// -AJA- 2000/10/02: support for () brackets
			case group_start:
				if (status == reading_data || status == reading_command)
					bracket_level++;
				break;

			case group_stop:
				if (status == reading_data || status == reading_command)
				{
					bracket_level--;
					if (bracket_level < 0)
						DDF_Error("Unexpected `)' bracket.\n");
				}
				break;

			case separator:
				if (bracket_level > 0)
				{
					token += (',');
					break;
				}

				if (current_cmd.empty())
					DDF_Error("Unexpected comma `,'.\n");

				if (firstgo)
					DDF_WarnError("Command %s used outside of any entry\n",
								   current_cmd.c_str());
				else
				{ 
					(* readinfo->parse_field)(current_cmd.c_str(), 
						  DDF_MainGetDefine(token.c_str()), current_index, false);
					current_index++;
				}

				token.clear();
				break;

				// -ACB- 1998/08/10 String Handling
			case string_start:
				status = reading_string;
				break;

				// -ACB- 1998/08/10 String Handling
			case string_stop:
				status = reading_data;
				break;

			case terminator:
				if (current_cmd.empty())
					DDF_Error("Unexpected semicolon `;'.\n");

				if (bracket_level > 0)
					DDF_Error("Missing ')' bracket in ddf command.\n");

				(* readinfo->parse_field)(current_cmd.c_str(), 
					  DDF_MainGetDefine(token.c_str()), current_index, true);
				current_index = 0;

				token.clear();
				status = reading_command;
				break;

			case property_read:
				DDF_WarnError("Badly formed command: Unexpected semicolon `;'\n");
				break;

			case nothing:
				break;

			case ok_char:
#if (DEBUG_DDFREAD)
				charcount++;
				I_Debugf("%c", character);
				if (charcount == 75)
				{
					charcount = 0;
					I_Debugf("\n");
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
		DDF_Error("Unclosed comments detected.\n");

	if (bracket_level > 0)
		DDF_Error("Unclosed () brackets detected.\n");

	if (status == reading_tag)
		DDF_Error("Unclosed <> brackets detected.\n");

	if (status == reading_newdef)
		DDF_Error("Unclosed [] brackets detected.\n");
	
	if (status == reading_data || status == reading_string)
		DDF_WarnError("Unfinished DDF command on last line.\n");

	// if firstgo is true, nothing was defined
	if (!firstgo)
		(* readinfo->finish_entry)();

	cur_ddf_entryname.clear();
	cur_ddf_filename.clear();

	defines.clear();

	if (readinfo->filename)
		delete[] memfile;

	return true;
}

#if 0
static XXX *parse_type;

bool DDF_Load(epi::file_c *f)
{
	byte *data = f->LoadIntoMemory();
	if (! data)
		return false;

	for (;;)
	{
		get token;

		if (token == EOF) break;

		if (token == WHITESPACE | COMMENTS) continue;

		if (token == DIRECTIVE && entries == 0) handle directive;

		if (token == TAG) set type

		if (token == ENTRY) begin entry

		else parse command to current entry
	}

	return true;
}
#endif


//
// DDF_MainGetNumeric
//
// Get numeric value directly from the file
//
void DDF_MainGetNumeric(const char *info, void *storage)
{
	int *dest = (int *)storage;

	SYS_ASSERT(info && storage);

	if (isalpha(info[0]))
	{
		DDF_WarnError("Bad numeric value: %s\n", info);
		return;
	}

	// -KM- 1999/01/29 strtol accepts hex and decimal.
	*dest = strtol(info, NULL, 0);  // straight conversion - no messin'
}

//
// DDF_MainGetBoolean
//
// Get true/false from the file
//
// -KM- 1998/09/01 Gets a true/false value
//
void DDF_MainGetBoolean(const char *info, void *storage)
{
	bool *dest = (bool *)storage;

	SYS_ASSERT(info && storage);

	if ((stricmp(info, "TRUE") == 0) || (strcmp(info, "1") == 0))
	{
		*dest = true;
		return;
	}

	if ((stricmp(info, "FALSE") == 0) || (strcmp(info, "0") == 0))
	{
		*dest = false;
		return;
	}

	DDF_Error("Bad boolean value: %s\n", info);
}

//
// DDF_MainGetString
//
// Get String value directly from the file
//
// -KM- 1998/07/31 Needed a string argument.  Based on DDF_MainGetNumeric.
// -AJA- 2000/02/09: Free any existing string (potential memory leak).
// -ACB- 2004/07/26: Use epi::strent_c
//
void DDF_MainGetString(const char *info, void *storage)
{
	epi::strent_c *dest = (epi::strent_c *)storage;

	SYS_ASSERT(info && storage);

	dest->Set(info);
}


//
// DDF_MainParseField
//
// Check if the command exists, and call the parser function if it
// does (and return true), otherwise return false.
//
bool DDF_MainParseField(const commandlist_t *commands, 
						const char *field, const char *contents,
						byte *obj_base)
{
	SYS_ASSERT(obj_base);

	for (int i=0; commands[i].name; i++)
	{
		const char * name = commands[i].name;

		if (name[0] == '!')
			name++;
    
		// handle subfields
		if (name[0] == '*')
		{
			name++;

			int len = strlen(name);
			SYS_ASSERT(len > 0);

			if (strncmp(field, name, len) == 0 && field[len] == '.' && 
				isalnum(field[len+1]))
			{
				// recursively parse the sub-field
				return DDF_MainParseField(commands[i].sub_comms, 
                        field + len + 1, contents,
						obj_base + commands[i].offset);
			}
      
			continue;
		}

		if (DDF_CompareName(field, name) != 0)
			continue;

		// found it, so call parse routine
		SYS_ASSERT(commands[i].parse_command);

		(* commands[i].parse_command)(contents, obj_base + commands[i].offset);

		return true;
	}

	return false;
}


void DDF_MainGetLumpName(const char *info, void *storage)
{
	// Gets the string and checks the length is valid for a lump.

	SYS_ASSERT(info && storage);

	lumpname_c *LN = (lumpname_c *)storage;

	if (strlen(info) == 9)
		DDF_WarnError("Name %s too long (should be 8 characters or less)\n", info);
	else if (strlen(info) > 9)
		DDF_Error("Name %s too long (must be 8 characters or less)\n", info);

	LN->Set(info);
}


void DDF_MainRefAttack(const char *info, void *storage)
{
	atkdef_c **dest = (atkdef_c **)storage;

	SYS_ASSERT(info && storage);

	*dest = (atkdef_c*)atkdefs.Lookup(info);
	if (*dest == NULL)
		DDF_WarnError("Unknown Attack: %s\n", info);
}


int DDF_MainLookupDirector(const mobjtype_c *info, const char *ref)
{
	const char *p = strchr(ref, ':');

	int len = p ? (p - ref) : strlen(ref);

	if (len <= 0)
		DDF_Error("Bad Director `%s' : Nothing after divide\n", ref);

	std::string director(ref, len);

	int state  = DDF_StateFindLabel(info->state_grp, director.c_str());
	int offset = p ? MAX(0, atoi(p + 1) - 1) : 0;

	// FIXME: check for overflow
	return state + offset;
}


void DDF_MainGetFloat(const char *info, void *storage)
{
	float *dest = (float *)storage;

	SYS_ASSERT(info && storage);

	if (strchr(info, '%') != NULL)
	{
		DDF_MainGetPercentAny(info, storage);
		return;
	}

	if (sscanf(info, "%f", dest) != 1)
		DDF_Error("Bad floating point value: %s\n", info);
}

// -AJA- 1999/09/11: Added DDF_MainGetAngle and DDF_MainGetSlope.

void DDF_MainGetAngle(const char *info, void *storage)
{
	SYS_ASSERT(info && storage);

	angle_t *dest = (angle_t *)storage;

	float val;

	if (sscanf(info, "%f", &val) != 1)
		DDF_Error("Bad angle value: %s\n", info);

	if ((int) val == 360)
		val = 359.5;
	else if (val > 360.0f)
		DDF_WarnError("Angle '%s' too large (must be less than 360)\n", info);

	*dest = FLOAT_2_ANG(val);
}

void DDF_MainGetSlope(const char *info, void *storage)
{
	float val;
	float *dest = (float *)storage;

	SYS_ASSERT(info && storage);

	if (sscanf(info, "%f", &val) != 1)
		DDF_Error("Bad slope value: %s\n", info);

	if (val > +89.5f)
		val = +89.5f;
	if (val < -89.5f)
		val = -89.5f;

	*dest = M_Tan(FLOAT_2_ANG(val));
}


static void DoGetFloat(const char *info, void *storage)
{
	float *dest = (float *)storage;

	SYS_ASSERT(info && storage);

	if (sscanf(info, "%f", dest) != 1)
		DDF_Error("Bad floating point value: %s\n", info);
}


//
// DDF_MainGetPercent
//
// Reads percentages (0%..100%).
//
void DDF_MainGetPercent(const char *info, void *storage)
{
	percent_t *dest = (percent_t *)storage;
	char s[101];
	char *p;
	float f;

	// check that the string is valid
	Z_StrNCpy(s, info, 100);
	for (p = s; isdigit(*p) || *p == '.'; p++)
	{ /* do nothing */ }

	// the number must be followed by %
	if (*p != '%')
	{
		DDF_WarnError("Bad percent value '%s': Should be a number followed by %%\n", info);
		// -AJA- 2001/01/27: backwards compatibility
		DoGetFloat(s, &f);
		*dest = MAX(0, MIN(1, f));
		return;
	}

	*p = 0;
  
	DoGetFloat(s, &f);
	if (f < 0.0f || f > 100.0f)
		DDF_Error("Bad percent value '%s': Must be between 0%% and 100%%\n", s);

	*dest = f / 100.0f;
}

//
// DDF_MainGetPercentAny
//
// Like the above routine, but allows percentages outside of the
// 0-100% range (which is useful in same instances).
//
void DDF_MainGetPercentAny(const char *info, void *storage)
{
	percent_t *dest = (percent_t *)storage;
	char s[101];
	char *p;
	float f;

	// check that the string is valid
	Z_StrNCpy(s, info, 100);
	for (p = s; isdigit(*p) || *p == '.'; p++)
	{ /* do nothing */ }

	// the number must be followed by %
	if (*p != '%')
	{
		DDF_WarnError("Bad percent value '%s': Should be a number followed by %%\n", info);
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

void DDF_MainGetTime(const char *info, void *storage)
{
	float val;
	int *dest = (int *)storage;

	SYS_ASSERT(info && storage);

	// -ES- 1999/09/14 MAXT means that time should be maximal.
	if (!stricmp(info, "maxt"))
	{
		*dest = INT_MAX; // -ACB- 1999/09/22 Standards, Please.
		return;
	}

	if (strchr(info, 'T'))
	{
		DDF_MainGetNumeric(info, storage);
		return;
	}

	if (sscanf(info, "%f", &val) != 1)
		DDF_Error("Bad time value: %s\n", info);

	*dest = (int)(val * (float)TICRATE);
}

//
// DDF_DummyFunction
//
void DDF_DummyFunction(const char *info, void *storage)
{
	/* does nothing */
}

//
// DDF_MainGetColourmap
//
void DDF_MainGetColourmap(const char *info, void *storage)
{
	const colourmap_c **result = (const colourmap_c **)storage;

	*result = colourmaps.Lookup(info);
	if (*result == NULL)
		DDF_Error("DDF_MainGetColourmap: No such colourmap '%s'\n", info);
	
}

//
// DDF_MainGetRGB
//
void DDF_MainGetRGB(const char *info, void *storage)
{
	rgbcol_t *result = (rgbcol_t *)storage;
	int r, g, b;

	SYS_ASSERT(info && storage);

	if (DDF_CompareName(info, "NONE") == 0)
	{
		*result = RGB_NO_VALUE;
		return;
	}

	if (sscanf(info, " #%2x%2x%2x ", &r, &g, &b) != 3)
		DDF_Error("Bad RGB colour value: %s\n", info);

	*result = (r << 16) | (g << 8) | b;

	// silently change if matches the "none specified" value
	if (*result == RGB_NO_VALUE)
		*result ^= RGB_MAKE(1,1,1);
}

//
// DDF_MainGetWhenAppear
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
void DDF_MainGetWhenAppear(const char *info, void *storage)
{
	when_appear_e *result = (when_appear_e *)storage;

	*result = WNAP_None;

	bool negate = (info[0] == '!');

	const char *range = strstr(info, "-");

	if (range)
	{
		if (range <= info   || range[+1] == 0  ||
			range[-1] < '1' || range[-1] > '5' ||
			range[+1] < '1' || range[+1] > '5' ||
			range[-1] > range[+1])
		{
			DDF_Error("Bad range in WHEN_APPEAR value: %s\n", info);
			return;
		}

		for (char sk = '1'; sk <= '5'; sk++)
			if (range[-1] <= sk && sk <= range[+1])
				*result = (when_appear_e)(*result | (WNAP_SkillLevel1 << (sk - '1')));
	}
	else
	{
		if (strstr(info, "1"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel1);

		if (strstr(info, "2"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel2);

		if (strstr(info, "3"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel3);

		if (strstr(info, "4"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel4);

		if (strstr(info, "5"))
			*result = (when_appear_e)(*result | WNAP_SkillLevel5);
	}

	if (strstr(info, "SP") || strstr(info, "sp"))
		*result = (when_appear_e)(*result| WNAP_Single);

	if (strstr(info, "COOP") || strstr(info, "coop"))
		*result = (when_appear_e)(*result | WNAP_Coop);

	if (strstr(info, "DM") || strstr(info, "dm"))
		*result = (when_appear_e)(*result | WNAP_DeathMatch);

	// allow more human readable strings...

	if (negate)
		*result = (when_appear_e)(*result ^ (WNAP_SkillBits | WNAP_NetBits));

	if ((*result & WNAP_SkillBits) == 0)
		*result = (when_appear_e)(*result | WNAP_SkillBits);

	if ((*result & WNAP_NetBits) == 0)
		*result = (when_appear_e)(*result | WNAP_NetBits);
}

#if 0  // DEBUGGING ONLY
void Test_ParseWhenAppear(void)
{
	when_appear_e val;

	DDF_MainGetWhenAppear("1", &val);  printf("WNAP '1' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("3", &val);  printf("WNAP '3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("5", &val);  printf("WNAP '5' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("7", &val);  printf("WNAP '7' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("1:2", &val);  printf("WNAP '1:2' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("5:3:1", &val);  printf("WNAP '5:3:1' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("1-3", &val);  printf("WNAP '1-3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("4-5", &val);  printf("WNAP '4-5' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("0-2", &val);  printf("WNAP '0-2' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("3-6", &val);  printf("WNAP '3-6' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("5-1", &val);  printf("WNAP '5-1' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("sp", &val);  printf("WNAP 'sp' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("coop", &val);  printf("WNAP 'coop' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("dm", &val);  printf("WNAP 'dm' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:coop", &val);  printf("WNAP 'sp:coop' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:dm", &val);  printf("WNAP 'sp:dm' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:coop:dm", &val);  printf("WNAP 'sp:coop:dm' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("sp:3", &val);  printf("WNAP 'sp:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:3:4", &val);  printf("WNAP 'sp:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:3-5", &val);  printf("WNAP 'sp:3-5' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("sp:dm:3", &val);  printf("WNAP 'sp:dm:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:dm:3:4", &val);  printf("WNAP 'sp:dm:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("sp:dm:3-5", &val);  printf("WNAP 'sp:dm:3-5' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("DM:COOP:SP:3", &val);  printf("WNAP 'DM:COOP:SP:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("DM:COOP:SP:3:4", &val);  printf("WNAP 'DM:COOP:SP:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("DM:COOP:SP:3-5", &val);  printf("WNAP 'DM:COOP:SP:3-5' --> 0x%04x\n", val);

	printf("------------------------------------------------------------\n");

	DDF_MainGetWhenAppear("!1", &val);  printf("WNAP '!1' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!3", &val);  printf("WNAP '!3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!5", &val);  printf("WNAP '!5' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!7", &val);  printf("WNAP '!7' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!1:2", &val);  printf("WNAP '!1:2' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!5:3:1", &val);  printf("WNAP '!5:3:1' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!1-3", &val);  printf("WNAP '!1-3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!4-5", &val);  printf("WNAP '!4-5' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!0-2", &val);  printf("WNAP '!0-2' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!3-6", &val);  printf("WNAP '!3-6' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!5-1", &val);  printf("WNAP '!5-1' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!sp", &val);  printf("WNAP '!sp' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!coop", &val);  printf("WNAP '!coop' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!dm", &val);  printf("WNAP '!dm' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:coop", &val);  printf("WNAP '!sp:coop' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:dm", &val);  printf("WNAP '!sp:dm' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:coop:dm", &val);  printf("WNAP '!sp:coop:dm' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!sp:3", &val);  printf("WNAP '!sp:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:3:4", &val);  printf("WNAP '!sp:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:3-5", &val);  printf("WNAP '!sp:3-5' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!sp:dm:3", &val);  printf("WNAP '!sp:dm:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:dm:3:4", &val);  printf("WNAP '!sp:dm:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!sp:dm:3-5", &val);  printf("WNAP '!sp:dm:3-5' --> 0x%04x\n", val);

	DDF_MainGetWhenAppear("!DM:COOP:SP:3", &val);  printf("WNAP '!DM:COOP:SP:3' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!DM:COOP:SP:3:4", &val);  printf("WNAP '!DM:COOP:SP:3:4' --> 0x%04x\n", val);
	DDF_MainGetWhenAppear("!DM:COOP:SP:3-5", &val);  printf("WNAP '!DM:COOP:SP:3-5' --> 0x%04x\n", val);
}
#endif

//
// DDF_MainGetBitSet
//
void DDF_MainGetBitSet(const char *info, void *storage)
{
	bitset_t *result = (bitset_t *)storage;
	int start, end;

	SYS_ASSERT(info && storage);

	// allow a numeric value
	if (sscanf(info, " %i ", result) == 1)
		return;

	*result = BITSET_EMPTY;

	for (; *info; info++)
	{
		if (*info < 'A' || *info > 'Z')
			continue;
    
		start = end = (*info) - 'A';

		// handle ranges
		if (info[1] == '-' && 'A' <= info[2] && info[2] <= 'Z' &&
			info[2] >= info[0])
		{
			end = info[2] - 'A';
		}

		for (; start <= end; start++)
			(*result) |= (1 << start);
	}
}

static int FindSpecialFlag(const char *prefix, const char *name,
						   const specflags_t *flag_set)
{
	int i;
	char try_name[512];

	for (i=0; flag_set[i].name; i++)
	{
		const char *current = flag_set[i].name;

		if (current[0] == '!')
			current++;
    
		sprintf(try_name, "%s%s", prefix, current);
    
		if (DDF_CompareName(name, try_name) == 0)
			return i;
	}

	return -1;
}

checkflag_result_e DDF_MainCheckSpecialFlag(const char *name,
							 const specflags_t *flag_set, int *flag_value, 
							 bool allow_prefixes, bool allow_user)
{
	int index;
	int negate = 0;
	int user = 0;

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
			index = FindSpecialFlag("NO_", name, flag_set);
		}

		// try name with NOT_ prefix...
		if (index == -1)
		{
			negate = 1;
			index = FindSpecialFlag("NOT_", name, flag_set);
		}

		// try name with DISABLE_ prefix...
		if (index == -1)
		{
			negate = 1;
			index = FindSpecialFlag("DISABLE_", name, flag_set);
		}

		// try name with USER_ prefix...
		if (index == -1 && allow_user)
		{
			user = 1;
			negate = 0;
			index = FindSpecialFlag("USER_", name, flag_set);
		}
	}

	if (index < 0)
		return CHKF_Unknown;

	(*flag_value) = flag_set[index].flags;

	if (flag_set[index].negative)
		negate = !negate;
  
	if (user)
		return CHKF_User;
  
	if (negate)
		return CHKF_Negative;

	return CHKF_Positive;
}

//
// DDF_DecodeBrackets
//
// Decode a keyword followed by something in () brackets.  Buf_len gives
// the maximum size of the output buffers.  The outer keyword is required
// to be non-empty, though the inside can be empty.  Returns false if
// cannot be parsed (e.g. no brackets).  Handles strings.
//
bool DDF_MainDecodeBrackets(const char *info, char *outer, char *inner,
	int buf_len)
{
	const char *pos = info;
	
	while (*pos && *pos != '(')
		pos++;
	
	if (*pos == 0 || pos == info)
		return false;
	
	if (pos - info >= buf_len)  // overflow
		return false;

	strncpy(outer, info, pos - info);
	outer[pos - info] = 0;

	pos++;  // skip the '('

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
			in_string = ! in_string;

		pos++;
	}

	if (*pos == 0)
		return false;

	if (pos - info >= buf_len)  // overflow
		return false;

	strncpy(inner, info, pos - info);
	inner[pos - info] = 0;

	return true;
}

//
// DDF_MainDecodeList
//
// Find the dividing character.  Returns NULL if not found.
// Handles strings and brackets unless simple is true.
//
const char *DDF_MainDecodeList(const char *info, char divider, bool simple)
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
		if (! simple)
		{
			if (pos[0] == '\\' && pos[1] == '"')
			{
				pos += 2;
				continue;
			}

			if (*pos == '"')
				in_string = ! in_string;

			if (!in_string && *pos == '(')
				brackets++;

			if (!in_string && *pos == ')')
			{
				brackets--;
				if (brackets < 0)
					DDF_Error("Too many ')' found: %s\n", info);
			}
		}

		pos++;
	}

	if (in_string)
		DDF_Error("Unterminated string found: %s\n", info);

	if (brackets != 0)
		DDF_Error("Unclosed brackets found: %s\n", info);

	return NULL;
}

// DDF OBJECTS


// ---> mobj_strref class

const mobjtype_c *mobj_strref_c::GetRef()
{
	if (def)
		return def;

	def = mobjtypes.Lookup(name.c_str());

	return def;
}

// ---> damage class

//
// damage_c Constructor
//
damage_c::damage_c()
{
}

//
// damage_c Copy constructor
//
damage_c::damage_c(damage_c &rhs)
{
	Copy(rhs);
}

//
// damage_c Destructor
//
damage_c::~damage_c()
{
}

//
// damage_c::Copy
//
void damage_c::Copy(damage_c &src)
{
	nominal = src.nominal;
	linear_max = src.linear_max;
	error = src.error;
	delay = src.delay;

	obituary = src.obituary;
	pain = src.pain;
	death = src.death;
	overkill = src.overkill;
	
	no_armour = src.no_armour;
}

//
// damage_c::Default
//
void damage_c::Default(damage_c::default_e def)
{
	obituary.clear();

	switch (def)
	{
		case DEFAULT_MobjChoke:
		{
			nominal	= 6.0f;	
			linear_max = 14.0f;	
			error = -1.0f;
			delay = 2 * TICRATE;
			obituary = "OB_DROWN";
			no_armour = true;
			break;
		}

		case DEFAULT_Sector:
		{
			nominal = 0.0f;
			linear_max = -1.0f;
			error = -1.0f;
			delay = 31;
			no_armour = false;
			break;
		}
		
		case DEFAULT_Attack:
		case DEFAULT_Mobj:
		default:
		{
			nominal = 0.0f;
			linear_max = -1.0f;     
			error = -1.0f;     
			delay = 0;      
			no_armour = false;
			break;
		}
	}

	pain.Default();
	death.Default();
	overkill.Default();
}

//
// damage_c assignment operator
//
damage_c& damage_c::operator=(damage_c &rhs)
{
	if (&rhs != this)
		Copy(rhs);
		
	return *this;
}

// ---> label offset class

//
// label_offset_c Constructor
//
label_offset_c::label_offset_c()
{
	offset = 0;
}

//
// label_offset_c Copy constructor
//
label_offset_c::label_offset_c(label_offset_c &rhs)
{
	Copy(rhs);
}

//
// label_offset_c Destructor
//
label_offset_c::~label_offset_c()
{
}

//
// label_offset_c::Copy
//
void label_offset_c::Copy(label_offset_c &src)
{
	label = src.label;
	offset = src.offset;
}

//
// label_offset_c::Default
//
void label_offset_c::Default()
{
	label.clear();
	offset = 0;
}


//
// label_offset_c assignment operator
//
label_offset_c& label_offset_c::operator=(label_offset_c& rhs)
{
	if (&rhs != this)
		Copy(rhs);
		
	return *this;
}

// ---> dlight_info class

dlight_info_c::dlight_info_c()
{
	Default();
}

dlight_info_c::dlight_info_c(dlight_info_c &rhs)
{
	Copy(rhs);
}

void dlight_info_c::Copy(dlight_info_c &src)
{
	type   = src.type;
	shape  = src.shape;
	radius = src.radius;
	colour = src.colour;
	height = src.height;
	leaky  = src.leaky;

	cache_data = NULL;
}

void dlight_info_c::Default()
{
	type   = DLITE_None;
	radius = 32;
	colour = RGB_MAKE(255, 255, 255);
	height = PERCENT_MAKE(50);
	leaky  = false;

	shape.Set("DLIGHT_EXP");

	cache_data = NULL;
}

dlight_info_c& dlight_info_c::operator= (dlight_info_c &rhs)
{
	CHECK_SELF_ASSIGN(rhs);

	Copy(rhs);

	return *this;
}

// ---> weakness_info class

weakness_info_c::weakness_info_c()
{
	Default();
}

weakness_info_c::weakness_info_c(weakness_info_c &rhs)
{
	Copy(rhs);
}

void weakness_info_c::Copy(weakness_info_c &src)
{
	height[0] = src.height[0];
	height[1] = src.height[1];
	angle[0]  = src.angle[0];
	angle[1]  = src.angle[1];

	classes    = src.classes;
	multiply   = src.multiply;
	painchance = src.painchance;
}

void weakness_info_c::Default()
{
	height[0] = PERCENT_MAKE(  0);
	height[1] = PERCENT_MAKE(100);

	angle[0] = ANG0;
	angle[1] = ANG_MAX;

	classes   = BITSET_EMPTY;
	multiply  = 2.5;
	painchance = -1; // disabled
}

weakness_info_c& weakness_info_c::operator=(weakness_info_c &rhs)
{
	CHECK_SELF_ASSIGN(rhs);

	Copy(rhs);

	return *this;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
