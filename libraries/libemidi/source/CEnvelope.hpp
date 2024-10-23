#ifndef __CENVELOPE_HPP__
#define __CENVELOPE_HPP__

#include <stdint.h>

namespace dsa
{

class CEnvelope
{
  public:
    enum EnvState
    {
        SETTLE,
        ATTACK,
        DECAY,
        SUSTINE,
        RELEASE,
        FINISH
    };
    struct Param
    {
        uint32_t ar, dr, sl, sr, rr;
    };
    struct ChannelInfo
    {
        EnvState state;
        uint32_t speed;
        uint32_t value;
        Param    param;
    };

  private:
    uint32_t     m_ch;
    ChannelInfo *m_ci;
    uint32_t     m_clock;
    uint32_t     m_rate;
    uint32_t     m_cnt;
    uint32_t     m_inc;
    uint32_t     _CalcSpeed(uint32_t ms);

  public:
    CEnvelope(uint32_t ch);
    ~CEnvelope();
    void     Reset(uint32_t clock = 44100, uint32_t rate = 60);
    void     KeyOn(uint32_t ch);
    void     KeyOff(uint32_t ch);
    bool     Update();
    void     SetParam(uint32_t ch, const Param &param);
    uint32_t GetValue(uint32_t ch) const;
};

} // namespace dsa
#endif
