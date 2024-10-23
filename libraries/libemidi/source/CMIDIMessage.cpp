#include "CMIDIMessage.hpp"

using namespace dsa;

CMIDIMsg::CMIDIMsg(MsgType type, int ch, uint8_t data1, uint8_t data2 )
    : m_type(type), m_ch(ch), m_data1(data1), m_data2(data2)
{
}

CMIDIMsg &CMIDIMsg::operator=(const CMIDIMsg &arg)
{
    m_type   = arg.m_type;
    m_ch     = arg.m_ch;
    m_data1  = arg.m_data1;
    m_data2  = arg.m_data2;
    return (*this);
}

CMIDIMsg::CMIDIMsg(const CMIDIMsg &arg)
{
    m_type   = arg.m_type;
    m_ch     = arg.m_ch;
    m_data1  = arg.m_data1;
    m_data2  = arg.m_data2;
}

CMIDIMsg::~CMIDIMsg()
{
}

const char *CMIDIMsg::c_str() const
{
    const char *text[] = {// CANNEL
                          "NOTE_OFF", "NOTE_ON", "POLYPHONIC_KEY_PRESSURE", "CONTROL_CHANGE", "PROGRAM_CHANGE",
                          "CHANNEL_PRESSURE", "PITCH_BEND_CHANGE",
                          // MODE
                          "ALL_SOUND_OFF", "RESET_ALL_CONTROLLERS", "LOCAL_CONTROL", "ALL_NOTES_OFF", "OMNI_OFF",
                          "OMNI_ON", "POLYPHONIC_OPERATION", "MONOPHONIC_OPERATION",
                          // SYSTEM
                          "SYSTEM_EXCLUSIVE", "MTC_QUARTER_FRAME", "SONG_POSITION_POINTER", "SONG_SELECT",
                          "TUNE_REQUEST",
                          // REALTIME
                          "REALTIME_CLOCK", "REALTIME_TICK", "REALTIME_START", "REALTIME_CONTINUE", "REALTIME_STOP",
                          "REALTIME_ACTIVE_SENSE", "REALTIME_SYSTEM_RESET", "UNKNOWN_MESSAGE"};
    return text[m_type];
}