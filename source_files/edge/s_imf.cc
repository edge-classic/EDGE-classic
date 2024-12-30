//----------------------------------------------------------------------------
//  EDGE IMF Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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

#include "s_imf.h"

#include "dm_state.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_misc.h"
#include "ddf_playlist.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "opal.h"
#include "s_blit.h"
// clang-format off
#define MidiFraction IMFFraction
#define MidiSequencer IMFSequencer
typedef struct MidiRealTimeInterface IMFInterface;
#include "s_midi.h"
// clang-format on
#include "s_music.h"
#include "snd_types.h"
#include "w_files.h"
#include "w_wad.h"

extern int  sound_device_frequency;

static uint16_t imf_rate = 0;

static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(channel);
    EPI_UNUSED(note);
    EPI_UNUSED(velocity);
}

static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(channel);
    EPI_UNUSED(note);
}

static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(channel);
    EPI_UNUSED(note);
    EPI_UNUSED(atVal);
}

static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(channel);
    EPI_UNUSED(atVal);
}

static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(channel);
    EPI_UNUSED(type);
    EPI_UNUSED(value);
}

static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(channel);
    EPI_UNUSED(patch);
}

static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(channel);
    EPI_UNUSED(msb);
    EPI_UNUSED(lsb);
}

static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(msg);
    EPI_UNUSED(size);
}

static void rtDeviceSwitch(void *userdata, size_t track, const char *data, size_t length)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(track);
    EPI_UNUSED(data);
    EPI_UNUSED(length);
}

static size_t rtCurrentDevice(void *userdata, size_t track)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(track);
    return 0;
}

static void rtRawOPL(void *userdata, uint8_t reg, uint8_t value)
{
    Opal *imf_opl = (Opal *)userdata;
    if ((reg & 0xF0) == 0xC0)
        value |= 0x30;
    imf_opl->Port(reg, value);
}

static void playSynth(void *userdata, uint8_t *stream, size_t length)
{
    Opal *imf_opl = (Opal *)userdata;
    size_t real_length = length / sizeof(float);
    for (size_t i = 0; i < real_length; i += 2)
        imf_opl->SampleFloat((float *)stream + i, (float *)stream + i + 1);
}

typedef struct
{
    ma_data_source_base ds;
    ma_read_proc onRead;
    ma_seek_proc onSeek;
    ma_tell_proc onTell;
    void* pReadSeekTellUserData;
    ma_allocation_callbacks allocationCallbacks;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint64 cursor;
    IMFInterface *imf_interface;
    IMFSequencer *imf_sequencer;
    Opal *imf_opl;
} ma_imf;

static void IMFSequencerInit(ma_imf *synth)
{
    EPI_CLEAR_MEMORY(synth->imf_interface, IMFInterface, 1);
    synth->imf_interface->rtUserData           = synth->imf_opl;
    synth->imf_interface->rt_noteOn            = rtNoteOn;
    synth->imf_interface->rt_noteOff           = rtNoteOff;
    synth->imf_interface->rt_noteAfterTouch    = rtNoteAfterTouch;
    synth->imf_interface->rt_channelAfterTouch = rtChannelAfterTouch;
    synth->imf_interface->rt_controllerChange  = rtControllerChange;
    synth->imf_interface->rt_patchChange       = rtPatchChange;
    synth->imf_interface->rt_pitchBend         = rtPitchBend;
    synth->imf_interface->rt_systemExclusive   = rtSysEx;
    synth->imf_interface->rt_rawOPL            = rtRawOPL;

    synth->imf_interface->onPcmRender          = playSynth;
    synth->imf_interface->onPcmRender_userdata = synth->imf_opl;

    synth->imf_interface->pcmSampleRate = sound_device_frequency;
    synth->imf_interface->pcmFrameSize  = 2 /*channels*/ * sizeof(float) /*size of one sample*/;

    synth->imf_interface->rt_deviceSwitch  = rtDeviceSwitch;
    synth->imf_interface->rt_currentDevice = rtCurrentDevice;

    synth->imf_sequencer->SetInterface(synth->imf_interface);
}

static ma_result ma_imf_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_imf* pIMF);
static ma_result ma_imf_init_memory(const void* pData, size_t dataSize, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_imf* pIMF);
static void ma_imf_uninit(ma_imf* pIMF, const ma_allocation_callbacks* pAllocationCallbacks);
static ma_result ma_imf_read_pcm_frames(ma_imf* pIMF, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
static ma_result ma_imf_seek_to_pcm_frame(ma_imf* pIMF, ma_uint64 frameIndex);
static ma_result ma_imf_get_data_format(ma_imf* pIMF, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
static ma_result ma_imf_get_cursor_in_pcm_frames(ma_imf* pIMF, ma_uint64* pCursor);
static ma_result ma_imf_get_length_in_pcm_frames(ma_imf* pIMF, ma_uint64* pLength);

static ma_result ma_imf_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_imf_read_pcm_frames((ma_imf*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_imf_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_imf_seek_to_pcm_frame((ma_imf*)pDataSource, frameIndex);
}

static ma_result ma_imf_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_imf_get_data_format((ma_imf*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_imf_ds_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_imf_get_cursor_in_pcm_frames((ma_imf*)pDataSource, pCursor);
}

static ma_result ma_imf_ds_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_imf_get_length_in_pcm_frames((ma_imf*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_imf_ds_vtable =
{
    ma_imf_ds_read,
    ma_imf_ds_seek,
    ma_imf_ds_get_data_format,
    ma_imf_ds_get_cursor,
    ma_imf_ds_get_length,
    NULL,   /* onSetLooping */
    0
};

static ma_result ma_imf_init_internal(const ma_decoding_backend_config* pConfig, ma_imf* pIMF)
{
    ma_result result;
    ma_data_source_config dataSourceConfig;

    EPI_UNUSED(pConfig);

    if (pIMF == NULL) {
        return MA_INVALID_ARGS;
    }

    EPI_CLEAR_MEMORY(pIMF, ma_imf, 1);
    pIMF->format = ma_format_f32;    /* Only supporting f32. */

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_imf_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pIMF->ds);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

static ma_result ma_imf_post_init(ma_imf* pIMF)
{
    EPI_ASSERT(pIMF != NULL);

    pIMF->channels   = 2;
    pIMF->sampleRate = sound_device_frequency;

    return MA_SUCCESS;
}

static ma_result ma_imf_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_imf* pIMF)
{
    EPI_UNUSED(pAllocationCallbacks);

    ma_result result;

    result = ma_imf_init_internal(pConfig, pIMF);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (onRead == NULL || onSeek == NULL) {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pIMF->onRead = onRead;
    pIMF->onSeek = onSeek;
    pIMF->onTell = onTell;
    pIMF->pReadSeekTellUserData = pReadSeekTellUserData;

    return MA_SUCCESS;
}

static ma_result ma_imf_init_memory(const void* pData, size_t dataSize, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_imf* pIMF)
{
    ma_result result;

    result = ma_imf_init_internal(pConfig, pIMF);
    if (result != MA_SUCCESS) {
        return result;
    }

    EPI_UNUSED(pAllocationCallbacks);

    pIMF->imf_sequencer = new IMFSequencer;
    pIMF->imf_interface = new IMFInterface;
    pIMF->imf_opl = new Opal(sound_device_frequency);

    IMFSequencerInit(pIMF);

    if (!pIMF->imf_sequencer->LoadMidi((const uint8_t *)pData, dataSize, imf_rate)) {
        return MA_INVALID_FILE;
    }

    result = ma_imf_post_init(pIMF);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_imf_uninit(ma_imf* pIMF, const ma_allocation_callbacks* pAllocationCallbacks)
{
    EPI_UNUSED(pAllocationCallbacks);

    if (pIMF == NULL) {
        return;
    }

    delete pIMF->imf_interface;
    pIMF->imf_interface = NULL;
    delete pIMF->imf_sequencer;
    pIMF->imf_sequencer = NULL;
    delete pIMF->imf_opl;
    pIMF->imf_opl = NULL;

    ma_data_source_uninit(&pIMF->ds);
}

static ma_result ma_imf_read_pcm_frames(ma_imf* pIMF, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (frameCount == 0) {
        return MA_INVALID_ARGS;
    }

    if (pIMF == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We always use floating point format. */
    ma_result result = MA_SUCCESS;  /* Must be initialized to MA_SUCCESS. */
    ma_uint64 totalFramesRead = 0;
    ma_format format;
    ma_uint32 channels;

    ma_imf_get_data_format(pIMF, &format, &channels, NULL, NULL, 0);

    if (format == ma_format_f32) {
        totalFramesRead = pIMF->imf_sequencer->PlayStream((uint8_t *)pFramesOut, frameCount * 2 * sizeof(float)) / 2 / sizeof(float);
    } else {
        result = MA_INVALID_ARGS;
    }

    pIMF->cursor += totalFramesRead;

    if (pFramesRead != NULL) {
        *pFramesRead = totalFramesRead;
    }

    if (result == MA_SUCCESS && pIMF->imf_sequencer->PositionAtEnd()) {
        result  = MA_AT_END;
    }

    return result;
}

static ma_result ma_imf_seek_to_pcm_frame(ma_imf* pIMF, ma_uint64 frameIndex)
{
    if (pIMF == NULL || frameIndex != 0) {
        return MA_INVALID_ARGS;
    }

    pIMF->imf_sequencer->Rewind();

    pIMF->cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result ma_imf_get_data_format(ma_imf* pIMF, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    /* Defaults for safety. */
    if (pFormat != NULL) {
        *pFormat = ma_format_unknown;
    }
    if (pChannels != NULL) {
        *pChannels = 0;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = 0;
    }
    if (pChannelMap != NULL) {
        EPI_CLEAR_MEMORY(pChannelMap, ma_channel, channelMapCap);
    }

    if (pIMF == NULL) {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL) {
        *pFormat = pIMF->format;
    }

    if (pChannels != NULL) {
        *pChannels = pIMF->channels;
    }

    if (pSampleRate != NULL) {
        *pSampleRate = pIMF->sampleRate;
    }

    if (pChannelMap != NULL) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, pIMF->channels);
    }

    return MA_SUCCESS;
}

static ma_result ma_imf_get_cursor_in_pcm_frames(ma_imf* pIMF, ma_uint64* pCursor)
{
    if (pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;   /* Safety. */

    if (pIMF == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = pIMF->cursor;

    return MA_SUCCESS;
}

static ma_result ma_imf_get_length_in_pcm_frames(ma_imf* pIMF, ma_uint64* pLength)
{
    if (pLength == NULL) {
        return MA_INVALID_ARGS;
    }

    *pLength = 0;   /* Safety. */

    if (pIMF == NULL) {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init__imf(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_imf* pIMF;

    EPI_UNUSED(pUserData);    /* For now not using pUserData, but once we start storing the vorbis decoder state within the ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pIMF = (ma_imf*)ma_malloc(sizeof(*pIMF), pAllocationCallbacks);
    if (pIMF == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_imf_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pIMF);
    if (result != MA_SUCCESS) {
        ma_free(pIMF, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pIMF;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init_memory__imf(void* pUserData, const void* pData, size_t dataSize, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_imf* pIMF;

    EPI_UNUSED(pUserData);    /* For now not using pUserData, but once we start storing the vorbis decoder state within the ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pIMF = (ma_imf*)ma_malloc(sizeof(*pIMF), pAllocationCallbacks);
    if (pIMF == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_imf_init_memory(pData, dataSize, pConfig, pAllocationCallbacks, pIMF);
    if (result != MA_SUCCESS) {
        ma_free(pIMF, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pIMF;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__imf(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_imf* pIMF = (ma_imf*)pBackend;

    EPI_UNUSED(pUserData);

    ma_imf_uninit(pIMF, pAllocationCallbacks);
    ma_free(pIMF, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_imf =
{
    ma_decoding_backend_init__imf,
    NULL, // onInitFile()
    NULL, // onInitFileW()
    ma_decoding_backend_init_memory__imf,
    ma_decoding_backend_uninit__imf
};

static ma_decoding_backend_vtable *imf_custom_vtable = &g_ma_decoding_backend_vtable_imf;

class IMFPlayer : public AbstractMusicPlayer
{
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

    ma_decoder imf_decoder_;
    ma_sound   imf_stream_;

  public:
    IMFPlayer(bool looping) : status_(kNotLoaded), looping_(looping)
    {
        EPI_CLEAR_MEMORY(&imf_decoder_, ma_decoder, 1);
        EPI_CLEAR_MEMORY(&imf_stream_, ma_sound, 1);
    }

    ~IMFPlayer()
    {
        Close();
    }

  public:
    bool OpenMemory(uint8_t *data, int length)
    {
        if (status_ != kNotLoaded)
            Close();

        ma_decoder_config decode_config = ma_decoder_config_init_default();
        decode_config.format = ma_format_f32;
        decode_config.customBackendCount = 1;
        decode_config.ppCustomBackendVTables = &imf_custom_vtable;

        if (ma_decoder_init_memory(data, length, &decode_config, &imf_decoder_) != MA_SUCCESS)
        {
            LogWarning("Failed to load IMF music\n");
            return false;
        }

        if (ma_sound_init_from_data_source(&music_engine, &imf_decoder_, MA_SOUND_FLAG_NO_PITCH|MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_UNKNOWN_LENGTH|MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &imf_stream_) != MA_SUCCESS)
        {
            ma_decoder_uninit(&imf_decoder_);
            LogWarning("Failed to load IMF music\n");
            return false;
        }

        // Loaded, but not playing
        status_ = kStopped;

        return true;
    }

    void Close(void)
    {
        if (status_ == kNotLoaded)
            return;

        // Stop playback
        Stop();

        ma_sound_uninit(&imf_stream_);

        ma_decoder_uninit(&imf_decoder_);

        status_ = kNotLoaded;
    }

    void Play(bool loop)
    {
        looping_ = loop;

        ma_sound_set_looping(&imf_stream_, looping_ ? MA_TRUE : MA_FALSE);

        // Let 'er rip (maybe)
        if (playing_movie)
            status_ = kPaused;
        else
        {
            status_  = kPlaying;
            ma_sound_start(&imf_stream_);
        }
    }

    void Stop(void)
    {
        if (status_ != kPlaying && status_ != kPaused)
            return;

        ma_sound_stop(&imf_stream_);

        ma_decoder_seek_to_pcm_frame(&imf_decoder_, 0);

        status_ = kStopped;
    }


    void Pause(void)
    {
        if (status_ != kPlaying)
            return;

        ma_sound_stop(&imf_stream_);

        status_ = kPaused;
    }

    void Resume(void)
    {
        if (status_ != kPaused)
            return;

        ma_sound_start(&imf_stream_);

        status_ = kPlaying;
    }

    void Ticker(void)
    {
        ma_engine_set_volume(&music_engine, music_volume.f_ * 0.25f);

        if (status_ == kPlaying)
        {
            if (pc_speaker_mode)
                Stop();
            if (ma_sound_at_end(&imf_stream_)) // This should only be true if finished and not set to looping
                Stop();
        }
    }
};

AbstractMusicPlayer *PlayIMFMusic(uint8_t *data, int length, bool loop, int type)
{
    IMFPlayer *player = new IMFPlayer(loop);

    if (!player)
    {
        LogDebug("IMF player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    switch (type)
    {
    case kDDFMusicIMF280:
        imf_rate = 280;
        break;
    case kDDFMusicIMF560:
        imf_rate = 560;
        break;
    case kDDFMusicIMF700:
        imf_rate = 700;
        break;
    default:
        imf_rate = 0;
        break;
    }

    if (imf_rate == 0)
    {
        LogDebug("IMF player: no IMF sample rate provided!\n");
        delete[] data;
        delete player;
        return nullptr;  
    }

    if (!player->OpenMemory(data, length)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("IMF player: failed to load IMF file!\n");
        delete[] data;
        delete player;
        return nullptr;
    }

    delete[] data;

    player->Play(loop);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
