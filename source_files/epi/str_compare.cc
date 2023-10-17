//----------------------------------------------------------------------------
//  EPI String Comparison
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2023  The EDGE Team.
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

#include <cstring>
#include <cctype>
#include <locale>

#undef strcmp
#undef strncmp

namespace epi
{

int strcmp(const char *A, const char *B)
{
	SYS_ASSERT(A && B);
	return std::strcmp(A, B);
}

int strcmp(const char *A, const std::string& B)
{
	return epi::strcmp(A, B.c_str());
}

int strcmp(const std::string& A, const char *B)
{
	return epi::strcmp(A.c_str(), B);
}

int strcmp(const std::string& A, const std::string& B)
{
	return epi::strcmp(A.c_str(), B.c_str());
}

//----------------------------------------------------------------------------

int strncmp(const char *A, const char *B, size_t n)
{
	SYS_ASSERT(A && B);
	return std::strncmp(A, B, n);
}

int strncmp(const char *A, const std::string& B, size_t n)
{
	return epi::strncmp(A, B.c_str(), n);
}

int strncmp(const std::string& A, const char *B, size_t n)
{
	return epi::strncmp(A.c_str(), B, n);
}

int strncmp(const std::string& A, const std::string& B, size_t n)
{
	return epi::strncmp(A.c_str(), B.c_str(), n);
}

//----------------------------------------------------------------------------

int case_cmp(const char *A, const char *B)
{
	SYS_ASSERT(A && B);

	for (;;)
	{
		int AC = std::tolower((unsigned char) *A++);
		int BC = std::tolower((unsigned char) *B++);

		if (AC != BC)
			return AC - BC;

		if (AC == 0)
			return 0;
	}
}

int case_cmp(const char *A, const std::string& B)
{
	return epi::case_cmp(A, B.c_str());
}

int case_cmp(const std::string& A, const char *B)
{
	return epi::case_cmp(A.c_str(), B);
}

int case_cmp(const std::string& A, const std::string& B)
{
	return epi::case_cmp(A.c_str(), B.c_str());
}

//----------------------------------------------------------------------------

int case_cmp_n(const char *A, const char *B, size_t n)
{
	SYS_ASSERT(A && B && strlen(A) > n && strlen(B) > n);
	return epi::case_cmp(std::string(A).substr(0, n), std::string(B).substr(0, n));
}

int case_cmp_n(const char *A, const std::string& B, size_t n)
{
	return epi::case_cmp_n(A, B.c_str(), n);
}

int case_cmp_n(const std::string& A, const char *B, size_t n)
{
	return epi::case_cmp_n(A.c_str(), B, n);
}

int case_cmp_n(const std::string& A, const std::string& B, size_t n)
{
	return epi::case_cmp_n(A.c_str(), B.c_str(), n);
}

//----------------------------------------------------------------------------

int prefix_cmp(const char *A, const char *B)
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

int prefix_cmp(const char *A, const std::string& B)
{
	return epi::prefix_cmp(A, B.c_str());
}

int prefix_cmp(const std::string& A, const char *B)
{
	return epi::prefix_cmp(A.c_str(), B);
}

int prefix_cmp(const std::string& A, const std::string& B)
{
	return epi::prefix_cmp(A.c_str(), B.c_str());
}

//----------------------------------------------------------------------------

int prefix_case_cmp(const char *A, const char *B)
{
	SYS_ASSERT(A && B);

	for (;;)
	{
		int AC = std::tolower((unsigned char) *A++);
		int BC = std::tolower((unsigned char) *B++);

		if (BC == 0)
			return 0;

		if (AC != BC)
			return AC - BC;
	}
}

int prefix_case_cmp(const char *A, const std::string& B)
{
	return epi::prefix_case_cmp(A, B.c_str());
}

int prefix_case_cmp(const std::string& A, const char *B)
{
	return epi::prefix_case_cmp(A.c_str(), B);
}

int prefix_case_cmp(const std::string& A, const std::string& B)
{
	return epi::prefix_case_cmp(A.c_str(), B.c_str());
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
