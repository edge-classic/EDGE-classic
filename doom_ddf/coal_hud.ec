//------------------------------------------
//  DOOM HUD CODE for EDGE
//  Copyright (c) 2009-2010 The Edge Team
//  Copyright (C) 1993-1996 by id Software, Inc.
//  Under the GNU General Public License
//------------------------------------------

var face_time  : float
var face_image : string


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
    var r = math.floor(math.random() * 2.99)

    return "" + r
}

function select_new_face() =
{
    // This routine handles the face states and their timing.
    // The precedence of expressions is:
    //
    //    dead > evil grin > turned head > straight ahead
    //

    // dead ?
    if (player.health() <= 0)
    {
        face_image = "STFDEAD0"
        face_time  = 10
        return
    }

    // evil grin when player just picked up a weapon
    if (player.is_grinning())
    {
        face_image = "STFEVL" + pain_digit()
        face_time  = 7
        return
    }

    // being attacked ?
    if (player.hurt_by())
    {
        if (player.hurt_pain() > 50)
        {
            face_image = "STFOUCH" + pain_digit()
            face_time = 26
            return
        }

        var dir = 0

        if (player.hurt_by() == "enemy" ||
            player.hurt_by() == "friend")
        {
            dir = player.hurt_dir()
        }

        if (dir < 0)
            face_image = "STFTL" + pain_digit() + "0"
        else if (dir > 0)
            face_image = "STFTR" + pain_digit() + "0"
        else
            face_image = "STFKILL" + pain_digit()

        face_time = 35
        return
    }

    // rampaging?
    if (player.is_rampaging())
    {
        face_image = "STFKILL" + pain_digit()
        face_time  = 7
        return
    }

    // god mode?
    if (player.has_power(player.INVULN))
    {
        face_image = "STFGOD0"
        face_time  = 7
        return
    }

    // default: look about the place...
    face_image = "STFST" + pain_digit() + turn_digit()
    face_time  = 17
}

function doomguy_face (x, y) =
{
    //---| doomguy_face |---

    face_time = face_time - hud.passed_time

    if (face_time <= 0)
        select_new_face()

    // FIXME faceback

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
    hud.draw_image(  0, 168, "STBAR")
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


function doom_automap() =
{
    // Background is already black, only need to use 'solid_box'
    // when we want a different color.
    //
    // hud.solid_box(0, 0, 320, 200 - 32, '80 80 80')

    hud.render_automap(0, 0, 320, 200 - 32)

    doom_status_bar()

    hud.text_font("DOOM")
    hud.draw_text(0, 200 - 32 - 10, hud.map_title())
}


function edge_air_bar() =
{
    if (player.health() <= 0)
        return

    if (! player.under_water())
        return

    var air = player.air_in_lungs()

    air = math.floor(1 + 21 * ((100 - air) / 100.1))

    var barname : string;
    
    if (air < 10)
        barname = "AIRBAR0" + air
    else
        barname = "AIRBAR" + air

    hud.draw_image(0, 0, barname)
}


function begin_level() =
{
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

    // there are three standard HUDs
    var which = hud.which_hud() % 3

    if (which == 0)
        hud.render_world(0, 0, 320, 200 - 32)
    else
        hud.render_world(0, 0, 320, 200)

    if (which == 0)
        doom_status_bar()
    else if (which == 2)
        doom_overlay_status()

    edge_air_bar()
}

