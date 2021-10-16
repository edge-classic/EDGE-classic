//------------------------------------------------------------------------
//  PATCH Loading
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include "i_defs.h"

#include "buffer.h"
#include "dh_plugin.h"
#include "patch.h"

#include "ammo.h"
#include "convert.h"
#include "frames.h"
#include "info.h"
#include "misc.h"
#include "mobj.h"
#include "sounds.h"
#include "storage.h"
#include "system.h"
#include "text.h"
#include "things.h"
#include "util.h"
#include "weapons.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace Deh_Edge
{

#define MAX_LINE  512
#define MAX_TEXT_STR  1200

#define PRETTY_LEN  28


namespace Patch
{
	parse_buffer_api *pat_buf;

	bool file_error;

	int patch_fmt;   /* 1 to 6 */
	int dhe_ver;     /* 12, 13, 20-24, 30-31 */
	int doom_ver;    /* 12, 16 to 21 */

	typedef enum
	{
		O_MOBJ, O_AMMO, O_WEAPON, O_FRAME, O_SOUND, O_SPRITE
	}
	object_kind_e;

	void DetectMsg(const char *kind)
	{
		PrintMsg("Detected %s patch file from DEHACKED v%d.%d\n",
			kind, dhe_ver / 10, dhe_ver % 10);
	}

	void VersionMsg(void)
	{
		PrintMsg("Patch format %d, for DOOM EXE %d.%d%s\n",
			patch_fmt, doom_ver / 10, doom_ver % 10,
			(doom_ver == 16) ? "66" : "");
	}

	int GetRawInt(void)
	{
		if (pat_buf->eof() || pat_buf->error())
			file_error = true;

		if (file_error)
			return -1;

		unsigned char raw[4];

		pat_buf->read(raw, 4);

		return ((int)raw[0]) +
		       ((int)raw[1] << 8) +
		       ((int)raw[2] << 16) +
		       ((int)(char)raw[3] << 24);
	}

	void GetRawString(char *buf, int max_len)
	{
		// luckily for us, DeHackEd ensured that replacement strings
		// were never truncated short (i.e. the NUL byte appearing
		// in an earlier 32-bit word).  Hence we don't need to know
		// the length of the original strings in order to read in
		// modified strings.

		int len = 0;

		for (;;)
		{
			int ch = pat_buf->getch();

			if (ch == 0)
				break;

			if (ch == EOF || pat_buf->error())
				file_error = true;

			if (file_error)
				break;

			buf[len++] = ch;

			if (len >= max_len)
			{
				FatalError("Text string exceeds internal buffer length.\n"
					"[> %d characters, from binary patch file]\n",
					MAX_TEXT_STR);
			}
		}

		buf[len] = 0;

		// strings are aligned to 4 byte boundaries
		for (; (len % 4) != 3; len++)
			pat_buf->getch();
	}

	const char *ObjectName(int o_kind)
	{
		switch (o_kind)
		{
			case O_MOBJ:   return "thing";
			case O_AMMO:   return "ammo";
			case O_WEAPON: return "weapon";
			case O_FRAME:  return "frame";
			case O_SOUND:  return "sound";
			case O_SPRITE: return "sprite";

			default: InternalError("Illegal O_KIND: %d\n", o_kind);
		}

		return NULL;  // not reached
	}

	void MarkObject(int o_kind, int o_num)
	{
		Debug_PrintMsg("[%d] MODIFIED\n", o_num);

		switch (o_kind)
		{
			case O_MOBJ:   Things:: MarkThing(o_num);  break;
			case O_AMMO:   Ammo::   MarkAmmo(o_num);   break;
			case O_WEAPON: Weapons::MarkWeapon(o_num); break;
			case O_FRAME:  Frames:: MarkState(o_num);  break;
			case O_SOUND:  Sounds:: MarkSound(o_num);  break;

			case O_SPRITE: // not needed 
				break;

			default: InternalError("Illegal O_KIND: %d\n", o_kind);
		}
	}

	void GetInt(int o_kind, int o_num, int *dest)
	{
		int temp = GetRawInt();

		Debug_PrintMsg("Int: %d\n", temp);

		if (*dest == temp)
			return;

		Storage::RememberMod(dest, temp);

		MarkObject(o_kind, o_num);
	}

	void GetFlags(int o_kind, int o_num, int *dest)
	{
		int temp = GetRawInt();

		Debug_PrintMsg("Flags: 0x%08x\n", temp);

		// prevent the BOOM/MBF specific flags from being set
		// from binary patch files.
		temp &= ~ ALL_BEX_FLAGS;

		if (*dest == temp)
			return;

		Storage::RememberMod(dest, temp);

		MarkObject(o_kind, o_num);
	}

	void GetFrame(int o_kind, int o_num, int *dest)
	{
		int temp = GetRawInt();

		Debug_PrintMsg("Frame: %d\n", temp);

		if (doom_ver == 12)
		{
			if (temp < 0 || temp >= FRAMES_1_2)
			{
				PrintWarn("Found illegal V1.2 frame number: %d\n", temp);
				return;
			}

			temp = frame12to166[temp];
		}

		if (temp < 0 || temp >= NUMSTATES)
		{
			PrintWarn("Found illegal frame number: %d\n", temp);
			return;
		}

		if (*dest == temp)
			return;

		Storage::RememberMod(dest, temp);

		MarkObject(o_kind, o_num);
	}

	void GetSprite(int o_kind, int o_num, int *dest)
	{
		int temp = GetRawInt();

		Debug_PrintMsg("Sprite: %d\n", temp);

		if (doom_ver == 12)
		{
			if (temp < 0 || temp >= SPRITES_1_2)
			{
				PrintWarn("Found illegal V1.2 sprite number: %d\n", temp);
				return;
			}

			temp = sprite12to166[temp];
		}

		if (temp < 0 || temp >= NUMSPRITES)
		{
			PrintWarn("Found illegal sprite number: %d\n", temp);
			return;
		}

		if (*dest == temp)
			return;

		Storage::RememberMod(dest, temp);

		MarkObject(o_kind, o_num);
	}

	void GetSound(int o_kind, int o_num, int *dest)
	{
		int temp = GetRawInt();

		Debug_PrintMsg("Sound: %d\n", temp);

		if (doom_ver == 12)
		{
			if (temp < 0 || temp >= SOUNDS_1_2)
			{
				PrintWarn("Found illegal V1.2 sound number: %d\n", temp);
				return;
			}

			temp = sound12to166[temp];
		}

		if (temp < 0 || temp >= NUMSFX)
		{
			PrintWarn("Found illegal sound number: %d\n", temp);
			return;
	    }

		if (*dest == temp)
			return;

		Storage::RememberMod(dest, temp);

		MarkObject(o_kind, o_num);
	}

	void GetAmmoType(int o_kind, int o_num, int *dest)
	{
		int temp = GetRawInt();

		Debug_PrintMsg("AmmoType: %d\n", temp);

		if (temp < 0 || temp > 5)
		{
			PrintWarn("Found illegal ammo type: %d\n", temp);
			return;
	    }

		if (temp == 4)
			temp = 5;

		if (*dest == temp)
			return;

		Storage::RememberMod(dest, temp);

		MarkObject(o_kind, o_num);
	}

	const char *PrettyTextString(const char *t)
	{
		static char buf[PRETTY_LEN*2 + 10];

		while (isspace(*t))
			t++;

		if (! *t)
			return "<<EMPTY>>";

		int len = 0;

		for (; *t && len < PRETTY_LEN; t++)
		{
			if (t[0] == t[1] && t[1] == t[2])
				continue;

			if (*t == '"')
			{
				buf[len++] = '\'';
			}
			else if (*t == '\n')
			{
				buf[len++] = '\\';
				buf[len++] = 'n';
			}
			else if ((unsigned char)*t < 32 || (unsigned char)*t >= 127)
			{
				buf[len++] = '?';
			}
			else
				buf[len++] = *t;
		}

		if (*t)
		{
			buf[len++] = '.';
			buf[len++] = '.';
			buf[len++] = '.';
		}

		buf[len] = 0;

		return buf;
	}

	void ReadBinaryThing(int mt_num)
	{
		Debug_PrintMsg("\n--- ReadBinaryThing %d ---\n", mt_num);

		if (file_error)
			FatalError("File error reading binary thing table.\n");

		mobjinfo_t *mobj = mobjinfo + mt_num;

		GetInt  (O_MOBJ, mt_num, &mobj->doomednum);
		GetFrame(O_MOBJ, mt_num, &mobj->spawnstate);
		GetInt  (O_MOBJ, mt_num, &mobj->spawnhealth);
		GetFrame(O_MOBJ, mt_num, &mobj->seestate);
		GetSound(O_MOBJ, mt_num, &mobj->seesound);
		GetInt  (O_MOBJ, mt_num, &mobj->reactiontime);

		GetSound(O_MOBJ, mt_num, &mobj->attacksound);
		GetFrame(O_MOBJ, mt_num, &mobj->painstate);
		GetInt  (O_MOBJ, mt_num, &mobj->painchance);
		GetSound(O_MOBJ, mt_num, &mobj->painsound);
		GetFrame(O_MOBJ, mt_num, &mobj->meleestate);
		GetFrame(O_MOBJ, mt_num, &mobj->missilestate);
		GetFrame(O_MOBJ, mt_num, &mobj->deathstate);
		GetFrame(O_MOBJ, mt_num, &mobj->xdeathstate);
		GetSound(O_MOBJ, mt_num, &mobj->deathsound);

		GetInt  (O_MOBJ, mt_num, &mobj->speed);
		GetInt  (O_MOBJ, mt_num, &mobj->radius);
		GetInt  (O_MOBJ, mt_num, &mobj->height);
		GetInt  (O_MOBJ, mt_num, &mobj->mass);
		GetInt  (O_MOBJ, mt_num, &mobj->damage);
		GetSound(O_MOBJ, mt_num, &mobj->activesound);
		GetFlags(O_MOBJ, mt_num, &mobj->flags);

		if (doom_ver != 12)
			GetFrame(O_MOBJ, mt_num, &mobj->raisestate);
	}

	void ReadBinaryAmmo(void)
	{
		Debug_PrintMsg("\n--- ReadBinaryAmmo ---\n");

		if (file_error)
			FatalError("File error reading binary ammo table.\n");

		GetInt(O_AMMO, 0, Ammo::plr_max + 0);
		GetInt(O_AMMO, 1, Ammo::plr_max + 1);
		GetInt(O_AMMO, 2, Ammo::plr_max + 2);
		GetInt(O_AMMO, 3, Ammo::plr_max + 3);

		GetInt(O_AMMO, 0, Ammo::pickups + 0);
		GetInt(O_AMMO, 1, Ammo::pickups + 1);
		GetInt(O_AMMO, 2, Ammo::pickups + 2);
		GetInt(O_AMMO, 3, Ammo::pickups + 3);
	}

	void ReadBinaryWeapon(int wp_num)
	{
		Debug_PrintMsg("\n--- ReadBinaryWeapon %d ---\n", wp_num);

		if (file_error)
			FatalError("File error reading binary weapon table.\n");

		weaponinfo_t *weap = weapon_info + wp_num;

		GetAmmoType(O_WEAPON, wp_num, &weap->ammo);

		GetFrame(O_WEAPON, wp_num, &weap->upstate);
		GetFrame(O_WEAPON, wp_num, &weap->downstate);
		GetFrame(O_WEAPON, wp_num, &weap->readystate);
		GetFrame(O_WEAPON, wp_num, &weap->atkstate);
		GetFrame(O_WEAPON, wp_num, &weap->flashstate);
	}

	void ReadBinaryFrame(int st_num)
	{
		Debug_PrintMsg("\n--- ReadBinaryFrame %d ---\n", st_num);

		if (file_error)
			FatalError("File error reading binary frame table.\n");

		state_t *state = states + st_num;

		GetSprite(O_FRAME, st_num, &state->sprite);
		GetInt   (O_FRAME, st_num, &state->frame);
		GetInt   (O_FRAME, st_num, &state->tics);

		GetRawInt();  // ignore code-pointer
		
		GetFrame(O_FRAME, st_num, &state->nextstate);
		GetInt  (O_FRAME, st_num, &state->misc1);
		GetInt  (O_FRAME, st_num, &state->misc2);

		if (state->misc1 || state->misc2)
			PrintWarn("frame %d has non-zero misc fields.\n", st_num);
	}

	void ReadBinarySound(int s_num)
	{
		Debug_PrintMsg("\n--- ReadBinarySound %d ---\n", s_num);

		if (file_error)
			FatalError("File error reading binary sprite table.\n");

		sfxinfo_t *sfx = S_sfx + s_num;

		GetRawInt();  // ignore sound name pointer
		GetRawInt();  // ignore singularity

		GetInt(O_SOUND, s_num, &sfx->priority);

		GetRawInt();  // ignore link pointer
		GetRawInt();  // ignore link pitch
		GetRawInt();  // ignore link volume

		GetRawInt();  // 
		GetRawInt();  // unused stuff
		GetRawInt();  // 
	}

	void ReadBinarySprite(int spr_num)
	{
		Debug_PrintMsg("\n--- ReadBinarySprite %d ---\n", spr_num);

		if (file_error)
			FatalError("File error reading binary sprite table.\n");

		GetRawInt();  // ignore sprite name pointer
	}

	void ReadBinaryText(int tx_num)
	{
		Debug_PrintMsg("\n--- ReadBinaryText %d ---\n", tx_num);

		if (file_error)
			FatalError("File error reading binary text table.\n");

		static char text_buf[MAX_TEXT_STR+8];

		GetRawString(text_buf, MAX_TEXT_STR);

		// Debug_PrintMsg("\"%s\"\n", PrettyTextString(text_buf));

		TextStr::ReplaceBinaryString(tx_num, text_buf);
	}

	dehret_e LoadReallyOld(void)
	{
		char tempfmt = 0;

		pat_buf->read(&tempfmt, 1);

		if (tempfmt < 1 || tempfmt > 2)
		{
			SetErrorMsg("Bad format byte in DeHackEd patch file.\n"
				"[Really old patch, format byte %d]\n", tempfmt);
			return DEH_E_ParseError;
		}

		patch_fmt = (int)tempfmt;
		doom_ver = 12;
		dhe_ver = 11 + patch_fmt;

		DetectMsg("really old");
		VersionMsg();
	
		pat_buf->showProgress();

		int j;

		for (j = 0; j < THINGS_1_2; j++)
			ReadBinaryThing(thing12to166[j]);

		pat_buf->showProgress();

		ReadBinaryAmmo();

		for (j = 0; j < 8; j++)
			ReadBinaryWeapon(j);  // no need to convert

		pat_buf->showProgress();

		if (patch_fmt == 2)
		{
			for (j = 0; j < FRAMES_1_2; j++)
				ReadBinaryFrame(frame12to166[j]);
		}

		return DEH_OK;
	}

	dehret_e LoadBinary(void)
	{
		char tempdoom = 0;
		char tempfmt  = 0;

		pat_buf->read(&tempdoom, 1);
		pat_buf->read(&tempfmt,  1);

		if (tempfmt == 3)
		{
			SetErrorMsg("Doom 1.6 beta patches are not supported.\n");
			return DEH_E_ParseError;
		}
		else if (tempfmt != 4)
		{
			SetErrorMsg("Bad format byte in DeHackEd patch file.\n"
				"[Binary patch, format byte %d]\n", tempfmt);
			return DEH_E_ParseError;
		}

		patch_fmt = 4;

		if (tempdoom != 12 && ! (tempdoom >= 16 && tempdoom <= 21))
		{
			SetErrorMsg("Bad Doom release number in patch file !\n"
				"[Binary patch, release number %d]\n", tempdoom);
			return DEH_E_ParseError;
		}

		doom_ver = (int)tempdoom;

		DetectMsg("binary");
		VersionMsg();

		pat_buf->showProgress();

		int j;

		if (doom_ver == 12)
		{
			for (j = 0; j < THINGS_1_2; j++)
				ReadBinaryThing(thing12to166[j]);
		}
		else
		{
			for (j = 0; j < NUMMOBJTYPES; j++)
				ReadBinaryThing(j);
		}

		pat_buf->showProgress();

		ReadBinaryAmmo();

		int num_weap = (doom_ver == 12) ? 8 : 9;

		for (j = 0; j < num_weap; j++)
			ReadBinaryWeapon(j);

		pat_buf->showProgress();

		if (doom_ver == 12)
		{
			for (j = 0; j < FRAMES_1_2; j++)
				ReadBinaryFrame(frame12to166[j]);
		}
		else
		{
			/* -AJA- NOTE WELL: the "- 1" here.  Testing confirms that the
			 * DeHackEd code omits the very last frame from the V1.666+
			 * binary format.  The V1.2 binary format is fine though.
			 */
			for (j = 0; j < NUMSTATES - 1; j++)
				ReadBinaryFrame(j);
		}

		pat_buf->showProgress();

		if (doom_ver == 12)
		{
			// Note: this V1.2 sound/sprite handling UNTESTED.  I'm not even
			// sure that there exists any such DEH patch files.

			for (j = 1; j < SOUNDS_1_2; j++)
				ReadBinarySound(sound12to166[j]);

			for (j = 0; j < SPRITES_1_2; j++)
				ReadBinarySprite(sprite12to166[j]);
		}
		else
		{
			/* -AJA- NOTE WELL: we start at one, as DEH patches don't
			 * include the dummy entry.  More important: the "- 1" here,
			 * the very last sound is "DSRADIO" which is omitted from the
			 * patch file.  Confirmed through testing.
			 */
			for (j = 1; j < NUMSFX - 1; j++)
				ReadBinarySound(j);

			for (j = 0; j < NUMSPRITES; j++)
				ReadBinarySprite(j);
		}

		pat_buf->showProgress();

		if (doom_ver == 16 || doom_ver == 17)
		{
			// -AJA- starts at one simply to match v166_index
			for (j = 1; j <= TEXTS_1_6; j++)
				ReadBinaryText(j);
		}

		return DEH_OK;
	}

	//------------------------------------------------------------------------

	typedef enum
	{
		// patch format 5:
		DEH_THING, DEH_SOUND, DEH_FRAME, DEH_SPRITE, DEH_AMMO, DEH_WEAPON,
		/* DEH_TEXT handled specially */

		// patch format 6:
		DEH_PTR, DEH_CHEAT, DEH_MISC,

		// boom extensions:
		BEX_HELPER, BEX_STRINGS, BEX_PARS, BEX_CODEPTR,
		BEX_SPRITES, BEX_SOUNDS, BEX_MUSIC,

		NUMSECTIONS
	}
	sectionkind_e;

	const char *section_name[] =
	{
		"Thing",   "Sound", "Frame", "Sprite", "Ammo", "Weapon",  
		"Pointer", "Cheat", "Misc",

		// Boom extensions:
		"[HELPER]", "[STRINGS]", "[PARS]", "[CODEPTR]",
		"[SPRITES]", "[SOUNDS]", "[MUSIC]"
	};

	char line_buf[MAX_LINE+4];
	int  line_num;

	char *equal_pos;

	int active_section = -1;
	int active_obj = -1;

	const char *cur_txt_ptr;
	bool syncing;

	void GetNextLine(void)
	{
		int len = 0;
		equal_pos = NULL;

		for (;;)
		{
			int ch = pat_buf->getch();

			if (ch == EOF)
			{
				if (pat_buf->error())
					PrintWarn("Read error on input file.\n");

				break;
			}

			// end-of-line detection.  We support the following conventions:
			//    1. CR LF    (MSDOS/Windows)
			//    2. LF only  (Unix)
			//    3. CR only  (Macintosh)

			if (ch == '\n')
				break;

			if (ch == '\r')
			{
				ch = pat_buf->getch();

				if (ch != EOF && ch != '\n')
					pat_buf->ungetch(ch);

				break;
			}

			if (len >= MAX_LINE)  // truncation mode
				continue;

			if (! equal_pos && (char)ch == '=')
				equal_pos = line_buf + len;

			line_buf[len++] = (char)ch;

			if (len == MAX_LINE)
				PrintWarn("Truncating very long line (#%d).\n", line_num);
		}

		line_buf[len] = 0;
		line_num++;
	}

	void StripTrailingSpace(void)
	{
		int len = strlen(line_buf);

		while (len > 0 && isspace(line_buf[len - 1]))
			len--;

		line_buf[len] = 0;
	}

	bool ValidateObject(void)
	{
		int min_obj = 0;
		int max_obj = 0;

		if (active_section == DEH_MISC || active_section == DEH_CHEAT ||
			active_section == DEH_SPRITE)
		{
			return true;  /* don't care */
		}

		if (patch_fmt <= 5)
		{
			switch (active_section)
			{
				case DEH_THING:  min_obj = 1; max_obj = NUMMOBJTYPES; break;

				case DEH_SOUND:  max_obj = NUMSFX - 1; break;
				case DEH_FRAME:  max_obj = NUMSTATES - 1; break;
				case DEH_AMMO:   max_obj = NUMAMMO - 1; break;
				case DEH_WEAPON: max_obj = NUMWEAPONS - 1; break;
				case DEH_PTR:    max_obj = POINTER_NUM - 1; break;

				default:
					InternalError("Bad active_section value %d\n", active_section);
			}
		}
		else /* patch_fmt == 6, allow BOOM/MBF stuff */
		{
			switch (active_section)
			{
				case DEH_THING:  min_obj = 1; max_obj = NUMMOBJTYPES_BEX; break;

				case DEH_SOUND:  max_obj = NUMSFX_BEX - 1; break;
				case DEH_FRAME:  max_obj = NUMSTATES_BEX - 1; break;
				case DEH_AMMO:   max_obj = NUMAMMO - 1; break;
				case DEH_WEAPON: max_obj = NUMWEAPONS - 1; break;
				case DEH_PTR:    max_obj = POINTER_NUM_BEX - 1; break;

				default:
					InternalError("Bad active_section value %d\n", active_section);
			}
		}

		if (active_obj < min_obj || active_obj > max_obj)
		{
			PrintWarn("Line %d: Illegal %s number: %d.\n",
				line_num, section_name[active_section], active_obj);

			syncing = true;
			return false;
		}

		return true;
	}

	bool CheckNewSection(void)
	{
		int i;
		int obj_num;

		for (i = 0; i < NUMSECTIONS; i++)
		{
			if (StrCaseCmpPartial(line_buf, section_name[i]) != 0)
				continue;

			// make sure no '=' appears (to prevent a mismatch with
			// DEH ^Frame sections and BEX CODEPTR ^FRAME lines).
			for (const char *pos = line_buf; *pos && *pos != '('; pos++)
				if (*pos == '=')
					return false;

			if (line_buf[0] == '[')
			{
				active_section = i;
				active_obj = -1;  // unused

				if (active_section == BEX_PARS || active_section == BEX_HELPER)
				{
					PrintWarn("Ignoring BEX %s section.\n", section_name[i]);
				}

				return true;
			}

			int sec_len = strlen(section_name[i]);

			if (! isspace(line_buf[sec_len]))
				continue;

			if (sscanf(line_buf + sec_len, " %i ", &obj_num) != 1)
				continue;

			active_section = i;
			active_obj = obj_num;

			return ValidateObject();
		}

		return false;
	}

	void ReadTextString(char *dest, int len)
	{
		assert(cur_txt_ptr);

		char *begin = dest;

		int start_line = line_num;

		while (len > 0)
		{
			if ((dest - begin) >= MAX_TEXT_STR)
			{
				FatalError("Text string exceeds internal buffer length.\n"
					"[> %d characters, starting on line %d]\n",
					MAX_TEXT_STR, start_line);
			}
			
			if (*cur_txt_ptr)
			{
				*dest++ = *cur_txt_ptr++;  len--;
				continue;
			}

			if (pat_buf->eof())
				FatalError("End of file while reading Text replacement.\n");

			GetNextLine();
			cur_txt_ptr = line_buf;

			*dest++ = '\n';  len--;
		}

		*dest = 0;
	}

	void ProcessTextSection(int len1, int len2)
	{
		Debug_PrintMsg("TEXT REPLACE: %d %d\n", len1, len2);

		static char text_1[MAX_TEXT_STR+8];
		static char text_2[MAX_TEXT_STR+8];

		GetNextLine();

		cur_txt_ptr = line_buf;

		ReadTextString(text_1, len1);
		ReadTextString(text_2, len2);

		Debug_PrintMsg("- Before <%s>\n", text_1);
		Debug_PrintMsg("- After  <%s>\n", text_2);

		if (len1 == 4 && len2 == 4)
			if (TextStr::ReplaceSprite(text_1, text_2))
				return;

		if (len1 < 7 && len2 < 7)
		{
			if (Sounds::ReplaceSound(text_1, text_2))
				return;

			if (Sounds::ReplaceMusic(text_1, text_2))
				return;
		}

		if (TextStr::ReplaceString(text_1, text_2))
			return;

		PrintWarn("Cannot match text: \"%s\"\n",
			PrettyTextString(text_1));
	}

	void ReadBexTextString(char *dest)  // upto MAX_TEXT_STR chars
	{
		assert(cur_txt_ptr);

		char *begin = dest;

		int start_line = line_num;

		for (;;)
		{
			if ((dest - begin) >= MAX_TEXT_STR)
			{
				FatalError("Bex String exceeds internal buffer length.\n"
					"[> %d characters, starting on line %d]\n",
					MAX_TEXT_STR, start_line);
			}

			if (*cur_txt_ptr == 0)
				break;

			// handle the newline sequence
			if (cur_txt_ptr[0] == '\\' && tolower(cur_txt_ptr[1]) == 'n')
			{
				cur_txt_ptr += 2;
				*dest++ = '\n';
				continue;
			}

			if (cur_txt_ptr[0] == '\\' && cur_txt_ptr[1] == 0)
			{
				do  // need a loop to ignore comment lines
				{
					if (pat_buf->eof())
						FatalError("End of file while reading Bex String replacement.\n");

					GetNextLine();
					StripTrailingSpace();
				}
				while (line_buf[0] == '#');

				cur_txt_ptr = line_buf;

				// strip leading whitespace from continuing lines
				while (isspace(*cur_txt_ptr))
					cur_txt_ptr++;

				continue;
			}

			*dest++ = *cur_txt_ptr++;
		}

		*dest = 0;
	}

	void ProcessBexString(void)
	{
		Debug_PrintMsg("BEX STRING REPLACE: %s\n", line_buf);

		if (strlen(line_buf) >= 100)
			FatalError("Bex string name too long !\nLine %d: %s\n",
				line_num, line_buf);

		char bex_field[104];

		strcpy(bex_field, line_buf);

		static char text_buf[MAX_TEXT_STR+8];

		cur_txt_ptr = equal_pos;

		ReadBexTextString(text_buf);

		Debug_PrintMsg("- Replacement <%s>\n", text_buf);

		if (! TextStr::ReplaceBexString(bex_field, text_buf))
			PrintWarn("Line %d: unknown BEX string name: %s\n",
				line_num, bex_field);
	}

	void ProcessLine(void)
	{
		assert(active_section >= 0);

		Debug_PrintMsg("Section %d Object %d : <%s>\n", active_section,
			active_obj, line_buf);

		if (active_section == BEX_PARS || active_section == BEX_HELPER)
			return;

		if (! equal_pos)
		{
			PrintWarn("Ignoring line: %s\n", line_buf);
			return;
		}

		if (active_section == BEX_STRINGS)
		{
			// this is needed for compatible handling of trailing '\'
			StripTrailingSpace();
		}

		// remove whitespace around '=' sign

		char *final = equal_pos;

		while (final > line_buf && isspace(final[-1]))
			final--;

		*final = 0;

		equal_pos++;

		while (*equal_pos && isspace(*equal_pos))
			equal_pos++;

		if (line_buf[0] == 0)
		{
			PrintWarn("Line %d: No field name before equal sign.\n", line_num);
			return;
		}
		else if (equal_pos[0] == 0)
		{
			PrintWarn("Line %d: No value after equal sign.\n", line_num);
			return;
		}

		if (patch_fmt >= 6 && active_section == DEH_THING &&
			StrCaseCmp(line_buf, "Bits") == 0)
		{
			Things::AlterBexBits(equal_pos);
			return;
		}

		int num_value = 0;

        if (active_section != DEH_CHEAT && active_section <= BEX_HELPER)
        {
            if (sscanf(equal_pos, " %i ", &num_value) != 1)
            {
                PrintWarn("Line %d: unreadable %s value: %s\n",
					line_num, section_name[active_section], equal_pos);
                return;
            }
        }

		switch (active_section)
		{
			case DEH_THING:  Things:: AlterThing(num_value);   break;
			case DEH_SOUND:  Sounds:: AlterSound(num_value);   break;
			case DEH_FRAME:  Frames:: AlterFrame(num_value);   break;
			case DEH_AMMO:   Ammo::   AlterAmmo(num_value);    break;
			case DEH_WEAPON: Weapons::AlterWeapon(num_value);  break;
			case DEH_PTR:    Frames:: AlterPointer(num_value); break;
			case DEH_MISC:   Misc::   AlterMisc(num_value);    break;

			case DEH_CHEAT:  TextStr::AlterCheat(equal_pos);   break;
			case DEH_SPRITE: /* ignored */ break;

			case BEX_CODEPTR: Frames::AlterBexCodePtr(equal_pos); break;
			case BEX_STRINGS: ProcessBexString(); break;

			case BEX_SOUNDS:  Sounds:: AlterBexSound(equal_pos);  break;
			case BEX_MUSIC:   Sounds:: AlterBexMusic(equal_pos);  break;
			case BEX_SPRITES: TextStr::AlterBexSprite(equal_pos); break;

			default:
				InternalError("Bad active_section value %d\n", active_section);
		}
	}

	dehret_e LoadDiff(bool no_header)
	{
		// set these to defaults
		doom_ver  = no_header ? 19 : 16;
		patch_fmt = no_header ? 6 : 5;

		line_num = 0;

		bool got_info = false;

		syncing = true;

		while (! pat_buf->eof())
		{
			pat_buf->showProgress();

			GetNextLine();

			if (line_buf[0] == 0 || line_buf[0] == '#')
				continue;

			// Debug_PrintMsg("LINE %d: <%s>\n", line_num, line_buf);

			if (StrCaseCmpPartial(line_buf, "Doom version") == 0)
			{
				if (! equal_pos)
				{
					SetErrorMsg("Badly formed directive !\nLine %d: %s\n",
						line_num, line_buf);
					return DEH_E_ParseError;
				}

				doom_ver = (int)strtol(equal_pos+1, NULL, 10);

				if (! (doom_ver == 12 ||
					  (doom_ver >= 16 && doom_ver <= 21)))
				{
					SetErrorMsg("Unknown doom version found: V%d.%d\n",
						doom_ver / 10, (doom_ver+1000) % 10);
					return DEH_E_ParseError;
				}

				// I don't think the DeHackEd code supports this correctly
				if (doom_ver == 12)
				{
					SetErrorMsg("Text patches for DOOM V1.2 are not supported.\n");
					return DEH_E_ParseError;
				}
			}

			if (StrCaseCmpPartial(line_buf, "Patch format") == 0)
			{
				if (got_info)
				{
					SetErrorMsg("Patch format is specified twice.\n");
					return DEH_E_ParseError;
				}

				got_info = true;

				if (! equal_pos)
				{
					SetErrorMsg("Badly formed directive !\nLine %d: %s\n",
						line_num, line_buf);
					return DEH_E_ParseError;
				}

				patch_fmt = (int)strtol(equal_pos+1, NULL, 10);

				if (patch_fmt < 5 || patch_fmt > 6)
				{
					SetErrorMsg("Unknown dehacked patch format found: %d\n",
						patch_fmt);
					return DEH_E_ParseError;
				}

				VersionMsg();
			}

			if (StrCaseCmpPartial(line_buf, "include") == 0)
			{
				SetErrorMsg("BEX INCLUDE directive is not supported.\n");
				return DEH_E_ParseError;
			}

			if (StrCaseCmpPartial(line_buf, "Text") == 0 &&
				isspace(line_buf[4]))
			{
				int len1, len2;

				if (sscanf(line_buf + 4, " %i %i ", &len1, &len2) == 2 &&
					len1 > 1)
				{
					ProcessTextSection(len1, len2);
					syncing = true;
					continue;
				}
			}

			if (CheckNewSection())
			{
				syncing = false;
				continue;
			}

			if (! syncing)
			{
				ProcessLine();
			}
		}

		return DEH_OK;
	}

	dehret_e LoadNormal(void)
	{
		char idstr[32];

		memset(idstr, 0, sizeof(idstr));

		pat_buf->read(idstr, 24);

		if (StrCaseCmp(idstr, "atch File for DeHackEd v") != 0)
		{
			SetErrorMsg("Not a DeHackEd patch file !\n");
			return DEH_E_ParseError;
		}

		memset(idstr, 0, 4);

		pat_buf->read(idstr, 3);

		if (! isdigit(idstr[0]) || idstr[1] != '.' || ! isdigit(idstr[2]))
		{
			SetErrorMsg("Bad version string in DeHackEd patch file.\n"
				"[String %s is not digit . digit]\n", idstr);
			return DEH_E_ParseError;
		}

		dhe_ver = (idstr[0] - '0') * 10 + (idstr[2] - '0');

		if (dhe_ver < 20 || dhe_ver > 31)
		{
			SetErrorMsg("This patch file has an incorrect version number !\n"
				"[Version %s]\n", idstr);
			return DEH_E_ParseError;
		}

		if (dhe_ver < 23)
			return LoadBinary();
		else
		{
			DetectMsg("text-based");
			return LoadDiff(false);
		}
	}
}

dehret_e Patch::Load(parse_buffer_api *buf)
{
	pat_buf = buf;
	assert(pat_buf);

	dehret_e result = DEH_OK;

	file_error = false;

	char tempver = pat_buf->getch();

	if (tempver == 12)
	{
		result = LoadReallyOld();
	}
	else if (tempver == 'P')
	{
		result = LoadNormal();
	}
	else if (! pat_buf->isBinary())
	{
		pat_buf->ungetch(tempver);

		PrintMsg("Missing header -- assuming text-based BEX patch !\n");
		dhe_ver = 31;
		result = LoadDiff(true);
	}
	else /* unknown binary format */
	{
		SetErrorMsg("Not a DeHackEd patch file !\n");
		result = DEH_E_ParseError;
	}

	PrintMsg("\n");
	pat_buf = NULL;

	return result;
}

}  // Deh_Edge
