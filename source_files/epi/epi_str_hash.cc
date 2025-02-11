// Copyright (c) 2008-2022 the Urho3D project.
// Copyright (c) 2024-2024 the rbfx project.
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT> or the accompanying LICENSE file.

#include "epi_str_hash.h"

#include "epi.h"
#include "stb_sprintf.h"

namespace epi
{

static const int  CONVERSION_BUFFER_LENGTH = 128;
static const std::string EMPTY_STRING{};

const StringHash StringHash::kEmpty{""};

#ifdef EPI_DEBUG_STRING_HASH
std::unordered_map<StringHash, std::string> StringHash::global_hash_registry_;
std::mutex StringHash::global_hash_mutex_;
void StringHash::Register(StringHash hash, std::string_view str)
{
    global_hash_mutex_.lock();

    auto iter = global_hash_registry_.find(hash);
    if (iter == global_hash_registry_.end())
    {
        global_hash_registry_.emplace(hash, std::string(str));
    }
    else if (std::string_view(iter->second) != str)
    {
        FatalError("StringHash collision detected! Both \"%s\" and \"%s\" have hash #%s",
            std::string(str).c_str(), iter->second.c_str(), hash.ToString().c_str());
    }

    global_hash_mutex_.unlock();
}
std::string StringHash::GetRegistered(StringHash hash)
{
    auto iter = global_hash_registry_.find(hash);
    return iter == global_hash_registry_.end() ? EMPTY_STRING : iter->second;
}
const std::unordered_map<StringHash, std::string> &StringHash::GetHashRegistry()
{
    return global_hash_registry_;
}
#endif

StringHash::StringHash(const std::string_view &str) : value_(Calculate(str.data(), str.length()))
{
#ifdef EPI_DEBUG_STRING_HASH
    Register(*this, str);
#endif
}

std::string StringHash::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    stbsp_sprintf(tempBuffer, "%08X", value_);
    return std::string(tempBuffer);
}

std::string StringHash::ToDebugString() const
{
#ifdef EPI_DEBUG_STRING_HASH
    return epi::StringFormat("#%s '%s'", ToString().c_str(), Reverse().c_str());
#else
    return epi::StringFormat("#%s", ToString().c_str());
#endif
}

std::string StringHash::Reverse() const
{
#ifdef EPI_DEBUG_STRING_HASH
    global_hash_mutex_.lock();
    const std::string copy = GetRegistered(*this);
    global_hash_mutex_.unlock();
    return copy;
#else
    return EMPTY_STRING;
#endif
}

} // namespace epi
