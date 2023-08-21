//------------------------------------------------------------------------
//  TEXT Strings
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

#ifndef __DEH_TEXT_HDR__
#define __DEH_TEXT_HDR__

namespace Deh_Edge
{

namespace TextStr
{
	void Init();
	void Shutdown();

	// these return true if the string was found.
	bool ReplaceString(const char *before, const char *after);
	bool ReplaceCheat(const char *deh_name, const char *str);
	bool ReplaceBexString(const char *bex_name, const char *after);
	void ReplaceBinaryString(int v166_index, const char *str);

	void AlterCheat(const char * new_val);

	const char *GetLDFForBex(const char *bex_name);

	void ConvertLDF(void);
}

}  // Deh_Edge

#endif /* __DEH_TEXT_HDR__ */
