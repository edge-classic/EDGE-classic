#include <cstring>
#include <vector>

#include "patches.h"
#include "ymf_player.h"

// ----------------------------------------------------------------------------
bool OPLPatch::load(OPLPatchSet& patches, const uint8_t *data, size_t size)
{
	return loadOP2(patches, data, size)
	    || loadAIL(patches, data, size)
	    || loadTMB(patches, data, size);
}

// ----------------------------------------------------------------------------
bool OPLPatch::loadOP2(OPLPatchSet& patches, const uint8_t *data, size_t size)
{
	if (size < 175 * (36 + 32) + 8)
		return false;
	
	if (strncmp((const char*)data, "#OPL_II#", 8))
		return false;

	// read data for all patches (128 melodic + 47 percussion)
	for (int i = 0; i < 128+47; i++)
	{
		// patches 0-127 are melodic; the rest are for percussion notes 35 thru 81
		unsigned key = (i < 128) ? i : (i + 35);
		
		OPLPatch &patch = patches[key];
		// clear patch data
		patch = OPLPatch();
		
		// seek to patch data
		const uint8_t *bytes = data + (36*i) + 8;
		
		// read the common data for both 2op voices
		// flag bit 0 is "fixed pitch" (for drums), but it's seemingly only used for drum patches anyway, so ignore it?
		patch.dualTwoOp = (bytes[0] & 4);
		// second voice detune
		patch.voice[1].finetune = OPLPlayer::midiCalcBend((int8_t)(bytes[2] - 128) / 64.0);
	
		patch.fixedNote = bytes[3];
		
		// read data for both 2op voices
		unsigned pos = 4;
		for (int j = 0; j < 2; j++)
		{
			PatchVoice &voice = patch.voice[j];
			
			for (int op = 0; op < 2; op++)
			{
				// operator mode
				voice.op_mode[op] = bytes[pos++];
				// operator envelope
				voice.op_ad[op] = bytes[pos++];
				voice.op_sr[op] = bytes[pos++];
				// operator waveform
				voice.op_wave[op] = bytes[pos++];
				// KSR & output level
				voice.op_ksr[op] = bytes[pos++] & 0xc0;
				voice.op_level[op] = bytes[pos++] & 0x3f;
				
				// feedback/connection (first op only)
				if (op == 0)
					voice.conn = bytes[pos];
				pos++;
			}
			
			// midi note offset (int16, but only really need the LSB)
			voice.tune = (int8_t)bytes[pos];
			pos += 2;
		}
		
		// fix for some bugged DMX patches (e.g. Doom II electric snare)
		if (!(patch.voice[1].op_ad[0] | patch.voice[1].op_ad[1]))
			patch.dualTwoOp = false;
		
		// seek to patch name
		bytes = data + (32*i) + (36*175) + 8;
		if (bytes[0])
			patch.name = std::string((const char*)bytes, 31);
		else
			patch.name = names[key];
	}
	
	return true;
}

// ----------------------------------------------------------------------------
bool OPLPatch::loadAIL(OPLPatchSet& patches, const uint8_t *data, size_t size)
{
	int index = 0;
	
	while (true)
	{
		if (size < index * 6)
			return false;
		
		const uint8_t *entry = data + (index * 6);
		uint16_t key;
		
		if (entry[0] == 0xff && entry[1] == 0xff)
			return true; // end of patches
		else if (entry[1] == 0x7f)
			key = entry[0] | 0x80;
		else
			key = (entry[0] | (entry[1] << 8)) & 0x7f7f;
		
		OPLPatch &patch = patches[key];
		// clear patch data
		patch = OPLPatch();
		patch.name = names[key & 0xff];
		
		uint32_t patchPos = entry[2] | (entry[3] << 8) | (entry[4] << 16) | (entry[5] << 24);
		if (size < patchPos)
			return false;
		
		const uint8_t *bytes = data + patchPos;
		
		if (size < patchPos + bytes[0])
			return false;
		else if (bytes[0] == 0x0e)
			patch.fourOp = false;
		else if (bytes[0] == 0x19)
			patch.fourOp = true;
		else
			return false;
		index++;
		
		patch.voice[0].tune = patch.voice[1].tune = (int8_t)bytes[2] - 12;
		patch.voice[0].conn = bytes[8] & 0x0f;
		patch.voice[1].conn = bytes[8] >> 7;
		
		unsigned pos = 3;
		for (int i = 0; i < (patch.fourOp ? 2 : 1); i++)
		{
			PatchVoice &voice = patch.voice[i];
			
			for (int op = 0; op < 2; op++)
			{
				// operator mode
				voice.op_mode[op] = bytes[pos++];
				// KSR & output level
				voice.op_ksr[op] = bytes[pos] & 0xc0;
				voice.op_level[op] = bytes[pos++] & 0x3f;
				// operator envelope
				voice.op_ad[op] = bytes[pos++];
				voice.op_sr[op] = bytes[pos++];
				// operator waveform
				voice.op_wave[op] = bytes[pos++];
				
				// already handled the feedback/connection byte
				if (op == 0)
					pos++;
			}
		}
	}
}

// ----------------------------------------------------------------------------
bool OPLPatch::loadTMB(OPLPatchSet& patches, const uint8_t *data, size_t size)
{
	if (size < 256 * 13)
		return false;
	
	for (uint16_t key = 0; key < 256; key++)
	{
		OPLPatch &patch = patches[key];
		// clear patch data
		patch = OPLPatch();
		patch.name = names[key];
		
		const uint8_t *bytes = data + (key * 13);
		
		// since this format has no identifying info, we can only really reject it
		// if it has invalid values in a few spots
		if ((bytes[8] | bytes[9] | bytes[10]) & 0xf0)
			return false;
		
		PatchVoice &voice = patch.voice[0];
		voice.op_mode[0]  = bytes[0];
		voice.op_mode[1]  = bytes[1];
		voice.op_ksr[0]   = bytes[2] & 0xc0;
		voice.op_level[0] = bytes[2] & 0x3f;
		voice.op_ksr[1]   = bytes[3] & 0xc0;
		voice.op_level[1] = bytes[3] & 0x3f;
		voice.op_ad[0]    = bytes[4];
		voice.op_ad[1]    = bytes[5];
		voice.op_sr[0]    = bytes[6];
		voice.op_sr[1]    = bytes[7];
		voice.op_wave[0]  = bytes[8];
		voice.op_wave[1]  = bytes[9];
		voice.conn        = bytes[10];
		voice.tune        = (int8_t)bytes[11] - 12;
		patch.velocity    = (int8_t)bytes[12];
	}
	
	return true;
}
