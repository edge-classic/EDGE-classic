//----------------------------------------------------------------------------
//  Tiny Nodes Builder
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2009  Andrew Apted
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

#include "i_defs.h"

#include "r_defs.h"
#include "r_state.h"
#include "m_bbox.h"
#include "p_local.h"
#include "dm_struct.h"

#include <algorithm>


#define DIST_EPSILON  0.2f

// #define DEBUG_CLOCKWISE  1

#define MIN_BINARY_SEG   160
#define MIN_BINARY_DIST  64


static int max_segs;
static int max_subsecs;
static int max_glvert;

static void Poly_Setup(void)
{
	numsegs = 0;
	numsubsectors = 0;
	numnodes = 0;
	num_gl_vertexes = 0;

	max_segs    = numsides * 4;
	max_subsecs = numlines * 2;
	max_glvert  = numsides * 3;

	segs        = new seg_t[max_segs];
	subsectors  = new subsector_t[max_subsecs];
	nodes       = new node_t[max_subsecs];
	gl_vertexes = new vertex_t[max_glvert];

	Z_Clear(segs, seg_t, max_segs);
	Z_Clear(subsectors, subsector_t, max_subsecs);
	Z_Clear(nodes, node_t, max_subsecs);
	Z_Clear(gl_vertexes, vertex_t, max_glvert);
}

// Nasty allocation shit....  Fixme

static seg_t *NewSeg(void)
{
	if (numsegs >= max_segs)
		I_Error("R_PolygonizeMap: ran out of segs !\n");

	seg_t *seg = segs + numsegs;

	memset(seg, 0, sizeof(seg_t));

	numsegs++;

	return seg;
}

static subsector_t *NewSubsector(void)
{
	if (numsubsectors >= max_segs)
		I_Error("WF_BuildBSP: ran out of subsectors !\n");

	subsector_t *sub = subsectors + numsubsectors;

	memset(sub, 0, sizeof(subsector_t));

	numsubsectors++;

	return sub;
}

static vertex_t *NewVertex(float x, float y)
{
	if (num_gl_vertexes >= max_glvert)
		I_Error("R_PolygonizeMap: ran out of vertices !\n");

	vertex_t *vert = gl_vertexes + num_gl_vertexes;

	memset(vert, 0, sizeof(vertex_t));
	vert->x = x;
	vert->y = y;

	num_gl_vertexes++;

	return vert;
}

static node_t *NewNode(void)
{
	if (numnodes >= max_subsecs)
		I_Error("R_PolygonizeMap: ran out of nodes !\n");

	node_t *node = nodes + numnodes;

	memset(node, 0, sizeof(node_t));

	numnodes++;

	return node;
}


//----------------------------------------------------------------------------

// an "intersection" remembers the vertex that touches a BSP divider
// line (especially a new vertex that is created at a seg split).

// Note: two points can exist in the intersection list with
//       the same along value but different dirs.
typedef struct
{
	vertex_t *v;

	// how far along the partition line the vertex is.
	// bigger value are further along the partition line.
	// Only used for sorting the list.
	int along;

	// direction that miniseg will touch, +1 for further along
	// the partition, and -1 for backwards on the partition.
	// The values +2 and -2 indicate REMOVED points.
	int dir;
}
intersect_t;

struct Compare_Intersect_pred
{
	inline bool operator() (const intersect_t& A, const intersect_t& B) const
	{
		if (A.along != B.along)
			return A.along < B.along;

		return A.dir < B.dir;
	}
};


static subsector_t *CreateSubsector(void)
{
	subsector_t *sub = NewSubsector();

	sub->seg_count = 0;

	M_ClearBox(sub->bbox);
	
	return sub;
}

static node_t *CreateNode(divline_t *party)
{
	node_t *nd = NewNode();

	nd->div = *party;
	nd->div_len = R_PointToDist(0, 0, party->dx, party->dy);

	// node bbox is computed at very end
	
	return nd;
}

static void AddSeg(subsector_t *sub, seg_t *seg)
{
	// add seg to subsector
	seg->sub_next = sub->segs;
	sub->segs = seg;

	sub->seg_count++;

	M_AddToBox(sub->bbox, seg->v1->x, seg->v1->y);
	M_AddToBox(sub->bbox, seg->v2->x, seg->v2->y);
}

static seg_t * CreateOneSeg(line_t *ld, sector_t *sec, int side,
                            vertex_t *v1, vertex_t *v2)
{
	seg_t *g = NewSeg();

	g->v1 = v1;
	g->v2 = v2;
	g->angle = R_PointToAngle(v1->x, v1->y, v2->x, v2->y);

	// Note: these fields are setup afterwards:
	//          back_sub,
	//          backsector
	//          length
	//          offset
	//

	g->miniseg = false;

	g->offset  = 0;
	g->sidedef = ld->side[side];
	g->linedef = ld;
	g->side    = side;

	g->frontsector = sec;

	return g;
}

static seg_t * CreateMiniSeg(vertex_t *v1, vertex_t *v2)
{
	seg_t *g = NewSeg();

	g->v1 = v1;
	g->v2 = v2;
	g->angle = R_PointToAngle(v1->x, v1->y, v2->x, v2->y);

	// Note: these fields are setup by GroupLines:
	//          front_sub,
	//          back_sub
	//          backsector

	g->miniseg = true;

	return g;
}


static inline void ComputeIntersection(seg_t *cur, divline_t& party,
  float perp_c, float perp_d, float *x, float *y)
{
	// horizontal partition against vertical seg
	if (party.dy == 0 && cur->v1->x == cur->v2->x)
	{
		*x = cur->v1->x;
		*y = party.y;
		return;
	}

	// vertical partition against horizontal seg
	if (party.dx == 0 && cur->v1->y == cur->v2->y)
	{
		*x = party.x;
		*y = cur->v1->y;
		return;
	}

	// 0 = start, 1 = end
	double ds = perp_c / (perp_c - perp_d);

	*x = cur->v1->x + ds * (cur->v2->x - cur->v1->x);
	*y = cur->v1->y + ds * (cur->v2->y - cur->v1->y);
}

//
// SplitSeg
//
// -AJA- Splits the given seg at the point (x,y).  The new seg is
//       returned.  The old seg is shortened (the original start
//       vertex is unchanged), whereas the new seg becomes the cut-off
//       tail (keeping the original end vertex).
//
//       If the seg has a partner, than that partner is also split.
//       NOTE WELL: the new piece of the partner seg is inserted into
//       the same list as the partner seg (and after it) -- thus ALL
//       segs (except the one we are currently splitting) must exist
//       on a singly-linked list somewhere. 
//
static seg_t *SplitSeg(seg_t *old_seg, float x, float y)
{
# if DEBUG_SPLIT
	if (old_seg->linedef)
		PrintDebug("Splitting Linedef %d (%p) at (%1.1f,%1.1f)\n",
				old_seg->linedef->index, old_seg, x, y);
	else
		PrintDebug("Splitting Miniseg %p at (%1.1f,%1.1f)\n", old_seg, x, y);
# endif

	seg_t *new_seg = NewSeg();

	vertex_t *new_vert = NewVertex(x, y);

	// copy seg info
	new_seg[0] = old_seg[0];
	new_seg->sub_next = NULL;

	old_seg->v2 = new_vert;
	new_seg->v1 = new_vert;

# if DEBUG_SPLIT
	PrintDebug("Splitting Vertex is %04X at (%1.1f,%1.1f)\n",
			new_vert->index, new_vert->x, new_vert->y);
# endif

	// handle partners

	if (old_seg->partner)
	{
#   if DEBUG_SPLIT
		PrintDebug("Splitting Partner %p\n", old_seg->partner);
#   endif

		new_seg->partner = NewSeg();

		// copy seg info
		new_seg->partner[0] = old_seg->partner[0];

		// IMPORTANT: keep partner relationship valid
		new_seg->partner->partner = new_seg;

		old_seg->partner->v1 = new_vert;
		new_seg->partner->v2 = new_vert;

		// link it into list
		old_seg->partner->sub_next = new_seg->partner;
	}

	return new_seg;
}


static void Poly_CreateSegs(subsector_t *sub)
{
	for (int i=0; i < numlines; i++)
	{
		line_t *ld = &lines[i];

		// ignore zero-length lines		
		if (ld->length < 0.1)
			continue;

		// TODO: handle overlapping lines

		// ignore self-referencing lines
		if (ld->frontsector == ld->backsector)
			continue;

		seg_t *right = CreateOneSeg(ld, ld->frontsector, 0, ld->v1, ld->v2);
		seg_t *left  = NULL;

		if (ld->backsector)
		{
			left = CreateOneSeg(ld, ld->backsector, 1, ld->v2, ld->v1 );

			right->partner = left;
			left ->partner = right;
		}

		if (right) AddSeg(sub, right);
		if (left)  AddSeg(sub, left);
	}
}


static void DetermineMiddle(subsector_t *sub, float *x, float *y)
{
	int total=0;

	*x = 0;
	*y = 0;

	for (seg_t *cur = sub->segs; cur; cur = cur->sub_next)
	{
		*x += cur->v1->x + cur->v2->x;
		*y += cur->v1->y + cur->v2->y;

		total += 2;
	}

	*x /= total;
	*y /= total;
}


static void ClockwiseOrder(subsector_t *sub)
{
	float mid_x, mid_y;

	DetermineMiddle(sub, &mid_x, &mid_y);

	seg_t *cur;
	seg_t ** array;
	seg_t *seg_buffer[32];

	int i;
	int total = 0;
	int first = 0;

#if DEBUG_CLOCKWISE
	I_Debugf("Subsec: Clockwising %d\n", sub - subsectors);
#endif

	// count segs and create an array to manipulate them
	for (cur=sub->segs; cur; cur=cur->sub_next)
		total++;

	// use local array if small enough
	if (total <= 32)
		array = seg_buffer;
	else
		array = new seg_t* [total];

	for (cur=sub->segs, i=0; cur; cur=cur->sub_next, i++)
		array[i] = cur;

	if (i != total)
		I_Error("ClockwiseOrder miscounted.");

	// sort segs by angle (from the middle point to the start vertex).
	// The desired order (clockwise) means descending angles.

	i = 0;

	while (i+1 < total)
	{
		seg_t *A = array[i];
		seg_t *B = array[i+1];

		angle_t angle1, angle2;

		angle1 = R_PointToAngle(mid_x, mid_y, A->v1->x, A->v1->y);
		angle2 = R_PointToAngle(mid_x, mid_y, B->v1->x, B->v1->y);

		if (angle1 < angle2)
		{
			// swap 'em
			array[i] = B;
			array[i+1] = A;

			// bubble down
			if (i > 0)
				i--;
		}
		else
		{
			// bubble up
			i++;
		}
	}

	// transfer sorted array back into sub
	sub->segs = NULL;

	for (i=total-1; i >= 0; i--)
	{
		int j = (i + first) % total;

		array[j]->sub_next = sub->segs;
		sub->segs = array[j];

		// assign 'front_sub' here
		array[j]->front_sub = sub;
	}

	if (total > 32)
		delete[] array;

#if DEBUG_CLOCKWISE
	I_Debugf("Sorted SEGS around (%1.1f,%1.1f)\n", mid_x, mid_y);

	for (cur=sub->segs; cur; cur=cur->sub_next)
	{
		angle_t angle = R_PointToAngle(mid_x, mid_y, cur->v1->x, cur->v1->y);

		I_Debugf("  Seg %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
				cur, ANG_2_FLOAT(angle), cur->v1->x, cur->v1->y, cur->v2->x, cur->v2->y);
	}
#endif
}


static inline bool SameDir(divline_t& party, seg_t *seg)
{
	float sdx = seg->v2->x - seg->v1->x;
	float sdy = seg->v2->y - seg->v1->y;

	return (sdx * party.dx + sdy * party.dy) >= 0;
}

static inline float PerpDist(divline_t& party, float x, float y)
{
	x -= party.x;
	y -= party.y;

	float len = sqrt(party.dx * party.dx + party.dy * party.dy);

	return (x * party.dy - y * party.dx) / len;
}

static inline float AlongDist(divline_t& party, float x, float y)
{
	x -= party.x;
	y -= party.y;

	float len = sqrt(party.dx * party.dx + party.dy * party.dy);

	return (x * party.dx + y * party.dy) / len;
}


static void AddIntersection(std::vector<intersect_t> & cut_list,
                               divline_t & party, vertex_t *v, int dir)
{
	float along = AlongDist(party, v->x, v->y);

	intersect_t new_cut;

	new_cut.v = v;
	new_cut.along = I_ROUND(along * 5.6f);
	new_cut.dir = dir;

	cut_list.push_back(new_cut);
}


static float EvaluateParty(seg_t *seg_list, divline_t& party)
{
	int left  = 0;
	int right = 0;
	int split = 0;

	for (seg_t *cur = seg_list; cur; cur = cur->sub_next)
	{
		float a = PerpDist(party, cur->v1->x, cur->v1->y);
		float b = PerpDist(party, cur->v2->x, cur->v2->y);

		int a_side = (a < -DIST_EPSILON) ? -1 : (a > DIST_EPSILON) ? +1 : 0;
		int b_side = (b < -DIST_EPSILON) ? -1 : (b > DIST_EPSILON) ? +1 : 0;

		if (a_side == 0 && b_side == 0)
		{
			// seg runs along the same line as the partition
			if (SameDir(party, cur))
				right++;
			else
				left++;
		}
		else if (a_side >= 0 && b_side >= 0)
		{
			right++;
		}
		else if (a_side <= 0 && b_side <= 0)
		{
			left++;
		}
		else
		{
			// partition line would split this seg
			left++; right++; split++;
		}
	}

	// completely unusable?
	if (left == 0 || right == 0)
		return -1;

	// determine cost
	int diff  = abs(left - right);
	int total = left + right;

	return (diff * 100.0f + split * split * 365.0f) / (float)total;
}


static void DivideOneSeg(seg_t *cur, divline_t& party,
	subsector_t *left, subsector_t *right,
	std::vector<intersect_t> & cut_list)
{
	seg_t *new_seg;

	/* get state of lines' relation to each other */
	float a = PerpDist(party, cur->v1->x, cur->v1->y);
	float b = PerpDist(party, cur->v2->x, cur->v2->y);

	int a_side = (a < -DIST_EPSILON) ? -1 : (a > DIST_EPSILON) ? +1 : 0;
	int b_side = (b < -DIST_EPSILON) ? -1 : (b > DIST_EPSILON) ? +1 : 0;

	/* check for being on the same line */
	if (a_side == 0 && b_side == 0)
	{
		// this seg runs along the same line as the partition.  check
		// whether it goes in the same direction or the opposite.

		if (SameDir(party, cur))
		{
			AddSeg(right, cur);

			// +2 and -2 mean "remove"
			AddIntersection(cut_list, party, cur->v1, +2);
			AddIntersection(cut_list, party, cur->v2, -2);
		}
		else
		{
			AddSeg(left, cur);

			AddIntersection(cut_list, party, cur->v1, -2);
			AddIntersection(cut_list, party, cur->v2, +2);
		}

		return;
	}

	/* check for right side */
	if (a_side >= 0 && b_side >= 0)
	{
		if (a_side == 0)
			AddIntersection(cut_list, party, cur->v1, -1);
		else if (b_side == 0)
			AddIntersection(cut_list, party, cur->v2, +1);

		AddSeg(right, cur);
		return;
	}

	/* check for left side */
	if (a_side <= 0 && b_side <= 0)
	{
		if (a_side == 0)
			AddIntersection(cut_list, party, cur->v1, +1);
		else if (b_side == 0)
			AddIntersection(cut_list, party, cur->v2, -1);

		AddSeg(left, cur);
		return;
	}

	// when we reach here, we have a and b non-zero and opposite sign,
	// hence this seg will be split by the partition line.

	SYS_ASSERT(a_side == -b_side);

	float x, y;

	ComputeIntersection(cur, party, a, b, &x, &y);

	new_seg = SplitSeg(cur, x, y);

	if (a_side < 0)
	{
		AddSeg(left,  cur);
		AddSeg(right, new_seg);
	}
	else
	{
		AddSeg(right, cur);
		AddSeg(left,  new_seg);
	}

	AddIntersection(cut_list, party, new_seg->v1, a_side);
}


static float NiceMidwayPoint(float low, float extent)
{
	int pow2 = 1;

	while (pow2 < extent/5)
		pow2 = pow2 << 1;
	
	int mid = I_ROUND((low + extent/2.0f) / pow2) * pow2;

	// SYS_ASSERT(low < mid && mid < low+extent);

	return mid;
}


static bool ChoosePartition(subsector_t *sub, divline_t *div)
{
	int total = 0;
	for (seg_t *Z = sub->segs; Z; Z=Z->sub_next)
		total++;

	if (total <= 3)
		return false;


	// try a pure binary split
	if (sub->seg_count >= MIN_BINARY_SEG)
	{
		float w = sub->bbox[BOXRIGHT] - sub->bbox[BOXLEFT];
		float h = sub->bbox[BOXTOP]   - sub->bbox[BOXBOTTOM];

		// IDEA: evaluate both options and pick best

		if (MAX(w, h) >= MIN_BINARY_DIST)
		{
			if (w >= h)
			{
				div->x  = NiceMidwayPoint(sub->bbox[BOXLEFT], w);
				div->y  = 0.0f;
				div->dx = 0.0f;
				div->dy = 1.0f;

//				I_Debugf("split horizontally @ x=%1.1f\n", div->x);
			}
			else
			{
				div->x  = 0.0f;
				div->y  = NiceMidwayPoint(sub->bbox[BOXBOTTOM], h);
				div->dx = 1.0f;
				div->dy = 0.0f;

//				I_Debugf("split vertically @ y=%1.1f\n", div->y);
			}
			return true;
		}
	}


	seg_t *best = NULL;
	float best_cost = 1e30;

	for (seg_t *P = sub->segs; P; P=P->sub_next)
	{
		if (P->miniseg)
			continue;

		divline_t party;

		party.x  = P->v1->x;
		party.y  = P->v1->y;
		party.dx = P->v2->x - party.x;
		party.dy = P->v2->y - party.y;

		float cost = EvaluateParty(sub->segs, party);

		if (cost < 0)
			continue;

		if (cost < best_cost)
		{
			best = P;
			best_cost = cost;
		}
	}

	if (! best)
		return false;

	div->x  = best->v1->x;
	div->y  = best->v1->y;
	div->dx = best->v2->x - div->x;
	div->dy = best->v2->y - div->y;
	
	return true;
}



static void AddMinisegs(divline_t& party,
	subsector_t *left, subsector_t *right,
	std::vector<intersect_t> & cut_list)
{
	if (cut_list.empty())
		return;

	std::sort(cut_list.begin(), cut_list.end(),
	          Compare_Intersect_pred());

	std::vector<intersect_t>::iterator A, B;

	A = cut_list.begin();

	while (A != cut_list.end())
	{
		if (A->dir != +1)
		{
			A++; continue;
		}

		B = A; B++;

		if (B == cut_list.end())
			break;

		// this handles multiple +1 entries and also ensures
		// that the +2 "remove" entry kills a +1 entry.
		if (A->along == B->along)
		{
			A++; continue;
		}

		if (B->dir != -1)
		{
			I_Debugf("WARNING: bad pair in intersection list\n");

			A = B; continue;
		}

		// found a viable miniseg!

		seg_t *seg_R = CreateMiniSeg(A->v, B->v);
		seg_t *seg_L = CreateMiniSeg(B->v, A->v);
		
		seg_R->partner = seg_L;
		seg_L->partner = seg_R;

		AddSeg(right, seg_R);
		AddSeg(left,  seg_L);

		B++; A = B; continue;
	}
}


static unsigned int Poly_Split(subsector_t *right)
{
	divline_t party;

	if (! ChoosePartition(right, &party))
	{
		return NF_V5_SUBSECTOR | (unsigned int)(right - subsectors);
	}

	node_t *node = CreateNode(&party);

	subsector_t *left = CreateSubsector();

	seg_t *seg_list = right->segs;

	right->segs = NULL;
	right->seg_count = 0;
	M_ClearBox(right->bbox);

	std::vector<intersect_t> cut_list;

	while (seg_list)
	{
		seg_t *cur = seg_list;
		seg_list = seg_list->sub_next;

		DivideOneSeg(cur, party, left, right, cut_list);
	}

	AddMinisegs(party, left, right, cut_list);

	node->children[0] = Poly_Split(right);
	node->children[1] = Poly_Split(left);

	return (unsigned int) (node - nodes);
}


static inline void DoAssignSec(subsector_t *sub, sector_t *sec)
{
	SYS_ASSERT(sec);

	sub->sector = sec;

	sub->sec_next = sec->subsectors;
	sec->subsectors = sub;
}


static void Poly_AssignSectors(void)
{
	for (int i = 0; i < numsubsectors; i++)
	{
		subsector_t *sub = &subsectors[i];

		for (seg_t *seg = sub->segs; seg; seg = seg->sub_next)
		{
			seg->front_sub = sub;

			if (! seg->miniseg && ! sub->sector)
				DoAssignSec(sub, seg->frontsector);
		}
	}

	// determine back_sub for each seg
	for (int k = 0; k < numsegs; k++)
	{
		seg_t *seg = &segs[k];

		if (seg->partner)
		{
			seg->back_sub   = seg->partner->front_sub;
			seg->backsector = seg->partner->frontsector;
		}
	}

	// take care of any island subsectors (all segs are minisegs)
	for (int loop = 0; loop < 32; loop--)
	{
		int changes = 0;

		for (int i = 0; i < numsubsectors; i++)
		{
			subsector_t *sub = &subsectors[i];

			if (sub->sector)
				continue;

			for (seg_t *seg = sub->segs; seg; seg = seg->sub_next)
			{
				if (seg->back_sub && seg->back_sub->sector)
				{
					I_Debugf("TinyBSP: Island fill for sub:%d sector:%d\n",
							 sub - subsectors, seg->back_sub->sector - sectors);

					DoAssignSec(sub, seg->back_sub->sector);
					changes++;
					break;
				}
			}
		}

		if (changes == 0)  // all done
			break;
	}

	// emergency fallback

	for (int i = 0; i < numsubsectors; i++)
	{
		subsector_t *sub = &subsectors[i];

		if (! sub->sector)
		{
			I_Debugf("TinyBSP: Emergency fallback for sub:%d\n", sub - subsectors);
			DoAssignSec(sub, &sectors[0]);
		}
	}
}


static void Poly_FinishSegs(void)
{
	for (int k = 0; k < numsegs; k++)
	{
		seg_t *g = &segs[k];

		g->length = R_PointToDist(g->v1->x, g->v1->y, g->v2->x, g->v2->y);

		if (! g->miniseg)
		{
			float lx = g->side ? g->linedef->v2->x : g->linedef->v1->x;
			float ly = g->side ? g->linedef->v2->y : g->linedef->v1->y;

			g->offset = R_PointToDist(lx, ly, g->v1->x, g->v1->y);
		}
	}
}


void TinyBSP(void)
{
	I_Printf("Building nodes with TinyBSP...\n");

	Poly_Setup();

	subsector_t *base_sub = CreateSubsector();

	Poly_CreateSegs(base_sub);

	root_node = Poly_Split(base_sub);

	Poly_AssignSectors();
	Poly_FinishSegs();

	for (int k = 0; k < numsubsectors; k++)
		ClockwiseOrder(&subsectors[k]);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
