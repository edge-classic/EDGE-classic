//------------------------------------------
//  DOOM HUD CODE for EDGE
//  Copyright (c) 2009-2023 The Edge community
//  Copyright (C) 1993-1996 by id Software, Inc.
//  Under the GNU General Public License
//------------------------------------------

//var inventory_handler : vector // First value is inventory left, second is inventory use, third is inventory next

function heretic_life_gem() =
{
	var TopX = 25
	var TopY = 190
	var BarHeight = 8
	var BarLength = 275
	var BarValue = 0
	
    if (player.health() <= 0)
        return

	BarValue = math.floor(player.health())
	BarValue = BarValue *(BarLength / 100)
	
	if (BarValue > 1)
	{
		hud.stretch_image(BarValue - 1, TopY, 21, 8, "LIFEGEM2")
	}
	
	
	//Godmode cheat active
	if (player.power_left(player.INVULN) == 0 && player.has_power(player.INVULN)) 
	{
		hud.draw_image(16, 167, "GOD1", 1)
		hud.draw_image(288, 167, "GOD2", 1)
	}

	hud.draw_image(0, 190, "LTFACE", 1)
	hud.draw_image(293, 190, "RTFACE", 1)
}



// Full-Screen Heretic status bar
function heretic_status_bar() =
{
	var AmmoGfx_X = 117
	var AmmoGfx_Y = 174
	
	var AmmoGfx_W = 13
	var AmmoGfx_H = 13
	
	//hud.stretch_image(x, y, w, h, name)
	hud.stretch_image( -128, 158, 75, 42, "STBARL") // Widescreen border
    hud.stretch_image(372, 158, 75, 42, "STBARR") // Widescreen border
	hud.draw_image(-53, 158, "STBARL") // Widescreen border
    hud.draw_image(320, 158, "STBARR") // Widescreen border
	
	//order is important because some of them overlap
	hud.draw_image(0, 158, "BARBACK")
	hud.draw_image(34, 160, "LIFEBAR")
	hud.draw_image(-1, 190, "CHAINBAC")	
	hud.draw_image(0, 190, "CHAIN")	
	hud.draw_image(0, 190, "LTFACE")
	hud.draw_image(293, 190, "RTFACE", 1)
	hud.draw_image(0, 148, "LTFCTOP")
	hud.draw_image(290, 148, "RTFCTOP")
	
	
	hud.text_font("HERETIC_DIGIT")
	
	//Health
	hud.draw_num2(85, 170, 9, player.health())
	
	//Armour
	hud.draw_num2(252, 170, 3, player.total_armor())
	
	//Keys
	if (player.has_key(2))
		hud.stretch_image(153, 164, 10, 6, "ykeyicon", 1)
	
	if (player.has_key(4))
		hud.stretch_image(153, 172, 10, 6, "gkeyicon", 1)
	
	if (player.has_key(1))
		hud.stretch_image(153, 180, 10, 6, "bkeyicon", 1)
	
	if(player.ammo_type(1) > 0)//only if weapon uses ammo
	{
		//Ammo quantity
		hud.draw_num2( 135, 162, 3, player.main_ammo(1))
	}
	
	//hud.stretch_image(x, y, w, h, name)
	//Ammo picture
	if (player.cur_weapon() == "ELF_WAND")
		hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMGLD", 1)
	
	if (player.cur_weapon() == "CROSSBOW")
		hud.stretch_image(AmmoGfx_X - 4, AmmoGfx_Y + 5, AmmoGfx_W + 5, AmmoGfx_H - 10, "INAMBOW", 1)
	
	if (player.cur_weapon() == "DRAGON_CLAW")
		hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMBST", 1)
	
	if (player.cur_weapon() == "PHOENIX_ROD")
		hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMPNX", 1)
	
	if (player.cur_weapon() == "HELLSTAFF")
		hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMRAM", 1)
	
	if (player.cur_weapon() == "MACE")
		hud.stretch_image(AmmoGfx_X, AmmoGfx_Y, AmmoGfx_W, AmmoGfx_H, "INAMLOB", 1)
			
}

function heretic_overlay_status() = 
{
    var RelX
	
	hud.text_font("HERETIC_BIG")
	hud.set_scale(1.0)

	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.8
	if (player.has_key(2))
		hud.stretch_image(RelX, 174, 15, 8, "ykeyicon", 1)
	
	if (player.has_key(4))
		hud.stretch_image(RelX, 182, 15, 8, "gkeyicon", 1)
	
	if (player.has_key(1))
		hud.stretch_image(RelX, 190, 15, 8, "bkeyicon", 1)
	
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.01
	hud.stretch_image(RelX, 181, 5, 15, "PTN1A0", 1)
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.05
	hud.draw_number(RelX, 179, 3, player.health(), 0)
	
	 

	if (player.total_armor()>0)
	{
		RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.23
    	hud.draw_number(RelX, 179, 3, player.total_armor(),0)
    	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.18
    	if (player.armor(1))
	    	hud.stretch_image(RelX, 181, 15, 15, "SHLDA0", 1)
	    else if (player.armor(2))
	    	hud.stretch_image(RelX, 181, 15, 15, "SHD2A0", 1)
    }
    
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.88
    if(player.ammo_type(1) > 0)//only show ammo if weapon uses ammo
		hud.draw_number(RelX, 179, 3, player.main_ammo(1), 0)
	
    hud.text_font("HERETIC")
    hud.text_color(hud.NO_COLOR)
	RelX = hud.x_left + (hud.x_right - hud.x_left) * 0.88
	hud.draw_text(RelX, 170, "AMMO")

}

function heretic_automap() =
{
	hud.automap_color(hud.AM_GRID, '84 59 23') // sectors
	hud.automap_color(hud.AM_ALLMAP, '96 96 96') // walls
	hud.automap_color(hud.AM_WALLS, '84 59 23') // sectors
	hud.automap_color(hud.AM_STEP, '208 176 133') // walls
	hud.automap_color(hud.AM_LEDGE, '208 176 133') // sectors
	hud.automap_color(hud.AM_CEIL, '84 59 23') // sectors
	hud.automap_color(hud.AM_PLAYER, '252 252 252')
	hud.automap_color(hud.AM_MONSTER, '33 33 33')
	hud.automap_color(hud.AM_CORPSE, '33 33 33')
	hud.automap_color(hud.AM_ITEM, '33 33 33')
	hud.automap_color(hud.AM_MISSILE, '33 33 33')
	hud.automap_color(hud.AM_SECRET, '33 33 33')
	hud.automap_color(hud.AM_SCENERY, '33 33 33')
    hud.render_automap(0, 0, 320, 200)


    hud.text_font("HERETIC")
    hud.text_color(hud.NO_COLOR)
    
    
    var which = hud.which_hud() % 3
    
//    if (which == 0)  //heretic_status_bar()
//    {    
//		hud.draw_text(30, 148, hud.map_title())
//		heretic_status_bar()
//		heretic_life_gem()
//	}
	if (which == 0)  //heretic_status_bar()
    {    
		hud.draw_text(30, 148, hud.map_title())
		hud.set_scale(0.75)
		hud.draw_text(10, 10, "Kills:    " + player.kills() + "/" + player.map_enemies())
		if (player.map_secrets() > 0)
		{
			hud.draw_text(10, 20, "Secrets: " + player.secrets() + "/" + player.map_secrets())
		}
		if (player.map_items() > 0)
		{
			hud.draw_text(10, 30, "Items:    " + player.items() + "/" + player.map_items())
		}
		hud.set_scale(1.0)
		heretic_status_bar()
		heretic_life_gem()
	}
    if (which == 1) //heretic_overlay_status()
    {
    	hud.draw_text(0, 155, hud.map_title())
    	hud.set_scale(0.75)
		hud.draw_text(10, 10, "Kills:    " + player.kills() + "/" + player.map_enemies())
		if (player.map_secrets() > 0)
		{
			hud.draw_text(10, 20, "Secrets: " + player.secrets() + "/" + player.map_secrets())
		}
		if (player.map_items() > 0)
		{
			hud.draw_text(10, 30, "Items:    " + player.items() + "/" + player.map_items())
		}
		heretic_overlay_status()
    }
    
	hud.set_scale(1.0)
}




//************************
//**                    **
//**   Inventory code   **
//**                    **
var TotalInventoryItem = 10 //24
var PreviousInventoryItem  : float
var CurrentInventoryItem  = 1
var spriteName : string
var CurrentDirection = 1
var ShowMulti = 0

function GetInventorySprite(InventoryType : float) : string =
{
	if (InventoryType == 1) return "ARTIPTN2"
	if (InventoryType == 2) return "ARTIINVS"
	if (InventoryType == 3) return "ARTIPWBK"
	if (InventoryType == 4) return "ARTISPHL"
	if (InventoryType == 5) return "ARTISOAR"
	if (InventoryType == 6) return "ARTIINVU"
	if (InventoryType == 7) return "ARTITRCH"
	if (InventoryType == 8) return "ARTIFBMB"
	if (InventoryType == 9) return "ARTIATLP"
	if (InventoryType == 10) return "ARTIEGGC"
	
	return "NULL" //just in case
}


function GetPreviousInventoryItem(CurrentItem : float) : float =
{
	var loopCounter = 1
	var highestFound = 0
	
	if (CurrentItem == 1)
	{
		for (loopCounter = CurrentItem + 1, TotalInventoryItem) 
	    {
	    	if (player.inventory(loopCounter) > 0)
	    	{
	    		highestFound = loopCounter
	    	}
	    }
	}
	else
	{
		for (loopCounter = 1 , CurrentItem - 1) 
	    {
	    	if (player.inventory(loopCounter) > 0)
	    	{
	    		highestFound = loopCounter
	    	}
	    }
	    
	    if (highestFound == 0) //found nothing
	    {
	    	for (loopCounter = CurrentItem + 1, TotalInventoryItem) 
		    {
		    	if (player.inventory(loopCounter) > 0)
		    	{
		    		highestFound = loopCounter
		    	}
		    }
	    }
	}

	if (highestFound == 0) highestFound = 1
	
	return highestFound
}

function GetNextInventoryItem(CurrentItem : float) : float =
{
	
	var loopCounter = 1
	
	for (loopCounter = CurrentItem + 1, TotalInventoryItem) 
    {
    	if (player.inventory(loopCounter) > 0)
    	return loopCounter
    }
    //if we're here we didn't find it, so try from the start
    for (loopCounter = 1 , CurrentItem - 1) 
    {
    	if (player.inventory(loopCounter) > 0)
    	return loopCounter
    }
	    
	//if we're here then we have no inventory items
	return 1
}

//This one shows just one item at a time and autofinds the next
//available item
function InventoryScreenOneItem() =
{
	hud.set_alpha(1.0)
	hud.text_font("HERETIC")

	var spacing = 0    //space between each gfx
	var invGfx_X = 183 //140  
	var invGfx_Y = 164 //195 //42
	var invGfx_H = 20 //10
	var invGfx_W = 20 //10
	var calculatedX = 0
	
	var which = hud.which_hud() % 3
	if (which == 1)
	{
		//calculatedX = invGfx_X + 5
		calculatedX = hud.x_left + (hud.x_right - hud.x_left) * 0.7
		invGfx_Y = 175
	}
	else
	{
		calculatedX = invGfx_X
	}
	
	
	//if we press left or right then activate multi-item menu to show full menu.
	//if we press enter then USE current item selected
    if (player.inv_prev_key() == 1 && ShowMulti == 0)//left
	{
		ShowMulti = 1
	}
	else 
	if (player.inv_next_key() == 1 && ShowMulti == 0)//right
	{
		ShowMulti = 1
	}
	else 
	if (player.inv_use_key() == 1 && ShowMulti == 0)//use
	{
		if ((CurrentInventoryItem == 1) && (player.health() > 99)) return

		if ((CurrentInventoryItem == 4) && (player.health() > 99)) return
		
		if (player.inventory(CurrentInventoryItem) < 1) return
		
		var InventoryScriptName : string
		if (CurrentInventoryItem < 10)
			InventoryScriptName = "INVENTORY0" + CurrentInventoryItem
		else
			InventoryScriptName = "INVENTORY" + CurrentInventoryItem
		
		if (hud.rts_isactive(InventoryScriptName) == 1) return
			
		player.use_inventory(CurrentInventoryItem)
		hud.play_sound("ARTIUSE")
		hud.stretch_image(calculatedX, invGfx_Y - 10, invGfx_H, invGfx_W, "USEARTIA", 1)
	}
	
	if (player.inventory(CurrentInventoryItem) > 0)
	{
		spriteName = GetInventorySprite(CurrentInventoryItem)
		hud.stretch_image(calculatedX, invGfx_Y, invGfx_H, invGfx_W, spriteName, 1)
		hud.set_scale(0.5)
	    hud.draw_text(calculatedX, invGfx_Y - 2,"" + player.inventory(CurrentInventoryItem))// + "/" + player.inventorymax(CurrentInventoryItem))
	    hud.set_scale(1.0)
	}   
	else
	{
		CurrentInventoryItem = GetNextInventoryItem(CurrentInventoryItem)
    }
}
	

function InventoryScreenMultiItem() =
{
	hud.set_alpha(1.0)
	hud.text_font("HERETIC")

	var loopCounter
	
	var spacing = 20    //space between each gfx
	var invGfx_X = 0 
	var invGfx_Y = 138
	var calculatedX = 0
	
	var invGfx_H = 20 
	var invGfx_W = 20 
	var tempX = 0
	

	//**********
	//Center our item menu on the screen. If we want to set a specific X start,
	//then comment out this code and set invGfx_X to where we want.
	tempX = (invGfx_W * TotalInventoryItem) / 2
	tempX =  (160 - tempX)
	invGfx_X =  tempX - invGfx_W
	//**********

	if (ShowMulti == 0) //Multi Item menu not being shown so get out of here
	{
		return
	}
	
	for (loopCounter = 1, TotalInventoryItem) //show all items
    {
    	spriteName = GetInventorySprite(loopCounter)
    	
    	calculatedX = invGfx_X + (loopCounter * spacing)
	    if (player.inventory(loopCounter) > 0)
    	{	
	    	hud.stretch_image(calculatedX, invGfx_Y, invGfx_H, invGfx_W, "ARTIBOX", 1)
	    	hud.stretch_image(calculatedX + 2, invGfx_Y + 2, invGfx_H - 5, invGfx_W - 5, spriteName, 1)
	    	
	    	hud.set_scale(0.4)
		    hud.draw_text(calculatedX + 1, invGfx_Y +1,"" + player.inventory(loopCounter))// + "/" + player.inventorymax(loopCounter))
	    	hud.set_scale(1.0)
	    }
	    else
	    {
	    	hud.stretch_image(calculatedX, invGfx_Y, invGfx_H, invGfx_W, "ARTIBOX", 1)
    	}
    	
		if (CurrentInventoryItem == loopCounter)//highlight the chose one
		{
			//hud.thin_box(calculatedX, invGfx_Y, invGfx_H, invGfx_W, hud.GREEN)
			hud.stretch_image(calculatedX, invGfx_Y, invGfx_H - 1, invGfx_W - 1, "SELECTBO", 1)
		}
		
    }
    
    hud.set_scale(1.0)
	if (player.inv_prev_key() == 1 && ShowMulti == 1) //left
	{
		CurrentInventoryItem = GetPreviousInventoryItem(CurrentInventoryItem)
		return
	}
	else if (player.inv_next_key() == 1 && ShowMulti == 1) //right
	{
		CurrentInventoryItem = GetNextInventoryItem(CurrentInventoryItem)
		return
	}
	else if (player.inv_use_key() == 1)//use
	{
		ShowMulti = 0
		return
	}
	
}	
	
//**                    **
//**   Inventory code   **
//**                    **
//************************


function begin_level() =
{
//	player.set_who(0)//need to hook our player mobj first
//	
//	if (player.inventory(1) > 0)
//		hud.rts_enable("REMOVEFLASKS")
}


//************************
//**                    **
//**   Tome of power    **
//**                    **
var TomeActivated = 0

function handle_tome_of_power() =
{
	if (player.has_power(player.BERSERK))//tome of power
	{
		if (TomeActivated == 0) 
		{
			TomeActivated = 1

			hud.rts_enable("SWITCH_TOMED_STAFF")
			hud.rts_enable("SWITCH_TOMED_ELF_WAND")
			hud.rts_enable("SWITCH_TOMED_CROSSBOW")
			hud.rts_enable("SWITCH_TOMED_GAUNTLETS")
			hud.rts_enable("SWITCH_TOMED_HELLSTAFF")
			hud.rts_enable("SWITCH_TOMED_PHOENIX_ROD")
			hud.rts_enable("SWITCH_TOMED_DRAGON_CLAW")
			hud.rts_enable("SWITCH_TOMED_MACE")
		}
		
		hud.stretch_image(305, 20, 20, 20, "SPINBK0")
	}
	
	if (player.has_power(player.BERSERK) < 1 && TomeActivated == 1)
	{
		TomeActivated = 0
		
		hud.rts_enable("SWITCH_STAFF")
		hud.rts_enable("SWITCH_ELF_WAND")
		hud.rts_enable("SWITCH_CROSSBOW")
		hud.rts_enable("SWITCH_GAUNTLETS")
		hud.rts_enable("SWITCH_HELLSTAFF")
		hud.rts_enable("SWITCH_PHOENIX_ROD")
		hud.rts_enable("SWITCH_DRAGON_CLAW")
		hud.rts_enable("SWITCH_MACE")
	}
	
}

//**                    **
//**   Tome of power    **
//**                    **
//************************

function draw_all() =
{
    hud.coord_sys(320, 200) //Didn't Heretic draw at 320x240?
    hud.grab_times()

	if (hud.check_automap())
	{
		heretic_automap()
		return
	}
	
	// there are three standard HUDs:
	// the first is the usual Heretic status bar
	// the second is the fullscreen hud
	// the third is "hudless", i.e. you can only see your weapon
	
    var which = hud.which_hud() % 3

	if (which == 0)
	{	
		hud.render_world(0, 0, 320, 200 - 32)
		heretic_status_bar()
    	heretic_life_gem()
	}
	else if (which == 1)
	{
		hud.render_world(0, 0, 320, 200)
		heretic_overlay_status()
	}
	else
	{
		hud.render_world(0, 0, 320, 200)
	} 
	
	
	//inventory_handler = player.inventory_events()
	InventoryScreenOneItem()
	InventoryScreenMultiItem()
	
    if (player.has_power(player.JET_PACK))//wings
    	hud.stretch_image(280, 20, 20, 20, "SPFLY0")
	
	
	handle_tome_of_power() //our special routine
	
    edge_air_bar()
    edge_footsteps()

}

  
 
 
 
 
 
 
 
 
 
