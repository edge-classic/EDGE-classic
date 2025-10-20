//------------------------------------------------------------------------
//
//  AJ-BSP  Copyright (C) 2000-2023  Andrew Apted, et al
//          Copyright (C) 1994-1998  Colin Reed
//          Copyright (C) 1997-1998  Lee Killough
//
//  Originally based on the program 'BSP', version 2.3.
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
//------------------------------------------------------------------------

#include <vector>

#include "bsp_local.h"
#include "bsp_utility.h"
#include "bsp_wad.h"
#include "epi_doomdefs.h"

#define AJBSP_DEBUG_WALLTIPS  0
#define AJBSP_DEBUG_WINDOW_FX 0
#define AJBSP_DEBUG_OVERLAPS  0
namespace ajbsp
{

//------------------------------------------------------------------------
// ANALYZE : Analyzing level structures
//------------------------------------------------------------------------

bool Vertex::Overlaps(const Vertex *other) const
{
    double dx = fabs(other->x_ - x_);
    double dy = fabs(other->y_ - y_);

    return (dx < kEpsilon) && (dy < kEpsilon);
}

// cmpVertex and revised *Compare functions adapted from k8vavoom
static inline int cmpVertex(const Vertex *A, const Vertex *B)
{
    const double xdiff = (A->x_ - B->x_);
    if (fabs(xdiff) > 0.0001)
        return (xdiff < 0 ? -1 : 1);

    const double ydiff = (A->y_ - B->y_);
    if (fabs(ydiff) > 0.0001)
        return (ydiff < 0 ? -1 : 1);

    return 0;
}

static int VertexCompare(const void *p1, const void *p2)
{
    int vert1 = ((const uint32_t *)p1)[0];
    int vert2 = ((const uint32_t *)p2)[0];

    if (vert1 == vert2)
        return 0;

    Vertex *A = level_vertices[vert1];
    Vertex *B = level_vertices[vert2];

    return cmpVertex(A, B);
}

void DetectOverlappingVertices(void)
{
    size_t    i;
    uint32_t *array = (uint32_t *)UtilCalloc(level_vertices.size() * sizeof(uint32_t));

    // sort array of indices
    for (i = 0; i < level_vertices.size(); i++)
        array[i] = i;

    qsort(array, level_vertices.size(), sizeof(uint32_t), VertexCompare);

    // now mark them off
    for (i = 0; i < level_vertices.size() - 1; i++)
    {
        // duplicate ?
        if (VertexCompare(array + i, array + i + 1) == 0)
        {
            Vertex *A = level_vertices[array[i]];
            Vertex *B = level_vertices[array[i + 1]];

            // found an overlap !
            B->overlap_ = A->overlap_ ? A->overlap_ : A;
        }
    }

    UtilFree(array);

    // update the linedefs

    // update all in-memory linedefs.
    // DOES NOT affect the on-disk linedefs.
    // this is mainly to help the miniseg creation code.

    for (i = 0; i < level_linedefs.size(); i++)
    {
        Linedef *L = level_linedefs[i];

        while (L->start->overlap_)
        {
            L->start = L->start->overlap_;
        }

        while (L->end->overlap_)
        {
            L->end = L->end->overlap_;
        }
    }
}

void PruneVerticesAtEnd(void)
{
    int old_num = level_vertices.size();

    // scan all vertices.
    // only remove from the end, so stop when hit a used one.

    for (int i = level_vertices.size() - 1; i >= 0; i--)
    {
        Vertex *V = level_vertices[i];

        if (V->is_used_)
            break;

        UtilFree(V);

        level_vertices.pop_back();
    }

    int unused = old_num - level_vertices.size();

    if (unused > 0)
    {
        LogDebug("    Pruned %d unused vertices at end\n", unused);
    }

    num_old_vert = level_vertices.size();
}

static inline int LineVertexLowest(const Linedef *L)
{
    // returns the "lowest" vertex (normally the left-most, but if the
    // line is vertical, then the bottom-most) => 0 for start, 1 for end.

    return ((int)L->start->x_ < (int)L->end->x_ ||
            ((int)L->start->x_ == (int)L->end->x_ && (int)L->start->y_ < (int)L->end->y_))
               ? 0
               : 1;
}

static int LineStartCompare(const void *p1, const void *p2)
{
    int line1 = ((const int *)p1)[0];
    int line2 = ((const int *)p2)[0];

    if (line1 == line2)
        return 0;

    Linedef *A = level_linedefs[line1];
    Linedef *B = level_linedefs[line2];

    // determine left-most vertex of each line
    Vertex *C = LineVertexLowest(A) ? A->end : A->start;
    Vertex *D = LineVertexLowest(B) ? B->end : B->start;

    return cmpVertex(C, D);
}

static int LineEndCompare(const void *p1, const void *p2)
{
    int line1 = ((const int *)p1)[0];
    int line2 = ((const int *)p2)[0];

    if (line1 == line2)
        return 0;

    Linedef *A = level_linedefs[line1];
    Linedef *B = level_linedefs[line2];

    // determine right-most vertex of each line
    Vertex *C = LineVertexLowest(A) ? A->start : A->end;
    Vertex *D = LineVertexLowest(B) ? B->start : B->end;

    return cmpVertex(C, D);
}

void DetectOverlappingLines(void)
{
    // Algorithm:
    //   Sort all lines by left-most vertex.
    //   Overlapping lines will then be near each other in this set.
    //   Note: does not detect partially overlapping lines.

    size_t i;
    int   *array = (int *)UtilCalloc(level_linedefs.size() * sizeof(int));

    // sort array of indices
    for (i = 0; i < level_linedefs.size(); i++)
        array[i] = i;

    qsort(array, level_linedefs.size(), sizeof(int), LineStartCompare);

    for (i = 0; i < level_linedefs.size() - 1; i++)
    {
        size_t j;

        for (j = i + 1; j < level_linedefs.size(); j++)
        {
            if (LineStartCompare(array + i, array + j) != 0)
                break;

            if (LineEndCompare(array + i, array + j) == 0)
            {
                // found an overlap !

                Linedef *A = level_linedefs[array[i]];
                Linedef *B = level_linedefs[array[j]];

                B->overlap = A->overlap ? A->overlap : A;
            }
        }
    }

    UtilFree(array);
}

/* ----- vertex routines ------------------------------- */

void Vertex::AddWallTip(double dx, double dy, bool open_left, bool open_right)
{
    EPI_ASSERT(overlap_ == nullptr);

    WallTip *tip = NewWallTip();
    WallTip *after;

    tip->angle      = ComputeAngle(dx, dy);
    tip->open_left  = open_left;
    tip->open_right = open_right;

    // find the correct place (order is increasing angle)
    for (after = tip_set_; after && after->next; after = after->next)
    {
    }

    while (after && tip->angle + kEpsilon < after->angle)
        after = after->previous;

    // link it in
    tip->next     = after ? after->next : tip_set_;
    tip->previous = after;

    if (after)
    {
        if (after->next)
            after->next->previous = tip;

        after->next = tip;
    }
    else
    {
        if (tip_set_ != nullptr)
            tip_set_->previous = tip;

        tip_set_ = tip;
    }
}

void CalculateWallTips()
{
    for (size_t i = 0; i < level_linedefs.size(); i++)
    {
        const Linedef *L = level_linedefs[i];

        if (L->overlap || L->zero_length)
            continue;

        double x1 = L->start->x_;
        double y1 = L->start->y_;
        double x2 = L->end->x_;
        double y2 = L->end->y_;

        bool left  = (L->left != nullptr) && (L->left->sector != nullptr);
        bool right = (L->right != nullptr) && (L->right->sector != nullptr);

        // note that start->overlap and end->overlap should be nullptr
        // due to logic in DetectOverlappingVertices.

        L->start->AddWallTip(x2 - x1, y2 - y1, left, right);
        L->end->AddWallTip(x1 - x2, y1 - y2, right, left);
    }

#if AJBSP_DEBUG_WALLTIPS
    for (int k = 0; k < level_vertices.size(); k++)
    {
        Vertex *V = level_vertices[k];

        LogDebug("WallTips for vertex %d:\n", k);

        for (WallTip *tip = V->tip_set; tip; tip = tip->next)
        {
            LogDebug("  Angle=%1.1f left=%d right=%d\n", tip->angle, tip->open_left ? 1 : 0, tip->open_right ? 1 : 0);
        }
    }
#endif
}

Vertex *NewVertexFromSplitSeg(Seg *seg, double x, double y)
{
    Vertex *vert = NewVertex();

    vert->x_ = x;
    vert->y_ = y;

    vert->is_new_  = true;
    vert->is_used_ = true;

    vert->index_ = num_new_vert;
    num_new_vert++;

    // compute wall-tip info
    if (seg->linedef_ == nullptr)
    {
        vert->AddWallTip(seg->pdx_, seg->pdy_, true, true);
        vert->AddWallTip(-seg->pdx_, -seg->pdy_, true, true);
    }
    else
    {
        const Sidedef *front = seg->side_ ? seg->linedef_->left : seg->linedef_->right;
        const Sidedef *back  = seg->side_ ? seg->linedef_->right : seg->linedef_->left;

        bool left  = (back != nullptr) && (back->sector != nullptr);
        bool right = (front != nullptr) && (front->sector != nullptr);

        vert->AddWallTip(seg->pdx_, seg->pdy_, left, right);
        vert->AddWallTip(-seg->pdx_, -seg->pdy_, right, left);
    }

    return vert;
}

Vertex *NewVertexDegenerate(Vertex *start, Vertex *end)
{
    // this is only called when rounding off the BSP tree and
    // all the segs are degenerate (zero length), hence we need
    // to create at least one seg which won't be zero length.

    double dx = end->x_ - start->x_;
    double dy = end->y_ - start->y_;

    double dlen = hypot(dx, dy);

    Vertex *vert = NewVertex();

    vert->is_new_  = false;
    vert->is_used_ = true;

    vert->index_ = num_old_vert;
    num_old_vert++;

    // compute new coordinates

    vert->x_ = start->x_;
    vert->y_ = start->x_;

    if (AlmostEquals(dlen, 0.0))
        FatalError("AJBSP: NewVertexDegenerate: bad delta!\n");

    dx /= dlen;
    dy /= dlen;

    while (RoundToInteger(vert->x_) == RoundToInteger(start->x_) &&
           RoundToInteger(vert->y_) == RoundToInteger(start->y_))
    {
        vert->x_ += dx;
        vert->y_ += dy;
    }

    return vert;
}

bool Vertex::CheckOpen(double dx, double dy) const
{
    const WallTip *tip;

    double angle = ComputeAngle(dx, dy);

    // first check whether there's a wall-tip that lies in the exact
    // direction of the given direction (which is relative to the
    // vertex).

    for (tip = tip_set_; tip; tip = tip->next)
    {
        if (fabs(tip->angle - angle) < kEpsilon || fabs(tip->angle - angle) > (360.0 - kEpsilon))
        {
            // found one, hence closed
            return false;
        }
    }

    // OK, now just find the first wall-tip whose angle is greater than
    // the angle we're interested in.  Therefore we'll be on the RIGHT
    // side of that wall-tip.

    for (tip = tip_set_; tip; tip = tip->next)
    {
        if (angle + kEpsilon < tip->angle)
        {
            // found it
            return tip->open_right;
        }

        if (!tip->next)
        {
            // no more tips, thus we must be on the LEFT side of the tip
            // with the largest angle.

            return tip->open_left;
        }
    }

    // usually won't get here
    return true;
}

} // namespace ajbsp

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
