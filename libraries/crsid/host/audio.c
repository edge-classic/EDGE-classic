#include <stdint.h>

void cRSID_generateSound(cRSID_C64instance *C64, unsigned char *buf, unsigned short len)
{
    static unsigned short i;
    static unsigned char  j;
    static cRSID_Output   Output;
    static signed short   OutputL, OutputR;

    for (i = 0; i < len; i += 4)
    {
        for (j = 0; j < C64->PlaybackSpeed; ++j)
            Output = cRSID_generateSample(C64);
        Output.L   = Output.L * C64->MainVolume / 256;
        Output.R   = Output.R * C64->MainVolume / 256;
        buf[i + 0] = Output.L & 0xFF;
        buf[i + 1] = Output.L >> 8;
        buf[i + 2] = Output.R & 0xFF;
        buf[i + 3] = Output.R >> 8;
    }
}

void cRSID_generateFloat(cRSID_C64instance *C64, float *buf, unsigned short len)
{
    static unsigned short i;
    static unsigned char  j;
    static cRSID_Output   Output;
    static int16_t   OutputL, OutputR;

    for (i = 0; i < len; i += 8)
    {
        for (j = 0; j < C64->PlaybackSpeed; ++j)
            Output = cRSID_generateSample(C64);
#if defined _MSC_VER || (defined __SIZEOF_FLOAT__ && __SIZEOF_FLOAT__ == 4)
        *(uint32_t *)buf=0x43818000^((uint16_t)Output.L * C64->MainVolume / 256);
        *buf++ -= 259.0f;
        *(uint32_t *)buf=0x43818000^((uint16_t)Output.R * C64->MainVolume / 256);
        *buf++ -= 259.0f;
#else   
        *buf++ = (float)(Output.L * C64->MainVolume / 256) * 0.000030517578125f;
        *buf++ = (float)(Output.R * C64->MainVolume / 256) * 0.000030517578125f;
#endif
    }
}

static inline cRSID_Output cRSID_generateSample(cRSID_C64instance *C64)
{ // call this from custom buffer-filler
    static cRSID_Output Output;
    signed short        PSIDdigi;
    Output = cRSID_emulateC64(C64);
    if (C64->PSIDdigiMode)
    {
        PSIDdigi = cRSID_playPSIDdigi(C64);
        Output.L += PSIDdigi;
        Output.R += PSIDdigi;
    }
    if (Output.L >= 32767)
        Output.L = 32767;
    else if (Output.L <= -32768)
        Output.L = -32768; // saturation logic on overflow
    if (Output.R >= 32767)
        Output.R = 32767;
    else if (Output.R <= -32768)
        Output.R = -32768; // saturation logic on overflow
    return Output;
}
