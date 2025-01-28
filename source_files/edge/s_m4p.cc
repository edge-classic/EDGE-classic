//----------------------------------------------------------------------------
//  EDGE Mod4Play (Tracker Module) Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 - The EDGE Team.
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

#include "ddf_playlist.h"
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "i_sound.h"
#include "m4p.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

extern int sound_device_frequency;

static ma_decoder m4p_decoder;
static ma_sound   m4p_stream;
typedef struct
{
    ma_data_source_base     ds;
    ma_read_proc            onRead;
    ma_seek_proc            onSeek;
    ma_tell_proc            onTell;
    void                   *pReadSeekTellUserData;
    ma_allocation_callbacks allocationCallbacks;
    ma_format               format;
    ma_uint32               channels;
    ma_uint32               sampleRate;
    ma_uint64               cursor;
} ma_m4p;

static ma_result ma_m4p_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData,
                             const ma_decoding_backend_config *pConfig,
                             const ma_allocation_callbacks *pAllocationCallbacks, ma_m4p *pM4P);
static ma_result ma_m4p_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                    const ma_allocation_callbacks *pAllocationCallbacks, ma_m4p *pM4P);
static void      ma_m4p_uninit(ma_m4p *pM4P, const ma_allocation_callbacks *pAllocationCallbacks);
static ma_result ma_m4p_read_pcm_frames(ma_m4p *pM4P, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead);
static ma_result ma_m4p_seek_to_pcm_frame(ma_m4p *pM4P, ma_uint64 frameIndex);
static ma_result ma_m4p_get_data_format(const ma_m4p *pM4P, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate,
                                        ma_channel *pChannelMap, size_t channelMapCap);
static ma_result ma_m4p_get_cursor_in_pcm_frames(const ma_m4p *pM4P, ma_uint64 *pCursor);
static ma_result ma_m4p_get_length_in_pcm_frames(const ma_m4p *pM4P, ma_uint64 *pLength);

static ma_result ma_m4p_ds_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount,
                                ma_uint64 *pFramesRead)
{
    return ma_m4p_read_pcm_frames((ma_m4p *)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_m4p_ds_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
    return ma_m4p_seek_to_pcm_frame((ma_m4p *)pDataSource, frameIndex);
}

static ma_result ma_m4p_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels,
                                           ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
    return ma_m4p_get_data_format((ma_m4p *)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_m4p_ds_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
    return ma_m4p_get_cursor_in_pcm_frames((ma_m4p *)pDataSource, pCursor);
}

static ma_result ma_m4p_ds_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
    return ma_m4p_get_length_in_pcm_frames((ma_m4p *)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_m4p_ds_vtable = {ma_m4p_ds_read,
                                                   ma_m4p_ds_seek,
                                                   ma_m4p_ds_get_data_format,
                                                   ma_m4p_ds_get_cursor,
                                                   ma_m4p_ds_get_length,
                                                   NULL, /* onSetLooping */
                                                   0};

static ma_result ma_m4p_init_internal(const ma_decoding_backend_config *pConfig, ma_m4p *pM4P)
{
    ma_result             result;
    ma_data_source_config dataSourceConfig;

    EPI_UNUSED(pConfig);

    if (pM4P == NULL)
    {
        return MA_INVALID_ARGS;
    }

    EPI_CLEAR_MEMORY(pM4P, ma_m4p, 1);
    pM4P->format = ma_format_s16;

    dataSourceConfig        = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_m4p_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pM4P->ds);
    if (result != MA_SUCCESS)
    {
        return result; /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

static ma_result ma_m4p_post_init(ma_m4p *pM4P)
{
    EPI_ASSERT(pM4P != NULL);

    pM4P->channels   = 2;
    pM4P->sampleRate = HMM_MIN(64000, sound_device_frequency);

    m4p_PlaySong();

    return MA_SUCCESS;
}

static ma_result ma_m4p_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData,
                             const ma_decoding_backend_config *pConfig,
                             const ma_allocation_callbacks *pAllocationCallbacks, ma_m4p *pM4P)
{
    EPI_UNUSED(pAllocationCallbacks);

    ma_result result;

    result = ma_m4p_init_internal(pConfig, pM4P);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    if (onRead == NULL || onSeek == NULL)
    {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pM4P->onRead                = onRead;
    pM4P->onSeek                = onSeek;
    pM4P->onTell                = onTell;
    pM4P->pReadSeekTellUserData = pReadSeekTellUserData;

    return MA_SUCCESS;
}

static ma_result ma_m4p_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                    const ma_allocation_callbacks *pAllocationCallbacks, ma_m4p *pM4P)
{
    ma_result result;

    result = ma_m4p_init_internal(pConfig, pM4P);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    EPI_UNUSED(pAllocationCallbacks);

    if (!m4p_LoadFromData((uint8_t *)pData, dataSize, HMM_MIN(64000, sound_device_frequency), 1024))
    {
        LogWarning("M4P: failure to load song!\n");
        return MA_INVALID_DATA;
    }

    ma_m4p_post_init(pM4P);

    return MA_SUCCESS;
}

static void ma_m4p_uninit(ma_m4p *pM4P, const ma_allocation_callbacks *pAllocationCallbacks)
{
    EPI_UNUSED(pAllocationCallbacks);

    if (pM4P == NULL)
    {
        return;
    }

    m4p_Close();
    m4p_FreeSong();

    ma_data_source_uninit(&pM4P->ds);
}

static ma_result ma_m4p_read_pcm_frames(ma_m4p *pM4P, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
    if (pFramesRead != NULL)
    {
        *pFramesRead = 0;
    }

    if (frameCount == 0)
    {
        return MA_INVALID_ARGS;
    }

    if (pM4P == NULL)
    {
        return MA_INVALID_ARGS;
    }

    ma_result result          = MA_SUCCESS; /* Must be initialized to MA_SUCCESS. */
    ma_uint64 totalFramesRead = 0;
    ma_format format;
    ma_uint32 channels;

    ma_m4p_get_data_format(pM4P, &format, &channels, NULL, NULL, 0);

    if (format == ma_format_s16)
    {
        m4p_GenerateSamples((int16_t *)pFramesOut, frameCount);
        totalFramesRead = frameCount;
    }
    else
    {
        result = MA_INVALID_ARGS;
    }

    pM4P->cursor += totalFramesRead;

    if (totalFramesRead == 0)
    {
        result = MA_AT_END;
    }

    if (pFramesRead != NULL)
    {
        *pFramesRead = totalFramesRead;
    }

    if (result == MA_SUCCESS && totalFramesRead == 0)
    {
        result = MA_AT_END;
    }

    return result;
}

static ma_result ma_m4p_seek_to_pcm_frame(ma_m4p *pM4P, ma_uint64 frameIndex)
{
    if (pM4P == NULL || frameIndex != 0)
    {
        return MA_INVALID_ARGS;
    }

    m4p_Stop();
    m4p_PlaySong();

    pM4P->cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result ma_m4p_get_data_format(const ma_m4p *pM4P, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate,
                                        ma_channel *pChannelMap, size_t channelMapCap)
{
    /* Defaults for safety. */
    if (pFormat != NULL)
    {
        *pFormat = ma_format_unknown;
    }
    if (pChannels != NULL)
    {
        *pChannels = 0;
    }
    if (pSampleRate != NULL)
    {
        *pSampleRate = 0;
    }
    if (pChannelMap != NULL)
    {
        EPI_CLEAR_MEMORY(pChannelMap, ma_channel, channelMapCap);
    }

    if (pM4P == NULL)
    {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL)
    {
        *pFormat = pM4P->format;
    }

    if (pChannels != NULL)
    {
        *pChannels = pM4P->channels;
    }

    if (pSampleRate != NULL)
    {
        *pSampleRate = pM4P->sampleRate;
    }

    if (pChannelMap != NULL)
    {
        ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, pM4P->channels);
    }

    return MA_SUCCESS;
}

static ma_result ma_m4p_get_cursor_in_pcm_frames(const ma_m4p *pM4P, ma_uint64 *pCursor)
{
    if (pCursor == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0; /* Safety. */

    if (pM4P == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = pM4P->cursor;

    return MA_SUCCESS;
}

static ma_result ma_m4p_get_length_in_pcm_frames(const ma_m4p *pM4P, ma_uint64 *pLength)
{
    if (pLength == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pLength = 0; /* Safety. */

    if (pM4P == NULL)
    {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init__m4p(void *pUserData, ma_read_proc onRead, ma_seek_proc onSeek,
                                               ma_tell_proc onTell, void *pReadSeekTellUserData,
                                               const ma_decoding_backend_config *pConfig,
                                               const ma_allocation_callbacks    *pAllocationCallbacks,
                                               ma_data_source                  **ppBackend)
{
    ma_result result;
    ma_m4p   *pM4P;

    EPI_UNUSED(pUserData);

    /* For now we're just allocating the decoder backend on the heap. */
    pM4P = (ma_m4p *)ma_malloc(sizeof(*pM4P), pAllocationCallbacks);
    if (pM4P == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_m4p_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pM4P);
    if (result != MA_SUCCESS)
    {
        ma_free(pM4P, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pM4P;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init_memory__m4p(void *pUserData, const void *pData, size_t dataSize,
                                                      const ma_decoding_backend_config *pConfig,
                                                      const ma_allocation_callbacks    *pAllocationCallbacks,
                                                      ma_data_source                  **ppBackend)
{
    ma_result result;
    ma_m4p   *pM4P;

    EPI_UNUSED(pUserData);

    /* For now we're just allocating the decoder backend on the heap. */
    pM4P = (ma_m4p *)ma_malloc(sizeof(*pM4P), pAllocationCallbacks);
    if (pM4P == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_m4p_init_memory(pData, dataSize, pConfig, pAllocationCallbacks, pM4P);
    if (result != MA_SUCCESS)
    {
        ma_free(pM4P, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pM4P;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__m4p(void *pUserData, ma_data_source *pBackend,
                                            const ma_allocation_callbacks *pAllocationCallbacks)
{
    ma_m4p *pM4P = (ma_m4p *)pBackend;

    EPI_UNUSED(pUserData);

    ma_m4p_uninit(pM4P, pAllocationCallbacks);
    ma_free(pM4P, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_m4p = {ma_decoding_backend_init__m4p,
                                                                      NULL, // onInitFile()
                                                                      NULL, // onInitFileW()
                                                                      ma_decoding_backend_init_memory__m4p,
                                                                      ma_decoding_backend_uninit__m4p};

static ma_decoding_backend_vtable *custom_vtable = &g_ma_decoding_backend_vtable_m4p;

class M4PPlayer : public AbstractMusicPlayer
{
  public:
    M4PPlayer();
    ~M4PPlayer() override;

    bool OpenMemory(const uint8_t *data, int length);

    void Close(void) override;

    void Play(bool loop) override;
    void Stop(void) override;

    void Pause(void) override;
    void Resume(void) override;

    void Ticker(void) override;
};

//----------------------------------------------------------------------------

M4PPlayer::M4PPlayer()
{
    status_ = kNotLoaded;
}

M4PPlayer::~M4PPlayer()
{
    Close();
}

bool M4PPlayer::OpenMemory(const uint8_t *data, int length)
{
    if (status_ != kNotLoaded)
        Close();

    ma_decoder_config decode_config      = ma_decoder_config_init_default();
    decode_config.format                 = ma_format_s16;
    decode_config.customBackendCount     = 1;
    decode_config.pCustomBackendUserData = NULL;
    decode_config.ppCustomBackendVTables = &custom_vtable;

    if (ma_decoder_init_memory(data, length, &decode_config, &m4p_decoder) != MA_SUCCESS)
    {
        LogWarning("Failed to load tracker music\n");
        return false;
    }

    if (ma_sound_init_from_data_source(&music_engine, &m4p_decoder,
                                       MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_UNKNOWN_LENGTH | MA_SOUND_FLAG_STREAM |
                                           MA_SOUND_FLAG_NO_SPATIALIZATION,
                                       NULL, &m4p_stream) != MA_SUCCESS)
    {
        ma_decoder_uninit(&m4p_decoder);
        LogWarning("Failed to load tracker music\n");
        return false;
    }

    // Loaded, but not playing
    status_ = kStopped;

    return true;
}

void M4PPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    Stop();

    ma_sound_uninit(&m4p_stream);

    ma_decoder_uninit(&m4p_decoder);

    status_ = kNotLoaded;
}

void M4PPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    ma_sound_stop(&m4p_stream);

    status_ = kPaused;
}

void M4PPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    ma_sound_start(&m4p_stream);

    status_ = kPlaying;
}

void M4PPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    looping_ = loop;

    ma_sound_set_looping(&m4p_stream, looping_ ? MA_TRUE : MA_FALSE);

    // Let 'er rip (maybe)
    if (playing_movie)
        status_ = kPaused;
    else
    {
        status_ = kPlaying;
        ma_sound_start(&m4p_stream);
    }
}

void M4PPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    ma_sound_set_volume(&m4p_stream, 0);
    ma_sound_stop(&m4p_stream);

    status_ = kStopped;
}

void M4PPlayer::Ticker()
{
    if (status_ == kPlaying)
    {
        ma_engine_set_volume(&music_engine, music_volume.f_ * 0.25f);

        if (pc_speaker_mode)
            Stop();
        if (ma_sound_at_end(&m4p_stream)) // This should only be true if finished and not set to looping
            Stop();
    }
}

//----------------------------------------------------------------------------

AbstractMusicPlayer *PlayM4PMusic(uint8_t *data, int length, bool looping)
{
    M4PPlayer *player = new M4PPlayer();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    delete[] data;

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
