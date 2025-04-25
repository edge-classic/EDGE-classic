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
#include "ddf_font.h"
#include "ddf_language.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_player.h"
#include "edge_profiling.h"
#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "i_defs_gl.h"
#include "i_movie.h"
#include "i_system.h"
#include "m_argv.h"
#include "n_network.h"
#include "r_backend.h"
#include "r_draw.h"
#include "r_image.h"
#include "r_modes.h"
#include "r_units.h"
#include "r_wipe.h"
#include "stb_sprintf.h"
#include "w_files.h"
#include "w_wad.h"

static constexpr uint8_t kConsoleWipeTics = 12;

EDGE_DEFINE_CONSOLE_VARIABLE(debug_fps, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(debug_position, "0", kConsoleVariableFlagArchive)

static ConsoleVisibility console_visible;

// stores the console toggle effect
static int   console_wipe_active       = 0;
static int   console_wipe_position     = 0;
static int   old_console_wipe_position = 0;
static Font *console_font;
Font        *endoom_font;

// the console's background
static Style *console_style;

static RGBAColor current_color;

extern void StartupProgressMessage(const char *message);

extern ConsoleVariable pixel_aspect_ratio;

static constexpr uint8_t kMaximumConsoleLines = 160;

// For Quit Screen ENDOOM (create once, always store)
ConsoleLine *quit_lines[kENDOOMLines];

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

int console_cursor;

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

    if (col == kRGBAGray && (epi::StringPrefixCaseCompareASCII(s, "WARNING") == 0))
        col = kRGBADarkOrange;

    console_lines[0] = new ConsoleLine(s, col);

    console_partial_last_line = partial;

    if (console_used_lines < kMaximumConsoleLines)
        console_used_lines++;
}

static void ConsoleAddENDOOMLine(const ConsoleLine *line)
{
    // scroll everything up
    delete console_lines[kMaximumConsoleLines - 1];

    for (int i = kMaximumConsoleLines - 1; i > 0; i--)
        console_lines[i] = console_lines[i - 1];

    console_lines[0] = new ConsoleLine();
    console_lines[0]->endoom_bytes_.resize(kENDOOMBytesPerLine);
    memcpy(console_lines[0]->endoom_bytes_.data(), line->endoom_bytes_.data(), kENDOOMBytesPerLine);

    console_partial_last_line = false;

    if (console_used_lines < kMaximumConsoleLines)
        console_used_lines++;
}

static void ConsoleAddCmdHistory(const char *s)
{
    // don't add if same as previous command
    if (command_used_history > 0)
        if (strcmp(s, cmd_history[0]->c_str()) == 0)
            return;

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

bool ConsoleIsVisible()
{
    return (console_visible != kConsoleNotVisible);
}

void SetConsoleVisibility(ConsoleVisibility v)
{
    if (v == kConsoleToggle)
    {
        v = (console_visible == kConsoleNotVisible) ? kConsoleMaximal
                                                              : kConsoleNotVisible;

        scroll_direction = 0;
    }

    if (console_visible == v)
        return;

    console_visible = v;

    if (v == kConsoleMaximal)
    {
        tabbed_last = false;
    }

    if (!console_wipe_active)
    {
        console_wipe_active       = true;
        console_wipe_position     = (v == kConsoleMaximal) ? 0 : kConsoleWipeTics;
        old_console_wipe_position = console_wipe_position;
    }
}

static void StripWhitespace(char *src)
{
    const char *start = src;

    while (*start && epi::IsSpaceASCII(*start))
        start++;

    const char *end = src + strlen(src);

    while (end > start && epi::IsSpaceASCII(end[-1]))
        end--;

    while (start < end)
    {
        *src++ = *start++;
    }

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

    if (line[0])
    {
        ConsoleAddLine(line, true);
    }

    current_color = kRGBAGray;
}

void ConsoleMessage(ConsoleMessageTarget target, const char *message, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, message);
    stbsp_vsprintf(buffer, message, argptr);
    va_end(argptr);

    switch(target)
    {
        case kConsoleHUDTop:
            HUDStartMessage(buffer);
            strcat(buffer, "\n");
            break;
        case kConsoleHUDCenter:
            HUDStartImportantMessage(buffer);
            strcat(buffer, "\n");
            break;
        case kConsoleOnly:
        default:
            break;
    }

    SplitIntoLines(buffer);
}

void ConsoleMessageColor(RGBAColor col)
{
    current_color = col;
}

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
        XMUL = RoundToInteger((console_font->image_monospace_width_ + console_font->spacing_) *
                              (FNSZ / console_font->image_character_height_));
}

static void SolidBox(float x, float y, float w, float h, RGBAColor col, float alpha)
{
    RendererVertex *glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0,
                                             alpha < 0.99f ? kBlendingAlpha : kBlendingNone);

    RGBAColor unit_col = col;
    epi::SetRGBAAlpha(unit_col, alpha);

    glvert->rgba       = unit_col;
    glvert++->position = {{x, y, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x, y + h, 0}};
    glvert->rgba       = unit_col;
    glvert++->position = {{x + w, y + h, 0}};
    glvert->rgba       = unit_col;
    glvert->position   = {{x + w, y, 0}};

    EndRenderUnit(4);
}

// set params and starts render unit for console text drawing
static RendererVertex *StartText()
{
    GLuint       tex_id = 0;
    BlendingMode blend  = kBlendingNone;

    if (console_font->definition_->type_ == kFontTypeImage)
    {
        // Always whiten the font when used with console output
        tex_id = ImageCache(console_font->font_image_, true, (const Colormap *)0, true);
        blend  = kBlendingMasked;
    }
    else if (console_font->definition_->type_ == kFontTypeTrueType)
    {
        if ((image_smoothing &&
             console_font->definition_->truetype_smoothing_ == FontDefinition::kTrueTypeSmoothOnDemand) ||
            console_font->definition_->truetype_smoothing_ == FontDefinition::kTrueTypeSmoothAlways)
            tex_id = console_font->truetype_smoothed_texture_id_[current_font_size];
        else
            tex_id = console_font->truetype_texture_id_[current_font_size];

        blend = kBlendingAlpha;
    }

    return BeginRenderUnit(GL_QUADS, kMaximumLocalVertices, GL_MODULATE, tex_id, (GLuint)kTextureEnvironmentDisable, 0, 0, blend);
}

static void AddChar(float x, float y, char ch, RendererVertex *&glvert, RGBAColor col)
{   
    if (console_font->definition_->type_ == kFontTypeTrueType)
    {
        float chwidth  = console_font->CharWidth(ch);
        XMUL           = RoundToInteger(chwidth * FNSZ_ratio / pixel_aspect_ratio.f_);
        float width    = (chwidth - console_font->spacing_) * FNSZ_ratio / pixel_aspect_ratio.f_;
        float x_adjust = (XMUL - width) / 2;
        float y_adjust = console_font->truetype_glyph_map_.at((uint8_t)ch).y_shift[current_font_size] * FNSZ_ratio;
        float height   = console_font->truetype_glyph_map_.at((uint8_t)ch).height[current_font_size] * FNSZ_ratio;
        stbtt_aligned_quad *q = &console_font->truetype_glyph_map_.at((uint8_t)ch).character_quad[current_font_size];
        glvert->rgba          = col;
        glvert->position      = {{x + x_adjust, y - y_adjust, 0}};
        glvert++->texture_coordinates[0]   = {{q->s0, q->t0}};
        glvert->rgba                       = col;
        glvert->position                   = {{x + x_adjust + width, y - y_adjust, 0}};
        glvert++->texture_coordinates[0]   = {{q->s1, q->t0}};
        glvert->rgba                       = col;
        glvert->position                   = {{x + x_adjust + width, y - y_adjust - height, 0}};
        glvert++->texture_coordinates[0]   = {{q->s1, q->t1}};
        glvert->rgba                       = col;
        glvert->position                   = {{x + x_adjust, y - y_adjust - height, 0}};
        glvert++->texture_coordinates[0]   = {{q->s0, q->t1}};
    }
    else // spritesheet font
    {
        uint8_t px = (uint8_t)ch % 16;
        uint8_t py = 15 - (uint8_t)ch / 16;

        float tx1 = (px)*console_font->font_image_->width_ratio_;
        float tx2 = (px + 1) * console_font->font_image_->width_ratio_;
        float ty1 = (py)*console_font->font_image_->height_ratio_;
        float ty2 = (py + 1) * console_font->font_image_->height_ratio_;

        glvert->rgba                       = col;
        glvert->position                   = {{x, y, 0}};
        glvert++->texture_coordinates[0]   = {{tx1, ty1}};
        glvert->rgba                       = col;
        glvert->position                   = {{x, y + FNSZ, 0}};
        glvert++->texture_coordinates[0]   = {{tx1, ty2}};
        glvert->rgba                       = col;
        glvert->position                   = {{x + FNSZ, y + FNSZ, 0}};
        glvert++->texture_coordinates[0]   = {{tx2, ty2}};
        glvert->rgba                       = col;
        glvert->position                   = {{x + FNSZ, y, 0}};
        glvert++->texture_coordinates[0]   = {{tx2, ty1}};
    }
}

// Add characters to current render unit; return total number of verts added
static uint16_t AddText(float x, float y, const char *s, RGBAColor col, RendererVertex *&runit)
{
    bool draw_cursor = (s == input_line && console_cursor < 16);
    uint16_t verts_added = 0;
    int pos = 0;
    for (; *s; s++, pos++)
    {
        AddChar(x, y, *s, runit, col);
        verts_added += 4;

        if (console_font->definition_->type_ == kFontTypeTrueType)
        {
            if (*(s + 1))
            {
                x += (float)stbtt_GetGlyphKernAdvance(console_font->truetype_info_, console_font->GetGlyphIndex(*s),
                                                      console_font->GetGlyphIndex(*(s + 1))) *
                     console_font->truetype_kerning_scale_[current_font_size] * FNSZ_ratio / pixel_aspect_ratio.f_;
            }
        }

        if (pos == input_position && draw_cursor)
        {
            AddChar(x, y, 95, runit, col);
            verts_added += 4;
            draw_cursor = false;
        }

        x += XMUL;

        if (x >= current_screen_width)
            break;
    }

    if (draw_cursor)
    {
        AddChar(x, y, 95, runit, col);
        verts_added += 4;
    }

    return verts_added;
}

void ConsoleSetupFont(void)
{
    if (!console_font)
    {
        FontDefinition *DEF = fontdefs.Lookup("CON_FONT_2");
        if (!DEF)
            FatalError("CON_FONT_2 definition missing from DDFFONT!\n");
        console_font = hud_fonts.Lookup(DEF);
        EPI_ASSERT(console_font);
        console_font->Load();
    }

    if (!endoom_font)
    {
        FontDefinition *DEF = fontdefs.Lookup("ENDFONT");
        if (!DEF)
            FatalError("ENDFONT definition missing from DDFFONT!\n");
        endoom_font = hud_fonts.Lookup(DEF);
        EPI_ASSERT(endoom_font);
        endoom_font->Load();
    }

    if (!console_style)
    {
        StyleDefinition *def = styledefs.Lookup("CONSOLE");
        if (!def)
            def = default_style;
        console_style = hud_styles.Lookup(def);
    }

    CalcSizes();
}

void ConsoleDrawer(void)
{
    ConsoleSetupFont();

    if (console_visible == kConsoleNotVisible && !console_wipe_active)
        return;

    // -- background --

    StartUnitBatch(false);

    int CON_GFX_HT = (current_screen_height * 3 / 5);

    int y = current_screen_height;

    if (console_wipe_active)
    {
        y = (int)((float)y - CON_GFX_HT * HMM_Lerp(old_console_wipe_position, fractional_tic, console_wipe_position) /
                                 kConsoleWipeTics);
    }
    else
        y = y - CON_GFX_HT;

    if (console_style->background_image_ != nullptr)
    {
        const Image *img = console_style->background_image_;

        HUDRawImage(0, y, current_screen_width, y + CON_GFX_HT, img, 0.0, 0.0, img->Right(), img->Top(),
                    console_style->definition_->bg_.translucency_, kRGBANoValue, 0, 0);
    }
    else
    {
        SolidBox(0, y, current_screen_width, current_screen_height - y,
                 console_style->definition_->bg_.colour_ != kRGBANoValue ? console_style->definition_->bg_.colour_
                                                                         : kRGBABlack,
                 console_style->definition_->bg_.translucency_);
    }

    y += FNSZ / 4 + (console_font->definition_->type_ == kFontTypeTrueType ? FNSZ : 0);

    // -- input line --

    int bottom_y = y;

    // -- text lines --
    bool draw_endoom = false;
    RendererVertex *console_glvert = nullptr;
    int console_verts = 0;

    y = bottom_y + ((FNSZ / 2) + (bottom_row == -1 ? FNSZ : 0));
    // First pass, draw ENDOOM background colors if needed
    for (int i = HMM_MAX(0, bottom_row); i < kMaximumConsoleLines; i++)
    {
        ConsoleLine *CL = console_lines[i];

        if (!CL)
            break;

        if (CL->endoom_bytes_.size() == kENDOOMBytesPerLine && CL->line_.empty())
        {
            if (!draw_endoom)
            {
                console_glvert =
                    BeginRenderUnit(GL_QUADS, kENDOOMTotalVerts, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);
                draw_endoom = true;
            }

            int x = 0;
            int enwidth = RoundToInteger((float)endoom_font->image_monospace_width_ *
                ((float)FNSZ / endoom_font->image_monospace_width_) / 2) / 2;
            for (int j = 1; j < kENDOOMBytesPerLine; j+=2)
            {
                RGBAColor col = kENDOOMColors[(CL->endoom_bytes_[j] >> 4) & 7];

                console_glvert->rgba                   = col;
                console_glvert++->position             = {{(float)(x - enwidth), (float)y, 0}};
                console_glvert->rgba                   = col;
                console_glvert++->position             = {{(float)(x - enwidth), (float)(y + FNSZ), 0}};
                console_glvert->rgba                   = col;
                console_glvert++->position             = {{(float)(x + enwidth), (float)(y + FNSZ), 0}};
                console_glvert->rgba                   = col;
                console_glvert++->position               = {{(float)(x + enwidth), (float)y, 0}};

                x += enwidth * 2;
                console_verts += 4;

                if (x >= current_screen_width)
                    break;
            }
        }

        y += FNSZ;

        if (y >= current_screen_height)
            break;
    }

    y = bottom_y + ((FNSZ / 2) + (bottom_row == -1 ? FNSZ : 0));
    // Second pass (if drawing ENDOOM); draw ASCII characters
    if (draw_endoom)
    {
        // Finish previous unit
        EndRenderUnit(console_verts);
        console_verts = 0;
        console_glvert = BeginRenderUnit(GL_QUADS, kENDOOMTotalVerts, GL_MODULATE, ImageCache(endoom_font->font_image_, true, 
            (const Colormap *)0, true), (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingMasked);
        int enwidth = RoundToInteger((float)endoom_font->image_monospace_width_ *
            ((float)FNSZ / endoom_font->image_monospace_width_) / 2);

        for (int i = HMM_MAX(0, bottom_row); i < kMaximumConsoleLines; i++)
        {
            ConsoleLine *CL = console_lines[i];

            if (!CL)
                break;

            if (CL->endoom_bytes_.size() == kENDOOMBytesPerLine && CL->line_.empty())
            {
                int x = 0;
                for (int j = 0; j < kENDOOMBytesPerLine; j+=2)
                {
                    uint8_t ch = CL->endoom_bytes_[j];
                    uint8_t info = CL->endoom_bytes_[j+1];
                    RGBAColor col = kENDOOMColors[info & 15];

                    // blink
                    if ((info & 128) && console_cursor >= 16)
                    {
                        x += enwidth;
                        continue;
                    }

                    uint8_t px = ch % 16;
                    uint8_t py = 15 - ch / 16;

                    float tx1 = (px)*endoom_font->font_image_->width_ratio_;
                    float tx2 = (px + 1) * endoom_font->font_image_->width_ratio_;

                    float ty1 = (py)*endoom_font->font_image_->height_ratio_;
                    float ty2 = (py + 1) * endoom_font->font_image_->height_ratio_;

                    console_glvert->rgba                   = col;
                    console_glvert->texture_coordinates[0] = {{tx1, ty1}};
                    console_glvert++->position             = {{(float)(x - enwidth), (float)y, 0}};
                    console_glvert->rgba                   = col;
                    console_glvert->texture_coordinates[0] = {{tx1, ty2}};
                    console_glvert++->position             = {{(float)(x - enwidth), (float)(y + FNSZ), 0}};
                    console_glvert->rgba                   = col;
                    console_glvert->texture_coordinates[0] = {{tx2, ty2}};
                    console_glvert++->position             = {{(float)(x + enwidth), (float)(y + FNSZ), 0}};
                    console_glvert->rgba                   = col;
                    console_glvert->texture_coordinates[0] = {{tx2, ty1}};
                    console_glvert++->position               = {{(float)(x + enwidth), (float)y, 0}};

                    x += enwidth;
                    console_verts += 4;

                    if (x >= current_screen_width)
                        break;
                }
            }

            y += FNSZ;

            if (y >= current_screen_height)
                break;
        }
        EndRenderUnit(console_verts);
        console_verts = 0;
    }

    console_glvert = StartText();

    // Third pass, draw regular text
    if (bottom_row == -1)
    {
        y = bottom_y;
        console_verts += AddText(0, y, ">", kRGBAMagenta, console_glvert);

        if (command_history_position >= 0)
        {
            std::string text = cmd_history[command_history_position]->c_str();

            if (console_cursor < 16)
                text.append("_");

            console_verts += AddText(XMUL, y, text.c_str(), kRGBAMagenta, console_glvert);
        }
        else
        {
            console_verts += AddText(XMUL, y, input_line, kRGBAMagenta, console_glvert);
        }
    }

    y = bottom_y + ((FNSZ / 2) + (bottom_row == -1 ? FNSZ : 0));
    for (int i = HMM_MAX(0, bottom_row); i < kMaximumConsoleLines; i++)
    {
        ConsoleLine *CL = console_lines[i];

        if (!CL)
            break;

        if (CL->endoom_bytes_.empty() && !CL->line_.empty())
        {
            console_verts += AddText(0, y, CL->line_.c_str(), CL->color_, console_glvert);
        }

        y += FNSZ;

        if (y >= current_screen_height)
            break;
    }

    if (console_verts > 0)
        EndRenderUnit(console_verts);

    FinishUnitBatch();
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
    if (ctrl)
        return 0;

    if (key < 32 || key > 126)
        return 0;

    if (!shift)
        return (char)key;

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

static void ListCompletions(std::vector<const char *> &list, int word_len, int max_row, RGBAColor color)
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
            if (last_ja == dotpos[-1])
                continue;

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
            ConsoleMessage(kConsoleOnly, "  %s\n", name);
            max_row--;
            continue;
        }

        if (buf_len + 1 + n_len > max_col)
        {
            ConsoleMessageColor(color);
            ConsoleMessage(kConsoleOnly, "  %s\n", buffer);
            max_row--;

            buf_len         = 0;
            buffer[buf_len] = 0;

            if (max_row <= 0)
            {
                ConsoleMessageColor(color);
                ConsoleMessage(kConsoleOnly, "  etc...\n");
                break;
            }
        }

        if (buf_len > 0)
            buffer[buf_len++] = ' ';

        strcpy(buffer + buf_len, name);

        buf_len += n_len;
    }

    if (buf_len > 0)
    {
        ConsoleMessageColor(color);
        ConsoleMessage(kConsoleOnly, "  %s\n", buffer);
    }
}

static void TabComplete(void)
{
    EditHistory();

    // check if we are positioned after a word
    {
        if (input_position == 0)
            return;

        if (epi::IsDigitASCII(input_line[0]))
            return;

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

    int num_cmd = MatchConsoleCommands(match_cmds, input_line);
    int num_var = MatchConsoleVariables(match_vars, input_line);
    int num_key = 0; ///  E_MatchAllKeys(match_keys, input_line);

    // we have an unambiguous match, no need to print anything
    if (num_cmd + num_var + num_key == 1)
    {
        input_line[input_position] = save_ch;

        const char *name = (num_var > 0) ? match_vars[0] : (num_key > 0) ? match_keys[0] : match_cmds[0];

        EPI_ASSERT((int)strlen(name) >= input_position);

        for (name += input_position; *name; name++)
            InsertChar(*name);

        if (save_ch != ' ')
            InsertChar(' ');

        console_cursor = 0;
        return;
    }

    // show what we were trying to match
    ConsoleMessageColor(kRGBALightBlue);
    ConsoleMessage(kConsoleOnly, ">%s\n", input_line);

    input_line[input_position] = save_ch;

    if (num_cmd + num_var + num_key == 0)
    {
        ConsoleMessage(kConsoleOnly, "No matches.\n");
        return;
    }

    if (match_vars.size() > 0)
    {
        ConsoleMessage(kConsoleOnly, "%u Possible variables:\n", (int)match_vars.size());

        ListCompletions(match_vars, input_position, 7, kRGBASpringGreen);
    }

    if (match_keys.size() > 0)
    {
        ConsoleMessage(kConsoleOnly, "%u Possible keys:\n", (int)match_keys.size());

        ListCompletions(match_keys, input_position, 4, kRGBASpringGreen);
    }

    if (match_cmds.size() > 0)
    {
        ConsoleMessage(kConsoleOnly, "%u Possible commands:\n", (int)match_cmds.size());

        ListCompletions(match_cmds, input_position, 3, kRGBASpringGreen);
    }

    // Add as many common characters as possible
    // (e.g. "mou <TAB>" should add the s, e and _).

    // begin by lumping all completions into one list
    unsigned int i;

    for (i = 0; i < match_keys.size(); i++)
        match_vars.push_back(match_keys[i]);

    for (i = 0; i < match_cmds.size(); i++)
        match_vars.push_back(match_cmds[i]);

    int pos = input_position;

    for (;;)
    {
        char ch = match_vars[0][pos];
        if (!ch)
            return;

        for (i = 1; i < match_vars.size(); i++)
            if (match_vars[i][pos] != ch)
                return;

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
        if (bottom_row < -1)
            bottom_row = -1;
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
        if (input_position > 0)
            input_position--;

        console_cursor = 0;
        break;

    case kRightArrow:
        // Move cursor right one character
        if (command_history_position < 0)
        {
            if (input_line[input_position] != 0)
                input_position++;
        }
        else
        {
            if (cmd_history[command_history_position]->c_str()[input_position] != 0)
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
            ConsoleMessageColor(kRGBALightBlue);
            ConsoleMessage(kConsoleOnly, ">\n");
        }
        else
        {
            // Add it to history & draw it
            ConsoleAddCmdHistory(input_line);

            ConsoleMessageColor(kRGBALightBlue);
            ConsoleMessage(kConsoleOnly, ">%s\n", input_line);

            // Run it!
            TryConsoleCommand(input_line);
        }

        ConsoleClearInputLine();

        // Bring user back to current line after entering command
        bottom_row -= kMaximumConsoleLines;
        if (bottom_row < -1)
            bottom_row = -1;

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

        SetConsoleVisibility(kConsoleNotVisible);
        break;

    // Allow screenshotting of console too - Dasho
    case kFunction1:
    case kPrintScreen:
        DeferredScreenShot();
        break;

    default: {
        char ch = KeyToCharacter(key, shift, ctrl);

        // ignore non-printable characters
        if (ch == 0)
            break;

        // no room?
        if (input_position >= kMaximumConsoleInput - 1)
            break;

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

    if (epi::IsPrintASCII(sym))
        return sym;

    return -1;
}

bool ConsoleResponder(InputEvent *ev)
{
    if (ev->type != kInputEventKeyUp && ev->type != kInputEventKeyDown)
        return false;

    if (ev->type == kInputEventKeyDown && CheckKeyMatch(key_console, ev->value.key.sym))
    {
        ClearEventInput();
        SetConsoleVisibility(kConsoleToggle);
        // Dasho - Was "return true", now sneakily change the
        // input to key_pause and let GameResponder pause if appropriate
        // Will I regret this?
        ev->value.key.sym = key_pause;
        return false;
    }

    if (console_visible == kConsoleNotVisible)
        return false;

    int key = GetKeycode(ev);
    if (key < 0)
        return true;

    if (ev->type == kInputEventKeyUp)
    {
        if (key == repeat_key)
            repeat_countdown = 0;

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
            repeat_countdown = kConsoleKeyRepeatDelay;
            break;
        default:
            repeat_countdown = 0;
            break;
        }

        repeat_key = key;

        ConsoleHandleKey(key, keys_shifted, false);
    }

    return true; // eat all keyboard events
}

void ConsoleTicker(void)
{
    if (playing_movie)
        return;

    console_cursor = (console_cursor + 1) & 31;

    if (console_visible != kConsoleNotVisible)
    {
        // Handle repeating keys
        switch (scroll_direction)
        {
        case +1:
            if (bottom_row < kMaximumConsoleLines - 10)
                bottom_row++;
            break;

        case -1:
            if (bottom_row > -1)
                bottom_row--;
            break;

        default:
            if (repeat_countdown)
            {
                repeat_countdown -= 1;

                while (repeat_countdown <= 0)
                {
                    repeat_countdown += kConsoleKeyRepeatRate;
                    ConsoleHandleKey(repeat_key, keys_shifted, false);
                }
            }
            break;
        }
    }

    if (console_wipe_active)
    {
        old_console_wipe_position = console_wipe_position;
        if (console_visible == kConsoleNotVisible)
        {
            console_wipe_position--;
            if (console_wipe_position <= 0)
                console_wipe_active = false;
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
    SortConsoleVariables();

    console_used_lines   = 0;
    command_used_history = 0;

    bottom_row               = -1;
    command_history_position = -1;

    ConsoleClearInputLine();

    current_color = kRGBAGray;

    ConsoleAddLine("", false);
    ConsoleAddLine("", false);
}

void ConsoleStart(void)
{
    working_directory = home_directory;
    console_visible   = kConsoleNotVisible;
    console_cursor    = 0;
    StartupProgressMessage("Starting console...");
}

static char *GetHumanSize(uint32_t bytes, char *hrbytes)
{
    const char *suffix[] = {"B", "KB", "MB", "GB", "TB"};
    char  length   = sizeof(suffix) / sizeof(suffix[0]);
    int   i;

    for (i = 0; i < length; i++)
    {
        if (bytes < 1024)
            break;

        bytes >>= 10;
    }

    snprintf(hrbytes, 128, "%u %s", bytes, suffix[i]);
    return (hrbytes);
}

void ConsoleShowFPS(void)
{
    if (debug_fps.d_ == 0)
        return;

    StartUnitBatch(false);

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

    int32_t chars = 16;
    if (debug_fps.d_ >= 3)
    {
        chars = 20;
    }

    int x = current_screen_width - XMUL * chars;
    int y = current_screen_height - FNSZ * 2;

    if (abs(debug_fps.d_) >= 2)
        y -= FNSZ;

    if (abs(debug_fps.d_) >= 3)
    {
        y -= (FNSZ * 4);
#ifdef EDGE_SOKOL
        y -= (FNSZ * 7);
#endif
    }

    SolidBox(x, y, current_screen_width, current_screen_height, kRGBABlack, 0.5);

    x += XMUL;
    y = current_screen_height - FNSZ - FNSZ * (console_font->definition_->type_ == kFontTypeTrueType ? -0.5 : 0.5);

    RendererVertex *console_glvert = StartText();
    uint16_t console_verts = 0;

    // show average...

    char textbuf[128];

    if (debug_fps.d_ < 0)
        stbsp_sprintf(textbuf, " %6.2f ms", avg_shown);
    else
        stbsp_sprintf(textbuf, " %6.2f fps", 1000 / avg_shown);

    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    // show worst...

    if (abs(debug_fps.d_) >= 2)
    {
        y -= FNSZ;

        if (debug_fps.d_ < 0)
            stbsp_sprintf(textbuf, " %6.2f max", worst_shown);
        else if (worst_shown > 0)
            stbsp_sprintf(textbuf, " %6.2f min", 1000 / worst_shown);

        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
    }

    // show frame metrics...

    if (abs(debug_fps.d_) >= 3)
    {
        y -= FNSZ;
        stbsp_sprintf(textbuf, "%i runit", ec_frame_stats.draw_render_units);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;
        stbsp_sprintf(textbuf, "%i wall", ec_frame_stats.draw_wall_parts);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;
        stbsp_sprintf(textbuf, "%i plane", ec_frame_stats.draw_planes);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;
        stbsp_sprintf(textbuf, "%i thing", ec_frame_stats.draw_things);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;

#ifdef EDGE_SOKOL

        FrameStats stats;
        render_backend->GetFrameStats(stats);

        stbsp_sprintf(textbuf, "%i draw", stats.num_draw_);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;

        stbsp_sprintf(textbuf, "%i pipelines", stats.num_apply_pipeline_);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;

        stbsp_sprintf(textbuf, "%i bindings", stats.num_apply_bindings_);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;

        stbsp_sprintf(textbuf, "%i uniforms", stats.num_apply_uniforms_);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;

        stbsp_sprintf(textbuf, "%i buffers", stats.num_update_buffer_);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;

        char hrbytes[128];

        GetHumanSize(stats.size_apply_uniforms_, hrbytes);
        stbsp_sprintf(textbuf, "%s uniform size", hrbytes);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;
        GetHumanSize(stats.size_update_buffer_, hrbytes);
        stbsp_sprintf(textbuf, "%s buffer size", hrbytes);
        console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);
        y -= FNSZ;

#endif
    }
    EndRenderUnit(console_verts);
    FinishUnitBatch();
}

void ConsoleShowPosition(void)
{
    if (debug_position.d_ <= 0)
        return;

    StartUnitBatch(false);

    ConsoleSetupFont();

    Player *p = players[display_player];
    if (p == nullptr)
        return;

    char textbuf[128];

    int x = current_screen_width - XMUL * 16;
    int y;
    
    if (debug_fps.d_ <= 0)
        y = current_screen_height;
    else if (debug_fps.d_ == 1)
        y = current_screen_height - FNSZ * 3;
    else if (debug_fps.d_ == 2)
        y = current_screen_height - FNSZ * 4;
    else
        y = current_screen_height - FNSZ * 15;

    SolidBox(x, y - FNSZ * 10, XMUL * 16, FNSZ * 10 + 2, kRGBABlack, 0.5);

    RendererVertex *console_glvert = StartText();
    uint16_t console_verts = 0;

    x += XMUL;
    y -= FNSZ * (console_font->definition_->type_ == kFontTypeTrueType ? 0.25 : 1.25);
    stbsp_sprintf(textbuf, "    x: %d", (int)p->map_object_->x);
    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    y -= FNSZ;
    stbsp_sprintf(textbuf, "    y: %d", (int)p->map_object_->y);
    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    y -= FNSZ;
    stbsp_sprintf(textbuf, "    z: %d", (int)p->map_object_->z);
    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    y -= FNSZ;
    stbsp_sprintf(textbuf, "angle: %d", (int)epi::DegreesFromBAM(p->map_object_->angle_));
    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    y -= FNSZ;
    stbsp_sprintf(textbuf, "x mom: %.4f", p->map_object_->momentum_.X);
    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    y -= FNSZ;
    stbsp_sprintf(textbuf, "y mom: %.4f", p->map_object_->momentum_.Y);
    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    y -= FNSZ;
    stbsp_sprintf(textbuf, "z mom: %.4f", p->map_object_->momentum_.Z);
    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    y -= FNSZ;
    stbsp_sprintf(textbuf, "  sec: %d", (int)(p->map_object_->subsector_->sector - level_sectors));
    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    y -= FNSZ;
    stbsp_sprintf(textbuf, "  sub: %d", (int)(p->map_object_->subsector_ - level_subsectors));
    console_verts += AddText(x, y, textbuf, kRGBAWebGray, console_glvert);

    EndRenderUnit(console_verts);
    FinishUnitBatch();
}

void ConsoleENDOOM()
{
    ConsoleMessage(kConsoleOnly, "\n");
    for (int i = 0; i < kENDOOMLines; i++)
    {
        ConsoleAddENDOOMLine(quit_lines[i]);
    }
    ConsoleMessage(kConsoleOnly, "\n");
}

void CreateQuitScreen()
{
    int      length = 0;
    uint8_t *data   = nullptr;

    data = OpenPackOrLumpInMemory("ENDOOM", {".bin"}, &length);
    if (!data)
        data = OpenPackOrLumpInMemory("ENDTEXT", {".bin"}, &length);
    if (!data)
        data = OpenPackOrLumpInMemory("ENDBOOM", {".bin"}, &length);
    if (!data)
        data = OpenPackOrLumpInMemory("ENDSTRF", {".bin"}, &length);
    if (!data)
    {
        ConsoleMessage(kConsoleOnly, "No ENDOOM screen found!\n");
        return;
    }
    if (length != 4000)
    {
        ConsoleMessage(kConsoleOnly, "CreateQuitScreen: ENDOOM exists, but is malformed! (Length "
                     "not equal to 4000 bytes)\n");
        delete[] data;
        return;
    }

    for (int i = kENDOOMLines-1; i >= 0; i--)
    {
        quit_lines[i] = new ConsoleLine();
        quit_lines[i]->endoom_bytes_.resize(kENDOOMBytesPerLine);
        memcpy(quit_lines[i]->endoom_bytes_.data(), &data[i * kENDOOMBytesPerLine], kENDOOMBytesPerLine);
    }

    delete[] data;
}

void ClearConsole()
{
    for (int i = 0; i < console_used_lines; i++)
    {
        console_lines[i]->Clear();
    }
    console_used_lines = 0;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
