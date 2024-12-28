#include "sokol_pipeline.h"

typedef std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> PipelineMap;

// flags => pipeline
static std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> pipelines;

sgl_pipeline GetPipeline(sgl_context context, uint32_t pipeline_flags)
{
    PipelineMap::iterator context_itr = pipelines.find(context.id);

    if (context_itr == pipelines.end())
    {
        pipelines[context.id] = std::unordered_map<uint32_t, uint32_t>();
        context_itr           = pipelines.find(context.id);
    }

    std::unordered_map<uint32_t, uint32_t>::iterator pipeline_itr = context_itr->second.find(pipeline_flags);
    uint32_t                                         pipeline_id  = 0xFFFFFFFF;
    if (pipeline_itr == context_itr->second.end())
    {
        sg_pipeline_desc pipeline_desc = {0};
        if (!(pipeline_flags & kPipelineDepthTest))
            pipeline_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
        else if (pipeline_flags & kPipelineDepthGreater)
            pipeline_desc.depth.compare = SG_COMPAREFUNC_GREATER;
        else
            pipeline_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;

        if (pipeline_flags & kPipelineDepthWrite)
            pipeline_desc.depth.write_enabled = true;

        if (pipeline_flags & kPipelineAlpha)
        {
            pipeline_desc.colors[0].blend.enabled        = true;
            pipeline_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
            pipeline_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        }
        if (pipeline_flags & kPipelineAdditive)
        {
            pipeline_desc.colors[0].blend.enabled        = true;
            pipeline_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
            pipeline_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        }

        pipeline_id = sgl_context_make_pipeline(context, &pipeline_desc).id;

        context_itr->second[pipeline_flags] = pipeline_id;
    }
    else
    {
        pipeline_id = pipeline_itr->second;
    }

    sgl_pipeline pipeline = {pipeline_id};
    return pipeline;
}

void InitPipelines()
{
}