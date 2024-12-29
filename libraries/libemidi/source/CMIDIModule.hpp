#ifndef __MIDI_MODULE_HPP__
#define __MIDI_MODULE_HPP__

#include <stdint.h>

#include "CMIDIMessage.hpp"
#include "ISoundDevice.hpp"

namespace dsa
{

class CMIDIModule
{
  private:
    struct KeyInfo
    {
        int midi_ch, dev_ch, note;
    };
    ISoundDevice *m_device;

    int m_NRPN[16], m_RPN[16];
    int m_volume[16];
    int m_bend_coarse[16];
    int m_bend_fine[16];
    int m_bend_range[16];
    int m_program[16];
    int m_pan[16];
    int m_bend[16];
    int m_drum[16];
    // そのキーを発音しているチャンネル番号を格納する配列
    int m_keyon_table[16][128];
    // MIDIチャンネルで使用しているOPLLチャンネルの集合(発音順のキュー）
    std::deque<KeyInfo> m_used_channels[16];
    // キーオフしているOPLLチャンネルの集合
    std::deque<KeyInfo> m_off_channels;
    // The current entry value of RPN/NRPN
    // NRPN=1, RPN=0;
    int m_entry_mode;

  protected:
    virtual void     ControlChange(uint8_t ch, uint8_t msb, uint8_t lsb);
    virtual void     NoteOn(uint8_t ch, uint8_t note, uint8_t velo);
    virtual void     NoteOff(uint8_t ch, uint8_t note);
    virtual void     UpdatePitchBend(uint8_t ch);
    virtual void     PitchBend(uint8_t ch, uint8_t msb, uint8_t lsb);
    virtual void     ChannelPressure(uint8_t ch, uint8_t velo);
    virtual void     DataEntry(uint8_t midi_ch, bool is_low, uint8_t data);
    virtual void     DataIncrement(uint8_t midi_ch);
    virtual void     DataDecrement(uint8_t midi_ch);
    virtual void     MainVolume(uint8_t midi_ch, bool is_fine, uint8_t data);
    virtual void     NRPN(uint8_t midi_ch, bool is_fine, uint8_t data);
    virtual void     RPN(uint8_t midi_ch, bool is_fine, uint8_t data);
    virtual void     LoadRPN(uint8_t midi_ch, uint16_t data);
    virtual void     LoadNRPN(uint8_t midi_ch, uint16_t data);
    virtual uint16_t SaveRPN(uint8_t midi_ch);
    virtual uint16_t SaveNRPN(uint8_t midi_ch);
    virtual void     ResetRPN(uint8_t midi_ch);
    virtual void     ResetNRPN(uint8_t midi_ch);
    virtual void     Panpot(uint8_t ch, bool is_fine, uint8_t data);

  public:
    CMIDIModule();
    virtual ~CMIDIModule();
    void AttachDevice(ISoundDevice *device)
    {
        m_device = device;
    }
    ISoundDevice *DetachDevice()
    {
        ISoundDevice *tmp = m_device;
        m_device          = NULL;
        return tmp;
    }
    bool Reset();
    // CMIDIメッセージ形式のMIDIメッセージを処理する。
    bool SendMIDIMsg(const CMIDIMsg &mes);
    // 音声のレンダリングを行う。
    bool Render(int32_t buf[2]);

    bool SetDrumChannel(int midi_ch, int enable);
};

} // namespace dsa

#endif
