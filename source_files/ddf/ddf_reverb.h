//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Reverbs)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2025 The EDGE Team.
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

#pragma once

#include <unordered_map>

#include "ddf_types.h"
#include "epi.h"
#include "epi_ename.h"
#include "verblib.h"

extern epi::EName DDFCreateEName(std::string_view name, bool no_create);
#ifdef __GNUC__
[[noreturn]] extern void DDFError(const char *err, ...) __attribute__((format(printf, 1, 2)));
#else
[[noreturn]] extern void DDFError(const char *err, ...);
#endif

namespace ddf
{
class ReverbDefinition final
{
  public:
    ReverbDefinition();
    ~ReverbDefinition() {};

    void Default(void);
    void CopyDetail(const ReverbDefinition &src);
    void ApplyReverb(verblib *reverb) const;
    static void ReadDDF(const std::string &data);
    static inline ReverbDefinition *Lookup(std::string_view refname)
    {
        if (refname.empty())
            return nullptr;

        epi::EName verb_ename = DDFCreateEName(refname, true);

        if (verb_ename == epi::kENameNone)
            return nullptr;

        if (reverb_defs_.find(verb_ename) != reverb_defs_.end())
            return reverb_defs_[verb_ename];
        else
            return nullptr;
    }
    static inline ReverbDefinition *Lookup(epi::KnownEName refname)
    {
        if (refname == epi::kENameNone)
            return nullptr;

        if (reverb_defs_.find(refname) != reverb_defs_.end())
            return reverb_defs_[refname];
        else
            return nullptr;
    }
    static inline ReverbDefinition *Lookup(epi::EName refname)
    {
        if (refname == epi::kENameNone)
            return nullptr;

        if (reverb_defs_.find(refname) != reverb_defs_.end())
            return reverb_defs_[refname];
        else
            return nullptr;
    }
    static inline void StoreReverb(epi::EName name, ReverbDefinition *reverb_def)
    {
        reverb_defs_.try_emplace(name, reverb_def);
    }
    // Called when parsing DDF sectors using the REVERB_PRESET special
    static inline void AssignReverb(const char *info, void *storage)
    {
        *(const ReverbDefinition **)storage = Lookup(info);
        if (*(const ReverbDefinition **)storage == nullptr)
            DDFError("AssignReverb: No such reverb preset '%s'\n", info);
    }

    // Member vars....
    float room_size_;
    float damping_level_;
    float wet_level_;
    float dry_level_;
    float reverb_width_;
    float reverb_gain_;

    // Presets for Dynamic Reverb
    static const ReverbDefinition kOutdoorStrong;
    static const ReverbDefinition kIndoorStrong;
    static const ReverbDefinition kOutdoorWeak;
    static const ReverbDefinition kIndoorWeak;

  private:
    ReverbDefinition(float size, float damp, float wet, float dry, float width, float gain);
    // disable copy construct and assignment operator
    explicit ReverbDefinition(ReverbDefinition &rhs)
    {
        EPI_UNUSED(rhs);
    }
    ReverbDefinition &operator=(ReverbDefinition &rhs)
    {
        EPI_UNUSED(rhs);
        return *this;
    }

  protected:
    class Container final : public std::unordered_map<epi::EName, ReverbDefinition *, epi::ContainerENameHash>
    {
      public:
        Container()
        {
        }
        ~Container()
        {
            for (std::unordered_map<epi::EName, ReverbDefinition *, epi::ContainerENameHash>::iterator iter = begin(), 
                iter_end = end(); iter != iter_end; iter++)
            {
                ReverbDefinition *verb = iter->second;
                delete verb;
                verb = nullptr;
            }
        }
    };
    
    static Container reverb_defs_;
};

} // namespace ddf

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
