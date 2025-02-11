// Copyright (c) 2008-2022 the Urho3D project.
// Copyright (c) 2024-2024 the rbfx project.
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT> or the accompanying LICENSE file.

#pragma once

#define EPI_DEBUG_STRING_HASH

#include <string>
#ifdef EPI_DEBUG_STRING_HASH
#include <mutex>
#include <unordered_map>
#endif

#include "epi_str_util.h"

namespace epi
{

/// Calculate hash in compile time.
/// This function should be identical to eastl::hash<string_view> specialization from EASTL/string_view.h
static constexpr uint32_t CalculateStringHash(const std::string_view &x)
{
    std::string_view::const_iterator p      = x.cbegin();
    std::string_view::const_iterator end    = x.cend();
    uint32_t                    result = 2166136261U; // We implement an FNV-like string hash.
    while (p != end)
        result = (result * 16777619) ^ (uint8_t)ToUpperASCII(*p++);
    return result;
}

static constexpr uint32_t CalculateStringHash(const char *x)
{
    uint32_t    result = 2166136261U; // We implement an FNV-like string hash.
    const char *p      = x;
    while (*p)
        result = (result * 16777619) ^ (uint8_t)ToUpperASCII(*p++);
    return result;
}

/// 32-bit hash value for a string.
class StringHash
{
  public:
    /// Construct with zero value.
    constexpr StringHash() : value_(kEmptyValue)
    {
    }

    /// Construct with an initial value.
    constexpr explicit StringHash(uint32_t value) : value_(value)
    {
    }

    /// Construct from a C string.
    constexpr StringHash(const char *str) : value_(CalculateStringHash(str))
    {
    }

    /// Construct from a string.
    StringHash(const std::string &str) : StringHash(std::string_view{str})
    {
#ifdef EPI_DEBUG_STRING_HASH
        Register(*this, str);
#endif
    }

    /// Construct from a string.
    StringHash(const std::string_view &str);

#ifdef EPI_DEBUG_STRING_HASH
    static std::string GetRegistered(StringHash hash);
    static void Register(StringHash hash, std::string_view str);
    static const std::unordered_map<StringHash, std::string> &GetHashRegistry();
#endif

    /// Test for equality with another hash.
    constexpr bool operator==(const StringHash &rhs) const
    {
        return value_ == rhs.value_;
    }

    /// Test for inequality with another hash.
    constexpr bool operator!=(const StringHash &rhs) const
    {
        return value_ != rhs.value_;
    }

    /// Test if less than another hash.
    constexpr bool operator<(const StringHash &rhs) const
    {
        return value_ < rhs.value_;
    }

    /// Test if greater than another hash.
    constexpr bool operator>(const StringHash &rhs) const
    {
        return value_ > rhs.value_;
    }

    /// Return true if nonempty hash value.
    constexpr bool IsEmpty() const
    {
        return value_ == kEmptyValue;
    }

    /// Return true if nonempty hash value.
    constexpr explicit operator bool() const
    {
        return !IsEmpty();
    }

    /// Return hash value.
    /// @property
    constexpr uint32_t Value() const
    {
        return value_;
    }

    /// Return mutable hash value. For internal use only.
    constexpr uint32_t &MutableValue()
    {
        return value_;
    }

    /// Return as string.
    std::string ToString() const;

    /// Return debug string that contains hash value, and reversed hash string if possible.
    std::string ToDebugString() const;

    /// Return string which has specific hash value. Return first string if many (in order of calculation).
    /// Use for debug purposes only. Return empty string if EPI_STRING_HASH_DEBUG is off.
    std::string Reverse() const;

    /// Return hash value for HashSet & HashMap.
    constexpr uint32_t ToHash() const
    {
        return value_;
    }

    /// Calculate hash value for string_view.
    static constexpr uint32_t Calculate(const std::string_view &view)
    {
        return CalculateStringHash(view);
    }

    /// Calculate hash value from a C string.
    static constexpr uint32_t Calculate(const char *str)
    { //
        return Calculate(std::string_view(str));
    }

    /// Calculate hash value from binary data.
    static constexpr uint32_t Calculate(const char *data, uint32_t length)
    {
        return Calculate(std::string_view(data, length));
    }

    /// Hash of empty string. Is *not* zero!
    static constexpr uint32_t kEmptyValue = CalculateStringHash(std::string_view{""});
    static const StringHash   kEmpty;

  protected:
    /// Hash value.
    uint32_t value_;
#ifdef EPI_DEBUG_STRING_HASH
    static std::unordered_map<StringHash, std::string> global_hash_registry_;
    static std::mutex global_hash_mutex_;
#endif
};

static_assert(sizeof(StringHash) == sizeof(uint32_t), "Unexpected StringHash size.");

#define EPI_KNOWN_STRINGHASH(v, s)                                                                                     \
    static constexpr char                                    v##_known_string_[] = s;                                 \
    static constexpr epi::KnownStringHash<v##_known_string_> v{};

template <const char *known_string> class KnownStringHash : public StringHash
{
  public:
    constexpr KnownStringHash() : StringHash(known_string)
    {
    }

    // implicit conversion so can be used in switch case statements, etc
    constexpr operator uint32_t() const
    {
        return value_;
    }
};

} // namespace epi
template <>
struct std::hash<epi::StringHash>
{
    size_t operator()(const epi::StringHash& k) const
    {
        return k.ToHash();
    }
};