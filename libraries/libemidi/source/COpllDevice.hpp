#ifndef __CDeviceOpll_H__
#define __CDeviceOpll_H__
#include <stdint.h>

#include <deque>

#include "ISoundDevice.hpp"

namespace dsa
{

namespace C
{
#include "device/emu2413.h"
} // namespace C

class COpllDevice : public ISoundDevice
{
  public:
    struct PercInfo
    {
        uint8_t volume;
        uint8_t vcache[5];
        uint8_t velocity[5];
        uint8_t keymap;
        uint8_t bank;
        uint8_t prog;
    };
    struct ChannelInfo
    {
        uint16_t fnum;
        uint8_t  bank;
        uint8_t  program;
        uint8_t  octave;
        uint8_t  velocity;
        uint8_t  volume;
        uint8_t  note;
        uint8_t  pan;
        int8_t   bend_coarse;
        int8_t   bend_fine;
        bool     keyon;
        double   _bend_fine;
    };

  private:
    uint32_t            m_nch;
    C::OPLL            *m_opll[2];
    uint8_t             m_reg_cache[2][0x80];
    ChannelInfo         m_ci[9];
    PercInfo            m_pi;
    std::deque<int32_t> m_rbuf[2]; // The rendering buffer

    void _UpdateFreq(uint32_t ch);
    void _UpdateVolume(uint32_t ch);
    void _PercUpdateVolume(uint8_t note);
    void _WriteReg(uint8_t reg, uint8_t val, int32_t pan = -1);

  public:
    COpllDevice(uint32_t rate = 44100, uint32_t nch = 2);
    virtual ~COpllDevice();

    const SoundDeviceInfo &GetDeviceInfo(void) const;
    bool                   Reset(void);
    bool                   Render(int32_t buf[2]);

    void SetProgram(uint32_t ch, uint8_t bank, uint8_t prog);
    void SetVelocity(uint32_t ch, uint8_t vel);
    void SetPan(uint32_t ch, uint8_t pan);
    void SetVolume(uint32_t ch, uint8_t vol);
    void SetBend(uint32_t ch, int8_t coarse, int8_t fine);
    void KeyOn(uint32_t ch, uint8_t note);
    void KeyOff(uint32_t ch);

    void PercKeyOn(uint8_t note);
    void PercKeyOff(uint8_t note);
    void PercSetProgram(uint8_t bank, uint8_t prog);
    void PercSetVelocity(uint8_t note, uint8_t vel);
    void PercSetVolume(uint8_t vol);
};

} // namespace dsa

#endif
