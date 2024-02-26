//----------------------------------------------------------------------------
//  EDGE Main Menu Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------
//
// See OptionMenuion.C for text built menus.
//
// -KM- 1998/07/21 Add support for message input.
//

#include "m_menu.h"

#include <math.h>

#include "am_map.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dstrings.h"
#include "e_main.h"
#include "epi_sdl.h"
#include "f_interm.h"
#include "filesystem.h"
#include "font.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "i_defs_gl.h"
#include "i_system.h"
#include "image_funcs.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_netgame.h"
#include "m_option.h"
#include "m_random.h"
#include "main.h"
#include "n_network.h"
#include "p_setup.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "s_music.h"
#include "s_sound.h"
#include "str_util.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "w_wad.h"

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

extern bool EventMatchesKey(int keyvar, int key);

extern ConsoleVariable v_secbright;

extern unsigned int R_UploadTexture(image_data_c *img, int flags, int max_pix);

extern const image_c *menu_backdrop;

//
// defaulted values
//

// Show messages has default, 0 = off, 1 = on
int show_messages;

extern ConsoleVariable m_language;

int screen_hud;  // has default

static std::string message_string;
static int         message_last_menu;
static int         message_mode;

static std::string input_string;

bool menu_active;

// timed message = no input from user
static bool message_needs_input;

static void (*message_key_routine)(int response)           = nullptr;
static void (*message_input_routine)(const char *response) = nullptr;

static int chosen_episode;

// SOUNDS
SoundEffect *sound_effect_swtchn;
SoundEffect *sound_effect_tink;
SoundEffect *sound_effect_radio;
SoundEffect *sound_effect_oof;
SoundEffect *sound_effect_pstop;
SoundEffect *sound_effect_stnmov;
SoundEffect *sound_effect_pistol;
SoundEffect *sound_effect_swtchx;
//
//  IMAGES USED
//
static const image_c *therm_l;
static const image_c *therm_m;
static const image_c *therm_r;
static const image_c *therm_o;

static const image_c *menu_load_game;
static const image_c *menu_save_game;
static const image_c *menu_sound_volume;
static const image_c *menu_doom;
static const image_c *menu_new_game;
static const image_c *menu_skill;
static const image_c *menu_episode;
static image_c       *menu_skull[2];
static const image_c *menu_read_this[2];

static Style *menu_default_style;
static Style *main_menu_style;
static Style *episode_style;
static Style *skill_style;
static Style *load_style;
static Style *save_style;
static Style *exit_style;

//
//  SAVE STUFF
//
static constexpr uint8_t kSaveStringSize = 24;
static constexpr uint8_t kTotalSaveSlots = 8;
static constexpr uint8_t kTotalSavePages = 100;

// -1 = no quicksave slot picked!
int quicksave_slot;
int quicksave_page;

// 25-6-98 KM Lots of save games... :-)
int save_page = 0;
int save_slot = 0;

static int entering_save_string;

// which char we're editing
static int save_string_character_index;

// old save description before edit
static char old_save_string[kSaveStringSize];

struct SaveSlotExtendedInformation
{
    bool empty;
    bool corrupt;

    char description[kSaveStringSize];
    char time_string[32];

    char map_name[10];
    char game_name[32];

    int skill;
    int network_game;

    // Useful for drawing skull/cursor and possible other calculations
    float y;
    float width;

    image_data_c *save_image_data = nullptr;
    unsigned int  save_texture_id = 0;
    int           save_image_page = 0;
};

static SaveSlotExtendedInformation
    save_extended_information_slots[kTotalSaveSlots];

// 98-7-10 KM New defines for slider left.
// Part of MainMenuSaveGame changes.
static constexpr int8_t kSliderLeft  = -1;
static constexpr int8_t kSliderRight = -2;

//
// MENU TYPEDEFS
//
struct MenuItem
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    int status = 0;

    // image for menu entry
    char           patch_name[10] = {0};
    const image_c *image          = nullptr;

    // choice = menu item #.
    // if status = 2, choice can be kSliderLeft or kSliderRight
    void (*select_function)(int choice) = nullptr;

    // hotkey in menu
    char alpha_key = 0;

    // Printed name test
    const char *name = nullptr;

    // Useful for drawing skull/cursor and possible other calculations
    int   x      = 0;
    int   y      = 0;
    float height = -1;
    float width  = -1;
};

struct Menu
{
    // # of menu items
    int total_items;

    // previous menu
    struct Menu *previous_menu;

    // menu items
    MenuItem *menu_items;

    // style variable
    Style **style_variable;

    // draw routine
    void (*draw_function)(void);

    // x,y of menu
    int x, y;

    // last item user was on in menu
    int last_on;
};

// menu item skull is on
static int item_on;

// current menudef
static Menu *current_menu;

//
// PROTOTYPES
//
static void MenuNewGame(int choice);
static void MenuEpisode(int choice);
static void MenuChooseSkill(int choice);
static void MenuLoadGame(int choice);
static void MenuSaveGame(int choice);

// 25-6-98 KM
extern void MenuOptions(int choice);
extern void MenuF4SoundOptions(int choice);
static void MenuLoadSavePage(int choice);
static void MenuReadThis(int choice);
static void MenuReadThis2(int choice);
void        MenuEndGame(int choice, ConsoleVariable *cvar);

static void MenuChangeMessages(int choice);

static void MenuFinishReadThis(int choice);
static void MenuLoadSelect(int choice);
static void MenuSaveSelect(int choice);
static void MenuReadSaveStrings(void);
static void MenuQuickSave(void);
static void MenuQuickLoad(void);

static void MenuDrawMainMenu(void);
static void MenuDrawReadThis1(void);
static void MenuDrawReadThis2(void);
static void MenuDrawNewGame(void);
static void MenuDrawEpisode(void);
static void MenuDrawLoad(void);
static void MenuDrawSave(void);

static void MenuSetupNextMenu(Menu *menudef);
void        MenuClear(void);
void        MenuStartControlPanel(void);
// static void MenuStopMessage(void);

//
// DOOM MENU
//
enum MainMenuCategory
{
    kMainMenuNewGame = 0,
    kMainMenuOptions,
    kMainMenuLoadGame,
    kMainMenuSaveGame,
    kMainMenuReadThis,
    kMainMenuQuitDoom,
    kTotalMainMenuCategories
};

static MenuItem MainMenu[] = {
    {1, "M_NGAME", nullptr, MenuNewGame, 'n', language["MainNewGame"]},
    {1, "M_OPTION", nullptr, MenuOptions, 'o', language["MainOptions"]},
    {1, "M_LOADG", nullptr, MenuLoadGame, 'l', language["MainLoadGame"]},
    {1, "M_SAVEG", nullptr, MenuSaveGame, 's', language["MainSaveGame"]},
    // Another hickup with Special edition.
    {1, "M_RDTHIS", nullptr, MenuReadThis, 'r', language["MainReadThis"]},
    {1, "M_QUITG", nullptr, MenuQuitEdge, 'q', language["MainQuitGame"]}};

static Menu MainMenuDefinition = {
    kTotalMainMenuCategories, nullptr, MainMenu, &main_menu_style,
    MenuDrawMainMenu,         94,      64,       0};

//
// EPISODE SELECT
//
// -KM- 1998/12/16 This is generated dynamically.
//
static MenuItem *EpisodeMenu          = nullptr;
static bool     *EpisodeMenuSkipSkill = nullptr;

static MenuItem DefaultEpisodeMenu = {1,          // status
                                      "Working",  // name
                                      nullptr,    // image
                                      nullptr,    // select_function
                                      'w',        // alphakey
                                      "DEFAULT"};

static Menu EpisodeMenuDefinition = {
    0,                    // ep_end,  // # of menu items
    &MainMenuDefinition,  // previous menu
    &DefaultEpisodeMenu,  // menuitem_t ->
    &episode_style,
    MenuDrawEpisode,  // drawing routine ->
    48,
    63,  // x,y
    0    // last_on
};

static MenuItem SkillMenu[] = {
    {1, "M_JKILL", nullptr, MenuChooseSkill, 'p', language["MenuDifficulty1"]},
    {1, "M_ROUGH", nullptr, MenuChooseSkill, 'r', language["MenuDifficulty2"]},
    {1, "M_HURT", nullptr, MenuChooseSkill, 'h', language["MenuDifficulty3"]},
    {1, "M_ULTRA", nullptr, MenuChooseSkill, 'u', language["MenuDifficulty4"]},
    {1, "M_NMARE", nullptr, MenuChooseSkill, 'n', language["MenuDifficulty5"]}};

static Menu SkillMenuDefinition = {
    sk_numtypes,             // # of menu items
    &EpisodeMenuDefinition,  // previous menu
    SkillMenu,               // menuitem_t ->
    &skill_style,
    MenuDrawNewGame,  // drawing routine ->
    48,
    63,        // x,y
    sk_medium  // last_on
};

//
// OPTIONS MENU
//

//
// Read This! MENU 1 & 2
//

static MenuItem ReadMenu1[] = {{1, "", nullptr, MenuReadThis2, 0}};

static Menu ReadThisMenuDefinition1 = {
    1,
    &MainMenuDefinition,
    ReadMenu1,
    &menu_default_style,  // FIXME: maybe have READ_1 and READ_2 styles ??
    MenuDrawReadThis1,
    1000,
    1000,
    0};

static MenuItem ReadMenu2[] = {{1, "", nullptr, MenuFinishReadThis, 0}};

static Menu ReadThisMenuDefinition2 = {
    1,
    &ReadThisMenuDefinition1,
    ReadMenu2,
    &menu_default_style,  // FIXME: maybe have READ_1 and READ_2 styles ??
    MenuDrawReadThis2,
    1000,
    1000,
    0};

//
// LOAD GAME MENU
//
// Note: upto 10 slots per page
//
static MenuItem LoadingMenu[] = {{2, "", nullptr, MenuLoadSelect, '1'},
                                 {2, "", nullptr, MenuLoadSelect, '2'},
                                 {2, "", nullptr, MenuLoadSelect, '3'},
                                 {2, "", nullptr, MenuLoadSelect, '4'},
                                 {2, "", nullptr, MenuLoadSelect, '5'},
                                 {2, "", nullptr, MenuLoadSelect, '6'},
                                 {2, "", nullptr, MenuLoadSelect, '7'},
                                 {2, "", nullptr, MenuLoadSelect, '8'},
                                 {2, "", nullptr, MenuLoadSelect, '9'},
                                 {2, "", nullptr, MenuLoadSelect, '0'}};

static Menu LoadMenuDefinition = {kTotalSaveSlots,
                                  &MainMenuDefinition,
                                  LoadingMenu,
                                  &load_style,
                                  MenuDrawLoad,
                                  30,
                                  42,
                                  0};

//
// SAVE GAME MENU
//
static MenuItem SavingMenu[] = {{2, "", nullptr, MenuSaveSelect, '1'},
                                {2, "", nullptr, MenuSaveSelect, '2'},
                                {2, "", nullptr, MenuSaveSelect, '3'},
                                {2, "", nullptr, MenuSaveSelect, '4'},
                                {2, "", nullptr, MenuSaveSelect, '5'},
                                {2, "", nullptr, MenuSaveSelect, '6'},
                                {2, "", nullptr, MenuSaveSelect, '7'},
                                {2, "", nullptr, MenuSaveSelect, '8'},
                                {2, "", nullptr, MenuSaveSelect, '9'},
                                {2, "", nullptr, MenuSaveSelect, '0'}};

static Menu SaveMenuDefinition = {kTotalSaveSlots,
                                  &MainMenuDefinition,
                                  SavingMenu,
                                  &save_style,
                                  MenuDrawSave,
                                  30,
                                  42,
                                  0};

// 98-7-10 KM Chooses the page of MainMenuSaveGames to view
void MenuLoadSavePage(int choice)
{
    switch (choice)
    {
        case kSliderLeft:
            // -AJA- could use `OOF' sound...
            if (save_page == 0) return;

            save_page--;
            break;

        case kSliderRight:
            if (save_page >= kTotalSavePages - 1) return;

            save_page++;
            break;
    }

    S_StartFX(sound_effect_swtchn);
    MenuReadSaveStrings();
}

//
// Read the strings from the MainMenuSaveGame files
//
// 98-7-10 KM Savegame slots increased
//
void MenuReadSaveStrings(void)
{
    int i, version;

    saveglobals_t *globs;

    for (i = 0; i < kTotalSaveSlots; i++)
    {
        save_extended_information_slots[i].empty   = false;
        save_extended_information_slots[i].corrupt = true;

        save_extended_information_slots[i].skill        = -1;
        save_extended_information_slots[i].network_game = -1;

        save_extended_information_slots[i].description[0] = 0;
        save_extended_information_slots[i].time_string[0] = 0;
        save_extended_information_slots[i].map_name[0]    = 0;
        save_extended_information_slots[i].game_name[0]   = 0;

        int         slot = save_page * kTotalSaveSlots + i;
        std::string fn(SV_FileName(SV_SlotName(slot), "head"));

        if (!SV_OpenReadFile(fn))
        {
            save_extended_information_slots[i].empty   = true;
            save_extended_information_slots[i].corrupt = false;
            continue;
        }

        if (!SV_VerifyHeader(&version))
        {
            SV_CloseReadFile();
            continue;
        }

        globs = SV_LoadGLOB();

        // close file now -- we only need the globals
        SV_CloseReadFile();

        if (!globs) continue;

        // --- pull info from global structure ---

        if (!globs->game || !globs->level || !globs->description)
        {
            SV_FreeGLOB(globs);
            continue;
        }

        save_extended_information_slots[i].corrupt = false;

        epi::CStringCopyMax(save_extended_information_slots[i].game_name,
                            globs->game, 32 - 1);
        epi::CStringCopyMax(save_extended_information_slots[i].map_name,
                            globs->level, 10 - 1);

        epi::CStringCopyMax(save_extended_information_slots[i].description,
                            globs->description, kSaveStringSize - 1);

        if (globs->desc_date)
            epi::CStringCopyMax(save_extended_information_slots[i].time_string,
                                globs->desc_date, 32 - 1);

        save_extended_information_slots[i].skill        = globs->skill;
        save_extended_information_slots[i].network_game = globs->netgame;

        SV_FreeGLOB(globs);

        epi::ReplaceExtension(fn, ".replace");
        if (epi::FileExists(fn))
        {
            delete save_extended_information_slots[i].save_image_data;
            save_extended_information_slots[i].save_image_data = nullptr;
            if (save_extended_information_slots[i].save_texture_id)
                glDeleteTextures(
                    1, &save_extended_information_slots[i].save_texture_id);
            save_extended_information_slots[i].save_texture_id = 0;
            save_extended_information_slots[i].save_image_page = save_page;
            epi::FileDelete(fn);
        }

        // Save screenshot
        epi::ReplaceExtension(fn, ".jpg");

        if (epi::FileExists(fn) &&
            (!save_extended_information_slots[i].save_image_data ||
             save_page != save_extended_information_slots[i].save_image_page))
        {
            delete save_extended_information_slots[i].save_image_data;
            if (save_extended_information_slots[i].save_texture_id)
                glDeleteTextures(
                    1, &save_extended_information_slots[i].save_texture_id);
            epi::File *svimg_file = epi::FileOpen(
                fn, epi::kFileAccessRead | epi::kFileAccessBinary);
            if (svimg_file)
            {
                save_extended_information_slots[i].save_image_data =
                    Image_Load(svimg_file);
                if (save_extended_information_slots[i].save_image_data)
                {
                    save_extended_information_slots[i].save_texture_id =
                        R_UploadTexture(
                            save_extended_information_slots[i].save_image_data,
                            2, (1 << 30));
                    save_extended_information_slots[i].save_image_page =
                        save_page;
                    delete svimg_file;
                }
                else
                {
                    LogWarning(
                        "Error reading MainMenuSaveGame screenshot %s!\n",
                        fn.c_str());
                    save_extended_information_slots[i].save_image_data =
                        nullptr;
                    save_extended_information_slots[i].save_texture_id =
                        0;  // just in case
                    save_extended_information_slots[i].save_image_page =
                        save_page;
                    delete svimg_file;
                }
            }
        }
    }

    // fix up descriptions
    for (i = 0; i < kTotalSaveSlots; i++)
    {
        if (save_extended_information_slots[i].corrupt)
        {
            strncpy(save_extended_information_slots[i].description,
                    language["Corrupt_Slot"], kSaveStringSize - 1);
            continue;
        }
        else if (save_extended_information_slots[i].empty)
        {
            strncpy(save_extended_information_slots[i].description,
                    language["EmptySlot"], kSaveStringSize - 1);
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
int CenterMenuImage2(Style *style, int text_type, const image_c *img)
{
    float CenterX  = 160;
    float txtscale = style->definition_->text_[text_type].scale_;
    float gfxWidth = 0;

    gfxWidth = IM_WIDTH(img) * txtscale;
    CenterX -= gfxWidth / 2;
    CenterX += style->definition_->text_[text_type].x_offset_;

    return CenterX;
}

int CenterMenuText(Style *style, int text_type, const char *str)
{
    float CenterX  = 160;
    float txtscale = style->definition_->text_[text_type].scale_;
    float txtWidth = 0;

    txtWidth = style->fonts_[text_type]->StringWidth(str) * txtscale;
    CenterX -= txtWidth / 2;
    CenterX += style->definition_->text_[text_type].x_offset_;

    // Should we also add "style->definition_->x_offset" here too?
    // CenterX += style->definition_->x_offset;

    return CenterX;
}

// AuxStringReplaceAll("Our_String", std::string("_"), std::string(" "));
//
std::string LoboStringReplaceAll(std::string str, const std::string &from,
                                 const std::string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos +=
            to.length();  // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

static void MenuDrawSaveLoadCommon(int row, int row2, Style *style,
                                   float LineHeight)
{
    int   y         = 0;  // LoadMenuDefinition.y + LineHeight * row;
    int   x         = 0;
    int   text_type = StyleDefinition::kTextSectionTitle;
    float txtscale  = style->definition_->text_[text_type].scale_;

    // TITLE.FONT="EDGE3"; // next page text
    // TEXT.FONT="EDGE3"; // save name & slot
    // ALT.FONT="EDGE3";  // when we edit the save name
    // HELP.FONT="EDGE3"; // save info text

    y = style->definition_->text_[text_type].y_offset_;
    y += style->definition_->entry_spacing_;
    x = style->definition_->text_[text_type].x_offset_;
    SaveSlotExtendedInformation *info;

    std::string temp_string = epi::StringFormat("PAGE %d", save_page + 1);

    if (save_page > 0) HudWriteText(style, text_type, x - 4, y, "< PREV");

    x += style->fonts_[text_type]->StringWidth("< PREV") * txtscale;
    x += 30;

    HudWriteText(style, text_type, x, y, temp_string.c_str());

    x += style->fonts_[text_type]->StringWidth(temp_string.c_str()) * txtscale;
    x += 30;

    if (save_page < kTotalSavePages - 1)
        HudWriteText(style, text_type, x, y, "NEXT >");

    info = save_extended_information_slots + item_on;
    SYS_ASSERT(0 <= item_on && item_on < kTotalSaveSlots);

    // show some info about the MainMenuSaveGame

    text_type = StyleDefinition::kTextSectionHelp;
    txtscale  = style->definition_->text_[text_type].scale_;

    // y = LoadMenuDefinition.y + LineHeight * (row2 + 1);
    y = style->definition_->text_[text_type].y_offset_;
    y += style->definition_->entry_spacing_;
    x = style->definition_->text_[text_type].x_offset_;

    LineHeight = style->fonts_[text_type]->NominalHeight() * txtscale;

    const Colormap *colmap = style->definition_->text_[text_type].colmap_;
    RGBAColor       col    = V_GetFontColor(colmap);
    // HudThinBox(x - 5, y - 5, x + 95, y + 50, col);
    HudThinBox(x - 5, y - 5, x + 95, y + 115, col);

    if (entering_save_string || info->empty || info->corrupt) return;

    temp_string = info->time_string;
    if (temp_string[0] ==
        ' ')  // Remove beginning space that legacy behavior had sometimes
        temp_string = temp_string.substr(1);
    size_t timesplit = temp_string.find("  ");
    SYS_ASSERT(timesplit != std::string::npos &&
               temp_string.size() > timesplit + 2);
    HudWriteText(style, text_type, x, y,
                 temp_string.substr(0, timesplit).c_str());
    y += LineHeight;
    y += style->definition_->entry_spacing_;
    HudWriteText(style, text_type, x, y,
                 temp_string.substr(timesplit + 2).c_str());
    y += LineHeight;
    y += style->definition_->entry_spacing_;

    temp_string = info->game_name;
    temp_string =
        LoboStringReplaceAll(temp_string, std::string("_"), std::string(" "));
    HudWriteText(style, text_type, x, y, temp_string.c_str());

    y += LineHeight;
    y += style->definition_->entry_spacing_;

    temp_string = info->map_name;
    HudWriteText(style, text_type, x, y, temp_string.c_str());

    y += LineHeight;
    y += style->definition_->entry_spacing_;

    switch (info->skill)
    {
        case 0:
            temp_string = language["MenuDifficulty1"];
            break;
        case 1:
            temp_string = language["MenuDifficulty2"];
            break;
        case 2:
            temp_string = language["MenuDifficulty3"];
            break;
        case 3:
            temp_string = language["MenuDifficulty4"];
            break;
        default:
            temp_string = language["MenuDifficulty5"];
            break;
    }
    HudWriteText(style, text_type, x, y, temp_string.c_str());

    /*int BottomY = 0;
    BottomY =
    style->definition_->text[StyleDefinition::kTextSectionHelp].y_offset;
    BottomY += style->definition_->entry_spacing;
    BottomY += 114;*/

    if (info->save_image_data && info->save_texture_id)
    {
        y += 20;
        // BottomY -= y;
        HudStretchFromImageData(
            x - 3, y, 95,
            (style->definition_->text_[text_type].y_offset_ +
             style->definition_->entry_spacing_ + 114) -
                y,
            info->save_image_data, info->save_texture_id, OPAC_Solid);
    }
}

// the new one
void MenuDrawLoad(void)
{
    int   i;
    int   fontType;
    float LineHeight;
    int   TempX = 0;
    int   TempY = 0;

    float old_alpha = HudGetAlpha();

    Style *style = LoadMenuDefinition.style_variable[0];

    SYS_ASSERT(style);
    style->DrawBackground();

    if (!style->fonts_[StyleDefinition::kTextSectionHeader])
        fontType = StyleDefinition::kTextSectionText;
    else
        fontType = StyleDefinition::kTextSectionHeader;

    HudSetAlpha(style->definition_->text_[fontType].translucency_);

    // 1. Draw the header i.e. "Load Game"
    TempX = CenterMenuText(style, fontType, language["MainLoadGame"]);
    TempY = 5;
    TempY += style->definition_->text_[fontType].y_offset_;

    HudWriteText(style, fontType, TempX, TempY, language["MainLoadGame"]);

    HudSetAlpha(old_alpha);

    TempX = 0;
    TempY = 0;

    fontType = StyleDefinition::kTextSectionText;

    TempX += style->definition_->text_[fontType].x_offset_;
    TempY += style->definition_->text_[fontType].y_offset_;
    TempY += style->definition_->entry_spacing_;

    RGBAColor col = V_GetFontColor(style->definition_->text_[fontType].colmap_);
    HudThinBox(TempX - 5, TempY - 5, TempX + 175, TempY + 115, col);

    // 2. draw the save games
    for (i = 0; i < kTotalSaveSlots; i++)
    {
        fontType = StyleDefinition::kTextSectionText;
        if (i == item_on)
        {
            if (style->definition_->text_[StyleDefinition::kTextSectionSelected]
                    .font_)
                fontType = StyleDefinition::kTextSectionSelected;
        }

        LineHeight = style->fonts_[fontType]->NominalHeight();  // * txtscale

        if (fontType == StyleDefinition::kTextSectionSelected)
        {
            if (style->fonts_[StyleDefinition::kTextSectionSelected]
                    ->definition_->type_ == kFontTypeTrueType)
            {
                // truetype_reference_yshift_ is important for TTF fonts.
                float y_shift =
                    style->fonts_[StyleDefinition::kTextSectionSelected]
                        ->truetype_reference_yshift_
                            [current_font_size];  // * txtscale;

                HudSetAlpha(0.33f);
                HudSolidBox(TempX - 3, TempY - 2 + (y_shift / 2), TempX + 173,
                            TempY + LineHeight + 2 + y_shift, col);
                HudSetAlpha(old_alpha);
            }
            else
            {
                HudSetAlpha(0.33f);
                HudSolidBox(TempX - 3, TempY - 2, TempX + 173,
                            TempY + LineHeight + 2, col);
                HudSetAlpha(old_alpha);
            }
        }
        if (style->fonts_[fontType]->definition_->type_ == kFontTypeTrueType)
            HudWriteText(style, fontType, TempX, TempY - (LineHeight / 2),
                         save_extended_information_slots[i].description);
        else
            HudWriteText(style, fontType, TempX, TempY - 1,
                         save_extended_information_slots[i].description);
        TempY += LineHeight + (LineHeight / 2);
        TempY += style->definition_->entry_spacing_;
    }

    MenuDrawSaveLoadCommon(i, i + 1, load_style, LineHeight);
}

//
// User wants to load this game
//
// 98-7-10 KM Savegame slots increased
//
void MenuLoadSelect(int choice)
{
    if (choice < 0 || save_extended_information_slots[choice].empty)
    {
        MenuLoadSavePage(choice);
        return;
    }

    GameDeferredLoadGame(save_page * kTotalSaveSlots + choice);
    MenuClear();
}

//
// Selected from DOOM menu
//
void MenuLoadGame(int choice)
{
    if (network_game)
    {
        MenuStartMessage(language["NoLoadInNetGame"], nullptr, false);
        return;
    }

    MenuSetupNextMenu(&LoadMenuDefinition);
    MenuReadSaveStrings();
}

// the new one
void MenuDrawSave(void)
{
    int   i;
    int   fontType;
    float LineHeight;
    int   TempX = 0;
    int   TempY = 0;

    float old_alpha = HudGetAlpha();

    Style *style = SaveMenuDefinition.style_variable[0];

    SYS_ASSERT(style);
    style->DrawBackground();

    if (!style->fonts_[StyleDefinition::kTextSectionHeader])
        fontType = StyleDefinition::kTextSectionText;
    else
        fontType = StyleDefinition::kTextSectionHeader;

    float txtscale = style->definition_->text_[fontType].scale_;

    HudSetAlpha(style->definition_->text_[fontType].translucency_);

    // 1. Draw the header i.e. "Load Game"
    TempX = CenterMenuText(style, fontType, language["MainSaveGame"]);
    TempY = 5;
    TempY += style->definition_->text_[fontType].y_offset_;

    HudWriteText(style, fontType, TempX, TempY, language["MainSaveGame"]);

    HudSetAlpha(old_alpha);

    fontType = StyleDefinition::kTextSectionText;
    TempX    = 0;
    TempY    = 0;
    TempX += style->definition_->text_[fontType].x_offset_;
    TempY += style->definition_->text_[fontType].y_offset_;
    TempY += style->definition_->entry_spacing_;

    RGBAColor col = V_GetFontColor(style->definition_->text_[fontType].colmap_);
    HudThinBox(TempX - 5, TempY - 5, TempX + 175, TempY + 115, col);

    // 2. draw the save games
    for (i = 0; i < kTotalSaveSlots; i++)
    {
        fontType = StyleDefinition::kTextSectionText;
        txtscale = style->definition_->text_[fontType].scale_;
        if (i == item_on)
        {
            if (style->definition_->text_[StyleDefinition::kTextSectionSelected]
                    .font_)
            {
                fontType = StyleDefinition::kTextSectionSelected;
                txtscale = style->definition_->text_[fontType].scale_;
            }
        }

        LineHeight = style->fonts_[fontType]->NominalHeight();  // * txtscale

        if (fontType == StyleDefinition::kTextSectionSelected)
        {
            if (style->fonts_[fontType]->definition_->type_ ==
                kFontTypeTrueType)
            {
                // truetype_reference_yshift_ is important for TTF fonts.
                float y_shift =
                    style->fonts_[fontType]->truetype_reference_yshift_
                        [current_font_size];  // * txtscale;

                HudSetAlpha(0.33f);
                HudSolidBox(TempX - 3, TempY - 2 + (y_shift / 2), TempX + 173,
                            TempY + LineHeight + 2 + y_shift, col);
                HudSetAlpha(old_alpha);
            }
            else
            {
                HudSetAlpha(0.33f);
                HudSolidBox(TempX - 3, TempY - 2, TempX + 173,
                            TempY + LineHeight + 2, col);
                HudSetAlpha(old_alpha);
            }
        }

        int  len           = 0;
        bool entering_save = false;
        if (entering_save_string && i == save_slot)
        {
            entering_save = true;
            if (!style->fonts_[StyleDefinition::kTextSectionAlternate])
            {
                fontType = StyleDefinition::kTextSectionText;
                txtscale = style->definition_->text_[fontType].scale_;
            }
            else
            {
                fontType = StyleDefinition::kTextSectionAlternate;
                txtscale = style->definition_->text_[fontType].scale_;
            }

            len = style->fonts_[fontType]->StringWidth(
                      save_extended_information_slots[save_slot].description) *
                  txtscale;
        }

        if (style->fonts_[fontType]->definition_->type_ == kFontTypeTrueType)
        {
            HudWriteText(style, fontType, TempX, TempY - (LineHeight / 2),
                         save_extended_information_slots[i].description);

            if (entering_save)
            {
                HudWriteText(style, fontType, TempX + len,
                             TempY - (LineHeight / 2), "_");
            }
        }
        else
        {
            HudWriteText(style, fontType, TempX, TempY - 1,
                         save_extended_information_slots[i].description);

            if (entering_save)
            {
                HudWriteText(style, fontType, TempX + len, TempY - 1, "_");
            }
        }

        TempY += LineHeight + (LineHeight / 2);
        TempY += style->definition_->entry_spacing_;
    }

    MenuDrawSaveLoadCommon(i, i + 1, save_style, LineHeight);
}

//
// MenuResponder calls this when user is finished
//
// 98-7-10 KM Savegame slots increased
//
static void M_DoSave(int page, int slot)
{
    GameDeferredSaveGame(page * kTotalSaveSlots + slot,
                         save_extended_information_slots[slot].description);
    MenuClear();

    // PICK QUICKSAVE SLOT YET?
    if (quicksave_slot == -2)
    {
        quicksave_page = page;
        quicksave_slot = slot;
    }

    LoadMenuDefinition.last_on = SaveMenuDefinition.last_on;
}

//
// User wants to save. Start string input for MenuResponder
//
void MenuSaveSelect(int choice)
{
    if (choice < 0)
    {
        MenuLoadSavePage(choice);
        return;
    }

    // we are going to be intercepting all chars
    entering_save_string = 1;

    save_slot = choice;
    strcpy(old_save_string,
           save_extended_information_slots[choice].description);

    if (save_extended_information_slots[choice].empty)
        save_extended_information_slots[choice].description[0] = 0;

    save_string_character_index =
        strlen(save_extended_information_slots[choice].description);
}

//
// Selected from DOOM menu
//
void MenuSaveGame(int choice)
{
    if (game_state != GS_LEVEL)
    {
        MenuStartMessage(language["SaveWhenNotPlaying"], nullptr, false);
        return;
    }

    // -AJA- big cop-out here (add RTS menu stuff to MainMenuSaveGame ?)
    if (rts_menu_active)
    {
        MenuStartMessage("You can't save during an RTS menu.\n\npress a key.",
                         nullptr, false);
        return;
    }

    MenuReadSaveStrings();
    MenuSetupNextMenu(&SaveMenuDefinition);

    need_save_screenshot  = true;
    save_screenshot_valid = false;
}

//
//   MenuQuickSave
//

static void QuickSaveResponse(int ch)
{
    if (ch == 'y' || ch == KEYD_GP_A || ch == KEYD_MOUSE1)
    {
        M_DoSave(quicksave_page, quicksave_slot);
        S_StartFX(sound_effect_swtchx);
    }
}

void MenuQuickSave(void)
{
    if (game_state != GS_LEVEL)
    {
        S_StartFX(sound_effect_oof);
        return;
    }

    if (quicksave_slot < 0)
    {
        MenuStartControlPanel();
        MenuReadSaveStrings();
        MenuSetupNextMenu(&SaveMenuDefinition);

        need_save_screenshot  = true;
        save_screenshot_valid = false;

        quicksave_slot = -2;  // means to pick a slot now
        return;
    }

    std::string s(epi::StringFormat(
        language["QuickSaveOver"],
        save_extended_information_slots[quicksave_slot].description));

    MenuStartMessage(s.c_str(), QuickSaveResponse, true);
}

static void QuickLoadResponse(int ch)
{
    if (ch == 'y' || ch == KEYD_GP_A || ch == KEYD_MOUSE1)
    {
        int tempsavepage = save_page;

        save_page = quicksave_page;
        MenuLoadSelect(quicksave_slot);

        save_page = tempsavepage;
        S_StartFX(sound_effect_swtchx);
    }
}

void MenuQuickLoad(void)
{
    if (network_game)
    {
        MenuStartMessage(language["NoQLoadInNet"], nullptr, false);
        return;
    }

    if (quicksave_slot < 0)
    {
        MenuStartMessage(language["NoQuickSaveSlot"], nullptr, false);
        return;
    }

    std::string s(epi::StringFormat(
        language["QuickLoad"],
        save_extended_information_slots[quicksave_slot].description));

    MenuStartMessage(s.c_str(), QuickLoadResponse, true);
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void MenuDrawReadThis1(void) { HudDrawImageTitleWS(menu_read_this[0]); }

//
// Read This Menus - optional second page.
//
void MenuDrawReadThis2(void) { HudDrawImageTitleWS(menu_read_this[1]); }

void MenuDrawMainMenu(void)
{
    float CenterX = 0;
    if (menu_doom->offset_x != 0.0f)  // Only auto-center if no Xoffset
        CenterX = MainMenuDefinition
                      .x;  // cannot get away from the damn hardcoded value
    else
        CenterX = CenterMenuImage(menu_doom);

    HudDrawImage(CenterX, 2, menu_doom);
}

void MenuDrawNewGame(void)
{
    int fontType;
    int x = 54;

    Style *style = skill_style;

    if (!style->fonts_[StyleDefinition::kTextSectionHeader])
        fontType = StyleDefinition::kTextSectionTitle;
    else
        fontType = StyleDefinition::kTextSectionHeader;

    float txtscale = style->definition_->text_[fontType].scale_;

    float old_alpha = HudGetAlpha();

    HudSetAlpha(style->definition_->text_[fontType].translucency_);

    if (custom_MenuDifficulty == false)
    {
        if (style->definition_->entry_alignment_ ==
            StyleDefinition::kAlignmentCenter)
            x = CenterMenuText(style, fontType, language["MainNewGame"]);
        else
            x = 94;
        HudWriteText(style, fontType,
                     x + style->definition_->text_[fontType].x_offset_,
                     14 + style->definition_->text_[fontType].y_offset_,
                     language["MainNewGame"]);

        HudSetAlpha(old_alpha);
        fontType = StyleDefinition::kTextSectionTitle;
        txtscale = style->definition_->text_[fontType].scale_;
        HudSetAlpha(style->definition_->text_[fontType].translucency_);

        if (style->definition_->entry_alignment_ ==
            StyleDefinition::kAlignmentCenter)
            x = CenterMenuText(style, fontType, language["MenuSkill"]);
        else
            x = 54;

        HudWriteText(style, fontType,
                     x + style->definition_->text_[fontType].x_offset_,
                     38 + style->definition_->text_[fontType].y_offset_,
                     language["MenuSkill"]);
    }
    else
    {
        const Colormap *colmap = style->definition_->text_[fontType].colmap_;
        if (menu_new_game->offset_x != 0.0f)  // Only auto-center if no Xoffset
            x = MainMenuDefinition
                    .x;  // cannot get away from the damn hardcoded value
        else
            x = CenterMenuImage2(style, fontType, menu_new_game);

        HudStretchImage(x, 14 + style->definition_->text_[fontType].y_offset_,
                        IM_WIDTH(menu_new_game) * txtscale,
                        IM_HEIGHT(menu_new_game) * txtscale, menu_new_game, 0.0,
                        0.0, colmap);

        // HudDrawImage(x + style->definition_->text[fontType].x_offset,
        //	14 + style->definition_->text[fontType].y_offset, menu_new_game,
        // colmap);

        HudSetAlpha(old_alpha);
        fontType = StyleDefinition::kTextSectionTitle;
        txtscale = style->definition_->text_[fontType].scale_;
        HudSetAlpha(style->definition_->text_[fontType].translucency_);

        x = 54;
        if (style->definition_->entry_alignment_ ==
            StyleDefinition::kAlignmentCenter)
        {
            if (menu_skill->offset_x != 0.0f)  // Only auto-center if no Xoffset
                x = 54;  // cannot get away from the damn hardcoded value
            else
                x = CenterMenuImage2(style, fontType, menu_skill);
        }
        colmap = style->definition_->text_[fontType].colmap_;
        HudStretchImage(x, 38 + style->definition_->text_[fontType].y_offset_,
                        IM_WIDTH(menu_skill) * txtscale,
                        IM_HEIGHT(menu_skill) * txtscale, menu_skill, 0.0, 0.0,
                        colmap);

        // HudDrawImage(x + style->definition_->text[fontType].x_offset,
        //	38 + style->definition_->text[fontType].y_offset, menu_skill,
        // colmap);
    }
    HudSetAlpha(old_alpha);
}

//
//      MenuEpisode
//

// -KM- 1998/12/16 Generates EpisodeMenuDefinition menu dynamically.
static void CreateEpisodeMenu(void)
{
    if (gamedefs.empty()) FatalError("No defined episodes !\n");

    EpisodeMenu          = new MenuItem[gamedefs.size()];
    EpisodeMenuSkipSkill = new bool[gamedefs.size()];

    int e = 0;

    for (auto g : gamedefs)
    {
        if (!g) continue;

        if (g->firstmap_.empty()) continue;

        if (W_CheckNumForName(g->firstmap_.c_str()) == -1) continue;

        EpisodeMenu[e].status          = 1;
        EpisodeMenu[e].select_function = MenuEpisode;
        EpisodeMenu[e].image           = nullptr;
        EpisodeMenu[e].alpha_key       = '1' + e;
        EpisodeMenuSkipSkill[e]        = g->no_skill_menu_;

        epi::CStringCopyMax(EpisodeMenu[e].patch_name, g->namegraphic_.c_str(),
                            8);
        EpisodeMenu[e].patch_name[8] = 0;

        if (g->description_ != "")
        {
            EpisodeMenu[e].name = language[g->description_];
        }
        else { EpisodeMenu[e].name = g->name_.c_str(); }

        if (EpisodeMenu[e].patch_name[0])
        {
            if (!EpisodeMenu[e].image)
                EpisodeMenu[e].image = W_ImageLookup(EpisodeMenu[e].patch_name);
        }

        e++;
    }

    if (e == 0) FatalError("No available episodes !\n");

    EpisodeMenuDefinition.total_items = e;
    EpisodeMenuDefinition.menu_items  = EpisodeMenu;
}

void MenuNewGame(int choice)
{
    if (network_game)
    {
        MenuStartMessage(language["NewNetGame"], nullptr, false);
        return;
    }

    if (!EpisodeMenu) CreateEpisodeMenu();

    if (EpisodeMenuDefinition.total_items == 1) { MenuEpisode(0); }
    else
        MenuSetupNextMenu(&EpisodeMenuDefinition);
}

void MenuDrawEpisode(void)
{
    int fontType;
    int x = 54;

    Style *style = episode_style;

    if (!style->fonts_[StyleDefinition::kTextSectionHeader])
        fontType = StyleDefinition::kTextSectionTitle;
    else
        fontType = StyleDefinition::kTextSectionHeader;

    float txtscale = style->definition_->text_[fontType].scale_;

    float old_alpha = HudGetAlpha();
    HudSetAlpha(style->definition_->text_[fontType].translucency_);

    if (custom_MenuEpisode == false)
    {
        if (style->definition_->entry_alignment_ ==
            StyleDefinition::kAlignmentCenter)
            x = CenterMenuText(style, fontType, language["MenuWhichEpisode"]);

        HudWriteText(style, fontType,
                     x + style->definition_->text_[fontType].x_offset_,
                     38 + style->definition_->text_[fontType].y_offset_,
                     language["MenuWhichEpisode"]);
    }
    else
    {
        if (style->definition_->entry_alignment_ ==
            StyleDefinition::kAlignmentCenter)
        {
            if (menu_episode->offset_x !=
                0.0f)    // Only auto-center if no Xoffset
                x = 54;  // cannot get away from the damn hardcoded value
            else
                x = CenterMenuImage2(style, fontType, menu_episode);
        }

        const Colormap *colmap = style->definition_->text_[fontType].colmap_;
        HudStretchImage(x, 38 + style->definition_->text_[fontType].y_offset_,
                        IM_WIDTH(menu_episode) * txtscale,
                        IM_HEIGHT(menu_episode) * txtscale, menu_episode, 0.0,
                        0.0, colmap);

        // HudDrawImage(x + episode_style->definition_->text[fontType].x_offset,
        //	38 + episode_style->definition_->text[fontType].y_offset,
        // menu_episode, colmap);
    }

    HudSetAlpha(old_alpha);
}

static void ReallyDoStartLevel(skill_t skill, GameDefinition *g)
{
    NewGameParameters params;

    params.skill_      = skill;
    params.deathmatch_ = 0;

    params.random_seed_ = PureRandomNumber();

    params.SinglePlayer(0);

    params.map_ = GameLookupMap(g->firstmap_.c_str());

    if (!params.map_)
    {
        // 23-6-98 KM Fixed this.
        MenuSetupNextMenu(&EpisodeMenuDefinition);
        MenuStartMessage(language["EpisodeNonExist"], nullptr, false);
        return;
    }

    SYS_ASSERT(GameMapExists(params.map_));
    SYS_ASSERT(params.map_->episode_);

    GameDeferredNewGame(params);

    MenuClear();
}

static void DoStartLevel(skill_t skill)
{
    // -KM- 1998/12/17 Clear the intermission.
    IntermissionClear();

    // find episode
    GameDefinition *g = nullptr;

    std::string chosen_episodesode =
        epi::StringFormat("%s", EpisodeMenu[chosen_episode].name);

    for (auto game : gamedefs)
    {
        // Lobo 2022: lets use text instead of M_EPIxx graphic
        if (game->description_ != "")
        {
            std::string gamedef_episode =
                epi::StringFormat("%s", language[game->description_.c_str()]);
            if (DDF_CompareName(gamedef_episode.c_str(),
                                chosen_episodesode.c_str()) == 0)
            {
                g = game;
                break;
            }
        }
        else
        {
            if (DDF_CompareName(game->name_.c_str(),
                                chosen_episodesode.c_str()) == 0)
            {
                g = game;
                break;
            }
        }

        /*
        if (!strcmp(game->namegraphic.c_str(),
        EpisodeMenu[chosen_episode].patch_name))
        {
            g = game;
            break;
        }
        */
    }

    // Sanity checking...
    if (!g)
    {
        LogWarning("Internal Error: no episode for '%s'.\n",
                   chosen_episodesode.c_str());
        MenuClear();
        return;
    }

    const MapDefinition *map = GameLookupMap(g->firstmap_.c_str());
    if (!map)
    {
        LogWarning("Cannot find map for '%s' (episode %s)\n",
                   g->firstmap_.c_str(), chosen_episodesode.c_str());
        MenuClear();
        return;
    }

    ReallyDoStartLevel(skill, g);
}

static void VerifyNightmare(int ch)
{
    if (ch != 'y' && ch != KEYD_GP_A && ch != KEYD_MOUSE1) return;

    DoStartLevel(sk_nightmare);
}

void MenuChooseSkill(int choice)
{
    if (choice == sk_nightmare)
    {
        MenuStartMessage(language["NightMareCheck"], VerifyNightmare, true);
        return;
    }

    DoStartLevel((skill_t)choice);
}

void MenuEpisode(int choice)
{
    chosen_episode = choice;
    if (EpisodeMenuSkipSkill[chosen_episode])
        DoStartLevel((skill_t)2);
    else
        MenuSetupNextMenu(&SkillMenuDefinition);
}

//
// Toggle messages on/off
//
void MenuChangeMessages(int choice)
{
    // warning: unused parameter `int choice'
    (void)choice;

    show_messages = 1 - show_messages;

    if (show_messages)
        ConsolePrint("%s\n", language["MessagesOn"]);
    else
        ConsolePrint("%s\n", language["MessagesOff"]);
}

static void EndGameResponse(int ch)
{
    if (ch != 'y' && ch != KEYD_GP_A && ch != KEYD_MOUSE1) return;

    GameDeferredEndGame();

    current_menu->last_on = item_on;
    MenuClear();
}

void MenuEndGame(int choice, ConsoleVariable *cvar)
{
    if (game_state != GS_LEVEL)
    {
        S_StartFX(sound_effect_oof);
        return;
    }

    option_menu_on  = 0;
    network_game_menu_on = 0;

    if (network_game)
    {
        MenuStartMessage(language["EndNetGame"], nullptr, false);
        return;
    }

    MenuStartMessage(language["EndGameCheck"], EndGameResponse, true);
}

void MenuReadThis(int choice) { MenuSetupNextMenu(&ReadThisMenuDefinition1); }

void MenuReadThis2(int choice) { MenuSetupNextMenu(&ReadThisMenuDefinition2); }

void MenuFinishReadThis(int choice) { MenuSetupNextMenu(&MainMenuDefinition); }

//
// -KM- 1998/12/16 Handle sfx that don't exist in this version.
// -KM- 1999/01/31 Generate quitsounds from default.ldf
//
static void QuitResponse(int ch)
{
    if (ch != 'y' && ch != KEYD_GP_A && ch != KEYD_MOUSE1) return;

    if (!network_game)
    {
        int  numsounds = 0;
        char refname[64];
        char sound[64];
        int  i, start;

        // Count the quit messages
        do {
            sprintf(refname, "QuitSnd%d", numsounds + 1);
            if (language.IsValidRef(refname))
                numsounds++;
            else
                break;
        } while (true);

        if (numsounds)
        {
            // cycle through all the quit sounds, until one of them exists
            // (some of the default quit sounds do not exist in DOOM 1)
            start = i = RandomByte() % numsounds;
            do {
                sprintf(refname, "QuitSnd%d", i + 1);
                sprintf(sound, "DS%s", language[refname]);
                if (W_CheckNumForName(sound) != -1)
                {
                    S_StartFX(sfxdefs.GetEffect(language[refname]));
                    break;
                }
                i = (i + 1) % numsounds;
            } while (i != start);
        }
    }

    // -ACB- 1999/09/20 New exit code order
    // Write the default config file first
    LogPrint("Saving system defaults...\n");
    ConfigurationSaveDefaults();

    LogPrint("Exiting...\n");

    E_EngineShutdown();
    SystemShutdown();

    CloseProgram(EXIT_SUCCESS);
}

//
// -ACB- 1998/07/19 Removed offensive messages selection (to some people);
//     Better Random Selection.
//
// -KM- 1998/07/21 Reinstated counting quit messages, so adding them to
// dstrings.c
//                   is all you have to do.  Using RandomByteDeterministic for the random
//                   number automatically kills the sync... (hence RandomByte()...
//                   -AJA-).
//
// -KM- 1998/07/31 Removed Limit. So there.
// -KM- 1999/01/31 Load quit messages from default.ldf
//
void MenuQuitEdge(int choice)
{
#if EDGE_WEB
    LogPrint("Quit ignored on web platform\n");
    return;
#endif

    char ref[64];

    std::string msg;

    int num_quitmessages = 0;

    // Count the quit messages
    do {
        num_quitmessages++;

        sprintf(ref, "QUITMSG%d", num_quitmessages);
    } while (language.IsValidRef(ref));

    // we stopped at one higher than the last
    num_quitmessages--;

    // -ACB- 2004/08/14 Allow fallback to just the "PressToQuit" message
    if (num_quitmessages > 0)
    {
        // Pick one at random
        sprintf(ref, "QUITMSG%d", 1 + (RandomByte() % num_quitmessages));

        // Construct the quit message in full
        msg = epi::StringFormat("%s\n\n%s", language[ref],
                                language["PressToQuit"]);
    }
    else { msg = std::string(language["PressToQuit"]); }

    // Trigger the message
    MenuStartMessage(msg.c_str(), QuitResponse, true);
}

// Accessible from console's 'quit now' command
void MenuImmediateQuit()
{
#if EDGE_WEB
    LogPrint("Quit ignored on web platform\n");
    return;
#endif

    LogPrint("Saving system defaults...\n");
    ConfigurationSaveDefaults();

    LogPrint("Exiting...\n");

    E_EngineShutdown();
    SystemShutdown();

    CloseProgram(EXIT_SUCCESS);
}

//----------------------------------------------------------------------------
//   MENU FUNCTIONS
//----------------------------------------------------------------------------

void MenuDrawSlider(int x, int y, float slider_position, float increment,
                    int div, float min, float max, std::string format_string)
{
    float basex      = x;
    int   step       = (8 / div);
    float scale_step = 50.0f / ((max - min) / increment);

    // Capture actual value first since it will be aligned to the slider
    // increment
    std::string actual_val =
        format_string.empty()
            ? ""
            : epi::StringFormat(format_string.c_str(), slider_position);

    slider_position = HMM_Clamp(min, slider_position, max);

    slider_position = slider_position - remainderf(slider_position, increment);

    Style *opt_style = hud_styles.Lookup(styledefs.Lookup("OPTIONS"));

    // If using an IMAGE or TRUETYPE type font for the menu, use a
    // COALHUDs-style bar for the slider instead
    if (opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                ->definition_->type_ == kFontTypeImage ||
        opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                ->definition_->type_ == kFontTypeTrueType)
    {
        RGBAColor slider_color = SG_WHITE_RGBA32;

        const Colormap *colmap =
            opt_style->definition_
                ->text_[StyleDefinition::kTextSectionAlternate]
                .colmap_;

        if (colmap) slider_color = V_GetFontColor(colmap);

        HudThinBox(
            x,
            y + (opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                             ->definition_->type_ == kFontTypeTrueType
                     ? opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                           ->truetype_reference_yshift_[current_font_size]
                     : 0),
            x + 50.0f,
            y +
                opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                    ->NominalHeight() +
                (opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                             ->definition_->type_ == kFontTypeTrueType
                     ? opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                               ->truetype_reference_yshift_[current_font_size] /
                           2
                     : 0),
            slider_color);
        HudSolidBox(
            x,
            y + (opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                             ->definition_->type_ == kFontTypeTrueType
                     ? opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                           ->truetype_reference_yshift_[current_font_size]
                     : 0),
            x + (((slider_position - min) / increment) * scale_step),
            y +
                opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                    ->NominalHeight() +
                (opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                             ->definition_->type_ == kFontTypeTrueType
                     ? opt_style->fonts_[StyleDefinition::kTextSectionAlternate]
                               ->truetype_reference_yshift_[current_font_size] /
                           2
                     : 0),
            slider_color);
        if (!actual_val.empty())
            HudWriteText(opt_style, StyleDefinition::kTextSectionAlternate,
                         x + 50.0f + step, y, actual_val.c_str());
    }
    else
    {
        // Note: the (step+1) here is for compatibility with the original
        // code.  It seems required to make the thermo bar tile properly.

        int i = 0;

        HudStretchImage(x, y, step + 1, IM_HEIGHT(therm_l) / div, therm_l, 0.0,
                        0.0);

        for (x += step; i < (50 / step); i++, x += step)
        {
            HudStretchImage(x, y, step + 1, IM_HEIGHT(therm_m) / div, therm_m,
                            0.0, 0.0);
        }

        HudStretchImage(x, y, step + 1, IM_HEIGHT(therm_r) / div, therm_r, 0.0,
                        0.0);

        HudStretchImage(
            basex + ((slider_position - min) / increment) * scale_step + 1, y,
            step + 1, IM_HEIGHT(therm_o) / div, therm_o, 0.0, 0.0);

        if (!actual_val.empty())
            HudWriteText(opt_style, StyleDefinition::kTextSectionAlternate,
                         basex + (((max - min) / increment) * scale_step) +
                             (step * 2 + 2),
                         y, actual_val.c_str());
    }
}

void MenuStartMessage(const char *string, void (*routine)(int response),
                      bool        input)
{
    message_last_menu     = menu_active;
    message_mode          = 1;
    message_string        = std::string(string);
    message_key_routine   = routine;
    message_input_routine = nullptr;
    message_needs_input   = input;
    menu_active           = true;
    ConsoleSetVisible(kConsoleVisibilityNotVisible);
    return;
}

//
// -KM- 1998/07/21 Call M_StartMesageInput to start a message that needs a
//                 string input. (You can convert it to a number if you want
//                 to.)
//
// string:  The prompt.
//
// routine: Format is void routine(char *s)  Routine will be called
//          with a pointer to the input in s.  s will be nullptr if the user
//          pressed ESCAPE to cancel the input.
//
void MenuStartMessageInput(const char *string,
                           void (*routine)(const char *response))
{
    message_last_menu     = menu_active;
    message_mode          = 2;
    message_string        = std::string(string);
    message_input_routine = routine;
    message_key_routine   = nullptr;
    message_needs_input   = true;
    menu_active           = true;
    ConsoleSetVisible(kConsoleVisibilityNotVisible);
    return;
}

#if 0
void M_StopMessage(void)
{
	menu_active = message_last_menu?true:false;
	message_string.Clear();
	message_mode = 0;
  
	if (!menu_active)
		save_screenshot_valid = false;
}
#endif

//
// CONTROL PANEL
//

//
// -KM- 1998/09/01 Analogue binding, and hat support
//
bool MenuResponder(InputEvent *ev)
{
    int i;

    if (ev->type != kInputEventKeyDown) return false;

    int ch = ev->value.key.sym;

    SDL_Keymod mod = SDL_GetModState();

    // -ACB- 1999/10/11 F1 is responsible for print screen at any time
    if (ch == KEYD_F1 || ch == KEYD_PRTSCR)
    {
        GameDeferredScreenShot();
        return true;
    }

    // Take care of any messages that need input
    // -KM- 1998/07/21 Message Input
    if (message_mode == 1)
    {
        if (message_needs_input == true &&
            !(ch == ' ' || ch == 'n' || ch == 'y' || ch == KEYD_ESCAPE ||
              ch == KEYD_GP_B || ch == KEYD_GP_A || ch == KEYD_MOUSE1 ||
              ch == KEYD_MOUSE2 || ch == KEYD_MOUSE3))
            return false;

        message_mode = 0;
        // -KM- 1998/07/31 Moved this up here to fix bugs.
        menu_active = message_last_menu ? true : false;

        if (message_key_routine) (*message_key_routine)(ch);

        S_StartFX(sound_effect_swtchx);
        return true;
    }
    else if (message_mode == 2)
    {
        if (ch == KEYD_ENTER || ch == KEYD_GP_A || ch == KEYD_MOUSE1)
        {
            menu_active  = message_last_menu ? true : false;
            message_mode = 0;

            if (message_input_routine)
                (*message_input_routine)(input_string.c_str());

            input_string.clear();

            MenuClear();
            S_StartFX(sound_effect_swtchx);
            return true;
        }

        if (ch == KEYD_ESCAPE || ch == KEYD_GP_B || ch == KEYD_MOUSE2 ||
            ch == KEYD_MOUSE3)
        {
            menu_active  = message_last_menu ? true : false;
            message_mode = 0;

            if (message_input_routine) (*message_input_routine)(nullptr);

            input_string.clear();

            MenuClear();
            S_StartFX(sound_effect_swtchx);
            return true;
        }

        if ((ch == KEYD_BACKSPACE || ch == KEYD_DELETE) &&
            !input_string.empty())
        {
            std::string s = input_string.c_str();

            if (input_string != "")
            {
                input_string.resize(input_string.size() - 1);
            }

            return true;
        }

        if (mod & KMOD_SHIFT || mod & KMOD_CAPS) ch = epi::ToUpperASCII(ch);
        if (ch == '-') ch = '_';

        if (ch >= 32 && ch <= 126)  // FIXME: international characters ??
        {
            // Set the input_string only if fits
            if (input_string.size() < 64) { input_string += ch; }
        }

        return true;
    }

    // new MainMenuOptions menu on - use that responder
    if (option_menu_on) return OptionMenuResponder(ev, ch);

    if (network_game_menu_on) return NetworkGameResponder(ev, ch);

    // Save Game string input
    if (entering_save_string)
    {
        switch (ch)
        {
            case KEYD_BACKSPACE:
                if (save_string_character_index > 0)
                {
                    save_string_character_index--;
                    save_extended_information_slots[save_slot]
                        .description[save_string_character_index] = 0;
                }
                break;

            case KEYD_ESCAPE:
            case KEYD_GP_B:
            case KEYD_MOUSE2:
            case KEYD_MOUSE3:
                entering_save_string = 0;
                strcpy(save_extended_information_slots[save_slot].description,
                       old_save_string);
                break;

            case KEYD_ENTER:
            case KEYD_GP_A:
            case KEYD_MOUSE1:
                entering_save_string = 0;
                if (save_extended_information_slots[save_slot].description[0])
                {
                    M_DoSave(save_page, save_slot);
                }
                else
                {
                    std::string default_name = epi::StringFormat(
                        "SAVE-%d", save_page * kTotalSaveSlots + save_slot + 1);
                    for (; (size_t)save_string_character_index <
                           default_name.size();
                         save_string_character_index++)
                    {
                        save_extended_information_slots[save_slot]
                            .description[save_string_character_index] =
                            default_name[save_string_character_index];
                    }
                    save_extended_information_slots[save_slot]
                        .description[save_string_character_index] = 0;
                    M_DoSave(save_page, save_slot);
                }
                break;

            default:
                if (mod & KMOD_SHIFT || mod & KMOD_CAPS)
                    ch = epi::ToUpperASCII(ch);
                SYS_ASSERT(save_style);
                if (ch >= 32 && ch <= 127 &&
                    save_string_character_index < kSaveStringSize - 1 &&
                    save_style->fonts_[1]->StringWidth(
                        save_extended_information_slots[save_slot]
                            .description) < (kSaveStringSize - 2) * 8)
                {
                    save_extended_information_slots[save_slot]
                        .description[save_string_character_index++] = ch;
                    save_extended_information_slots[save_slot]
                        .description[save_string_character_index] = 0;
                }
                break;
        }
        return true;
    }

    // F-Keys
    if (!menu_active)
    {
        if (EventMatchesKey(key_screenshot, ch)) { ch = KEYD_SCREENSHOT; }
        if (EventMatchesKey(key_save_game, ch)) { ch = KEYD_SAVEGAME; }
        if (EventMatchesKey(key_load_game, ch)) { ch = KEYD_LOADGAME; }
        if (EventMatchesKey(key_sound_controls, ch))
        {
            ch = KEYD_SOUNDCONTROLS;
        }
        if (EventMatchesKey(key_options_menu, ch)) { ch = KEYD_OPTIONSMENU; }
        if (EventMatchesKey(key_quick_save, ch)) { ch = KEYD_QUICKSAVE; }
        if (EventMatchesKey(key_end_game, ch)) { ch = KEYD_ENDGAME; }
        if (EventMatchesKey(key_message_toggle, ch))
        {
            ch = KEYD_MESSAGETOGGLE;
        }
        if (EventMatchesKey(key_quick_load, ch)) { ch = KEYD_QUICKLOAD; }
        if (EventMatchesKey(key_quit_edge, ch)) { ch = KEYD_QUITEDGE; }
        if (EventMatchesKey(key_gamma_toggle, ch)) { ch = KEYD_GAMMATOGGLE; }

        switch (ch)
        {
            case KEYD_MINUS:  // Screen size down

                if (automap_active) return false;

                screen_hud = (screen_hud - 1 + NUMHUD) % NUMHUD;

                S_StartFX(sound_effect_stnmov);
                return true;

            case KEYD_EQUALS:  // Screen size up

                if (automap_active) return false;

                screen_hud = (screen_hud + 1) % NUMHUD;

                S_StartFX(sound_effect_stnmov);
                return true;

            case KEYD_SAVEGAME:  // Save

                MenuStartControlPanel();
                S_StartFX(sound_effect_swtchn);
                MenuSaveGame(0);
                return true;

            case KEYD_LOADGAME:  // Load

                MenuStartControlPanel();
                S_StartFX(sound_effect_swtchn);
                MenuLoadGame(0);
                return true;

            case KEYD_SOUNDCONTROLS:  // Sound Volume

                S_StartFX(sound_effect_swtchn);
                MenuStartControlPanel();
                MenuF4SoundOptions(0);
                return true;

            case KEYD_OPTIONSMENU:  // Detail toggle, now loads MainMenuOptions
                                    // menu
                // -KM- 1998/07/31 F5 now loads MainMenuOptions menu, detail is
                // obsolete.

                S_StartFX(sound_effect_swtchn);
                MenuStartControlPanel();
                MenuOptions(1);
                return true;

            case KEYD_QUICKSAVE:  // Quicksave

                S_StartFX(sound_effect_swtchn);
                MenuQuickSave();
                return true;

            case KEYD_ENDGAME:  // End game

                S_StartFX(sound_effect_swtchn);
                MenuEndGame(0);
                return true;

            case KEYD_MESSAGETOGGLE:  // Toggle messages

                MenuChangeMessages(0);
                S_StartFX(sound_effect_swtchn);
                return true;

            case KEYD_QUICKLOAD:  // Quickload

                S_StartFX(sound_effect_swtchn);
                MenuQuickLoad();
                return true;

            case KEYD_QUITEDGE:  // Quit DOOM

                S_StartFX(sound_effect_swtchn);
                MenuQuitEdge(0);
                return true;

            case KEYD_GAMMATOGGLE:  // gamma toggle

                v_secbright.d_++;
                if (v_secbright.d_ > 10) v_secbright.d_ = 0;

                v_secbright = v_secbright.d_;

                std::string msg =
                    "Sector Brightness ";  // TODO: Make language entry - Dasho

                switch (v_secbright.d_)
                {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                        msg.append("-");
                        msg.append(std::to_string((5 - v_secbright.d_) * 10));
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
                        msg.append(std::to_string((5 - v_secbright.d_) * -10));
                        break;
                    default:
                        msg.clear();
                        break;
                }

                if (!msg.empty())
                    ConsolePlayerMessage(consoleplayer, "%s", msg.c_str());

                // -AJA- 1999/07/03: removed PLAYPAL reference.
                return true;
        }

        // Pop-up menu?
        if (ch == KEYD_ESCAPE || ch == KEYD_GP_START)
        {
            MenuStartControlPanel();
            S_StartFX(sound_effect_swtchn);
            return true;
        }
        return false;
    }

    // Keys usable within menu
    switch (ch)
    {
        case KEYD_WHEEL_DN:
            do {
                if (item_on + 1 > current_menu->total_items - 1)
                {
                    if (current_menu->menu_items[item_on].select_function &&
                        current_menu->menu_items[item_on].status == 2)
                    {
                        S_StartFX(sound_effect_stnmov);
                        // 98-7-10 KM Use new defines
                        (*current_menu->menu_items[item_on].select_function)(
                            kSliderRight);
                        item_on = 0;
                        return true;
                    }
                    else
                        item_on = 0;
                }
                else
                    item_on++;
                S_StartFX(sound_effect_pstop);
            } while (current_menu->menu_items[item_on].status == -1);
            return true;

        case KEYD_WHEEL_UP:
            do {
                if (item_on == 0)
                {
                    if (current_menu->menu_items[item_on].select_function &&
                        current_menu->menu_items[item_on].status == 2)
                    {
                        S_StartFX(sound_effect_stnmov);
                        // 98-7-10 KM Use new defines
                        (*current_menu->menu_items[item_on].select_function)(
                            kSliderLeft);
                        item_on = current_menu->total_items - 1;
                        return true;
                    }
                    else
                        item_on = current_menu->total_items - 1;
                }
                else
                    item_on--;
                S_StartFX(sound_effect_pstop);
            } while (current_menu->menu_items[item_on].status == -1);
            return true;

        case KEYD_DOWNARROW:
        case KEYD_GP_DOWN:
            do {
                if (item_on + 1 > current_menu->total_items - 1)
                    item_on = 0;
                else
                    item_on++;
                S_StartFX(sound_effect_pstop);
            } while (current_menu->menu_items[item_on].status == -1);
            return true;

        case KEYD_UPARROW:
        case KEYD_GP_UP:
            do {
                if (item_on == 0)
                    item_on = current_menu->total_items - 1;
                else
                    item_on--;
                S_StartFX(sound_effect_pstop);
            } while (current_menu->menu_items[item_on].status == -1);
            return true;

        case KEYD_PGUP:
        case KEYD_LEFTARROW:
        case KEYD_GP_LEFT:
            if (current_menu->menu_items[item_on].select_function &&
                current_menu->menu_items[item_on].status == 2)
            {
                S_StartFX(sound_effect_stnmov);
                // 98-7-10 KM Use new defines
                (*current_menu->menu_items[item_on].select_function)(
                    kSliderLeft);
            }
            return true;

        case KEYD_PGDN:
        case KEYD_RIGHTARROW:
        case KEYD_GP_RIGHT:
            if (current_menu->menu_items[item_on].select_function &&
                current_menu->menu_items[item_on].status == 2)
            {
                S_StartFX(sound_effect_stnmov);
                // 98-7-10 KM Use new defines
                (*current_menu->menu_items[item_on].select_function)(
                    kSliderRight);
            }
            return true;

        case KEYD_ENTER:
        case KEYD_MOUSE1:
        case KEYD_GP_A:
            if (current_menu->menu_items[item_on].select_function &&
                current_menu->menu_items[item_on].status)
            {
                current_menu->last_on = item_on;
                (*current_menu->menu_items[item_on].select_function)(item_on);
                S_StartFX(sound_effect_pistol);
            }
            return true;

        case KEYD_ESCAPE:
        case KEYD_MOUSE2:
        case KEYD_MOUSE3:
        case KEYD_GP_START:
            current_menu->last_on = item_on;
            MenuClear();
            S_StartFX(sound_effect_swtchx);
            return true;

        case KEYD_BACKSPACE:
        case KEYD_GP_B:
            current_menu->last_on = item_on;
            if (current_menu->previous_menu)
            {
                current_menu = current_menu->previous_menu;
                item_on      = current_menu->last_on;
                S_StartFX(sound_effect_swtchn);
            }
            return true;

        default:
            for (i = item_on + 1; i < current_menu->total_items; i++)
                if (current_menu->menu_items[i].alpha_key == ch)
                {
                    item_on = i;
                    S_StartFX(sound_effect_pstop);
                    return true;
                }
            for (i = 0; i <= item_on; i++)
                if (current_menu->menu_items[i].alpha_key == ch)
                {
                    item_on = i;
                    S_StartFX(sound_effect_pstop);
                    return true;
                }
            break;
    }

    return false;
}

void MenuStartControlPanel(void)
{
    // intro might call this repeatedly
    if (menu_active) return;

    menu_active = true;
    ConsoleSetVisible(kConsoleVisibilityNotVisible);

    current_menu = &MainMenuDefinition;    // JDC
    item_on      = current_menu->last_on;  // JDC

    OptionMenuCheckNetworkGame();
}

static int FindChar(std::string &str, char ch, int pos)
{
    SYS_ASSERT(pos <= (int)str.size());

    const char *scan = strchr(str.c_str() + pos, ch);

    if (!scan) return -1;

    return (int)(scan - str.c_str());
}

static std::string GetMiddle(std::string &str, int pos, int len)
{
    SYS_ASSERT(pos >= 0 && len >= 0);
    SYS_ASSERT(pos + len <= (int)str.size());

    if (len == 0) return std::string();

    return std::string(str.c_str() + pos, len);
}

static void DrawMessage(void)
{
    if (message_key_routine == QuitResponse &&
        !exit_style->background_image_)  // Respect dialog styles with custom
                                         // backgrounds
    {
        StartFrame();  // To clear and ensure solid black background regardless
                       // of style

        if (exit_style->definition_->text_[StyleDefinition::kTextSectionText]
                .colmap_)
        {
            HudSetTextColor(
                V_GetFontColor(exit_style->definition_
                                   ->text_[StyleDefinition::kTextSectionText]
                                   .colmap_));
        }

        if (exit_style->fonts_[StyleDefinition::kTextSectionText])
        {
            HudSetFont(exit_style->fonts_[StyleDefinition::kTextSectionText]);
        }
        HudSetScale(
            exit_style->definition_->text_[StyleDefinition::kTextSectionText]
                .scale_);

        HudDrawQuitScreen();
        return;
    }

    // short x; // Seems unused for now - Dasho
    short y;

    SYS_ASSERT(exit_style);

    exit_style->DrawBackground();

    // FIXME: HU code should support center justification: this
    // would remove the code duplication below...

    std::string msg(message_string);

    std::string input(input_string);

    if (message_mode == 2) input += "_";

    // Calc required height

    std::string s = msg + input;

    y = 100 -
        (exit_style->fonts_[StyleDefinition::kTextSectionText]->StringLines(
             s.c_str()) *
         exit_style->fonts_[StyleDefinition::kTextSectionText]
             ->NominalHeight() /
         2);

    if (!msg.empty())
    {
        int oldpos = 0;
        int pos;

        do {
            pos = FindChar(msg, '\n', oldpos);

            if (pos < 0)
                s = std::string(msg, oldpos);
            else
                s = GetMiddle(msg, oldpos, pos - oldpos);

            if (s.size() > 0)
            {
                HudSetAlignment(0, -1);  // center it
                HudWriteText(exit_style, StyleDefinition::kTextSectionText, 160,
                             y, s.c_str());
                HudSetAlignment(-1, -1);  // set it back to usual
            }

            y += exit_style->fonts_[StyleDefinition::kTextSectionText]
                     ->NominalHeight();

            oldpos = pos + 1;
        } while (pos >= 0 && oldpos < (int)msg.size());
    }

    if (!input.empty())
    {
        int oldpos = 0;
        int pos;

        do {
            pos = FindChar(input, '\n', oldpos);

            if (pos < 0)
                s = std::string(input, oldpos);
            else
                s = GetMiddle(input, oldpos, pos - oldpos);

            if (s.size() > 0)
            {
                HudSetAlignment(0, -1);  // center it
                HudWriteText(exit_style, StyleDefinition::kTextSectionText, 160,
                             y, s.c_str());
                HudSetAlignment(-1, -1);  // set it back to usual
            }

            y += exit_style->fonts_[0]->NominalHeight();

            oldpos = pos + 1;
        } while (pos >= 0 && oldpos < (int)input.size());
    }
}

float ShortestLine;
float TallestLine;
float WidestLine;
//
// Draw our menu cursor
//
void MenuDrawCursor(Style *style, bool graphical_item)
{
    bool  graphical_cursor = false;
    float TempScale        = 0;
    float TempWidth        = 0;
    float TempSpacer       = 0;
    float y_shift          = 0;
    int   txtWidth         = 0;
    short old_offset_x     = 0;
    short old_offset_y     = 0;
    short TempX            = 0;
    short TempY            = 0;

    float old_alpha = HudGetAlpha();

    float txtscale =
        style->definition_->text_[StyleDefinition::kTextSectionText].scale_;

    // const colourmap_c *colmap =
    // style->definition_->text[StyleDefinition::kTextSectionText].colmap; //
    // Should we allow a colmap for the cursor?
    const Colormap *colmap = nullptr;

    //-------------------------------------------------------------
    // 1. First up, do we want a graphical cursor or a text one?
    //-------------------------------------------------------------
    image_c *cursor;
    if (style->definition_->cursor_.cursor_string_ != "")
        cursor = nullptr;
    else if (style->definition_->cursor_.alt_cursor_ != "")
        cursor = (image_c *)W_ImageLookup(
            style->definition_->cursor_.alt_cursor_.c_str());
    else
        cursor = menu_skull[0];

    if (cursor)  // we're using a graphic for the cursor
        graphical_cursor = true;

    HudSetAlpha(style->definition_->cursor_.translucency_);

    //-------------------------------------------------------------
    // 2. Start drawing our cursor. We have to check if the
    // current menu item is graphical or text to do the calcs.
    //-------------------------------------------------------------
    // graphical_item==false //We're going text-based menu items
    // graphical_item==true //We're going graphic-based menu items
    if (graphical_cursor == false)  // We're going text-based cursor
    {
        TempWidth =
            style->fonts_[StyleDefinition::kTextSectionText]->StringWidth(
                style->definition_->cursor_.cursor_string_.c_str()) *
            txtscale;
        TempSpacer =
            style->fonts_[StyleDefinition::kTextSectionText]->CharWidth(
                style->definition_->cursor_.cursor_string_[0]) *
            txtscale * 0.2;
    }
    else  // We're going graphical cursor
    {
        old_offset_x     = cursor->offset_x;
        old_offset_y     = cursor->offset_y;
        cursor->offset_x = 0;
        cursor->offset_y = 0;

        if (style->definition_->cursor_.force_offsets_)
        {
            cursor->offset_x += old_offset_x;
            cursor->offset_y += old_offset_y;
        }

        if (graphical_item == false)
        {
            if (style->fonts_[StyleDefinition::kTextSectionText]
                    ->definition_->type_ == kFontTypeTrueType)
            {
                ShortestLine =
                    style->fonts_[StyleDefinition::kTextSectionText]
                        ->truetype_reference_height_[current_font_size] *
                    txtscale;
                y_shift = style->fonts_[StyleDefinition::kTextSectionText]
                              ->truetype_reference_yshift_[current_font_size] *
                          txtscale;
            }
        }
        TempScale = ShortestLine / IM_HEIGHT(cursor);
        TempWidth = IM_WIDTH(cursor) * TempScale;
        if (!style->definition_->cursor_.scaling_)
        {
            current_menu->menu_items[item_on].y -=
                (IM_HEIGHT(cursor) - ShortestLine) / 2;
            ShortestLine = IM_HEIGHT(cursor);
            TempWidth    = IM_WIDTH(cursor);
        }
    }

    TempSpacer = TempWidth * 0.2;  // 20% of cursor graphic is our space
    if (style->definition_->cursor_.position_ ==
        StyleDefinition::kAlignmentBoth)
    {
        if (style->definition_->entry_alignment_ ==
            StyleDefinition::kAlignmentRight)
        {
            // Left cursor
            if (graphical_item == false)
                txtWidth =
                    style->fonts_[StyleDefinition::kTextSectionText]
                        ->StringWidth(current_menu->menu_items[item_on].name) *
                    txtscale;
            else
                txtWidth = IM_WIDTH(current_menu->menu_items[item_on].image) *
                           txtscale;

            TempX =
                current_menu->menu_items[item_on].x + WidestLine - TempSpacer;
            TempX -= txtWidth;
            TempX -= TempWidth;

            TempY = current_menu->menu_items[item_on].y + y_shift;
            if (graphical_item == true)
            {
                TempX -= (current_menu->menu_items[item_on].image->offset_x *
                          txtscale) *
                         2;
                TempY -= (current_menu->menu_items[item_on].image->offset_y *
                          txtscale);
            }
            if (graphical_cursor == true)
            {
                TempX -= (cursor->offset_x * txtscale);
                TempY -= (cursor->offset_y * txtscale);
            }

            if (graphical_cursor == true)
            {
                HudStretchImage(TempX, TempY, TempWidth, ShortestLine, cursor,
                                0.0, 0.0, colmap);
            }
            else
                HudWriteText(
                    style, StyleDefinition::kTextSectionText, TempX, TempY,
                    style->definition_->cursor_.cursor_string_.c_str());

            // Right cursor
            TempX =
                current_menu->menu_items[item_on].x + WidestLine + TempSpacer;

            TempY = current_menu->menu_items[item_on].y + y_shift;
            if (graphical_item == true)
            {
                TempX -= (current_menu->menu_items[item_on].image->offset_x *
                          txtscale) *
                         2;
                TempY -= (current_menu->menu_items[item_on].image->offset_y *
                          txtscale);
            }
            if (graphical_cursor == true)
            {
                TempX -= (cursor->offset_x * txtscale);
                TempY -= (cursor->offset_y * txtscale);
            }

            if (graphical_cursor == true)
            {
                HudStretchImage(TempX, TempY, TempWidth, ShortestLine, cursor,
                                0.0, 0.0, colmap);
            }
            else
                HudWriteText(
                    style, StyleDefinition::kTextSectionText, TempX, TempY,
                    style->definition_->cursor_.cursor_string_.c_str());
        }
        else
        {
            // Left cursor
            TempX =
                current_menu->menu_items[item_on].x - TempWidth - TempSpacer;
            TempY = current_menu->menu_items[item_on].y + y_shift;

            if (graphical_item == true)
            {
                TempX -= (current_menu->menu_items[item_on].image->offset_x *
                          txtscale) *
                         2;
                TempY -= (current_menu->menu_items[item_on].image->offset_y *
                          txtscale);
            }
            if (graphical_cursor == true)
            {
                TempX -= (cursor->offset_x * txtscale);
                TempY -= (cursor->offset_y * txtscale);
            }

            if (graphical_cursor == true)
            {
                HudStretchImage(TempX, TempY, TempWidth, ShortestLine, cursor,
                                0.0, 0.0, colmap);
            }
            else
                HudWriteText(
                    style, StyleDefinition::kTextSectionText, TempX, TempY,
                    style->definition_->cursor_.cursor_string_.c_str());

            // Right cursor
            if (graphical_item == false)
                txtWidth =
                    style->fonts_[StyleDefinition::kTextSectionText]
                        ->StringWidth(current_menu->menu_items[item_on].name) *
                    txtscale;
            else
                txtWidth = IM_WIDTH(current_menu->menu_items[item_on].image) *
                           txtscale;

            TempX = current_menu->menu_items[item_on].x + txtWidth + TempSpacer;
            TempY = current_menu->menu_items[item_on].y + y_shift;
            if (graphical_item == true)
            {
                TempX -= (current_menu->menu_items[item_on].image->offset_x *
                          txtscale) *
                         2;
                TempY -= (current_menu->menu_items[item_on].image->offset_y *
                          txtscale);
            }
            if (graphical_cursor == true)
            {
                TempX -= (cursor->offset_x * txtscale);
                TempY -= (cursor->offset_y * txtscale);
            }
            if (graphical_cursor == true)
            {
                HudStretchImage(TempX, TempY, TempWidth, ShortestLine, cursor,
                                0.0, 0.0, colmap);
            }
            else
                HudWriteText(
                    style, StyleDefinition::kTextSectionText, TempX, TempY,
                    style->definition_->cursor_.cursor_string_.c_str());
        }
    }
    else if (style->definition_->cursor_.position_ ==
             StyleDefinition::kAlignmentCenter)
    {
        TempX = 0;

        if (graphical_cursor == true)
        {
            TempX = CenterMenuImage2(style, StyleDefinition::kTextSectionText,
                                     cursor);  // + TempSpacer;
            TempY = current_menu->menu_items[item_on].y + y_shift;
            if (graphical_item == true)
            {
                TempY -= (current_menu->menu_items[item_on].image->offset_y *
                          txtscale);
            }
            TempX -= (cursor->offset_x * txtscale);
            TempY -= (cursor->offset_y * txtscale);

            if (style->definition_->cursor_.border_)
                HudStretchImage(current_menu->menu_items[item_on].x, TempY,
                                WidestLine, TallestLine, cursor, 0.0, 0.0,
                                colmap);
            else
                HudStretchImage(TempX, TempY, TempWidth, ShortestLine, cursor,
                                0.0, 0.0, colmap);
        }
        else
        {
            TempX = CenterMenuText(style, StyleDefinition::kTextSectionText,
                                   style->definition_->cursor_.cursor_string_
                                       .c_str());  // + TempSpacer;
            TempY = current_menu->menu_items[item_on].y + y_shift;
            if (graphical_item == true)
            {
                TempY -= (current_menu->menu_items[item_on].image->offset_y *
                          txtscale);
            }
            HudWriteText(style, StyleDefinition::kTextSectionText, TempX, TempY,
                         style->definition_->cursor_.cursor_string_.c_str());
        }
    }
    else if (style->definition_->cursor_.position_ ==
             StyleDefinition::kAlignmentRight)
    {
        TempX = 0;

        if (style->definition_->entry_alignment_ ==
            StyleDefinition::kAlignmentCenter)
        {
            if (graphical_item == false)
                txtWidth =
                    style->fonts_[StyleDefinition::kTextSectionText]
                        ->StringWidth(current_menu->menu_items[item_on].name) *
                    txtscale;
            else
                txtWidth = IM_WIDTH(current_menu->menu_items[item_on].image) *
                           txtscale;

            TempX = current_menu->menu_items[item_on].x + txtWidth + TempSpacer;
        }
        else
            TempX =
                current_menu->menu_items[item_on].x + WidestLine + TempSpacer;

        TempY = current_menu->menu_items[item_on].y + y_shift;
        if (graphical_item == true)
        {
            TempX -=
                (current_menu->menu_items[item_on].image->offset_x * txtscale) *
                2;
            TempY -=
                (current_menu->menu_items[item_on].image->offset_y * txtscale);
        }
        if (graphical_cursor == true)
        {
            TempX -= (cursor->offset_x * txtscale);
            TempY -= (cursor->offset_y * txtscale);
        }

        if (graphical_cursor == true)
        {
            HudStretchImage(TempX, TempY, TempWidth, ShortestLine, cursor, 0.0,
                            0.0, colmap);
        }
        else
            HudWriteText(style, StyleDefinition::kTextSectionText, TempX, TempY,
                         style->definition_->cursor_.cursor_string_.c_str());
    }
    else
    {
        TempX = current_menu->menu_items[item_on].x - TempWidth - TempSpacer;
        TempY = current_menu->menu_items[item_on].y + y_shift;
        if (graphical_item == true)
        {
            TempX -=
                (current_menu->menu_items[item_on].image->offset_x * txtscale) *
                2;
            TempY -=
                (current_menu->menu_items[item_on].image->offset_y * txtscale);
        }
        if (graphical_cursor == true)
        {
            TempX -= (cursor->offset_x * txtscale);
            TempY -= (cursor->offset_y * txtscale);
        }
        if (graphical_cursor == true)
        {
            HudStretchImage(TempX, TempY, TempWidth, ShortestLine, cursor, 0.0,
                            0.0, colmap);
            /*
            char mbuffer[200];
            sprintf(mbuffer, "CX%d MX%d", TempX,
            current_menu->menu_items[item_on].x);
            HudWriteText(style,StyleDefinition::kTextSectionText, 10, 185,
            mbuffer, 0.7f);
            */
        }
        else
            HudWriteText(style, StyleDefinition::kTextSectionText, TempX, TempY,
                         style->definition_->cursor_.cursor_string_.c_str());
    }

    if (graphical_cursor == true)
    {
        cursor->offset_x = old_offset_x;
        cursor->offset_y = old_offset_y;
    }
    HudSetAlpha(old_alpha);
}

//
// Draw our menu items
//
void MenuDrawItems(Style *style, bool graphical_item)
{
    short x, y;
    int   i;
    int   j;
    int   max;

    short TempX = 0;

    ShortestLine = 0.0f;
    TallestLine  = 0.0f;
    WidestLine   = 0.0f;

    x = current_menu->x;
    y = current_menu->y;

    max = current_menu->total_items;

    float old_alpha = HudGetAlpha();

    float txtscale =
        style->definition_->text_[StyleDefinition::kTextSectionText].scale_;

    //---------------------------------------------------
    // 1. For each menu item calculate x, width, height
    //---------------------------------------------------
    if (graphical_item == false)  // We're going text-based menu items
    {
        ShortestLine =
            txtscale *
            style->fonts_[StyleDefinition::kTextSectionText]->NominalHeight();
        TallestLine =
            txtscale *
            style->fonts_[StyleDefinition::kTextSectionText]->NominalHeight();
        for (i = 0; i < max; i++)
        {
            current_menu->menu_items[i].height = ShortestLine;
            if (style->definition_->entry_alignment_ ==
                StyleDefinition::kAlignmentCenter)
                current_menu->menu_items[i].x =
                    CenterMenuText(style, StyleDefinition::kTextSectionText,
                                   current_menu->menu_items[i].name);
            else
                current_menu->menu_items[i].x =
                    x + style->definition_->x_offset_ +
                    style->definition_->text_[StyleDefinition::kTextSectionText]
                        .x_offset_;

            current_menu->menu_items[i].y =
                y + style->definition_->y_offset_ +
                style->definition_->text_[StyleDefinition::kTextSectionText]
                    .y_offset_;
            if (current_menu->menu_items[i].width < 0)
                current_menu->menu_items[i].width =
                    style->fonts_[StyleDefinition::kTextSectionText]
                        ->StringWidth(current_menu->menu_items[i].name) *
                    txtscale;
            if (current_menu->menu_items[i].width > WidestLine)
                WidestLine = current_menu->menu_items[i].width;

            y += current_menu->menu_items[i].height + 1 +
                 style->definition_->entry_spacing_;
        }
    }
    else
    {
        ShortestLine = 10000.0f;
        TallestLine  = 0.0f;
        for (i = 0; i < max; i++)
        {
            if (!current_menu->menu_items[i].patch_name[0]) continue;
            if (!current_menu->menu_items[i].image)
                current_menu->menu_items[i].image =
                    W_ImageLookup(current_menu->menu_items[i].patch_name);

            const image_c *image = current_menu->menu_items[i].image;

            current_menu->menu_items[i].height = IM_HEIGHT(image) * txtscale;
            current_menu->menu_items[i].width  = IM_WIDTH(image) * txtscale;

            if (!image->is_empty)
            {
                if (current_menu->menu_items[i].height < ShortestLine)
                    ShortestLine = current_menu->menu_items[i].height;
                if (current_menu->menu_items[i].height > TallestLine)
                    TallestLine = current_menu->menu_items[i].height;
                if (current_menu->menu_items[i].width > WidestLine)
                    WidestLine = current_menu->menu_items[i].width;

                if (style->definition_->entry_alignment_ ==
                    StyleDefinition::kAlignmentCenter)
                    current_menu->menu_items[i].x = CenterMenuImage2(
                        style, StyleDefinition::kTextSectionText, image);
                else
                    current_menu->menu_items[i].x =
                        x + (image->offset_x * txtscale) +
                        style->definition_->x_offset_ +
                        style->definition_
                            ->text_[StyleDefinition::kTextSectionText]
                            .x_offset_;

                current_menu->menu_items[i].y =
                    y - image->offset_y + style->definition_->y_offset_ +
                    style->definition_->text_[StyleDefinition::kTextSectionText]
                        .y_offset_;
                y += current_menu->menu_items[i].height +
                     style->definition_->entry_spacing_;
            }
            else
            {
                current_menu->menu_items[i].x = x;
                current_menu->menu_items[i].y = y;
                y += 15 + style->definition_->entry_spacing_;
            }
        }
        if (ShortestLine == 10000.0f && TallestLine == 0.0f)
        {
            ShortestLine = 20.0f;
            TallestLine  = 20.0f;
            WidestLine   = 121.0f;
            HudSetAlpha(old_alpha);
            // We have empty menu items so don't draw anything...
            return;
        }
    }

    int textstyle = StyleDefinition::kTextSectionText;
    txtscale      = style->definition_->text_[textstyle].scale_;

    //---------------------------------------------------
    // 2. Draw each menu item
    //---------------------------------------------------
    for (j = 0; j < max; j++)
    {
        // int textstyle = i == item_on ?
        // (style->definition_->text[StyleDefinition::kTextSectionSelected].font
        // ? StyleDefinition::kTextSectionSelected :
        // StyleDefinition::kTextSectionText) :
        // StyleDefinition::kTextSectionText;

        textstyle = StyleDefinition::kTextSectionText;
        txtscale  = style->definition_->text_[textstyle].scale_;
        if (j == item_on)
        {
            if (style->definition_->text_[StyleDefinition::kTextSectionSelected]
                    .font_)
            {
                textstyle = StyleDefinition::kTextSectionSelected;
                txtscale  = style->definition_->text_[textstyle].scale_;
            }
        }

        HudSetAlpha(style->definition_->text_[textstyle].translucency_);

        if (style->definition_->entry_alignment_ ==
            StyleDefinition::kAlignmentRight)
            TempX = current_menu->menu_items[j].x + WidestLine -
                    current_menu->menu_items[j].width;
        else
            TempX = current_menu->menu_items[j].x;

        if (graphical_item == false)  // We're going text-based menu items
        {
            HudWriteText(style, textstyle, TempX, current_menu->menu_items[j].y,
                         current_menu->menu_items[j].name);
        }
        else  // We're going graphical menu items
        {
            // const colourmap_c *colmap = i == item_on ?
            // style->definition_->text[StyleDefinition::kTextSectionSelected].colmap
            // :
            //		style->definition_->text[StyleDefinition::kTextSectionText].colmap;

            textstyle = StyleDefinition::kTextSectionText;
            txtscale  = style->definition_->text_[textstyle].scale_;
            if (j == item_on)
            {
                if (style->definition_
                        ->text_[StyleDefinition::kTextSectionSelected]
                        .colmap_)
                {
                    textstyle = StyleDefinition::kTextSectionSelected;
                    txtscale  = style->definition_->text_[textstyle].scale_;
                }
            }

            const Colormap *colmap =
                style->definition_->text_[textstyle].colmap_;
            // colourmap_c *colmap = nullptr;

            // HudStretchImage() will apply image.offset_x again so subtract it
            // first
            TempX -= (current_menu->menu_items[j].image->offset_x * txtscale);
            HudStretchImage(TempX, current_menu->menu_items[j].y,
                            current_menu->menu_items[j].width,
                            current_menu->menu_items[j].height,
                            current_menu->menu_items[j].image, 0.0, 0.0,
                            colmap);
        }
        HudSetAlpha(old_alpha);
    }
    HudSetAlpha(old_alpha);
}

//
// Called after the view has been rendered,
// but before it has been blitted.
//
void MenuDrawer(void)
{
    if (!menu_active) return;

    if (menu_backdrop && (option_menu_on || network_game_menu_on ||
                          (current_menu->draw_function == MenuDrawLoad ||
                           current_menu->draw_function == MenuDrawSave)))
    {
        if (r_titlescaling.d_)  // Fill Border
        {
            if (!menu_backdrop->blurred_version)
            {
                W_ImageStoreBlurred(menu_backdrop, 0.75f);
                menu_backdrop->blurred_version->grayscale = true;
            }
            HudStretchImage(-320, -200, 960, 600,
                            menu_backdrop->blurred_version, 0, 0);
        }
        else
            HudSolidBox(-320, -200, 960, 600, 0);
        HudDrawImageTitleWS(menu_backdrop);
    }

    // Horiz. & Vertically center string and print it.
    if (message_mode)
    {
        DrawMessage();
        return;
    }

    // new MainMenuOptions menu enable, use that drawer instead
    if (option_menu_on)
    {
        OptionMenuDrawer();
        return;
    }

    if (network_game_menu_on)
    {
        NetworkGameDrawer();
        return;
    }

    // Lobo 2022: Check if we're going to use text-based menus
    // or the users (custom)graphics
    bool custom_menu = false;
    if ((current_menu->draw_function == MenuDrawMainMenu) &&
        (custom_MenuMain == true))
        custom_menu = true;

    if ((current_menu->draw_function == MenuDrawNewGame) &&
        (custom_MenuDifficulty == true))
        custom_menu = true;

    if (current_menu->draw_function == MenuDrawEpisode &&
        custom_MenuEpisode == true)
        custom_menu = true;

    Style *style = current_menu->style_variable[0];
    SYS_ASSERT(style);

    style->DrawBackground();

    // call Draw routine
    if (current_menu->draw_function) (*current_menu->draw_function)();

    // custom_menu==false //We're going text-based menu items
    // custom_menu==true //We're going graphic-based menu items
    MenuDrawItems(style, custom_menu);

    if (!(current_menu->draw_function == MenuDrawLoad ||
          current_menu->draw_function == MenuDrawSave))
    {
        // custom_menu==false //We're going text-based menu items
        // custom_menu==true //We're going graphic-based menu items
        MenuDrawCursor(style, custom_menu);
    }
}

void MenuClear(void)
{
    // -AJA- 2007/12/24: save user changes ASAP (in case of crash)
    if (menu_active) { ConfigurationSaveDefaults(); }

    menu_active           = false;
    save_screenshot_valid = false;
    option_menu_on         = 0;
}

void MenuSetupNextMenu(Menu *menudef)
{
    current_menu = menudef;
    item_on      = current_menu->last_on;
}

void MenuTicker(void)
{
    // update language if it changed
    if (m_language.CheckModified())
        if (!language.Select(m_language.c_str()))
            LogPrint("Unknown language: %s\n", m_language.c_str());

    if (option_menu_on)
    {
        OptionMenuTicker();
        return;
    }

    if (network_game_menu_on)
    {
        NetworkGameTicker();
        return;
    }
}

void MenuInitialize(void)
{
    E_ProgressMessage(language["MiscInfo"]);

    current_menu = &MainMenuDefinition;
    menu_active  = false;
    item_on      = current_menu->last_on;
    message_mode = 0;
    message_string.clear();
    message_last_menu = menu_active;
    quicksave_slot    = -1;

    // lookup styles
    StyleDefinition *def;

    def = styledefs.Lookup("MENU");
    if (!def) def = default_style;
    menu_default_style = hud_styles.Lookup(def);

    def             = styledefs.Lookup("MAIN MENU");
    main_menu_style = def ? hud_styles.Lookup(def) : menu_default_style;

    def           = styledefs.Lookup("CHOOSE EPISODE");
    episode_style = def ? hud_styles.Lookup(def) : menu_default_style;

    def         = styledefs.Lookup("CHOOSE SKILL");
    skill_style = def ? hud_styles.Lookup(def) : menu_default_style;

    def        = styledefs.Lookup("LOAD SAVE MENU");
    load_style = def ? hud_styles.Lookup(def) : menu_default_style;

    def        = styledefs.Lookup("LOAD SAVE MENU");
    save_style = def ? hud_styles.Lookup(def) : menu_default_style;

    def        = styledefs.Lookup("EXIT_SCREEN");
    exit_style = def ? hud_styles.Lookup(def) : menu_default_style;

    def = styledefs.Lookup("OPTIONS");
    if (!def) def = default_style;

    language.Select(m_language.c_str());

    // Lobo 2022: load our ddflang stuff
    MainMenu[kMainMenuNewGame].name  = language["MainNewGame"];
    MainMenu[kMainMenuOptions].name  = language["MainOptions"];
    MainMenu[kMainMenuLoadGame].name = language["MainLoadGame"];
    MainMenu[kMainMenuSaveGame].name = language["MainSaveGame"];
    MainMenu[kMainMenuReadThis].name = language["MainReadThis"];
    MainMenu[kMainMenuQuitDoom].name = language["MainQuitGame"];

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

    menu_load_game    = W_ImageLookup("M_LOADG");
    menu_save_game    = W_ImageLookup("M_SAVEG");
    menu_sound_volume = W_ImageLookup("M_SVOL");
    menu_new_game     = W_ImageLookup("M_NEWG");
    menu_skill        = W_ImageLookup("M_SKILL");
    menu_episode      = W_ImageLookup("M_EPISOD");
    menu_skull[0]     = (image_c *)W_ImageLookup("M_SKULL1");
    menu_skull[1]     = (image_c *)W_ImageLookup("M_SKULL2");

    // Check for custom menu graphics in pwads:
    // If we have them then use them instead of our
    //  text-based ones.
    if (W_IsLumpInPwad("M_NEWG")) custom_MenuMain = true;

    if (W_IsLumpInPwad("M_LOADG")) custom_MenuMain = true;

    if (W_IsLumpInPwad("M_SAVEG")) custom_MenuMain = true;

    if (W_IsLumpInPwad("M_EPISOD")) custom_MenuEpisode = true;

    if (W_IsLumpInPwad("M_EPI1")) custom_MenuEpisode = true;

    if (W_IsLumpInPwad("M_EPI2")) custom_MenuEpisode = true;

    if (W_IsLumpInPwad("M_EPI3")) custom_MenuEpisode = true;

    if (W_IsLumpInPwad("M_EPI4")) custom_MenuEpisode = true;

    if (W_IsLumpInPwad("M_JKILL")) custom_MenuDifficulty = true;

    if (W_IsLumpInPwad("M_NMARE")) custom_MenuDifficulty = true;

    LogDebug("custom_MenuMain =%d \n", custom_MenuMain);
    LogDebug("custom_MenuEpisode =%d \n", custom_MenuEpisode);
    LogDebug("custom_MenuDifficulty =%d \n", custom_MenuDifficulty);

    menu_doom = W_ImageLookup("M_DOOM");

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.
    //    if (W_CheckNumForName("M_EPI4") < 0)
    //      EpisodeMenuDefinition.total_items -= 2;
    //    else if (W_CheckNumForName("M_EPI5") < 0)
    //      EpisodeMenuDefinition.total_items--;

    if (W_IsLumpInAnyWad("HELP"))  // doom2
    {
        menu_read_this[0] = W_ImageLookup("HELP");
        menu_read_this[1] = W_ImageLookup(
            "CREDIT");  // Unnecessary since we won't see it anyway...
        MainMenu[kMainMenuReadThis] = MainMenu[kMainMenuQuitDoom];
        MainMenuDefinition.total_items--;
        MainMenuDefinition.y += 8;  // FIXME
        SkillMenuDefinition.previous_menu     = &MainMenuDefinition;
        ReadThisMenuDefinition1.draw_function = MenuDrawReadThis1;
        ReadThisMenuDefinition1.x             = 330;
        ReadThisMenuDefinition1.y             = 165;
        ReadMenu1[0].select_function          = MenuFinishReadThis;
    }
    else  // doom or shareware doom
    {
        menu_read_this[0] = W_ImageLookup("HELP1");
        if (W_IsLumpInAnyWad("HELP2"))
            menu_read_this[1] = W_ImageLookup("HELP2");  // Shareware doom
        else
            menu_read_this[1] = W_ImageLookup("CREDIT");  // Full doom
    }

    // Lobo 2022: Use new sfx definitions so we don't have to share names with
    // normal doom sfx.
    sound_effect_swtchn = sfxdefs.GetEffect("MENU_IN");   // Enter Menu
    sound_effect_tink   = sfxdefs.GetEffect("TINK");      // unused
    sound_effect_radio  = sfxdefs.GetEffect("RADIO");     // unused
    sound_effect_oof    = sfxdefs.GetEffect("MENU_INV");  // invalid choice
    sound_effect_pstop =
        sfxdefs.GetEffect("MENU_MOV");  // moving cursor in a menu
    sound_effect_stnmov = sfxdefs.GetEffect("MENU_SLD");  // slider move
    sound_effect_pistol = sfxdefs.GetEffect("MENU_SEL");  // select in menu
    sound_effect_swtchx = sfxdefs.GetEffect("MENU_OUT");  // cancel/exit menu

    OptionMenuInitialize();
    NetworkGameInitialize();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
