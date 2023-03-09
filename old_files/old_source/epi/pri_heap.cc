//----------------------------------------------------------------------------
//  Priority Heap
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2005-2008 The EDGE Team.
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

#include "epi.h"

#include "pri_heap.h"

// #define TEST_HEAP  1


priority_heap_c::priority_heap_c(int _size)
{
	total_size = _size;

	heap = new int[total_size];
	map  = new int[total_size];

	Clear();
}

priority_heap_c::~priority_heap_c()
{
	delete[] heap;
	delete[] map;
}

void priority_heap_c::Clear()
{
	heap_size = 0;

	Z_Clear(map, int, total_size);
}

bool priority_heap_c::IsMapped(int node)
{
	return map[node] != 0;
}

void priority_heap_c::Insert(int node)
{
	SYS_ASSERT(heap_size < total_size);  // overflow ?
	SYS_ASSERT(! IsMapped(node));

	heap[heap_size] = node;
	map[node] = heap_size + 1;

	heap_size++;

	Adjust(heap_size-1);

#ifdef TEST_HEAP
	Validate(0);
#endif
}

int priority_heap_c::RemoveHead()
{
	SYS_ASSERT(heap_size > 0);

	int node = heap[0];

	SYS_ASSERT(IsMapped(node));

	if (heap_size > 1)
	{
		// move last entry to top, then adjust (it trickles down)
		Swap(0, heap_size-1);

		heap_size--;

		Adjust(0);
	}
	else
	{
		heap_size--;
	}

#ifdef TEST_HEAP
	Validate(0);
#endif

	map[node] = 0;

	return node;
}

void priority_heap_c::ChangeScore(int node)
{
	SYS_ASSERT(IsMapped(node));

	Adjust(map[node] - 1);

#ifdef TEST_HEAP
	Validate(0);
#endif
}

void priority_heap_c::Adjust(int h_idx)
{
	for (;;)
	{
		// check for trickle up
		if (h_idx > 0)
		{
			if (ScoreAt(h_idx) > ScoreAt(Par(h_idx)))
			{
				Swap(h_idx, Par(h_idx));

				h_idx = Par(h_idx);
				continue;
			}
		}

		// check for trickle down
		if (HasLeft(h_idx))
		{
			int score = ScoreAt(h_idx);

			int left_sc = ScoreAt(Left(h_idx));

			if (HasRight(h_idx))
			{
				int right_sc = ScoreAt(Right(h_idx));

				if (score < right_sc && right_sc >= left_sc)
				{
					Swap(h_idx, Right(h_idx));

					h_idx = Right(h_idx);
					continue;
				}
			}

			if (score < left_sc)
			{
				Swap(h_idx, Left(h_idx));

				h_idx = Left(h_idx);
				continue;
			}
		}

		// everything was OK
		break;
	}
}

void priority_heap_c::DebugDump(int h_idx, int level)
{
	if (h_idx == 0 && IsEmpty())
	{
		I_Debugf("HEAP EMPTY\n");
		return;
	}

	static char buf[2048];

	buf[0] = 0;

	for (int k = 0; k < level-1; k++)
		strcat(buf, "|   ");

	if (level > 0)
		strcat(buf, "|__ ");

	I_Debugf("%s%d (node %d)\n", buf, ScoreAt(h_idx), heap[h_idx]);

	if (HasLeft(h_idx))
		DebugDump(Left(h_idx), level+1);

	if (HasRight(h_idx))
		DebugDump(Right(h_idx), level+1);
}

void priority_heap_c::Validate(int h_idx)
{
	if (h_idx == 0 && IsEmpty())
		return;
	
	SYS_ASSERT(h_idx < heap_size);

	if (HasLeft(h_idx))
	{
		if (ScoreAt(Left(h_idx)) > ScoreAt(h_idx))
		{
			DebugDump();
			I_Error("priority_heap_c: Validate failed at %d (< L)\n", h_idx); 
		}

		Validate(Left(h_idx));
	}

	if (HasRight(h_idx))
	{
		if (ScoreAt(Right(h_idx)) > ScoreAt(h_idx))
		{
			DebugDump();
			I_Error("priority_heap_c: Validate failed at %d (< R)\n", h_idx); 
		}

		Validate(Right(h_idx));
	}
}

#ifdef TEST_HEAP
void TestHeap(void)
{
	srand(123);

	I_Debugf("========================================\n");
	I_Debugf("  TEST HEAP\n");
	I_Debugf("========================================\n");

	I_Debugf("Insertion test:\n");

	priority_heap_c *heap = new priority_heap_c(1024);

	for (int i=0; i < 32; i++)
	{
		int node = rand() & 1023;

		if (heap->IsMapped(node))
			continue;

		I_Debugf("  Adding #%i, node %d, score %d\n", i, node, heap->Score(node));

		heap->Insert(node);
	}

	I_Debugf("\nDump:\n");

	heap->DebugDump();

	I_Debugf("\nRemoval test:\n");

	while (! heap->IsEmpty())
	{
		int node = heap->RemoveHead();

		I_Debugf("  Removed node %d, score %d\n", node, heap->Score(node));
	}

	I_Debugf("\nTest completely successfully !\n");
	I_Debugf("========================================\n");

	delete heap;
}
#endif

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
