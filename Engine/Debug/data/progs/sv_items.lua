-- sv_items.lua

-- ============================================================================
-- 
-- General pick-up item handling
-- 
-- ============================================================================

-- ===================
-- Heal
-- 
-- VERY IMPORTANT: This should be the ONLY function to increase health
-- Will heal entity "idx_other"
-- ===================
function Heal(idx_other, amount)
	if not entities[idx_other].takedamage then
		return
	end

	-- check here too, just to be safe
	if entities[idx_other].health <= 0 then
		return
	end

	entities[idx_other].health = entities[idx_other].health + amount
	-- check here too, just to be safe
	if entities[idx_other].health > entities[idx_other].max_health then
		entities[idx_other].health = entities[idx_other].max_health
	end

	PrintS(idx_other, "You have got " .. amount .. " health\n")
end

-- ===================
-- GiveArmor
-- 
-- Will give more armor to entity "idx_other"
-- ===================
function GiveArmor(idx_other, value)
	if not entities[idx_other].takedamage then
		return
	end

	entities[idx_other].armor = entities[idx_other].armor + value
	if entities[idx_other].armor >= entities[idx_other].max_armor then
		entities[idx_other].armor = entities[idx_other].max_armor
	end

	PrintS(idx_other, "You have got " .. value .. " armor\n")
end

-- ===================
-- GiveAmmo
-- 
-- Will give "amount" ammo of type "ammotype" to entity "idx_other"
-- Returns true if anything was given
-- ===================
function GiveAmmo(idx_other, ammotype, amount)
	if ammotype == -1 then
		return false -- no ammo to give
	end

	if ammotype < 0 or ammotype >= AMMO_MAX_TYPES then
		error("GiveAmmo: ammotype " .. ammotype .. " is invalid\n")
	end

	if entities[idx_other].ammo[ammotype] >= entities[idx_other].ammo_capacity[ammotype] then
		return false
	end

	entities[idx_other].ammo[ammotype] = entities[idx_other].ammo[ammotype] + amount
	entities[idx_other].ammo[ammotype] = math.max(0, entities[idx_other].ammo[ammotype])
	entities[idx_other].ammo[ammotype] = math.min(entities[idx_other].ammo[ammotype], entities[idx_other].ammo_capacity[ammotype])

	PrintS(idx_other, "You have got some ammo\n")
	return true
end

-- ===================
-- GiveWeapon
-- 
-- Will give weapons in weap to entity "idx_other"
-- Returns true if anything was given
-- ===================
function GiveWeapon(idx_other, weap)
	local already_have
	local weap_idx

	if MathPopCount(weap) ~= 1 then
		error("GiveWeapon: weaps should only have one weapon (cnt " .. MathPopCount(weap) .. ", value " .. weap .. ")\n") -- TODO FIXME
	end

	if entities[idx_other].weapons & weap == weap then
		already_have = true
	else
		already_have = false
	end

	weap_idx = WeaponBitToIdx(idx_other, weap)

	if not already_have then
		if gamestate.weapon_info[weap_idx].ammo_capacity > 0 then
			entities[idx_other].weapons_ammo[weap_idx] = gamestate.weapon_info[weap_idx].ammo_pickup_amount
		else
			GiveAmmo(idx_other, WeaponGetAmmoIdx(weap_idx, true), gamestate.weapon_info[weap_idx].ammo_pickup_amount)
		end
	else
		return GiveAmmo(idx_other, WeaponGetAmmoIdx(weap_idx, true), gamestate.weapon_info[weap_idx].ammo_pickup_amount)
	end

	local message
	if weap == WEAPON_PUNCH_BIT then
		error("Punchs are not valid as pickups!\n")
	elseif weap == WEAPON_FALCON2_BIT then
		message = "You have got the Falcon 2\n"
	elseif weap == WEAPON_CROSSBOW_BIT then
		message = "You have got the Crossbow\n"
	elseif weap == WEAPON_SUPERDRAGON_BIT then
		message = "You have got the SuperDragon\n"
	elseif weap == WEAPON_CMP150_BIT then
		message = "You have got the CMP-150\n"
	elseif weap == WEAPON_NBOMB_BIT then
		message = "You have got the N-Bomb\n"
	elseif weap == WEAPON_SLAYER_BIT then
		message = "You have got the Slayer\n"
	elseif weap == WEAPON_ENERGYBALL_BIT then
		message = "You have got the Energy Ball\n"
	elseif weap == WEAPON_AXE_BIT then
		message = "You have got the Axe\n"
	elseif weap == WEAPON_SHOTGUN_BIT then
		message = "You have got the Shotgun\n"
	elseif weap == WEAPON_SUPERSHOTGUN_BIT then
		message = "You have got the Super Shotgun\n"
	elseif weap == WEAPON_NAILGUN_BIT then
		message = "You have got the Nailgun\n"
	elseif weap == WEAPON_SUPERNAILGUN_BIT then
		message = "You have got the Super Nailgun\n"
	elseif weap == WEAPON_GRENADELAUNCHER_BIT then
		message = "You have got the Grenade Launcher\n"
	elseif weap == WEAPON_ROCKETLAUNCHER_BIT then
		message = "You have got the Rocket Launcher\n"
	elseif weap == WEAPON_THUNDERBOLT_BIT then
		message = "You have got the Thunderbolt\n"
	elseif weap == WEAPON_AQ2MK23_BIT then
		message = "You have got the MK23 Pistol\n"
	elseif weap == WEAPON_AQ2MK23DUAL_BIT then
		message = "You have got another MK23 Pistol\n"
	elseif weap == WEAPON_AQ2HANDCANNON_BIT then
		message = "You have got the Hand Cannon\n"
	elseif weap == WEAPON_AQ2M61FRAG_BIT then
		message = "You have got a M61 Frag Grenade\n"
	elseif weap == WEAPON_AQ2KNIFE_BIT then
		message = "You have got a Knife\n"
	elseif weap == WEAPON_AQ2M4_BIT then
		message = "You have got the M4 Assault Rifle\n"
	elseif weap == WEAPON_AQ2MP5_BIT then
		message = "You have got the MP5 Submachine Gun\n"
	elseif weap == WEAPON_AQ2SUPER90_BIT then
		message = "You have got the Super 90 Shotgun\n"
	elseif weap == WEAPON_AQ2SNIPER_BIT then
		message = "You have got Sniper Rifle\n"
	else
		error("GiveWeapon: unhandled weapon " .. weao .. "\n")
	end
	PrintS(idx_other, message)

	entities[idx_other].weapons = entities[idx_other].weapons | weap
	-- TODO: weapon auto-select logic based on the bitfield contents of weap (may be more than one) and current_weapon? Keep in mind weapon types also to avoid switching from pistol to explosive, for example?
	return true
end

-- ===================
-- GiveItem
-- 
-- Will give items in "items" to entity "idx_other"
-- ===================
function GiveItem(idx_other, items)
	local message

	if MathPopCount(items) ~= 1 then
		error("GiveItem: items should only have one item (cnt " .. MathPopCount(items) .. ", value " .. items .. ")\n") -- TODO FIXME
	end

	entities[idx_other].items = entities[idx_other].items | items

	if items & ITEM_PEPPER_BIT == ITEM_PEPPER_BIT then
		entities[idx_other].item_pepper_finished = gamestate.time + 10000
		entities[idx_other].item_pepper_attack_finished = 0
	end

	if items == ITEM_PEPPER_BIT then
		message = "You have got the P\n"
	else
		error("GiveItem: unhandled item " .. items .. "\n")
	end
	PrintS(idx_other, message)
end

-- ===================
-- ItemBecomeAvailable
-- 
-- "Spawns" and "Respawns" items, ready to be picked up
-- ===================
function ItemBecomeAvailable(idx_self)
	local mins, maxs
	local item_size

	mins, maxs = GetModelAABB(entities[idx_self].modelindex, ANIMATION_BASE_FRAME)
	item_size = AABBToBoxHalfExtents(mins, maxs)
	PhysicsCreateFromData(idx_self, PHYSICS_SHAPE_BOX, item_size, 10, entities[idx_self].origin, entities[idx_self].angles, nil, false) -- TODO: get mass from model AABB and density?
	PhysicsSetSolidState(idx_self, SOLID_ENTITY_TRIGGER) -- if can't pick up, pass through
	entities[idx_self].touch = "ItemTouch"
	entities[idx_self].visible = "VISIBLE_TEST"
	-- TODO: sound
end

-- ===================
-- ItemTouch
-- 
-- Used for items.
-- TODO: currently only works right if we only have one type of item
-- ===================
-- 
function ItemTouch(idx_self, idx_other)
	-- not a player? can't get items
	if not IsPlayer(idx_other) then
		return
	end

	-- dead bodies can't get items
	if entities[idx_other].health ~= nil and entities[idx_other].health <= 0 then
		return
	end

	if not isemptynumber(entities[idx_self].health) and (entities[idx_other].health >= entities[idx_other].max_health) and entities[idx_other].takedamage then
		return
	end
	if not isemptynumber(entities[idx_self].health) and (entities[idx_other].health < entities[idx_other].max_health) and entities[idx_other].takedamage then
		Heal(idx_other, entities[idx_self].health)
	end

	if not isemptynumber(entities[idx_self].armor) and (entities[idx_other].armor >= entities[idx_other].max_armor) and entities[idx_other].takedamage then
		return
	end
	if not isemptynumber(entities[idx_self].armor) and (entities[idx_other].armor < entities[idx_other].max_armor) and entities[idx_other].takedamage then
		GiveArmor(idx_other, entities[idx_self].armor)
	end

	if not isemptynumber(entities[idx_self].weapons) and not GiveWeapon(idx_other, entities[idx_self].weapons)  then
		return
	end

	for i = 0, AMMO_MAX_TYPES - 1 do
		if entities[idx_self].ammo ~= nil then
			if not isemptynumber(entities[idx_self].ammo[i]) and not GiveAmmo(idx_other, i, entities[idx_self].ammo[i]) then
				return
			end
		end
	end

	if not isemptynumber(entities[idx_self].items) then
		GiveItem(idx_other, entities[idx_self].items)
	end

	-- don't let anyone else touch us and remove the item as soon as possible
	entities[idx_self].touch = nil
	entities[idx_self].visible = VISIBLE_NO
	PhysicsDestroy(idx_self)
	if entities[idx_self].respawnable == true then
		entities[idx_self].nextthink1 = gamestate.time + gamestate.item_respawn_time
		entities[idx_self].think1 = "ItemBecomeAvailable"
	elseif entities[idx_self].respawnable == false then
		entities[idx_self].nextthink1 = gamestate.time + 1
		entities[idx_self].think1 = "SUB_Remove"
	else
		error("ItemTouch: unknown state " .. tostring(entities[idx_self].state) .. "\n")
	end
end

-- ===================
-- item_spawn
-- 
-- SPAWN FUNCTION
-- Set model to the item model
-- Set entities[idx_self].weapons to give weapons to whoever touchs it
-- Set entities[idx_self].health to heal whoever touchs it with the specified amount of health
-- Set entities[idx_self].armor to give armor to whoever touchs it with the specified amount of armor
-- Set entities[idx_self].items to give other items to whoever touchs it
-- Set "origin"
-- Set "angle"
-- ===================
function item_spawn(idx_self)
	ItemBecomeAvailable(idx_self)
end

-- =====================================================
--
-- Specific item handling
--
-- =====================================================

-- ===================
-- ItemsPrecache
-- 
-- This function should only be called by Game_SV_NewGame to load
-- item data specific to this match.
-- 
-- TODO: have a "none" value (like -2) for a item slot, allowing interpolation between item slots (use same formula as Perfect Dark) or just having them empty
-- ===================
function ItemsPrecache()

	gamestate.item_slot_info = {}
	for i = 1, GAME_MAX_ITEM_SLOTS do -- let the cvars be 1-based
		gamestate.item_slot_info[i - 1] = CvarGetInteger("g_item_slot" .. tostring(i))
	end

	gamestate.item_respawn_time = CvarGetInteger("g_item_respawn_time")

	PrecacheModel("g_armor")
	PrecacheModel("g_ammobox")
	
	-- weapon item models are precached by WeaponPrecache() TODO: only precache the ones who will appear in the current game/map
end

-- ===================
-- item_slot
-- 
-- SPAWN FUNCTION
-- Set "counter" to the item slot this item represents, the spawn
-- function will then read the slot configuration and spawn the
-- appropriate item.
-- ===================
function item_slot(idx_self)
	local ent = nil
	local what_item
	local origin, angles

	origin = tostring(entities[idx_self].origin)
	angles = tostring(entities[idx_self].angles)
	
	if entities[idx_self].counter < 1 or entities[idx_self].counter > GAME_MAX_ITEM_SLOTS then
		error("item_slot: " .. tostring(entities[idx_self].counter) .. " is out of range\n")
	end

	what_item = gamestate.item_slot_info[entities[idx_self].counter - 1] -- slots are 1-based

	if what_item == ITEM_SLOT_NOTHING then
	elseif what_item == ITEM_SLOT_ARMOR then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_armor"})
			entities[ent].armor = 100
	elseif what_item == ITEM_SLOT_WEAPON_FALCON2 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_FALCON2_IDX].g_model})
			entities[ent].weapons = WEAPON_FALCON2_BIT
	elseif what_item == ITEM_SLOT_WEAPON_CROSSBOW then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_CROSSBOW_IDX].g_model})
			entities[ent].weapons = WEAPON_CROSSBOW_BIT
	elseif what_item == ITEM_SLOT_WEAPON_SUPERDRAGON then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].g_model})
			entities[ent].weapons = WEAPON_SUPERDRAGON_BIT
	elseif what_item == ITEM_SLOT_WEAPON_CMP150 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_CMP150_IDX].g_model})
			entities[ent].weapons = WEAPON_CMP150_BIT
	elseif what_item == ITEM_SLOT_WEAPON_NBOMB then
			-- ammo only
	elseif what_item == ITEM_SLOT_WEAPON_SLAYER then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_SLAYER_IDX].g_model})
			entities[ent].weapons = WEAPON_SLAYER_BIT
	elseif what_item == ITEM_SLOT_WEAPON_ENERGYBALL then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].g_model})
			entities[ent].weapons = WEAPON_ENERGYBALL_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AXE then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_AXE_IDX].g_model})
			entities[ent].weapons = WEAPON_AXE_BIT
	elseif what_item == ITEM_SLOT_WEAPON_SHOTGUN then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_SHOTGUN_IDX].g_model})
			entities[ent].weapons = WEAPON_SHOTGUN_BIT
	elseif what_item == ITEM_SLOT_WEAPON_SUPERSHOTGUN then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].g_model})
			entities[ent].weapons = WEAPON_SUPERSHOTGUN_BIT
	elseif what_item == ITEM_SLOT_WEAPON_NAILGUN then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_NAILGUN_IDX].g_model})
			entities[ent].weapons = WEAPON_NAILGUN_BIT
	elseif what_item == ITEM_SLOT_WEAPON_SUPERNAILGUN then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].g_model})
			entities[ent].weapons = WEAPON_SUPERNAILGUN_BIT
	elseif what_item == ITEM_SLOT_WEAPON_GRENADELAUNCHER then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].g_model})
			entities[ent].weapons = WEAPON_GRENADELAUNCHER_BIT
	elseif what_item == ITEM_SLOT_WEAPON_ROCKETLAUNCHER then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].g_model})
			entities[ent].weapons = WEAPON_ROCKETLAUNCHER_BIT
	elseif what_item == ITEM_SLOT_WEAPON_THUNDERBOLT then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].g_model})
			entities[ent].weapons = WEAPON_THUNDERBOLT_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AQ2MK23 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_AQ2MK23_IDX].g_model})
			entities[ent].weapons = WEAPON_AQ2MK23_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AQ2MK23DUAL then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].g_model})
			entities[ent].weapons = WEAPON_AQ2MK23DUAL_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AQ2HADNCANNON then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].g_model})
			entities[ent].weapons = WEAPON_AQ2HANDCANNON_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AQ2M61FRAG then
			-- ammo only
	elseif what_item == ITEM_SLOT_WEAPON_AQ2KNIFE then
			-- ammo only
	elseif what_item == ITEM_SLOT_WEAPON_AQ2M4 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_AQ2M4_IDX].g_model})
			entities[ent].weapons = WEAPON_AQ2M4_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AQ2MP5 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_AQ2MP5_IDX].g_model})
			entities[ent].weapons = WEAPON_AQ2MP5_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AQ2SUPER90 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].g_model})
			entities[ent].weapons = WEAPON_AQ2SUPER90_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AQ2SNIPER then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].g_model})
			entities[ent].weapons = WEAPON_AQ2SNIPER_BIT
	else
			error("item_slot: item not recognized: " .. tostring(what_item) .. "\n")
	end

	if ent ~= nil then
		entities[ent].respawnable = true
	end
	Free(idx_self)
end

-- ===================
-- item_slot_ammo
-- 
-- SPAWN FUNCTION
-- Set "counter" to the item slot this item represents, the spawn
-- function will then read the slot configuration and spawn the
-- appropriate item ammo, if applicable
-- TODO: do not use the same amount for the ammo box as the weapon pickup ammo?
-- ===================
function item_slot_ammo(idx_self)
	local ent = nil
	local what_item
	local origin, angles

	origin = tostring(entities[idx_self].origin)
	angles = tostring(entities[idx_self].angles)

	if entities[idx_self].counter < 1 or entities[idx_self].counter > GAME_MAX_ITEM_SLOTS then
		error("item_slot: " .. tostring(entities[idx_self].counter) .. " is out of range\n")
	end

	what_item = gamestate.item_slot_info[entities[idx_self].counter - 1] -- slots are 1-based

	if what_item == ITEM_SLOT_NOTHING then
	elseif what_item == ITEM_SLOT_ARMOR then
			-- armor
	elseif what_item == ITEM_SLOT_WEAPON_FALCON2 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_FALCON2_IDX, false)] = gamestate.weapon_info[WEAPON_FALCON2_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_CROSSBOW then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_CROSSBOW_IDX, false)] = gamestate.weapon_info[WEAPON_CROSSBOW_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_SUPERDRAGON then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_SUPERDRAGON_IDX, false)] = gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_CMP150 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_CMP150_IDX, false)] = gamestate.weapon_info[WEAPON_CMP150_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_NBOMB then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].weapons = WEAPON_NBOMB_BIT
	elseif what_item == ITEM_SLOT_WEAPON_SLAYER then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_SLAYER_IDX, false)] = gamestate.weapon_info[WEAPON_SLAYER_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_ENERGYBALL then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_ENERGYBALL_IDX, false)] = gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_AXE then
			-- axe
	elseif what_item == ITEM_SLOT_WEAPON_SHOTGUN then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_SHOTGUN_IDX, false)] = gamestate.weapon_info[WEAPON_SHOTGUN_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_SUPERSHOTGUN then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_SUPERSHOTGUN_IDX, false)] = gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_NAILGUN then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_NAILGUN_IDX, false)] = gamestate.weapon_info[WEAPON_NAILGUN_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_SUPERNAILGUN then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_SUPERNAILGUN_IDX, false)] = gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_GRENADELAUNCHER then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_GRENADELAUNCHER_IDX, false)] = gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_ROCKETLAUNCHER then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_ROCKETLAUNCHER_IDX, false)] = gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_THUNDERBOLT then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_THUNDERBOLT_IDX, false)] = gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_AQ2MK23 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_AQ2MK23_IDX, false)] = gamestate.weapon_info[WEAPON_AQ2MK23_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_AQ2MK23DUAL then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_AQ2MK23DUAL_IDX, false)] = gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_AQ2HADNCANNON then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_AQ2HANDCANNON_IDX, false)] = gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_AQ2M61FRAG then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].weapons = WEAPON_AQ2M61FRAG_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AQ2KNIFE then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].weapons = WEAPON_AQ2KNIFE_BIT
	elseif what_item == ITEM_SLOT_WEAPON_AQ2M4 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_AQ2M4_IDX, false)] = gamestate.weapon_info[WEAPON_AQ2M4_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_AQ2MP5 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_AQ2MP5_IDX, false)] = gamestate.weapon_info[WEAPON_AQ2MP5_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_AQ2SUPER90 then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_AQ2SUPER90_IDX, false)] = gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].ammo_pickup_amount
	elseif what_item == ITEM_SLOT_WEAPON_AQ2SNIPER then
			ent = Spawn({["classname"] = "item_spawn", ["origin"] = origin, ["angles"] = angles, ["model"] = "g_ammobox"})
			entities[ent].ammo = {}
			entities[ent].ammo[WeaponGetAmmoIdx(WEAPON_AQ2SNIPER_IDX, false)] = gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].ammo_pickup_amount
	else
		error("item_slot_ammo: item not recognized: " .. tostring(what_item) .. "\n")
	end

	if ent ~= nil then
		entities[ent].respawnable = true
	end
	Free(idx_self)
end

-- ===================
-- ItemsDrop
-- 
-- Drop all of self items
-- TODO: drop ammo that was inside the weapons or drop default always? and the rest of the ammo?
-- TODO: currently only drops weapons
-- ===================
function ItemsDrop(idx_self)
	local ent, i

	i = 1
	while true do
		if (entities[idx_self].weapons & i == i) and not isemptystring(gamestate.weapon_info[WeaponBitToIdx(idx_self, i)].g_model) and gamestate.weapon_info[WeaponBitToIdx(idx_self, i)].g_model ~= "null" then -- only drop weapon if we have a pickable model
			local dir
			local multiplier

			ent = Spawn({["classname"] = "item_spawn", ["origin"] = tostring(entities[idx_self].origin), ["angles"] = tostring(entities[idx_self].angles), ["model"] = gamestate.weapon_info[WeaponBitToIdx(idx_self, i)].g_model})
			entities[ent].weapons = i
			entities[ent].respawnable = false

			-- TODO: parametize this for use by other types of droping
			multiplier = 16
			-- TODO: open interval at the end? but who cares
			dir = Vector3(math.random(-10000, 10000) / 10000 * multiplier, math.random(0, 10000) / 10000 * multiplier, math.random(-10000, 10000) / 10000 * multiplier)
			-- add some impulses in circular fashion to avoid them all cramping up on top of one another
			PhysicsApplyImpulse(ent, dir, Vector3())
		end

		-- overflow will happen, so we can't check in the loop
		if i == WEAPON_MAX_BIT then
			break
		end
			
		i = i << 1
	end
end
