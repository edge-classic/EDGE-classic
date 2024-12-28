#include "CSccDevice.hpp"

#include <limits.h>
#include <math.h>
#include <string.h>

using namespace dsa;
using namespace dsa::C;

static CSccDevice::Instrument inst_table[128] = {
#include "SccInst.h"
};
static uint32_t decay_table[256][4];

static uint8_t scctone[128][32] = {
#include "SccWave.h"
};

void CSccDevice::_CalcEnvelope(void)
{

    m_env_counter += m_env_incr;
    if (m_env_counter < 0x10000000)
        return;
    m_env_counter &= 0xFFFFFFF;

    for (uint32_t ch = 0; ch < 5; ch++)
    {
        switch (m_ci[ch].env_state)
        {
        case ATTACK:
            if (m_ci[ch].env_speed + m_ci[ch].env_value < 0x10000000)
                m_ci[ch].env_value += m_ci[ch].env_speed;
            else
            {
                m_ci[ch].env_value = 0x10000000;
                m_ci[ch].env_speed = decay_table[inst_table[m_ci[ch].program].dr][0] >> 4;
                m_ci[ch].env_state = DECAY;
            }
            break;
        case DECAY:
            if ((m_ci[ch].env_value > m_ci[ch].env_speed) &&
                (m_ci[ch].env_value > (((uint32_t)inst_table[m_ci[ch].program].sl) << 20)))
            {
                m_ci[ch].env_value -= m_ci[ch].env_speed;
            }
            else
            {
                m_ci[ch].env_speed = decay_table[inst_table[m_ci[ch].program].sr][0] >> 4;
                m_ci[ch].env_value = (((uint32_t)inst_table[m_ci[ch].program].sl) << 20);
                m_ci[ch].env_state = SUSTINE;
            }
            break;
        case SUSTINE:
            if ((m_ci[ch].env_speed > m_ci[ch].env_value))
            {
                m_ci[ch].env_value = 0;
                m_ci[ch].env_state = FINISH;
            }
            else
            {
                m_ci[ch].env_value -= m_ci[ch].env_speed;
            }
            break;
        case RELEASE:
            if ((m_ci[ch].env_speed > m_ci[ch].env_value))
            {
                m_ci[ch].env_value = 0;
                m_ci[ch].env_state = FINISH;
            }
            else
            {
                m_ci[ch].env_value -= m_ci[ch].env_speed;
            }
            break;
        default:
            break;
        }
        _UpdateVolume(ch);
    }
}

CSccDevice::CSccDevice(uint32_t rate, uint32_t nch)
{

    if (nch == 2)
        m_nch = 2;
    else
        m_nch = 1;
    m_rate = rate;

    {
        for (uint32_t i = 0; i < m_nch; i++)
            m_scc[i] = SCC_new(3579545, rate);
    }

    Reset();

    {
        for (int i = 0; i < 127; i++)
        {
            m_note2freq[i] = (uint16_t)(3579545.0 / 16 / (440.0 * pow(2.0, (double)(i - 57) / 12)));
            if (0xFFF < m_note2freq[i])
                m_note2freq[i] = 0xFFF;
        }
    }

    for (int j = 0; j < 4; j++)
    {
        double span[4]      = {1600.0, 1400.0, 1200.0, 1000.0};
        double mult         = pow(10.0, log10(span[j]) / 256); // 256 = env width
        double base         = 1.0;
        decay_table[255][j] = 0x10000000;
        for (int i = 1; i < 255; i++)
        {
            double tmp              = (uint32_t)(1000.0 / base * 0x10000000 / 60);
            decay_table[255 - i][j] = ((tmp < 0x10000000) ? (uint32_t)tmp : 0x10000000);
            base *= mult;
        }
        decay_table[0][j] = 0;
    }
}

CSccDevice::~CSccDevice()
{
    for (uint32_t i = 0; i < m_nch; i++)
    {
        m_rbuf[i].clear();
        SCC_delete(m_scc[i]);
    }
}

const SoundDeviceInfo &CSccDevice::GetDeviceInfo(void) const
{

    static SoundDeviceInfo si;
    si.max_ch  = 5;
    si.version = 0x0001;
    si.name    = (uint8_t *)"SCC";
    si.desc    = (uint8_t *)"";
    return si;
}

bool CSccDevice::Reset(void)
{

    {
        for (uint32_t i = 0; i < m_nch; i++)
        {
            SCC_reset(m_scc[i]);
            SCC_set_type(m_scc[i], SCC_ENHANCED);
            memset(m_reg_cache[i],0,256);
            m_rbuf[i].clear();
        }
    }

    m_env_counter = 0;
    m_env_incr    = (0x10000000 / m_rate) * 60;

    for (int i = 0; i < 5; i++)
    {
        m_ci[i].keyon       = false;
        m_ci[i].note        = 0;
        m_ci[i].program     = 0;
        m_ci[i].bend_coarse = 0;
        m_ci[i].bend_fine   = 0;
        m_ci[i]._bend_fine  = 1.0;
        m_ci[i].freq        = 0;
        m_ci[i].velocity    = 127;
        m_ci[i].volume      = 127;
        m_ci[i].pan         = 64;
        m_ci[i].env_state   = FINISH;
        m_ci[i].env_value   = 0;
    }

    return true;
}

void CSccDevice::_WriteReg(uint8_t reg, uint8_t val, int32_t pan)
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
        SCC_writeReg(m_scc[pan], reg, val);
        m_reg_cache[pan][reg] = val;
        if (m_rbuf[pan].size() < 8192)
        {
            m_rbuf[pan].push_back(SCC_calc(m_scc[pan]));
            if (!pan)
                _CalcEnvelope();
        }
    }
}

bool CSccDevice::Render(int32_t buf[2])
{

    for (uint32_t i = 0; i < m_nch; i++)
    {
        if (m_rbuf[i].empty())
        {
            buf[i] = SCC_calc(m_scc[i]);
            if (!i)
                _CalcEnvelope();
        }
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

void CSccDevice::SetPan(uint32_t ch, uint8_t pan)
{
    m_ci[ch].pan = pan;
    _UpdateVolume(ch);
}

void CSccDevice::_UpdateVolume(uint32_t ch)
{

    int vol = m_ci[ch].volume / 16 + m_ci[ch].velocity / 16 + 1;
    vol     = (vol * (m_ci[ch].env_value >> 20) / 256);
    if (vol > 15)
        vol = 15;

    if (m_ci[ch].keyon == false)
    {
        _WriteReg(0xD0 + ch, 0);
        return;
    }

    if (m_nch < 2)
    {
        _WriteReg(0xD0 + ch, vol);
        return;
    }

    // LEFT CHANNEL
    if (64 < m_ci[ch].pan)
    {
        int tmp = vol - (m_ci[ch].pan - 64) / 4;
        _WriteReg(0xD0 + ch, (0 < tmp) ? tmp : 0, 0);
    }
    else
    {
        _WriteReg(0xD0 + ch, vol, 0);
    }

    // RIGHT CHANNEL
    if (m_ci[ch].pan < 64)
    {
        int tmp = vol - (63 - m_ci[ch].pan) / 4;
        _WriteReg(0xD0 + ch, (0 < tmp) ? tmp : 0, 1);
    }
    else
    {
        _WriteReg(0xD0 + ch, vol, 1);
    }
}

void CSccDevice::_UpdateFreq(uint32_t ch)
{
    int note = m_ci[ch].note + m_ci[ch].bend_coarse + (int)inst_table[m_ci[ch].program].oct * 12;
    if (note < 0)
        note = 0;
    else if (127 < note)
        note = 127;

    int fnum = (int)((double)m_note2freq[note] / m_ci[ch]._bend_fine);
    if (0xFFF < fnum)
        fnum = 0xFFF;

    _WriteReg(0xC0 + ch * 2, fnum & 0xff);
    _WriteReg(0xC0 + ch * 2 + 1, fnum >> 8);
}

void CSccDevice::SetBend(uint32_t ch, int8_t coarse, int8_t fine)
{
    m_ci[ch].bend_coarse = coarse;
    m_ci[ch].bend_fine   = fine;
    m_ci[ch]._bend_fine  = pow(2.0, (double)fine / 1200);
    _UpdateFreq(ch);
}

void CSccDevice::_UpdateProgram(uint32_t ch)
{
    for (int i = 0; i < 32; i++)
        _WriteReg(ch * 32 + i, scctone[inst_table[m_ci[ch].program].wav][i]);
}

void CSccDevice::SetProgram(uint32_t ch, uint8_t bank, uint8_t prog)
{
    (void)bank;
    m_ci[ch].program = prog;
}

void CSccDevice::SetVolume(uint32_t ch, uint8_t vol)
{
    m_ci[ch].volume = vol;
    _UpdateVolume(ch);
}

void CSccDevice::SetVelocity(uint32_t ch, uint8_t velo)
{
    m_ci[ch].velocity = velo;
    _UpdateVolume(ch);
}

void CSccDevice::KeyOn(uint32_t ch, uint8_t note)
{
    if (!m_ci[ch].keyon)
    {
        m_ci[ch].note      = note;
        m_ci[ch].keyon     = true;
        m_ci[ch].env_value = 0; // inst_table[m_ci[ch].program].iv<<20;
        m_ci[ch].env_speed = decay_table[inst_table[m_ci[ch].program].ar][0];
        m_ci[ch].env_state = ATTACK;
        _UpdateProgram(ch);
        _UpdateFreq(ch);
        _UpdateVolume(ch);
    }
}

void CSccDevice::KeyOff(uint32_t ch)
{
    if (m_ci[ch].keyon)
    {
        m_ci[ch].keyon     = false;
        m_ci[ch].env_state = RELEASE;
        m_ci[ch].env_speed = decay_table[inst_table[m_ci[ch].program].rr][0] >> 4;
        _UpdateVolume(ch);
    }
}
