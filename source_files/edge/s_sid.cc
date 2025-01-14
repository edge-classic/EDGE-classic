//----------------------------------------------------------------------------
//  EDGE cRSID Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023-2024 - The EDGE Team.
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

#include "s_sid.h"

#include "ddf_playlist.h"
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "i_sound.h"
#include "i_system.h"
#include "libcRSID.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

extern int sound_device_frequency;

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
    cRSID_C64instance      *C64;
    cRSID_SIDheader        *C64_song;
} ma_crsid;

static ma_result ma_crsid_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell,
                               void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig,
                               const ma_allocation_callbacks *pAllocationCallbacks, ma_crsid *pcrSID);
static ma_result ma_crsid_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                      const ma_allocation_callbacks *pAllocationCallbacks, ma_crsid *pcrSID);
static void      ma_crsid_uninit(ma_crsid *pcrSID, const ma_allocation_callbacks *pAllocationCallbacks);
static ma_result ma_crsid_read_pcm_frames(ma_crsid *pcrSID, void *pFramesOut, ma_uint64 frameCount,
                                          ma_uint64 *pFramesRead);
static ma_result ma_crsid_seek_to_pcm_frame(ma_crsid *pcrSID, ma_uint64 frameIndex);
static ma_result ma_crsid_get_data_format(ma_crsid *pcrSID, ma_format *pFormat, ma_uint32 *pChannels,
                                          ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);
static ma_result ma_crsid_get_cursor_in_pcm_frames(ma_crsid *pcrSID, ma_uint64 *pCursor);
static ma_result ma_crsid_get_length_in_pcm_frames(ma_crsid *pcrSID, ma_uint64 *pLength);

static ma_result ma_crsid_ds_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount,
                                  ma_uint64 *pFramesRead)
{
    return ma_crsid_read_pcm_frames((ma_crsid *)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_crsid_ds_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
    return ma_crsid_seek_to_pcm_frame((ma_crsid *)pDataSource, frameIndex);
}

static ma_result ma_crsid_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels,
                                             ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
    return ma_crsid_get_data_format((ma_crsid *)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap,
                                    channelMapCap);
}

static ma_result ma_crsid_ds_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
    return ma_crsid_get_cursor_in_pcm_frames((ma_crsid *)pDataSource, pCursor);
}

static ma_result ma_crsid_ds_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
    return ma_crsid_get_length_in_pcm_frames((ma_crsid *)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_crsid_ds_vtable = {ma_crsid_ds_read,
                                                     ma_crsid_ds_seek,
                                                     ma_crsid_ds_get_data_format,
                                                     ma_crsid_ds_get_cursor,
                                                     ma_crsid_ds_get_length,
                                                     NULL, /* onSetLooping */
                                                     0};

static ma_result ma_crsid_init_internal(const ma_decoding_backend_config *pConfig, ma_crsid *pcrSID)
{
    ma_result             result;
    ma_data_source_config dataSourceConfig;

    EPI_UNUSED(pConfig);

    if (pcrSID == NULL)
    {
        return MA_INVALID_ARGS;
    }

    EPI_CLEAR_MEMORY(pcrSID, ma_crsid, 1);
    pcrSID->format = ma_format_s16;

    dataSourceConfig        = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_crsid_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pcrSID->ds);
    if (result != MA_SUCCESS)
    {
        return result; /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

static ma_result ma_crsid_post_init(ma_crsid *pcrSID)
{
    EPI_ASSERT(pcrSID != NULL);

    pcrSID->channels   = 2;
    pcrSID->sampleRate = sound_device_frequency;
    cRSID_initSIDtune(pcrSID->C64, pcrSID->C64_song, 0);

    return MA_SUCCESS;
}

static ma_result ma_crsid_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell,
                               void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig,
                               const ma_allocation_callbacks *pAllocationCallbacks, ma_crsid *pcrSID)
{
    EPI_UNUSED(pAllocationCallbacks);

    ma_result result;

    result = ma_crsid_init_internal(pConfig, pcrSID);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    if (onRead == NULL || onSeek == NULL)
    {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pcrSID->onRead                = onRead;
    pcrSID->onSeek                = onSeek;
    pcrSID->onTell                = onTell;
    pcrSID->pReadSeekTellUserData = pReadSeekTellUserData;

    return MA_SUCCESS;
}

static ma_result ma_crsid_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                      const ma_allocation_callbacks *pAllocationCallbacks, ma_crsid *pcrSID)
{
    ma_result result;

    result = ma_crsid_init_internal(pConfig, pcrSID);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    EPI_UNUSED(pAllocationCallbacks);

    pcrSID->C64 = cRSID_init(sound_device_frequency);

    if (!pcrSID->C64)
    {
        return MA_ERROR;
    }

    pcrSID->C64_song = cRSID_processSIDfile(pcrSID->C64, (unsigned char *)pData, dataSize);

    if (!pcrSID->C64_song)
    {
        return MA_INVALID_DATA;
    }

    ma_crsid_post_init(pcrSID);

    return MA_SUCCESS;
}

static void ma_crsid_uninit(ma_crsid *pcrSID, const ma_allocation_callbacks *pAllocationCallbacks)
{
    EPI_UNUSED(pAllocationCallbacks);

    if (pcrSID == NULL)
    {
        return;
    }

    cRSID_initC64(pcrSID->C64);

    ma_data_source_uninit(&pcrSID->ds);
}

static ma_result ma_crsid_read_pcm_frames(ma_crsid *pcrSID, void *pFramesOut, ma_uint64 frameCount,
                                          ma_uint64 *pFramesRead)
{
    if (pFramesRead != NULL)
    {
        *pFramesRead = 0;
    }

    if (frameCount == 0)
    {
        return MA_INVALID_ARGS;
    }

    if (pcrSID == NULL)
    {
        return MA_INVALID_ARGS;
    }

    /* We always use floating point format. */
    ma_result result          = MA_SUCCESS; /* Must be initialized to MA_SUCCESS. */
    ma_uint64 totalFramesRead = 0;
    ma_format format;
    ma_uint32 channels;

    ma_crsid_get_data_format(pcrSID, &format, &channels, NULL, NULL, 0);

    if (format == ma_format_s16)
    {
        cRSID_generateSound(pcrSID->C64, (uint8_t *)pFramesOut, frameCount * 2 * sizeof(int16_t));
        totalFramesRead = frameCount;
    }
    else
    {
        result = MA_INVALID_ARGS;
    }

    pcrSID->cursor += totalFramesRead;

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

static ma_result ma_crsid_seek_to_pcm_frame(ma_crsid *pcrSID, ma_uint64 frameIndex)
{
    if (pcrSID == NULL || frameIndex != 0)
    {
        return MA_INVALID_ARGS;
    }

    cRSID_initSIDtune(pcrSID->C64, pcrSID->C64_song, 0);

    pcrSID->cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result ma_crsid_get_data_format(ma_crsid *pcrSID, ma_format *pFormat, ma_uint32 *pChannels,
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

    if (pcrSID == NULL)
    {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL)
    {
        *pFormat = pcrSID->format;
    }

    if (pChannels != NULL)
    {
        *pChannels = pcrSID->channels;
    }

    if (pSampleRate != NULL)
    {
        *pSampleRate = pcrSID->sampleRate;
    }

    if (pChannelMap != NULL)
    {
        ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, pcrSID->channels);
    }

    return MA_SUCCESS;
}

static ma_result ma_crsid_get_cursor_in_pcm_frames(ma_crsid *pcrSID, ma_uint64 *pCursor)
{
    if (pCursor == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0; /* Safety. */

    if (pcrSID == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = pcrSID->cursor;

    return MA_SUCCESS;
}

static ma_result ma_crsid_get_length_in_pcm_frames(ma_crsid *pcrSID, ma_uint64 *pLength)
{
    if (pLength == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pLength = 0; /* Safety. */

    if (pcrSID == NULL)
    {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init__crsid(void *pUserData, ma_read_proc onRead, ma_seek_proc onSeek,
                                                 ma_tell_proc onTell, void *pReadSeekTellUserData,
                                                 const ma_decoding_backend_config *pConfig,
                                                 const ma_allocation_callbacks    *pAllocationCallbacks,
                                                 ma_data_source                  **ppBackend)
{
    ma_result result;
    ma_crsid *pcrSID;

    EPI_UNUSED(pUserData);

    /* For now we're just allocating the decoder backend on the heap. */
    pcrSID = (ma_crsid *)ma_malloc(sizeof(*pcrSID), pAllocationCallbacks);
    if (pcrSID == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_crsid_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pcrSID);
    if (result != MA_SUCCESS)
    {
        ma_free(pcrSID, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pcrSID;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init_memory__crsid(void *pUserData, const void *pData, size_t dataSize,
                                                        const ma_decoding_backend_config *pConfig,
                                                        const ma_allocation_callbacks    *pAllocationCallbacks,
                                                        ma_data_source                  **ppBackend)
{
    ma_result result;
    ma_crsid *pcrSID;

    EPI_UNUSED(pUserData);

    /* For now we're just allocating the decoder backend on the heap. */
    pcrSID = (ma_crsid *)ma_malloc(sizeof(*pcrSID), pAllocationCallbacks);
    if (pcrSID == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_crsid_init_memory(pData, dataSize, pConfig, pAllocationCallbacks, pcrSID);
    if (result != MA_SUCCESS)
    {
        ma_free(pcrSID, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pcrSID;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__crsid(void *pUserData, ma_data_source *pBackend,
                                              const ma_allocation_callbacks *pAllocationCallbacks)
{
    ma_crsid *pcrSID = (ma_crsid *)pBackend;

    EPI_UNUSED(pUserData);

    ma_crsid_uninit(pcrSID, pAllocationCallbacks);
    ma_free(pcrSID, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_crsid = {ma_decoding_backend_init__crsid,
                                                                        NULL, // onInitFile()
                                                                        NULL, // onInitFileW()
                                                                        ma_decoding_backend_init_memory__crsid,
                                                                        ma_decoding_backend_uninit__crsid};

static ma_decoding_backend_vtable *custom_vtable = &g_ma_decoding_backend_vtable_crsid;

class SIDPlayer : public AbstractMusicPlayer
{
  public:
    SIDPlayer();
    ~SIDPlayer();

  private:
    enum Status
    {
        kNotLoaded,
        kPlaying,
        kPaused,
        kStopped
    };

    int  status_;
    bool looping_;

    ma_decoder sid_decoder_;
    ma_sound   sid_stream_;

  public:
    bool OpenMemory(uint8_t *data, int length);

    virtual void Close(void);

    virtual void Play(bool loop);
    virtual void Stop(void);

    virtual void Pause(void);
    virtual void Resume(void);

    virtual void Ticker(void);
};

//----------------------------------------------------------------------------

SIDPlayer::SIDPlayer() : status_(kNotLoaded)
{
    EPI_CLEAR_MEMORY(&sid_decoder_, ma_decoder, 1);
    EPI_CLEAR_MEMORY(&sid_stream_, ma_sound, 1);
}

SIDPlayer::~SIDPlayer()
{
    Close();
}

bool SIDPlayer::OpenMemory(uint8_t *data, int length)
{
    if (status_ != kNotLoaded)
        Close();

    ma_decoder_config decode_config      = ma_decoder_config_init_default();
    decode_config.format                 = ma_format_s16;
    decode_config.customBackendCount     = 1;
    decode_config.pCustomBackendUserData = NULL;
    decode_config.ppCustomBackendVTables = &custom_vtable;

    if (ma_decoder_init_memory(data, length, &decode_config, &sid_decoder_) != MA_SUCCESS)
    {
        LogWarning("Failed to load SID music (corrupt sid?)\n");
        return false;
    }

    if (ma_sound_init_from_data_source(&music_engine, &sid_decoder_,
                                       MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_UNKNOWN_LENGTH | MA_SOUND_FLAG_STREAM |
                                           MA_SOUND_FLAG_NO_SPATIALIZATION,
                                       NULL, &sid_stream_) != MA_SUCCESS)
    {
        ma_decoder_uninit(&sid_decoder_);
        LogWarning("Failed to load SID music (corrupt sid?)\n");
        return false;
    }

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

    ma_sound_uninit(&sid_stream_);

    ma_decoder_uninit(&sid_decoder_);

    status_ = kNotLoaded;
}

void SIDPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    ma_sound_stop(&sid_stream_);

    status_ = kPaused;
}

void SIDPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    ma_sound_start(&sid_stream_);

    status_ = kPlaying;
}

void SIDPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    looping_ = loop;

    ma_sound_set_looping(&sid_stream_, looping_ ? MA_TRUE : MA_FALSE);

    // Let 'er rip (maybe)
    if (playing_movie)
        status_ = kPaused;
    else
    {
        status_ = kPlaying;
        ma_sound_start(&sid_stream_);
    }
}

void SIDPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    ma_sound_stop(&sid_stream_);

    ma_decoder_seek_to_pcm_frame(&sid_decoder_, 0);

    status_ = kStopped;
}

void SIDPlayer::Ticker()
{
    ma_engine_set_volume(&music_engine, music_volume.f_ * 0.25f);

    if (status_ == kPlaying)
    {
        if (pc_speaker_mode)
            Stop();
        if (ma_sound_at_end(&sid_stream_)) // This should only be true if finished and not set to looping
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
        return NULL;
    }

    // cRSID retains the data after initializing the track
    delete[] data;

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
