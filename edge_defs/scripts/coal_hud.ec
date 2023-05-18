//------------------------------------------
//  DOOM HUD CODE for EDGE
//  Copyright (C) 2023 the EDGE-Classic Community
//  Copyright (c) 2009-2010 The Edge Team
//  Under the GNU General Public License
//------------------------------------------

var face_state : float
var face_tic  : float
var face_image : string
var step_tic  : float

// Default all of these to black so that in a "worse-case scenario", it's just the old behavior of black areas
var custom_stbar_average_color : vector = '0 0 0'
var custom_stbar_darkest_color : vector = '0 0 0'
var custom_stbar_lightest_color : vector = '0 0 0'
var custom_stbar_average_hue : vector = '0 0 0'

function doom_weapon_icon(slot, x, y, off_pic : string, on_pic : string) =
{
    if (player.has_weapon_slot(slot))
        hud.draw_image(x, y, on_pic)
    else
        hud.draw_image(x, y, off_pic)
}


function doom_key(x, y, card, skull,
    card_pic : string, skull_pic : string, both_pic : string) =
{
    var has_cd = player.has_key(card)
    var has_sk = player.has_key(skull)

    if (has_cd && has_sk)
    {
        hud.draw_image(x, y, both_pic)
    }
    else if (has_cd)
    {
        hud.draw_image(x, y, card_pic)
    }
    else if (has_sk)
    {
        hud.draw_image(x, y, skull_pic)
    }
}


function pain_digit() : string =
{
    var health = player.health()
    if (health > 100)
        health = 100

    var index = math.floor(4.99 * (100 - health) / 100)

    assert(index >= 0)
    assert(index <= 4)

    return "" + index
}

function turn_digit() : string =
{   
    var r = math.rint(math.rand_range(0, 2)) //always between 0 and 2
    
    return "" + r
}

function check_face_state() : float =
{
    // This routine handles the face states and their timing.
    // The precedence of expressions is:
    //
    //    dead > evil grin > turned head > straight ahead
    //

    // dead ?
    if (player.health() <= 0)
    {
        return 1
    }

    // evil grin when player just picked up a weapon
    if (player.is_grinning())
    {
        return 2
    }

    // being attacked ?
    if (player.hurt_by())
    {
        return 3
    }

    // rampaging?
    if (player.is_rampaging())
    {
        return 4
    }

    // god mode?
    if (player.has_power(player.INVULN))
    {
        return 5
    }

    // All "looking around" states are rolled up in one default state
    return 6
}

function set_face_params() =
{
    // This routine handles the face states and their timing.
    // The precedence of expressions is:
    //
    //    dead > evil grin > turned head > straight ahead
    //

    // dead ?
    if (face_state == 1)
    {
        face_image = "STFDEAD0"
        face_tic  = sys.gametic + 10
        return
    }

    // evil grin when player just picked up a weapon
    if (face_state == 2)
    {
        face_image = "STFEVL" + pain_digit()
        face_tic  = sys.gametic + 7
        return
    }

    // being attacked ?
    if (face_state == 3)
    {
        if (player.hurt_pain() > 20)
        {
            face_image = "STFOUCH" + pain_digit()
            face_tic = sys.gametic + 26
            return
        }

        var dir = 0

        if (player.hurt_by() != "self")
        {
            dir = player.hurt_dir()
        }

        if (dir < 0)
            face_image = "STFTL" + pain_digit() + "0"
        else if (dir > 0)
            face_image = "STFTR" + pain_digit() + "0"
        else
            face_image = "STFKILL" + pain_digit()

        face_tic = sys.gametic + 35
        return
    }

    // rampaging?
    if (face_state == 4)
    {
        face_image = "STFKILL" + pain_digit()
        face_tic  = sys.gametic + 7
        return
    }

    // god mode?
    if (face_state == 5)
    {
        face_image = "STFGOD0"
        face_tic  = sys.gametic + 7
        return
    }

    // default: look about the place...
    face_image = "STFST" + pain_digit() + turn_digit()
    face_tic  = sys.gametic + 17
}

function doomguy_face (x, y) =
{
    //---| doomguy_face |---

	var old_face = face_state
	
	face_state = check_face_state()

    if (!face_image || face_tic == sys.gametic || face_state < old_face || (face_state == 3 && old_face == 3))
        set_face_params()

    hud.draw_image(x - 1, y - 1, face_image)
}


function doom_little_ammo() =
{
    hud.text_font("YELLOW_DIGIT")
    hud.text_color(hud.NO_COLOR)

    hud.draw_num2(288, 173, 3, player.ammo(1))
    hud.draw_num2(288, 179, 3, player.ammo(2))
    hud.draw_num2(288, 185, 3, player.ammo(3))
    hud.draw_num2(288, 191, 3, player.ammo(4))

    hud.draw_num2(314, 173, 3, player.ammomax(1))
    hud.draw_num2(314, 179, 3, player.ammomax(2))
    hud.draw_num2(314, 185, 3, player.ammomax(3))
    hud.draw_num2(314, 191, 3, player.ammomax(4))
}


function doom_status_bar() =
{
	//Draw our extenders first, just in case the statusbar is already widescreen
	if (!hud.custom_stbar)
	{
		hud.draw_image(  -83, 168, "STBARL") // Widescreen border
		hud.draw_image(  320, 168, "STBARR") // Widescreen border
	}
	else
	{
		hud.solid_box(-83, 168, 83, 32, custom_stbar_average_color)
		hud.solid_box(320, 168, 83, 32, custom_stbar_average_color)
	}
	
	var centerOffsetX : float
	var tempwidth : float
	
	tempwidth = hud.get_image_width("STBAR")
	centerOffsetX = tempwidth / 2;
	
	//hud.draw_image(  0, 168, "STBAR")
	hud.draw_image(  160 - centerOffsetX, 168, "STBAR",1)
	
    hud.draw_image( 90, 171, "STTPRCNT")
    hud.draw_image(221, 171, "STTPRCNT")

    hud.text_font("BIG_DIGIT")

    hud.draw_num2( 44, 171, 3, player.main_ammo(1) )
    hud.draw_num2( 90, 171, 3, player.health()     )
    hud.draw_num2(221, 171, 3, player.total_armor())

    if (hud.game_mode() == "dm")
    {
        hud.draw_num2(138, 171, 2, player.frags())
    }
    else
    {
        hud.draw_image(104, 168, "STARMS")

        doom_weapon_icon(2, 111, 172, "STGNUM2", "STYSNUM2")
        doom_weapon_icon(3, 123, 172, "STGNUM3", "STYSNUM3")
        doom_weapon_icon(4, 135, 172, "STGNUM4", "STYSNUM4")

        doom_weapon_icon(5, 111, 182, "STGNUM5", "STYSNUM5")
        doom_weapon_icon(6, 123, 182, "STGNUM6", "STYSNUM6")
        doom_weapon_icon(7, 135, 182, "STGNUM7", "STYSNUM7")
    }

    doomguy_face(144, 169)

    doom_key(239, 171, 1, 5, "STKEYS0", "STKEYS3", "STKEYS6")
    doom_key(239, 181, 2, 6, "STKEYS1", "STKEYS4", "STKEYS7")
    doom_key(239, 191, 3, 7, "STKEYS2", "STKEYS5", "STKEYS8")

    doom_little_ammo()
}

function doom_status_bar2() =
{
   
    //Draw our extenders first, just in case the statusbar is already widescreen
	hud.draw_image(  -83, 168, "STBARL") // Widescreen border
	hud.draw_image(  320, 168, "STBARR") // Widescreen border
	if (hud.custom_stbar)
	{	
		hud.set_alpha(0.85) //**Alters Transparency of HUD Elements**
		hud.solid_box(-83, 168, 83, 32, custom_stbar_average_color)
		hud.solid_box(320, 168, 83, 32, custom_stbar_average_color)
		hud.set_alpha(1.0) //**Alters Transparency of HUD Elements**
	}
	
	var centerOffsetX : float
	var tempwidth : float
	
	tempwidth = hud.get_image_width("STBAR")
	centerOffsetX = tempwidth / 2;
	
	//hud.draw_image(  0, 168, "STBAR")
	hud.draw_image(  160 - centerOffsetX, 168, "STBAR")
	
    hud.draw_image( 90, 171, "STTPRCNT")
    hud.draw_image(221, 171, "STTPRCNT")

    hud.text_font("BIG_DIGIT")

    hud.draw_num2( 44, 171, 3, player.main_ammo(1) )
    hud.draw_num2( 90, 171, 3, player.health()     )
    hud.draw_num2(221, 171, 3, player.total_armor())

    if (hud.game_mode() == "dm")
    {
        hud.draw_num2(138, 171, 2, player.frags())
    }
    else
    {
        hud.draw_image(104, 168, "STARMS")

        doom_weapon_icon(2, 111, 172, "STGNUM2", "STYSNUM2")
        doom_weapon_icon(3, 123, 172, "STGNUM3", "STYSNUM3")
        doom_weapon_icon(4, 135, 172, "STGNUM4", "STYSNUM4")

        doom_weapon_icon(5, 111, 182, "STGNUM5", "STYSNUM5")
        doom_weapon_icon(6, 123, 182, "STGNUM6", "STYSNUM6")
        doom_weapon_icon(7, 135, 182, "STGNUM7", "STYSNUM7")
    }

    doomguy_face(144, 169)

    doom_key(239, 171, 1, 5, "STKEYS0", "STKEYS3", "STKEYS6")
    doom_key(239, 181, 2, 6, "STKEYS1", "STKEYS4", "STKEYS7")
    doom_key(239, 191, 3, 7, "STKEYS2", "STKEYS5", "STKEYS8")

    doom_little_ammo()
    hud.set_alpha(1.0) //**Alters Transparency of HUD Elements**
}


function doom_overlay_status() = 
{
    hud.text_font("BIG_DIGIT")

    hud.draw_num2(100, 171, 3, player.health())

    hud.text_color(hud.YELLOW)
    hud.draw_num2( 44, 171, 3, player.main_ammo(1))

    if (player.total_armor() > 100)
        hud.text_color(hud.BLUE)
    else
        hud.text_color(hud.GREEN)

    hud.draw_num2(242, 171, 3, player.total_armor())

    doom_key(256, 171, 1, 5, "STKEYS0", "STKEYS3", "STKEYS6")
    doom_key(256, 181, 2, 6, "STKEYS1", "STKEYS4", "STKEYS7")
    doom_key(256, 191, 3, 7, "STKEYS2", "STKEYS5", "STKEYS8")

    doom_little_ammo()
}


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
        hud.text_color(hud.RED) 
		
    if (player.health() > 34)
        hud.text_color(hud.GREEN)

		
	hud.set_scale(0.75)

	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.065
	hud.draw_number(RelX, 180, 3, player.health(),0)
	hud.text_color(hud.NO_COLOR)

	hud.set_scale(1.0)
	
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.01
	if (player.total_armor() > 0)
	{
		if (player.total_armor() > 0)
			hud.text_color(hud.GREEN)
		
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

    if (player.cur_weapon() == "SHOTGUN")
    hud.stretch_image(RelX, 181, 15, 11, "SHELA0", 1)

    if (player.cur_weapon() == "SUPERSHOTGUN")
    hud.stretch_image(RelX, 181, 15, 11, "SHELA0", 1)

    if (player.cur_weapon() == "CHAINGUN")
	hud.stretch_image(RelX, 181, 13, 11, "CLIPA0", 1)

    if (player.cur_weapon() == "ROCKET_LAUNCHER")
	hud.stretch_image(RelX, 181, 9, 11, "ROCKA0", 1)

    if (player.cur_weapon() == "PLASMA_RIFLE")
 	hud.stretch_image(RelX, 181, 13, 11, "CELLA0", 1)

    if (player.cur_weapon() == "BFG9000")
	hud.stretch_image(RelX, 181, 13, 11, "CELLA0", 1)
	
	hud.text_font("DOOM")
	hud.text_color(hud.NO_COLOR)
	
}


function doom_automap() =
{
    // Background is already black, only need to use 'solid_box'
    // when we want a different color.
    //
    // hud.solid_box(0, 0, 320, 200 - 32, '80 80 80')

    hud.render_automap(0, 0, 320, 200 - 32)
	var which = hud.which_hud() % 4
	
	if (which == 0)
	{	
		doom_status_bar()
	}
	else if (which == 1)
	{
		doom_status_bar2()
	}
	else if (which == 2)
	{
		new_overlay_status()
	}
//	else
//	{
//		//new_overlay_status()
//	}

    hud.text_font("DOOM")
    hud.text_color(hud.GREEN)
    
    if (strings.len(hud.map_author()) > 0)
    {
        hud.draw_text(0, 200 - 32 - 20, hud.map_title())
        hud.draw_text(0, 200 - 32 - 10, " Author: " + hud.map_author())
    }
    else
    {
        hud.draw_text(0, 200 - 32 - 10, hud.map_title())
    }
    
    hud.set_scale(0.75)
    hud.draw_text(10, 20, "Kills:    " + player.kills() + "/" + player.map_enemies())

	if (player.map_secrets() > 0)
	{
		hud.draw_text(10, 25, "Secrets: " + player.secrets() + "/" + player.map_secrets())
	}
	if (player.map_items() > 0)
	{
		hud.draw_text(10, 30, "Items:    " + player.items() + "/" + player.map_items())
	}
	hud.set_scale(1.0)
}



function edge_draw_bar(BarPos : vector, BarHeight, BarLength, MaxValue, CurrentValue, BarColor1 : vector, BarColor2 : vector) =
{  
    var BarValue = 0
	var percentvalue = 0
    
    var TopX = BarPos * '1 0 0'
	var TopY = BarPos * '0 1 0'
	TopX = hud.x_left + (hud.x_right - hud.x_left) * TopX
    
    //Convert to a percentage of the bars length
	percentvalue = CurrentValue //strings.tonumber(CurrentValue)
	percentvalue = percentvalue * BarLength
	percentvalue = percentvalue / MaxValue //(strings.tonumber(MaxValue))
	
	BarValue = math.floor(percentvalue - 1)
	var BottomBarHeight = BarHeight / 2
	
    hud.thin_box(TopX, TopY, BarLength, BarHeight, hud.GRAY)

	if (BarValue > 1)
	{
		hud.gradient_box(TopX + 1, TopY + 1, BarValue - 1, BottomBarHeight, BarColor1, BarColor2, BarColor1, BarColor2)
		hud.gradient_box(TopX + 1, TopY + BottomBarHeight, BarValue - 1, BottomBarHeight - 1, BarColor2, BarColor1, BarColor2, BarColor1)
	}
}

function edge_air_bar() =
{
	if (player.health() <= 0)
        return

    if (! player.under_water())
        return
        
    //var TopX = 250     //Where we want it drawn
	//var TopY = 10      //Where we want it drawn
	var BarLocation : vector
	BarLocation  = '0.8 10 0' //'X Y 0'
	
	var BarHeight = 8  //How high we want the bar
	var BarLength = 51 //How long we want the bar
	
    var BarMaxValue = 0
    var CurrentValue = 0
    
    BarMaxValue = 100 //Air_in_lungs is a percentage value so max is 100
    CurrentValue = math.floor(player.air_in_lungs()) //current air
    
    //edge_draw_bar(TopX, TopY, BarHeight, BarLength, MaxValue, CurrentValue) 
	edge_draw_bar(BarLocation, BarHeight, BarLength, BarMaxValue, CurrentValue, hud.BLACK, hud.LIGHTBLUE)
	hud.play_sound("HEARTBT1")
}

function edge_time_bar() =
{
	if (player.health() <= 0)
        return

    if (! player.has_power(player.STOP_TIME))
        return
        
    //var TopX = 250     //Where we want it drawn
	//var TopY = 10      //Where we want it drawn
	var BarLocation : vector
	BarLocation  = '0.8 20 0' //'X Y 0'
	
	var BarHeight = 8  //How high we want the bar
	var BarLength = 51 //How long we want the bar
	
    var BarMaxValue = 0
    var CurrentValue = 0
    
    BarMaxValue = 20 //default STOP_TIME lasts 20 seconds
    CurrentValue = math.floor(player.power_left(player.STOP_TIME)) //current air
    
    //edge_draw_bar(TopX, TopY, BarHeight, BarLength, MaxValue, CurrentValue) 
	edge_draw_bar(BarLocation, BarHeight, BarLength, BarMaxValue, CurrentValue, hud.BLACK, hud.PURPLE)
	hud.play_sound("HEARTBT1")
}

//***********************
// Start footsteps code

function DoesNameStartWith(TheName : string, ThePart : string) : float =
{
	var tempstr : string
	var templen : float
	
	tempstr = strings.sub(TheName, 1, strings.len(ThePart))
	
	//hud.draw_text(10, 10, tempstr)
	
	if (tempstr == ThePart)
		return 1
	
	return 0
}

function edge_footsteps() =
{
    if (player.is_swimming())
        return
        
    if (player.is_jumping())
        return    

    if (! player.on_ground())
		return
	
    if (hud.game_paused() == 1)
		return 
	
	if (player.move_speed() <= 1.4)
	{	
		step_tic  = sys.gametic + 12
		return
	}
    
   	//hud.text_font("DOOM")
    //hud.draw_text(10, 10, player.floor_flat()) 
    //hud.draw_text(10, 20,"compare:" + strings.find(player.floor_flat(), "WATER"))
    //hud.draw_text(10, 50,"speed:" + player.move_speed())
    //hud.draw_text(10, 30,"sector tag:" + player.sector_tag())
    
    if (step_tic > sys.gametic)
    	return
   
    player.play_footstep(player.floor_flat())

    //var loopCounter = 0
    //for (loopCounter = 1, 5) 
    //{
    //	hud.draw_text(loopCounter * 10, 30, ".")
    //}
    
	if (player.move_speed() > 10) //we're running so speed up time between sfx
	{	
		step_tic  = sys.gametic + 9
	}	
	else
	{
		step_tic  = sys.gametic + 12
    }
}
// End footsteps code
//***********************

function new_game() =
{
	if (hud.custom_stbar)
	{
		custom_stbar_average_color = hud.get_average_color("STBAR")
		custom_stbar_darkest_color = hud.get_darkest_color("STBAR")
		custom_stbar_lightest_color = hud.get_lightest_color("STBAR")
	}
}

function load_game() =
{
	if (hud.custom_stbar)
	{
		custom_stbar_average_color = hud.get_average_color("STBAR")
		custom_stbar_darkest_color = hud.get_darkest_color("STBAR")
		custom_stbar_lightest_color = hud.get_lightest_color("STBAR")
	}
}

function save_game() =
{

}

function begin_level() =
{
	face_tic = sys.gametic
	step_tic = sys.gametic
	face_state = 6 // Idle face
}

function end_level() =
{

}



function draw_all() =
{
    hud.coord_sys(320, 200)

    if (hud.check_automap())
    {
        doom_automap()
        return
    }
	
    // there are four standard HUDs:
	// the first two have different styles for the widescreen status bar extenders
	// the third is the fullscreen hud
	// the fourth is "hudless", i.e. you can only see your weapon
	
    var which = hud.which_hud() % 4

	if (which == 0)
	{
		hud.universal_y_adjust = -16
		hud.render_world(0, 0, 320, 200 - 32)
		doom_status_bar()
	}
	else if (which == 1)
	{
		hud.universal_y_adjust = -16
		hud.render_world(0, 0, 320, 200 - 32)
		doom_status_bar2()
	}
	else if (which == 2)
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

 
 
