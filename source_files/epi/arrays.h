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
#ifndef __EPI_ARRAY_CLASS__
#define __EPI_ARRAY_CLASS__

#ifndef NULL
#define NULL ((void*)0)
#endif

namespace epi
{
	// Forward declarations
	class array_c;
	class array_iterator_c;

	// Typedefs
	typedef int array_block_t;

	// Classes

	// The array
	class array_c
	{
		friend class array_iterator_c;

	public:
		array_c(const int objsize);
		virtual ~array_c();

	protected:
		unsigned int array_block_objsize;
		unsigned int array_objsize;

		array_block_t *array;
		array_block_t *array_end;
		int array_entries;
		int array_max_entries;

		void* ExpandAtTail(void);
		
		void* FetchObjectDirect(int pos) const
			{ return (void*)&array[pos*array_block_objsize]; }

		void ShrinkAtTail(void);
		
		// Count management
		void DecrementCount()
		{
			array_entries--;
			array_end -= array_block_objsize;
		}
		 
		void IncrementCount()
		{
			array_entries++;
			array_end += array_block_objsize;
		}
		
		void SetCount(int count)
		{
			array_entries = count; 
			array_end = array + (array_block_objsize*array_entries); 
		}
		
	public:
		// Object Management
		virtual void CleanupObject(void *obj) = 0;
		void* FetchObject(int pos) const;
		int InsertObject(void *obj, int pos = -1);
		void RemoveObject(int pos);
		
		// Iterator handling
		array_iterator_c GetBaseIterator();
		array_iterator_c GetIterator(int pos);
		array_iterator_c GetTailIterator();

		// List Management
		void Clear(void);
		bool Size(int entries);
		bool Trim(void);
		void ZeroiseCount(void) { array_end = array; array_entries = 0; }
	};


	// The array iterator
	class array_iterator_c
	{
	public:
		array_iterator_c()
		{
			parent = (epi::array_c*)NULL;
			idx = 0;
		}	

		array_iterator_c(array_c *ptr, int initpos)
		{
			parent = ptr;
			idx = initpos*parent->array_block_objsize;
		}

		~array_iterator_c()
		{
		}

	private:
		array_c* parent;
		int idx;

	public:
		int GetPos() { return idx ? idx/parent->array_block_objsize : 0; }

		bool IsValid()
		{
			return ((parent->array + idx) >= parent->array_end)?false:true;
		}

		void SetPos(int pos) { idx = pos * parent->array_block_objsize; }

		operator void* () const
		{
			return (void*)((array_block_t*)(parent->array + idx));
		}
		
		void operator++(int)
		{
			idx += parent->array_block_objsize;
		}

		void operator--(int)
		{
			if (idx)
				idx -= parent->array_block_objsize;
			else
				idx = parent->array_entries*parent->array_block_objsize; // Max out to make invalid
		}
	};
};

// Helper defines
#define BEGIN_IMPLEMENT_ARRAY(_type, _name) \
	class _name : public epi::array_c \
	{\
	public:\
		_name() : epi::array_c(sizeof(_type)) {}\
		~_name() { Clear(); } \
		\
		int GetSize() const { return array_entries; } \
		_type* operator[](int idx) { return (_type*)FetchObject(idx); } 

#define END_IMPLEMENT_ARRAY \
	}

#define IMPLEMENT_PRIMITIVE_ARRAY(_type, _name) \
	BEGIN_IMPLEMENT_ARRAY(_type, _name) \
		private: void CleanupObject(void *obj) { /* Do Nothing */ } \
		public: int Insert(_type p) { return InsertObject((void*)&p); } \
	END_IMPLEMENT_ARRAY

#define ITERATOR_TO_PTR(_it, _type) \
	((_type*)((void*)(_it)))

#define ITERATOR_TO_TYPE(_it, _type) \
	(*(_type*)((void*)(_it)))

#endif /* __EPI_ARRAY_CLASS__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
