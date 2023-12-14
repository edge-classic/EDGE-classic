--------------------------------------------
--  DOOM HUD CODE for EDGE
--  Copyright (C) 2023 the EDGE-Classic Community
--  Copyright (c) 2009-2010 The Edge Team
--  Under the GNU General Public License
--------------------------------------------

local face_state = 0
local face_tic = 0
local face_image = ""
local step_tic = 0

-- Default all of these to black so that in a "worse-case scenario", it's just the old behavior of black areas
local custom_stbar_average_color = vec3(0, 0, 0)
local custom_stbar_darkest_color = vec3(0, 0, 0)
local custom_stbar_lightest_color = vec3(0, 0, 0)
local custom_stbar_average_hue = vec3(0, 0, 0)

function doom_weapon_icon(slot, x, y, off_pic, on_pic)
    if (player.has_weapon_slot(slot)) then
        hud.draw_image(x, y, on_pic)
    else
        hud.draw_image(x, y, off_pic)
    end
end

function doom_key(x, y, card, skull,
                  card_pic, skull_pic, both_pic)
    local has_cd = player.has_key(card)
    local has_sk = player.has_key(skull)

    if (has_cd and has_sk) then
        hud.draw_image(x, y, both_pic)
    elseif (has_cd) then
        hud.draw_image(x, y, card_pic)
    elseif (has_sk) then
        hud.draw_image(x, y, skull_pic)
    end
end

function pain_digit()
    local health = player.health()
    if (health > 100) then
        health = 100
    end

    local index = math.floor(4.99 * (100 - health) / 100)

    assert(index >= 0)
    assert(index <= 4)
    
    return tostring(math.tointeger(index))
end

function turn_digit()
    local r = math.random(0, 2)     
    return tostring(r)
end

function check_face_state()
    -- This routine handles the face states and their timing.
    -- The precedence of expressions is:
    --
    --    dead > evil grin > turned head > straight ahead
    --

    -- dead ?
    if (player.health() <= 0) then
        return 1
    end

    -- evil grin when player just picked up a weapon
    if (player.is_grinning()) then
        return 2
    end

    -- being attacked ?
    if (player.hurt_by() ~= "") then
        return 3
    end

    -- rampaging?
    if (player.is_rampaging()) then
        return 4
    end

    -- god mode?
    if (player.has_power(player.INVULN)) then
        return 5
    end

    -- All "looking around" states are rolled up in one default state
    return 6
end

function set_face_params()
    -- This routine handles the face states and their timing.
    -- The precedence of expressions is:
    --
    --    dead > evil grin > turned head > straight ahead
    --

    -- dead ?
    if (face_state == 1) then
        face_image = "STFDEAD0"
        face_tic   = sys.gametic + 10
        return
    end

    -- evil grin when player just picked up a weapon
    if (face_state == 2) then
        face_image = "STFEVL" .. pain_digit()
        face_tic   = sys.gametic + 7
        return
    end

    -- being attacked ?
    if (face_state == 3) then
        if (player.hurt_pain() > 20) then
            face_image = "STFOUCH" .. pain_digit()
            face_tic = sys.gametic + 26
            return
        end

        local dir = 0

        if (player.hurt_by() ~= "self") then            
            dir = player.hurt_dir()            
        end

        if (dir < 0) then
            face_image = "STFTL" .. pain_digit() .. "0"
        elseif (dir > 0) then
            face_image = "STFTR" .. pain_digit() .. "0"
        else
            face_image = "STFKILL" .. pain_digit()
        end

        face_tic = sys.gametic + 35
        return
    end

    -- rampaging?
    if (face_state == 4) then
        face_image = "STFKILL" .. pain_digit()
        face_tic   = sys.gametic + 7
        return
    end

    -- god mode?
    if (face_state == 5) then
        face_image = "STFGOD0"
        face_tic   = sys.gametic + 7
        return
    end

    -- default: look about the place...
    face_image = "STFST" .. pain_digit() .. turn_digit()    
    face_tic   = sys.gametic + 17
end

function doomguy_face(x, y)
    -----| doomguy_face |---

    local old_face = face_state

    face_state = check_face_state()

    if (face_image == "" or face_tic == sys.gametic or face_state < old_face or (face_state == 3 and old_face == 3)) then
        set_face_params()
    end

    hud.draw_image(x - 1, y - 1, face_image)
end

function doom_little_ammo()
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
end

function doom_status_bar_common()
    local centerOffsetX = 0
    local tempwidth     = 0

    tempwidth           = hud.get_image_width("STBAR")
    centerOffsetX       = tempwidth / 2;

    --hud.draw_image(  0, 168, "STBAR")
    hud.draw_image(160 - centerOffsetX, 168, "STBAR", 1)

    hud.draw_image(90, 171, "STTPRCNT")
    hud.draw_image(221, 171, "STTPRCNT")

    hud.text_font("BIG_DIGIT")

    hud.draw_num2(44, 171, 3, player.main_ammo(1))
    hud.draw_num2(90, 171, 3, player.health())
    hud.draw_num2(221, 171, 3, player.total_armor())

    if (hud.game_mode() == "dm") then
        hud.draw_num2(138, 171, 2, player.frags())
    else
        hud.draw_image(104, 168, "STARMS")

        doom_weapon_icon(2, 111, 172, "STGNUM2", "STYSNUM2")
        doom_weapon_icon(3, 123, 172, "STGNUM3", "STYSNUM3")
        doom_weapon_icon(4, 135, 172, "STGNUM4", "STYSNUM4")

        doom_weapon_icon(5, 111, 182, "STGNUM5", "STYSNUM5")
        doom_weapon_icon(6, 123, 182, "STGNUM6", "STYSNUM6")
        doom_weapon_icon(7, 135, 182, "STGNUM7", "STYSNUM7")
    end

    doomguy_face(144, 169)

    doom_key(239, 171, 1, 5, "STKEYS0", "STKEYS3", "STKEYS6")
    doom_key(239, 181, 2, 6, "STKEYS1", "STKEYS4", "STKEYS7")
    doom_key(239, 191, 3, 7, "STKEYS2", "STKEYS5", "STKEYS8")

    doom_little_ammo()

    hud.set_alpha(1.0) --**Alters Transparency of HUD Elements**
end

--This one adds plain extenders of the average colour
function doom_status_bar()
    --Draw our extenders first, just in case the statusbar is already widescreen
    if (not hud.custom_stbar) then
        hud.draw_image(-83, 168, "STBARL") -- Widescreen border
        hud.draw_image(320, 168, "STBARR") -- Widescreen border	
    else
        hud.solid_box(-83, 168, 83, 32, custom_stbar_average_color)
        hud.solid_box(320, 168, 83, 32, custom_stbar_average_color)
    end

    doom_status_bar_common()
end

--This one adds textured extenders of the average colour
function doom_status_bar2()
    --Draw our extenders first, just in case the statusbar is already widescreen
    hud.draw_image(-83, 168, "STBARL") -- Widescreen border
    hud.draw_image(320, 168, "STBARR") -- Widescreen border
    if (hud.custom_stbar) then
        hud.set_alpha(0.85)            --**Alters Transparency of HUD Elements**
        hud.solid_box(-83, 168, 83, 32, custom_stbar_average_color)
        hud.solid_box(320, 168, 83, 32, custom_stbar_average_color)
        hud.set_alpha(1.0) --**Alters Transparency of HUD Elements**
    end

    doom_status_bar_common()
end

function new_overlay_status()
    local RelX = 0
    local AbsY = 0
    local YOffset = 0


    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.17
    AbsY = 166
    YOffset = 8

    if (player.has_key(4)) then --green card
        hud.draw_image(RelX, AbsY, "STKEYS9")
    end

    AbsY = AbsY + YOffset
    if (player.has_key(1)) then
        hud.draw_image(RelX, AbsY, "STKEYS0")
    end

    AbsY = AbsY + YOffset
    if (player.has_key(2)) then
        hud.draw_image(RelX, AbsY, "STKEYS1")
    end

    AbsY = AbsY + YOffset
    if (player.has_key(3)) then
        hud.draw_image(RelX, AbsY, "STKEYS2")
    end



    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.20
    AbsY = 166
    YOffset = 8

    if (player.has_key(8)) then --green skull
        hud.draw_image(RelX, AbsY, "STKEYSA")
    end

    AbsY = AbsY + YOffset
    if (player.has_key(5)) then
        hud.draw_image(RelX, AbsY, "STKEYS3")
    end

    AbsY = AbsY + YOffset
    if (player.has_key(6)) then
        hud.draw_image(RelX, AbsY, "STKEYS4")
    end

    AbsY = AbsY + YOffset
    if (player.has_key(7)) then
        hud.draw_image(RelX, AbsY, "STKEYS5")
    end



    hud.set_alpha(0.9) --**Alters Transparency of HUD Elements**
    hud.text_font("BIG_DIGIT")
    hud.set_scale(0.80)

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.945
    if (player.ammo_type(1) > 0) then
        hud.draw_number(RelX, 180, 3, player.main_ammo(1), 1)
    end


    hud.text_color(hud.NO_COLOR)

    if (player.health() < 35) then
        hud.text_color(hud.RED)
    end

    if (player.health() > 34) then
        hud.text_color(hud.GREEN)
    end


    hud.set_scale(0.75)

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.065
    hud.draw_number(RelX, 180, 3, player.health(), 0)
    hud.text_color(hud.NO_COLOR)

    hud.set_scale(1.0)

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.01
    if (player.total_armor() > 0) then

        hud.text_color(hud.GREEN)

        if (player.armor(player.GREEN_ARMOR) > 0) then
            hud.stretch_image(RelX, 165, 16, 10, "ARM1A0", 1)
        end

        if (player.armor(player.BLUE_ARMOR) > 0) then
            hud.stretch_image(RelX, 165, 16, 10, "ARM2A0", 1)
        end

        if (player.armor(player.PURPLE_ARMOR) > 0) then
            hud.stretch_image(RelX, 165, 16, 10, "ARM3A0", 1)
        end

        if (player.armor(player.YELLOW_ARMOR) > 0) then
            hud.stretch_image(RelX, 165, 16, 10, "ARM4A0", 1)
        end

        if (player.armor(player.RED_ARMOR) > 0) then
            hud.stretch_image(RelX, 165, 16, 10, "ARM5A0", 1)
        end

        hud.set_scale(0.75)
        RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.065
        hud.draw_number(RelX, 165, 3, player.total_armor(), 0)
    end

    hud.set_scale(1.0)

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.01
    hud.stretch_image(RelX, 181, 16, 10, "MEDIA0", 1)

    hud.set_scale(0.85)

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.95

    if (player.cur_weapon() == "PISTOL") then
        hud.stretch_image(RelX, 181, 13, 11, "CLIPA0", 1)
    end

    if (player.cur_weapon() == "SHOTGUN") then
        hud.stretch_image(RelX, 181, 15, 11, "SHELA0", 1)
    end

    if (player.cur_weapon() == "SUPERSHOTGUN") then
        hud.stretch_image(RelX, 181, 15, 11, "SHELA0", 1)
    end

    if (player.cur_weapon() == "CHAINGUN") then
        hud.stretch_image(RelX, 181, 13, 11, "CLIPA0", 1)
    end

    if (player.cur_weapon() == "ROCKET_LAUNCHER") then
        hud.stretch_image(RelX, 181, 9, 11, "ROCKA0", 1)
    end

    if (player.cur_weapon() == "PLASMA_RIFLE") then
        hud.stretch_image(RelX, 181, 13, 11, "CELLA0", 1)
    end

    if (player.cur_weapon() == "BFG9000") then
        hud.stretch_image(RelX, 181, 13, 11, "CELLA0", 1)
    end

    hud.text_font("DOOM")
    hud.text_color(hud.NO_COLOR)
end

function doom_automap()
    -- Background is already black, only need to use 'solid_box'
    -- when we want a different color.
    --
    -- hud.solid_box(0, 0, 320, 200 - 32, '80 80 80')

    hud.render_automap(0, 0, 320, 200 - 32)
    local which = hud.which_hud() % 4

    if (which == 0) then
        doom_status_bar()
    elseif (which == 1) then
        doom_status_bar2()
    elseif (which == 2) then
        new_overlay_status()
    end


    hud.text_font("DOOM")
    hud.text_color(hud.GREEN)

    if (#hud.map_author() > 0) then
        hud.draw_text(0, 200 - 32 - 20, hud.map_title())
        hud.draw_text(0, 200 - 32 - 10, " Author: " .. hud.map_author())
    else
        hud.draw_text(0, 200 - 32 - 10, hud.map_title())
    end

    hud.set_scale(0.75)
    hud.draw_text(10, 20, "Kills:    " .. player.kills() .. "/" .. player.map_enemies())

    if (player.map_secrets() > 0) then
        hud.draw_text(10, 25, "Secrets: " .. player.secrets() .. "/" .. player.map_secrets())
    end
    if (player.map_items() > 0) then
        hud.draw_text(10, 30, "Items:    " .. player.items() .. "/" .. player.map_items())
    end
    hud.set_scale(1.0)
end

function edge_draw_bar(BarPos, BarHeight, BarLength, MaxValue, CurrentValue, BarColor1, BarColor2)
    local BarValue = 0
    local percentvalue = 0

    local TopX = BarPos.x
    local TopY = BarPos.y
    TopX = hud.x_left + (hud.x_right - hud.x_left) * TopX

    --Convert to a percentage of the bars length
    percentvalue = CurrentValue            --strings.tonumber(CurrentValue)
    percentvalue = percentvalue * BarLength
    percentvalue = percentvalue / MaxValue --(strings.tonumber(MaxValue))

    BarValue = math.floor(percentvalue - 1)
    local BottomBarHeight = BarHeight / 2

    hud.thin_box(TopX, TopY, BarLength, BarHeight, hud.GRAY)

    if (BarValue > 1) then
        hud.gradient_box(TopX + 1, TopY + 1, BarValue - 1, BottomBarHeight, BarColor1, BarColor2, BarColor1, BarColor2)
        hud.gradient_box(TopX + 1, TopY + BottomBarHeight, BarValue - 1, BottomBarHeight - 1, BarColor2, BarColor1,
            BarColor2, BarColor1)
    end
end

function edge_air_bar()
    if (player.health() <= 0) then
        return
    end

    if (not player.under_water()) then
        return
    end

    --local TopX = 250     --Where we want it drawn
    --local TopY = 10      --Where we want it drawn
    local BarLocation = vec3(0.8, 10, 0)

    local BarHeight = 8  --How high we want the bar
    local BarLength = 51 --How long we want the bar

    local BarMaxValue = 0
    local CurrentValue = 0

    BarMaxValue = 100                                  --Air_in_lungs is a percentage value so max is 100
    CurrentValue = math.floor(player.air_in_lungs()) --current air

    --edge_draw_bar(TopX, TopY, BarHeight, BarLength, MaxValue, CurrentValue)
    edge_draw_bar(BarLocation, BarHeight, BarLength, BarMaxValue, CurrentValue, hud.BLACK, hud.LIGHTBLUE)
    hud.play_sound("HEARTBT1")
end

function edge_time_bar()
    if (player.health() <= 0) then
        return
    end

    if (not player.has_power(player.STOP_TIME)) then
        return
    end

    --local TopX = 250     --Where we want it drawn
    --local TopY = 10      --Where we want it drawn
    local BarLocation = vec3(0.8, 20, 0)

    local BarHeight = 8  --How high we want the bar
    local BarLength = 51 --How long we want the bar

    local BarMaxValue = 0
    local CurrentValue = 0

    BarMaxValue = 20                                                 --default STOP_TIME lasts 20 seconds
    CurrentValue = math.floor(player.power_left(player.STOP_TIME)) --current air

    --edge_draw_bar(TopX, TopY, BarHeight, BarLength, MaxValue, CurrentValue)
    edge_draw_bar(BarLocation, BarHeight, BarLength, BarMaxValue, CurrentValue, hud.BLACK, hud.PURPLE)
    hud.play_sound("HEARTBT1")
end

--***********************
-- Start footsteps code

function DoesNameStartWith(TheName, ThePart)
    local tempstr = "";
    local templen = 0

    tempstr = string.sub(TheName, 1, #ThePart)

    --hud.draw_text(10, 10, tempstr)

    if (tempstr == ThePart) then
        return 1
    end

    return 0
end

function edge_footsteps()
    if (player.is_swimming()) then
        return
    end

    if (player.is_jumping()) then
        return
    end

    if (not player.on_ground()) then
        return
    end

    if (hud.game_paused() == 1) then
        return
    end

    if (player.move_speed() <= 1.4) then
        step_tic = sys.gametic + 12
        return
    end

    --hud.text_font("DOOM")
    --hud.draw_text(10, 10, player.floor_flat())
    --hud.draw_text(10, 20,"compare:" + strings.find(player.floor_flat(), "WATER"))
    --hud.draw_text(10, 50,"speed:" + player.move_speed())
    --hud.draw_text(10, 30,"sector tag:" + player.sector_tag())

    if (step_tic > sys.gametic) then
        return
    end

    player.play_footstep(player.floor_flat())

    --local loopCounter = 0
    --for (loopCounter = 1, 5)
    --{
    --	hud.draw_text(loopCounter * 10, 30, ".")
    --}

    if (player.move_speed() > 10) then --we're running so speed up time between sfx
        step_tic = sys.gametic + 9
    else
        step_tic = sys.gametic + 12
    end
end

-- End footsteps code
--***********************

function new_game()
    if (hud.custom_stbar) then
        custom_stbar_average_color = hud.get_average_color("STBAR")
        custom_stbar_darkest_color = hud.get_darkest_color("STBAR")
        custom_stbar_lightest_color = hud.get_lightest_color("STBAR")
    end
end

function load_game()
    if (hud.custom_stbar) then
        custom_stbar_average_color = hud.get_average_color("STBAR")
        custom_stbar_darkest_color = hud.get_darkest_color("STBAR")
        custom_stbar_lightest_color = hud.get_lightest_color("STBAR")
    end
end

function save_game() end

function begin_level()
    face_tic = sys.gametic
    step_tic = sys.gametic
    face_state = 6 -- Idle face
end

function end_level() end

function draw_all()
    hud.coord_sys(320, 200)

    if (hud.check_automap()) then
        doom_automap()
        return
    end

    -- there are four standard HUDs:
    -- the first two have different styles for the widescreen status bar extenders
    -- the third is the fullscreen hud
    -- the fourth is "hudless", i.e. you can only see your weapon

    local which = hud.which_hud() % 4

    if (which == 0) then
        hud.universal_y_adjust = -16
        hud.render_world(0, 0, 320, 200 - 32)
        doom_status_bar()
    elseif (which == 1) then
        hud.universal_y_adjust = -16
        hud.render_world(0, 0, 320, 200 - 32)
        doom_status_bar2()
    elseif (which == 2) then
        hud.universal_y_adjust = 0
        hud.render_world(0, 0, 320, 200)
        new_overlay_status()
    else
        hud.universal_y_adjust = 0
        hud.render_world(0, 0, 320, 200)
    end
    edge_air_bar()
    edge_time_bar()
    edge_footsteps()
end
