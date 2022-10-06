//----------------------------------------------------------------------------
//  EDGE Console Interface code.
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
//  Copyright (c) 1998       Randy Heit
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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
// grateful.  The original GPL'd version `c_consol.c' can be found
// in the contrib/ directory.
//

#include "i_defs.h"
#include "i_defs_gl.h"

#include "font.h"
#include "language.h"

#include "con_main.h"
#include "con_var.h"
#include "e_input.h"
#include "e_player.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "m_argv.h"
#include "r_draw.h"
#include "r_image.h"
#include "r_modes.h"
#include "r_wipe.h"
#include "w_wad.h"

#include <vector>


#define CON_WIPE_TICS  12


DEF_CVAR(debug_fps, "0", CVAR_ARCHIVE)
DEF_CVAR(debug_pos, "0", CVAR_ARCHIVE)

static visible_t con_visible;

// stores the console toggle effect
static int conwipeactive = 0;
static int conwipepos = 0;
static font_c *con_font;
static font_c *endoom_font;

// the console's background
static style_c *console_style;

static rgbcol_t current_color;

const rgbcol_t endoom_colors[16] = 
{
0x000000,
0x0000AA,
0x00AA00,
0x00AAAA,
0xAA0000,
0xAA00AA,
0xAA5500,
0xAAAAAA,
0x555555,
0x5555FF,
0x55FF55,
0x55FFFF,
0xFF5555,
0xFF55FF,
0xFFFF55,
0xFFFFFF
};

extern void E_ProgressMessage(const char *message);

#define T_GREY176  RGB_MAKE(176,176,176)
 
// TODO: console var
#define MAX_CON_LINES  160

class console_line_c
{
public:
	std::string line;

	rgbcol_t color;

	std::vector<byte> endoom_bytes;

public:
	console_line_c(const std::string& text, rgbcol_t _col = T_LGREY) :
		line(text), color(_col) 
	{ }

	console_line_c(const char *text, rgbcol_t _col = T_LGREY) :
		line(text), color(_col)
	{ }

	~console_line_c()
	{ }

	void Append(const char *text)
	{
		line = line + std::string(text);
	}

	void AppendEndoom(byte endoom_byte)
	{
		endoom_bytes.push_back(endoom_byte);
	}

	void Clear()
	{
		line.clear();
		endoom_bytes.clear();
	}
};

// entry [0] is the bottom-most one
static console_line_c * console_lines[MAX_CON_LINES];

static int con_used_lines = 0;
static bool con_partial_last_line = false;

// the console row that is displayed at the bottom of screen, -1 if cmdline
// is the bottom one.
static int bottomrow = -1;


#define MAX_CON_INPUT  255

static char input_line[MAX_CON_INPUT+2];
static int  input_pos = 0;

static int con_cursor;


#define KEYREPEATDELAY ((250 * TICRATE) / 1000)
#define KEYREPEATRATE  (TICRATE / 15)


// HISTORY

// TODO: console var to control history size
#define MAX_CMD_HISTORY  100

static std::string *cmd_history[MAX_CMD_HISTORY];

static int cmd_used_hist = 0;

// when browsing the cmdhistory, this shows the current index. Otherwise it's -1.
static int cmd_hist_pos = -1;


// always type ev_keydown
static int repeat_key;
static int repeat_countdown;

// tells whether shift is pressed, and pgup/dn should scroll to top/bottom of linebuffer.
static bool KeysShifted;

static bool TabbedLast;

static int scroll_dir;


static void CON_AddLine(const char *s, bool partial)
{
	if (con_partial_last_line)
	{
		SYS_ASSERT(console_lines[0]);

		console_lines[0]->Append(s);

		con_partial_last_line = partial;
		return;
	}

	// scroll everything up 

	delete console_lines[MAX_CON_LINES-1];

	for (int i = MAX_CON_LINES-1; i > 0; i--)
		console_lines[i] = console_lines[i-1];

	rgbcol_t col = current_color;

	if (col == T_LGREY && (prefix_icmp(s, "WARNING") == 0))
		col = T_ORANGE;

	console_lines[0] = new console_line_c(s, col);

	con_partial_last_line = partial;

	if (con_used_lines < MAX_CON_LINES)
		con_used_lines++;
}

static void CON_EndoomAddLine(byte endoom_byte, const char *s, bool partial)
{
	if (con_partial_last_line)
	{
		SYS_ASSERT(console_lines[0]);

		console_lines[0]->Append(s);

		console_lines[0]->AppendEndoom(endoom_byte);

		con_partial_last_line = partial;
		return;
	}

	// scroll everything up 

	delete console_lines[MAX_CON_LINES-1];

	for (int i = MAX_CON_LINES-1; i > 0; i--)
		console_lines[i] = console_lines[i-1];

	rgbcol_t col = current_color;

	if (col == T_LGREY && (prefix_icmp(s, "WARNING") == 0))
		col = T_ORANGE;

	console_lines[0] = new console_line_c(s, col);

	console_lines[0]->AppendEndoom(endoom_byte);

	con_partial_last_line = partial;

	if (con_used_lines < MAX_CON_LINES)
		con_used_lines++;
}

static void CON_AddCmdHistory(const char *s)
{
	// don't add if same as previous command
	if (cmd_used_hist > 0)
    	if (strcmp(s, cmd_history[0]->c_str()) == 0)
        	return;

	// scroll everything up 
	delete cmd_history[MAX_CMD_HISTORY-1];

	for (int i = MAX_CMD_HISTORY-1; i > 0; i--)
		cmd_history[i] = cmd_history[i-1];

	cmd_history[0] = new std::string(s);

	if (cmd_used_hist < MAX_CMD_HISTORY)
		cmd_used_hist++;
}

static void CON_ClearInputLine(void)
{
	input_line[0] = 0;
	input_pos = 0;
}


void CON_SetVisible(visible_t v)
{
	if (v == vs_toggle)
	{
		v = (con_visible == vs_notvisible) ? vs_maximal : vs_notvisible;

		scroll_dir = 0;
	}

	if (con_visible == v)
		return;

	con_visible = v;

	if (v == vs_maximal)
	{
		TabbedLast = false;
	}

	if (! conwipeactive)
	{
		conwipeactive = true;
		conwipepos = (v == vs_maximal) ? 0 : CON_WIPE_TICS;
	}
}


static void StripWhitespace(char *src)
{
	const char *start = src;

	while (*start && isspace(*start))
		start++;

	const char *end = src + strlen(src);

	while (end > start && isspace(end[-1]))
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

			CON_AddLine(line, false);

			line = dest;

			src++; continue;
		}

		// disregard if outside of extended ASCII range
		if (*src > 128 || *src < -128)
		{
			src++; continue;
		}

		*dest++ = *src++;
	}

	*dest++ = 0;

	if (line[0])
	{
		CON_AddLine(line, true);
	}

	current_color = T_LGREY;
}

static void EndoomSplitIntoLines(byte endoom_byte, char *src)
{
	char *dest = src;
	char *line = dest;

	while (*src)
	{
		if (*src == '\n')
		{
			*dest++ = 0;

			CON_AddLine(line, false);

			line = dest;

			src++; continue;
		}

		// disregard if outside of extended ASCII range
		if (*src > 128 || *src < -128)
		{
			src++; continue;
		}

		*dest++ = *src++;
	}

	*dest++ = 0;

	if (line[0])
	{
		CON_EndoomAddLine(endoom_byte, line, true);
	}

	current_color = T_LGREY;
}

void CON_Printf(const char *message, ...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, message);
	vsprintf(buffer, message, argptr);
	va_end(argptr);

	SplitIntoLines(buffer);
}

void CON_EndoomPrintf(byte endoom_byte, const char *message, ...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, message);
	vsprintf(buffer, message, argptr);
	va_end(argptr);

	EndoomSplitIntoLines(endoom_byte, buffer);
}

void CON_Message(const char *message,...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, message);

	// Print the message into a text string
	vsprintf(buffer, message, argptr);

	va_end(argptr);


	HU_StartMessage(buffer);

	strcat(buffer, "\n");

	SplitIntoLines(buffer);

}

void CON_MessageLDF(const char *lookup, ...)
{
	va_list argptr;
	char buffer[1024];

	lookup = language[lookup];

	va_start(argptr, lookup);
	vsprintf(buffer, lookup, argptr);
	va_end(argptr);

	HU_StartMessage(buffer);

	strcat(buffer, "\n");

	SplitIntoLines(buffer);
}

void CON_MessageColor(rgbcol_t col)
{
	current_color = col;
}



static int FNSZ;
static int XMUL;
static int YMUL;

static void CalcSizes()
{
	// Would it be preferable to store the reduced sizes in the font_c class? Hmm
	if (SCREENWIDTH <= 400)
	{
		FNSZ = 11; XMUL = 7; YMUL = 11;
	}
	else if (SCREENWIDTH < 640)
	{
		FNSZ = 13; XMUL = 9; YMUL = 13;
	}
	else
	{
		FNSZ = 16; XMUL = 11; YMUL = 16;
	}
}


static void SolidBox(int x, int y, int w, int h, rgbcol_t col, float alpha)
{
	if (alpha < 0.99f)
		glEnable(GL_BLEND);

	glColor4f(RGB_RED(col)/255.0, RGB_GRN(col)/255.0, RGB_BLU(col)/255.0, alpha);

	glBegin(GL_QUADS);

	glVertex2i(x,   y);
	glVertex2i(x,   y+h);
	glVertex2i(x+w, y+h);
	glVertex2i(x+w, y);

	glEnd();

	glDisable(GL_BLEND);
}

static void HorizontalLine(int y, rgbcol_t col)
{
	float alpha = 1.0f;

	SolidBox(0, y, SCREENWIDTH-1, 1, col, alpha);
}


static void DrawChar(int x, int y, char ch, rgbcol_t col)
{
	if (x + FNSZ < 0)
		return;

	float alpha = 1.0f;

	glColor4f(RGB_RED(col)/255.0f, RGB_GRN(col)/255.0f, 
				RGB_BLU(col)/255.0f, alpha);

	int px =      int((byte)ch) % 16;
	int py = 15 - int((byte)ch) / 16;

	float tx1 = (px  ) * con_font->font_image->ratio_w;
	float tx2 = (px+1) * con_font->font_image->ratio_w;
	float ty1 = (py  ) * con_font->font_image->ratio_h;
	float ty2 = (py+1) * con_font->font_image->ratio_h;
	int x_adjust = FNSZ;

	glBegin(GL_POLYGON);

	glTexCoord2f(tx1, ty1);
	glVertex2i(x, y);

	glTexCoord2f(tx1, ty2); 
	glVertex2i(x, y + FNSZ);

	glTexCoord2f(tx2, ty2);
	glVertex2i(x + x_adjust, y + FNSZ);

	glTexCoord2f(tx2, ty1);
	glVertex2i(x + x_adjust, y);

	glEnd();

}

static void DrawEndoomChar(int x, int y, char ch, rgbcol_t col, rgbcol_t col2, bool blink, GLuint tex_id)
{
	if (x + FNSZ < 0)
		return;

	float alpha = 1.0f;

	glDisable(GL_TEXTURE_2D);

	glColor4f(RGB_RED(col2)/255.0f, RGB_GRN(col2)/255.0f, 
						RGB_BLU(col2)/255.0f, alpha);

	glBegin(GL_QUADS);

	// Tweak x to prevent overlap of subsequent letters; may need to make this a bit more smart down the line
	glVertex2i(x + 4, y);

	glVertex2i(x + 4, y + FNSZ);

	glVertex2i(x + FNSZ - 3, y + FNSZ);

	glVertex2i(x + FNSZ - 3, y);

	glEnd();

	glEnable(GL_TEXTURE_2D);

	glColor4f(RGB_RED(col)/255.0f, RGB_GRN(col)/255.0f, 
				RGB_BLU(col)/255.0f, alpha);

	if (blink && con_cursor >= 16)
		ch = 0x20;

	int px =      int((byte)ch) % 16;
	int py = 15 - int((byte)ch) / 16;

	float tx1 = (px  ) * endoom_font->font_image->ratio_w;
	float tx2 = (px+1) * endoom_font->font_image->ratio_w;

	float ty1 = (py  ) * endoom_font->font_image->ratio_h;
	float ty2 = (py+1) * endoom_font->font_image->ratio_h;

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

// writes the text on coords (x,y) of the console
static void DrawText(int x, int y, const char *s, rgbcol_t col)
{
	// Always whiten the font when used with console output
	GLuint tex_id = W_ImageCache(con_font->font_image, true, (const colourmap_c *)0, true);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_id);
 
	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0);

	bool draw_cursor = false;

	if (s == input_line)
	{
		if (con_cursor < 16)
			draw_cursor = true;
	}

	int pos = 0;
	for (; *s; s++, pos++)
	{
		DrawChar(x, y, *s, col);

		if (pos == input_pos && draw_cursor)
		{
			DrawChar(x, y, 95, col);
			draw_cursor = false;
		}

		x += I_ROUND(FNSZ * (con_font->im_mono_width / con_font->im_char_height)) + I_ROUND(con_font->spacing);

		if (x >= SCREENWIDTH)
			break;
	}

	if (draw_cursor)
		DrawChar(x, y, 95, col);

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
}


static void EndoomDrawText(int x, int y, console_line_c *endoom_line)
{
	// Always whiten the font when used with console output
	GLuint tex_id = W_ImageCache(endoom_font->font_image, true, (const colourmap_c *)0, true);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_id);
 
	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0);

	for (int i=0; i < 80; i++)
	{
		byte info = endoom_line->endoom_bytes.at(i);

		DrawEndoomChar(x, y, endoom_line->line.at(i), endoom_colors[info & 15],
			endoom_colors[(info >> 4) & 7], info & 128, tex_id);

		x += XMUL + I_ROUND(endoom_font->spacing);

		if (x >= SCREENWIDTH)
			break;
	}

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
}

void CON_SetupFont(void)
{
	if (! con_font)
	{
		fontdef_c *DEF = fontdefs.Lookup("CON_FONT_2");
		if (!DEF)
			I_Error("CON_FONT_2 definition missing from DDFFONT!\n");
		con_font = hu_fonts.Lookup(DEF);
		SYS_ASSERT(con_font);
		con_font->Load();
	}

	if (! endoom_font)
	{
		fontdef_c *DEF = fontdefs.Lookup("ENDFONT");
		if (!DEF)
			I_Error("ENDFONT definition missing from DDFFONT!\n");
		endoom_font = hu_fonts.Lookup(DEF);
		SYS_ASSERT(endoom_font);
		endoom_font->Load();
	}

	if (! console_style)
	{
		styledef_c *def = styledefs.Lookup("CONSOLE");
		if (! def)
			def = default_style;
		console_style = hu_styles.Lookup(def);
	}

	CalcSizes();
}


void CON_Drawer(void)
{
	CON_SetupFont();

	if (con_visible == vs_notvisible && !conwipeactive)
		return;

	// -- background --

	int CON_GFX_HT = (SCREENHEIGHT * 3 / 5) / YMUL;

	CON_GFX_HT = (CON_GFX_HT - 1) * YMUL + YMUL * 3 / 4 - 2;


	int y = SCREENHEIGHT;

	if (conwipeactive)
		y = y - CON_GFX_HT * (conwipepos) / CON_WIPE_TICS;
	else
		y = y - CON_GFX_HT;

	if (console_style->bg_image != NULL)
	{
		const image_c *img = console_style->bg_image;

		HUD_RawImage(0, y, SCREENWIDTH, y + CON_GFX_HT, img,
			0.0, 0.0, IM_RIGHT(img), IM_TOP(img),
			console_style->def->bg.translucency,
			RGB_NO_VALUE, NULL, 0, 0);
	}
	else
	{
		SolidBox(0, y, SCREENWIDTH, SCREENHEIGHT - y, console_style->def->bg.colour != RGB_NO_VALUE ?
			console_style->def->bg.colour : RGB_MAKE(0,0,0), console_style->def->bg.translucency);
	}

	y += YMUL / 4;

	// -- input line --

	if (bottomrow == -1)
	{
		DrawText(0, y, ">", T_PURPLE);

		if (cmd_hist_pos >= 0)
		{
			std::string text = cmd_history[cmd_hist_pos]->c_str();

			if (con_cursor < 16)
				text.append("_");

			DrawText(XMUL, y, text.c_str(), T_PURPLE);
		}
		else
		{
			DrawText(XMUL, y, input_line, T_PURPLE);
		}

		y += YMUL;
	}

	y += YMUL / 2;

	// -- text lines --

	for (int i = MAX(0,bottomrow); i < MAX_CON_LINES; i++)
	{
		console_line_c *CL = console_lines[i];

		if (! CL)
			break;

		if (prefix_cmp(CL->line.c_str(), "--------") == 0)
			HorizontalLine(y + YMUL/2, CL->color);
		else if (CL->endoom_bytes.size() == 80 && CL->line.size() == 80) // 80 ENDOOM characters + newline
			EndoomDrawText(0, y, CL);
		else
			DrawText(0, y, CL->line.c_str(), CL->color);

		y += YMUL;

		if (y >= SCREENHEIGHT)
			break;
	}
}


static void GotoEndOfLine(void)
{
	if (cmd_hist_pos < 0)
		input_pos = strlen(input_line);
	else
		input_pos = strlen(cmd_history[cmd_hist_pos]->c_str());

	con_cursor = 0;
}


static void EditHistory(void)
{
	if (cmd_hist_pos >= 0)
	{
		strcpy(input_line, cmd_history[cmd_hist_pos]->c_str());

		cmd_hist_pos = -1;
	}
}


static void InsertChar(char ch)
{
	// make room for new character, shift the trailing NUL too

	for (int j = MAX_CON_INPUT-2; j >= input_pos; j--)
		input_line[j+1] = input_line[j];

	input_line[MAX_CON_INPUT-1] = 0;

	input_line[input_pos++] = ch;
}


static char KeyToCharacter(int key, bool shift, bool ctrl)
{
	if (ctrl)
		return 0;

	if (key < 32 || key > 126)
		return 0;

	if (! shift)
		return (char)key;

	// the following assumes a US keyboard layout
	switch (key)
	{
		case '1':  return '!';
		case '2':  return '@';
		case '3':  return '#';
		case '4':  return '$';
		case '5':  return '%';
		case '6':  return '^';
		case '7':  return '&';
		case '8':  return '*';
		case '9':  return '(';
		case '0':  return ')';

		case '`':  return '~';
		case '-':  return '_';
		case '=':  return '+';
		case '\\': return '|';
		case '[':  return '{';
		case ']':  return '}';
		case ';':  return ':';
		case '\'': return '"';
		case ',':  return '<';
		case '.':  return '>';
		case '/':  return '?';
		case '@':  return '\'';
	}

	return toupper(key);
}


static void ListCompletions(std::vector<const char *> & list,
                            int word_len, int max_row, rgbcol_t color)
{
	int max_col = SCREENWIDTH / XMUL - 4;
	max_col = CLAMP(24, max_col, 78);

	char buffer[200];
	int buf_len = 0;

	buffer[buf_len] = 0;

	char temp[200];
	char last_ja = 0;

	for (int i = 0; i < (int)list.size(); i++)
	{
		const char *name = list[i];
		int n_len = (int)strlen(name);

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
			CON_MessageColor(color);
			CON_Printf("  %s\n", name);
			max_row--;
			continue;
		}

		if (buf_len + 1 + n_len > max_col)
		{
			CON_MessageColor(color);
			CON_Printf("  %s\n", buffer);
			max_row--;

			buf_len = 0;
			buffer[buf_len] = 0;

			if (max_row <= 0)
			{
				CON_MessageColor(color);
				CON_Printf("  etc...\n");
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
		CON_MessageColor(color);
		CON_Printf("  %s\n", buffer);
	}
}


static void TabComplete(void)
{
	EditHistory();

	// check if we are positioned after a word
	{
		if (input_pos == 0)
			return ;

		if (isdigit(input_line[0]))
			return ;

		for (int i=0; i < input_pos; i++)
		{
			char ch = input_line[i];

			if (! (isalnum(ch) || ch == '_' || ch == '.'))
				return;
		}
	}

	char save_ch = input_line[input_pos];
	input_line[input_pos] = 0;

	std::vector<const char *> match_cmds;
	std::vector<const char *> match_vars;
	std::vector<const char *> match_keys;

	int num_cmd = CON_MatchAllCmds(match_cmds, input_line);
	int num_var = CON_MatchAllVars(match_vars, input_line);
	int num_key = 0; ///  E_MatchAllKeys(match_keys, input_line);

	// we have an unambiguous match, no need to print anything
	if (num_cmd + num_var + num_key == 1)
	{
		input_line[input_pos] = save_ch;

		const char *name = (num_var > 0) ? match_vars[0] :
		                   (num_key > 0) ? match_keys[0] : match_cmds[0];

		SYS_ASSERT((int)strlen(name) >= input_pos);

		for (name += input_pos; *name; name++)
			InsertChar(*name);

		if (save_ch != ' ')
			InsertChar(' ');

		con_cursor = 0;
		return;
	}

	// show what we were trying to match
	CON_MessageColor(T_LTBLUE);
	CON_Printf(">%s\n", input_line);

	input_line[input_pos] = save_ch;

	if (num_cmd + num_var + num_key == 0)
	{
		CON_Printf("No matches.\n");
		return;
	}

	if (match_vars.size() > 0)
	{
		CON_Printf("%u Possible variables:\n", (int)match_vars.size());

		ListCompletions(match_vars, input_pos, 7, RGB_MAKE(0,208,72));
	}

	if (match_keys.size() > 0)
	{
		CON_Printf("%u Possible keys:\n", (int)match_keys.size());

		ListCompletions(match_keys, input_pos, 4, RGB_MAKE(0,208,72));
	}

	if (match_cmds.size() > 0)
	{
		CON_Printf("%u Possible commands:\n", (int)match_cmds.size());

		ListCompletions(match_cmds, input_pos, 3, T_ORANGE);
	}

	// Add as many common characters as possible
	// (e.g. "mou <TAB>" should add the s, e and _).

	// begin by lumping all completions into one list
	unsigned int i;

	for (i = 0; i < match_keys.size(); i++)
		match_vars.push_back(match_keys[i]);

	for (i = 0; i < match_cmds.size(); i++)
		match_vars.push_back(match_cmds[i]);

	int pos = input_pos;

	for (;;)
	{
		char ch = match_vars[0][pos];
		if (! ch)
			return;

		for (i = 1; i < match_vars.size(); i++)
			if (match_vars[i][pos] != ch)
				return;

		InsertChar(ch);

		pos++;
	}
}


void CON_HandleKey(int key, bool shift, bool ctrl)
{
	switch (key)
	{
	case KEYD_RALT:
	case KEYD_RCTRL:
		// Do nothing
		break;
	
	case KEYD_RSHIFT:
		// SHIFT was pressed
		KeysShifted = true;
		break;
	
	case KEYD_PGUP:
		if (shift)
			// Move to top of console buffer
			bottomrow = MAX(-1, con_used_lines-10);
		else
			// Start scrolling console buffer up
			scroll_dir = +1;
		break;
	
	case KEYD_PGDN:
		if (shift)
			// Move to bottom of console buffer
			bottomrow = -1;
		else
			// Start scrolling console buffer down
			scroll_dir = -1;
		break;
	
    case KEYD_WHEEL_UP:
        bottomrow += 4;
        if (bottomrow > MAX(-1, con_used_lines-10))
            bottomrow = MAX(-1, con_used_lines-10);
        break;
    
    case KEYD_WHEEL_DN:
        bottomrow -= 4;
        if (bottomrow < -1)
            bottomrow = -1;
        break;

	case KEYD_HOME:
		// Move cursor to start of line
		input_pos = 0;
		con_cursor = 0;
		break;
	
	case KEYD_END:
		// Move cursor to end of line
		GotoEndOfLine();
		break;
	
	case KEYD_UPARROW:
		// Move to previous entry in the command history
		if (cmd_hist_pos < cmd_used_hist-1)
		{
			cmd_hist_pos++;
			GotoEndOfLine();
		}
		TabbedLast = false;
		break;
	
	case KEYD_DOWNARROW:
		// Move to next entry in the command history
		if (cmd_hist_pos > -1)
		{
			cmd_hist_pos--;
			GotoEndOfLine();
		}
		TabbedLast = false;
		break;

	case KEYD_LEFTARROW:
		// Move cursor left one character
		if (input_pos > 0)
			input_pos--;

		con_cursor = 0;
		break;
	
	case KEYD_RIGHTARROW:
		// Move cursor right one character
		if (cmd_hist_pos < 0)
		{
			if (input_line[input_pos] != 0)
				input_pos++;
		}
		else
		{
			if (cmd_history[cmd_hist_pos]->c_str()[input_pos] != 0)
				input_pos++;
		}
		con_cursor = 0;
		break;

	case KEYD_ENTER:
		EditHistory();

		// Execute command line (ENTER)
		StripWhitespace(input_line);

		if (strlen(input_line) == 0)
		{
			CON_MessageColor(T_LTBLUE);
			CON_Printf(">\n");
		}
		else
		{
			// Add it to history & draw it
			CON_AddCmdHistory(input_line);

			CON_MessageColor(T_LTBLUE);
			CON_Printf(">%s\n", input_line);
		
			// Run it!
			CON_TryCommand(input_line);
		}
	
		CON_ClearInputLine();

		// Bring user back to current line after entering command
        bottomrow -= MAX_CON_LINES;
        if (bottomrow < -1) bottomrow = -1;

		TabbedLast = false;
		break;
	
	case KEYD_BACKSPACE:
		// Erase character to left of cursor
		EditHistory();
	
		if (input_pos > 0)
		{
			input_pos--;

			// shift characters back
			for (int j = input_pos; j < MAX_CON_INPUT-2; j++)
				input_line[j] = input_line[j+1];
		}
	
		TabbedLast = false;
		con_cursor = 0;
		break;
	
	case KEYD_DELETE:
		// Erase charater under cursor
		EditHistory();
	
		if (input_line[input_pos] != 0)
		{
			// shift characters back
			for (int j = input_pos; j < MAX_CON_INPUT-2; j++)
				input_line[j] = input_line[j+1];
		}
	
		TabbedLast = false;
		con_cursor = 0;
		break;
	
	case KEYD_TAB:
		// Try to do tab-completion
		TabComplete();
		break;

	case KEYD_ESCAPE:
		// Close console, clear command line, but if we're in the
		// fullscreen console mode, there's nothing to fall back on
		// if it's closed.
		CON_ClearInputLine();
	
		cmd_hist_pos = -1;
		TabbedLast = false;
	
		CON_SetVisible(vs_notvisible);
		break;

	// Allow screenshotting of console too - Dasho
	case KEYD_F1:
	case KEYD_PRTSCR:
		G_DeferredScreenShot();
		break;

	default:
		{
			char ch = KeyToCharacter(key, shift, ctrl);

			// ignore non-printable characters
			if (ch == 0)
				break;

			// no room?
			if (input_pos >= MAX_CON_INPUT-1)
				break;

			EditHistory();
			InsertChar(ch);

			TabbedLast = false;
			con_cursor = 0;
		}
		break;
	}
}


static int GetKeycode(event_t *ev)
{
    int sym = ev->value.key.sym;

	switch (sym)
	{
		case KEYD_TAB:
		case KEYD_PGUP:
		case KEYD_PGDN:
		case KEYD_HOME:
		case KEYD_END:
		case KEYD_LEFTARROW:
		case KEYD_RIGHTARROW:
		case KEYD_BACKSPACE:
		case KEYD_DELETE:
		case KEYD_UPARROW:
		case KEYD_DOWNARROW:
		case KEYD_WHEEL_UP:
		case KEYD_WHEEL_DN:
		case KEYD_ENTER:
		case KEYD_ESCAPE:
		case KEYD_RSHIFT:
		case KEYD_F1:
		case KEYD_PRTSCR:
			return sym;

		default:
			break;
    }

    if (HU_IS_PRINTABLE(sym))
        return sym;

    return -1;
}


bool CON_Responder(event_t * ev)
{
	if (ev->type != ev_keyup && ev->type != ev_keydown)
		return false;

	if (ev->type == ev_keydown && E_MatchesKey(key_console, ev->value.key.sym))
	{
		E_ClearInput();
		CON_SetVisible(vs_toggle);
		return true;
	}

	if (con_visible == vs_notvisible)
		return false;

	int key = GetKeycode(ev);
	if (key < 0)
		return true;

	if (ev->type == ev_keyup)
	{
		if (key == repeat_key)
			repeat_countdown = 0;

		switch (key)
		{
			case KEYD_PGUP:
			case KEYD_PGDN:
				scroll_dir = 0;
				break;
			case KEYD_RSHIFT:
				KeysShifted = false;
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
			case KEYD_RIGHTARROW:
			case KEYD_LEFTARROW:
			case KEYD_UPARROW:
			case KEYD_DOWNARROW:
			case KEYD_SPACE:
			case KEYD_BACKSPACE:
			case KEYD_DELETE:
				repeat_countdown = KEYREPEATDELAY;
				break;
			default:
				repeat_countdown = 0;
				break;
		}

		repeat_key = key;

		CON_HandleKey(key, KeysShifted, false);
	}

	return true;  // eat all keyboard events
}


void CON_Ticker(void)
{
	con_cursor = (con_cursor + 1) & 31;

	if (con_visible != vs_notvisible)
	{
		// Handle repeating keys
		switch (scroll_dir)
		{
		case +1:
			if (bottomrow < MAX_CON_LINES-10)
				bottomrow++;
			break;

		case -1:
			if (bottomrow > -1)
				bottomrow--;
			break;  

		default:
			if (repeat_countdown)
			{
				repeat_countdown -= 1;

				while (repeat_countdown <= 0)
				{
					repeat_countdown += KEYREPEATRATE;
					CON_HandleKey(repeat_key, KeysShifted, false);
				}
			}
			break;
		}
	}

	if (conwipeactive)
	{
		if (con_visible == vs_notvisible)
		{
			conwipepos--;
			if (conwipepos <= 0)
				conwipeactive = false;
		}
		else
		{
			conwipepos++;
			if (conwipepos >= CON_WIPE_TICS)
				conwipeactive = false;
		}
	}
}


//
// Initialises the console
//
void CON_InitConsole(void)
{
	CON_SortVars();

	con_used_lines = 0;
	cmd_used_hist  = 0;

	bottomrow = -1;
	cmd_hist_pos = -1;

	CON_ClearInputLine();

	current_color = T_LGREY;

	CON_AddLine("", false);
	CON_AddLine("", false);
}


void CON_Start(void)
{
	con_visible = vs_notvisible;
	con_cursor  = 0;
	E_ProgressMessage("Starting console...");
}


void CON_ShowFPS(void)
{
	if (debug_fps.d == 0)
		return;

	CON_SetupFont();

	// -AJA- 2022: reworked for better accuracy, ability to show WORST time

	// get difference since last call
	static u32_t last_time = 0;
	u32_t time = I_GetMicros();
	u32_t diff = time - last_time;
	last_time  = time;

	// last computed value, state to compute average
	static float avg_shown   = 100.00;
	static float worst_shown = 100.00;

	static u32_t frames = 0;
	static u32_t total  = 0;
	static u32_t worst  = 0;

	// ignore a large diff or timer wrap-around
	if (diff < 1000000)
	{
		frames += 1;
		total  += diff;
		worst  = std::max(worst, diff);

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

	int x = SCREENWIDTH  - XMUL * 16;
	int y = SCREENHEIGHT - YMUL * 2;

	if (abs(debug_fps.d) >= 2)
		y -= YMUL;

	SolidBox(x, y, SCREENWIDTH, SCREENHEIGHT, RGB_MAKE(0,0,0), 0.5);

	x += XMUL;
	y = SCREENHEIGHT - YMUL - YMUL/2;

	// show average...

	char textbuf[128];

	if (debug_fps.d < 0)
		sprintf(textbuf, " %6.2f ms", avg_shown);
	else
		sprintf(textbuf, " %6.2f fps", 1000 / avg_shown);

	DrawText(x, y, textbuf, T_GREY176);

	// show worst...

	if (abs(debug_fps.d) >= 2)
	{
		y -= YMUL;

		if (debug_fps.d < 0)
			sprintf(textbuf, " %6.2f max", worst_shown);
		else if (worst_shown > 0)
			sprintf(textbuf, " %6.2f min", 1000 / worst_shown);

		DrawText(x, y, textbuf, T_GREY176);
	}
}


void CON_ShowPosition(void)
{
	if (debug_pos.d <= 0)
		return;

	CON_SetupFont();

	player_t *p = players[displayplayer];
	if (p == NULL)
		return;

	char textbuf[128];

	int x = SCREENWIDTH  - XMUL * 16;
	int y = SCREENHEIGHT - YMUL * 5;

	SolidBox(x, y - YMUL * 7, XMUL * 16, YMUL * 7 + 2, RGB_MAKE(0,0,0), 0.5);

	x += XMUL;
	y -= YMUL;
	sprintf(textbuf, "    x: %d", (int)p->mo->x);
	DrawText(x, y, textbuf, T_GREY176);

	y -= YMUL;
	sprintf(textbuf, "    y: %d", (int)p->mo->y);
	DrawText(x, y, textbuf, T_GREY176);

	y -= YMUL;
	sprintf(textbuf, "    z: %d", (int)p->mo->z);
	DrawText(x, y, textbuf, T_GREY176);

	y -= YMUL;
	sprintf(textbuf, "angle: %d", (int)ANG_2_FLOAT(p->mo->angle));
	DrawText(x, y, textbuf, T_GREY176);

	y -= YMUL;
	sprintf(textbuf, "  sec: %d", (int)(p->mo->subsector->sector - sectors));
	DrawText(x, y, textbuf, T_GREY176);

	y -= YMUL;
	sprintf(textbuf, "  sub: %d", (int)(p->mo->subsector - subsectors));
	DrawText(x, y, textbuf, T_GREY176);
}


void CON_PrintEndoom(int en_lump)
{
	int length;
	byte *data = (byte *)W_LoadLump(en_lump, &length);
	if (!data)
	{
		CON_Printf("CON_PrintEndoom: Failed to read data lump!\n");
		return;
	}
	if (length != 4000)
	{
		CON_Printf("CON_PrintEndoom: Lump exists, but is malformed! (Length not equal to 4000 bytes)\n");
		W_DoneWithLump(data);
		return;
	}
	int row_counter = 0;
	for (int i = 0; i < 4000; i+=2)
	{
		CON_EndoomPrintf(data[i+1], "%c", ((int)data[i] == 0 || (int)data[i] == 255) ? 0x20 : (int)data[i]); // Fix crumpled up ENDOOMs lol
		row_counter++;
		if (row_counter == 80) 
		{
			CON_Printf("\n");
			row_counter = 0;
		}
	}
	W_DoneWithLump(data);
}


void CON_ClearLines()
{
	for (int i = 0 ; i < con_used_lines; i++)
	{
		console_lines[i]->Clear();
	}
	con_used_lines = 0;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
