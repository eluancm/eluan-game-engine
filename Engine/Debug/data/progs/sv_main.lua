-- sv_main.lua

-- these should be first, to affect the entire code
-- this is code to avoid referencing to undeclared globals
-- usage: global_declare("entities", {}) or global_declare("NewGame")
-- commented because this makes we need to declare even functions!

-- local declaredNames = {}

-- function global_isnil(var)
-- 	if rawget(_G, var) == nil then
-- 		-- `var' is undeclared
-- 		return true
-- 	else
-- 		return false
-- 	end
-- end

-- function global_declare (name, initval)
-- 	rawset(_G, name, initval)
-- 	declaredNames[name] = true
-- end

-- setmetatable(_G, {
-- 	__newindex = function (t, n, v)
-- 		if not declaredNames[n] then
-- 			error("attempt to write to undeclared var. " .. n, 2) -- 2 will cast the error upwards in the call stack
-- 		else
-- 			rawset(t, n, v) -- do the actual set
-- 		end
-- 	end,
-- 	__index = function (_, n)
-- 		if not declaredNames[n] then
-- 			error("attempt to read undeclared var. " .. n, 2) -- 2 will cast the error upwards in the call stack
-- 		else
-- 			return nil
-- 		end
-- 	end,
-- })

-- this table should be the ONLY game state global in ALL of the lua code, only its contents will be preserved when saving/loading a game
-- please only store integers, numbers and strings in these tables and subtables. Entity references should be integers with the index. Function
-- pointers should be strings containing the function name and should be called with function_call() (for example, calling function_call(ent.touch)
-- with ent.touch = "mytouch" will call mytouch()). Remember that vectors and matrices are tables, but they are copied on some operations, not
-- referenced. Operations that return the vector reference instead of a copy: Vector2.=, Vector2:rotate, Vector2:set, Vector2:clear, Vector2:normalize,
-- Vector3.=, Vector3:set, Vector3:clear, Vector3:normalize, Vector4.=, Vector4:set, Vector4:clear, Vector4:normalize, Matrix4x4.=. Also remember
-- that in lua, 0 == true! Only nil and false evaluate as false
gamestate = {}

entities = {} -- just a shortcut to gamestate.entities

spawnparms = {}

-- SVC_SOUND and SVC_STOPSOUND channel names
CHAN_VOICE				= 0
CHAN_WEAPON				= 1
CHAN_BODY				= 2
CHAN_EXTRA				= 3

GAME_AI_FPS				= 10

LoadProg("shared_defs")
LoadProg("shared_lib")
LoadProg("shared_pred")
LoadProg("sv_animation")
LoadProg("sv_buttons")
LoadProg("sv_client")
LoadProg("sv_doors")
LoadProg("sv_edict")
LoadProg("sv_enemies")
LoadProg("sv_enemy_zombie")
LoadProg("sv_items")
LoadProg("sv_misc")
LoadProg("sv_triggers")
LoadProg("sv_vehicle")
LoadProg("sv_weapons")
LoadProg("sv_worldspawn")

-- ===================
-- StartParticleB
-- ===================
function StartParticleB(parttype, pos)
	MessageWriteByte(SVC_PARTICLE)
	MessageWriteByte(parttype)
	MessageWriteVec3(pos)
	MessageSendBroadcastUnreliable()
end

-- ===================
-- StartSoundB
-- 
-- TODO: cache all models and sounds by precacheindex_t, never by using strings and SV_GetSoundIndex/SV_GetModelIndex (search entire code)
-- ===================
function StartSoundB(sndindex, forent, channel, pitch, gain, attenuation, loop)
	MessageWriteByte(SVC_SOUND)
	MessageWritePrecache(sndindex)
	MessageWriteEntity(forent)
	MessageWriteVec3(entities[forent].origin)
	MessageWriteVec3(entities[forent].velocity)
	MessageWriteByte(channel)
	MessageWriteVec1(pitch)
	MessageWriteVec1(gain)
	MessageWriteVec1(attenuation)
	if loop then
		MessageWriteByte(1)
	else
		MessageWriteByte(0)
	end
	MessageSendBroadcastUnreliable()
end

-- ===================
-- StopSoundB
-- ===================
function StopSoundB(forent, channel)
	MessageWriteByte(SVC_STOPSOUND)
	MessageWriteEntity(forent)
	MessageWriteByte(channel)
	MessageSendBroadcastUnreliable() -- TODO FIXME: whoever doesn't get it will have the sound playing forever - SNAPSHOTS PLEASE!!!
end

-- ===================
-- CenterPrintS
-- ===================
function CenterPrintS(dest, message)
	MessageWriteByte(SVC_CENTERPRINT)
	MessageWriteString(message)
	MessageSendToClientReliable(GetPlayerSlot(dest))
end

-- ===================
-- CenterPrintB
-- ===================
function CenterPrintB(message)
	MessageWriteByte(SVC_CENTERPRINT)
	MessageWriteString(message)
	MessageSendBroadcastReliable()
end

-- ===================
-- PrintS
-- ===================
function PrintS(dest, message)
	MessageWriteByte(SVC_PRINT)
	MessageWriteString(message)
	MessageSendToClientReliable(GetPlayerSlot(dest))
end

-- ===================
-- PrintB
-- ===================
function PrintB(message)
	MessageWriteByte(SVC_PRINT)
	MessageWriteString(message)
	MessageSendBroadcastReliable()
end

-- ===================
-- EndGameS
-- ===================
function EndGameS(dest)
	MessageWriteByte(SVC_ENDGAME)
	MessageSendToClientReliable(GetPlayerSlot(dest))
end

-- ===================
-- AABBToBoxHalfExtents
-- 
-- This helper function transforms an AABB into a box half extents vector
-- ===================
function AABBToBoxHalfExtents(mins, maxs)
	local result, nmins, nmaxs
	-- if mins.x > 0 or maxs.x < 0 or mins.y > 0 or maxs.y < 0 or mins.z > 0 or maxs.z < 0 then
	--	error("Game_SV_AABBToBoxHalfExtents: box out of origin: [" .. mins.x .. " " .. mins.y .. " " .. mins.z .. "],[" .. maxs.x .. " " .. maxs.y .. " " .. maxs.z .. "]\n")
	-- end
	
	nmins = mins:clone()
	nmaxs = mins:clone()
	
	if nmins.x > 0 then
		nmins.x = 0
	end
	if nmins.y > 0 then
		nmins.y = 0
	end
	if nmins.z > 0 then
		nmins.z = 0
	end
	if nmaxs.x < 0 then
		nmaxs.x = 0
	end
	if nmaxs.y < 0 then
		nmaxs.y = 0
	end
	if nmaxs.z < 0 then
		nmaxs.z = 0
	end

	result = Vector3(math.max(-nmins.x, nmaxs.x), math.max(-nmins.y, nmaxs.y), math.max(-nmins.z, nmaxs.z))
	
	return result
end

-- ===================
-- GameOver
-- 
-- Ends the current game
-- ===================
function GameOver()
	gamestate.gameover = true
	for i = 0, MAX_CLIENTS - 1 do
		if entities[gamestate.players[i]].connected then -- WARNING: anything here must be equal to the gameover code in the Game_SV_ClientConnect function
			ClientIntermission(i)
		end
	end
end

-- ===================
-- CheckRules
-- 
-- Called at the start of each frame
-- ===================
function CheckRules()
	local endgame = false

	if gamestate.gameover then
		return
	end

	-- update rules every frame because they can change when the game has already started
	gamestate.timelimit = CvarGetReal("timelimit")
	gamestate.fraglimit = CvarGetReal("fraglimit")
	gamestate.noexit = CvarGetReal("noexit")

	if gamestate.timelimit > 0 then
		if gamestate.timelimit <= (gamestate.time / 1000.0) / 60.0 then
			PrintB("Timelimit reached!\n")
			endgame = true
		end
	end

	if gamestate.fraglimit > 0 then
		for i = 0, MAX_CLIENTS - 1 do
			if entities[gamestate.players[i]].connected and entities[gamestate.players[i]].frags >= gamestate.fraglimit then
				endgame = true
				PrintB(entities[gamestate.players[i]].netname .. " (client " .. i .. ") reached fraglimit!\n")
			end
		end
	end

	if endgame then
		GameOver()
		PrintC("endgame.\n")
	end
end

-- ===================
-- ClientSetName
-- 
-- Prevent null and repeated names
-- TODO: change names back in the client when repeated/unnamed or leave the (0), etc, server-side?
-- ===================
function ClientSetName(slot, intended_netname, broadcast)
	local idx_self = gamestate.players[slot]
	local newname
	local oldname
	local repeated = 0
	local num_checked

	if not isemptystring(intended_netname) then
		newname = intended_netname
	else
		newname = "(unnamed" .. slot .. ")"
	end

	if broadcast then
		oldname = entities[idx_self].netname
	end

	entities[idx_self].netname = newname
::checkagain::
	num_checked = 0
	for i = 0, MAX_CLIENTS - 1 do
		num_checked = num_checked + 1
		if i ~= slot and entities[gamestate.players[i]].connected and entities[gamestate.players[i]].netname == entities[idx_self].netname then
			repeated = repeated + 1
			break
		end
	end

	if repeated == MAX_CLIENTS then
		PrintC("Giving up trying to give an unrepeated name to client " .. slot .. " (currently " .. entities[idx_self].netname .. ")\n")
	elseif num_checked < MAX_CLIENTS then
		entities[idx_self].netname = newname .. "(" .. (repeated - 1) .. ")"
		goto checkagain
	end

	if broadcast then
		if oldname ~= entities[idx_self].netname then
			PrintB(oldname .. " is now known as " .. entities[idx_self].netname .. ".\n")
		end
	end
end

-- ===================
-- VoxelPopulate
-- 
-- Called by the server when it is starting a new map.
-- By not using Host_VoxelSetBlock, these blocks are NOT added
-- to the queue of anyone already ingame, so only use this
-- function when loading.
-- 
-- TODO: be very careful about very large worlds: allocation failure will result in segfaults in some subsystem not using Sys_MemAlloc!
-- ===================
function VoxelPopulate()
	if not gamestate.initializing then
		error("VoxelPopulate: not loading.\n")
	end

	if gamestate.name ~= "level2" then
		return
	end

	VoxelChunkBufferClear()

	-- ijk = chunkorigin, xyz = blockindex (inside chunk)
	for k = 0, 95 do
		for j = 0, 0 do
			for i = 0, 63 do
				for z = 0, VOXEL_CHUNK_SIZE_Z - 1 do
					for y = 0, VOXEL_CHUNK_SIZE_Y - 1 do
						for x = 0, VOXEL_CHUNK_SIZE_X - 1 do
							local absx = i * VOXEL_CHUNK_SIZE_X + x
							local absy = j * VOXEL_CHUNK_SIZE_Y + y
							local absz = k * VOXEL_CHUNK_SIZE_Z + z
							if absy - 10 < math.cos(absx / 32.0) * math.cos(absz / 32.0) * 20 or absy == 0 then
								VoxelChunkSetBlock(Vector3(x, y, z), (y + 1) % 16)
							else
								VoxelChunkSetBlock(Vector3(x, y, z), 0)
							end
						end
					end
				end
				
				VoxelChunkCommit(Vector3(i, j, k))
			end
		end
	end

	-- for (z = 0; z < 1024; z++)
	-- {
	-- 	for (y = 0; y < 16; y++)
	-- 	{
	-- 		for (x = 0; x < 512; x++)
	-- 		{
	-- 			//if (sqrt((float) (x-VOXEL_CHUNK_SIZE/2)*(x-VOXEL_CHUNK_SIZE/2) + (y-VOXEL_CHUNK_SIZE/2)*(y-VOXEL_CHUNK_SIZE/2) + (z-VOXEL_CHUNK_SIZE/2)*(z-VOXEL_CHUNK_SIZE/2)) <= VOXEL_CHUNK_SIZE/2)
	-- 			if (y - 10 < cos((vec_t)x / 32.f) * cos((vec_t)z / 32.f) * 20 || y == 0)
	-- 			{
	-- 				SV_VoxelSetBlock(x, y, z, (y + 1) % 16);
	-- 			}
	-- 		}
	-- 	}
	-- }

	for z = -64, -1 do
		for y = 16, 31 do
			for x = -64, -1 do
				if ((x + y + z) & 1) == 1 then
					if (y + 1) % 16 ~= VOXEL_BLOCKTYPE_EMPTY then
						VoxelSet(Vector3(x, y, z), (y + 1) % 16)
					end
				end
			end
		end
	end
end

-- =====================================================
--
-- FUNCTIONS CALLED EXCLUSIVELY BY C CODE
--
-- =====================================================

-- ===================
-- UpdatePhysStats
-- 
-- Called by the physics code to update physics data for any entity with an active physical representation
-- ===================
function UpdatePhysStats(ent, originx, originy, originz, anglesx, anglesy, anglesz, velocityx, velocityy, velocityz, avelocityx, avelocityy, avelocityz, onground)
	entities[ent].origin = Vector3(originx, originy, originz)
	entities[ent].angles = Vector3(anglesx, anglesy, anglesz)
	entities[ent].velocity = Vector3(velocityx, velocityy, velocityz)
	entities[ent].avelocity = Vector3(avelocityx, avelocityy, avelocityz)
	entities[ent].onground = onground
end

-- ===================
-- UpdatePhysDirections
-- 
-- Called by the physics code to update physics data for any entity with an active physical representation
-- ===================
function UpdatePhysDirections(ent, forwardx, forwardy, forwardz, rightx, righty, rightz, upx, upy, upz)
	entities[ent].forward = Vector3(forwardx, forwardy, forwardz)
	entities[ent].right = Vector3(rightx, righty, rightz)
	entities[ent].up = Vector3(upx, upy, upz)
end

-- ===================
-- PostPhysics
-- 
-- Called by the physics code at the end of the physics frame
-- ===================
function PostPhysics()
	for i,ent in pairs(entities) do
		if ent.movetype == MOVETYPE_FOLLOW or ent.movetype == MOVETYPE_FOLLOWANGLES then
			local neworigin

			if ent.owner == nil then
				error("entity " .. i .. " (" .. ent.classname .. ") owner is nil and movetype is follow/followangles\n")
			end

			neworigin = entities[ent.owner].origin:clone()
			neworigin = neworigin + entities[ent.owner].forward * (ent.followforward or 0)
			neworigin = neworigin + entities[ent.owner].right * (ent.followright or 0)
			neworigin = neworigin + entities[ent.owner].up * (ent.followup or 0)

			if ent.movetype == MOVETYPE_FOLLOWANGLES then
				SetTransform(i, neworigin, entities[ent.owner].angles, nil)
			else
				SetTransform(i, neworigin, nil, nil)
			end
		end
	end
end

-- ===================
-- ClientPreThink
-- 
-- Called before physics code is run
-- ===================
function ClientPreThink(slot)
	local idx_self = gamestate.players[slot]
	local playercontents

	-- from here, only stuff used during a match
	if gamestate.gameover then
		clearinput(idx_self)
		return
	end

	-- don't do anything past this block if dead
	if entities[idx_self].health <= 0 then
		return
	end

	WeaponFrame(idx_self)

	-- TODO: better ways to do this (also, for some games: just use inertia, allow between MAX_SPEED and MAX_AIRSPEED, but do not let INCREASING if more than MAX_AIRSPEED, etc...
	if entities[idx_self].onground then
		entities[idx_self].maxspeed = Vector3(PLAYER_INITIAL_MAX_SPEED, PLAYER_INITIAL_MAX_SPEED, PLAYER_INITIAL_MAX_SPEED)
		entities[idx_self].acceleration = Vector3(PLAYER_INITIAL_ACCELERATION, PLAYER_INITIAL_ACCELERATION, PLAYER_INITIAL_ACCELERATION)
	else
		entities[idx_self].maxspeed = Vector3(PLAYER_INITIAL_MAX_AIRSPEED, PLAYER_INITIAL_MAX_AIRSPEED, PLAYER_INITIAL_MAX_AIRSPEED)
		entities[idx_self].acceleration = Vector3(PLAYER_INITIAL_AIRACCELERATION, PLAYER_INITIAL_AIRACCELERATION, PLAYER_INITIAL_AIRACCELERATION)
	end

	-- TODO: deal with me making onground take a while to clear (important)
	-- TODO: waterlevel, to-out-of-water jump
	-- jumping and water movement
	if entities[idx_self].onground and (entities[idx_self].buttoncmd & BUTTONCMD_JUMP == 0) then
		entities[idx_self].jump_released = true
	elseif not entities[idx_self].onground then
		entities[idx_self].jump_released = false
	end
	playercontents = PointContents(entities[gamestate.world].modelindex, entities[idx_self].origin)
	if playercontents & CONTENTS_WATER_BIT == 0 then -- TODO: add other liquids here
		-- we have full air
		entities[idx_self].air_finished = gamestate.time + PLAYER_UNDERWATER_AIR_TIME
		-- if entities[idx_self].air_damage_finished ~= 0 TODO: GASPING SOUND HERE, WE WERE DROWNING
		entities[idx_self].air_damage_finished = 0

		entities[idx_self].movetype = MOVETYPE_WALK
	else
		-- handle drowning
		if entities[idx_self].air_finished < gamestate.time then
			if entities[idx_self].air_damage_finished < gamestate.time then
				entities[idx_self].air_damage_finished = gamestate.time + PLAYER_UNDERWATER_DAMAGE_INTERVAL
				DealDamage(idx_self, gamestate.world, PLAYER_UNDERWATER_DAMAGE)
			end
		end

		entities[idx_self].movetype = MOVETYPE_FLY

	end

	if entities[idx_self].climbing then
		entities[idx_self].movetype = MOVETYPE_FLY
		entities[idx_self].ignore_gravity = true
		entities[idx_self].climbing = false -- let whatever activated it activate again next frame
	else
		entities[idx_self].ignore_gravity = false
	end

	if entities[idx_self].cmdent == idx_self then -- controlling ourself
		-- crouching
		if entities[idx_self].buttoncmd & BUTTONCMD_CROUCH == BUTTONCMD_CROUCH then
			if not entities[idx_self].crouching then
				PlayerCrouch(idx_self)
			end
		else
			if entities[idx_self].crouching then
				PlayerStandUp(idx_self, false)
			end
		end
	end

	-- TODO CONSOLEDEBUG
	-- Sys_Printf("Player %d is in %d, movetype %d, speed %f\n", slot, playercontents, entities[idx_self].movetype, sqrt(entities[idx_self].velocity[0] * entities[idx_self].velocity[0] + entities[idx_self].velocity[1] * entities[idx_self].velocity[1] + entities[idx_self].velocity[2] * entities[idx_self].velocity[2]));
	-- Sys_Printf("Player voxelcontents = %d\n", SV_VoxelPointContents(entities[idx_self].origin));
end

-- TODO CONSOLEDEBUG
-- void Debug_Touch(void)
-- {
-- 	Sys_Printf("%s touched by %s at [%04.02f %04.02f %04.02f] normal [%04.02f %04.02f %04.02f] depth [%04.02f] reaction? %s\n", entities[idx_self].classname, other->classname, entities[idx_self].contact_pos[0], entities[idx_self].contact_pos[1], entities[idx_self].contact_pos[2], entities[idx_self].contact_normal[0], entities[idx_self].contact_normal[1], entities[idx_self].contact_normal[2], entities[idx_self].contact_distance, entities[idx_self].contact_reaction ? "YES" : "NO");
-- }

-- ===================
-- ClientPostThink
-- 
-- Called after physics code is run
-- ===================
function ClientPostThink(slot)
	local idx_self = gamestate.players[slot]

	-- from here, only stuff used during a match
	if gamestate.gameover then
		return
	end

	-- don't do anything past here if dead
	if entities[idx_self].health <= 0 then
		return
	end

	-- falling damage TODO: can cause problems with teleporters?
	if entities[idx_self].onground and entities[idx_self].last_falling_speed < PLAYER_FALLING_SPEED_THRESHOLD then
		DealDamage(idx_self, gamestate.world, (PLAYER_FALLING_SPEED_THRESHOLD - entities[idx_self].last_falling_speed) * PLAYER_FALLING_DAMAGE_MULTIPLIER)
		entities[idx_self].last_falling_speed = 0
		-- TODO: sound here, also: waterline crossing/land landing sounds for all entities
	end
	if not entities[idx_self].onground then
		entities[idx_self].last_falling_speed = entities[idx_self].velocity.y
	end

	CheckItems(idx_self)

	if entities[idx_self].cmdent ~= idx_self then -- not controlling ourself
		WeaponFireReleased(idx_self) -- just to keep things consistent
	else -- controlling ourself
		-- attacks, done in post-think so that we have correct angles and direction vectors for this frame
		if entities[idx_self].triggerbuttoncmd & TRIGGERBUTTONCMD_RELOAD == TRIGGERBUTTONCMD_RELOAD then
			WeaponReload(idx_self)
		else
			if entities[idx_self].buttoncmd & BUTTONCMD_FIRE == BUTTONCMD_FIRE then
				WeaponFire(idx_self)
			else
				WeaponFireReleased(idx_self)
			end
		end

		-- use button
		if entities[idx_self].triggerbuttoncmd & TRIGGERBUTTONCMD_USE == TRIGGERBUTTONCMD_USE then
			local trace, closest_hit_idx
			-- TODO: this uses the camera position from the old frame
			trace, closest_hit_idx = PhysicsTraceline(idx_self, entities[entities[idx_self].cameraent].origin, entities[idx_self].forward, 1.5, true, 0)

			if closest_hit_idx ~= nil then
				create_instant_use_event(idx_self, trace[closest_hit_idx].ent)
			end
		end

		-- TODO CONSOLEDEBUG - uses the endpos defined in the if block below, move it out to make it happen even without the button pressing
		-- game_edict_t *marker;
		-- vec3_t markerpos;
		-- markerpos[0] = (vec_t)(floor((endpos[0] + VOXEL_SIZE_X_2) / VOXEL_SIZE_X)) * (vec_t)VOXEL_SIZE_X;
		-- markerpos[1] = (vec_t)(floor((endpos[1] + VOXEL_SIZE_Y_2) / VOXEL_SIZE_Y)) * (vec_t)VOXEL_SIZE_Y;
		-- markerpos[2] = (vec_t)(floor((endpos[2] + VOXEL_SIZE_Z_2) / VOXEL_SIZE_Z)) * (vec_t)VOXEL_SIZE_Z;
		-- marker = Spawn("none", NULL, NULL, markerpos, entities[idx_self].angles, "energyball", 0, NULL);
		-- Sys_Snprintf(marker->think, MAX_GAME_STRING, "SUB_Remove");
		-- marker->nextthink = gamestate.time + 1;
		-- marker->owner = NUM_FOR_EDICT(self);
		-- marker->visible = VISIBLE_ALWAYS_OWNER;
		-- 
		-- marker = Spawn("none", NULL, NULL, endpos, entities[idx_self].angles, "energyball", 0, NULL);
		-- Sys_Snprintf(marker->think, MAX_GAME_STRING, "SUB_Remove");
		-- marker->nextthink = gamestate.time + 1;
		-- marker->owner = NUM_FOR_EDICT(self);
		-- marker->visible = VISIBLE_ALWAYS_OWNER;

		-- TODO: trace, if there is a hit, go a little towards the normal for inserting and a little against the normal for removing?
		if entities[idx_self].triggerbuttoncmd & TRIGGERBUTTONCMD_VOXELREMOVE == TRIGGERBUTTONCMD_VOXELREMOVE or entities[idx_self].triggerbuttoncmd & TRIGGERBUTTONCMD_VOXELSET == TRIGGERBUTTONCMD_VOXELSET then
			local endpos, trace, closest_hit_idx

			-- TODO: this uses the camera position from the old frame
			trace, closest_hit_idx = PhysicsTraceline(idx_self, entities[entities[idx_self].cameraent].origin, entities[idx_self].forward, 1.7, false, 0)
			if closest_hit_idx == nil then
				-- default distance
				-- TODO: this uses the camera position from the old frame
				endpos = entities[entities[idx_self].cameraent].origin + entities[idx_self].forward * 1.7
			else
				-- grab the closest hit and go a little in the direction opposite to the normal, it works if we only have CUBES
				-- TODO: make sure that the phsycis code sends the closest first and tune this "-0.5" in relation to VOXEL_SIZE_* (depending on where the normal points)
				endpos = Vector3(trace[closest_hit_idx].posx, trace[closest_hit_idx].posy, trace[closest_hit_idx].posz) - 0.5 * Vector3(trace[closest_hit_idx].normalx, trace[closest_hit_idx].normaly, trace[closest_hit_idx].normalz)
			end

			-- absolute coordinates integers are from the center of the voxels, so we need to adjust a little by VOXEL_SIZE_*_2
			local point = Vector3()
			point.x = math.floor((endpos.x + VOXEL_SIZE_X_2) / VOXEL_SIZE_X)
			point.y = math.floor((endpos.y + VOXEL_SIZE_Y_2) / VOXEL_SIZE_Y)
			point.z = math.floor((endpos.z + VOXEL_SIZE_Z_2) / VOXEL_SIZE_Z)

			if entities[idx_self].triggerbuttoncmd & TRIGGERBUTTONCMD_VOXELREMOVE == TRIGGERBUTTONCMD_VOXELREMOVE then
				local voxeltype = VoxelRemove(point)

				if voxeltype ~= VOXEL_BLOCKTYPE_EMPTY then
					PrintC("Removed type: " .. voxeltype .. " at " .. tostring(point) .. "\n")
				end
			end
			if entities[idx_self].triggerbuttoncmd & TRIGGERBUTTONCMD_VOXELSET == TRIGGERBUTTONCMD_VOXELSET then
				local voxeltype = 1
				local okay = VoxelSet(point, voxeltype)
				if okay then
					PrintC("Added   type: " .. voxeltype .. " at " .. tostring(point) .. "\n")
				end
			end
		end
	end

	-- TODO CONSOLEDEBUG
	-- int i;
	-- 
	-- if (entities[idx_self].onground)
	-- 	Sys_Printf("   ONGROUND!\n");
	-- else
	-- 	Sys_Printf("NOTONGROUND!\n");
	-- 
	-- if (entities[idx_self].triggerbuttoncmd & TRIGGERBUTTONCMD_RELOAD)
	-- {
	-- 	Sys_PhysicsTraceline(NUM_FOR_EDICT(self), entities[idx_self].origin, entities[idx_self].forward, 10, true, 0);
	-- 	for (i = 0; i < entities[idx_self].trace_numhits; i++)
	-- 	{
	-- 		vec3_t marker_angles;
	-- 		game_edict_t *marker;
	-- 
	-- 		Sys_Printf("TRACELINE: %d: pos[%4.2f %4.2f %4.2f] nor[%4.2f %4.2f %4.2f] frc[%4.2f], CLASSNAME: %s\n", i, entities[idx_self].trace_pos[i][0], entities[idx_self].trace_pos[i][1], entities[idx_self].trace_pos[i][2], entities[idx_self].trace_normal[i][0], entities[idx_self].trace_normal[i][1], entities[idx_self].trace_normal[i][2], entities[idx_self].trace_fraction[i], gamestate.entities[entities[idx_self].trace_ent[i]].classname);
	-- 		Math_VecToAngles(entities[idx_self].trace_normal[i], NULL, marker_angles);
	-- 		marker = Spawn("none", NULL, NULL, entities[idx_self].trace_pos[i], marker_angles, "energyball", 0);
	-- 		Sys_Snprintf(marker->think, MAX_GAME_STRING, "SUB_Remove");
	-- 		marker->nextthink = gamestate.time + 10000;
	-- 	}
	-- }
	-- 
	-- Sys_Printf("ANG: %4f %4f %4f\n", entities[idx_self].angles[0], entities[idx_self].angles[1], entities[idx_self].angles[2]);
	-- Sys_Printf("FOR: %4f %4f %4f\n", entities[idx_self].forward[0], entities[idx_self].forward[1], entities[idx_self].forward[2]);
	-- Sys_Printf("RIG: %4f %4f %4f\n", entities[idx_self].right[0], entities[idx_self].right[1], entities[idx_self].right[2]);
	-- Sys_Printf("UP : %4f %4f %4f\n", entities[idx_self].up[0], entities[idx_self].up[1], entities[idx_self].up[2]);
	-- 
	-- Sys_Printf("cam %4.4f %4.4f %4.4f\n", gamestate.entities[entities[idx_self].viewent].origin[0], gamestate.entities[entities[idx_self].viewent].origin[1], gamestate.entities[entities[idx_self].viewent].origin[2]);
	-- {
	-- 	vec3_t a, d, p;
	-- 	Sys_ModelStaticLightInPoint(svs.precached_models_data[1]->data, entities[idx_self].origin, a, d, p);
	-- 	Sys_Printf("amb %4.4f %4.4f %4.4f\n", a[0], a[1], a[2]);
	-- 	Sys_Printf("dir %4.4f %4.4f %4.4f\n", d[0], d[1], d[2]);
	-- 	Sys_Printf("pos %4.4f %4.4f %4.4f\n", p[0], p[1], p[2]);
	-- }
	-- Sys_Printf("plr %4.4f %4.4f %4.4f\n", entities[idx_self].origin[0], entities[idx_self].origin[1], entities[idx_self].origin[2]);
	-- {
	-- 	vec3_t mins = {-0.25, -0.25, -0.25};
	-- 	vec3_t maxs = {0.25, 0.25, 0.25};
	-- 	vec3_t dir, traceorigin, endpos, plane_normal;
	-- 	vec_t fraction, plane_dist;
	-- 	int allsolid, startsolid;
	-- 	game_edict_t *marker;
	-- 	vec3_t marker_angles;
	-- 	Math_Vector3ScaleAdd(entities[idx_self].forward, 10, entities[idx_self].origin, dir);
	-- 	Math_Vector3Copy(entities[idx_self].origin, traceorigin);
	-- 	//traceorigin[1] -= 1;
	-- 	//Sys_ModelTraceline(svs.precached_models_data[world->model]->data, traceorigin, dir, &allsolid, &startsolid, &fraction, endpos, plane_normal, &plane_dist);
	-- 	//Sys_ModelTracesphere(svs.precached_models_data[world->model]->data, traceorigin, dir, 0.125f, &allsolid, &startsolid, &fraction, endpos, plane_normal, &plane_dist);
	-- 	Sys_ModelTracebox(svs.precached_models_data[world->model]->data, traceorigin, dir, mins, maxs, &allsolid, &startsolid, &fraction, endpos, plane_normal, &plane_dist);
	-- 	Sys_Printf("S [%04.02f %04.02f %04.02f] E [%04.02f %04.02f %04.02f] %s %s F %04.02f EP [%04.02f %04.02f %04.02f] N [%04.02f %04.02f %04.02f] D %04.02f\n", traceorigin[0], traceorigin[1], traceorigin[2], dir[0], dir[1], dir[2], allsolid ? " AS" : "!AS", startsolid ? " SS" : "!SS", fraction, endpos[0], endpos[1], endpos[2], plane_normal[0], plane_normal[1], plane_normal[2], plane_dist);
	-- 	Math_VecForwardToAngles(plane_normal, marker_angles);
	-- 	marker = Spawn("none", NULL, NULL, endpos, marker_angles, "energyball", 0, NULL);
	-- 	Sys_Snprintf(marker->think, MAX_GAME_STRING, "SUB_Remove");
	-- 	marker->nextthink = gamestate.time + 1;
	-- }
	-- Sys_Snprintf(entities[idx_self].touch, MAX_GAME_STRING, "Debug_Touch");

	entities[idx_self].impulse = 0 -- impulse reset after getting evaluated
	entities[idx_self].triggerbuttoncmd = 0 -- trigger buttons are reset after getting evaluated
end

-- ===================
-- ClientConnect
-- 
-- Called when an client finishes his signon phase and is now in game.
-- The edict is ALWAYS active, even if the client is not connected.
-- Use the field "connected" to see if a player is online.
-- TODO: if a player builds a sentry gun, leaves the server, the sentry stays, someone elses logs in place of
-- this player, the sentry gun kill someone. Will the sentry give frags to this new player? reset owner of EVERY entity
-- a player owns to "world" when he quits?
-- TODO: do NOT call ClientConnect and ClientDisconnect when changing levels?
-- ===================
function ClientConnect(slot, netname, loading_saved_game)
	local idx_self = gamestate.players[slot]

	if not loading_saved_game then
		Clean(idx_self) -- just to be sure
		entities[idx_self].netname = ""
		entities[idx_self].frags = 0
		entities[idx_self].connected = true
		entities[idx_self].connected_time = 0
		
		-- just for the lua code, to avoid uninitialized use
		entities[idx_self].movecmd = Vector3()
		entities[idx_self].aimcmd = Vector3()
		entities[idx_self].buttoncmd = 0
		entities[idx_self].triggerbuttoncmd = 0
		entities[idx_self].impulse = 0
	end
	-- these are not preserved in saved games
	entities[idx_self].input_frametime = 0
	entities[idx_self].input_seq = 0

	ClientSetName(slot, netname, false)

	if not loading_saved_game then
		PrintB(entities[idx_self].netname .. " (client " .. slot .. ") entered the game.\n")

		-- since players have no spawn function, let's call the equivalent to reset our edict
		PutClientInServer(idx_self)

		-- go to intermission if needed
		if gamestate.gameover then -- WARNING: anything here must be equal to the gameover code in the GameOver function
			ClientIntermission(slot)
		end

		CenterPrintS(idx_self, "Connected to sever\n\nEnjoy!") -- TODO: motd
	end
end

-- ===================
-- ClientDisconnect
-- 
-- Called when disconnecting a client
-- 
-- TODO: save player information, for Game_SV_ClientConnect to load
-- if the player is coming back? :) needs authentication from the
-- engine first, them passing the ID to Game_SV_ClientConnect, which
-- will load the saved data OR create a new one if a new player.
-- But be careful with things such as: saving a single player game,
-- losing, doing a disconnect and saving player data, loading game
-- and finding your data as it was when you lost, not when you saved!
-- ===================
function ClientDisconnect(slot)
	local idx_self = gamestate.players[slot]

	-- TODO: not necessary to set these two here?
	entities[idx_self].connected = false
	entities[idx_self].connected_time = 0
	PrintB(entities[idx_self].netname .. " (client " .. slot .. ") has left the game with " .. entities[idx_self].frags .. " frags.\n")
	Clean(idx_self)
end

-- ===================
-- ParseClientMessages
-- 
-- Called to process extra commands from the client
-- ===================
function ParseClientMessages(slot, cmd, msg, readdata, datalen)
	if cmd == CLC_MOVE then
		local movecmd = MessageReadVec3(msg, readdata, datalen)
		local aimcmd = MessageReadVec3(msg, readdata, datalen)
		local buttoncmd = MessageReadByte(msg, readdata, datalen)
		local triggerbuttoncmd = MessageReadByte(msg, readdata, datalen)
		local impulse = MessageReadByte(msg, readdata, datalen)
		local frametime = MessageReadTime(msg, readdata, datalen)
		local seq = MessageReadShort(msg, readdata, datalen)

		restoreinput(gamestate.players[slot])
		if seq == entities[gamestate.players[slot]].input_seq + 1 then
			entities[gamestate.players[slot]].movecmd = movecmd
			entities[gamestate.players[slot]].aimcmd = aimcmd
			entities[gamestate.players[slot]].buttoncmd = buttoncmd
			entities[gamestate.players[slot]].triggerbuttoncmd = triggerbuttoncmd
			entities[gamestate.players[slot]].impulse = impulse
			entities[gamestate.players[slot]].input_frametime = frametime
		else
			-- dropped input
			--entities[gamestate.players[slot]].movecmd = Vector3()
			--entities[gamestate.players[slot]].aimcmd = Vector3()
			--entities[gamestate.players[slot]].buttoncmd = 0
			entities[gamestate.players[slot]].triggerbuttoncmd = 0
			entities[gamestate.players[slot]].impulse = 0
			--entities[gamestate.players[slot]].input_frametime = 0
		end
		entities[gamestate.players[slot]].input_seq = seq
		saveinput(gamestate.players[slot])
		async_playermove(gamestate.players[slot])
	else
		return false -- not processed
	end

	return true -- processed
end

-- ===================
-- SendClientMessages
-- 
-- Called to send game-specific state updates
-- TODO: send fixangle here and make sure it gets sent at the same time as teletransports and spawns
-- ===================
function SendClientMessages(slot)
	local weap_idx = WeaponBitToIdx(gamestate.players[slot], entities[gamestate.players[slot]].current_weapon)
	local ammo_idx = WeaponGetAmmoIdx(weap_idx, true)

	MessageWriteByte(SVC_UPDATESTATS) -- TODO: put these in the differential snapshot
	MessageWriteVec1(entities[gamestate.players[slot]].health)
	MessageWriteVec1(entities[gamestate.players[slot]].armor)
	MessageWriteInt(entities[gamestate.players[slot]].weapons)
	if ammo_idx == -1 then
		MessageWriteShort(0)
		MessageWriteShort(0)
		MessageWriteShort(0)
		MessageWriteShort(0)
	else
		MessageWriteShort(entities[gamestate.players[slot]].weapons_ammo[weap_idx]) -- TODO: converting to short, should be enough for most games
		MessageWriteShort(gamestate.weapon_info[weap_idx].ammo_capacity) -- TODO: converting to short, should be enough for most games
		MessageWriteShort(entities[gamestate.players[slot]].ammo[ammo_idx]) -- TODO: converting to short, should be enough for most games
		MessageWriteShort(entities[gamestate.players[slot]].ammo_capacity[ammo_idx]) -- TODO: converting to short, should be enough for most games
	end
	MessageWriteInt(entities[gamestate.players[slot]].current_weapon)
	MessageWriteInt(entities[gamestate.players[slot]].items)
	for i = 0, MAX_CLIENTS - 1 do
		MessageWriteString(entities[gamestate.players[i]].netname)
		MessageWriteInt(entities[gamestate.players[i]].frags)
		MessageWriteShort(math.floor((entities[gamestate.players[i]].connected_time / 1000) / 60)) -- TODO: converting to short, should be enough for most games
		MessageWriteShort(NetworkGetPing(i)) -- TODO: converting to short, should be enough for most games
		MessageWriteShort(NetworkGetPacketLoss(i)) -- TODO: converting to short, should be enough for most games
	end
	MessageSendToClientUnreliable(slot) -- send as unreliable because everything is being re-sent every frame
end

-- ===================
-- StartFrame
-- 
-- Called at the start of each frame
-- ===================
function StartFrame(last_frametime)
	gamestate.time = gamestate.time + last_frametime
	gamestate.frametime = last_frametime

	for i = 0, MAX_CLIENTS - 1 do
		if entities[gamestate.players[i]].connected then
			entities[gamestate.players[i]].connected_time = entities[gamestate.players[i]].connected_time + gamestate.frametime
		end
	end

	CheckRules()
end

-- ===================
-- EndFrame
-- 
-- Called at the end of each frame
-- ===================
function EndFrame()
	for i,ent in pairs(entities) do
		-- update air time
		if not PhysicsIsDynamic(i) then
			ent.airborne_time = 0
		elseif ent.onground or ent.movetype == MOVETYPE_FLY or ent.visible == VISIBLE_NO or ent.velocity == nil or (ent.velocity ~= nil and ent.velocity:iszero()) then
			ent.airborne_time = 0
		else
			ent.airborne_time = (ent.airborne_time or 0) + gamestate.frametime
		end

		-- TODO: ability to change timeout, allow deactivation
		if ent.airborne_time > 5000 and ent.movetype ~= MOVETYPE_FLY and not ent.velocity:iszero() then
			PrintC("entity " .. i .. " (" .. ent.classname .. ") at " .. tostring(ent.origin) .. " probably fell out of the map\n")
		end

		-- update animations TODO: do these post-physics?
		if ent.animation_manager_type ~= nil then
			if ent.animation_manager_type == ANIMATION_MANAGER_NONE then
				ent.anim_pitch = false
			elseif ent.animation_manager_type == ANIMATION_MANAGER_HUMANOID then
				if ent.health > 0 then
					ent.anim_pitch = true
				else
					ent.anim_pitch = false -- TODO: use last position when creating ragdolls
				end
				AnimationHumanoidFrame(i)
			else
				error("EndFrame: unknown animation_manager_type: " .. ent.animation_manager_type .. "\n")
			end
		end

		AnimationFrame(i)
		
		if entities[i].cameraent ~= nil then
			local cam = entities[i].cameraent

			if entities[cam].classname ~= "camera" then
				error("Entity " .. i .. " (" .. entities[i].classname .. ") has a camera with wrong classname " .. entites[cam.classname] .. "\n")
			end
			
			if entities[i].classname == "player" then
				-- camera updating, done here because the camera think runs before physics
				-- TODO: fix the cameraent to a tag on the player model, so that it fits correctly dead, etc
				entities[cam].origin = entities[i].origin + entities[cam].camera_tilt_origin
				entities[cam].angles = entities[i].angles + entities[cam].camera_tilt_angles

				if entities[i].crouching then
					entities[cam].origin.y = entities[cam].origin.y + PLAYER_CAMERA_YOFFSET_CROUCH
				else
					entities[cam].origin.y = entities[cam].origin.y + PLAYER_CAMERA_YOFFSET
				end
			elseif entities[i].classname == "vehicle_fourwheels" then
				if not isemptynumber(entities[i].counter) then
					local turret = entities[i].attachment5
					entities[cam].angles = entities[turret].angles:clone()

					entities[cam].origin = entities[turret].origin - 5 * entities[turret].forward
					entities[cam].origin.y = entities[cam].origin.y + 1 -- this will make the line of sight not quite line up
				end
			elseif entities[i].classname == "vehicle_vertiglider" then
				if not isemptynumber(entities[i].counter) then
					-- TODO: sensitivity, etc... do this in physics code or client side
					entities[cam].angles = entities[i].angles:clone()

					entities[cam].origin = entities[i].origin - 5 * entities[i].forward
					entities[cam].origin.y = entities[cam].origin.y + 1 -- this will make the line of sight not quite line up
				end
			else
				error("Don't know how to update camera for entity " .. i .. " (" .. entities[i].classname .. ")\n")
			end
		end
	end

	for i,ent in pairs(entities) do
		-- do vweap stuff here because the animation manager code will hese "anim_pitch", etc
		if entities[i].weaponent ~= nil then
			if not isemptynumber(entities[i].modelindex) then -- catch nil and modelindex 0
				entities[entities[i].weaponent].modelindex = gamestate.weapon_info[WeaponBitToIdx(i, entities[i].current_weapon)].w_modelindex
				if entities[entities[i].weaponent].modelindex ~= nil then
					local vertex_animation

					_, _, _, _, _, vertex_animation = AnimationInfo(entities[i].modelindex, ANIMATION_BASE)
					
					-- skeletal model with multiple animations - need to calculate tag position
					if not vertex_animation then
						-- do server side animation to get the tag location
						for anim = 0, ANIMATION_MAX_BLENDED_FRAMES - 1 do
							entities[entities[i].weaponent].frame[anim] = 0
						end
						entities[entities[i].weaponent].anim_pitch = false
						Animate(entities[i].modelindex, i, entities[i].origin, entities[i].angles, entities[i].frame, entities[i].anim_pitch)
						local weapontagorigin = GetModelTagTransform(entities[i].modelindex, MODEL_TAG_RIGHTHAND, false, true, false, false, false, i)
						SetTransform(entities[i].weaponent, weapontagorigin, entities[i].angles, nil)
					else -- for vertex animation, assume that the tag location is embedded in the model and in sync with the parent entity's model
						for anim = 0, ANIMATION_MAX_BLENDED_FRAMES - 1 do
							entities[entities[i].weaponent].frame[anim] = entities[i].frame[anim]
						end
						entities[entities[i].weaponent].anim_pitch = entities[i].anim_pitch
						SetTransform(entities[i].weaponent, entities[i].origin, entities[i].angles, nil)
					end
				end
			else
				entities[entities[i].weaponent].modelindex = nil
			end
		end
	end
end

-- ===================
-- RunThinks
-- 
-- Called to wake up entities at a predetermined time
-- 
-- See code in Host_FilterTime to see the minimum and maximum
-- frametimes, to take into account when setting nextthinks.
-- ===================
function RunThinks()
	local thinkentities = {}
	local savedthink
	local ent

	ent = gamestate.entities_allocated_head
	while ent ~= nil do
		thinkentities[ent] = ent
		ent = gamestate.entities[ent].nextent
	end

	for k in pairs(thinkentities) do
		if entities[k].nextthink1 ~= nil and entities[k].nextthink1 <= gamestate.time and not isemptystring(entities[k].think1) then
			savedthink = entities[k].nextthink1
			
			local thinkfunction = function_get(entities[k].think1)
			if thinkfunction == nil then
				error("Invalid think1 function for entity " .. k .. ": " .. entities[k].think1 .. "\n")
			elseif type(thinkfunction) == "function" then
				thinkfunction(k)
			else
				error("Invalid think1 function for entity " .. k .. ": " .. entities[k].think1 .. "\n")
			end

			if entities[k] ~= nil then -- not removed itself during think1?
				if entities[k].nextthink1 == savedthink then -- didn't set a new think1 time
					entities[k].nextthink1 = 0
					entities[k].think1 = nil
				end
			end
		end

		if entities[k] ~= nil then -- not removed itself during think1?
			if entities[k].nextthink2 ~= nil and entities[k].nextthink2 <= gamestate.time and not isemptystring(entities[k].think2) then
				savedthink = entities[k].nextthink2
				
				local thinkfunction = function_get(entities[k].think2)
				if thinkfunction == nil then
					error("Invalid think2 function for entity " .. k .. ": " .. entities[k].think2 .. "\n")
				elseif type(thinkfunction) == "function" then
					thinkfunction(k)
				else
					error("Invalid think2 function for entity " .. k .. ": " .. entities[k].think2 .. "\n")
				end

				if entities[k] ~= nil then -- not removed itself during think2?
					if entities[k].nextthink2 == savedthink then -- didn't set a new think2 time
						entities[k].nextthink2 = 0
						entities[k].think2 = nil
					end
				end
			end
		end
	end
end

-- ===================
-- Touchents
-- 
-- Called by physics code to run the touch function of an entity
-- Touched entities should not be removed inside touch functions because other entities
-- may want to touch it in the same frame, or worse: new entities may occupy it's slot
-- and get touched!
-- To remove, set a next think for the next millisecond. The think functions will
-- run before the next frame's physics code. (Use SUB_Remove for the think function)
-- ===================
function Touchents(who, by, pos, normal, distance, reaction, impulse)
	if entities[who] == nil then
		error("Inactive entity " .. who .. " touched! (removed by another touch function?)\n")
	end

	if entities[by] == nil then
		error("Inactive entity " .. by .. " touching! (removed by another touch function?)\n")
	end

	if not isemptystring(entities[who].touch) then
		entities[who].contact_pos = pos
		entities[who].contact_normal = normal
		entities[who].contact_distance = distance
		entities[who].contact_reaction = reaction
		if reaction then
			entities[who].contact_impulse = impulse
		else
			entities[who].contact_impulse = 0
		end

		local touchfunction = function_get(entities[who].touch)
		if touchfunction == nil then
			error("Invalid touch function for entity " .. who .. ": " .. entities[who].touch .. "\n")
		elseif type(touchfunction) == "function" then
			touchfunction(who, by)
		else
			error("Invalid touch function for entity " .. who .. ": " .. entities[who].touch .. "\n")
		end

		if entities[who] == nil then
			error("Touched entity " .. who .. " removed mid-touch function!\n")
		end

		if entities[by] == nil then
			error("Touching entity " .. by .. " removed mid-touch function!\n")
		end
	end
end

-- ===================
-- CheckPhysicalCollisionResponse
-- 
-- Last check done by the physics code to see if two entities should generate
-- a collision response. (i.e. prevent inter-penetration)
-- ===================
function CheckPhysicalCollisionResponse(e1, e2)
	-- this prevents instantaneous self-killing with projectiles, for owners, grand-owners and siblings
	if e1 == gamestate.world or e2 == gamestate.world then
		return true
	end
	if entities[e1].owner == e2 then
		return false
	end
	if entities[e2].owner == e1 then
		return false
	end
	if gamestate.entities[e1].owner ~= nil and entities[gamestate.entities[e1].owner].owner == e2 then
		return false
	end
	if gamestate.entities[e2].owner ~= nil and entities[gamestate.entities[e2].owner].owner == e1 then
		return false
	end
	if entities[e1].owner == entities[e2].owner and entities[e1].owner ~= nil then
		return false
	end

	return true
end

-- ===================
-- NewGame
-- 
-- Called to start a new game, will load all resources
-- ===================
function NewGame(str_name, loading_saved_game)
	math.randomseed(tonumber(tostring(os.time()):reverse():sub(1,6)))

	-- it's important to zero out everything, the code relies on default and sane properties to be zero or nil
	gamestate = {}
	
	-- set up a shortcut
	gamestate.entities = {}
	entities = gamestate.entities
	-- TODO: put spawnparms inside gamestate here and in SaveGame()? See if it works, because spawnparms may be
	-- manipulated before NewGame() is called, but it is interesting to save spawnparms. For example, in Quake,
	-- where when you die the level is reloaded with the old spawnparms, so that you may try again starting with
	-- the same inventory, etc.
	
	gamestate.entities_free = {}
	for ent = 0, MAX_EDICTS - 1 do
		-- this array of indices is for fast allocation, it's maintained by AllocEdict and FreeEdict, do NOT set manually
		gamestate.entities_free[ent] = MAX_EDICTS - ent - 1 -- so that worldspawn will be the first
	end
	gamestate.entities_free_num = MAX_EDICTS
	
	FreeAll()
	gamestate.name = str_name
	gamestate.time = 0
	gamestate.initializing = true
	gamestate.num_enemies = 0
	gamestate.killed_enemies = 0

	-- the first model in memory will be the "invisible" model, for control entities
	PrecacheModel("null")
	-- then we load the world map from which we will load initial geometry and entities
	PrecacheModel(gamestate.name)
	-- TODO: some of this stuff should be loaded in their own spawn functions, not in all games
	-- now let's load in memory the stuff that we will need in all games
	PrecacheModel("player")
	WeaponPrecaches()
	ItemsPrecache()
	
	-- since the world should be entity zero, client entities are slot + 1 in game logic
	-- in short:
	-- 
	-- in entities[]: slot + 1
	-- in gamestate.players[]: slot
	-- in sv_clients: slot
	-- 
	-- Never change this. Lots of places assume it's this way.
	-- 
	-- Having world as entity zero lets us have some nice !entity_index comparisons.

	gamestate.world = 0 -- set it first because there's a comparison at the world spawn function

	-- after everything is set up, we may load the initial entities for this map
	local newents = GetModelEntities(GetModelIndex(gamestate.name))

	for k, v in ipairs(newents) do -- use ipairs to get a sorted list, because we need worldspawn first
		Spawn(v, nil, GetModelIndex(gamestate.name))
	end

	if not loading_saved_game then -- do not bother creating data that will be replaced
		-- call the game code to create the initial voxel world
		VoxelPopulate()
		-- initial update, if anything was populated in the function above
		VoxelCommitUpdates()
	end
	
	gamestate.initializing = false
	
	return gamestate.world
end

-- ===================
-- SaveGame
-- 
-- Returns a string with the entire gamestate table as lua code
-- ===================
function SaveGame()
	-- append the shortcut to gamestate.entities
	return table.show(gamestate, "gamestate") .. "\nentities = gamestate.entities;\n"
end

-- ===================
-- SaveSpawnParms
-- 
-- Returns a string with the entire spawnparms table as lua code
-- ===================
function SaveSpawnParms()
	local i, j

	spawnparms = {}
	spawnparms.players = {}
	for i = 0, MAX_CLIENTS - 1 do
		spawnparms.players[i] = {}
		if entities[gamestate.players[i]].connected == true then
			spawnparms.players[i].has_data = true
			spawnparms.players[i].health = entities[gamestate.players[i]].health
			spawnparms.players[i].max_health = entities[gamestate.players[i]].max_health
			spawnparms.players[i].armor = entities[gamestate.players[i]].armor
			spawnparms.players[i].max_armor = entities[gamestate.players[i]].max_armor
			spawnparms.players[i].weapons = entities[gamestate.players[i]].weapons
			spawnparms.players[i].weapons_ammo = {}
			for j = 0, WEAPON_MAX_IDX - 1 do
				spawnparms.players[i].weapons_ammo[j] = entities[gamestate.players[i]].weapons_ammo[j]
			end
			spawnparms.players[i].ammo= {}
			for j = 0, AMMO_MAX_TYPES - 1 do
				spawnparms.players[i].ammo[j] = entities[gamestate.players[i]].ammo[j]
			end
			spawnparms.players[i].ammo_capacity = {}
			for j = 0, AMMO_MAX_TYPES - 1 do
				spawnparms.players[i].ammo_capacity[j] = entities[gamestate.players[i]].ammo_capacity[j]
			end
			spawnparms.players[i].current_weapon = entities[gamestate.players[i]].current_weapon
		else
			spawnparms.players[i].has_data = false
		end
	end

	return table.show(spawnparms, "spawnparms")
end

-- ===================
-- SetNewSpawnParms
-- 
-- Empties the entire spawnparms table
-- ===================
function SetNewSpawnParms()
	spawnparms = {}
end

-- ===================
-- ClientNewSpawnParms
-- 
-- Empties a player's entire spawnparms table
-- ===================
function ClientSetNewSpawnParms(slot)
	-- clear changelevel spawn parms
	if spawnparms.players then
		if spawnparms.players[slot] then
			spawnparms.players[slot].has_data = false
		end
	end
end
