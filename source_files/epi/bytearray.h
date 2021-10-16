//----------------------------------------------------------------------------
//  EDGE ByteArray Class
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

#ifndef __EPI_BYTEARRAY_CLASS__
#define __EPI_BYTEARRAY_CLASS__

namespace epi
{
	// a simple growable array of bytes, useful for reading files into
	// memory or building files before writing them.  It wouldn't be
	// suitable for very large arrays though (over 1 megabyte).

	class bytearray_c
	{
	public:
		bytearray_c(int initial_size = 0);
		~bytearray_c();

	private:
		byte *array;
		int length;
		int space;

		inline void RequireSpace(int count)
		{
			if (count > space)
			{
				int new_total = length + space;

				if (new_total == 0)
					new_total = count;

				while ((new_total - length) < count)
					new_total *= 2;

				Resize(length, new_total - length);
			}
		}

		void Resize(int new_len, int new_space);

	public:
		void Clear();
		void Resize(int new_len);
		void Trim() { Resize(length, 0); }

		inline int Length() const { return length; }

		void Read(int pos, void *data, int count) const;
		void Write(int pos, const void *data, int count);

		byte& operator[](int pos) const;

		void Append(byte value);
		void Append(const void *data, int count);

		void operator+= (byte value) { Append(value); }

		byte *GetRawBase() const { return array; }

	private: /* this class is inherently non-copiable */
		bytearray_c(const bytearray_c& Rhs) { }
		bytearray_c& operator=(const bytearray_c& Rhs) { return *this; }
	};
};

#endif /* __EPI_BYTEARRAY_CLASS__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
