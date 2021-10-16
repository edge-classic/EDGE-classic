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

#include "epi.h"
#include "timestamp.h"

namespace epi
{

timestamp_c::timestamp_c()
{
	Clear();
}

timestamp_c::~timestamp_c()
{
}

void timestamp_c::Clear()
{
	hour = 0;
	min = 0;
	sec = 0;
	day = 1;
	month = 1;
	year = 1900;
}

void timestamp_c::DecodeDate(unsigned int date)
{
    unsigned int dufus;

	dufus = (date & 0x3F);        day   = (unsigned char)dufus;
	dufus = ((date>>6)& 0x3F);    month = (unsigned char)dufus;
	dufus = ((date>>22)& 0xFFFF); year  = (unsigned short)dufus;

	return;
}

void timestamp_c::DecodeTime(unsigned int time)
{
	unsigned int dufus;

    dufus = (time & 0x1F);      hour = (unsigned char)dufus;
    dufus = ((time<<5)& 0x3F);  min  = (unsigned char)dufus;
    dufus = ((time<<11)& 0x3F); sec  = (unsigned char)dufus;

	return;
}

bool timestamp_c::IsValid(void)
{
	unsigned char nMax;

	if (year == 0 || month>12 || hour>23 || min>59 || sec>59)
	{
		return false;
	}

	if (day>28)
	{
		if (month == 3 || month == 6 || month == 9 || month == 11)
		{
			nMax = 30;
		}
		else if (month == 2)
		{
			if (!(year%4) && (year%100 || !(year%400)))
			{
				nMax = 29;
			}
			else
			{
				nMax = 28;
			}
		}
		else
		{
			nMax = 31;
		}

		if (day>nMax)
		{
			return false;
		}
	}

	return true;
}

void timestamp_c::Set(unsigned char _day, 
					  unsigned char _month, 
					  unsigned short _year, 
					  unsigned char _hour, 
					  unsigned char _min, 
					  unsigned char _sec)
{
	day   = _day;
	month = _month;
	year  = _year;

	hour  = _hour;
	min   = _min;
	sec   = _sec;
}

int timestamp_c::Cmp(const timestamp_c &Rhs) const
{
	if (year != Rhs.year)
		return year - Rhs.year;

	if (month != Rhs.month)
		return month - Rhs.month;

	if (day != Rhs.day)
		return day - Rhs.day;

	if (hour != Rhs.hour)
		return hour - Rhs.hour;

	if (min != Rhs.min)
		return min - Rhs.min;

	return sec - Rhs.sec;
}

void timestamp_c::operator=(const timestamp_c &Rhs)
{
	day = Rhs.day;
	month = Rhs.month;
	year = Rhs.year;
	hour = Rhs.hour;
	min = Rhs.min;
	sec = Rhs.sec;
}

};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
