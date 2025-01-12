#pragma once

#include "sokol_local.h"

void RegisterImageSampler(uint32_t imageId, sg_sampler_desc *desc);
void GetImageSampler(uint32_t imageId, uint32_t *sampler_id);

void DeleteImage(sg_image image);
void FinalizeDeletedImages();

void InitImages();
