
#include "i_defs.h"
#include "e_player.h"
#include "hu_draw.h"
#include "dm_state.h"
#include "lua_compat.h"
#include "g_game.h"
#include "s_sound.h"
#include "am_map.h"
#include "font.h"
#include "r_misc.h"
#include "w_wad.h"
#include "rad_trig.h"
#include "r_colormap.h"

extern cvar_c      r_doubleframes;
extern bool        erraticism_active;
extern std::string w_map_title;

extern epi::image_data_c *ReadAsEpiBlock(image_c *rim);
extern epi::image_data_c *R_PalettisedToRGB(epi::image_data_c *src, const byte *palette, int opacity);

extern player_t *ui_player_who;

extern player_t *ui_hud_who;

static int   ui_hud_automap_flags[2]; // 0 = disabled, 1 = enabled
static float ui_hud_automap_zoom;

static rgbcol_t HD_VectorToColor(const HMM_Vec3 &v)
{
    if (v.X < 0)
        return RGB_NO_VALUE;

    int r = CLAMP(0, (int)v.X, 255);
    int g = CLAMP(0, (int)v.Y, 255);
    int b = CLAMP(0, (int)v.Z, 255);

    rgbcol_t rgb = RGB_MAKE(r, g, b);

    // ensure we don't get the "no color" value by mistake
    if (rgb == RGB_NO_VALUE)
        rgb ^= 0x000101;

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

    if (w < 64 || h < 64)
        I_Error("Bad hud.coord_sys size: %fx%f\n", w, h);

    HUD_SetCoordSys(w, h);

    LUA_SetFloat(L, "hud", "x_left", hud_x_left);
    LUA_SetFloat(L, "hud", "x_right", hud_x_right);

    return 0;
}

// hud.game_mode()
//
static int HD_game_mode(lua_State *L)
{
    if (DEATHMATCH())
        lua_pushstring(L, "dm");
    else if (COOP_MATCH())
        lua_pushstring(L, "coop");
    else
        lua_pushstring(L, "sp");

    return 1;
}

// hud.game_name()
//
static int HD_game_name(lua_State *L)
{
    gamedef_c *g = currmap->episode;
    SYS_ASSERT(g);

    lua_pushstring(L, g->name.c_str());

    return 1;
}

// hud.game_skill()
// Lobo: December 2023
static int HD_game_skill(lua_State *L)
{
    lua_pushinteger(L, gameskill);
    return 1;
}

// hud.map_name()
//
static int HD_map_name(lua_State *L)
{
    lua_pushstring(L, currmap->name.c_str());
    return 1;
}

// hud.map_title()
//
static int HD_map_title(lua_State *L)
{
    lua_pushstring(L, w_map_title.c_str());
    return 1;
}

// hud.map_author()
//
static int HD_map_author(lua_State *L)
{
    lua_pushstring(L, currmap->author.c_str());
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
    lua_pushboolean(L, automapactive ? 1 : 0);
    return 1;
}

// hud.get_time()
//
static int HD_get_time(lua_State *L)
{
    int time = I_GetTime() / (r_doubleframes.d ? 2 : 1);
    lua_pushnumber(L, (double)time);
    return 1;
}

// hud.text_font(name)
//
static int HD_text_font(lua_State *L)
{
    const char *font_name = luaL_checkstring(L, 1);

    fontdef_c *DEF = fontdefs.Lookup(font_name);
    SYS_ASSERT(DEF);

    if (!DEF)
        I_Error("hud.text_font: Bad font name: %s\n", font_name);

    font_c *font = hu_fonts.Lookup(DEF);
    SYS_ASSERT(font);

    if (!font)
        I_Error("hud.text_font: Bad font name: %s\n", font_name);

    HUD_SetFont(font);

    return 0;
}

// hud.text_color(rgb)
//
static int HD_text_color(lua_State *L)
{
    rgbcol_t color = HD_VectorToColor(LUA_CheckVector3(L, 1));

    HUD_SetTextColor(color);

    return 0;
}

// hud.set_scale(value)
//
static int HD_set_scale(lua_State *L)
{
    float scale = luaL_checknumber(L, 1);

    if (scale <= 0)
        I_Error("hud.set_scale: Bad scale value: %1.3f\n", scale);

    HUD_SetScale(scale);

    return 0;
}

// hud.set_alpha(value)
//
static int HD_set_alpha(lua_State *L)
{
    float alpha = (float)luaL_checknumber(L, 1);

    HUD_SetAlpha(alpha);

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

    rgbcol_t rgb = HD_VectorToColor(LUA_CheckVector3(L, 5));

    HUD_SolidBox(x, y, x + w, y + h, rgb);

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

    rgbcol_t rgb = HD_VectorToColor(LUA_CheckVector3(L, 5));

    HUD_SolidLine(x1, y1, x2, y2, rgb);

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

    rgbcol_t rgb = HD_VectorToColor(LUA_CheckVector3(L, 5));

    HUD_ThinBox(x, y, x + w, y + h, rgb);

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

    rgbcol_t cols[4];

    cols[0] = HD_VectorToColor(LUA_CheckVector3(L, 5));
    cols[1] = HD_VectorToColor(LUA_CheckVector3(L, 6));
    cols[2] = HD_VectorToColor(LUA_CheckVector3(L, 7));
    cols[3] = HD_VectorToColor(LUA_CheckVector3(L, 8));

    HUD_GradientBox(x, y, x + w, y + h, cols);

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

    const image_c *img = W_ImageLookup(name, INS_Graphic);

    int noOffset = (int)luaL_optnumber(L, 4, 0);

    if (img)
    {
        if (noOffset)
            HUD_DrawImageNoOffset(x, y, img);
        else
            HUD_DrawImage(x, y, img);
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

    const image_c *img      = W_ImageLookup(name, INS_Graphic);
    int            noOffset = (int)luaL_optnumber(L, 6, 0);

    if (img)
    {
        if (noOffset)
            HUD_ScrollImageNoOffset(
                x, y, img, -sx, -sy); // Invert sx/sy so that user can enter positive X for right and positive Y for up
        else
            HUD_ScrollImage(x, y, img, -sx,
                            -sy); // Invert sx/sy so that user can enter positive X for right and positive Y for up
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

    const image_c *img      = W_ImageLookup(name, INS_Graphic);
    int            noOffset = (int)luaL_optnumber(L, 6, 0);

    if (img)
    {
        if (noOffset)
            HUD_StretchImageNoOffset(x, y, w, h, img, 0.0, 0.0);
        else
            HUD_StretchImage(x, y, w, h, img, 0.0, 0.0);
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

    const image_c *img = W_ImageLookup(name, INS_Texture);

    if (img)
    {
        HUD_TileImage(x, y, w, h, img, offset_x, offset_y);
    }

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

    HUD_DrawText(x, y, str, size);

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
        I_Error("hud.draw_number: bad field length: %d\n", len);

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

    HUD_SetAlignment(+1, -1);
    HUD_DrawText(x, y, pos, size);
    HUD_SetAlignment();

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
        I_Error("hud.draw_number: bad field length: %d\n", len);

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
        HUD_DrawText(x, y, pos, size);
    }
    else
    {
        HUD_SetAlignment(+1, -1);
        HUD_DrawText(x, y, pos, size);
        HUD_SetAlignment();
    }

    return 0;
}

// hud.game_paused()
//
static int HD_game_paused(lua_State *L)
{
    if (paused || menuactive || rts_menuactive || time_stop_active || erraticism_active)
    {
        lua_pushboolean(L, 1);
    }
    else
    {
        lua_pushboolean(L, 0);
    }

    return 1;
}

// hud.erraticism_active()
//
static int HD_erraticism_active(lua_State *L)
{
    if (erraticism_active)
    {
        lua_pushboolean(L, 1);
    }
    else
    {
        lua_pushboolean(L, 0);
    }

    return 1;
}

// hud.time_stop_active()
//
static int HD_time_stop_active(lua_State *L)
{
    if (time_stop_active)
    {
        lua_pushboolean(L, 1);
    }
    else
    {
        lua_pushboolean(L, 0);
    }

    return 1;
}

static int HD_render_world(lua_State *L)
{
    float x     = (float)luaL_checknumber(L, 1);
    float y     = (float)luaL_checknumber(L, 2);
    float w     = (float)luaL_checknumber(L, 3);
    float h     = (float)luaL_checknumber(L, 4);
    int   flags = (int)luaL_optnumber(L, 5, 0);

    HUD_RenderWorld(x, y, w, h, ui_hud_who->mo, flags);

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

    AM_GetState(&old_state, &old_zoom);

    int new_state = old_state;
    new_state &= ~ui_hud_automap_flags[0];
    new_state |= ui_hud_automap_flags[1];

    float new_zoom = old_zoom;
    if (ui_hud_automap_zoom > 0.1)
        new_zoom = ui_hud_automap_zoom;

    AM_SetState(new_state, new_zoom);

    HUD_RenderAutomap(x, y, w, h, ui_hud_who->mo, flags);

    AM_SetState(old_state, old_zoom);

    return 0;
}

// hud.automap_color(which, color)
//
static int HD_automap_color(lua_State *L)
{
    int which = (int)luaL_checknumber(L, 1);

    if (which < 1 || which > AM_NUM_COLORS)
        I_Error("hud.automap_color: bad color number: %d\n", which);

    which--;

    rgbcol_t rgb = HD_VectorToColor(LUA_CheckVector3(L, 2));

    AM_SetColor(which, rgb);

    return 0;
}

// hud.automap_option(which, value)
//
static int HD_automap_option(lua_State *L)
{
    int which = (int)luaL_checknumber(L, 1);
    int value = (int)luaL_checknumber(L, 2);

    if (which < 1 || which > 7)
        I_Error("hud.automap_color: bad color number: %d\n", which);

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
    ui_hud_automap_zoom = CLAMP(0.2f, zoom, 100.0f);

    return 0;
}

// hud.automap_player_arrow(type)
//
static int HD_automap_player_arrow(lua_State *L)
{
    int arrow = (int)luaL_checknumber(L, 1);

    AM_SetArrow((automap_arrow_e)arrow);

    return 0;
}

// hud.set_render_who(index)
//
static int HD_set_render_who(lua_State *L)
{
    int index = (int)luaL_checknumber(L, 1);

    if (index < 0 || index >= numplayers)
        I_Error("hud.set_render_who: bad index value: %d (numplayers=%d)\n", index, numplayers);

    if (index == 0)
    {
        ui_hud_who = players[consoleplayer];
        return 0;
    }

    int who = displayplayer;

    for (; index > 1; index--)
    {
        do
        {
            who = (who + 1) % MAXPLAYERS;
        } while (players[who] == NULL);
    }

    ui_hud_who = players[who];

    return 0;
}

// hud.play_sound(name)
//
static int HD_play_sound(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    sfx_t *fx = sfxdefs.GetEffect(name);

    if (fx)
        S_StartFX(fx);
    else
        I_Warning("hud.play_sound: unknown sfx '%s'\n", name);

    return 0;
}

// hud.screen_aspect()
//
static int HD_screen_aspect(lua_State *L)
{
    lua_pushnumber(L, std::ceil(v_pixelaspect.f * 100.0) / 100.0);
    return 1;
}

static int HD_get_average_color(lua_State *L)
{
    HMM_Vec3    rgb;
    const char    *name         = luaL_checkstring(L, 1);
    double         from_x       = luaL_optnumber(L, 2, -1);
    double         to_x         = luaL_optnumber(L, 3, 1000000);
    double         from_y       = luaL_optnumber(L, 4, -1);
    double         to_y         = luaL_optnumber(L, 5, 1000000);
    const byte    *what_palette = (const byte *)&playpal_data[0];
    const image_c *tmp_img_c    = W_ImageLookup(name, INS_Graphic, 0);
    if (tmp_img_c->source_palette >= 0)
        what_palette = (const byte *)W_LoadLump(tmp_img_c->source_palette);
    epi::image_data_c *tmp_img_data =
        R_PalettisedToRGB(ReadAsEpiBlock((image_c *)tmp_img_c), what_palette, tmp_img_c->opacity);
    u8_t temp_rgb[3];
    tmp_img_data->AverageColor(temp_rgb, from_x, to_x, from_y, to_y);
    rgb.X = temp_rgb[0];
    rgb.Y = temp_rgb[1];
    rgb.Z = temp_rgb[2];
    delete tmp_img_data;

    LUA_PushVector3(L, rgb);

    return 1;
}

static int HD_get_lightest_color(lua_State *L)
{
    HMM_Vec3    rgb;
    const char    *name         = luaL_checkstring(L, 1);
    double         from_x       = luaL_optnumber(L, 2, -1);
    double         to_x         = luaL_optnumber(L, 3, 1000000);
    double         from_y       = luaL_optnumber(L, 4, -1);
    double         to_y         = luaL_optnumber(L, 5, 1000000);
    const byte    *what_palette = (const byte *)&playpal_data[0];
    const image_c *tmp_img_c    = W_ImageLookup(name, INS_Graphic, 0);
    if (tmp_img_c->source_palette >= 0)
        what_palette = (const byte *)W_LoadLump(tmp_img_c->source_palette);
    epi::image_data_c *tmp_img_data =
        R_PalettisedToRGB(ReadAsEpiBlock((image_c *)tmp_img_c), what_palette, tmp_img_c->opacity);
    u8_t temp_rgb[3];
    tmp_img_data->LightestColor(temp_rgb, from_x, to_x, from_y, to_y);
    rgb.X = temp_rgb[0];
    rgb.Y = temp_rgb[1];
    rgb.Z = temp_rgb[2];
    delete tmp_img_data;

    LUA_PushVector3(L, rgb);
    return 1;
}

static int HD_get_darkest_color(lua_State *L)
{
    HMM_Vec3    rgb;
    const char    *name         = luaL_checkstring(L, 1);
    double         from_x       = luaL_optnumber(L, 2, -1);
    double         to_x         = luaL_optnumber(L, 3, 1000000);
    double         from_y       = luaL_optnumber(L, 4, -1);
    double         to_y         = luaL_optnumber(L, 5, 1000000);
    const byte    *what_palette = (const byte *)&playpal_data[0];
    const image_c *tmp_img_c    = W_ImageLookup(name, INS_Graphic, 0);
    if (tmp_img_c->source_palette >= 0)
        what_palette = (const byte *)W_LoadLump(tmp_img_c->source_palette);
    epi::image_data_c *tmp_img_data =
        R_PalettisedToRGB(ReadAsEpiBlock((image_c *)tmp_img_c), what_palette, tmp_img_c->opacity);
    u8_t temp_rgb[3];
    tmp_img_data->DarkestColor(temp_rgb, from_x, to_x, from_y, to_y);
    rgb.X = temp_rgb[0];
    rgb.Y = temp_rgb[1];
    rgb.Z = temp_rgb[2];
    delete tmp_img_data;

    LUA_PushVector3(L, rgb);
    return 1;
}

static int HD_get_average_hue(lua_State *L)
{
    HMM_Vec3    rgb;
    const char    *name         = luaL_checkstring(L, 1);
    double         from_x       = luaL_optnumber(L, 2, -1);
    double         to_x         = luaL_optnumber(L, 3, 1000000);
    double         from_y       = luaL_optnumber(L, 4, -1);
    double         to_y         = luaL_optnumber(L, 5, 1000000);
    const byte    *what_palette = (const byte *)&playpal_data[0];
    const image_c *tmp_img_c    = W_ImageLookup(name, INS_Graphic, 0);
    if (tmp_img_c->source_palette >= 0)
        what_palette = (const byte *)W_LoadLump(tmp_img_c->source_palette);
    epi::image_data_c *tmp_img_data =
        R_PalettisedToRGB(ReadAsEpiBlock((image_c *)tmp_img_c), what_palette, tmp_img_c->opacity);
    u8_t temp_rgb[3];
    tmp_img_data->AverageHue(temp_rgb, NULL, from_x, to_x, from_y, to_y);
    rgb.X = temp_rgb[0];
    rgb.Y = temp_rgb[1];
    rgb.Z = temp_rgb[2];
    delete tmp_img_data;

    LUA_PushVector3(L, rgb);
    return 1;
}

// These two aren't really needed anymore with the AverageColor rework, but keeping them in case COALHUDS in the wild
// use them - Dasho
static int HD_get_average_top_border_color(lua_State *L)
{
    HMM_Vec3    rgb;
    const char    *name         = luaL_checkstring(L, 1);
    const byte    *what_palette = (const byte *)&playpal_data[0];
    const image_c *tmp_img_c    = W_ImageLookup(name, INS_Graphic, 0);
    if (tmp_img_c->source_palette >= 0)
        what_palette = (const byte *)W_LoadLump(tmp_img_c->source_palette);
    epi::image_data_c *tmp_img_data =
        R_PalettisedToRGB(ReadAsEpiBlock((image_c *)tmp_img_c), what_palette, tmp_img_c->opacity);
    u8_t temp_rgb[3];
    tmp_img_data->AverageColor(temp_rgb, 0, tmp_img_c->actual_w, tmp_img_c->actual_h - 1, tmp_img_c->actual_h);
    rgb.X = temp_rgb[0];
    rgb.Y = temp_rgb[1];
    rgb.Z = temp_rgb[2];
    delete tmp_img_data;

    LUA_PushVector3(L, rgb);
    return 1;
}
static int HD_get_average_bottom_border_color(lua_State *L)
{
    HMM_Vec3    rgb;
    const char    *name         = luaL_checkstring(L, 1);
    const byte    *what_palette = (const byte *)&playpal_data[0];
    const image_c *tmp_img_c    = W_ImageLookup(name, INS_Graphic, 0);
    if (tmp_img_c->source_palette >= 0)
        what_palette = (const byte *)W_LoadLump(tmp_img_c->source_palette);
    epi::image_data_c *tmp_img_data =
        R_PalettisedToRGB(ReadAsEpiBlock((image_c *)tmp_img_c), what_palette, tmp_img_c->opacity);
    u8_t temp_rgb[3];
    tmp_img_data->AverageColor(temp_rgb, 0, tmp_img_c->actual_w, 0, 1);
    rgb.X = temp_rgb[0];
    rgb.Y = temp_rgb[1];
    rgb.Z = temp_rgb[2];
    delete tmp_img_data;

    LUA_PushVector3(L, rgb);
    return 1;
}

// hud.rts_enable(tag)
//
static int HD_rts_enable(lua_State *L)
{

    std::string name = luaL_checkstring(L, 1);

    if (!name.empty())
        RAD_EnableByTag(NULL, name.c_str(), false);

    return 0;
}

// hud.rts_disable(tag)
//
static int HD_rts_disable(lua_State *L)
{

    std::string name = luaL_checkstring(L, 1);

    if (!name.empty())
        RAD_EnableByTag(NULL, name.c_str(), true);

    return 0;
}

// hud.rts_isactive(tag)
//
static int HD_rts_isactive(lua_State *L)
{

    std::string name = luaL_checkstring(L, 1);

    if (!name.empty())
    {
        if (RAD_IsActiveByTag(NULL, name.c_str()))
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

    const image_c *img = W_ImageLookup(name, INS_Graphic);

    if (img)
    {
        lua_pushinteger(L, HUD_GetImageWidth(img));
    }
    else
    {
        lua_pushinteger(L, 0);
    }

    return 1;
}

// hud.get_image_height(name)
//
static int HD_get_image_height(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    const image_c *img = W_ImageLookup(name, INS_Graphic);

    if (img)
    {
        lua_pushinteger(L, HUD_GetImageHeight(img));
    }
    else
    {
        lua_pushinteger(L, 0);
    }

    return 1;
}

static const luaL_Reg hudlib[] = {{"game_mode", HD_game_mode},
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
                                  {NULL, NULL}};

static int luaopen_hud(lua_State *L)
{
    luaL_newlib(L, hudlib);
    return 1;
}

void LUA_RegisterHudLibrary(lua_State *L)
{
    luaL_requiref(L, "_hud", luaopen_hud, 1);
    lua_pop(L, 1);
}

void LUA_RunHud(void)
{
    HUD_Reset();

    ui_hud_who    = players[displayplayer];
    ui_player_who = players[displayplayer];

    ui_hud_automap_flags[0] = 0;
    ui_hud_automap_flags[1] = 0;
    ui_hud_automap_zoom     = -1;

    LUA_CallGlobalFunction(global_lua_state, "draw_all");

    HUD_Reset();
}