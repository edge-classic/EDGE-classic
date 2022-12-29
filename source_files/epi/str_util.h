//----------------------------------------------------------------------------
//  EPI String Utilities
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2008  The EDGE Team.
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

#ifndef __EPI_STR_UTIL_H__
#define __EPI_STR_UTIL_H__

#include <string>
#include <vector>
#include <locale>

namespace epi
{

void str_lower(std::string& s);
void str_upper(std::string& s);

std::string STR_Format(const char *fmt, ...) GCCATTR((format(printf, 1, 2)));

std::vector<std::string> STR_SepStringVector(std::string str, char separator);

// The following string conversion classes/code are adapted from public domain
// code by Andrew Choi originally found at https://web.archive.org/web/20151209032329/http://members.shaw.ca/akochoi/articles/unicode-processing-c++0x/

struct UTF8 {
  typedef char storage_type;
  typedef std::string string_type;
};

struct UTF16 {
  typedef char16_t storage_type;
  typedef std::u16string string_type;
};

struct UTF32 {
  typedef char32_t storage_type;
  typedef std::u32string string_type;
};

template<class T, class F> int storageMultiplier();

template<class T, class F, class I>
class str_converter {
 private:
  typedef typename F::storage_type from_storage_type;
  typedef typename T::storage_type to_storage_type;
  typedef typename I::storage_type intern_storage_type;

  typedef std::codecvt<intern_storage_type, UTF8::storage_type, std::mbstate_t> codecvt_type;

 public:

  typedef typename F::string_type from_string_type;
  typedef typename T::string_type to_string_type;

  to_string_type out(const from_string_type& s)
  {
    static std::locale loc(std::locale::classic(), new codecvt_type);
    static std::mbstate_t state;
    static const codecvt_type& cvt = std::use_facet<codecvt_type>(loc);

    const from_storage_type * enx;

    int len = s.length() * storageMultiplier<T, F>();
    to_storage_type *i = new to_storage_type[len];
    to_storage_type *inx;

    typename codecvt_type::result r =
      cvt.out(state, s.c_str(), s.c_str() + s.length(), enx, i, i + len, inx);

    if (r != codecvt_type::ok)
      I_Error("EPI: String Conversion Failed!\n");

    return to_string_type(i, inx - i);
  }

  to_string_type in(const from_string_type& s)
  {
    static std::locale loc(std::locale::classic(), new codecvt_type);
    static std::mbstate_t state;
    static const codecvt_type& cvt = std::use_facet<codecvt_type>(loc);

    const from_storage_type* enx;

    int len = s.length() * storageMultiplier<T, F>();
    to_storage_type *i = new to_storage_type[len];
    to_storage_type *inx;

    typename codecvt_type::result r =
      cvt.in(state, s.c_str(), s.c_str() + s.length(), enx, i, i + len, inx);

    if (r != codecvt_type::ok)
      I_Error("EPI: String Conversion Failed!\n");

    return to_string_type(i, inx - i);
  }
};

extern std::string to_u8string(const std::u16string& s);
extern std::string to_u8string(const std::u32string& s);
extern std::u16string to_u16string(const std::string& s);
extern std::u32string to_u32string(const std::string& s);

} // namespace epi

#endif /* __EPI_STR_UTIL_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
