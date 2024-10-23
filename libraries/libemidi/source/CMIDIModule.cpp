#include "CMIDIModule.hpp"

using namespace dsa;

CMIDIModule::CMIDIModule() : m_device(NULL)
{
}
CMIDIModule::~CMIDIModule()
{
}

bool CMIDIModule::Reset()
{

    if (m_device == NULL)
        return false;
    if (!m_device->Reset())
        return false;

    m_off_channels.clear();

    {
        for (int i = 0; i < 16; i++)
        {
            m_used_channels[i].clear();
            m_program[i]     = 3;
            m_volume[i]      = 127;
            m_bend[i]        = 0;
            m_bend_coarse[i] = 0;
            m_bend_fine[i]   = 0;
            m_bend_range[i]  = (2 << 7);
            m_pan[i]         = 64;
            m_RPN[i] = m_NRPN[i] = 0;
            m_drum[i]            = 0;
            for (int j = 0; j < 128; j++)
                m_keyon_table[i][j] = -1;
        }
    }
    m_drum[9] = 1;

    m_entry_mode = 0;

    const SoundDeviceInfo &si = m_device->GetDeviceInfo();

    {
        for (uint32_t i = 0; i < si.max_ch; i++)
        {
            KeyInfo ki;
            ki.midi_ch          = i;
            ki.dev_ch           = i;
            ki.note             = 0;
            m_keyon_table[i][0] = 0;
            m_off_channels.push_back(ki);
            m_used_channels[i].push_back(ki);
        }
    }

    return true;
}

void CMIDIModule::Panpot(uint8_t midi_ch, bool is_fine, uint8_t data)
{
    if (!is_fine)
    {
        m_pan[midi_ch] = data;
        std::deque<KeyInfo>::iterator it;
        for (it = m_used_channels[midi_ch].begin(); it != m_used_channels[midi_ch].end(); it++)
            m_device->SetPan((*it).dev_ch, m_pan[midi_ch]);
    }
}

void CMIDIModule::UpdatePitchBend(uint8_t midi_ch)
{
    int range = (m_bend_range[midi_ch] >> 7);
    if (range != 0)
    {
        m_bend_coarse[midi_ch] = (m_bend[midi_ch] * range) / 8192;                          // note offset
        m_bend_fine[midi_ch]   = ((m_bend[midi_ch] % (8192 / range)) * 100 * range) / 8192; // cent offset
    }
    else
    {
        m_bend_coarse[midi_ch] = 0;
        m_bend_fine[midi_ch]   = 0;
    }
    std::deque<KeyInfo>::iterator it;
    for (it = m_used_channels[midi_ch].begin(); it != m_used_channels[midi_ch].end(); it++)
        m_device->SetBend((*it).dev_ch, m_bend_coarse[midi_ch], m_bend_fine[midi_ch]);
}

void CMIDIModule::PitchBend(uint8_t midi_ch, uint8_t msb, uint8_t lsb)
{
    m_bend[midi_ch] = ((msb & 0x7f) | ((lsb & 0x7f) << 7)) - 8192;
    UpdatePitchBend(midi_ch);
}

void CMIDIModule::ChannelPressure(uint8_t midi_ch, uint8_t velo)
{
    std::deque<KeyInfo>::iterator it;
    for (it = m_used_channels[midi_ch].begin(); it != m_used_channels[midi_ch].end(); it++)
        m_device->SetVelocity((*it).dev_ch, velo);
}

void CMIDIModule::NoteOn(uint8_t midi_ch, uint8_t note, uint8_t velo)
{

    if (m_drum[midi_ch])
    {
        m_device->PercSetVelocity(note, velo);
        m_device->PercKeyOn(note);
        return;
    }

    if (0 <= m_keyon_table[midi_ch][note])
        return; // キーオン中なら無視

    KeyInfo ki;

    if (m_off_channels.empty())
    {     // キーオフ中のデバイスチャンネルが無いとき
        ki.dev_ch = -1;
        for (int i = 0; i < 16; i++)
        { // 発音数が規定値より多いMIDIチャンネルを消音
            if (m_used_channels[i].size() > 1)
            {
                ki = m_used_channels[i].front();
                m_device->KeyOff(ki.dev_ch);
                m_keyon_table[i][ki.note] = -1;
                m_used_channels[i].pop_front();
                break;
            }
        }
        if (ki.dev_ch == -1)
        { // だめならどこでもいいから消音
            for (int i = 0; i < 16; i++)
            {
                if (!m_used_channels[i].empty())
                {
                    ki = m_used_channels[i].front();
                    m_device->KeyOff(ki.dev_ch);
                    m_keyon_table[i][ki.note] = -1;
                    m_used_channels[i].pop_front();
                    break;
                }
            }
        }
    }
    else
    { // キーオフ中のチャンネルがあるときはそれを利用
        ki = m_off_channels.front();
        m_off_channels.pop_front();
        std::deque<KeyInfo>::iterator it;
        for (it = m_used_channels[ki.midi_ch].begin(); it != m_used_channels[ki.midi_ch].end(); it++)
        {
            if ((*it).dev_ch == ki.dev_ch)
            {
                m_used_channels[ki.midi_ch].erase(it);
                break;
            }
        }
    }

    m_device->SetProgram(ki.dev_ch, 0, m_program[midi_ch]);
    m_device->SetVolume(ki.dev_ch, m_volume[midi_ch]);
    m_device->SetVelocity(ki.dev_ch, velo);
    m_device->SetBend(ki.dev_ch, m_bend_coarse[midi_ch], m_bend_fine[midi_ch]);
    m_device->SetPan(ki.dev_ch, m_pan[midi_ch]);
    m_device->KeyOn(ki.dev_ch, note);
    m_keyon_table[midi_ch][note] = ki.dev_ch;
    ki.midi_ch                   = midi_ch;
    ki.note                      = note;
    m_used_channels[midi_ch].push_back(ki);
}

void CMIDIModule::NoteOff(uint8_t midi_ch, uint8_t note, uint8_t velo)
{

    if (m_drum[midi_ch])
    {
        m_device->PercKeyOff(note);
    }

    int dev_ch = m_keyon_table[midi_ch][note];
    if (dev_ch < 0)
        return;
    m_device->KeyOff(dev_ch);
    m_keyon_table[midi_ch][note] = -1;
    std::deque<KeyInfo>::iterator it;
    KeyInfo                       ki;
    ki.dev_ch  = dev_ch;
    ki.midi_ch = midi_ch;
    ki.note    = 0;
    m_off_channels.push_back(ki);
}

void CMIDIModule::MainVolume(uint8_t midi_ch, bool is_fine, uint8_t data)
{

    if (is_fine)
        return;

    if (m_drum[midi_ch])
    {
        m_device->PercSetVolume(data);
        return;
    }

    std::deque<KeyInfo>::iterator it;
    for (it = m_used_channels[midi_ch].begin(); it != m_used_channels[midi_ch].end(); it++)
        m_device->SetVolume((*it).dev_ch, data);
}

void CMIDIModule::LoadRPN(uint8_t midi_ch, uint16_t data)
{
    switch (m_RPN[midi_ch])
    {
    case 0x0000:
        m_bend_range[midi_ch] = data;
        UpdatePitchBend(midi_ch);
        break;
    default:
        break;
    }
}

uint16_t CMIDIModule::SaveRPN(uint8_t midi_ch)
{
    switch (m_RPN[midi_ch])
    {
    case 0x0000:
        return m_bend_range[midi_ch];
        break;
    default:
        return 0;
        break;
    }
}

void CMIDIModule::ResetRPN(uint8_t midi_ch)
{
    m_bend_range[midi_ch] = (2 << 7);
}

void CMIDIModule::LoadNRPN(uint8_t midi_ch, uint16_t data)
{
}

uint16_t CMIDIModule::SaveNRPN(uint8_t midi_ch)
{
    return 0;
}

void CMIDIModule::ResetNRPN(uint8_t midi_ch)
{
}

void CMIDIModule::DataEntry(uint8_t midi_ch, bool is_fine, uint8_t data)
{
    int entry = m_entry_mode ? SaveNRPN(midi_ch) : SaveRPN(midi_ch);
    if (is_fine)
        entry = (entry & 0x3F80) | (data & 0x7F);
    else
        entry = ((data & 0x7F) << 7) | (entry & 0x7F);
    m_entry_mode ? LoadNRPN(midi_ch, entry) : LoadRPN(midi_ch, entry);
}

void CMIDIModule::DataIncrement(uint8_t midi_ch, uint8_t data)
{
    int entry = m_entry_mode ? SaveNRPN(midi_ch) : SaveRPN(midi_ch);
    if (entry < 0x3FFF)
        entry++;
    m_entry_mode ? LoadNRPN(midi_ch, entry) : LoadRPN(midi_ch, entry);
}

void CMIDIModule::DataDecrement(uint8_t midi_ch, uint8_t data)
{
    int entry = m_entry_mode ? SaveNRPN(midi_ch) : SaveRPN(midi_ch);
    if (entry > 0)
        entry--;
    m_entry_mode ? LoadNRPN(midi_ch, entry) : LoadRPN(midi_ch, entry);
}

void CMIDIModule::NRPN(uint8_t midi_ch, bool is_lsb, uint8_t data)
{
    if (is_lsb)
    {
        m_NRPN[midi_ch] = (m_NRPN[midi_ch] & 0x3F80) | (data & 0x7F);
    }
    else
    {
        m_NRPN[midi_ch] = ((data & 0x7F) << 7) | (m_NRPN[midi_ch] & 0x7F);
    }
    if (m_NRPN[midi_ch] == 0x3FFF)
    { // NRPN NULL
        ResetNRPN(midi_ch);
    }
    if (m_entry_mode == 0)
    {
        m_entry_mode = 1; // NRPN MODE
    }
}

void CMIDIModule::RPN(uint8_t midi_ch, bool is_lsb, uint8_t data)
{
    if (is_lsb)
    {
        m_RPN[midi_ch] = (m_RPN[midi_ch] & 0x3F80) | (data & 0x7F);
        // if(m_RPN[midi_ch] == 0x3FFF) RPN_Reset();
    }
    else
    {
        m_RPN[midi_ch] = ((data & 0x7F) << 7) | (m_RPN[midi_ch] & 0x7F);
    }
    if (m_RPN[midi_ch] == 0x3FFF)
    { // RPN NULL
        ResetRPN(midi_ch);
    }
    if (m_entry_mode == 1)
    {
        m_entry_mode = 0; // RPN MODE
    }
}

void CMIDIModule::ControlChange(uint8_t midi_ch, uint8_t msb, uint8_t lsb)
{

    if (msb < 0x40)
    { // 14-bit
        bool is_low = (msb & 0x20) ? true : false;
        switch (msb & 0x1F)
        {
        // case 0x00: BankSelect(midi_ch, is_low, lsb); break;
        // case 0x01: ModulationDepth(midi_ch, is_low, lsb); break;
        // case 0x02: BreathControl(midi_ch, is_low, lsb); break;
        // case 0x04: FootControl(midi_ch, is_low, lsb); break;
        // case 0x05: PortamentTime(midi_ch, is_low, lsb); break;
        case 0x06:
            DataEntry(midi_ch, is_low, lsb);
            break;
        case 0x07:
            MainVolume(midi_ch, is_low, lsb);
            break;
        // case 0x08: BalanceControl(midi_ch, is_low, lsb); break;
        case 0x0A:
            Panpot(midi_ch, is_low, lsb);
            break;
        // case 0x11: Expression(midi_ch, is_low, lsb); break;
        default:
            break;
        }
    }
    else
    { // 7-bit
        switch (msb)
        {
        case 0x40:
            break;
        case 0x60:
            DataIncrement(midi_ch, lsb);
            break;
        case 0x61:
            DataDecrement(midi_ch, lsb);
            break;
        case 0x62:
            NRPN(midi_ch, 0, lsb);
            break;
        case 0x63:
            NRPN(midi_ch, 1, lsb);
            break;
        case 0x64:
            RPN(midi_ch, 0, lsb);
            break;
        case 0x65:
            RPN(midi_ch, 1, lsb);
            break;
        default:
            break;
        }
    }
}

bool CMIDIModule::Render(int32_t buf[2])
{
    if (m_device == NULL)
        return false;
    else
        return m_device->Render(buf);
}

bool CMIDIModule::SendMIDIMsg(const CMIDIMsg &msg)
{

    if (m_device == NULL)
        return false;

    if (msg.m_type == CMIDIMsg::NOTE_OFF)
    {
        NoteOff(msg.m_ch, msg.m_data1, msg.m_data2);
    }
    else if (msg.m_type == CMIDIMsg::NOTE_ON)
    {
        if (msg.m_data2 == 0)
            NoteOff(msg.m_ch, msg.m_data1, msg.m_data2);
        else
            NoteOn(msg.m_ch, msg.m_data1, msg.m_data2);
    }
    else if (msg.m_type == CMIDIMsg::PROGRAM_CHANGE)
    {
        m_program[msg.m_ch] = msg.m_data1;
    }
    else if (msg.m_type == CMIDIMsg::CONTROL_CHANGE)
    {
        ControlChange(msg.m_ch, msg.m_data1, msg.m_data2);
    }
    else if (msg.m_type == CMIDIMsg::PITCH_BEND_CHANGE)
    {
        PitchBend(msg.m_ch, msg.m_data1, msg.m_data2);
    }
    else if (msg.m_type == CMIDIMsg::CHANNEL_PRESSURE)
    {
        ChannelPressure(msg.m_ch, msg.m_data1);
    }
    return true;
}

bool CMIDIModule::SetDrumChannel(int midi_ch, int enable)
{
    m_drum[midi_ch] = enable;
    return true;
}
