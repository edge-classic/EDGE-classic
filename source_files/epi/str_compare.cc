//----------------------------------------------------------------------------
//  EPI String Comparison
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#include "epi.h"
#include "str_compare.h"
#include "str_util.h"

#include <string.h>
#include <ctype.h>
#include <locale>

namespace epi
{

int STR_Cmp(const char *A, const char *B)
{
    SYS_ASSERT(A && B);
    for (;;)
	{
		int AC = (int)(unsigned char)*A++;
        int BC = (int)(unsigned char)*B++;

		if (AC != BC)
			return AC - BC;

		if (AC == 0)
			return 0;
	}
}

int STR_Cmp(const char *A, const std::string &B)
{
    return epi::STR_Cmp(A, B.c_str());
}

int STR_Cmp(const std::string &A, const char *B)
{
    return epi::STR_Cmp(A.c_str(), B);
}

int STR_Cmp(const std::string &A, const std::string &B)
{
    return epi::STR_Cmp(A.c_str(), B.c_str());
}

//----------------------------------------------------------------------------

int STR_CmpMax(const char *A, const char *B, size_t n)
{
    SYS_ASSERT(A && B);
    SYS_ASSERT(n != 0);
	for (;;)
	{
		if (n == 0)
			return 0;

		int AC = (int)(unsigned char)*A++;
        int BC = (int)(unsigned char)*B++;

		if (AC != BC)
			return AC - BC;

		if (AC == 0)
			return 0;

		n--;
	}
}

int STR_CmpMax(const char *A, const std::string &B, size_t n)
{
    return epi::STR_CmpMax(A, B.c_str(), n);
}

int STR_CmpMax(const std::string &A, const char *B, size_t n)
{
    return epi::STR_CmpMax(A.c_str(), B, n);
}

int STR_CmpMax(const std::string &A, const std::string &B, size_t n)
{
    return epi::STR_CmpMax(A.c_str(), B.c_str(), n);
}

//----------------------------------------------------------------------------

int STR_CaseCmp(const char *A, const char *B)
{
    SYS_ASSERT(A && B);
    for (;;)
    {
        int AC = tolower((unsigned char)*A++);
        int BC = tolower((unsigned char)*B++);

        if (AC != BC)
            return AC - BC;

        if (AC == 0)
            return 0;
    }
}

int STR_CaseCmp(const char *A, const std::string &B)
{
    return epi::STR_CaseCmp(A, B.c_str());
}

int STR_CaseCmp(const std::string &A, const char *B)
{
    return epi::STR_CaseCmp(A.c_str(), B);
}

int STR_CaseCmp(const std::string &A, const std::string &B)
{
    return epi::STR_CaseCmp(A.c_str(), B.c_str());
}

//----------------------------------------------------------------------------

int STR_CaseCmpMax(const char *A, const char *B, size_t n)
{
    SYS_ASSERT(A && B);
    SYS_ASSERT(n != 0);
	for (;;)
	{
		if (n == 0)
			return 0;

		int AC = tolower((unsigned char)*A++);
        int BC = tolower((unsigned char)*B++);

		if (AC != BC)
			return AC - BC;

		if (AC == 0)
			return 0;

		n--;
	}
}

int STR_CaseCmpMax(const char *A, const std::string &B, size_t n)
{
    return epi::STR_CaseCmpMax(A, B.c_str(), n);
}

int STR_CaseCmpMax(const std::string &A, const char *B, size_t n)
{
    return epi::STR_CaseCmpMax(A.c_str(), B, n);
}

int STR_CaseCmpMax(const std::string &A, const std::string &B, size_t n)
{
    return epi::STR_CaseCmpMax(A.c_str(), B.c_str(), n);
}

//----------------------------------------------------------------------------

int STR_PrefixCmp(const char *A, const char *B)
{
    SYS_ASSERT(A && B);
    for (;;)
    {
        int AC = (int)(unsigned char)*A++;
        int BC = (int)(unsigned char)*B++;

        if (BC == 0)
            return 0;

        if (AC != BC)
            return AC - BC;
    }
}

int STR_PrefixCmp(const char *A, const std::string &B)
{
    return epi::STR_PrefixCmp(A, B.c_str());
}

int STR_PrefixCmp(const std::string &A, const char *B)
{
    return epi::STR_PrefixCmp(A.c_str(), B);
}

int STR_PrefixCmp(const std::string &A, const std::string &B)
{
    return epi::STR_PrefixCmp(A.c_str(), B.c_str());
}

//----------------------------------------------------------------------------

int STR_PrefixCaseCmp(const char *A, const char *B)
{
    SYS_ASSERT(A && B);

    for (;;)
    {
        int AC = tolower((unsigned char)*A++);
        int BC = tolower((unsigned char)*B++);

        if (BC == 0)
            return 0;

        if (AC != BC)
            return AC - BC;
    }
}

int STR_PrefixCaseCmp(const char *A, const std::string &B)
{
    return epi::STR_PrefixCaseCmp(A, B.c_str());
}

int STR_PrefixCaseCmp(const std::string &A, const char *B)
{
    return epi::STR_PrefixCaseCmp(A.c_str(), B);
}

int STR_PrefixCaseCmp(const std::string &A, const std::string &B)
{
    return epi::STR_PrefixCaseCmp(A.c_str(), B.c_str());
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
