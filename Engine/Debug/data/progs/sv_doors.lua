-- sv_doors.lua

-- =====================================================
--
-- Doors
--
-- =====================================================

STATE_CLOSED					= 0
STATE_CLOSING					= 1
STATE_OPEN						= 2
STATE_OPENING					= 3

-- ===================
-- func_door_start_sound
-- ===================
function func_door_start_sound(idx_self)
	if entities[idx_self].state == STATE_OPENING then
		if not isemptystring(entities[idx_self].noise1) then
			StartSoundB(GetSoundIndex(entities[idx_self].noise1), idx_self, CHAN_BODY, 1, 1, 0.25, true)
		else
			StopSoundB(idx_self, CHAN_BODY)
		end
	elseif entities[idx_self].state == STATE_OPEN then
		if not isemptystring(entities[idx_self].noise2) then
			StartSoundB(GetSoundIndex(entities[idx_self].noise2), idx_self, CHAN_BODY, 1, 1, 0.25, false)
		else
			StopSoundB(idx_self, CHAN_BODY)
		end
	elseif entities[idx_self].state == STATE_CLOSING then
		if not isemptystring(entities[idx_self].noise3) then
			StartSoundB(GetSoundIndex(entities[idx_self].noise3), idx_self, CHAN_BODY, 1, 1, 0.25, true)
		else
			StopSoundB(idx_self, CHAN_BODY)
		end
	elseif entities[idx_self].state == STATE_CLOSED then
		if not isemptystring(entities[idx_self].noise4) then
			StartSoundB(GetSoundIndex(entities[idx_self].noise4), idx_self, CHAN_BODY, 1, 1, 0.25, false)
		else
			StopSoundB(idx_self, CHAN_BODY)
		end
	else
		error("func_door_start_sound: unknown state " .. entities[idx_self].state .. " for entity " .. idx_self .. "\n")
	end
end

-- ===================
-- func_door_invert_motion
-- ===================
function func_door_invert_motion(idx_self)
	local tmp1, tmp2

	if entities[idx_self].movedir ~= nil then
		tmp1 = (entities[idx_self].movedir * entities[idx_self].movedist) + entities[idx_self].moveorigin -- use movedist instead of moveddist to make the full motion */
		entities[idx_self].moveddist = entities[idx_self].movedist - entities[idx_self].moveddist -- move back by how much we moved
		entities[idx_self].movedir = -entities[idx_self].movedir
		entities[idx_self].moveorigin = tmp1
	end

	if entities[idx_self].moveangledir ~= nil then
		tmp2 = (entities[idx_self].moveangledir * entities[idx_self].moveangledist) + entities[idx_self].moveangles -- use moveangledist instead of movedangledist to make the full motion
		entities[idx_self].movedangledist = entities[idx_self].moveangledist - entities[idx_self].movedangledist -- move back by how much we moved
		entities[idx_self].moveangledir = -entities[idx_self].moveangledir
		entities[idx_self].moveangles = tmp2
	end
end

-- ===================
-- func_door_setstate
-- ===================
function func_door_setstate(idx_self, state)
	entities[idx_self].state = state
	func_door_start_sound(idx_self)
end

-- ===================
-- func_door_checkstate
-- ===================
function func_door_checkstate(idx_self)
	if entities[idx_self].state == STATE_CLOSED or entities[idx_self].state == STATE_OPEN then
		return
	end

	if entities[idx_self].moveddist == entities[idx_self].movedist and entities[idx_self].movedangledist == entities[idx_self].moveangledist then
		if entities[idx_self].state == STATE_OPENING then
			func_door_setstate(idx_self, STATE_OPEN)
		elseif entities[idx_self].state == STATE_CLOSING then
			func_door_setstate(idx_self, STATE_CLOSED)
		end

		-- the following code is simpler, but we just call the generic function
		-- 
		-- entities[idx_self].moveddist = 0;
		-- Math_Vector3Scale(entities[idx_self].movedir, entities[idx_self].movedir, -1);
		-- Math_Vector3Copy(entities[idx_self].origin, entities[idx_self].moveorigin);
		-- entities[idx_self].movedangledist = 0;
		-- Math_Vector3Scale(entities[idx_self].moveangledir, entities[idx_self].moveangledir, -1);
		-- Math_Vector3Copy(entities[idx_self].angles, entities[idx_self].moveangles);

		func_door_invert_motion(idx_self)
		return
	end

	entities[idx_self].think2 = "func_door_checkstate"
	entities[idx_self].nextthink2 = gamestate.time + 1
end

-- ===================
-- func_door_use
-- ===================
function func_door_use(idx_self, idx_other)
	if entities[idx_self].state == STATE_OPEN and entities[idx_self].wait == 0 then -- do not come back TODO: what if it starts OPEN?
		return
	end

	-- make new move or resume previous movement
	entities[idx_self].think1 = "KinematicMove"
	entities[idx_self].nextthink1 = gamestate.time + 1

	entities[idx_self].think2 = "func_door_checkstate"
	entities[idx_self].nextthink2 = gamestate.time + 1

	-- make new move?
	if entities[idx_self].state == STATE_CLOSED then
		func_door_setstate(idx_self, STATE_OPENING)
	elseif entities[idx_self].state == STATE_OPEN then
		func_door_setstate(idx_self, STATE_CLOSING)
	end

	-- else continue the old move
end

-- ===================
-- func_door_touch
-- ===================
function func_door_touch(idx_self, idx_other)
	if entities[idx_self].contact_distance < -0.1 and (entities[idx_self].state == STATE_CLOSING or entities[idx_self].state == STATE_OPENING) then -- blocked TODO: be aware of the physics engine allowable penetration and non-ccd physics, adjust this depending on speed? have a default constant for the entire code
		if entities[idx_self].damage then
			DealDamage(idx_other, idx_self, entities[idx_self].damage)
		end

		-- this will make it stop and resume when use()'d again TODO: make an option of it
		--Sys_Snprintf(entities[idx_self].think1, MAX_GAME_STRING, "");
		--Sys_Snprintf(entities[idx_self].think2, MAX_GAME_STRING, "");

		-- come back if we can be activated more than once
		if entities[idx_self].wait then
			-- TODO FIXME: this code may be activated more than once if the first step of going back didn't remove the penetration depth
			if entities[idx_self].state == STATE_CLOSING then
				func_door_setstate(idx_self, STATE_OPENING)
			elseif entities[idx_self].state == STATE_OPENING then
				func_door_setstate(idx_self, STATE_CLOSING)
			end

			func_door_invert_motion(idx_self)
		end
	end
	-- TODO: also have a option to make it just wait for the blocking entities to get out of the way or stop mid-moving waiting to be activated again
end

-- ===================
-- func_door
-- 
-- SPAWN FUNCTION
-- When use()'d, changes state. Defaults to CLOSED.
-- 
-- Set "wait" to the wait time before it can be use()'d again. (in ms) (wait of zero means that it can only be activated ONCE)
-- Set "movedist" to distance it should move when opening
-- Set "movedir" to the movement direction vector (will be normalized)
-- Set "movespeed" to the speed of movement
-- Set "moveangledist" to distance it should turn when opening
-- Set "moveangledir" to the turning direction vector (will be normalized)
-- Set "moveanglespeed" to the speed of turning
-- Set "damage" to the amount of damage this door will cause if movement is blocked
-- Set "model"
-- Set "targetname" if door won't be found by other methods (like tracelines from the player or touch)
-- Set "origin" if "model" isn't in absolute coordinates
-- Set "angle" if "model" isn't in absolute coordinates
-- Set "noise1" for the sound effect of the door starting to open (will loop)
-- Set "noise2" for the sound effect of the door finishing opening (will not loop)
-- Set "noise3" for the sound effect of the door starting to close (will loop)
-- Set "noise4" for the sound effect of the door finishing closing (will not loop)
-- 
-- Remember that rotation will be around the models origin, so use the right tool in the mapping program to set an origin (in Quake3BSP/GtkRadiant/Q3MAP2, just use origin brushes)
-- 
-- TODO: make it possible to begin open? (for positioning the future fat triggers in the closed position), auto-close option using wait, stop when blocked option, spawnflags for these stuff instead of making new entvars, option for coming back or stoping even if blocked when wait == 0
-- TODO: checking for Vector3(0, 0, 0) but not for nil
-- ===================
function func_door(idx_self)
	-- quick hack for doors without any parameters
	if isemptynumber(entities[idx_self].movedist) and isemptynumber(entities[idx_self].moveangledist) then
		entities[idx_self].movedir = Vector3(0, 0, 1)
		entities[idx_self].movespeed = 1
		entities[idx_self].wait = 1000
		entities[idx_self].movedist = 5
	end

	if entities[idx_self].modelindex == nil then
		error("func_door needs a model, entity " .. tonumber(idx_self) .. "\n")
	end
	if isemptynumber(entities[idx_self].movedist) and isemptynumber(entities[idx_self].moveangledist) then
		error("func_door needs at least a movedist or moveangledist, entity " .. tonumber(idx_self) .. "%d\n")
	end
	if entities[idx_self].movedist ~= nil and entities[idx_self].movedist < 0 then
		error("func_door needs a movedist >= 0, entity " .. tonumber(idx_self) .. "%d\n")
	end
	if entities[idx_self].moveangledist ~= nil and entities[idx_self].moveangledist < 0 then
		error("func_door needs a moveangledist >= 0, entity " .. tonumber(idx_self) .. "%d\n")
	end
	if not isemptynumber(entities[idx_self].movedist) and entities[idx_self].movedir == Vector3(0, 0, 0) then
		error("func_door needs a movedir if movedist is specified, entity " .. tonumber(idx_self) .. "%d\n")
	end
	if not isemptynumber(entities[idx_self].moveangledist) and entities[idx_self].moveangledir == Vector3(0, 0, 0) then
		error("func_door needs a moveangledir if moveangledist is specified, entity " .. tonumber(idx_self) .. "%d\n")
	end
	if not isemptynumber(entities[idx_self].movedist) and entities[idx_self].movespeed ~= nil and entities[idx_self].movespeed <= 0 then
		error("func_door needs a movespeed > 0 if movedist is specified, entity " .. tonumber(idx_self) .. "%d\n")
	end
	if not isemptynumber(entities[idx_self].moveangledist) and entities[idx_self].moveanglespeed ~= nil and entities[idx_self].moveanglespeed <= 0 then
		error("func_door needs a moveanglespeed > 0 if moveangledist is specified, entity " .. tonumber(idx_self) .. "%d\n")
	end
	if isemptynumber(entities[idx_self].wait) and isemptynumber(entities[idx_self].damage) then
		error("func_door doesn't come back if blocked and causes no damage if blocked, entity " .. tonumber(idx_self) .. "%d\n")
	end

	if not isemptystring(entities[idx_self].noise1) then
		PrecacheSound(entities[idx_self].noise1)
	end
	if not isemptystring(entities[idx_self].noise2) then
		PrecacheSound(entities[idx_self].noise2)
	end
	if not isemptystring(entities[idx_self].noise3) then
		PrecacheSound(entities[idx_self].noise3)
	end
	if not isemptystring(entities[idx_self].noise4) then
		PrecacheSound(entities[idx_self].noise4)
	end

	entities[idx_self].state = STATE_CLOSED
	if entities[idx_self].movedir ~= nil then
		entities[idx_self].movedir:normalize()
		entities[idx_self].moveddist = 0
	end
	if entities[idx_self].moveangledir ~= nil then
		entities[idx_self].moveangledir:normalize()
		entities[idx_self].movedangledist = 0
	end
	entities[idx_self].moveorigin = entities[idx_self].origin:clone()
	entities[idx_self].moveangles = entities[idx_self].angles:clone()
	PhysicsSetSolidState(idx_self, SOLID_WORLD) -- TODO: SOLID_ENTITY to make it pass through dead bodies? but then it won't push them :(
	entities[idx_self].use = "func_door_use"
	entities[idx_self].touch = "func_door_touch"
end
