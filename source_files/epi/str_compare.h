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

namespace epi
{

int STR_Cmp(const char *A, const char *B);
int STR_Cmp(const char *A, const std::string &B);
int STR_Cmp(const std::string &A, const char *B);
int STR_Cmp(const std::string &A, const std::string &B);

int STR_CmpMax(const char *A, const char *B, size_t n);
int STR_CmpMax(const char *A, const std::string &B, size_t n);
int STR_CmpMax(const std::string &A, const char *B, size_t n);
int STR_CmpMax(const std::string &A, const std::string &B, size_t n);

int STR_CaseCmp(const char *A, const char *B);
int STR_CaseCmp(const char *A, const std::string &B);
int STR_CaseCmp(const std::string &A, const char *B);
int STR_CaseCmp(const std::string &A, const std::string &B);

int STR_CaseCmpMax(const char *A, const char *B, size_t n);
int STR_CaseCmpMax(const char *A, const std::string &B, size_t n);
int STR_CaseCmpMax(const std::string &A, const char *B, size_t n);
int STR_CaseCmpMax(const std::string &A, const std::string &B, size_t n);

int STR_PrefixCmp(const char *A, const char *B);
int STR_PrefixCmp(const char *A, const std::string &B);
int STR_PrefixCmp(const std::string &A, const char *B);
int STR_PrefixCmp(const std::string &A, const std::string &B);

int STR_PrefixCaseCmp(const char *A, const char *B);
int STR_PrefixCaseCmp(const char *A, const std::string &B);
int STR_PrefixCaseCmp(const std::string &A, const char *B);
int STR_PrefixCaseCmp(const std::string &A, const std::string &B);

} // namespace epi

#endif /* __EPI_STR_COMPARE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
