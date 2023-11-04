//------------------------------------------
//  DOOM HUD CODE for EDGE
//  Copyright (c) 2009-2022 The Edge Team
//  Copyright (C) 1993-1996 by id Software, Inc.
//  Under the GNU General Public License
//------------------------------------------


function new_overlay_status() = 
 {
	var RelX
	
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.17
	
	if (player.has_key(1))
		hud.draw_image(  RelX, 165, "STKEYS0")

	if (player.has_key(2))
		hud.draw_image(  RelX, 175, "STKEYS1")

	if (player.has_key(3))
		hud.draw_image(  RelX, 185, "STKEYS2")

	
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.20
	
	if (player.has_key(5))
		hud.draw_image(  RelX, 165, "STKEYS3")

	if (player.has_key(6))
		hud.draw_image(  RelX, 175, "STKEYS4")

	if (player.has_key(7))
		hud.draw_image(  RelX, 185, "STKEYS5")


    hud.set_alpha(0.9) //**Alters Transparency of HUD Elements**
    hud.text_font("BIG_DIGIT")
	hud.set_scale(0.80)
	
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.945
	if(player.ammo_type(1) > 0)
		hud.draw_number( RelX, 180, 3, player.main_ammo(1), 1)
	
	
	hud.text_color(hud.NO_COLOR)

    if (player.health() < 35)
        hud.text_color(hud.LIGHTRED) 
		
    if (player.health() > 34)
        hud.text_color(hud.NO_COLOR)

		
	hud.set_scale(0.75)

	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.065
	hud.draw_number(RelX, 180, 3, player.health(),0)
	hud.text_color(hud.NO_COLOR)

	hud.set_scale(1.0)
	
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.01
	if (player.total_armor() > 0)
	{
		if (player.armor(1))
			hud.stretch_image(RelX, 165, 16, 10, "ARM1A0", 1)
		
		if (player.armor(2))
			hud.stretch_image(RelX, 165, 16, 10, "ARM2A0", 1)
			
		hud.set_scale(0.75)
		RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.065
		hud.draw_number(RelX, 165, 3, player.total_armor(),0)	
	}

    hud.set_scale(1.0)
	
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.01
	hud.stretch_image(RelX, 181, 16, 10, "MEDIA0", 1)
	
	hud.set_scale(0.85)
	
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.95
	
    if (player.cur_weapon() == "PISTOL")
    hud.stretch_image(RelX, 181, 13, 11, "CLIPA0", 1)

    else if (player.cur_weapon() == "SHOTGUN")
    hud.stretch_image(RelX, 181, 15, 11, "SHELA0", 1)

    else if (player.cur_weapon() == "SUPERSHOTGUN")
    hud.stretch_image(RelX, 181, 15, 11, "SHELA0", 1)

    else if (player.cur_weapon() == "CHAINGUN")
	hud.stretch_image(RelX, 181, 13, 11, "CLIPA0", 1)

    else if (player.cur_weapon() == "ROCKET_LAUNCHER")
	hud.stretch_image(RelX, 181, 9, 11, "ROCKA0", 1)

    else if (player.cur_weapon() == "PLASMA_RIFLE")
 	hud.stretch_image(RelX, 181, 13, 11, "CELLA0", 1)

    else if (player.cur_weapon() == "BFG9000")
	hud.stretch_image(RelX, 181, 9, 11, "ROCKA0", 1)
	
	hud.text_font("DOOM")
	hud.text_color(hud.NO_COLOR)
	
}


function draw_all() =
{
    hud.coord_sys(320, 200)
    hud.grab_times()

    if (hud.check_automap())
    {
        doom_automap()
        return
    }
	
    // there are three standard HUDs:
	// the first uses the built-in widescreen status bar
	// the second is the fullscreen hud
	// the third is "hudless", i.e. you can only see your weapon
	
    var which = hud.which_hud() % 3

	if (which == 0)
	{
		hud.universal_y_adjust = -16
		hud.render_world(0, 0, 320, 200 - 32)
		doom_status_bar()
	}
	else if (which == 1)
	{
		hud.universal_y_adjust = 0
		hud.render_world(0, 0, 320, 200)
		new_overlay_status()
	}
	else
	{
		hud.universal_y_adjust = 0
		hud.render_world(0, 0, 320, 200)
	}
		
	edge_air_bar()
	edge_time_bar()
    edge_footsteps()

}

 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
