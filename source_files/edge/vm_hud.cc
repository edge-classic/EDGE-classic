//------------------------------------------------------------------------
//  COAL HUD module
//------------------------------------------------------------------------
//
//  Copyright (c) 2006-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include <math.h>

#include "am_map.h" // AutomapDrawer
#include "coal.h"
#include "ddf_font.h"
#include "ddf_main.h"
#include "dm_state.h"
#include "e_main.h"
#include "e_player.h"
#include "epi.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_font.h"
#include "i_system.h"
#include "im_data.h"
#include "r_colormap.h"
#include "r_misc.h"
#include "r_modes.h"
#include "rad_trig.h" //Lobo: need this to access RTS
#include "s_sound.h"
#include "vm_coal.h"
#include "w_wad.h"

extern coal::VM *ui_vm;

extern void COALSetVector(coal::VM *vm, const char *mod_name, const char *var_name, double val_1, double val_2,
                          double val_3);
extern void COALSetFloat(coal::VM *vm, const char *mod, const char *name, double value);
extern void COALCallFunction(coal::VM *vm, const char *name);

// Needed for color functions
extern ImageData *ReadAsEpiBlock(Image *rim);

extern ImageData *RGBFromPalettised(ImageData *src, const uint8_t *palette, int opacity);

Player *ui_hud_who = nullptr;

extern Player *ui_player_who;

extern std::string current_map_title;

extern bool erraticism_active;

static int   ui_hud_automap_flags[2]; // 0 = disabled, 1 = enabled
static float ui_hud_automap_zoom;

//------------------------------------------------------------------------

RGBAColor COALVectorToColor(double *v)
{
    if (v[0] < 0)
        return kRGBANoValue;

    int r = HMM_Clamp(0, (int)v[0], 255);
    int g = HMM_Clamp(0, (int)v[1], 255);
    int b = HMM_Clamp(0, (int)v[2], 255);

    RGBAColor rgb = epi::MakeRGBA(r, g, b);

    // ensure we don't get the "no color" value by mistake
    if (rgb == kRGBANoValue)
        rgb ^= 0x00010100;

    return rgb;
}

//------------------------------------------------------------------------
//  HUD MODULE
//------------------------------------------------------------------------

// hud.coord_sys(w, h)
//
static void HD_coord_sys(coal::VM *vm, int argc)
{
    (void)argc;

    int w = (int)*vm->AccessParam(0);
    int h = (int)*vm->AccessParam(1);

    if (w < 64 || h < 64)
        FatalError("Bad hud.coord_sys size: %dx%d\n", w, h);

    HUDSetCoordinateSystem(w, h);

    COALSetFloat(ui_vm, "hud", "x_left", hud_x_left);
    COALSetFloat(ui_vm, "hud", "x_right", hud_x_right);
}

// hud.game_mode()
//
static void HD_game_mode(coal::VM *vm, int argc)
{
    (void)argc;

    if (InDeathmatch())
        vm->ReturnString("dm");
    else if (InCooperativeMatch())
        vm->ReturnString("coop");
    else
        vm->ReturnString("sp");
}

// hud.game_name()
//
static void HD_game_name(coal::VM *vm, int argc)
{
    (void)argc;

    GameDefinition *g = current_map->episode_;
    EPI_ASSERT(g);

    vm->ReturnString(g->name_.c_str());
}

// hud.game_skill()
// Lobo: May 2024
static void HD_game_skill(coal::VM *vm, int argc)
{
    (void)argc;

    vm->ReturnFloat((double)game_skill);
}

// hud.map_name()
//
static void HD_map_name(coal::VM *vm, int argc)
{
    (void)argc;

    vm->ReturnString(current_map->name_.c_str());
}

// hud.map_title()
//
static void HD_map_title(coal::VM *vm, int argc)
{
    (void)argc;

    vm->ReturnString(current_map_title.c_str());
}

// hud.map_author()
//
static void HD_map_author(coal::VM *vm, int argc)
{
    (void)argc;

    vm->ReturnString(current_map->author_.c_str());
}

// hud.which_hud()
//
static void HD_which_hud(coal::VM *vm, int argc)
{
    (void)argc;

    vm->ReturnFloat((double)screen_hud);
}

// hud.check_automap()
//
static void HD_check_automap(coal::VM *vm, int argc)
{
    (void)argc;

    vm->ReturnFloat(automap_active ? 1 : 0);
}

// hud.get_time()
//
static void HD_get_time(coal::VM *vm, int argc)
{
    (void)argc;

    int time = GetTime();
    vm->ReturnFloat((double)time);
}

// hud.text_font(name)
//
static void HD_text_font(coal::VM *vm, int argc)
{
    (void)argc;

    const char *font_name = vm->AccessParamString(0);

    FontDefinition *DEF = fontdefs.Lookup(font_name);
    if (!DEF)
        return;

    Font *font = hud_fonts.Lookup(DEF);
    if (!font)
        return;

    HUDSetFont(font);
}

// hud.text_color(rgb)
//
static void HD_text_color(coal::VM *vm, int argc)
{
    (void)argc;

    double *v = vm->AccessParam(0);

    RGBAColor color = COALVectorToColor(v);

    HUDSetTextColor(color);
}

// hud.set_scale(value)
//
static void HD_set_scale(coal::VM *vm, int argc)
{
    (void)argc;

    float scale = *vm->AccessParam(0);

    if (scale <= 0)
        FatalError("hud.set_scale: Bad scale value: %1.3f\n", scale);

    HUDSetScale(scale);
}

// hud.set_alpha(value)
//
static void HD_set_alpha(coal::VM *vm, int argc)
{
    (void)argc;

    float alpha = *vm->AccessParam(0);

    HUDSetAlpha(alpha);
}

// hud.get_text_width(String,Size)
//
static void HD_get_text_width(coal::VM *vm, int argc)
{
    (void)argc;

    const char *str  = vm->AccessParamString(0);
    float       size = *vm->AccessParam(1);

    float TheWidth = 0;
    /*
        TheWidth = HUDFontWidthNew(size ? *size : 0);

        if (!str)
            TheWidth = 0;
        else
        {
            // get the length of the line
            int len = 0;
            while (str[len] && str[len] != '\n')
                len++;
            TheWidth *= len;
        }
    */
    TheWidth = HUDStringWidthNew(str, size);
    vm->ReturnFloat(TheWidth);
}

// hud.solid_box(x, y, w, h, color)
//
static void HD_solid_box(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);
    float w = *vm->AccessParam(2);
    float h = *vm->AccessParam(3);

    RGBAColor rgb = COALVectorToColor(vm->AccessParam(4));

    HUDSolidBox(x, y, x + w, y + h, rgb);
}

// hud.solid_line(x1, y1, x2, y2, color)
//
static void HD_solid_line(coal::VM *vm, int argc)
{
    (void)argc;

    float x1 = *vm->AccessParam(0);
    float y1 = *vm->AccessParam(1);
    float x2 = *vm->AccessParam(2);
    float y2 = *vm->AccessParam(3);

    RGBAColor rgb = COALVectorToColor(vm->AccessParam(4));

    HUDSolidLine(x1, y1, x2, y2, rgb);
}

// hud.thin_box(x, y, w, h, color)
//
static void HD_thin_box(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);
    float w = *vm->AccessParam(2);
    float h = *vm->AccessParam(3);

    RGBAColor rgb = COALVectorToColor(vm->AccessParam(4));

    HUDThinBox(x, y, x + w, y + h, rgb);
}

// hud.gradient_box(x, y, w, h, TL, BL, TR, BR)
//
static void HD_gradient_box(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);
    float w = *vm->AccessParam(2);
    float h = *vm->AccessParam(3);

    RGBAColor cols[4];

    cols[0] = COALVectorToColor(vm->AccessParam(4));
    cols[1] = COALVectorToColor(vm->AccessParam(5));
    cols[2] = COALVectorToColor(vm->AccessParam(6));
    cols[3] = COALVectorToColor(vm->AccessParam(7));

    HUDGradientBox(x, y, x + w, y + h, cols);
}

// hud.draw_image(x, y, name, [noOffset])
// if we specify noOffset then it ignores
// X and Y offsets from doom or images.ddf
//
static void HD_draw_image(coal::VM *vm, int argc)
{
    (void)argc;

    float       x    = *vm->AccessParam(0);
    float       y    = *vm->AccessParam(1);
    const char *name = vm->AccessParamString(2);

    const Image *img = ImageLookup(name, kImageNamespaceGraphic);

    double *noOffset = vm->AccessParam(3);

    if (img)
    {
        if (noOffset)
            HUDDrawImageNoOffset(x, y, img);
        else
            HUDDrawImage(x, y, img);
    }
}

// Dasho 2022: Same as above but adds x/y texcoord scrolling
// hud.scroll_image(x, y, name, sx, sy, [noOffset])
//
static void HD_scroll_image(coal::VM *vm, int argc)
{
    (void)argc;

    float       x    = *vm->AccessParam(0);
    float       y    = *vm->AccessParam(1);
    const char *name = vm->AccessParamString(2);
    float       sx   = *vm->AccessParam(3);
    float       sy   = *vm->AccessParam(4);

    const Image *img      = ImageLookup(name, kImageNamespaceGraphic);
    double      *noOffset = vm->AccessParam(5);

    if (img)
    {
        if (noOffset)
            HUDScrollImageNoOffset(x, y, img, -sx,
                                   -sy); // Invert sx/sy so that user can enter positive X for
                                         // right and positive Y for up
        else
            HUDScrollImage(x, y, img, -sx,
                           -sy);         // Invert sx/sy so that user can enter
                                         // positive X for right and positive Y for up
    }
}

// hud.stretch_image(x, y, w, h, name, [noOffset])
// if we specify noOffset then it ignores
// X and Y offsets from doom or images.ddf
//
static void HD_stretch_image(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);
    float w = *vm->AccessParam(2);
    float h = *vm->AccessParam(3);

    const char *name = vm->AccessParamString(4);

    const Image *img      = ImageLookup(name, kImageNamespaceGraphic);
    double      *noOffset = vm->AccessParam(5);

    if (img)
    {
        if (noOffset)
            HUDStretchImageNoOffset(x, y, w, h, img, 0.0, 0.0);
        else
            HUDStretchImage(x, y, w, h, img, 0.0, 0.0);
    }
}

// hud.tile_image(x, y, w, h, name, offset_x, offset_y)
//
static void HD_tile_image(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);
    float w = *vm->AccessParam(2);
    float h = *vm->AccessParam(3);

    const char *name = vm->AccessParamString(4);

    float offset_x = *vm->AccessParam(5);
    float offset_y = *vm->AccessParam(6);

    const Image *img = ImageLookup(name, kImageNamespaceTexture);

    if (img)
    {
        HUDTileImage(x, y, w, h, img, offset_x, offset_y);
    }
}

// hud.draw_text(x, y, str, [size])
//
static void HD_draw_text(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);

    const char *str = vm->AccessParamString(2);

    double *size = vm->AccessParam(3);

    HUDDrawText(x, y, str, size ? *size : 0);
}

// hud.draw_num2(x, y, len, num, [size])
//
static void HD_draw_num2(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);

    int len = (int)*vm->AccessParam(2);
    int num = (int)*vm->AccessParam(3);

    double *size = vm->AccessParam(4);

    if (len < 1 || len > 20)
        FatalError("hud.draw_number: bad field length: %d\n", len);

    bool is_neg = false;

    if (num < 0 && len > 1)
    {
        is_neg = true;
        len--;
    }

    // build the integer backwards

    char  buffer[200];
    char *pos = &buffer[sizeof(buffer) - 4];

    *--pos = 0;

    if (num == 0)
    {
        *--pos = '0';
    }
    else
    {
        for (; num > 0 && len > 0; num /= 10, len--)
            *--pos = '0' + (num % 10);

        if (is_neg)
            *--pos = '-';
    }

    HUDSetAlignment(+1, -1);
    HUDDrawText(x, y, pos, size ? *size : 0);
    HUDSetAlignment();
}

// Lobo November 2021:
// hud.draw_number(x, y, len, num, align_right, [size])
//
static void HD_draw_number(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);

    int     len         = (int)*vm->AccessParam(2);
    int     num         = (int)*vm->AccessParam(3);
    int     align_right = (int)*vm->AccessParam(4);
    double *size        = vm->AccessParam(5);

    if (len < 1 || len > 20)
        FatalError("hud.draw_number: bad field length: %d\n", len);

    bool is_neg = false;

    if (num < 0 && len > 1)
    {
        is_neg = true;
        len--;
    }

    // build the integer backwards

    char  buffer[200];
    char *pos = &buffer[sizeof(buffer) - 4];

    *--pos = 0;

    if (num == 0)
    {
        *--pos = '0';
    }
    else
    {
        for (; num > 0 && len > 0; num /= 10, len--)
            *--pos = '0' + (num % 10);

        if (is_neg)
            *--pos = '-';
    }

    if (align_right == 0)
    {
        HUDDrawText(x, y, pos, size ? *size : 0);
    }
    else
    {
        HUDSetAlignment(+1, -1);
        HUDDrawText(x, y, pos, size ? *size : 0);
        HUDSetAlignment();
    }
}

// hud.game_paused()
//
static void HD_game_paused(coal::VM *vm, int argc)
{
    (void)argc;

    if (paused || menu_active || rts_menu_active || time_stop_active || erraticism_active)
    {
        vm->ReturnFloat(1);
    }
    else
    {
        vm->ReturnFloat(0);
    }
}

// hud.erraticism_active()
//
static void HD_erraticism_active(coal::VM *vm, int argc)
{
    (void)argc;

    if (erraticism_active)
    {
        vm->ReturnFloat(1);
    }
    else
    {
        vm->ReturnFloat(0);
    }
}

// hud.time_stop_active()
//
static void HD_time_stop_active(coal::VM *vm, int argc)
{
    (void)argc;

    if (time_stop_active)
    {
        vm->ReturnFloat(1);
    }
    else
    {
        vm->ReturnFloat(0);
    }
}

// hud.render_world(x, y, w, h, [flags])
//
static void HD_render_world(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);
    float w = *vm->AccessParam(2);
    float h = *vm->AccessParam(3);

    double *flags = vm->AccessParam(4);

    HUDRenderWorld(x, y, w, h, ui_hud_who->map_object_, flags ? (int)*flags : 0);
}

// hud.render_automap(x, y, w, h, [flags])
//
static void HD_render_automap(coal::VM *vm, int argc)
{
    (void)argc;

    float x = *vm->AccessParam(0);
    float y = *vm->AccessParam(1);
    float w = *vm->AccessParam(2);
    float h = *vm->AccessParam(3);

    double *flags = vm->AccessParam(4);

    int   old_state;
    float old_zoom;

    AutomapGetState(&old_state, &old_zoom);

    int new_state = old_state;
    new_state &= ~ui_hud_automap_flags[0];
    new_state |= ui_hud_automap_flags[1];

    float new_zoom = old_zoom;
    if (ui_hud_automap_zoom > 0.1)
        new_zoom = ui_hud_automap_zoom;

    AutomapSetState(new_state, new_zoom);

    HUDRenderAutomap(x, y, w, h, ui_hud_who->map_object_, flags ? (int)*flags : 0);

    AutomapSetState(old_state, old_zoom);
}

// hud.automap_color(which, color)
//
static void HD_automap_color(coal::VM *vm, int argc)
{
    (void)argc;

    int which = (int)*vm->AccessParam(0);

    if (which < 1 || which > kTotalAutomapColors)
        FatalError("hud.automap_color: bad color number: %d\n", which);

    which--;

    RGBAColor rgb = COALVectorToColor(vm->AccessParam(1));

    AutomapSetColor(which, rgb);
}

// hud.automap_option(which, value)
//
static void HD_automap_option(coal::VM *vm, int argc)
{
    (void)argc;

    int which = (int)*vm->AccessParam(0);
    int value = (int)*vm->AccessParam(1);

    if (which < 1 || which > 7)
        FatalError("hud.automap_color: bad color number: %d\n", which);

    which--;

    if (value <= 0)
        ui_hud_automap_flags[0] |= (1 << which);
    else
        ui_hud_automap_flags[1] |= (1 << which);
}

// hud.automap_zoom(value)
//
static void HD_automap_zoom(coal::VM *vm, int argc)
{
    (void)argc;

    float zoom = *vm->AccessParam(0);

    // impose a very broad limit
    ui_hud_automap_zoom = HMM_Clamp(0.2f, zoom, 100.0f);
}

// hud.automap_player_arrow(type)
//
static void HD_automap_player_arrow(coal::VM *vm, int argc)
{
    (void)argc;

    int arrow = (int)*vm->AccessParam(0);

    AutomapSetArrow((AutomapArrowStyle)arrow);
}

// hud.set_render_who(index)
//
static void HD_set_render_who(coal::VM *vm, int argc)
{
    (void)argc;

    int index = (int)*vm->AccessParam(0);

    if (index < 0 || index >= total_players)
        FatalError("hud.set_render_who: bad index value: %d (numplayers=%d)\n", index, total_players);

    if (index == 0)
    {
        ui_hud_who = players[console_player];
        return;
    }

    int who = display_player;

    for (; index > 1; index--)
    {
        do
        {
            who = (who + 1) % kMaximumPlayers;
        } while (players[who] == nullptr);
    }

    ui_hud_who = players[who];
}

// hud.play_sound(name)
//
static void HD_play_sound(coal::VM *vm, int argc)
{
    (void)argc;

    const char *name = vm->AccessParamString(0);

    SoundEffect *fx = sfxdefs.GetEffect(name);

    if (fx)
        StartSoundEffect(fx);
    else
        LogWarning("hud.play_sound: unknown sfx '%s'\n", name);
}

// hud.screen_aspect()
//
static void HD_screen_aspect(coal::VM *vm, int argc)
{
    (void)argc;

    float TempAspect = std::ceil(pixel_aspect_ratio.f_ * 100.0) / 100.0;

    vm->ReturnFloat(TempAspect);
}

static void HD_get_average_color(coal::VM *vm, int argc)
{
    (void)argc;

    double         rgb[3];
    const char    *name         = vm->AccessParamString(0);
    double        *from_x       = vm->AccessParam(1);
    double        *to_x         = vm->AccessParam(2);
    double        *from_y       = vm->AccessParam(3);
    double        *to_y         = vm->AccessParam(4);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = ReadAsEpiBlock((Image *)tmp_img_c);
    if (tmp_img_data->depth_ == 1)
    {
        ImageData *rgb_img_data = RGBFromPalettised(tmp_img_data, what_palette, tmp_img_c->opacity_);
        delete tmp_img_data;
        tmp_img_data = rgb_img_data;
    }
    RGBAColor col = tmp_img_data->AverageColor(from_x ? *from_x : -1, to_x ? *to_x : 1000000, from_y ? *from_y : -1,
                                               to_y ? *to_y : 1000000);
    rgb[0] = epi::GetRGBARed(col);
    rgb[1] = epi::GetRGBAGreen(col);
    rgb[2] = epi::GetRGBABlue(col);
    delete tmp_img_data;
    vm->ReturnVector(rgb);
}

static void HD_get_lightest_color(coal::VM *vm, int argc)
{
    (void)argc;

    double         rgb[3];
    const char    *name         = vm->AccessParamString(0);
    double        *from_x       = vm->AccessParam(1);
    double        *to_x         = vm->AccessParam(2);
    double        *from_y       = vm->AccessParam(3);
    double        *to_y         = vm->AccessParam(4);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = ReadAsEpiBlock((Image *)tmp_img_c);
    if (tmp_img_data->depth_ == 1)
    {
        ImageData *rgb_img_data = RGBFromPalettised(tmp_img_data, what_palette, tmp_img_c->opacity_);
        delete tmp_img_data;
        tmp_img_data = rgb_img_data;
    }
    RGBAColor col = tmp_img_data->LightestColor(from_x ? *from_x : -1, to_x ? *to_x : 1000000, from_y ? *from_y : -1,
                                                to_y ? *to_y : 1000000);
    rgb[0] = epi::GetRGBARed(col);
    rgb[1] = epi::GetRGBAGreen(col);
    rgb[2] = epi::GetRGBABlue(col);
    delete tmp_img_data;
    vm->ReturnVector(rgb);
}

static void HD_get_darkest_color(coal::VM *vm, int argc)
{
    (void)argc;

    double         rgb[3];
    const char    *name         = vm->AccessParamString(0);
    double        *from_x       = vm->AccessParam(1);
    double        *to_x         = vm->AccessParam(2);
    double        *from_y       = vm->AccessParam(3);
    double        *to_y         = vm->AccessParam(4);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = ReadAsEpiBlock((Image *)tmp_img_c);
    if (tmp_img_data->depth_ == 1)
    {
        ImageData *rgb_img_data = RGBFromPalettised(tmp_img_data, what_palette, tmp_img_c->opacity_);
        delete tmp_img_data;
        tmp_img_data = rgb_img_data;
    }
    RGBAColor col = tmp_img_data->DarkestColor(from_x ? *from_x : -1, to_x ? *to_x : 1000000, from_y ? *from_y : -1,
                                               to_y ? *to_y : 1000000);
    rgb[0] = epi::GetRGBARed(col);
    rgb[1] = epi::GetRGBAGreen(col);
    rgb[2] = epi::GetRGBABlue(col);
    delete tmp_img_data;
    vm->ReturnVector(rgb);
}

static void HD_get_average_hue(coal::VM *vm, int argc)
{
    (void)argc;

    double         rgb[3];
    const char    *name         = vm->AccessParamString(0);
    double        *from_x       = vm->AccessParam(1);
    double        *to_x         = vm->AccessParam(2);
    double        *from_y       = vm->AccessParam(3);
    double        *to_y         = vm->AccessParam(4);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = ReadAsEpiBlock((Image *)tmp_img_c);
    if (tmp_img_data->depth_ == 1)
    {
        ImageData *rgb_img_data = RGBFromPalettised(tmp_img_data, what_palette, tmp_img_c->opacity_);
        delete tmp_img_data;
        tmp_img_data = rgb_img_data;
    }

    RGBAColor hue = tmp_img_data->AverageHue(from_x ? *from_x : -1, to_x ? *to_x : 1000000, from_y ? *from_y : -1,
                             to_y ? *to_y : 1000000);
    delete tmp_img_data;
    rgb[0] = epi::GetRGBARed(hue);
    rgb[1] = epi::GetRGBAGreen(hue);
    rgb[2] = epi::GetRGBABlue(hue);
    vm->ReturnVector(rgb);
}

// These two aren't really needed anymore with the AverageColor rework, but
// keeping them in case COALHUDS in the wild use them - Dasho
static void HD_get_average_top_border_color(coal::VM *vm, int argc)
{
    (void)argc;

    double         rgb[3];
    const char    *name         = vm->AccessParamString(0);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = ReadAsEpiBlock((Image *)tmp_img_c);
    if (tmp_img_data->depth_ == 1)
    {
        ImageData *rgb_img_data = RGBFromPalettised(tmp_img_data, what_palette, tmp_img_c->opacity_);
        delete tmp_img_data;
        tmp_img_data = rgb_img_data;
    }
    RGBAColor col = tmp_img_data->AverageColor(0, tmp_img_c->actual_width_, tmp_img_c->actual_height_ - 1,
                                               tmp_img_c->actual_height_);
    rgb[0]        = epi::GetRGBARed(col);
    rgb[1]        = epi::GetRGBAGreen(col);
    rgb[2]        = epi::GetRGBABlue(col);
    delete tmp_img_data;
    vm->ReturnVector(rgb);
}
static void HD_get_average_bottom_border_color(coal::VM *vm, int argc)
{
    (void)argc;

    double         rgb[3];
    const char    *name         = vm->AccessParamString(0);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = ReadAsEpiBlock((Image *)tmp_img_c);
    if (tmp_img_data->depth_ == 1)
    {
        ImageData *rgb_img_data = RGBFromPalettised(tmp_img_data, what_palette, tmp_img_c->opacity_);
        delete tmp_img_data;
        tmp_img_data = rgb_img_data;
    }
    RGBAColor col = tmp_img_data->AverageColor(0, tmp_img_c->actual_width_, 0, 1);
    rgb[0]        = epi::GetRGBARed(col);
    rgb[1]        = epi::GetRGBAGreen(col);
    rgb[2]        = epi::GetRGBABlue(col);
    delete tmp_img_data;
    vm->ReturnVector(rgb);
}

// hud.rts_enable(tag)
//
static void HD_rts_enable(coal::VM *vm, int argc)
{
    (void)argc;

    std::string name = vm->AccessParamString(0);

    if (!name.empty())
        ScriptEnableByTag(nullptr, name.c_str(), false);
}

// hud.rts_disable(tag)
//
static void HD_rts_disable(coal::VM *vm, int argc)
{
    (void)argc;

    std::string name = vm->AccessParamString(0);

    if (!name.empty())
        ScriptEnableByTag(nullptr, name.c_str(), true);
}

// hud.rts_isactive(tag)
//
static void HD_rts_isactive(coal::VM *vm, int argc)
{
    (void)argc;

    std::string name = vm->AccessParamString(0);

    if (!name.empty())
    {
        if (CheckActiveScriptByTag(nullptr, name.c_str()))
            vm->ReturnFloat(1);
        else
            vm->ReturnFloat(0);
    }
}

// hud.get_image_width(name)
//
static void HD_get_image_width(coal::VM *vm, int argc)
{
    (void)argc;
    const char *name = vm->AccessParamString(0);

    const Image *img = ImageLookup(name, kImageNamespaceGraphic, kImageLookupNull);

    if (img)
    {
        vm->ReturnFloat(HUDGetImageWidth(img));
    }
    else
    {
        vm->ReturnFloat(0);
    }
}

// hud.get_image_height(name)
//
static void HD_get_image_height(coal::VM *vm, int argc)
{
    (void)argc;
    const char *name = vm->AccessParamString(0);

    const Image *img = ImageLookup(name, kImageNamespaceGraphic, kImageLookupNull);

    if (img)
    {
        vm->ReturnFloat(HUDGetImageHeight(img));
    }
    else
    {
        vm->ReturnFloat(0);
    }
}

// hud.lookup_LDF(languageEntry)
//
static void HD_lookup_LDF(coal::VM *vm, int argc)
{
    (void)argc;

    const char *name = vm->AccessParamString(0);

    vm->ReturnString(language[name]);
}

//------------------------------------------------------------------------
// HUD Functions
//------------------------------------------------------------------------

void COALRegisterHUD()
{
    // query functions
    ui_vm->AddNativeFunction("hud.game_mode", HD_game_mode);
    ui_vm->AddNativeFunction("hud.game_name", HD_game_name);
    ui_vm->AddNativeFunction("hud.map_name", HD_map_name);
    ui_vm->AddNativeFunction("hud.map_title", HD_map_title);
    ui_vm->AddNativeFunction("hud.map_author", HD_map_author);

    ui_vm->AddNativeFunction("hud.which_hud", HD_which_hud);
    ui_vm->AddNativeFunction("hud.check_automap", HD_check_automap);
    ui_vm->AddNativeFunction("hud.get_time", HD_get_time);

    // set-state functions
    ui_vm->AddNativeFunction("hud.coord_sys", HD_coord_sys);

    ui_vm->AddNativeFunction("hud.text_font", HD_text_font);
    ui_vm->AddNativeFunction("hud.text_color", HD_text_color);
    ui_vm->AddNativeFunction("hud.set_scale", HD_set_scale);
    ui_vm->AddNativeFunction("hud.set_alpha", HD_set_alpha);

    ui_vm->AddNativeFunction("hud.set_render_who", HD_set_render_who);
    ui_vm->AddNativeFunction("hud.automap_color", HD_automap_color);
    ui_vm->AddNativeFunction("hud.automap_option", HD_automap_option);
    ui_vm->AddNativeFunction("hud.automap_zoom", HD_automap_zoom);
    ui_vm->AddNativeFunction("hud.automap_player_arrow", HD_automap_player_arrow);

    // drawing functions
    ui_vm->AddNativeFunction("hud.solid_box", HD_solid_box);
    ui_vm->AddNativeFunction("hud.solid_line", HD_solid_line);
    ui_vm->AddNativeFunction("hud.thin_box", HD_thin_box);
    ui_vm->AddNativeFunction("hud.gradient_box", HD_gradient_box);

    ui_vm->AddNativeFunction("hud.draw_image", HD_draw_image);
    ui_vm->AddNativeFunction("hud.stretch_image", HD_stretch_image);
    ui_vm->AddNativeFunction("hud.scroll_image", HD_scroll_image);

    ui_vm->AddNativeFunction("hud.tile_image", HD_tile_image);
    ui_vm->AddNativeFunction("hud.draw_text", HD_draw_text);
    ui_vm->AddNativeFunction("hud.draw_num2", HD_draw_num2);

    ui_vm->AddNativeFunction("hud.draw_number", HD_draw_number);
    ui_vm->AddNativeFunction("hud.game_paused", HD_game_paused);
    ui_vm->AddNativeFunction("hud.erraticism_active", HD_erraticism_active);
    ui_vm->AddNativeFunction("hud.time_stop_active", HD_time_stop_active);
    ui_vm->AddNativeFunction("hud.screen_aspect", HD_screen_aspect);

    ui_vm->AddNativeFunction("hud.render_world", HD_render_world);
    ui_vm->AddNativeFunction("hud.render_automap", HD_render_automap);

    // sound functions
    ui_vm->AddNativeFunction("hud.play_sound", HD_play_sound);

    // image color functions
    ui_vm->AddNativeFunction("hud.get_average_color", HD_get_average_color);
    ui_vm->AddNativeFunction("hud.get_average_top_border_color", HD_get_average_top_border_color);
    ui_vm->AddNativeFunction("hud.get_average_bottom_border_color", HD_get_average_bottom_border_color);
    ui_vm->AddNativeFunction("hud.get_lightest_color", HD_get_lightest_color);
    ui_vm->AddNativeFunction("hud.get_darkest_color", HD_get_darkest_color);
    ui_vm->AddNativeFunction("hud.get_average_hue", HD_get_average_hue);

    ui_vm->AddNativeFunction("hud.rts_enable", HD_rts_enable);
    ui_vm->AddNativeFunction("hud.rts_disable", HD_rts_disable);
    ui_vm->AddNativeFunction("hud.rts_isactive", HD_rts_isactive);

    ui_vm->AddNativeFunction("hud.get_image_width", HD_get_image_width);
    ui_vm->AddNativeFunction("hud.get_image_height", HD_get_image_height);
    ui_vm->AddNativeFunction("hud.lookup_LDF", HD_lookup_LDF);

    ui_vm->AddNativeFunction("hud.get_text_width", HD_get_text_width);

    ui_vm->AddNativeFunction("hud.game_skill", HD_game_skill);
}

void COALNewGame(void)
{
    COALCallFunction(ui_vm, "new_game");
}

void COALLoadGame(void)
{
    // Need to set these to prevent nullptr references if using any player.xxx
    // in the load_level hook
    ui_hud_who    = players[display_player];
    ui_player_who = players[display_player];

    COALCallFunction(ui_vm, "load_game");
}

void COALSaveGame(void)
{
    COALCallFunction(ui_vm, "save_game");
}

void COALBeginLevel(void)
{
    // Need to set these to prevent nullptr references if using player.xxx in
    // the begin_level hook
    ui_hud_who    = players[display_player];
    ui_player_who = players[display_player];
    COALCallFunction(ui_vm, "begin_level");
}

void COALEndLevel(void)
{
    COALCallFunction(ui_vm, "end_level");
}

void COALRunHUD(void)
{
    HUDReset();

    ui_hud_who    = players[display_player];
    ui_player_who = players[display_player];

    ui_hud_automap_flags[0] = 0;
    ui_hud_automap_flags[1] = 0;
    ui_hud_automap_zoom     = -1;

    COALCallFunction(ui_vm, "draw_all");

    COALSetVector(ui_vm, "player", "inventory_event_handler", 0, 0, 0);

    HUDReset();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
