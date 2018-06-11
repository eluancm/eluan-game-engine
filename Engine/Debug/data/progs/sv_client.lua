-- sv_client.lua

PLAYER_INITIAL_MAX_HEALTH			= 100
PLAYER_INITIAL_MAX_ARMOR			= 100
PLAYER_INITIAL_MAX_SPEED			= 8
PLAYER_INITIAL_ACCELERATION			= 50
PLAYER_INITIAL_MAX_AIRSPEED			= 6
PLAYER_INITIAL_AIRACCELERATION		= 25
PLAYER_CAPSULE_RADIUS				= 0.25 -- caps of the capsule
PLAYER_CAPSULE_HEIGHT				= 1.3 -- minus 2 * radius, the caps of the cylinder forming the capsule, AABB of [-0.25, -0.9, -0.25][0.25, 0.9, 0.25]
PLAYER_CAPSULE_HEIGHT_CROUCH		= 0.3 -- minus 2 * radius, the caps of the cylinder forming the capsule, AABB of [-0.25, -0.4, -0.25][0.25, 0.4, 0.25]
PLAYER_CAMERA_YOFFSET				= 0.80 -- offset in the Y axis of the camera
PLAYER_CAMERA_YOFFSET_CROUCH		= (PLAYER_CAMERA_YOFFSET - (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0) -- offset in the Y axis of the camera
PLAYER_CAMERA_TILTRATE_ORIGIN		= 6 -- how fast we lose the tilt
PLAYER_CAMERA_TILTRATE_ANGLES		= 6 -- how fast we lose the tilt TODO FIXME: test this
PLAYER_UNDERWATER_AIR_TIME			= 10000 -- 10 seconds
PLAYER_UNDERWATER_DAMAGE			= 5
PLAYER_UNDERWATER_DAMAGE_INTERVAL	= 1000 -- 1 second
PLAYER_FALLING_SPEED_THRESHOLD		= -10 -- -10 m/s
PLAYER_FALLING_DAMAGE_MULTIPLIER	= 15
PLAYER_MAX_PITCH					= 85
PLAYER_MIN_PITCH					= -85
PLAYER_MASS							= 70

BUTTONCMD_JUMP						= 1
BUTTONCMD_FIRE						= 2
BUTTONCMD_CROUCH					= 4
TRIGGERBUTTONCMD_RELOAD				= 1
TRIGGERBUTTONCMD_VOXELREMOVE		= 2
TRIGGERBUTTONCMD_VOXELSET			= 4
TRIGGERBUTTONCMD_USE				= 8

-- ============================================================================
-- 
-- Player support entities
-- 
-- ============================================================================

-- ===================
-- camera_tilt_decrease_component
-- 
-- TODO: sync with animation, or just use a model tag for the camera!
-- ===================
function camera_tilt_decrease_component(component, speed)
	local out = Vector3()
	for k, v in pairs(component) do
		if v > 0 then
			v = v - gamestate.frametime / 1000.0 * speed
			if v < 0 then
				v = 0
			end
		elseif v < 0 then
			v = v + gamestate.frametime / 1000.0 * speed
			if v > 0 then
				v = 0
			end
		end
		out[k] = v
	end
	
	return out
end

-- ===================
-- camera_think
-- 
-- Housekepping stuff about the camera. We can't update position here
-- because thinks run before physics.
-- After spawning a camera, just set camera.owner
-- ===================
function camera_think(idx_self)
	if entities[idx_self].owner == nil then -- client disconnected?
		-- TODO CONSOLEDEBUG PrintC("Removing lone camera...\n")
		Free(idx_self)
		return
	end

	entities[idx_self].camera_tilt_origin = camera_tilt_decrease_component(entities[idx_self].camera_tilt_origin, PLAYER_CAMERA_TILTRATE_ORIGIN)
	entities[idx_self].camera_tilt_angles = camera_tilt_decrease_component(entities[idx_self].camera_tilt_angles, PLAYER_CAMERA_TILTRATE_ANGLES)

	-- TODO: will this be used somewhere? clone other stuff?
	if entities[entities[idx_self].owner].velocity then
		entities[idx_self].velocity = entities[entities[idx_self].owner].velocity:clone()
	end
	if entities[entities[idx_self].owner].avelocity then
		entities[idx_self].avelocity = entities[entities[idx_self].owner].avelocity:clone()
	end

	entities[idx_self].think1 = "camera_think"
	entities[idx_self].nextthink1 = gamestate.time + 1 -- recheck every frame
end

-- ===================
-- camera
-- 
-- SPAWN FUNCTION
-- Will keep track of the player's position
-- ===================
function camera(idx_self)
	entities[idx_self].camera_tilt_origin = Vector3()
	entities[idx_self].camera_tilt_angles = Vector3()

	entities[idx_self].think1 = "camera_think"
	entities[idx_self].nextthink1 = gamestate.time + 1 -- on the next frame

	-- the camera's model becomes the client's viewmodel (weapons, hands, etc)
	-- so nobody else should see it, because they are already seeing the full
	-- body model.
	entities[idx_self].visible = VISIBLE_ALWAYS_OWNER
end

-- ===================
-- weaponent_think
-- 
-- Housekepping stuff about the weaponent. We can't update position here
-- because thinks run before physics.
-- After spawning a weaponent, just set weaponent.owner
-- ===================
function weaponent_think(idx_self)
	if entities[idx_self].owner == nil then -- client disconnected?
		-- TODO CONSOLEDEBUG PrintC("Removing lone weaponent...\n")
		Free(idx_self)
		return
	end

	entities[idx_self].think1 = "weaponent_think"
	entities[idx_self].nextthink1 = gamestate.time + 1 -- recheck every frame
end

-- ===================
-- weaponent
-- 
-- SPAWN FUNCTION
-- Weapon model that other players see
-- ===================
function weaponent(idx_self)
	entities[idx_self].think1 = "weaponent_think"
	entities[idx_self].nextthink1 = gamestate.time + 1 -- on the next frame
	
	-- so that other players can see our weapon
	entities[idx_self].visible = VISIBLE_NEVER_OWNER
end

-- ============================================================================
-- 
-- Game logic for the players
-- 
-- ============================================================================

-- ===================
-- FindStartPosition
-- 
-- TODO: gameplay rules (teamplay, deathmatch, single player, etc) here and elsewhere
-- TODO: pre-linked list of spawn positions for fast searching
-- TODO: random?
-- ===================
function FindStartPosition()
	local firsttime = false
	local ent

	if gamestate.last_spawnpoint == nil then
		firsttime = true
		ent = Find(0, "classname", "info_player_deathmatch", true, true)
	else
		ent = Find(gamestate.last_spawnpoint, "classname", "info_player_deathmatch", true, false)
	end

	if ent == nil and not firsttime then
		ent = Find(0, "classname", "info_player_deathmatch", true, true)
	end
	if ent ~= nil then
		gamestate.last_spawnpoint = ent
		return ent
	end

	ent = Find(0, "classname", "info_player_start", true, true)

	if ent == nil then
		error("No info_player_start found on map, no starting position.\n")
	end

	return ent
end

-- ===================
-- player_die
-- ===================
function player_die(idx_self)
	entities[idx_self].animation_manager_type = ANIMATION_MANAGER_NONE
	entities[idx_self].movetype = MOVETYPE_FREE
	entities[idx_self].anglesflags = 0 -- TODO FIXME: this line causes sudden jumps in camera angles (setting transform here to the kinematic angles set in entities[idx_self].angles could make the collision object stuck/pass through some walls
	PhysicsSetSolidState(idx_self, SOLID_ENTITY_WITHWORLDONLY)
	entities[idx_self].takedamage = false

	ItemsDrop(idx_self)

	WeaponSelect(idx_self, WEAPON_PUNCH_BIT, true)

	CenterPrintS(idx_self, "You are dead!")
end

-- ===================
-- player_pain
-- ===================
function player_pain(idx_self)
end

-- ===================
-- PlayerPlaceAtSpawnPoint
-- 
-- Places the player at an specific spawn point and updates the camera.
-- SHOULD BE THE ONLY FUNCTION TO SET THE PLAYER ORIGIN IF A CAMERA HAS TO BE UPDATED SEPARATELY FROM THE PLAYER, if in this game the camera isn't set per-frame
-- ===================
function PlayerPlaceAtSpawnPoint(idx_self, idx_dest)
	local kinematic_angles = entities[idx_dest].angles:clone()

	kinematic_angles:setAngle(ANGLES_PITCH, 0)
	-- move player
	SetTransform(idx_self, entities[idx_dest].origin, entities[idx_dest].angles, kinematic_angles)
end

-- 
-- ===================
-- PlayerStandUp
-- 
-- TODO: trace a little more far to see if there is space?
-- TODO: settransform will make the player lose speed
-- ===================
function PlayerStandUp(idx_self, force)
	local player_size = Vector2(PLAYER_CAPSULE_RADIUS, PLAYER_CAPSULE_HEIGHT) -- TODO: use model...
	local up = Vector3(0, 1, 0)
	local origin_shift
	local trace_origin
	local kinematic_angles
	local trace

	if force then
		origin_shift = entities[idx_self].origin:clone()
		goto force_up
	end

	-- TODO: do we want this? nice side effect of disallowing flying with the "retreat legs" crouching type without doing traceline downwards
	if not entities[idx_self].onground then
		return
	end

	-- TODO: do a tracebox or tracecapsule, see if these calculations of the distances are right, are we doing too much? (getting stuck crouched at walls, etc)
	trace, _ = PhysicsTraceline(idx_self, entities[idx_self].origin, up, (PLAYER_CAPSULE_HEIGHT / 2.0 + PLAYER_CAPSULE_RADIUS + (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0), true, 0)
	if tablelength(trace) > 0 then
		return
	end

	trace_origin = entities[idx_self].origin:clone()

	trace_origin.x = trace_origin.x + PLAYER_CAPSULE_RADIUS
	trace_origin.z = trace_origin.z + PLAYER_CAPSULE_RADIUS
	trace, _ = PhysicsTraceline(idx_self, trace_origin, up, (PLAYER_CAPSULE_HEIGHT / 2.0 + PLAYER_CAPSULE_RADIUS + (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0), true, 0)
	if tablelength(trace) > 0 then
		return
	end

	trace_origin.x = trace_origin.x - PLAYER_CAPSULE_RADIUS * 2.0
	trace, _ = PhysicsTraceline(idx_self, trace_origin, up, (PLAYER_CAPSULE_HEIGHT / 2.0 + PLAYER_CAPSULE_RADIUS + (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0), true, 0)
	if tablelength(trace) > 0 then
		return
	end

	trace_origin.x = trace_origin.x + PLAYER_CAPSULE_RADIUS * 2.0
	trace_origin.z = trace_origin.z - PLAYER_CAPSULE_RADIUS * 2.0
	trace, _ = PhysicsTraceline(idx_self, trace_origin, up, (PLAYER_CAPSULE_HEIGHT / 2.0 + PLAYER_CAPSULE_RADIUS + (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0), true, 0)
	if tablelength(trace) > 0 then
		return
	end

	trace_origin.x = trace_origin.x - PLAYER_CAPSULE_RADIUS * 2.0
	trace, _ = PhysicsTraceline(idx_self, trace_origin, up, (PLAYER_CAPSULE_HEIGHT / 2.0 + PLAYER_CAPSULE_RADIUS + (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0), true, 0)
	if tablelength(trace) > 0 then
		return
	end

	-- TODO: overwriting a previous value, tracehits and etc will be instantaneous from the new value
	entities[entities[idx_self].cameraent].camera_tilt_origin.y = PLAYER_CAPSULE_HEIGHT_CROUCH - PLAYER_CAPSULE_HEIGHT
	origin_shift = entities[idx_self].origin:clone()
	origin_shift.y = origin_shift.y + (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0
::force_up::
	kinematic_angles = entities[idx_self].angles:clone()
	kinematic_angles:setAngle(ANGLES_PITCH, 0)
	PhysicsCreateFromData(idx_self, PHYSICS_SHAPE_CAPSULE_Y, player_size, PLAYER_MASS, origin_shift, entities[idx_self].angles, kinematic_angles, true) -- TODO: get mass from model AABB and density? different models may have different gameplay characteristics then
	PhysicsSetSolidState(idx_self, SOLID_ENTITY)

	entities[idx_self].crouching = false
end

-- ===================
-- PlayerCrouch
-- 
-- TODO: settransform will make the player lose speed
-- TODO: walk slower if crouched
-- ===================
function PlayerCrouch(idx_self)
	-- TODO: see if there is room
	local player_size = Vector2(PLAYER_CAPSULE_RADIUS, PLAYER_CAPSULE_HEIGHT_CROUCH) -- TODO: use model...
	local origin_shift
	local kinematic_angles

	kinematic_angles = entities[idx_self].angles:clone()
	kinematic_angles:setAngle(ANGLES_PITCH, 0)

	-- TODO: deal with animations in BOTH crouching types
	origin_shift = entities[idx_self].origin:clone()
	if entities[idx_self].onground then
		-- go down
		origin_shift.y = origin_shift.y - (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0
		-- TODO: overwriting a previous value, tracehits and etc will be instantaneous from the new value
		entities[entities[idx_self].cameraent].camera_tilt_origin.y = (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0
	else
		-- retreat legs
		origin_shift.y = origin_shift.y + (PLAYER_CAPSULE_HEIGHT - PLAYER_CAPSULE_HEIGHT_CROUCH) / 2.0
	end

	PhysicsCreateFromData(idx_self, PHYSICS_SHAPE_CAPSULE_Y, player_size, PLAYER_MASS, origin_shift, entities[idx_self].angles, kinematic_angles, true) -- TODO: get mass from model AABB and density? different models may have different gameplay characteristics then
	PhysicsSetSolidState(idx_self, SOLID_ENTITY)

	entities[idx_self].crouching = true
end

-- ===================
-- player_hide
-- ===================
function player_hide(idx_self)
	if entities[idx_self].health <= 0 then
		return
	end

	entities[idx_self].animation_manager_type = ANIMATION_MANAGER_NONE
	SetModel(idx_self, "null")
	entities[idx_self].takedamage = false
	PhysicsDestroy(idx_self)
end

-- ===================
-- player_unhide
-- ===================
function player_unhide(idx_self)
	if entities[idx_self].health <= 0 then
		return
	end

	entities[idx_self].animation_manager_type = ANIMATION_MANAGER_HUMANOID
	SetModel(idx_self, "player")
	entities[idx_self].takedamage = true
	PlayerStandUp(idx_self, true)
end

-- ===================
-- player_touch
-- ===================
function player_touch(idx_self, idx_other)
	if entities[idx_self].health <= 0 then
		return
	end
end

-- ===================
-- SetFromSpawnParms
-- 
-- See if we have saved data carried from the previous game
-- ===================
function SetFromSpawnParms(idx_self)
	local slot = GetPlayerSlot(idx_self)

	if spawnparms.players then
		if spawnparms.players[slot].has_data == true then
			spawnparms.players[slot].has_data = false
			entities[idx_self].health = spawnparms.players[slot].health
			entities[idx_self].max_health = spawnparms.players[slot].max_health
			entities[idx_self].armor = spawnparms.players[slot].armor
			entities[idx_self].max_armor = spawnparms.players[slot].max_armor
			entities[idx_self].weapons = spawnparms.players[slot].weapons
			entities[idx_self].weapons_ammo = {}
			for i = 0, WEAPON_MAX_IDX - 1 do
				entities[idx_self].weapons_ammo[i] = spawnparms.players[slot].weapons_ammo[i]
			end
			entities[idx_self].ammo = {}
			for i = 0, AMMO_MAX_TYPES - 1 do
				entities[idx_self].ammo[i] = spawnparms.players[slot].ammo[i]
			end
			entities[idx_self].ammo_capacity = {}
			for i = 0, AMMO_MAX_TYPES - 1 do
				entities[idx_self].ammo_capacity[i] = spawnparms.players[slot].ammo_capacity[i]
			end
			WeaponSelect(idx_self, spawnparms.players[slot].current_weapon, true)
			
			return
		end
	end

	entities[idx_self].health = PLAYER_INITIAL_MAX_HEALTH
	entities[idx_self].max_health = PLAYER_INITIAL_MAX_HEALTH
	entities[idx_self].armor = 0
	entities[idx_self].max_armor = PLAYER_INITIAL_MAX_ARMOR
	entities[idx_self].weapons = WEAPON_PUNCH_BIT
	entities[idx_self].weapons_ammo = {}
	for i = 0, WEAPON_MAX_IDX - 1 do
		entities[idx_self].weapons_ammo[i] = 0
	end
	entities[idx_self].ammo = {}
	for i = 0, AMMO_MAX_TYPES - 1 do
		entities[idx_self].ammo[i] = 0
	end
	WeaponSetAmmoCapacities(idx_self)
	WeaponSelect(idx_self, WEAPON_PUNCH_BIT, true)
end

-- ===================
-- PutClientInServer
-- 
-- (Re)spawns a client.
-- All properties should be re-set instead of calling Game_SV_EdictClean(), because there may
-- be stuff we want to keep (example: frags/score/points, the connected flag, etc...)
-- TODO: set stuff like sounds, hud icon, etc based on player model! even viewable items like armor may have a special size/scale for placement in the tagged location in the model
-- ===================
function PutClientInServer(idx_self)
	local start

	-- player camera position - cameraent will be our eyes, viewent can change for other points of view
	if entities[idx_self].cameraent == nil then
		entities[idx_self].cameraent = Spawn({["classname"] = "camera"}) -- create one if one doesn't already exist.
		entities[entities[idx_self].cameraent].owner = idx_self -- keep track of us
	end
	entities[idx_self].viewent = entities[idx_self].cameraent
	entities[idx_self].cmdent = idx_self
	
	-- player weapon external model
	if entities[idx_self].weaponent == nil then
		entities[idx_self].weaponent = Spawn({["classname"] = "weaponent"}) -- create one if one doesn't already exist.
		entities[entities[idx_self].weaponent].owner = entities[idx_self].cameraent -- for visibility calculations
	end

	-- player
	entities[idx_self].classname = "player"

	start = FindStartPosition()
	-- TODO: create the real model (for weapon fire collision detection) and let the model ONLY collide with weapons while making this model NOT collide with weapons
	entities[idx_self].anglesflags = ANGLES_KINEMATICANGLES_BIT | ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT
	PlayerPlaceAtSpawnPoint(idx_self, start)

	entities[idx_self].airborne_time = 0

	entities[idx_self].touch = "player_touch"
	-- entities[idx_self].movetype will be set each frame just before physics
	-- entities[idx_self].maxspeed will be set each frame just before physics
	-- entities[idx_self].acceleration will be set each frame just before physics
	entities[idx_self].ignore_gravity = false

	-- TODO: remove ourselves from the linked list in entities[idx_self].chain?
	-- TODO: clear buttons and move/aim cmds?

	entities[idx_self].owner = nil

	entities[idx_self].attack_finished = gamestate.time + 100 -- wait 0.1 seconds before being able to shoot (may be overridden by WeaponSelect)
	entities[idx_self].items = 0

	entities[idx_self].jump_released = true
	entities[idx_self].air_finished = gamestate.time + PLAYER_UNDERWATER_AIR_TIME
	entities[idx_self].air_damage_finished = 0
	entities[idx_self].last_falling_speed = 0
	entities[idx_self].climbing = false
	entities[entities[idx_self].cameraent].camera_tilt_origin:clear()
	entities[entities[idx_self].cameraent].camera_tilt_angles:clear()
	entities[idx_self].respawnable = false

	entities[idx_self].touchdamage_finished = 0
	entities[idx_self].die = "player_die"
	entities[idx_self].pain = "player_pain"

	entities[idx_self].target = nil
	entities[idx_self].targetname = nil
	entities[idx_self].use = nil
	entities[idx_self].wait_finished = 0
	entities[idx_self].wait = 0

	SetFromSpawnParms(idx_self)

	player_unhide(idx_self)
end

-- ===================
-- ClientIntermission
-- 
-- Puts a client in intermission mode.
-- ===================
function ClientIntermission(slot)
	local intermission

	EndGameS(gamestate.players[slot])

	-- player camera position
	-- TODO: use info_intermission or some other type of logic depending on game outcome
	intermission = Find(0, "classname", "info_player_start", true, true)
	if intermission == nil then
		intermission = Find(0, "classname", "info_player_deathmatch", true, true)
	end
	if intermission == nil then
		error("Can't find intermission spot!\n")
	end
	entities[gamestate.players[slot]].viewent = intermission

	entities[gamestate.players[slot]].takedamage = false -- TODO: see what this means for doors that don't return (will they push the player out of the map?) and also see if this is enough to make frags FIXED at the final score
end

-- ============================================================================
-- 
-- Server-side game client handling
-- 
-- ============================================================================

-- ===================
-- clearinput
-- ===================
function clearinput(idx_self)
	-- lua-isms
	if not entities[idx_self] then
		return
	end
	if entities[idx_self].movecmd then
		entities[idx_self].movecmd:clear()
	end
	if entities[idx_self].aimcmd then
		entities[idx_self].aimcmd:clear()
	end
	entities[idx_self].buttoncmd = 0
	entities[idx_self].triggerbuttoncmd = 0
	entities[idx_self].impulse = 0
	entities[idx_self].input_frametime = 0
	-- don't clear this, it's used for acks
	--entities[idx_self].input_seq = 0
end

-- ===================
-- copyinput
-- ===================
function copyinput(idx_src, idx_dst)
	-- lua-isms
	if not entities[idx_src] or not entities[idx_dst] then
		return
	end
	if entities[idx_src].movecmd then
		entities[idx_dst].movecmd = entities[idx_src].movecmd:clone()
	end
	if entities[idx_src].aimcmd then
		entities[idx_dst].aimcmd = entities[idx_src].aimcmd:clone()
	end
	entities[idx_dst].buttoncmd = entities[idx_src].buttoncmd
	entities[idx_dst].triggerbuttoncmd = entities[idx_src].triggerbuttoncmd
	entities[idx_dst].impulse = entities[idx_src].impulse
	entities[idx_dst].input_frametime = entities[idx_src].input_frametime
	entities[idx_dst].input_seq = entities[idx_src].input_seq
end

-- ===================
-- saveinput
-- ===================
function saveinput(idx_self)
	-- lua-isms
	if not entities[idx_self] then
		return
	end
	if entities[idx_self].movecmd then
		entities[idx_self].last_movecmd = entities[idx_self].movecmd:clone()
	end
	if entities[idx_self].aimcmd then
		entities[idx_self].last_aimcmd = entities[idx_self].aimcmd:clone()
	end
	entities[idx_self].last_buttoncmd = entities[idx_self].buttoncmd
	entities[idx_self].last_triggerbuttoncmd = entities[idx_self].triggerbuttoncmd
	entities[idx_self].last_impulse = entities[idx_self].impulse
	entities[idx_self].last_input_frametime = entities[idx_self].input_frametime
	entities[idx_self].last_input_seq = entities[idx_self].input_seq
end

-- ===================
-- restoreinput
-- ===================
function restoreinput(idx_self)
	-- lua-isms
	if not entities[idx_self] then
		return
	end
	-- assume everything exists if we saved at least once
	if entities[idx_self].last_movecmd then
		entities[idx_self].movecmd = entities[idx_self].last_movecmd:clone()
		entities[idx_self].aimcmd = entities[idx_self].last_aimcmd:clone()
		entities[idx_self].buttoncmd = entities[idx_self].last_buttoncmd
		entities[idx_self].triggerbuttoncmd = entities[idx_self].last_triggerbuttoncmd
		entities[idx_self].impulse = entities[idx_self].last_impulse
		entities[idx_self].input_frametime = entities[idx_self].last_input_frametime
		entities[idx_self].input_seq = entities[idx_self].last_input_seq
	end
end

-- ===================
-- create_instant_use_event
-- ===================
function create_instant_use_event(idx_self, idx_other)
	-- lua-isms
	if not entities[idx_self] or not entities[idx_other] then
		return
	end
	if not isemptystring(entities[idx_other].use) then
		local saved_targetname
		local saved_target

		-- make a temporary target-targetname pair because only SUB_UseTargets() should call use()
		-- TODO FIXME: make sure this new pair is UNIQUE, this is slow, etc...

		saved_targetname = entities[idx_other].targetname
		saved_target = entities[idx_self].target

		entities[idx_other].targetname = "playerent" .. idx_self .. "useframe"
		entities[idx_self].target = "playerent" .. idx_self .. "useframe"

		SUB_UseTargets(idx_self)

		entities[idx_other].targetname = saved_targetname
		entities[idx_self].target = saved_target
	end
end
