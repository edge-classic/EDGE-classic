//------------------------------------------------------------------------
//  BUFFER for Parsing
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2023  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------

#ifndef __DEH_BUFFER_HDR__
#define __DEH_BUFFER_HDR__

namespace Deh_Edge
{

class input_buffer_c
{
public:
	input_buffer_c(const char *_data, int _length);
	~input_buffer_c();

	bool eof();
	bool error();
	int  read(void *buf, int count);
	int  getch();
	void ungetch(int c);
	bool isBinary() const;

private:
	const char *data;  // base pointer
	const char *ptr;   // current read pointer

	int length;
};

}  // Deh_Edge

#endif /* __DEH_BUFFER_HDR__ */
