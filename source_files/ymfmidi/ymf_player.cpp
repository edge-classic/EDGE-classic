#include "ymf_player.h"

#include <cmath>
#include <cstring>

static const unsigned voice_num[18] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
	0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108
};

static const unsigned oper_num[18] = {
	0x0, 0x1, 0x2, 0x8, 0x9, 0xA, 0x10, 0x11, 0x12,
	0x100, 0x101, 0x102, 0x108, 0x109, 0x10A, 0x110, 0x111, 0x112
};

// ----------------------------------------------------------------------------
OPLPlayer::OPLPlayer(int frequency) : ymfm::ymfm_interface()
{
	m_voices.resize(18);
	
	m_opl3 = new ymfm::ymf262(*this);
	m_output.clear();
	
	m_filterFreq = 5.0; // 5Hz default to reduce DC offset
	setSampleRate(frequency); // setup both sample step and filter coefficient
	setGain(1.0);
	
	reset();
}

// ----------------------------------------------------------------------------
OPLPlayer::~OPLPlayer()
{
	delete m_opl3;
}

// ----------------------------------------------------------------------------
void OPLPlayer::setSampleRate(uint32_t rate)
{
	uint32_t rateOPL = m_opl3->sample_rate(masterClock);
	m_sampleStep = (double)rate / rateOPL;
	m_sampleRate = rate;
	
	setFilter(m_filterFreq);
}

// ----------------------------------------------------------------------------
void OPLPlayer::setGain(double gain)
{
	m_sampleGain = gain;
	m_sampleScale = 32768.0 / gain;
}

// ----------------------------------------------------------------------------
void OPLPlayer::setFilter(double cutoff)
{
	m_filterFreq = cutoff;
	
	if (m_filterFreq <= 0.0)
	{
		m_filterCoef = 1.0;
	}
	else
	{
		static const double pi = 3.14159265358979323846;
		m_filterCoef = 1.0 / ((2 * pi * cutoff) / m_sampleRate + 1);
	}
//	printf("sample rate = %u / cutoff %f Hz / filter coef %f\n", m_sampleRate, cutoff, m_filterCoef);
}

// ----------------------------------------------------------------------------
bool OPLPlayer::loadPatches(const uint8_t *data, size_t size)
{
	return OPLPatch::load(m_patches, data, size);
}

// ----------------------------------------------------------------------------
void OPLPlayer::generate(float *data, unsigned numSamples)
{
	unsigned samp = 0;

		while (samp < numSamples * 2)
		{
			updateMIDI();

			data[samp]   += m_output.data[0] / m_sampleScale;
			data[samp+1] += m_output.data[1] / m_sampleScale;
				
			if (m_filterCoef < 1.0)
			{
				for (int i = 0; i < 2; i++)
				{
					float lastIn = m_lastInF[i];
					m_lastInF[i] = data[samp+i];
						
					m_lastOutF[i] = m_filterCoef * (m_lastOutF[i] + data[samp+i] - lastIn);
					data[samp+i] = m_lastOutF[i];
				}
			}
				
			samp += 2;
		}
}

// ----------------------------------------------------------------------------
void OPLPlayer::generate(int16_t *data, unsigned numSamples)
{
	unsigned int samp = 0;

	while (samp < numSamples * 2)
	{
		updateMIDI();

		int32_t samples[2] = {0};
			
		samples[0] += (int32_t)m_output.data[0] * m_sampleGain;
		samples[1] += (int32_t)m_output.data[1] * m_sampleGain;
				
		if (m_filterCoef < 1.0)
		{
			for (int i = 0; i < 2; i++)
			{
				const int32_t lastIn = m_lastIn[i];
				m_lastIn[i] = samples[i];
						
				m_lastOut[i] = m_filterCoef * (m_lastOut[i] + samples[i] - lastIn);
				samples[i] = m_lastOut[i];
			}
		}

		data[samp] = ymfm::clamp(samples[0], -32768, 32767);
		data[samp+1] = ymfm::clamp(samples[1], -32768, 32767);

		samp += 2;
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::updateMIDI()
{
	for (auto& voice : m_voices)
	{
		if (voice.duration < UINT_MAX)
			voice.duration++;
		voice.justChanged = false;
	}

	if (m_sampleFIFO.empty())
	{
		m_opl3->generate(&m_output);
	}
	else
	{
		m_output = m_sampleFIFO.front();
		m_sampleFIFO.pop();
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::reset()
{
	m_opl3->reset();
	// enable OPL3 stuff
	write(REG_NEW, 1);
		
	// reset MIDI channel and OPL voice status
	m_midiType = GeneralMIDI;
	for (int i = 0; i < 16; i++)
	{
		m_channels[i] = MIDIChannel();
		m_channels[i].num = i;
	}
	m_channels[9].percussion = true;
	
	for (int i = 0; i < m_voices.size(); i++)
	{
		m_voices[i] = OPLVoice();
		m_voices[i].chip = i / 18;
		m_voices[i].num = voice_num[i % 18];
		m_voices[i].op = oper_num[i % 18];
		
		// configure 4op voices
		
		switch (i % 9)
		{
		case 0: case 1: case 2:
			m_voices[i].fourOpPrimary = true;
			m_voices[i].fourOpOther = &m_voices[i+3];
			break;
		case 3: case 4: case 5:
			m_voices[i].fourOpPrimary = false;
			m_voices[i].fourOpOther = &m_voices[i-3];
			break;
		default:
			m_voices[i].fourOpPrimary = false;
			m_voices[i].fourOpOther = nullptr;
			break;
		}
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::disableOPL3()
{
	write(REG_NEW, 0);
}

// ----------------------------------------------------------------------------
void OPLPlayer::enableOPL3()
{
	write(REG_NEW, 1);
}

// ----------------------------------------------------------------------------
void OPLPlayer::runOneSample()
{
	// clock one sample after changing the 4op state before writing other registers
	// so that ymfm can reassign operators to channels, etc
	ymfm::ymf262::output_data output;
	m_opl3->generate(&output);
	m_sampleFIFO.push(output);
}

// ----------------------------------------------------------------------------
void OPLPlayer::write(uint16_t addr, uint8_t data)
{
	if (addr < 0x100)
		m_opl3->write_address((uint8_t)addr);
	else
		m_opl3->write_address_hi((uint8_t)addr);
	m_opl3->write_data(data);
}

// ----------------------------------------------------------------------------
OPLVoice* OPLPlayer::findVoice(uint8_t channel, const OPLPatch *patch, uint8_t note)
{
	OPLVoice *found = nullptr;
	uint32_t duration = 0;
	
	// try to find the "oldest" voice, prioritizing released notes
	// (or voices that haven't ever been used yet)
	for (auto& voice : m_voices)
	{
		if (useFourOp(patch) && !voice.fourOpPrimary)
			continue;
	
		if (!voice.channel)
			return &voice;
	
		if (!voice.on && !voice.justChanged)
		{
			if (voice.channel->num == channel && voice.note == note)
			{
				// found an old voice that was using the same note and patch - use it again
				return &voice;
			}
			if (voice.duration > duration)
			{
				found = &voice;
				duration = voice.duration;
			}
		}
	}
	
	if (found) return found;
	// if we didn't find one yet, just try to find an old one
	// using the same channel and/or patch, even if it should still be playing.
	
	for (auto& voice : m_voices)
	{
		if (useFourOp(patch) && !voice.fourOpPrimary)
			continue;
		
		if ((voice.channel->num == channel || voice.patch == patch)
		    && voice.duration > duration)
		{
			found = &voice;
			duration = voice.duration;
		}
	}
	
	if (found) return found;
	// last resort - just find any old voice at all
	
	for (auto& voice : m_voices)
	{
		if (useFourOp(patch) && !voice.fourOpPrimary)
			continue;
		// don't let a 2op instrument steal an active voice from a 4op one
		if (!useFourOp(patch) && voice.on && useFourOp(voice.patch))
			continue;
		
		if (voice.duration > duration)
		{
			found = &voice;
			duration = voice.duration;
		}
	}
	
	return found;
}

// ----------------------------------------------------------------------------
OPLVoice* OPLPlayer::findVoice(uint8_t channel, uint8_t note, bool justChanged)
{
	channel &= 15;
	for (auto& voice : m_voices)
	{
		if (voice.on 
		    && voice.justChanged == justChanged
		    && voice.channel == &m_channels[channel]
		    && voice.note == note)
		{
			return &voice;
		}
	}
	
	return nullptr;
}

// ----------------------------------------------------------------------------
const OPLPatch* OPLPlayer::findPatch(uint8_t channel, uint8_t note) const
{
	uint16_t key;
	const MIDIChannel &ch = m_channels[channel & 15];

	if (ch.percussion)
		key = 0x80 | note | (ch.patchNum << 8);
	else
		key = ch.patchNum | (ch.bank << 8);
	
	// if this patch+bank combo doesn't exist, default to bank 0
	if (!m_patches.count(key))
		key &= 0x00ff;
	// if patch still doesn't exist in bank 0, use patch 0 (or drum note 0)
	if (!m_patches.count(key))
		key &= 0x0080;
	// if that somehow still doesn't exist, forget it
	if (!m_patches.count(key))
		return nullptr;
	
	return &m_patches.at(key);
}

// ----------------------------------------------------------------------------
bool OPLPlayer::useFourOp(const OPLPatch *patch) const
{
	return patch->fourOp;
}

// ----------------------------------------------------------------------------
void OPLPlayer::updateChannelVoices(uint8_t channel, void(OPLPlayer::*func)(OPLVoice&))
{
	for (auto& voice : m_voices)
	{
		if (voice.channel == &m_channels[channel & 15])
			(this->*func)(voice);
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::updatePatch(OPLVoice& voice, const OPLPatch *newPatch, uint8_t numVoice)
{	
	// assign the MIDI channel's current patch (or the current drum patch) to this voice

	const PatchVoice& patchVoice = newPatch->voice[numVoice];
	
	if (voice.patchVoice != &patchVoice)
	{
		bool oldFourOp = voice.patch ? useFourOp(voice.patch) : false;
	
		voice.patch = newPatch;
		voice.patchVoice = &patchVoice;
		
		// update enable status for 4op channels on this chip
		if (useFourOp(newPatch) != oldFourOp)
		{
			// if going from part of a 4op patch to a 2op one, kill the other one
			OPLVoice *other = voice.fourOpOther;
			if (other && other->patch
				&& useFourOp(other->patch) && !useFourOp(newPatch))
			{
				silenceVoice(*other);
			}
		
			uint8_t enable = 0x00;
			uint8_t bit = 0x01;
			for (unsigned i = voice.chip * 18; i < voice.chip * 18 + 18; i++)
			{
				if (m_voices[i].fourOpPrimary)
				{
					if (m_voices[i].patch && useFourOp(m_voices[i].patch))
						enable |= bit;
					bit <<= 1;
				}
			}
			
			write(REG_4OP, enable);
			runOneSample();
		}
		
		// 0x20: vibrato, sustain, multiplier
		write(REG_OP_MODE + voice.op,     patchVoice.op_mode[0]);
		write(REG_OP_MODE + voice.op + 3, patchVoice.op_mode[1]);
		// 0x60: attack/decay
		write(REG_OP_AD + voice.op,     patchVoice.op_ad[0]);
		write(REG_OP_AD + voice.op + 3, patchVoice.op_ad[1]);
		// 0x80: sustain/release
		write(REG_OP_SR + voice.op,     patchVoice.op_sr[0]);
		write(REG_OP_SR + voice.op + 3, patchVoice.op_sr[1]);
		// 0xe0: waveform
		write(REG_OP_WAVEFORM + voice.op,     patchVoice.op_wave[0]);
		write(REG_OP_WAVEFORM + voice.op + 3, patchVoice.op_wave[1]);
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::updateVolume(OPLVoice& voice)
{
	// lookup table shamelessly stolen from Nuke.YKT
	static const uint8_t opl_volume_map[32] =
	{
		80, 63, 40, 36, 32, 28, 23, 21,
		19, 17, 15, 14, 13, 12, 11, 10,
		 9,  8,  7,  6,  5,  5,  4,  4,
		 3,  3,  2,  2,  1,  1,  0,  0
	};

	if (!voice.patch || !voice.channel) return;
	
	auto patchVoice = voice.patchVoice;

	uint8_t atten = opl_volume_map[(voice.velocity * voice.channel->volume) >> 10];
	uint8_t level;
	bool scale[2] = {0};
	
	// determine which operator(s) to scale based on the current operator settings
	if (!useFourOp(voice.patch))
	{
		// 2op FM (0): scale op 2 only
		// 2op AM (1): scale op 1 and 2
		scale[0] = (patchVoice->conn & 1);
		scale[1] = true;
	}
	else if (voice.fourOpPrimary)
	{
		// 4op FM+FM (0, 0): don't scale op 1 or 2
		// 4op AM+FM (1, 0): scale op 1 only
		// 4op FM+AM (0, 1): scale op 2 only
		// 4op AM+AM (1, 1): scale op 1 only
		scale[0] = (voice.patch->voice[0].conn & 1);
		scale[1] = (voice.patch->voice[1].conn & 1) && !scale[0];
	}
	else
	{
		// 4op FM+FM (0, 0): scale op 4 only
		// 4op AM+FM (1, 0): scale op 4 only
		// 4op FM+AM (0, 1): scale op 4 only
		// 4op AM+AM (1, 1): scale op 3 and 4
		scale[0] = (voice.patch->voice[0].conn & 1)
		        && (voice.patch->voice[1].conn & 1);
		scale[1] = true;
	}
	
	// 0x40: key scale / volume
	if (scale[0])
		level = std::min(0x3f, patchVoice->op_level[0] + atten);
	else
		level = patchVoice->op_level[0];
	write(REG_OP_LEVEL + voice.op,     level | patchVoice->op_ksr[0]);
	
	if (scale[1])
		level = std::min(0x3f, patchVoice->op_level[1] + atten);
	else
		level = patchVoice->op_level[1];
	write(REG_OP_LEVEL + voice.op + 3, level | patchVoice->op_ksr[1]);
}

// ----------------------------------------------------------------------------
void OPLPlayer::updatePanning(OPLVoice& voice)
{
	if (!voice.patch || !voice.channel) return;
	
	// 0xc0: output/feedback/mode
	uint8_t pan = 0x30;
	if (voice.channel->pan < 32)
		pan = 0x10;
	else if (voice.channel->pan >= 96)
		pan = 0x20;
	
	write(REG_VOICE_CNT + voice.num, voice.patchVoice->conn | pan);
}

// ----------------------------------------------------------------------------
void OPLPlayer::updateFrequency(OPLVoice& voice)
{
	static const uint16_t noteFreq[12] = {
		// calculated from A440
		345, 365, 387, 410, 435, 460, 488, 517, 547, 580, 615, 651
	};

	if (!voice.patch || !voice.channel) return;
	if (useFourOp(voice.patch) && !voice.fourOpPrimary) return;
	
	int note = (!voice.channel->percussion ? voice.note : voice.patch->fixedNote)
	         + voice.patchVoice->tune;
	
	int octave = note / 12;
	note %= 12;
	
	// calculate base frequency (and apply pitch bend / patch detune)
	unsigned freq = (note >= 0) ? noteFreq[note] : (noteFreq[note + 12] >> 1);
	if (octave < 0)
		freq >>= -octave;
	else if (octave > 0)
		freq <<= octave;
	
	freq *= voice.channel->pitch * voice.patchVoice->finetune;
	
	// convert the calculated frequency back to a block and F-number
	octave = 0;
	while (freq > 0x3ff)
	{
		freq >>= 1;
		octave++;
	}
	octave = std::min(7, octave);
	voice.freq = freq | (octave << 10);
	
	write(REG_VOICE_FREQL + voice.num, voice.freq & 0xff);
	write(REG_VOICE_FREQH + voice.num, (voice.freq >> 8) | (voice.on ? (1 << 5) : 0));
}

// ----------------------------------------------------------------------------
void OPLPlayer::silenceVoice(OPLVoice& voice)
{
	voice.channel    = nullptr;
	voice.patch      = nullptr;
	voice.patchVoice = nullptr;
	
	voice.on = false;
	voice.justChanged = true;
	voice.duration = UINT_MAX;
	
	write(REG_OP_LEVEL + voice.op,     0xff);
	write(REG_OP_LEVEL + voice.op + 3, 0xff);
	write(REG_VOICE_FREQL + voice.num, 0x00);
	write(REG_VOICE_FREQH + voice.num, 0x00);
	write(REG_VOICE_CNT + voice.num,   0x00);
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
	note &= 0x7f;
	velocity &= 0x7f;

	// if we just now turned this same note on, don't do it again
	if (findVoice(channel, note, true))
		return;
	
	midiNoteOff(channel, note);
	if (!velocity)
		return;
	
//	printf("midiNoteOn: chn %u, note %u\n", channel, note);
	const OPLPatch *newPatch = findPatch(channel, note);
	if (!newPatch) return;
	
	const int numVoices = ((useFourOp(newPatch) || newPatch->dualTwoOp) ? 2 : 1);

	OPLVoice *voice = nullptr;
	for (int i = 0; i < numVoices; i++)
	{
		if (voice && useFourOp(newPatch) && voice->fourOpOther)
			voice = voice->fourOpOther;
		else
			voice = findVoice(channel, newPatch, note);
		if (!voice) continue; // ??
		
		if (voice->on)
		{
			silenceVoice(*voice);
			runOneSample();
		}
		
		// update the note parameters for this voice
		voice->channel = &m_channels[channel & 15];
		voice->on = voice->justChanged = true;
		voice->note = note;
		voice->velocity = ymfm::clamp((int)velocity + newPatch->velocity, 0, 127);
		// for dual 2op, set the second voice's duration to 1 so it can get dropped if we need it to
		voice->duration = newPatch->dualTwoOp ? i : 0;
		
		updatePatch(*voice, newPatch, i);
		updateVolume(*voice);
		updatePanning(*voice);
		
		// for 4op instruments, don't key on until we've written both voices...
		if (!useFourOp(newPatch))
		{
			updateFrequency(*voice);
			runOneSample();
		}
		else if (i > 0)
		{
			updateFrequency(*voice->fourOpOther);
			runOneSample();
		}
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiNoteOff(uint8_t channel, uint8_t note)
{
	note &= 0x7f;
	
//	printf("midiNoteOff: chn %u, note %u\n", channel, note);
	OPLVoice *voice;
	while ((voice = findVoice(channel, note)) != nullptr)
	{
		voice->justChanged = voice->on;
		voice->on = false;

		write(REG_VOICE_FREQH + voice->num, voice->freq >> 8);
		runOneSample();
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiPitchControl(uint8_t channel, double pitch)
{
//	printf("midiPitchControl: chn %u, val %.02f\n", channel, pitch);
	MIDIChannel& ch = m_channels[channel & 15];
	
	ch.basePitch = pitch;
	ch.pitch = midiCalcBend(pitch * ch.bendRange);
	updateChannelVoices(channel, &OPLPlayer::updateFrequency);
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiProgramChange(uint8_t channel, uint8_t patchNum)
{
	m_channels[channel & 15].patchNum = patchNum & 0x7f;
	// patch change will take effect on the next note for this channel
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiControlChange(uint8_t channel, uint8_t control, uint8_t value)
{
	channel &= 15;
	control &= 0x7f;
	value   &= 0x7f;
	
	MIDIChannel& ch = m_channels[channel];
	
//	printf("midiControlChange: chn %u, ctrl %u, val %u\n", channel, control, value);
	switch (control)
	{
	case 0:
		if (m_midiType == RolandGS)
			ch.bank = value;
		else if (m_midiType == YamahaXG)
			ch.percussion = (value == 0x7f);
		break;
		
	case 6:
		if (ch.rpn == 0)
		{
			ch.bendRange = value;
			midiPitchControl(channel, ch.basePitch);
		}
		break;
	
	case 7:
		ch.volume = value;
		updateChannelVoices(channel, &OPLPlayer::updateVolume);
		break;
	
	case 10:
		ch.pan = value;
		updateChannelVoices(channel, &OPLPlayer::updatePanning);
		break;
	
	case 32:
		if (m_midiType == YamahaXG || m_midiType == GeneralMIDI2)
			ch.bank = value;
		break;
	
	case 98:
	case 99:
		ch.rpn = 0x3fff;
		break;
	
	case 100:
		ch.rpn &= 0x3f80;
		ch.rpn |= value;
		break;
		
	case 101:
		ch.rpn &= 0x7f;
		ch.rpn |= (value << 7);
		break;
	
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiSysEx(const uint8_t *data, uint32_t length)
{
	if (length > 0 && data[0] == 0xF0)
	{
		data++;
		length--;
	}

	if (length == 0)
		return;

	if (data[0] == 0x7e) // universal non-realtime
	{
		if (length == 5 && data[1] == 0x7f && data[2] == 0x09)
		{
			if (data[3] == 0x01)
				m_midiType = GeneralMIDI;
			else if (data[3] == 0x03)
				m_midiType = GeneralMIDI2;
		}
	}
	else if (data[0] == 0x41 && length >= 10 // Roland
	         && data[2] == 0x42 && data[3] == 0x12)
	{
		// if we received one of these, assume GS mode
		// (some MIDIs seem to e.g. send drum map messages without a GS reset)
		m_midiType = RolandGS;
		
		uint32_t address = (data[4] << 16) | (data[5] << 8) | data[6];
		// for single part parameters, map "part number" to channel number
		// (using the default mapping)
		uint8_t channel = (address & 0xf00) >> 8;
		if (channel == 0)
			channel = 9;
		else if (channel <= 9)
			channel--;
			
		// Roland GS part parameters
		if ((address & 0xfff0ff) == 0x401015) // set drum map
			m_channels[channel].percussion = (data[7] != 0x00);
	}
	else if (length >= 8 && !memcmp(data, "\x43\x10\x4c\x00\x00\x7e\x00\xf7", 8)) // Yamaha
	{
		m_midiType = YamahaXG;
	}
}

// ----------------------------------------------------------------------------
void OPLPlayer::midiRawOPL(uint16_t addr, uint8_t data)
{
	write(addr, data);
	runOneSample();
}

// ----------------------------------------------------------------------------
double OPLPlayer::midiCalcBend(double semitones)
{
	return pow(2, semitones / 12.0);
}
