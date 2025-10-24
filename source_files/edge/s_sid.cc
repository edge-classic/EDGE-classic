//----------------------------------------------------------------------------
//  EDGE SID Music Player
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
#include "libcRSID.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

extern int sound_device_frequency;

static ma_decoder sid_decoder;
static ma_sound   sid_stream;
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
    cRSID_C64instance      *SID;
    cRSID_SIDheader        *SIDheader;
} ma_sid;

static ma_result ma_sid_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData,
                             const ma_decoding_backend_config *pConfig,
                             const ma_allocation_callbacks *pAllocationCallbacks, ma_sid *pSID);
static ma_result ma_sid_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                    const ma_allocation_callbacks *pAllocationCallbacks, ma_sid *pSID);
static void      ma_sid_uninit(ma_sid *pSID, const ma_allocation_callbacks *pAllocationCallbacks);
static ma_result ma_sid_read_pcm_frames(ma_sid *pSID, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead);
static ma_result ma_sid_seek_to_pcm_frame(ma_sid *pSID, ma_uint64 frameIndex);
static ma_result ma_sid_get_data_format(const ma_sid *pSID, ma_format *pFormat, ma_uint32 *pChannels,
                                        ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);
static ma_result ma_sid_get_cursor_in_pcm_frames(const ma_sid *pSID, ma_uint64 *pCursor);
static ma_result ma_sid_get_length_in_pcm_frames(const ma_sid *pSID, ma_uint64 *pLength);

static ma_result ma_sid_ds_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount,
                                ma_uint64 *pFramesRead)
{
    return ma_sid_read_pcm_frames((ma_sid *)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_sid_ds_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
    return ma_sid_seek_to_pcm_frame((ma_sid *)pDataSource, frameIndex);
}

static ma_result ma_sid_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels,
                                           ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
    return ma_sid_get_data_format((ma_sid *)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_sid_ds_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
    return ma_sid_get_cursor_in_pcm_frames((ma_sid *)pDataSource, pCursor);
}

static ma_result ma_sid_ds_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
    return ma_sid_get_length_in_pcm_frames((ma_sid *)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_sid_ds_vtable = {ma_sid_ds_read,
                                                   ma_sid_ds_seek,
                                                   ma_sid_ds_get_data_format,
                                                   ma_sid_ds_get_cursor,
                                                   ma_sid_ds_get_length,
                                                   NULL, /* onSetLooping */
                                                   0};

static ma_result ma_sid_init_internal(const ma_decoding_backend_config *pConfig, ma_sid *pSID)
{
    ma_result             result;
    ma_data_source_config dataSourceConfig;

    EPI_UNUSED(pConfig);

    if (pSID == NULL)
    {
        return MA_INVALID_ARGS;
    }

    EPI_CLEAR_MEMORY(pSID, ma_sid, 1);
    pSID->format = ma_format_s16;

    dataSourceConfig        = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_sid_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pSID->ds);
    if (result != MA_SUCCESS)
    {
        return result; /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

static ma_result ma_sid_post_init(ma_sid *pSID)
{
    EPI_ASSERT(pSID != NULL);

    pSID->channels   = 2;
    pSID->sampleRate = sound_device_frequency;
    cRSID_initSIDtune(pSID->SID, pSID->SIDheader, 0);

    return MA_SUCCESS;
}

static ma_result ma_sid_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData,
                             const ma_decoding_backend_config *pConfig,
                             const ma_allocation_callbacks *pAllocationCallbacks, ma_sid *pSID)
{
    EPI_UNUSED(pAllocationCallbacks);

    ma_result result;

    result = ma_sid_init_internal(pConfig, pSID);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    if (onRead == NULL || onSeek == NULL)
    {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pSID->onRead                = onRead;
    pSID->onSeek                = onSeek;
    pSID->onTell                = onTell;
    pSID->pReadSeekTellUserData = pReadSeekTellUserData;

    return MA_SUCCESS;
}

static ma_result ma_sid_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                    const ma_allocation_callbacks *pAllocationCallbacks, ma_sid *pSID)
{
    ma_result result;

    result = ma_sid_init_internal(pConfig, pSID);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    EPI_UNUSED(pAllocationCallbacks);

    pSID->SID = cRSID_init(sound_device_frequency, 0);

    pSID->SIDheader = cRSID_processSIDbuffer(pSID->SID, (unsigned char *)pData, dataSize);

    if (!pSID->SIDheader)
    {
        LogWarning("SID: failure to load song!\n");
        return MA_INVALID_DATA;
    }

    ma_sid_post_init(pSID);

    return MA_SUCCESS;
}

static void ma_sid_uninit(ma_sid *pSID, const ma_allocation_callbacks *pAllocationCallbacks)
{
    EPI_UNUSED(pAllocationCallbacks);

    if (pSID == NULL)
    {
        return;
    }

    ma_data_source_uninit(&pSID->ds);
}

static ma_result ma_sid_read_pcm_frames(ma_sid *pSID, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
    if (pFramesRead != NULL)
    {
        *pFramesRead = 0;
    }

    if (frameCount == 0)
    {
        return MA_INVALID_ARGS;
    }

    if (pSID == NULL)
    {
        return MA_INVALID_ARGS;
    }

    ma_result result          = MA_SUCCESS; /* Must be initialized to MA_SUCCESS. */
    ma_uint64 totalFramesRead = 0;
    ma_format format;
    ma_uint32 channels;

    ma_sid_get_data_format(pSID, &format, &channels, NULL, NULL, 0);

    if (format == ma_format_s16)
    {
        cRSID_generateSound(pSID->SID, (unsigned char *)pFramesOut, (unsigned short)frameCount * 2 * sizeof(int16_t));
        totalFramesRead = frameCount;
    }
    else
    {
        result = MA_INVALID_ARGS;
    }

    pSID->cursor += totalFramesRead;

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

static ma_result ma_sid_seek_to_pcm_frame(ma_sid *pSID, ma_uint64 frameIndex)
{
    if (pSID == NULL || frameIndex != 0)
    {
        return MA_INVALID_ARGS;
    }

    cRSID_initSIDtune(pSID->SID, pSID->SIDheader, 0);

    pSID->cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result ma_sid_get_data_format(const ma_sid *pSID, ma_format *pFormat, ma_uint32 *pChannels,
                                        ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
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

    if (pSID == NULL)
    {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL)
    {
        *pFormat = pSID->format;
    }

    if (pChannels != NULL)
    {
        *pChannels = pSID->channels;
    }

    if (pSampleRate != NULL)
    {
        *pSampleRate = pSID->sampleRate;
    }

    if (pChannelMap != NULL)
    {
        ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, pSID->channels);
    }

    return MA_SUCCESS;
}

static ma_result ma_sid_get_cursor_in_pcm_frames(const ma_sid *pSID, ma_uint64 *pCursor)
{
    if (pCursor == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0; /* Safety. */

    if (pSID == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = pSID->cursor;

    return MA_SUCCESS;
}

static ma_result ma_sid_get_length_in_pcm_frames(const ma_sid *pSID, ma_uint64 *pLength)
{
    if (pLength == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pLength = 0; /* Safety. */

    if (pSID == NULL)
    {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init__sid(void *pUserData, ma_read_proc onRead, ma_seek_proc onSeek,
                                               ma_tell_proc onTell, void *pReadSeekTellUserData,
                                               const ma_decoding_backend_config *pConfig,
                                               const ma_allocation_callbacks    *pAllocationCallbacks,
                                               ma_data_source                  **ppBackend)
{
    ma_result result;
    ma_sid   *pSID;

    EPI_UNUSED(pUserData);

    /* For now we're just allocating the decoder backend on the heap. */
    pSID = (ma_sid *)ma_malloc(sizeof(*pSID), pAllocationCallbacks);
    if (pSID == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_sid_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pSID);
    if (result != MA_SUCCESS)
    {
        ma_free(pSID, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pSID;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init_memory__sid(void *pUserData, const void *pData, size_t dataSize,
                                                      const ma_decoding_backend_config *pConfig,
                                                      const ma_allocation_callbacks    *pAllocationCallbacks,
                                                      ma_data_source                  **ppBackend)
{
    ma_result result;
    ma_sid   *pSID;

    EPI_UNUSED(pUserData);

    /* For now we're just allocating the decoder backend on the heap. */
    pSID = (ma_sid *)ma_malloc(sizeof(*pSID), pAllocationCallbacks);
    if (pSID == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_sid_init_memory(pData, dataSize, pConfig, pAllocationCallbacks, pSID);
    if (result != MA_SUCCESS)
    {
        ma_free(pSID, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pSID;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__sid(void *pUserData, ma_data_source *pBackend,
                                            const ma_allocation_callbacks *pAllocationCallbacks)
{
    ma_sid *pSID = (ma_sid *)pBackend;

    EPI_UNUSED(pUserData);

    ma_sid_uninit(pSID, pAllocationCallbacks);
    ma_free(pSID, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_sid = {ma_decoding_backend_init__sid,
                                                                      NULL, // onInitFile()
                                                                      NULL, // onInitFileW()
                                                                      ma_decoding_backend_init_memory__sid,
                                                                      ma_decoding_backend_uninit__sid};

static ma_decoding_backend_vtable *custom_vtable = &g_ma_decoding_backend_vtable_sid;

class SIDPlayer : public AbstractMusicPlayer
{
  public:
    SIDPlayer();
    ~SIDPlayer() override;

    bool OpenMemory(const uint8_t *data, int length);

    void Close(void) override;

    void Play(bool loop) override;
    void Stop(void) override;

    void Pause(void) override;
    void Resume(void) override;

    void Ticker(void) override;
};

//----------------------------------------------------------------------------

SIDPlayer::SIDPlayer()
{
    status_ = kNotLoaded;
}

SIDPlayer::~SIDPlayer()
{
    Close();
}

bool SIDPlayer::OpenMemory(const uint8_t *data, int length)
{
    if (status_ != kNotLoaded)
        Close();

    ma_decoder_config decode_config      = ma_decoder_config_init_default();
    decode_config.format                 = ma_format_s16;
    decode_config.customBackendCount     = 1;
    decode_config.pCustomBackendUserData = NULL;
    decode_config.ppCustomBackendVTables = &custom_vtable;

    if (ma_decoder_init_memory(data, length, &decode_config, &sid_decoder) != MA_SUCCESS)
    {
        LogWarning("Failed to load tracker music\n");
        return false;
    }

    if (ma_sound_init_from_data_source(&sound_engine, &sid_decoder,
                                       MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_UNKNOWN_LENGTH | MA_SOUND_FLAG_STREAM |
                                           MA_SOUND_FLAG_NO_SPATIALIZATION,
                                       NULL, &sid_stream) != MA_SUCCESS)
    {
        ma_decoder_uninit(&sid_decoder);
        LogWarning("Failed to load tracker music\n");
        return false;
    }

    ma_node_attach_output_bus(&sid_stream, 0, &music_node, 0);

    // Loaded, but not playing
    status_ = kStopped;

    return true;
}

void SIDPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    Stop();

    ma_sound_uninit(&sid_stream);

    ma_decoder_uninit(&sid_decoder);

    status_ = kNotLoaded;
}

void SIDPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    ma_sound_stop(&sid_stream);

    status_ = kPaused;
}

void SIDPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    ma_sound_start(&sid_stream);

    status_ = kPlaying;
}

void SIDPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    looping_ = loop;

    ma_sound_set_looping(&sid_stream, looping_ ? MA_TRUE : MA_FALSE);

    // Let 'er rip (maybe)
    if (playing_movie)
        status_ = kPaused;
    else
    {
        status_ = kPlaying;
        ma_sound_start(&sid_stream);
    }
}

void SIDPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    ma_sound_stop(&sid_stream);

    status_ = kStopped;
}

void SIDPlayer::Ticker()
{
    if (status_ == kPlaying)
    {
        if (pc_speaker_mode)
            Stop();
        if (ma_sound_at_end(&sid_stream)) // This should only be true if finished and not set to looping
            Stop();
    }
}

//----------------------------------------------------------------------------

AbstractMusicPlayer *PlaySIDMusic(uint8_t *data, int length, bool looping)
{
    SIDPlayer *player = new SIDPlayer();

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
