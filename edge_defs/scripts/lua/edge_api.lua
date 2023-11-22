------------------------------------------------------------------------------
-- EDGE CLASSIC LUA API
------------------------------------------------------------------------------

------------------------------------------------------------------------------
-- SYSTEM
------------------------------------------------------------------------------

sys.TICRATE            = 35
sys.gametic            = 0

------------------------------------------------------------------------------
-- MATH
------------------------------------------------------------------------------

-- implemented natively, rounds the provided value using the C round function
-- math.rint(value)

-- Lua math.random handles both of these, though here for porting reference
--[[

math.random2           = function()
    return math.random(0, 10)
end

math.rand_range        = function(low, high)
    return math.random(low, high)
end

]]--

------------------------------------------------------------------------------
-- HUD
------------------------------------------------------------------------------

hud.which              = 0
hud.automap            = 0

hud.now_time           = 0
hud.passed_time        = 0
hud.last_time          = -1

hud.custom_stbar       = false
hud.universal_y_adjust = 0

hud.x_left             = 0
hud.x_right            = 320

-- handy colors
hud.NO_COLOR           = vec3(-1, -1, -1)

hud.BLACK              = vec3(0, 0, 0)
hud.WHITE              = vec3(255, 255, 255)
hud.RED                = vec3(255, 0, 0)
hud.LIGHTRED           = vec3(220, 0, 0)
hud.GREEN              = vec3(0, 255, 0)
hud.LIGHTGREEN         = vec3(0, 255, 144)
hud.BLUE               = vec3(0, 0, 220)
hud.LIGHTBLUE          = vec3(0, 0, 255)
hud.YELLOW             = vec3(255, 255, 0)
hud.PURPLE             = vec3(255, 0, 255)
hud.CYAN               = vec3(0, 255, 255)
hud.ORANGE             = vec3(255, 160, 0)
hud.GRAY               = vec3(128, 128, 128)
hud.LIGHTGRAY          = vec3(192, 192, 192)


-- automap options
hud.AM_GRID          = 1 -- also a color
hud.AM_ALLMAP        = 2 -- also a color
hud.AM_WALLS         = 3 -- also a color
hud.AM_THINGS        = 4
hud.AM_FOLLOW        = 5
hud.AM_ROTATE        = 6
hud.AM_HIDELINES     = 7

-- automap colors
hud.AM_STEP          = 4
hud.AM_LEDGE         = 5
hud.AM_CEIL          = 6
hud.AM_SECRET        = 7

hud.AM_PLAYER        = 8
hud.AM_MONSTER       = 9
hud.AM_CORPSE        = 10
hud.AM_ITEM          = 11
hud.AM_MISSILE       = 12
hud.AM_SCENERY       = 13

hud.AM_ARROW_DOOM    = 0
hud.AM_ARROW_HERETIC = 1

hud.grab_times       = function()
    hud.now_time = hud.get_time()
    hud.passed_time = 0

    if hud.last_time > 0 and hud.last_time <= hud.now_time then
        hud.passed_time = math.min(hud.now_time - hud.last_time, sys.TICRATE)
    end

    hud.last_time = hud.now_time
end


------------------------------------------------------------------------------
-- PLAYER
------------------------------------------------------------------------------

player.inventory_event_handler = vec3(0, 0, 0)

-- ammo
player.BULLETS                 = 1
player.SHELLS                  = 2
player.ROCKETS                 = 3
player.CELLS                   = 4
player.PELLETS                 = 5
player.NAILS                   = 6
player.GRENADES                = 7
player.GAS                     = 8

-- armors
player.GREEN_ARMOR             = 1
player.BLUE_ARMOR              = 2
player.PURPLE_ARMOR            = 3
player.YELLOW_ARMOR            = 4
player.RED_ARMOR               = 5

-- powerups
player.INVULN                  = 1
player.BERSERK                 = 2
player.INVIS                   = 3
player.ACID_SUIT               = 4
player.AUTOMAP                 = 5
player.GOGGLES                 = 6
player.JET_PACK                = 7
player.NIGHT_VIS               = 8
player.SCUBA                   = 9
player.STOP_TIME               = 10

-- keys
player.BLUE_CARD               = 1
player.YELLOW_CARD             = 2
player.RED_CARD                = 3
player.GREEN_CARD              = 4
player.BLUE_SKULL              = 5
player.YELLOW_SKULL            = 6
player.RED_SKULL               = 7
player.GREEN_SKULL             = 8

player.GOLD_KEY                = 9
player.SILVER_KEY              = 10
player.BRASS_KEY               = 11
player.COPPER_KEY              = 12
player.STEEL_KEY               = 13
player.WOODEN_KEY              = 14
player.FIRE_KEY                = 15
player.WATER_KEY               = 16

-- counters
player.LIVES                   = 1
player.SCORE                   = 2
player.MONEY                   = 3
player.EXPERIENCE              = 4


player.inv_prev_key      = function()
    return player.inventory_event_handler.x
end

player.inv_use_key       = function()
    return player.inventory_event_handler.y
end

player.inv_next_key      = function()
    return player.inventory_event_handler.z
end

------------------------------------------------------------------------------
-- MAPOBJECT
------------------------------------------------------------------------------

-- whatinfo
mapobject.NAME           = 1
mapobject.CURRENT_HEALTH = 2
mapobject.SPAWN_HEALTH   = 3
mapobject.PICKUP_BENEFIT = 4
mapobject.KILL_BENEFIT   = 5

------------------------------------------------------------------------------
-- BENEFIT
------------------------------------------------------------------------------

benefit                  = {}

-- BENEFIT GROUP
benefit.HEALTH           = "HEALTH"
benefit.AMMO             = "AMMO"
benefit.ARMOUR           = "ARMOUR"
benefit.KEY              = "KEY"
benefit.POWERUP          = "POWERUP"
benefit.COUNTER          = "COUNTER"
benefit.INVENTORY        = "INVENTORY"
benefit.WEAPON           = "WEAPON"

-- parse our benefit string to get the type
benefit.get_type         = function(TheString, BenefitName)
    local tempstr = ""
    local temppos = 0
    local equalpos = 0

    temppos = string.find(TheString, BenefitName)
    equalpos = string.find(TheString, "=")
    if (temppos) then
        if (equalpos) then --a XXXX99=999 kind of benefit i.e. AMMO, ARMOUR
            temppos = temppos + 1
            temppos = temppos + #BenefitName
            tempstr = string.sub(TheString, temppos, equalpos)
        elseif (not equalpos) then --a XXXX99 kind of benefit i.e. KEY
            temppos = temppos + 1
            temppos = temppos + #BenefitName
            tempstr = string.sub(TheString, temppos, #TheString)
        end
    end

    return math.tointeger(tempstr)
end

-- parse our benefit string to get the amount
benefit.get_amount       = function(TheString)
    local tempstr = ""
    local equalpos = 0 --position of "="

    equalpos = string.find(TheString, "=")
    if (equalpos) then --its a XXXX99=999 kind of benefit i.e. AMMO, ARMOUR
        equalpos = equalpos + 2
        tempstr = string.sub(TheString, equalpos, #TheString)
    else
        tempstr = "1"
    end

    return math.tointeger(tempstr)
end

--Note this only returns the first benefit, which is usually
-- enough: the only item which is multi-benefit is the backpack.
benefit.get_group        = function(BenefitFull)
    local BenefitType = 0

    local tempbenefitgroup = ""

    for loopCounter = 1, 8 do
        if (loopCounter == 1) then tempbenefitgroup = benefit.AMMO end
        if (loopCounter == 2) then tempbenefitgroup = benefit.ARMOUR end
        if (loopCounter == 3) then tempbenefitgroup = benefit.COUNTER end
        if (loopCounter == 4) then tempbenefitgroup = benefit.INVENTORY end
        if (loopCounter == 5) then tempbenefitgroup = benefit.KEY end
        if (loopCounter == 6) then tempbenefitgroup = benefit.POWERUP end
        if (loopCounter == 7) then tempbenefitgroup = benefit.HEALTH end
        if (loopCounter == 8) then tempbenefitgroup = benefit.WEAPON end

        BenefitType = get_type(BenefitFull, tempbenefitgroup)

        if (BenefitType > 0) then
            break
        end
    end
    return tempbenefitgroup
end
