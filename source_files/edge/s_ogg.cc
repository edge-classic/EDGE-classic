//----------------------------------------------------------------------------
//  EDGE OGG Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2024 The EDGE Team.
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

#include "s_ogg.h"

#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "i_sound.h"
// clang-format off
#define OV_EXCLUDE_STATIC_CALLBACKS
#include "minivorbis.h"
// clang-format on
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"

static ma_decoder ogg_decoder;
static ma_sound   ogg_stream;

static size_t ogg_epi_memread(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    epi::MemFile *d = (epi::MemFile *)datasource;
    return d->Read(ptr, size * nmemb) / size;
}

static int ogg_epi_memseek(void *datasource, ogg_int64_t offset, int whence)
{
    epi::MemFile *d = (epi::MemFile *)datasource;

    switch (whence)
    {
    case SEEK_SET: {
        return d->Seek(offset, epi::File::kSeekpointStart) ? 0 : -1;
    }
    case SEEK_CUR: {
        return d->Seek(offset, epi::File::kSeekpointCurrent) ? 0 : -1;
    }
    case SEEK_END: {
        return d->Seek(-offset, epi::File::kSeekpointEnd) ? 0 : -1;
    }
    default: {
        return -1;
    } // WTF?
    }
}

static int ogg_epi_memclose(void *datasource)
{
    // we don't free the data here
    EPI_UNUSED(datasource);
    return 0;
}

static long ogg_epi_memtell(void *datasource)
{
    epi::MemFile *d = (epi::MemFile *)datasource;
    return d->GetPosition();
}

static constexpr ov_callbacks ogg_epi_callbacks = {ogg_epi_memread, ogg_epi_memseek, ogg_epi_memclose, ogg_epi_memtell};

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
    epi::MemFile           *memfile;
    OggVorbis_File          ogg;
} ma_stbvorbis;

static ma_result ma_stbvorbis_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell,
                                   void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig,
                                   const ma_allocation_callbacks *pAllocationCallbacks, ma_stbvorbis *pVorbis);
static ma_result ma_stbvorbis_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                          const ma_allocation_callbacks *pAllocationCallbacks, ma_stbvorbis *pVorbis);
static void      ma_stbvorbis_uninit(ma_stbvorbis *pVorbis, const ma_allocation_callbacks *pAllocationCallbacks);
static ma_result ma_stbvorbis_read_pcm_frames(ma_stbvorbis *pVorbis, void *pFramesOut, ma_uint64 frameCount,
                                              ma_uint64 *pFramesRead);
static ma_result ma_stbvorbis_seek_to_pcm_frame(ma_stbvorbis *pVorbis, ma_uint64 frameIndex);
static ma_result ma_stbvorbis_get_data_format(const ma_stbvorbis *pVorbis, ma_format *pFormat, ma_uint32 *pChannels,
                                              ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);
static ma_result ma_stbvorbis_get_cursor_in_pcm_frames(const ma_stbvorbis *pVorbis, ma_uint64 *pCursor);
static ma_result ma_stbvorbis_get_length_in_pcm_frames(ma_stbvorbis *pVorbis, ma_uint64 *pLength);

static ma_result ma_stbvorbis_ds_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount,
                                      ma_uint64 *pFramesRead)
{
    return ma_stbvorbis_read_pcm_frames((ma_stbvorbis *)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_stbvorbis_ds_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
    return ma_stbvorbis_seek_to_pcm_frame((ma_stbvorbis *)pDataSource, frameIndex);
}

static ma_result ma_stbvorbis_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels,
                                                 ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
    return ma_stbvorbis_get_data_format((ma_stbvorbis *)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap,
                                        channelMapCap);
}

static ma_result ma_stbvorbis_ds_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
    return ma_stbvorbis_get_cursor_in_pcm_frames((ma_stbvorbis *)pDataSource, pCursor);
}

static ma_result ma_stbvorbis_ds_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
    return ma_stbvorbis_get_length_in_pcm_frames((ma_stbvorbis *)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_stbvorbis_ds_vtable = {ma_stbvorbis_ds_read,
                                                         ma_stbvorbis_ds_seek,
                                                         ma_stbvorbis_ds_get_data_format,
                                                         ma_stbvorbis_ds_get_cursor,
                                                         ma_stbvorbis_ds_get_length,
                                                         NULL, /* onSetLooping */
                                                         0};

static ma_result ma_stbvorbis_init_internal(const ma_decoding_backend_config *pConfig, ma_stbvorbis *pVorbis)
{
    ma_result             result;
    ma_data_source_config dataSourceConfig;

    EPI_UNUSED(pConfig);

    if (pVorbis == NULL)
    {
        return MA_INVALID_ARGS;
    }

    EPI_CLEAR_MEMORY(pVorbis, ma_stbvorbis, 1);
    pVorbis->format = ma_format_f32; /* Only supporting f32. */

    dataSourceConfig        = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_stbvorbis_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pVorbis->ds);
    if (result != MA_SUCCESS)
    {
        return result; /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

static ma_result ma_stbvorbis_post_init(ma_stbvorbis *pVorbis)
{
    EPI_ASSERT(pVorbis != NULL);

    const vorbis_info *info = ov_info(&pVorbis->ogg, -1);

    if (info == NULL)
    {
        return MA_INVALID_DATA;
    }

    pVorbis->channels   = info->channels;
    pVorbis->sampleRate = info->rate;

    return MA_SUCCESS;
}

static ma_result ma_stbvorbis_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell,
                                   void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig,
                                   const ma_allocation_callbacks *pAllocationCallbacks, ma_stbvorbis *pVorbis)
{
    EPI_UNUSED(pAllocationCallbacks);

    ma_result result;

    result = ma_stbvorbis_init_internal(pConfig, pVorbis);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    if (onRead == NULL || onSeek == NULL)
    {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pVorbis->onRead                = onRead;
    pVorbis->onSeek                = onSeek;
    pVorbis->onTell                = onTell;
    pVorbis->pReadSeekTellUserData = pReadSeekTellUserData;

    return MA_SUCCESS;
}

static ma_result ma_stbvorbis_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                          const ma_allocation_callbacks *pAllocationCallbacks, ma_stbvorbis *pVorbis)
{
    ma_result result;

    result = ma_stbvorbis_init_internal(pConfig, pVorbis);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    EPI_UNUSED(pAllocationCallbacks);

    pVorbis->memfile = new epi::MemFile((const uint8_t *)pData, dataSize);

    if (pVorbis->memfile == NULL)
    {
        return MA_INVALID_DATA;
    }

    if (ov_open_callbacks((void *)pVorbis->memfile, &pVorbis->ogg, NULL, 0, ogg_epi_callbacks) < 0)
    {
        delete pVorbis->memfile;
        return MA_INVALID_DATA;
    }

    result = ma_stbvorbis_post_init(pVorbis);
    if (result != MA_SUCCESS)
    {
        delete pVorbis->memfile;
        return result;
    }

    return MA_SUCCESS;
}

static void ma_stbvorbis_uninit(ma_stbvorbis *pVorbis, const ma_allocation_callbacks *pAllocationCallbacks)
{
    EPI_UNUSED(pAllocationCallbacks);

    if (pVorbis == NULL)
    {
        return;
    }

    ov_clear(&pVorbis->ogg);

    delete (pVorbis->memfile);

    ma_data_source_uninit(&pVorbis->ds);
}

static ma_result ma_stbvorbis_read_pcm_frames(ma_stbvorbis *pVorbis, void *pFramesOut, ma_uint64 frameCount,
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

    if (pVorbis == NULL)
    {
        return MA_INVALID_ARGS;
    }

    /* We always use floating point format. */
    ma_result result          = MA_SUCCESS; /* Must be initialized to MA_SUCCESS. */
    ma_uint64 totalFramesRead = 0;
    int       section         = 0;
    ma_format format;
    ma_uint32 channels;
    ma_uint64 framesLeft  = frameCount;
    float    *pFramesOutF = (float *)pFramesOut;

    ma_stbvorbis_get_data_format(pVorbis, &format, &channels, NULL, NULL, 0);

    if (format == ma_format_f32)
    {
        while (framesLeft > 0)
        {
            float **outFrames  = NULL;
            long    framesRead = ov_read_float(&pVorbis->ogg, &outFrames, framesLeft, &section);
            if (framesRead <= 0)
                break;

            for (ma_uint32 j = 0; j < channels; ++j)
            {
                for (int i = 0; i < framesRead; ++i)
                {
                    pFramesOutF[i * channels + j] = outFrames[j][i];
                }
            }

            framesLeft -= framesRead;
            totalFramesRead += framesRead;
            pFramesOutF += framesRead * channels;
        }
    }
    else
    {
        result = MA_INVALID_ARGS;
    }

    pVorbis->cursor += totalFramesRead;

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

static ma_result ma_stbvorbis_seek_to_pcm_frame(ma_stbvorbis *pVorbis, ma_uint64 frameIndex)
{
    if (pVorbis == NULL)
    {
        return MA_INVALID_ARGS;
    }

    int vorbisResult = ov_pcm_seek(&pVorbis->ogg, frameIndex);

    if (vorbisResult != 0)
    {
        return MA_ERROR; /* See failed. */
    }

    pVorbis->cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result ma_stbvorbis_get_data_format(const ma_stbvorbis *pVorbis, ma_format *pFormat, ma_uint32 *pChannels,
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

    if (pVorbis == NULL)
    {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL)
    {
        *pFormat = pVorbis->format;
    }

    if (pChannels != NULL)
    {
        *pChannels = pVorbis->channels;
    }

    if (pSampleRate != NULL)
    {
        *pSampleRate = pVorbis->sampleRate;
    }

    if (pChannelMap != NULL)
    {
        ma_channel_map_init_standard(ma_standard_channel_map_vorbis, pChannelMap, channelMapCap, pVorbis->channels);
    }

    return MA_SUCCESS;
}

static ma_result ma_stbvorbis_get_cursor_in_pcm_frames(const ma_stbvorbis *pVorbis, ma_uint64 *pCursor)
{
    if (pCursor == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0; /* Safety. */

    if (pVorbis == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = pVorbis->cursor;

    return MA_SUCCESS;
}

static ma_result ma_stbvorbis_get_length_in_pcm_frames(ma_stbvorbis *pVorbis, ma_uint64 *pLength)
{
    if (pLength == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pLength = 0; /* Safety. */

    if (pVorbis == NULL)
    {
        return MA_INVALID_ARGS;
    }

    ogg_int64_t res = ov_pcm_total(&pVorbis->ogg, -1);

    if (res <= 0)
    {
        return MA_INVALID_DATA;
    }

    *pLength = (ma_uint64)res;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init__stbvorbis(void *pUserData, ma_read_proc onRead, ma_seek_proc onSeek,
                                                     ma_tell_proc onTell, void *pReadSeekTellUserData,
                                                     const ma_decoding_backend_config *pConfig,
                                                     const ma_allocation_callbacks    *pAllocationCallbacks,
                                                     ma_data_source                  **ppBackend)
{
    ma_result     result;
    ma_stbvorbis *pVorbis;

    EPI_UNUSED(pUserData); /* For now not using pUserData, but once we start storing the vorbis decoder state within the
                              ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pVorbis = (ma_stbvorbis *)ma_malloc(sizeof(*pVorbis), pAllocationCallbacks);
    if (pVorbis == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_stbvorbis_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pVorbis);
    if (result != MA_SUCCESS)
    {
        ma_free(pVorbis, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pVorbis;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init_memory__stbvorbis(void *pUserData, const void *pData, size_t dataSize,
                                                            const ma_decoding_backend_config *pConfig,
                                                            const ma_allocation_callbacks    *pAllocationCallbacks,
                                                            ma_data_source                  **ppBackend)
{
    ma_result     result;
    ma_stbvorbis *pVorbis;

    EPI_UNUSED(pUserData); /* For now not using pUserData, but once we start storing the vorbis decoder state within the
                              ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pVorbis = (ma_stbvorbis *)ma_malloc(sizeof(*pVorbis), pAllocationCallbacks);
    if (pVorbis == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_stbvorbis_init_memory(pData, dataSize, pConfig, pAllocationCallbacks, pVorbis);
    if (result != MA_SUCCESS)
    {
        ma_free(pVorbis, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pVorbis;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__stbvorbis(void *pUserData, ma_data_source *pBackend,
                                                  const ma_allocation_callbacks *pAllocationCallbacks)
{
    ma_stbvorbis *pVorbis = (ma_stbvorbis *)pBackend;

    EPI_UNUSED(pUserData);

    ma_stbvorbis_uninit(pVorbis, pAllocationCallbacks);
    ma_free(pVorbis, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_stbvorbis = {ma_decoding_backend_init__stbvorbis,
                                                                            NULL, // onInitFile()
                                                                            NULL, // onInitFileW()
                                                                            ma_decoding_backend_init_memory__stbvorbis,
                                                                            ma_decoding_backend_uninit__stbvorbis};

static ma_decoding_backend_vtable *custom_vtable = &g_ma_decoding_backend_vtable_stbvorbis;

class OGGPlayer : public AbstractMusicPlayer
{
  public:
    OGGPlayer();
    ~OGGPlayer() override;

  private:
    const uint8_t *ogg_data_;

  public:
    bool OpenMemory(const uint8_t *data, int length);

    void Close(void) override;

    void Play(bool loop) override;
    void Stop(void) override;

    void Pause(void) override;
    void Resume(void) override;

    void Ticker(void) override;
};

//----------------------------------------------------------------------------

OGGPlayer::OGGPlayer() : ogg_data_(nullptr)
{
    status_ = kNotLoaded;
}

OGGPlayer::~OGGPlayer()
{
    Close();
}

bool OGGPlayer::OpenMemory(const uint8_t *data, int length)
{
    if (status_ != kNotLoaded)
        Close();

    ma_decoder_config decode_config      = ma_decoder_config_init_default();
    decode_config.format                 = ma_format_f32;
    decode_config.customBackendCount     = 1;
    decode_config.pCustomBackendUserData = NULL;
    decode_config.ppCustomBackendVTables = &custom_vtable;

    if (ma_decoder_init_memory(data, length, &decode_config, &ogg_decoder) != MA_SUCCESS)
    {
        LogWarning("Failed to load OGG music (corrupt ogg?)\n");
        return false;
    }

    if (ma_sound_init_from_data_source(&music_engine, &ogg_decoder,
                                       MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, NULL,
                                       &ogg_stream) != MA_SUCCESS)
    {
        ma_decoder_uninit(&ogg_decoder);
        LogWarning("Failed to load OGG music (corrupt ogg?)\n");
        return false;
    }

    ogg_data_ = data;

    // Loaded, but not playing
    status_ = kStopped;

    return true;
}

void OGGPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    Stop();

    ma_sound_uninit(&ogg_stream);

    ma_decoder_uninit(&ogg_decoder);

    delete[] ogg_data_;

    status_ = kNotLoaded;
}

void OGGPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    ma_sound_stop(&ogg_stream);

    status_ = kPaused;
}

void OGGPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    ma_sound_start(&ogg_stream);

    status_ = kPlaying;
}

void OGGPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    looping_ = loop;

    ma_sound_set_looping(&ogg_stream, looping_ ? MA_TRUE : MA_FALSE);

    // Let 'er rip (maybe)
    if (playing_movie)
        status_ = kPaused;
    else
    {
        status_ = kPlaying;
        ma_sound_start(&ogg_stream);
    }
}

void OGGPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    ma_sound_set_volume(&ogg_stream, 0);
    ma_sound_stop(&ogg_stream);

    status_ = kStopped;
}

void OGGPlayer::Ticker()
{
    if (status_ == kPlaying)
    {
        ma_engine_set_volume(&music_engine, music_volume.f_);

        if (pc_speaker_mode)
            Stop();
        if (ma_sound_at_end(&ogg_stream)) // This should only be true if finished and not set to looping
            Stop();
    }
}

//----------------------------------------------------------------------------

AbstractMusicPlayer *PlayOGGMusic(uint8_t *data, int length, bool looping)
{
    OGGPlayer *player = new OGGPlayer();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    player->Play(looping);

    return player;
}

bool LoadOGGSound(SoundData *buf, const uint8_t *data, int length)
{
    ma_decoder_config decode_config      = ma_decoder_config_init_default();
    decode_config.format                 = ma_format_f32;
    decode_config.customBackendCount     = 1;
    decode_config.pCustomBackendUserData = NULL;
    decode_config.ppCustomBackendVTables = &custom_vtable;
    ma_decoder decode;

    if (ma_decoder_init_memory(data, length, &decode_config, &decode) != MA_SUCCESS)
    {
        LogWarning("Failed to load OGG sound (corrupt ogg?)\n");
        return false;
    }

    if (decode.outputChannels > 2)
    {
        LogWarning("OGG SFX Loader: too many channels: %d\n", decode.outputChannels);
        ma_decoder_uninit(&decode);
        return false;
    }

    ma_uint64 frame_count = 0;

    if (ma_decoder_get_length_in_pcm_frames(&decode, &frame_count) != MA_SUCCESS)
    {
        LogWarning("OGG SFX Loader: no samples!\n");
        ma_decoder_uninit(&decode);
        return false;
    }

    LogDebug("OGG SFX Loader: freq %d Hz, %d channels\n", decode.outputSampleRate, decode.outputChannels);

    bool is_stereo = (decode.outputChannels > 1);

    buf->frequency_ = decode.outputSampleRate;

    SoundGatherer gather;

    float *buffer = gather.MakeChunk(frame_count, is_stereo);

    ma_uint64 frames_read = 0;

    if (ma_decoder_read_pcm_frames(&decode, buffer, frame_count, &frames_read) != MA_SUCCESS)
    {
        LogWarning("OGG SFX Loader: failure loading samples!\n");
        gather.DiscardChunk();
        ma_decoder_uninit(&decode);
        return false;
    }

    gather.CommitChunk(frames_read);

    if (!gather.Finalise(buf))
        LogWarning("OGG SFX Loader: no samples!\n");

    ma_decoder_uninit(&decode);

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
