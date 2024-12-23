#ifndef __CSCC_DEVICE_HPP__
#define __CSCC_DEVICE_HPP__
#include <stdint.h>

#include <deque>
namespace dsa
{
namespace C
{
#include "device/emu2212.h"
}
}; // namespace dsa
#include "ISoundDevice.hpp"

namespace dsa
{

class CSccDevice : public ISoundDevice
{
  public:
    struct Instrument
    {
        uint8_t wav;
        int8_t  oct;
        uint8_t ar, dr, sl, sr, rr;
    };
    enum EnvState
    {
        SETTLE,
        ATTACK,
        DECAY,
        SUSTINE,
        RELEASE,
        FINISH
    };
    struct ChannelInfo
    {
        EnvState env_state;
        uint32_t env_speed;
        uint32_t env_value;
        uint8_t  program;
        uint8_t  volume;
        uint8_t  velocity;
        uint16_t freq;
        uint8_t  note;
        int8_t   bend_coarse;
        int8_t   bend_fine;
        double   _bend_fine;
        uint8_t  pan;
        bool     keyon;
    };

  private:
    uint32_t            m_rate;
    uint32_t            m_env_counter, m_env_incr;
    uint32_t            m_nch;
    C::SCC             *m_scc[2];
    uint8_t             m_reg_cache[2][0x100];
    uint16_t            m_note2freq[128];
    ChannelInfo         m_ci[5];
    std::deque<int32_t> m_rbuf[2]; // The rendering buffer
    void                _UpdateVolume(uint32_t ch);
    void                _UpdateFreq(uint32_t ch);
    void                _UpdateProgram(uint32_t ch);
    void                _WriteReg(uint8_t reg, uint8_t val, int32_t pan = -1);
    void                _CalcEnvelope(void);

  public:
    CSccDevice(uint32_t rate = 44100, uint32_t nch = 2);
    virtual ~CSccDevice();
    const SoundDeviceInfo &GetDeviceInfo(void) const;
    bool                   Reset(void);
    bool                   Render(int32_t buf[2]);

    void PercKeyOn(uint8_t note) {(void)note;};
    void PercKeyOff(uint8_t note) {(void)note;};
    void PercSetVolume(uint8_t vol) {(void)vol;};
    void PercSetVelocity(uint8_t note, uint8_t velo) {(void)note; (void)velo;};
    void PercSetProgram(uint8_t note, uint8_t velo) {(void)note; (void)velo;};

    void SetProgram(uint32_t ch, uint8_t bank, uint8_t prog);
    void SetVelocity(uint32_t ch, uint8_t vel);
    void SetPan(uint32_t ch, uint8_t pan);
    void SetVolume(uint32_t ch, uint8_t vol);
    void SetBend(uint32_t ch, int8_t coarse, int8_t fine);
    void KeyOn(uint32_t ch, uint8_t note);
    void KeyOff(uint32_t ch);
};

} // namespace dsa

#endif
