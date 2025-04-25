//----------------------------------------------------------------------------
//  EDGE MIDI Music Player
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

#include <stdint.h>

#include <set>

#include "HandmadeMath.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "fluidlite.h"
#include "i_movie.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_misc.h"
#include "opalmidi.h"
#include "s_blit.h"
#include "s_midi.h"
#include "s_midi_seq.h"
#include "s_music.h"
#include "w_files.h"

extern int sound_device_frequency;

bool midi_disabled = false;

static fluid_synth_t    *edge_fluid            = nullptr;
static fluid_settings_t *edge_fluid_settings   = nullptr;
static fluid_sfloader_t *edge_fluid_sf2_loader = nullptr;
static int               edge_fluid_sf2_index  = -1;
static OPLPlayer        *edge_opl              = nullptr;
static bool              opl_playback          = false;
static uint16_t          imf_rate              = 0;

EDGE_DEFINE_CONSOLE_VARIABLE(midi_soundfont, "Default", kConsoleVariableFlagArchive)

EDGE_DEFINE_CONSOLE_VARIABLE(fluidlite_gain, "0.6", kConsoleVariableFlagArchive)

extern std::set<std::string> available_soundfonts;

static constexpr uint8_t kFluidOk     = 0;
static constexpr int8_t  kFluidFailed = -1;

static void FluidError(int level, char *message, void *data)
{
    EPI_UNUSED(level);
    EPI_UNUSED(data);
    FatalError("Fluidlite: %s\n", message);
}

static void *edge_fluid_fopen(fluid_fileapi_t *fileapi, const char *filename)
{
    EPI_UNUSED(fileapi);
    epi::File *fp = nullptr;
    // If default, look for SNDFONT. This can be a lump or pack file
    if (epi::StringCompare(filename, "Default") == 0)
    {
        int      raw_length = 0;
        uint8_t *raw_sf2    = OpenPackOrLumpInMemory("SNDFONT", {".sf2", ".sf3"}, &raw_length);
        if (raw_sf2)
        {
            fp = new epi::MemFile(raw_sf2, raw_length);
            delete[] raw_sf2;
        }
    }
    else // Check home, then game directory for SF2/SF3 file
    {
        std::string soundfont_dir = epi::PathAppend(home_directory, "soundfont");
        std::string sf_check      = epi::PathAppend(soundfont_dir, filename);
        epi::ReplaceExtension(sf_check, ".sf2");
        if (epi::FileExists(sf_check))
            fp = epi::FileOpen(sf_check, epi::kFileAccessRead | epi::kFileAccessBinary);
        else
        {
            epi::ReplaceExtension(sf_check, ".sf3");
            if (epi::FileExists(sf_check))
                fp = epi::FileOpen(sf_check, epi::kFileAccessRead | epi::kFileAccessBinary);
        }
        if (!fp && home_directory != game_directory)
        {
            soundfont_dir = epi::PathAppend(game_directory, "soundfont");
            sf_check      = epi::PathAppend(soundfont_dir, filename);
            epi::ReplaceExtension(sf_check, ".sf2");
            if (epi::FileExists(sf_check))
                fp = epi::FileOpen(sf_check, epi::kFileAccessRead | epi::kFileAccessBinary);
            else
            {
                epi::ReplaceExtension(sf_check, ".sf3");
                if (epi::FileExists(sf_check))
                    fp = epi::FileOpen(sf_check, epi::kFileAccessRead | epi::kFileAccessBinary);
            }
        }
    }

    return fp;
}

static int edge_fluid_fread(void *buf, int count, void *handle)
{
    if (count < 0)
        return kFluidFailed;
    epi::File *fp = (epi::File *)handle;
    if (fp->Read(buf, count) == (unsigned int)count)
        return kFluidOk;
    else
        return kFluidFailed;
}

static int edge_fluid_fclose(void *handle)
{
    epi::File *fp = (epi::File *)handle;
    delete fp;
    fp = nullptr;
    return kFluidOk;
}

static long edge_fluid_ftell(void *handle)
{
    epi::File *fp  = (epi::File *)handle;
    long       ret = fp->GetPosition();
    if (ret == -1)
        return kFluidFailed;
    return ret;
}

static int edge_fluid_free(fluid_fileapi_t *fileapi)
{
    if (fileapi)
        delete fileapi;
    return kFluidOk;
}

static int edge_fluid_fseek(void *handle, long offset, int origin)
{
    epi::File *fp       = (epi::File *)handle;
    bool       did_seek = false;
    switch (origin)
    {
    case SEEK_SET:
        did_seek = fp->Seek(offset, epi::File::kSeekpointStart);
        break;
    case SEEK_CUR:
        did_seek = fp->Seek(offset, epi::File::kSeekpointCurrent);
        break;
    case SEEK_END:
        did_seek = fp->Seek(-offset, epi::File::kSeekpointEnd);
        break;
    default:
        break;
    }
    if (did_seek)
        return kFluidOk;
    else
        return kFluidFailed;
}

void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
{
    EPI_UNUSED(userdata);
    if (opl_playback)
        edge_opl->midiNoteOn(channel, note, velocity);
    else
        fluid_synth_noteon(edge_fluid, channel, note, velocity);
}

void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
{
    EPI_UNUSED(userdata);
    if (opl_playback)
        edge_opl->midiNoteOff(channel, note);
    else
        fluid_synth_noteoff(edge_fluid, channel, note);
}

void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
{
    EPI_UNUSED(userdata);
    if (!opl_playback)
        fluid_synth_key_pressure(edge_fluid, channel, note, atVal);
}

void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
{
    EPI_UNUSED(userdata);
    if (!opl_playback)
        fluid_synth_channel_pressure(edge_fluid, channel, atVal);
}

void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
{
    EPI_UNUSED(userdata);
    if (opl_playback)
        edge_opl->midiControlChange(channel, type, value);
    else
        fluid_synth_cc(edge_fluid, channel, type, value);
}

void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
{
    EPI_UNUSED(userdata);
    if (opl_playback)
        edge_opl->midiProgramChange(channel, patch);
    else
        fluid_synth_program_change(edge_fluid, channel, patch);
}

void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
{
    EPI_UNUSED(userdata);
    if (opl_playback)
        edge_opl->midiPitchControl(channel, (msb - 64) / 127.0);
    else
        fluid_synth_pitch_bend(edge_fluid, channel, (msb << 7) | lsb);
}

void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
{
    EPI_UNUSED(userdata);
    if (!opl_playback)
        fluid_synth_sysex(edge_fluid, (const char *)msg, (int)size, nullptr, nullptr, nullptr, 0);
}

static void rtRawOPL(void *userdata, uint8_t reg, uint8_t value)
{
    EPI_UNUSED(userdata);
    edge_opl->midiRawOPL(reg, value);
}

void playSynth(void *userdata, uint8_t *stream, size_t length)
{
    EPI_UNUSED(userdata);
    if (opl_playback)
        edge_opl->generate((int16_t *)(stream), length / (2 * sizeof(int16_t)));
    else
        fluid_synth_write_float(edge_fluid, (int)length / 2 / sizeof(float), stream, 0, 2, stream + sizeof(float), 0,
                                2);
}

static MidiRealTimeInterface *midi_interface = nullptr;
static MidiSequencer         *midi_sequencer = nullptr;

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
} ma_midi;

static ma_result ma_midi_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell,
                              void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig,
                              const ma_allocation_callbacks *pAllocationCallbacks, ma_midi *pMIDI);
static ma_result ma_midi_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                     const ma_allocation_callbacks *pAllocationCallbacks, ma_midi *pMIDI);
static void      ma_midi_uninit(ma_midi *pMIDI, const ma_allocation_callbacks *pAllocationCallbacks);
static ma_result ma_midi_read_pcm_frames(ma_midi *pMIDI, void *pFramesOut, ma_uint64 frameCount,
                                         ma_uint64 *pFramesRead);
static ma_result ma_midi_seek_to_pcm_frame(ma_midi *pMIDI, ma_uint64 frameIndex);
static ma_result ma_midi_get_data_format(const ma_midi *pMIDI, ma_format *pFormat, ma_uint32 *pChannels,
                                         ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);
static ma_result ma_midi_get_cursor_in_pcm_frames(const ma_midi *pMIDI, ma_uint64 *pCursor);
static ma_result ma_midi_get_length_in_pcm_frames(const ma_midi *pMIDI, ma_uint64 *pLength);

static ma_result ma_midi_ds_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount,
                                 ma_uint64 *pFramesRead)
{
    return ma_midi_read_pcm_frames((ma_midi *)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_midi_ds_seek(ma_data_source *pDataSource, ma_uint64 frameIndex)
{
    return ma_midi_seek_to_pcm_frame((ma_midi *)pDataSource, frameIndex);
}

static ma_result ma_midi_ds_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels,
                                            ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap)
{
    return ma_midi_get_data_format((ma_midi *)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_midi_ds_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor)
{
    return ma_midi_get_cursor_in_pcm_frames((ma_midi *)pDataSource, pCursor);
}

static ma_result ma_midi_ds_get_length(ma_data_source *pDataSource, ma_uint64 *pLength)
{
    return ma_midi_get_length_in_pcm_frames((ma_midi *)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_midi_ds_vtable = {ma_midi_ds_read,
                                                    ma_midi_ds_seek,
                                                    ma_midi_ds_get_data_format,
                                                    ma_midi_ds_get_cursor,
                                                    ma_midi_ds_get_length,
                                                    NULL, /* onSetLooping */
                                                    0};

static ma_result ma_midi_init_internal(const ma_decoding_backend_config *pConfig, ma_midi *pMIDI)
{
    ma_result             result;
    ma_data_source_config dataSourceConfig;

    EPI_UNUSED(pConfig);

    if (pMIDI == NULL)
    {
        return MA_INVALID_ARGS;
    }

    EPI_CLEAR_MEMORY(pMIDI, ma_midi, 1);

    if (opl_playback)
        pMIDI->format = ma_format_s16;
    else
        pMIDI->format = ma_format_f32;

    dataSourceConfig        = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_midi_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pMIDI->ds);
    if (result != MA_SUCCESS)
    {
        return result; /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

static ma_result ma_midi_post_init(ma_midi *pMIDI)
{
    EPI_ASSERT(pMIDI != NULL);

    pMIDI->channels   = 2;
    pMIDI->sampleRate = sound_device_frequency;

    return MA_SUCCESS;
}

static ma_result ma_midi_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell,
                              void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig,
                              const ma_allocation_callbacks *pAllocationCallbacks, ma_midi *pMIDI)
{
    if (midi_disabled || edge_fluid == NULL)
        return MA_ERROR;

    if (edge_opl == NULL)
        return MA_ERROR;

    EPI_UNUSED(pAllocationCallbacks);

    ma_result result;

    result = ma_midi_init_internal(pConfig, pMIDI);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    if (onRead == NULL || onSeek == NULL)
    {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pMIDI->onRead                = onRead;
    pMIDI->onSeek                = onSeek;
    pMIDI->onTell                = onTell;
    pMIDI->pReadSeekTellUserData = pReadSeekTellUserData;

    return MA_SUCCESS;
}

static ma_result ma_midi_init_memory(const void *pData, size_t dataSize, const ma_decoding_backend_config *pConfig,
                                     const ma_allocation_callbacks *pAllocationCallbacks, ma_midi *pMIDI)
{
    ma_result result;

    result = ma_midi_init_internal(pConfig, pMIDI);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    EPI_UNUSED(pAllocationCallbacks);

    midi_interface->pcmFrameSize = 2 * (opl_playback ? sizeof(int16_t) : sizeof(float));
    midi_sequencer->SetInterface(midi_interface);

    if (!midi_sequencer->LoadMidi((const uint8_t *)pData, dataSize, imf_rate))
    {
        return MA_INVALID_FILE;
    }

    result = ma_midi_post_init(pMIDI);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_midi_uninit(ma_midi *pMIDI, const ma_allocation_callbacks *pAllocationCallbacks)
{
    EPI_UNUSED(pAllocationCallbacks);

    if (pMIDI == NULL)
    {
        return;
    }

    ma_data_source_uninit(&pMIDI->ds);
}

static ma_result ma_midi_read_pcm_frames(ma_midi *pMIDI, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead)
{
    if (pFramesRead != NULL)
    {
        *pFramesRead = 0;
    }

    if (frameCount == 0)
    {
        return MA_INVALID_ARGS;
    }

    if (pMIDI == NULL)
    {
        return MA_INVALID_ARGS;
    }

    /* We always use floating point format. */
    ma_result result          = MA_SUCCESS; /* Must be initialized to MA_SUCCESS. */
    ma_uint64 totalFramesRead = 0;
    ma_format format;
    ma_uint32 channels;

    ma_midi_get_data_format(pMIDI, &format, &channels, NULL, NULL, 0);

    if (format == ma_format_f32)
    {
        totalFramesRead =
            midi_sequencer->PlayStream((uint8_t *)pFramesOut, frameCount * 2 * sizeof(float)) / 2 / sizeof(float);
    }
    else if (format == ma_format_s16)
    {
        totalFramesRead =
            midi_sequencer->PlayStream((uint8_t *)pFramesOut, frameCount * 2 * sizeof(int16_t)) / 2 / sizeof(int16_t);
    }
    else
    {
        result = MA_INVALID_ARGS;
    }

    pMIDI->cursor += totalFramesRead;

    if (pFramesRead != NULL)
    {
        *pFramesRead = totalFramesRead;
    }

    if (result == MA_SUCCESS && midi_sequencer->PositionAtEnd())
    {
        result = MA_AT_END;
    }

    return result;
}

static ma_result ma_midi_seek_to_pcm_frame(ma_midi *pMIDI, ma_uint64 frameIndex)
{
    if (pMIDI == NULL || frameIndex != 0)
    {
        return MA_INVALID_ARGS;
    }

    midi_sequencer->Rewind();

    pMIDI->cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result ma_midi_get_data_format(const ma_midi *pMIDI, ma_format *pFormat, ma_uint32 *pChannels,
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

    if (pMIDI == NULL)
    {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL)
    {
        *pFormat = pMIDI->format;
    }

    if (pChannels != NULL)
    {
        *pChannels = pMIDI->channels;
    }

    if (pSampleRate != NULL)
    {
        *pSampleRate = pMIDI->sampleRate;
    }

    if (pChannelMap != NULL)
    {
        ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, pMIDI->channels);
    }

    return MA_SUCCESS;
}

static ma_result ma_midi_get_cursor_in_pcm_frames(const ma_midi *pMIDI, ma_uint64 *pCursor)
{
    if (pCursor == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0; /* Safety. */

    if (pMIDI == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pCursor = pMIDI->cursor;

    return MA_SUCCESS;
}

static ma_result ma_midi_get_length_in_pcm_frames(const ma_midi *pMIDI, ma_uint64 *pLength)
{
    if (pLength == NULL)
    {
        return MA_INVALID_ARGS;
    }

    *pLength = 0; /* Safety. */

    if (pMIDI == NULL)
    {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init__midi(void *pUserData, ma_read_proc onRead, ma_seek_proc onSeek,
                                                ma_tell_proc onTell, void *pReadSeekTellUserData,
                                                const ma_decoding_backend_config *pConfig,
                                                const ma_allocation_callbacks    *pAllocationCallbacks,
                                                ma_data_source                  **ppBackend)
{
    ma_result result;
    ma_midi  *pMIDI;

    EPI_UNUSED(pUserData); /* For now not using pUserData, but once we start storing the vorbis decoder state within the
                              ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pMIDI = (ma_midi *)ma_malloc(sizeof(*pMIDI), pAllocationCallbacks);
    if (pMIDI == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_midi_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pMIDI);
    if (result != MA_SUCCESS)
    {
        ma_free(pMIDI, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pMIDI;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init_memory__midi(void *pUserData, const void *pData, size_t dataSize,
                                                       const ma_decoding_backend_config *pConfig,
                                                       const ma_allocation_callbacks    *pAllocationCallbacks,
                                                       ma_data_source                  **ppBackend)
{
    ma_result result;
    ma_midi  *pMIDI;

    EPI_UNUSED(pUserData); /* For now not using pUserData, but once we start storing the vorbis decoder state within the
                              ma_decoder structure this will be set to the decoder so we can avoid a malloc. */

    /* For now we're just allocating the decoder backend on the heap. */
    pMIDI = (ma_midi *)ma_malloc(sizeof(*pMIDI), pAllocationCallbacks);
    if (pMIDI == NULL)
    {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_midi_init_memory(pData, dataSize, pConfig, pAllocationCallbacks, pMIDI);
    if (result != MA_SUCCESS)
    {
        ma_free(pMIDI, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pMIDI;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__midi(void *pUserData, ma_data_source *pBackend,
                                             const ma_allocation_callbacks *pAllocationCallbacks)
{
    ma_midi *pMIDI = (ma_midi *)pBackend;

    EPI_UNUSED(pUserData);

    ma_midi_uninit(pMIDI, pAllocationCallbacks);
    ma_free(pMIDI, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_midi = {ma_decoding_backend_init__midi,
                                                                       NULL, // onInitFile()
                                                                       NULL, // onInitFileW()
                                                                       ma_decoding_backend_init_memory__midi,
                                                                       ma_decoding_backend_uninit__midi};

static ma_decoding_backend_vtable *midi_custom_vtable = &g_ma_decoding_backend_vtable_midi;

static ma_decoder_config midi_decoder_config;

bool StartupMIDI(void)
{
    LogPrint("Initializing MIDI...\n");

    if (!midi_interface)
    {
        midi_interface = new MidiRealTimeInterface;
        EPI_CLEAR_MEMORY(midi_interface, MidiRealTimeInterface, 1);
        midi_interface->rtUserData           = NULL;
        midi_interface->rt_noteOn            = rtNoteOn;
        midi_interface->rt_noteOff           = rtNoteOff;
        midi_interface->rt_noteAfterTouch    = rtNoteAfterTouch;
        midi_interface->rt_channelAfterTouch = rtChannelAfterTouch;
        midi_interface->rt_controllerChange  = rtControllerChange;
        midi_interface->rt_patchChange       = rtPatchChange;
        midi_interface->rt_pitchBend         = rtPitchBend;
        midi_interface->rt_systemExclusive   = rtSysEx;
        midi_interface->rt_rawOPL            = rtRawOPL;

        midi_interface->onPcmRender          = playSynth;
        midi_interface->onPcmRender_userdata = NULL;

        midi_interface->pcmSampleRate = sound_device_frequency;
        midi_interface->pcmFrameSize  = 0; // will be set when track initializes
    }

    if (!midi_sequencer)
        midi_sequencer = new MidiSequencer;

    midi_decoder_config                        = ma_decoder_config_init_default();
    midi_decoder_config.customBackendCount     = 1;
    midi_decoder_config.pCustomBackendUserData = NULL;
    midi_decoder_config.ppCustomBackendVTables = &midi_custom_vtable;

    // Check for presence of previous CVAR value's file
    if (!available_soundfonts.count(midi_soundfont.s_))
    {
        LogWarning("MIDI: Cannot find previously used soundfont %s, falling back to "
                   "default!\n",
                   midi_soundfont.c_str());
        midi_soundfont = "Default";
    }

    bool add_loader = true;

    if (!edge_fluid_settings)
    {
        // Initialize settings and change values from default if needed
        fluid_set_log_function(FLUID_PANIC, FluidError, nullptr);
        fluid_set_log_function(FLUID_ERR, nullptr, nullptr);
        fluid_set_log_function(FLUID_WARN, nullptr, nullptr);
        fluid_set_log_function(FLUID_DBG, nullptr, nullptr);
        edge_fluid_settings = new_fluid_settings();
        fluid_settings_setstr(edge_fluid_settings, "synth.reverb.active", "no");
        fluid_settings_setstr(edge_fluid_settings, "synth.chorus.active", "no");
        fluid_settings_setnum(edge_fluid_settings, "synth.gain", fluidlite_gain.f_);
        fluid_settings_setnum(edge_fluid_settings, "synth.sample-rate", sound_device_frequency);
        fluid_settings_setnum(edge_fluid_settings, "synth.audio-channels", 2);
        fluid_settings_setnum(edge_fluid_settings, "synth.polyphony", 64);
    }

    if (!edge_fluid)
        edge_fluid = new_fluid_synth(edge_fluid_settings);
    else
        add_loader = false;

    // Register loader that uses our custom function to provide
    // a FILE pointer
    if (!edge_fluid_sf2_loader)
    {
        edge_fluid_sf2_loader          = new_fluid_defsfloader();
        edge_fluid_sf2_loader->fileapi = new fluid_fileapi_t;
        fluid_init_default_fileapi(edge_fluid_sf2_loader->fileapi);
        edge_fluid_sf2_loader->fileapi->fopen  = edge_fluid_fopen;
        edge_fluid_sf2_loader->fileapi->fclose = edge_fluid_fclose;
        edge_fluid_sf2_loader->fileapi->ftell  = edge_fluid_ftell;
        edge_fluid_sf2_loader->fileapi->fseek  = edge_fluid_fseek;
        edge_fluid_sf2_loader->fileapi->fread  = edge_fluid_fread;
        edge_fluid_sf2_loader->fileapi->free   = edge_fluid_free;
    }

    if (add_loader)
        fluid_synth_add_sfloader(edge_fluid, edge_fluid_sf2_loader);

    if (epi::StringCompare(midi_soundfont.s_, "OPL Emulation") != 0)
    {
        edge_fluid_sf2_index = fluid_synth_sfload(edge_fluid, midi_soundfont.c_str(), 1);
        if (edge_fluid_sf2_index == -1)
        {
            LogWarning("MIDI: Initialization failure.\n");
            delete_fluid_synth(edge_fluid);
            delete_fluid_settings(edge_fluid_settings);
            edge_fluid            = nullptr;
            edge_fluid_sf2_loader = nullptr; // already deleted when deleting the synth
            edge_fluid_settings   = nullptr;
            return false;
        }

        fluid_synth_program_reset(edge_fluid);
    }

    if (!edge_opl)
    {
        edge_opl = new OPLPlayer(sound_device_frequency);

        if (!edge_opl)
        {
            LogWarning("MIDI: Initialization failure.\n");
            delete_fluid_synth(edge_fluid);
            delete_fluid_settings(edge_fluid_settings);
            edge_fluid            = nullptr;
            edge_fluid_sf2_loader = nullptr; // already deleted when deleting the synth
            edge_fluid_settings   = nullptr;
            return false;
        }

        // Check for GENMIDI bank; this is not a failure if absent as OpalMIDI has
        // built-in instruments

        int      raw_length = 0;
        uint8_t *raw_bank   = OpenPackOrLumpInMemory("GENMIDI", {".wopl", ".op2", ".ad", ".opl", ".tmb"}, &raw_length);
        if (raw_bank)
        {
            if (!edge_opl->loadPatches((const uint8_t *)raw_bank, (size_t)raw_length))
            {
                LogWarning("MIDI: Error loading external OPL instruments! Falling back to default!\n");
                edge_opl->loadDefaultPatches();
            }
            delete[] raw_bank;
        }
        else
            edge_opl->loadDefaultPatches();
    }

    return true; // OK!
}

// Should only be invoked when switching soundfonts
void RestartMIDI(void)
{
    if (midi_disabled)
        return;

    LogPrint("Restarting MIDI...\n");

    int old_entry = entry_playing;

    StopMusic();
    // We only unload the Fluidlite font, OPL instruments are determined once on startup,
    // no need to reload; just reset it
    edge_opl->reset();
    if (edge_fluid_sf2_index > -1)
    {
        fluid_synth_sfunload(edge_fluid, edge_fluid_sf2_index, 1);
        edge_fluid_sf2_index = -1;
    }

    if (!StartupMIDI())
    {
        midi_disabled = true;
        return;
    }

    ChangeMusic(old_entry,
                true); // Restart track that was playing when switched

    return;            // OK!
}

static ma_decoder midi_decoder;
static ma_sound   midi_stream;

class MIDIPlayer : public AbstractMusicPlayer
{
  public:
    MIDIPlayer(bool looping)
    {
        status_  = kNotLoaded;
        looping_ = looping;
    }

    ~MIDIPlayer() override
    {
        Close();
    }

    bool OpenMemory(const uint8_t *data, int length)
    {
        if (status_ != kNotLoaded)
            Close();

        opl_playback = ((epi::StringCompare(midi_soundfont.s_, "OPL Emulation") == 0) || imf_rate > 0);

        if (opl_playback)
            edge_opl->reset();

        midi_decoder_config.format = opl_playback ? ma_format_s16 : ma_format_f32;

        if (ma_decoder_init_memory(data, length, &midi_decoder_config, &midi_decoder) != MA_SUCCESS)
        {
            LogWarning("Failed to load MIDI music\n");
            return false;
        }

        if (ma_sound_init_from_data_source(&music_engine, &midi_decoder,
                                           MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_STREAM |
                                               MA_SOUND_FLAG_UNKNOWN_LENGTH | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                           NULL, &midi_stream) != MA_SUCCESS)
        {
            ma_decoder_uninit(&midi_decoder);
            LogWarning("Failed to load MIDI music\n");
            return false;
        }

        // Loaded, but not playing
        status_ = kStopped;

        return true;
    }

    void Close(void) override
    {
        if (status_ == kNotLoaded)
            return;

        // Stop playback
        Stop();

        ma_sound_uninit(&midi_stream);

        ma_decoder_uninit(&midi_decoder);

        if (opl_playback)
            edge_opl->reset();
        else
            fluid_synth_system_reset(edge_fluid);

        status_ = kNotLoaded;
    }

    void Play(bool loop) override
    {
        looping_ = loop;

        ma_sound_set_looping(&midi_stream, looping_ ? MA_TRUE : MA_FALSE);

        // Let 'er rip (maybe)
        if (playing_movie)
            status_ = kPaused;
        else
        {
            status_ = kPlaying;
            ma_sound_start(&midi_stream);
        }
    }

    void Stop(void) override
    {
        if (status_ != kPlaying && status_ != kPaused)
            return;

        ma_sound_set_volume(&midi_stream, 0);
        ma_sound_stop(&midi_stream);

        // reset imf_rate in case tracks are switched to another format
        imf_rate = 0;

        status_ = kStopped;
    }

    void Pause(void) override
    {
        if (status_ != kPlaying)
            return;

        ma_sound_stop(&midi_stream);
        if (!opl_playback)
            fluid_synth_all_voices_pause(edge_fluid);

        status_ = kPaused;
    }

    void Resume(void) override
    {
        if (status_ != kPaused)
            return;

        ma_sound_start(&midi_stream);

        status_ = kPlaying;
    }

    void Ticker(void) override
    {
        if (fluidlite_gain.CheckModified())
        {
            fluidlite_gain.f_ = HMM_Clamp(0.0, fluidlite_gain.f_, 2.0f);
            fluidlite_gain    = fluidlite_gain.f_;
            fluid_synth_set_gain(edge_fluid, fluidlite_gain.f_);
        }

        if (status_ == kPlaying)
        {
            ma_engine_set_volume(&music_engine, music_volume.f_);

            if (pc_speaker_mode)
                Stop();
            if (ma_sound_at_end(&midi_stream)) // This should only be true if finished and not set to looping
                Stop();
        }
    }
};

AbstractMusicPlayer *PlayMIDIMusic(uint8_t *data, int length, bool loop)
{
    if (midi_disabled)
    {
        delete[] data;
        return nullptr;
    }

    MIDIPlayer *player = new MIDIPlayer(loop);

    if (!player)
    {
        LogDebug("MIDI player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    if (!player->OpenMemory(data,
                            length)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("MIDI player: failed to load MIDI file!\n");
        delete[] data;
        delete player;
        return nullptr;
    }

    delete[] data;

    player->Play(loop);

    return player;
}

AbstractMusicPlayer *PlayIMFMusic(uint8_t *data, int length, bool loop, int type)
{
    MIDIPlayer *player = new MIDIPlayer(loop);

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
