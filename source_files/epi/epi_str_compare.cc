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

#include "epi_str_compare.h"

#include <ctype.h>
#include <string.h>

#include "epi.h"
#include "epi_str_util.h"

namespace epi
{

int StringCompare(std::string_view A, std::string_view B)
{
    size_t        A_pos = 0;
    size_t        B_pos = 0;
    size_t        A_end = A.size();
    size_t        B_end = B.size();
    unsigned char AC    = 0;
    unsigned char BC    = 0;

    for (;; A_pos++, B_pos++)
    {
        if (A_pos >= A_end)
            AC = 0;
        else
            AC = (int)(unsigned char)A[A_pos];
        if (B_pos >= B_end)
            BC = 0;
        else
            BC = (int)(unsigned char)B[B_pos];

        if (AC != BC)
            return AC - BC;

        if (A_pos == A_end)
            return 0;
    }
}

//----------------------------------------------------------------------------

int StringCompareMax(std::string_view A, std::string_view B, size_t n)
{
    EPI_ASSERT(n != 0);
    size_t        A_pos = 0;
    size_t        B_pos = 0;
    size_t        A_end = A.size();
    size_t        B_end = B.size();
    unsigned char AC    = 0;
    unsigned char BC    = 0;

    for (;; A_pos++, B_pos++)
    {
        if (n == 0)
            return 0;

        if (A_pos >= A_end)
            AC = 0;
        else
            AC = (int)(unsigned char)A[A_pos];
        if (B_pos >= B_end)
            BC = 0;
        else
            BC = (int)(unsigned char)B[B_pos];

        if (AC != BC)
            return AC - BC;

        if (A_pos == A_end)
            return 0;

        n--;
    }
}

//----------------------------------------------------------------------------

int StringCaseCompareASCII(std::string_view A, std::string_view B)
{
    size_t        A_pos = 0;
    size_t        B_pos = 0;
    size_t        A_end = A.size();
    size_t        B_end = B.size();
    unsigned char AC    = 0;
    unsigned char BC    = 0;

    for (;; A_pos++, B_pos++)
    {
        if (A_pos >= A_end)
            AC = 0;
        else
        {
            AC = (int)(unsigned char)A[A_pos];
            if (AC > '@' && AC < '[')
                AC ^= 0x20;
        }
        if (B_pos >= B_end)
            BC = 0;
        else
        {
            BC = (int)(unsigned char)B[B_pos];
            if (BC > '@' && BC < '[')
                BC ^= 0x20;
        }

        if (AC != BC)
            return AC - BC;

        if (A_pos == A_end)
            return 0;
    }
}

//----------------------------------------------------------------------------

int StringCaseCompareMaxASCII(std::string_view A, std::string_view B, size_t n)
{
    EPI_ASSERT(n != 0);
    size_t        A_pos = 0;
    size_t        B_pos = 0;
    size_t        A_end = A.size();
    size_t        B_end = B.size();
    unsigned char AC    = 0;
    unsigned char BC    = 0;

    for (;; A_pos++, B_pos++)
    {
        if (n == 0)
            return 0;

        if (A_pos >= A_end)
            AC = 0;
        else
        {
            AC = (int)(unsigned char)A[A_pos];
            if (AC > '@' && AC < '[')
                AC ^= 0x20;
        }
        if (B_pos >= B_end)
            BC = 0;
        else
        {
            BC = (int)(unsigned char)B[B_pos];
            if (BC > '@' && BC < '[')
                BC ^= 0x20;
        }

        if (AC != BC)
            return AC - BC;

        if (A_pos == A_end)
            return 0;

        n--;
    }
}

//----------------------------------------------------------------------------

int StringPrefixCompare(std::string_view A, std::string_view B)
{
    size_t        A_pos = 0;
    size_t        B_pos = 0;
    size_t        A_end = A.size();
    size_t        B_end = B.size();
    unsigned char AC    = 0;
    unsigned char BC    = 0;

    for (;; A_pos++, B_pos++)
    {
        if (A_pos >= A_end)
            AC = 0;
        else
            AC = (int)(unsigned char)A[A_pos];
        if (B_pos >= B_end)
            BC = 0;
        else
            BC = (int)(unsigned char)B[B_pos];

        if (B_pos == B_end)
            return 0;

        if (AC != BC)
            return AC - BC;
    }
}

//----------------------------------------------------------------------------

int StringPrefixCaseCompareASCII(std::string_view A, std::string_view B)
{
    size_t        A_pos = 0;
    size_t        B_pos = 0;
    size_t        A_end = A.size();
    size_t        B_end = B.size();
    unsigned char AC    = 0;
    unsigned char BC    = 0;

    for (;; A_pos++, B_pos++)
    {
        if (A_pos >= A_end)
            AC = 0;
        else
        {
            AC = (int)(unsigned char)A[A_pos];
            if (AC > '@' && AC < '[')
                AC ^= 0x20;
        }
        if (B_pos >= B_end)
            BC = 0;
        else
        {
            BC = (int)(unsigned char)B[B_pos];
            if (BC > '@' && BC < '[')
                BC ^= 0x20;
        }

        if (B_pos == B_end)
            return 0;

        if (AC != BC)
            return AC - BC;
    }
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
