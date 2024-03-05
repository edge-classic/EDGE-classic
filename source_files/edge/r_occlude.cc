//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Occlusion testing)
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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

#include "r_occlude.h"

#include "epi.h"
#include "math_bam.h"
#include "types.h"

// #define EDGE_DEBUG_OCCLUSION 1

struct AngleRange
{
    BAMAngle low, high;

    AngleRange *next;
    AngleRange *previous;
};

static AngleRange *occlusion_buffer_head = nullptr;
static AngleRange *occlusion_buffer_tail = nullptr;

static AngleRange *free_occlusion_range = nullptr;

#ifdef EDGE_DEBUG_OCCLUSION
static void ValidateBuffer(void)
{
    if (!occlusion_buffer_head)
    {
        SYS_ASSERT(!occlusion_buffer_tail);
        return;
    }

    for (AngleRange *AR = occlusion_buffer_head; AR; AR = AR->next)
    {
        SYS_ASSERT(AR->low <= AR->high);

        if (AR->next)
        {
            SYS_ASSERT(AR->next->previous == AR);
            SYS_ASSERT(AR->next->low > AR->high);
        }
        else { SYS_ASSERT(AR == occlusion_buffer_tail); }

        if (AR->prev) { SYS_ASSERT(AR->prev->next == AR); }
        else { SYS_ASSERT(AR == occlusion_buffer_head); }
    }
}
#endif  // DEBUG_OCCLUSION

void RendererOcclusionClear(void)
{
    // Clear all angles in the whole buffer
    // (i.e. mark them as open / non-blocking).

    if (occlusion_buffer_head)
    {
        occlusion_buffer_tail->next = free_occlusion_range;

        free_occlusion_range = occlusion_buffer_head;

        occlusion_buffer_head = nullptr;
        occlusion_buffer_tail = nullptr;
    }

#ifdef EDGE_DEBUG_OCCLUSION
    ValidateBuffer();
#endif
}

static inline AngleRange *GetNewRange(BAMAngle low, BAMAngle high)
{
    AngleRange *R;

    if (free_occlusion_range)
    {
        R = free_occlusion_range;

        free_occlusion_range = R->next;
    }
    else
        R = new AngleRange;

    R->low  = low;
    R->high = high;

    return R;
}

static inline void LinkBefore(AngleRange *X, AngleRange *N)
{
    // X = eXisting range
    // N = New range

    N->next     = X;
    N->previous = X->previous;

    X->previous = N;

    if (N->previous)
        N->previous->next = N;
    else
        occlusion_buffer_head = N;
}

static inline void LinkInTail(AngleRange *N)
{
    N->next     = nullptr;
    N->previous = occlusion_buffer_tail;

    if (occlusion_buffer_tail)
        occlusion_buffer_tail->next = N;
    else
        occlusion_buffer_head = N;

    occlusion_buffer_tail = N;
}

static inline void RemoveRange(AngleRange *R)
{
    if (R->next)
        R->next->previous = R->previous;
    else
        occlusion_buffer_tail = R->previous;

    if (R->previous)
        R->previous->next = R->next;
    else
        occlusion_buffer_head = R->next;

    // add it to the quick-alloc list
    R->next     = free_occlusion_range;
    R->previous = nullptr;

    free_occlusion_range = R;
}

static void DoSet(BAMAngle low, BAMAngle high)
{
    for (AngleRange *AR = occlusion_buffer_head; AR; AR = AR->next)
    {
        if (high < AR->low)
        {
            LinkBefore(AR, GetNewRange(low, high));
            return;
        }

        if (low > AR->high) continue;

        // the new range overlaps the old range.
        //
        // The above test (i.e. low > AR->high) guarantees that if
        // we reduce AR->low, it cannot touch the previous range.
        //
        // However by increasing AR->high we may touch or overlap
        // some subsequent ranges in the list.  When that happens,
        // we must remove them (and adjust the current range).

        AR->low  = HMM_MIN(AR->low, low);
        AR->high = HMM_MAX(AR->high, high);

#ifdef EDGE_DEBUG_OCCLUSION
        if (AR->prev) { SYS_ASSERT(AR->low > AR->prev->high); }
#endif
        while (AR->next && AR->high >= AR->next->low)
        {
            AR->high = HMM_MAX(AR->high, AR->next->high);

            RemoveRange(AR->next);
        }

        return;
    }

    // the new range is greater than all existing ranges

    LinkInTail(GetNewRange(low, high));
}

void RendererOcclusionSet(BAMAngle low, BAMAngle high)
{
    // Set all angles in the given range, i.e. mark them as blocking.
    // The angles are relative to the VIEW angle.

    SYS_ASSERT((BAMAngle)(high - low) < kBAMAngle180);

    if (low <= high)
        DoSet(low, high);
    else
    {
        DoSet(low, kBAMAngle360);
        DoSet(0, high);
    }

#ifdef EDGE_DEBUG_OCCLUSION
    ValidateBuffer();
#endif
}

static inline bool DoTest(BAMAngle low, BAMAngle high)
{
    for (AngleRange *AR = occlusion_buffer_head; AR; AR = AR->next)
    {
        if (AR->low <= low && high <= AR->high) return true;

        if (AR->high > low) break;
    }

    return false;
}

bool RendererOcclusionTest(BAMAngle low, BAMAngle high)
{
    // Check whether all angles in the given range are set (i.e. blocked).
    // Returns true if the entire range is blocked, false otherwise.
    // Angles are relative to the VIEW angle.

    SYS_ASSERT((BAMAngle)(high - low) < kBAMAngle180);

    if (low <= high)
        return DoTest(low, high);
    else
        return DoTest(low, kBAMAngle360) && DoTest(0, high);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab