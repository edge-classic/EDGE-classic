
#include "am_map.h"
#include "dm_state.h"
#include "e_player.h"
#include "font.h"
#include "g_game.h"
#include "hu_draw.h"
#include "i_system.h"
#include "lua_compat.h"
#include "r_colormap.h"
#include "r_misc.h"
#include "rad_trig.h"
#include "s_sound.h"
#include "w_wad.h"

extern ConsoleVariable double_framerate;
extern bool            erraticism_active;
extern std::string     current_map_title;

extern ImageData *ReadAsEpiBlock(Image *rim);
extern ImageData *RgbFromPalettised(ImageData *src, const uint8_t *palette,
                                    int opacity);

extern Player *ui_player_who;

extern Player *ui_hud_who;

static int   ui_hud_automap_flags[2];  // 0 = disabled, 1 = enabled
static float ui_hud_automap_zoom;

static RGBAColor HD_VectorToColor(const HMM_Vec3 &v)
{
    if (v.X < 0) return kRGBANoValue;

    int r = HMM_Clamp(0, (int)v.X, 255);
    int g = HMM_Clamp(0, (int)v.Y, 255);
    int b = HMM_Clamp(0, (int)v.Z, 255);

    RGBAColor rgb = epi::MakeRGBA(r, g, b);

    // ensure we don't get the "no color" value by mistake
    if (rgb == kRGBANoValue) rgb ^= 0x00010100;

    return rgb;
}

//------------------------------------------------------------------------
//  HUD MODULE
//------------------------------------------------------------------------

// hud.coord_sys(w, h)
//
static int HD_coord_sys(lua_State *L)
{
    double w = luaL_checknumber(L, 1);
    double h = luaL_checknumber(L, 2);

    if (w < 64 || h < 64) FatalError("Bad hud.coord_sys size: %fx%f\n", w, h);

    HudSetCoordinateSystem(w, h);

    LuaSetFloat(L, "hud", "x_left", hud_x_left);
    LuaSetFloat(L, "hud", "x_right", hud_x_right);

    return 0;
}

// hud.game_mode()
//
static int HD_game_mode(lua_State *L)
{
    if (InDeathmatch())
        lua_pushstring(L, "dm");
    else if (InCooperativeMatch())
        lua_pushstring(L, "coop");
    else
        lua_pushstring(L, "sp");

    return 1;
}

// hud.game_name()
//
static int HD_game_name(lua_State *L)
{
    GameDefinition *g = current_map->episode_;
    SYS_ASSERT(g);

    lua_pushstring(L, g->name_.c_str());

    return 1;
}

// hud.game_skill()
// Lobo: December 2023
static int HD_game_skill(lua_State *L)
{
    lua_pushinteger(L, game_skill);
    return 1;
}

// hud.map_name()
//
static int HD_map_name(lua_State *L)
{
    lua_pushstring(L, current_map->name_.c_str());
    return 1;
}

// hud.map_title()
//
static int HD_map_title(lua_State *L)
{
    lua_pushstring(L, current_map_title.c_str());
    return 1;
}

// hud.map_author()
//
static int HD_map_author(lua_State *L)
{
    lua_pushstring(L, current_map->author_.c_str());
    return 1;
}

// hud.which_hud()
//
static int HD_which_hud(lua_State *L)
{
    lua_pushinteger(L, (double)screen_hud);
    return 1;
}

// hud.check_automap()
//
static int HD_check_automap(lua_State *L)
{
    lua_pushboolean(L, automap_active ? 1 : 0);
    return 1;
}

// hud.get_time()
//
static int HD_get_time(lua_State *L)
{
    int time = GetTime() / (double_framerate.d_ ? 2 : 1);
    lua_pushnumber(L, (double)time);
    return 1;
}

// hud.text_font(name)
//
static int HD_text_font(lua_State *L)
{
    const char *font_name = luaL_checkstring(L, 1);

    FontDefinition *DEF = fontdefs.Lookup(font_name);
    SYS_ASSERT(DEF);

    if (!DEF) FatalError("hud.text_font: Bad font name: %s\n", font_name);

    Font *font = hud_fonts.Lookup(DEF);
    SYS_ASSERT(font);

    if (!font) FatalError("hud.text_font: Bad font name: %s\n", font_name);

    HudSetFont(font);

    return 0;
}

// hud.text_color(rgb)
//
static int HD_text_color(lua_State *L)
{
    RGBAColor color = HD_VectorToColor(LuaCheckVector3(L, 1));

    HudSetTextColor(color);

    return 0;
}

// hud.set_scale(value)
//
static int HD_set_scale(lua_State *L)
{
    float scale = luaL_checknumber(L, 1);

    if (scale <= 0)
        FatalError("hud.set_scale: Bad scale value: %1.3f\n", scale);

    HudSetScale(scale);

    return 0;
}

// hud.set_alpha(value)
//
static int HD_set_alpha(lua_State *L)
{
    float alpha = (float)luaL_checknumber(L, 1);

    HudSetAlpha(alpha);

    return 0;
}

// hud.solid_box(x, y, w, h, color)
//
static int HD_solid_box(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);
    float w = luaL_checknumber(L, 3);
    float h = luaL_checknumber(L, 4);

    RGBAColor rgb = HD_VectorToColor(LuaCheckVector3(L, 5));

    HudSolidBox(x, y, x + w, y + h, rgb);

    return 0;
}

// hud.solid_line(x1, y1, x2, y2, color)
//
static int HD_solid_line(lua_State *L)
{
    float x1 = luaL_checknumber(L, 1);
    float y1 = luaL_checknumber(L, 2);
    float x2 = luaL_checknumber(L, 3);
    float y2 = luaL_checknumber(L, 4);

    RGBAColor rgb = HD_VectorToColor(LuaCheckVector3(L, 5));

    HudSolidLine(x1, y1, x2, y2, rgb);

    return 0;
}

// hud.thin_box(x, y, w, h, color)
//
static int HD_thin_box(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);
    float w = luaL_checknumber(L, 3);
    float h = luaL_checknumber(L, 4);

    RGBAColor rgb = HD_VectorToColor(LuaCheckVector3(L, 5));

    HudThinBox(x, y, x + w, y + h, rgb);

    return 0;
}

// hud.gradient_box(x, y, w, h, TL, BL, TR, BR)
//
static int HD_gradient_box(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);
    float w = luaL_checknumber(L, 3);
    float h = luaL_checknumber(L, 4);

    RGBAColor cols[4];

    cols[0] = HD_VectorToColor(LuaCheckVector3(L, 5));
    cols[1] = HD_VectorToColor(LuaCheckVector3(L, 6));
    cols[2] = HD_VectorToColor(LuaCheckVector3(L, 7));
    cols[3] = HD_VectorToColor(LuaCheckVector3(L, 8));

    HudGradientBox(x, y, x + w, y + h, cols);

    return 0;
}

// hud.draw_image(x, y, name, [noOffset])
// if we specify noOffset then it ignores
// X and Y offsets from doom or images.ddf
//
static int HD_draw_image(lua_State *L)
{
    float       x    = (float)luaL_checknumber(L, 1);
    float       y    = (float)luaL_checknumber(L, 2);
    const char *name = luaL_checkstring(L, 3);

    const Image *img = ImageLookup(name, kImageNamespaceGraphic);

    int noOffset = (int)luaL_optnumber(L, 4, 0);

    if (img)
    {
        if (noOffset)
            HudDrawImageNoOffset(x, y, img);
        else
            HudDrawImage(x, y, img);
    }

    return 0;
}

// Dasho 2022: Same as above but adds x/y texcoord scrolling
// hud.scroll_image(x, y, name, sx, sy, [noOffset])
//
static int HD_scroll_image(lua_State *L)
{
    float       x    = luaL_checknumber(L, 1);
    float       y    = luaL_checknumber(L, 2);
    const char *name = luaL_checkstring(L, 3);
    float       sx   = luaL_checknumber(L, 4);
    float       sy   = luaL_checknumber(L, 5);

    const Image *img      = ImageLookup(name, kImageNamespaceGraphic);
    int          noOffset = (int)luaL_optnumber(L, 6, 0);

    if (img)
    {
        if (noOffset)
            HudScrollImageNoOffset(
                x, y, img, -sx,
                -sy);  // Invert sx/sy so that user can enter positive X for
                       // right and positive Y for up
        else
            HudScrollImage(x, y, img, -sx,
                           -sy);  // Invert sx/sy so that user can enter
                                  // positive X for right and positive Y for up
    }

    return 0;
}

// hud.stretch_image(x, y, w, h, name, [noOffset])
// if we specify noOffset then it ignores
// X and Y offsets from doom or images.ddf
//
static int HD_stretch_image(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);
    float w = luaL_checknumber(L, 3);
    float h = luaL_checknumber(L, 4);

    const char *name = luaL_checkstring(L, 5);

    const Image *img      = ImageLookup(name, kImageNamespaceGraphic);
    int          noOffset = (int)luaL_optnumber(L, 6, 0);

    if (img)
    {
        if (noOffset)
            HudStretchImageNoOffset(x, y, w, h, img, 0.0, 0.0);
        else
            HudStretchImage(x, y, w, h, img, 0.0, 0.0);
    }

    return 0;
}

// hud.tile_image(x, y, w, h, name, offset_x, offset_y)
//
static int HD_tile_image(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);
    float w = luaL_checknumber(L, 3);
    float h = luaL_checknumber(L, 4);

    const char *name = luaL_checkstring(L, 5);

    float offset_x = luaL_checknumber(L, 6);
    float offset_y = luaL_checknumber(L, 7);

    const Image *img = ImageLookup(name, kImageNamespaceTexture);

    if (img) { HudTileImage(x, y, w, h, img, offset_x, offset_y); }

    return 0;
}

// hud.draw_text(x, y, str, [size])
//
static int HD_draw_text(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);

    const char *str = luaL_checkstring(L, 3);

    double size = luaL_optnumber(L, 4, 0);

    HudDrawText(x, y, str, size);

    return 0;
}

// hud.draw_num2(x, y, len, num, [size])
//
static int HD_draw_num2(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);

    int len = (int)luaL_checknumber(L, 3);
    int num = (int)luaL_checknumber(L, 4);

    double size = luaL_optnumber(L, 5, 0);

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

    if (num == 0) { *--pos = '0'; }
    else
    {
        for (; num > 0 && len > 0; num /= 10, len--) *--pos = '0' + (num % 10);

        if (is_neg) *--pos = '-';
    }

    HudSetAlignment(+1, -1);
    HudDrawText(x, y, pos, size);
    HudSetAlignment();

    return 0;
}

// Lobo November 2021:
// hud.draw_number(x, y, len, num, align_right, [size])
//
static int HD_draw_number(lua_State *L)
{
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);

    int    len         = (int)luaL_checknumber(L, 3);
    int    num         = (int)luaL_checknumber(L, 4);
    int    align_right = (int)luaL_checknumber(L, 5);
    double size        = luaL_optnumber(L, 6, 0);

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

    if (num == 0) { *--pos = '0'; }
    else
    {
        for (; num > 0 && len > 0; num /= 10, len--) *--pos = '0' + (num % 10);

        if (is_neg) *--pos = '-';
    }

    if (align_right == 0) { HudDrawText(x, y, pos, size); }
    else
    {
        HudSetAlignment(+1, -1);
        HudDrawText(x, y, pos, size);
        HudSetAlignment();
    }

    return 0;
}

// hud.game_paused()
//
static int HD_game_paused(lua_State *L)
{
    if (paused || menu_active || rts_menu_active || time_stop_active ||
        erraticism_active)
    {
        lua_pushboolean(L, 1);
    }
    else { lua_pushboolean(L, 0); }

    return 1;
}

// hud.erraticism_active()
//
static int HD_erraticism_active(lua_State *L)
{
    if (erraticism_active) { lua_pushboolean(L, 1); }
    else { lua_pushboolean(L, 0); }

    return 1;
}

// hud.time_stop_active()
//
static int HD_time_stop_active(lua_State *L)
{
    if (time_stop_active) { lua_pushboolean(L, 1); }
    else { lua_pushboolean(L, 0); }

    return 1;
}

static int HD_render_world(lua_State *L)
{
    float x     = (float)luaL_checknumber(L, 1);
    float y     = (float)luaL_checknumber(L, 2);
    float w     = (float)luaL_checknumber(L, 3);
    float h     = (float)luaL_checknumber(L, 4);
    int   flags = (int)luaL_optnumber(L, 5, 0);

    HudRenderWorld(x, y, w, h, ui_hud_who->map_object_, flags);

    return 0;
}

// hud.render_automap(x, y, w, h, [flags])
//
static int HD_render_automap(lua_State *L)
{
    float x     = luaL_checknumber(L, 1);
    float y     = luaL_checknumber(L, 2);
    float w     = luaL_checknumber(L, 3);
    float h     = luaL_checknumber(L, 4);
    int   flags = (int)luaL_optnumber(L, 5, 0);

    int   old_state;
    float old_zoom;

    AutomapGetState(&old_state, &old_zoom);

    int new_state = old_state;
    new_state &= ~ui_hud_automap_flags[0];
    new_state |= ui_hud_automap_flags[1];

    float new_zoom = old_zoom;
    if (ui_hud_automap_zoom > 0.1) new_zoom = ui_hud_automap_zoom;

    AutomapSetState(new_state, new_zoom);

    HudRenderAutomap(x, y, w, h, ui_hud_who->map_object_, flags);

    AutomapSetState(old_state, old_zoom);

    return 0;
}

// hud.automap_color(which, color)
//
static int HD_automap_color(lua_State *L)
{
    int which = (int)luaL_checknumber(L, 1);

    if (which < 1 || which > kTotalAutomapColors)
        FatalError("hud.automap_color: bad color number: %d\n", which);

    which--;

    RGBAColor rgb = HD_VectorToColor(LuaCheckVector3(L, 2));

    AutomapSetColor(which, rgb);

    return 0;
}

// hud.automap_option(which, value)
//
static int HD_automap_option(lua_State *L)
{
    int which = (int)luaL_checknumber(L, 1);
    int value = (int)luaL_checknumber(L, 2);

    if (which < 1 || which > 7)
        FatalError("hud.automap_color: bad color number: %d\n", which);

    which--;

    if (value <= 0)
        ui_hud_automap_flags[0] |= (1 << which);
    else
        ui_hud_automap_flags[1] |= (1 << which);

    return 0;
}

// hud.automap_zoom(value)
//
static int HD_automap_zoom(lua_State *L)
{
    float zoom = luaL_checknumber(L, 1);

    // impose a very broad limit
    ui_hud_automap_zoom = HMM_Clamp(0.2f, zoom, 100.0f);

    return 0;
}

// hud.automap_player_arrow(type)
//
static int HD_automap_player_arrow(lua_State *L)
{
    int arrow = (int)luaL_checknumber(L, 1);

    AutomapSetArrow((AutomapArrowStyle)arrow);

    return 0;
}

// hud.set_render_who(index)
//
static int HD_set_render_who(lua_State *L)
{
    int index = (int)luaL_checknumber(L, 1);

    if (index < 0 || index >= total_players)
        FatalError("hud.set_render_who: bad index value: %d (numplayers=%d)\n",
                   index, total_players);

    if (index == 0)
    {
        ui_hud_who = players[console_player];
        return 0;
    }

    int who = display_player;

    for (; index > 1; index--)
    {
        do {
            who = (who + 1) % kMaximumPlayers;
        } while (players[who] == nullptr);
    }

    ui_hud_who = players[who];

    return 0;
}

// hud.play_sound(name)
//
static int HD_play_sound(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    SoundEffect *fx = sfxdefs.GetEffect(name);

    if (fx)
        StartSoundEffect(fx);
    else
        LogWarning("hud.play_sound: unknown sfx '%s'\n", name);

    return 0;
}

// hud.screen_aspect()
//
static int HD_screen_aspect(lua_State *L)
{
    lua_pushnumber(L, std::ceil(pixel_aspect_ratio.f_ * 100.0) / 100.0);
    return 1;
}

static int HD_get_average_color(lua_State *L)
{
    HMM_Vec3       rgb;
    const char    *name         = luaL_checkstring(L, 1);
    double         from_x       = luaL_optnumber(L, 2, -1);
    double         to_x         = luaL_optnumber(L, 3, 1000000);
    double         from_y       = luaL_optnumber(L, 4, -1);
    double         to_y         = luaL_optnumber(L, 5, 1000000);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = RgbFromPalettised(
        ReadAsEpiBlock((Image *)tmp_img_c), what_palette, tmp_img_c->opacity_);
    RGBAColor col = tmp_img_data->AverageColor(from_x, to_x, from_y, to_y);
    rgb.X         = epi::GetRGBARed(col);
    rgb.Y         = epi::GetRGBAGreen(col);
    rgb.Z         = epi::GetRGBABlue(col);
    delete tmp_img_data;

    LuaPushVector3(L, rgb);

    return 1;
}

static int HD_get_lightest_color(lua_State *L)
{
    HMM_Vec3       rgb;
    const char    *name         = luaL_checkstring(L, 1);
    double         from_x       = luaL_optnumber(L, 2, -1);
    double         to_x         = luaL_optnumber(L, 3, 1000000);
    double         from_y       = luaL_optnumber(L, 4, -1);
    double         to_y         = luaL_optnumber(L, 5, 1000000);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = RgbFromPalettised(
        ReadAsEpiBlock((Image *)tmp_img_c), what_palette, tmp_img_c->opacity_);
    RGBAColor col = tmp_img_data->LightestColor(from_x, to_x, from_y, to_y);
    rgb.X         = epi::GetRGBARed(col);
    rgb.Y         = epi::GetRGBAGreen(col);
    rgb.Z         = epi::GetRGBABlue(col);
    delete tmp_img_data;

    LuaPushVector3(L, rgb);
    return 1;
}

static int HD_get_darkest_color(lua_State *L)
{
    HMM_Vec3       rgb;
    const char    *name         = luaL_checkstring(L, 1);
    double         from_x       = luaL_optnumber(L, 2, -1);
    double         to_x         = luaL_optnumber(L, 3, 1000000);
    double         from_y       = luaL_optnumber(L, 4, -1);
    double         to_y         = luaL_optnumber(L, 5, 1000000);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = RgbFromPalettised(
        ReadAsEpiBlock((Image *)tmp_img_c), what_palette, tmp_img_c->opacity_);
    RGBAColor col = tmp_img_data->DarkestColor(from_x, to_x, from_y, to_y);
    rgb.X         = epi::GetRGBARed(col);
    rgb.Y         = epi::GetRGBAGreen(col);
    rgb.Z         = epi::GetRGBABlue(col);
    delete tmp_img_data;

    LuaPushVector3(L, rgb);
    return 1;
}

static int HD_get_average_hue(lua_State *L)
{
    HMM_Vec3       rgb;
    const char    *name         = luaL_checkstring(L, 1);
    double         from_x       = luaL_optnumber(L, 2, -1);
    double         to_x         = luaL_optnumber(L, 3, 1000000);
    double         from_y       = luaL_optnumber(L, 4, -1);
    double         to_y         = luaL_optnumber(L, 5, 1000000);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = RgbFromPalettised(
        ReadAsEpiBlock((Image *)tmp_img_c), what_palette, tmp_img_c->opacity_);
    uint8_t temp_rgb[3];
    tmp_img_data->AverageHue(temp_rgb, nullptr, from_x, to_x, from_y, to_y);
    rgb.X = temp_rgb[0];
    rgb.Y = temp_rgb[1];
    rgb.Z = temp_rgb[2];
    delete tmp_img_data;

    LuaPushVector3(L, rgb);
    return 1;
}

// These two aren't really needed anymore with the AverageColor rework, but
// keeping them in case COALHUDS in the wild use them - Dasho
static int HD_get_average_top_border_color(lua_State *L)
{
    HMM_Vec3       rgb;
    const char    *name         = luaL_checkstring(L, 1);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = RgbFromPalettised(
        ReadAsEpiBlock((Image *)tmp_img_c), what_palette, tmp_img_c->opacity_);
    RGBAColor col = tmp_img_data->AverageColor(0, tmp_img_c->actual_width_,
                                               tmp_img_c->actual_height_ - 1,
                                               tmp_img_c->actual_height_);
    rgb.X         = epi::GetRGBARed(col);
    rgb.Y         = epi::GetRGBAGreen(col);
    rgb.Z         = epi::GetRGBABlue(col);
    delete tmp_img_data;

    LuaPushVector3(L, rgb);
    return 1;
}
static int HD_get_average_bottom_border_color(lua_State *L)
{
    HMM_Vec3       rgb;
    const char    *name         = luaL_checkstring(L, 1);
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    const Image   *tmp_img_c    = ImageLookup(name, kImageNamespaceGraphic, 0);
    if (tmp_img_c->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(tmp_img_c->source_palette_);
    ImageData *tmp_img_data = RgbFromPalettised(
        ReadAsEpiBlock((Image *)tmp_img_c), what_palette, tmp_img_c->opacity_);
    RGBAColor col =
        tmp_img_data->AverageColor(0, tmp_img_c->actual_width_, 0, 1);
    rgb.X = epi::GetRGBARed(col);
    rgb.Y = epi::GetRGBAGreen(col);
    rgb.Z = epi::GetRGBABlue(col);
    delete tmp_img_data;

    LuaPushVector3(L, rgb);
    return 1;
}

// hud.rts_enable(tag)
//
static int HD_rts_enable(lua_State *L)
{
    std::string name = luaL_checkstring(L, 1);

    if (!name.empty()) RAD_EnableByTag(nullptr, name.c_str(), false);

    return 0;
}

// hud.rts_disable(tag)
//
static int HD_rts_disable(lua_State *L)
{
    std::string name = luaL_checkstring(L, 1);

    if (!name.empty()) RAD_EnableByTag(nullptr, name.c_str(), true);

    return 0;
}

// hud.rts_isactive(tag)
//
static int HD_rts_isactive(lua_State *L)
{
    std::string name = luaL_checkstring(L, 1);

    if (!name.empty())
    {
        if (RAD_IsActiveByTag(nullptr, name.c_str()))
            lua_pushboolean(L, 1);
        else
            lua_pushboolean(L, 0);
    }

    return 1;
}

// hud.get_image_width(name)
//
static int HD_get_image_width(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    const Image *img = ImageLookup(name, kImageNamespaceGraphic);

    if (img) { lua_pushinteger(L, HudGetImageWidth(img)); }
    else { lua_pushinteger(L, 0); }

    return 1;
}

// hud.get_image_height(name)
//
static int HD_get_image_height(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    const Image *img = ImageLookup(name, kImageNamespaceGraphic);

    if (img) { lua_pushinteger(L, HudGetImageHeight(img)); }
    else { lua_pushinteger(L, 0); }

    return 1;
}

static const luaL_Reg hudlib[] = {
    {"game_mode", HD_game_mode},
    {"game_name", HD_game_name},
    {"game_skill", HD_game_skill},
    {"map_name", HD_map_name},
    {"map_title", HD_map_title},
    {"map_author", HD_map_author},

    {"which_hud", HD_which_hud},
    {"check_automap", HD_check_automap},
    {"get_time", HD_get_time},

    // set-state functions
    {"coord_sys", HD_coord_sys},

    {"text_font", HD_text_font},
    {"text_color", HD_text_color},
    {"set_scale", HD_set_scale},
    {"set_alpha", HD_set_alpha},

    {"set_render_who", HD_set_render_who},
    {"automap_color", HD_automap_color},
    {"automap_option", HD_automap_option},
    {"automap_zoom", HD_automap_zoom},
    {"automap_player_arrow", HD_automap_player_arrow},

    // drawing functions
    {"solid_box", HD_solid_box},
    {"solid_line", HD_solid_line},
    {"thin_box", HD_thin_box},
    {"gradient_box", HD_gradient_box},

    {"draw_image", HD_draw_image},
    {"stretch_image", HD_stretch_image},
    {"scroll_image", HD_scroll_image},

    {"tile_image", HD_tile_image},
    {"draw_text", HD_draw_text},
    {"draw_num2", HD_draw_num2},

    {"draw_number", HD_draw_number},
    {"game_paused", HD_game_paused},
    {"erraticism_active", HD_erraticism_active},
    {"time_stop_active", HD_time_stop_active},
    {"screen_aspect", HD_screen_aspect},

    {"render_world", HD_render_world},
    {"render_automap", HD_render_automap},

    // sound functions
    {"play_sound", HD_play_sound},

    // image color functions
    {"get_average_color", HD_get_average_color},
    {"get_average_top_border_color", HD_get_average_top_border_color},
    {"get_average_bottom_border_color", HD_get_average_bottom_border_color},
    {"get_lightest_color", HD_get_lightest_color},
    {"get_darkest_color", HD_get_darkest_color},
    {"get_average_hue", HD_get_average_hue},

    {"rts_enable", HD_rts_enable},
    {"rts_disable", HD_rts_disable},
    {"rts_isactive", HD_rts_isactive},

    {"get_image_width", HD_get_image_width},
    {"get_image_height", HD_get_image_height},
    {nullptr, nullptr}};

static int luaopen_hud(lua_State *L)
{
    luaL_newlib(L, hudlib);
    return 1;
}

void LuaRegisterHudLibrary(lua_State *L)
{
    luaL_requiref(L, "_hud", luaopen_hud, 1);
    lua_pop(L, 1);
}

void LuaRunHud(void)
{
    HudReset();

    ui_hud_who    = players[display_player];
    ui_player_who = players[display_player];

    ui_hud_automap_flags[0] = 0;
    ui_hud_automap_flags[1] = 0;
    ui_hud_automap_zoom     = -1;

    LuaCallGlobalFunction(global_lua_state, "draw_all");

    HudReset();
}