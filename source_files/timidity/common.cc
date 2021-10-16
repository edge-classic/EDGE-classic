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

   common.c
*/

#include "config.h"

#include <errno.h>

#include "common.h"
#include "mix.h"
#include "ctrlmode.h"


char current_filename[1024];

static PathList *pathlist=NULL;


static bool filename_absolute(const char *name)
{
	if (name[0] == DIRSEPARATOR)
		return true;

	if (isalpha(name[0]) && name[1] == ':')
		return true;

	return false;
}


/* This is meant to find and open files for reading, possibly piping
   them through a decompressor. */
FILE *open_file_via_paths(const char *name, int noise_mode)
{
	PathList *plp;

	if (!name || !(*name))
	{
		ctl_msg(CMSG_ERROR, VERB_NORMAL, "Attempted to open nameless file.");
		return 0;
	}

	/* First try the given name */

	strncpy(current_filename, name, 1023);
	current_filename[1023]='\0';

	ctl_msg(CMSG_INFO, VERB_DEBUG, "Trying to open %s", current_filename);

	FILE *fp = fopen(current_filename, "rb");
	if (fp)
		return fp;

#ifdef ENOENT
	if (noise_mode && (errno != ENOENT))
	{
		ctl_msg(CMSG_ERROR, VERB_NORMAL, "%s: %s", 
				current_filename, strerror(errno));
		return 0;
	}
#endif

	if (! filename_absolute(name))
	{
		/* Try along the path then */
		for (plp = pathlist; plp; plp = plp->next)
		{
			current_filename[0] = 0;

			int l = strlen(plp->path);
			if (l > 0)
			{
				strcpy(current_filename, plp->path);

				if (current_filename[l-1] != DIRSEPARATOR)
				{
					current_filename[l]   = DIRSEPARATOR;
					current_filename[l+1] = 0;
				}
			}
			
			strcat(current_filename, name);

			ctl_msg(CMSG_INFO, VERB_DEBUG, "Trying to open %s", current_filename);

			fp = fopen(current_filename, "rb");
			if (fp)
				return fp;

#ifdef ENOENT
			if (noise_mode && (errno != ENOENT))
			{
				ctl_msg(CMSG_ERROR, VERB_NORMAL, "%s: %s", 
						current_filename, strerror(errno));
				return 0;
			}
#endif
		}
	}

	/* Nothing could be opened. */

	*current_filename=0;

	if (noise_mode >= 2)
		ctl_msg(CMSG_ERROR, VERB_NORMAL, "%s: %s", name, strerror(errno));

	return NULL;
}

FILE *open_via_paths_NOCASE(const char *name, int noise_mode)
{
	FILE *fp = open_file_via_paths(name, noise_mode);
	if (fp)
		return fp;

#ifndef WIN32
	char *u_name = safe_strdup(name);
	char *p;
	for (p = u_name; *p; p++)
		*p = toupper(*p);

	fp = open_file_via_paths(u_name, noise_mode);
	free(u_name);

	if (fp)
		return fp;
#endif

#ifndef WIN32
	char *l_name = safe_strdup(name);
	for (p = l_name; *p; p++)
		*p = tolower(*p);

	fp = open_file_via_paths(l_name, noise_mode);
	free(l_name);

	if (fp)
		return fp;
#endif

	return NULL;
}


/* This is meant for skipping a few bytes in a file or fifo. */
void skip(FILE *fp, size_t len)
{
	size_t c;
	char tmp[1024];

	while (len>0)
	{
		c=len;
		if (c>1024) c=1024;
		len-=c;
		if (c!=fread(tmp, 1, c, fp))
			ctl_msg(CMSG_ERROR, VERB_NORMAL, "%s: skip: %s",
					current_filename, strerror(errno));
	}
}

/* This'll allocate memory or die. */
void *safe_malloc(size_t count)
{
	if (count > (1<<21))
	{
		I_Error("Timidity felt like allocating %d bytes. This must be a bug.\n", count);
		/* NOT REACHED */
	}

	void *p = malloc(count);

	if (! p)
	{
		I_Error("Timidity: Couldn't malloc %d bytes.\n", count);
		/* NOT REACHED */
	}

	return p;
}

char *safe_strdup(const char *orig)
{
	if (! orig) orig = "";

	int len = strlen(orig);

	char *result = (char *)safe_malloc(len+1);

	strcpy(result, orig);;

	return result;
}


void init_pathlist(void)
{
	pathlist = NULL;

#ifdef DEFAULT_PATH
	/* Generate path list */
	add_to_pathlist(DEFAULT_PATH);
#endif
}

/* This adds a directory to the path list */
void add_to_pathlist(const char *s)
{
// fprintf(stderr, "add_to_pathlist: '%s'\n", s);

	// no need to add the current directory
	if (strcmp(s, ".") == 0)
		return;

	PathList *pl = (PathList*) safe_malloc(sizeof(PathList));
	pl->path = safe_strdup(s);

	// add to head of list (will be checked first)
	pl->next = pathlist;
	pathlist = pl;
}

void add_basedir_to_pathlist(const char *s)
{
	const char *pos = s + strlen(s) - 1;

	for (; pos > s; pos--)
	{
		if (*pos == '/')
			break;

#ifdef WIN32
		if (*pos == '\\')
			break;

		if (*pos == ':')
			return;
#endif
	}

	// no directory separator from 2nd character onwards
	if (pos <= s)
		return;

	char *dir = safe_strdup(s);
	dir[pos - s] = 0;

	add_to_pathlist(dir);

	free(dir);
}

/* Free memory associated to path list */
void free_pathlist(void)
{
	while (pathlist)
	{
		PathList *pl = pathlist;
		pathlist = pl->next;

		free(pl->path);
		free(pl);
	}
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
