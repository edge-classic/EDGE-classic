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

int strcmp(const std::u32string& A, const std::u32string& B)
{
	SYS_ASSERT(!A.empty() && !B.empty());

	if (A.size() != B.size())
		return A.size() - B.size();

	for (int i=0; i < A.size(); i++)
	{
		if (A != B)
			return A.at(i) - B.at(i);
	}
	return 0;
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

int case_cmp(const std::u32string& A, const std::u32string& B)
{
	SYS_ASSERT(!A.empty() && !B.empty());

	if (A.size() != B.size())
		return A.size() - B.size();

	for (int i=0; i < A.size(); i++)
	{
		if (std::tolower(A.at(i), std::locale()) != std::tolower(B.at(i), std::locale()))
			return A.at(i) - B.at(i);
	}
	return 0;
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
