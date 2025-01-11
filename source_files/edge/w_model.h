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

#pragma once

#include "r_defs.h"

class MD2Model;
class MDLModel;

constexpr uint8_t kMaximumModelSkins = 10;

class ModelDefinition
{
  public:
    // four letter model name (e.g. "TROO").
    char name_[6];

    // radius in model (vertex) space
    float radius_;

    MD2Model *md2_model_;
    MDLModel *mdl_model_;

    const Image *skins_[kMaximumModelSkins];

  public:
    ModelDefinition(const char *prefix);
    ~ModelDefinition();
};

/* Functions */

void InitializeModels(void);

void PrecacheModels(void);

ModelDefinition *GetModel(int model_num);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
