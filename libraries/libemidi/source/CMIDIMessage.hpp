#ifndef __MIDI_MESSAGE_HPP__
#define __MIDI_MESSAGE_HPP__
#include <stdint.h>
#include <string.h>

#include <deque>
#include <string>

namespace dsa
{

// MIDI Msgs (Channel or exclusive Msg)
class CMIDIMsg
{
  public:
    enum MsgType
    {
        // CHANNEL Msg
        NOTE_OFF = 0,            // 8n #note #velo
        NOTE_ON,                 // 9n #note #velo
        POLYPHONIC_KEY_PRESSURE, // An #note #data
        CONTROL_CHANGE,          // Bn #ctrl #data
        PROGRAM_CHANGE,          // Cn #data
        CHANNEL_PRESSURE,        // Dn #data
        PITCH_BEND_CHANGE,       // En #data #data
        // MODE Msg
        ALL_SOUND_OFF,         // Bn 78 00
        RESET_ALL_CONTROLLERS, // Bn 79 00
        LOCAL_CONTROL,         // Bn 7A #data
        ALL_NOTES_OFF,         // Bn 7B 00
        OMNI_OFF,              // Bn 7C 00
        OMNI_ON,               // Bn 7D 00
        POLYPHONIC_OPERATION,  // Bn 7E 00
        MONOPHONIC_OPERATION,  // Bn 7F 00
        // SYSTEM Msg
        SYSTEM_EXCLUSIVE,      // F0 ... F7
        MTC_QUARTER_FRAME,     // F1 #data
        SONG_POSITION_POINTER, // F2 #data #data
        SONG_SELECT,           // F3 #data
        TUNE_REQUEST,          // F6
        // REALTIME Msg
        REALTIME_CLOCK,        // F8
        REALTIME_TICK,         // F9
        REALTIME_START,        // FA
        REALTIME_CONTINUE,     // FB
        REALTIME_STOP,         // FC
        REALTIME_ACTIVE_SENSE, // FE
        REALTIME_SYSTEM_RESET, // FF
        UNKNOWN_MESSAGE
    };

    MsgType  m_type;   // The Msg identifier
    uint32_t m_ch;     // The channel
    uint8_t m_data1;
    uint8_t m_data2;
    CMIDIMsg(MsgType type = UNKNOWN_MESSAGE, int ch = 0, uint8_t data1 = 0, uint8_t data2 = 0);
    CMIDIMsg(const CMIDIMsg &);
    ~CMIDIMsg();
    CMIDIMsg   &operator=(const CMIDIMsg &arg);
    const char *c_str() const;
};

} // namespace dsa

#endif
