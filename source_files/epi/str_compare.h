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

#ifndef __EPI_STR_COMPARE_H__
#define __EPI_STR_COMPARE_H__

#undef strcmp
#undef strncmp
#undef stricmp
#undef strnicmp
#undef strcasecmp
#undef strncasecmp

#define strcmp        epi::str_compare
#define strncmp       epi::str_compare_max

#define strcasecmp    epi::str_compare_nocase
#define stricmp       epi::str_compare_nocase

#define prefix_cmp    epi::prefix_compare
#define prefix_icmp   epi::prefix_compare_nocase

namespace epi
{

int str_compare(const char *A, const char *B);
int str_compare(const char *A, const std::string& B);
int str_compare(const std::string& A, const char *B);
int str_compare(const std::string& A, const std::string& B);

int str_compare_max(const char *A, const char *B, size_t n);
int str_compare_max(const char *A, const std::string& B, size_t n);
int str_compare_max(const std::string& A, const char *B, size_t n);
int str_compare_max(const std::string& A, const std::string& B, size_t n);

int str_compare_nocase(const char *A, const char *B);
int str_compare_nocase(const char *A, const std::string& B);
int str_compare_nocase(const std::string& A, const char *B);
int str_compare_nocase(const std::string& A, const std::string& B);

int prefix_compare(const char *A, const char *B);
int prefix_compare(const char *A, const std::string& B);
int prefix_compare(const std::string& A, const char *B);
int prefix_compare(const std::string& A, const std::string& B);

int prefix_compare_nocase(const char *A, const char *B);
int prefix_compare_nocase(const char *A, const std::string& B);
int prefix_compare_nocase(const std::string& A, const char *B);
int prefix_compare_nocase(const std::string& A, const std::string& B);

} // namespace epi

#endif /* __EPI_STR_COMPARE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
