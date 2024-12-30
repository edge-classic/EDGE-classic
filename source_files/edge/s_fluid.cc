//----------------------------------------------------------------------------
//  EDGE Fluidlite Music Player
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

#include "s_fluid.h"

#include <stdint.h>

#include "HandmadeMath.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "fluidlite.h"
#include "i_movie.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_misc.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "s_blit.h"
// clang-format off
#define MidiFraction FluidFraction
#define MidiSequencer FluidSequencer
typedef struct MidiRealTimeInterface FluidInterface;
#include "s_midi.h"
// clang-format on
#include "s_music.h"

extern int  sound_device_frequency;

bool fluid_disabled = false;

fluid_synth_t    *edge_fluid            = nullptr;
fluid_settings_t *edge_fluid_settings   = nullptr;
fluid_sfloader_t *edge_fluid_sf2_loader = nullptr;

EDGE_DEFINE_CONSOLE_VARIABLE(midi_soundfont, "", (ConsoleVariableFlag)(kConsoleVariableFlagArchive | kConsoleVariableFlagFilepath))

EDGE_DEFINE_CONSOLE_VARIABLE(fluid_player_gain, "0.6", kConsoleVariableFlagArchive)

extern std::vector<std::string> available_soundfonts;

static void FluidError(int level, char *message, void *data)
{
    EPI_UNUSED(level);
    EPI_UNUSED(data);
    FatalError("Fluidlite: %s\n", message);
}

static void *edge_fluid_fopen(fluid_fileapi_t *fileapi, const char *filename)
{
    EPI_UNUSED(fileapi);
    FILE *fp = epi::FileOpenRaw(filename, epi::kFileAccessRead | epi::kFileAccessBinary);
    if (!fp)
        return nullptr;
    return fp;
}

void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
{
    EPI_UNUSED(userdata);
    fluid_synth_noteon(edge_fluid, channel, note, velocity);
}

void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
{
    EPI_UNUSED(userdata);
    fluid_synth_noteoff(edge_fluid, channel, note);
}

void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
{
    EPI_UNUSED(userdata);
    fluid_synth_key_pressure(edge_fluid, channel, note, atVal);
}

void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
{
    EPI_UNUSED(userdata);
    fluid_synth_channel_pressure(edge_fluid, channel, atVal);
}

void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
{
    EPI_UNUSED(userdata);
    fluid_synth_cc(edge_fluid, channel, type, value);
}

void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
{
    EPI_UNUSED(userdata);
    fluid_synth_program_change(edge_fluid, channel, patch);
}

void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
{
    EPI_UNUSED(userdata);
    fluid_synth_pitch_bend(edge_fluid, channel, (msb << 7) | lsb);
}

void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
{
    EPI_UNUSED(userdata);
    fluid_synth_sysex(edge_fluid, (const char *)msg, (int)size, nullptr, nullptr, nullptr, 0);
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
    fluid_synth_write_float(edge_fluid, (int)length / 2 / sizeof(float), stream, 0, 2, stream + sizeof(float), 0, 2);
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
    FluidInterface *fluid_interface;
    FluidSequencer *fluid_sequencer;
} ma_fluid;

static void FluidSequencerInit(ma_fluid *synth)
{
    EPI_CLEAR_MEMORY(synth->fluid_interface, FluidInterface, 1);
    synth->fluid_interface->rtUserData           = NULL;
    synth->fluid_interface->rt_noteOn            = rtNoteOn;
    synth->fluid_interface->rt_noteOff           = rtNoteOff;
    synth->fluid_interface->rt_noteAfterTouch    = rtNoteAfterTouch;
    synth->fluid_interface->rt_channelAfterTouch = rtChannelAfterTouch;
    synth->fluid_interface->rt_controllerChange  = rtControllerChange;
    synth->fluid_interface->rt_patchChange       = rtPatchChange;
    synth->fluid_interface->rt_pitchBend         = rtPitchBend;
    synth->fluid_interface->rt_systemExclusive   = rtSysEx;

    synth->fluid_interface->onPcmRender          = playSynth;
    synth->fluid_interface->onPcmRender_userdata = NULL;

    synth->fluid_interface->pcmSampleRate = sound_device_frequency;
    synth->fluid_interface->pcmFrameSize  = 2 /*channels*/ * sizeof(float) /*size of one sample*/;

    synth->fluid_interface->rt_deviceSwitch  = rtDeviceSwitch;
    synth->fluid_interface->rt_currentDevice = rtCurrentDevice;

    synth->fluid_sequencer->SetInterface(synth->fluid_interface);
}

static ma_result ma_fluid_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fluid* pFluid);
static ma_result ma_fluid_init_memory(const void* pData, size_t dataSize, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fluid* pFluid);
static void ma_fluid_uninit(ma_fluid* pFluid, const ma_allocation_callbacks* pAllocationCallbacks);
static ma_result ma_fluid_read_pcm_frames(ma_fluid* pFluid, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
static ma_result ma_fluid_seek_to_pcm_frame(ma_fluid* pFluid, ma_uint64 frameIndex);
static ma_result ma_fluid_get_data_format(ma_fluid* pFluid, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
static ma_result ma_fluid_get_cursor_in_pcm_frames(ma_fluid* pFluid, ma_uint64* pCursor);
static ma_result ma_fluid_get_length_in_pcm_frames(ma_fluid* pFluid, ma_uint64* pLength);

static ma_result ma_fluid_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_fluid_read_pcm_frames((ma_fluid*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_fluid_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_fluid_seek_to_pcm_frame((ma_fluid*)pDataSource, frameIndex);
}

static ma_result ma_fluid_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_fluid_get_data_format((ma_fluid*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_fluid_ds_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_fluid_get_cursor_in_pcm_frames((ma_fluid*)pDataSource, pCursor);
}

static ma_result ma_fluid_ds_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_fluid_get_length_in_pcm_frames((ma_fluid*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_fluid_ds_vtable =
{
    ma_fluid_ds_read,
    ma_fluid_ds_seek,
    ma_fluid_ds_get_data_format,
    ma_fluid_ds_get_cursor,
    ma_fluid_ds_get_length,
    NULL,   /* onSetLooping */
    0
};

static ma_result ma_fluid_init_internal(const ma_decoding_backend_config* pConfig, ma_fluid* pFluid)
{
    ma_result result;
    ma_data_source_config dataSourceConfig;

    EPI_UNUSED(pConfig);

    if (pFluid == NULL) {
        return MA_INVALID_ARGS;
    }

    EPI_CLEAR_MEMORY(pFluid, ma_fluid, 1);
    pFluid->format = ma_format_f32;    /* Only supporting f32. */

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_fluid_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pFluid->ds);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

static ma_result ma_fluid_post_init(ma_fluid* pFluid)
{
    EPI_ASSERT(pFluid != NULL);

    pFluid->channels   = 2;
    pFluid->sampleRate = sound_device_frequency;

    return MA_SUCCESS;
}

static ma_result ma_fluid_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fluid* pFluid)
{
    if (fluid_disabled || edge_fluid == NULL)
        return MA_ERROR;

    EPI_UNUSED(pAllocationCallbacks);

    ma_result result;

    result = ma_fluid_init_internal(pConfig, pFluid);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (onRead == NULL || onSeek == NULL) {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pFluid->onRead = onRead;
    pFluid->onSeek = onSeek;
    pFluid->onTell = onTell;
    pFluid->pReadSeekTellUserData = pReadSeekTellUserData;

    return MA_SUCCESS;
}

static ma_result ma_fluid_init_memory(const void* pData, size_t dataSize, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fluid* pFluid)
{
    ma_result result;

    result = ma_fluid_init_internal(pConfig, pFluid);
    if (result != MA_SUCCESS) {
        return result;
    }

    EPI_UNUSED(pAllocationCallbacks);

    pFluid->fluid_sequencer = new FluidSequencer;
    pFluid->fluid_interface = new FluidInterface;

    FluidSequencerInit(pFluid);

    if (!pFluid->fluid_sequencer->LoadMidi((const uint8_t *)pData, dataSize)) {
        return MA_INVALID_FILE;
    }

    result = ma_fluid_post_init(pFluid);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_fluid_uninit(ma_fluid* pFluid, const ma_allocation_callbacks* pAllocationCallbacks)
{
    EPI_UNUSED(pAllocationCallbacks);

    if (pFluid == NULL) {
        return;
    }

    delete pFluid->fluid_interface;
    pFluid->fluid_interface = NULL;
    delete pFluid->fluid_sequencer;
    pFluid->fluid_sequencer = NULL;

    ma_data_source_uninit(&pFluid->ds);
}

static ma_result ma_fluid_read_pcm_frames(ma_fluid* pFluid, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (frameCount == 0) {
        return MA_INVALID_ARGS;
    }

    if (pFluid == NULL || pFluid->fluid_sequencer == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We always use floating point format. */
    ma_result result = MA_SUCCESS;  /* Must be initialized to MA_SUCCESS. */
    ma_uint64 totalFramesRead = 0;
    ma_format format;
    ma_uint32 channels;

    ma_fluid_get_data_format(pFluid, &format, &channels, NULL, NULL, 0);

    if (format == ma_format_f32) {
        totalFramesRead = pFluid->fluid_sequencer->PlayStream((uint8_t *)pFramesOut, frameCount * 2 * sizeof(float)) / 2 / sizeof(float);
    } else {
        result = MA_INVALID_ARGS;
    }

    pFluid->cursor += totalFramesRead;

    if (pFramesRead != NULL) {
        *pFramesRead = totalFramesRead;
    }

    if (result == MA_SUCCESS && pFluid->fluid_sequencer->PositionAtEnd()) {
        result  = MA_AT_END;
    }

    return result;
}

static ma_result ma_fluid_seek_to_pcm_frame(ma_fluid* pFluid, ma_uint64 frameIndex)
{
    if (pFluid == NULL || frameIndex != 0 || pFluid->fluid_sequencer == NULL) {
        return MA_INVALID_ARGS;
    }

    pFluid->fluid_sequencer->Rewind();

    pFluid->cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result ma_fluid_get_data_format(ma_fluid* pFluid, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
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

    if (pFluid == NULL) {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL) {
        *pFormat = pFluid->format;
    }

    if (pChannels != NULL) {
        *pChannels = pFluid->channels;
    }

    if (pSampleRate != NULL) {
        *pSampleRate = pFluid->sampleRate;
    }

    if (pChannelMap != NULL) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, pFluid->channels);
    }

    return MA_SUCCESS;
}

static ma_result ma_fluid_get_cursor_in_pcm_frames(ma_fluid* pFluid, ma_uint64* pCursor)
{
    if (pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;   /* Safety. */

    if (pFluid == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = pFluid->cursor;

    return MA_SUCCESS;
}

static ma_result ma_fluid_get_length_in_pcm_frames(ma_fluid* pFluid, ma_uint64* pLength)
{
    if (pLength == NULL) {
        return MA_INVALID_ARGS;
    }

    *pLength = 0;   /* Safety. */

    if (pFluid == NULL) {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init__fluid(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_fluid* pFluid;

    EPI_UNUSED(pUserData);    /* For now not using pUserData, but once we start storing the vorbis decoder state within the ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pFluid = (ma_fluid*)ma_malloc(sizeof(*pFluid), pAllocationCallbacks);
    if (pFluid == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_fluid_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pFluid);
    if (result != MA_SUCCESS) {
        ma_free(pFluid, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pFluid;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init_memory__fluid(void* pUserData, const void* pData, size_t dataSize, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_fluid* pFluid;

    EPI_UNUSED(pUserData);    /* For now not using pUserData, but once we start storing the vorbis decoder state within the ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pFluid = (ma_fluid*)ma_malloc(sizeof(*pFluid), pAllocationCallbacks);
    if (pFluid == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_fluid_init_memory(pData, dataSize, pConfig, pAllocationCallbacks, pFluid);
    if (result != MA_SUCCESS) {
        ma_free(pFluid, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pFluid;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__fluid(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_fluid* pFluid = (ma_fluid*)pBackend;

    EPI_UNUSED(pUserData);

    ma_fluid_uninit(pFluid, pAllocationCallbacks);
    ma_free(pFluid, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_fluid =
{
    ma_decoding_backend_init__fluid,
    NULL, // onInitFile()
    NULL, // onInitFileW()
    ma_decoding_backend_init_memory__fluid,
    ma_decoding_backend_uninit__fluid
};

static ma_decoding_backend_vtable *fluid_custom_vtable = &g_ma_decoding_backend_vtable_fluid;

bool StartupFluid(void)
{
    LogPrint("Initializing Fluidlite...\n");

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
            FatalError("Fluidlite: Cannot locate default soundfont (Default.sf2)! "
                       "Please check the /soundfont directory "
                       "of your EDGE-Classic install!\n");
    }

    // Initialize settings and change values from default if needed
    fluid_set_log_function(FLUID_PANIC, FluidError, nullptr);
    fluid_set_log_function(FLUID_ERR, nullptr, nullptr);
    fluid_set_log_function(FLUID_WARN, nullptr, nullptr);
    fluid_set_log_function(FLUID_DBG, nullptr, nullptr);
    edge_fluid_settings = new_fluid_settings();
    fluid_settings_setstr(edge_fluid_settings, "synth.reverb.active", "no");
    fluid_settings_setstr(edge_fluid_settings, "synth.chorus.active", "no");
    fluid_settings_setnum(edge_fluid_settings, "synth.gain", fluid_player_gain.f_);
    fluid_settings_setnum(edge_fluid_settings, "synth.sample-rate", sound_device_frequency);
    fluid_settings_setnum(edge_fluid_settings, "synth.audio-channels", 2);
    fluid_settings_setnum(edge_fluid_settings, "synth.polyphony", 64);
    edge_fluid = new_fluid_synth(edge_fluid_settings);

    // Register loader that uses our custom function to provide
    // a FILE pointer
    edge_fluid_sf2_loader          = new_fluid_defsfloader();
    edge_fluid_sf2_loader->fileapi = new fluid_fileapi_t;
    fluid_init_default_fileapi(edge_fluid_sf2_loader->fileapi);
    edge_fluid_sf2_loader->fileapi->fopen = edge_fluid_fopen;
    fluid_synth_add_sfloader(edge_fluid, edge_fluid_sf2_loader);

    if (fluid_synth_sfload(edge_fluid, midi_soundfont.c_str(), 1) == -1)
    {
        LogWarning("FluidLite: Initialization failure.\n");
        delete_fluid_synth(edge_fluid);
        delete_fluid_settings(edge_fluid_settings);
        return false;
    }

    fluid_synth_program_reset(edge_fluid);

    return true; // OK!
}

// Should only be invoked when switching soundfonts
void RestartFluid(void)
{
    if (fluid_disabled)
        return;

    LogPrint("Restarting Fluidlite...\n");

    int old_entry = entry_playing;

    StopMusic();

    delete_fluid_synth(edge_fluid);
    delete_fluid_settings(edge_fluid_settings);
    edge_fluid            = nullptr;
    edge_fluid_settings   = nullptr;
    edge_fluid_sf2_loader = nullptr; // This is already deleted upon invoking delete_fluid_synth

    if (!StartupFluid())
    {
        fluid_disabled = true;
        return;
    }

    ChangeMusic(old_entry,
                true); // Restart track that was playing when switched

    return;            // OK!
}

class FluidPlayer : public AbstractMusicPlayer
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

    ma_decoder fluid_decoder_;
    ma_sound   fluid_stream_;

  public:
    FluidPlayer(bool looping) : status_(kNotLoaded), looping_(looping)
    {
        EPI_CLEAR_MEMORY(&fluid_decoder_, ma_decoder, 1);
        EPI_CLEAR_MEMORY(&fluid_stream_, ma_sound, 1);
    }

    ~FluidPlayer()
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
        decode_config.ppCustomBackendVTables = &fluid_custom_vtable;

        if (ma_decoder_init_memory(data, length, &decode_config, &fluid_decoder_) != MA_SUCCESS)
        {
            LogWarning("Failed to load MIDI music\n");
            return false;
        }

        if (ma_sound_init_from_data_source(&music_engine, &fluid_decoder_, MA_SOUND_FLAG_NO_PITCH|MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_UNKNOWN_LENGTH|MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &fluid_stream_) != MA_SUCCESS)
        {
            ma_decoder_uninit(&fluid_decoder_);
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

        ma_sound_uninit(&fluid_stream_);

        ma_decoder_uninit(&fluid_decoder_);

        status_ = kNotLoaded;
    }

    void Play(bool loop)
    {
        looping_ = loop;

        ma_sound_set_looping(&fluid_stream_, looping_ ? MA_TRUE : MA_FALSE);

        // Let 'er rip (maybe)
        if (playing_movie)
            status_ = kPaused;
        else
        {
            status_  = kPlaying;
            ma_sound_start(&fluid_stream_);
        }
    }

    void Stop(void)
    {
        if (status_ != kPlaying && status_ != kPaused)
            return;

        ma_sound_stop(&fluid_stream_);

        fluid_synth_all_voices_stop(edge_fluid);

        status_ = kStopped;
    }

    void Pause(void)
    {
        if (status_ != kPlaying)
            return;

        fluid_synth_all_voices_pause(edge_fluid);

        ma_sound_stop(&fluid_stream_);

        status_ = kPaused;
    }

    void Resume(void)
    {
        if (status_ != kPaused)
            return;

        ma_sound_start(&fluid_stream_);

        status_ = kPlaying;
    }

    void Ticker(void)
    {
        ma_engine_set_volume(&music_engine, music_volume.f_ * 0.25f);

        if (fluid_player_gain.CheckModified())
        {
            fluid_player_gain.f_ = HMM_Clamp(0.0, fluid_player_gain.f_, 2.0f);
            fluid_player_gain    = fluid_player_gain.f_;
            fluid_synth_set_gain(edge_fluid, fluid_player_gain.f_);
        }

        if (status_ == kPlaying)
        {
            if (pc_speaker_mode)
                Stop();
            if (ma_sound_at_end(&fluid_stream_)) // This should only be true if finished and not set to looping
                Stop();
        }
    }
};

AbstractMusicPlayer *PlayFluidMusic(uint8_t *data, int length, bool loop)
{
    if (fluid_disabled)
    {
        delete[] data;
        return nullptr;
    }

    FluidPlayer *player = new FluidPlayer(loop);

    if (!player)
    {
        LogDebug("Fluidlite player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    if (!player->OpenMemory(data,
                           length)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("Fluidlite player: failed to load MIDI file!\n");
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
