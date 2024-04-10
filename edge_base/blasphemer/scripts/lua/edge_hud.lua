--------------------------------------------
--  DOOM HUD CODE for EDGE
--  Copyright (c) 2009-2023 The Edge community
--  Copyright (C) 1993-1996 by id Software, Inc.
--  Under the GNU General Public License
--------------------------------------------

--local inventory_handler : vector -- First value is inventory left, second is inventory use, third is inventory next

-- Startup stuff
function new_game()
    if (hud.custom_stbar) then
        custom_stbar_average_color = hud.get_average_color("STBAR")
        custom_stbar_darkest_color = hud.get_darkest_color("STBAR")
        custom_stbar_lightest_color = hud.get_lightest_color("STBAR")
    end
    hud.automap_player_arrow(hud.AM_ARROW_HERETIC)
end

function load_game()
    if (hud.custom_stbar) then
        custom_stbar_average_color = hud.get_average_color("STBAR")
        custom_stbar_darkest_color = hud.get_darkest_color("STBAR")
        custom_stbar_lightest_color = hud.get_lightest_color("STBAR")
    end
    hud.automap_player_arrow(hud.AM_ARROW_HERETIC)
end

function heretic_life_gem()
    local TopX = 25
    local TopY = 190
    local BarHeight = 8
    local BarLength = 275
    local BarValue = 0

    if (player.health() <= 0) then
        return
    end

    BarValue = math.floor(player.health())
    BarValue = BarValue * (BarLength / 100)

    if (BarValue > 1) then
        hud.stretch_image(BarValue - 1, TopY, 21, 8, "LIFEGEM2")
    end

    --Godmode cheat active
    if (player.power_left(player.INVULN) == 0 and player.has_power(player.INVULN)) then
        hud.draw_image(16, 167, "GOD1")
        hud.draw_image(288, 167, "GOD2")
    end

    hud.draw_image(0, 190, "LTFACE")
    hud.draw_image(276, 190, "RTFACE")
end

-- Full-Screen Heretic status bar
function heretic_status_bar()
    local AmmoGfx_X = 117
    local AmmoGfx_Y = 174

    local AmmoGfx_W = 13
    local AmmoGfx_H = 13

    local centerOffsetX = 0
    local tempwidth = 0

    tempwidth = hud.get_image_width("BARBACK")
    centerOffsetX = tempwidth / 2;

    hud.draw_image(-53, 158, "STBARL") -- Widescreen border
    hud.draw_image(320, 158, "STBARR") -- Widescreen border

    --order is important because some of them overlap
    --hud.draw_image(0, 158, "BARBACK")
    hud.draw_image(160 - centerOffsetX, 158, "BARBACK", 1)
    hud.draw_image(34, 160, "LIFEBAR")
    hud.draw_image(-1, 190, "CHAINBAC")
    hud.draw_image(0, 190, "CHAIN")
    hud.draw_image(0, 190, "LTFACE")
    hud.draw_image(276, 190, "RTFACE")
    hud.draw_image(0, 148, "LTFCTOP")
    hud.draw_image(290, 148, "RTFCTOP")

    hud.text_font("HERETIC_DIGIT")

    --Health
    hud.draw_num2(85, 170, 9, player.health())

    --Armour
    hud.draw_num2(252, 170, 3, player.total_armor())

    --Keys
    if (player.has_key(2)) then
        hud.stretch_image(153, 164, 10, 6, "ykeyicon", 1)
    end

    if (player.has_key(4)) then
        hud.stretch_image(153, 172, 10, 6, "gkeyicon", 1)
    end

    if (player.has_key(1)) then
        hud.stretch_image(153, 180, 10, 6, "bkeyicon", 1)
    end

    if (player.ammo_type(1) > 0) then --only if weapon uses ammo
        --Ammo quantity
        hud.draw_num2(135, 162, 3, player.main_ammo(1))
    end

    --hud.stretch_image_nooffsets(x, y, w, h, name)
    --Ammo picture for normal weapons
    if (player.cur_weapon() == "ELF_WAND") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMGLD", 1)
    end

    if (player.cur_weapon() == "CROSSBOW") then
        hud.stretch_image(AmmoGfx_X - 4, AmmoGfx_Y + 5, AmmoGfx_W + 5, AmmoGfx_H - 10, "INAMBOW", 1)
    end

    if (player.cur_weapon() == "DRAGON_CLAW") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMBST", 1)
    end

    if (player.cur_weapon() == "PHOENIX_ROD") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMPNX", 1)
    end

    if (player.cur_weapon() == "HELLSTAFF") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMRAM", 1)
    end

    if (player.cur_weapon() == "MACE") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMLOB", 1)
    end

    --Ammo picture for tomed weapons
    if (player.cur_weapon() == "ELF_WAND_TOMED") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMGLD", 1)
    end

    if (player.cur_weapon() == "CROSSBOW_TOMED") then
        hud.stretch_image(AmmoGfx_X - 4, AmmoGfx_Y + 5, AmmoGfx_W + 5, AmmoGfx_H - 10, "INAMBOW", 1)
    end

    if (player.cur_weapon() == "DRAGON_CLAW_TOMED") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMBST", 1)
    end

    if (player.cur_weapon() == "PHOENIX_ROD_TOMED") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMPNX", 1)
    end

    if (player.cur_weapon() == "HELLSTAFF_TOMED") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMRAM", 1)
    end

    if (player.cur_weapon() == "MACE_TOMED") then
        hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMLOB", 1)
    end
end

function heretic_overlay_status()
    local RelX

    hud.text_font("HERETIC_BIG")
    hud.set_scale(1.0)

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.8
    if (player.has_key(2)) then
        hud.stretch_image(RelX, 174, 15, 8, "ykeyicon", 1)
    end

    if (player.has_key(4)) then
        hud.stretch_image(RelX, 182, 15, 8, "gkeyicon", 1)
    end

    if (player.has_key(1)) then
        hud.stretch_image(RelX, 190, 15, 8, "bkeyicon", 1)
    end

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.01
    hud.stretch_image(RelX, 181, 5, 15, "PTN1A0", 1)
    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.05
    hud.draw_number(RelX, 179, 3, player.health(), 0)

    if (player.total_armor() > 0) then
        RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.23
        hud.draw_number(RelX, 179, 3, player.total_armor(), 0)
        RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.18
        if (player.armor(1) > 0) then
            hud.stretch_image(RelX, 181, 15, 15, "SHLDA0", 1)
        elseif (player.armor(2) > 0) then
            hud.stretch_image(RelX, 181, 15, 15, "SHD2A0", 1)
        end
    end

    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.88
    if (player.ammo_type(1) > 0) then --only show ammo if weapon uses ammo
        hud.draw_number(RelX, 179, 3, player.main_ammo(1), 0)
    end

    hud.text_font("HERETIC")
    hud.text_color(hud.NO_COLOR)
    RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.88
    hud.draw_text(RelX, 170, "AMMO")
end

function heretic_automap()
    -- fixme, this is allocating all  these vec3's per frame
    hud.automap_color(hud.AM_GRID, vec3(84, 59, 23))     -- sectors
    hud.automap_color(hud.AM_ALLMAP, vec3(96, 96, 96))   -- walls
    hud.automap_color(hud.AM_WALLS, vec3(84, 59, 23))    -- sectors
    hud.automap_color(hud.AM_STEP, vec3(208, 176, 133))  -- walls
    hud.automap_color(hud.AM_LEDGE, vec3(208, 176, 133)) -- sectors
    hud.automap_color(hud.AM_CEIL, vec3(84, 59, 23))     -- sectors
    hud.automap_color(hud.AM_PLAYER, vec3(252, 252, 252))
    hud.automap_color(hud.AM_MONSTER, vec3(33, 33, 33))
    hud.automap_color(hud.AM_CORPSE, vec3(33, 33, 33))
    hud.automap_color(hud.AM_ITEM, vec3(33, 33, 33))
    hud.automap_color(hud.AM_MISSILE, vec3(33, 33, 33))
    hud.automap_color(hud.AM_SECRET, vec3(33, 33, 33))
    hud.automap_color(hud.AM_SCENERY, vec3(33, 33, 33))
    hud.render_automap(0, 0, 320, 200)

    hud.text_font("HERETIC")
    hud.text_color(hud.NO_COLOR)

    local which = hud.which_hud() % 3

    --    if (which == 0)  --heretic_status_bar()
    --    {
    --		hud.draw_text(30, 148, hud.map_title())
    --		heretic_status_bar()
    --		heretic_life_gem()
    --	}
	local TempMapName = hud.map_title()
    
    if (DoesNameStartWith(hud.map_title(),hud.map_name()) == 0) then
    	TempMapName = hud.map_name() .. ": " ..  hud.map_title()
    end
	
    if (which == 0) then --heretic_status_bar()
        hud.draw_text(30, 148, TempMapName)
        hud.set_scale(0.75)
        hud.draw_text(10, 10, "Kills:    " .. player.kills() .. "/" .. player.map_enemies())
        if (player.map_secrets() > 0) then
            hud.draw_text(10, 20, "Secrets: " .. player.secrets() .. "/" .. player.map_secrets())
        end
        if (player.map_items() > 0) then
            hud.draw_text(10, 30, "Items:    " .. player.items() .. "/" .. player.map_items())
        end
        hud.set_scale(1.0)
        heretic_status_bar()
        heretic_life_gem()
    end
    if (which == 1) then --heretic_overlay_status()
        hud.draw_text(0, 155, TempMapName)
        hud.set_scale(0.75)
        hud.draw_text(10, 10, "Kills:    " .. player.kills() .. "/" .. player.map_enemies())
        if (player.map_secrets() > 0) then
            hud.draw_text(10, 20, "Secrets: " .. player.secrets() .. "/" .. player.map_secrets())
        end
        if (player.map_items() > 0) then
            hud.draw_text(10, 30, "Items:    " .. player.items() .. "/" .. player.map_items())
        end
        heretic_overlay_status()
    end

    hud.set_scale(1.0)
end

--************************
--**                    **
--**   Inventory code   **
--**                    **
local TotalInventoryItem    = 10 --24
local PreviousInventoryItem = 0
local CurrentInventoryItem  = 1
local spriteName            = ""
local CurrentDirection      = 1
local ShowMulti             = 0

function GetInventorySprite(InventoryType)
    if (InventoryType == 1) then return "ARTIPTN2" end
    if (InventoryType == 2) then return "ARTIINVS" end
    if (InventoryType == 3) then return "ARTIPWBK" end
    if (InventoryType == 4) then return "ARTISPHL" end
    if (InventoryType == 5) then return "ARTISOAR" end
    if (InventoryType == 6) then return "ARTIINVU" end
    if (InventoryType == 7) then return "ARTITRCH" end
    if (InventoryType == 8) then return "ARTIFBMB" end
    if (InventoryType == 9) then return "ARTIATLP" end
    if (InventoryType == 10) then return "ARTIEGGC" end

    return "NULL" --just in case
end

function GetPreviousInventoryItem(CurrentItem)
    local loopCounter = 1
    local highestFound = 0

    if (CurrentItem == 1) then
        for loopCounter = CurrentItem + 1, TotalInventoryItem do
            if (player.inventory(loopCounter) > 0) then
                highestFound = loopCounter
            end
        end
    else
        for loopCounter = 1, CurrentItem - 1 do
            if (player.inventory(loopCounter) > 0) then
                highestFound = loopCounter
            end
        end

        if (highestFound == 0) then --found nothing
            for loopCounter = CurrentItem + 1, TotalInventoryItem do
                if (player.inventory(loopCounter) > 0) then
                    highestFound = loopCounter
                end
            end
        end
    end

    if (highestFound == 0) then highestFound = 1 end

    return highestFound
end

function GetNextInventoryItem(CurrentItem)
    for loopCounter = CurrentItem + 1, TotalInventoryItem do
        if (player.inventory(loopCounter) > 0) then
            return loopCounter
        end
    end
    --if we're here we didn't find it, so try from the start
    for loopCounter = 1, CurrentItem - 1, 1 do
        if (player.inventory(loopCounter) > 0) then
            return loopCounter
        end
    end

    --if we're here then we have no inventory items
    return 1
end

--This one shows just one item at a time and autofinds the next
--available item
function InventoryScreenOneItem()
    hud.set_alpha(1.0)
    hud.text_font("HERETIC")

    local spacing = 0    --space between each gfx
    local invGfx_X = 183 --140
    local invGfx_Y = 164 --195 --42
    local invGfx_H = 20  --10
    local invGfx_W = 20  --10
    local calculatedX = 0

    local which = hud.which_hud() % 3
    if (which == 1) then
        --calculatedX = invGfx_X + 5
        calculatedX = hud.x_left + (hud.x_right - hud.x_left) * 0.7
        invGfx_Y = 175
    else
        calculatedX = invGfx_X
    end


    --if we press left or right then activate multi-item menu to show full menu.
    --if we press enter then USE current item selected
    if (player.inv_prev_key() == 1 and ShowMulti == 0) then     --left
        ShowMulti = 1
    elseif (player.inv_next_key() == 1 and ShowMulti == 0) then --right	
        ShowMulti = 1
    elseif (player.inv_use_key() == 1 and ShowMulti == 0) then  --use
        if ((CurrentInventoryItem == 1) and (player.health() > 99)) then return end

        if ((CurrentInventoryItem == 4) and (player.health() > 99)) then return end

        if (player.inventory(CurrentInventoryItem) < 1) then return end

        local InventoryScriptName = ""
        if (CurrentInventoryItem < 10) then
            InventoryScriptName = "INVENTORY0" .. CurrentInventoryItem
        else
            InventoryScriptName = "INVENTORY" .. CurrentInventoryItem
        end

        if (hud.rts_isactive(InventoryScriptName) == 1) then return end

        player.use_inventory(CurrentInventoryItem)
        hud.play_sound("ARTIUSE")
        hud.stretch_image(calculatedX, invGfx_Y - 10, invGfx_H, invGfx_W, "USEARTIA", 1)
    end

    if (player.inventory(CurrentInventoryItem) > 0) then
        spriteName = GetInventorySprite(CurrentInventoryItem)
        hud.stretch_image(calculatedX, invGfx_Y, invGfx_H, invGfx_W, spriteName, 1)
        hud.set_scale(0.5)
        hud.draw_text(calculatedX, invGfx_Y - 2, tostring(player.inventory(CurrentInventoryItem))) -- + "/" + player.inventorymax(CurrentInventoryItem))
        hud.set_scale(1.0)
    else
        CurrentInventoryItem = GetNextInventoryItem(CurrentInventoryItem)
    end
end

function InventoryScreenMultiItem()
    hud.set_alpha(1.0)
    hud.text_font("HERETIC")

    local spacing = 20 --space between each gfx
    local invGfx_X = 0
    local invGfx_Y = 138
    local calculatedX = 0

    local invGfx_H = 20
    local invGfx_W = 20
    local tempX = 0

    --**********
    --Center our item menu on the screen. If we want to set a specific X start,
    --then comment out this code and set invGfx_X to where we want.
    tempX = (invGfx_W * TotalInventoryItem) / 2
    tempX = (160 - tempX)
    invGfx_X = tempX - invGfx_W
    --**********

    if (ShowMulti == 0) then --Multi Item menu not being shown so get out of here
        return
    end

    for loopCounter = 1, TotalInventoryItem do --show all items
        spriteName = GetInventorySprite(loopCounter)

        calculatedX = invGfx_X + (loopCounter * spacing)
        if (player.inventory(loopCounter) > 0) then
            hud.stretch_image(calculatedX, invGfx_Y, invGfx_H, invGfx_W, "ARTIBOX", 1)
            hud.stretch_image(calculatedX + 2, invGfx_Y + 2, invGfx_H - 5, invGfx_W - 5, spriteName, 1)

            hud.set_scale(0.4)
            hud.draw_text(calculatedX + 1, invGfx_Y + 1, tostring(player.inventory(loopCounter))) -- + "/" + player.inventorymax(loopCounter))
            hud.set_scale(1.0)
        else
            hud.stretch_image(calculatedX, invGfx_Y, invGfx_H, invGfx_W, "ARTIBOX", 1)
        end

        if (CurrentInventoryItem == loopCounter) then --highlight the chose one
            --hud.thin_box(calculatedX, invGfx_Y, invGfx_H, invGfx_W, hud.GREEN)
            hud.stretch_image(calculatedX, invGfx_Y, invGfx_H - 1, invGfx_W - 1, "SELECTBO", 1)
        end
    end

    hud.set_scale(1.0)
    if (player.inv_prev_key() == 1 and ShowMulti == 1) then --left
        CurrentInventoryItem = GetPreviousInventoryItem(CurrentInventoryItem)
        return
    elseif (player.inv_next_key() == 1 and ShowMulti == 1) then --right	
        CurrentInventoryItem = GetNextInventoryItem(CurrentInventoryItem)
        return
    elseif (player.inv_use_key() == 1) then --use
        ShowMulti = 0
        return
    end
end

--**                    **
--**   Inventory code   **
--**                    **
--************************


function begin_level()
    --	player.set_who(0)--need to hook our player mobj first
    --	
    --	if (player.inventory(1) > 0)
    --		hud.rts_enable("REMOVEFLASKS")
end

--************************
--**                    **
--**   Tome of power    **
--**                    **
local TomeActivated = 0

function handle_tome_of_power()
    if (player.has_power(player.BERSERK)) then --tome of power	
        if (TomeActivated == 0) then
            TomeActivated = 1

            hud.rts_enable("SWITCH_TOMED_STAFF")
            hud.rts_enable("SWITCH_TOMED_ELF_WAND")
            hud.rts_enable("SWITCH_TOMED_CROSSBOW")
            hud.rts_enable("SWITCH_TOMED_GAUNTLETS")
            hud.rts_enable("SWITCH_TOMED_HELLSTAFF")
            hud.rts_enable("SWITCH_TOMED_PHOENIX_ROD")
            hud.rts_enable("SWITCH_TOMED_DRAGON_CLAW")
            hud.rts_enable("SWITCH_TOMED_MACE")
        end

        hud.stretch_image(290, 5, 20, 20, "SPINBK0", 1)
    end

    if (not player.has_power(player.BERSERK) and TomeActivated == 1) then
        TomeActivated = 0

        hud.rts_enable("SWITCH_STAFF")
        hud.rts_enable("SWITCH_ELF_WAND")
        hud.rts_enable("SWITCH_CROSSBOW")
        hud.rts_enable("SWITCH_GAUNTLETS")
        hud.rts_enable("SWITCH_HELLSTAFF")
        hud.rts_enable("SWITCH_PHOENIX_ROD")
        hud.rts_enable("SWITCH_DRAGON_CLAW")
        hud.rts_enable("SWITCH_MACE")
    end
end

--**                    **
--**   Tome of power    **
--**                    **
--************************

function draw_all()
    hud.coord_sys(320, 200) --Didn't Heretic draw at 320x240?
    hud.grab_times()

    if (hud.check_automap()) then
        heretic_automap()
        return
    end

    -- there are three standard HUDs:
    -- the first is the usual Heretic status bar
    -- the second is the fullscreen hud
    -- the third is "hudless", i.e. you can only see your weapon

    local which = hud.which_hud() % 3

    if (which == 0) then
        hud.universal_y_adjust = 0
        hud.render_world(0, 0, 320, 200 - 32)
        heretic_status_bar()
        heretic_life_gem()
    elseif (which == 1) then
        hud.universal_y_adjust = 0
        hud.render_world(0, 0, 320, 200)
        heretic_overlay_status()
    else
        hud.universal_y_adjust = 0
        hud.render_world(0, 0, 320, 200)
    end

    --inventory_handler = player.inventory_events()
    InventoryScreenOneItem()
    InventoryScreenMultiItem()

    if (player.has_power(player.JET_PACK)) then --wings
        hud.stretch_image(10, 5, 20, 20, "SPFLY0", 1)
    end


    handle_tome_of_power() --our special routine

    edge_air_bar()
    edge_time_bar()
    edge_footsteps()
end
