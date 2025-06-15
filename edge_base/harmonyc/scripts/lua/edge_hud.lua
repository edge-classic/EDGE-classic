--------------------------------------------
--  DOOM HUD CODE for EDGE
--  Copyright (C) 2023 the EDGE-Classic Community
--  Copyright (c) 2009-2010 The Edge Team
--  Under the GNU General Public License
--------------------------------------------

function new_overlay_status()
    local RelX = 0
    local AbsY = 0
    local YOffset = 0


    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.17
    AbsY = 166
    YOffset = 8

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
        hud.text_color(hud.LIGHTRED)
    end

    if (player.health() > 34) then
        hud.text_color(hud.NO_COLOR)
    end


    hud.set_scale(0.75)

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.065
    hud.draw_number(RelX, 180, 3, player.health(), 0)
    hud.text_color(hud.NO_COLOR)

    hud.set_scale(1.0)

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.01
    if (player.total_armor() > 0) then

        if (player.armor(player.GREEN_ARMOR) > 0) then
            hud.stretch_image(RelX, 165, 16, 10, "ARM1A0", 1)
        end

        if (player.armor(player.BLUE_ARMOR) > 0) then
            hud.stretch_image(RelX, 165, 16, 10, "ARM2A0", 1)
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