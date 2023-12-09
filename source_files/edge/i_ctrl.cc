//----------------------------------------------------------------------------
//  EDGE SDL Controller Stuff
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2023  The EDGE Team.
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

#include "i_defs.h"
#include "i_video.h" // i_sdlinc.h is also covered here - Dasho

#include "dm_defs.h"
#include "dm_state.h"
#include "e_event.h"
#include "e_input.h"
#include "e_main.h"
#include "m_argv.h"
#include "r_modes.h"

#include "str_util.h"
#include "edge_profiling.h"

#undef DEBUG_KB

// FIXME: Combine all these SDL bool vars into an int/enum'd flags structure

extern cvar_c r_doubleframes;

// Work around for alt-tabbing
bool alt_is_down;
bool eat_mouse_motion = true;

DEF_CVAR(in_keypad, "1", CVAR_ARCHIVE)

bool nojoy; // what a wowser, joysticks completely disabled

int joystick_device; // choice in menu, 0 for none

static int          num_joys;
static int          cur_joy; // 0 for none
static bool         need_mouse_recapture = false;
SDL_Joystick       *joy_info             = nullptr;
SDL_GameController *gamepad_info         = nullptr;
SDL_JoystickID      gamepad_inst         = -1;

// Track trigger state to avoid pushing multiple unnecessary trigger events
bool right_trigger_pulled = false;
bool left_trigger_pulled  = false;

//
// Translates a key from SDL -> EDGE
// Returns -1 if no suitable translation exists.
//
int TranslateSDLKey(SDL_Scancode key)
{
    // if keypad is not wanted, convert to normal keys
    if (!in_keypad.d)
    {
        if (SDL_SCANCODE_KP_1 <= key && key < SDL_SCANCODE_KP_0)
            return '0' + (key - 88);

        if (key == SDL_SCANCODE_KP_0)
            return '0';

        switch (key)
        {
        case SDL_SCANCODE_KP_PLUS:
            return '+';
        case SDL_SCANCODE_KP_MINUS:
            return '-';
        case SDL_SCANCODE_KP_PERIOD:
            return '.';
        case SDL_SCANCODE_KP_MULTIPLY:
            return '*';
        case SDL_SCANCODE_KP_DIVIDE:
            return '/';
        case SDL_SCANCODE_KP_EQUALS:
            return '=';
        case SDL_SCANCODE_KP_ENTER:
            return KEYD_ENTER;

        default:
            break;
        }
    }

    switch (key)
    {
    case SDL_SCANCODE_GRAVE:
        return KEYD_TILDE;
    case SDL_SCANCODE_MINUS:
        return KEYD_MINUS;
    case SDL_SCANCODE_EQUALS:
        return KEYD_EQUALS;

    case SDL_SCANCODE_TAB:
        return KEYD_TAB;
    case SDL_SCANCODE_RETURN:
        return KEYD_ENTER;
    case SDL_SCANCODE_ESCAPE:
        return KEYD_ESCAPE;
    case SDL_SCANCODE_BACKSPACE:
        return KEYD_BACKSPACE;

    case SDL_SCANCODE_UP:
        return KEYD_UPARROW;
    case SDL_SCANCODE_DOWN:
        return KEYD_DOWNARROW;
    case SDL_SCANCODE_LEFT:
        return KEYD_LEFTARROW;
    case SDL_SCANCODE_RIGHT:
        return KEYD_RIGHTARROW;

    case SDL_SCANCODE_HOME:
        return KEYD_HOME;
    case SDL_SCANCODE_END:
        return KEYD_END;
    case SDL_SCANCODE_INSERT:
        return KEYD_INSERT;
    case SDL_SCANCODE_DELETE:
        return KEYD_DELETE;
    case SDL_SCANCODE_PAGEUP:
        return KEYD_PGUP;
    case SDL_SCANCODE_PAGEDOWN:
        return KEYD_PGDN;

    case SDL_SCANCODE_F1:
        return KEYD_F1;
    case SDL_SCANCODE_F2:
        return KEYD_F2;
    case SDL_SCANCODE_F3:
        return KEYD_F3;
    case SDL_SCANCODE_F4:
        return KEYD_F4;
    case SDL_SCANCODE_F5:
        return KEYD_F5;
    case SDL_SCANCODE_F6:
        return KEYD_F6;
    case SDL_SCANCODE_F7:
        return KEYD_F7;
    case SDL_SCANCODE_F8:
        return KEYD_F8;
    case SDL_SCANCODE_F9:
        return KEYD_F9;
    case SDL_SCANCODE_F10:
        return KEYD_F10;
    case SDL_SCANCODE_F11:
        return KEYD_F11;
    case SDL_SCANCODE_F12:
        return KEYD_F12;

    case SDL_SCANCODE_KP_0:
        return KEYD_KP0;
    case SDL_SCANCODE_KP_1:
        return KEYD_KP1;
    case SDL_SCANCODE_KP_2:
        return KEYD_KP2;
    case SDL_SCANCODE_KP_3:
        return KEYD_KP3;
    case SDL_SCANCODE_KP_4:
        return KEYD_KP4;
    case SDL_SCANCODE_KP_5:
        return KEYD_KP5;
    case SDL_SCANCODE_KP_6:
        return KEYD_KP6;
    case SDL_SCANCODE_KP_7:
        return KEYD_KP7;
    case SDL_SCANCODE_KP_8:
        return KEYD_KP8;
    case SDL_SCANCODE_KP_9:
        return KEYD_KP9;

    case SDL_SCANCODE_KP_PERIOD:
        return KEYD_KP_DOT;
    case SDL_SCANCODE_KP_PLUS:
        return KEYD_KP_PLUS;
    case SDL_SCANCODE_KP_MINUS:
        return KEYD_KP_MINUS;
    case SDL_SCANCODE_KP_MULTIPLY:
        return KEYD_KP_STAR;
    case SDL_SCANCODE_KP_DIVIDE:
        return KEYD_KP_SLASH;
    case SDL_SCANCODE_KP_EQUALS:
        return KEYD_KP_EQUAL;
    case SDL_SCANCODE_KP_ENTER:
        return KEYD_KP_ENTER;

    case SDL_SCANCODE_PRINTSCREEN:
        return KEYD_PRTSCR;
    case SDL_SCANCODE_CAPSLOCK:
        return KEYD_CAPSLOCK;
    case SDL_SCANCODE_NUMLOCKCLEAR:
        return KEYD_NUMLOCK;
    case SDL_SCANCODE_SCROLLLOCK:
        return KEYD_SCRLOCK;
    case SDL_SCANCODE_PAUSE:
        return KEYD_PAUSE;

    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
        return KEYD_RSHIFT;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
        return KEYD_RCTRL;
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_LALT:
        return KEYD_LALT;
    case SDL_SCANCODE_RGUI:
    case SDL_SCANCODE_RALT:
        return KEYD_RALT;

    default:
        break;
    }

    if (key <= 0x7f)
        return tolower(SDL_GetKeyFromScancode(key));

    return -1;
}

void HandleFocusGain(void)
{
    // Hide cursor and grab input
    I_GrabCursor(true);

    // Ignore any pending mouse motion
    eat_mouse_motion = true;

    // Now active again
    app_state |= APP_STATE_ACTIVE;
}

void HandleFocusLost(void)
{
    I_GrabCursor(false);

    E_Idle();

    // No longer active
    app_state &= ~APP_STATE_ACTIVE;
}

void HandleKeyEvent(SDL_Event *ev)
{
    if (ev->type != SDL_KEYDOWN && ev->type != SDL_KEYUP)
        return;

#ifdef DEBUG_KB
    if (ev->type == SDL_KEYDOWN)
        L_WriteDebug("  HandleKey: DOWN\n");
    else if (ev->type == SDL_KEYUP)
        L_WriteDebug("  HandleKey: UP\n");
#endif

    SDL_Scancode sym = ev->key.keysym.scancode;

    event_t event;
    event.value.key.sym = TranslateSDLKey(sym);

    // handle certain keys which don't behave normally
    if (sym == SDL_SCANCODE_CAPSLOCK || sym == SDL_SCANCODE_NUMLOCKCLEAR)
    {
#ifdef DEBUG_KB
        L_WriteDebug("   HandleKey: CAPS or NUMLOCK\n");
#endif
        if (ev->type != SDL_KEYDOWN)
            return;
        event.type = ev_keydown;
        E_PostEvent(&event);

        event.type = ev_keyup;
        E_PostEvent(&event);
        return;
    }

    event.type = (ev->type == SDL_KEYDOWN) ? ev_keydown : ev_keyup;

#ifdef DEBUG_KB
    L_WriteDebug("   HandleKey: sym=%d scan=%d --> key=%d\n", sym, ev->key.keysym.scancode, event.value.key);
#endif

    if (event.value.key.sym < 0)
    {
        // No translation possible for SDL symbol and no unicode value
        return;
    }

    if (event.value.key.sym == KEYD_TAB && alt_is_down)
    {
#ifdef DEBUG_KB
        L_WriteDebug("   HandleKey: ALT-TAB\n");
#endif
        alt_is_down = false;
        return;
    }

#ifndef EDGE_WEB // Not sure if this is desired on the web build
    if (event.value.key.sym == KEYD_ENTER && alt_is_down)
    {
#ifdef DEBUG_KB
        L_WriteDebug("   HandleKey: ALT-ENTER\n");
#endif
        alt_is_down = false;
        R_ToggleFullscreen();
        if (DISPLAYMODE == scrmode_c::SCR_WINDOW)
        {
            I_GrabCursor(false);
            need_mouse_recapture = true;
        }
        return;
    }
#endif

    if (event.value.key.sym == KEYD_LALT)
        alt_is_down = (event.type == ev_keydown);

    E_PostEvent(&event);
}

void HandleMouseButtonEvent(SDL_Event *ev)
{
    event_t event;

    if (ev->type == SDL_MOUSEBUTTONDOWN)
        event.type = ev_keydown;
    else if (ev->type == SDL_MOUSEBUTTONUP)
        event.type = ev_keyup;
    else
        return;

    switch (ev->button.button)
    {
    case 1:
        event.value.key.sym = KEYD_MOUSE1;
        break;
    case 2:
        event.value.key.sym = KEYD_MOUSE2;
        break;
    case 3:
        event.value.key.sym = KEYD_MOUSE3;
        break;
    case 4:
        event.value.key.sym = KEYD_MOUSE4;
        break;
    case 5:
        event.value.key.sym = KEYD_MOUSE5;
        break;
    case 6:
        event.value.key.sym = KEYD_MOUSE6;
        break;

    default:
        return;
    }

    E_PostEvent(&event);
}

void HandleMouseWheelEvent(SDL_Event *ev)
{
    event_t event;
    event_t release;

    event.type   = ev_keydown;
    release.type = ev_keyup;

    if (ev->wheel.y > 0)
    {
        event.value.key.sym   = KEYD_WHEEL_UP;
        release.value.key.sym = KEYD_WHEEL_UP;
    }
    else if (ev->wheel.y < 0)
    {
        event.value.key.sym   = KEYD_WHEEL_DN;
        release.value.key.sym = KEYD_WHEEL_DN;
    }
    else
    {
        return;
    }
    E_PostEvent(&event);
    E_PostEvent(&release);
}

static void HandleGamepadButtonEvent(SDL_Event *ev)
{
    // ignore other gamepads;
    if (ev->cbutton.which != gamepad_inst)
        return;

    event_t event;

    if (ev->type == SDL_CONTROLLERBUTTONDOWN)
        event.type = ev_keydown;
    else if (ev->type == SDL_CONTROLLERBUTTONUP)
        event.type = ev_keyup;
    else
        return;

    if (ev->cbutton.button >= SDL_CONTROLLER_BUTTON_MAX) // How would this happen? - Dasho
        return;

    event.value.key.sym = KEYD_GP_A + ev->cbutton.button;

    E_PostEvent(&event);
}

static void HandleGamepadTriggerEvent(SDL_Event *ev)
{
    // ignore other gamepads
    if (ev->caxis.which != gamepad_inst)
        return;

    Uint8 current_axis = ev->caxis.axis;

    // ignore non-trigger axes
    if (current_axis != SDL_CONTROLLER_AXIS_TRIGGERLEFT && current_axis != SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        return;

    event_t event;

    int thresh = I_ROUND(*joy_deads[current_axis] * 32767.0f);
    int input  = ev->caxis.value;

    if (current_axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
    {
        event.value.key.sym = KEYD_TRIGGER_LEFT;
        if (input < thresh)
        {
            if (!left_trigger_pulled)
                return;
            event.type          = ev_keyup;
            left_trigger_pulled = false;
        }
        else
        {
            if (left_trigger_pulled)
                return;
            event.type          = ev_keydown;
            left_trigger_pulled = true;
        }
    }
    else
    {
        event.value.key.sym = KEYD_TRIGGER_RIGHT;
        if (input < thresh)
        {
            if (!right_trigger_pulled)
                return;
            event.type           = ev_keyup;
            right_trigger_pulled = false;
        }
        else
        {
            if (right_trigger_pulled)
                return;
            event.type           = ev_keydown;
            right_trigger_pulled = true;
        }
    }

    E_PostEvent(&event);
}

void HandleMouseMotionEvent(SDL_Event *ev)
{
    int dx, dy;

    dx = ev->motion.xrel;
    dy = ev->motion.yrel;

    if (dx || dy)
    {
        event_t event;

        event.type           = ev_mouse;
        event.value.mouse.dx = dx;
        event.value.mouse.dy = -dy; // -AJA- positive should be "up"

        E_PostEvent(&event);
    }
}

int I_JoyGetAxis(int n) // n begins at 0
{
    if (nojoy || !joy_info || !gamepad_info)
        return 0;

    return SDL_GameControllerGetAxis(gamepad_info, static_cast<SDL_GameControllerAxis>(n));
}

static void I_OpenJoystick(int index)
{
    SYS_ASSERT(1 <= index && index <= num_joys);

    joy_info = SDL_JoystickOpen(index - 1);
    if (!joy_info)
    {
        I_Printf("Unable to open joystick %d (SDL error)\n", index);
        return;
    }

    cur_joy = index;

    gamepad_info = SDL_GameControllerOpen(cur_joy - 1);

    if (!gamepad_info)
    {
        I_Printf("Unable to open joystick %s as a gamepad!\n", SDL_JoystickName(joy_info));
        SDL_JoystickClose(joy_info);
        joy_info = nullptr;
        return;
    }

    gamepad_inst = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad_info));

    const char *name = SDL_GameControllerName(gamepad_info);
    if (!name)
        name = "(UNKNOWN)";

    int gp_num_joysticks = 0;
    int gp_num_triggers  = 0;
    int gp_num_buttons   = 0;

    if (SDL_GameControllerHasAxis(gamepad_info, SDL_CONTROLLER_AXIS_LEFTX) &&
        SDL_GameControllerHasAxis(gamepad_info, SDL_CONTROLLER_AXIS_LEFTY))
        gp_num_joysticks++;
    if (SDL_GameControllerHasAxis(gamepad_info, SDL_CONTROLLER_AXIS_RIGHTX) &&
        SDL_GameControllerHasAxis(gamepad_info, SDL_CONTROLLER_AXIS_RIGHTY))
        gp_num_joysticks++;
    if (SDL_GameControllerHasAxis(gamepad_info, SDL_CONTROLLER_AXIS_TRIGGERLEFT))
        gp_num_triggers++;
    if (SDL_GameControllerHasAxis(gamepad_info, SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
        gp_num_triggers++;
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
    {
        if (SDL_GameControllerHasButton(gamepad_info, static_cast<SDL_GameControllerButton>(i)))
            gp_num_buttons++;
    }

    I_Printf("Opened gamepad %d : %s\n", cur_joy, name);
    I_Printf("Sticks:%d Triggers: %d Buttons: %d Touchpads: %d\n", gp_num_joysticks, gp_num_triggers, gp_num_buttons,
             SDL_GameControllerGetNumTouchpads(gamepad_info));
    I_Printf("Rumble:%s Trigger Rumble: %s LED: %s\n", SDL_GameControllerHasRumble(gamepad_info) ? "Yes" : "No",
             SDL_GameControllerHasRumbleTriggers(gamepad_info) ? "Yes" : "No",
             SDL_GameControllerHasLED(gamepad_info) ? "Yes" : "No");
}

static void CheckJoystickChanged(void)
{
    int new_num_joys = SDL_NumJoysticks();

    if (new_num_joys == num_joys && cur_joy == joystick_device)
        return;

    if (new_num_joys == 0)
    {
        if (gamepad_info)
        {
            SDL_GameControllerClose(gamepad_info);
            gamepad_info = nullptr;
        }
        if (joy_info)
        {
            SDL_JoystickClose(joy_info);
            joy_info = nullptr;
        }
        num_joys        = 0;
        joystick_device = 0;
        cur_joy         = 0;
        gamepad_inst    = -1;
        return;
    }

    int new_joy = joystick_device;

    if (joystick_device < 0 || joystick_device > new_num_joys)
    {
        joystick_device = 0;
        new_joy         = 0;
    }

    if (new_joy == cur_joy && cur_joy > 0)
        return;

    if (joy_info)
    {
        if (gamepad_info)
        {
            SDL_GameControllerClose(gamepad_info);
            gamepad_info = nullptr;
        }

        SDL_JoystickClose(joy_info);
        joy_info = nullptr;

        I_Printf("Closed joystick %d\n", cur_joy);
        cur_joy = 0;

        gamepad_inst = -1;
    }

    if (new_joy > 0)
    {
        num_joys        = new_num_joys;
        joystick_device = new_joy;
        I_OpenJoystick(new_joy);
    }
    else if (num_joys == 0 && new_num_joys > 0)
    {
        num_joys        = new_num_joys;
        joystick_device = new_joy = 1;
        I_OpenJoystick(new_joy);
    }
    else
        num_joys = new_num_joys;
}

//
// Event handling while the application is active
//
void ActiveEventProcess(SDL_Event *sdl_ev)
{
    switch (sdl_ev->type)
    {
    case SDL_WINDOWEVENT: {
        if (sdl_ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        {
            HandleFocusLost();
        }
#ifdef EDGE_WEB
        if (sdl_ev->window.event == SDL_WINDOWEVENT_RESIZED)
        {
            printf("SDL window resize event %i %i\n", sdl_ev->window.data1, sdl_ev->window.data2);
            SCREENWIDTH  = sdl_ev->window.data1;
            SCREENHEIGHT = sdl_ev->window.data2;
            SCREENBITS   = 24;
            DISPLAYMODE  = 0;
            I_DeterminePixelAspect();
        }
#endif

        break;
    }

    case SDL_KEYDOWN:
    case SDL_KEYUP:
        HandleKeyEvent(sdl_ev);
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
#ifdef EDGE_WEB
        // On web, we don't want clicks coming through when changing pointer lock
        // Otherwise, menus will be selected, weapons fired, unexpectedly
        if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE)
            HandleMouseButtonEvent(sdl_ev);
#else
        if (need_mouse_recapture)
        {
            I_GrabCursor(true);
            need_mouse_recapture = false;
            break;
        }
        HandleMouseButtonEvent(sdl_ev);
#endif
        break;

    case SDL_MOUSEWHEEL:
        if (!need_mouse_recapture)
            HandleMouseWheelEvent(sdl_ev);
        break;

    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        HandleGamepadButtonEvent(sdl_ev);
        break;

    // Analog triggers should be the only thing handled here -Dasho
    case SDL_CONTROLLERAXISMOTION:
        HandleGamepadTriggerEvent(sdl_ev);
        break;

    case SDL_MOUSEMOTION:
        if (eat_mouse_motion)
        {
            eat_mouse_motion = false; // One motion needs to be discarded
            break;
        }
        if (!need_mouse_recapture)
            HandleMouseMotionEvent(sdl_ev);
        break;

    case SDL_QUIT:
        // Note we deliberate clear all other flags here. Its our method of
        // ensuring nothing more is done with events.
        app_state = APP_STATE_PENDING_QUIT;
        break;

    case SDL_CONTROLLERDEVICEADDED:
    case SDL_CONTROLLERDEVICEREMOVED:
        CheckJoystickChanged();
        break;

    default:
        break; // Don't care
    }
}

//
// Event handling while the application is not active
//
void InactiveEventProcess(SDL_Event *sdl_ev)
{
    switch (sdl_ev->type)
    {
    case SDL_WINDOWEVENT:
        if (app_state & APP_STATE_PENDING_QUIT)
            break; // Don't care: we're going to exit

        if (sdl_ev->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
            HandleFocusGain();
        break;

    case SDL_QUIT:
        // Note we deliberate clear all other flags here. Its our method of
        // ensuring nothing more is done with events.
        app_state = APP_STATE_PENDING_QUIT;
        break;

    case SDL_CONTROLLERDEVICEADDED:
    case SDL_CONTROLLERDEVICEREMOVED:
        CheckJoystickChanged();
        break;

    default:
        break; // Don't care
    }
}

void I_ShowGamepads(void)
{
    if (nojoy)
    {
        I_Printf("Gamepad system is disabled.\n");
        return;
    }

    if (num_joys == 0)
    {
        I_Printf("No gamepads found.\n");
        return;
    }

    I_Printf("Gamepads:\n");

    for (int i = 0; i < num_joys; i++)
    {
        const char *name = SDL_GameControllerNameForIndex(i);
        if (!name)
            name = "(UNKNOWN)";

        I_Printf("  %2d : %s\n", i + 1, name);
    }
}

void I_StartupJoystick(void)
{
    cur_joy         = 0;
    joystick_device = 0;

    if (argv::Find("nojoy") > 0)
    {
        I_Printf("I_StartupControl: Gamepad system disabled.\n");
        nojoy = true;
        return;
    }

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
    {
        I_Printf("I_StartupControl: Couldn't init SDL GAMEPAD!\n");
        nojoy = true;
        return;
    }

    SDL_GameControllerEventState(SDL_ENABLE);

    num_joys = SDL_NumJoysticks();

    I_Printf("I_StartupControl: %d gamepads found.\n", num_joys);

    if (num_joys == 0)
        return;
    else
    {
        joystick_device = 1; // Automatically set to first detected gamepad
        I_OpenJoystick(joystick_device);
    }
}

/****** Input Event Generation ******/

void I_StartupControl(void)
{
    alt_is_down = false;

    I_StartupJoystick();
}

void I_ControlGetEvents(void)
{
    EDGE_ZoneScoped;

    SDL_Event sdl_ev;

    while (SDL_PollEvent(&sdl_ev))
    {
#ifdef DEBUG_KB
        L_WriteDebug("#I_ControlGetEvents: type=%d\n", sdl_ev.type);
#endif
        if (app_state & APP_STATE_ACTIVE)
            ActiveEventProcess(&sdl_ev);
        else
            InactiveEventProcess(&sdl_ev);
    }
}

void I_ShutdownControl(void)
{
    if (gamepad_info)
    {
        SDL_GameControllerClose(gamepad_info);
        gamepad_info = nullptr;
    }
    if (joy_info)
    {
        SDL_JoystickClose(joy_info);
        joy_info = nullptr;
    }
}

int I_GetTime(void)
{
    Uint32 t = SDL_GetTicks();

    int factor = (r_doubleframes.d ? 70 : 35);

    // more complex than "t*70/1000" to give more accuracy
    return (t / 1000) * factor + (t % 1000) * factor / 1000;
}

int I_GetMillies(void)
{
    return SDL_GetTicks();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
