//----------------------------------------------------------------------------
//  EPI String Comparison
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022  The EDGE Team.
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
#include "str_compare.h"

#include <cstring>
#include <cctype>

#undef strcmp
#undef strncmp

namespace epi
{

int str_compare(const char *A, const char *B)
{
	SYS_ASSERT(A && B);
	return std::strcmp(A, B);
}

int str_compare(const char *A, const std::string& B)
{
	return str_compare(A, B.c_str());
}

int str_compare(const std::string& A, const char *B)
{
	return str_compare(A.c_str(), B);
}

int str_compare(const std::string& A, const std::string& B)
{
	return str_compare(A.c_str(), B.c_str());
}

//----------------------------------------------------------------------------

int str_compare_max(const char *A, const char *B, size_t n)
{
	SYS_ASSERT(A && B);
	return std::strncmp(A, B, n);
}

int str_compare_max(const char *A, const std::string& B, size_t n)
{
	return str_compare_max(A, B.c_str(), n);
}

int str_compare_max(const std::string& A, const char *B, size_t n)
{
	return str_compare_max(A.c_str(), B, n);
}

int str_compare_max(const std::string& A, const std::string& B, size_t n)
{
	return str_compare_max(A.c_str(), B.c_str(), n);
}

//----------------------------------------------------------------------------

int str_compare_nocase(const char *A, const char *B)
{
	SYS_ASSERT(A && B);

	for (;;)
	{
		int AC = std::tolower(*A++);
		int BC = std::tolower(*B++);

		if (AC != BC)
			return AC - BC;

		if (AC == 0)
			return 0;
	}
}

int str_compare_nocase(const char *A, const std::string& B)
{
	return str_compare_nocase(A, B.c_str());
}

int str_compare_nocase(const std::string& A, const char *B)
{
	return str_compare_nocase(A.c_str(), B);
}

int str_compare_nocase(const std::string& A, const std::string& B)
{
	return str_compare_nocase(A.c_str(), B.c_str());
}

//----------------------------------------------------------------------------

int prefix_compare(const char *A, const char *B)
{
	SYS_ASSERT(A && B);

	for (;;)
	{
		int AC = (int)(unsigned char) *A++;
		int BC = (int)(unsigned char) *B++;

		if (BC == 0)
			return 0;

		if (AC != BC)
			return AC - BC;
	}
}

int prefix_compare(const char *A, const std::string& B)
{
	return prefix_compare(A, B.c_str());
}

int prefix_compare(const std::string& A, const char *B)
{
	return prefix_compare(A.c_str(), B);
}

int prefix_compare(const std::string& A, const std::string& B)
{
	return prefix_compare(A.c_str(), B.c_str());
}

//----------------------------------------------------------------------------

int prefix_compare_nocase(const char *A, const char *B)
{
	SYS_ASSERT(A && B);

	for (;;)
	{
		int AC = std::tolower(*A++);
		int BC = std::tolower(*B++);

		if (BC == 0)
			return 0;

		if (AC != BC)
			return AC - BC;
	}
}

int prefix_compare_nocase(const char *A, const std::string& B)
{
	return prefix_compare_nocase(A, B.c_str());
}

int prefix_compare_nocase(const std::string& A, const char *B)
{
	return prefix_compare_nocase(A.c_str(), B);
}

int prefix_compare_nocase(const std::string& A, const std::string& B)
{
	return prefix_compare_nocase(A.c_str(), B.c_str());
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
