/*
    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

	 This program is free software; you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
	 (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	timidity.c
*/

#include "config.h"

#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "loadmidi.h"
#include "mix.h"
#include "ctrlmode.h"
#include "timidity.h"
#include "tables.h"

// pointer to conversion function
void (*s32tobuf)(void *dp, const s32_t *lp, int count);

int num_ochannels;

int free_instruments_afterwards=0;
static char def_instr_name[256]="";

#define MAXWORDS 10

static int read_config_file(const char *name)
{
	ToneBank *bank = NULL;

	static int recursion=0;

	if (recursion > 50)
	{
		ctl_msg(CMSG_ERROR, VERB_NORMAL,
				"Probable source loop in configuration files");
		return (-1);
	}

	FILE *fp = open_file_via_paths(name, OF_VERBOSE);

	if (! fp)
		return -1;

	int line=0;

	char tmp[1024];
	char *w[MAXWORDS];
	char *cp;
	
	while (fgets(tmp, sizeof(tmp), fp))
	{
		int words=0;

		line++;

		w[0] = strtok(tmp, " \t\r\n\240");

		if (!w[0] || (*w[0]=='#'))
			continue;

		while (w[words] && words < MAXWORDS)
		{
			w[++words] = strtok(0," \t\r\n\240");

			if (w[words] && w[words][0]=='#')
				break;
		}

		if (strcmp(w[0], "map") == 0)
			continue;

		if (strcmp(w[0], "opt") == 0)
			continue;

		if (strcmp(w[0], "dir") == 0)
		{
			if (words < 2)
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: No directory given\n", name, line);
				return -2;
			}

			for (int i=1; i < words; i++)
				add_to_pathlist(w[i]);
		}
		else if (0 == strcmp(w[0], "source"))
		{
			if (words < 2)
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: No file name given\n", name, line);
				return -2;
			}
			for (int i=1; i < words; i++)
			{
				recursion++;
				read_config_file(w[i]);
				recursion--;
			}
		}
		else if (0 == strcmp(w[0], "default"))
		{
			if (words != 2)
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL, 
						"%s: line %d: Must specify exactly one patch name\n",
						name, line);
				return -2;
			}
			strncpy(def_instr_name, w[1], 255);
			def_instr_name[255]='\0';
		}
		else if (0 == strcmp(w[0], "drumset"))
		{
			if (words < 2)
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: No drum set number given\n", 
						name, line);
				return -2;
			}

			int i = atoi(w[1]);

			if (i<0 || i>127)
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL, 
						"%s: line %d: Drum set must be between 0 and 127\n",
						name, line);
				return -2;
			}

			if (!drumset[i])
			{
				drumset[i] = (ToneBank*)safe_malloc(sizeof(ToneBank));
				memset(drumset[i], 0, sizeof(ToneBank));
			}
			bank=drumset[i];
		}
		else if (0 == strcmp(w[0], "bank"))
		{
			if (words < 2)
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: No bank number given\n", 
						name, line);
				return -2;
			}

			int i = atoi(w[1]);

			if (i<0 || i>127)
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL, 
						"%s: line %d: Tone bank must be between 0 and 127\n",
						name, line);
				return -2;
			}
			if (!tonebank[i])
			{
				tonebank[i] = (ToneBank*)safe_malloc(sizeof(ToneBank));
				memset(tonebank[i], 0, sizeof(ToneBank));
			}
			bank=tonebank[i];
		}
		else if (0 == strcmp(w[0], "soundfont"))
		{
			ctl_msg(CMSG_ERROR, VERB_NORMAL,
					"SoundFonts (SF2 files) are not supported.\n");
			return -2;
		}
		else
		{
			if ((words < 2) || (*w[0] < '0' || *w[0] > '9'))
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: syntax error\n", name, line);
				return -2;
			}
	
			int i = atoi(w[0]);
			
			if (i<0 || i>127)
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: Program must be between 0 and 127\n",
						name, line);
				return -2;
			}
			if (!bank)
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL, 
						"%s: line %d: Must specify tone bank or drum set before assignment\n",
						name, line);
				return -2;
			}
			if (bank->tone[i].name)
				free(bank->tone[i].name);

			bank->tone[i].name = safe_strdup(w[1]);

			bank->tone[i].note = -1;
			bank->tone[i].amp  = -1;
			bank->tone[i].pan  = -1;
			bank->tone[i].strip_loop = -1;
			bank->tone[i].strip_envelope = -1;
			bank->tone[i].strip_tail = -1;

			for (int j=2; j < words; j++)
			{
				if (! (cp=strchr(w[j], '=')))
				{
					ctl_msg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: bad patch option %s\n",
							name, line, w[j]);
					return -2;
				}

				*cp++=0;

				if (0 == strcmp(w[j], "amp"))
				{
					int k=atoi(cp);

					if ((k<0 || k>MAX_AMPLIFICATION) || (*cp < '0' || *cp > '9'))
					{
						ctl_msg(CMSG_ERROR, VERB_NORMAL, 
								"%s: line %d: amplification must be between 0 and %d\n", name, line, MAX_AMPLIFICATION);
						return -2;
					}
					bank->tone[i].amp=k;
				}
				else if (0 == strcmp(w[j], "note"))
				{
					int k=atoi(cp);

					if ((k<0 || k>127) || (*cp < '0' || *cp > '9'))
					{
						ctl_msg(CMSG_ERROR, VERB_NORMAL, 
								"%s: line %d: note must be between 0 and 127\n",
								name, line);
						return -2;
					}
					bank->tone[i].note=k;
				}
				else if (0 == strcmp(w[j], "pan"))
				{
					int k;

					if (0 == strcmp(cp, "center"))
						k=64;
					else if (0 == strcmp(cp, "left"))
						k=0;
					else if (0 == strcmp(cp, "right"))
						k=127;
					else
						k=((atoi(cp)+100) * 100) / 157;

					if ((k<0 || k>127) ||
							(k==0 && *cp!='-' && (*cp < '0' || *cp > '9')))
					{
						ctl_msg(CMSG_ERROR, VERB_NORMAL, 
								"%s: line %d: panning must be left, right, center, or between -100 and 100\n",
								name, line);
						return -2;
					}
					bank->tone[i].pan=k;
				}
				else if (0 == strcmp(w[j], "keep"))
				{
					if (0 == strcmp(cp, "env"))
						bank->tone[i].strip_envelope=0;
					else if (0 == strcmp(cp, "loop"))
						bank->tone[i].strip_loop=0;
					else
					{
						ctl_msg(CMSG_ERROR, VERB_NORMAL,
								"%s: line %d: keep must be env or loop\n", name, line);
						return -2;
					}
				}
				else if (0 == strcmp(w[j], "strip"))
				{
					if (0 == strcmp(cp, "env"))
						bank->tone[i].strip_envelope=1;
					else if (0 == strcmp(cp, "loop"))
						bank->tone[i].strip_loop=1;
					else if (0 == strcmp(cp, "tail"))
						bank->tone[i].strip_tail=1;
					else
					{
						ctl_msg(CMSG_ERROR, VERB_NORMAL,
								"%s: line %d: strip must be env, loop, or tail\n",
								name, line);
						return -2;
					}
				}
				else
				{
					ctl_msg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: bad patch option %s\n",
							name, line, w[j]);
					return -2;
				}
			}
		}
	}

	if (ferror(fp))
	{
		ctl_msg(CMSG_ERROR, VERB_NORMAL, "Can't read from %s\n", name);

		fclose(fp);
		return -2;
	}

	fclose(fp);
	return 0;
}


bool Timidity_Init(const char *conf_file, int rate, int channels)
{
	switch (channels)
	{
		case 1: case 2: case 4: case 6:
			/* OK */
			break;

		default:
			return false;
	}

	num_ochannels = channels;

	init_pathlist();

	// add the directory containing the config file to pathlist
	add_basedir_to_pathlist(conf_file);

	if (read_config_file(conf_file) < 0)
		return false;

	/* Set play mode parameters */
	play_mode_rate = rate;
	play_mode_encoding = PE_16BIT | PE_SIGNED;

	if (channels == 1)
	{
		play_mode_encoding |= PE_MONO;
	} 

	// EDGE's blitter only needs s32_t samples.
	s32tobuf = s32tos16;

	if (!control_ratio)
	{
		control_ratio = play_mode_rate / CONTROLS_PER_SECOND;
		control_ratio = CLAMP(1, control_ratio, MAX_CONTROL_RATIO);
	}

	if (*def_instr_name)
		set_default_instrument(def_instr_name);

	return true; // OK
}

#if 0  // NOT USED
char timidity_error[1024] = "";

const char *Timidity_Error(void)
{
  return(timidity_error);
}
#endif

//------------------------------------------------------------------------

/* export the playback mode */

int play_mode_rate = DEFAULT_RATE;
int play_mode_encoding = PE_16BIT|PE_SIGNED;

int ctl_verbosity = VERB_NORMAL;

void ctl_msg(int type, int verbosity, const char *fmt, ...)
{
	va_list ap;

	if ((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
		(verbosity > ctl_verbosity))
		return;

	char buffer[2048];

	buffer[2047] = 0;

	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);

	if (verbosity >= VERB_DEBUG)
		I_Debugf("Timidity: %s\n", buffer);
	else
		I_Printf("Timidity: %s\n", buffer);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
