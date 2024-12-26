#pragma once

#include "sokol_local.h"

enum PipelineFlags
{
    kPipelineDepthWrite   = 1 << 0,
    kPipelineDepthGreater = 1 << 1,
    kPipelineAdditive     = 1 << 2,
    kPipelineAlpha        = 1 << 3    
};

void InitPipelines();

sgl_pipeline GetPipeline(sgl_context context, uint32_t pipeline_flags);


