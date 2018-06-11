-- sv_animation.lua

-- ============================================================================
--
-- Entity model animation
--
-- TODO: when updating animation transition, do not do it on the client side
-- because savegames will be affected. Save cur_anim and prev_anim on the
-- server!
-- TODO: callback for when an animation ends
-- 
-- ============================================================================

ANIMATION_MANAGER_NONE				= 0
ANIMATION_MANAGER_HUMANOID			= 1

-- should be kept in sync with the definitions in game_shared.h/game_sv_lua_main.c
animation_names =
{
	[0] = "Base",
	[1] = "Idle",
	[2] = "Fire",
	[3] = "Fire2",
	[4] = "Fire3",
	[5] = "Fire4",
	[6] = "Fire5",
	[7] = "Fire6",
	[8] = "Fire7",
	[9] = "Fire8",
	[10] = "FireEmpty",
	[11] = "Reload",
	[12] = "WeaponActivate",
	[13] = "WeaponDeactivate",
	[14] = "Run",
	[15] = "RunRight",
	[16] = "RunLeft",
}

-- should be kept in sync with the definitions in game_shared.h/game_sv_lua_main.c and ANIMATION_MAX_BLENDED_FRAMES in host.h
animation_slot_names =
{
	[0] = "AllJoints",
	[1] = "Arms",
	[2] = "Legs",
	[3] = "Pelvis",
	[4] = "Torso"
}

-- should be kept in sync with the definitions in game_shared.h/game_sv_lua_main.c TODO: does this belong in this file?
model_tag_names =
{
	[0] = "RightHand",
	[1] = "LeftHand"
}

-- ===================
-- AnimationHumanoidFrame
-- 
-- Call at each frame to update animations
-- ===================
function AnimationHumanoidFrame(idx_self)
	if isemptynumber(entities[idx_self].modelindex) then
		return
	end

	-- the movecmd may have been manipulated for async physics
	local saved_movecmd
	if IsPlayer(idx_self) then
		saved_movecmd = entities[idx_self].movecmd:clone()
		entities[idx_self].movecmd = entities[idx_self].last_movecmd:clone()
	end

	local multiple_slots
	_, _, _, _, multiple_slots, _ = AnimationInfo(entities[idx_self].modelindex, ANIMATION_BASE)

	if multiple_slots then
		-- have the idle animation always
		AnimationStart(idx_self, ANIMATION_IDLE, ANIMATION_SLOT_ALLJOINTS, false, 0)

		-- TODO: alter animation FPS depending on the movement speed, blend the four directions of movement
		if entities[idx_self].movecmd.z < 0 then -- going forward
			AnimationStart(idx_self, ANIMATION_RUN, ANIMATION_SLOT_LEGS, false, 0)
		elseif entities[idx_self].movecmd.z > 0 then -- going backwards
			-- TODO: backwards animation
			AnimationStart(idx_self, ANIMATION_RUN, ANIMATION_SLOT_LEGS, false, 0)
		else -- stopped in this axis
			if entities[idx_self].movecmd.x > 0 then -- going right
				AnimationStart(idx_self, ANIMATION_RUNRIGHT, ANIMATION_SLOT_LEGS, false, 0)
			elseif entities[idx_self].movecmd.x < 0 then -- going left
				AnimationStart(idx_self, ANIMATION_RUNLEFT, ANIMATION_SLOT_LEGS, false, 0)
			else -- stopped in this axis
				AnimationStart(idx_self, ANIMATION_BASE, ANIMATION_SLOT_LEGS, false, 0)
			end
		end
	else
		-- TODO: alter animation FPS depending on the movement speed, blend the four directions of movement
		if entities[idx_self].movecmd.z < 0 then -- going forward
			AnimationStart(idx_self, ANIMATION_RUN, ANIMATION_SLOT_ALLJOINTS, false, 0)
		elseif entities[idx_self].movecmd.z > 0 then -- going backwards
			-- TODO: backwards animation
			AnimationStart(idx_self, ANIMATION_RUN, ANIMATION_SLOT_ALLJOINTS, false, 0)
		else -- stopped in this axis
			if entities[idx_self].movecmd.x > 0 then -- going right
				AnimationStart(idx_self, ANIMATION_RUNRIGHT, ANIMATION_SLOT_ALLJOINTS, false, 0)
			elseif entities[idx_self].movecmd.x < 0 then -- going left
				AnimationStart(idx_self, ANIMATION_RUNLEFT, ANIMATION_SLOT_ALLJOINTS, false, 0)
			else -- stopped in this axis
				AnimationStart(idx_self, ANIMATION_IDLE, ANIMATION_SLOT_ALLJOINTS, false, 0)
			end
		end
	end

	-- restore if we messed with it
	if IsPlayer(idx_self) then
		entities[idx_self].movecmd = saved_movecmd:clone()
	end
end

-- ============================================================================
-- 
-- Generic entity model animation
-- 
-- ============================================================================

-- ===================
-- AnimationFrame
-- 
-- Called automatically to update animations
-- TODO: CHECK THESE CALCULATIONS!
-- ===================
function AnimationFrame(idx_self)
	if isemptynumber(entities[idx_self].modelindex) then
		return
	end
	local vertex_animation
	for i = 0, ANIMATION_MAX_BLENDED_FRAMES - 1 do
		if entities[idx_self].frame ~= nil and entities[idx_self].frame[i] ~= nil and not isemptynumber(entities[idx_self].cur_anim_framespersecond[i]) then
			-- advance animation
			entities[idx_self].frame[i] = entities[idx_self].frame[i] + (gamestate.frametime / 1000.0) * entities[idx_self].cur_anim_framespersecond[i]

			_, _, _, _, _, vertex_animation = AnimationInfo(entities[idx_self].modelindex, entities[idx_self].cur_anim[i])

			if not vertex_animation then
				-- for looping frames animations, the last frame must be equal to the first: we will use it for interpolating as if it were the first, but won't ever set to it, we will set the first
				if entities[idx_self].frame[i] >= entities[idx_self].cur_anim_start_frame[i] + entities[idx_self].cur_anim_num_frames[i] - 1 then
					-- TODO: remove this loop and use mod - do not just subtract once because, depending on framerate, stuff may overflow too far and not get back
					if entities[idx_self].cur_anim_loop[i] then
						while entities[idx_self].frame[i] >= entities[idx_self].cur_anim_start_frame[i] + entities[idx_self].cur_anim_num_frames[i] - 1 do
							entities[idx_self].frame[i] = entities[idx_self].frame[i] - (entities[idx_self].cur_anim_num_frames[i] - 1) -- begin again
						end
					else
						entities[idx_self].frame[i] = entities[idx_self].cur_anim_start_frame[i] + entities[idx_self].cur_anim_num_frames[i] - 1 -- stop at it
					end
				end
			else
				-- in vertex animations, we actually set to the last frame
				if entities[idx_self].frame[i] >= entities[idx_self].cur_anim_start_frame[i] + entities[idx_self].cur_anim_num_frames[i] then
					-- TODO: remove this loop and use mod - do not just subtract once because, depending on framerate, stuff may overflow too far and not get back
					if entities[idx_self].cur_anim_loop[i] then
						while entities[idx_self].frame[i] >= entities[idx_self].cur_anim_start_frame[i] + entities[idx_self].cur_anim_num_frames[i] do
							entities[idx_self].frame[i] = entities[idx_self].frame[i] - (entities[idx_self].cur_anim_num_frames[i]) -- begin again
						end
					else
						entities[idx_self].frame[i] = entities[idx_self].cur_anim_start_frame[i] + entities[idx_self].cur_anim_num_frames[i] - 1 -- stop at it
						entities[idx_self].cur_anim_framespersecond[i] = 0 -- since we must stay 1/fps seconds at the last frame, this is how we know it has ended
					end
				end
			end
			
			-- TODO: for both vertex and skeletal animations, have an animation queue so that we intorpolate correctly to the next animation, specially on non-looping vertex animations, which may cause a 1/fps second pause at the end
			-- for vertex animation, we supply in frame[2] the next frame for interpolation
			if vertex_animation and i == ANIMATION_SLOT_ALLJOINTS then
				entities[idx_self].frame[2] = entities[idx_self].frame[i] + 1
				
				if entities[idx_self].frame[2] >= entities[idx_self].cur_anim_start_frame[i] + entities[idx_self].cur_anim_num_frames[i] then
					if entities[idx_self].cur_anim_loop[i] then
						while entities[idx_self].frame[2] >= entities[idx_self].cur_anim_start_frame[i] + entities[idx_self].cur_anim_num_frames[i] do
							entities[idx_self].frame[2] = entities[idx_self].frame[2] - (entities[idx_self].cur_anim_num_frames[i])
						end
					else
						entities[idx_self].frame[2] = entities[idx_self].cur_anim_start_frame[i] + entities[idx_self].cur_anim_num_frames[i] - 1
					end
				end
			end
		end
	end
end

-- ===================
-- AnimationTimeLeft
-- 
-- Returns how much time is left before the animation (or cycle, if looping) ends
-- May return slightly different results depending if it was called before or after
-- AnimationFrame in this frame. Check SV_Frame() for execution order.
-- ===================
function AnimationTimeLeft(idx_self, frame_slot)
	local vertex_animation
	_, _, _, _, _, vertex_animation = AnimationInfo(entities[idx_self].modelindex, entities[idx_self].cur_anim[frame_slot])
	
	if vertex_animation then
		if isemptynumber(entities[idx_self].cur_anim_framespersecond[frame_slot]) and isemptynumber(entities[idx_self].cur_anim_last_framespersecond[frame_slot]) then
			return 0
		else
			-- TODO: check this math
			local cur_anim_frame = entities[idx_self].frame[frame_slot] - entities[idx_self].cur_anim_start_frame[frame_slot]
			local frames_left = (entities[idx_self].cur_anim_num_frames[frame_slot]) - cur_anim_frame
			return frames_left / entities[idx_self].cur_anim_framespersecond[frame_slot] * 1000.0
		end
	else
		-- TODO: check this math
		local cur_anim_frame = entities[idx_self].frame[frame_slot] - entities[idx_self].cur_anim_start_frame[frame_slot]
		local frames_left = (entities[idx_self].cur_anim_num_frames[frame_slot] - 1) - cur_anim_frame
		return frames_left / entities[idx_self].cur_anim_framespersecond[frame_slot] * 1000.0
	end
end

-- ===================
-- AnimationStart
-- 
-- Set ups an entity for animation and starts it.
-- If "allow_restart" is false, won't begin the animation from the beggining if it's already running
-- "start_from" is 0-1 defines from where to start the animation
-- TODO: cache animation info
-- ===================
function AnimationStart(idx_self, animation, frame_slot, allow_restart, start_from)
	if entities[idx_self].cur_anim[frame_slot] == animation and not allow_restart then
		return
	end

	if frame_slot < 0 or frame_slot >= ANIMATION_MAX_BLENDED_FRAMES then
		error("AnimationStart: unknown frame slot " .. frame_slot .. " for entity " .. idx_self .. " (" .. entities[idx_self].classname .. ")\n")
	end

	if animation < 0 or animation >= NUM_ANIMATIONS then
		error("AnimationStart: unknown animation " .. animation .. " for entity " .. idx_self .. " (" .. entities[idx_self].classname .. ")\n")
	end

	entities[idx_self].cur_anim_start_frame[frame_slot], entities[idx_self].cur_anim_num_frames[frame_slot], entities[idx_self].cur_anim_loop[frame_slot], entities[idx_self].cur_anim_framespersecond[frame_slot], _, _ = AnimationInfo(entities[idx_self].modelindex, animation)
	entities[idx_self].cur_anim[frame_slot] = animation
	if start_from == 1.0 and entities[idx_self].cur_anim_loop[frame_slot] then
		-- rewind if looping and at the last frame
		entities[idx_self].frame[frame_slot] = entities[idx_self].cur_anim_start_frame[frame_slot]
	elseif start_from >= 0.0 and start_from <= 1.0 then
		entities[idx_self].frame[frame_slot] = entities[idx_self].cur_anim_start_frame[frame_slot] + (entities[idx_self].cur_anim_num_frames[frame_slot] - 1) * start_from
	else
		error("AnimationStart: start_from out of range: " .. start_from .. "\n")
	end

	entities[idx_self].cur_anim_last_framespersecond[frame_slot] = 0
end

-- ===================
-- AnimationPauseStop
-- 
-- Stops/Pauses all animations
-- TODO: test
-- ===================
function AnimationPauseStop(idx_self, frame_slot)
	-- prevent animation from advancing
	entities[idx_self].cur_anim_last_framespersecond[frame_slot] = entities[idx_self].cur_anim_framespersecond[frame_slot]
	entities[idx_self].cur_anim_framespersecond[frame_slot] = 0
end

-- ===================
-- AnimationResume
-- 
-- Resumes a paused animation
-- TODO: test
-- ===================
function AnimationResume(idx_self, frame_slot)
	if frame_slot < 0 or frame_slot >= ANIMATION_MAX_BLENDED_FRAMES then
		error("AnimationStart: unknown frame slot " .. frame_slot .. " for entity " .. idx_self .. " (" .. entities[idx_self].classname .. ")\n")
	end

	if entities[idx_self].cur_anim[frame_slot] < 0 or entities[idx_self].cur_anim[frame_slot] >= NUM_ANIMATIONS then
		error("AnimationResume: unknown animation " .. entities[idx_self].cur_anim[frame_slot] .. " for entity " .. idx_self .. " (" .. entities[idx_self].classname .. ")\n")
	end

	if isemptynumber(entities[idx_self].cur_anim_last_framespersecond[frame_slot]) then
		error("AnimationResume: animation " .. entities[idx_self].cur_anim[frame_slot] .. " for entity " .. idx_self .. " (" .. entities[idx_self].classname .. ") wasn't paused\n")
	end

	-- get the frames per second back so animation will continue automatically
	entities[idx_self].cur_anim_framespersecond[frame_slot] = entities[idx_self].cur_anim_last_framespersecond[frame_slot]
	entities[idx_self].cur_anim_last_framespersecond[frame_slot] = 0
end
