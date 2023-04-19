//----------------------------------------------------------------------------
//  EDGE Main Menu Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------
//
// See M_Option.C for text built menus.
//
// -KM- 1998/07/21 Add support for message input.
//

#include "i_defs.h"

#include "str_util.h"

#include "main.h"

#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dstrings.h"
#include "e_main.h"
#include "g_game.h"
#include "f_interm.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "m_netgame.h"
#include "m_option.h"
#include "m_random.h"
#include "n_network.h"
#include "p_setup.h"
#include "am_map.h"
#include "r_local.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_colormap.h"
#include "s_sound.h"
#include "s_music.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "w_wad.h"

#include "font.h"

#include "i_sdlinc.h"

// Menu navigation stuff
int key_menu_open;
int key_menu_up;
int key_menu_down;
int key_menu_left;
int key_menu_right;
int key_menu_select;
int key_menu_cancel;

// Program stuff
int key_screenshot;
int key_save_game;
int key_load_game;
int key_sound_controls;
int key_options_menu;
int key_quick_save;
int key_end_game;
int key_message_toggle;
int key_quick_load;
int key_quit_edge;
int key_gamma_toggle;

extern bool E_MatchesKey(int keyvar, int key);

extern cvar_c v_secbright;

//
// defaulted values
//

// Show messages has default, 0 = off, 1 = on
int showMessages;

extern cvar_c m_language;

int screen_hud;  // has default

static std::string msg_string;
static int msg_lastmenu;
static int msg_mode;

static std::string input_string;		

bool menuactive;

// timed message = no input from user
static bool msg_needsinput;

static void (* message_key_routine)(int response) = NULL;
static void (* message_input_routine)(const char *response) = NULL;

static int chosen_epi;

// SOUNDS
sfx_t * sfx_swtchn;
sfx_t * sfx_tink;
sfx_t * sfx_radio;
sfx_t * sfx_oof;
sfx_t * sfx_pstop;
sfx_t * sfx_stnmov;
sfx_t * sfx_pistol;
sfx_t * sfx_swtchx;
//
//  IMAGES USED
//
static const image_c *therm_l;
static const image_c *therm_m;
static const image_c *therm_r;
static const image_c *therm_o;

static const image_c *menu_loadg;
static const image_c *menu_saveg;
static const image_c *menu_svol;
static const image_c *menu_doom;
static const image_c *menu_newgame;
static const image_c *menu_skill;
static const image_c *menu_episode;
static image_c *menu_skull[2];
static const image_c *menu_readthis[2];

static style_c *menu_def_style;
static style_c *main_menu_style;
static style_c *episode_style;
static style_c *skill_style;
static style_c *load_style;
static style_c *save_style;
static style_c *exit_style;

//
//  SAVE STUFF
//
#define SAVESTRINGSIZE 	24

#define SAVE_SLOTS  8
#define SAVE_PAGES  100  // more would be rather unwieldy

// -1 = no quicksave slot picked!
int quickSaveSlot;
int quickSavePage;

// 25-6-98 KM Lots of save games... :-)
int save_page = 0;
int save_slot = 0;

// we are going to be entering a savegame string
static int saveStringEnter;

// which char we're editing
static int saveCharIndex;

// old save description before edit
static char saveOldString[SAVESTRINGSIZE];

typedef struct slot_extra_info_s
{
	bool empty;
	bool corrupt;

	char desc[SAVESTRINGSIZE];
	char timestr[32];
  
	char mapname[10];
	char gamename[32];
  
	int skill;
	int netgame;
	bool has_view;

	// Useful for drawing skull/cursor and possible other calculations
	float y;
	float width;
}
slot_extra_info_t;

static slot_extra_info_t ex_slots[SAVE_SLOTS];

// 98-7-10 KM New defines for slider left.
// Part of savegame changes.
#define SLIDERLEFT  -1
#define SLIDERRIGHT -2


//
// MENU TYPEDEFS
//
typedef struct menuitem_s
{
	// 0 = no cursor here, 1 = ok, 2 = arrows ok
	int status = 0;

  	// image for menu entry
	char patch_name[10] = {0};
	const image_c *image = nullptr;

  	// choice = menu item #.
  	// if status = 2, choice can be SLIDERLEFT or SLIDERRIGHT
	void (* select_func)(int choice) = nullptr;

	// hotkey in menu
	char alpha_key = 0;

	// Printed name test
	const char *name = nullptr;

	// Useful for drawing skull/cursor and possible other calculations
	int x = 0;
	int y = 0;
	float height = -1;
	float width = -1;
}
menuitem_t;

typedef struct menu_s
{
	// # of menu items
	int numitems;

  // previous menu
	struct menu_s *prevMenu;

	// menu items
	menuitem_t *menuitems;

	// style variable
	style_c **style_var;

	// draw routine
	void (* draw_func)(void);

	// x,y of menu
	int x, y;

	// last item user was on in menu
	int lastOn;
}
menu_t;

// menu item skull is on
static int itemOn;

// current menudef
static menu_t *currentMenu;

//
// PROTOTYPES
//
static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_ChooseSkill(int choice);
static void M_LoadGame(int choice);
static void M_SaveGame(int choice);

// 25-6-98 KM
extern void M_Options(int choice);
extern void M_F4SoundOptions(int choice);
static void M_LoadSavePage(int choice);
static void M_ReadThis(int choice);
static void M_ReadThis2(int choice);
void M_EndGame(int choice);

static void M_ChangeMessages(int choice);

static void M_FinishReadThis(int choice);
static void M_LoadSelect(int choice);
static void M_SaveSelect(int choice);
static void M_ReadSaveStrings(void);
static void M_QuickSave(void);
static void M_QuickLoad(void);

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawReadThis2(void);
static void M_DrawNewGame(void);
static void M_DrawEpisode(void);
static void M_DrawLoad(void);
static void M_DrawSave(void);

// static void M_DrawSaveLoadBorder(float x, float y, int len);
static void M_SetupNextMenu(menu_t * menudef);
void M_ClearMenus(void);
void M_StartControlPanel(void);
// static void M_StopMessage(void);

//
// DOOM MENU
//
typedef enum
{
	newgame = 0,
	options,
	loadgame,
	savegame,
	readthis,
	quitdoom,
	main_end
}
main_e;

static menuitem_t MainMenu[] =
{
	{1, "M_NGAME",   NULL, M_NewGame, 'n', language["MainNewGame"]},
	{1, "M_OPTION",  NULL, M_Options, 'o', language["MainOptions"]},
	{1, "M_LOADG",   NULL, M_LoadGame, 'l', language["MainLoadGame"]},
	{1, "M_SAVEG",   NULL, M_SaveGame, 's', language["MainSaveGame"]},
	// Another hickup with Special edition.
	{1, "M_RDTHIS",  NULL, M_ReadThis, 'r', language["MainReadThis"]},
	{1, "M_QUITG",   NULL, M_QuitEDGE, 'q', language["MainQuitGame"]}
};

static menu_t MainDef =
{
	main_end,
	NULL,
	MainMenu,
	&main_menu_style,
	M_DrawMainMenu,
	94, 64,
	0
};

//
// EPISODE SELECT
//
// -KM- 1998/12/16 This is generated dynamically.
//
static menuitem_t *EpisodeMenu = NULL;

static menuitem_t DefaultEpiMenu =
{
	1,  // status
	"Working",  // name
	NULL,  // image
	NULL,  // select_func
	'w',  // alphakey
	"DEFAULT"
};

static menu_t EpiDef =
{
	0,  //ep_end,  // # of menu items
	&MainDef,  // previous menu
	&DefaultEpiMenu,  // menuitem_t ->
	&episode_style,
	M_DrawEpisode,  // drawing routine ->
	48, 63,  // x,y
	0  // lastOn
};

static menuitem_t SkillMenu[] =
{
	{1, "M_JKILL", NULL, M_ChooseSkill, 'p', language["MenuDifficulty1"]},
	{1, "M_ROUGH", NULL, M_ChooseSkill, 'r', language["MenuDifficulty2"]},
	{1, "M_HURT",  NULL, M_ChooseSkill, 'h', language["MenuDifficulty3"]},
	{1, "M_ULTRA", NULL, M_ChooseSkill, 'u', language["MenuDifficulty4"]},
	{1, "M_NMARE", NULL, M_ChooseSkill, 'n', language["MenuDifficulty5"]}
};

static menu_t SkillDef =
{
	sk_numtypes,  // # of menu items
	&EpiDef,  // previous menu
	SkillMenu,  // menuitem_t ->
	&skill_style,
	M_DrawNewGame,  // drawing routine ->
	48, 63,  // x,y
	sk_medium  // lastOn
};

//
// OPTIONS MENU
//
typedef enum
{
	endgame,
	messages,
	scrnsize,
	option_empty1,
	mousesens,
	option_empty2,
	soundvol,
	opt_end
}
options_e;

//
// Read This! MENU 1 & 2
//

static menuitem_t ReadMenu1[] =
{
	{1, "", NULL, M_ReadThis2, 0}
};

static menu_t ReadDef1 =
{
	1,
	&MainDef,
	ReadMenu1,
	&menu_def_style,  // FIXME: maybe have READ_1 and READ_2 styles ??
	M_DrawReadThis1,
	1000, 1000,
	0
};

static menuitem_t ReadMenu2[] =
{
	{1, "", NULL, M_FinishReadThis, 0}
};

static menu_t ReadDef2 =
{
	1,
	&ReadDef1,
	ReadMenu2,
	&menu_def_style,  // FIXME: maybe have READ_1 and READ_2 styles ??
	M_DrawReadThis2,
	1000, 1000,
	0
};

//
// LOAD GAME MENU
//
// Note: upto 10 slots per page
//
static menuitem_t LoadingMenu[] =
{
	{2, "", NULL, M_LoadSelect, '1'},
	{2, "", NULL, M_LoadSelect, '2'},
	{2, "", NULL, M_LoadSelect, '3'},
	{2, "", NULL, M_LoadSelect, '4'},
	{2, "", NULL, M_LoadSelect, '5'},
	{2, "", NULL, M_LoadSelect, '6'},
	{2, "", NULL, M_LoadSelect, '7'},
	{2, "", NULL, M_LoadSelect, '8'},
	{2, "", NULL, M_LoadSelect, '9'},
	{2, "", NULL, M_LoadSelect, '0'}
};

static menu_t LoadDef =
{
	SAVE_SLOTS,
	&MainDef,
	LoadingMenu,
	&load_style,
	M_DrawLoad,
	30, 42,
	0
};

//
// SAVE GAME MENU
//
static menuitem_t SavingMenu[] =
{
	{2, "", NULL, M_SaveSelect, '1'},
	{2, "", NULL, M_SaveSelect, '2'},
	{2, "", NULL, M_SaveSelect, '3'},
	{2, "", NULL, M_SaveSelect, '4'},
	{2, "", NULL, M_SaveSelect, '5'},
	{2, "", NULL, M_SaveSelect, '6'},
	{2, "", NULL, M_SaveSelect, '7'},
	{2, "", NULL, M_SaveSelect, '8'},
	{2, "", NULL, M_SaveSelect, '9'},
	{2, "", NULL, M_SaveSelect, '0'}
};

static menu_t SaveDef =
{
	SAVE_SLOTS,
	&MainDef,
	SavingMenu,
	&save_style,
	M_DrawSave,
	30, 42,
	0
};

// 98-7-10 KM Chooses the page of savegames to view
void M_LoadSavePage(int choice)
{
	switch (choice)
	{
		case SLIDERLEFT:
			// -AJA- could use `OOF' sound...
			if (save_page == 0)
				return;

			save_page--;
			break;
      
		case SLIDERRIGHT:
			if (save_page >= SAVE_PAGES-1)
				return;

			save_page++;
			break;
	}

	S_StartFX(sfx_swtchn);
	M_ReadSaveStrings();
}

//
// Read the strings from the savegame files
//
// 98-7-10 KM Savegame slots increased
//
void M_ReadSaveStrings(void)
{
	int i, version;
  
	saveglobals_t *globs;

	for (i=0; i < SAVE_SLOTS; i++)
	{
		ex_slots[i].empty = false;
		ex_slots[i].corrupt = true;

		ex_slots[i].skill = -1;
		ex_slots[i].netgame = -1;
		ex_slots[i].has_view = false;

		ex_slots[i].desc[0] = 0;
		ex_slots[i].timestr[0] = 0;
		ex_slots[i].mapname[0] = 0;
		ex_slots[i].gamename[0] = 0;
    
		int slot = save_page * SAVE_SLOTS + i;
		std::filesystem::path fn(SV_FileName(SV_SlotName(slot), "head"));

		if (! SV_OpenReadFile(fn))
		{
			ex_slots[i].empty = true;
			ex_slots[i].corrupt = false;
			continue;
		}

		if (! SV_VerifyHeader(&version))
		{
			SV_CloseReadFile();
			continue;
		}

		globs = SV_LoadGLOB();

		// close file now -- we only need the globals
		SV_CloseReadFile();

		if (! globs)
			continue;

		// --- pull info from global structure ---

		if (!globs->game || !globs->level || !globs->description)
		{
			SV_FreeGLOB(globs);
			continue;
		}

		ex_slots[i].corrupt = false;

		Z_StrNCpy(ex_slots[i].gamename, globs->game,  32-1);
		Z_StrNCpy(ex_slots[i].mapname,  globs->level, 10-1);

		Z_StrNCpy(ex_slots[i].desc, globs->description, SAVESTRINGSIZE-1);

		if (globs->desc_date)
			Z_StrNCpy(ex_slots[i].timestr, globs->desc_date, 32-1);

		ex_slots[i].skill   = globs->skill;
		ex_slots[i].netgame = globs->netgame;

		SV_FreeGLOB(globs);
    
#if 0
		// handle screenshot
		if (globs->view_pixels)
		{
			int x, y;
      
			for (y=0; y < 100; y++)
				for (x=0; x < 160; x++)
				{
					save_screenshot[x][y] = SV_GetShort();
				}
		}
#endif
	}

	// fix up descriptions
	for (i=0; i < SAVE_SLOTS; i++)
	{
		if (ex_slots[i].corrupt)
		{
			strncpy(ex_slots[i].desc, language["Corrupt_Slot"],
					SAVESTRINGSIZE - 1);
			continue;
		}
		else if (ex_slots[i].empty)
		{
			strncpy(ex_slots[i].desc, language["EmptySlot"],
					SAVESTRINGSIZE - 1);
			continue;
		}
	}
}

int CenterMenuImage(const image_c *img)
{
	float CenterX = 160;
	CenterX -= IM_WIDTH(img) / 2;

	return CenterX;
}

//
// Center an image applying any SCALE and X_OFFSET from
// styles.ddf
int CenterMenuImage2(style_c *style, int text_type, const image_c *img)
{
	float CenterX = 160;
	float txtscale = 1.0;
	float gfxWidth = 0;

	if(style->def->text[text_type].scale)
	{
		txtscale = style->def->text[text_type].scale;
	}

	gfxWidth = IM_WIDTH(img) * txtscale;
	CenterX -= gfxWidth / 2;
	CenterX += style->def->text[text_type].x_offset;

	return CenterX;
}

int CenterMenuText(style_c *style, int text_type, const char *str)
{
	float CenterX = 160;
	float txtscale = 1.0;
	float txtWidth = 0;

	if(style->def->text[text_type].scale)
	{
		txtscale = style->def->text[text_type].scale;
	}
	txtWidth = style->fonts[text_type]->StringWidth(str) * txtscale;
	CenterX -= txtWidth / 2;
	CenterX += style->def->text[text_type].x_offset;

	// Should we also add "style->def->x_offset" here too?
	// CenterX += style->def->x_offset;

	return CenterX;
}

//the old one
static void M_DrawSaveLoadCommon_old(int row, int row2, style_c *style, float LineHeight)
{
	int y = LoadDef.y + LineHeight * row;

	slot_extra_info_t *info;

	char mbuffer[200];

	sprintf(mbuffer, "PAGE %d", save_page + 1);


	// -KM-  1998/06/25 This could quite possibly be replaced by some graphics...
	if (save_page > 0)
		HL_WriteText(style, styledef_c::T_TITLE, LoadDef.x - 4 + style->def->text[styledef_c::T_TITLE].x_offset, 
			y + style->def->text[styledef_c::T_TITLE].y_offset, "< PREV");

	HL_WriteText(style, styledef_c::T_TITLE, LoadDef.x + 94 - style->fonts[styledef_c::T_TITLE]->StringWidth(mbuffer) / 2 + 
		style->def->text[styledef_c::T_TITLE].x_offset, y + style->def->text[styledef_c::T_TITLE].y_offset, mbuffer);

	if (save_page < SAVE_PAGES-1)
		HL_WriteText(style, styledef_c::T_TITLE, LoadDef.x + 192 - style->fonts[styledef_c::T_TITLE]->StringWidth("NEXT >") +
		style->def->text[styledef_c::T_TITLE].x_offset, y + style->def->text[styledef_c::T_TITLE].y_offset, "NEXT >");
 
	info = ex_slots + itemOn;
	SYS_ASSERT(0 <= itemOn && itemOn < SAVE_SLOTS);

	if (saveStringEnter || info->empty || info->corrupt)
		return;

	// show some info about the savegame

	y = LoadDef.y + LineHeight * (row2 + 1);

	mbuffer[0] = 0;

	strcat(mbuffer, info->timestr);

	HL_WriteText(style, styledef_c::T_HELP, 310 - style->fonts[styledef_c::T_HELP]->StringWidth(mbuffer) + style->def->text[styledef_c::T_HELP].x_offset, 
		y + style->def->text[styledef_c::T_HELP].y_offset, mbuffer);

	y -= LineHeight;
    
	mbuffer[0] = 0;

	switch (info->skill)
	{
		case 0: strcat(mbuffer, language["MenuDifficulty1"]); break;
		case 1: strcat(mbuffer, language["MenuDifficulty2"]); break;
		case 2: strcat(mbuffer, language["MenuDifficulty3"]); break;
		case 3: strcat(mbuffer, language["MenuDifficulty4"]); break;
		default: strcat(mbuffer, language["MenuDifficulty5"]); break;
	}

	HL_WriteText(style, style->def->T_HELP, 310 - style->fonts[styledef_c::T_HELP]->StringWidth(mbuffer) + style->def->text[styledef_c::T_HELP].x_offset, 
		y + style->def->text[styledef_c::T_HELP].y_offset, mbuffer);

	y -= LineHeight;
  
	mbuffer[0] = 0;

	// FIXME: use Language entries
	switch (info->netgame)
	{
		case 0: strcat(mbuffer, "SP MODE"); break;
		case 1: strcat(mbuffer, "COOP MODE"); break;
		default: strcat(mbuffer, "DM MODE"); break;
	}
  
	HL_WriteText(style, styledef_c::T_HELP, 310 - style->fonts[styledef_c::T_HELP]->StringWidth(mbuffer) + style->def->text[styledef_c::T_HELP].x_offset, 
		y + style->def->text[styledef_c::T_HELP].y_offset, mbuffer);

	y -= LineHeight;
  
	mbuffer[0] = 0;

	strcat(mbuffer, info->mapname);

	HL_WriteText(style, styledef_c::T_HELP, 310 - style->fonts[styledef_c::T_HELP]->StringWidth(mbuffer) + style->def->text[styledef_c::T_HELP].x_offset, 
		y + style->def->text[styledef_c::T_HELP].y_offset, mbuffer);
}


static void M_DrawSaveLoadCommon(int row, int row2, style_c *style, float LineHeight)
{
	int y = 0; //LoadDef.y + LineHeight * row;
	int x = 0;
	float txtscale = 1.0;

	//TITLE.FONT="EDGE3"; // next page text
	//TEXT.FONT="EDGE3"; // save name & slot
	//ALT.FONT="EDGE3";  // when we edit the save name
	//HELP.FONT="EDGE3"; // save info text

	y = style->def->text[styledef_c::T_TITLE].y_offset;
	y += style->def->entry_spacing;
	x = style->def->text[styledef_c::T_TITLE].x_offset;
	slot_extra_info_t *info;

	char mbuffer[200];

	sprintf(mbuffer, "PAGE %d", save_page + 1);

	
	if (style->def->text[styledef_c::T_TITLE].scale)
		txtscale=style->def->text[styledef_c::T_TITLE].scale;

	if (save_page > 0)
		HL_WriteText(style, styledef_c::T_TITLE, x - 4, y, "< PREV");

	x += style->fonts[styledef_c::T_TITLE]->StringWidth("< PREV") * txtscale;
	x += 30;

	HL_WriteText(style, styledef_c::T_TITLE, x, y, mbuffer);

	x += style->fonts[styledef_c::T_TITLE]->StringWidth(mbuffer) * txtscale;
	x += 30;

	if (save_page < SAVE_PAGES-1)
		HL_WriteText(style, styledef_c::T_TITLE, x, y, "NEXT >");
 
	info = ex_slots + itemOn;
	SYS_ASSERT(0 <= itemOn && itemOn < SAVE_SLOTS);

	// show some info about the savegame

	//y = LoadDef.y + LineHeight * (row2 + 1);
	y = style->def->text[styledef_c::T_HELP].y_offset;
	y += style->def->entry_spacing;
	x = style->def->text[styledef_c::T_HELP].x_offset;

	if (style->def->text[styledef_c::T_HELP].scale)
		txtscale=style->def->text[styledef_c::T_HELP].scale;

	LineHeight = style->fonts[styledef_c::T_HELP]->NominalHeight() * txtscale;

	const colourmap_c *colmap = style->def->text[styledef_c::T_HELP].colmap;
	rgbcol_t col = V_GetFontColor(colmap);
	HUD_ThinBox(x - 5, y - 5, x + 95, y + 50, col);

	if (saveStringEnter || info->empty || info->corrupt)
		return;

	mbuffer[0] = 0;
	strcat(mbuffer, info->timestr);
	HL_WriteText(style, styledef_c::T_HELP, x, y, mbuffer);

	y += LineHeight + (LineHeight/2);
	y += style->def->entry_spacing;
	mbuffer[0] = 0;
	strcat(mbuffer, info->mapname);
	HL_WriteText(style, styledef_c::T_HELP, x, y, mbuffer);

	y += LineHeight + (LineHeight/2);
	y += style->def->entry_spacing;
	mbuffer[0] = 0;
	strcat(mbuffer, info->gamename);
	HL_WriteText(style, styledef_c::T_HELP, x, y, mbuffer);

	y += LineHeight + (LineHeight/2);
	y += style->def->entry_spacing;
	mbuffer[0] = 0;
	switch (info->skill)
	{
		case 0: strcat(mbuffer, language["MenuDifficulty1"]); break;
		case 1: strcat(mbuffer, language["MenuDifficulty2"]); break;
		case 2: strcat(mbuffer, language["MenuDifficulty3"]); break;
		case 3: strcat(mbuffer, language["MenuDifficulty4"]); break;
		default: strcat(mbuffer, language["MenuDifficulty5"]); break;
	}
	HL_WriteText(style, styledef_c::T_HELP, x, y, mbuffer);

/*
	y += LineHeight + (LineHeight/2);
	mbuffer[0] = 0;
	// FIXME: use Language entries
	switch (info->netgame)
	{
		case 0: strcat(mbuffer, "SP MODE"); break;
		case 1: strcat(mbuffer, "COOP MODE"); break;
		default: strcat(mbuffer, "DM MODE"); break;
	}
	HL_WriteText(style, styledef_c::T_HELP, x, y, mbuffer);
*/
	
}

//the new one
void M_DrawLoad(void)
{
	int i;
	int fontType;
	float txtscale = 1.0;
	float LineHeight;
	int TempX = 0;
	int TempY = 0;

	float old_alpha = HUD_GetAlpha();

	style_c *style = LoadDef.style_var[0];

	SYS_ASSERT(style);
	style->DrawBackground();

	if (! style->fonts[styledef_c::T_HEADER])
		fontType=styledef_c::T_TEXT;
	else
		fontType=styledef_c::T_HEADER;

	if (style->def->text[fontType].scale)
		txtscale=style->def->text[fontType].scale;

	HUD_SetAlpha(style->def->text[fontType].translucency);

	//1. Draw the header i.e. "Load Game"
	TempX = CenterMenuText(style, fontType, language["MainLoadGame"]);
	TempY = 5;
	TempY += style->def->text[fontType].y_offset;

	HL_WriteText(style, fontType, TempX, TempY, language["MainLoadGame"]);

/*
	const colourmap_c *colmap = style->def->text[fontType].colmap;
	if (custom_MenuMain == false) //text-based
	{
		TempX = CenterMenuText(style, fontType, language["MainLoadGame"]);
		TempY = 5;
		TempY += style->def->text[fontType].y_offset;

		HL_WriteText(style, fontType, TempX, TempY, language["MainLoadGame"]);
	}
	else //graphic based
	{
		TempX = CenterMenuImage2(style, fontType, menu_loadg);
		TempY = 5;
		TempY += style->def->text[fontType].y_offset;

		HUD_StretchImage(TempX, TempY,
				IM_WIDTH(menu_loadg) * txtscale, IM_HEIGHT(menu_loadg) * txtscale,menu_loadg,0.0,0.0,colmap);
	}
*/
	HUD_SetAlpha(old_alpha);

	TempX = 0;
	TempY = 0; 

	fontType=styledef_c::T_TEXT;
	if (style->def->text[fontType].scale)
		txtscale=style->def->text[fontType].scale;

	TempX += style->def->text[styledef_c::T_TEXT].x_offset;
	TempY += style->def->text[styledef_c::T_TEXT].y_offset;
	TempY += style->def->entry_spacing;

	rgbcol_t col = V_GetFontColor(style->def->text[styledef_c::T_TEXT].colmap);
	HUD_ThinBox(TempX - 5,TempY - 5, TempX + 175,  TempY + 115, col);

	//2. draw the save games
	for (i = 0; i < SAVE_SLOTS; i++)
	{
		fontType = styledef_c::T_TEXT;
		if (i == itemOn)
		{
			if (style->def->text[styledef_c::T_SELECTED].font)
				fontType = styledef_c::T_SELECTED;
		}

		LineHeight = style->fonts[fontType]->NominalHeight(); // * txtscale

		if (fontType == styledef_c::T_SELECTED)
		{
			// ttf_ref_yshift is important for TTF fonts.
			float y_shift = style->fonts[styledef_c::T_SELECTED]->ttf_ref_yshift; // * txtscale;
			
			HUD_SetAlpha(0.33f);
			HUD_SolidBox(TempX - 3, TempY - 2 + y_shift, TempX + 173, TempY + LineHeight + 2 + y_shift, col);
			HUD_SetAlpha(old_alpha);
		}
		HL_WriteText(style, fontType, TempX, 
				TempY - (LineHeight / 2), ex_slots[i].desc);
		TempY += LineHeight + (LineHeight/2);
		TempY += style->def->entry_spacing;
	}

	M_DrawSaveLoadCommon(i, i+1, load_style, LineHeight);
}


//
// 1998/07/10 KM Savegame slots increased
//
void M_DrawLoad_old(void)
{
	const image_c *L = W_ImageLookup("M_LSLEFT");
	const image_c *C = W_ImageLookup("M_LSCNTR");
	const image_c *R = W_ImageLookup("M_LSRGHT");
	int i;
	int y = LoadDef.y;

	style_c *style = LoadDef.style_var[0];

	SYS_ASSERT(style);
	style->DrawBackground();

	int fontType;
	float x = 72;
	if (! style->fonts[styledef_c::T_HEADER])
		fontType=styledef_c::T_TEXT;
	else
		fontType=styledef_c::T_HEADER;

	if (custom_MenuMain == false) //Yeah, this depends on what main menu is, so sue me.
	{
		if (style->def->entry_alignment == episode_style->def->C_CENTER)
			x = CenterMenuText(style, fontType, language["MainLoadGame"]);
		else
			x = 72;
		
		HL_WriteText(style, fontType, x + style->def->text[fontType].x_offset, 
			8 + style->def->text[fontType].y_offset, language["MainLoadGame"]);
	}
	else
	{
		const colourmap_c *colmap = style->def->text[fontType].colmap;
		if (menu_loadg->offset_x != 0.0f) //Only auto-center if no Xoffset 
			x = 72; //SaveDef.x; //cannot get away from the damn hardcoded value
		else
			x = CenterMenuImage(menu_loadg);
		
		HUD_DrawImage(x + style->def->text[fontType].x_offset, 8 + style->def->text[fontType].y_offset, menu_loadg, colmap);
	}


	// Use center box graphic for LineHeight unless the load game font text is actually taller
	// (this should only happen if the boxes aren't being drawn in theory)
	float LineHeight = IM_HEIGHT(C);
	if (style->fonts[styledef_c::T_TEXT]->NominalHeight() > LineHeight) 
		LineHeight = style->fonts[styledef_c::T_TEXT]->NominalHeight();
	float WidestLine = IM_WIDTH(C) * 24 + IM_WIDTH(L) + IM_WIDTH(R);

	for (i = 0; i < SAVE_SLOTS; i++)
	{
		if (custom_MenuMain==false)
		{
			x = LoadDef.x;
			HUD_StretchImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
				y + style->def->text[styledef_c::T_TEXT].y_offset,IM_WIDTH(L),IM_HEIGHT(L),L, 0.0, 0.0);
			x += IM_WIDTH(L);
			for (int j = 0; j < 24; j++, x += IM_WIDTH(C))
				HUD_StretchImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
					y + style->def->text[styledef_c::T_TEXT].y_offset,IM_WIDTH(C),IM_HEIGHT(C),C, 0.0, 0.0);

			HUD_StretchImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
				y + style->def->text[styledef_c::T_TEXT].y_offset,IM_WIDTH(R),IM_HEIGHT(R),R, 0.0, 0.0);
		}
		else
		{
			x = LoadDef.x;
			HUD_DrawImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
				y + style->def->text[styledef_c::T_TEXT].y_offset, L);
			x += IM_WIDTH(L);
			for (int j = 0; j < 24; j++, x += IM_WIDTH(C))
				HUD_DrawImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
					y + style->def->text[styledef_c::T_TEXT].y_offset, C);

			HUD_DrawImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
				y + style->def->text[styledef_c::T_TEXT].y_offset, R);
		}
		ex_slots[i].y = y;
		ex_slots[i].width = style->fonts[ex_slots[i].corrupt ? styledef_c::T_HELP : styledef_c::T_TEXT]->StringWidth(ex_slots[i].desc);
		if (ex_slots[i].width > WidestLine) 
			WidestLine = ex_slots[i].width;
		y += LineHeight + style->def->entry_spacing;
	}
	
	for (i = 0; i < SAVE_SLOTS; i++)
	{
		int textstyle = i == itemOn ? (style->def->text[styledef_c::T_SELECTED].font ? styledef_c::T_SELECTED : styledef_c::T_TEXT) : 
					styledef_c::T_TEXT;
		if (LineHeight == IM_HEIGHT(C))
		{
			if (style->def->entry_alignment == style->def->C_RIGHT)
				HL_WriteText(load_style, textstyle, LoadDef.x - 8 + WidestLine - (ex_slots[i].width) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
			else if (style->def->entry_alignment == style->def->C_CENTER)
				HL_WriteText(load_style, textstyle, LoadDef.x + (WidestLine / 2) - (ex_slots[i].width / 2) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
			else
				HL_WriteText(load_style, textstyle, LoadDef.x + 8 + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
		}
		else
		{
			if (style->def->entry_alignment == style->def->C_RIGHT)
				HL_WriteText(load_style, textstyle, LoadDef.x + WidestLine - ex_slots[i].width + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
			else if (style->def->entry_alignment == style->def->C_CENTER)
				HL_WriteText(load_style, textstyle, LoadDef.x + (WidestLine / 2) - (ex_slots[i].width / 2) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
			else
				HL_WriteText(load_style, textstyle, LoadDef.x + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
		}
	}
	image_c *cursor;
	if (style->def->cursor.cursor_string != "")
		cursor = NULL;
	else if (style->def->cursor.alt_cursor != "")
		cursor = (image_c *)W_ImageLookup(style->def->cursor.alt_cursor.c_str());
	else
		cursor = menu_skull[0];
	if (cursor)
	{
		short old_offset_x = cursor->offset_x;
		short old_offset_y = cursor->offset_y;
		cursor->offset_x = LineHeight == IM_HEIGHT(C) ? C->offset_x : 0;
		cursor->offset_y = LineHeight == IM_HEIGHT(C) ? C->offset_y : 0;
		if (style->def->cursor.force_offsets)
		{
			cursor->offset_x += old_offset_x;
			cursor->offset_y += old_offset_y;
		}
		cursor->offset_x -= style->def->text[styledef_c::T_TEXT].x_offset;
		cursor->offset_y += style->def->text[styledef_c::T_TEXT].y_offset;
		float TempHeight = MIN(LineHeight, IM_HEIGHT(cursor));
		float TempScale = 0;
		float TempWidth = 0;
		float TempSpacer = 0;
		TempScale = TempHeight / IM_HEIGHT(cursor);
		TempWidth = IM_WIDTH(cursor) * TempScale;
		if (!style->def->cursor.scaling)
		{
			ex_slots[itemOn].y -= (IM_HEIGHT(cursor) - TempHeight) / 2;
			TempHeight = IM_HEIGHT(cursor);
			TempWidth = IM_WIDTH(cursor);
		}
		TempSpacer = TempWidth * 0.2; // 20% of cursor graphic is our space
		float old_alpha = HUD_GetAlpha();
		HUD_SetAlpha(style->def->cursor.translucency);
		if (style->def->cursor.position == style->def->C_BOTH)
		{
			HUD_StretchImage(LoadDef.x + WidestLine + TempSpacer,ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
			HUD_StretchImage(LoadDef.x - TempWidth - TempSpacer,ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
		}
		else if (style->def->cursor.position == style->def->C_CENTER)
		{
			if (style->def->cursor.border)
				HUD_StretchImage(LoadDef.x,ex_slots[itemOn].y,WidestLine,LineHeight,cursor, 0.0, 0.0);
			else
				HUD_StretchImage(LoadDef.x + (WidestLine / 2) - (TempWidth / 2),ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
		}
		else if (style->def->cursor.position == style->def->C_RIGHT)
			HUD_StretchImage(LoadDef.x + WidestLine + TempSpacer,ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
		else
			HUD_StretchImage(LoadDef.x - TempWidth - TempSpacer,ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
		cursor->offset_x = old_offset_x;
		cursor->offset_y = old_offset_y;
		HUD_SetAlpha(old_alpha);
	}
	else
	{
		float old_alpha = HUD_GetAlpha();
		HUD_SetAlpha(style->def->cursor.translucency);
		float TempWidth = style->fonts[styledef_c::T_TEXT]->StringWidth(style->def->cursor.cursor_string.c_str()) * style->def->text[styledef_c::T_TEXT].scale;
		float TempSpacer = style->fonts[styledef_c::T_TEXT]->CharWidth(style->def->cursor.cursor_string[0]) * style->def->text[styledef_c::T_TEXT].scale * 0.5;
		if (LineHeight == IM_HEIGHT(C))
		{
			TempSpacer *= 2;
			if (style->def->cursor.position == style->def->C_BOTH)
			{
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x - TempWidth - TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + WidestLine + TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			}
			else if (style->def->cursor.position == style->def->C_CENTER)
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + (WidestLine/2) - (TempWidth / 2) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			else if (style->def->cursor.position == style->def->C_RIGHT)
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + WidestLine + TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			else
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x - TempWidth - TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
		}
		else
		{
			if (style->def->cursor.position == style->def->C_BOTH)
			{
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x - TempWidth - TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + WidestLine + TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			}
			else if (style->def->cursor.position == style->def->C_CENTER)
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + (WidestLine/2) - (TempWidth / 2) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			else if (style->def->cursor.position == style->def->C_RIGHT)
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + WidestLine + TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			else
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x - TempWidth - TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
		}
		HUD_SetAlpha(old_alpha);
	}

	M_DrawSaveLoadCommon(i, i+1, load_style, LineHeight);
}

//
// User wants to load this game
//
// 98-7-10 KM Savegame slots increased
//
void M_LoadSelect(int choice)
{
	if (choice < 0 || ex_slots[choice].empty)
	{
		M_LoadSavePage(choice);
		return;
	}

	G_DeferredLoadGame(save_page * SAVE_SLOTS + choice);
	M_ClearMenus();
}

//
// Selected from DOOM menu
//
void M_LoadGame(int choice)
{
	if (netgame)
	{
		M_StartMessage(language["NoLoadInNetGame"], NULL, false);
		return;
	}

	M_SetupNextMenu(&LoadDef);
	M_ReadSaveStrings();
}




//the new one
void M_DrawSave(void)
{
	int i;
	int fontType;
	float txtscale = 1.0;
	float LineHeight;
	int TempX = 0;
	int TempY = 0;

	float old_alpha = HUD_GetAlpha();

	style_c *style = SaveDef.style_var[0];

	SYS_ASSERT(style);
	style->DrawBackground();

	if (! style->fonts[styledef_c::T_HEADER])
		fontType=styledef_c::T_TEXT;
	else
		fontType=styledef_c::T_HEADER;

	if (style->def->text[fontType].scale)
		txtscale=style->def->text[fontType].scale;

	HUD_SetAlpha(style->def->text[fontType].translucency);

	//1. Draw the header i.e. "Load Game"
	TempX = CenterMenuText(style, fontType, language["MainSaveGame"]);
	TempY = 5;
	TempY += style->def->text[fontType].y_offset;

	HL_WriteText(style, fontType, TempX, TempY, language["MainSaveGame"]);

	HUD_SetAlpha(old_alpha);

	TempX = 0;
	TempY = 0; 
	TempX += style->def->text[styledef_c::T_TEXT].x_offset;
	TempY += style->def->text[styledef_c::T_TEXT].y_offset;
	TempY += style->def->entry_spacing;
	fontType=styledef_c::T_TEXT;
	
	rgbcol_t col = V_GetFontColor(style->def->text[styledef_c::T_TEXT].colmap);
	HUD_ThinBox(TempX - 5,TempY - 5, TempX + 175,  TempY + 115, col);

	//2. draw the save games
	for (i = 0; i < SAVE_SLOTS; i++)
	{
		fontType = styledef_c::T_TEXT;
		if (i == itemOn)
		{
			if (style->def->text[styledef_c::T_SELECTED].font)
				fontType = styledef_c::T_SELECTED;
		}
		
		LineHeight = style->fonts[fontType]->NominalHeight(); // * txtscale

		if (fontType == styledef_c::T_SELECTED)
		{
			// ttf_ref_yshift is important for TTF fonts.
			float y_shift = style->fonts[styledef_c::T_SELECTED]->ttf_ref_yshift; // * txtscale;
			
			HUD_SetAlpha(0.33f);
			HUD_SolidBox(TempX - 3, TempY - 2 + y_shift, TempX + 173, TempY + LineHeight + 2 + y_shift, col);
			HUD_SetAlpha(old_alpha);
		}

		int len = 0;
		bool entering_save = false;
		if (saveStringEnter && i == save_slot)
		{
			entering_save = true;
			if (! style->fonts[styledef_c::T_ALT])
				fontType=styledef_c::T_TEXT;
			else
				fontType=styledef_c::T_ALT;

			if (style->def->text[fontType].scale)
				txtscale=style->def->text[fontType].scale;
			len = style->fonts[fontType]->StringWidth(ex_slots[save_slot].desc) * txtscale;
		}

		HL_WriteText(style, fontType, TempX, 
				TempY - (LineHeight / 2), ex_slots[i].desc);
		
		if (entering_save)
		{
			HL_WriteText(style, fontType, TempX + len, 
				TempY - (LineHeight / 2), "_");
		}

		TempY += LineHeight + (LineHeight/2);
		TempY += style->def->entry_spacing;
	}

/*
	col = V_GetFontColor(style->def->text[styledef_c::T_HELP].colmap);
	int ScreenshotX =style->def->text[styledef_c::T_HELP].x_offset - 5;
	int ScreenshotY =style->def->text[styledef_c::T_HELP].y_offset;
	ScreenshotY += 55;
	HUD_ThinBox(ScreenshotX, ScreenshotY, ScreenshotX + 100, ScreenshotY + 60, col);
	HUD_RenderWorld(ScreenshotX + 5, ScreenshotY + 5, 
		90,  50, players[0]->mo, 0);
*/
	M_DrawSaveLoadCommon(i, i+1, save_style, LineHeight);
}

//
// 98-7-10 KM Savegame slots increased
//
void M_DrawSave_old(void)
{
	const image_c *L = W_ImageLookup("M_LSLEFT");
	const image_c *C = W_ImageLookup("M_LSCNTR");
	const image_c *R = W_ImageLookup("M_LSRGHT");
	int i;
	int y = SaveDef.y;

	style_c *style = SaveDef.style_var[0];

	SYS_ASSERT(style);
	style->DrawBackground();

	int fontType;
	float x = 72;
	if (! style->fonts[styledef_c::T_HEADER])
		fontType=styledef_c::T_TEXT;
	else
		fontType=styledef_c::T_HEADER;

	if (custom_MenuMain == false) //Yeah, this depends on what main menu is, so sue me.
	{
		if (style->def->entry_alignment == episode_style->def->C_CENTER)
			x = CenterMenuText(style, fontType, language["MainSaveGame"]);
		else
			x = 72;
		
		HL_WriteText(style, fontType, x + style->def->text[fontType].x_offset, 
			8 + style->def->text[fontType].y_offset, language["MainSaveGame"]);
	}
	else
	{
		const colourmap_c *colmap = style->def->text[fontType].colmap;
		if (menu_saveg->offset_x != 0.0f) //Only auto-center if no Xoffset 
			x = 72; //SaveDef.x; //cannot get away from the damn hardcoded value
		else
			x = CenterMenuImage(menu_saveg);
		
		HUD_DrawImage(x + style->def->text[fontType].x_offset, 8 + style->def->text[fontType].y_offset, menu_saveg, colmap);
	}

	// Use center box graphic for LineHeight unless the load game font text is actually taller
	// (this should only happen if the boxes aren't being drawn in theory)
	float LineHeight = IM_HEIGHT(C);
	if (style->fonts[styledef_c::T_TEXT]->NominalHeight() > LineHeight) 
		LineHeight = style->fonts[0]->NominalHeight();
	float WidestLine = IM_WIDTH(C) * 24 + IM_WIDTH(L) + IM_WIDTH(R);

	for (i = 0; i < SAVE_SLOTS; i++)
	{
		if (custom_MenuMain==false)
		{
			x = LoadDef.x;
			HUD_StretchImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
				y + style->def->text[styledef_c::T_TEXT].y_offset,IM_WIDTH(L),IM_HEIGHT(L),L, 0.0, 0.0);
			x += IM_WIDTH(L);
			for (int j = 0; j < 24; j++, x += IM_WIDTH(C))
				HUD_StretchImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
					y + style->def->text[styledef_c::T_TEXT].y_offset,IM_WIDTH(C),IM_HEIGHT(C),C, 0.0, 0.0);

			HUD_StretchImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
				y + style->def->text[styledef_c::T_TEXT].y_offset,IM_WIDTH(R),IM_HEIGHT(R),R, 0.0, 0.0);
		}
		else
		{
			x = LoadDef.x; 
			HUD_DrawImage(x + style->def->text[styledef_c::T_TEXT].x_offset, 
				y + style->def->text[styledef_c::T_TEXT].y_offset, L);
			x += IM_WIDTH(L);
			for (int j = 0; j < 24; j++, x += IM_WIDTH(C))
				HUD_DrawImage(x + style->def->text[styledef_c::T_TEXT].x_offset, y + style->def->text[styledef_c::T_TEXT].y_offset, C);

			HUD_DrawImage(x + style->def->text[styledef_c::T_TEXT].x_offset, y + style->def->text[styledef_c::T_TEXT].y_offset, R);
		}

		if (! style->fonts[styledef_c::T_ALT])
			fontType=styledef_c::T_TEXT;
		else
			fontType=styledef_c::T_ALT;

		if (saveStringEnter && i == save_slot)
			ex_slots[i].width = style->fonts[fontType]->StringWidth("_");
		else
			ex_slots[i].width = 0;
		ex_slots[i].y = y;
		ex_slots[i].width = ex_slots[i].width + style->fonts[styledef_c::T_TEXT]->StringWidth(ex_slots[i].desc);
		if (ex_slots[i].width > WidestLine) 
			WidestLine = ex_slots[i].width;
		y += LineHeight + style->def->entry_spacing;
	}

	for (i = 0; i < SAVE_SLOTS; i++)
	{
		int len = 0;
		int font = i == itemOn ? (style->def->text[styledef_c::T_SELECTED].font ? styledef_c::T_SELECTED : styledef_c::T_TEXT) : 
			styledef_c::T_TEXT;
		bool entering_save = false;
		if (saveStringEnter && i == save_slot)
		{
			if (! style->fonts[styledef_c::T_SELECTED])
				fontType=styledef_c::T_TEXT;
			else
				fontType=styledef_c::T_SELECTED;
			len = save_style->fonts[fontType]->StringWidth(ex_slots[save_slot].desc);
			font = fontType;
			entering_save = true;
		}
		if (LineHeight == IM_HEIGHT(C))
		{
			if (style->def->entry_alignment == style->def->C_RIGHT)
			{
				HL_WriteText(save_style, font, LoadDef.x - 8 + WidestLine - (ex_slots[i].width) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
				if (entering_save)
					HL_WriteText(save_style, font, LoadDef.x - 8 + WidestLine - (ex_slots[i].width) + len + style->def->text[styledef_c::T_TEXT].x_offset, 
						ex_slots[i].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, "_");
			}
			else if (style->def->entry_alignment == style->def->C_CENTER)
			{
				HL_WriteText(save_style, font, LoadDef.x + (WidestLine / 2) - (ex_slots[i].width / 2) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
				if (entering_save)
					HL_WriteText(save_style, font, LoadDef.x + (WidestLine / 2) - (ex_slots[i].width / 2) + len + style->def->text[styledef_c::T_TEXT].x_offset, 
						ex_slots[i].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, "_");
			}
			else
			{
				HL_WriteText(save_style, font, LoadDef.x + 8 + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
				if (entering_save)
					HL_WriteText(save_style, font, LoadDef.x + 8 + len + style->def->text[styledef_c::T_TEXT].x_offset, 
						ex_slots[i].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, "_");
			}
		}
		else
		{
			if (style->def->entry_alignment == style->def->C_RIGHT)
			{
				HL_WriteText(save_style, font, LoadDef.x + WidestLine - ex_slots[i].width + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
				if (entering_save)
					HL_WriteText(save_style, font, LoadDef.x + WidestLine - ex_slots[i].width + len + style->def->text[styledef_c::T_TEXT].x_offset, 
						ex_slots[i].y + style->def->text[styledef_c::T_TEXT].y_offset, "_");
			}
			else if (style->def->entry_alignment == style->def->C_CENTER)
			{
				HL_WriteText(save_style, font, LoadDef.x + (WidestLine / 2) - (ex_slots[i].width / 2) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
				if (entering_save)
					HL_WriteText(save_style, font, LoadDef.x + (WidestLine / 2) - (ex_slots[i].width / 2) + len + style->def->text[styledef_c::T_TEXT].x_offset, 
						ex_slots[i].y + style->def->text[styledef_c::T_TEXT].y_offset, "_");
			}
			else
			{
				HL_WriteText(save_style, font, LoadDef.x + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[i].y + style->def->text[styledef_c::T_TEXT].y_offset, ex_slots[i].desc);
				if (entering_save)
					HL_WriteText(save_style, font, LoadDef.x + len + style->def->text[styledef_c::T_TEXT].x_offset, 
						ex_slots[i].y + style->def->text[styledef_c::T_TEXT].y_offset, "_");
			}
		}
	}
	image_c *cursor;
	if (style->def->cursor.cursor_string != "")
		cursor = NULL;
	else if (style->def->cursor.alt_cursor != "")
		cursor = (image_c *)W_ImageLookup(style->def->cursor.alt_cursor.c_str());
	else
		cursor = menu_skull[0];
	if (cursor)
	{
		short old_offset_x = cursor->offset_x;
		short old_offset_y = cursor->offset_y;
		cursor->offset_x = LineHeight == IM_HEIGHT(C) ? C->offset_x : 0;
		cursor->offset_y = LineHeight == IM_HEIGHT(C) ? C->offset_y : 0;
		if (style->def->cursor.force_offsets)
		{
			cursor->offset_x += old_offset_x;
			cursor->offset_y += old_offset_y;
		}
		cursor->offset_x -= style->def->text[styledef_c::T_TEXT].x_offset;
		cursor->offset_y += style->def->text[styledef_c::T_TEXT].y_offset;
		float TempHeight = MIN(LineHeight, IM_HEIGHT(cursor));
		float TempScale = 0;
		float TempWidth = 0;
		float TempSpacer = 0;
		TempScale = TempHeight / IM_HEIGHT(cursor);
		TempWidth = IM_WIDTH(cursor) * TempScale;
		TempSpacer = TempWidth * 0.2; // 20% of cursor graphic is our space
		float old_alpha = HUD_GetAlpha();
		HUD_SetAlpha(style->def->cursor.translucency);
		if (style->def->cursor.position == style->def->C_BOTH)
		{
			HUD_StretchImage(LoadDef.x + WidestLine + TempSpacer,ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
			HUD_StretchImage(LoadDef.x - TempWidth - TempSpacer,ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
		}
		else if (style->def->cursor.position == style->def->C_CENTER)
		{
			if (style->def->cursor.border)
				HUD_StretchImage(LoadDef.x,ex_slots[itemOn].y,WidestLine,LineHeight,cursor, 0.0, 0.0);
			else
				HUD_StretchImage(LoadDef.x + (WidestLine / 2) - (TempWidth / 2),ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
		}
		else if (style->def->cursor.position == style->def->C_RIGHT)
			HUD_StretchImage(LoadDef.x + WidestLine + TempSpacer,ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
		else
			HUD_StretchImage(LoadDef.x - TempWidth - TempSpacer,ex_slots[itemOn].y,TempWidth,TempHeight,cursor, 0.0, 0.0);
		cursor->offset_x = old_offset_x;
		cursor->offset_y = old_offset_y;
		HUD_SetAlpha(old_alpha);
	}
	else
	{
		float old_alpha = HUD_GetAlpha();
		HUD_SetAlpha(style->def->cursor.translucency);
		float TempWidth = style->fonts[0]->StringWidth(style->def->cursor.cursor_string.c_str()) * style->def->text[styledef_c::T_TEXT].scale;
		float TempSpacer = style->fonts[0]->CharWidth(style->def->cursor.cursor_string[0]) * style->def->text[styledef_c::T_TEXT].scale * 0.5;
		if (LineHeight == IM_HEIGHT(C))
		{
			TempSpacer *= 2;
			if (style->def->cursor.position == style->def->C_BOTH)
			{
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x - TempWidth - TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + WidestLine + TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			}
			else if (style->def->cursor.position == style->def->C_CENTER)
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + (WidestLine/2) - (TempWidth / 2) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			else if (style->def->cursor.position == style->def->C_RIGHT)
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + WidestLine + TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			else
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x - TempWidth - TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y - C->offset_y + (LineHeight / 4) + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
		}
		else
		{
			if (style->def->cursor.position == style->def->C_BOTH)
			{
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x - TempWidth - TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + WidestLine + TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			}
			else if (style->def->cursor.position == style->def->C_CENTER)
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + (WidestLine/2) - (TempWidth / 2) + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			else if (style->def->cursor.position == style->def->C_RIGHT)
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x + WidestLine + TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
			else
				HL_WriteText(style, styledef_c::T_TEXT, LoadDef.x - TempWidth - TempSpacer + style->def->text[styledef_c::T_TEXT].x_offset, 
					ex_slots[itemOn].y + style->def->text[styledef_c::T_TEXT].y_offset, style->def->cursor.cursor_string.c_str());
		}
		HUD_SetAlpha(old_alpha);
	}

	M_DrawSaveLoadCommon(i, i+1, save_style, LineHeight);
}

//
// M_Responder calls this when user is finished
//
// 98-7-10 KM Savegame slots increased
//
static void M_DoSave(int page, int slot)
{
	G_DeferredSaveGame(page * SAVE_SLOTS + slot, ex_slots[slot].desc);
	M_ClearMenus();

	// PICK QUICKSAVE SLOT YET?
	if (quickSaveSlot == -2)
	{
		quickSavePage = page;
		quickSaveSlot = slot;
	}

	LoadDef.lastOn = SaveDef.lastOn;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
	if (choice < 0)
	{
		M_LoadSavePage(choice);
		return;
	}

	// we are going to be intercepting all chars
	saveStringEnter = 1;

	save_slot = choice;
	strcpy(saveOldString, ex_slots[choice].desc);

	if (ex_slots[choice].empty)
		ex_slots[choice].desc[0] = 0;

	saveCharIndex = strlen(ex_slots[choice].desc);
}

//
// Selected from DOOM menu
//
void M_SaveGame(int choice)
{
	if (gamestate != GS_LEVEL)
	{
		M_StartMessage(language["SaveWhenNotPlaying"], NULL, false);
		return;
	}

	// -AJA- big cop-out here (add RTS menu stuff to savegame ?)
	if (rts_menuactive)
	{
		M_StartMessage("You can't save during an RTS menu.\n\npress a key.", NULL, false);
		return;
	}

	M_ReadSaveStrings();
	M_SetupNextMenu(&SaveDef);

	need_save_screenshot = true;
	save_screenshot_valid = false;
}

//
//   M_QuickSave
//

static void QuickSaveResponse(int ch)
{
	if (ch == 'y' || ch == KEYD_MENU_SELECT || ch == KEYD_MOUSE1)
	{
		M_DoSave(quickSavePage, quickSaveSlot);
		S_StartFX(sfx_swtchx);
	}
}

void M_QuickSave(void)
{
	if (gamestate != GS_LEVEL)
	{
		S_StartFX(sfx_oof);
		return;
	}

	if (quickSaveSlot < 0)
	{
		M_StartControlPanel();
		M_ReadSaveStrings();
		M_SetupNextMenu(&SaveDef);

		need_save_screenshot = true;
		save_screenshot_valid = false;

		quickSaveSlot = -2;  // means to pick a slot now
		return;
	}
	
	std::string s(epi::STR_Format(language["QuickSaveOver"],
				  ex_slots[quickSaveSlot].desc));

	M_StartMessage(s.c_str(), QuickSaveResponse, true);
}

static void QuickLoadResponse(int ch)
{
	if (ch == 'y' || ch == KEYD_MENU_SELECT || ch == KEYD_MOUSE1)
	{
		int tempsavepage = save_page;

		save_page = quickSavePage;
		M_LoadSelect(quickSaveSlot);

		save_page = tempsavepage;
		S_StartFX(sfx_swtchx);
	}
}

void M_QuickLoad(void)
{
	if (netgame)
	{
		M_StartMessage(language["NoQLoadInNet"], NULL, false);
		return;
	}

	if (quickSaveSlot < 0)
	{
		M_StartMessage(language["NoQuickSaveSlot"], NULL, false);
		return;
	}

	std::string s(epi::STR_Format(language["QuickLoad"],
					ex_slots[quickSaveSlot].desc));

	M_StartMessage(s.c_str(), QuickLoadResponse, true);
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
	HUD_DrawImageTitleWS(menu_readthis[0]);
}

//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
	HUD_DrawImageTitleWS(menu_readthis[1]);
}



void M_DrawMainMenu(void)
{
	float CenterX = 0;
	if (menu_doom->offset_x != 0.0f) //Only auto-center if no Xoffset 
		CenterX = MainDef.x; //cannot get away from the damn hardcoded value
	else
		CenterX = CenterMenuImage(menu_doom);

	HUD_DrawImage(CenterX, 2, menu_doom);
}

void M_DrawNewGame(void)
{
	int fontType;
	int x = 54;
	float txtscale = 1.0f;

	style_c *style = skill_style;

	if (! style->fonts[styledef_c::T_HEADER])
		fontType = styledef_c::T_TITLE;
	else
		fontType = styledef_c::T_HEADER;

	if (style->def->text[fontType].scale)
		txtscale = style->def->text[fontType].scale;
	else
		txtscale = 1.0f;

	float old_alpha = HUD_GetAlpha();

	HUD_SetAlpha(style->def->text[fontType].translucency);

	if (custom_MenuDifficulty==false)
	{
		if (style->def->entry_alignment == style->def->C_CENTER)
			x = CenterMenuText(style, fontType, language["MainNewGame"]);
		else
			x = 94;
		HL_WriteText(style, fontType, x + style->def->text[fontType].x_offset, 
			14 + style->def->text[fontType].y_offset, language["MainNewGame"]);

		HUD_SetAlpha(old_alpha);
		fontType = styledef_c::T_TITLE;
		HUD_SetAlpha(style->def->text[fontType].translucency);

		if (style->def->entry_alignment == style->def->C_CENTER)
			x = CenterMenuText(style, fontType, language["MenuSkill"]);
		else
			x = 54;

		HL_WriteText(style, fontType, x + style->def->text[fontType].x_offset, 
			38 + style->def->text[fontType].y_offset, language["MenuSkill"]);
	}
	else
	{
		const colourmap_c *colmap = style->def->text[fontType].colmap;
		if (menu_newgame->offset_x != 0.0f) //Only auto-center if no Xoffset 
			x = MainDef.x; //cannot get away from the damn hardcoded value
		else
			x = CenterMenuImage2(style, fontType, menu_newgame);
		
		HUD_StretchImage(x, 14 + style->def->text[fontType].y_offset,
				IM_WIDTH(menu_newgame) * txtscale, IM_HEIGHT(menu_newgame) * txtscale,menu_newgame,0.0,0.0,colmap);

		//HUD_DrawImage(x + style->def->text[fontType].x_offset, 
		//	14 + style->def->text[fontType].y_offset, menu_newgame, colmap);

		HUD_SetAlpha(old_alpha);
		fontType = styledef_c::T_TITLE;
		HUD_SetAlpha(style->def->text[fontType].translucency);

		if (style->def->text[fontType].scale)
			txtscale = style->def->text[fontType].scale;
		else
			txtscale = 1.0f;

		x = 54;
		if (style->def->entry_alignment == style->def->C_CENTER)
		{
			if (menu_skill->offset_x != 0.0f) //Only auto-center if no Xoffset 
				x = 54; //cannot get away from the damn hardcoded value
			else
				x = CenterMenuImage2(style, fontType, menu_skill);
		}
		colmap = style->def->text[fontType].colmap;
		HUD_StretchImage(x, 38 + style->def->text[fontType].y_offset,
				IM_WIDTH(menu_skill) * txtscale, IM_HEIGHT(menu_skill) * txtscale,menu_skill,0.0,0.0,colmap);

		//HUD_DrawImage(x + style->def->text[fontType].x_offset, 
		//	38 + style->def->text[fontType].y_offset, menu_skill, colmap);
	}
	HUD_SetAlpha(old_alpha);
}

//
//      M_Episode
//

// -KM- 1998/12/16 Generates EpiDef menu dynamically.
static void CreateEpisodeMenu(void)
{
	if (gamedefs.GetSize() == 0)
		I_Error("No defined episodes !\n");

	EpisodeMenu = new menuitem_t[gamedefs.GetSize()];

	int e = 0;
	epi::array_iterator_c it;

	for (it = gamedefs.GetBaseIterator(); it.IsValid(); it++)
	{
		gamedef_c *g = ITERATOR_TO_TYPE(it, gamedef_c*);
		if (! g) continue;

		if (g->firstmap.empty())
			continue;

		if (W_CheckNumForName(g->firstmap.c_str()) == -1)
			continue;

		EpisodeMenu[e].status = 1;
		EpisodeMenu[e].select_func = M_Episode;
		EpisodeMenu[e].image = NULL;
		EpisodeMenu[e].alpha_key = '1' + e;

		Z_StrNCpy(EpisodeMenu[e].patch_name, g->namegraphic.c_str(), 8);
		EpisodeMenu[e].patch_name[8] = 0;
		
		if (g->description != "")
		{
			EpisodeMenu[e].name = language[g->description];
		}
		else
		{
			EpisodeMenu[e].name = g->name.c_str();
		}

		if (EpisodeMenu[e].patch_name[0])
		{
			if (! EpisodeMenu[e].image)
				EpisodeMenu[e].image = W_ImageLookup(EpisodeMenu[e].patch_name);
		}

		e++;
	}

	if (e == 0)
		I_Error("No available episodes !\n");

	EpiDef.numitems  = e;
	EpiDef.menuitems = EpisodeMenu;
}

void M_NewGame(int choice)
{
	if (netgame)
	{
		M_StartMessage(language["NewNetGame"], NULL, false);
		return;
	}

	if (!EpisodeMenu)
		CreateEpisodeMenu();

	if (EpiDef.numitems == 1)
	{
		M_Episode(0);
	}
	else
		M_SetupNextMenu(&EpiDef);
}


void M_DrawEpisode(void)
{
	int fontType;
	int x = 54;
	float txtscale = 1.0f;

	style_c *style = episode_style;

	if (! style->fonts[styledef_c::T_HEADER])
		fontType=styledef_c::T_TITLE;
	else
		fontType=styledef_c::T_HEADER;

	if (style->def->text[fontType].scale)
		txtscale = style->def->text[fontType].scale;

	float old_alpha = HUD_GetAlpha();
	HUD_SetAlpha(style->def->text[fontType].translucency);

	if (custom_MenuEpisode==false)
	{
		if (style->def->entry_alignment == style->def->C_CENTER)
			x = CenterMenuText(style,fontType, language["MenuWhichEpisode"]);

		HL_WriteText(style, fontType, x + style->def->text[fontType].x_offset, 
			38 + style->def->text[fontType].y_offset, language["MenuWhichEpisode"]);
	}
	else
	{
		if (style->def->entry_alignment == style->def->C_CENTER)
		{
			if (menu_episode->offset_x != 0.0f) //Only auto-center if no Xoffset 
				x = 54; //cannot get away from the damn hardcoded value
			else
				x = CenterMenuImage2(style, fontType, menu_episode);
		}

		const colourmap_c *colmap = style->def->text[fontType].colmap;
		HUD_StretchImage(x, 38 + style->def->text[fontType].y_offset,
				IM_WIDTH(menu_episode) * txtscale, IM_HEIGHT(menu_episode) * txtscale,menu_episode,0.0,0.0,colmap);

		//HUD_DrawImage(x + episode_style->def->text[fontType].x_offset, 
		//	38 + episode_style->def->text[fontType].y_offset, menu_episode, colmap);
	}

	HUD_SetAlpha(old_alpha);
}

static void ReallyDoStartLevel(skill_t skill, gamedef_c *g)
{
	newgame_params_c params;

	params.skill = skill;
	params.deathmatch = 0;

	params.random_seed = I_PureRandom();

	params.SinglePlayer(0);

	params.map = G_LookupMap(g->firstmap.c_str());

	if (! params.map)
	{
		// 23-6-98 KM Fixed this.
		M_SetupNextMenu(&EpiDef);
		M_StartMessage(language["EpisodeNonExist"], NULL, false);
		return;
	}

	SYS_ASSERT(G_MapExists(params.map));
	SYS_ASSERT(params.map->episode);

	G_DeferredNewGame(params);

	M_ClearMenus();
}

static void DoStartLevel(skill_t skill)
{
	// -KM- 1998/12/17 Clear the intermission.
	WI_Clear();
  
	// find episode
	gamedef_c *g = NULL;
	epi::array_iterator_c it;

	std::string chosen_episode = epi::STR_Format("%s",EpisodeMenu[chosen_epi].name);

	for (it = gamedefs.GetBaseIterator(); it.IsValid(); it++) 
	{ 
		g = ITERATOR_TO_TYPE(it, gamedef_c*);

		//Lobo 2022: lets use text instead of M_EPIxx graphic
		if (g->description != "") 
		{
			std::string gamedef_episode = epi::STR_Format("%s",language[g->description.c_str()]);
			if (DDF_CompareName(gamedef_episode.c_str(), chosen_episode.c_str()) == 0)
				break;		
		}
		else
		{	
			if (DDF_CompareName(g->name.c_str(), chosen_episode.c_str()) == 0)
				break;
		}
		
		/*
		if (!strcmp(g->namegraphic.c_str(), EpisodeMenu[chosen_epi].patch_name))
		{
			break;
		}
		*/
	}

	// Sanity checking...
	if (! g)
	{
		I_Warning("Internal Error: no episode for '%s'.\n",
			chosen_episode.c_str());
		M_ClearMenus();
		return;
	}

	const mapdef_c * map = G_LookupMap(g->firstmap.c_str());
	if (! map)
	{
		I_Warning("Cannot find map for '%s' (episode %s)\n",
			g->firstmap.c_str(),
			chosen_episode.c_str());
		M_ClearMenus();
		return;
	}

	ReallyDoStartLevel(skill, g);
}

static void VerifyNightmare(int ch)
{
	if (ch != 'y' && ch != KEYD_MENU_SELECT && ch != KEYD_MOUSE1)
		return;

	DoStartLevel(sk_nightmare);
}

void M_ChooseSkill(int choice)
{
	if (choice == sk_nightmare)
	{
		M_StartMessage(language["NightMareCheck"], VerifyNightmare, true);
		return;
	}
	
	DoStartLevel((skill_t)choice);
}

void M_Episode(int choice)
{
	chosen_epi = choice;
	M_SetupNextMenu(&SkillDef);
}

//
// Toggle messages on/off
//
void M_ChangeMessages(int choice)
{
	// warning: unused parameter `int choice'
	(void) choice;

	showMessages = 1 - showMessages;

	if (showMessages)
		CON_Printf("%s\n", language["MessagesOn"]);
	else
		CON_Printf("%s\n", language["MessagesOff"]);
}

static void EndGameResponse(int ch)
{
	if (ch != 'y' && ch != KEYD_MENU_SELECT && ch != KEYD_MOUSE1)
		return;

	G_DeferredEndGame();

	currentMenu->lastOn = itemOn;
	M_ClearMenus();
}

void M_EndGame(int choice)
{
	if (gamestate != GS_LEVEL)
	{
		S_StartFX(sfx_oof);
		return;
	}

	option_menuon  = 0;
	netgame_menuon = 0;

	if (netgame)
	{
		M_StartMessage(language["EndNetGame"], NULL, false);
		return;
	}

	M_StartMessage(language["EndGameCheck"], EndGameResponse, true);
}

void M_ReadThis(int choice)
{
	M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
	M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
	M_SetupNextMenu(&MainDef);
}

//
// -KM- 1998/12/16 Handle sfx that don't exist in this version.
// -KM- 1999/01/31 Generate quitsounds from default.ldf
//
static void QuitResponse(int ch)
{
	if (ch != 'y' && ch != KEYD_MENU_SELECT && ch != KEYD_MOUSE1)
		return;
		
	if (!netgame)
	{
		int numsounds = 0;
		char refname[16];
		char sound[16];
		int i, start;

		// Count the quit messages
		do
		{
			sprintf(refname, "QuitSnd%d", numsounds + 1);
			if (language.IsValidRef(refname))
				numsounds++;
			else
				break;
		}
		while (true);

		if (numsounds)
		{
			// cycle through all the quit sounds, until one of them exists
			// (some of the default quit sounds do not exist in DOOM 1)
			start = i = M_Random() % numsounds;
			do
			{
				sprintf(refname, "QuitSnd%d", i + 1);
				sprintf(sound, "DS%s", language[refname]);
				if (W_CheckNumForName(sound) != -1)
				{
					S_StartFX(sfxdefs.GetEffect(language[refname]));
					break;
				}
				i = (i + 1) % numsounds;
			}
			while (i != start);
		}
	}

	// -ACB- 1999/09/20 New exit code order
	// Write the default config file first
	I_Printf("Saving system defaults...\n");
	M_SaveDefaults();

	I_Printf("Exiting...\n");

	E_EngineShutdown();
	I_SystemShutdown();

	I_CloseProgram(EXIT_SUCCESS);
}

//
// -ACB- 1998/07/19 Removed offensive messages selection (to some people);
//     Better Random Selection.
//
// -KM- 1998/07/21 Reinstated counting quit messages, so adding them to dstrings.c
//                   is all you have to do.  Using P_Random for the random number
//                   automatically kills the sync...
//                   (hence M_Random()... -AJA-).
//
// -KM- 1998/07/31 Removed Limit. So there.
// -KM- 1999/01/31 Load quit messages from default.ldf
//
void M_QuitEDGE(int choice)
{
#if EDGE_WEB
	I_Printf("Quit ignored on web platform\n");
	return;
#endif	

	char ref[64];

	std::string msg;

	int num_quitmessages = 0;

	// Count the quit messages
	do
	{
		num_quitmessages++;

		sprintf(ref, "QUITMSG%d", num_quitmessages);
	}
	while (language.IsValidRef(ref));

	// we stopped at one higher than the last
	num_quitmessages--;

	// -ACB- 2004/08/14 Allow fallback to just the "PressToQuit" message
	if (num_quitmessages > 0)
	{
		// Pick one at random
		sprintf(ref, "QUITMSG%d", 1 + (M_Random() % num_quitmessages));

		// Construct the quit message in full
		msg = epi::STR_Format("%s\n\n%s", language[ref], language["PressToQuit"]);
	}
	else
	{
		msg = std::string(language["PressToQuit"]);
	}

	// Trigger the message
	M_StartMessage(msg.c_str(), QuitResponse, true);
}

// Accessible from console's 'quit now' command
void M_ImmediateQuit()
{

#if EDGE_WEB
	I_Printf("Quit ignored on web platform\n");
	return;
#endif	

	I_Printf("Saving system defaults...\n");
	M_SaveDefaults();

	I_Printf("Exiting...\n");

	E_EngineShutdown();
	I_SystemShutdown();

	I_CloseProgram(EXIT_SUCCESS);
}

//----------------------------------------------------------------------------
//   MENU FUNCTIONS
//----------------------------------------------------------------------------

void M_DrawThermo(int x, int y, int thermWidth, int thermDot, int div)
{
	int i, basex = x;
	int step = (8 / div);
	int pos = 254;

	style_c *opt_style = hu_styles.Lookup(styledefs.Lookup("OPTIONS"));

	// If using an IMAGE or TRUETYPE type font for the menu, use symbols for the slider instead
	if (opt_style->fonts[styledef_c::T_ALT]->def->type == FNTYP_Image || opt_style->fonts[styledef_c::T_ALT]->def->type == FNTYP_TrueType)
	{
		// Quick solid box code if a background is desired for the slider in the future
		// HUD_SolidBox(x, y, x+(thermWidth*step), y+opt_style->fonts[styledef_c::T_ALT]->im_char_height, RGB_MAKE(100,100,100));
		for (i=0; i < thermDot; i++, x += step)
		{
			HL_WriteText(opt_style, styledef_c::T_ALT, x, y, (const char *)&pos);
		}
		for (i=1; i < thermWidth - thermDot; i++, x += step)
		{
			HL_WriteText(opt_style, styledef_c::T_ALT, x, y-2, "-", 1.5);
		}
	}
	else
	{
		// Note: the (step+1) here is for compatibility with the original
		// code.  It seems required to make the thermo bar tile properly.

		HUD_StretchImage(x, y, step+1, IM_HEIGHT(therm_l)/div, therm_l, 0.0, 0.0);

		for (i=0, x += step; i < thermWidth; i++, x += step)
		{
			HUD_StretchImage(x, y, step+1, IM_HEIGHT(therm_m)/div, therm_m, 0.0, 0.0);
		}

		HUD_StretchImage(x, y, step+1, IM_HEIGHT(therm_r)/div, therm_r, 0.0, 0.0);

		x = basex + step + thermDot * step;

		HUD_StretchImage(x, y, step+1, IM_HEIGHT(therm_o)/div, therm_o, 0.0, 0.0);
	}
}

void M_StartMessage(const char *string, void (* routine)(int response), 
					bool input)
{
	msg_lastmenu = menuactive;
	msg_mode = 1;
	msg_string = std::string(string);
	message_key_routine = routine;
	message_input_routine = NULL;
	msg_needsinput = input;
	menuactive = true;
	CON_SetVisible(vs_notvisible);
	return;
}

//
// -KM- 1998/07/21 Call M_StartMesageInput to start a message that needs a
//                 string input. (You can convert it to a number if you want to.)
//                 
// string:  The prompt.
//
// routine: Format is void routine(char *s)  Routine will be called
//          with a pointer to the input in s.  s will be NULL if the user
//          pressed ESCAPE to cancel the input.
//
void M_StartMessageInput(const char *string,
						 void (* routine)(const char *response))
{
	msg_lastmenu = menuactive;
	msg_mode = 2;
	msg_string = std::string(string);
	message_input_routine = routine;
	message_key_routine = NULL;
	msg_needsinput = true;
	menuactive = true;
	CON_SetVisible(vs_notvisible);
	return;
}

#if 0
void M_StopMessage(void)
{
	menuactive = msg_lastmenu?true:false;
	msg_string.Clear();
	msg_mode = 0;
  
	if (!menuactive)
		save_screenshot_valid = false;
}
#endif

//
// CONTROL PANEL
//

//
// -KM- 1998/09/01 Analogue binding, and hat support
//
bool M_Responder(event_t * ev)
{
	int i;

	if (ev->type != ev_keydown)
		return false;

	int ch = ev->value.key.sym;

	SDL_Keymod mod = SDL_GetModState();

	// Produce psuedo keycodes from menu navigation buttons bound in the options menu
	if (E_MatchesKey(key_menu_open, ch))
	{
		ch = KEYD_MENU_OPEN;
	}
	else if (E_MatchesKey(key_menu_up, ch))
	{
		ch = KEYD_MENU_UP;
	}
	else if (E_MatchesKey(key_menu_down, ch))
	{
		ch = KEYD_MENU_DOWN;
	}
	else if (E_MatchesKey(key_menu_left, ch))
	{
		ch = KEYD_MENU_LEFT;
	}
	else if (E_MatchesKey(key_menu_right, ch))
	{
		ch = KEYD_MENU_RIGHT;
	}
	else if (E_MatchesKey(key_menu_select, ch))
	{
		ch = KEYD_MENU_SELECT;
	}
	else if (E_MatchesKey(key_menu_cancel, ch))
	{
		ch = KEYD_MENU_CANCEL;
	}

	// -ACB- 1999/10/11 F1 is responsible for print screen at any time
	if (ch == KEYD_F1 || ch == KEYD_PRTSCR)
	{
		G_DeferredScreenShot();
		return true;
	}

	// Take care of any messages that need input
	// -KM- 1998/07/21 Message Input
	if (msg_mode == 1)
	{
		if (msg_needsinput == true &&
			!(ch == ' ' || ch == 'n' || ch == 'y' || ch == KEYD_ESCAPE || ch == KEYD_MENU_CANCEL || ch == KEYD_MENU_SELECT || ch == KEYD_MOUSE1 || ch == KEYD_MOUSE2 || ch == KEYD_MOUSE3))
			return false;

		msg_mode = 0;
		// -KM- 1998/07/31 Moved this up here to fix bugs.
		menuactive = msg_lastmenu?true:false;

		if (message_key_routine)
			(* message_key_routine)(ch);

		S_StartFX(sfx_swtchx);
		return true;
	}
	else if (msg_mode == 2)
	{		
		if (ch == KEYD_ENTER || ch == KEYD_MENU_SELECT || ch == KEYD_MOUSE1)
		{
			menuactive = msg_lastmenu?true:false;
			msg_mode = 0;

			if (message_input_routine)
				(* message_input_routine)(input_string.c_str());

			input_string.clear();
			
			M_ClearMenus();
			S_StartFX(sfx_swtchx);
			return true;
		}

		if (ch == KEYD_ESCAPE || ch == KEYD_MENU_CANCEL || ch == KEYD_MOUSE2 || ch == KEYD_MOUSE3)
		{
			menuactive = msg_lastmenu?true:false;
			msg_mode = 0;
      
			if (message_input_routine)
				(* message_input_routine)(NULL);

			input_string.clear();
			
			M_ClearMenus();
			S_StartFX(sfx_swtchx);
			return true;
		}

		if ((ch == KEYD_BACKSPACE || ch == KEYD_DELETE) && !input_string.empty())
		{
			std::string s = input_string.c_str();

			if (input_string != "")
			{
				input_string.resize(input_string.size() - 1);
			}

			return true;
		}
		
		if (mod & KMOD_SHIFT || mod & KMOD_CAPS)
			ch = toupper(ch);
		if (ch == '-')
			ch = '_';
			
		if (ch >= 32 && ch <= 126)  // FIXME: international characters ??
		{
			// Set the input_string only if fits
			if (input_string.size() < 64)
			{
				input_string += ch;
			}
		}

		return true;
	}

	// new options menu on - use that responder
	if (option_menuon)
		return M_OptResponder(ev, ch);

	if (netgame_menuon)
		return M_NetGameResponder(ev, ch);

	// Save Game string input
	if (saveStringEnter)
	{
		switch (ch)
		{
			case KEYD_BACKSPACE:
				if (saveCharIndex > 0)
				{
					saveCharIndex--;
					ex_slots[save_slot].desc[saveCharIndex] = 0;
				}
				break;

			case KEYD_ESCAPE:
			case KEYD_MENU_CANCEL:
			case KEYD_MOUSE2:
			case KEYD_MOUSE3:
				saveStringEnter = 0;
				strcpy(ex_slots[save_slot].desc, saveOldString);
				break;

			case KEYD_ENTER:
			case KEYD_MENU_SELECT:
			case KEYD_MOUSE1:
				saveStringEnter = 0;
				if (ex_slots[save_slot].desc[0])
				{
					M_DoSave(save_page, save_slot);
				}
				else
				{
					std::string default_name = epi::STR_Format("SAVE-%d", save_slot+1);
					for (; (size_t) saveCharIndex < default_name.size(); saveCharIndex++)
					{
						ex_slots[save_slot].desc[saveCharIndex] = default_name[saveCharIndex];
					}
					ex_slots[save_slot].desc[saveCharIndex] = 0;
					M_DoSave(save_page, save_slot);
				}
				break;

			default:
				if (mod & KMOD_SHIFT || mod & KMOD_CAPS)
					ch = toupper(ch);
				SYS_ASSERT(save_style);
				if (ch >= 32 && ch <= 127 &&
					saveCharIndex < SAVESTRINGSIZE - 1 &&
					save_style->fonts[1]->StringWidth(ex_slots[save_slot].desc) <
					(SAVESTRINGSIZE - 2) * 8)
				{
					ex_slots[save_slot].desc[saveCharIndex++] = ch;
					ex_slots[save_slot].desc[saveCharIndex] = 0;
				}
				break;
		}
		return true;
	}

	// F-Keys
	if (!menuactive)
	{

		if (E_MatchesKey(key_screenshot, ch))
		{
			ch = KEYD_SCREENSHOT;
		}
		if (E_MatchesKey(key_save_game, ch))
		{
			ch = KEYD_SAVEGAME;
		}
		if (E_MatchesKey(key_load_game, ch))
		{
			ch = KEYD_LOADGAME;
		}
		if (E_MatchesKey(key_sound_controls, ch))
		{
			ch = KEYD_SOUNDCONTROLS;
		}
		if (E_MatchesKey(key_options_menu, ch))
		{
			ch = KEYD_OPTIONSMENU;
		}
		if (E_MatchesKey(key_quick_save, ch))
		{
			ch = KEYD_QUICKSAVE;
		}
		if (E_MatchesKey(key_end_game, ch))
		{
			ch = KEYD_ENDGAME;
		}
		if (E_MatchesKey(key_message_toggle, ch))
		{
			ch = KEYD_MESSAGETOGGLE;
		}
		if (E_MatchesKey(key_quick_load, ch))
		{
			ch = KEYD_QUICKLOAD;
		}
		if (E_MatchesKey(key_quit_edge, ch))
		{
			ch = KEYD_QUITEDGE;
		}
		if (E_MatchesKey(key_gamma_toggle, ch))
		{
			ch = KEYD_GAMMATOGGLE;
		}

		switch (ch)
		{
			case KEYD_MINUS:  // Screen size down

				if (automapactive || chat_on)
					return false;

				screen_hud = (screen_hud - 1 + NUMHUD) % NUMHUD;

				S_StartFX(sfx_stnmov);
				return true;

			case KEYD_EQUALS:  // Screen size up

				if (automapactive || chat_on)
					return false;

				screen_hud = (screen_hud + 1) % NUMHUD;

				S_StartFX(sfx_stnmov);
				return true;

			case KEYD_SAVEGAME:  // Save

				M_StartControlPanel();
				S_StartFX(sfx_swtchn);
				M_SaveGame(0);
				return true;

			case KEYD_LOADGAME:  // Load

				M_StartControlPanel();
				S_StartFX(sfx_swtchn);
				M_LoadGame(0);
				return true;

			case KEYD_SOUNDCONTROLS:  // Sound Volume

				S_StartFX(sfx_swtchn);
				M_StartControlPanel();
				M_F4SoundOptions(0);
				return true;

			case KEYD_OPTIONSMENU:  // Detail toggle, now loads options menu
				// -KM- 1998/07/31 F5 now loads options menu, detail is obsolete.

				S_StartFX(sfx_swtchn);
				M_StartControlPanel();
				M_Options(1);
				return true;

			case KEYD_QUICKSAVE:  // Quicksave

				S_StartFX(sfx_swtchn);
				M_QuickSave();
				return true;

			case KEYD_ENDGAME:  // End game

				S_StartFX(sfx_swtchn);
				M_EndGame(0);
				return true;

			case KEYD_MESSAGETOGGLE:  // Toggle messages

				M_ChangeMessages(0);
				S_StartFX(sfx_swtchn);
				return true;

			case KEYD_QUICKLOAD:  // Quickload

				S_StartFX(sfx_swtchn);
				M_QuickLoad();
				return true;

			case KEYD_QUITEDGE:  // Quit DOOM

				S_StartFX(sfx_swtchn);
				M_QuitEDGE(0);
				return true;

			case KEYD_GAMMATOGGLE:  // gamma toggle

				v_secbright.d++;
				if (v_secbright.d > 10)
					v_secbright.d = 0;

				v_secbright = v_secbright.d;

				std::string msg = "Sector Brightness "; // TODO: Make language entry - Dasho

				switch(v_secbright.d)
				{
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:
					  msg.append("-");
					  msg.append(std::to_string((5 - v_secbright.d) * 10));
					  break;
					case 5:
					  msg.append("Default");  
					  break;
					case 6:
					case 7:
					case 8:
					case 9:
					case 10:
					  msg.append("+");
					  msg.append(std::to_string((5 - v_secbright.d) * -10));
					  break;
					default:
						msg.clear();
						break;
				}
				
				if (!msg.empty())
					CON_PlayerMessage(consoleplayer, "%s", msg.c_str());

				// -AJA- 1999/07/03: removed PLAYPAL reference.
				return true;

		}

		// Pop-up menu?
		if (ch == KEYD_ESCAPE || ch == KEYD_MENU_OPEN)
		{
			M_StartControlPanel();
			S_StartFX(sfx_swtchn);
			return true;
		}
		return false;
	}

	// Keys usable within menu
	switch (ch)
	{

		case KEYD_WHEEL_DN:
			do
			{
				if (itemOn + 1 > currentMenu->numitems - 1)
				{
					if (currentMenu->menuitems[itemOn].select_func &&
						currentMenu->menuitems[itemOn].status == 2)
					{
						S_StartFX(sfx_stnmov);
						// 98-7-10 KM Use new defines
						(* currentMenu->menuitems[itemOn].select_func)(SLIDERRIGHT);
						itemOn = 0;
						return true;
					}
					else
						itemOn = 0;
				}
				else
					itemOn++;
				S_StartFX(sfx_pstop);
			}
			while (currentMenu->menuitems[itemOn].status == -1);
			return true;

		case KEYD_WHEEL_UP:
			do
			{
				if (itemOn == 0)
				{
					if (currentMenu->menuitems[itemOn].select_func &&
						currentMenu->menuitems[itemOn].status == 2)
					{
						S_StartFX(sfx_stnmov);
						// 98-7-10 KM Use new defines
						(* currentMenu->menuitems[itemOn].select_func)(SLIDERLEFT);
						itemOn = currentMenu->numitems - 1;
						return true;
					}
					else
						itemOn = currentMenu->numitems - 1;
				}
				else
					itemOn--;
				S_StartFX(sfx_pstop);
			}
			while (currentMenu->menuitems[itemOn].status == -1);
			return true;
		
		case KEYD_DOWNARROW:
		case KEYD_DPAD_DOWN:
		case KEYD_MENU_DOWN:
			do
			{
				if (itemOn + 1 > currentMenu->numitems - 1)
					itemOn = 0;
				else
					itemOn++;
				S_StartFX(sfx_pstop);
			}
			while (currentMenu->menuitems[itemOn].status == -1);
			return true;

		case KEYD_UPARROW:
		case KEYD_DPAD_UP:
		case KEYD_MENU_UP:
			do
			{
				if (itemOn == 0)
					itemOn = currentMenu->numitems - 1;
				else
					itemOn--;
				S_StartFX(sfx_pstop);
			}
			while (currentMenu->menuitems[itemOn].status == -1);
			return true;

		case KEYD_PGUP:
		case KEYD_LEFTARROW:
		case KEYD_DPAD_LEFT:
		case KEYD_MENU_LEFT:
			if (currentMenu->menuitems[itemOn].select_func &&
				currentMenu->menuitems[itemOn].status == 2)
			{
				S_StartFX(sfx_stnmov);
				// 98-7-10 KM Use new defines
				(* currentMenu->menuitems[itemOn].select_func)(SLIDERLEFT);
			}
			return true;

		case KEYD_PGDN:
		case KEYD_RIGHTARROW:
		case KEYD_DPAD_RIGHT:
		case KEYD_MENU_RIGHT:
			if (currentMenu->menuitems[itemOn].select_func &&
				currentMenu->menuitems[itemOn].status == 2)
			{
				S_StartFX(sfx_stnmov);
				// 98-7-10 KM Use new defines
				(* currentMenu->menuitems[itemOn].select_func)(SLIDERRIGHT);
			}
			return true;

		case KEYD_ENTER:
		case KEYD_MOUSE1:
		case KEYD_MENU_SELECT:
			if (currentMenu->menuitems[itemOn].select_func &&
				currentMenu->menuitems[itemOn].status)
			{
				currentMenu->lastOn = itemOn;
				(* currentMenu->menuitems[itemOn].select_func)(itemOn);
				S_StartFX(sfx_pistol);
			}
			return true;

		case KEYD_ESCAPE:
		case KEYD_MOUSE2:
		case KEYD_MOUSE3:
		case KEYD_MENU_OPEN:
			currentMenu->lastOn = itemOn;
			M_ClearMenus();
			S_StartFX(sfx_swtchx);
			return true;

		case KEYD_BACKSPACE:
		case KEYD_MENU_CANCEL:
			currentMenu->lastOn = itemOn;
			if (currentMenu->prevMenu)
			{
				currentMenu = currentMenu->prevMenu;
				itemOn = currentMenu->lastOn;
				S_StartFX(sfx_swtchn);
			}
			return true;

		default:
			for (i = itemOn + 1; i < currentMenu->numitems; i++)
				if (currentMenu->menuitems[i].alpha_key == ch)
				{
					itemOn = i;
					S_StartFX(sfx_pstop);
					return true;
				}
			for (i = 0; i <= itemOn; i++)
				if (currentMenu->menuitems[i].alpha_key == ch)
				{
					itemOn = i;
					S_StartFX(sfx_pstop);
					return true;
				}
			break;

	}

	return false;
}


void M_StartControlPanel(void)
{
	// intro might call this repeatedly
	if (menuactive)
		return;

	menuactive = true;
	CON_SetVisible(vs_notvisible);

	currentMenu = &MainDef;  // JDC
	itemOn = currentMenu->lastOn;  // JDC

	M_OptCheckNetgame();
}


static int FindChar(std::string& str, char ch, int pos)
{
	SYS_ASSERT(pos <= (int)str.size());

	const char *scan = strchr(str.c_str() + pos, ch);

	if (! scan)
		return -1;

	return (int)(scan - str.c_str());
}


static std::string GetMiddle(std::string& str, int pos, int len)
{
	SYS_ASSERT(pos >= 0 && len >= 0);
	SYS_ASSERT(pos + len <= (int)str.size());

	if (len == 0)
		return std::string();

	return std::string(str.c_str() + pos, len);
}


static void DrawMessage(void)
{

	if (message_key_routine == QuitResponse && !exit_style->bg_image) // Respect dialog styles with custom backgrounds
	{
		I_StartFrame(); // To clear and ensure solid black background regardless of style
		
		if (exit_style->def->text[styledef_c::T_TEXT].colmap)
		{
			HUD_SetTextColor(V_GetFontColor(exit_style->def->text[styledef_c::T_TEXT].colmap));
		}
		
		if(exit_style->fonts[styledef_c::T_TEXT])
		{
			HUD_SetFont(exit_style->fonts[styledef_c::T_TEXT]);
		}
		HUD_SetScale(exit_style->def->text[styledef_c::T_TEXT].scale);

		HUD_DrawQuitScreen();
		return;
	}

	//short x; // Seems unused for now - Dasho
	short y;

	SYS_ASSERT(exit_style);

	exit_style->DrawBackground();

	// FIXME: HU code should support center justification: this
	// would remove the code duplication below...

	std::string msg(msg_string);

	std::string input(input_string);

	if (msg_mode == 2)
		input += "_";
	
	// Calc required height

	std::string s = msg + input;

	y = 100 - (exit_style->fonts[styledef_c::T_TEXT]->StringLines(s.c_str()) *
		exit_style->fonts[styledef_c::T_TEXT]->NominalHeight()/ 2);

	if (!msg.empty())
	{
		int oldpos = 0;
		int pos;

		do
		{
			pos = FindChar(msg, '\n', oldpos);

			if (pos < 0)
				s = std::string(msg, oldpos);
			else
				s = GetMiddle(msg, oldpos, pos-oldpos);
		
			if (s.size() > 0)
			{
				HUD_SetAlignment(0, -1);//center it
				HL_WriteText(exit_style, styledef_c::T_TEXT, 160, y, s.c_str());
				HUD_SetAlignment(-1, -1);//set it back to usual
			}
			
			y += exit_style->fonts[styledef_c::T_TEXT]->NominalHeight();

			oldpos = pos + 1;
		}
		while (pos >= 0 && oldpos < (int)msg.size());
	}

	if (! input.empty())
	{
		int oldpos = 0;
		int pos;

		do
		{
			pos = FindChar(input, '\n', oldpos);

			if (pos < 0)
				s = std::string(input, oldpos);
			else
				s = GetMiddle(input, oldpos, pos-oldpos);
		
			if (s.size() > 0)
			{
				HUD_SetAlignment(0, -1);//center it
				HL_WriteText(exit_style, styledef_c::T_TEXT, 160, y, s.c_str());
				HUD_SetAlignment(-1, -1);//set it back to usual
			}
			
			y += exit_style->fonts[0]->NominalHeight();

			oldpos = pos + 1;
		}
		while (pos >= 0 && oldpos < (int)input.size());
	}
}


float ShortestLine;
float TallestLine;
float WidestLine;
//
// Draw our menu cursor
//
void M_DrawCursor(style_c *style, bool graphical_item)
{
	float txtscale = 1.0;
	bool graphical_cursor = false;
	float TempScale = 0;
	float TempWidth = 0;
	float TempSpacer = 0;
	float y_shift = 0;
	int txtWidth = 0;
	short old_offset_x = 0;
	short old_offset_y = 0;
	short TempX = 0;
	short TempY = 0;
	
	float old_alpha = HUD_GetAlpha();

	if (style->def->text[styledef_c::T_TEXT].scale)
		txtscale=style->def->text[styledef_c::T_TEXT].scale;

	//const colourmap_c *colmap = style->def->text[styledef_c::T_TEXT].colmap; // Should we allow a colmap for the cursor?
	const colourmap_c *colmap = NULL;
					
	//-------------------------------------------------------------
	// 1. First up, do we want a graphical cursor or a text one?
	//-------------------------------------------------------------
	image_c *cursor;
	if (style->def->cursor.cursor_string != "")
		cursor = NULL;
	else if (style->def->cursor.alt_cursor != "")
		cursor = (image_c *)W_ImageLookup(style->def->cursor.alt_cursor.c_str());
	else
		cursor = menu_skull[0];

	if (cursor) //we're using a graphic for the cursor
		graphical_cursor = true;

	HUD_SetAlpha(style->def->cursor.translucency);
	
	//-------------------------------------------------------------
	// 2. Start drawing our cursor. We have to check if the 
	// current menu item is graphical or text to do the calcs.
	//-------------------------------------------------------------
	// graphical_item==false //We're going text-based menu items
	// graphical_item==true //We're going graphic-based menu items
	if (graphical_cursor == false) //We're going text-based cursor
	{
		TempWidth = style->fonts[styledef_c::T_TEXT]->StringWidth(style->def->cursor.cursor_string.c_str()) * txtscale;
		TempSpacer = style->fonts[styledef_c::T_TEXT]->CharWidth(style->def->cursor.cursor_string[0]) * txtscale * 0.2;
	}
	else //We're going graphical cursor
	{		
		old_offset_x = cursor->offset_x;
		old_offset_y = cursor->offset_y;
		cursor->offset_x = 0;
		cursor->offset_y = 0;
		
		if (style->def->cursor.force_offsets)
		{
			cursor->offset_x += old_offset_x;
			cursor->offset_y += old_offset_y;
		}
		
		if (graphical_item == false)
		{
			if (style->fonts[styledef_c::T_TEXT]->def->type == FNTYP_TrueType)
			{
				ShortestLine = style->fonts[styledef_c::T_TEXT]->ttf_ref_height * txtscale;
				y_shift = style->fonts[styledef_c::T_TEXT]->ttf_ref_yshift * txtscale;
			}
		}	
		TempScale = ShortestLine / IM_HEIGHT(cursor);
		TempWidth = IM_WIDTH(cursor) * TempScale;
		if (!style->def->cursor.scaling)
		{
			currentMenu->menuitems[itemOn].y -= (IM_HEIGHT(cursor) - ShortestLine) / 2;
			ShortestLine = IM_HEIGHT(cursor);
			TempWidth = IM_WIDTH(cursor);
		}
		
	}
	
	TempSpacer = TempWidth * 0.2; // 20% of cursor graphic is our space
	if (style->def->cursor.position == style->def->C_BOTH)
	{
		if (style->def->entry_alignment == style->def->C_RIGHT)
		{
			//Left cursor
			if(graphical_item == false)
				txtWidth = style->fonts[styledef_c::T_TEXT]->StringWidth(currentMenu->menuitems[itemOn].name) * txtscale;
			else
				txtWidth = IM_WIDTH(currentMenu->menuitems[itemOn].image) * txtscale;
			
			TempX = currentMenu->menuitems[itemOn].x + WidestLine - TempSpacer;
			TempX -= txtWidth; 
			TempX -= TempWidth;
			
			TempY = currentMenu->menuitems[itemOn].y + y_shift;
			if (graphical_item == true)
			{
				TempX -= (currentMenu->menuitems[itemOn].image->offset_x * txtscale) * 2;
				TempY -= (currentMenu->menuitems[itemOn].image->offset_y * txtscale);
			}
			if (graphical_cursor == true)	
			{
				TempX -= (cursor->offset_x * txtscale);
				TempY -= (cursor->offset_y * txtscale);
			}	

			if (graphical_cursor == true)
			{
				HUD_StretchImage(TempX,TempY,TempWidth,ShortestLine,cursor, 0.0, 0.0,colmap);
			}
			else
				HL_WriteText(style,styledef_c::T_TEXT, TempX, 
				TempY, style->def->cursor.cursor_string.c_str());
			
			//Right cursor
			TempX = currentMenu->menuitems[itemOn].x + WidestLine + TempSpacer;
			
			TempY = currentMenu->menuitems[itemOn].y + y_shift;
			if (graphical_item == true)
			{
				TempX -= (currentMenu->menuitems[itemOn].image->offset_x * txtscale) * 2;
				TempY -= (currentMenu->menuitems[itemOn].image->offset_y * txtscale);
			}
			if (graphical_cursor == true)	
			{
				TempX -= (cursor->offset_x * txtscale);
				TempY -= (cursor->offset_y * txtscale);
			}	

			if (graphical_cursor == true)
			{
				HUD_StretchImage(TempX,TempY,TempWidth,ShortestLine,cursor, 0.0, 0.0,colmap);
			}
			else
				HL_WriteText(style,styledef_c::T_TEXT, TempX, 
				TempY, style->def->cursor.cursor_string.c_str());
		}
		else
		{
			//Left cursor
			TempX = currentMenu->menuitems[itemOn].x - TempWidth - TempSpacer;
			TempY = currentMenu->menuitems[itemOn].y + y_shift;

			if (graphical_item == true)
			{
				TempX -= (currentMenu->menuitems[itemOn].image->offset_x * txtscale) * 2;
				TempY -= (currentMenu->menuitems[itemOn].image->offset_y * txtscale);
			}
			if (graphical_cursor == true)	
			{
				TempX -= (cursor->offset_x * txtscale);
				TempY -= (cursor->offset_y * txtscale);
			}	
			
			if (graphical_cursor == true)
			{
				HUD_StretchImage(TempX,TempY,TempWidth,ShortestLine,cursor, 0.0, 0.0,colmap);
			}
			else
				HL_WriteText(style,styledef_c::T_TEXT, TempX, 
				TempY, style->def->cursor.cursor_string.c_str());
			
			//Right cursor
			if (graphical_item == false)
				txtWidth = style->fonts[styledef_c::T_TEXT]->StringWidth(currentMenu->menuitems[itemOn].name) * txtscale;
			else
				txtWidth = IM_WIDTH(currentMenu->menuitems[itemOn].image) * txtscale;
			
			TempX = currentMenu->menuitems[itemOn].x + txtWidth + TempSpacer;
			TempY = currentMenu->menuitems[itemOn].y + y_shift;
			if (graphical_item == true)
			{
				TempX -= (currentMenu->menuitems[itemOn].image->offset_x * txtscale) * 2;
				TempY -= (currentMenu->menuitems[itemOn].image->offset_y * txtscale);
			}
			if (graphical_cursor == true)	
			{
				TempX -= (cursor->offset_x * txtscale);
				TempY -= (cursor->offset_y * txtscale);
			}	
			if (graphical_cursor == true)
			{
				HUD_StretchImage(TempX,TempY,TempWidth,ShortestLine,cursor, 0.0, 0.0,colmap);
			}
			else
				HL_WriteText(style,styledef_c::T_TEXT, TempX, 
				TempY, style->def->cursor.cursor_string.c_str());
		}
	}
	else if (style->def->cursor.position == style->def->C_CENTER)
	{
		TempX = 0;

		if (graphical_cursor == true)
		{	
			TempX = CenterMenuImage2(style, styledef_c::T_TEXT,cursor);// + TempSpacer;
			TempY = currentMenu->menuitems[itemOn].y + y_shift;
			if (graphical_item == true)
			{
				TempY -= (currentMenu->menuitems[itemOn].image->offset_y * txtscale);
			}
			TempX -= (cursor->offset_x * txtscale);
			TempY -= (cursor->offset_y * txtscale);

			if (style->def->cursor.border)
				HUD_StretchImage(currentMenu->menuitems[itemOn].x,TempY,WidestLine,TallestLine,cursor, 0.0, 0.0,colmap);
			else
				HUD_StretchImage(TempX,TempY,TempWidth,ShortestLine,cursor, 0.0, 0.0,colmap);
		}
		else
		{
			TempX = CenterMenuText(style, styledef_c::T_TEXT,style->def->cursor.cursor_string.c_str());// + TempSpacer;
			TempY = currentMenu->menuitems[itemOn].y + y_shift;
			if (graphical_item == true)
			{
				TempY -= (currentMenu->menuitems[itemOn].image->offset_y * txtscale);
			}
			HL_WriteText(style,styledef_c::T_TEXT, TempX, 
			TempY, style->def->cursor.cursor_string.c_str());
		}
	}
	else if (style->def->cursor.position == style->def->C_RIGHT)
	{
		TempX = 0;
		
		if (style->def->entry_alignment == style->def->C_CENTER)
		{
			if (graphical_item == false)
				txtWidth = style->fonts[styledef_c::T_TEXT]->StringWidth(currentMenu->menuitems[itemOn].name) * txtscale;
			else
				txtWidth = IM_WIDTH(currentMenu->menuitems[itemOn].image) * txtscale;
			
			TempX = currentMenu->menuitems[itemOn].x + txtWidth + TempSpacer;
		}
		else
			TempX = currentMenu->menuitems[itemOn].x + WidestLine + TempSpacer;
		
		TempY = currentMenu->menuitems[itemOn].y + y_shift;
		if (graphical_item == true)
		{
			TempX -= (currentMenu->menuitems[itemOn].image->offset_x * txtscale) * 2;
			TempY -= (currentMenu->menuitems[itemOn].image->offset_y * txtscale);
		}
		if (graphical_cursor == true)	
		{
			TempX -= (cursor->offset_x * txtscale);
			TempY -= (cursor->offset_y * txtscale);
		}	

		if (graphical_cursor == true)
		{
			HUD_StretchImage(TempX,TempY,TempWidth,ShortestLine,cursor, 0.0, 0.0,colmap);
		}
		else
			HL_WriteText(style,styledef_c::T_TEXT, TempX, 
			TempY, style->def->cursor.cursor_string.c_str());
	}
	else
	{
		TempX = currentMenu->menuitems[itemOn].x - TempWidth - TempSpacer;
		TempY = currentMenu->menuitems[itemOn].y + y_shift;
		if (graphical_item == true)
		{
			TempX -= (currentMenu->menuitems[itemOn].image->offset_x * txtscale) * 2;
			TempY -= (currentMenu->menuitems[itemOn].image->offset_y * txtscale);
		}
		if (graphical_cursor == true)	
		{
			TempX -= (cursor->offset_x * txtscale);
			TempY -= (cursor->offset_y * txtscale);
		}	
		if (graphical_cursor == true)
		{
			HUD_StretchImage(TempX,TempY,TempWidth,ShortestLine,cursor, 0.0, 0.0,colmap);
			/*
			char mbuffer[200];
			sprintf(mbuffer, "CX%d MX%d", TempX, currentMenu->menuitems[itemOn].x);
			HL_WriteText(style,styledef_c::T_TEXT, 10, 185, mbuffer, 0.7f);
			*/
		}
		else
			HL_WriteText(style,styledef_c::T_TEXT, TempX, 
			TempY, style->def->cursor.cursor_string.c_str());
	}

	if (graphical_cursor == true)
	{	
		cursor->offset_x = old_offset_x;
		cursor->offset_y = old_offset_y;
	}
	HUD_SetAlpha(old_alpha);
}

//
// Draw our menu items
//
void M_DrawItems(style_c *style, bool graphical_item)
{
	short x, y;
	int i;
	int j;
	int max;

	float txtscale = 1.0;
	short TempX = 0;
	
	ShortestLine = 0.0f;
	TallestLine = 0.0f;
	WidestLine = 0.0f;

	x = currentMenu->x;
	y = currentMenu->y;
	
	max = currentMenu->numitems;
	
	float old_alpha = HUD_GetAlpha();
	
	if(style->def->text[styledef_c::T_TEXT].scale)
		txtscale=style->def->text[styledef_c::T_TEXT].scale;


	//---------------------------------------------------
	// 1. For each menu item calculate x, width, height
	//---------------------------------------------------
	if (graphical_item == false) //We're going text-based menu items
	{
		ShortestLine = txtscale * style->fonts[styledef_c::T_TEXT]->NominalHeight();
		TallestLine = txtscale * style->fonts[styledef_c::T_TEXT]->NominalHeight();
		for (i = 0; i < max; i++)
		{
			currentMenu->menuitems[i].height = ShortestLine;
			if (style->def->entry_alignment == style->def->C_CENTER)
				currentMenu->menuitems[i].x = CenterMenuText(style, styledef_c::T_TEXT, currentMenu->menuitems[i].name);
			else
				currentMenu->menuitems[i].x = x + style->def->x_offset + style->def->text[styledef_c::T_TEXT].x_offset;
			
			currentMenu->menuitems[i].y = y + style->def->y_offset + style->def->text[styledef_c::T_TEXT].y_offset;
			if (currentMenu->menuitems[i].width < 0)
				currentMenu->menuitems[i].width = style->fonts[styledef_c::T_TEXT]->StringWidth(currentMenu->menuitems[i].name) * txtscale;
			if (currentMenu->menuitems[i].width > WidestLine) 
				WidestLine = currentMenu->menuitems[i].width;
			
			y += currentMenu->menuitems[i].height + 1 + style->def->entry_spacing;
		}
	}
	else
	{
		ShortestLine = 10000.0f;
		TallestLine = 0.0f;
		bool backdrop_menu = false;
		for (i = 0; i < max; i++)
		{
			if (! currentMenu->menuitems[i].patch_name[0])
				continue;
			if (! currentMenu->menuitems[i].image)
				currentMenu->menuitems[i].image = W_ImageLookup(currentMenu->menuitems[i].patch_name);
		
			const image_c *image = currentMenu->menuitems[i].image;

			currentMenu->menuitems[i].height = IM_HEIGHT(image) * txtscale;
			currentMenu->menuitems[i].width = IM_WIDTH(image) * txtscale;

			if (!image->is_empty)
			{
				if (currentMenu->menuitems[i].height < ShortestLine) 
					ShortestLine = currentMenu->menuitems[i].height;
				if (currentMenu->menuitems[i].height > TallestLine) 
					TallestLine = currentMenu->menuitems[i].height;
				if (currentMenu->menuitems[i].width > WidestLine) 
					WidestLine = currentMenu->menuitems[i].width;

				if (style->def->entry_alignment == style->def->C_CENTER)
					currentMenu->menuitems[i].x = CenterMenuImage2(style, styledef_c::T_TEXT, image);
				else
					currentMenu->menuitems[i].x = x + (image->offset_x * txtscale) + style->def->x_offset + style->def->text[styledef_c::T_TEXT].x_offset;

				currentMenu->menuitems[i].y = y - image->offset_y + style->def->y_offset + style->def->text[styledef_c::T_TEXT].y_offset;
				y += currentMenu->menuitems[i].height + style->def->entry_spacing;
			}
			else
			{
				currentMenu->menuitems[i].x = x;
				currentMenu->menuitems[i].y = y;
				y += 15 + style->def->entry_spacing;
			}
		}
		if (ShortestLine == 10000.0f && TallestLine == 0.0f)
		{
			backdrop_menu = true;
			ShortestLine = 20.0f;
			TallestLine = 20.0f;
			WidestLine = 121.0f;
			HUD_SetAlpha(old_alpha);
			//We have empty menu items so don't draw anything...
			return; 
		}
	}

	int textstyle = styledef_c::T_TEXT;
	
	//---------------------------------------------------
	// 2. Draw each menu item
	//---------------------------------------------------
	for (j = 0; j < max; j++)
	{
		//int textstyle = i == itemOn ? (style->def->text[styledef_c::T_SELECTED].font ? styledef_c::T_SELECTED : styledef_c::T_TEXT) : 
		//		styledef_c::T_TEXT;

		textstyle = styledef_c::T_TEXT;
		if (j == itemOn)
		{
			if (style->def->text[styledef_c::T_SELECTED].font)
				textstyle = styledef_c::T_SELECTED;
		}
		
		HUD_SetAlpha(style->def->text[textstyle].translucency);

		if (style->def->entry_alignment == style->def->C_RIGHT)
			TempX = currentMenu->menuitems[j].x + WidestLine - currentMenu->menuitems[j].width;
		else
			TempX = currentMenu->menuitems[j].x;

		if (graphical_item == false) //We're going text-based menu items
		{
			HL_WriteText(style, textstyle, TempX, 
				currentMenu->menuitems[j].y, currentMenu->menuitems[j].name);
		}
		else //We're going graphical menu items
		{
			//const colourmap_c *colmap = i == itemOn ? style->def->text[styledef_c::T_SELECTED].colmap : 
			//		style->def->text[styledef_c::T_TEXT].colmap;

			textstyle = styledef_c::T_TEXT;
			if (j == itemOn)
			{
				if (style->def->text[styledef_c::T_SELECTED].colmap)
					textstyle = styledef_c::T_SELECTED;
			}

			const colourmap_c *colmap = style->def->text[textstyle].colmap;
			//colourmap_c *colmap = NULL;

			//HUD_StretchImage() will apply image.offset_x again so subtract it first
			TempX -= (currentMenu->menuitems[j].image->offset_x * txtscale);
			HUD_StretchImage(TempX, currentMenu->menuitems[j].y,
				currentMenu->menuitems[j].width,currentMenu->menuitems[j].height,currentMenu->menuitems[j].image, 0.0, 0.0, colmap);
		}
		HUD_SetAlpha(old_alpha);
		
	}
	HUD_SetAlpha(old_alpha);
	
}

//
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer(void)
{
	if (!menuactive)
		return;

	// Horiz. & Vertically center string and print it.
	if (msg_mode)
	{
		DrawMessage();
		return;
	}

	// new options menu enable, use that drawer instead
	if (option_menuon)
	{
		M_OptDrawer();
		return;
	}

	if (netgame_menuon)
	{
		M_NetGameDrawer();
		return;
	}
	
	//Lobo 2022: Check if we're going to use text-based menus
	//or the users (custom)graphics
	bool custom_menu = false;
	if ((currentMenu->draw_func == M_DrawMainMenu) && (custom_MenuMain == true)) 
		custom_menu=true;

	if ((currentMenu->draw_func == M_DrawNewGame) && (custom_MenuDifficulty == true)) 
		custom_menu=true;

	if (currentMenu->draw_func == M_DrawEpisode && custom_MenuEpisode == true) 
		custom_menu=true;

	style_c *style = currentMenu->style_var[0];
	SYS_ASSERT(style);

	style->DrawBackground();

	// call Draw routine
	if (currentMenu->draw_func)
		(* currentMenu->draw_func)();

	// custom_menu==false //We're going text-based menu items
	// custom_menu==true //We're going graphic-based menu items
	M_DrawItems(style, custom_menu);
	
	if (!(currentMenu->draw_func == M_DrawLoad || currentMenu->draw_func == M_DrawSave))
	{
		// custom_menu==false //We're going text-based menu items
		// custom_menu==true //We're going graphic-based menu items
		M_DrawCursor(style, custom_menu);
	}

}


void M_ClearMenus(void)
{
	// -AJA- 2007/12/24: save user changes ASAP (in case of crash)
	if (menuactive)
	{
		M_SaveDefaults();
	}

	menuactive = false;
	save_screenshot_valid = false;
	option_menuon = 0;
}

void M_SetupNextMenu(menu_t * menudef)
{
	currentMenu = menudef;
	itemOn = currentMenu->lastOn;
}

void M_Ticker(void)
{
	// update language if it changed
	if (m_language.CheckModified())
		if (! language.Select(m_language.c_str()))
			I_Printf("Unknown language: %s\n", m_language.c_str());

	if (option_menuon)
	{
		M_OptTicker();
		return;
	}

	if (netgame_menuon)
	{
		M_NetGameTicker();
		return;
	}
}

void M_Init(void)
{
	E_ProgressMessage(language["MiscInfo"]);

	currentMenu = &MainDef;
	menuactive = false;
	itemOn = currentMenu->lastOn;
	msg_mode = 0;
	msg_string.clear();
	msg_lastmenu = menuactive;
	quickSaveSlot = -1;

	// lookup styles
	styledef_c *def;

	def = styledefs.Lookup("MENU");
	if (! def) def = default_style;
	menu_def_style = hu_styles.Lookup(def);

	def = styledefs.Lookup("MAIN MENU");
	main_menu_style = def ? hu_styles.Lookup(def) : menu_def_style;

	def = styledefs.Lookup("CHOOSE EPISODE");
	episode_style = def ? hu_styles.Lookup(def) : menu_def_style;

	def = styledefs.Lookup("CHOOSE SKILL");
	skill_style = def ? hu_styles.Lookup(def) : menu_def_style;

	def = styledefs.Lookup("LOAD SAVE MENU");
	load_style = def ? hu_styles.Lookup(def) : menu_def_style;

	def = styledefs.Lookup("LOAD SAVE MENU");
	save_style = def ? hu_styles.Lookup(def) : menu_def_style;

	def = styledefs.Lookup("EXIT_SCREEN");
	exit_style = def ? hu_styles.Lookup(def) : menu_def_style;

	def = styledefs.Lookup("OPTIONS");
	if (! def) def = default_style;

	language.Select(m_language.c_str());

	//Lobo 2022: load our ddflang stuff
	MainMenu[newgame].name = language["MainNewGame"];
	MainMenu[options].name = language["MainOptions"];
	MainMenu[loadgame].name = language["MainLoadGame"];
	MainMenu[savegame].name = language["MainSaveGame"];
	MainMenu[readthis].name = language["MainReadThis"];
	MainMenu[quitdoom].name = language["MainQuitGame"];
	
	SkillMenu[0].name = language["MenuDifficulty1"];
	SkillMenu[1].name = language["MenuDifficulty2"];
	SkillMenu[2].name = language["MenuDifficulty3"];
	SkillMenu[3].name = language["MenuDifficulty4"];
	SkillMenu[4].name = language["MenuDifficulty5"];

	// lookup required images
	therm_l = W_ImageLookup("M_THERML");
	therm_m = W_ImageLookup("M_THERMM");
	therm_r = W_ImageLookup("M_THERMR");
	therm_o = W_ImageLookup("M_THERMO");

	menu_loadg    = W_ImageLookup("M_LOADG");
	menu_saveg    = W_ImageLookup("M_SAVEG");
	menu_svol     = W_ImageLookup("M_SVOL");
	menu_newgame  = W_ImageLookup("M_NEWG");
	menu_skill    = W_ImageLookup("M_SKILL");
	menu_episode  = W_ImageLookup("M_EPISOD");
	menu_skull[0] = (image_c *)W_ImageLookup("M_SKULL1");
	menu_skull[1] = (image_c *)W_ImageLookup("M_SKULL2");
	
	//Check for custom menu graphics in pwads:
	//If we have them then use them instead of our 
	// text-based ones.
	if (W_IsLumpInPwad("M_NEWG"))
		custom_MenuMain=true;

	if (W_IsLumpInPwad("M_LOADG"))
		custom_MenuMain=true;

	if (W_IsLumpInPwad("M_SAVEG"))
		custom_MenuMain=true;

	if (W_IsLumpInPwad("M_EPISOD"))
		custom_MenuEpisode=true;

	if (W_IsLumpInPwad("M_EPI1"))
		custom_MenuEpisode=true;
	
	if (W_IsLumpInPwad("M_EPI2"))
		custom_MenuEpisode=true;

	if (W_IsLumpInPwad("M_EPI3"))
		custom_MenuEpisode=true;
	
	if (W_IsLumpInPwad("M_EPI4"))
		custom_MenuEpisode=true;
	
	if (W_IsLumpInPwad("M_JKILL"))
		custom_MenuDifficulty=true;
	
	if (W_IsLumpInPwad("M_NMARE"))
		custom_MenuDifficulty=true;
	
	I_Debugf("custom_MenuMain =%d \n",custom_MenuMain);
	I_Debugf("custom_MenuEpisode =%d \n",custom_MenuEpisode);
	I_Debugf("custom_MenuDifficulty =%d \n",custom_MenuDifficulty);

	if (W_IsLumpInAnyWad("M_HTIC"))
		menu_doom = W_ImageLookup("M_HTIC");
	else
		menu_doom = W_ImageLookup("M_DOOM");

	// Here we could catch other version dependencies,
	//  like HELP1/2, and four episodes.
	//    if (W_CheckNumForName("M_EPI4") < 0)
	//      EpiDef.numitems -= 2;
	//    else if (W_CheckNumForName("M_EPI5") < 0)
	//      EpiDef.numitems--;

	if (W_IsLumpInAnyWad("HELP")) //doom2
    {
        menu_readthis[0] = W_ImageLookup("HELP");
        menu_readthis[1] = W_ImageLookup("CREDIT"); //Unnecessary since we won't see it anyway...
        MainMenu[readthis] = MainMenu[quitdoom];
        MainDef.numitems--;
        MainDef.y += 8; // FIXME
        SkillDef.prevMenu = &MainDef;
        ReadDef1.draw_func = M_DrawReadThis1;
        ReadDef1.x = 330;
        ReadDef1.y = 165;
        ReadMenu1[0].select_func = M_FinishReadThis;
    }
    else //doom or shareware doom
    {
        menu_readthis[0] = W_ImageLookup("HELP1");
        if (W_IsLumpInAnyWad("HELP2"))
            menu_readthis[1] = W_ImageLookup("HELP2"); //Shareware doom
        else
            menu_readthis[1] = W_ImageLookup("CREDIT"); //Full doom
    }

 	//Lobo 2022: Use new sfx definitions so we don't have to share names with
	//normal doom sfx.
 	sfx_swtchn = sfxdefs.GetEffect("MENU_IN"); //Enter Menu
 	sfx_tink   = sfxdefs.GetEffect("TINK"); //unused
 	sfx_radio  = sfxdefs.GetEffect("RADIO"); //unused
 	sfx_oof    = sfxdefs.GetEffect("MENU_INV"); //invalid choice
 	sfx_pstop  = sfxdefs.GetEffect("MENU_MOV"); //moving cursor in a menu
 	sfx_stnmov = sfxdefs.GetEffect("MENU_SLD"); //slider move
 	sfx_pistol = sfxdefs.GetEffect("MENU_SEL"); //select in menu
 	sfx_swtchx = sfxdefs.GetEffect("MENU_OUT"); //cancel/exit menu

	M_OptMenuInit();
	M_NetGameInit();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
