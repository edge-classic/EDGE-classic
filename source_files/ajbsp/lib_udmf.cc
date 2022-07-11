//------------------------------------------------------------------------
//  UDMF PARSING / WRITING
//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2019 Andrew Apted
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
//------------------------------------------------------------------------

#include "ajbsp.h"

namespace ajbsp
{

class Udmf_Token
{
private:
	// empty means EOF
	std::string text;

public:
	Udmf_Token(const char *str) : text(str)
	{ }

	Udmf_Token(const char *str, int len) : text(str, 0, len)
	{ }

	const char *c_str()
	{
		return text.c_str();
	}

	bool IsEOF() const
	{
		return text.empty();
	}

	bool IsIdentifier() const
	{
		if (text.size() == 0)
			return false;

		char ch = text[0];

		return isalpha(ch) || ch == '_';
	}

	bool IsString() const
	{
		return text.size() > 0 && text[0] == '"';
	}

	bool Match(const char *name) const
	{
		return y_stricmp(text.c_str(), name) == 0;
	}

	int DecodeInt() const
	{
		return atoi(text.c_str());
	}

	double DecodeFloat() const
	{
		return atof(text.c_str());
	}

	std::string DecodeString() const
	{
		if (! IsString() || text.size() < 2)
		{
			// TODO warning
			return std::string();
		}

		return std::string(text, 1, text.size() - 2);
	}

	int DecodeCoord() const
	{
		return ((int) I_ROUND((DecodeFloat()) * 4096.0));
	}

	const char *DecodeTexture() const
	{
		char buffer[16];

		if (! IsString())
		{
			// TODO warning
			strcpy(buffer, "-");
		}
		else
		{
			int use_len = 8;

			if (text.size() < 10)
				use_len = (int)text.size() - 2;

			strncpy(buffer, text.c_str() + 1, use_len);
			buffer[use_len] = 0;
		}

		if (buffer[0] == 0)
			return "-";

		for (size_t i = 0 ; i < WAD_TEX_NAME && buffer[i]; i++)
		{
			if (buffer[i] == '"')
				buffer[i] = '_';
			else
				buffer[i] = toupper(buffer[i]);
		}

		return buffer;
	}
};


// since UDMF lumps can be very large, we read chunks of it
// as-needed instead of loading the whole thing into memory.
// the buffer size should be over 2x maximum token length.
#define U_BUF_SIZE  16384

class Udmf_Parser
{
private:
	Lump_c *lump;

	// reached EOF or a file read error
	bool done;

	// we have seen a "/*" but not the closing "*/"
	bool in_comment;

	// number of remaining bytes
	int remaining;

	// read buffer
	char buffer[U_BUF_SIZE];

	// position in buffer and used size of buffer
	int b_pos;
	int b_size;

public:
	Udmf_Parser(Lump_c *_lump) :
		lump(_lump),
		done(false), in_comment(false),
		b_pos(0), b_size(0)
	{
		remaining = lump->Length();
	}

	Udmf_Token Next()
	{
		for (;;)
		{
			if (done)
				return Udmf_Token("");

			// when position reaches half-way point, shift buffer down
			if (b_pos >= U_BUF_SIZE/2)
			{
				memmove(buffer, buffer + U_BUF_SIZE/2, U_BUF_SIZE/2);

				b_pos  -= U_BUF_SIZE/2;
				b_size -= U_BUF_SIZE/2;
			}

			// top up the buffer
			if (remaining > 0 && b_size < U_BUF_SIZE)
			{
				int want = U_BUF_SIZE - b_size;
				if (want > remaining)
					want = remaining;

				if (! lump->Read(buffer + b_size, want))
				{
					// TODO mark error somewhere, show dialog later
					done = true;
					continue;
				}

				remaining -= want;
				b_size    += want;
			}

			// end of file?
			if (remaining <= 0 && b_pos >= b_size)
			{
				done = true;
				continue;
			}

			if (in_comment)
			{
				// end of multi-line comment?
				if (b_pos+2 <= b_size &&
					buffer[b_pos] == '*' &&
					buffer[b_pos+1] == '/')
				{
					in_comment = false;
					b_pos += 2;
					continue;
				}

				b_pos++;
				continue;
			}

			// check for multi-line comment
			if (b_pos+2 <= b_size &&
				buffer[b_pos] == '/' &&
				buffer[b_pos+1] == '*')
			{
				in_comment = true;
				b_pos += 2;
				continue;
			}

			// check for single-line comment
			if (b_pos+2 <= b_size &&
				buffer[b_pos] == '/' &&
				buffer[b_pos+1] == '/')
			{
				SkipToEOLN();
				continue;
			}

			// skip whitespace (assumes ASCII)
			int start = b_pos;
			unsigned char ch = buffer[b_pos];

			if ((ch <= 32) || (ch >= 127 && ch <= 160))
			{
				b_pos++;
				continue;
			}

			// an actual token, yay!

			// is it a string?
			if (ch == '"')
			{
				b_pos++;

				while (b_pos < b_size)
				{
					// skip escapes
					if (buffer[b_pos] == '\\' && b_pos+1 < b_size)
					{
						b_pos += 2;
						continue;
					}

					if (buffer[b_pos] == '"')
					{
						// include trailing double quote
						b_pos++;
						break;
					}

					b_pos++;
				}

				return Udmf_Token(buffer+start, b_pos - start);
			}

			// is it a identifier or number?
			if (isalnum(ch) || ch == '_' || ch == '-' || ch == '+')
			{
				b_pos++;

				while (b_pos < b_size)
				{
					char ch = buffer[b_pos];
					if (isalnum(ch) || ch == '_' || ch == '-' || ch == '+' || ch == '.')
					{
						b_pos++;
						continue;
					}
					break;
				}

				return Udmf_Token(buffer+start, b_pos - start);
			}

			// it must be a symbol, such as '{' or '}'
			b_pos++;

			return Udmf_Token(buffer+start, 1);
		}
	}

	bool Expect(const char *name)
	{
		Udmf_Token tok = Next();
		return tok.Match(name);
	}

	void SkipToEOLN()
	{
		while (b_pos < b_size && buffer[b_pos] != '\n')
			b_pos++;
	}
};


static void UDMF_ParseGlobalVar(Udmf_Parser& parser, Udmf_Token& name)
{
	Udmf_Token value = parser.Next();
	if (value.IsEOF())
	{
		// TODO mark error
		return;
	}
	if (!parser.Expect(";"))
	{
		// TODO mark error
		parser.SkipToEOLN();
		return;
	}

	if (name.Match("namespace"))
	{
		//Not sure what namespaces we'll support; probably just "Doom"

		//Udmf_namespace = value.DecodeString();
	}
	else if (name.Match("ee_compat"))
	{
		// odd Eternity thing, ignore it
	}
	else
	{
		PrintMsg(StringPrintf("skipping unknown global '%s' in UDMF\n", name.c_str()));
	}
}

static void UDMF_ParseThingField(thing_t *T, Udmf_Token& field, Udmf_Token& value)
{
	// just ignore any setting with the "false" keyword
	if (value.Match("false"))
		return;

	// TODO hexen options

	// TODO strife options

	if (field.Match("x"))
		T->x = value.DecodeFloat();
	else if (field.Match("y"))
		T->y = value.DecodeFloat();
	//else if (field.Match("height"))
		//T->raw_h = value.DecodeCoord();
	else if (field.Match("type"))
		T->type = value.DecodeInt();
	//else if (field.Match("angle"))
		//T->angle = value.DecodeInt();

	/*else if (field.Match("id"))
		T->tid = value.DecodeInt();
	else if (field.Match("special"))
		T->special = value.DecodeInt();
	else if (field.Match("arg0"))
		T->arg1 = value.DecodeInt();
	else if (field.Match("arg1"))
		T->arg2 = value.DecodeInt();
	else if (field.Match("arg2"))
		T->arg3 = value.DecodeInt();
	else if (field.Match("arg3"))
		T->arg4 = value.DecodeInt();
	else if (field.Match("arg4"))
		T->arg5 = value.DecodeInt();*/
	else if (field.Match("skill1"))
		T->options |= MTF_Easy;
	else if (field.Match("skill2"))
		T->options |= MTF_Easy;
	else if (field.Match("skill3"))
		T->options |= MTF_Medium;
	else if (field.Match("skill4"))
		T->options |= MTF_Hard;
	else if (field.Match("ambush"))
		T->options |= MTF_Ambush;
	else if (field.Match("friend"))
		T->options |= MTF_Friend;
	else if (field.Match("single"))
		T->options &= ~MTF_Not_SP;
	else if (field.Match("coop"))
		T->options &= ~MTF_Not_COOP;
	else if (field.Match("dm"))
		T->options &= ~MTF_Not_DM;
}

static void UDMF_ParseVertexField(vertex_t *V, Udmf_Token& field, Udmf_Token& value)
{
	if (field.Match("x"))
		V->x = value.DecodeFloat();
	else if (field.Match("y"))
		V->y = value.DecodeFloat();
}

static void UDMF_ParseLinedefField(linedef_t *LD, Udmf_Token& field, Udmf_Token& value)
{
	// Note: vertex and sidedef numbers are validated later on

	// just ignore any setting with the "false" keyword
	if (value.Match("false"))
		return;

	// TODO hexen flags

	// TODO strife flags

	if (field.Match("v1"))
	{
		LD->udmf_start_lookup = LE_U16(value.DecodeInt());
	}
	else if (field.Match("v2"))
	{
		LD->udmf_end_lookup = LE_U16(value.DecodeInt());
	}
	else if (field.Match("sidefront"))
		LD->udmf_right_lookup = value.DecodeInt();
	else if (field.Match("sideback"))
		LD->udmf_left_lookup = value.DecodeInt();
	else if (field.Match("special"))
		LD->type = value.DecodeInt();

	/*
	else if (field.Match("arg0"))
		LD->tag = value.DecodeInt();
	else if (field.Match("arg1"))
		LD->arg2 = value.DecodeInt();
	else if (field.Match("arg2"))
		LD->arg3 = value.DecodeInt();
	else if (field.Match("arg3"))
		LD->arg4 = value.DecodeInt();
	else if (field.Match("arg4"))
		LD->arg5 = value.DecodeInt();*/

	else if (field.Match("blocking"))
		LD->flags |= MLF_Blocking;
	else if (field.Match("blockmonsters"))
		LD->flags |= MLF_BlockMonsters;
	else if (field.Match("twosided"))
		LD->flags |= MLF_TwoSided;
	else if (field.Match("dontpegtop"))
		LD->flags |= MLF_UpperUnpegged;
	else if (field.Match("dontpegbottom"))
		LD->flags |= MLF_LowerUnpegged;
	else if (field.Match("secret"))
		LD->flags |= MLF_Secret;
	else if (field.Match("blocksound"))
		LD->flags |= MLF_SoundBlock;
	else if (field.Match("dontdraw"))
		LD->flags |= MLF_DontDraw;
	else if (field.Match("mapped"))
		LD->flags |= MLF_Mapped;

	else if (field.Match("passuse"))
		LD->flags |= MLF_Boom_PassThru;
}

static void UDMF_ParseSidedefField(sidedef_t *SD, Udmf_Token& field, Udmf_Token& value)
{
	// Note: sector numbers are validated later on

	// TODO: consider how to handle "offsetx_top" (etc), if at all

	if (field.Match("sector"))
		SD->udmf_sector_lookup = value.DecodeInt();
	else if (field.Match("texturetop"))
		memcpy(SD->upper_tex, value.DecodeTexture(), 8);
	else if (field.Match("texturebottom"))
		memcpy(SD->lower_tex, value.DecodeTexture(), 8);
	else if (field.Match("texturemiddle"))
		memcpy(SD->mid_tex, value.DecodeTexture(), 8);
	else if (field.Match("offsetx"))
		SD->x_offset = value.DecodeInt();
	else if (field.Match("offsety"))
		SD->y_offset = value.DecodeInt();
}

static void UDMF_ParseSectorField(sector_t *S, Udmf_Token& field, Udmf_Token& value)
{
	if (field.Match("heightfloor"))
		S->floor_h = value.DecodeInt();
	else if (field.Match("heightceiling"))
		S->ceil_h = value.DecodeInt();
	else if (field.Match("texturefloor"))
		memcpy(S->floor_tex, value.DecodeTexture(), 8);
	else if (field.Match("textureceiling"))
		memcpy(S->ceil_tex, value.DecodeTexture(), 8);
	else if (field.Match("lightlevel"))
		S->light = value.DecodeInt();
	else if (field.Match("special"))
		S->special = value.DecodeInt();
	else if (field.Match("id"))
		S->tag = value.DecodeInt();
}

static void UDMF_ParseObject(Udmf_Parser& parser, Udmf_Token& name)
{
	thing_t   *new_T  = NULL;
	vertex_t  *new_V  = NULL;
	linedef_t *new_LD = NULL;
	sidedef_t *new_SD = NULL;
	sector_t  *new_S  = NULL;

	if (name.Match("thing"))
	{
		new_T = NewThing();
		new_T->index = num_things - 1;
	}
	else if (name.Match("vertex"))
	{
		new_V = NewVertex();
		new_V->index = num_vertices - 1;
		num_old_vert = num_vertices;
	}
	else if (name.Match("linedef"))
	{
		new_LD = NewLinedef();
		new_LD->index = num_linedefs - 1;
	}
	else if (name.Match("sidedef"))
	{
		new_SD = NewSidedef();
		memset(new_SD->mid_tex, 0, 8);
		memcpy(new_SD->mid_tex, "-", 1);
		memset(new_SD->upper_tex, 0, 8);
		memcpy(new_SD->upper_tex, "-", 1);
		memset(new_SD->lower_tex, 0, 8);
		memcpy(new_SD->lower_tex, "-", 1);
		new_SD->index = num_sidedefs - 1;
	}
	else if (name.Match("sector"))
	{
		new_S = NewSector();
		new_S->light = 160;
		new_S->index = num_sectors - 1;
		new_S->warned_facing = -1;
	}
	else
	{
		// unknown object kind
		PrintMsg(StringPrintf("skipping unknown block '%s' in UDMF\n", name.c_str()));
	}

	for (;;)
	{
		Udmf_Token tok = parser.Next();
		if (tok.IsEOF())
			break;

		if (tok.Match("}"))
			break;

		if (! parser.Expect("="))
		{
			// TODO mark error
			parser.SkipToEOLN();
			continue;
		}

		Udmf_Token value = parser.Next();
		if (value.IsEOF())
			break;

		if (! parser.Expect(";"))
		{
			// TODO mark error
			parser.SkipToEOLN();
			continue;
		}

		if (new_T)
			UDMF_ParseThingField(new_T, tok, value);

		if (new_V)
			UDMF_ParseVertexField(new_V, tok, value);

		if (new_LD)
			UDMF_ParseLinedefField(new_LD, tok, value);

		if (new_SD)
			UDMF_ParseSidedefField(new_SD, tok, value);

		if (new_S)
			UDMF_ParseSectorField(new_S, tok, value);
	}
}

void UDMF_LoadLevel()
{
	Lump_c *lump = FindLevelLump("TEXTMAP");

	// we assume this cannot happen
	if (! lump)
		BugError("AJBSP: Null TEXTMAP lump passed to UDMF parser?\n");

	lump->Seek(0);

	Udmf_Parser parser(lump);

	for (;;)
	{
		Udmf_Token tok = parser.Next();

		if (tok.IsEOF())
			break;

		if (! tok.IsIdentifier())
		{
			// something has gone wrong
			// TODO mark the error somehow, pop-up dialog later
			parser.SkipToEOLN();
			continue;
		}

		Udmf_Token tok2 = parser.Next();
		if (tok2.IsEOF())
			break;

		if (tok2.Match("="))
		{
			UDMF_ParseGlobalVar(parser, tok);
			continue;
		}
		if (tok2.Match("{"))
		{
			UDMF_ParseObject(parser, tok);
			continue;
		}

		// unexpected symbol
		// TODO mark the error somehow, show dialog later
		parser.SkipToEOLN();
	}
}

} // namespace ajbsp 

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
