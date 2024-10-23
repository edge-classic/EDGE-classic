#include "CEnvelope.hpp"

using namespace dsa;

#define GETA_BITS 20
#define MAX_CNT   (1 << (GETA_BITS + 8))

uint32_t CEnvelope::_CalcSpeed(uint32_t ms)
{
    if (ms == 0)
        return MAX_CNT;
    else
        return (MAX_CNT / (ms * m_clock / 1000)) * m_rate;
}

CEnvelope::CEnvelope(uint32_t ch) : m_ch(ch)
{
    m_ci = new ChannelInfo[ch];
}

CEnvelope::~CEnvelope()
{
    delete[] m_ci;
}

void CEnvelope::Reset(uint32_t clock, uint32_t rate)
{
    m_rate  = rate;
    m_clock = clock;
    m_cnt   = 0;
    m_inc   = (MAX_CNT / m_clock) * m_rate;
    for (uint32_t ch = 0; ch < m_ch; ch++)
    {
        m_ci[ch].value = 0;
        m_ci[ch].speed = 0;
        m_ci[ch].state = FINISH;
    }
}

bool CEnvelope::Update()
{

    m_cnt += m_inc;
    if (m_cnt < MAX_CNT)
        return false;
    m_cnt &= 0xFFFFFFF;

    for (uint32_t ch = 0; ch < m_ch; ch++)
    {

        switch (m_ci[ch].state)
        {
        case ATTACK:
            if (m_ci[ch].speed + m_ci[ch].value < MAX_CNT)
                m_ci[ch].value += m_ci[ch].speed;
            else
            {
                m_ci[ch].value = MAX_CNT;
                m_ci[ch].speed = _CalcSpeed(m_ci[ch].param.dr);
                m_ci[ch].state = DECAY;
            }
            break;
        case DECAY:
            if ((m_ci[ch].value > m_ci[ch].speed) && (m_ci[ch].value > (uint32_t)m_ci[ch].param.sl << GETA_BITS))
            {
                m_ci[ch].value -= m_ci[ch].speed;
            }
            else
            {
                m_ci[ch].speed = _CalcSpeed(m_ci[ch].param.sr);
                m_ci[ch].value = (uint32_t)m_ci[ch].param.sl << GETA_BITS;
                m_ci[ch].state = SUSTINE;
            }
            break;
        case SUSTINE:
            if ((m_ci[ch].speed > m_ci[ch].value))
            {
                m_ci[ch].value = 0;
                m_ci[ch].state = FINISH;
            }
            else
            {
                m_ci[ch].value -= m_ci[ch].speed;
            }
            break;
        case RELEASE:
            if ((m_ci[ch].speed > m_ci[ch].value))
            {
                m_ci[ch].value = 0;
                m_ci[ch].state = FINISH;
            }
            else
            {
                m_ci[ch].value -= m_ci[ch].speed;
            }
            break;
        default:
            break;
        }
    }

    return true;
}

void CEnvelope::KeyOn(uint32_t ch)
{
    m_ci[ch].value = 0;
    m_ci[ch].speed = _CalcSpeed(m_ci[ch].param.ar);
    m_ci[ch].state = ATTACK;
}

void CEnvelope::KeyOff(uint32_t ch)
{
    m_ci[ch].state = RELEASE;
    m_ci[ch].speed = _CalcSpeed(m_ci[ch].param.rr);
}

void CEnvelope::SetParam(uint32_t ch, const Param &param)
{
    m_ci[ch].param = param;
}

uint32_t CEnvelope::GetValue(uint32_t ch) const
{
    return m_ci[ch].value >> GETA_BITS;
}
