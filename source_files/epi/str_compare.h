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

#ifndef __EPI_STR_COMPARE_H__
#define __EPI_STR_COMPARE_H__

#undef strcmp
#undef strncmp
#undef stricmp
#undef strnicmp
#undef strcasecmp
#undef strncasecmp

namespace epi
{

int strcmp(const char *A, const char *B);
int strcmp(const char *A, const std::string &B);
int strcmp(const std::string &A, const char *B);
int strcmp(const std::string &A, const std::string &B);

int strncmp(const char *A, const char *B, size_t n);
int strncmp(const char *A, const std::string &B, size_t n);
int strncmp(const std::string &A, const char *B, size_t n);
int strncmp(const std::string &A, const std::string &B, size_t n);

int case_cmp(const char *A, const char *B);
int case_cmp(const char *A, const std::string &B);
int case_cmp(const std::string &A, const char *B);
int case_cmp(const std::string &A, const std::string &B);

int case_cmp_n(const char *A, const char *B, size_t n);
int case_cmp_n(const char *A, const std::string &B, size_t n);
int case_cmp_n(const std::string &A, const char *B, size_t n);
int case_cmp_n(const std::string &A, const std::string &B, size_t n);

int prefix_cmp(const char *A, const char *B);
int prefix_cmp(const char *A, const std::string &B);
int prefix_cmp(const std::string &A, const char *B);
int prefix_cmp(const std::string &A, const std::string &B);

int prefix_case_cmp(const char *A, const char *B);
int prefix_case_cmp(const char *A, const std::string &B);
int prefix_case_cmp(const std::string &A, const char *B);
int prefix_case_cmp(const std::string &A, const std::string &B);

} // namespace epi

#endif /* __EPI_STR_COMPARE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
