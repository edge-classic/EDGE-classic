// Copyright (c) 2008-2022 the Urho3D project.
// Copyright (c) 2024-2024 the rbfx project.
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT> or the accompanying LICENSE file.

#include "epi_str_hash.h"

#include "stb_sprintf.h"

namespace epi
{

const std::string EMPTY_STRING{};
static const int  CONVERSION_BUFFER_LENGTH = 128;

const StringHash StringHash::kEmpty{""};

StringHash::StringHash(const std::string_view &str) : value_(Calculate(str.data(), str.length()))
{
}

std::string StringHash::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    stbsp_sprintf(tempBuffer, "%08X", value_);
    return std::string(tempBuffer);
}

} // namespace epi
