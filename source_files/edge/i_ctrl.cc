//----------------------------------------------------------------------------
//  EDGE SDL Controller Stuff
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

#include "i_defs.h"
#include "i_sdlinc.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "e_event.h"
#include "e_input.h"
#include "e_main.h"
#include "m_argv.h"
#include "r_modes.h"


#undef DEBUG_KB


// FIXME: Combine all these SDL bool vars into an int/enum'd flags structure

// Work around for alt-tabbing
bool alt_is_down;
bool eat_mouse_motion = true;

cvar_c in_keypad;
cvar_c in_warpmouse;


bool nojoy;  // what a wowser, joysticks completely disabled

int joystick_device;  // choice in menu, 0 for none

static int num_joys;
static int cur_joy;  // 0 for none
static SDL_Joystick *joy_info;

static int joy_num_axes;
static int joy_num_buttons;
static int joy_num_hats;
static int joy_num_balls;

//
// Translates a key from SDL -> EDGE
// Returns -1 if no suitable translation exists.
//
int TranslateSDLKey(int key)
{
	// if keypad is not wanted, convert to normal keys
	if (! in_keypad.d)
	{
		if (SDLK_KP0 <= key && key <= SDLK_KP9)
			return '0' + (key - SDLK_KP0);

		switch (key)
		{
			case SDLK_KP_PLUS:     return '+';
			case SDLK_KP_MINUS:    return '-';
			case SDLK_KP_PERIOD:   return '.';
			case SDLK_KP_MULTIPLY: return '*';
			case SDLK_KP_DIVIDE:   return '/';
			case SDLK_KP_EQUALS:   return '=';
			case SDLK_KP_ENTER:    return KEYD_ENTER;

			default: break;
		}
	}

	switch (key) 
	{
		case SDLK_TAB: return KEYD_TAB;
		case SDLK_RETURN: return KEYD_ENTER;
		case SDLK_ESCAPE: return KEYD_ESCAPE;
		case SDLK_BACKSPACE: return KEYD_BACKSPACE;

		case SDLK_UP:    return KEYD_UPARROW;
		case SDLK_DOWN:  return KEYD_DOWNARROW;
		case SDLK_LEFT:  return KEYD_LEFTARROW;
		case SDLK_RIGHT: return KEYD_RIGHTARROW;

		case SDLK_HOME:   return KEYD_HOME;
		case SDLK_END:    return KEYD_END;
		case SDLK_INSERT: return KEYD_INSERT;
		case SDLK_DELETE: return KEYD_DELETE;
		case SDLK_PAGEUP: return KEYD_PGUP;
		case SDLK_PAGEDOWN: return KEYD_PGDN;

		case SDLK_F1:  return KEYD_F1;
		case SDLK_F2:  return KEYD_F2;
		case SDLK_F3:  return KEYD_F3;
		case SDLK_F4:  return KEYD_F4;
		case SDLK_F5:  return KEYD_F5;
		case SDLK_F6:  return KEYD_F6;
		case SDLK_F7:  return KEYD_F7;
		case SDLK_F8:  return KEYD_F8;
		case SDLK_F9:  return KEYD_F9;
		case SDLK_F10: return KEYD_F10;
		case SDLK_F11: return KEYD_F11;
		case SDLK_F12: return KEYD_F12;

		case SDLK_KP0: return KEYD_KP0;
		case SDLK_KP1: return KEYD_KP1;
		case SDLK_KP2: return KEYD_KP2;
		case SDLK_KP3: return KEYD_KP3;
		case SDLK_KP4: return KEYD_KP4;
		case SDLK_KP5: return KEYD_KP5;
		case SDLK_KP6: return KEYD_KP6;
		case SDLK_KP7: return KEYD_KP7;
		case SDLK_KP8: return KEYD_KP8;
		case SDLK_KP9: return KEYD_KP9;

		case SDLK_KP_PERIOD:   return KEYD_KP_DOT;
		case SDLK_KP_PLUS:     return KEYD_KP_PLUS;
		case SDLK_KP_MINUS:    return KEYD_KP_MINUS;
		case SDLK_KP_MULTIPLY: return KEYD_KP_STAR;
		case SDLK_KP_DIVIDE:   return KEYD_KP_SLASH;
		case SDLK_KP_EQUALS:   return KEYD_KP_EQUAL;
		case SDLK_KP_ENTER:    return KEYD_KP_ENTER;

		case SDLK_PRINT:     return KEYD_PRTSCR;
		case SDLK_CAPSLOCK:  return KEYD_CAPSLOCK;
		case SDLK_NUMLOCK:   return KEYD_NUMLOCK;
		case SDLK_SCROLLOCK: return KEYD_SCRLOCK;
		case SDLK_PAUSE:     return KEYD_PAUSE;

		case SDLK_LSHIFT:
		case SDLK_RSHIFT: return KEYD_RSHIFT;
		case SDLK_LCTRL:
		case SDLK_RCTRL:  return KEYD_RCTRL;
		case SDLK_LMETA:
		case SDLK_LALT:   return KEYD_LALT;
		case SDLK_RMETA:
		case SDLK_RALT:   return KEYD_RALT;

		default: break;
	}

	if (key <= 0x7f)
		return tolower(key);

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


void HandleKeyEvent(SDL_Event* ev)
{
	if (ev->type != SDL_KEYDOWN && ev->type != SDL_KEYUP) 
		return;

#ifdef DEBUG_KB
	if (ev->type == SDL_KEYDOWN)
		L_WriteDebug("  HandleKey: DOWN\n");
	else if (ev->type == SDL_KEYUP)
		L_WriteDebug("  HandleKey: UP\n");
#endif

	int sym = (int)ev->key.keysym.sym;

	event_t event;
	event.value.key.sym = TranslateSDLKey(sym);
	event.value.key.unicode = ev->key.keysym.unicode;

	// handle certain keys which don't behave normally
	if (sym == SDLK_CAPSLOCK || sym == SDLK_NUMLOCK)
	{
#ifdef DEBUG_KB
		L_WriteDebug("   HandleKey: CAPS or NUMLOCK\n");
#endif

		// -AJA- for some reason (perhaps not SDL's fault), the CAPSLOCK
		//       key behaves differently on Win32 and Linux.  Under Win32
		//       we get the "long press" behaviour, but on Linux we get
		//       "faked key-ups" behaviour.  Oi oi oi.
#ifdef LINUX
		if (ev->type != SDL_KEYDOWN)
			return;
#endif
		event.type = ev_keydown;
		E_PostEvent(&event);

		event.type = ev_keyup;
		E_PostEvent(&event);
		return;
	}

	event.type = (ev->type == SDL_KEYDOWN) ? ev_keydown : ev_keyup;

#ifdef DEBUG_KB
	L_WriteDebug("   HandleKey: sym=%d scan=%d unicode=%d --> key=%d\n",
			sym, ev->key.keysym.scancode, ev->key.keysym.unicode, event.value.key);
#endif

	if (event.value.key.sym < 0 &&
	    event.value.key.unicode == 0)
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

	if (event.value.key.sym == KEYD_LALT)
		alt_is_down = (event.type == ev_keydown);

	E_PostEvent(&event);
}


void HandleMouseButtonEvent(SDL_Event * ev)
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
		case 1: event.value.key.sym = KEYD_MOUSE1; break;
		case 2: event.value.key.sym = KEYD_MOUSE2; break;
		case 3: event.value.key.sym = KEYD_MOUSE3; break;

		// handle the mouse wheel
		case 4: event.value.key.sym = KEYD_WHEEL_UP; break; 
		case 5: event.value.key.sym = KEYD_WHEEL_DN; break; 

		case 6: event.value.key.sym = KEYD_MOUSE4; break;
		case 7: event.value.key.sym = KEYD_MOUSE5; break;
		case 8: event.value.key.sym = KEYD_MOUSE6; break;

		default:
			return;
	}

	E_PostEvent(&event);
}


void HandleJoystickButtonEvent(SDL_Event * ev)
{
	// ignore other joysticks;
	if ((int)ev->jbutton.which != cur_joy-1)
		return;

	event_t event;

	if (ev->type == SDL_JOYBUTTONDOWN) 
		event.type = ev_keydown;
	else if (ev->type == SDL_JOYBUTTONUP) 
		event.type = ev_keyup;
	else 
		return;

	if (ev->jbutton.button > 14)
		return;

	event.value.key.sym = KEYD_JOY1 + ev->jbutton.button;

	E_PostEvent(&event);
}


void HandleMouseMotionEvent(SDL_Event * ev)
{
	int dx, dy;

	if (in_warpmouse.d)
	{
		// -DEL- 2001/01/29 SDL_WarpMouse doesn't work properly on beos so
		// calculate relative movement manually.

		dx = ev->motion.x - (SCREENWIDTH/2);
		dy = ev->motion.y - (SCREENHEIGHT/2);

		// don't warp if we don't need to
		if (dx || dy)
			I_CentreMouse();
	}
	else
	{
		dx = ev->motion.xrel;
		dy = ev->motion.yrel;
	}

	if (dx || dy)
	{
		event_t event;

		event.type = ev_mouse;
		event.value.mouse.dx =  dx;
		event.value.mouse.dy = -dy;  // -AJA- positive should be "up"

		E_PostEvent(&event);
	}
}


int I_JoyGetAxis(int n)  // n begins at 0
{
	if (nojoy || !joy_info)
		return 0;

	if (n < joy_num_axes)
		return SDL_JoystickGetAxis(joy_info, n);
	
	n -= joy_num_axes;

	// -AJA- handle joystick HATS by mapping it to axes
	if (n/2 < joy_num_hats)
	{
		Uint8 hat = SDL_JoystickGetHat(joy_info, n/2);

		if (n & 1)
		{
			return (hat & SDL_HAT_DOWN) ? -32767 :
				   (hat & SDL_HAT_UP)   ? +32767 : 0;
		}
		else
		{
			return (hat & SDL_HAT_LEFT)  ? -32767 :
				   (hat & SDL_HAT_RIGHT) ? +32767 : 0;
		}
	}

	return 0;
}


//
// Event handling while the application is active
//
void ActiveEventProcess(SDL_Event *sdl_ev)
{
	switch(sdl_ev->type)
	{
		case SDL_ACTIVEEVENT:
		{
			if ((sdl_ev->active.state & SDL_APPINPUTFOCUS) &&
				(sdl_ev->active.gain == 0))
			{
				HandleFocusLost();
			}
			
			break;
		}
		
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			HandleKeyEvent(sdl_ev);
			break;
				
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			HandleMouseButtonEvent(sdl_ev);
			break;
		
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			HandleJoystickButtonEvent(sdl_ev);
			break;

		case SDL_MOUSEMOTION:
			if (eat_mouse_motion) 
			{
				eat_mouse_motion = false; // One motion needs to be discarded
				break;
			}

			HandleMouseMotionEvent(sdl_ev);
			break;
		
		case SDL_QUIT:
			// Note we deliberate clear all other flags here. Its our method of 
			// ensuring nothing more is done with events.
			app_state = APP_STATE_PENDING_QUIT;
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
	switch(sdl_ev->type)
	{
		case SDL_ACTIVEEVENT:
			if (app_state & APP_STATE_PENDING_QUIT)
				break; // Don't care: we're going to exit
			
			if (!sdl_ev->active.gain)
				break;
				
			if (sdl_ev->active.state & SDL_APPACTIVE ||
                sdl_ev->active.state & SDL_APPINPUTFOCUS)
				HandleFocusGain();
			break;

		case SDL_QUIT:
			// Note we deliberate clear all other flags here. Its our method of 
			// ensuring nothing more is done with events.
			app_state = APP_STATE_PENDING_QUIT;
			break;
					
		default:
			break; // Don't care
	}
}


void I_CentreMouse(void)
{
	SDL_WarpMouse(SCREENWIDTH/2, SCREENHEIGHT/2);
}


void I_ShowJoysticks(void)
{
	if (nojoy)
	{
		I_Printf("Joystick system is disabled.\n");
		return;
	}

	if (num_joys == 0)
	{
		I_Printf("No joysticks found.\n");
		return;
	}

	I_Printf("Joysticks:\n");

	for (int i = 0; i < num_joys; i++)
	{
		const char *name = SDL_JoystickName(i);
		if (! name)
			name = "(UNKNOWN)";

		I_Printf("  %2d : %s\n", i+1, name);
	}
}


void I_OpenJoystick(int index)
{
	SYS_ASSERT(1 <= index && index <= num_joys);

	joy_info = SDL_JoystickOpen(index-1);
	if (! joy_info)
	{
		I_Printf("Unable to open joystick %d (SDL error)\n", index);
		return;
	}

	cur_joy = index;

	const char *name = SDL_JoystickName(cur_joy-1);
	if (! name)
		name = "(UNKNOWN)";

	joy_num_axes    = SDL_JoystickNumAxes(joy_info);
	joy_num_buttons = SDL_JoystickNumButtons(joy_info);
	joy_num_hats    = SDL_JoystickNumHats(joy_info);
	joy_num_balls   = SDL_JoystickNumBalls(joy_info);

	I_Printf("Opened joystick %d : %s\n", cur_joy, name);
	I_Printf("Axes:%d buttons:%d hats:%d balls:%d\n",
			 joy_num_axes, joy_num_buttons, joy_num_hats, joy_num_balls);
}


void I_StartupJoystick(void)
{
	cur_joy = 0;

	if (M_CheckParm("-nojoy"))
	{
		I_Printf("I_StartupControl: Joystick system disabled.\n");
		nojoy = true;
		return;
	}

	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
	{
		I_Printf("I_StartupControl: Couldn't init SDL JOYSTICK!\n");
		nojoy = true;
		return;
	}

	SDL_JoystickEventState(SDL_ENABLE);

	num_joys = SDL_NumJoysticks();

	I_Printf("I_StartupControl: %d joysticks found.\n", num_joys);

	joystick_device = CLAMP(0, joystick_device, num_joys);

	if (num_joys == 0)
		return;

	if (joystick_device > 0)
		I_OpenJoystick(joystick_device);
}


void CheckJoystickChanged(void)
{
	int new_joy = joystick_device;

	if (joystick_device < 0 || joystick_device > num_joys)
		new_joy = 0;

	if (new_joy == cur_joy)
		return;

	if (joy_info)
	{
		SDL_JoystickClose(joy_info);
		joy_info = NULL;

		I_Printf("Closed joystick %d\n", cur_joy);
		cur_joy = 0;
	}

	if (new_joy > 0)
	{
		I_OpenJoystick(new_joy);
	}
}


/****** Input Event Generation ******/

void I_StartupControl(void)
{
	alt_is_down = false;

	SDL_EnableUNICODE(1);

	I_StartupJoystick();
}

void I_ControlGetEvents(void)
{
	CheckJoystickChanged();

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
}


int I_GetTime(void)
{
    Uint32 t = SDL_GetTicks();

    // more complex than "t*35/1000" to give more accuracy
    return (t / 1000) * 35 + (t % 1000) * 35 / 1000;
}


//
// Same as I_GetTime, but returns time in milliseconds
//
int I_GetMillies(void)
{
    return SDL_GetTicks();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
