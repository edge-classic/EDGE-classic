#include "sokol_pipeline.h"

#include "epi.h"

typedef std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> PipelineMap;

// flags => pipeline
static std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> pipelines;

sgl_pipeline GetPipeline(sgl_context context, uint32_t pipeline_flags, GLenum src_blend, GLenum dst_blend)
{
    PipelineMap::iterator context_itr = pipelines.find(context.id);

    if (context_itr == pipelines.end())
    {
        pipelines[context.id] = std::unordered_map<uint32_t, uint32_t>();
        context_itr           = pipelines.find(context.id);
    }

    if (pipeline_flags & kPipelineBlend)
    {
        switch (src_blend)
        {
        case GL_SRC_ALPHA:
            pipeline_flags |= kPipelineBlendSrc_SrcAlpha;
            break;
        case GL_ONE_MINUS_DST_COLOR:
            pipeline_flags |= kPipelineBlendSrc_OneMinusDestColor;
            break;
        case GL_DST_COLOR:
            pipeline_flags |= kPipelineBlendSrc_DstColor;
            break;
        case GL_ZERO:
            pipeline_flags |= kPipelineBlendSrc_Zero;
            break;
        }

        switch (dst_blend)
        {
        case GL_ONE:
            pipeline_flags |= kPipelineBlendDst_One;
            break;
        case GL_ONE_MINUS_SRC_ALPHA:
            pipeline_flags |= kPipelineBlendDst_OneMinusSrcAlpha;
            break;
        case GL_SRC_COLOR:
            pipeline_flags |= kPipelineBlendDst_SrcColor;
            break;
        case GL_ZERO:
            pipeline_flags |= kPipelineBlendDst_Zero;
            break;
        }
    }

    std::unordered_map<uint32_t, uint32_t>::iterator pipeline_itr = context_itr->second.find(pipeline_flags);
    uint32_t                                         pipeline_id  = 0xFFFFFFFF;
    if (pipeline_itr == context_itr->second.end())
    {
        sg_pipeline_desc pipeline_desc;
        EPI_CLEAR_MEMORY(&pipeline_desc, sg_pipeline_desc, 1);
        if (!(pipeline_flags & kPipelineDepthTest))
            pipeline_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
        else if (pipeline_flags & kPipelineDepthGreater)
            pipeline_desc.depth.compare = SG_COMPAREFUNC_GREATER;
        else
            pipeline_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;

        if (pipeline_flags & kPipelineDepthWrite)
            pipeline_desc.depth.write_enabled = true;

        // Note: This is set in the r_state, and only this value, if culling issues, check if this ever changes
        // in external code.  I didn't want to eat up another state bit with this, and the state handling needs a reboot anyway
        pipeline_desc.face_winding = SG_FACEWINDING_CW;

        if (pipeline_flags & kPipelineCullBack)
        {
            pipeline_desc.cull_mode = SG_CULLMODE_BACK;
        }
        else if (pipeline_flags & kPipelineCullFront)
        {
            pipeline_desc.cull_mode = SG_CULLMODE_FRONT;
        }

        if (pipeline_flags & kPipelineBlend)
        {
            sg_blend_factor src_factor = SG_BLENDFACTOR_ZERO;
            sg_blend_factor dst_factor = SG_BLENDFACTOR_ZERO;

            pipeline_desc.colors[0].blend.enabled = true;

            switch (src_blend)
            {
            case GL_SRC_ALPHA:
                src_factor = SG_BLENDFACTOR_SRC_ALPHA;
                break;
            case GL_ONE_MINUS_DST_COLOR:
                src_factor = SG_BLENDFACTOR_ONE_MINUS_DST_COLOR;
                break;
            case GL_DST_COLOR:
                src_factor = SG_BLENDFACTOR_DST_COLOR;
                break;
            case GL_ZERO:
                src_factor = SG_BLENDFACTOR_ZERO;
                break;
            }

            switch (dst_blend)
            {
            case GL_ONE:
                dst_factor = SG_BLENDFACTOR_ONE;
                break;
            case GL_ONE_MINUS_SRC_ALPHA:
                dst_factor = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                break;
            case GL_SRC_COLOR:
                dst_factor = SG_BLENDFACTOR_SRC_COLOR;
                break;
            case GL_ZERO:
                dst_factor = SG_BLENDFACTOR_ZERO;
                break;
            }

            pipeline_desc.colors[0].blend.src_factor_rgb = src_factor;
            pipeline_desc.colors[0].blend.dst_factor_rgb = dst_factor;
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