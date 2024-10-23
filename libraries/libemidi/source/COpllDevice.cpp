#include "COpllDevice.hpp"

#include <math.h>
#include <string.h>

#include <cstdio>

using namespace dsa;
using namespace dsa::C;

// GM音色マップ
static uint8_t program_table[128] = {
    3,  3,  3,  3,  3,  3,  11, 11, // 000- PIANO
    12, 12, 12, 12, 12, 12, 12, 12, // 008- BELL
    8,  8,  8,  8,  8,  8,  8,  8,  // 016- ORGAN
    2,  2,  2,  2,  0,  0,  0,  0,  // 024- GUITAR
    14, 15, 15, 14, 15, 15, 14, 15, // 032- BASS
    1,  1,  1,  1,  1,  1,  1,  1,  // 040- STRING
    1,  1,  1,  1,  3,  4,  3,  13, // 048- STRING2
    7,  7,  7,  7,  9,  7,  7,  7,  // 056- BRASS
    6,  6,  6,  6,  6,  9,  5,  5,  // 064- LEAD
    4,  4,  4,  4,  4,  4,  4,  4,  // 072- PIPE
    5,  1,  1,  1,  1,  1,  1,  7,  // 080- SYN LEAD
    9,  9,  3,  9,  9,  9,  9,  0,  // 088- SYN
    5,  5,  5,  5,  5,  5,  5,  5,  // 096- SYN.EFFECT
    0,  0,  0,  2,  2,  2,  5,  5,  // 104- ETHNIC
    5,  5,  5,  5,  5,  5,  5,  5,  // 112- EFFECT
    5,  5,  5,  5,  5,  5,  5,  5   // 120- SFX
};

// 音色別の音量差調整 -n 音量アップ +n 音量ダウン
static int prog_att[16] = {
    -1, // 0 org
    -2, // 1 violin
    1,  // 2 guitar
    1,  // 3 piano
    0,  // 4 flute
    0,  // 5 clarinet
    0,  // 6 oboe
    1,  // 7 trumpet
    0,  // 8 organ
    0,  // 9 horn
    0,  // 10 synth
    0,  // 11 harp
    0,  // 12 vibra
    0,  // 13 s.bass
    0,  // 14 w.bass
    0,  // 15 e.bass
};

static int prog_oct[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static uint8_t perc_table[128] = {
    // 5:B.D 4:S.D 3:TOM 2:CYM 1:HH 0:NONE
    0, 0, 0, 0, 0, 0, 0, 0, // 000-
    0, 0, 0, 0, 0, 0, 0, 5, // 008-
    0, 0, 0, 0, 0, 0, 0, 0, // 016-
    0, 0, 0, 0, 0, 0, 0, 0, // 024-
    0, 5, 4, 5, 5, 1, 4, 1, // 032-
    4, 3, 1, 3, 1, 3, 2, 3, // 040-
    3, 2, 3, 2, 2, 2, 2, 2, // 048-
    2, 2, 2, 2, 3, 3, 3, 3, // 056-
    3, 3, 3, 0, 0, 0, 0, 0, // 064-
    0, 0, 0, 0, 0, 0, 0, 0, // 072-
    0, 0, 0, 0, 0, 0, 0, 0, // 080-
    0, 0, 0, 0, 0, 0, 0, 0, // 088-
    0, 0, 0, 0, 0, 0, 0, 0, // 096-
    0, 0, 0, 0, 0, 0, 0, 0, // 104-
    0, 0, 0, 0, 0, 0, 0, 0, // 112-
    0, 0, 0, 0, 0, 0, 0, 0  // 120-
};

COpllDevice::COpllDevice(uint32_t rate, uint32_t nch)
{

    if (nch == 2)
        m_nch = 2;
    else
        m_nch = 1;

    for (uint32_t i = 0; i < m_nch; i++)
    {
        m_opll[i] = OPLL_new(3579545, rate);
        memset(m_reg_cache[i],0,128);
        m_rbuf[i].clear();
    }
    Reset();
}

COpllDevice::~COpllDevice()
{
    for (uint32_t i = 0; i < m_nch; i++)
    {
        m_rbuf[i].clear();
        OPLL_delete(m_opll[i]);
    }
}

const SoundDeviceInfo &COpllDevice::GetDeviceInfo(void) const
{
    static SoundDeviceInfo si;
    si.name    = (uint8_t *)"OPLL Module";
    si.desc    = (uint8_t *)"(C) Mitsutaka Okazaki 2004" __FILE__;
    si.version = 0x0001;
    si.max_ch  = 6;
    return si;
}

bool COpllDevice::Reset()
{

    {
        for (uint32_t i = 0; i < m_nch; i++)
        {
            OPLL_reset(m_opll[i]);
            OPLL_set_quality(m_opll[i], 1);
            // Rhythm Initial Value
            _WriteReg(0x16, 0x20, i);
            _WriteReg(0x26, 0x05, i);
            _WriteReg(0x17, 0x50, i);
            _WriteReg(0x27, 0x05, i);
            _WriteReg(0x18, 0xC0, i);
            _WriteReg(0x28, 0x01, i);
            // Original Voice
            _WriteReg(0x00, 0x61, i);
            _WriteReg(0x01, 0x61, i);
            _WriteReg(0x02, 0x03, i);
            _WriteReg(0x03, 0x0D, i);
            _WriteReg(0x04, 0xf9, i);
            _WriteReg(0x05, 0xf4, i);
            _WriteReg(0x06, 0x37, i);
            _WriteReg(0x07, 0x27, i);
            memset(m_reg_cache[i], 0, 128);
            m_rbuf[i].clear();
        }
    }

    for (int i = 0; i < 9; i++)
    {
        m_ci[i].bend_coarse = 0;
        m_ci[i].bend_fine   = 0;
        m_ci[i]._bend_fine  = 1.0;
        m_ci[i].octave      = 0;
        m_ci[i].volume      = 127;
        m_ci[i].velocity    = 127;
        m_ci[i].program     = 0;
        m_ci[i].note        = 0;
        m_ci[i].pan         = 64;
        m_ci[i].keyon       = false;
        m_ci[i].fnum        = 0;
    }

    m_pi.volume = 127;
    m_pi.prog   = 0;
    m_pi.bank   = 0;
    for (int i = 0; i < 5; i++)
    {
        m_pi.velocity[i] = 127;
        m_pi.vcache[i]   = 0;
    }
    m_pi.keymap = 0;

    return true;
}

void COpllDevice::_WriteReg(uint8_t reg, uint8_t val, int32_t pan)
{

    if (m_nch == 2)
    {
        if (pan < 0 || 1 < pan)
        {
            _WriteReg(reg, val, 1);
            pan = 0;
        }
    }
    else
        pan = 0;

    if (m_reg_cache[pan][reg] != val)
    {
        OPLL_writeReg(m_opll[pan], reg, val);
        m_reg_cache[pan][reg] = val;

        // At least one calc() method must be invoked between two sequence of writeReg().
        if (m_rbuf[pan].size() < 8192)
        {
            m_rbuf[pan].push_back(OPLL_calc(m_opll[pan]));
        }
    }
}

bool COpllDevice::Render(int32_t buf[2])
{

    for (uint32_t i = 0; i < m_nch; i++)
    {
        if (m_rbuf[i].empty())
            buf[i] = OPLL_calc(m_opll[i]);
        else
        {
            buf[i] = m_rbuf[i].front();
            m_rbuf[i].pop_front();
        }
    }
    if (m_nch < 2)
        buf[1] = buf[0];
    return true;
}

void COpllDevice::_UpdateVolume(uint32_t ch)
{

    int32_t att = 14 - m_ci[ch].volume / 16 - m_ci[ch].velocity / 16 + prog_att[m_ci[ch].program];
    if (att < 0)
        att = 0;
    else if (15 < att)
        att = 15;

    if (m_nch < 2)
    {
        _WriteReg(0x30 + ch, att | ((m_ci[ch].program) << 4));
        return;
    }

    // LEFT CHANNEL
    if (64 < m_ci[ch].pan)
    {
        int tmp = att + (m_ci[ch].pan - 64) / 4;
        _WriteReg(0x30 + ch, ((tmp < 15) ? tmp : 15) | ((m_ci[ch].program) << 4), 0);
    }
    else
    {
        _WriteReg(0x30 + ch, att | ((m_ci[ch].program) << 4), 0);
    }

    // RIGHT CHANNEL
    if (m_ci[ch].pan < 64)
    {
        int tmp = att + (63 - m_ci[ch].pan) / 4;
        _WriteReg(0x30 + ch, ((tmp < 15) ? tmp : 15) | ((m_ci[ch].program) << 4), 1);
    }
    else
    {
        _WriteReg(0x30 + ch, att | ((m_ci[ch].program) << 4), 1);
    }
}

void COpllDevice::SetPan(uint32_t ch, uint8_t pan)
{
    m_ci[ch].pan = pan;
    _UpdateVolume(ch);
}

void COpllDevice::_UpdateFreq(uint32_t ch)
{
    static const uint8_t  base          = 67; // G
    static const uint16_t note2freq[12] = {   // 172, 183, 194, 205, 217, 230, 244,
                                           258, 274, 290, 307, 325, 344, 365, 387, 410, 434, 460, 487};

    int32_t  note = m_ci[ch].note + m_ci[ch].bend_coarse;
    uint16_t freq = (int)(m_ci[ch]._bend_fine * note2freq[(note + 240 - base) % 12]);
    int32_t  oct  = 4 + prog_oct[m_ci[ch].program];

    if (note >= base)
        oct += (note - base) / 12;
    else
        oct -= ((base - note - 1) / 12 + 1);

    while (oct < 0)
    {
        oct++;
        freq = (freq >> 1) + 1;
    }
    while (7 < oct)
    {
        oct--;
        freq <<= 1;
    }

    while (0x1ff < freq)
    {
        if (oct < 7)
        {
            freq = (freq >> 1) + 1;
            oct++;
        }
        else
        {
            freq = 0x1FF;
        }
    }

    _WriteReg(0x10 + ch, freq & 0xFF);
    _WriteReg(0x20 + ch, (m_ci[ch].keyon ? 0xF0 : 0) | (oct << 1) | (freq >> 8));

    m_ci[ch].fnum = (oct << 9) | freq;
}

void COpllDevice::SetBend(uint32_t ch, int8_t coarse, int8_t fine)
{
    m_ci[ch].bend_coarse = coarse;
    m_ci[ch].bend_fine   = fine;
    m_ci[ch]._bend_fine  = pow(2.0, (double)fine / 1200);
    _UpdateFreq(ch);
}

void COpllDevice::SetProgram(uint32_t ch, uint8_t bank, uint8_t prog)
{
    m_ci[ch].program = program_table[prog];
    _UpdateVolume(ch);
}

void COpllDevice::SetVolume(uint32_t ch, uint8_t vol)
{
    m_ci[ch].volume = vol;
    _UpdateVolume(ch);
}

void COpllDevice::SetVelocity(uint32_t ch, uint8_t velo)
{
    m_ci[ch].velocity = velo;
    _UpdateVolume(ch);
}

void COpllDevice::KeyOn(uint32_t ch, uint8_t note)
{
    m_ci[ch].note  = note;
    m_ci[ch].keyon = true;
    _UpdateFreq(ch);
}

void COpllDevice::KeyOff(uint32_t ch)
{
    m_ci[ch].keyon = false;
    _WriteReg(0x20 + ch, m_ci[ch].fnum >> 8);
}

void COpllDevice::PercSetVelocity(uint8_t note, uint8_t velo)
{
    note = perc_table[note];
    if (0 < note)
    {
        m_pi.velocity[note - 1] = velo;
        _PercUpdateVolume(note);
    }
}

void COpllDevice::_PercUpdateVolume(uint8_t note)
{

    if (note < 1 || note > 5)
        return;

    int vol = 13 - m_pi.volume / 16 - m_pi.velocity[note-1] / 16;
    if (vol < 0)
        m_pi.vcache[note-1] = 0;
    else if (15 < vol)
        m_pi.vcache[note-1] = 15;
    else
        m_pi.vcache[note-1] = vol;

    switch (note-1)
    {
    case 4: // B.D
        _WriteReg(0x30 + 6, m_pi.vcache[4]);
        break;
    case 3: // S.D
    case 0: // HH
        _WriteReg(0x30 + 7, m_pi.vcache[3] | (m_pi.vcache[0] << 4));
        break;
    case 2: // TOM
    case 1: // CYM
        _WriteReg(0x30 + 8, m_pi.vcache[1] | (m_pi.vcache[2] << 4));
        break;
    default:
        break;
    }
}

void COpllDevice::PercSetProgram(uint8_t bank, uint8_t prog)
{
    m_pi.bank = bank;
    m_pi.prog = prog;
}

void COpllDevice::PercSetVolume(uint8_t vol)
{
    m_pi.volume = vol;
    for (int i = 1; i < 6; i++)
        _PercUpdateVolume(i);
}

void COpllDevice::PercKeyOn(uint8_t note)
{
    note = perc_table[note];
    if (0 < note)
    {
        if (m_pi.keymap & (1 << (note - 1)))
            _WriteReg(0xE, 0x20 | (m_pi.keymap & ~(1 << (note - 1))));
        m_pi.keymap |= (1 << (note - 1));
        _WriteReg(0xE, 0x20 | m_pi.keymap);
    }
}

void COpllDevice::PercKeyOff(uint8_t note)
{
    note = perc_table[note];
    if (note)
    {
        m_pi.keymap &= ~(1 << (note - 1));
        _WriteReg(0xE, 0x20 | m_pi.keymap);
    }
}
