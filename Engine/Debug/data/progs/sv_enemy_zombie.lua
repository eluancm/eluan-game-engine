-- sv_enemy_zombie.lua

-- ============================================================================
-- 
-- Zombie enemy
-- 
-- ============================================================================

-- ===================
-- zombie_think
-- 
-- AI
-- ===================
function zombie_think(idx_self)
	local idx_target
	local player_direction
	local angles
	local kinematic_angles

	if entities[idx_self].health <= 0 then
		return
	end

	SUB_SetAINextThink(idx_self, nil, 0)

	-- by default, we don't move
	entities[idx_self].movecmd:clear()
	entities[idx_self].aimcmd:clear()

	entities[idx_self].movecmd.x = math.random(-10000, 10000) / 10000
	entities[idx_self].movecmd.z = math.random(-10000, 10000) / 10000

	target = enemies_commonfindplayer(idx_self, 10)
	if target == nil then
		WeaponFireReleased(idx_self)
		return -- don't shoot
	end

	-- calculate distance and direction towards the player
	player_direction = entities[target].origin - entities[idx_self].origin

	-- just in case
	if player_direction:iszero() then
		return
	end

	-- since setting aimcmd won't set angles for this frame, we do this differently
	-- TODO: constraints for aiming (up/down, etc)
	angles = MathVecForwardToAngles(player_direction)
	kinematic_angles = angles:clone()
	kinematic_angles:setAngle(ANGLES_PITCH, 0)
	-- TODO: move softly, also
	SetTransform(idx_self, nil, angles, kinematic_angles) -- create entities[idx_self].forward and company

	-- TODO: animation
	WeaponFire(idx_self)
end

-- ===================
-- zombie_die
-- ===================
function zombie_die(idx_self)
	-- TODO: animation
	enemies_commondie(idx_self)
end

-- ===================
-- zombie_pain
-- ===================
function zombie_pain(idx_self)
	-- TODO: animation
	enemies_commonpain(idx_self)
end

-- ===================
-- enemy_zombie
-- 
-- SPAWN FUNCTION
-- Basic enemy.
-- Set "origin"
-- Set "angle"
-- Set "target" optionally
-- TODO: Set "targetname" optionally, for making angry at someone?
-- ===================
function enemy_zombie(idx_self)
	local zombie_size = Vector2(PLAYER_CAPSULE_RADIUS, PLAYER_CAPSULE_HEIGHT) -- TODO: use model... also see if we are using the model EVERYWHERE we create an object
	local kinematic_angles

	if not enemies_commonprespawn(idx_self) then
		return
	end

	PrecacheModel("zombie")
	SetModel(idx_self, "zombie")
	entities[idx_self].anglesflags = ANGLES_KINEMATICANGLES_BIT | ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT
	kinematic_angles = entities[idx_self].angles:clone()
	kinematic_angles:setAngle(ANGLES_PITCH, 0)
	PhysicsCreateFromData(idx_self, PHYSICS_SHAPE_CAPSULE_Y, zombie_size, 40, entities[idx_self].origin, entities[idx_self].angles, kinematic_angles, false) -- TODO: get mass from model AABB and density?
	entities[idx_self].maxspeed = Vector3(3, 3, 3)
	entities[idx_self].acceleration = Vector3(PLAYER_INITIAL_ACCELERATION, PLAYER_INITIAL_ACCELERATION, PLAYER_INITIAL_ACCELERATION)

	entities[idx_self].health = 20
	entities[idx_self].max_health = 20
	entities[idx_self].die = "zombie_die"
	entities[idx_self].pain = "zombie_pain"

	SUB_SetAINextThink(idx_self, "zombie_think", math.random(0, 1000000) / 10000) -- don't let them all think at the same time TODO: tune the additional time

	entities[idx_self].weapons = WEAPON_FALCON2_BIT
	entities[idx_self].weapons_ammo = {}
	entities[idx_self].weapons_ammo[WEAPON_FALCON2_IDX] = 100
	WeaponSelect(idx_self, WEAPON_FALCON2_BIT, true)

	entities[idx_self].animation_manager_type = ANIMATION_MANAGER_HUMANOID

	enemies_commonspawn(idx_self)
end
