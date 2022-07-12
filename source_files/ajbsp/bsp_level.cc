//------------------------------------------------------------------------
//
//  AJ-BSP  Copyright (C) 2000-2018  Andrew Apted, et al
//          Copyright (C) 1994-1998  Colin Reed
//          Copyright (C) 1997-1998  Lee Killough
//
//  Originally based on the program 'BSP', version 2.3.
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
//------------------------------------------------------------------------

#include "ajbsp.h"

namespace ajbsp
{

#define DEBUG_BLOCKMAP  0


static int block_x, block_y;
static int block_w, block_h;
static int block_count;

static int block_mid_x = 0;
static int block_mid_y = 0;

static u16_t ** block_lines;

static u16_t *block_ptrs;
static u16_t *block_dups;

static int block_compression;
static int block_overflowed;

#define BLOCK_LIMIT  16000

#define DUMMY_DUP  0xFFFF

// UDMF parser and loading routines backported from EDGE 2.x codebase
// with non-Vanilla namespace items removed for the time being
typedef struct {
	uint8_t *buffer;
	uint8_t line[512];
	int length;
	int next;
	int prev;
} parser_t;

static byte *udmf_lump;

static parser_t udmf_psr;

static bool GetNextLine(parser_t *psr)
{
	if (psr->next >= psr->length)
		return false; // no more lines

	int i;
	// get next line
	psr->prev = psr->next;
	uint8_t *lp = &psr->buffer[psr->next];
	for (i=0; i<(psr->length - psr->next); i++, lp++)
		if (*lp == 0x0A || *lp == 0x0D)
			break;
	if (i == (psr->length - psr->next))
		lp = &psr->buffer[psr->length - 1]; // last line
	psr->next = (int)(lp - psr->buffer) + 1;
	memcpy(psr->line, &psr->buffer[psr->prev], MIN(511, psr->next - psr->prev - 1));
	psr->line[MIN(511, psr->next - psr->prev) - 1] = 0;
	// skip any more CR/LF
	while (psr->buffer[psr->next] == 0x0A || psr->buffer[psr->next] == 0x0D)
		psr->next++;

	// check for comments
	lp = psr->line;
	while (lp[0] != 0 && lp[0] != 0x2F && lp[1] != 0x2F)
		lp++; // find full line comment start (if present)
	if (lp[0] != 0)
	{
		*lp = 0; // terminate at full line comment start
	}
	else
	{
		lp = psr->line;
		while (lp[0] != 0 && lp[0] != 0x2F && lp[1] != 0x2A)
			lp++; // find multi-line comment start (if present)
		if (lp[0] != 0)
		{
			*lp = 0; // terminate at multi-line comment start
			uint8_t *ep = &lp[2];
			while (ep[0] != 0 && ep[0] != 0x2A && ep[1] != 0x2F)
				ep++; // find multi-line comment end (if present)
			if (ep[0] == 0)
			{
				ep = &psr->buffer[psr->next];
				for (i=0; i<(psr->length - psr->next); i++, ep++)
					if (ep[0] == 0x2A && ep[1] == 0x2F)
						break;
				if (i == (psr->length - psr->next))
					ep = &psr->buffer[psr->length - 2];
			}
			psr->next = (int)(ep - psr->buffer) + 2; // skip comment for next line
		}
	}

	//I_Debugf(" parser next line: %s\n", (char *)psr->line);
	return true;
}

static bool GetNextAssign(parser_t *psr, uint8_t *ident, uint8_t *val)
{
	if (!GetNextLine(psr))
		return false; // no more lines

	int len = 0;
	while (psr->line[len] != 0 && psr->line[len] != 0x7D)
		len++;
	if (psr->line[len] == 0x7D)
	{
		//I_Debugf(" parser: block end\n");
		return false;
	}
	else if (len < 4)
		return false; //line too short for assignment

	uint8_t *lp = psr->line;
	while (*lp != 0x3D && *lp != 0)
		lp++; // find '='
	if (lp[0] != 0x3D)
		return false; // not an assignment line

	*lp++ = 0; // split at assignment operator
	int i = 0;
	while (psr->line[i] == 0x20 || psr->line[i] == 0x09)
		i++; // skip whitespace before indentifier
	memcpy(ident, &psr->line[i], lp - &psr->line[i] - 1);
	i = lp - &psr->line[i] - 2;
	while (ident[i] == 0x20 || ident[i] == 0x09)
		i--; // skip whitespace after identifier
	ident[i+1] = 0;

	i = 0;
	while (lp[i] == 0x20 || lp[i] == 0x09 || lp[i] == 0x22)
		i++; // skip whitespace and quotes before value
	memcpy(val, &lp[i], &psr->line[len] - &lp[i]);
	i = (int)(&psr->line[len] - &lp[i]) - 2;
	while (val[i] == 0x20 || val[i] == 0x09 || val[i] == 0x22  || val[i] == 0x3B)
		i--; // skip whitespace, quote, and semi-colon after value
	val[i+1] = 0;

	//I_Debugf(" parser: ident = %s, val = %s\n", (char *)ident, (char *)val);
	return true;
}

static bool GetNextBlock(parser_t *psr, uint8_t *ident)
{
	if (!GetNextLine(psr))
		return false; // no more lines

	int len, i = 0;
	while (psr->line[i] == 0x20 || psr->line[i] == 0x09)
		i++; // skip whitespace
blk_loop:
	len = 0;
	while (psr->line[len] != 0)
		len++;

	memcpy(ident, &psr->line[i], len - i);
	i = len - i - 1;
	while (ident[i] == 0x20 || ident[i] == 0x09)
		i--; // skip whitespace from end of line
	ident[i+1] = 0;

	if (!GetNextLine(psr))
		return false; // no more lines

	i = 0;
	while (psr->line[i] == 0x20 || psr->line[i] == 0x09)
		i++; // skip whitespace before indentifier or block start
	if (psr->line[i] != 0x7B)
		goto blk_loop; // not a block start

	//I_Debugf(" parser: block start = %s\n", ident);
	return true;
}

static bool str2bool(char *val)
{
	if (y_stricmp(val, "true") == 0)
		return true;

	// default is always false
	return false;
}

static int str2int(char *val, int def)
{
	int ret;

	if (sscanf(val, "%d", &ret) == 1)
		return ret;

	// error - return default
	return def;
}

static float str2float(char *val, float def)
{
	float ret;

	if (sscanf(val, "%f", &ret) == 1)
		return ret;

	// error - return default
	return def;
}

void GetBlockmapBounds(int *x, int *y, int *w, int *h)
{
	*x = block_x; *y = block_y;
	*w = block_w; *h = block_h;
}


int CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax,
		int x1, int y1, int x2, int y2)
{
	int count = 2;
	int tmp;

	for (;;)
	{
		if (y1 > ymax)
		{
			if (y2 > ymax)
				return false;

			x1 = x1 + (int) ((x2-x1) * (double)(ymax-y1) / (double)(y2-y1));
			y1 = ymax;

			count = 2;
			continue;
		}

		if (y1 < ymin)
		{
			if (y2 < ymin)
				return false;

			x1 = x1 + (int) ((x2-x1) * (double)(ymin-y1) / (double)(y2-y1));
			y1 = ymin;

			count = 2;
			continue;
		}

		if (x1 > xmax)
		{
			if (x2 > xmax)
				return false;

			y1 = y1 + (int) ((y2-y1) * (double)(xmax-x1) / (double)(x2-x1));
			x1 = xmax;

			count = 2;
			continue;
		}

		if (x1 < xmin)
		{
			if (x2 < xmin)
				return false;

			y1 = y1 + (int) ((y2-y1) * (double)(xmin-x1) / (double)(x2-x1));
			x1 = xmin;

			count = 2;
			continue;
		}

		count--;

		if (count == 0)
			break;

		// swap end points
		tmp=x1;  x1=x2;  x2=tmp;
		tmp=y1;  y1=y2;  y2=tmp;
	}

	// linedef touches block
	return true;
}

static void LoadUDMFVertexes(parser_t *psr)
{
	char ident[128];
	char val[128];

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (y_stricmp(ident, "vertex") == 0)
		{
			float x = 0.0f, y = 0.0f;

			// process vertex block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						break; // end of lump
					}

					continue; // skip line
				}
				// process assignment
				if (y_stricmp(ident, "x") == 0)
				{
					x = str2float(val, 0.0f);
				}
				else if (y_stricmp(ident, "y") == 0)
				{
					y = str2float(val, 0.0f);
				}
			}

			vertex_t *vv = NewVertex();
			vv->x = x;
			vv->y = y;
			vv->index = num_vertices - 1;
			num_old_vert = num_vertices;
		}
	}
}

static void LoadUDMFSectors(parser_t *psr)
{
	char ident[128];
	char val[128];

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (y_stricmp(ident, "sector") == 0)
		{
			float cz = 0.0f, fz = 0.0f;
			int light = 160, type = 0, tag = 0;
			char floor_tex[10];
			char ceil_tex[10];
			strcpy(floor_tex, "-");
			strcpy(ceil_tex, "-");

			// process sector block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						break; // end of lump
					}

					continue; // skip line
				}
				// process assignment
				if (y_stricmp(ident, "heightfloor") == 0)
				{
					fz = str2float(val, 0.0f);
				}
				else if (y_stricmp(ident, "heightceiling") == 0)
				{
					cz = str2float(val, 0.0f);
				}
				else if (y_stricmp(ident, "texturefloor") == 0)
				{
					strncpy(floor_tex, val, 8);
				}
				else if (y_stricmp(ident, "textureceiling") == 0)
				{
					strncpy(ceil_tex, val, 8);
				}
				else if (y_stricmp(ident, "lightlevel") == 0)
				{
					light = str2int(val, 160);
				}
				else if (y_stricmp(ident, "special") == 0)
				{
					type = str2int(val, 0);
				}
				else if (y_stricmp(ident, "id") == 0)
				{
					tag = str2int(val, 0);
				}
			}

			sector_t *ss = NewSector();
			ss->light = light;
			ss->index = num_sectors - 1;
			ss->warned_facing = -1;
			ss->floor_h = fz;
			ss->ceil_h = cz;
			strncpy(ss->ceil_tex, ceil_tex, 8);
			strncpy(ss->floor_tex, floor_tex, 8);
			ss->special = type;
			ss->tag = tag;
			ss->coalesce = (ss->tag >= 900 && ss->tag < 1000) ? 1 : 0;
		}
	}
}

static void LoadUDMFSideDefs(parser_t *psr)
{
	char ident[128];
	char val[128];

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (y_stricmp(ident, "sidedef") == 0)
		{
			float x = 0, y = 0;
			int sec_num = 0;
			char top_tex[10];
			char bottom_tex[10];
			char middle_tex[10];
			strcpy(top_tex, "-");
			strcpy(bottom_tex, "-");
			strcpy(middle_tex, "-");

			// process sidedef block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						break; // end of lump
					}

					continue; // skip line
				}
				// process assignment
				if (y_stricmp(ident, "offsetx") == 0)
				{
					x = str2float(val, 0);
				}
				else if (y_stricmp(ident, "offsety") == 0)
				{
					y = str2float(val, 0);
				}
				else if (y_stricmp(ident, "texturetop") == 0)
				{
					strncpy(top_tex, val, 8);
				}
				else if (y_stricmp(ident, "texturebottom") == 0)
				{
					strncpy(bottom_tex, val, 8);
				}
				else if (y_stricmp(ident, "texturemiddle") == 0)
				{
					strncpy(middle_tex, val, 8);
				}
				else if (y_stricmp(ident, "sector") == 0)
				{
					sec_num = str2int(val, 0);
				}
			}
			sidedef_t *sd = NewSidedef();
			sd->index = num_sidedefs - 1;
			sd->sector = sec_num == -1 ? NULL : LookupSector(sec_num);
			if (sd->sector)
				sd->sector->is_used = 1;
			sd->x_offset = x;
			sd->y_offset = y;
			strncpy(sd->upper_tex, top_tex, 8);
			strncpy(sd->mid_tex, middle_tex, 8);
			strncpy(sd->lower_tex, bottom_tex, 8);
		}
	}
}

static void LoadUDMFLineDefs(parser_t *psr)
{
	char ident[128];
	char val[128];
	int i = 0;

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (y_stricmp(ident, "linedef") == 0)
		{
			int flags = 0, v1 = 0, v2 = 0;
			int side0 = -1, side1 = -1, tag = -1;
			int special = 0;

			// process lindef block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						break; // end of lump
					}

					continue; // skip line
				}
				// process assignment
				if (y_stricmp(ident, "id") == 0)
				{
					tag = str2int(val, -1);
				}
				else if (y_stricmp(ident, "v1") == 0)
				{
					v1 = str2int(val, 0);
				}
				else if (y_stricmp(ident, "v2") == 0)
				{
					v2 = str2int(val, 0);
				}
				else if (y_stricmp(ident, "special") == 0)
				{
					special = str2int(val, 0);
				}
				else if (y_stricmp(ident, "arg0") == 0)
				{
					tag = str2int(val, 0);
				}
				else if (y_stricmp(ident, "sidefront") == 0)
				{
					side0 = str2int(val, -1);
				}
				else if (y_stricmp(ident, "sideback") == 0)
				{
					side1 = str2int(val, -1);
				}
				else if (y_stricmp(ident, "blocking") == 0)
				{
					if (str2bool(val))
						flags |= 0x0001;
				}
				else if (y_stricmp(ident, "blockmonsters") == 0)
				{
					if (str2bool(val))
						flags |= 0x0002;
				}
				else if (y_stricmp(ident, "twosided") == 0)
				{
					if (str2bool(val))
						flags |= 0x0004;
				}
				else if (y_stricmp(ident, "dontpegtop") == 0)
				{
					if (str2bool(val))
						flags |= 0x0008;
				}
				else if (y_stricmp(ident, "dontpegbottom") == 0)
				{
					if (str2bool(val))
						flags |= 0x0010;
				}
				else if (y_stricmp(ident, "secret") == 0)
				{
					if (str2bool(val))
						flags |= 0x0020;
				}
				else if (y_stricmp(ident, "blocksound") == 0)
				{
					if (str2bool(val))
						flags |= 0x0040;
				}
				else if (y_stricmp(ident, "dontdraw") == 0)
				{
					if (str2bool(val))
						flags |= 0x0080;
				}
				else if (y_stricmp(ident, "mapped") == 0)
				{
					if (str2bool(val))
						flags |= 0x0100;
				}
				else if (y_stricmp(ident, "passuse") == 0)
				{
					if (str2bool(val))
						flags |= 0x0200; // BOOM flag
				}
			}

			linedef_t *ld = NewLinedef();
			ld->index = num_linedefs - 1;
			ld->start = LookupVertex(v1);
			ld->start->is_used = 1;
			ld->end = LookupVertex(v2);
			ld->end->is_used = 1;
			ld->zero_len = (fabs(ld->start->x - ld->end->x) < DIST_EPSILON) &&
				(fabs(ld->start->y - ld->end->y) < DIST_EPSILON);
			ld->type = special;
			ld->tag = tag;
			ld->flags = flags;
			ld->two_sided = (ld->flags & MLF_TwoSided) ? 1 : 0;
			ld->right = side0 == -1 ? NULL : SafeLookupSidedef(side0);
			ld->left = side1 == -1 ? NULL : SafeLookupSidedef(side1);
			ld->is_precious = (ld->tag >= 900 && ld->tag < 1000) ? 1 : 0;
			if (ld->right)
			{
				ld->right->is_used = 1;
				ld->right->on_special |= (ld->type > 0) ? 1 : 0;			
			}
			if (ld->left)
			{
				ld->left->is_used = 1;
				ld->left->on_special |= (ld->type > 0) ? 1 : 0;			
			}
			if (ld->right || ld->left)
				num_real_lines++;
			ld->self_ref = (ld->left && ld->right &&
				(ld->left->sector == ld->right->sector));
		}
	}
}

static void LoadUDMFThings(parser_t *psr)
{
	char ident[128];
	char val[128];

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (y_stricmp(ident, "thing") == 0)
		{
			float x = 0.0f, y = 0.0f, z = 0.0f;
			int options = MTF_Not_SP | MTF_Not_DM | MTF_Not_COOP;
			int typenum = -1;
			int tag = 0;

			// process thing block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
						break; // end of block
					if (psr->next >= psr->length)
						break; // end of lump

					continue; // skip line
				}
				// process assignment
				if (y_stricmp(ident, "id") == 0)
				{
					tag = str2int(val, 0);
				}
				else if (y_stricmp(ident, "x") == 0)
				{
					x = str2float(val, 0.0f);
				}
				else if (y_stricmp(ident, "y") == 0)
				{
					y = str2float(val, 0.0f);
				}
				else if (y_stricmp(ident, "type") == 0)
				{
					typenum = str2int(val, 0);
				}
				else if (y_stricmp(ident, "skill1") == 0)
				{
					options |= MTF_Easy;
				}
				else if (y_stricmp(ident, "skill2") == 0)
				{
					if (str2bool(val))
						options |= MTF_Easy;
				}
				else if (y_stricmp(ident, "skill3") == 0)
				{
					if (str2bool(val))
						options |= MTF_Medium;
				}
				else if (y_stricmp(ident, "skill4") == 0)
				{
					if (str2bool(val))
						options |= MTF_Hard;
				}
				else if (y_stricmp(ident, "skill5") == 0)
				{
					if (str2bool(val))
						options |= MTF_Hard;
				}
				else if (y_stricmp(ident, "ambush") == 0)
				{
					if (str2bool(val))
						options |= MTF_Ambush;
				}
				else if (y_stricmp(ident, "single") == 0)
				{
					if (str2bool(val))
						options &= ~MTF_Not_SP;
				}
				else if (y_stricmp(ident, "dm") == 0)
				{
					if (str2bool(val))
						options &= ~MTF_Not_DM;
				}
				else if (y_stricmp(ident, "coop") == 0)
				{
					if (str2bool(val))
						options &= ~MTF_Not_COOP;
				}
				// MBF flag
				else if (y_stricmp(ident, "friend") == 0)
				{
					if (str2bool(val))
						options |= MTF_Friend;
				}

				thing_t *t = NewThing();
				t->index = num_things - 1;
				t->x = x;
				t->y = y;
				t->type = typenum;
				t->options = options;
			}
		}
	}
}

/* ----- create blockmap ------------------------------------ */

#define BK_NUM    0
#define BK_MAX    1
#define BK_XOR    2
#define BK_FIRST  3

#define BK_QUANTUM  32

static void BlockAdd(int blk_num, int line_index)
{
	u16_t *cur = block_lines[blk_num];

# if DEBUG_BLOCKMAP
	DebugPrintf("Block %d has line %d\n", blk_num, line_index);
# endif

	if (blk_num < 0 || blk_num >= block_count)
		BugError(StringPrintf("BlockAdd: bad block number %d\n", blk_num));

	if (! cur)
	{
		// create empty block
		block_lines[blk_num] = cur = (u16_t *)UtilCalloc(BK_QUANTUM * sizeof(u16_t));
		cur[BK_NUM] = 0;
		cur[BK_MAX] = BK_QUANTUM;
		cur[BK_XOR] = 0x1234;
	}

	if (BK_FIRST + cur[BK_NUM] == cur[BK_MAX])
	{
		// no more room, so allocate some more...
		cur[BK_MAX] += BK_QUANTUM;

		block_lines[blk_num] = cur = (u16_t *)UtilRealloc(cur, cur[BK_MAX] * sizeof(u16_t));
	}

	// compute new checksum
	cur[BK_XOR] = ((cur[BK_XOR] << 4) | (cur[BK_XOR] >> 12)) ^ line_index;

	cur[BK_FIRST + cur[BK_NUM]] = LE_U16(line_index);
	cur[BK_NUM]++;
}


static void BlockAddLine(linedef_t *L)
{
	int x1 = (int) L->start->x;
	int y1 = (int) L->start->y;
	int x2 = (int) L->end->x;
	int y2 = (int) L->end->y;

	int bx1 = (MIN(x1,x2) - block_x) / 128;
	int by1 = (MIN(y1,y2) - block_y) / 128;
	int bx2 = (MAX(x1,x2) - block_x) / 128;
	int by2 = (MAX(y1,y2) - block_y) / 128;

	int bx, by;
	int line_index = L->index;

# if DEBUG_BLOCKMAP
	DebugPrintf("BlockAddLine: %d (%d,%d) -> (%d,%d)\n", line_index,
			x1, y1, x2, y2);
# endif

	// handle truncated blockmaps
	if (bx1 < 0) bx1 = 0;
	if (by1 < 0) by1 = 0;
	if (bx2 >= block_w) bx2 = block_w - 1;
	if (by2 >= block_h) by2 = block_h - 1;

	if (bx2 < bx1 || by2 < by1)
		return;

	// handle simple case #1: completely horizontal
	if (by1 == by2)
	{
		for (bx=bx1 ; bx <= bx2 ; bx++)
		{
			int blk_num = by1 * block_w + bx;
			BlockAdd(blk_num, line_index);
		}
		return;
	}

	// handle simple case #2: completely vertical
	if (bx1 == bx2)
	{
		for (by=by1 ; by <= by2 ; by++)
		{
			int blk_num = by * block_w + bx1;
			BlockAdd(blk_num, line_index);
		}
		return;
	}

	// handle the rest (diagonals)

	for (by=by1 ; by <= by2 ; by++)
	for (bx=bx1 ; bx <= bx2 ; bx++)
	{
		int blk_num = by * block_w + bx;

		int minx = block_x + bx * 128;
		int miny = block_y + by * 128;
		int maxx = minx + 127;
		int maxy = miny + 127;

		if (CheckLinedefInsideBox(minx, miny, maxx, maxy, x1, y1, x2, y2))
		{
			BlockAdd(blk_num, line_index);
		}
	}
}


static void CreateBlockmap(void)
{
	int i;

	block_lines = (u16_t **) UtilCalloc(block_count * sizeof(u16_t *));

	for (i=0 ; i < num_linedefs ; i++)
	{
		linedef_t *L = LookupLinedef(i);

		// ignore zero-length lines
		if (L->zero_len)
			continue;

		BlockAddLine(L);
	}
}


static int BlockCompare(const void *p1, const void *p2)
{
	int blk_num1 = ((const u16_t *) p1)[0];
	int blk_num2 = ((const u16_t *) p2)[0];

	const u16_t *A = block_lines[blk_num1];
	const u16_t *B = block_lines[blk_num2];

	if (A == B)
		return 0;

	if (A == NULL) return -1;
	if (B == NULL) return +1;

	if (A[BK_NUM] != B[BK_NUM])
	{
		return A[BK_NUM] - B[BK_NUM];
	}

	if (A[BK_XOR] != B[BK_XOR])
	{
		return A[BK_XOR] - B[BK_XOR];
	}

	return memcmp(A+BK_FIRST, B+BK_FIRST, A[BK_NUM] * sizeof(u16_t));
}

static void FindBlockmapLimits(bbox_t *bbox)
{
	int i;

	int mid_x = 0;
	int mid_y = 0;

	bbox->minx = bbox->miny = SHRT_MAX;
	bbox->maxx = bbox->maxy = SHRT_MIN;

	for (i=0 ; i < num_linedefs ; i++)
	{
		linedef_t *L = LookupLinedef(i);

		if (! L->zero_len)
		{
			double x1 = L->start->x;
			double y1 = L->start->y;
			double x2 = L->end->x;
			double y2 = L->end->y;

			int lx = (int)floor(MIN(x1, x2));
			int ly = (int)floor(MIN(y1, y2));
			int hx = (int)ceil(MAX(x1, x2));
			int hy = (int)ceil(MAX(y1, y2));

			if (lx < bbox->minx) bbox->minx = lx;
			if (ly < bbox->miny) bbox->miny = ly;
			if (hx > bbox->maxx) bbox->maxx = hx;
			if (hy > bbox->maxy) bbox->maxy = hy;

			// compute middle of cluster (roughly, so we don't overflow)
			mid_x += (lx + hx) / 32;
			mid_y += (ly + hy) / 32;
		}
	}

	if (num_linedefs > 0)
	{
		block_mid_x = (mid_x / num_linedefs) * 16;
		block_mid_y = (mid_y / num_linedefs) * 16;
	}

# if DEBUG_BLOCKMAP
	DebugPrintf("Blockmap lines centered at (%d,%d)\n", block_mid_x, block_mid_y);
# endif
}


void InitBlockmap()
{
	bbox_t map_bbox;

	// find limits of linedefs, and store as map limits
	FindBlockmapLimits(&map_bbox);

	block_x = map_bbox.minx - (map_bbox.minx & 0x7);
	block_y = map_bbox.miny - (map_bbox.miny & 0x7);

	block_w = ((map_bbox.maxx - block_x) / 128) + 1;
	block_h = ((map_bbox.maxy - block_y) / 128) + 1;

	block_count = block_w * block_h;
}

//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
//------------------------------------------------------------------------


// Note: ZDoom format support based on code (C) 2002,2003 Randy Heit


#define DEBUG_LOAD      0
#define DEBUG_BSP       0

#define ALLOC_BLKNUM  1024


// per-level variables

const char *lev_current_name;

short lev_current_idx;
short lev_current_start;

bool lev_doing_hexen;

bool lev_force_v5;

bool lev_long_name;

#define LEVELARRAY(TYPE, BASEVAR, NUMVAR)  \
	TYPE ** BASEVAR = NULL;  \
	int NUMVAR = 0;


LEVELARRAY(vertex_t,  lev_vertices,   num_vertices)
LEVELARRAY(linedef_t, lev_linedefs,   num_linedefs)
LEVELARRAY(sidedef_t, lev_sidedefs,   num_sidedefs)
LEVELARRAY(sector_t,  lev_sectors,    num_sectors)
LEVELARRAY(thing_t,   lev_things,     num_things)

static LEVELARRAY(seg_t,     segs,       num_segs)
static LEVELARRAY(subsec_t,  subsecs,    num_subsecs)
static LEVELARRAY(node_t,    nodes,      num_nodes)
static LEVELARRAY(wall_tip_t,wall_tips,  num_wall_tips)


int num_old_vert = 0;
int num_new_vert = 0;
int num_complete_seg = 0;
int num_real_lines = 0;


/* ----- allocation routines ---------------------------- */

#define ALLIGATOR(TYPE, BASEVAR, NUMVAR)  \
{  \
	if ((NUMVAR % ALLOC_BLKNUM) == 0)  \
	{  \
		BASEVAR = (TYPE **) UtilRealloc(BASEVAR, (NUMVAR + ALLOC_BLKNUM) * sizeof(TYPE *));  \
	}  \
	BASEVAR[NUMVAR] = (TYPE *) UtilCalloc(sizeof(TYPE));  \
	NUMVAR += 1;  \
	return BASEVAR[NUMVAR - 1];  \
}


vertex_t *NewVertex(void)
	ALLIGATOR(vertex_t, lev_vertices, num_vertices)

linedef_t *NewLinedef(void)
	ALLIGATOR(linedef_t, lev_linedefs, num_linedefs)

sidedef_t *NewSidedef(void)
	ALLIGATOR(sidedef_t, lev_sidedefs, num_sidedefs)

sector_t *NewSector(void)
	ALLIGATOR(sector_t, lev_sectors, num_sectors)

thing_t *NewThing(void)
	ALLIGATOR(thing_t, lev_things, num_things)

seg_t *NewSeg(void)
	ALLIGATOR(seg_t, segs, num_segs)

subsec_t *NewSubsec(void)
	ALLIGATOR(subsec_t, subsecs, num_subsecs)

node_t *NewNode(void)
	ALLIGATOR(node_t, nodes, num_nodes)

wall_tip_t *NewWallTip(void)
	ALLIGATOR(wall_tip_t, wall_tips, num_wall_tips)


/* ----- free routines ---------------------------- */

#define FREEMASON(TYPE, BASEVAR, NUMVAR)  \
{  \
	int i;  \
	for (i=0 ; i < NUMVAR ; i++)  \
		UtilFree(BASEVAR[i]);  \
	if (BASEVAR)  \
		UtilFree(BASEVAR);  \
	BASEVAR = NULL; NUMVAR = 0;  \
}


void FreeVertices(void)
	FREEMASON(vertex_t, lev_vertices, num_vertices)

void FreeLinedefs(void)
	FREEMASON(linedef_t, lev_linedefs, num_linedefs)

void FreeSidedefs(void)
	FREEMASON(sidedef_t, lev_sidedefs, num_sidedefs)

void FreeSectors(void)
	FREEMASON(sector_t, lev_sectors, num_sectors)

void FreeThings(void)
	FREEMASON(thing_t, lev_things, num_things)

void FreeSegs(void)
	FREEMASON(seg_t, segs, num_segs)

void FreeSubsecs(void)
	FREEMASON(subsec_t, subsecs, num_subsecs)

void FreeNodes(void)
	FREEMASON(node_t, nodes, num_nodes)

void FreeWallTips(void)
	FREEMASON(wall_tip_t, wall_tips, num_wall_tips)


/* ----- lookup routines ------------------------------ */

#define LOOKERUPPER(BASEVAR, NUMVAR, NAMESTR)  \
{  \
	if (index < 0 || index >= NUMVAR)  \
		BugError(StringPrintf("No such %s number #%d\n", NAMESTR, index));  \
	return BASEVAR[index];  \
}

vertex_t *LookupVertex(int index)
	LOOKERUPPER(lev_vertices, num_vertices, "vertex")

linedef_t *LookupLinedef(int index)
	LOOKERUPPER(lev_linedefs, num_linedefs, "linedef")

sidedef_t *LookupSidedef(int index)
	LOOKERUPPER(lev_sidedefs, num_sidedefs, "sidedef")

sector_t *LookupSector(int index)
	LOOKERUPPER(lev_sectors, num_sectors, "sector")

thing_t *LookupThing(int index)
	LOOKERUPPER(lev_things, num_things, "thing")

seg_t *LookupSeg(int index)
	LOOKERUPPER(segs, num_segs, "seg")

subsec_t *LookupSubsec(int index)
	LOOKERUPPER(subsecs, num_subsecs, "subsector")

node_t *LookupNode(int index)
	LOOKERUPPER(nodes, num_nodes, "node")


/* ----- reading routines ------------------------------ */


void GetVertices(void)
{
	int i, count=-1;

	Lump_c *lump = FindLevelLump("VERTEXES");

	if (lump)
		count = lump->Length() / sizeof(raw_vertex_t);

# if DEBUG_LOAD
	DebugPrintf("GetVertices: num = %d\n", count);
# endif

	if (!lump || count == 0)
		return;

	if (! lump->Seek())
		FatalError(StringPrintf("Error seeking to vertices.\n"));

	for (i = 0 ; i < count ; i++)
	{
		raw_vertex_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			FatalError(StringPrintf("Error reading vertices.\n"));

		vertex_t *vert = NewVertex();

		vert->x = (double) LE_S16(raw.x);
		vert->y = (double) LE_S16(raw.y);

		vert->index = i;
	}

	num_old_vert = num_vertices;
}


void GetSectors(void)
{
	int i, count=-1;

	Lump_c *lump = FindLevelLump("SECTORS");

	if (lump)
		count = lump->Length() / sizeof(raw_sector_t);

	if (!lump || count == 0)
		return;

	if (! lump->Seek())
		FatalError(StringPrintf("Error seeking to sectors.\n"));

# if DEBUG_LOAD
	DebugPrintf("GetSectors: num = %d\n", count);
# endif

	for (i = 0 ; i < count ; i++)
	{
		raw_sector_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			FatalError(StringPrintf("Error reading sectors.\n"));

		sector_t *sector = NewSector();

		sector->floor_h = LE_S16(raw.floorh);
		sector->ceil_h  = LE_S16(raw.ceilh);

		memcpy(sector->floor_tex, raw.floor_tex, sizeof(sector->floor_tex));
		memcpy(sector->ceil_tex,  raw.ceil_tex,  sizeof(sector->ceil_tex));

		sector->light = LE_U16(raw.light);
		sector->special = LE_U16(raw.type);
		sector->tag = LE_S16(raw.tag);

		sector->coalesce = (sector->tag >= 900 && sector->tag < 1000) ? 1 : 0;

		// sector indices never change
		sector->index = i;

		sector->warned_facing = -1;

		// Note: rej_* fields are handled completely in reject.c
	}
}


void GetThings(void)
{
	int i, count=-1;

	Lump_c *lump = FindLevelLump("THINGS");

	if (lump)
		count = lump->Length() / sizeof(raw_thing_t);

	if (!lump || count == 0)
		return;

	if (! lump->Seek())
		FatalError(StringPrintf("Error seeking to things.\n"));

# if DEBUG_LOAD
	DebugPrintf("GetThings: num = %d\n", count);
# endif

	for (i = 0 ; i < count ; i++)
	{
		raw_thing_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			FatalError(StringPrintf("Error reading things.\n"));

		thing_t *thing = NewThing();

		thing->x = LE_S16(raw.x);
		thing->y = LE_S16(raw.y);

		thing->type = LE_U16(raw.type);
		thing->options = LE_U16(raw.options);

		thing->index = i;
	}
}


void GetThingsHexen(void)
{
	int i, count=-1;

	Lump_c *lump = FindLevelLump("THINGS");

	if (lump)
		count = lump->Length() / sizeof(raw_hexen_thing_t);

	if (!lump || count == 0)
		return;

	if (! lump->Seek())
		FatalError(StringPrintf("Error seeking to things.\n"));

# if DEBUG_LOAD
	DebugPrintf("GetThingsHexen: num = %d\n", count);
# endif

	for (i = 0 ; i < count ; i++)
	{
		raw_hexen_thing_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			FatalError(StringPrintf("Error reading things.\n"));

		thing_t *thing = NewThing();

		thing->x = LE_S16(raw.x);
		thing->y = LE_S16(raw.y);

		thing->type = LE_U16(raw.type);
		thing->options = LE_U16(raw.options);

		thing->index = i;
	}
}


void GetSidedefs(void)
{
	int i, count=-1;

	Lump_c *lump = FindLevelLump("SIDEDEFS");

	if (lump)
		count = lump->Length() / sizeof(raw_sidedef_t);

	if (!lump || count == 0)
		return;

	if (! lump->Seek())
		FatalError(StringPrintf("Error seeking to sidedefs.\n"));

# if DEBUG_LOAD
	DebugPrintf("GetSidedefs: num = %d\n", count);
# endif

	for (i = 0 ; i < count ; i++)
	{
		raw_sidedef_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			FatalError(StringPrintf("Error reading sidedefs.\n"));

		sidedef_t *side = NewSidedef();

		side->sector = (LE_S16(raw.sector) == -1) ? NULL :
			LookupSector(LE_U16(raw.sector));

		if (side->sector)
			side->sector->is_used = 1;

		side->x_offset = LE_S16(raw.x_offset);
		side->y_offset = LE_S16(raw.y_offset);

		memcpy(side->upper_tex, raw.upper_tex, sizeof(side->upper_tex));
		memcpy(side->lower_tex, raw.lower_tex, sizeof(side->lower_tex));
		memcpy(side->mid_tex,   raw.mid_tex,   sizeof(side->mid_tex));

		// sidedef indices never change
		side->index = i;
	}
}

sidedef_t *SafeLookupSidedef(u16_t num)
{
	if (num == 0xFFFF)
		return NULL;

	if ((int)num >= num_sidedefs && (s16_t)(num) < 0)
		return NULL;

	return LookupSidedef(num);
}


void GetLinedefs(void)
{
	int i, count=-1;

	Lump_c *lump = FindLevelLump("LINEDEFS");

	if (lump)
		count = lump->Length() / sizeof(raw_linedef_t);

	if (!lump || count == 0)
		return;

	if (! lump->Seek())
		FatalError(StringPrintf("Error seeking to linedefs.\n"));

# if DEBUG_LOAD
	DebugPrintf("GetLinedefs: num = %d\n", count);
# endif

	for (i = 0 ; i < count ; i++)
	{
		raw_linedef_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			FatalError(StringPrintf("Error reading linedefs.\n"));

		linedef_t *line;

		vertex_t *start = LookupVertex(LE_U16(raw.start));
		vertex_t *end   = LookupVertex(LE_U16(raw.end));

		start->is_used = 1;
		  end->is_used = 1;

		line = NewLinedef();

		line->start = start;
		line->end   = end;

		// check for zero-length line
		line->zero_len = (fabs(start->x - end->x) < DIST_EPSILON) &&
			(fabs(start->y - end->y) < DIST_EPSILON);

		line->flags = LE_U16(raw.flags);
		line->type = LE_U16(raw.type);
		line->tag  = LE_S16(raw.tag);

		line->two_sided = (line->flags & MLF_TwoSided) ? 1 : 0;
		line->is_precious = (line->tag >= 900 && line->tag < 1000) ? 1 : 0;

		line->right = SafeLookupSidedef(LE_U16(raw.right));
		line->left  = SafeLookupSidedef(LE_U16(raw.left));

		if (line->right)
		{
			line->right->is_used = 1;
			line->right->on_special |= (line->type > 0) ? 1 : 0;
		}

		if (line->left)
		{
			line->left->is_used = 1;
			line->left->on_special |= (line->type > 0) ? 1 : 0;
		}

		if (line->right || line->left)
			num_real_lines++;

		line->self_ref = (line->left && line->right &&
				(line->left->sector == line->right->sector));

		line->index = i;
	}
}


void GetLinedefsHexen(void)
{
	int i, j, count=-1;

	Lump_c *lump = FindLevelLump("LINEDEFS");

	if (lump)
		count = lump->Length() / sizeof(raw_hexen_linedef_t);

	if (!lump || count == 0)
		return;

	if (! lump->Seek())
		FatalError(StringPrintf("Error seeking to linedefs.\n"));

# if DEBUG_LOAD
	DebugPrintf("GetLinedefsHexen: num = %d\n", count);
# endif

	for (i = 0 ; i < count ; i++)
	{
		raw_hexen_linedef_t raw;

		if (! lump->Read(&raw, sizeof(raw)))
			FatalError(StringPrintf("Error reading linedefs.\n"));

		linedef_t *line;

		vertex_t *start = LookupVertex(LE_U16(raw.start));
		vertex_t *end   = LookupVertex(LE_U16(raw.end));

		start->is_used = 1;
		  end->is_used = 1;

		line = NewLinedef();

		line->start = start;
		line->end   = end;

		// check for zero-length line
		line->zero_len = (fabs(start->x - end->x) < DIST_EPSILON) &&
			(fabs(start->y - end->y) < DIST_EPSILON);

		line->flags = LE_U16(raw.flags);
		line->type = (u8_t)(raw.type);
		line->tag  = 0;

		// read specials
		for (j=0 ; j < 5 ; j++)
			line->specials[j] = (u8_t)(raw.args[j]);

		// -JL- Added missing twosided flag handling that caused a broken reject
		line->two_sided = (line->flags & MLF_TwoSided) ? 1 : 0;

		line->right = SafeLookupSidedef(LE_U16(raw.right));
		line->left  = SafeLookupSidedef(LE_U16(raw.left));

		// -JL- Added missing sidedef handling that caused all sidedefs to be pruned
		if (line->right)
		{
			line->right->is_used = 1;
			line->right->on_special |= (line->type > 0) ? 1 : 0;
		}

		if (line->left)
		{
			line->left->is_used = 1;
			line->left->on_special |= (line->type > 0) ? 1 : 0;
		}

		if (line->right || line->left)
			num_real_lines++;

		line->self_ref = (line->left && line->right &&
				(line->left->sector == line->right->sector));

		line->index = i;
	}
}

static int SegCompare(const void *p1, const void *p2)
{
	const seg_t *A = ((const seg_t **) p1)[0];
	const seg_t *B = ((const seg_t **) p2)[0];

	if (A->index < 0)
		BugError(StringPrintf("Seg %p never reached a subsector !\n", A));

	if (B->index < 0)
		BugError(StringPrintf("Seg %p never reached a subsector !\n", B));

	return (A->index - B->index);
}

/* ----- writing routines ------------------------------ */

static inline u32_t VertexIndex_XNOD(const vertex_t *v)
{
	if (v->is_new)
		return (u32_t) (num_old_vert + v->index);

	return (u32_t) v->index;
}

static int node_cur_index;

void CheckLimits()
{
	if (num_sectors > 65534)
	{
		FatalError(StringPrintf("AJBSP: %s in file %s has too many sectors! (%d)", lev_current_name, FindBaseName(edit_wad->PathName()), num_sectors));
	}

	if (num_sidedefs > 65534)
	{
		FatalError(StringPrintf("AJBSP: %s in file %s has too many sidedefs! (%d)", lev_current_name, FindBaseName(edit_wad->PathName()), num_sidedefs));
	}

	if (num_linedefs > 65535)
	{
		FatalError(StringPrintf("AJBSP: %s in file %s has too many linedefs (%d)", lev_current_name, FindBaseName(edit_wad->PathName()), num_linedefs));
	}
}

void SortSegs()
{
	// sort segs into ascending index
	qsort(segs, num_segs, sizeof(seg_t *), SegCompare);
}

/* ----- ZDoom format writing --------------------------- */

static const u8_t *lev_XGL3_magic = (u8_t *) "XGL3";

void PutXGL3Vertices(void)
{
	int count, i;

	u32_t orgverts = LE_U32(num_old_vert);
	u32_t newverts = LE_U32(num_new_vert);

	XGL3AppendLump(&orgverts, 4);
	XGL3AppendLump(&newverts, 4);

	for (i=0, count=0 ; i < num_vertices ; i++)
	{
		raw_v2_vertex_t raw;

		vertex_t *vert = lev_vertices[i];

		if (! vert->is_new)
			continue;

		raw.x = LE_S32(I_ROUND(vert->x * 65536.0));
		raw.y = LE_S32(I_ROUND(vert->y * 65536.0));

		XGL3AppendLump(&raw, sizeof(raw));

		count++;
	}

	if (count != num_new_vert)
		BugError(StringPrintf("PutXGL3Vertices miscounted (%d != %d)\n",
				count, num_new_vert));
}


void PutXGL3Subsecs(void)
{
	int i;
	int count;
	u32_t raw_num = LE_U32(num_subsecs);

	int cur_seg_index = 0;

	XGL3AppendLump(&raw_num, 4);

	for (i=0 ; i < num_subsecs ; i++)
	{
		subsec_t *sub = subsecs[i];
		seg_t *seg;

		raw_num = LE_U32(sub->seg_count);

		XGL3AppendLump(&raw_num, 4);

		// sanity check the seg index values
		count = 0;
		for (seg = sub->seg_list ; seg ; seg = seg->next, cur_seg_index++)
		{
			if (cur_seg_index != seg->index)
				BugError(StringPrintf("PutXGL3Subsecs: seg index mismatch in sub %d (%d != %d)\n",
						i, cur_seg_index, seg->index));

			count++;
		}

		if (count != sub->seg_count)
			BugError(StringPrintf("PutXGL3Subsecs: miscounted segs in sub %d (%d != %d)\n",
					i, count, sub->seg_count));
	}

	if (cur_seg_index != num_complete_seg)
		BugError(StringPrintf("PutXGL3Subsecs miscounted segs (%d != %d)\n",
				cur_seg_index, num_complete_seg));
}

void PutXGL3Segs()
{
	int i, count;
	u32_t raw_num = LE_U32(num_segs);

	XGL3AppendLump(&raw_num, 4);

	for (i=0, count=0 ; i < num_segs ; i++)
	{
		seg_t *seg = segs[i];

		if (count != seg->index)
			BugError(StringPrintf("PutXGL3Segs: seg index mismatch (%d != %d)\n",
					count, seg->index));

		{
			u32_t v1   = LE_U32(VertexIndex_XNOD(seg->start));
			u32_t partner = LE_U32(seg->partner ? seg->partner->index : -1);
			u32_t line = LE_U32(seg->linedef ? seg->linedef->index : -1);
			u8_t  side = ((seg->linedef && seg->linedef->two_sided) ? seg->side : 0);

# if DEBUG_BSP
			fprintf(stderr, "SEG[%d] v1=%d partner=%d line=%d side=%d\n", i, v1, partner, line, side);
# endif
			XGL3AppendLump(&v1,      4);
			XGL3AppendLump(&partner, 4);
			XGL3AppendLump(&line,    4);
			XGL3AppendLump(&side,    1);
		}

		count++;
	}

	if (count != num_segs)
		BugError(StringPrintf("PutXGL3Segs miscounted (%d != %d)\n", count, num_segs));
}

static void PutOneXGL3Node(node_t *node)
{
	raw_v5_node_t raw;

	if (node->r.node)
		PutOneXGL3Node(node->r.node);

	if (node->l.node)
		PutOneXGL3Node(node->l.node);

	node->index = node_cur_index++;

	u32_t x  = LE_S32(I_ROUND(node->x  * 65536.0));
	u32_t y  = LE_S32(I_ROUND(node->y  * 65536.0));
	u32_t dx = LE_S32(I_ROUND(node->dx * 65536.0));
	u32_t dy = LE_S32(I_ROUND(node->dy * 65536.0));

	XGL3AppendLump(&x,  4);
	XGL3AppendLump(&y,  4);
	XGL3AppendLump(&dx, 4);
	XGL3AppendLump(&dy, 4);

	raw.b1.minx = LE_S16(node->r.bounds.minx);
	raw.b1.miny = LE_S16(node->r.bounds.miny);
	raw.b1.maxx = LE_S16(node->r.bounds.maxx);
	raw.b1.maxy = LE_S16(node->r.bounds.maxy);

	raw.b2.minx = LE_S16(node->l.bounds.minx);
	raw.b2.miny = LE_S16(node->l.bounds.miny);
	raw.b2.maxx = LE_S16(node->l.bounds.maxx);
	raw.b2.maxy = LE_S16(node->l.bounds.maxy);

	XGL3AppendLump(&raw.b1, sizeof(raw.b1));
	XGL3AppendLump(&raw.b2, sizeof(raw.b2));

	if (node->r.node)
		raw.right = LE_U32(node->r.node->index);
	else if (node->r.subsec)
		raw.right = LE_U32(node->r.subsec->index | 0x80000000U);
	else
		BugError(StringPrintf("Bad right child in node %d\n", node->index));

	if (node->l.node)
		raw.left = LE_U32(node->l.node->index);
	else if (node->l.subsec)
		raw.left = LE_U32(node->l.subsec->index | 0x80000000U);
	else
		BugError(StringPrintf("Bad left child in node %d\n", node->index));

	XGL3AppendLump(&raw.right, 4);
	XGL3AppendLump(&raw.left,  4);

# if DEBUG_BSP
	DebugPrintf("PUT Z NODE %08X  Left %08X  Right %08X  "
			"(%d,%d) -> (%d,%d)\n", node->index, LE_U32(raw.left),
			LE_U32(raw.right), node->x, node->y,
			node->x + node->dx, node->y + node->dy);
# endif
}


void PutXGL3Nodes(node_t *root)
{
	u32_t raw_num = LE_U32(num_nodes);

	XGL3AppendLump(&raw_num, 4);

	node_cur_index = 0;

	if (root)
		PutOneXGL3Node(root);

	if (node_cur_index != num_nodes)
		BugError(StringPrintf("PutXGL3Nodes miscounted (%d != %d)\n",
				node_cur_index, num_nodes));
}

void SaveXGL3Format(node_t *root_node)
{
	// WISH : compute a max_size

	Lump_c *lump = CreateLevelLump("XGLNODES", -1);

	lump->Write(lev_XGL3_magic, 4);

	XGL3BeginLump(lump);

	PutXGL3Vertices();
	PutXGL3Subsecs();
	PutXGL3Segs();
	PutXGL3Nodes(root_node);

	XGL3FinishLump();
}

/* ----- whole-level routines --------------------------- */

void PruneVerticesAtEnd(void)
{
	// scan all vertices.
	// only remove from the end, so stop when hit a used one.

	for (int i = num_vertices - 1 ; i >= 0 ; i--)
	{
		vertex_t *V = lev_vertices[i];

		if (V->is_used)
			break;

		UtilFree(V);

		num_vertices -= 1;
	}
}

void LoadLevel()
{
	Lump_c *LEV = edit_wad->GetLump(lev_current_start);

	lev_current_name = LEV->Name();

	// -JL- Identify Hexen mode by presence of BEHAVIOR lump
	lev_doing_hexen = (FindLevelLump("BEHAVIOR") != NULL);

	UpdateProgress(StringPrintf("Building nodes for %s...\n", lev_current_name));

	PrintMsg(StringPrintf("Building nodes for %s...\n", lev_current_name));

	num_new_vert = 0;
	num_complete_seg = 0;
	num_real_lines = 0;

	if (Level_format == MAPF_UDMF)
	{
		Lump_c *lump = FindLevelLump("TEXTMAP");
		int udmf_lump_len = W_LoadLumpData(lump, &udmf_lump);
		// initialize the parser
		udmf_psr.buffer = (uint8_t *)udmf_lump;
		udmf_psr.length = udmf_lump_len;
		udmf_psr.next = 0; // start at first line
		LoadUDMFVertexes(&udmf_psr);
		LoadUDMFSectors(&udmf_psr);
		LoadUDMFSideDefs(&udmf_psr);
		LoadUDMFLineDefs(&udmf_psr);
		LoadUDMFThings(&udmf_psr);
	}
	else
	{
		GetVertices();
		GetSectors();
		GetSidedefs();

		if (lev_doing_hexen)
		{
			GetLinedefsHexen();
			GetThingsHexen();
		}
		else
		{
			GetLinedefs();
			GetThings();
		}
	}

	PrintDetail(StringPrintf("%s: Level Loaded...\n", lev_current_name));

	PruneVerticesAtEnd();

	DetectOverlappingVertices();
	DetectOverlappingLines();

	CalculateWallTips();

	if (lev_doing_hexen)
	{
		// -JL- Find sectors containing polyobjs
		DetectPolyobjSectors();
	}
}

void FreeLevel(void)
{
	FreeVertices();
	FreeSidedefs();
	FreeLinedefs();
	FreeSectors();
	FreeThings();
	FreeSegs();
	FreeSubsecs();
	FreeNodes();
	FreeWallTips();
}


static u32_t CalcGLChecksum(void)
{
	u32_t crc;

	Adler32_Begin(&crc);

	Lump_c *lump = FindLevelLump("VERTEXES");

	if (lump && lump->Length() > 0)
	{
		u8_t *data = new u8_t[lump->Length()];

		if (! lump->Seek() ||
		    ! lump->Read(data, lump->Length()))
			FatalError(StringPrintf("Error reading vertices (for checksum).\n"));

		Adler32_AddBlock(&crc, data, lump->Length());
		delete[] data;
	}

	lump = FindLevelLump("LINEDEFS");

	if (lump && lump->Length() > 0)
	{
		u8_t *data = new u8_t[lump->Length()];

		if (! lump->Seek() ||
		    ! lump->Read(data, lump->Length()))
			FatalError(StringPrintf("Error reading linedefs (for checksum).\n"));

		Adler32_AddBlock(&crc, data, lump->Length());
		delete[] data;
	}

	Adler32_Finish(&crc);

	return crc;
}

static const char *CalcOptionsString()
{
	static char buffer[256];

	sprintf(buffer, "--cost %d", cur_info->factor);

	if (cur_info->fast)
		strcat(buffer, " --fast");

	return buffer;
}


void UpdateGLMarker(Lump_c *marker)
{
	// this is very conservative, around 4 times the actual size
	const int max_size = 512;

	// we *must* compute the checksum BEFORE (re)creating the lump
	// [ otherwise we write data into the wrong part of the file ]
	u32_t crc = CalcGLChecksum();

	gwa_wad->RecreateLump(marker, max_size);

	if (lev_long_name)
	{
		marker->Printf("LEVEL=%s\n", lev_current_name);
	}

	marker->Printf("BUILDER=%s\n", "AJBSP " AJBSP_VERSION);

	marker->Printf("OPTIONS=%s\n", CalcOptionsString());

	marker->Printf("CHECKSUM=0x%08x\n", crc);

	marker->Finish();
}


static void AddMissingLump(const char *name, const char *after)
{
	if (edit_wad->LevelLookupLump(lev_current_idx, name) >= 0)
		return;

	short exist = edit_wad->LevelLookupLump(lev_current_idx, after);

	// if this happens, the level structure is very broken
	if (exist < 0)
	{
		Warning("Lump missing -- level structure is broken\n");

		exist = edit_wad->LevelLastLump(lev_current_idx);
	}

	edit_wad->InsertPoint(exist + 1);

	edit_wad->AddLump(name)->Finish();
}

build_result_e SaveLevel(node_t *root_node)
{
	if (Level_format != MAPF_UDMF)
		CheckLimits();

	gwa_wad->BeginWrite();

	Lump_c * gl_marker = NULL;

	if (num_real_lines > 0)
	{
		gl_marker = CreateGLMarker();

		SortSegs();

		SaveXGL3Format(root_node);
	}

	if (gl_marker)
	{
		UpdateGLMarker(gl_marker);
	}

	gwa_wad->EndWrite();

	return BUILD_OK;
}

//----------------------------------------------------------------------

static Lump_c  *xgl3_lump;

void XGL3BeginLump(Lump_c *lump)
{
	xgl3_lump = lump;
}

void XGL3AppendLump(const void *data, int length)
{
	xgl3_lump->Write(data, length);
	return;
}

void XGL3FinishLump(void)
{
	xgl3_lump = NULL;
	return;
}


/* ---------------------------------------------------------------- */

Lump_c * FindLevelLump(const char *name)
{
	short idx = edit_wad->LevelLookupLump(lev_current_idx, name);

	if (idx < 0)
		return NULL;

	return edit_wad->GetLump(idx);
}


Lump_c * CreateLevelLump(const char *name, int max_size)
{
	return gwa_wad->AddLump(name, max_size);
}


Lump_c * CreateGLMarker()
{
	char name_buf[64];

	if (strlen(lev_current_name) <= 5)
	{
		sprintf(name_buf, "XG_%s", lev_current_name);

		lev_long_name = false;
	}
	else
	{
		// support for level names longer than 5 letters
		strcpy(name_buf, "XG_LEVEL");

		lev_long_name = true;
	}

	Lump_c *marker = gwa_wad->AddLump(name_buf);

	marker->Finish();

	return marker;
}


//------------------------------------------------------------------------
// MAIN STUFF
//------------------------------------------------------------------------


nodebuildinfo_t * cur_info = NULL;


/* ----- build nodes for a single level ----- */

build_result_e BuildNodesForLevel(nodebuildinfo_t *info, short lev_idx)
{
	cur_info = info;

	node_t *root_node  = NULL;
	subsec_t *root_sub = NULL;

	build_result_e ret = BUILD_OK;

	if (cur_info->cancelled)
		return BUILD_Cancelled;

	Level_format = edit_wad->LevelFormat(lev_idx);

	lev_current_idx   = lev_idx;
	lev_current_start = edit_wad->LevelHeader(lev_idx);

	LoadLevel();

	InitBlockmap();

	if (num_real_lines > 0)
	{
		bbox_t seg_bbox;

		// create initial segs
		superblock_t * seg_list = CreateSegs();

		FindLimits(seg_list, &seg_bbox);

		// recursively create nodes
		ret = BuildNodes(seg_list, &root_node, &root_sub, 0, &seg_bbox);

		FreeSuper(seg_list);
	}

	if (ret == BUILD_OK)
	{
		ClockwiseBspTree();

		SaveLevel(root_node);
	}
	else
	{
		/* build was Cancelled by the user */
	}

	FreeLevel();
	FreeQuickAllocCuts();
	FreeQuickAllocSupers();

	return ret;
}

}  // namespace ajbsp


build_result_e AJBSP_BuildLevel(nodebuildinfo_t *info, short lev_idx)
{
	return ajbsp::BuildNodesForLevel(info, lev_idx);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
