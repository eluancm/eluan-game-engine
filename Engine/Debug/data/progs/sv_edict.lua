-- sv_edict.lua

-- ============================================================================
-- 
-- Game logic entity dictionary game support functions
-- 
-- ============================================================================

-- ===================
-- PR_AllocEdict
-- 
-- Use forcedslot_num to force a slot position, useful for player entities.
-- forcedslot_num == nil will alloc in the first free slot.
-- ===================
function AllocEdict(forcedslot_num)
	local i

	if forcedslot_num == nil then
		if gamestate.entities_free_num > 0 then
			gamestate.entities_free_num = gamestate.entities_free_num - 1
			i = gamestate.entities_free[gamestate.entities_free_num] -- TODO: other cases where a predecrement is needed and I didn't do it
			-- TODO: set the index in the entities_free table to nil for the gc? maybe the negligible memory should be ignore for faster freeing of entities later
		else
			error("AllocEdict: No free edicts!\n")
		end

	else
		i = forcedslot_num

		if entities[i] ~= nil then
			error("AllocEdict: forced slot " .. i .. " already active!\n")
		end

		-- this is a slow path
		for j = 0, gamestate.entities_free_num - 1 do
			if gamestate.entities_free[j] == i then
				-- TODO: test this
				for k = j, gamestate.entities_free_num - 2 do
					gamestate.entities_free[k] = gamestate.entities_free[k + 1]
				end
				gamestate.entities_free_num = gamestate.entities_free_num - 1
				-- TODO: set the index in the entities_free table to nil for the gc? maybe the negligible memory should be ignore for faster freeing of entities later
				break
			end
		end
	end

	entities[i] = {}
	
	-- TODO: don't error before freeing useless edicts! (dead bodies, etc)
	-- TODO: clean here? this wouldn't be necessary if no stray logic uses a freed edict AFTER IT WAS CLEANED AFTER FREEING! Before allocating check to see if it's not memset'd to zero, a indication of stray logic usage of a freed entity (does not work if stray logic only reads from an entity)

	if gamestate.entities_allocated_tail ~= nil then
		entities[gamestate.entities_allocated_tail].nextent = i
		entities[i].nextent = nil
		entities[i].prevent = gamestate.entities_allocated_tail
		gamestate.entities_allocated_tail = i
	elseif gamestate.entities_allocated_head == nil and gamestate.entities_allocated_tail == nil then
		gamestate.entities_allocated_head = i
		gamestate.entities_allocated_tail = i
		entities[i].nextent = -1
		entities[i].prevent = -1
	else
		error("AllocEdict: broken linked list (head == " .. gamestate.entities_allocated_head .. ", tail == " .. gamestate.entities_allocated_tail .. ")\n")
	end

	return i
end

-- ===================
-- FreeEdict
-- 
-- If "justclean", this function won't free the entity, it will merely clean its properties
-- Make sure to NOT use this edict anymore after freeing it!
-- Things like freeing a missile owner may cause mayhem if you don't deal with them properly
-- TODO: see if clearing the think and physical object is enough to never make the removed entity be used again
-- TODO: search for references everywhere and change them to NULL/world
-- TODO: see if we are clearing on destruction AND on allocation! Shouldn't do this!
-- ===================
function FreeEdict(ent, justclean)
	local save, save2

	-- make sure leftovers don't ruin the physics simulation
	PhysicsDestroy(ent)

	if justclean then
		save = entities[ent].prevent
		save2 = entities[ent].nextent
	end

	if not justclean and entities[ent] ~= nil then
		if entities[ent].nextent ~= nil then
			entities[entities[ent].nextent].prevent = entities[ent].prevent
		end
		if entities[ent].prevent ~= nil then
			entities[entities[ent].prevent].nextent = entities[ent].nextent
		end
		if ent == gamestate.entities_allocated_head then
			gamestate.entities_allocated_head = entities[ent].nextent
		end
		if ent == gamestate.entities_allocated_tail then
			gamestate.entities_allocated_tail = entities[ent].prevent
		end
		gamestate.entities_free[gamestate.entities_free_num] = ent
		gamestate.entities_free_num = gamestate.entities_free_num + 1
	end

	if justclean then
		entities[ent] = {}
		entities[ent].prevent = save
		entities[ent].nextent = save2
	else
		entities[ent] = nil
	end
end

-- ===================
-- SetDefaults
-- 
-- Sets default properties that should be present for all entities
-- ===================
function SetDefaults(idx_edict)
	-- some default values TODO: see what's needed to be set for ALL entities
	SetTransform(idx_edict, Vector3(), Vector3(), nil)
	entities[idx_edict].model = ""
	entities[idx_edict].classname = ""
	entities[idx_edict].anglesflags = 0
	
	entities[idx_edict].cur_anim = {}
	entities[idx_edict].cur_anim_start_frame = {}
	entities[idx_edict].cur_anim_num_frames = {}
	entities[idx_edict].cur_anim_loop = {}
	entities[idx_edict].cur_anim_framespersecond = {}
	entities[idx_edict].cur_anim_last_framespersecond = {}
	
	entities[idx_edict].frame = {}
	
	entities[idx_edict].movecmd = Vector3()
	entities[idx_edict].aimcmd = Vector3()

	if IsPlayer(idx_edict) then
		entities[idx_edict].netname = ""
		entities[idx_edict].frags = 0
		entities[idx_edict].connected_time = 0
	end
end

-- ===================
-- Spawn
-- 
-- Creates a new entity with the specified index. Index may be nil
-- tbl_properties is a collection of string key-pair values, may be nil
-- base_modelindex if this entity has a model key that points to a submodel, set to the base modelindex (eg. a map door has the base_modelindex set to the map modelindex)
--
-- TODO: clean this mess of parameters
-- TODO: make sure to not drop anything to floor when spawning initial entities, because whatever may be below the dropped entity may not have spawned yet (maybe only the map has)
-- TODO: make spawn functions consistent.
-- For example:
-- We still need to set origin and angles for some entities after spawning them
-- Some entities should only spawn during level load, others only ingame
-- Etc.
-- ===================
function Spawn(tbl_properties, idx_force, base_modelindex)
	if idx_force ~= nil then
		if entities[idx_force] ~= nil then
			error("SpawnIdx: entity " .. tonumber(idx_force) .. " already exists")
		end
	end

	local idx_edict

	idx_edict = AllocEdict(idx_force)
	SetDefaults(idx_edict)

	if tbl_properties ~= nil then
		-- parse what we received
		-- TODO: map all map attributes to gamecode entity attributes?
		for k, v in pairs(tbl_properties) do
			-- TODO: search for the most common ones first?
			if k == "classname" then
				entities[idx_edict][k] = v
			elseif k == "origin" then
				entities[idx_edict][k] = Vector3.fromstring(v)
			elseif k == "angles" then
				entities[idx_edict][k] = Vector3.fromstring(v)
			elseif k == "model" then
				if string.sub(v, 1, 1) == "*" then
					entities[idx_edict][k] = v .. "*" .. tostring(base_modelindex) -- submodel definition: *submodelnumber*basemodelprecacheindex
				else
					entities[idx_edict][k] = v
				end
				PrecacheModel(entities[idx_edict][k]) -- precache it so that setting the model works
			elseif k == "target" then
				entities[idx_edict][k] = v
			elseif k == "targetname" then
				entities[idx_edict][k] = v
			elseif k == "angle" then --  scalar angle just for yaw, special meanings for up and down
			    entities[idx_edict]["angles"] = Vector3()
				entities[idx_edict]["angles"]:setAngle(ANGLES_ROLL, 0)
				if v == "-1" then -- looking up
					entities[idx_edict]["angles"]:setAngle(ANGLES_PITCH, 90)
					entities[idx_edict]["angles"]:setAngle(ANGLES_YAW, 0)
				elseif v == "-2" then -- looking down
					entities[idx_edict]["angles"]:setAngle(ANGLES_PITCH, 270)
					entities[idx_edict]["angles"]:setAngle(ANGLES_YAW, 0)
				else
					entities[idx_edict]["angles"]:setAngle(ANGLES_PITCH, 0)
					entities[idx_edict]["angles"]:setAngle(ANGLES_YAW, tonumber(v))
				end
			elseif k == "spawnflags" then -- TODO: is this still used?
				entities[idx_edict][k] = tonumber(v)
			elseif k == "wait" then
				entities[idx_edict][k] = tonumber(v)
			elseif k == "map" then
				entities[idx_edict][k] = v
			elseif k == "movedist" then
				entities[idx_edict][k] = tonumber(v)
			elseif k == "movedir" then
				entities[idx_edict][k] = Vector3.fromstring(v)
			elseif k == "movespeed" then
				entities[idx_edict][k] = tonumber(v)
			elseif k == "moveangledist" then
				entities[idx_edict][k] = tonumber(v)
			elseif k == "moveangledir" then
				entities[idx_edict][k] = Vector3.fromstring(v)
			elseif k == "moveanglespeed" then
				entities[idx_edict][k] = tonumber(v)
			elseif k == "damage" then
				entities[idx_edict][k] = tonumber(v)
			elseif k == "noise1" then
				entities[idx_edict][k] = v
			elseif k == "noise2" then
				entities[idx_edict][k] = v
			elseif k == "noise3" then
				entities[idx_edict][k] = v
			elseif k == "noise4" then
				entities[idx_edict][k] = v
			elseif k == "counter" then
				entities[idx_edict][k] = tonumber(v)
			else
				error("Unknown attribute entity " .. tonumber(idx_edict) .. ": " .. k .. " = " .. v .. "\n")
			end
		end

		-- set up initial entity data, the order IS IMPORTANT
		SetTransform(idx_edict, entities[idx_edict].origin or Vector3(), entities[idx_edict].angles or Vector3(), nil)
		if not isemptystring(entities[idx_edict].model) then
			SetModel(idx_edict, entities[idx_edict].model)

			-- if it's a submodel, load collision brush data TODO: decide this by other means
			if string.sub(entities[idx_edict].model, 1, 1) == "*" then
				-- TODO: submodels need absmin and absmax. Only submodels?
				-- TODO: PhysicsSetSolidState here?
				PhysicsCreateFromModel(idx_edict, entities[idx_edict].modelindex, 0, entities[idx_edict].origin, entities[idx_edict].angles, nil, false) -- TODO: will all of them be kinematic?
			end -- otherwise they will default to having no physical representation and being static because of this
		end
		if not isemptystring(entities[idx_edict].classname) then
			local spawnfunction = function_get(entities[idx_edict].classname)
			if spawnfunction == nil and gamestate.initializing then
				error("No spawn function for entity " .. entities[idx_edict].classname .. "\n") -- TODO: option to switch this between a warning and an error
			end
			if type(spawnfunction) == "function" then
				spawnfunction(idx_edict)
				-- TODO: static-static collision error and nonsolid world when it didn't find the spawn function for trigger_changelevel
			end
		end

		-- TODO CONSOLEDEBUG if (entities[idx_self].active) Sys_Printf("Spawned %d: %s at {%f %f %f} with angles {%f %f %f}, target %s, targetname %s, model %d and wait %ld\n", NUM_FOR_EDICT(self), entities[idx_self].classname, entities[idx_self].origin[0], entities[idx_self].origin[1], entities[idx_self].origin[2], entities[idx_self].angles[0], entities[idx_self].angles[1], entities[idx_self].angles[2], entities[idx_self].target, entities[idx_self].targetname, entities[idx_self].model, entities[idx_self].wait);

		if entities[idx_edict] ~= nil then
			return idx_edict
		else
			return nil -- TODO: return world?
		end
	end
	
	return idx_edict
end

-- ===================
-- CleanOwnership
-- 
-- Every entity that makes idx_edict its owner will have the ownership reset
-- ===================
function CleanOwnership(idx_edict)
	-- make everything we own owned by no one TODO: fix the others entindex_t in the entity struct
	for _,v in pairs(entities) do
		if v.owner == idx_edict then
			v.owner = nil
		end
	end
end

-- ===================
-- Free
-- 
-- Delete an entity
-- ===================
function Free(idx_edict)
	FreeEdict(idx_edict, false)
	CleanOwnership(idx_edict)
end

-- ===================
-- Clean
-- 
-- Cleans an entity of all properties
-- ===================
function Clean(idx_edict)
	FreeEdict(idx_edict, true)
	CleanOwnership(idx_edict)
	SetDefaults(idx_edict)
end

-- ===================
-- FreeAll
-- 
-- This function shall deactivate all entities
-- ===================
function FreeAll()
	for i = 0, MAX_EDICTS - 1 do -- this is inclusive
		Free(i)
	end
end

-- ===================
-- IsPlayer
-- 
-- Returns true if a given entity is a player edict
-- ===================
function IsPlayer(idx_playerent)
	if idx_playerent == nil then
		return false
	end
	-- depends on edict order being right
	if idx_playerent > 0 and idx_playerent <= MAX_CLIENTS then
		return true
	end

	return false
end

-- ===================
-- GetPlayerSlot
-- 
-- Returns the engine player slot of a game entity
-- ===================
function GetPlayerSlot(idx_playerent)
	if not IsPlayer(idx_playerent) then
		error("GetPlayerSlot: not a player: " .. tonumber(idx_playerent) .. " " .. entities[idx_playerent].classname .. "\n")
	end

	-- depends on edict order being right!
	return idx_playerent - 1
end

-- ===================
-- Find
-- 
-- Returns the entity index of the first entity found with the given "key" (as a string) set to "value" (any type)
-- "start" is the entity from which to start looking for, exclusive or inclusive, depending on "firstinclusive". To iteratively find, subsequent call should use the entity returned in the previous call as the start point, with "firstinclusive" == false
-- if "onlyfirst" == true, the function returns upon finding the first entity
-- if "onlyfirst" == false, the functions searches until the ends of the edicts, returning
-- the first one and setting the game_edict_t.chain property to the next entity found. The
-- last entity found will have chain == nil.
-- if the entity is a player, we will only consider him/her/it if connected
-- TODO: use regions to find entities (most likely 3d partitions)
-- TODO: JUST MAKE THESE FASTER
-- TODO: for some (like targetname), use a tree with pointers to entities on the nodes identified by chars?
-- ===================
function Find(start, key, value, onlyfirst, firstinclusive)
	local first = nil
	local last, i

	if start < 0 then
		error("Searching starting from negative entity index for \"" .. key .. "\"" .. value .. "\"")
	end

	if key == nil or value == nil then
		Host_Error("Can't search for nil key or value\n")
	end

	if firstinclusive then
		i = start
	else
		i = entities[start].nextent
	end
	while i ~= nil do
		if not IsPlayer(i) or (IsPlayer(i) and entities[i].connected == true) then -- if it's a player, only consider if connected
			if entities[i][key] == value then
				if first ~= nil then
					entities[last].chain = i
				else
					first = i
				end

				if onlyfirst then
					entities[first].chain = nil
					return first
				else
					last = i
				end
			end
		end
		
		i = entities[i].nextent
	end

	if first ~= nil then -- prevent uninitialized use
		entities[last].chain = nil
	end

	return first
end

-- ===================
-- FindByRadius
-- 
-- Mostly the same instructions as FindByClassname, but using a pseudo-key radius
-- ===================
function FindByRadius(start, maxdist, origin, onlyfirst, firstinclusive)
	local first = nil
	local last, i

	if start < 0 then
		error("Searching starting from negative entity index for radius \"" .. maxdist .. "\"\n")
	end

	if maxdist < 0 then
		error("FindByRadius: maxdist < 0\n")
	end

	if firstinclusive then
		i = start
	else
		i = entities[start].nextent
	end
	while i ~= nil do
		if not IsPlayer(i) or (IsPlayer(i) and entities[i].connected == true) then -- if it's a player, only consider if connected
			if origin:CheckIfClose(entities[i].origin, maxdist) then
				if first ~= nil then
					entities[last].chain = i
				else
					first = i
				end

				if onlyfirst then
					entities[first].chain = nil
					return first
				else
					last = i
				end
			end
		end
		
		i = entities[i].nextent
	end

	if first ~= nil then -- prevent uninitialized use
		entities[last].chain = nil
	end

	return first
end

-- ===================
-- SUB_Remove
-- 
-- Useful to be used as an entity think function
-- ===================
function SUB_Remove(idx_self)
	Free(idx_self)
end

-- ===================
-- SUB_Null
-- 
-- Lets a function be defined without doing anything
-- ===================
function SUB_Null(idx_self)
end

-- ===================
-- SUB_UseTargets
-- 
-- Calls use() on every entity in which the targetname equals to entities[idx_self].target
-- Will make game_edict_t *self and *other valid.
-- This should be the ONLY function calling ent->use()
-- ===================
function SUB_UseTargets(idx_self)
	local ent

	-- no target? return
	if isemptystring(entities[idx_self].target) then
		return
	end

	ent = Find(0, "targetname", entities[idx_self].target, false, true)

	while ent ~= nil do
		if isemptystring(entities[ent].use) then
			error("SUB_UseTargets: entity " .. entities[ent].classname .. " at " .. tostring(entities[ent].origin) .. " has no use function\n")
		end

		if (entities[ent].wait_finished or 0) <= gamestate.time then
			local usefunction = function_get(entities[ent].use)
			if usefunction == nil then
				error("SUB_UseTargets: entity " .. entities[ent].classname .. " at " .. tostring(entities[ent].origin) .. " has no use function\n")
			elseif type(usefunction) == "function" then
				usefunction(ent, idx_self)
			else
				error("SUB_UseTargets: entity " .. entities[ent].classname .. " at " .. tostring(entities[ent].origin) .. " has an invalid use function: " .. entities[ent].use .. "\n")
			end
			entities[ent].wait_finished = (entities[ent].wait or 0) + gamestate.time
		end

		ent = entities[ent].chain
	end
end

-- ===================
-- SUB_SetAINextThink
-- 
-- Will set the nextthink time based on the AI interval, should be called at spawn and at each AI think
-- "additional" is useful to use on spawn to make entities not all think at the same time. Only makes
-- sense if 0 < additional < aiframetime.
-- ===================
function SUB_SetAINextThink(idx_self, thinkfn, additional)
	entities[idx_self].nextthink1 = gamestate.time + ((1.0 / GAME_AI_FPS) * 1000.0) + additional
	if thinkfn ~= nil then
		entities[idx_self].think1 = thinkfn
	end
end

-- ===================
-- KinematicMove
-- 
-- TODO FIXME: Uses think1
-- ===================
function KinematicMove(idx_self)
	local movedorigin = false
	local movedangles = false
	local destorigin = nil
	local destangles = nil

	if entities[idx_self].movedangledist ~= nil then
		if entities[idx_self].movedangledist < entities[idx_self].moveangledist then
			local movement = entities[idx_self].moveanglespeed * gamestate.frametime / 1000.0

			entities[idx_self].movedangledist = entities[idx_self].movedangledist + movement
			entities[idx_self].movedangledist = math.max(entities[idx_self].movedangledist, 0)
			entities[idx_self].movedangledist = math.min(entities[idx_self].movedangledist, entities[idx_self].moveangledist)
			destangles = entities[idx_self].moveangledir * entities[idx_self].movedangledist + entities[idx_self].moveangles
			movedangles = true
		end
	end

	if entities[idx_self].moveddist ~= nil then
		if entities[idx_self].moveddist < entities[idx_self].movedist then
			local movement = entities[idx_self].movespeed * gamestate.frametime / 1000.0

			entities[idx_self].moveddist = entities[idx_self].moveddist + movement
			entities[idx_self].moveddist = math.max(entities[idx_self].moveddist, 0)
			entities[idx_self].moveddist = math.min(entities[idx_self].moveddist, entities[idx_self].movedist)
			destorigin = entities[idx_self].movedir * entities[idx_self].moveddist + entities[idx_self].moveorigin
			movedorigin = true
		end
	end

	-- pushing and moving stuff on top will be handled by the physics engine
	if movedorigin or movedangles then
		SetTransform(idx_self, destorigin, destangles, nil)
	end
	-- TODO: are these necessary? are the velocity values correct?
	if movedorigin then
		PhysicsSetLinearVelocity(idx_self, entities[idx_self].movedir * entities[idx_self].movespeed)
	else
		PhysicsSetLinearVelocity(idx_self, Vector3(0, 0, 0))
	end
	if movedangles then
		PhysicsSetAngularVelocity(idx_self, entities[idx_self].moveangledir * entities[idx_self].moveanglespeed)
	else
		PhysicsSetAngularVelocity(idx_self, Vector3(0, 0, 0))
	end

	-- keep moving until we reach the final position
	if (entities[idx_self].movedangledist ~= entities[idx_self].moveangledist) or (entities[idx_self].moveddist ~= entities[idx_self].movedist) then
		entities[idx_self].think1 = "KinematicMove"
		entities[idx_self].nextthink1 = gamestate.time + 1
	end
end

-- ===================
-- SetModel
-- 
-- Models should ONLY be changed through this function. It takes care of everything.
-- ===================
--
function SetModel(idx_edict, str_model)
	entities[idx_edict].modelindex = GetModelIndex(str_model)
	entities[idx_edict].modelname = str_model
	-- from the moment we explicitly set a model on an invisible entity, set the visible status TODO: what if it's the world model?
	if entities[idx_edict].visible == VISIBLE_NO or entities[idx_edict].visible == nil then
		entities[idx_edict].visible = VISIBLE_TEST
	end
end

-- ===================
-- SetTransform
-- 
-- Transforms should ONLY be changed through this function. It takes care of everything.
-- ===================
--
function SetTransform(idx_edict, origin, angles, locked_angles)
	if origin ~= nil then
		entities[idx_edict].origin = origin:clone()
	end
	if angles ~= nil then
		entities[idx_edict].angles = angles:clone()
	end
	PhysicsSetTransform(idx_edict, origin, angles, locked_angles)
end
