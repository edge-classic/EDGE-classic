/* Include miniaudio_freeverb.h after miniaudio.h */
#ifndef miniaudio_freeverb_h
#define miniaudio_freeverb_h

#include "verblib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
The reverb node has one input and one output.
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_uint32 channels;         /* The number of channels of the source, which will be the same as the output. Must be 1 or 2. */
    ma_uint32 sampleRate;
    float roomSize;
    float damping;
    float width;
    float wetVolume;
    float dryVolume;
    float mode;
} ma_freeverb_node_config;

MA_API ma_freeverb_node_config ma_freeverb_node_config_init(ma_uint32 channels, ma_uint32 sampleRate);


typedef struct
{
    ma_node_base baseNode;
    verblib reverb;
    ma_atomic_float  roomSize;
    ma_atomic_float  damping;
    ma_atomic_float  width;
    ma_atomic_float  wetVolume;
    ma_atomic_float  dryVolume;
    ma_atomic_float  mode;
    ma_atomic_float  gain;
    ma_atomic_bool32 pending_change;
} ma_freeverb_node;

MA_API ma_result ma_freeverb_node_init(ma_node_graph* pNodeGraph, const ma_freeverb_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_freeverb_node* pReverbNode);
MA_API void ma_freeverb_node_uninit(ma_freeverb_node* pReverbNode, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API void ma_freeverb_update_verb(ma_freeverb_node* pReverbNode, const float *room_size, const float *damping, const float *wet, const float *dry, const float *width, const float *gain);

#ifdef __cplusplus
}
#endif

#ifdef MINIAUDIO_FREEVERB_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

MA_API ma_freeverb_node_config ma_freeverb_node_config_init(ma_uint32 channels, ma_uint32 sampleRate)
{
    ma_freeverb_node_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_node_config_init();  /* Input and output channels will be set in ma_freeverb_node_init(). */
    config.channels   = channels;
    config.sampleRate = sampleRate;
    config.roomSize   = verblib_initialroom;
    config.damping    = verblib_initialdamp;
    config.width      = verblib_initialwidth;
    config.wetVolume  = verblib_initialwet;
    config.dryVolume  = verblib_initialdry;
    config.mode       = verblib_initialmode;

    return config;
}

static void ma_freeverb_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_freeverb_node* pReverbNode = (ma_freeverb_node*)pNode;

    (void)pFrameCountIn;

    if (ma_atomic_bool32_get(&pReverbNode->pending_change) == MA_TRUE)
    {
        verblib *verb = &pReverbNode->reverb;
        verblib_set_room_size(verb, ma_atomic_float_get(&pReverbNode->roomSize));
        verblib_set_damping(verb, ma_atomic_float_get(&pReverbNode->damping));
        verblib_set_wet(verb, ma_atomic_float_get(&pReverbNode->wetVolume));
        verblib_set_dry(verb, ma_atomic_float_get(&pReverbNode->dryVolume));
        verblib_set_width(verb, ma_atomic_float_get(&pReverbNode->width));
        verblib_set_gain(verb, ma_atomic_float_get(&pReverbNode->gain));
        ma_atomic_bool32_set(&pReverbNode->pending_change, MA_FALSE);
    }

    verblib_process(&pReverbNode->reverb, ppFramesIn[0], ppFramesOut[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_freeverb_node_vtable =
{
    ma_freeverb_node_process_pcm_frames,
    NULL,
    1,  /* 1 input channel. */
    1,  /* 1 output channel. */
    MA_NODE_FLAG_CONTINUOUS_PROCESSING  /* Reverb requires continuous processing to ensure the tail get's processed. */
};

MA_API ma_result ma_freeverb_node_init(ma_node_graph* pNodeGraph, const ma_freeverb_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_freeverb_node* pReverbNode)
{
    ma_result result;
    ma_node_config baseConfig;

    if (pReverbNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pReverbNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (verblib_initialize(&pReverbNode->reverb, (unsigned long)pConfig->sampleRate, (unsigned int)pConfig->channels) == 0) {
        return MA_INVALID_ARGS;
    }

    baseConfig = pConfig->nodeConfig;
    baseConfig.vtable          = &g_ma_freeverb_node_vtable;
    baseConfig.pInputChannels  = &pConfig->channels;
    baseConfig.pOutputChannels = &pConfig->channels;

    result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pReverbNode->baseNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    ma_atomic_float_set(&pReverbNode->damping, pConfig->damping);
    ma_atomic_float_set(&pReverbNode->dryVolume, pConfig->dryVolume);
    ma_atomic_float_set(&pReverbNode->mode, pConfig->mode);
    ma_atomic_float_set(&pReverbNode->gain, verblib_fixedgain);
    ma_atomic_float_set(&pReverbNode->width, pConfig->width);
    ma_atomic_float_set(&pReverbNode->roomSize, pConfig->roomSize);
    ma_atomic_float_set(&pReverbNode->wetVolume, pConfig->wetVolume);
    ma_atomic_bool32_set(&pReverbNode->pending_change, MA_FALSE);

    return MA_SUCCESS;
}

MA_API void ma_freeverb_node_uninit(ma_freeverb_node* pReverbNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    /* The base node is always uninitialized first. */
    ma_node_uninit(pReverbNode, pAllocationCallbacks);
}

MA_API void ma_freeverb_update_verb(ma_freeverb_node* pReverbNode, const float *room_size, const float *damping, const float *wet, const float *dry, const float *width, const float *gain)
{
    if (pReverbNode == NULL)
        return;
   
    if (room_size != NULL)
        ma_atomic_float_set(&pReverbNode->roomSize, *room_size);
    if (damping != NULL)
        ma_atomic_float_set(&pReverbNode->damping, *damping);
    if (wet != NULL)
        ma_atomic_float_set(&pReverbNode->wetVolume, *wet);
    if (dry != NULL)
        ma_atomic_float_set(&pReverbNode->dryVolume, *dry);
    if (width != NULL)
        ma_atomic_float_set(&pReverbNode->width, *width);
    if (gain != NULL)
        ma_atomic_float_set(&pReverbNode->gain, *gain);

    ma_atomic_bool32_set(&pReverbNode->pending_change, MA_TRUE);
}

#ifdef __cplusplus
}
#endif

#endif /* MINIAUDIO_FREEVERB_IMPLEMENTATION */

#endif  /* miniaudio_freeverb_h */
