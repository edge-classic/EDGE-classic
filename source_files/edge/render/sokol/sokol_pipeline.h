#pragma once

#include "sokol_local.h"

enum PipelineFlags
{
    kPipelineDepthTest    = 1 << 0,
    kPipelineDepthWrite   = 1 << 1,
    kPipelineDepthGreater = 1 << 2,
    kPipelineAdditive     = 1 << 3,
    kPipelineAlpha        = 1 << 4    
};

void InitPipelines();

sgl_pipeline GetPipeline(sgl_context context, uint32_t pipeline_flags);


