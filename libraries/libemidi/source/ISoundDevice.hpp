#ifndef __DSA_ISOUND_DEVICE_HPP__
#define __DSA_ISOUND_DEVICE_HPP__
#include <stdint.h>
// dsa
namespace dsa
{

struct SoundDeviceInfo
{
    uint8_t *name;
    uint8_t *desc;
    uint32_t max_ch;  // Maximum Channels.
    uint32_t version; // Version no.
};

// Sound Device Interface
//
class ISoundDevice
{
  public:
    virtual ~ISoundDevice()
    {
    }
    virtual const SoundDeviceInfo &GetDeviceInfo(void) const = 0;
    virtual bool                   Reset(void)               = 0;
    virtual bool                   Render(int32_t buf[2])    = 0;

    virtual void SetProgram(uint32_t ch, uint8_t bank, uint8_t prog) = 0;
    virtual void SetVelocity(uint32_t ch, uint8_t vel)               = 0;
    virtual void SetPan(uint32_t ch, uint8_t pan)                    = 0;
    virtual void SetVolume(uint32_t ch, uint8_t vol)                 = 0;
    // coarse: Bend depth at note (-128 to +127)
    // fine  : Bend depth at cent (-128 to +128) 100 cent equals 1 note.
    virtual void SetBend(uint32_t ch, int8_t coarse, int8_t fine) = 0;
    virtual void KeyOn(uint32_t ch, uint8_t note)                 = 0;
    virtual void KeyOff(uint32_t ch)                              = 0;

    // For percussions
    virtual void PercKeyOn(uint8_t note)                    = 0;
    virtual void PercKeyOff(uint8_t note)                   = 0;
    virtual void PercSetProgram(uint8_t bank, uint8_t prog) = 0;
    virtual void PercSetVelocity(uint8_t note, uint8_t vel) = 0;
    virtual void PercSetVolume(uint8_t vol)                 = 0;
};

} // namespace dsa

#endif // __DSA_ISOUND_DEVICE_HPP__
