//----------------------------------------------------------------------------
//  EDGE Timestamp Class
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
#ifndef __EPI_TIMESTAMP_CLASS__
#define __EPI_TIMESTAMP_CLASS__

namespace epi
{

class timestamp_c
{
public:
	timestamp_c();
	~timestamp_c();

private:
	unsigned char sec;    // 0-59
	unsigned char min;    // 0-59
	unsigned char hour;   // 0-23 (0 is 12am i.e. midnight)

	unsigned char day;    // 1-31
	unsigned char month;  // 1-12
	unsigned short year;  // 1900-2100

public:
	// Clear
	void Clear(void);

	// Validation
	bool IsValid(void);

	// Decoding/Encoding
	void DecodeDate(unsigned int date);
	void DecodeTime(unsigned int time);

	unsigned int EncodeDate(void) { return ((year & 0xFFFF) << 22) + ((month & 0x3F) << 6) + (day & 0x3F); }
	unsigned int EncodeTime(void) { return ((sec & 0x3F) << 11) + ((min & 0x3F) << 5) + (hour & 0x1F); }

	void Set(unsigned char _day,
			 unsigned char _month,
			 unsigned short _year,
			 unsigned char _hour,
			 unsigned char _min,
			 unsigned char _sec);

	int Cmp(const timestamp_c &Rhs) const;
	// result is same as strcmp() : negative when Lhs < Rhs, zero equal,
	// and positive value when Lhs > Rhs.

	// Operators
	inline bool operator> (const timestamp_c &Rhs) const { return Cmp(Rhs) > 0;  }
	inline bool operator< (const timestamp_c &Rhs) const { return Cmp(Rhs) < 0;  }
	inline bool operator<=(const timestamp_c &Rhs) const { return Cmp(Rhs) <= 0; }
	inline bool operator>=(const timestamp_c &Rhs) const { return Cmp(Rhs) >= 0; }
	inline bool operator==(const timestamp_c &Rhs) const { return Cmp(Rhs) == 0; }
	inline bool operator!=(const timestamp_c &Rhs) const { return Cmp(Rhs) != 0; }

	void operator=(const timestamp_c &Rhs);
};

};
#endif // __EPI_TIMESTAMP_CLASS__


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
