/*
* This file contains everything to do with the emulation of the SID chip's filter.
*
* WebSid (c) 2019 Jürgen Wothke
* version 0.93
*
* Terms of Use: This software is licensed under a CC BY-NC-SA
* (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

// note on performance: see http://nicolas.limare.net/pro/notes/2014/12/12_arit_speed/

// example x86-64 (may differ depending on machine architecture):
// 278 Mips integer multiplication								=> 1
// 123 Mips integer division									=> 2.26 slower
// 138 Mips double multiplication (32-bit is actually slower)	=> 2.01 slower
// 67 Mips double division (32-bit is actually slower)			=> 4.15 slower

// example: non-digi song, means per frame volume is set 1x and then 882 samples (or more) are rendered
// 			- cost without prescaling: 882 * (1 + 2.26) =>	2875.32
//			- cost with prescaling: 4.15 + 882 *(4.15)	=>	3664.45
// 			=> for a digi song the costs for prescaling would be even higher

// => repeated integer oparations are usually cheaper when floating point divisions are the alternative

#include "filter.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>


const double SCOPE_SCALEDOWN =  0.5;
const double FILTERED_SCOPE_SCALEDOWN =  0.2;	// tuned using "424.sid"

#include "sid.h"

uint32_t Filter::_sample_rate;

Filter::Filter(WebSID* sid) {
	_sid = sid;
}

Filter::~Filter() {
}

void Filter::setSampleRate(uint32_t sample_rate) {
	_sample_rate = sample_rate;

	_lp_out = _bp_out = _hp_out = 0;
	
	resyncCache();	
}

/* Get the bit from an uint32_t at a specified position */
static bool getBit(uint32_t val, uint8_t idx) { return (bool) ((val >> idx) & 1); }

void Filter::clearSimOut(uint8_t voice_idx) {
	struct FilterState *state = &_sim_voice[voice_idx];
	state->_lp_out = state->_bp_out = state->_hp_out = 0;
}

void Filter::poke(uint8_t reg, uint8_t val) {
	switch (reg) {
        case 0x15: { _reg_cutoff_lo = val & 0x7;	resyncCache();  break; }
        case 0x16: { _reg_cutoff_hi = val;			resyncCache();  break; }
        case 0x17: {
				_reg_res_flt = val & 0xf7;	// ignore FiltEx
				resyncCache();

				for (uint8_t voice_idx= 0; voice_idx<3; voice_idx++) {
					_filter_ena[voice_idx] = getBit(val, voice_idx);

					// ditch cached stuff when filter is turned off
					if (!_filter_ena[voice_idx]) {
						clearSimOut(voice_idx);
					}
				}

			break;
			}
        case 0x18: {
				//  test-case Kapla_Caves and Immigrant_Song: song "randomly switches between
				// filter modes which will also occationally disable all filters:

				
				if (!(val & 0x70)) {
					_lp_out = _bp_out= _hp_out = 0;
					
					clearSimOut(0);
					clearSimOut(1);
					clearSimOut(2);
				}
#ifdef USE_FILTER
				_lowpass_ena =	getBit(val, 4);
				_bandpass_ena =	getBit(val, 5);
				_hipass_ena =	getBit(val, 6);

				_voice3_ena = !getBit(val, 7);		// chan3 off

				_is_filter_off = (val & 0x70) == 0;	// optimization
#endif
			break;
			}
	};
}

// voice output visualization works better if the effect of the filter is included
int16_t Filter::simOutput(uint8_t voice_idx, int32_t voice_out) {
	int32_t o = 0, f = 0;	// isolated from other voices

	uint8_t is_filtered = routeSignal(voice_out, &f, &o, voice_idx);	// redundant.. see above

#ifdef USE_FILTER
	struct FilterState *state = &_sim_voice[voice_idx];

	double output =	runFilter((double)f, (double)o, &(state->_bp_out), &(state->_lp_out), &(state->_hp_out));

	// not using master volume here: "original" voice output is more interesting without potential distortions
	// caused by the D418 digis.. (which are already tracked as a dedicated 4th voice)
	
	return (int16_t) (is_filtered ? output * FILTERED_SCOPE_SCALEDOWN : output * SCOPE_SCALEDOWN);
#else
	return (int16_t)((*out) * SCOPE_SCALEDOWN);
#endif
}

int32_t Filter::getOutput(int32_t* sum_filter_in, int32_t* out) {

#ifdef USE_FILTER
	return runFilter((double)(*sum_filter_in), (double)(*out), &(_bp_out), &(_lp_out), &(_hp_out));
#else
	return (*out);
#endif
}

uint8_t Filter::isSilencedVoice3(uint8_t voice_idx) {
	return ((voice_idx == 2) && (!_voice3_ena) && (!_filter_ena[2]));
}

uint8_t Filter::routeSignal(int32_t voice_out, int32_t* outf, int32_t* outo, uint8_t voice_idx) {
#ifdef USE_FILTER
	// note: compared to other emus output volume is quite high.. maybe better reduce it a bit?

	// regular routing
	if (_filter_ena[voice_idx]) {
		// route to filter
		(*outf) += voice_out;
		return 1;
	} else {
		// route directly to output
		(*outo) += voice_out;
		return 0;
	}
#else
	// Don't use filters, just mix all voices together
	if (*not_muted) {
		(*outo) += voice_out;
	}
	return 0;
#endif
}

double Filter::runFilter(double sum_filter_in, double sum_nofilter_in, double* band_pass, double* low_pass, double* hi_pass) {
	if (_is_filter_off) {
		// fixed bug in Hermit's impl: when neither high, band, nor lowpass is active then the
		// 'sum_nofilter_in' (i.e. unfiltered signal) was returned and 'sum_filter_in' was completely ignored!
		return sum_nofilter_in + sum_filter_in; 	// see Dancing_in_the_Moonlight
	} else {						
		return doGetFilterOutput(sum_filter_in, sum_nofilter_in, band_pass, low_pass, hi_pass);
	}
}