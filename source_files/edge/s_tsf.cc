//----------------------------------------------------------------------------
//  EDGE TinySoundFont Music Player
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

#include "s_tsf.h"

#include <stdint.h>

#include "HandmadeMath.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_misc.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "s_blit.h"
// clang-format off
#define MidiFraction TSFFraction
#define MidiSequencer TSFSequencer
typedef struct MidiRealTimeInterface TSFInterface;
#include "s_midi.h"
// clang-format on
#include "s_music.h"
#include "tsf.h"

extern int  sound_device_frequency;

bool tsf_disabled = false;

tsf *edge_tsf = nullptr;

EDGE_DEFINE_CONSOLE_VARIABLE(midi_soundfont, "", (ConsoleVariableFlag)(kConsoleVariableFlagArchive | kConsoleVariableFlagFilepath))

EDGE_DEFINE_CONSOLE_VARIABLE(tsf_player_gain, "0.6", kConsoleVariableFlagArchive)

extern std::vector<std::string> available_soundfonts;

void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
{
    EPI_UNUSED(userdata);
    tsf_channel_note_on(edge_tsf, channel, note, static_cast<float>(velocity) / 127.0f);
}

void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
{
    EPI_UNUSED(userdata);
    tsf_channel_note_off(edge_tsf, channel, note);
}

void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(channel); 
    EPI_UNUSED(note);
    EPI_UNUSED(atVal);
}

void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(channel);
    EPI_UNUSED(atVal);
}

void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
{
    EPI_UNUSED(userdata);
    tsf_channel_midi_control(edge_tsf, channel, type, value);
}

void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
{
    EPI_UNUSED(userdata);
    tsf_channel_set_presetnumber(edge_tsf, channel, patch, channel == 9);
}

void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
{
    EPI_UNUSED(userdata);
    tsf_channel_set_pitchwheel(edge_tsf, channel, (msb << 7) | lsb);
}

void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(msg);
    EPI_UNUSED(size);
}

void rtDeviceSwitch(void *userdata, size_t track, const char *data, size_t length)
{
    EPI_UNUSED(userdata); 
    EPI_UNUSED(track); 
    EPI_UNUSED(data);
    EPI_UNUSED(length);
}

size_t rtCurrentDevice(void *userdata, size_t track)
{
    EPI_UNUSED(userdata);
    EPI_UNUSED(track);
    return 0;
}

void playSynth(void *userdata, uint8_t *stream, size_t length)
{
    EPI_UNUSED(userdata);
    tsf_render_float(edge_tsf, (float *)stream, length / 2 / sizeof(float), 0);
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
    TSFInterface *tsf_interface;
    TSFSequencer *tsf_sequencer;
} ma_tsf;

static void TSFSequencerInit(ma_tsf *synth)
{
    EPI_CLEAR_MEMORY(synth->tsf_interface, TSFInterface, 1);
    synth->tsf_interface->rtUserData           = NULL;
    synth->tsf_interface->rt_noteOn            = rtNoteOn;
    synth->tsf_interface->rt_noteOff           = rtNoteOff;
    synth->tsf_interface->rt_noteAfterTouch    = rtNoteAfterTouch;
    synth->tsf_interface->rt_channelAfterTouch = rtChannelAfterTouch;
    synth->tsf_interface->rt_controllerChange  = rtControllerChange;
    synth->tsf_interface->rt_patchChange       = rtPatchChange;
    synth->tsf_interface->rt_pitchBend         = rtPitchBend;
    synth->tsf_interface->rt_systemExclusive   = rtSysEx;

    synth->tsf_interface->onPcmRender          = playSynth;
    synth->tsf_interface->onPcmRender_userdata = NULL;

    synth->tsf_interface->pcmSampleRate = sound_device_frequency;
    synth->tsf_interface->pcmFrameSize  = 2 /*channels*/ * sizeof(float) /*size of one sample*/;

    synth->tsf_interface->rt_deviceSwitch  = rtDeviceSwitch;
    synth->tsf_interface->rt_currentDevice = rtCurrentDevice;

    synth->tsf_sequencer->SetInterface(synth->tsf_interface);
}

static ma_result ma_tsf_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_tsf* pTSF);
static ma_result ma_tsf_init_memory(const void* pData, size_t dataSize, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_tsf* pTSF);
static void ma_tsf_uninit(ma_tsf* pTSF, const ma_allocation_callbacks* pAllocationCallbacks);
static ma_result ma_tsf_read_pcm_frames(ma_tsf* pTSF, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
static ma_result ma_tsf_seek_to_pcm_frame(ma_tsf* pTSF, ma_uint64 frameIndex);
static ma_result ma_tsf_get_data_format(ma_tsf* pTSF, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
static ma_result ma_tsf_get_cursor_in_pcm_frames(ma_tsf* pTSF, ma_uint64* pCursor);
static ma_result ma_tsf_get_length_in_pcm_frames(ma_tsf* pTSF, ma_uint64* pLength);

static ma_result ma_tsf_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_tsf_read_pcm_frames((ma_tsf*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_tsf_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_tsf_seek_to_pcm_frame((ma_tsf*)pDataSource, frameIndex);
}

static ma_result ma_tsf_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_tsf_get_data_format((ma_tsf*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_tsf_ds_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_tsf_get_cursor_in_pcm_frames((ma_tsf*)pDataSource, pCursor);
}

static ma_result ma_tsf_ds_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_tsf_get_length_in_pcm_frames((ma_tsf*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_tsf_ds_vtable =
{
    ma_tsf_ds_read,
    ma_tsf_ds_seek,
    ma_tsf_ds_get_data_format,
    ma_tsf_ds_get_cursor,
    ma_tsf_ds_get_length,
    NULL,   /* onSetLooping */
    0
};

static ma_result ma_tsf_init_internal(const ma_decoding_backend_config* pConfig, ma_tsf* pTSF)
{
    ma_result result;
    ma_data_source_config dataSourceConfig;

    (void)pConfig;

    if (pTSF == NULL) {
        return MA_INVALID_ARGS;
    }

    EPI_CLEAR_MEMORY(pTSF, ma_tsf, 1);
    pTSF->format = ma_format_f32;    /* Only supporting f32. */

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_tsf_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pTSF->ds);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

static ma_result ma_tsf_post_init(ma_tsf* pTSF)
{
    EPI_ASSERT(pTSF != NULL);

    pTSF->channels   = 2;
    pTSF->sampleRate = sound_device_frequency;

    return MA_SUCCESS;
}

static ma_result ma_tsf_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_tsf* pTSF)
{
    if (tsf_disabled || edge_tsf == NULL)
        return MA_ERROR;

    ma_result result;

    result = ma_tsf_init_internal(pConfig, pTSF);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (onRead == NULL || onSeek == NULL) {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pTSF->onRead = onRead;
    pTSF->onSeek = onSeek;
    pTSF->onTell = onTell;
    pTSF->pReadSeekTellUserData = pReadSeekTellUserData;

    return MA_SUCCESS;
}

static ma_result ma_tsf_init_memory(const void* pData, size_t dataSize, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_tsf* pTSF)
{
    ma_result result;

    result = ma_tsf_init_internal(pConfig, pTSF);
    if (result != MA_SUCCESS) {
        return result;
    }

    (void)pAllocationCallbacks;

    pTSF->tsf_sequencer = new TSFSequencer;
    pTSF->tsf_interface = new TSFInterface;

    TSFSequencerInit(pTSF);

    if (!pTSF->tsf_sequencer->LoadMidi((const uint8_t *)pData, dataSize)) {
        return MA_INVALID_FILE;
    }

    result = ma_tsf_post_init(pTSF);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_tsf_uninit(ma_tsf* pTSF, const ma_allocation_callbacks* pAllocationCallbacks)
{
    (void)pAllocationCallbacks;

    if (pTSF == NULL) {
        return;
    }

    delete pTSF->tsf_interface;
    pTSF->tsf_interface = NULL;
    delete pTSF->tsf_sequencer;
    pTSF->tsf_sequencer = NULL;

    ma_data_source_uninit(&pTSF->ds);
}

static ma_result ma_tsf_read_pcm_frames(ma_tsf* pTSF, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (frameCount == 0) {
        return MA_INVALID_ARGS;
    }

    if (pTSF == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We always use floating point format. */
    ma_result result = MA_SUCCESS;  /* Must be initialized to MA_SUCCESS. */
    ma_uint64 totalFramesRead = 0;
    ma_format format;
    ma_uint32 channels;

    ma_tsf_get_data_format(pTSF, &format, &channels, NULL, NULL, 0);

    if (format == ma_format_f32) {
        totalFramesRead = pTSF->tsf_sequencer->PlayStream((uint8_t *)pFramesOut, frameCount * 2 * sizeof(float)) / 2 / sizeof(float);
    } else {
        result = MA_INVALID_ARGS;
    }

    pTSF->cursor += totalFramesRead;

    if (pFramesRead != NULL) {
        *pFramesRead = totalFramesRead;
    }

    if (result == MA_SUCCESS && pTSF->tsf_sequencer->PositionAtEnd()) {
        result  = MA_AT_END;
    }

    return result;
}

static ma_result ma_tsf_seek_to_pcm_frame(ma_tsf* pTSF, ma_uint64 frameIndex)
{
    if (pTSF == NULL || frameIndex != 0) {
        return MA_INVALID_ARGS;
    }

    pTSF->tsf_sequencer->Rewind();

    pTSF->cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result ma_tsf_get_data_format(ma_tsf* pTSF, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
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

    if (pTSF == NULL) {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL) {
        *pFormat = pTSF->format;
    }

    if (pChannels != NULL) {
        *pChannels = pTSF->channels;
    }

    if (pSampleRate != NULL) {
        *pSampleRate = pTSF->sampleRate;
    }

    if (pChannelMap != NULL) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, pTSF->channels);
    }

    return MA_SUCCESS;
}

static ma_result ma_tsf_get_cursor_in_pcm_frames(ma_tsf* pTSF, ma_uint64* pCursor)
{
    if (pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;   /* Safety. */

    if (pTSF == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = pTSF->cursor;

    return MA_SUCCESS;
}

static ma_result ma_tsf_get_length_in_pcm_frames(ma_tsf* pTSF, ma_uint64* pLength)
{
    if (pLength == NULL) {
        return MA_INVALID_ARGS;
    }

    *pLength = 0;   /* Safety. */

    if (pTSF == NULL) {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init__tsf(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_tsf* pTSF;

    (void)pUserData;    /* For now not using pUserData, but once we start storing the vorbis decoder state within the ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pTSF = (ma_tsf*)ma_malloc(sizeof(*pTSF), pAllocationCallbacks);
    if (pTSF == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_tsf_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pTSF);
    if (result != MA_SUCCESS) {
        ma_free(pTSF, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pTSF;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init_memory__tsf(void* pUserData, const void* pData, size_t dataSize, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_tsf* pTSF;

    (void)pUserData;    /* For now not using pUserData, but once we start storing the vorbis decoder state within the ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pTSF = (ma_tsf*)ma_malloc(sizeof(*pTSF), pAllocationCallbacks);
    if (pTSF == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_tsf_init_memory(pData, dataSize, pConfig, pAllocationCallbacks, pTSF);
    if (result != MA_SUCCESS) {
        ma_free(pTSF, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pTSF;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__tsf(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_tsf* pTSF = (ma_tsf*)pBackend;

    (void)pUserData;

    ma_tsf_uninit(pTSF, pAllocationCallbacks);
    ma_free(pTSF, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_tsf =
{
    ma_decoding_backend_init__tsf,
    NULL, // onInitFile()
    NULL, // onInitFileW()
    ma_decoding_backend_init_memory__tsf,
    ma_decoding_backend_uninit__tsf
};

static ma_decoding_backend_vtable *tsf_custom_vtable = &g_ma_decoding_backend_vtable_tsf;

bool StartupTSF(void)
{
    LogPrint("Initializing TinySoundFont...\n");

    // Check for presence of previous CVAR value's file
    bool cvar_good = false;
    for (size_t i = 0; i < available_soundfonts.size(); i++)
    {
        if (epi::StringCaseCompareASCII(midi_soundfont.s_, available_soundfonts.at(i)) == 0)
        {
            cvar_good = true;
            break;
        }
    }

    if (!cvar_good)
    {
        LogWarning("Cannot find previously used soundfont %s, falling back to "
                   "default!\n",
                   midi_soundfont.c_str());
        midi_soundfont = epi::SanitizePath(epi::PathAppend(game_directory, "soundfont/Default.sf2"));
        if (!epi::FileExists(midi_soundfont.s_))
            FatalError("TinySoundFont: Cannot locate default soundfont (Default.sf2)! "
                       "Please check the /soundfont directory "
                       "of your EDGE-Classic install!\n");
    }

    epi::File *raw_sf2 = epi::FileOpen(midi_soundfont.s_, epi::kFileAccessBinary|epi::kFileAccessRead);
    size_t raw_sf2_size = raw_sf2->GetLength();
    uint8_t *raw_sf2_data = raw_sf2->LoadIntoMemory();

    edge_tsf = tsf_load_memory(raw_sf2_data, raw_sf2_size);

    delete[] raw_sf2_data;
    delete raw_sf2;

    for(int ch = 0; ch < 16; ch++)
        tsf_channel_set_bank(edge_tsf, ch, 0);
    tsf_channel_set_bank_preset(edge_tsf, 9, 128, 0);
    tsf_set_output(edge_tsf, TSF_STEREO_INTERLEAVED, sound_device_frequency);
    tsf_set_volume(edge_tsf, 1.0f);

    return true; // OK!
}

// Should only be invoked when switching soundfonts
void RestartTSF(void)
{
    if (tsf_disabled)
        return;

    LogPrint("Restarting TinySoundFont...\n");

    int old_entry = entry_playing;

    StopMusic();

    tsf_close(edge_tsf);
    edge_tsf = nullptr;

    if (!StartupTSF())
    {
        tsf_disabled = true;
        return;
    }

    ChangeMusic(old_entry,
                true); // Restart track that was playing when switched

    return;            // OK!
}

class TSFPlayer : public AbstractMusicPlayer
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

    ma_decoder tsf_decoder_;
    ma_sound   tsf_stream_;

  public:
    TSFPlayer(bool looping) : status_(kNotLoaded), looping_(looping)
    {
        EPI_CLEAR_MEMORY(&tsf_decoder_, ma_decoder, 1);
        EPI_CLEAR_MEMORY(&tsf_stream_, ma_sound, 1);
    }

    ~TSFPlayer()
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
        decode_config.pCustomBackendUserData = NULL;
        decode_config.ppCustomBackendVTables = &tsf_custom_vtable;

        if (ma_decoder_init_memory(data, length, &decode_config, &tsf_decoder_) != MA_SUCCESS)
        {
            LogWarning("Failed to load MIDI music\n");
            return false;
        }

        if (ma_sound_init_from_data_source(&music_engine, &tsf_decoder_, MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_UNKNOWN_LENGTH|MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &tsf_stream_) != MA_SUCCESS)
        {
            ma_decoder_uninit(&tsf_decoder_);
            LogWarning("Failed to load MIDI music\n");
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

        ma_sound_uninit(&tsf_stream_);

        ma_decoder_uninit(&tsf_decoder_);

        status_ = kNotLoaded;
    }

    void Play(bool loop)
    {
        status_  = kPlaying;
        looping_ = loop;

        ma_sound_set_looping(&tsf_stream_, looping_ ? MA_TRUE : MA_FALSE);

        // Let 'er rip
        ma_sound_start(&tsf_stream_);
    }

    void Stop(void)
    {
        if (status_ != kPlaying && status_ != kPaused)
            return;

        tsf_note_off_all(edge_tsf);
        for(int ch = 0; ch < 16; ch++)
            tsf_channel_sounds_off_all(edge_tsf, ch);

        ma_sound_stop(&tsf_stream_);

        ma_decoder_seek_to_pcm_frame(&tsf_decoder_, 0);

        status_ = kStopped;
    }

    void Pause(void)
    {
        if (status_ != kPlaying)
            return;

        tsf_note_off_all(edge_tsf);

        ma_sound_stop(&tsf_stream_);

        status_ = kPaused;
    }

    void Resume(void)
    {
        if (status_ != kPaused)
            return;

        ma_sound_start(&tsf_stream_);

        status_ = kPlaying;
    }

    void Ticker(void)
    {
        ma_engine_set_volume(&music_engine, music_volume.f_);

        if (status_ == kPlaying)
        {
            if (pc_speaker_mode)
                Stop();
            if (ma_sound_at_end(&tsf_stream_)) // This should only be true if finished and not set to looping
                Stop();
        }
    }
};

AbstractMusicPlayer *PlayTSFMusic(uint8_t *data, int length, bool loop)
{
    if (tsf_disabled)
    {
        delete[] data;
        return nullptr;
    }

    TSFPlayer *player = new TSFPlayer(loop);

    if (!player)
    {
        LogDebug("TinySoundFont player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    if (!player->OpenMemory(data,
                           length)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("TinySoundFont player: failed to load MIDI file!\n");
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
