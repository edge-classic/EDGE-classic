//----------------------------------------------------------------------------
//  EDGE Console Interface code.
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//  Copyright (c) 1998       Randy Heit
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
//----------------------------------------------------------------------------
//
// Originally based on the ZDoom console code, by Randy Heit
// (rheit@iastate.edu).  Randy Heit has given his permission to
// release this code under the GPL, for which the EDGE Team is very
// grateful.

#include <stdarg.h>
#include <string.h>

#include <vector>

#include "con_main.h"
#include "con_var.h"
#include "e_input.h"
#include "e_player.h"
#include "edge_profiling.h"
#include "epi.h"
#include "font.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "i_defs_gl.h"
#include "i_system.h"
#include "language.h"
#include "m_argv.h"
#include "r_draw.h"
#include "r_image.h"
#include "r_modes.h"
#include "r_wipe.h"
#include "str_compare.h"
#include "str_util.h"
#include "w_files.h"
#include "w_wad.h"

static constexpr uint8_t kConsoleWipeTics = 12;

EDGE_DEFINE_CONSOLE_VARIABLE(debug_fps, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(debug_position, "0", kConsoleVariableFlagArchive)

static ConsoleVisibility console_visible;

// stores the console toggle effect
static int   console_wipe_active   = 0;
static int   console_wipe_position = 0;
static Font *console_font;
Font        *endoom_font;

// the console's background
static Style *console_style;

static RGBAColor current_color;

extern void StartupProgressMessage(const char *message);

extern ConsoleVariable pixel_aspect_ratio;

static constexpr uint8_t kMaximumConsoleLines = 160;

// For Quit Screen ENDOOM (create once, always store)
ConsoleLine *quit_lines[kEndoomLines];
static int   quit_used_lines        = 0;
static bool  quit_partial_last_line = false;

// entry [0] is the bottom-most one
static ConsoleLine *console_lines[kMaximumConsoleLines];
static int          console_used_lines        = 0;
static bool         console_partial_last_line = false;

// the console row that is displayed at the bottom of screen, -1 if cmdline
// is the bottom one.
static int bottom_row = -1;

static constexpr uint8_t kMaximumConsoleInput = 255;

static char input_line[kMaximumConsoleInput + 2];
static int  input_position = 0;

int                    console_cursor;
extern ConsoleVariable double_framerate;

static constexpr uint8_t kConsoleKeyRepeatDelay = ((250 * kTicRate) / 1000);
static constexpr uint8_t kConsoleKeyRepeatRate  = (kTicRate / 15);

// HISTORY

static constexpr uint8_t kConsoleMaximumCommandHistory = 100;

static std::string *cmd_history[kConsoleMaximumCommandHistory];

static int command_used_history = 0;

// when browsing the cmdhistory, this shows the current index. Otherwise it's
// -1.
static int command_history_position = -1;

// always type kInputEventKeyDown
static int repeat_key;
static int repeat_countdown;

// tells whether shift is pressed, and pgup/dn should scroll to top/bottom of
// linebuffer.
static bool keys_shifted;

static bool tabbed_last;

static int scroll_direction;

static void ConsoleAddLine(const char *s, bool partial)
{
    if (console_partial_last_line)
    {
        EPI_ASSERT(console_lines[0]);

        console_lines[0]->Append(s);

        console_partial_last_line = partial;
        return;
    }

    // scroll everything up

    delete console_lines[kMaximumConsoleLines - 1];

    for (int i = kMaximumConsoleLines - 1; i > 0; i--)
        console_lines[i] = console_lines[i - 1];

    RGBAColor col = current_color;

    if (col == SG_GRAY_RGBA32 &&
        (epi::StringPrefixCaseCompareASCII(s, "WARNING") == 0))
        col = SG_DARK_ORANGE_RGBA32;

    console_lines[0] = new ConsoleLine(s, col);

    console_partial_last_line = partial;

    if (console_used_lines < kMaximumConsoleLines) console_used_lines++;
}

static void ConsoleEndoomAddLine(uint8_t endoom_byte, const char *s,
                                 bool partial)
{
    if (console_partial_last_line)
    {
        EPI_ASSERT(console_lines[0]);

        console_lines[0]->Append(s);

        console_lines[0]->AppendEndoom(endoom_byte);

        console_partial_last_line = partial;
        return;
    }

    // scroll everything up

    delete console_lines[kMaximumConsoleLines - 1];

    for (int i = kMaximumConsoleLines - 1; i > 0; i--)
        console_lines[i] = console_lines[i - 1];

    RGBAColor col = current_color;

    if (col == SG_GRAY_RGBA32 &&
        (epi::StringPrefixCaseCompareASCII(s, "WARNING") == 0))
        col = SG_DARK_ORANGE_RGBA32;

    console_lines[0] = new ConsoleLine(s, col);

    console_lines[0]->AppendEndoom(endoom_byte);

    console_partial_last_line = partial;

    if (console_used_lines < kMaximumConsoleLines) console_used_lines++;
}

static void ConsoleQuitAddLine(const char *s, bool partial)
{
    if (quit_partial_last_line)
    {
        EPI_ASSERT(quit_lines[0]);

        quit_lines[0]->Append(s);

        quit_partial_last_line = partial;
        return;
    }

    // scroll everything up

    delete quit_lines[kEndoomLines - 1];

    for (int i = kEndoomLines - 1; i > 0; i--)
        quit_lines[i] = quit_lines[i - 1];

    RGBAColor col = current_color;

    quit_lines[0] = new ConsoleLine(s, col);

    quit_partial_last_line = partial;

    if (quit_used_lines < kEndoomLines) quit_used_lines++;
}

static void ConsoleQuitEndoomAddLine(uint8_t endoom_byte, const char *s,
                                     bool partial)
{
    if (quit_partial_last_line)
    {
        EPI_ASSERT(quit_lines[0]);

        quit_lines[0]->Append(s);

        quit_lines[0]->AppendEndoom(endoom_byte);

        quit_partial_last_line = partial;
        return;
    }

    // scroll everything up

    delete quit_lines[kEndoomLines - 1];

    for (int i = kEndoomLines - 1; i > 0; i--)
        quit_lines[i] = quit_lines[i - 1];

    RGBAColor col = current_color;

    quit_lines[0] = new ConsoleLine(s, col);

    quit_lines[0]->AppendEndoom(endoom_byte);

    quit_partial_last_line = partial;

    if (quit_used_lines < kEndoomLines) quit_used_lines++;
}

static void ConsoleAddCmdHistory(const char *s)
{
    // don't add if same as previous command
    if (command_used_history > 0)
        if (strcmp(s, cmd_history[0]->c_str()) == 0) return;

    // scroll everything up
    delete cmd_history[kConsoleMaximumCommandHistory - 1];

    for (int i = kConsoleMaximumCommandHistory - 1; i > 0; i--)
        cmd_history[i] = cmd_history[i - 1];

    cmd_history[0] = new std::string(s);

    if (command_used_history < kConsoleMaximumCommandHistory)
        command_used_history++;
}

static void ConsoleClearInputLine(void)
{
    input_line[0]  = 0;
    input_position = 0;
}

void ConsoleSetVisible(ConsoleVisibility v)
{
    if (v == kConsoleVisibilityToggle)
    {
        v = (console_visible == kConsoleVisibilityNotVisible)
                ? kConsoleVisibilityMaximal
                : kConsoleVisibilityNotVisible;

        scroll_direction = 0;
    }

    if (console_visible == v) return;

    console_visible = v;

    if (v == kConsoleVisibilityMaximal) { tabbed_last = false; }

    if (!console_wipe_active)
    {
        console_wipe_active = true;
        console_wipe_position =
            (v == kConsoleVisibilityMaximal) ? 0 : kConsoleWipeTics;
    }
}

static void StripWhitespace(char *src)
{
    const char *start = src;

    while (*start && epi::IsSpaceASCII(*start)) start++;

    const char *end = src + strlen(src);

    while (end > start && epi::IsSpaceASCII(end[-1])) end--;

    while (start < end) { *src++ = *start++; }

    *src = 0;
}

static void SplitIntoLines(char *src)
{
    char *dest = src;
    char *line = dest;

    while (*src)
    {
        if (*src == '\n')
        {
            *dest++ = 0;

            ConsoleAddLine(line, false);

            line = dest;

            src++;
            continue;
        }

        *dest++ = *src++;
    }

    *dest++ = 0;

    if (line[0]) { ConsoleAddLine(line, true); }

    current_color = SG_GRAY_RGBA32;
}

static void EndoomSplitIntoLines(uint8_t endoom_byte, char *src)
{
    char *dest = src;
    char *line = dest;

    while (*src)
    {
        if (*src == '\n')
        {
            *dest++ = 0;

            ConsoleAddLine(line, false);

            line = dest;

            src++;
            continue;
        }

        *dest++ = *src++;
    }

    *dest++ = 0;

    if (line[0]) { ConsoleEndoomAddLine(endoom_byte, line, true); }

    current_color = SG_GRAY_RGBA32;
}

static void QuitSplitIntoLines(char *src)
{
    char *dest = src;
    char *line = dest;

    while (*src)
    {
        if (*src == '\n')
        {
            *dest++ = 0;

            ConsoleQuitAddLine(line, false);

            line = dest;

            src++;
            continue;
        }

        *dest++ = *src++;
    }

    *dest++ = 0;

    if (line[0]) { ConsoleQuitAddLine(line, true); }

    current_color = SG_GRAY_RGBA32;
}

static void QuitEndoomSplitIntoLines(uint8_t endoom_byte, char *src)
{
    char *dest = src;
    char *line = dest;

    while (*src) { *dest++ = *src++; }

    *dest++ = 0;

    if (line[0]) { ConsoleQuitEndoomAddLine(endoom_byte, line, true); }

    current_color = SG_GRAY_RGBA32;
}

void ConsolePrint(const char *message, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, message);
    vsprintf(buffer, message, argptr);
    va_end(argptr);

    SplitIntoLines(buffer);
}

void ConsoleEndoomPrintf(uint8_t endoom_byte, const char *message, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, message);
    vsprintf(buffer, message, argptr);
    va_end(argptr);

    EndoomSplitIntoLines(endoom_byte, buffer);
}

void ConsoleQuitPrintf(const char *message, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, message);
    vsprintf(buffer, message, argptr);
    va_end(argptr);

    QuitSplitIntoLines(buffer);
}

void ConsoleQuitEndoomPrintf(uint8_t endoom_byte, const char *message, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, message);
    vsprintf(buffer, message, argptr);
    va_end(argptr);

    QuitEndoomSplitIntoLines(endoom_byte, buffer);
}

void ConsoleMessage(const char *message, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, message);

    // Print the message into a text string
    vsprintf(buffer, message, argptr);

    va_end(argptr);

    HudStartMessage(buffer);

    strcat(buffer, "\n");

    SplitIntoLines(buffer);
}

void ConsoleMessageLDF(const char *lookup, ...)
{
    va_list argptr;
    char    buffer[1024];

    lookup = language[lookup];

    va_start(argptr, lookup);
    vsprintf(buffer, lookup, argptr);
    va_end(argptr);

    HudStartMessage(buffer);

    strcat(buffer, "\n");

    SplitIntoLines(buffer);
}

void ConsoleImportantMessageLDF(const char *lookup, ...)
{
    va_list argptr;
    char    buffer[1024];

    lookup = language[lookup];

    va_start(argptr, lookup);
    vsprintf(buffer, lookup, argptr);
    va_end(argptr);

    HudStartImportantMessage(buffer);

    strcat(buffer, "\n");

    SplitIntoLines(buffer);
}

void ConsoleMessageColor(RGBAColor col) { current_color = col; }

static int   XMUL;
static int   FNSZ;
static float FNSZ_ratio;

static void CalcSizes()
{
    if (current_screen_width < 1024)
        FNSZ = 16;
    else
        FNSZ = 24;

    FNSZ_ratio = FNSZ / console_font->definition_->default_size_;
    if (console_font->definition_->type_ == kFontTypeImage)
        XMUL = RoundToInteger(
            (console_font->image_monospace_width_ + console_font->spacing_) *
            (FNSZ / console_font->image_character_height_));
}

static void SolidBox(int x, int y, int w, int h, RGBAColor col, float alpha)
{
    if (alpha < 0.99f) glEnable(GL_BLEND);

    sg_color sgcol = sg_make_color_1i(col);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, alpha);

    glBegin(GL_QUADS);

    glVertex2i(x, y);
    glVertex2i(x, y + h);
    glVertex2i(x + w, y + h);
    glVertex2i(x + w, y);

    glEnd();

    glDisable(GL_BLEND);
}

static void HorizontalLine(int y, RGBAColor col)
{
    float alpha = 1.0f;

    SolidBox(0, y, current_screen_width - 1, 1, col, alpha);
}

static void DrawChar(int x, int y, char ch, RGBAColor col)
{
    if (x + FNSZ < 0) return;

    sg_color sgcol = sg_make_color_1i(col);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, 1.0f);

    if (console_font->definition_->type_ == kFontTypeTrueType)
    {
        float chwidth = console_font->CharWidth(ch);
        XMUL = RoundToInteger(chwidth * FNSZ_ratio / pixel_aspect_ratio.f_);
        float width = (chwidth - console_font->spacing_) * FNSZ_ratio /
                      pixel_aspect_ratio.f_;
        float x_adjust = (XMUL - width) / 2;
        float y_adjust = console_font->truetype_glyph_map_.at((uint8_t)ch)
                             .y_shift[current_font_size] *
                         FNSZ_ratio;
        float height = console_font->truetype_glyph_map_.at((uint8_t)ch)
                           .height[current_font_size] *
                       FNSZ_ratio;
        stbtt_aligned_quad *q =
            console_font->truetype_glyph_map_.at((uint8_t)ch)
                .character_quad[current_font_size];
        glBegin(GL_POLYGON);
        glTexCoord2f(q->s0, q->t0);
        glVertex2f(x + x_adjust, y - y_adjust);
        glTexCoord2f(q->s1, q->t0);
        glVertex2f(x + x_adjust + width, y - y_adjust);
        glTexCoord2f(q->s1, q->t1);
        glVertex2f(x + x_adjust + width, y - y_adjust - height);
        glTexCoord2f(q->s0, q->t1);
        glVertex2f(x + x_adjust, y - y_adjust - height);
        glEnd();
        return;
    }

    uint8_t px = (uint8_t)ch % 16;
    uint8_t py = 15 - (uint8_t)ch / 16;

    float tx1 = (px)*console_font->font_image_->width_ratio_;
    float tx2 = (px + 1) * console_font->font_image_->width_ratio_;
    float ty1 = (py)*console_font->font_image_->height_ratio_;
    float ty2 = (py + 1) * console_font->font_image_->height_ratio_;

    glBegin(GL_POLYGON);

    glTexCoord2f(tx1, ty1);
    glVertex2i(x, y);

    glTexCoord2f(tx1, ty2);
    glVertex2i(x, y + FNSZ);

    glTexCoord2f(tx2, ty2);
    glVertex2i(x + FNSZ, y + FNSZ);

    glTexCoord2f(tx2, ty1);
    glVertex2i(x + FNSZ, y);

    glEnd();
}

static void DrawEndoomChar(float x, float y, char ch, RGBAColor col,
                           RGBAColor col2, bool blink, int enwidth)
{
    if (x + FNSZ < 0) return;

    sg_color sgcol = sg_make_color_1i(col2);

    glDisable(GL_TEXTURE_2D);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, 1.0f);

    glBegin(GL_QUADS);

    glVertex2i(x - (enwidth / 2), y);

    glVertex2i(x - (enwidth / 2), y + FNSZ);

    glVertex2i(x + (enwidth / 2), y + FNSZ);

    glVertex2i(x + (enwidth / 2), y);

    glEnd();

    glEnable(GL_TEXTURE_2D);

    sgcol = sg_make_color_1i(col);

    glColor4f(sgcol.r, sgcol.g, sgcol.b, 1.0f);

    if (blink && console_cursor >= 16) ch = 0x20;

    uint8_t px = (uint8_t)ch % 16;
    uint8_t py = 15 - (uint8_t)ch / 16;

    float tx1 = (px)*endoom_font->font_image_->width_ratio_;
    float tx2 = (px + 1) * endoom_font->font_image_->width_ratio_;

    float ty1 = (py)*endoom_font->font_image_->height_ratio_;
    float ty2 = (py + 1) * endoom_font->font_image_->height_ratio_;

    glBegin(GL_POLYGON);

    glTexCoord2f(tx1, ty1);
    glVertex2i(x - enwidth, y);

    glTexCoord2f(tx1, ty2);
    glVertex2i(x - enwidth, y + FNSZ);

    glTexCoord2f(tx2, ty2);
    glVertex2i(x + enwidth, y + FNSZ);

    glTexCoord2f(tx2, ty1);
    glVertex2i(x + enwidth, y);

    glEnd();
}

// writes the text on coords (x,y) of the console
static void DrawText(int x, int y, const char *s, RGBAColor col)
{
    if (console_font->definition_->type_ == kFontTypeImage)
    {
        // Always whiten the font when used with console output
        GLuint tex_id = ImageCache(console_font->font_image_, true,
                                   (const Colormap *)0, true);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex_id);

        glEnable(GL_BLEND);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0);
    }
    else if (console_font->definition_->type_ == kFontTypeTrueType)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        if ((image_smoothing &&
             console_font->definition_->truetype_smoothing_ ==
                 FontDefinition::kTrueTypeSmoothOnDemand) ||
            console_font->definition_->truetype_smoothing_ ==
                FontDefinition::kTrueTypeSmoothAlways)
            glBindTexture(
                GL_TEXTURE_2D,
                console_font->truetype_smoothed_texture_id_[current_font_size]);
        else
            glBindTexture(
                GL_TEXTURE_2D,
                console_font->truetype_texture_id_[current_font_size]);
    }

    bool draw_cursor = false;

    if (s == input_line)
    {
        if (console_cursor < 16) draw_cursor = true;
    }

    int pos = 0;
    for (; *s; s++, pos++)
    {
        DrawChar(x, y, *s, col);

        if (console_font->definition_->type_ == kFontTypeTrueType)
        {
            if (s + 1)
            {
                x += (float)stbtt_GetGlyphKernAdvance(
                         console_font->truetype_info_,
                         console_font->GetGlyphIndex(*s),
                         console_font->GetGlyphIndex(*(s + 1))) *
                     console_font->truetype_kerning_scale_[current_font_size] *
                     FNSZ_ratio / pixel_aspect_ratio.f_;
            }
        }

        if (pos == input_position && draw_cursor)
        {
            DrawChar(x, y, 95, col);
            draw_cursor = false;
        }

        x += XMUL;

        if (x >= current_screen_width) break;
    }

    if (draw_cursor) DrawChar(x, y, 95, col);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
}

static void EndoomDrawText(int x, int y, ConsoleLine *endoom_line)
{
    // Always whiten the font when used with console output
    GLuint tex_id =
        ImageCache(endoom_font->font_image_, true, (const Colormap *)0, true);

    int enwidth =
        RoundToInteger((float)endoom_font->image_monospace_width_ *
                       ((float)FNSZ / endoom_font->image_monospace_width_) / 2);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    glEnable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);

    for (int i = 0; i < 80; i++)
    {
        uint8_t info = endoom_line->endoom_bytes_.at(i);

        DrawEndoomChar(x, y, endoom_line->line_.at(i), endoom_colors[info & 15],
                       endoom_colors[(info >> 4) & 7], info & 128, enwidth);

        x += enwidth;

        if (x >= current_screen_width) break;
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
}

void ConsoleSetupFont(void)
{
    if (!console_font)
    {
        FontDefinition *DEF = fontdefs.Lookup("CON_FONT_2");
        if (!DEF) FatalError("CON_FONT_2 definition missing from DDFFONT!\n");
        console_font = hud_fonts.Lookup(DEF);
        EPI_ASSERT(console_font);
        console_font->Load();
    }

    if (!endoom_font)
    {
        FontDefinition *DEF = fontdefs.Lookup("ENDFONT");
        if (!DEF) FatalError("ENDFONT definition missing from DDFFONT!\n");
        endoom_font = hud_fonts.Lookup(DEF);
        EPI_ASSERT(endoom_font);
        endoom_font->Load();
    }

    if (!console_style)
    {
        StyleDefinition *def = styledefs.Lookup("CONSOLE");
        if (!def) def = default_style;
        console_style = hud_styles.Lookup(def);
    }

    CalcSizes();
}

void ConsoleDrawer(void)
{
    ConsoleSetupFont();

    if (console_visible == kConsoleVisibilityNotVisible && !console_wipe_active)
        return;

    // -- background --

    int CON_GFX_HT = (current_screen_height * 3 / 5);

    int y = current_screen_height;

    if (console_wipe_active)
        y = y - CON_GFX_HT * (console_wipe_position) / kConsoleWipeTics;
    else
        y = y - CON_GFX_HT;

    if (console_style->background_image_ != nullptr)
    {
        const Image *img = console_style->background_image_;

        HudRawImage(0, y, current_screen_width, y + CON_GFX_HT, img, 0.0, 0.0,
                    img->Right(), img->Top(),
                    console_style->definition_->bg_.translucency_, kRGBANoValue,
                    nullptr, 0, 0);
    }
    else
    {
        SolidBox(0, y, current_screen_width, current_screen_height - y,
                 console_style->definition_->bg_.colour_ != kRGBANoValue
                     ? console_style->definition_->bg_.colour_
                     : SG_BLACK_RGBA32,
                 console_style->definition_->bg_.translucency_);
    }

    y += FNSZ / 4 +
         (console_font->definition_->type_ == kFontTypeTrueType ? FNSZ : 0);

    // -- input line --

    if (bottom_row == -1)
    {
        DrawText(0, y, ">", SG_MAGENTA_RGBA32);

        if (command_history_position >= 0)
        {
            std::string text = cmd_history[command_history_position]->c_str();

            if (console_cursor < 16) text.append("_");

            DrawText(XMUL, y, text.c_str(), SG_MAGENTA_RGBA32);
        }
        else { DrawText(XMUL, y, input_line, SG_MAGENTA_RGBA32); }

        y += FNSZ;
    }

    y += FNSZ / 2;

    // -- text lines --

    for (int i = HMM_MAX(0, bottom_row); i < kMaximumConsoleLines; i++)
    {
        ConsoleLine *CL = console_lines[i];

        if (!CL) break;

        if (epi::StringPrefixCompare(CL->line_, "--------") == 0)
            HorizontalLine(y + FNSZ / 2, CL->color_);
        else if (CL->endoom_bytes_.size() == 80 &&
                 CL->line_.size() == 80)  // 80 ENDOOM characters + newline
            EndoomDrawText(0, y, CL);
        else
            DrawText(0, y, CL->line_.c_str(), CL->color_);

        y += FNSZ;

        if (y >= current_screen_height) break;
    }
}

static void GotoEndOfLine(void)
{
    if (command_history_position < 0)
        input_position = strlen(input_line);
    else
        input_position = strlen(cmd_history[command_history_position]->c_str());

    console_cursor = 0;
}

static void EditHistory(void)
{
    if (command_history_position >= 0)
    {
        strcpy(input_line, cmd_history[command_history_position]->c_str());

        command_history_position = -1;
    }
}

static void InsertChar(char ch)
{
    // make room for new character, shift the trailing NUL too

    for (int j = kMaximumConsoleInput - 2; j >= input_position; j--)
        input_line[j + 1] = input_line[j];

    input_line[kMaximumConsoleInput - 1] = 0;

    input_line[input_position++] = ch;
}

static char KeyToCharacter(int key, bool shift, bool ctrl)
{
    if (ctrl) return 0;

    if (key < 32 || key > 126) return 0;

    if (!shift) return (char)key;

    // the following assumes a US keyboard layout
    switch (key)
    {
        case '1':
            return '!';
        case '2':
            return '@';
        case '3':
            return '#';
        case '4':
            return '$';
        case '5':
            return '%';
        case '6':
            return '^';
        case '7':
            return '&';
        case '8':
            return '*';
        case '9':
            return '(';
        case '0':
            return ')';

        case '`':
            return '~';
        case '-':
            return '_';
        case '=':
            return '+';
        case '\\':
            return '|';
        case '[':
            return '{';
        case ']':
            return '}';
        case ';':
            return ':';
        case '\'':
            return '"';
        case ',':
            return '<';
        case '.':
            return '>';
        case '/':
            return '?';
        case '@':
            return '\'';
    }

    return epi::ToUpperASCII(key);
}

static void ListCompletions(std::vector<const char *> &list, int word_len,
                            int max_row, RGBAColor color)
{
    int max_col = current_screen_width / XMUL - 4;
    max_col     = HMM_Clamp(24, max_col, 78);

    char buffer[200];
    int  buf_len = 0;

    buffer[buf_len] = 0;

    char temp[200];
    char last_ja = 0;

    for (int i = 0; i < (int)list.size(); i++)
    {
        const char *name  = list[i];
        int         n_len = (int)strlen(name);

        // support for names with a '.' in them
        const char *dotpos = strchr(name, '.');
        if (dotpos && dotpos > name + word_len)
        {
            if (last_ja == dotpos[-1]) continue;

            last_ja = dotpos[-1];

            n_len = (int)(dotpos - name);

            strcpy(temp, name);
            temp[n_len] = 0;

            name = temp;
        }
        else
            last_ja = 0;

        if (n_len >= max_col * 2 / 3)
        {
            ConsoleMessageColor(color);
            ConsolePrint("  %s\n", name);
            max_row--;
            continue;
        }

        if (buf_len + 1 + n_len > max_col)
        {
            ConsoleMessageColor(color);
            ConsolePrint("  %s\n", buffer);
            max_row--;

            buf_len         = 0;
            buffer[buf_len] = 0;

            if (max_row <= 0)
            {
                ConsoleMessageColor(color);
                ConsolePrint("  etc...\n");
                break;
            }
        }

        if (buf_len > 0) buffer[buf_len++] = ' ';

        strcpy(buffer + buf_len, name);

        buf_len += n_len;
    }

    if (buf_len > 0)
    {
        ConsoleMessageColor(color);
        ConsolePrint("  %s\n", buffer);
    }
}

static void TabComplete(void)
{
    EditHistory();

    // check if we are positioned after a word
    {
        if (input_position == 0) return;

        if (epi::IsDigitASCII(input_line[0])) return;

        for (int i = 0; i < input_position; i++)
        {
            char ch = input_line[i];

            if (!(epi::IsAlphanumericASCII(ch) || ch == '_' || ch == '.'))
                return;
        }
    }

    char save_ch               = input_line[input_position];
    input_line[input_position] = 0;

    std::vector<const char *> match_cmds;
    std::vector<const char *> match_vars;
    std::vector<const char *> match_keys;

    int num_cmd = ConsoleMatchAllCmds(match_cmds, input_line);
    int num_var = ConsoleMatchAllVariables(match_vars, input_line);
    int num_key = 0;  ///  E_MatchAllKeys(match_keys, input_line);

    // we have an unambiguous match, no need to print anything
    if (num_cmd + num_var + num_key == 1)
    {
        input_line[input_position] = save_ch;

        const char *name = (num_var > 0)   ? match_vars[0]
                           : (num_key > 0) ? match_keys[0]
                                           : match_cmds[0];

        EPI_ASSERT((int)strlen(name) >= input_position);

        for (name += input_position; *name; name++) InsertChar(*name);

        if (save_ch != ' ') InsertChar(' ');

        console_cursor = 0;
        return;
    }

    // show what we were trying to match
    ConsoleMessageColor(SG_LIGHT_BLUE_RGBA32);
    ConsolePrint(">%s\n", input_line);

    input_line[input_position] = save_ch;

    if (num_cmd + num_var + num_key == 0)
    {
        ConsolePrint("No matches.\n");
        return;
    }

    if (match_vars.size() > 0)
    {
        ConsolePrint("%u Possible variables:\n", (int)match_vars.size());

        ListCompletions(match_vars, input_position, 7, SG_SPRING_GREEN_RGBA32);
    }

    if (match_keys.size() > 0)
    {
        ConsolePrint("%u Possible keys:\n", (int)match_keys.size());

        ListCompletions(match_keys, input_position, 4, SG_SPRING_GREEN_RGBA32);
    }

    if (match_cmds.size() > 0)
    {
        ConsolePrint("%u Possible commands:\n", (int)match_cmds.size());

        ListCompletions(match_cmds, input_position, 3, SG_SPRING_GREEN_RGBA32);
    }

    // Add as many common characters as possible
    // (e.g. "mou <TAB>" should add the s, e and _).

    // begin by lumping all completions into one list
    unsigned int i;

    for (i = 0; i < match_keys.size(); i++) match_vars.push_back(match_keys[i]);

    for (i = 0; i < match_cmds.size(); i++) match_vars.push_back(match_cmds[i]);

    int pos = input_position;

    for (;;)
    {
        char ch = match_vars[0][pos];
        if (!ch) return;

        for (i = 1; i < match_vars.size(); i++)
            if (match_vars[i][pos] != ch) return;

        InsertChar(ch);

        pos++;
    }
}

void ConsoleHandleKey(int key, bool shift, bool ctrl)
{
    switch (key)
    {
        case kRightAlt:
        case kRightControl:
            // Do nothing
            break;

        case kRightShift:
            // SHIFT was pressed
            keys_shifted = true;
            break;

        case kPageUp:
            if (shift)
                // Move to top of console buffer
                bottom_row = HMM_MAX(-1, console_used_lines - 10);
            else
                // Start scrolling console buffer up
                scroll_direction = +1;
            break;

        case kPageDown:
            if (shift)
                // Move to bottom of console buffer
                bottom_row = -1;
            else
                // Start scrolling console buffer down
                scroll_direction = -1;
            break;

        case kMouseWheelUp:
            bottom_row += 4;
            if (bottom_row > HMM_MAX(-1, console_used_lines - 10))
                bottom_row = HMM_MAX(-1, console_used_lines - 10);
            break;

        case kMouseWheelDown:
            bottom_row -= 4;
            if (bottom_row < -1) bottom_row = -1;
            break;

        case kHome:
            // Move cursor to start of line
            input_position = 0;
            console_cursor = 0;
            break;

        case kEnd:
            // Move cursor to end of line
            GotoEndOfLine();
            break;

        case kUpArrow:
            // Move to previous entry in the command history
            if (command_history_position < command_used_history - 1)
            {
                command_history_position++;
                GotoEndOfLine();
            }
            tabbed_last = false;
            break;

        case kDownArrow:
            // Move to next entry in the command history
            if (command_history_position > -1)
            {
                command_history_position--;
                GotoEndOfLine();
            }
            tabbed_last = false;
            break;

        case kLeftArrow:
            // Move cursor left one character
            if (input_position > 0) input_position--;

            console_cursor = 0;
            break;

        case kRightArrow:
            // Move cursor right one character
            if (command_history_position < 0)
            {
                if (input_line[input_position] != 0) input_position++;
            }
            else
            {
                if (cmd_history[command_history_position]
                        ->c_str()[input_position] != 0)
                    input_position++;
            }
            console_cursor = 0;
            break;

        case kEnter:
            EditHistory();

            // Execute command line (ENTER)
            StripWhitespace(input_line);

            if (strlen(input_line) == 0)
            {
                ConsoleMessageColor(SG_LIGHT_BLUE_RGBA32);
                ConsolePrint(">\n");
            }
            else
            {
                // Add it to history & draw it
                ConsoleAddCmdHistory(input_line);

                ConsoleMessageColor(SG_LIGHT_BLUE_RGBA32);
                ConsolePrint(">%s\n", input_line);

                // Run it!
                ConsoleTryCommand(input_line);
            }

            ConsoleClearInputLine();

            // Bring user back to current line after entering command
            bottom_row -= kMaximumConsoleLines;
            if (bottom_row < -1) bottom_row = -1;

            tabbed_last = false;
            break;

        case kBackspace:
            // Erase character to left of cursor
            EditHistory();

            if (input_position > 0)
            {
                input_position--;

                // shift characters back
                for (int j = input_position; j < kMaximumConsoleInput - 2; j++)
                    input_line[j] = input_line[j + 1];
            }

            tabbed_last    = false;
            console_cursor = 0;
            break;

        case kDelete:
            // Erase charater under cursor
            EditHistory();

            if (input_line[input_position] != 0)
            {
                // shift characters back
                for (int j = input_position; j < kMaximumConsoleInput - 2; j++)
                    input_line[j] = input_line[j + 1];
            }

            tabbed_last    = false;
            console_cursor = 0;
            break;

        case kTab:
            // Try to do tab-completion
            TabComplete();
            break;

        case kEscape:
            // Close console, clear command line, but if we're in the
            // fullscreen console mode, there's nothing to fall back on
            // if it's closed.
            ConsoleClearInputLine();

            command_history_position = -1;
            tabbed_last              = false;

            ConsoleSetVisible(kConsoleVisibilityNotVisible);
            break;

        // Allow screenshotting of console too - Dasho
        case kFunction1:
        case kPrintScreen:
            GameDeferredScreenShot();
            break;

        default:
        {
            char ch = KeyToCharacter(key, shift, ctrl);

            // ignore non-printable characters
            if (ch == 0) break;

            // no room?
            if (input_position >= kMaximumConsoleInput - 1) break;

            EditHistory();
            InsertChar(ch);

            tabbed_last    = false;
            console_cursor = 0;
        }
        break;
    }
}

static int GetKeycode(InputEvent *ev)
{
    int sym = ev->value.key.sym;

    switch (sym)
    {
        case kTab:
        case kPageUp:
        case kPageDown:
        case kHome:
        case kEnd:
        case kLeftArrow:
        case kRightArrow:
        case kBackspace:
        case kDelete:
        case kUpArrow:
        case kDownArrow:
        case kMouseWheelUp:
        case kMouseWheelDown:
        case kEnter:
        case kEscape:
        case kRightShift:
        case kFunction1:
        case kPrintScreen:
            return sym;

        default:
            break;
    }

    if (epi::IsPrintASCII(sym)) return sym;

    return -1;
}

bool ConsoleResponder(InputEvent *ev)
{
    if (ev->type != kInputEventKeyUp && ev->type != kInputEventKeyDown)
        return false;

    if (ev->type == kInputEventKeyDown &&
        EventMatchesKey(key_console, ev->value.key.sym))
    {
        EventClearInput();
        ConsoleSetVisible(kConsoleVisibilityToggle);
        return true;
    }

    if (console_visible == kConsoleVisibilityNotVisible) return false;

    int key = GetKeycode(ev);
    if (key < 0) return true;

    if (ev->type == kInputEventKeyUp)
    {
        if (key == repeat_key) repeat_countdown = 0;

        switch (key)
        {
            case kPageUp:
            case kPageDown:
                scroll_direction = 0;
                break;
            case kRightShift:
                keys_shifted = false;
                break;
            default:
                break;
        }
    }
    else
    {
        // Okay, fine. Most keys don't repeat
        switch (key)
        {
            case kRightArrow:
            case kLeftArrow:
            case kUpArrow:
            case kDownArrow:
            case kSpace:
            case kBackspace:
            case kDelete:
                repeat_countdown =
                    kConsoleKeyRepeatDelay * (double_framerate.d_ ? 2 : 1);
                break;
            default:
                repeat_countdown = 0;
                break;
        }

        repeat_key = key;

        ConsoleHandleKey(key, keys_shifted, false);
    }

    return true;  // eat all keyboard events
}

void ConsoleTicker(void)
{
    int add = 1;
    if (double_framerate.d_ && !(hud_tic & 1)) add = 0;

    console_cursor = (console_cursor + add) & 31;

    if (console_visible != kConsoleVisibilityNotVisible)
    {
        // Handle repeating keys
        switch (scroll_direction)
        {
            case +1:
                if (bottom_row < kMaximumConsoleLines - 10) bottom_row++;
                break;

            case -1:
                if (bottom_row > -1) bottom_row--;
                break;

            default:
                if (repeat_countdown)
                {
                    repeat_countdown -= 1;

                    while (repeat_countdown <= 0)
                    {
                        repeat_countdown += kConsoleKeyRepeatRate *
                                            (double_framerate.d_ ? 2 : 1);
                        ConsoleHandleKey(repeat_key, keys_shifted, false);
                    }
                }
                break;
        }
    }

    if (console_wipe_active)
    {
        if (console_visible == kConsoleVisibilityNotVisible)
        {
            console_wipe_position--;
            if (console_wipe_position <= 0) console_wipe_active = false;
        }
        else
        {
            console_wipe_position++;
            if (console_wipe_position >= kConsoleWipeTics)
                console_wipe_active = false;
        }
    }
}

//
// Initialises the console
//
void ConsoleInit(void)
{
    ConsoleSortVariables();

    console_used_lines   = 0;
    command_used_history = 0;

    bottom_row               = -1;
    command_history_position = -1;

    ConsoleClearInputLine();

    current_color = SG_GRAY_RGBA32;

    ConsoleAddLine("", false);
    ConsoleAddLine("", false);
}

void ConsoleStart(void)
{
    console_visible = kConsoleVisibilityNotVisible;
    console_cursor  = 0;
    StartupProgressMessage("Starting console...");
}

void ConsoleShowFPS(void)
{
    if (debug_fps.d_ == 0) return;

    ConsoleSetupFont();

    // -AJA- 2022: reworked for better accuracy, ability to show WORST time

    // get difference since last call
    static uint32_t last_time = 0;
    uint32_t        time      = GetMicroseconds();
    uint32_t        diff      = time - last_time;
    last_time                 = time;

    // last computed value, state to compute average
    static float avg_shown   = 100.00;
    static float worst_shown = 100.00;

    static uint32_t frames = 0;
    static uint32_t total  = 0;
    static uint32_t worst  = 0;

    // ignore a large diff or timer wrap-around
    if (diff < 1000000)
    {
        frames += 1;
        total += diff;
        worst = HMM_MAX(worst, diff);

        // update every second
        if (total > 999999)
        {
            avg_shown   = (double)total / (double)(frames * 1000);
            worst_shown = (double)worst / 1000.0;

            frames = 0;
            total  = 0;
            worst  = 0;
        }
    }

    int x = current_screen_width - XMUL * 16;
    int y = current_screen_height - FNSZ * 2;

    if (abs(debug_fps.d_) >= 2) y -= FNSZ;

    if (abs(debug_fps.d_) >= 3) y -= (FNSZ * 4);

    SolidBox(x, y, current_screen_width, current_screen_height, SG_BLACK_RGBA32,
             0.5);

    x += XMUL;
    y = current_screen_height - FNSZ -
        FNSZ * (console_font->definition_->type_ == kFontTypeTrueType ? -0.5
                                                                      : 0.5);

    // show average...

    char textbuf[128];

    if (debug_fps.d_ < 0)
        sprintf(textbuf, " %6.2f ms", avg_shown);
    else
        sprintf(textbuf, " %6.2f fps", 1000 / avg_shown);

    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);

    // show worst...

    if (abs(debug_fps.d_) >= 2)
    {
        y -= FNSZ;

        if (debug_fps.d_ < 0)
            sprintf(textbuf, " %6.2f max", worst_shown);
        else if (worst_shown > 0)
            sprintf(textbuf, " %6.2f min", 1000 / worst_shown);

        DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);
    }

    // show frame metrics...

    if (abs(debug_fps.d_) >= 3)
    {
        y -= FNSZ;
        sprintf(textbuf, "%i runit", ec_frame_stats.draw_render_units);
        DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);
        y -= FNSZ;
        sprintf(textbuf, "%i wall", ec_frame_stats.draw_wall_parts);
        DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);
        y -= FNSZ;
        sprintf(textbuf, "%i plane", ec_frame_stats.draw_planes);
        DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);
        y -= FNSZ;
        sprintf(textbuf, "%i thing", ec_frame_stats.draw_things);
        DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);
        y -= FNSZ;
        sprintf(textbuf, "%i state", ec_frame_stats.draw_state_change);
        DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);
        y -= FNSZ;
        sprintf(textbuf, "%i texture", ec_frame_stats.draw_texture_change);
        DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);
    }
}

void ConsoleShowPosition(void)
{
    if (debug_position.d_ <= 0) return;

    ConsoleSetupFont();

    Player *p = players[display_player];
    if (p == nullptr) return;

    char textbuf[128];

    int x = current_screen_width - XMUL * 16;
    int y = current_screen_height - FNSZ * 5;

    SolidBox(x, y - FNSZ * 10, XMUL * 16, FNSZ * 10 + 2, SG_BLACK_RGBA32, 0.5);

    x += XMUL;
    y -= FNSZ *
         (console_font->definition_->type_ == kFontTypeTrueType ? 0.25 : 1.25);
    sprintf(textbuf, "    x: %d", (int)p->map_object_->x);
    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);

    y -= FNSZ;
    sprintf(textbuf, "    y: %d", (int)p->map_object_->y);
    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);

    y -= FNSZ;
    sprintf(textbuf, "    z: %d", (int)p->map_object_->z);
    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);

    y -= FNSZ;
    sprintf(textbuf, "angle: %d",
            (int)epi::DegreesFromBAM(p->map_object_->angle_));
    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);

    y -= FNSZ;
    sprintf(textbuf, "x mom: %.4f", p->map_object_->momentum_.X);
    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);

    y -= FNSZ;
    sprintf(textbuf, "y mom: %.4f", p->map_object_->momentum_.Y);
    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);

    y -= FNSZ;
    sprintf(textbuf, "z mom: %.4f", p->map_object_->momentum_.Z);
    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);

    y -= FNSZ;
    sprintf(textbuf, "  sec: %d",
            (int)(p->map_object_->subsector_->sector - level_sectors));
    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);

    y -= FNSZ;
    sprintf(textbuf, "  sub: %d",
            (int)(p->map_object_->subsector_ - level_subsectors));
    DrawText(x, y, textbuf, SG_WEB_GRAY_RGBA32);
}

void ConsolePrintEndoom()
{
    int      length = 0;
    uint8_t *data   = nullptr;

    data = OpenPackOrLumpInMemory("ENDOOM", {".bin"}, &length);
    if (!data) data = OpenPackOrLumpInMemory("ENDTEXT", {".bin"}, &length);
    if (!data) data = OpenPackOrLumpInMemory("ENDBOOM", {".bin"}, &length);
    if (!data) data = OpenPackOrLumpInMemory("ENDSTRF", {".bin"}, &length);
    if (!data)
    {
        ConsolePrint("ConsolePrintEndoom: No ENDOOM screen found!\n");
        return;
    }
    if (length != 4000)
    {
        ConsolePrint(
            "ConsolePrintEndoom: Lump exists, but is malformed! (Length not "
            "equal "
            "to 4000 bytes)\n");
        delete[] data;
        return;
    }
    ConsolePrint("\n\n");
    int row_counter = 0;
    for (int i = 0; i < 4000; i += 2)
    {
        ConsoleEndoomPrintf(data[i + 1], "%c",
                            ((int)data[i] == 0 || (int)data[i] == 255)
                                ? 0x20
                                : (int)data[i]);  // Fix crumpled up ENDOOMs lol
        row_counter++;
        if (row_counter == 80)
        {
            ConsolePrint("\n");
            row_counter = 0;
        }
    }
    ConsolePrint("\n");
    delete[] data;
}

void ConsoleCreateQuitScreen()
{
    int      length = 0;
    uint8_t *data   = nullptr;

    data = OpenPackOrLumpInMemory("ENDOOM", {".bin"}, &length);
    if (!data) data = OpenPackOrLumpInMemory("ENDTEXT", {".bin"}, &length);
    if (!data) data = OpenPackOrLumpInMemory("ENDBOOM", {".bin"}, &length);
    if (!data) data = OpenPackOrLumpInMemory("ENDSTRF", {".bin"}, &length);
    if (!data)
    {
        ConsolePrint("No ENDOOM screen found for this WAD!\n");
        return;
    }
    if (length != 4000)
    {
        ConsolePrint(
            "ConsoleCreateQuitScreen: ENDOOM exists, but is malformed! (Length "
            "not equal to 4000 bytes)\n");
        delete[] data;
        return;
    }
    int row_counter = 0;
    for (int i = 0; i < 4000; i += 2)
    {
        ConsoleQuitEndoomPrintf(
            data[i + 1], "%c",
            ((uint8_t)data[i] == 0 || (uint8_t)data[i] == 255)
                ? 0x20
                : (uint8_t)data[i]);
        row_counter++;
        if (row_counter == 80)
        {
            ConsoleQuitPrintf("\n");
            row_counter = 0;
        }
    }
    delete[] data;
}

void ConsoleClearLines()
{
    for (int i = 0; i < console_used_lines; i++) { console_lines[i]->Clear(); }
    console_used_lines = 0;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
