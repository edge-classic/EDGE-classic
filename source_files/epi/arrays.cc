//----------------------------------------------------------------------------
//  EDGE Array Class
//----------------------------------------------------------------------------
//
//  Copyright (c) 2002-2008  The EDGE Team.
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
#include "arrays.h"

#include <string.h>

//void I_Printf(const char *message,...) GCCATTR(format(printf, 1, 2));

#define INITIAL_ARRAY_SIZE 2

namespace epi
{

	//
	// array_c constructor
	//
	array_c::array_c(const int objsize) :
		array(NULL), array_end(NULL), array_entries(0), array_max_entries(0)
	{
		// Size to the nearest block
		array_block_objsize = 
			(objsize+(sizeof(array_block_t)-1))/sizeof(array_block_t);

		// Need later for memcpy()'s
		array_objsize = objsize;
	}

	//
	// array_c destructor
	//
	array_c::~array_c()
	{
	}

	//
	// array_c::ExpandAtTail()
	//
	// Expands the array by one element and
	// returns a pointer that looks at that new data
	//
	void* array_c::ExpandAtTail()
	{
		if (array_entries >= array_max_entries)
		{
			// FIXME: Size should throw an error
			if (!Size(array_entries?(array_entries*2):INITIAL_ARRAY_SIZE))
	   			return NULL;
		}	
	
		// Record 
		void* p = (void*)array_end;
		
		IncrementCount();
		
		return p;
	}

	//
	// array_c::FetchObject
	//
	void* array_c::FetchObject(int pos) const
	{
		// Only check that the pos is valid within the list
		if (pos < 0 || pos >= array_max_entries)
			return NULL;

		return (void*)FetchObjectDirect(pos);
	}

	//
	// array_c::InsertObject
	//
	int array_c::InsertObject(void *obj, int pos)
	{
		if (!obj)
			return -1;
	       
		// Any invalid position translates to stick on the end
		if (pos<0 || pos>=array_entries)
			pos = array_entries;

		if (array_entries >= array_max_entries)
		{
       		if (!Size(array_entries?(array_entries*2):INITIAL_ARRAY_SIZE))
           		return -1;
		}

		if (pos < array_entries)
		{
			memmove(FetchObjectDirect(pos+1), 
					FetchObjectDirect(pos), 
					sizeof(array_block_t)*
						array_block_objsize*(array_entries-pos));
		}

		memcpy(FetchObjectDirect(pos), obj, array_objsize);
		IncrementCount();
		return pos;
	}

	//
	// array_c::RemoveObject
	//
	void array_c::RemoveObject(int pos)
	{
		if (pos<0 || pos>=array_entries)
			return;

		CleanupObject(FetchObjectDirect(pos));

		// Reduce the count here, we can do
		// a simpler check on the memmove()
		DecrementCount();

		if (pos < array_entries)
		{
			memmove(FetchObjectDirect(pos), 
					FetchObjectDirect(pos+1), 
					sizeof(array_block_t)*
						array_block_objsize*(array_entries-pos));
		}
		
		return;
	}

	//
	// array_iterator_c GetBaseIterator()
	//
	array_iterator_c array_c::GetBaseIterator(void)
	{
		array_iterator_c base_iterator(this, 0);
		return base_iterator;
	}

	//
	// array_iterator_c GetIterator()
	//
	// -ACB- 2004/06/08 Added
	//
	array_iterator_c array_c::GetIterator(int pos)
	{
		array_iterator_c iterator(this, pos);
		return iterator;
	}

	//
	// array_iterator_c GetTailIterator()
	//
	// -ACB- 2004/06/03 Added
	//
	array_iterator_c array_c::GetTailIterator(void)
	{
		array_iterator_c tail_iterator(this, 
                                       array_entries>1?array_entries-1:0);

		return tail_iterator;
	}

	//
	// array_c::Clear
	//
	void array_c::Clear(void)
	{
		if (array)
		{
			while(array_entries>0)
			{
				CleanupObject(FetchObjectDirect(array_entries-1));
				array_entries--;
			}
	        
			delete [] array;
	        
			array = NULL;
			array_end = NULL;
			array_entries = 0;
			array_max_entries = 0;
		}
	}

	//
	// array_c::Size
	//
	bool array_c::Size(int entries)
	{
		array_block_t *newarray;

		if (entries <= 0)
			return false;

		newarray = new array_block_t[array_block_objsize*entries];
		if (!newarray)
			return false;

		// if there is any existing data, then copy from old data
		if (array_entries>0)
		{
			int datasize;
	        
			datasize = (entries>array_entries)?array_entries:entries;
			datasize *= array_block_objsize;
			datasize *= sizeof(array_block_t);
			memcpy(newarray, array, datasize);

            //I_Printf("[Size] %d => %d\n",array_entries,entries);

			// Remove any entries that get nuked by the resize
			while (array_entries>entries)
			{
				CleanupObject(FetchObjectDirect(array_entries-1));
				array_entries--;
			}

			delete [] array;
		}
	    
		// Set the new array and the new max
		array = newarray;    
		array_max_entries = entries;

		// Update the end pointer
		SetCount(array_entries);
		return true;
	}

	//
	// array_c::ShrinkAtTail()
	//
	// Reduces the array by one object, but DOES not
	// perform any object deletion
	//
	void array_c::ShrinkAtTail(void)
	{
		if (array_entries>0)
			DecrementCount();
	}

	//
	// array_c::Trim
	//
	bool array_c::Trim(void)
	{
		return Size(array_entries);
	}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
