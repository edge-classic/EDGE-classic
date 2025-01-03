/****************************************************************************

  emu2212.c -- S.C.C. emulator by Mitsutaka Okazaki 2001

  2001 09-30 : Version 1.00
  2001 10-03 : Version 1.01 -- Added SCC_set_quality().
  2002 02-14 : Version 1.10 -- Added SCC_writeReg(), SCC_set_type().
                               Fixed SCC_write().
  2002 02-17 : Version 1.11 -- Fixed SCC_write().
  2002 03-02 : Version 1.12 -- Removed SCC_init & SCC_close.
  2003 09-19 : Version 1.13 -- Added SCC_setMask() and SCC_toggleMask()

  Registar map for SCC_writeReg()

  $00-1F : WaveTable CH.A
  $20-3F : WaveTable CH.B
  $40-5F : WaveTable CH.C
  $60-7F : WaveTable CH.D&E(SCC), CH.D(SCC+)
  $80-9F : WaveTable CH.E

  $C0    : CH.A Freq(L)
  $C1    : CH.A Freq(H)
  $C2    : CH.B Freq(L)
  $C3    : CH.B Freq(H)
  $C4    : CH.C Freq(L)
  $C5    : CH.C Freq(H)
  $C6    : CH.D Freq(L)
  $C7    : CH.D Freq(H)
  $C8    : CH.E Freq(L)
  $C9    : CH.E Freq(H)

  $D0    : CH.A Volume
  $D1    : CH.B Volume
  $D2    : CH.C Volume
  $D3    : CH.D Volume
  $D4    : CH.E Volume

  $E0    : Bit0 = 0:SCC, 1:SCC+
  $E1    : CH mask
  $E2    : Extra Flags

(Extra)
  $F0    : CH.A Pan
  $F1    : CH.B Pan
  $F2    : CH.C Pan
  $F3    : CH.D Pan
  $F4    : CH.E Pan

*****************************************************************************/
#include "emu2212.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GETA_BITS 22

static void internal_refresh(SCC *scc)
{
    if (scc->quality)
    {
        scc->base_incr = 2 << GETA_BITS;
        scc->realstep  = (e_uint32)((1 << 31) / scc->rate);
        scc->sccstep   = (e_uint32)((1 << 31) / (scc->clk / 2));
        scc->scctime   = 0;
    }
    else
    {
        scc->base_incr = (e_uint32)((double)scc->clk * (1 << GETA_BITS) / scc->rate);
    }
}

EMU2212_API e_uint32 SCC_setMask(SCC *scc, e_uint32 mask)
{
    e_uint32 ret = 0;
    if (scc)
    {
        ret       = scc->mask;
        scc->mask = mask;
    }
    return ret;
}

EMU2212_API e_uint32 SCC_toggleMask(SCC *scc, e_uint32 mask)
{
    e_uint32 ret = 0;
    if (scc)
    {
        ret = scc->mask;
        scc->mask ^= mask;
    }
    return ret;
}

EMU2212_API void SCC_set_quality(SCC *scc, e_uint32 q)
{
    scc->quality = q;
    internal_refresh(scc);
}

EMU2212_API void SCC_set_rate(SCC *scc, e_uint32 r)
{
    scc->rate = r ? r : 44100;
    internal_refresh(scc);
}

EMU2212_API SCC *SCC_new(e_uint32 c, e_uint32 r)
{
    SCC *scc;

    scc = (SCC *)malloc(sizeof(SCC));
    if (scc == NULL)
        return NULL;
    memset(scc, 0, sizeof(SCC));

    scc->clk  = c;
    scc->rate = r ? r : 44100;
    SCC_set_quality(scc, 0);

    return scc;
}

EMU2212_API void SCC_reset(SCC *scc)
{

    int i, j;

    if (scc == NULL)
        return;

    scc->type      = SCC_STANDARD;
    scc->mode      = 0;
    scc->save_9000 = 0x3f;
    scc->save_B000 = 0;
    scc->save_BFFE = 0;

    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 5; j++)
            scc->wave[i][j] = 0;
        scc->count[i]  = 0;
        scc->freq[i]   = 0;
        scc->phase[i]  = 0;
        scc->volume[i] = 0;
        scc->offset[i] = 0;
        scc->rotate[i] = 0;
        scc->ch_pan[i] = 3;
    }

    scc->mask = 0;

    scc->ch_enable      = 0xff;
    scc->ch_enable_next = 0xff;

    scc->cycle_4bit = 0;
    scc->cycle_8bit = 0;
    scc->refresh    = 0;

    scc->out  = 0;
    scc->prev = 0;
    scc->next = 0;

    return;
}

EMU2212_API void SCC_delete(SCC *scc)
{
    if (scc != NULL)
        free(scc);
}

INLINE static e_int16 calc(SCC *scc)
{
    int     i;
    e_int32 mix = 0;

    for (i = 0; i < 5; i++)
    {
        scc->count[i] = (scc->count[i] + scc->incr[i]);

        if (scc->count[i] & (1 << (GETA_BITS + 5)))
        {
            scc->count[i] &= ((1 << (GETA_BITS + 5)) - 1);
            scc->offset[i] = (scc->offset[i] + 31) & scc->rotate[i];
            scc->ch_enable &= ~(1 << i);
            scc->ch_enable |= scc->ch_enable_next & (1 << i);
        }

        if (scc->ch_enable & (1 << i))
        {
            scc->phase[i] = ((scc->count[i] >> (GETA_BITS)) + scc->offset[i]) & 0x1F;
            if (!(scc->mask & SCC_MASK_CH(i)))
                mix += ((((e_int8)(scc->wave[i][scc->phase[i]]) * (e_int8)scc->volume[i]))) >> 4;
        }
    }

    return (e_int16)(mix << 4);
}

EMU2212_API e_int16 SCC_calc(SCC *scc)
{
    if (!scc->quality)
        return calc(scc);

    while (scc->realstep > scc->scctime)
    {
        scc->scctime += scc->sccstep;
        scc->prev = scc->next;
        scc->next = calc(scc);
    }

    scc->scctime -= scc->realstep;
    scc->out = (e_int16)(((double)scc->next * (scc->sccstep - scc->scctime) + (double)scc->prev * scc->scctime) /
                         scc->sccstep);

    return (e_int16)(scc->out);
}

EMU2212_API e_uint32 SCC_readReg(SCC *scc, e_uint32 adr)
{
    if (adr < 0xA0)
        return scc->wave[adr >> 5][adr & 0x1f];
    else
        return 0;
}

EMU2212_API e_uint32 SCC_read(SCC *scc, e_uint32 adr)
{
    if (!scc->mode && scc->save_9000 == 0x3F && (adr & 0xF800) == 0x9800)
    {
        adr &= 0xff;

        if (adr < 0x80)
            return scc->wave[adr >> 5][adr & 0x1f];
        else
            return 0;
    }
    else if (scc->mode && scc->save_B000 == 0x80 && (adr & 0xF800) == 0xB800)
    {
        adr &= 0xff;
        if (adr < 0x80)
            return scc->wave[adr >> 5][adr & 0x1f];
        else if (0xA0 <= adr && adr <= 0xBF)
            return scc->wave[4][adr & 0x1f];
        else
            return 0;
    }
    else
        return 0;
}

EMU2212_API void SCC_writeReg(SCC *scc, e_uint32 adr, e_uint32 val)
{
    int      ch;
    e_uint32 freq;

    adr &= 0xFF;

    if (adr < 0xA0)
    {
        ch = (adr & 0xF0) >> 5;
        if (!scc->rotate[ch])
        {
            scc->wave[ch][adr & 0x1F] = (e_int8)val;
            if (scc->mode == 0 && ch == 3)
                scc->wave[4][adr & 0x1F] = (e_int8)val;
        }
    }
    else if (0xC0 <= adr && adr <= 0xC9)
    {
        ch = (adr & 0x0F) >> 1;
        if (adr & 1)
            scc->freq[ch] = ((val & 0xF) << 8) | (scc->freq[ch] & 0xFF);
        else
            scc->freq[ch] = (scc->freq[ch] & 0xF00) | (val & 0xFF);

        if (scc->refresh)
            scc->count[ch] = 0;
        freq = scc->freq[ch];
        if (scc->cycle_8bit)
            freq &= 0xFF;
        if (scc->cycle_4bit)
            freq >>= 8;
        if (freq <= 8)
            scc->incr[ch] = 0;
        else
            scc->incr[ch] = scc->base_incr / (freq + 1);
    }
    else if (0xD0 <= adr && adr <= 0xD4)
    {
        scc->volume[adr & 0x0F] = (e_uint8)(val & 0xF);
    }
    else if (adr == 0xE0)
    {
        scc->mode = (e_uint8)val & 1;
        if (scc->mode)
            scc->save_BFFE |= 0x20;
        else
            scc->save_BFFE &= 0xDF; /* Synchronized with BFFE */
    }
    else if (adr == 0xE1)
    {
        scc->ch_enable_next = (e_uint8)val & 0x1F;
    }
    else if (adr == 0xE2)
    {
        scc->cycle_4bit = val & 1;
        scc->cycle_8bit = val & 2;
        scc->refresh    = val & 32;
        if (val & 64)
            for (ch = 0; ch < 5; ch++)
                scc->rotate[ch] = 0x1F;
        else
            for (ch = 0; ch < 5; ch++)
                scc->rotate[ch] = 0;
        if (val & 128)
            scc->rotate[3] = scc->rotate[4] = 0x1F;
    }
    else if (0xF0 <= adr && adr <= 0xF4)
        scc->ch_pan[adr & 0xF] = val;

    return;
}

INLINE static void write_standard(SCC *scc, e_uint32 adr, e_uint32 val)
{
    if ((adr & 0xF800) != 0x9800)
        return;

    adr &= 0xFF;

    if (adr < 0x80) /* wave */
    {
        SCC_writeReg(scc, adr, val);
    }
    else if (adr < 0x8A) /* freq */
    {
        SCC_writeReg(scc, adr + 0xC0 - 0x80, val);
    }
    else if (adr < 0x8F) /* volume */
    {
        SCC_writeReg(scc, adr + 0xD0 - 0x8A, val);
    }
    else if (adr == 0x8F) /* ch enable */
    {
        SCC_writeReg(scc, 0xE1, val);
    }
    else if (0xE0 <= adr) /* flags */
    {
        SCC_writeReg(scc, 0xE2, val);
    }
}

INLINE static void write_compatible(SCC *scc, e_uint32 adr, e_uint32 val)
{
    if ((adr & 0xF800) != 0x9800)
        return;

    adr &= 0xFF;

    if (adr < 0x80) /* wave */
    {
        SCC_writeReg(scc, adr, val);
    }
    else if (adr < 0x8A) /* freq */
    {
        SCC_writeReg(scc, adr + 0xC0 - 0x80, val);
    }
    else if (adr < 0x8F) /* volume */
    {
        SCC_writeReg(scc, adr + 0xD0 - 0x8A, val);
    }
    else if (adr == 0x8F) /* ch enable */
    {
        SCC_writeReg(scc, 0xE1, val);
    }
    else if (0xC0 <= adr && adr <= 0xDF) /* flags */
    {
        SCC_writeReg(scc, 0xE2, val);
    }
}

INLINE static void write_enhanced(SCC *scc, e_uint32 adr, e_uint32 val)
{
    if ((adr & 0xF800) != 0xB800)
        return;

    adr &= 0xFF;

    if (adr < 0x80) /* wave */
    {
        SCC_writeReg(scc, adr, val);
    }
    else if (adr < 0x8A) /* freq */
    {
        SCC_writeReg(scc, adr + 0xC0 - 0x80, val);
    }
    else if (adr < 0x8F) /* volume */
    {
        SCC_writeReg(scc, adr + 0xD0 - 0x8A, val);
    }
    else if (adr == 0x8F) /* ch enable */
    {
        SCC_writeReg(scc, 0xE1, val);
    }
    else if (0xA0 <= adr && adr <= 0xBF && scc->mode) /* ch[4].wave */
    {
        SCC_writeReg(scc, adr + 0x80 - 0xA0, val);
    }
    else if (0xC0 <= adr && adr <= 0xDF) /* flags */
    {
        SCC_writeReg(scc, 0xE2, val);
    }
}

INLINE static int BFFE_write(SCC *scc, e_uint32 adr, e_uint32 val)
{
    if ((adr & 0xFFFE) == 0xBFFE)
    {
        scc->save_BFFE = (e_uint8)val;
        if (val & 0x20)
            SCC_writeReg(scc, 0xE0, 1);
        else
            SCC_writeReg(scc, 0xE0, 0);
        return 1;
    }
    else
    {
        return 0;
    }
}

INLINE static int mapper_write(SCC *scc, e_uint32 adr, e_uint32 val)
{
    if ((adr & 0xF800) == 0x9000)
    {
        scc->save_9000 = (e_uint8)(val & 0x3F);
        return 1;
    }
    else if (scc->type == SCC_ENHANCED)
    {
        if (BFFE_write(scc, adr, val))
        {
            return 1;
        }
        else if ((adr & 0xF800) == 0xB000)
        {
            scc->save_B000 = (e_uint8)(val & 0x80);
            return 1;
        }
    }
    return 0;
}

EMU2212_API void SCC_write(SCC *scc, e_uint32 adr, e_uint32 val)
{
    val = val & 0xFF;

    if (scc->save_BFFE & 0x10) /* BFFE can only be accessed. */
    {
        BFFE_write(scc, adr, val);
        return;
    }
    else if ((scc->save_BFFE & 0x04) && (adr < 0xA000)) /* Ignore 8000H - 9FFFH */
    {
        return;
    }
    else if (mapper_write(scc, adr, val))
        return; /* Mapper ctrl registers */

    switch (scc->type)
    {
    case SCC_STANDARD:
        if (scc->save_9000 == 0x3F)
            write_standard(scc, adr, val);
        break;

    case SCC_ENHANCED:
        if (!scc->mode && scc->save_9000 == 0x3F)
            write_compatible(scc, adr, val);
        else if (scc->mode && scc->save_B000 == 0x80)
            write_enhanced(scc, adr, val);
        break;

    default:
        break;
    }

    return;
}

EMU2212_API void SCC_set_type(SCC *scc, e_uint32 type)
{
    scc->type = type;
}

EMU2212_API void SCC_calc_stereo(SCC *scc, e_int16 buf[2])
{

    int     i;
    e_int16 b;
    buf[0] = buf[1] = 0;

    for (i = 0; i < 5; i++)
    {
        scc->count[i] = (scc->count[i] + scc->incr[i]);

        if (scc->count[i] & (1 << (GETA_BITS + 5)))
        {
            scc->count[i] &= ((1 << (GETA_BITS + 5)) - 1);
            scc->offset[i] = (scc->offset[i] + 31) & scc->rotate[i];
            scc->ch_enable &= ~(1 << i);
            scc->ch_enable |= scc->ch_enable_next & (1 << i);
        }

        if (scc->ch_enable & (1 << i))
        {
            scc->phase[i] = ((scc->count[i] >> (GETA_BITS)) + scc->offset[i]) & 0x1F;
            if (!(scc->mask & SCC_MASK_CH(i)))
            {
                b = ((((e_int8)(scc->wave[i][scc->phase[i]]) * (e_int8)scc->volume[i]))) >> 4;
                if (scc->ch_pan[i] == 1)
                    buf[0] += b;
                else if (scc->ch_pan[i] == 2)
                    buf[1] += b;
                else
                {
                    buf[0] += b;
                    buf[1] += b;
                }
            }
        }
    }

    buf[0] <<= 3;
    buf[1] <<= 3;
}
