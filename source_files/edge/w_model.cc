//----------------------------------------------------------------------------
//  EDGE Model Management
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

#include "i_defs.h"
#include "i_defs_gl.h"

// EPI
#include "str_util.h"
#include "str_compare.h"
#include "e_main.h"
#include "r_image.h"
#include "r_md2.h"
#include "r_mdl.h"
#include "r_things.h"
#include "w_files.h"
#include "w_model.h"
#include "w_wad.h"

#include "p_local.h" // mobjlisthead

// Model storage
static modeldef_c **models;
static int          nummodels = 0;

modeldef_c::modeldef_c(const char *_prefix) : md2_model(nullptr), mdl_model(nullptr)
{
    strcpy(name, _prefix);

    for (int i = 0; i < MAX_MODEL_SKINS; i++)
        skins[i] = nullptr;
}

modeldef_c::~modeldef_c()
{
    // FIXME: delete model;

    // TODO: free the skins
}

static void FindModelFrameNames(md2_model_c *md, int model_num)
{
    int missing = 0;

    I_Debugf("Finding frame names for model '%s'...\n", ddf_model_names[model_num].c_str());

    for (int stnum = 1; stnum < num_states; stnum++)
    {
        State *st = &states[stnum];

        if (st->sprite != model_num)
            continue;

        if (!(st->flags & kStateFrameFlagModel))
            continue;

        if (!(st->flags & kStateFrameFlagUnmapped))
            continue;

        SYS_ASSERT(st->model_frame);

        st->frame = MD2_FindFrame(md, st->model_frame);

        if (st->frame >= 0)
        {
            st->flags &= ~kStateFrameFlagUnmapped;
        }
        else
        {
            missing++;
            I_Printf("-- no such frame '%s'\n", st->model_frame);
        }
    }

    if (missing > 0)
        I_Error("Failed to find %d frames for model '%s' (see EDGE.LOG)\n", missing,
                ddf_model_names[model_num].c_str());
}

static void FindModelFrameNames(mdl_model_c *md, int model_num)
{
    int missing = 0;

    I_Debugf("Finding frame names for model '%s'...\n", ddf_model_names[model_num].c_str());

    for (int stnum = 1; stnum < num_states; stnum++)
    {
        State *st = &states[stnum];

        if (st->sprite != model_num)
            continue;

        if (!(st->flags & kStateFrameFlagModel))
            continue;

        if (!(st->flags & kStateFrameFlagUnmapped))
            continue;

        SYS_ASSERT(st->model_frame);

        st->frame = MDL_FindFrame(md, st->model_frame);

        if (st->frame >= 0)
        {
            st->flags &= ~kStateFrameFlagUnmapped;
        }
        else
        {
            missing++;
            I_Printf("-- no such frame '%s'\n", st->model_frame);
        }
    }

    if (missing > 0)
        I_Error("Failed to find %d frames for model '%s' (see EDGE.LOG)\n", missing,
                ddf_model_names[model_num].c_str());
}

modeldef_c *LoadModelFromLump(int model_num)
{
    std::string basename = ddf_model_names[model_num];

    modeldef_c *def = new modeldef_c(basename.c_str());

    std::string lumpname;
    std::string packname;
    std::string skinname;

    int          lump_num  = -1;
    int          pack_num  = -1;
    epi::File *f         = nullptr;
    bool         pack_file = false;

    // try MD3 first, then MD2, then MDL, then voxels

    // This section is going to get kinda weird with the introduction of EPKs
    lumpname = epi::StringFormat("%sMD3", basename.c_str());
    lump_num = W_CheckFileNumForName(lumpname.c_str());
    packname = epi::StringFormat("%s.md3", basename.c_str());
    pack_num = W_CheckPackForName(packname);
    if (pack_num == -1)
    {
        packname = epi::StringFormat("%sMD3.md3", basename.c_str());
        pack_num = W_CheckPackForName(packname);
    }

    if (lump_num > -1 || pack_num > -1)
    {
        if (pack_num > lump_num)
        {
            f = W_OpenPackFile(packname);
            if (f)
            {
                I_Debugf("Loading MD3 model from pack file : %s\n", packname.c_str());
                def->md2_model = MD3_LoadModel(f);
                pack_file      = true;
            }
        }
        else
        {
            I_Debugf("Loading MD3 model from lump : %s\n", lumpname.c_str());
            f = W_OpenLump(lumpname.c_str());
            if (f)
                def->md2_model = MD3_LoadModel(f);
        }
    }

    if (!f)
    {
        lumpname = epi::StringFormat("%sMD2", basename.c_str());
        lump_num = W_CheckFileNumForName(lumpname.c_str());
        packname = epi::StringFormat("%s.md2", basename.c_str());
        pack_num = W_CheckPackForName(packname);
        if (pack_num == -1)
        {
            packname = epi::StringFormat("%sMD2.md2", basename.c_str());
            pack_num = W_CheckPackForName(packname);
        }
        if (lump_num > -1 || pack_num > -1)
        {
            if (pack_num > lump_num)
            {
                f = W_OpenPackFile(packname);
                if (f)
                {
                    I_Debugf("Loading MD2 model from pack file : %s\n", packname.c_str());
                    def->md2_model = MD2_LoadModel(f);
                    pack_file      = true;
                }
            }
            else
            {
                I_Debugf("Loading MD2 model from lump : %s\n", lumpname.c_str());
                f = W_OpenLump(lumpname.c_str());
                if (f)
                    def->md2_model = MD2_LoadModel(f);
            }
        }
    }

    if (!f)
    {
        lumpname = epi::StringFormat("%sMDL", basename.c_str());
        lump_num = W_CheckFileNumForName(lumpname.c_str());
        packname = epi::StringFormat("%s.mdl", basename.c_str());
        pack_num = W_CheckPackForName(packname);
        if (pack_num == -1)
        {
            packname = epi::StringFormat("%sMDL.mdl", basename.c_str());
            pack_num = W_CheckPackForName(packname);
        }
        if (lump_num > -1 || pack_num > -1)
        {
            if (pack_num > lump_num)
            {
                f = W_OpenPackFile(packname);
                if (f)
                {
                    I_Debugf("Loading MDL model from pack file : %s\n", packname.c_str());
                    def->mdl_model = MDL_LoadModel(f);
                    pack_file      = true;
                }
            }
            else
            {
                I_Debugf("Loading MDL model from lump : %s\n", lumpname.c_str());
                f = W_OpenLump(lumpname.c_str());
                if (f)
                    def->mdl_model = MDL_LoadModel(f);
            }
        }
    }

    if (!f)
        I_Error("Missing model lump for: %s\n!", basename.c_str());

    SYS_ASSERT(def->md2_model || def->mdl_model);

    // close the lump/packfile
    delete f;

    if (def->md2_model)
    {
        for (int i = 0; i < 10; i++)
        {
            if (pack_file)
            {
                skinname      = epi::StringFormat("%s%d", basename.c_str(), i);
                def->skins[i] = W_ImageLookup(skinname.c_str(), kImageNamespaceSprite, ILF_Null);
                if (!def->skins[i])
                {
                    skinname      = epi::StringFormat("%sSKN%d", basename.c_str(), i);
                    def->skins[i] = W_ImageLookup(skinname.c_str(), kImageNamespaceSprite, ILF_Null);
                }
            }
            else
            {
                skinname      = epi::StringFormat("%sSKN%d", basename.c_str(), i);
                def->skins[i] = W_ImageLookup(skinname.c_str(), kImageNamespaceSprite, ILF_Null);
            }
        }
    }

    // need at least one skin (MD2/MD3 only; MDLs and VXLs should have them baked in already)
    if (def->md2_model)
    {
        if (!def->skins[1]) // What happened to skin 0? - Dasho
        {
            if (pack_file)
                I_Error("Missing model skin: %s1\n", basename.c_str());
            else
                I_Error("Missing model skin: %sSKN1\n", basename.c_str());
        }
    }

    if (def->md2_model)
        FindModelFrameNames(def->md2_model, model_num);

    if (def->mdl_model)
        FindModelFrameNames(def->mdl_model, model_num);

    return def;
}

void W_InitModels(void)
{
    nummodels = (int)ddf_model_names.size();

    SYS_ASSERT(nummodels >= 1); // at least SPR_NULL

    E_ProgressMessage("Setting up models...");

    I_Printf("W_InitModels: Setting up\n");

    models = new modeldef_c *[nummodels];

    for (int i = 0; i < nummodels; i++)
        models[i] = nullptr;
}

modeldef_c *W_GetModel(int model_num)
{
    // model_num comes from the 'sprite' field of State, and
    // is also an index into ddf_model_names vector.

    SYS_ASSERT(model_num > 0);
    SYS_ASSERT(model_num < nummodels);

    if (!models[model_num])
    {
        models[model_num] = LoadModelFromLump(model_num);
    }

    return models[model_num];
}

void W_PrecacheModels(void)
{
    if (nummodels <= 0)
        return;

    uint8_t *model_present = new uint8_t[nummodels];
    memset(model_present, 0, nummodels);

    // mark all monsters (etc) in the level
    for (mobj_t *mo = mobjlisthead; mo; mo = mo->next)
    {
        SYS_ASSERT(mo->state);

        if (!(mo->state->flags & kStateFrameFlagModel))
            continue;

        int model = mo->state->sprite;

        const char *model_name = nullptr;

        if (model >= 1 && model < nummodels)
            model_name = ddf_model_names[model].c_str();

        if (model_name)
        {
            for (int i = 1; i < nummodels; i++)
            {
                if (epi::StringCaseCompareMaxASCII(model_name, ddf_model_names[i], 4) == 0)
                    model_present[i] = 1;
            }
        }
    }

    // mark all weapons
    for (int k = 1; k < num_states; k++)
    {
        if ((states[k].flags & (kStateFrameFlagWeapon | kStateFrameFlagModel)) != (kStateFrameFlagWeapon | kStateFrameFlagModel))
            continue;

        int model = states[k].sprite;

        const char *model_name = nullptr;

        if (model >= 1 && model < nummodels)
            model_name = ddf_model_names[model].c_str();

        if (model_name)
        {
            for (int i = 1; i < nummodels; i++)
            {
                if (epi::StringCaseCompareMaxASCII(model_name, ddf_model_names[i], 4) == 0)
                    model_present[i] = 1;
            }
        }
    }

    for (int i = 1; i < nummodels; i++) // ignore SPR_NULL
    {
        if (model_present[i])
        {
            I_Debugf("Precaching model: %s\n", ddf_model_names[i].c_str());

            modeldef_c *def = W_GetModel(i);

            // precache skins too
            for (int n = 0; n < 10; n++)
            {
                if (def && def->skins[n])
                    W_ImagePreCache(def->skins[n]);
            }
        }
    }

    delete[] model_present;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
