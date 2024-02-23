//----------------------------------------------------------------------------
//  EDGE Colour Code
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------


#include "i_defs_gl.h"
#include "i_system.h"

#include "main.h"
#include "colormap.h"
#include "game.h"

#include "r_colormap.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_main.h"
#include "g_game.h" // current_map
#include "e_player.h"
#include "m_argv.h"
#include "r_misc.h"
#include "r_gldefs.h"
#include "r_modes.h"
#include "r_image.h"
#include "r_shader.h"
#include "r_texgl.h"
#include "r_units.h"
#include "w_files.h"
#include "w_wad.h"

#include "str_util.h"

extern ConsoleVariable r_forceflatlighting;

// -AJA- 1999/06/30: added this
uint8_t playpal_data[14][256][3];

// -AJA- 1999/09/18: fixes problem with black text etc.
static bool loaded_playpal = false;

// -AJA- 1999/07/03: moved these here from st_stuff.c:
// Palette indices.
// For damage/bonus red-/gold-shifts
#define PAIN_PALS      1
#define BONUS_PALS     9
#define NUM_PAIN_PALS  8
#define NUM_BONUS_PALS 4
// Radiation suit, green shift.
#define RADIATION_PAL 13

EDGE_DEFINE_CONSOLE_VARIABLE(v_secbright, "5", kConsoleVariableFlagArchive)

// colour indices from palette
int pal_black, pal_white, pal_gray239;
int pal_red, pal_green, pal_blue;
int pal_yellow, pal_green1, pal_brown1;

static int V_FindPureColour(int which);

void V_InitPalette(void)
{
    int t, i;

    int         pal_length = 0;
    const uint8_t *pal        = (const uint8_t *)W_OpenPackOrLumpInMemory("PLAYPAL", {".pal"}, &pal_length);

    if (!pal)
        FatalError("V_InitPalette: Error opening PLAYPAL!\n");

    // read in palette colours
    for (t = 0; t < 14; t++)
    {
        for (i = 0; i < 256; i++)
        {
            playpal_data[t][i][0] = pal[(t * 256 + i) * 3 + 0];
            playpal_data[t][i][1] = pal[(t * 256 + i) * 3 + 1];
            playpal_data[t][i][2] = pal[(t * 256 + i) * 3 + 2];
        }
    }

    delete[] pal;
    loaded_playpal = true;

    // lookup useful colours
    pal_black   = V_FindColour(0, 0, 0);
    pal_white   = V_FindColour(255, 255, 255);
    pal_gray239 = V_FindColour(239, 239, 239);

    pal_red   = V_FindPureColour(0);
    pal_green = V_FindPureColour(1);
    pal_blue  = V_FindPureColour(2);

    pal_yellow = V_FindColour(255, 255, 0);
    pal_green1 = V_FindColour(64, 128, 48);
    pal_brown1 = V_FindColour(192, 128, 74);

    LogPrint("Loaded global palette.\n");

    LogDebug("Black:%d White:%d Red:%d Green:%d Blue:%d\n", pal_black, pal_white, pal_red, pal_green, pal_blue);
}

static int cur_palette = -1;

void V_InitColour(void)
{
}

//
// Find the closest matching colour in the palette.
//
int V_FindColour(int r, int g, int b)
{
    int i;

    int best      = 0;
    int best_dist = 1 << 30;

    for (i = 0; i < 256; i++)
    {
        int d_r = HMM_ABS(r - playpal_data[0][i][0]);
        int d_g = HMM_ABS(g - playpal_data[0][i][1]);
        int d_b = HMM_ABS(b - playpal_data[0][i][2]);

        int dist = d_r * d_r + d_g * d_g + d_b * d_b;

        if (dist == 0)
            return i;

        if (dist < best_dist)
        {
            best      = i;
            best_dist = dist;
        }
    }

    return best;
}

//
// Find the best match for the pure colour.  `which' is 0 for red, 1
// for green and 2 for blue.
//
static int V_FindPureColour(int which)
{
    int i;

    int best      = 0;
    int best_dist = 1 << 30;

    for (i = 0; i < 256; i++)
    {
        int a = playpal_data[0][i][which];
        int b = playpal_data[0][i][(which + 1) % 3];
        int c = playpal_data[0][i][(which + 2) % 3];
        int d = HMM_MAX(b, c);

        int dist = 255 - (a - d);

        // the pure colour must shine through
        if (a <= d)
            continue;

        if (dist < best_dist)
        {
            best      = i;
            best_dist = dist;
        }
    }

    return best;
}

void V_SetPalette(int type, float amount)
{
    int palette = 0;

    // -AJA- 1999/09/17: fixes problems with black text etc.
    if (!loaded_playpal)
        return;

    if (amount >= 0.95f)
        amount = 0.95f;

    switch (type)
    {
        // Pain color fading is now handled differently in V_IndexColourToRGB
        // case PALETTE_PAIN:
        // palette = (int)(PAIN_PALS + amount * NUM_PAIN_PALS);
        // break;

    case PALETTE_BONUS:
        palette = (int)(BONUS_PALS + amount * NUM_BONUS_PALS);
        break;

    case PALETTE_SUIT:
        palette = RADIATION_PAL;
        break;
    }

    if (palette == cur_palette)
        return;

    cur_palette = palette;
}

//
// Computes the right "colourmap" (more precisely, coltable) to put into
// the dc_colourmap & ds_colourmap variables for use by the column &
// span drawers.
//
static void LoadColourmap(const Colormap *colm)
{
    int   size;
    uint8_t *data;

    // we are writing to const marked memory here. Here is the only place
    // the cache struct is touched.
    ColormapCache *cache = (ColormapCache *)&colm->cache_;

    if (colm->pack_name_ != "")
    {
        epi::File *f = W_OpenPackFile(colm->pack_name_);
        if (f == nullptr)
            FatalError("No such colormap file: %s\n", colm->pack_name_.c_str());
        size = f->GetLength();
        data = f->LoadIntoMemory();
        delete f; // close file
    }
    else
    {
        data = W_LoadLump(colm->lump_name_.c_str(), &size);
    }

    if ((colm->start_ + colm->length_) * 256 > size)
    {
        FatalError("Colourmap [%s] is too small ! (LENGTH too big)\n", colm->name_.c_str());
    }

    cache->size = colm->length_ * 256;
    cache->data = new uint8_t[cache->size];

    memcpy(cache->data, data + (colm->start_ * 256), cache->size);

    delete[] data;
}

const uint8_t *V_GetTranslationTable(const Colormap *colmap)
{
    // Do we need to load or recompute this colourmap ?

    if (colmap->cache_.data == nullptr)
        LoadColourmap(colmap);

    return (const uint8_t *)colmap->cache_.data;
}

void R_TranslatePalette(uint8_t *new_pal, const uint8_t *old_pal, const Colormap *trans)
{

    // is the colormap just using GL_COLOUR?
    if (trans->length_ == 0)
    {
        int r = epi::GetRGBARed(trans->gl_color_);
        int g = epi::GetRGBAGreen(trans->gl_color_);
        int b = epi::GetRGBABlue(trans->gl_color_);

        for (int j = 0; j < 256; j++)
        {
            new_pal[j * 3 + 0] = old_pal[j * 3 + 0] * (r + 1) / 256;
            new_pal[j * 3 + 1] = old_pal[j * 3 + 1] * (g + 1) / 256;
            new_pal[j * 3 + 2] = old_pal[j * 3 + 2] * (b + 1) / 256;
        }
    }
    else
    {
        // do the actual translation
        const uint8_t *trans_table = V_GetTranslationTable(trans);

        for (int j = 0; j < 256; j++)
        {
            int k = trans_table[j];

            new_pal[j * 3 + 0] = old_pal[k * 3 + 0];
            new_pal[j * 3 + 1] = old_pal[k * 3 + 1];
            new_pal[j * 3 + 2] = old_pal[k * 3 + 2];
        }
    }
}

static int AnalyseColourmap(const uint8_t *table, int alpha, int *r, int *g, int *b)
{
    /* analyse whole colourmap */
    int r_tot = 0;
    int g_tot = 0;
    int b_tot = 0;
    int total = 0;

    for (int j = 0; j < 256; j++)
    {
        int r0 = playpal_data[0][j][0];
        int g0 = playpal_data[0][j][1];
        int b0 = playpal_data[0][j][2];

        // give the grey-scales more importance
        int weight = (r0 == g0 && g0 == b0) ? 3 : 1;

        r0 = (255 * alpha + r0 * (255 - alpha)) / 255;
        g0 = (255 * alpha + g0 * (255 - alpha)) / 255;
        b0 = (255 * alpha + b0 * (255 - alpha)) / 255;

        int r1 = playpal_data[0][table[j]][0];
        int g1 = playpal_data[0][table[j]][1];
        int b1 = playpal_data[0][table[j]][2];

        int r_div = 255 * HMM_MAX(4, r1) / HMM_MAX(4, r0);
        int g_div = 255 * HMM_MAX(4, g1) / HMM_MAX(4, g0);
        int b_div = 255 * HMM_MAX(4, b1) / HMM_MAX(4, b0);

        r_div = HMM_MAX(4, HMM_MIN(4096, r_div));
        g_div = HMM_MAX(4, HMM_MIN(4096, g_div));
        b_div = HMM_MAX(4, HMM_MIN(4096, b_div));

#if 0 // DEBUGGING
		LogPrint("#%02x%02x%02x / #%02x%02x%02x = (%d,%d,%d)\n",
				 r1, g1, b1, r0, g0, b0, r_div, g_div, b_div);
#endif
        r_tot += r_div * weight;
        g_tot += g_div * weight;
        b_tot += b_div * weight;
        total += weight;
    }

    (*r) = r_tot / total;
    (*g) = g_tot / total;
    (*b) = b_tot / total;

    // scale down when too large to fit
    int ity = HMM_MAX(*r, HMM_MAX(*g, *b));

    if (ity > 255)
    {
        (*r) = (*r) * 255 / ity;
        (*g) = (*g) * 255 / ity;
        (*b) = (*b) * 255 / ity;
    }

    // compute distance score
    total = 0;

    for (int k = 0; k < 256; k++)
    {
        int r0 = playpal_data[0][k][0];
        int g0 = playpal_data[0][k][1];
        int b0 = playpal_data[0][k][2];

        // on-screen colour: c' = c * M * (1 - A) + M * A
        int sr = (r0 * (*r) / 255 * (255 - alpha) + (*r) * alpha) / 255;
        int sg = (g0 * (*g) / 255 * (255 - alpha) + (*g) * alpha) / 255;
        int sb = (b0 * (*b) / 255 * (255 - alpha) + (*b) * alpha) / 255;

        // FIXME: this is the INVULN function
#if 0
		sr = (HMM_MAX(0, (*r /2 + 128) - r0) * (255 - alpha) + (*r) * alpha) / 255;
		sg = (HMM_MAX(0, (*g /2 + 128) - g0) * (255 - alpha) + (*g) * alpha) / 255;
		sb = (HMM_MAX(0, (*b /2 + 128) - b0) * (255 - alpha) + (*b) * alpha) / 255;
#endif

        int r1 = playpal_data[0][table[k]][0];
        int g1 = playpal_data[0][table[k]][1];
        int b1 = playpal_data[0][table[k]][2];

        // FIXME: use weighting (more for greyscale)
        total += (sr - r1) * (sr - r1);
        total += (sg - g1) * (sg - g1);
        total += (sb - b1) * (sb - b1);
    }

    return total / 256;
}

void TransformColourmap(Colormap *colmap)
{
    const uint8_t *table = colmap->cache_.data;

    if (table == nullptr && (!colmap->lump_name_.empty() || !colmap->pack_name_.empty()))
    {
        LoadColourmap(colmap);

        table = (uint8_t *)colmap->cache_.data;
    }

    if (colmap->font_colour_ == kRGBANoValue)
    {
        if (colmap->gl_color_ != kRGBANoValue)
            colmap->font_colour_ = colmap->gl_color_;
        else
        {
            SYS_ASSERT(table);

            // for fonts, we only care about the GRAY colour
            int r = playpal_data[0][table[pal_gray239]][0] * 255 / 239;
            int g = playpal_data[0][table[pal_gray239]][1] * 255 / 239;
            int b = playpal_data[0][table[pal_gray239]][2] * 255 / 239;

            r = HMM_MIN(255, HMM_MAX(0, r));
            g = HMM_MIN(255, HMM_MAX(0, g));
            b = HMM_MIN(255, HMM_MAX(0, b));

            colmap->font_colour_ = epi::MakeRGBA(r, g, b);
        }
    }

    if (colmap->gl_color_ == kRGBANoValue)
    {
        SYS_ASSERT(table);

        int r, g, b;

        // int score =
        AnalyseColourmap(table, 0, &r, &g, &b);

#if 0 // DEBUGGING
		LogDebug("COLMAP [%s] alpha %d --> (%d %d %d)\n",
				 colmap->name.c_str(), 0, r, g, b);
#endif

        r = HMM_MIN(255, HMM_MAX(0, r));
        g = HMM_MIN(255, HMM_MAX(0, g));
        b = HMM_MIN(255, HMM_MAX(0, b));

        colmap->gl_color_ = epi::MakeRGBA(r, g, b);
    }

    LogDebug("TransformColourmap [%s]\n", colmap->name_.c_str());
    LogDebug("- gl_color_   = #%06x\n", colmap->gl_color_);
}

void V_GetColmapRGB(const Colormap *colmap, float *r, float *g, float *b)
{
    if (colmap->gl_color_ == kRGBANoValue)
    {
        // Intention Const Override
        TransformColourmap((Colormap *)colmap);
    }

    RGBAColor col = colmap->gl_color_;

    (*r) = GAMMA_CONV((col >> 24) & 0xFF) / 255.0f;
    (*g) = GAMMA_CONV((col >> 16) & 0xFF) / 255.0f;
    (*b) = GAMMA_CONV((col >> 8) & 0xFF) / 255.0f;
}

RGBAColor V_GetFontColor(const Colormap *colmap)
{
    if (!colmap)
        return kRGBANoValue;

    if (colmap->font_colour_ == kRGBANoValue)
    {
        // Intention Const Override
        TransformColourmap((Colormap *)colmap);
    }

    return colmap->font_colour_;
}

RGBAColor V_ParseFontColor(const char *name, bool strict)
{
    if (!name || !name[0])
        return kRGBANoValue;

    RGBAColor rgb;

    if (name[0] == '#')
    {
        int       r, g, b;

        if (sscanf(name, " #%2x%2x%2x ", &r, &g, &b) != 3)
            FatalError("Bad RGB colour value: %s\n", name);

        rgb = epi::MakeRGBA((uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
    else
    {
        const Colormap *colmap = colormaps.Lookup(name);

        if (!colmap)
        {
            if (strict)
                FatalError("Unknown colormap: '%s'\n", name);
            else
                LogDebug("Unknown colormap: '%s'\n", name);

            return SG_MAGENTA_RGBA32;
        }

        rgb = V_GetFontColor(colmap);
    }

    if (rgb == kRGBANoValue)
        rgb ^= 0x00010100;

    return rgb;
}

//
// Returns an RGB value from an index value - used the current
// palette.  The byte pointer is assumed to point a 3-byte array.
//
void V_IndexColourToRGB(int indexcol, uint8_t *returncol, RGBAColor last_damage_colour, float damageAmount)
{
    if ((cur_palette == PALETTE_NORMAL) || (cur_palette == PALETTE_PAIN))
    {
        float r = (float)epi::GetRGBARed(last_damage_colour) / 255.0;
        float g = (float)epi::GetRGBAGreen(last_damage_colour) / 255.0;
        float b = (float)epi::GetRGBABlue(last_damage_colour) / 255.0;

        returncol[0] = (uint8_t)HMM_MAX(0, HMM_MIN(255, r * damageAmount * 2.5));
        returncol[1] = (uint8_t)HMM_MAX(0, HMM_MIN(255, g * damageAmount * 2.5));
        returncol[2] = (uint8_t)HMM_MAX(0, HMM_MIN(255, b * damageAmount * 2.5));
    }
    else
    {
        returncol[0] = playpal_data[cur_palette][indexcol][0];
        returncol[1] = playpal_data[cur_palette][indexcol][1];
        returncol[2] = playpal_data[cur_palette][indexcol][2];
    }
}

RGBAColor V_LookupColour(int col)
{
    int r = playpal_data[0][col][0];
    int g = playpal_data[0][col][1];
    int b = playpal_data[0][col][2];

    return epi::MakeRGBA(r, g, b);
}

#if 0 // OLD BUT POTENTIALLY USEFUL
static void SetupLightMap(LightingModel model)
{
	for (i=0; i < 256; i++)
	{
		// Approximation of standard Doom lighting: 
		// (based on side-by-side comparison)
		//    [0,72] --> [0,16]
		//    [72,112] --> [16,56]
		//    [112,255] --> [56,255]

		if (i <= 72)
			rgl_light_map[i] = i * 16 / 72;
		else if (i <= 112)
			rgl_light_map[i] = 16 + (i - 72) * 40 / 40;
		else if (i < 255)
			rgl_light_map[i] = 56 + (i - 112) * 200 / 144;
		else
			rgl_light_map[i] = 255;
	}
}
#endif

// -AJA- 1999/07/03: Rewrote this routine, since the palette handling
// has been moved to v_colour.c/h (and made more flexible).  Later on it
// might be good to DDF-ify all this, allowing other palette lumps and
// being able to set priorities for the different effects.

void R_PaletteStuff(void)
{
    int   palette = PALETTE_NORMAL;
    float amount  = 0;

    player_t *p = players[displayplayer];
    SYS_ASSERT(p);

    int cnt = p->damagecount;

    if (cnt)
    {
        palette = PALETTE_PAIN;
        amount  = (cnt + 7) / 160.0f; // 64.0f;
    }
    else if (p->bonuscount)
    {
        palette = PALETTE_BONUS;
        amount  = (p->bonuscount + 7) / 32.0f;
    }
    else if (p->powers[kPowerTypeAcidSuit] > 4 * 32 || fmod(p->powers[kPowerTypeAcidSuit], 16) >= 8)
    {
        palette = PALETTE_SUIT;
        amount  = 1.0f;
    }

    // This routine will limit `amount' to acceptable values, and will
    // only update the video palette/colormaps when the palette actually
    // changes.
    V_SetPalette(palette, amount);
}

//----------------------------------------------------------------------------
//  COLORMAP SHADERS
//----------------------------------------------------------------------------

int R_DoomLightingEquation(int L, float dist)
{
    /* L in the range 0 to 63 */

    int min_L = HMM_Clamp(0, 36 - L, 31);

    int index = (59 - L) - int(1280 / HMM_MAX(1, dist));

    /* result is colormap index (0 bright .. 31 dark) */
    return HMM_Clamp(min_L, index, 31);
}

class colormap_shader_c : public abstract_shader_c
{
  private:
    const Colormap *colmap;

    int light_lev;

    GLuint fade_tex;

    bool             simple_cmap;
    LightingModel lt_model;

    RGBAColor whites[32];

    RGBAColor fog_color;
    float    fog_density;

    // for DDFLEVL fog checks
    sector_t *sec;

  public:
    colormap_shader_c(const Colormap *CM)
        : colmap(CM), light_lev(255), fade_tex(0), simple_cmap(true), lt_model(kLightingModelDoom), fog_color(kRGBANoValue),
          fog_density(0), sec(nullptr)
    {
    }

    virtual ~colormap_shader_c()
    {
        DeleteTex();
    }

  private:
    inline float DistFromViewplane(float x, float y, float z)
    {
        float dx = (x - viewx) * viewforward.X;
        float dy = (y - viewy) * viewforward.Y;
        float dz = (z - viewz) * viewforward.Z;

        return dx + dy + dz;
    }

    inline void TexCoord(local_gl_vert_t *v, int t, const HMM_Vec3 *lit_pos)
    {
        float dist = DistFromViewplane(lit_pos->X, lit_pos->Y, lit_pos->Z);

        int L = light_lev / 4; // need integer range 0-63

        v->texc[t].X = dist / 1600.0;
        v->texc[t].Y = (L + 0.5) / 64.0;
    }

  public:
    virtual void Sample(multi_color_c *col, float x, float y, float z)
    {
        // FIXME: assumes standard COLORMAP

        float dist = DistFromViewplane(x, y, z);

        int cmap_idx;

        if (lt_model >= kLightingModelFlat)
            cmap_idx = HMM_Clamp(0, 42 - light_lev / 6, 31);
        else
            cmap_idx = R_DoomLightingEquation(light_lev / 4, dist);

        RGBAColor WH = whites[cmap_idx];

        col->mod_R += epi::GetRGBARed(WH);
        col->mod_G += epi::GetRGBAGreen(WH);
        col->mod_B += epi::GetRGBABlue(WH);

        // FIXME: for foggy maps, need to adjust add_R/G/B too
    }

    virtual void Corner(multi_color_c *col, float nx, float ny, float nz, struct mobj_s *mod_pos, bool is_weapon)
    {
        // TODO: improve this (normal-ise a little bit)

        float mx = mod_pos->x;
        float my = mod_pos->y;
        float mz = mod_pos->z + mod_pos->height / 2;

        if (is_weapon)
        {
            mx += viewcos * 110;
            my += viewsin * 110;
        }

        Sample(col, mx, my, mz);
    }

    virtual void WorldMix(GLuint shape, int num_vert, GLuint tex, float alpha, int *pass_var, int blending, bool masked,
                          void *data, shader_coord_func_t func)
    {
        RGBAColor fc_to_use = fog_color;
        float    fd_to_use = fog_density;
        // check for DDFLEVL fog
        if (fc_to_use == kRGBANoValue)
        {
            if (IS_SKY(sec->ceil))
            {
                fc_to_use = current_map->outdoor_fog_color_;
                fd_to_use = 0.01f * current_map->outdoor_fog_density_;
            }
            else
            {
                fc_to_use = current_map->indoor_fog_color_;
                fd_to_use = 0.01f * current_map->indoor_fog_density_;
            }
        }

        local_gl_vert_t *glvert =
            RGL_BeginUnit(shape, num_vert, GL_MODULATE, tex, (simple_cmap || r_dumbmulti.d_) ? GL_MODULATE : GL_DECAL,
                          fade_tex, *pass_var, blending, fc_to_use, fd_to_use);

        for (int v_idx = 0; v_idx < num_vert; v_idx++)
        {
            local_gl_vert_t *dest = glvert + v_idx;

            dest->rgba[3] = alpha;

            HMM_Vec3 lit_pos;

            (*func)(data, v_idx, &dest->pos, dest->rgba, &dest->texc[0], &dest->normal, &lit_pos);

            TexCoord(dest, 1, &lit_pos);
        }

        RGL_EndUnit(num_vert);

        (*pass_var) += 1;
    }

  private:
    void MakeColormapTexture(int mode)
    {
        image_data_c img(256, 64, 4);

        const uint8_t *map    = nullptr;
        int         length = 32;

        if (colmap && colmap->length_ > 0)
        {
            map    = V_GetTranslationTable(colmap);
            length = colmap->length_;

            for (int ci = 0; ci < 32; ci++)
            {
                int cmap_idx = length * ci / 32;

                // +4 gets the white pixel -- FIXME: doom specific
                const uint8_t new_col = map[cmap_idx * 256 + 4];

                int r = playpal_data[0][new_col][0];
                int g = playpal_data[0][new_col][1];
                int b = playpal_data[0][new_col][2];

                whites[ci] = epi::MakeRGBA(r, g, b);
            }
        }
        else if (colmap) // GL_COLOUR
        {
            for (int ci = 0; ci < 32; ci++)
            {
                int r = epi::GetRGBARed(colmap->gl_color_) * (31 - ci) / 31;
                int g = epi::GetRGBAGreen(colmap->gl_color_) * (31 - ci) / 31;
                int b = epi::GetRGBABlue(colmap->gl_color_) * (31 - ci) / 31;

                whites[ci] = epi::MakeRGBA(r, g, b);
            }
        }
        else
        {
            for (int ci = 0; ci < 32; ci++)
            {
                int ity = 255 - ci * 8 - ci / 5;

                whites[ci] = epi::MakeRGBA(ity, ity, ity);
            }
        }

        for (int L = 0; L < 64; L++)
        {
            uint8_t *dest = img.PixelAt(0, L);

            for (int x = 0; x < 256; x++, dest += 4)
            {
                float dist = 1600.0f * x / 255.0;

                int index;

                if (lt_model >= kLightingModelFlat)
                {
                    // FLAT lighting
                    index = HMM_Clamp(0, 42 - (L * 2 / 3), 31);
                }
                else
                {
                    // DOOM lighting formula
                    index = R_DoomLightingEquation(L, dist);
                }

                // WTF  index = index * length / 32;

                if (false) //!!!! (mode == 1)
                {
                    // GL_DECAL mode
                    dest[0] = 0;
                    dest[1] = 0;
                    dest[2] = 0;
                    dest[3] = 0 + index * 8;
                }
                else if (mode == 0)
                {
                    // GL_MODULATE mode
                    if (colmap)
                    {
                        dest[0] = epi::GetRGBARed(whites[index]);
                        dest[1] = epi::GetRGBAGreen(whites[index]);
                        dest[2] = epi::GetRGBABlue(whites[index]);
                        dest[3] = 255;
                    }
                    else
                    {
                        dest[0] = 255 - index * 8;
                        dest[1] = dest[0];
                        dest[2] = dest[0];
                        dest[3] = 255;
                    }
                }
                else if (mode == 2)
                {
                    // additive pass (OLD CARDS)
                    dest[0] = index * 8 * 128 / 256;
                    dest[1] = dest[0];
                    dest[2] = dest[0];
                    dest[3] = 255;
                }
            }
        }

        fade_tex = R_UploadTexture(&img, UPL_Smooth | UPL_Clamp);
    }

  public:
    void Update()
    {
        if (fade_tex == 0 || (r_forceflatlighting.d_&& lt_model != kLightingModelFlat) ||
            (!r_forceflatlighting.d_&& lt_model != current_map->episode_->lighting_))
        {
            if (fade_tex != 0)
            {
                glDeleteTextures(1, &fade_tex);
            }

            if (r_forceflatlighting.d_)
                lt_model = kLightingModelFlat;
            else
                lt_model = current_map->episode_->lighting_;

            MakeColormapTexture(0);
        }
    }

    void DeleteTex()
    {
        if (fade_tex != 0)
        {
            glDeleteTextures(1, &fade_tex);
            fade_tex = 0;
        }
    }

    void SetLight(int _level)
    {
        light_lev = _level;
    }

    void SetFog(RGBAColor _fog_color, float _fog_density)
    {
        fog_color   = _fog_color;
        fog_density = _fog_density;
    }

    void SetSector(sector_t *_sec)
    {
        sec = _sec;
    }
};

static colormap_shader_c *std_cmap_shader;

abstract_shader_c *R_GetColormapShader(const struct region_properties_s *props, int light_add, sector_t *sec)
{
    if (!std_cmap_shader)
        std_cmap_shader = new colormap_shader_c(nullptr);

    colormap_shader_c *shader = std_cmap_shader;

    if (props->colourmap)
    {
        if (props->colourmap->analysis_)
            shader = (colormap_shader_c *)props->colourmap->analysis_;
        else
        {
            shader = new colormap_shader_c(props->colourmap);

            // Intentional Const Override
            Colormap *CM = (Colormap *)props->colourmap;
            CM->analysis_    = shader;
        }
    }

    SYS_ASSERT(shader);

    shader->Update();

    int lit_Nom = props->lightlevel + light_add + ((v_secbright.d_- 5) * 10);

    if (!(props->colourmap && (props->colourmap->special_ & kColorSpecialNoFlash)) || ren_extralight > 250)
    {
        lit_Nom += ren_extralight;
    }

    lit_Nom = HMM_Clamp(0, lit_Nom, 255);

    shader->SetLight(lit_Nom);

    shader->SetFog(props->fog_color, props->fog_density);

    shader->SetSector(sec);

    return shader;
}

void DeleteColourmapTextures(void)
{
    if (std_cmap_shader)
        std_cmap_shader->DeleteTex();

    std_cmap_shader = nullptr;

    for (auto cmap : colormaps)
    {
        if (cmap->analysis_)
        {
            colormap_shader_c *shader = (colormap_shader_c *)cmap->analysis_;

            shader->DeleteTex();
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
