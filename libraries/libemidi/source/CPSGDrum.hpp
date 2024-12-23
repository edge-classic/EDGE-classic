#ifndef __CPSG_DRUM_HPP__
#include <stdint.h>

#include <deque>
namespace dsa
{
namespace C
{
#include "device/emu2149.h"
}
}; // namespace dsa
#include "CEnvelope.hpp"
#include "ISoundDevice.hpp"

namespace dsa
{

class CPSGDrum : public ISoundDevice
{
  public:
    struct Instrument
    {
        uint8_t          note;
        int8_t           vol;
        uint8_t          noise;
        CEnvelope::Param param;
    };

    struct ChannelInfo
    {
        uint8_t note;
        int8_t  vol;
        uint8_t noise;
        bool    keyon;
    };

  private:
    uint32_t m_rate;
    uint32_t m_nch;
    C::PSG  *m_psg[2];
    uint8_t  m_reg_cache[2][0x10];
    uint8_t  m_noise_mode[2];
    uint16_t m_note2freq[128];
    struct KeyInfo
    {
        uint32_t ch;
        uint8_t  note;
    };
    std::deque<KeyInfo>  m_on_channels;
    std::deque<uint32_t> m_off_channels;
    ChannelInfo          m_ci[6];
    CEnvelope            m_env;
    uint8_t              m_volume;
    uint8_t              m_velocity[128];
    int32_t              m_keytable[128];
    std::deque<int32_t>  m_rbuf[2];
    void                 _UpdateMode(uint32_t ch);
    void                 _UpdateVolume(uint32_t ch);
    void                 _UpdateFreq(uint32_t ch);
    void                 _UpdateProgram(uint32_t ch);
    void                 _WriteReg(uint8_t reg, uint8_t val, uint32_t id);

  public:
    explicit CPSGDrum(uint32_t rate = 44100, uint32_t m_nch = 1);
    virtual ~CPSGDrum();
    const SoundDeviceInfo &GetDeviceInfo(void) const;
    bool                   Reset(void);
    bool                   Render(int32_t buf[2]);

    void PercKeyOn(uint8_t note);
    void PercKeyOff(uint8_t note);
    void PercSetVolume(uint8_t vol);
    void PercSetVelocity(uint8_t note, uint8_t velo);
    void PercSetProgram(uint8_t bank, uint8_t prog);

    void SetProgram(uint32_t ch, uint8_t bank, uint8_t prog) {(void)ch; (void)bank; (void)prog;};
    void SetVelocity(uint32_t ch, uint8_t vel) {(void)ch; (void)vel;};
    void SetPan(uint32_t ch, uint8_t pan) {(void)ch; (void)pan;};
    void SetVolume(uint32_t ch, uint8_t vol) {(void)ch; (void)vol;};
    void SetBend(uint32_t ch, int8_t coarse, int8_t fine) {(void)ch, (void)coarse; (void)fine;};
    void KeyOn(uint32_t ch, uint8_t note) {(void)ch; (void)note;};
    void KeyOff(uint32_t ch) {(void)ch;};
};

} // namespace dsa

#endif
