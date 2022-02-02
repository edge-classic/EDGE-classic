/*
* This file provides the interface to the JavaScript world (i.e. it 
* provides all the APIs expected by the "backend adapter").
*
* It also handles the output audio buffer logic.
*
*
* The general coding conventions used in this project are:
*
*  - type names use camelcase & start uppercase
*  - method/function names use camelcase & start lowercase
*  - vars use lowercase and _ delimiters
*  - private vars (instance vars, etc) start with _
*  - global constants and defines are all uppercase
*  - cross C-file APIs (for those older parts that still use C)
*    start with a prefix that reflects the file that provides them,
*    e.g. "vic...()" is provided by "vic.c".
*
* WebSid (c) 2019 Jürgen Wothke
* version 0.93
*
* Terms of Use: This software is licensed under a CC BY-NC-SA 
* (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

extern "C" {
#include "system.h"	
#include "vic.h"	
#include "core.h"
#include "memory.h"
}
#include "filter6581.h"
#include "sid.h"
extern "C" uint8_t	sidReadMem(uint16_t addr);
extern "C" void 	sidWriteMem(uint16_t addr, uint8_t value);

#include "loaders.h"	

static FileLoader*	_loader;

// --------- audio output buffer management ------------------------

// keep it down to one screen to allow for  
// more direct feedback to WebAudio side:
#define BUFLEN 96000/50	* 2
#define CHANNELS 2

static int16_t 		_soundBuffer[BUFLEN * CHANNELS];

// max 10 sids*4 voices (1 digi channel)
#define MAX_VOICES 			40					
#define MAX_SCOPE_BUFFERS 	40

// output "scope" streams corresponding to final audio buffer
static int16_t* 	_scope_buffers[MAX_SCOPE_BUFFERS];	

// these buffers are "per frame" i.e. 1 screen refresh, e.g. 822 samples
static int16_t* 	_synth_buffer = 0;
static int16_t** 	_synth_trace_buffers = 0;

static uint16_t 	_chunk_size; 	// number of samples per call

static uint32_t 	_number_of_samples_rendered = 0;
static uint32_t 	_number_of_samples_to_render = 0;

static uint8_t	 	_sound_started;
static uint8_t	 	_skip_silence_loop;

static uint32_t		_sample_rate;

static uint32_t		_trace_sid = 0;
static uint8_t		_ready_to_play = 0;


static void resetTimings(uint8_t is_ntsc) {
	vicSetModel(is_ntsc);	// see for timing details

	uint32_t clock_rate= 	sysGetClockRate(is_ntsc);
	uint8_t is_rsid=		FileLoader::isRSID();
	uint8_t is_compatible=	FileLoader::getCompatibility();
	
	WebSID::resetAll(_sample_rate, clock_rate, is_rsid, is_compatible);
}

static void resetScopeBuffers() {
	if (_scope_buffers[0] == 0) {
		// alloc once
		for (int i= 0; i<MAX_SCOPE_BUFFERS; i++) {
			_scope_buffers[i] = (int16_t*) calloc(sizeof(int16_t), BUFLEN);
		}
	} else {
		for (int i= 0; i<MAX_SCOPE_BUFFERS; i++) {
			// just to make sure there is no garbage left	
			memset(_scope_buffers[i], 0, sizeof(int16_t)*BUFLEN);
		}
	}
}

static void resetSynthBuffer(uint16_t size) {
	if (_synth_buffer) free(_synth_buffer);

	_synth_buffer= (int16_t*)malloc(sizeof(int16_t)*
						(size * CHANNELS + 1));
}

static void discardSynthTraceBuffers() {
	// trace output (corresponding to _synth_buffer)
	if (_synth_trace_buffers) {
		for (int i= 0; i<MAX_VOICES; i++) {
			if (_synth_trace_buffers[i]) {
				free(_synth_trace_buffers[i]);
				_synth_trace_buffers[i] = 0; 
			}
		}
		free(_synth_trace_buffers);
		_synth_trace_buffers = 0;
	}	
}

static void allocSynthTraceBuffers(uint16_t size) {
	// availability of _synth_trace_buffers controls 
	// if SID will generate the respective output
	
	if (_trace_sid) {	
		if (!_synth_trace_buffers) {
			_synth_trace_buffers = (int16_t**)calloc(sizeof(int16_t*), MAX_VOICES);
		}
		for (int i= 0; i<MAX_VOICES; i++) {
			_synth_trace_buffers[i] = (int16_t*)calloc(sizeof(int16_t), size + 1);			
		}		
		
	} else {
		if (_synth_trace_buffers) {
			for (int i= 0; i<MAX_VOICES; i++) {
				if (_synth_trace_buffers[i]) {
					free(_synth_trace_buffers[i]); 
					_synth_trace_buffers[i] = 0;
				}
			}		
			free(_synth_trace_buffers); 
			_synth_trace_buffers = 0; // disables respective SID rendering
		}
	}
}

static void resetSynthTraceBuffers(uint16_t size) {
	discardSynthTraceBuffers();
	allocSynthTraceBuffers(size);
}

static void resetAudioBuffers() {
	
	// number of samples corresponding to one simulated frame/
	// screen (granularity of emulation is 1 screen), e.g.	
	// NTSC: 735*60=44100		800*60=48000  
	// PAL: 882*50=44100		960*50=48000
	
	_chunk_size = _sample_rate / vicFramesPerSecond();

	resetScopeBuffers();
	resetSynthBuffer(_chunk_size);	
	resetSynthTraceBuffers(_chunk_size);
	
	_number_of_samples_rendered = 0;
	_number_of_samples_to_render = 0;	
}

#ifdef _MSC_VER
extern "C" uint8_t envSetNTSC(uint8_t is_ntsc);
#else
extern "C" uint8_t envSetNTSC(uint8_t is_ntsc)  __attribute__((noinline));
#endif
extern "C" uint8_t EMSCRIPTEN_KEEPALIVE envSetNTSC(uint8_t is_ntsc) {	
	resetTimings(is_ntsc);
	resetAudioBuffers();

	return 0;
}

// ----------------- generic handling -----------------------------------------

// This is driving the emulation: Each call to computeAudioSamples() delivers
// some fixed numberof audio samples and the necessary emulation timespan is
// derived from it:

#ifdef _MSC_VER
extern "C" int32_t computeAudioSamples();
#else
extern "C" int32_t computeAudioSamples()  __attribute__((noinline));
#endif
extern "C" int32_t EMSCRIPTEN_KEEPALIVE computeAudioSamples() {
	if(!_ready_to_play) return 0;
	
	uint8_t is_simple_sid_mode =	!FileLoader::isExtendedSidFile();
	int sid_voices =				WebSID::getNumberUsedChips() * 4;
	uint8_t speed =					FileLoader::getCurrentSongSpeed();

#ifdef TEST
	return 0;
#endif
	_number_of_samples_rendered = 0;
			
	uint32_t sample_buffer_idx = 0;
	
	while (_number_of_samples_rendered < _chunk_size) {
		if (_number_of_samples_to_render == 0) {
			_number_of_samples_to_render = _chunk_size;
			sample_buffer_idx = 0;
			
			// limit "skipping" so as not to make the browser unresponsive
			for (uint16_t i= 0; i<_skip_silence_loop; i++) { 
			
				Core::runOneFrame(is_simple_sid_mode, speed, _synth_buffer,
									_synth_trace_buffers, _chunk_size);
												
				if (!_sound_started) {
					if (WebSID::isAudible()) {
						_sound_started = 1;
						break;
					}
				} else {
					break;
				}			
			}			
		}
		
		if (_number_of_samples_rendered + _number_of_samples_to_render > _chunk_size) {
			uint32_t available_space = _chunk_size-_number_of_samples_rendered;
			
			memcpy(	&_soundBuffer[_number_of_samples_rendered],
					&_synth_buffer[sample_buffer_idx], 
					sizeof(int16_t) * available_space * CHANNELS);
				
			// In addition to the actual sample data played by WebAudio, buffers
			// containing raw voice data are also created here. These are 1:1 in
			// sync with the sample buffer, i.e. for each sample entry in the
			// sample buffer there is a corresponding entry in the additional 
			// buffers - which are all exactly the same size as the sample buffer. 
	
			if (_trace_sid) {
				// do the same for the respective voice traces
				for (int i= 0; i<sid_voices; i++) {
					if (is_simple_sid_mode || (sid_voices % 4) != 3) {	// no digi 
						memcpy(	&(_scope_buffers[i][_number_of_samples_rendered]), 
								&(_synth_trace_buffers[i][sample_buffer_idx]), 
								sizeof(int16_t) * available_space);
					}
				}
			}
			sample_buffer_idx += available_space;
			_number_of_samples_to_render -= available_space;
			_number_of_samples_rendered = _chunk_size;
		} else {
			memcpy(	&_soundBuffer[_number_of_samples_rendered],
					&_synth_buffer[sample_buffer_idx], 
					sizeof(int16_t) * CHANNELS * _number_of_samples_to_render);

			if (_trace_sid) {
				// do the same for the respecive voice traces
				for (int i= 0; i<sid_voices; i++) {
					if (is_simple_sid_mode || (sid_voices % 4) != 3) {	// no digi 
						memcpy(	&(_scope_buffers[i][_number_of_samples_rendered]),
								&(_synth_trace_buffers[i][sample_buffer_idx]),
								sizeof(int16_t) * _number_of_samples_to_render);
					}
				}
			}
			_number_of_samples_rendered += _number_of_samples_to_render;
			_number_of_samples_to_render = 0;
		} 
	}
	
	if (_loader->isTrackEnd()) { // "play" must have been called before 1st use of this check
		return -1;
	}
	
	return _number_of_samples_rendered;
}

#ifdef _MSC_VER
extern "C" uint32_t enableVoice(uint8_t sid_idx, uint8_t voice, uint8_t on);
#else
extern "C" uint32_t enableVoice(uint8_t sid_idx, uint8_t voice, uint8_t on)  __attribute__((noinline));
#endif
extern "C" uint32_t EMSCRIPTEN_KEEPALIVE enableVoice(uint8_t sid_idx, uint8_t voice, uint8_t on) {
	WebSID::setMute(sid_idx, voice, !on);
	
	return 0;
}

#ifdef _MSC_VER
extern "C" uint32_t sid_playTune(uint32_t selected_track, uint32_t trace_sid);
#else
extern "C" uint32_t playTune(uint32_t selected_track, uint32_t trace_sid)  __attribute__((noinline));
#endif
extern "C" uint32_t EMSCRIPTEN_KEEPALIVE sid_playTune(uint32_t selected_track, uint32_t trace_sid) {
	_ready_to_play = 0;
	_trace_sid = trace_sid; 
				
	_sound_started = 0;
		
	// note: crappy BASIC songs like Baroque_Music_64_BASIC take 100sec before
	// they even start playing.. unfortunately the emulation is NOT fast enough
	// (at least on my old PC) to just skip that phase in "no time" and if the
	// calculations take too long then they will block the browser completely
	
	// performing respective skipping-logic directly within INIT is a bad idea
	// since it will completely block the browser until it is completed. From
	// "UI responsiveness" perspective it is preferable to attempt a "speedup"
	// on limited slices from within the audio-rendering loop.
	
	// should keep the UI responsive; means that on a fast machine the above
	// garbage song will still take 10 secs before it plays
	_skip_silence_loop = 10;
	
	// XXX FIXME the separate handling of the INIT call is an annoying legacy
	// of the old impl.. respective PSID handling should better to moved into
	// the respective C64 side driver so that users of the emulator do not need
	// to handle this potentially long running emu scenario (see SID callbacks 
	// triggered on Raspberry SID device).
	_loader->initTune(_sample_rate, selected_track);

	resetAudioBuffers();

	_ready_to_play = 1;

	return 0;
}

#ifdef _MSC_VER
extern "C" uint32_t loadSidFile(uint32_t is_mus, void* in_buffer, uint32_t in_buf_size,
								uint32_t sample_rate, char* filename, void* basic_ROM,
								void* char_ROM, void* kernal_ROM);
#else
extern "C" uint32_t loadSidFile(uint32_t is_mus, void* in_buffer, uint32_t in_buf_size,
								uint32_t sample_rate, char* filename, void* basic_ROM,
								void* char_ROM, void* kernal_ROM)  __attribute__((noinline));
#endif
extern "C" uint32_t EMSCRIPTEN_KEEPALIVE loadSidFile(uint32_t is_mus, void* in_buffer, uint32_t in_buf_size,
								uint32_t sample_rate, char* filename, void* basic_ROM,
								void* char_ROM, void* kernal_ROM) {
	
	_ready_to_play = 0;											// stop any emulator use
    _sample_rate = sample_rate > 96000 ? 48000 : sample_rate; 	// see hardcoded BUFLEN

	_loader = FileLoader::getInstance(is_mus, in_buffer, in_buf_size);
	
	if (!_loader) return 1;	// error

	uint32_t result = _loader->load((uint8_t *)in_buffer, in_buf_size, filename,
									basic_ROM, char_ROM, kernal_ROM);
	if (!result) {
		uint8_t is_ntsc = FileLoader::getNTSCMode();
		envSetNTSC(is_ntsc);
	}
	return result;
}
#ifdef _MSC_VER
extern "C" char** sid_getMusicInfo();
#else
extern "C" char** getMusicInfo() __attribute__((noinline));
#endif
extern "C" char** EMSCRIPTEN_KEEPALIVE sid_getMusicInfo() {
	return FileLoader::getInfoStrings();
}
#ifdef _MSC_VER
extern "C" uint32_t getSoundBufferLen();
#else
extern "C" uint32_t getSoundBufferLen() __attribute__((noinline));
#endif
extern "C" uint32_t EMSCRIPTEN_KEEPALIVE getSoundBufferLen() {
	return _number_of_samples_rendered;	// in samples
}
#ifdef _MSC_VER
extern "C" char* getSoundBuffer();
#else
extern "C" char* getSoundBuffer() __attribute__((noinline));
#endif
extern "C" char* EMSCRIPTEN_KEEPALIVE getSoundBuffer() {
	return (char*) _soundBuffer;
}
#ifdef _MSC_VER
extern "C" uint32_t getSampleRate();
#else
extern "C" uint32_t getSampleRate() __attribute__((noinline));
#endif
extern "C" uint32_t EMSCRIPTEN_KEEPALIVE getSampleRate() {
	return _sample_rate;
}

// additional accessors that might be useful for tweaking defaults from the GUI
#ifdef _MSC_VER
extern "C" uint8_t envIsSID6581();
#else
extern "C" uint8_t envIsSID6581()  __attribute__((noinline));
#endif
extern "C" uint8_t EMSCRIPTEN_KEEPALIVE envIsSID6581() {
	return (uint8_t)WebSID::isSID6581();
}
#ifdef _MSC_VER
extern "C" uint8_t envSetSID6581(uint8_t is6581);
#else
extern "C" uint8_t envSetSID6581(uint8_t is6581)  __attribute__((noinline));
#endif
extern "C" uint8_t EMSCRIPTEN_KEEPALIVE envSetSID6581(uint8_t is6581) {
	return WebSID::setSID6581((bool)is6581);
}
#ifdef _MSC_VER
extern "C" uint8_t getDigiType();
#else
extern "C" uint8_t getDigiType()  __attribute__((noinline));
#endif
extern "C" uint8_t EMSCRIPTEN_KEEPALIVE getDigiType() {
	return WebSID::getGlobalDigiType();
}
#ifdef _MSC_VER
extern "C" const char* getDigiTypeDesc();
#else
extern "C" const char* getDigiTypeDesc()  __attribute__((noinline));
#endif
extern "C" const char* EMSCRIPTEN_KEEPALIVE getDigiTypeDesc() {
	return WebSID::getGlobalDigiTypeDesc();
}
#ifdef _MSC_VER
extern "C" uint16_t getDigiRate();
#else
extern "C" uint16_t getDigiRate()  __attribute__((noinline));
#endif
extern "C" uint16_t EMSCRIPTEN_KEEPALIVE getDigiRate() {
	return WebSID::getGlobalDigiRate()*vicFramesPerSecond();
}
#ifdef _MSC_VER
extern "C" uint8_t envIsNTSC();
#else
extern "C" uint8_t envIsNTSC()  __attribute__((noinline));
#endif
extern "C" uint8_t EMSCRIPTEN_KEEPALIVE envIsNTSC() {
	return FileLoader::getNTSCMode();
}
#ifdef _MSC_VER
extern "C" uint16_t getRegisterSID(uint16_t reg);
#else
extern "C" uint16_t getRegisterSID(uint16_t reg) __attribute__((noinline));
#endif
extern "C" uint16_t EMSCRIPTEN_KEEPALIVE getRegisterSID(uint16_t reg) {
	return  reg >= 0x1B ? sidReadMem(0xd400 + reg) : memReadIO(0xd400 + reg);
}
#ifdef _MSC_VER
extern "C" void setRegisterSID(uint16_t reg, uint8_t value);
#else
extern "C" void setRegisterSID(uint16_t reg, uint8_t value) __attribute__((noinline));
#endif
extern "C" void EMSCRIPTEN_KEEPALIVE setRegisterSID(uint16_t reg, uint8_t value) {
	sidWriteMem(0xd400 + reg, value);
}
#ifdef _MSC_VER
extern "C" uint16_t getRAM(uint16_t addr);
#else
extern "C" uint16_t getRAM(uint16_t addr) __attribute__((noinline));
#endif
extern "C" uint16_t EMSCRIPTEN_KEEPALIVE getRAM(uint16_t addr) {
	return  memReadRAM(addr);
}
#ifdef _MSC_VER
extern "C" void setRAM(uint16_t addr, uint8_t value);
#else
extern "C" void setRAM(uint16_t addr, uint8_t value) __attribute__((noinline));
#endif
extern "C" void EMSCRIPTEN_KEEPALIVE setRAM(uint16_t addr, uint8_t value) {
	memWriteRAM(addr, value);
}
#ifdef _MSC_VER
extern "C" uint16_t getGlobalDigiType();
#else
extern "C" uint16_t getGlobalDigiType() __attribute__((noinline));
#endif
extern "C" uint16_t EMSCRIPTEN_KEEPALIVE getGlobalDigiType() {
	return WebSID::getGlobalDigiType();
}
#ifdef _MSC_VER
extern "C" const char * getGlobalDigiTypeDesc();
#else
extern "C" const char * getGlobalDigiTypeDesc() __attribute__((noinline));
#endif
extern "C" const char * EMSCRIPTEN_KEEPALIVE getGlobalDigiTypeDesc() {
	uint16_t t = WebSID::getGlobalDigiType();
	return (t > 0) ? WebSID::getGlobalDigiTypeDesc() : "";
}
#ifdef _MSC_VER
extern "C" uint16_t getGlobalDigiRate();
#else
extern "C" uint16_t getGlobalDigiRate() __attribute__((noinline));
#endif
extern "C" uint16_t EMSCRIPTEN_KEEPALIVE getGlobalDigiRate() {
	uint16_t t = WebSID::getGlobalDigiType();
	return (t > 0) ? WebSID::getGlobalDigiRate() : 0;
}
#ifdef _MSC_VER
extern "C" int getNumberTraceStreams();
#else
extern "C" int getNumberTraceStreams() __attribute__((noinline));
#endif
extern "C" int EMSCRIPTEN_KEEPALIVE getNumberTraceStreams() {
	// always use additional stream for digi samples..
	return WebSID::getNumberUsedChips() * 4;
}
#ifdef _MSC_VER
extern "C" const char** getTraceStreams();
#else
extern "C" const char** getTraceStreams() __attribute__((noinline));
#endif
extern "C" const char** EMSCRIPTEN_KEEPALIVE getTraceStreams() {
	return (const char**)_scope_buffers;	// ugly cast to make emscripten happy
}
#ifdef _MSC_VER
extern "C" int setFilterConfig6581(double base, double max, double steepness, double x_offset, double distort, double distort_offset, double distort_scale, double distort_threshold, double kink);
#else
extern "C" int setFilterConfig6581(double base, double max, double steepness, double x_offset, double distort, double distort_offset, double distort_scale, double distort_threshold, double kink) __attribute__((noinline));
#endif
extern "C" int EMSCRIPTEN_KEEPALIVE setFilterConfig6581(double base, double max, double steepness, double x_offset, double distort, double distort_offset, double distort_scale, double distort_threshold, double kink) {
	return Filter6581::setFilterConfig6581(base, max, steepness, x_offset, distort, distort_offset, distort_scale, distort_threshold, kink);
}
#ifdef _MSC_VER
extern "C" double* getFilterConfig6581();
#else
extern "C" double* getFilterConfig6581() __attribute__((noinline));
#endif
extern "C" double* EMSCRIPTEN_KEEPALIVE getFilterConfig6581() {
	return Filter6581::getFilterConfig6581();
}
#ifdef _MSC_VER
extern "C" double* getCutoff6581(int slice);
#else
extern "C" double* getCutoff6581(int slice) __attribute__((noinline));
#endif
extern "C" double* EMSCRIPTEN_KEEPALIVE getCutoff6581(int slice) {
	return Filter6581::getCutoff6581(slice);
}




// ----------- deprecated stuff that should no longer be used -----------------

/**
* @deprecated bit0=voice0, bit1=voice1,..
*/
#ifdef _MSC_VER
extern "C" uint32_t enableVoices(uint32_t mask);
#else
extern "C" uint32_t enableVoices(uint32_t mask)  __attribute__((noinline));
#endif
extern "C" uint32_t EMSCRIPTEN_KEEPALIVE enableVoices(uint32_t mask) {
	for(uint8_t i= 0; i<3; i++) {
		WebSID::setMute(0, i, !(mask & 0x1));
		mask = mask >> 1;
	}
	
	return 0;
}
/**
* @deprecated use getTraceStreams instead
*/
#ifdef _MSC_VER
extern "C" char* getBufferVoice1();
#else
extern "C" char* getBufferVoice1() __attribute__((noinline));
#endif
extern "C" char* EMSCRIPTEN_KEEPALIVE getBufferVoice1() {
	return (char*) _scope_buffers[0];
}
/**
* @deprecated use getTraceStreams instead
*/
#ifdef _MSC_VER
extern "C" char* getBufferVoice2();
#else
extern "C" char* getBufferVoice2() __attribute__((noinline));
#endif
extern "C" char* EMSCRIPTEN_KEEPALIVE getBufferVoice2() {
	return (char*) _scope_buffers[1];
}
/**
* @deprecated use getTraceStreams instead
*/
#ifdef _MSC_VER
extern "C" char* getBufferVoice3();
#else
extern "C" char* getBufferVoice3() __attribute__((noinline));
#endif
extern "C" char* EMSCRIPTEN_KEEPALIVE getBufferVoice3() {
	return (char*) _scope_buffers[2];
}
/**
* @deprecated use getTraceStreams instead
*/
#ifdef _MSC_VER
extern "C" char* getBufferVoice4();
#else
extern "C" char* getBufferVoice4() __attribute__((noinline));
#endif
extern "C" char* EMSCRIPTEN_KEEPALIVE getBufferVoice4() {
	return (char*) _scope_buffers[3];
}
