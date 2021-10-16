//------------------------------------------------------------------------
//  STORAGE for modifications
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
#include "storage.h"

#include "dh_plugin.h"
#include "system.h"

#include <assert.h>
#include <stdio.h>

// DESCRIPTION:
//
//    The problem with binary patch files is they need to compare the
//    fields from the patch against the "standard" values to determine
//    exactly what the patch modified.  (like "DIFF").  So when an
//    earlier patch changes the base values, the later patch will not
//    do the right thing.
//
//    Two possible solutions: (a) make a read-only copy of all the base
//    values for doing the comparisons, or (b) don't modify the values
//    until after all patches are read in (i.e. remember what we need to
//    change).  I've opted for the second way.
//
//    Addendum: since DEH_EDGE can now be used as a plugin module, it
//    needs to remember the old values and restore them afterwards.
//    Luckily this doesn't require any more storage.

namespace Deh_Edge
{

#define ITEMS_PER_BOX  256


typedef struct
{
	int *target;
	int value;
}
storageitem_t;

typedef struct storagebox_s
{
	// next in linked-list
	struct storagebox_s *next;
	struct storagebox_s *prev;

	int used;
	
	storageitem_t items[ITEMS_PER_BOX];
}
storagebox_t;

namespace Storage
{
	storagebox_t *head;
	storagebox_t *tail;
}


//----------------------------------------------------------------------------


void Storage::Startup(void)
{
	head = tail = NULL;
}

void Storage::RememberMod(int *target, int value)
{
	if (! tail || tail->used >= ITEMS_PER_BOX)
	{
		storagebox_t *box = new storagebox_t;

		if (! box)
			FatalError("Out of memory (%d storage bytes)\n",
				(int)sizeof(storagebox_t));

		box->next = NULL;
		box->prev = tail;

		box->used = 0;

		if (tail)
			tail->next = box;
		else
			head = box;

		tail = box;
	}

	assert(tail);
	
	tail->items[tail->used].target = target;
	tail->items[tail->used].value  = value;

	tail->used++;
}

void Storage::ApplyAll(void)
{
	for (storagebox_t *box = head; box; box = box->next)
	{
		for (int i = 0; i < box->used; i++)
		{
			// remembers the old value, so we can restore it

			int old_value = box->items[i].target[0];

			box->items[i].target[0] = box->items[i].value;
			box->items[i].value = old_value;
		}
	}
}

void Storage::RestoreAll(void)
{
	// this also frees the list

	// NOTE: we go backwards (tail -> head), to handle aliases correctly

	while (tail)
	{
		storagebox_t *box = tail;
		tail = tail->prev;

		for (int i = box->used-1; i >= 0; i--)
		{
			box->items[i].target[0] = box->items[i].value;
		}

		delete box;
	}

	head = tail = NULL;
}

}  // Deh_Edge
