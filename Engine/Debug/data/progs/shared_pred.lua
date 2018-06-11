-- shared_pred.lua

-- =====================================================
--
-- Movement code shared between client and server
--
-- Movement happens asynchronously with each input packet
-- generated by the client
-- TODO: move more stuff here: shooting, crouching, etc (also from vehicles, etc)
-- TODO: see if the cmdents are getting TOO MUCH simulation during the synchronous frames!!!! (prediction errors when NOT running a sync simulation step on the client will indicate this!)
-- TODO: jump_released is set in the sync think, will this cause some short jumps when we hold jump?
--
-- =====================================================

-- ===================
-- async_playermove
-- ===================
function async_playermove(idx_self)
	-- from here, only stuff used during a match
	if gamestate.gameover then
		clearinput(idx_self)
		return
	end

	-- don't do anything past this block if dead
	if entities[idx_self].health <= 0 then
		if entities[idx_self].cmdent ~= idx_self then -- not controlling ourself
			clearinput(entities[idx_self].cmdent)
		end
		if entities[idx_self].buttoncmd == 0 and entities[idx_self].triggerbuttoncmd == 0 then
			entities[idx_self].respawnable = true
		end
		if (entities[idx_self].buttoncmd ~= 0 or entities[idx_self].triggerbuttoncmd ~= 0) and entities[idx_self].respawnable then
			PutClientInServer(idx_self) -- respawn
		end
	elseif entities[idx_self].cmdent ~= idx_self then -- not controlling ourself
		local idx_realself
		-- since this is async, better to not copy anything
		idx_realself = idx_self
		idx_self = entities[idx_self].cmdent
		if entities[idx_self].classname == "vehicle_fourwheels" then
			local turret

			turret = entities[idx_self].attachment5
			if not isemptynumber(entities[idx_self].counter) then
				-- TODO: sensitivity, etc... do this in physics code or client side
				-- use moveangles to store the UNTRANSFORMED ORIENTATION
				entities[turret].moveangles:setAngle(ANGLES_PITCH, entities[turret].moveangles:getAngle(ANGLES_PITCH) + entities[idx_realself].aimcmd:getAngle(ANGLES_PITCH) * entities[idx_realself].input_frametime)
				entities[turret].moveangles:setAngle(ANGLES_YAW, entities[turret].moveangles:getAngle(ANGLES_YAW) + entities[idx_realself].aimcmd:getAngle(ANGLES_YAW) * entities[idx_realself].input_frametime)
				if entities[turret].moveangles:getAngle(ANGLES_PITCH) < -10 then
					entities[turret].moveangles:setAngle(ANGLES_PITCH, -10)
				end
				if entities[turret].moveangles:getAngle(ANGLES_PITCH) > 60 then
					entities[turret].moveangles:setAngle(ANGLES_PITCH, 60)
				end
			end

			if entities[idx_realself].buttoncmd & BUTTONCMD_JUMP == BUTTONCMD_JUMP then
				entities[idx_realself].movecmd.y = 1 -- player jump == vehicle handbrake
			end
        elseif entities[idx_self].classname == "vehicle_vertiglider" then
			-- currently done by physics code

			if entities[idx_self].onground then
				entities[idx_realself].movecmd:clear()
				entities[idx_realself].aimcmd:clear()
			end
			if entities[idx_realself].buttoncmd & BUTTONCMD_JUMP == BUTTONCMD_JUMP then
				-- player jump == go up
				entities[idx_realself].movecmd.y = 1
			end

			-- correct roll (if we landed on a slope, when taking off we must unroll) TODO: roll controls, airplane controls in physics (done, needs enabling)
			local roll_losing_speed = entities[idx_realself].input_frametime * 0.1

			if entities[idx_self].angles:getAngle(ANGLES_ROLL) > 0 and not entities[idx_self].onground then
				entities[idx_self].angles:setAngle(ANGLES_ROLL, entities[idx_self].angles:getAngle(ANGLES_ROLL) - roll_losing_speed)
				if entities[idx_self].angles:getAngle(ANGLES_ROLL) < 0 then
					entities[idx_self].angles:setAngle(ANGLES_ROLL, 0)
				end
				SetTransform(idx_self, nil, entities[idx_self].angles, nil)
			elseif entities[idx_self].angles:getAngle(ANGLES_ROLL) < 0 and not entities[idx_self].onground then
				entities[idx_self].angles:setAngle(ANGLES_ROLL, entities[idx_self].angles:getAngle(ANGLES_ROLL) + roll_losing_speed)
				if entities[idx_self].angles:getAngle(ANGLES_ROLL) > 0 then
					entities[idx_self].angles:setAngle(ANGLES_ROLL, 0)
				end
				SetTransform(idx_self, nil, entities[idx_self].angles, nil)
			end
		end
		idx_self = idx_realself
		copyinput(idx_self, entities[idx_self].cmdent)
		clearinput(idx_self)
	else -- controlling ourself
		if entities[idx_self].movetype == MOVETYPE_WALK then
			if entities[idx_self].onground and (entities[idx_self].buttoncmd & BUTTONCMD_JUMP == BUTTONCMD_JUMP) and entities[idx_self].jump_released then -- no pogo-stick
				entities[idx_self].movecmd.y = 1
			else
				entities[idx_self].movecmd.y = 0
			end
		elseif entities[idx_self].movetype == MOVETYPE_FLY then
			-- the jump button will override the received movecmd.y
			if entities[idx_self].buttoncmd & BUTTONCMD_JUMP == BUTTONCMD_JUMP then
				entities[idx_self].movecmd.y = 1
			end
			-- otherwise moveup will climb/go up
		end
		-- player doesn't have full control over roll angles
		entities[idx_self].aimcmd:setAngle(ANGLES_ROLL, 0) -- TODO: allow leaning a little

		-- ignore physics angle-setting and do it here TODO: unify, remove redundancies but leave physics code for vehicles etc
		-- TODO: turn rate (and unify with physics)
		entities[idx_self].angles:setAngle(ANGLES_PITCH, entities[idx_self].angles:getAngle(ANGLES_PITCH) + entities[idx_self].input_frametime * entities[idx_self].aimcmd:getAngle(ANGLES_PITCH) * 180.0 / 1000.0)
		entities[idx_self].angles:setAngle(ANGLES_YAW, entities[idx_self].angles:getAngle(ANGLES_YAW) + entities[idx_self].input_frametime * entities[idx_self].aimcmd:getAngle(ANGLES_YAW) * 180.0 / 1000.0)

		entities[idx_self].angles:setAngle(ANGLES_PITCH, math.max(entities[idx_self].angles:getAngle(ANGLES_PITCH), PLAYER_MIN_PITCH))
		entities[idx_self].angles:setAngle(ANGLES_PITCH, math.min(entities[idx_self].angles:getAngle(ANGLES_PITCH), PLAYER_MAX_PITCH))

		if (entities[idx_self].anglesflags & ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT) == ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT or
			(entities[idx_self].anglesflags & ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT) == ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT or
			(entities[idx_self].anglesflags & ANGLES_KINEMATICANGLES_LOCK_YAW_BIT) == ANGLES_KINEMATICANGLES_LOCK_YAW_BIT then
			local kinematic_angles = entities[idx_self].angles:clone()
			kinematic_angles:setAngle(ANGLES_PITCH, 0)
			SetTransform(idx_self, nil, entities[idx_self].angles, kinematic_angles)
		else
			SetTransform(idx_self, nil, entities[idx_self].angles, nil)
		end

		-- do not let physics change angles
		entities[idx_self].aimcmd:setAngle(ANGLES_PITCH, 0)
		entities[idx_self].aimcmd:setAngle(ANGLES_YAW, 0)
	end

	PhysicsSimulateEntity(entities[entities[idx_self].cmdent].input_frametime, entities[idx_self].cmdent)

	--TODO: not everything is async, so we can't just clear
	if entities[idx_self].cmdent == idx_self then
		--clearinput(entities[idx_self].cmdent)
		entities[entities[idx_self].cmdent].movecmd:clear()
	end
end
