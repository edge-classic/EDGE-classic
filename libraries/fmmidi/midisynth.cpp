// �\�t�g�E�F�AMIDI�V���Z�T�C�U�B
// Copyright(c)2003-2005 yuno
#include "midisynth.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace midisynth
{
// �`�����l���R���X�g���N�^�B
channel::channel(note_factory *factory_, int bank) : factory(factory_), default_bank(bank)
{
    notes.reserve(16);
    reset_all_parameters();
}
// �`�����l���f�X�g���N�^�B
channel::~channel()
{
    all_sound_off_immediately();
}
// �������̉�����������B
int channel::synthesize(int_least32_t *out, std::size_t samples, float rate, int_least32_t master_volume,
                        int master_balance)
{
    double volume =
        mute ? 0.0
             : std::pow(static_cast<double>(master_volume) * this->volume * expression / (16383.0 * 16383.0 * 16383.0),
                        2) *
                   16383.0;
    int                         num_notes = 0;
    std::vector<NOTE>::iterator i         = notes.begin();
    while (i != notes.end())
    {
        class note   *note   = i->note;
        int_least32_t panpot = note->get_panpot();
        if (this->panpot <= 8192)
        {
            panpot = panpot * this->panpot / 8192;
        }
        else
        {
            panpot = panpot * (16384 - this->panpot) / 8192 + (this->panpot - 8192) * 2;
        }
        if (master_balance <= 8192)
        {
            panpot = panpot * master_balance / 8192;
        }
        else
        {
            panpot = panpot * (16384 - master_balance) / 8192 + (master_balance - 8192) * 2;
        }
        int_least32_t left =
            static_cast<int_least32_t>(volume * std::cos(std::max(0, panpot - 1) * (M_PI / 2 / 16382)));
        int_least32_t right =
            static_cast<int_least32_t>(volume * std::sin(std::max(0, panpot - 1) * (M_PI / 2 / 16382)));
        bool ret = note->synthesize(out, samples, rate, left, right);
        if (ret)
        {
            ++i;
        }
        else
        {
            i = notes.erase(i);
            delete note;
        }
        ++num_notes;
    }
    return num_notes;
}
// ���ׂẴp�����[�^��������Ԃɖ߂��B
void channel::reset_all_parameters()
{
    program                     = default_bank * 128;
    bank                        = default_bank;
    panpot                      = 8192;
    volume                      = 12800;
    fine_tuning                 = 8192;
    coarse_tuning               = 8192;
    tremolo_frequency           = 3;
    vibrato_frequency           = 3;
    master_frequency_multiplier = 1;
    mono                        = false;
    mute                        = false;
    system_mode                 = system_mode_default;
    reset_all_controller();
}
// �p�����[�^��������Ԃɖ߂��B
void channel::reset_all_controller()
{
    expression = 16383;
    channel_pressure(0);
    pitch_bend             = 8192;
    pitch_bend_sensitivity = 256;
    update_frequency_multiplier();
    modulation_depth       = 0;
    modulation_depth_range = 64;
    update_modulation();
    set_damper(0);
    set_sostenute(0);
    set_freeze(0);
    RPN  = 0x3FFF;
    NRPN = 0x3FFF;
}
// ���ׂẲ����m�[�g�I�t����B
void channel::all_note_off()
{
    for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
    {
        if (i->status == NOTE::NOTEON)
        {
            i->status = NOTE::NOTEOFF;
            i->note->note_off(64);
        }
    }
}
// ���ׂẲ����T�E���h�I�t����B
void channel::all_sound_off()
{
    for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
    {
        if (i->status != NOTE::SOUNDOFF)
        {
            i->status = NOTE::SOUNDOFF;
            i->note->sound_off();
        }
    }
}
// ���������B
void channel::all_sound_off_immediately()
{
    for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
    {
        delete i->note;
    }
    notes.clear();
}
// �m�[�g�I���B�����o���B
void channel::note_on(int note, int velocity)
{
    assert(note >= 0 && note < NUM_NOTES);
    assert(velocity >= 0 && velocity <= 127);

    note_off(note, 64);
    if (velocity)
    {
        if (mono)
        {
            all_sound_off();
        }
        class note *p = factory->note_on(program, note, velocity, frequency_multiplier);
        if (p)
        {
            int assign = p->get_assign();
            if (assign)
            {
                for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
                {
                    if (i->note->get_assign() == assign)
                    {
                        i->note->sound_off();
                    }
                }
            }
            if (freeze)
            {
                p->set_freeze(freeze);
            }
            if (damper)
            {
                p->set_damper(damper);
            }
            if (modulation_depth)
            {
                float depth = static_cast<double>(modulation_depth) * modulation_depth_range / (16383.0 * 128.0);
                p->set_vibrato(depth, vibrato_frequency);
            }
            if (pressure)
            {
                p->set_tremolo(pressure, tremolo_frequency);
            }
            notes.push_back(NOTE(p, note));
        }
    }
}
// �m�[�g�I�t�B���������[�X�^�C���ɓ���B
void channel::note_off(int note, int velocity)
{
    assert(note >= 0 && note < NUM_NOTES);
    assert(velocity >= 0 && velocity <= 127);
    for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
    {
        if (i->key == note && i->status == NOTE::NOTEON)
        {
            i->status = NOTE::NOTEOFF;
            i->note->note_off(velocity);
        }
    }
}
// �|���t�H�j�b�N�L�[�v���b�V���B
void channel::polyphonic_key_pressure(int note, int value)
{
    assert(note >= 0 && note < NUM_NOTES);
    assert(value >= 0 && value <= 127);
    for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
    {
        if (i->key == note && i->status == NOTE::NOTEON)
        {
            i->note->set_tremolo(value, tremolo_frequency);
        }
    }
}
// �`�����l���v���b�V���B
void channel::channel_pressure(int value)
{
    assert(value >= 0 && value <= 127);
    if (pressure != value)
    {
        pressure = value;
        for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
        {
            if (i->status == NOTE::NOTEON)
            {
                i->note->set_tremolo(value, tremolo_frequency);
            }
        }
    }
}
// �R���g���[���`�F���W�B
void channel::control_change(int control, int value)
{
    assert(value >= 0 && value <= 0x7F);
    switch (control)
    {
    case 0x00:
        bank_select((bank & 0x7F) | (value << 7));
        break;
    case 0x01:
        set_modulation_depth((modulation_depth & 0x7F) | (value << 7));
        break;
    case 0x06:
        set_registered_parameter((get_registered_parameter() & 0x7F) | (value << 7));
        break;
    case 0x07:
        volume = (volume & 0x7F) | (value << 7);
        break;
    case 0x0A:
        panpot = (panpot & 0x7F) | (value << 7);
        break;
    case 0x0B:
        expression = (expression & 0x7F) | (value << 7);
        break;
    case 0x20:
        bank_select((bank & 0x7F) | (value << 7));
        break;
    case 0x21:
        set_modulation_depth((modulation_depth & ~0x7F) | value);
        break;
    case 0x26:
        set_registered_parameter((get_registered_parameter() & ~0x7F) | value);
        break;
    case 0x27:
        volume = (volume & ~0x7F) | value;
        break;
    case 0x2A:
        panpot = (panpot & ~0x7F) | value;
        break;
    case 0x2B:
        expression = (expression & ~0x7F) | value;
        break;
    case 0x40:
        set_damper(value);
        break;
    case 0x42:
        set_sostenute(value);
        break;
    case 0x45:
        set_freeze(value);
        break;
    case 0x60:
        set_registered_parameter(std::min(0x3FFF, get_registered_parameter() + 1));
        break;
    case 0x61:
        set_registered_parameter(std::max(0, get_registered_parameter() - 1));
        break;
    case 0x62:
        set_NRPN((NRPN & ~0x7F) | value);
        break;
    case 0x63:
        set_NRPN((NRPN & 0x7F) | (value << 7));
        break;
    case 0x64:
        set_RPN((RPN & ~0x7F) | value);
        break;
    case 0x65:
        set_RPN((RPN & 0x7F) | (value << 7));
        break;
    case 0x78:
        all_sound_off();
        break;
    case 0x79:
        reset_all_controller();
        break;
    case 0x7B:
    case 0x7C:
    case 0x7D:
        all_note_off();
        break;
    case 0x7E:
        mono_mode_on();
        break;
    case 0x7F:
        poly_mode_on();
        break;
    }
}
// �o���N�Z���N�g
void channel::bank_select(int value)
{
    switch (system_mode)
    {
    case system_mode_gm:
        break;
    case system_mode_gs:
        if (((bank & 0x3F80) == 0x3C00) == ((value & 0x3F80) == 0x3C00))
        {
            set_bank(value);
        }
        break;
    case system_mode_xg:
        if (default_bank == 0x3C00)
        {
            set_bank(0x3C00 | (value & 0x7F));
        }
        else if ((value & 0x3F80) == 0x3F80)
        {
            set_bank(0x3C00 | (value & 0x7F));
        }
        else
        {
            set_bank(value);
        }
        break;
    default:
        if (default_bank == 0x3C00)
        {
            set_bank(0x3C00 | (value & 0x7F));
        }
        else
        {
            set_bank(value);
        }
        break;
    }
}
// �_���p�[���ʁB
void channel::set_damper(int value)
{
    if (damper != value)
    {
        damper = value;
        for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
        {
            i->note->set_damper(value);
        }
    }
}
// �\�X�e�k�[�g���ʁB
void channel::set_sostenute(int value)
{
    sostenute = value;
    for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
    {
        i->note->set_sostenute(value);
    }
}
// �t���[�Y���ʁB
void channel::set_freeze(int value)
{
    if (freeze != value)
    {
        freeze = value;
        for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
        {
            i->note->set_freeze(value);
        }
    }
}
// RPN�擾�B
int channel::get_registered_parameter()
{
    switch (RPN)
    {
    case 0x0000:
        return pitch_bend_sensitivity;
    case 0x0001:
        return fine_tuning;
    case 0x0002:
        return coarse_tuning;
    case 0x0005:
        return modulation_depth_range;
    default:
        return 0;
    }
}
// RPN�ݒ�B
void channel::set_registered_parameter(int value)
{
    switch (RPN)
    {
    case 0x0000:
        set_pitch_bend_sensitivity(value);
        break;
    case 0x0001:
        set_fine_tuning(value);
        break;
    case 0x0002:
        set_coarse_tuning(value);
        break;
    case 0x0005:
        set_modulation_depth_range(value);
        break;
    default:
        break;
    }
}
// ���g���{�����Čv�Z���X�V����B
void channel::update_frequency_multiplier()
{
    float value =
        master_frequency_multiplier *
        std::pow(2, (coarse_tuning - 8192) / (128.0 * 100.0 * 12.0) + (fine_tuning - 8192) / (8192.0 * 100.0 * 12.0) +
                        static_cast<double>(pitch_bend - 8192) * pitch_bend_sensitivity / (8192.0 * 128.0 * 12.0));
    if (frequency_multiplier != value)
    {
        frequency_multiplier = value;
        for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
        {
            i->note->set_frequency_multiplier(value);
        }
    }
}
// ���W�����[�V�����f�v�X���ʂ̍X�V�B
void channel::update_modulation()
{
    float depth = static_cast<double>(modulation_depth) * modulation_depth_range / (16383.0 * 128.0);
    for (std::vector<NOTE>::iterator i = notes.begin(); i != notes.end(); ++i)
    {
        i->note->set_vibrato(depth, vibrato_frequency);
    }
}

// �V���Z�T�C�U�R���X�g���N�^�B
synthesizer::synthesizer(note_factory *factory)
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        channels[i] = new channel(factory, i == 9 ? 0x3C00 : 0x3C80);
    }
    reset_all_parameters();
}

synthesizer::~synthesizer()
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
        delete channels[i];
}
// �`�����l���擾�B
channel *synthesizer::get_channel(int ch)
{
    assert(ch >= 0 && ch < NUM_CHANNELS);
    return channels[ch];
}
// ������������B��������Ԃ��B
int synthesizer::synthesize(int_least16_t *output, std::size_t samples, float rate)
{
    if (samples == 0)
        return 0;
    std::size_t                n = samples * 2;
    std::vector<int_least32_t> buf(n);
    int                        num_notes = synthesize_mixing(&buf[0], samples, rate);
    if (num_notes)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            int_least32_t x = buf[i];
            if (x < -32767)
            {
                output[i] = -32767;
            }
            else if (x > 32767)
            {
                output[i] = 32767;
            }
            else
            {
                output[i] = static_cast<int_least16_t>(x);
            }
        }
    }
    else
    {
        std::memset(output, 0, sizeof(int_least16_t) * n);
    }
    return num_notes;
}
int synthesizer::synthesize_mixing(int_least32_t *output, std::size_t samples, float rate)
{
    if (active_sensing == 0)
    {
        all_sound_off();
        active_sensing = -1;
    }
    else if (active_sensing > 0)
    {
        active_sensing = std::max(0.0f, active_sensing - samples / rate);
    }
    int_least32_t volume    = static_cast<int_least32_t>(main_volume) * master_volume / 16384;
    int           num_notes = 0;
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        num_notes += channels[i]->synthesize(output, samples, rate, volume, master_balance);
    }
    return num_notes;
}
// �V���Z�T�C�U�����S�Ƀ��Z�b�g����B
void synthesizer::reset()
{
    all_sound_off_immediately();
    reset_all_parameters();
}
// ���ׂẴp�����[�^��������Ԃɖ߂��B
void synthesizer::reset_all_parameters()
{
    active_sensing              = -1;
    main_volume                 = 6144;
    master_volume               = 16383;
    master_balance              = 8192;
    master_fine_tuning          = 8192;
    master_coarse_tuning        = 8192;
    master_frequency_multiplier = 1;
    system_mode                 = system_mode_default;
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        channels[i]->reset_all_parameters();
    }
}
// �p�����[�^��������Ԃɖ߂��B
void synthesizer::reset_all_controller()
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        channels[i]->reset_all_controller();
    }
}
// �I�[���m�[�g�I�t�B���ׂẲ����m�[�g�I�t����B
void synthesizer::all_note_off()
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        channels[i]->all_note_off();
    }
}
// �I�[���T�E���h�I�t�B���ׂẲ����T�E���h�I�t����B
void synthesizer::all_sound_off()
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        channels[i]->all_sound_off();
    }
}
// ���������B
void synthesizer::all_sound_off_immediately()
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        channels[i]->all_sound_off_immediately();
    }
}
// �V�X�e���G�N�X�N���[�V�u���b�Z�[�W�̉��ߎ��s�B
void synthesizer::sysex_message(const void *pvdata, std::size_t size)
{
    const unsigned char *data = reinterpret_cast<const unsigned char *>(pvdata);
    if (size == 6 && std::memcmp(data, "\xF0\x7E\x7F\x09\x01\xF7", 6) == 0)
    {
        /* GM system on */
        set_system_mode(system_mode_gm);
    }
    else if (size == 6 && std::memcmp(data, "\xF0\x7E\x7F\x09\x02\xF7", 6) == 0)
    {
        /* GM system off */
        set_system_mode(system_mode_gm2);
    }
    else if (size == 6 && std::memcmp(data, "\xF0\x7E\x7F\x09\x03\xF7", 6) == 0)
    {
        /* GM2 system on */
        set_system_mode(system_mode_gm2);
    }
    else if (size == 11 && std::memcmp(data, "\xF0\x41", 2) == 0 &&
             std::memcmp(data + 3, "\x42\x12\x40\x00\x7F\x00\x41\xF7", 8) == 0)
    {
        /* GS reset */
        set_system_mode(system_mode_gs);
    }
    else if (size == 9 && std::memcmp(data, "\xF0\x43", 2) == 0 && (data[2] & 0xF0) == 0x10 &&
             std::memcmp(data + 3, "\x4C\x00\x00\x7E\x00\xF7", 6) == 0)
    {
        /* XG system on */
        set_system_mode(system_mode_xg);
    }
    else if (size == 8 && std::memcmp(data, "\xF0\x7F\x7F\x04\x01", 5) == 0 && data[7] == 0xF7)
    {
        /* master volume */
        set_master_volume((data[5] & 0x7F) | ((data[6] & 0x7F) << 7));
    }
    else if (size == 8 && std::memcmp(data, "\xF0\x7F\x7F\x04\x02", 5) == 0 && data[7] == 0xF7)
    {
        /* master balance */
        set_master_balance((data[5] & 0x7F) | ((data[6] & 0x7F) << 7));
    }
    else if (size == 8 && std::memcmp(data, "\xF0\x7F\x7F\x04\x03", 5) == 0 && data[7] == 0xF7)
    {
        /* master fine tuning */
        set_master_fine_tuning((data[5] & 0x7F) | ((data[6] & 0x7F) << 7));
    }
    else if (size == 8 && std::memcmp(data, "\xF0\x7F\x7F\x04\x04", 5) == 0 && data[7] == 0xF7)
    {
        /* master coarse tuning */
        set_master_coarse_tuning((data[5] & 0x7F) | ((data[6] & 0x7F) << 7));
    }
    else if (size == 11 && std::memcmp(data, "\xF0\x41", 2) == 0 && (data[2] & 0xF0) == 0x10 &&
             std::memcmp(data + 3, "\x42\x12\x40", 3) == 0 && (data[6] & 0xF0) == 0x10 && data[7] == 0x15 &&
             data[10] == 0xF7)
    {
        /* use for rhythm part */
        int channel = data[6] & 0x0F;
        int map     = data[8];
        if (map == 0)
        {
            channels[channel]->set_bank(0x3C80);
        }
        else
        {
            channels[channel]->set_bank(0x3C00);
        }
        channels[channel]->program_change(0);
    }
}
// MIDI�C�x���g�̉��ߎ��s�B
void synthesizer::midi_event(int event, int param1, int param2)
{
    if (event == 0xFE)
        active_sensing = 0.33f;
    else if (event == 0xFF)
    {
        all_sound_off();
        reset_all_parameters();
    }
    else
    {
        switch (event & 0xF0)
        {
        case 0x80:
            note_off(event & 0x0F, param1 & 0x7F, param2 & 0x7F);
            break;
        case 0x90:
            note_on(event & 0x0F, param1 & 0x7F, param2 & 0x7F);
            break;
        case 0xA0:
            polyphonic_key_pressure(event & 0x0F, param1 & 0x7F, param2 & 0x7F);
            break;
        case 0xB0:
            control_change(event & 0x0F, param1 & 0x7F, param2 & 0x7F);
            break;
        case 0xC0:
            program_change(event & 0x0F, param1 & 0x7F);
            break;
        case 0xD0:
            channel_pressure(event & 0x0F, param1 & 0x7F);
            break;
        case 0xE0:
            pitch_bend_change(event & 0x0F, ((param2 & 0x7F) << 7) | (param1 & 0x7F));
            break;
        }
    }
}
// �V�X�e�����[�h��ύX����B
void synthesizer::set_system_mode(system_mode_t mode)
{
    all_sound_off();
    reset_all_parameters();
    system_mode = mode;
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        channels[i]->set_system_mode(mode);
    }
}
// �}�X�^�[�`���[�j���O���Čv�Z���X�V����B
void synthesizer::update_master_frequency_multiplier()
{
    float value = std::pow(2, (master_coarse_tuning - 8192) / (128.0 * 100.0 * 12.0) +
                                  (master_fine_tuning - 8192) / (8192.0 * 100.0 * 12.0));
    if (master_frequency_multiplier != value)
    {
        master_frequency_multiplier = value;
        for (int i = 0; i < NUM_CHANNELS; ++i)
        {
            channels[i]->set_master_frequency_multiplier(value);
        }
    }
}

// �����e�[�u���B�����g�W�F�l���[�^�p�B
namespace
{
class sine_table
{
  public:
    enum
    {
        DIVISION = 4096
    };
    sine_table();
    int_least16_t get(int n) const
    {
        return data[n];
    }

  private:
    int_least16_t data[DIVISION];
} sine_table;

sine_table::sine_table()
{
    for (int i = 0; i < DIVISION; ++i)
    {
        data[i] = static_cast<int_least16_t>(32767 * std::sin(i * 2 * M_PI / DIVISION));
    }
}
} // namespace
// �����g������R���X�g���N�^�B
inline sine_wave_generator::sine_wave_generator() : position(0), step(0)
{
}
inline sine_wave_generator::sine_wave_generator(float cycle) : position(0)
{
    set_cycle(cycle);
}
// �����g�̎�����ύX����B
void sine_wave_generator::set_cycle(float cycle)
{
    if (cycle)
    {
        step = static_cast<uint_least32_t>(sine_table::DIVISION * 32768.0 / cycle);
    }
    else
    {
        step = 0;
    }
}
// ���W�����[�V������������B
void sine_wave_generator::add_modulation(int_least32_t x)
{
    position += static_cast<int_least32_t>(static_cast<int_least64_t>(step) * x >> 16);
}
// ���̃T���v�������o���B
inline int sine_wave_generator::get_next()
{
    return sine_table.get((position += step) / 32768 % sine_table::DIVISION);
}
// ���̃T���v�������o��(���g���ϒ��t��)�B
inline int sine_wave_generator::get_next(int_least32_t modulation)
{
    uint_least32_t m = modulation * sine_table::DIVISION / 65536;
    uint_least32_t p = ((position += step) / 32768 + m) % sine_table::DIVISION;
    return sine_table.get(p);
}

// �ΐ��ϊ��e�[�u���B�G���x���[�v�W�F�l���[�^�̃f�B�P�C�ȍ~�Ŏg���B
namespace
{
#define LOG10_32767     4.5154366811416989472479934140484
#define LOGTABLE_SIZE   4096
#define LOGTABLE_FACTOR (LOGTABLE_SIZE / LOG10_32767)
class log_table
{
  public:
    log_table();
    uint_least16_t get(int x) const
    {
        return data[x];
    }

  private:
    uint_least16_t data[LOGTABLE_SIZE];
} log_table;
log_table::log_table()
{
    for (int i = 0; i < LOGTABLE_SIZE; ++i)
    {
        data[i] = static_cast<uint_least16_t>(std::pow(10, static_cast<double>(i) / LOGTABLE_FACTOR));
    }
}
} // namespace

// ���[�g�e�[�u���BAR�ADR�ASR�ARR�̌v�Z�����̍������B
namespace
{
struct envelope_table
{
    envelope_table();
    uint_least32_t TL[128];
    uint_least32_t SL[16][128];
    double         AR[64][128];
    double         RR[64][128];
} const envelope_table;

envelope_table::envelope_table()
{
    for (int t = 0; t < 128; ++t)
    {
        double fTL = 32767 * std::pow(10, t * -0.75 / 10);
        TL[t]      = static_cast<uint_least32_t>(fTL);
        if (TL[t] == 0)
        {
            TL[t] = 1;
        }
        for (int s = 0; s < 16; ++s)
        {
            double x = fTL * std::pow(10, s * -3.0 / 10);
            if (x <= 1)
            {
                SL[s][t] = 0;
            }
            else
            {
                SL[s][t] = static_cast<uint_least32_t>(65536 * LOGTABLE_FACTOR * std::log10(x));
            }
        }
    }
    for (int x = 0; x < 64; ++x)
    {
        double attack_time  = 15.3262 * std::pow(10, x * -0.75 / 10);
        double release_time = 211.84 * std::pow(10, x * -0.75 / 10);
        for (int t = 0; t < 128; ++t)
        {
            AR[x][t] = TL[t] / attack_time;
            RR[x][t] = 65536 * LOGTABLE_FACTOR * 48.0 / 10 * TL[t] / 32767 / release_time;
        }
    }
}
} // namespace

// �G���x���[�v������R���X�g���N�^�B
envelope_generator::envelope_generator(int AR_, int DR_, int SR_, int RR_, int SL, int TL_)
    : state(ATTACK), AR(AR_), DR(DR_), SR(SR_), RR(RR_), TL(TL_), current(0), rate(1), hold(0), freeze(0)
{
    if (AR >= 63)
        AR = 63;
    if (DR >= 63)
        DR = 63;
    if (SR >= 63)
        SR = 63;
    if (RR >= 63)
        RR = 63;
    assert(AR >= 0);
    assert(DR >= 0);
    assert(SR >= 0);
    assert(RR >= 0);
    assert(SL >= 0 && SL <= 15);
    assert(TL >= 0 && TL <= 127);

    fTL = envelope_table.TL[TL];
    fSS = fSL = envelope_table.SL[SL][TL];
    fAR       = 0;
    fDR       = 0;
    fSR       = 0;
    fRR       = 0;
    fOR       = 0;
    fDRR      = 0;
    fDSS      = 0;
}
// �Đ����[�g��ݒ肷��B
inline void envelope_generator::set_rate(float rate)
{
    this->rate = rate ? rate : 1;
    update_parameters();
}
// �z�[���h(�_���p�[&�\�X�e�k�[�g)��ݒ肷��B
void envelope_generator::set_hold(float hold)
{
    if (this->hold > hold || state <= SASTAIN || current >= fSL)
    {
        this->hold = hold;
        update_parameters();
    }
}
// �t���[�Y��ݒ肷��B
void envelope_generator::set_freeze(float freeze)
{
    if (this->freeze > freeze || state <= SASTAIN || current >= fSL)
    {
        this->freeze = freeze;
        update_parameters();
    }
}
// �e�p�����[�^�̍X�V�B
void envelope_generator::update_parameters()
{
    double fAR = envelope_table.AR[AR][TL] / rate;
    double fDR = envelope_table.RR[DR][TL] / rate;
    double fSR = envelope_table.RR[SR][TL] / rate;
    double fRR = envelope_table.RR[RR][TL] / rate;

    if (fRR < 1)
    {
        fRR = 1;
    }
    if (hold > 0)
    {
        fRR = fSR * hold + fRR * (1 - hold);
    }
    if (freeze > 0)
    {
        fDR *= (1 - freeze);
        fSR *= (1 - freeze);
        fRR *= (1 - freeze);
    }
    if (fAR < 1)
    {
        fAR = 1;
    }
    this->fAR  = static_cast<uint_least32_t>(fAR);
    this->fDR  = static_cast<uint_least32_t>(fDR);
    this->fSR  = static_cast<uint_least32_t>(fSR);
    this->fRR  = static_cast<uint_least32_t>(fRR);
    this->fOR  = static_cast<uint_least32_t>(envelope_table.RR[63][0] / rate);
    this->fSS  = std::max(this->fDR, fSL);
    this->fDRR = std::max(this->fDR, this->fRR);
    this->fDSS = std::max(this->fDRR, this->fSS);
}
// �L�[�I�t�B�����[�X�ɓ���B
void envelope_generator::key_off()
{
    switch (state)
    {
    case ATTACK:
        state = ATTACK_RELEASE;
        break;
    case DECAY:
        state = DECAY_RELEASE;
        break;
    case SASTAIN:
        state = RELEASE;
        break;
    default:
        break;
    }
}
// �T�E���h�I�t�B�}���������[�h�ɓ���B
void envelope_generator::sound_off()
{
    switch (state)
    {
    case ATTACK:
    case ATTACK_RELEASE:
        if (current)
        {
            current = static_cast<uint_least32_t>(65536 * LOGTABLE_FACTOR * std::log10(static_cast<double>(current)));
        }
        break;
    default:
        break;
    }
    state = SOUNDOFF;
}
// �����[�X����T�E���h�I�t�Ɉڂ郌�x���B�����[�X�̒����������܂ł�����
// ���Ă��CPU�p���[�̖��ʂȂ̂œK���ȂƂ���Ő؂邽�߁B�����͑ΐ��Ȃ̂�
// ���ʂ��^�Ƀ[���ɂȂ�ɂ͖����̎��Ԃ�v����B���ۂɂ͐����ɐ؂�̂Ă�
// �������Ă���̂łP�����ɂȂ����疳���ɂȂ�Ƃ͂����c�B
// �s���R�łȂ����x�ɂȂ�ׂ������l�̕����p�t�H�[�}���X�����シ��B
#define SOUNDOFF_LEVEL 1024
// ���̃T���v���𓾂�B
int envelope_generator::get_next()
{
    uint_least32_t current = this->current;
    switch (state)
    {
    case ATTACK:
        if (current < fTL)
        {
            return this->current = current + fAR;
        }
        this->current = static_cast<uint_least32_t>(65536 * LOGTABLE_FACTOR * std::log10(static_cast<double>(fTL)));
        state         = DECAY;
        return fTL;
    case DECAY:
        if (current > fSS)
        {
            this->current = current -= fDR;
            return log_table.get(current / 65536);
        }
        this->current = current = fSL;
        state                   = SASTAIN;
        return log_table.get(current / 65536);
    case SASTAIN:
        if (current > fSR)
        {
            this->current = current -= fSR;
            int n         = log_table.get(current / 65536);
            if (n > 1)
            {
                return n;
            }
        }
        state = FINISHED;
        return 0;
    case ATTACK_RELEASE:
        if (current < fTL)
        {
            return this->current = current + fAR;
        }
        this->current = static_cast<uint_least32_t>(65536 * LOGTABLE_FACTOR * std::log10(static_cast<double>(fTL)));
        state         = DECAY_RELEASE;
        return fTL;
    case DECAY_RELEASE:
        if (current > fDSS)
        {
            this->current = current -= fDRR;
            return log_table.get(current / 65536);
        }
        this->current = current = fSL;
        state                   = RELEASE;
        return log_table.get(current / 65536);
    case RELEASE:
        if (current > fRR)
        {
            this->current = current -= fRR;
            int n         = log_table.get(current / 65536);
            if (n > SOUNDOFF_LEVEL)
            {
                return n;
            }
            state = SOUNDOFF;
            return n;
        }
        state = FINISHED;
        return 0;
    case SOUNDOFF:
        if (current > fOR)
        {
            this->current = current -= fOR;
            int n         = log_table.get(current / 65536);
            if (n > 1)
            {
                return n;
            }
        }
        state = FINISHED;
        return 0;
    default:
        return 0;
    }
}

namespace
{
// �L�[�X�P�[�����O�e�[�u��
const int keyscale_table[4][128] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7},
    {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  7,  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  10, 10, 10, 10, 10, 10,
     10, 10, 10, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14,
     14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
    {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  2,  2,  3,  4,  4,
     4,  4,  4,  4,  4,  5,  5,  6,  6,  7,  8,  8,  8,  8,  8,  8,  8,  9,  9,  10, 10, 11, 12, 12, 12, 12,
     12, 12, 12, 13, 13, 14, 14, 15, 16, 16, 16, 16, 16, 16, 16, 17, 17, 18, 18, 19, 20, 20, 20, 20, 20, 20,
     20, 21, 21, 22, 22, 23, 24, 24, 24, 24, 24, 24, 24, 25, 25, 26, 26, 27, 28, 28, 28, 28, 28, 28, 28, 29,
     29, 30, 30, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31}};
// �f�B�`���[���e�[�u��
const float detune_table[4][128] = {
    {0},
    {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000,
     0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053,
     0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053,
     0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106,
     0.106, 0.106, 0.106, 0.159, 0.159, 0.159, 0.159, 0.159, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212,
     0.212, 0.212, 0.212, 0.264, 0.264, 0.264, 0.264, 0.264, 0.264, 0.264, 0.264, 0.317, 0.317, 0.317, 0.317, 0.370,
     0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423,
     0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.423},
    {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.053, 0.053,
     0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.053, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106,
     0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106,
     0.159, 0.159, 0.159, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.264,
     0.264, 0.264, 0.264, 0.264, 0.264, 0.264, 0.264, 0.317, 0.317, 0.317, 0.317, 0.370, 0.423, 0.423, 0.423,
     0.423, 0.423, 0.423, 0.423, 0.423, 0.423, 0.476, 0.476, 0.529, 0.582, 0.582, 0.582, 0.582, 0.582, 0.582,
     0.582, 0.635, 0.635, 0.688, 0.688, 0.741, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846,
     0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846},
    {0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.106, 0.106, 0.106,
     0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.106, 0.159,
     0.159, 0.159, 0.159, 0.159, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.212, 0.264,
     0.264, 0.264, 0.264, 0.264, 0.264, 0.264, 0.264, 0.317, 0.317, 0.317, 0.317, 0.370, 0.423, 0.423, 0.423, 0.423,
     0.423, 0.423, 0.423, 0.423, 0.423, 0.476, 0.476, 0.529, 0.582, 0.582, 0.582, 0.582, 0.582, 0.582, 0.582, 0.635,
     0.635, 0.688, 0.688, 0.741, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.846, 0.899, 0.899, 1.005, 1.005, 1.058,
     1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164,
     1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164, 1.164}};
// LFO�e�[�u��
const uint_least32_t ams_table[4] = {0, static_cast<uint_least32_t>(128 - 128 * std::pow(10, -1.44 / 10)),
                                     static_cast<uint_least32_t>(128 - 128 * std::pow(10, -5.9 / 10)),
                                     static_cast<uint_least32_t>(128 - 128 * std::pow(10, -11.8 / 10))};
} // namespace

// FM�I�y���[�^�̃R���X�g���N�^�B
fm_operator::fm_operator(int AR, int DR, int SR, int RR, int SL, int TL, int KS, int ML_, int DT_, int AMS_, int key)
    : eg(AR * 2 + keyscale_table[KS][key], DR * 2 + keyscale_table[KS][key], SR * 2 + keyscale_table[KS][key],
         RR * 4 + keyscale_table[KS][key] + 2, SL, TL)
{
    assert(AR >= 0 && AR <= 31);
    assert(DR >= 0 && DR <= 31);
    assert(SR >= 0 && SR <= 31);
    assert(RR >= 0 && RR <= 15);
    assert(SL >= 0);
    assert(TL >= 0);
    assert(KS >= 0 && KS <= 3);
    assert(ML_ >= 0 && ML_ <= 15);
    assert(DT_ >= 0 && DT_ <= 7);
    assert(AMS_ >= 0 && AMS_ <= 3);
    assert(key >= 0 && key <= 127);

    if (DT_ >= 4)
    {
        DT = -detune_table[DT_ - 4][key];
    }
    else
    {
        DT = detune_table[DT_][key];
    }
    if (ML_ == 0)
    {
        ML = 0.5;
    }
    else
    {
        ML = ML_;
    }

    ams_factor = ams_table[AMS_] / 2;
    ams_bias   = 32768 - ams_factor * 256;
}
// �Đ����g���ݒ�B
void fm_operator::set_freq_rate(float freq, float rate)
{
    freq += DT;
    freq *= ML;
    swg.set_cycle(rate / freq);
    eg.set_rate(rate);
}
// ���̃T���v���𓾂�B
inline int fm_operator::get_next()
{
    return static_cast<int_least32_t>(swg.get_next()) * eg.get_next() >> 15;
}
inline int fm_operator::get_next(int modulate)
{
    return static_cast<int_least32_t>(swg.get_next(modulate)) * eg.get_next() >> 15;
}
inline int fm_operator::get_next(int ams, int modulate)
{
    return (static_cast<int_least32_t>(swg.get_next(modulate)) * eg.get_next() >> 15) * (ams * ams_factor + ams_bias) >>
           15;
}

// �r�u���[�g�e�[�u���B
namespace
{
class vibrato_table
{
  public:
    enum
    {
        DIVISION = 16384
    };
    vibrato_table();
    int_least32_t get(int x) const
    {
        return data[x + DIVISION / 2];
    }

  private:
    int_least32_t data[DIVISION];
} vibrato_table;

vibrato_table::vibrato_table()
{
    for (int i = 0; i < DIVISION; ++i)
    {
        double x = (static_cast<double>(i) / DIVISION - 0.5) * 256.0 / 12.0;
        data[i]  = static_cast<int_least32_t>((std::pow(2, x) - 1) * 65536.0);
    }
}
} // namespace

// FM�����R���X�g���N�^�B
fm_sound_generator::fm_sound_generator(const FMPARAMETER &params, int note, float frequency_multiplier)
    : op1(params.op1.AR, params.op1.DR, params.op1.SR, params.op1.RR, params.op1.SL, params.op1.TL, params.op1.KS,
          params.op1.ML, params.op1.DT, params.op1.AMS, note),
      op2(params.op2.AR, params.op2.DR, params.op2.SR, params.op2.RR, params.op2.SL, params.op2.TL, params.op2.KS,
          params.op2.ML, params.op2.DT, params.op2.AMS, note),
      op3(params.op3.AR, params.op3.DR, params.op3.SR, params.op3.RR, params.op3.SL, params.op3.TL, params.op3.KS,
          params.op3.ML, params.op3.DT, params.op3.AMS, note),
      op4(params.op4.AR, params.op4.DR, params.op4.SR, params.op4.RR, params.op4.SL, params.op4.TL, params.op4.KS,
          params.op4.ML, params.op4.DT, params.op4.AMS, note),
      ALG(params.ALG), freq(440 * std::pow(2.0, (note - 69) / 12.0)), freq_mul(frequency_multiplier), tremolo_depth(0),
      tremolo_freq(1), vibrato_depth(0), vibrato_freq(1), rate(0), feedback(0), damper(0), sostenute(0)
{
    assert(ALG >= 0 && ALG <= 7);
    assert(params.LFO >= 0 && params.LFO <= 7);
    assert(params.FB >= 0 && params.FB <= 7);

    static const int feedbacks[8] = {31, 6, 5, 4, 3, 2, 1, 0};
    FB                            = feedbacks[params.FB];

    static const float ams_table[8] = {3.98, 5.56, 6.02, 6.37, 6.88, 9.63, 48.1, 72.2};
    ams_freq                        = ams_table[params.LFO];
    ams_enable                      = (params.op1.AMS + params.op2.AMS + params.op3.AMS + params.op4.AMS != 0);
}
// �Đ����[�g�ݒ�B
void fm_sound_generator::set_rate(float rate)
{
    if (this->rate != rate)
    {
        this->rate = rate;
        ams_lfo.set_cycle(rate / ams_freq);
        vibrato_lfo.set_cycle(rate / vibrato_freq);
        tremolo_lfo.set_cycle(rate / tremolo_freq);
        float f = freq * freq_mul;
        op1.set_freq_rate(f, rate);
        op2.set_freq_rate(f, rate);
        op3.set_freq_rate(f, rate);
        op4.set_freq_rate(f, rate);
    }
}
// ���g���{���ݒ�B
void fm_sound_generator::set_frequency_multiplier(float value)
{
    freq_mul = value;
    float f  = freq * freq_mul;
    op1.set_freq_rate(f, rate);
    op2.set_freq_rate(f, rate);
    op3.set_freq_rate(f, rate);
    op4.set_freq_rate(f, rate);
}
// �_���p�[���ʐݒ�B
void fm_sound_generator::set_damper(int damper)
{
    this->damper = damper;
    float value  = 1.0 - (1.0 - damper / 127.0) * (1.0 - sostenute / 127.0);
    op1.set_hold(value);
    op2.set_hold(value);
    op3.set_hold(value);
    op4.set_hold(value);
}
// �\�X�e�k�[�g���ʐݒ�B
void fm_sound_generator::set_sostenute(int sostenute)
{
    this->sostenute = sostenute;
    float value     = 1.0 - (1.0 - damper / 127.0) * (1.0 - sostenute / 127.0);
    op1.set_hold(value);
    op2.set_hold(value);
    op3.set_hold(value);
    op4.set_hold(value);
}
// �t���[�Y���ʐݒ�B
void fm_sound_generator::set_freeze(int freeze)
{
    float value = freeze / 127.0;
    op1.set_freeze(value);
    op2.set_freeze(value);
    op3.set_freeze(value);
    op4.set_freeze(value);
}
// �g���������ʐݒ�B
void fm_sound_generator::set_tremolo(int depth, float frequency)
{
    tremolo_depth = depth;
    tremolo_freq  = frequency;
    tremolo_lfo.set_cycle(rate / frequency);
}
// �r�u���[�g���ʐݒ�B
void fm_sound_generator::set_vibrato(float depth, float frequency)
{
    vibrato_depth = static_cast<int>(depth * (vibrato_table::DIVISION / 256.0));
    vibrato_freq  = frequency;
    vibrato_lfo.set_cycle(rate / frequency);
}
// �L�[�I�t�B
void fm_sound_generator::key_off()
{
    op1.key_off();
    op2.key_off();
    op3.key_off();
    op4.key_off();
}
// �T�E���h�I�t�B
void fm_sound_generator::sound_off()
{
    op1.sound_off();
    op2.sound_off();
    op3.sound_off();
    op4.sound_off();
}
// ���̔������I���������ǂ�����Ԃ��B
bool fm_sound_generator::is_finished() const
{
    switch (ALG)
    {
    case 0:
    case 1:
    case 2:
    case 3:
        return op4.is_finished();
    case 4:
        return op2.is_finished() && op4.is_finished();
    case 5:
    case 6:
        return op2.is_finished() && op3.is_finished() && op4.is_finished();
    case 7:
        return op1.is_finished() && op2.is_finished() && op3.is_finished() && op4.is_finished();
    default:
        assert(!"fm_sound_generator: invalid algorithm number");
        return true;
    }
}
// ���̃T���v���𓾂�B
int fm_sound_generator::get_next()
{
    if (vibrato_depth)
    {
        int           x          = static_cast<int_least32_t>(vibrato_lfo.get_next()) * vibrato_depth >> 15;
        int_least32_t modulation = vibrato_table.get(x);
        op1.add_modulation(modulation);
        op2.add_modulation(modulation);
        op3.add_modulation(modulation);
        op4.add_modulation(modulation);
    }
    int feedback = (this->feedback << 1) >> FB;
    int ret;
    if (ams_enable)
    {
        int ams = ams_lfo.get_next() >> 7;
        switch (ALG)
        {
        case 0:
            ret = op4(ams, op3(ams, op2(ams, this->feedback = op1(ams, feedback))));
            break;
        case 1:
            ret = op4(ams, op3(ams, op2(ams, 0) + (this->feedback = op1(ams, feedback))));
            break;
        case 2:
            ret = op4(ams, op3(ams, op2(ams, 0)) + (this->feedback = op1(ams, feedback)));
            break;
        case 3:
            ret = op4(ams, op3(ams, 0) + op2(ams, this->feedback = op1(ams, feedback)));
            break;
        case 4:
            ret = op4(ams, op3(ams, 0)) + op2(ams, this->feedback = op1(ams, feedback));
            break;
        case 5:
            this->feedback = feedback = op1(ams, feedback);
            ret                       = op4(ams, feedback) + op3(ams, feedback) + op2(ams, feedback);
            break;
        case 6:
            ret = op4(ams, 0) + op3(ams, 0) + op2(ams, this->feedback = op1(ams, feedback));
            break;
        case 7:
            ret = op4(ams, 0) + op3(ams, 0) + op2(ams, 0) + (this->feedback = op1(ams, feedback));
            break;
        default:
            assert(!"fm_sound_generator: invalid algorithm number");
            return 0;
        }
    }
    else
    {
        switch (ALG)
        {
        case 0:
            ret = op4(op3(op2(this->feedback = op1(feedback))));
            break;
        case 1:
            ret = op4(op3(op2() + (this->feedback = op1(feedback))));
            break;
        case 2:
            ret = op4(op3(op2()) + (this->feedback = op1(feedback)));
            break;
        case 3:
            ret = op4(op3() + op2(this->feedback = op1(feedback)));
            break;
        case 4:
            ret = op4(op3()) + op2(this->feedback = op1(feedback));
            break;
        case 5:
            this->feedback = feedback = op1(feedback);
            ret                       = op4(feedback) + op3(feedback) + op2(feedback);
            break;
        case 6:
            ret = op4() + op3() + op2(this->feedback = op1(feedback));
            break;
        case 7:
            ret = op4() + op3() + op2() + (this->feedback = op1(feedback));
            break;
        default:
            assert(!"fm_sound_generator: invalid algorithm number");
            return 0;
        }
    }
    if (tremolo_depth)
    {
        int_least32_t x = 4096 - (((static_cast<int_least32_t>(tremolo_lfo.get_next()) + 32768) * tremolo_depth) >> 11);
        ret             = ret * x >> 12;
    }
    return ret;
}

// FM�m�[�g�̃R���X�g���N�^�B
fm_note::fm_note(const FMPARAMETER &params, int note, int velocity_, int panpot, int assign, float frequency_multiplier)
    : midisynth::note(assign, panpot), fm(params, note, frequency_multiplier), velocity(velocity_)
{
    assert(velocity >= 1 && velocity <= 127);
    ++velocity;
}
// �g�`�o�́B
bool fm_note::synthesize(int_least32_t *buf, std::size_t samples, float rate, int_least32_t left, int_least32_t right)
{
    left  = (left * velocity) >> 7;
    right = (right * velocity) >> 7;
    fm.set_rate(rate);
    for (std::size_t i = 0; i < samples; ++i)
    {
        int_least32_t sample = fm.get_next();
        buf[i * 2 + 0] += (sample * left) >> 14;
        buf[i * 2 + 1] += (sample * right) >> 14;
    }
    return !fm.is_finished();
}
// �m�[�g�I�t�B
void fm_note::note_off(int)
{
    fm.key_off();
}
// �T�E���h�I�t�B
void fm_note::sound_off()
{
    fm.sound_off();
}
// ���g���{���ݒ�B
void fm_note::set_frequency_multiplier(float value)
{
    fm.set_frequency_multiplier(value);
}
// �g���������ʐݒ�B
void fm_note::set_tremolo(int depth, float freq)
{
    fm.set_tremolo(depth, freq);
}
// �r�u���[�g���ʐݒ�B
void fm_note::set_vibrato(float depth, float freq)
{
    fm.set_vibrato(depth, freq);
}
// �_���p�[���ʐݒ�B
void fm_note::set_damper(int value)
{
    fm.set_damper(value);
}
// �\�X�e�k�[�g���ʐݒ�B
void fm_note::set_sostenute(int value)
{
    fm.set_sostenute(value);
}
// �t���[�Y���ʐݒ�B
void fm_note::set_freeze(int value)
{
    fm.set_freeze(value);
}

// FM�m�[�g�t�@�N�g���������B
fm_note_factory::fm_note_factory()
{
    clear();
    midisynth::DRUMPARAMETER p;
    p.ALG = 4, p.FB = 3, p.LFO = 0, p.op1.AR = 26, p.op1.DR = 10, p.op1.SR = 1, p.op1.RR = 0, p.op1.SL = 0,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 10, p.op2.SR = 2,
    p.op2.RR = 7, p.op2.SL = 2, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 3, p.op2.AMS = 0, p.op3.AR = 26,
    p.op3.DR = 10, p.op3.SR = 2, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 7,
    p.op3.AMS = 0, p.op4.AR = 18, p.op4.DR = 6, p.op4.SR = 1, p.op4.RR = 6, p.op4.SL = 4, p.op4.TL = 2, p.op4.KS = 1,
    p.op4.ML = 1, p.op4.DT = 7, p.op4.AMS = 0;
    set_program(0, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 26, p.op1.DR = 10, p.op1.SR = 1, p.op1.RR = 0, p.op1.SL = 0,
    p.op1.TL = 5, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 10, p.op2.SR = 3,
    p.op2.RR = 7, p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 3, p.op2.AMS = 0, p.op3.AR = 26,
    p.op3.DR = 10, p.op3.SR = 2, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 20, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 7,
    p.op3.AMS = 0, p.op4.AR = 18, p.op4.DR = 6, p.op4.SR = 2, p.op4.RR = 6, p.op4.SL = 4, p.op4.TL = 2, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 7, p.op4.AMS = 0;
    set_program(1, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 26, p.op1.DR = 10, p.op1.SR = 1, p.op1.RR = 0, p.op1.SL = 0,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 10, p.op2.SR = 2,
    p.op2.RR = 7, p.op2.SL = 2, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 3, p.op2.AMS = 0, p.op3.AR = 26,
    p.op3.DR = 10, p.op3.SR = 2, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 7,
    p.op3.AMS = 0, p.op4.AR = 18, p.op4.DR = 6, p.op4.SR = 1, p.op4.RR = 6, p.op4.SL = 4, p.op4.TL = 2, p.op4.KS = 1,
    p.op4.ML = 1, p.op4.DT = 7, p.op4.AMS = 0;
    set_program(2, p);
    p.ALG = 5, p.FB = 7, p.LFO = 0, p.op1.AR = 26, p.op1.DR = 10, p.op1.SR = 1, p.op1.RR = 0, p.op1.SL = 0,
    p.op1.TL = 10, p.op1.KS = 3, p.op1.ML = 6, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 10, p.op2.SR = 4,
    p.op2.RR = 7, p.op2.SL = 1, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 7, p.op2.AMS = 0, p.op3.AR = 15,
    p.op3.DR = 20, p.op3.SR = 5, p.op3.RR = 8, p.op3.SL = 1, p.op3.TL = 2, p.op3.KS = 1, p.op3.ML = 2, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 8, p.op4.DR = 15, p.op4.SR = 3, p.op4.RR = 11, p.op4.SL = 2, p.op4.TL = 3, p.op4.KS = 3,
    p.op4.ML = 3, p.op4.DT = 3, p.op4.AMS = 0;
    set_program(3, p);
    p.ALG = 4, p.FB = 3, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 10, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 16, p.op2.SR = 0,
    p.op2.RR = 12, p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 10, p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 20, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 22, p.op4.DR = 9, p.op4.SR = 0, p.op4.RR = 10, p.op4.SL = 4, p.op4.TL = 2, p.op4.KS = 0,
    p.op4.ML = 2, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(4, p);
    p.ALG = 6, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 12, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 29, p.op2.DR = 12, p.op2.SR = 1,
    p.op2.RR = 7, p.op2.SL = 1, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 26,
    p.op3.DR = 12, p.op3.SR = 2, p.op3.RR = 6, p.op3.SL = 2, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 3,
    p.op3.AMS = 0, p.op4.AR = 26, p.op4.DR = 12, p.op4.SR = 2, p.op4.RR = 6, p.op4.SL = 2, p.op4.TL = 5, p.op4.KS = 0,
    p.op4.ML = 0, p.op4.DT = 7, p.op4.AMS = 0;
    set_program(5, p);
    p.ALG = 2, p.FB = 4, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 16, p.op1.SR = 1, p.op1.RR = 0, p.op1.SL = 0,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 7, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 16, p.op2.SR = 2,
    p.op2.RR = 8, p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 3, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 14, p.op3.SR = 2, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 3,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 12, p.op4.SR = 1, p.op4.RR = 7, p.op4.SL = 2, p.op4.TL = 0, p.op4.KS = 1,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(6, p);
    p.ALG = 2, p.FB = 4, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 16, p.op1.SR = 1, p.op1.RR = 0, p.op1.SL = 0,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 7, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 16, p.op2.SR = 2,
    p.op2.RR = 7, p.op2.SL = 1, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 3, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 14, p.op3.SR = 2, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 4, p.op3.DT = 3,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 14, p.op4.SR = 1, p.op4.RR = 7, p.op4.SL = 2, p.op4.TL = 0, p.op4.KS = 1,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(7, p);
    p.ALG = 7, p.FB = 6, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 18, p.op1.SR = 9, p.op1.RR = 6, p.op1.SL = 1,
    p.op1.TL = 1, p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 14, p.op2.SR = 8,
    p.op2.RR = 5, p.op2.SL = 3, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 3, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 14, p.op3.SR = 9, p.op3.RR = 6, p.op3.SL = 1, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 14, p.op4.SR = 10, p.op4.RR = 6, p.op4.SL = 2, p.op4.TL = 2, p.op4.KS = 0,
    p.op4.ML = 0, p.op4.DT = 7, p.op4.AMS = 0;
    set_program(8, p);
    p.ALG = 7, p.FB = 6, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 26, p.op1.SR = 21, p.op1.RR = 10, p.op1.SL = 2,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 7, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 11,
    p.op2.RR = 5, p.op2.SL = 0, p.op2.TL = 7, p.op2.KS = 0, p.op2.ML = 4, p.op2.DT = 1, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 0, p.op3.SR = 11, p.op3.RR = 5, p.op3.SL = 0, p.op3.TL = 3, p.op3.KS = 0, p.op3.ML = 4, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 28, p.op4.SR = 15, p.op4.RR = 7, p.op4.SL = 3, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 2, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(9, p);
    p.ALG = 6, p.FB = 2, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 2,
    p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 13, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 25, p.op3.RR = 12, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 13, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 3, p.op4.ML = 4,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(10, p);
    p.ALG = 6, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 16, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 2,
    p.op1.TL = 1, p.op1.KS = 0, p.op1.ML = 7, p.op1.DT = 7, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 10,
    p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 3, p.op2.AMS = 0, p.op3.AR = 24,
    p.op3.DR = 10, p.op3.SR = 9, p.op3.RR = 7, p.op3.SL = 1, p.op3.TL = 2, p.op3.KS = 1, p.op3.ML = 1, p.op3.DT = 7,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 24, p.op4.SR = 13, p.op4.RR = 9, p.op4.SL = 4, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(11, p);
    p.ALG = 7, p.FB = 1, p.LFO = 0, p.op1.AR = 27, p.op1.DR = 20, p.op1.SR = 25, p.op1.RR = 12, p.op1.SL = 1,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 13,
    p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 12, p.op2.KS = 0, p.op2.ML = 4, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 16,
    p.op3.DR = 0, p.op3.SR = 15, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 27, p.op4.DR = 0, p.op4.SR = 19, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0,
    p.op4.ML = 4, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(12, p);
    p.ALG = 7, p.FB = 0, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 11, p.op1.RR = 5, p.op1.SL = 0,
    p.op1.TL = 0, p.op1.KS = 2, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 15,
    p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 1, p.op2.ML = 3, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 0, p.op3.SR = 19, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 8, p.op3.KS = 0, p.op3.ML = 6, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127,
    p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(13, p);
    p.ALG = 6, p.FB = 3, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 2,
    p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 16, p.op2.SR = 11, p.op2.RR = 5,
    p.op2.SL = 2, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 3, p.op2.AMS = 0, p.op3.AR = 24, p.op3.DR = 0,
    p.op3.SR = 13, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 7, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 19, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 1, p.op4.KS = 0, p.op4.ML = 3,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(14, p);
    p.ALG = 4, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 7,
    p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 24, p.op2.SR = 9, p.op2.RR = 6,
    p.op2.SL = 1, p.op2.TL = 1, p.op2.KS = 1, p.op2.ML = 1, p.op2.DT = 7, p.op2.AMS = 0, p.op3.AR = 28, p.op3.DR = 0,
    p.op3.SR = 9, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 7, p.op3.DT = 3, p.op3.AMS = 0,
    p.op4.AR = 28, p.op4.DR = 24, p.op4.SR = 9, p.op4.RR = 6, p.op4.SL = 2, p.op4.TL = 3, p.op4.KS = 1, p.op4.ML = 1,
    p.op4.DT = 3, p.op4.AMS = 0;
    set_program(15, p);
    p.ALG = 7, p.FB = 5, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 12, p.op1.SL = 0,
    p.op1.TL = 4, p.op1.KS = 0, p.op1.ML = 0, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 12, p.op2.SL = 0, p.op2.TL = 6, p.op2.KS = 0, p.op2.ML = 4, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 12, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 12, p.op4.SL = 0, p.op4.TL = 3, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(16, p);
    p.ALG = 7, p.FB = 3, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 22, p.op1.SR = 0, p.op1.RR = 12, p.op1.SL = 10,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 20, p.op2.SR = 0,
    p.op2.RR = 12, p.op2.SL = 2, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 18, p.op3.SR = 0, p.op3.RR = 12, p.op3.SL = 1, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 18, p.op4.SR = 0, p.op4.RR = 12, p.op4.SL = 1, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 0, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(17, p);
    p.ALG = 7, p.FB = 5, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 12, p.op1.SL = 0,
    p.op1.TL = 4, p.op1.KS = 0, p.op1.ML = 0, p.op1.DT = 0, p.op1.AMS = 3, p.op2.AR = 28, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 12, p.op2.SL = 0, p.op2.TL = 6, p.op2.KS = 0, p.op2.ML = 4, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 12, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 12, p.op4.SL = 0, p.op4.TL = 3, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 2;
    set_program(18, p);
    p.ALG = 6, p.FB = 5, p.LFO = 0, p.op1.AR = 18, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 0, p.op1.TL = 2,
    p.op1.KS = 0, p.op1.ML = 6, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 10, p.op3.SL = 0, p.op3.TL = 3, p.op3.KS = 0, p.op3.ML = 4, p.op3.DT = 3, p.op3.AMS = 0,
    p.op4.AR = 28, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 12, p.op4.SL = 0, p.op4.TL = 5, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 7, p.op4.AMS = 0;
    set_program(19, p);
    p.ALG = 6, p.FB = 2, p.LFO = 0, p.op1.AR = 14, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 10, p.op1.SL = 0,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 6, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 12, p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 1, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 14,
    p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 17, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 8, p.op4.KS = 1,
    p.op4.ML = 0, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(20, p);
    p.ALG = 3, p.FB = 3, p.LFO = 0, p.op1.AR = 16, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 12, p.op2.DR = 3, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 4, p.op2.TL = 6, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 14, p.op3.DR = 7,
    p.op3.SR = 0, p.op3.RR = 8, p.op3.SL = 4, p.op3.TL = 3, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 22, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(21, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 16, p.op1.DR = 0, p.op1.SR = 1, p.op1.RR = 11, p.op1.SL = 0,
    p.op1.TL = 3, p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 3,
    p.op2.RR = 9, p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 3, p.op2.AMS = 0, p.op3.AR = 18,
    p.op3.DR = 0, p.op3.SR = 1, p.op3.RR = 11, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 4, p.op3.DT = 7,
    p.op3.AMS = 0, p.op4.AR = 24, p.op4.DR = 0, p.op4.SR = 2, p.op4.RR = 10, p.op4.SL = 0, p.op4.TL = 2, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 7, p.op4.AMS = 0;
    set_program(22, p);
    p.ALG = 3, p.FB = 3, p.LFO = 0, p.op1.AR = 20, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 2,
    p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 16, p.op2.DR = 3, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 4, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 7,
    p.op3.SR = 0, p.op3.RR = 8, p.op3.SL = 4, p.op3.TL = 3, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 24, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(23, p);
    p.ALG = 0, p.FB = 0, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 31, p.op1.SR = 31, p.op1.RR = 15, p.op1.SL = 0,
    p.op1.TL = 127, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 0, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 0, p.op3.SR = 4, p.op3.RR = 10, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 29, p.op4.DR = 14, p.op4.SR = 8, p.op4.RR = 9, p.op4.SL = 2, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(24, p);
    p.ALG = 1, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 4, p.op3.RR = 10, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 20, p.op4.SR = 8, p.op4.RR = 9, p.op4.SL = 2, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(25, p);
    p.ALG = 1, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 31, p.op1.SR = 31, p.op1.RR = 15, p.op1.SL = 0,
    p.op1.TL = 127, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 0, p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 3, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 0, p.op3.SR = 4, p.op3.RR = 10, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 12, p.op4.SR = 8, p.op4.RR = 9, p.op4.SL = 1, p.op4.TL = 3, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(26, p);
    p.ALG = 0, p.FB = 0, p.LFO = 0, p.op1.AR = 20, p.op1.DR = 0, p.op1.SR = 2, p.op1.RR = 9, p.op1.SL = 0,
    p.op1.TL = 10, p.op1.KS = 0, p.op1.ML = 12, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 0, p.op2.SR = 2,
    p.op2.RR = 10, p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 4, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 0, p.op3.SR = 4, p.op3.RR = 10, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 14, p.op4.SR = 8, p.op4.RR = 9, p.op4.SL = 2, p.op4.TL = 4, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(27, p);
    p.ALG = 0, p.FB = 0, p.LFO = 0, p.op1.AR = 20, p.op1.DR = 0, p.op1.SR = 2, p.op1.RR = 15, p.op1.SL = 0,
    p.op1.TL = 10, p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 0, p.op2.SR = 2,
    p.op2.RR = 15, p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 0, p.op3.SR = 6, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 8, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 16, p.op4.SR = 10, p.op4.RR = 15, p.op4.SL = 2, p.op4.TL = 5, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(28, p);
    p.ALG = 1, p.FB = 0, p.LFO = 0, p.op1.AR = 16, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 12, p.op1.SL = 0,
    p.op1.TL = 4, p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 18, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 12, p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 3, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 20,
    p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 12, p.op3.SL = 0, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 29, p.op4.DR = 4, p.op4.SR = 2, p.op4.RR = 12, p.op4.SL = 1, p.op4.TL = 2, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(29, p);
    p.ALG = 5, p.FB = 0, p.LFO = 0, p.op1.AR = 20, p.op1.DR = 0, p.op1.SR = 1, p.op1.RR = 9, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 14, p.op2.SR = 1, p.op2.RR = 9,
    p.op2.SL = 1, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 5, p.op2.DT = 7, p.op2.AMS = 0, p.op3.AR = 28, p.op3.DR = 14,
    p.op3.SR = 1, p.op3.RR = 9, p.op3.SL = 1, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 28, p.op4.DR = 14, p.op4.SR = 1, p.op4.RR = 9, p.op4.SL = 1, p.op4.TL = 4, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 3, p.op4.AMS = 0;
    set_program(30, p);
    p.ALG = 4, p.FB = 3, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 0, p.op1.TL = 2,
    p.op1.KS = 0, p.op1.ML = 8, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 12, p.op2.SR = 2, p.op2.RR = 9,
    p.op2.SL = 2, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 18,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 5, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 10, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 18, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 3, p.op4.TL = 2, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(31, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 24, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 2, p.op1.ML = 0, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 24, p.op2.DR = 18, p.op2.SR = 4, p.op2.RR = 8,
    p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 2, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 18,
    p.op3.SR = 3, p.op3.RR = 7, p.op3.SL = 1, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 22, p.op4.DR = 18, p.op4.SR = 3, p.op4.RR = 7, p.op4.SL = 1, p.op4.TL = 2, p.op4.KS = 0, p.op4.ML = 2,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(32, p);
    p.ALG = 2, p.FB = 3, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 10, p.op1.SR = 0, p.op1.RR = 15, p.op1.SL = 1,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 22, p.op2.SR = 0,
    p.op2.RR = 6, p.op2.SL = 1, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 8, p.op3.SR = 8, p.op3.RR = 8, p.op3.SL = 1, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 22, p.op4.DR = 14, p.op4.SR = 0, p.op4.RR = 7, p.op4.SL = 1, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(33, p);
    p.ALG = 2, p.FB = 3, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 10, p.op1.SR = 5, p.op1.RR = 15, p.op1.SL = 1,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 22, p.op2.SR = 10,
    p.op2.RR = 6, p.op2.SL = 1, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 8, p.op3.SR = 8, p.op3.RR = 8, p.op3.SL = 1, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 14, p.op4.SR = 6, p.op4.RR = 7, p.op4.SL = 1, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(34, p);
    p.ALG = 4, p.FB = 0, p.LFO = 0, p.op1.AR = 24, p.op1.DR = 8, p.op1.SR = 3, p.op1.RR = 13, p.op1.SL = 0,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 9, p.op2.SR = 1,
    p.op2.RR = 13, p.op2.SL = 2, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127,
    p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(35, p);
    p.ALG = 2, p.FB = 0, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 10, p.op1.SR = 5, p.op1.RR = 15, p.op1.SL = 1,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 18, p.op2.SR = 8,
    p.op2.RR = 6, p.op2.SL = 1, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 6, p.op3.SR = 6, p.op3.RR = 10, p.op3.SL = 1, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 4, p.op4.SR = 6, p.op4.RR = 10, p.op4.SL = 1, p.op4.TL = 2, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(36, p);
    p.ALG = 2, p.FB = 0, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 10, p.op1.SR = 2, p.op1.RR = 15, p.op1.SL = 1,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 18, p.op2.SR = 0,
    p.op2.RR = 6, p.op2.SL = 1, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 6, p.op3.SR = 2, p.op3.RR = 10, p.op3.SL = 1, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 4, p.op4.SR = 4, p.op4.RR = 10, p.op4.SL = 1, p.op4.TL = 2, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(37, p);
    p.ALG = 2, p.FB = 4, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 16, p.op1.SR = 12, p.op1.RR = 15, p.op1.SL = 1,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 7, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 12, p.op2.SR = 6,
    p.op2.RR = 6, p.op2.SL = 2, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 13, p.op3.SR = 11, p.op3.RR = 8, p.op3.SL = 1, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 3,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 10, p.op4.SR = 6, p.op4.RR = 12, p.op4.SL = 3, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(38, p);
    p.ALG = 3, p.FB = 1, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 7, p.op1.SR = 3, p.op1.RR = 15, p.op1.SL = 1,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 10, p.op2.SR = 2,
    p.op2.RR = 6, p.op2.SL = 1, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 10, p.op3.SR = 1, p.op3.RR = 8, p.op3.SL = 1, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 12, p.op4.SL = 1, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(39, p);
    p.ALG = 5, p.FB = 7, p.LFO = 0, p.op1.AR = 20, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 6, p.op1.SL = 0,
    p.op1.TL = 11, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 16,
    p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 4, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 16, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 10, p.op4.KS = 0,
    p.op4.ML = 8, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(40, p);
    p.ALG = 3, p.FB = 7, p.LFO = 0, p.op1.AR = 20, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 8,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 16, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 8, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 16, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(41, p);
    p.ALG = 2, p.FB = 4, p.LFO = 0, p.op1.AR = 20, p.op1.DR = 1, p.op1.SR = 1, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 2, p.op2.SR = 2, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 20, p.op3.DR = 2,
    p.op3.SR = 2, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 1, p.op4.SR = 1, p.op4.RR = 1, p.op4.SL = 7, p.op4.TL = 0, p.op4.KS = 3, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(42, p);
    p.ALG = 2, p.FB = 5, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 1, p.op1.SR = 1, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 1, p.op2.SR = 1, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 1,
    p.op3.SR = 1, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 3, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 1, p.op4.SR = 1, p.op4.RR = 7, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 1, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(43, p);
    p.ALG = 3, p.FB = 4, p.LFO = 1, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 5,
    p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 0, p.op1.AMS = 1, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 20, p.op3.DR = 10,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 5, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 14, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 1, p.op4.TL = 2, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(44, p);
    p.ALG = 5, p.FB = 4, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 0, p.op1.SR = 13, p.op1.RR = 6, p.op1.SL = 0,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 0, p.op2.SR = 13,
    p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 0, p.op3.SR = 19, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 4, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 17, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 2, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(45, p);
    p.ALG = 6, p.FB = 2, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 0, p.op1.SR = 9, p.op1.RR = 6, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 1, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 16, p.op2.SR = 9, p.op2.RR = 6,
    p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 1, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28, p.op3.DR = 16,
    p.op3.SR = 9, p.op3.RR = 6, p.op3.SL = 1, p.op3.TL = 0, p.op3.KS = 1, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(46, p);
    p.ALG = 0, p.FB = 6, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 13, p.op1.RR = 6, p.op1.SL = 0,
    p.op1.TL = 0, p.op1.KS = 2, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 13,
    p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 2, p.op2.ML = 4, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 0, p.op3.SR = 13, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 2, p.op3.ML = 2, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 13, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 2,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(47, p);
    p.ALG = 3, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 5,
    p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 24, p.op3.DR = 10,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 5, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 14, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 1, p.op4.TL = 2, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(48, p);
    p.ALG = 3, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 5,
    p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 24, p.op3.DR = 10,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 5, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 12, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(49, p);
    p.ALG = 3, p.FB = 4, p.LFO = 0, p.op1.AR = 14, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 14, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 14, p.op3.DR = 10,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 5, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 14, p.op4.DR = 14, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 1, p.op4.TL = 2, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(50, p);
    p.ALG = 3, p.FB = 4, p.LFO = 0, p.op1.AR = 14, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 14, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 14, p.op3.DR = 10,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 5, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 12, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 2, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(51, p);
    p.ALG = 7, p.FB = 5, p.LFO = 0, p.op1.AR = 16, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 8, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 7, p.op1.AMS = 0, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 7, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 3, p.op3.AMS = 0,
    p.op4.AR = 18, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 3, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 3, p.op4.AMS = 0;
    set_program(52, p);
    p.ALG = 7, p.FB = 4, p.LFO = 0, p.op1.AR = 24, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 8, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 24, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 7, p.op2.AMS = 0, p.op3.AR = 24, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 15, p.op3.KS = 0, p.op3.ML = 4, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 24, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 18, p.op4.KS = 0, p.op4.ML = 0,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(53, p);
    p.ALG = 7, p.FB = 3, p.LFO = 0, p.op1.AR = 17, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 7,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 7, p.op2.AMS = 0, p.op3.AR = 17, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 8, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 17, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 7, p.op4.SL = 0, p.op4.TL = 14, p.op4.KS = 0, p.op4.ML = 4,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(54, p);
    p.ALG = 5, p.FB = 5, p.LFO = 0, p.op1.AR = 16, p.op1.DR = 0, p.op1.SR = 13, p.op1.RR = 6, p.op1.SL = 0,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 18, p.op2.DR = 0, p.op2.SR = 13,
    p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 0, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 20,
    p.op3.DR = 0, p.op3.SR = 13, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 16, p.op4.DR = 0, p.op4.SR = 13, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 2, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(55, p);
    p.ALG = 4, p.FB = 4, p.LFO = 0, p.op1.AR = 22, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 22, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 22, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 8, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 22, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(56, p);
    p.ALG = 4, p.FB = 4, p.LFO = 0, p.op1.AR = 17, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 8, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 20, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 8, p.op3.SL = 1, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 1, p.op4.TL = 12, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(57, p);
    p.ALG = 4, p.FB = 3, p.LFO = 0, p.op1.AR = 17, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 8, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 20, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 8, p.op3.SL = 1, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 1, p.op4.TL = 12, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(58, p);
    p.ALG = 3, p.FB = 3, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 4, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 18, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(59, p);
    p.ALG = 4, p.FB = 2, p.LFO = 0, p.op1.AR = 17, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 8, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 20, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 8, p.op3.SL = 1, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 1, p.op4.TL = 12, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(60, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 17, p.op1.DR = 9, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 1, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 17, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 17, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(61, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 17, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 9, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 2, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 17, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0, p.op4.ML = 3,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(62, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 14, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 14, p.op2.DR = 7, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 2, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 14, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0, p.op4.ML = 3,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(63, p);
    p.ALG = 3, p.FB = 5, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 8,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 10, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(64, p);
    p.ALG = 1, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 10, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(65, p);
    p.ALG = 1, p.FB = 5, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 10, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(66, p);
    p.ALG = 3, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 24, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 10, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(67, p);
    p.ALG = 5, p.FB = 5, p.LFO = 0, p.op1.AR = 18, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 3,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 18, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(68, p);
    p.ALG = 4, p.FB = 4, p.LFO = 0, p.op1.AR = 20, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 24, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 24, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(69, p);
    p.ALG = 3, p.FB = 2, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 7,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 22, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 10, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(70, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 5,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 1, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 16, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 1, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(71, p);
    p.ALG = 4, p.FB = 4, p.LFO = 0, p.op1.AR = 16, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 0,
    p.op1.TL = 12, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 9, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 1, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 12, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 16, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 8, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(72, p);
    p.ALG = 6, p.FB = 1, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 18, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 18, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 3, p.op4.KS = 0, p.op4.ML = 3,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(73, p);
    p.ALG = 6, p.FB = 3, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 8,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 20, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 28, p.op4.DR = 18, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 1, p.op4.TL = 2, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(74, p);
    p.ALG = 4, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 10, p.op1.SL = 0,
    p.op1.TL = 6, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 10, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 17, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 10, p.op4.SL = 0, p.op4.TL = 8, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(75, p);
    p.ALG = 6, p.FB = 4, p.LFO = 0, p.op1.AR = 13, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 0, p.op1.TL = 8,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 3, p.op1.AMS = 0, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 16, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 8, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 16, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 16, p.op4.KS = 0, p.op4.ML = 4,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(76, p);
    p.ALG = 4, p.FB = 4, p.LFO = 2, p.op1.AR = 12, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 12, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 10, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 2,
    p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(77, p);
    p.ALG = 6, p.FB = 2, p.LFO = 0, p.op1.AR = 14, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 8,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 18, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 18, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 2, p.op4.KS = 0, p.op4.ML = 4,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(78, p);
    p.ALG = 7, p.FB = 4, p.LFO = 0, p.op1.AR = 17, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 17, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(79, p);
    p.ALG = 6, p.FB = 5, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 15, p.op1.SL = 0,
    p.op1.TL = 4, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 15, p.op2.SL = 0, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127,
    p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(80, p);
    p.ALG = 6, p.FB = 5, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 15, p.op1.SL = 0,
    p.op1.TL = 4, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 15, p.op2.SL = 0, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127,
    p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(81, p);
    p.ALG = 4, p.FB = 2, p.LFO = 0, p.op1.AR = 2, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 10, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 16, p.op2.SR = 0, p.op2.RR = 10,
    p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 20, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 10, p.op3.SL = 0, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 16, p.op4.SR = 0, p.op4.RR = 10, p.op4.SL = 1, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(82, p);
    p.ALG = 3, p.FB = 7, p.LFO = 0, p.op1.AR = 16, p.op1.DR = 0, p.op1.SR = 14, p.op1.RR = 11, p.op1.SL = 0,
    p.op1.TL = 5, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 0, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 11, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(83, p);
    p.ALG = 5, p.FB = 4, p.LFO = 0, p.op1.AR = 16, p.op1.DR = 16, p.op1.SR = 1, p.op1.RR = 9, p.op1.SL = 1,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 14, p.op2.SR = 1,
    p.op2.RR = 9, p.op2.SL = 1, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 4, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 14, p.op3.SR = 1, p.op3.RR = 9, p.op3.SL = 1, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 14, p.op4.SR = 1, p.op4.RR = 9, p.op4.SL = 1, p.op4.TL = 2, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(84, p);
    p.ALG = 7, p.FB = 5, p.LFO = 0, p.op1.AR = 16, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 8, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 7, p.op1.AMS = 0, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 7, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 3, p.op3.AMS = 0,
    p.op4.AR = 18, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 0, p.op4.TL = 3, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 3, p.op4.AMS = 0;
    set_program(85, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 26, p.op1.DR = 4, p.op1.SR = 0, p.op1.RR = 9, p.op1.SL = 1, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 4, p.op2.SR = 0, p.op2.RR = 9,
    p.op2.SL = 1, p.op2.TL = 1, p.op2.KS = 0, p.op2.ML = 3, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 26, p.op3.DR = 4,
    p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 1, p.op3.TL = 1, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 26, p.op4.DR = 4, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 1, p.op4.TL = 1, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(86, p);
    p.ALG = 5, p.FB = 5, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 16, p.op1.SR = 0, p.op1.RR = 10, p.op1.SL = 1,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 16, p.op2.SR = 0,
    p.op2.RR = 10, p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 16, p.op3.SR = 0, p.op3.RR = 10, p.op3.SL = 1, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 16, p.op4.SR = 0, p.op4.RR = 10, p.op4.SL = 1, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(87, p);
    p.ALG = 7, p.FB = 4, p.LFO = 1, p.op1.AR = 18, p.op1.DR = 0, p.op1.SR = 4, p.op1.RR = 5, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 2, p.op2.AR = 18, p.op2.DR = 0, p.op2.SR = 4, p.op2.RR = 5,
    p.op2.SL = 0, p.op2.TL = 1, p.op2.KS = 1, p.op2.ML = 2, p.op2.DT = 1, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 0,
    p.op3.SR = 4, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 2, p.op3.ML = 4, p.op3.DT = 2, p.op3.AMS = 0,
    p.op4.AR = 18, p.op4.DR = 0, p.op4.SR = 4, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 6, p.op4.KS = 3, p.op4.ML = 8,
    p.op4.DT = 3, p.op4.AMS = 0;
    set_program(88, p);
    p.ALG = 6, p.FB = 3, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 1, p.op2.AR = 10, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 7,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 10, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(89, p);
    p.ALG = 5, p.FB = 5, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 14, p.op1.SR = 0, p.op1.RR = 10, p.op1.SL = 1,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 24, p.op2.DR = 10, p.op2.SR = 0,
    p.op2.RR = 6, p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 1, p.op2.AMS = 0, p.op3.AR = 26,
    p.op3.DR = 14, p.op3.SR = 0, p.op3.RR = 9, p.op3.SL = 1, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 14, p.op4.SR = 0, p.op4.RR = 9, p.op4.SL = 1, p.op4.TL = 4, p.op4.KS = 0,
    p.op4.ML = 4, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(90, p);
    p.ALG = 7, p.FB = 2, p.LFO = 4, p.op1.AR = 16, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 6, p.op1.SL = 0, p.op1.TL = 2,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 7, p.op1.AMS = 0, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 7,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 7, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 3, p.op3.AMS = 0,
    p.op4.AR = 18, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 7, p.op4.SL = 0, p.op4.TL = 3, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 3, p.op4.AMS = 3;
    set_program(91, p);
    p.ALG = 7, p.FB = 4, p.LFO = 0, p.op1.AR = 12, p.op1.DR = 8, p.op1.SR = 0, p.op1.RR = 6, p.op1.SL = 1, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 15, p.op2.DR = 8, p.op2.SR = 0, p.op2.RR = 6,
    p.op2.SL = 1, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 1, p.op2.AMS = 0, p.op3.AR = 16, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 8, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 16, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 24, p.op4.KS = 0, p.op4.ML = 6,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(92, p);
    p.ALG = 6, p.FB = 6, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 6,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 11, p.op2.DR = 6, p.op2.SR = 4, p.op2.RR = 7,
    p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31,
    p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(93, p);
    p.ALG = 7, p.FB = 4, p.LFO = 2, p.op1.AR = 20, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 2,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 7, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 7,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 7, p.op2.AMS = 0, p.op3.AR = 20, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 3, p.op3.AMS = 3,
    p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 3, p.op4.AMS = 0;
    set_program(94, p);
    p.ALG = 6, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 12, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 1, p.op2.AMS = 0, p.op3.AR = 12, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 3, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 12, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 6, p.op4.KS = 0, p.op4.ML = 3,
    p.op4.DT = 2, p.op4.AMS = 0;
    set_program(95, p);
    p.ALG = 4, p.FB = 4, p.LFO = 5, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 26, p.op2.DR = 16, p.op2.SR = 4, p.op2.RR = 6,
    p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 7, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 26, p.op4.DR = 16, p.op4.SR = 5, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 16, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 3;
    set_program(96, p);
    p.ALG = 5, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 11, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 1,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 2, p.op1.AMS = 1, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 0,
    p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 16,
    p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 16, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 8, p.op4.KS = 0,
    p.op4.ML = 6, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(97, p);
    p.ALG = 6, p.FB = 2, p.LFO = 0, p.op1.AR = 24, p.op1.DR = 0, p.op1.SR = 7, p.op1.RR = 5, p.op1.SL = 0, p.op1.TL = 8,
    p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 24, p.op2.DR = 0, p.op2.SR = 7, p.op2.RR = 5,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31,
    p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(98, p);
    p.ALG = 3, p.FB = 2, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 8,
    p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 16, p.op2.SR = 0, p.op2.RR = 6,
    p.op2.SL = 1, p.op2.TL = 6, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28, p.op3.DR = 16,
    p.op3.SR = 0, p.op3.RR = 6, p.op3.SL = 1, p.op3.TL = 6, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 28, p.op4.DR = 16, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 1, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(99, p);
    p.ALG = 5, p.FB = 4, p.LFO = 5, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 8, p.op1.RR = 5, p.op1.SL = 0, p.op1.TL = 8,
    p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 1, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 0, p.op2.SR = 8, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28, p.op3.DR = 0,
    p.op3.SR = 8, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 2,
    p.op4.AR = 28, p.op4.DR = 0, p.op4.SR = 8, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0, p.op4.ML = 3,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(100, p);
    p.ALG = 7, p.FB = 2, p.LFO = 0, p.op1.AR = 10, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 6, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 10, p.op2.DR = 0, p.op2.SR = 1, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 10, p.op3.DR = 0,
    p.op3.SR = 2, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 8, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 10, p.op4.DR = 0, p.op4.SR = 2, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 12, p.op4.KS = 0, p.op4.ML = 3,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(101, p);
    p.ALG = 4, p.FB = 3, p.LFO = 1, p.op1.AR = 22, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 7,
    p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 1, p.op2.AR = 22, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 22, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 1,
    p.op4.AR = 22, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(102, p);
    p.ALG = 4, p.FB = 6, p.LFO = 2, p.op1.AR = 22, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 6,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 22, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 22, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0, p.op3.AMS = 1,
    p.op4.AR = 22, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 8, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(103, p);
    p.ALG = 5, p.FB = 4, p.LFO = 0, p.op1.AR = 22, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 22, p.op2.DR = 0, p.op2.SR = 6, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 22, p.op3.DR = 0,
    p.op3.SR = 6, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 3, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(104, p);
    p.ALG = 7, p.FB = 5, p.LFO = 0, p.op1.AR = 24, p.op1.DR = 0, p.op1.SR = 7, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 14, p.op2.SR = 14, p.op2.RR = 15,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31,
    p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(105, p);
    p.ALG = 3, p.FB = 0, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 1,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 17, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 24, p.op4.DR = 0, p.op4.SR = 10, p.op4.RR = 7, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(106, p);
    p.ALG = 3, p.FB = 0, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 2,
    p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 17, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 24, p.op4.DR = 0, p.op4.SR = 11, p.op4.RR = 5, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(107, p);
    p.ALG = 4, p.FB = 5, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 20, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 2,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 0, p.op2.SR = 13,
    p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 28, p.op4.DR = 0, p.op4.SR = 13, p.op4.RR = 7, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(108, p);
    p.ALG = 3, p.FB = 2, p.LFO = 0, p.op1.AR = 18, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 2,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 20, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31,
    p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(109, p);
    p.ALG = 3, p.FB = 0, p.LFO = 0, p.op1.AR = 18, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 22, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 0,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 18, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 8, p.op3.KS = 0, p.op3.ML = 2, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 22, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(110, p);
    p.ALG = 4, p.FB = 6, p.LFO = 0, p.op1.AR = 15, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 11,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 15, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 0, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 17, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 11, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(111, p);
    p.ALG = 4, p.FB = 4, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 10, p.op1.RR = 6, p.op1.SL = 0,
    p.op1.TL = 2, p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 24, p.op2.DR = 0, p.op2.SR = 10,
    p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 0, p.op3.SR = 10, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 5, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 24, p.op4.DR = 0, p.op4.SR = 10, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0,
    p.op4.ML = 4, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(112, p);
    p.ALG = 5, p.FB = 5, p.LFO = 6, p.op1.AR = 24, p.op1.DR = 0, p.op1.SR = 15, p.op1.RR = 8, p.op1.SL = 0,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 2, p.op2.AR = 24, p.op2.DR = 0, p.op2.SR = 15,
    p.op2.RR = 8, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 22,
    p.op3.DR = 0, p.op3.SR = 15, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 2, p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 20, p.op4.DR = 0, p.op4.SR = 15, p.op4.RR = 8, p.op4.SL = 0, p.op4.TL = 4, p.op4.KS = 0,
    p.op4.ML = 5, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(113, p);
    p.ALG = 4, p.FB = 3, p.LFO = 5, p.op1.AR = 28, p.op1.DR = 0, p.op1.SR = 11, p.op1.RR = 5, p.op1.SL = 0,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 0, p.op2.SR = 11,
    p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28,
    p.op3.DR = 0, p.op3.SR = 11, p.op3.RR = 5, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 2, p.op4.AR = 28, p.op4.DR = 0, p.op4.SR = 11, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 5, p.op4.KS = 0,
    p.op4.ML = 2, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(114, p);
    p.ALG = 5, p.FB = 3, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 5, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 21, p.op2.RR = 10,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 21, p.op3.RR = 10, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 5, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 21, p.op4.RR = 10, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 7,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(115, p);
    p.ALG = 6, p.FB = 7, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 0, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 19, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 0, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0,
    p.op3.SR = 13, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 11, p.op4.RR = 5, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 0,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(116, p);
    p.ALG = 5, p.FB = 5, p.LFO = 0, p.op1.AR = 28, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 4,
    p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 28, p.op2.DR = 0, p.op2.SR = 17, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 0, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 28, p.op3.DR = 0,
    p.op3.SR = 19, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 0, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 28, p.op4.DR = 0, p.op4.SR = 21, p.op4.RR = 10, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 2,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(117, p);
    p.ALG = 4, p.FB = 7, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 0, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 19, p.op2.RR = 9,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 0, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 14, p.op3.DR = 0,
    p.op3.SR = 15, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 0, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 11, p.op4.RR = 5, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 0,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(118, p);
    p.ALG = 6, p.FB = 7, p.LFO = 6, p.op1.AR = 7, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 6,
    p.op1.KS = 0, p.op1.ML = 4, p.op1.DT = 0, p.op1.AMS = 1, p.op2.AR = 8, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 15,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31,
    p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(119, p);
    p.ALG = 7, p.FB = 0, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 31, p.op1.SR = 31, p.op1.RR = 15, p.op1.SL = 0,
    p.op1.TL = 127, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 31,
    p.op2.SR = 31, p.op2.RR = 15, p.op2.SL = 0, p.op2.TL = 127, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0,
    p.op3.AR = 31, p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0,
    p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0,
    p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(120, p);
    p.ALG = 6, p.FB = 7, p.LFO = 0, p.op1.AR = 17, p.op1.DR = 0, p.op1.SR = 17, p.op1.RR = 8, p.op1.SL = 0,
    p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 15, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 17, p.op2.DR = 0, p.op2.SR = 17,
    p.op2.RR = 8, p.op2.SL = 0, p.op2.TL = 16, p.op2.KS = 0, p.op2.ML = 1, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31,
    p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0,
    p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127,
    p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(121, p);
    p.ALG = 6, p.FB = 7, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 15, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 8, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 15, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31,
    p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(122, p);
    p.ALG = 5, p.FB = 2, p.LFO = 5, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 12, p.op1.DT = 0, p.op1.AMS = 1, p.op2.AR = 16, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 7,
    p.op2.SL = 0, p.op2.TL = 2, p.op2.KS = 0, p.op2.ML = 3, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 16, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 4, p.op3.KS = 0, p.op3.ML = 9, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 16, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 7, p.op4.SL = 0, p.op4.TL = 6, p.op4.KS = 0, p.op4.ML = 12,
    p.op4.DT = 0, p.op4.AMS = 0;
    set_program(123, p);
    p.ALG = 5, p.FB = 4, p.LFO = 5, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 13, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 2, p.op2.DT = 0, p.op2.AMS = 1, p.op3.AR = 31, p.op3.DR = 31,
    p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(124, p);
    p.ALG = 6, p.FB = 7, p.LFO = 5, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 15, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 8,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 0, p.op2.DT = 0, p.op2.AMS = 3, p.op3.AR = 31, p.op3.DR = 31,
    p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(125, p);
    p.ALG = 5, p.FB = 7, p.LFO = 6, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 15, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 8, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 6,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 4, p.op2.DT = 0, p.op2.AMS = 3, p.op3.AR = 8, p.op3.DR = 0,
    p.op3.SR = 0, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 0, p.op3.KS = 0, p.op3.ML = 8, p.op3.DT = 0, p.op3.AMS = 3,
    p.op4.AR = 8, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 6, p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 12,
    p.op4.DT = 0, p.op4.AMS = 3;
    set_program(126, p);
    p.ALG = 5, p.FB = 7, p.LFO = 0, p.op1.AR = 31, p.op1.DR = 0, p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0,
    p.op1.KS = 0, p.op1.ML = 11, p.op1.DT = 0, p.op1.AMS = 0, p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 15, p.op2.RR = 7,
    p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 11, p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31,
    p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0,
    p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0,
    p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_program(127, p);
    p.ALG = 7, p.FB = 7, p.LFO = 0, p.key = 36, p.panpot = 8192, p.assign = 1, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 17, p.op1.RR = 8, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 1, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 0, p.op2.RR = 15, p.op2.SL = 0, p.op2.TL = 127, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 0, p.op3.RR = 15, p.op3.SL = 0, p.op3.TL = 127,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 0, p.op4.RR = 15,
    p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(-1, p);
    p.ALG = 5, p.FB = 0, p.LFO = 0, p.key = 11, p.panpot = 8192, p.assign = 35, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 17, p.op1.RR = 8, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 17, p.op2.RR = 8, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 17, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 17, p.op4.RR = 8,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(35, p);
    p.ALG = 5, p.FB = 0, p.LFO = 0, p.key = 14, p.panpot = 8192, p.assign = 36, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 15, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 15, p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 15, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 15, p.op4.RR = 7,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(36, p);
    p.ALG = 6, p.FB = 7, p.LFO = 0, p.key = 59, p.panpot = 8192, p.assign = 37, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 19, p.op1.RR = 9, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 19, p.op2.RR = 9, p.op2.SL = 0, p.op2.TL = 4, p.op2.KS = 0, p.op2.ML = 2,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0,
    p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31,
    p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(37, p);
    p.ALG = 5, p.FB = 7, p.LFO = 0, p.key = 48, p.panpot = 8192, p.assign = 38, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 15, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 24, p.op2.SR = 17, p.op2.RR = 8, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 24, p.op3.SR = 17, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 24, p.op4.SR = 17, p.op4.RR = 8,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(38, p);
    p.ALG = 5, p.FB = 7, p.LFO = 0, p.key = 87, p.panpot = 6912, p.assign = 39, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 15, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 19, p.op2.RR = 9, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 1, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 19, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 2, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 19, p.op4.RR = 9,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 3, p.op4.AMS = 0;
    set_drum_program(39, p);
    p.ALG = 5, p.FB = 7, p.LFO = 0, p.key = 55, p.panpot = 8192, p.assign = 40, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 15, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 19, p.op2.RR = 9, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 19, p.op3.RR = 9, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 19, p.op4.RR = 9,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(40, p);
    p.ALG = 5, p.FB = 0, p.LFO = 0, p.key = 14, p.panpot = 4352, p.assign = 41, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 15, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 15, p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 15, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 15, p.op4.RR = 7,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(41, p);
    p.ALG = 6, p.FB = 7, p.LFO = 0, p.key = 99, p.panpot = 10752, p.assign = 42, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 21, p.op2.RR = 10, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0,
    p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31,
    p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(42, p);
    p.ALG = 5, p.FB = 0, p.LFO = 0, p.key = 18, p.panpot = 5888, p.assign = 43, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 15, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 15, p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 15, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 15, p.op4.RR = 7,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(43, p);
    p.ALG = 6, p.FB = 7, p.LFO = 0, p.key = 100, p.panpot = 10752, p.assign = 42, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 21, p.op2.RR = 10, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0,
    p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31,
    p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(44, p);
    p.ALG = 5, p.FB = 0, p.LFO = 0, p.key = 20, p.panpot = 7424, p.assign = 45, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 15, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 15, p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 15, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 15, p.op4.RR = 7,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(45, p);
    p.ALG = 6, p.FB = 7, p.LFO = 0, p.key = 100, p.panpot = 10752, p.assign = 42, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 13, p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 31, p.op3.SR = 31, p.op3.RR = 15, p.op3.SL = 0,
    p.op3.TL = 127, p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31,
    p.op4.SR = 31, p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(46, p);
    p.ALG = 5, p.FB = 0, p.LFO = 0, p.key = 24, p.panpot = 8960, p.assign = 47, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 15, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 15, p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 15, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 15, p.op4.RR = 7,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(47, p);
    p.ALG = 5, p.FB = 0, p.LFO = 0, p.key = 27, p.panpot = 10496, p.assign = 48, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 15, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 15, p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 15, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 15, p.op4.RR = 7,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(48, p);
    p.ALG = 5, p.FB = 7, p.LFO = 3, p.key = 104, p.panpot = 10752, p.assign = 49, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 8, p.op1.DT = 0, p.op1.AMS = 1,
    p.op2.AR = 31, p.op2.DR = 20, p.op2.SR = 11, p.op2.RR = 5, p.op2.SL = 1, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 2,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 20, p.op3.SR = 11, p.op3.RR = 5, p.op3.SL = 2, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 3, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 20, p.op4.SR = 11, p.op4.RR = 5,
    p.op4.SL = 3, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 5, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(49, p);
    p.ALG = 5, p.FB = 0, p.LFO = 0, p.key = 31, p.panpot = 12032, p.assign = 50, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 15, p.op1.RR = 7, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 3, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 15, p.op2.RR = 7, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 15, p.op3.RR = 7, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 0, p.op4.SR = 15, p.op4.RR = 7,
    p.op4.SL = 0, p.op4.TL = 0, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(50, p);
    p.ALG = 5, p.FB = 7, p.LFO = 0, p.key = 97, p.panpot = 5632, p.assign = 51, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 6, p.op1.KS = 0, p.op1.ML = 8, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 11, p.op2.RR = 5, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 11, p.op3.RR = 5, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15,
    p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(51, p);
    p.ALG = 5, p.FB = 7, p.LFO = 3, p.key = 94, p.panpot = 5632, p.assign = 52, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 0, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 2, p.op1.DT = 0, p.op1.AMS = 1,
    p.op2.AR = 31, p.op2.DR = 0, p.op2.SR = 11, p.op2.RR = 5, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 1,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 0, p.op3.SR = 11, p.op3.RR = 5, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 1, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31, p.op4.RR = 15,
    p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 1, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(52, p);
    p.ALG = 5, p.FB = 4, p.LFO = 0, p.key = 72, p.panpot = 3072, p.assign = 80, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 10, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 15, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 24, p.op2.SR = 17, p.op2.RR = 8, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 15,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 24, p.op3.SR = 17, p.op3.RR = 8, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 15, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31,
    p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 15, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(80, p);
    p.ALG = 5, p.FB = 4, p.LFO = 0, p.key = 72, p.panpot = 3072, p.assign = 80, p.op1.AR = 31, p.op1.DR = 0,
    p.op1.SR = 5, p.op1.RR = 0, p.op1.SL = 0, p.op1.TL = 0, p.op1.KS = 0, p.op1.ML = 15, p.op1.DT = 0, p.op1.AMS = 0,
    p.op2.AR = 31, p.op2.DR = 24, p.op2.SR = 13, p.op2.RR = 6, p.op2.SL = 0, p.op2.TL = 0, p.op2.KS = 0, p.op2.ML = 15,
    p.op2.DT = 0, p.op2.AMS = 0, p.op3.AR = 31, p.op3.DR = 24, p.op3.SR = 13, p.op3.RR = 6, p.op3.SL = 0, p.op3.TL = 0,
    p.op3.KS = 0, p.op3.ML = 15, p.op3.DT = 0, p.op3.AMS = 0, p.op4.AR = 31, p.op4.DR = 31, p.op4.SR = 31,
    p.op4.RR = 15, p.op4.SL = 0, p.op4.TL = 127, p.op4.KS = 0, p.op4.ML = 15, p.op4.DT = 0, p.op4.AMS = 0;
    set_drum_program(81, p);
}
// �N���A�B
void fm_note_factory::clear()
{
    // �f�t�H���g�̉��F(sin�g)
    static const struct FMPARAMETER param = {7,
                                             0,
                                             0, // ALG FB LFO
                                             // AR DR SR RR SL  TL KS ML DT AMS
                                             {31, 0, 0, 15, 0, 0, 0, 0, 0, 0},
                                             {0, 0, 0, 15, 0, 127, 0, 0, 0, 0},
                                             {0, 0, 0, 15, 0, 127, 0, 0, 0, 0},
                                             {0, 0, 0, 15, 0, 127, 0, 0, 0, 0}};
    drums.clear();
    programs.clear();
    programs[-1] = param;
}
// ���F�p�����[�^�̐���������
namespace
{
bool is_valid_fmparameter(const FMPARAMETER &p)
{
    return p.ALG >= 0 && p.ALG <= 7 && p.FB >= 0 && p.FB <= 7 && p.LFO >= 0 && p.LFO <= 7 && p.op1.AR >= 0 &&
           p.op1.AR <= 31 && p.op1.DR >= 0 && p.op1.DR <= 31 && p.op1.SR >= 0 && p.op1.SR <= 31 && p.op1.RR >= 0 &&
           p.op1.RR <= 15 && p.op1.SL >= 0 && p.op1.SL <= 15 && p.op1.TL >= 0 && p.op1.TL <= 127 && p.op1.KS >= 0 &&
           p.op1.KS <= 3 && p.op1.ML >= 0 && p.op1.ML <= 15 && p.op1.DT >= 0 && p.op1.DT <= 7 && p.op1.AMS >= 0 &&
           p.op1.AMS <= 3 && p.op2.AR >= 0 && p.op2.AR <= 31 && p.op2.DR >= 0 && p.op2.DR <= 31 && p.op2.SR >= 0 &&
           p.op2.SR <= 31 && p.op2.RR >= 0 && p.op2.RR <= 15 && p.op2.SL >= 0 && p.op2.SL <= 15 && p.op2.TL >= 0 &&
           p.op2.TL <= 127 && p.op2.KS >= 0 && p.op2.KS <= 3 && p.op2.ML >= 0 && p.op2.ML <= 15 && p.op2.DT >= 0 &&
           p.op2.DT <= 7 && p.op2.AMS >= 0 && p.op2.AMS <= 3 && p.op3.AR >= 0 && p.op3.AR <= 31 && p.op3.DR >= 0 &&
           p.op3.DR <= 31 && p.op3.SR >= 0 && p.op3.SR <= 31 && p.op3.RR >= 0 && p.op3.RR <= 15 && p.op3.SL >= 0 &&
           p.op3.SL <= 15 && p.op3.TL >= 0 && p.op3.TL <= 127 && p.op3.KS >= 0 && p.op3.KS <= 3 && p.op3.ML >= 0 &&
           p.op3.ML <= 15 && p.op3.DT >= 0 && p.op3.DT <= 7 && p.op3.AMS >= 0 && p.op3.AMS <= 3 && p.op4.AR >= 0 &&
           p.op4.AR <= 31 && p.op4.DR >= 0 && p.op4.DR <= 31 && p.op4.SR >= 0 && p.op4.SR <= 31 && p.op4.RR >= 0 &&
           p.op4.RR <= 15 && p.op4.SL >= 0 && p.op4.SL <= 15 && p.op4.TL >= 0 && p.op4.TL <= 127 && p.op4.KS >= 0 &&
           p.op4.KS <= 3 && p.op4.ML >= 0 && p.op4.ML <= 15 && p.op4.DT >= 0 && p.op4.DT <= 7 && p.op4.AMS >= 0 &&
           p.op4.AMS <= 3;
}
bool is_valid_drumparameter(const DRUMPARAMETER &p)
{
    return is_valid_fmparameter(p) && p.key >= 0 && p.key <= 127 && p.panpot >= 0 && p.panpot <= 16383;
}
} // namespace
// ���F�p�����[�^�擾�B
void fm_note_factory::get_program(int program, FMPARAMETER &p)
{
    if (programs.find(program) != programs.end())
    {
        p = programs[program];
    }
    else if (programs.find(program & 0x3FFF) != programs.end())
    {
        p = programs[program & 0x3FFF];
    }
    else if (programs.find(program & 0x7F) != programs.end())
    {
        p = programs[program & 0x7F];
    }
    else
    {
        p = programs[-1];
    }
}
// ���F�p�����[�^���Z�b�g�B
bool fm_note_factory::set_program(int number, const FMPARAMETER &p)
{
    if (is_valid_fmparameter(p))
    {
        programs[number] = p;
        return true;
    }
    else
    {
        return false;
    }
}
// �h�������F�p�����[�^���Z�b�g�B
bool fm_note_factory::set_drum_program(int number, const DRUMPARAMETER &p)
{
    if (is_valid_drumparameter(p))
    {
        drums[number] = p;
        return true;
    }
    else
    {
        return false;
    }
}
// �m�[�g�I���B
note *fm_note_factory::note_on(int_least32_t program, int note, int velocity, float frequency_multiplier)
{
    bool drum = (program >> 14) == 120;
    if (drum)
    {
        int                   n = (program & 0x3FFF) * 128 + note;
        struct DRUMPARAMETER *p;
        if (drums.find(n) != drums.end())
        {
            p = &drums[n];
        }
        else if (drums.find(n & 0x3FFF) != drums.end())
        {
            p = &drums[n & 0x3FFF];
        }
        else if (drums.find(note) != drums.end())
        {
            p = &drums[note];
        }
        else if (drums.find(-1) != drums.end())
        {
            p = &drums[-1];
        }
        else
        {
            return NULL;
        }
        return new fm_note(*p, p->key, velocity, p->panpot, p->assign, 1);
    }
    else
    {
        struct FMPARAMETER *p;
        if (programs.find(program) != programs.end())
        {
            p = &programs[program];
        }
        else if (programs.find(program & 0x7F) != programs.end())
        {
            p = &programs[program & 0x7F];
        }
        else
        {
            p = &programs[-1];
        }
        return new fm_note(*p, note, velocity, 8192, 0, frequency_multiplier);
    }
}
} // namespace midisynth
