-- sv_weapons.lua

-- ============================================================================
-- 
-- Damage and scoring handling
-- 
-- TODO: multiply impulse in Sys_PhysicsApplyImpulse and Sys_PhysicsTraceline by
-- the inflictor's mass.
-- TODO: checking against origin in radius damage may cause LESS damage to very
-- big entities.
-- TODO: decrease acurracy depending on weapon, decrease acurracy more if running (depending on weapon too)
-- TODO: secondary fire modes, with possibly other types of ammo
-- TODO: crossbow: hold fire to fire faster, farther and causing more damage
-- 
-- ============================================================================

GAME_RADIUS_DAMAGE_PUSH_MULTIPLIER		= 10.0
GAME_HITSCAN_DAMAGE_PUSH_MULTIPLIER		= 2.0

-- ===================
-- ClientObituary
-- 
-- Displays messages and handles scoring.
-- 
-- TODO:
-- for this to work owner chains must be only one level deep
-- problems if player changes weapon after firing
-- etc... lots of false messages!
-- teamplay rules, here and elsewhere
-- ===================
function ClientObituary(victim, inflictor)
	if entities[inflictor].classname == "trigger_changelevel" then
		entities[victim].frags = entities[victim].frags - 1
		PrintB(entities[victim].netname .. " tried to exit.\n")
		return
	end

	if inflictor == gamestate.world or inflictor == victim or entities[inflictor].owner == victim then
		entities[victim].frags = entities[victim].frags - 1
		PrintB(entities[victim].netname .. " becomes bored with life.\n")
		return
	end

	if IsPlayer(inflictor) or IsPlayer(entities[inflictor].owner) then
		local killer
		if IsPlayer(entities[inflictor].owner) then
			killer = entities[inflictor].owner
		else
			killer = inflictor
		end

		PrintB(entities[victim].netname .. " was killed by " .. entities[killer].netname .. ".\n")
		entities[killer].frags = entities[killer].frags + 1
		return
	end

	-- failsafe
	PrintB(entities[victim].netname .. " died.\n")
end

-- ===================
-- DealDamage
-- 
-- VERY IMPORTANT: This should be the ONLY function to reduce health
-- Will make game_edict_t *self and *other valid for calling the die and pain functions.
-- 
-- TODO: do not damage things inflictor owns?
-- TODO: splash force according to damage?
-- TODO: ignore armor/damage only armor parameter (and for stuff which doesn't ignore, wee where it hits to decide if that part is uncovered by some armor type!)
-- ===================
function DealDamage(victim, inflictor, damage)
	if not entities[victim].takedamage then
		return
	end

	if isemptystring(entities[victim].die) then
		error("Entity " .. victim .. " (" .. entities[victim].classname .. ") can take damage but has no die function!\n")
	end

	if isemptystring(entities[victim].pain) then
		error("Entity " .. victim .. " (" .. entities[victim].classname .. ") can take damage but has no pain function!\n")
	end

	if entities[victim].health <= 0 then
		return -- already dead
	end

	-- /* armorlevel type armor TODO? */
	-- if (entities[victim].armor) /* absorb 50% damage TODO: armorlevel? */
	-- {
	-- 	entities[victim].armor -= damage * 0.5f;
	-- 	damage -= damage * 0.5f;
	-- 	if (entities[victim].armor < 0)
	-- 	{
	-- 		damage -= entities[victim].armor; /* minus with minus equals plus, give back negative armor to the damage amount */
	-- 		entities[victim].armor = 0;
	-- 	}
	-- }

	-- shield type armor
	if entities[victim].armor ~= nil then
		if entities[victim].armor > 0 then
			entities[victim].armor = entities[victim].armor - damage
			damage = 0
			if entities[victim].armor < 0 then
				damage = -entities[victim].armor -- minus with minus equals plus, give back negative armor to the damage amount
				entities[victim].armor = 0
			end
		end
	end

	entities[victim].health = entities[victim].health - damage

	-- TODO: do not remove other or entities[idx_other].owner before running the pain and death functions
	if entities[victim].health <= 0 then
		if IsPlayer(victim) then
			ClientObituary(victim, inflictor)
		end
		StartSoundB(GetModelSoundIndex(entities[victim].modelindex, SOUND_DIE), victim, CHAN_VOICE, 1, 1, 1, false)
		local diefunction = function_get(entities[victim].die)
		if diefunction == nil then
			error("Invalid die function for entity " .. victim .. ": " .. entities[victim].die .. "\n")
		elseif type(diefunction) == "function" then
			diefunction(victim, inflictor)
		else
			error("Invalid die function for entity " .. victim .. ": " .. entities[victim].die .. "\n")
		end
	else
		StartSoundB(GetModelSoundIndex(entities[victim].modelindex, SOUND_PAIN), victim, CHAN_VOICE, 1, 1, 1, false)
		local painfunction = function_get(entities[victim].pain)
		if painfunction == nil then
			error("Invalid pain function for entity " .. victim .. ": " .. entities[victim].pain .. "\n")
		elseif type(painfunction) == "function" then
			painfunction(victim, inflictor)
		else
			error("Invalid pain function for entity " .. victim .. ": " .. entities[victim].pain .. "\n")
		end
	end
end

-- ===================
-- DelayedImpulse
-- 
-- For splash impulse after entities die
-- TODO: stupid way and wastes entities, along with creation/destruction
-- TODO: THIS IS A HACK!
-- ===================
function DelayedImpulse(idx_self)
	if entities[idx_self].owner ~= nil then
		PhysicsApplyImpulse(entities[idx_self].owner, entities[idx_self].impulse, entities[idx_self].origin_localspace)
	end
	Free(idx_self)
end

-- ===================
-- ApplyRadiusDamage
-- 
-- To be called only by DealRadiusDamage and DealTouchDamage.
-- Set "apply_impulse" if an impulse proportional to the damage
-- should be applied in the inflictor -> victim direction
-- Returns true if damage was tried
-- ===================
function ApplyRadiusDamage(victim, inflictor, damage, radius, origin, apply_impulse)
	local dist
	local scalardist
	local normalizeddist
	local tracedir
	local trace, closest_hit_idx
	local tried_damage = false

	-- TODO FIXME: using this to know if an entity can be thrown by an explosion, but obviously an entity can have the physical representation disabled and maxspeed still be set. Fix for optimization
	if not PhysicsIsDynamic(victim) then
		return false
	end

	-- calculate the distance vector
	dist = entities[victim].origin - origin -- doing as end - start, for tracedir
	-- and see is how far it is
	scalardist = dist:len()
	if scalardist > radius then
		return false -- this may happen because entities are not points TODO: what to do here? still do some minimal damage? because some partes of the entity WILL be within radius
	end
	-- then normalize it
	normalizeddist = scalardist / radius

	if scalardist > 0 then -- TODO: use epsilon
		-- see if there is a line of sight
		tracedir = dist:normalized()
		trace, closest_hit_idx = PhysicsTraceline(inflictor, origin, tracedir, scalardist, true, 0)

		if closest_hit_idx ~= nil then
			if trace[closest_hit_idx].ent == victim then
				DealDamage(victim, inflictor, damage * (1 - normalizeddist)) -- deal damage according to distance TODO: inverse square?
				tried_damage = true

				if apply_impulse then
					-- TODO: assuming we only did one tracedir, modify this when doing various
					local impulse
					local hit_pos_victim_space
					local delayer
					delayer = Spawn({["classname"] = "delayer"})
					hit_pos_victim_space = Vector3(trace[closest_hit_idx].posx, trace[closest_hit_idx].posy, trace[closest_hit_idx].posz) - entities[victim].origin
					-- do impulse later, because the damage function may call the die function and eliminate angle locks TODO: impulse currently is on the center, so victim won't spin
					impulse = tracedir * damage * GAME_RADIUS_DAMAGE_PUSH_MULTIPLIER * (1 - normalizeddist) -- push entities according to distance TODO: inverse square?

					entities[delayer].owner = victim
					entities[delayer].impulse = impulse
					entities[delayer].origin_localspace = hit_pos_victim_space
					entities[delayer].nextthink1 = gamestate.time + 100
					entities[delayer].think1 = "DelayedImpulse"
				end
			end
		end
	else
		-- if entity is exactly at the explosion location, just damage it
		-- TODO: assuming we only did one tracedir, modify this when doing various
		-- nowhere to push if at the center of the entity
		DealDamage(victim, inflictor, damage)
		tried_damage = true
	end

	return tried_damage
end

-- ===================
-- DealRadiusDamage
-- 
-- For splash damage
-- "ignore" doesn't receive damage (useful if taking damage from a direct hit already)
-- 
-- TODO FIXME IMPORTANT:
-- VERY VERY VERY VERY SLOW. USE A TOUCH SPHERE TO TEST?
-- CHECK IF BLOCKED WITH MORE TRACELINES TO PREVENT FALSE NEGATIVES??!?!
-- Use varius tracelines from different points and/or ignore everything but world, doors, etc when determining if we are the closest one?
-- (currently any entity may block the splash damage, even dead bodies!)
-- ===================
function DealRadiusDamage(ignore, inflictor, damage, radius, origin)
	local victim = FindByRadius(0, radius, origin, false, true)

	while victim ~= nil do
		if victim ~= ignore then
			ApplyRadiusDamage(victim, inflictor, damage, radius, origin, true)
		end

		victim = entities[victim].chain
	end
end

-- ===================
-- DealTouchDamage
-- 
-- Should be used for the enemies/objects touch functions
-- If radius is set, will decrease damage according to distance (for explosions, for example, in which you can go inside)
-- ===================
function DealTouchDamage(victim, inflictor, damage, radius)
	local damage_done

	if (entities[victim].touchdamage_finished or 0) > gamestate.time then
		return
	end

	if radius <= 0 then
		DealDamage(victim, inflictor, damage)
		damage_done = true
	else
		damage_done = ApplyRadiusDamage(victim, inflictor, damage, radius, entities[inflictor].origin, false)
	end

	if damage_done then
		-- this should be the only function to set this!
		entities[victim].touchdamage_finished = gamestate.time + 300
	end
end

-- ============================================================================
-- 
-- Weapon fire auxiliar functions
-- 
-- FIXME: How to handle projectiles touching more than one entity in the same
-- frame? Damage them all or only the first/closest one?
-- 
-- ============================================================================

EXPLOSION_DAMAGE						= 30
EXPLOSION_RADIUS						= 3
NBOMB_DETECTION_RADIUS					= 2.5 -- to let the player go closer
MUZZLEFLASH_INTENSITY					= -0.5

STATE_NOT_TOUCHED						= 0
STATE_TOUCHED							= 1

-- ===================
-- ProjectileFire
-- 
-- Generic projectile spawning code
-- dir_fraction is the multiplier applied to dir to decide where the projectile will spawn
-- TODO: need to REALLY ignore touchs with owner, otherwise the projectile's path may change after a collision with its owner (and no explosion).
-- ===================
function ProjectileFire(idx_self, info)
	local ent
	local spawnorigin

	-- TODO: not always exaclty entities[idx_self].cameraent->origin or entities[idx_self].origin
	-- TODO: if we are seeing throught a camera or something else, the fire will come from there!! stop it from firing
	-- TODO: if the network code ALWAYS sends entities who have light, the player will be visible to hackers from anywhere now...
	if entities[idx_self].cameraent ~= nil and entities[idx_self].cameraent ~= gamestate.world then
		spawnorigin = entities[entities[idx_self].cameraent].origin + info.dir * info.dir_fraction
		spawnorigin = spawnorigin + entities[idx_self].up * -0.45
	else
		spawnorigin = entities[idx_self].origin + info.dir * info.dir_fraction
	end
	spawnorigin = spawnorigin + info.origin_adjust
	ent = Spawn({["classname"] = "projectile", ["origin"] = tostring(spawnorigin), ["angles"] = tostring(info.angles), ["model"] = info.model})

	-- TODO: use model... get radius and mass from model AABB and density?
	PhysicsCreateFromData(ent, PHYSICS_SHAPE_SPHERE, info.radius, info.mass, entities[ent].origin, entities[ent].angles, nil, false)

	-- inherit velocity
	PhysicsSetLinearVelocity(ent, entities[idx_self].velocity + info.dir * info.initial_speed)
	if not info.initial_avelocity:iszero() then
		local self_rotationmatrix, rotate_avel
		self_rotationmatrix = Matrix4x4()
		-- TODO: currently rotating from the owner dir, should rotate from the projectile dir?
		self_rotationmatrix = self_rotationmatrix:RotateFromVectors(entities[idx_self].forward, entities[idx_self].right, entities[idx_self].up)
		rotate_avel = self_rotationmatrix:applyToVector3(info.initial_avelocity)
		PhysicsSetAngularVelocity(ent, entities[idx_self].avelocity + rotate_avel)
	end
	if isemptynumber(info.initial_speed) then -- if set, projectile will be MOVETYPE_FREE
		entities[ent].movetype = MOVETYPE_FLY
		if info.set_viewent_and_cmdent then -- if set, will be slower if a MOVETYPE_FLY projectile
			entities[ent].movecmd.z = -1
			-- TODO: with the current physics code will make it go faster in corners
			entities[ent].maxspeed = Vector3(10, 10, 10)
			entities[ent].acceleration = Vector3(4, 4, 4)
			entities[ent].ignore_gravity = true
			entities[ent].anglesflags = entities[ent].anglesflags | ANGLES_KINEMATICANGLES_BIT -- permit control
		else
			entities[ent].movecmd.z = -1
			-- TODO: with the current physics code will make it go faster in corners
			entities[ent].maxspeed = Vector3(96, 96, 96)
			entities[ent].acceleration = Vector3(1024, 1024, 1024)
		end
	else
		entities[ent].movetype = MOVETYPE_FREE
	end

	PhysicsSetSolidState(ent, SOLID_ENTITY)
	entities[ent].takedamage = true
	entities[ent].health = 1
	entities[ent].die = info.diefn
	entities[ent].pain = info.diefn

	entities[ent].touch = info.touchfn

	if isemptystring(info.touchfn) then
		entities[ent].nextthink1 = gamestate.time + 500 -- wait a little more if we do not have a touch function
	else
		entities[ent].nextthink1 = gamestate.time + 1
	end
	entities[ent].think1 = info.thinkfn

	if not info.set_viewent_and_cmdent then
		entities[ent].nextthink2 = gamestate.time + 300000 -- five minutes
		entities[ent].think2 = info.diefn
	end

	entities[ent].owner = info.owner

	entities[ent].light_intensity = MUZZLEFLASH_INTENSITY * info.light_multiplier

	if info.set_viewent_and_cmdent then
		entities[idx_self].viewent = ent
		entities[idx_self].cmdent = ent
		if entities[idx_self].buttoncmd & BUTTONCMD_FIRE == BUTTONCMD_FIRE then
			entities[ent].counter = 1 -- to prevent instant explode TODO: still needed now that we have cmdent?
		end
	end
	
	entities[ent].state = STATE_NOT_TOUCHED
end

-- ===================
-- ProjectileThink
-- 
-- Projectile maintenance
-- ===================
function ProjectileThink(idx_self)
	-- TODO FIXME: when a player quits the server, the slayer becomes a nbomb!!!
	if entities[idx_self].owner and entities[entities[idx_self].owner].cmdent == idx_self then
		-- TODO: search the code for this operator precedence error:
		-- WRONG:
		-- if (!entities[idx_self].buttoncmd & BUTTONCMD_FIRE)
		-- RIGHT:
		-- if (!(entities[idx_self].buttoncmd & BUTTONCMD_FIRE))

		if entities[idx_self].buttoncmd & BUTTONCMD_FIRE == 0 then
			entities[idx_self].counter = 0
		end
		if (entities[idx_self].buttoncmd & BUTTONCMD_FIRE == BUTTONCMD_FIRE) and isemptynumber(entities[idx_self].counter) then
			-- firing again will explode
			local diefunction = function_get(entities[idx_self].die)
			if diefunction == nil then
				error("Invalid die function for entity " .. idx_self .. ": " .. entities[idx_self].die .. "\n")
			elseif type(diefunction) == "function" then
				diefunction(idx_self, nil)
			else
				error("Invalid die function for entity " .. idx_self .. ": " .. entities[idx_self].die .. "\n")
			end
			return
		end
		-- no sideways or up/down move allowed
		entities[idx_self].movecmd.x = 0
		entities[idx_self].movecmd.y = 0

		-- always go forward, just set min/max speed
		if entities[idx_self].movecmd.z <= 0 then
			entities[idx_self].maxspeed = Vector3(10, 10, 10)
			entities[idx_self].acceleration = Vector3(8, 8, 8)
		else
			entities[idx_self].maxspeed = Vector3(1, 1, 1)
			entities[idx_self].acceleration = Vector3(2, 2, 2)
		end
		entities[idx_self].movecmd.z = -1

		-- run every frame
		entities[idx_self].nextthink1 = gamestate.time + 1
		entities[idx_self].think1 = "ProjectileThink"
		return
	else
		-- proximity + movement threshold
		if entities[idx_self].velocity:lenSq() < 0.1 then -- wait for it to settle TODO: we may move after this being true (high point of a parabola in a upward throw, for example)
			local victim = FindByRadius(0, NBOMB_DETECTION_RADIUS, entities[idx_self].origin, false, true)

			while victim ~= nil do
				if victim == idx_self then
					victim = entities[victim].chain
				elseif not PhysicsIsDynamic(victim) then -- TODO FIXME: using this to know if an entity can be thrown by an explosion, but obviously an entity can have the physical representation disabled and maxspeed still be set. Fix for optimization
					victim = entities[victim].chain
				elseif entities[victim].velocity:lenSq() > 0.01 then
					local diefunction = function_get(entities[idx_self].die)
					if diefunction == nil then
						error("Invalid die function for entity " .. idx_self .. ": " .. entities[idx_self].die .. "\n")
					elseif type(diefunction) == "function" then
						diefunction(idx_self, nil)
					else
						error("Invalid die function for entity " .. idx_self .. ": " .. entities[idx_self].die .. "\n")
					end
					return
				else
					victim = entities[victim].chain
				end
			end

			-- check every half second
			entities[idx_self].nextthink1 = gamestate.time + 100
			entities[idx_self].think1 = "ProjectileThink"
		else
			-- run every frame
			entities[idx_self].nextthink1 = gamestate.time + 1
			entities[idx_self].think1 = "ProjectileThink"
		end

		return
	end
end

-- ===================
-- ExplosionThink
-- 
-- For effects and removal
-- ===================
function ExplosionThink(idx_self)
	entities[idx_self].counter = entities[idx_self].counter - gamestate.frametime
	if entities[idx_self].counter <= 0 then
		Free(idx_self)
	else
		entities[idx_self].light_intensity = MUZZLEFLASH_INTENSITY * EXPLOSION_RADIUS * (math.random(80000, 100000) / 100000.0)
		entities[idx_self].nextthink1 = gamestate.time + 1
		entities[idx_self].think1 = "ExplosionThink"
	end
end

-- ===================
-- ExplosionTouch
-- 
-- For damage
-- ===================
function ExplosionTouch(idx_self, idx_other)
	DealTouchDamage(idx_other, idx_self, EXPLOSION_DAMAGE, EXPLOSION_RADIUS)
end

-- ===================
-- CreateExplosion
-- 
-- Creates an explosion
-- ===================
function CreateExplosion(owner, origin, angles)
	local ent = Spawn({["classname"] = "explosion", ["origin"] = tostring(origin), ["angles"] = tostring(angles), ["model"] = "energyexplosion"})

	entities[ent].touch = "ExplosionTouch"
	entities[ent].nextthink1 = gamestate.time + 1
	entities[ent].think1 = "ExplosionThink"
	entities[ent].counter = 3000
	entities[ent].owner = owner

	PhysicsCreateFromData(ent, PHYSICS_SHAPE_SPHERE, EXPLOSION_RADIUS, -1, origin, angles, nil, false)
	PhysicsSetSolidState(ent, SOLID_WORLD_TRIGGER)
end

-- ===================
-- EnergyBallExplode
-- 
-- Makes Ka-boom!
-- ===================
function EnergyBallExplode(idx_self, idx_other)
	local explosion_origin

	if entities[idx_self].state == STATE_TOUCHED then -- we are using this to signal that we are coming from a touch function
		-- we may be already past the touch point because of possible physics subframes, so use the stored value

		-- since we are not point-size, back up a little bit TODO: go back by a radius instead of penetration + fixed amount
		-- TODO FIXME: sometimes still pass through walls
		if entities[idx_self].contact_distance < 0.1 then
			explosion_origin = entities[idx_self].contact_pos + entities[idx_self].contact_normal * (-entities[idx_self].contact_distance + 0.1)
		else
			explosion_origin = entities[idx_self].contact_pos:clone()
		end
	elseif entities[idx_self].state == STATE_NOT_TOUCHED then
		-- we were killed or had a timeout, use our origin
		explosion_origin = entities[idx_self].origin:clone()
	else
		error("EnergyBallExplode: unknown state " .. entities[idx_self].state .. "\n")
	end

	-- TODO: is this part emulating a die function?

	-- start sound
	StartSoundB(GetModelSoundIndex(entities[idx_self].modelindex, SOUND_DIE), idx_self, CHAN_BODY, 1, 2, 0.5, false)

	-- prevent unexpected stuff
	entities[idx_self].takedamage = false
	entities[idx_self].health = 0

	SetModel(idx_self, "null")
	-- we can't just become an energy explosion because inertia could send the explosion away, so create a new entity for that
	-- do not use origin, it may have gone farther after touching
	-- do some instant damage to push the objects
	DealRadiusDamage(idx_self, idx_self, EXPLOSION_DAMAGE, EXPLOSION_RADIUS, explosion_origin)
	-- create explosion for continuous damage
	CreateExplosion(entities[idx_self].owner, explosion_origin, entities[idx_self].angles) -- TODO: use contact normal?

	-- as always, can't remove self in the middle of touch functions TODO: create protections for that
	entities[idx_self].nextthink1 = gamestate.time + 1
	entities[idx_self].think1 = "SUB_Remove"
	entities[idx_self].nextthink2 = gamestate.time
	entities[idx_self].think2 = nil
	entities[idx_self].visible = VISIBLE_NO -- physics may jump us around
	PhysicsDestroy(idx_self)

	-- start particle effect
	StartParticleB(PARTICLE_EXPLOSION, explosion_origin)

	-- reset the owner's camera and commands, if necessary
	if entities[entities[idx_self].owner].viewent == idx_self then
		entities[entities[idx_self].owner].viewent = entities[entities[idx_self].owner].cameraent
	end
	if entities[entities[idx_self].owner].cmdent == idx_self then
		entities[entities[idx_self].owner].cmdent = entities[idx_self].owner
	end
end

-- ===================
-- EnergyBallTouch
-- 
-- When it touches something
-- ===================
function EnergyBallTouch(idx_self, idx_other)
	if entities[idx_self].think1 == "SUB_Remove" then -- already touched someone
		return
	end

	if not entities[idx_self].contact_reaction then -- don't explode on items, etc
		return
	end

	if idx_other == entities[idx_self].owner or entities[idx_other].owner == entities[idx_self].owner then -- TODO: move this for sys_physics.c, it's for everyone
		return
	end

	entities[idx_self].state = STATE_TOUCHED -- use entities[idx_self].state to signal that we are coming from a touch function
	EnergyBallExplode(idx_self, idx_other)

	-- reflect a new ball
	-- TODO
	-- vec3_t newangles, newforward;
	-- Math_ReflectVectorAroundNormal(entities[idx_self].forward, entities[idx_self].contact_normal, newforward);
	-- Math_VecForwardToAngles(newforward, newangles);
	-- ProjectileFire("energyball", newforward, 0.6f, newangles, "EnergyBallTouch", "", &gamestate.entities[entities[idx_self].owner]);
end

-- ===================
-- RemoveMuzzleFlash
-- 
-- Think function for an entity that will remove the light intensity from itself and then remove itself
-- ===================
function RemoveMuzzleFlash(idx_self)
	entities[idx_self].light_intensity = 0
	Free(idx_self)
end

-- ===================
-- HitscanFire
-- 
-- Generic hitscan firing code. Beware of very high "distance".
-- If "reduce_with_distance" is true, the damage done will reduce linearly from 100% with o distance to 0% at "distance"
-- TODO: conserve impulse because entities may be re-transformed after dying, losing the dying shot impulse and falling to the wrong side.
-- ===================
function HitscanFire(idx_self, dir, damage, distance, has_muzzleflash, reduce_with_distance)
	local tmp
	local hit
	local trace, closest_hit_idx

	-- muzzleflash TODO: SLOW, need to be positioned in the tip of the barrel for better light effects
	if has_muzzleflash then
		tmp = Spawn({["classname"] = "remove_muzzleflash"})
		entities[tmp].nextthink1 = gamestate.time + 1 -- just 1 frame
		entities[tmp].think1 = "RemoveMuzzleFlash"
		entities[tmp].light_intensity = MUZZLEFLASH_INTENSITY -- TODO: send as SVC
	end

	-- TODO: not always exaclty entities[idx_self].cameraent->origin or entities[idx_self].origin
	-- TODO: if we are seeing throught a camera or something else, the fire will come from there!! stop it from firing
	-- TODO: if the network code ALWAYS sends entities who have light, the player will be visible to hackers from anywhere now...
	if entities[idx_self].cameraent ~= nil and entities[idx_self].cameraent ~= gamestate.world then
		trace, closest_hit_idx = PhysicsTraceline(idx_self, entities[entities[idx_self].cameraent].origin, dir, distance, true, damage * GAME_HITSCAN_DAMAGE_PUSH_MULTIPLIER)
		if has_muzzleflash then
			SetTransform(tmp, entities[entities[idx_self].cameraent].origin, nil, nil)
		end
	else
		trace, closest_hit_idx = PhysicsTraceline(idx_self, entities[idx_self].origin, dir, distance, true, damage * GAME_HITSCAN_DAMAGE_PUSH_MULTIPLIER)
		if has_muzzleflash then
			SetTransform(tmp, entities[idx_self].origin, nil, nil)
		end
	end

	if closest_hit_idx ~= nil then -- if we did go all the way in the trace
		hit = trace[closest_hit_idx].ent

		-- TODO: more different particles depending on material
		if entities[hit].takedamage then -- TODO: keep takedamage of dead entities and create the entities[idx_self].dead = {DEAD_NO, DEAD_DYING, DEAD_DEAD} to handle death, allowing blood to keep coming out when shooting dead entities
			local fraction
			if reduce_with_distance then
				fraction = 1.0 - trace[closest_hit_idx].fraction
			else
				fraction = 1
			end
			DealDamage(hit, idx_self, damage * fraction)
			StartParticleB(PARTICLE_BLOOD, Vector3(trace[closest_hit_idx].posx, trace[closest_hit_idx].posy, trace[closest_hit_idx].posz)) -- TODO: intensity depending on damage
		else
			StartParticleB(PARTICLE_GUNSHOT, Vector3(trace[closest_hit_idx].posx, trace[closest_hit_idx].posy, trace[closest_hit_idx].posz)) -- TODO: intensity depending on damage
		end
	end
end

-- ===================
-- NailDie
-- 
-- Deletes the nail
-- ===================
function NailDie(idx_self, idx_other)
	-- we were killed or had a timeout

	-- prevent unexpected stuff
	entities[idx_self].takedamage = false
	entities[idx_self].health = 0

	SetModel(idx_self, "null")

	-- as always, can't remove self in the middle of touch functions TODO: create protections for that
	entities[idx_self].nextthink1 = gamestate.time + 1
	entities[idx_self].think1 = "SUB_Remove"
	entities[idx_self].nextthink2 = gamestate.time
	entities[idx_self].think2 = nil
	entities[idx_self].touch = nil
	entities[idx_self].visible = VISIBLE_NO -- physics may jump us around
	PhysicsDestroy(idx_self)
end

-- ===================
-- NailTouch
-- 
-- When it touches something
-- ===================
function NailTouch(idx_self, idx_other)
	local hit_origin

	if not entities[idx_self].contact_reaction then -- don't hit contactless items, etc
		return
	end

	if idx_other == entities[idx_self].owner or entities[idx_other].owner == entities[idx_self].owner then -- TODO: move this for sys_physics.c, it's for everyone
		return
	end

	-- we may be already past the touch point because of possible physics subframes, so use the stored value

	-- since we are not point-size, back up a little bit TODO: go back by a radius instead of penetration + fixed amount
	-- TODO FIXME: sometimes still pass through walls
	if entities[idx_self].contact_distance < 0.1 then
		hit_origin = entities[idx_self].contact_normal * (-entities[idx_self].contact_distance + 0.1) + entities[idx_self].contact_pos
	else
		hit_origin = entities[idx_self].contact_pos:clone()
	end

	-- start sound
	StartSoundB(GetModelSoundIndex(entities[idx_self].modelindex, SOUND_DIE), idx_self, CHAN_BODY, 1, 1, 1, false)

	-- TODO: more different particles depending on material
	if entities[idx_other].takedamage then -- TODO: keep takedamage of dead entities and create the self->dead = {DEAD_NO, DEAD_DYING, DEAD_DEAD} to handle death, allowing blood to keep coming out when shooting dead entities
		DealDamage(idx_other, idx_self, 10)
		StartParticleB(PARTICLE_BLOOD, hit_origin) -- TODO: intensity depending on damage
	else
		StartParticleB(PARTICLE_GUNSHOT, hit_origin) -- TODO: intensity depending on damage
	end

	NailDie(idx_self, idx_other)
end

-- ===================
-- InstantExplosionThink
-- 
-- For effects and removal
-- ===================
function InstantExplosionThink(idx_self)
	if AnimationTimeLeft(idx_self, ANIMATION_SLOT_ALLJOINTS) <= 0 then
		FreeEdict(idx_self)
	else
		entities[idx_self].light_intensity = MUZZLEFLASH_INTENSITY * EXPLOSION_RADIUS * (math.random(80000, 100000) / 100000)
		entities[idx_self].nextthink1 = gamestate.time + 1
		entities[idx_self].think1 = "InstantExplosionThink"
	end
end

-- ===================
-- CreateInstantExplosion
-- 
-- Creates an instant explosion
-- ===================
function CreateInstantExplosion(owner, origin, angles)
	local ent = Spawn({["classname"] = "instantexplosion", ["origin"] = tostring(origin), ["angles"] = tostring(angles), ["model"] = "s_explod"})

	entities[ent].nextthink1 = gamestate.time + 1
	entities[ent].think1 = "InstantExplosionThink"
	entities[ent].owner = owner

	AnimationStart(ent, ANIMATION_FIRE, ANIMATION_SLOT_ALLJOINTS, true, 0)
end

-- ===================
-- GrenadeExplode
-- 
-- Makes Ka-boom!
-- ===================
function GrenadeExplode(idx_self, idx_other)
	local explosion_origin

	if entities[idx_self].state == STATE_TOUCHED then -- we are using this to signal that we are coming from a touch function
		-- we may be already past the touch point because of possible physics subframes, so use the stored value

		-- since we are not point-size, back up a little bit TODO: go back by a radius instead of penetration + fixed amount
		-- TODO FIXME: sometimes still pass through walls
		if entities[idx_self].contact_distance < 0.1 then
			explosion_origin = entities[idx_self].contact_normal * (-entities[idx_self].contact_distance + 0.1) + entities[idx_self].contact_pos
		else
			explosion_origin = entities[idx_self].contact_pos:clone()
		end
	elseif entities[idx_self].state == STATE_NOT_TOUCHED then
		-- we were killed or had a timeout, use our origin
		explosion_origin = entities[idx_self].origin:clone()
	else
		error("EnergyBallExplode: unknown state " .. entities[idx_self].state .. "\n")
	end

	-- start sound
	StartSoundB(GetModelSoundIndex(entities[idx_self].modelindex, SOUND_DIE), idx_self, CHAN_BODY, 1, 2, 0.5, false)

	-- prevent unexpected stuff
	entities[idx_self].takedamage = false
	entities[idx_self].health = 0

	SetModel(idx_self, "null")
	-- we can't just become an energy explosion because inertia could send the explosion away, so create a new entity for that
	-- do not use origin, it may have gone farther after touching
	-- do some instant damage to push the objects
	DealRadiusDamage(idx_self, idx_self, EXPLOSION_DAMAGE * 3.5, EXPLOSION_RADIUS, explosion_origin)
	-- create visual representation
	CreateInstantExplosion(entities[idx_self].owner, explosion_origin, entities[idx_self].angles) -- TODO: use contact normal?

	-- as always, can't remove self in the middle of touch functions TODO: create protections for that
	entities[idx_self].nextthink1 = gamestate.time + 1
	entities[idx_self].think1 = "SUB_Remove"
	entities[idx_self].nextthink2 = gamestate.time
	entities[idx_self].think2 = nil
	entities[idx_self].visible = VISIBLE_NO -- physics may jump us around
	PhysicsDestroy(idx_self)

	-- start particle effect
	StartParticleB(PARTICLE_EXPLOSION, explosion_origin)
end

-- ===================
-- GrenadeTouch
-- 
-- When it touches something
-- ===================
function GrenadeTouch(idx_self, idx_other)
	if entities[idx_self].think1 == "SUB_Remove" then -- already touched someone
		return
	end

	if not entities[idx_self].contact_reaction then -- don't explode on items, etc
		return
	end

	if idx_other == entities[idx_self].owner or entities[idx_other].owner == entities[idx_self].owner then -- TODO: move this for sys_physics.c, it's for everyone
		return
	end

	-- explode only when hitting the floor or anything with the normal almost up
	if entities[idx_self].contact_normal.y > math.sin(math.rad(80)) or (entities[idx_self].counter and entities[idx_self].counter >= 5) then
		entities[idx_self].state = STATE_TOUCHED -- use entities[idx_self].state to signal that we are coming from a touch function
		GrenadeExplode(idx_self, idx_other)
	else
		StartSoundB(GetModelSoundIndex(entities[idx_self].modelindex, SOUND_PAIN), idx_self, CHAN_BODY, 1, 1, 1, false)
	end

	if entities[idx_self].counter then
		entities[idx_self].counter = entities[idx_self].counter + 1
	else
		entities[idx_self].counter = 1
	end
end

-- ===================
-- RocketTouch
-- 
-- When it touches something
-- ===================
function RocketTouch(idx_self, idx_other)
	if entities[idx_self].think1 == "SUB_Remove" then -- already touched someone
		return
	end

	if not entities[idx_self].contact_reaction then -- don't explode on items, etc
		return
	end

	if idx_other == entities[idx_self].owner or entities[idx_other].owner == entities[idx_self].owner then -- TODO: move this for sys_physics.c, it's for everyone
		return
	end

	entities[idx_self].state = STATE_TOUCHED -- use entities[idx_self].state to signal that we are coming from a touch function
	GrenadeExplode(idx_self, idx_other)
end

-- =====================================================
--
-- Weapon handling
--
-- TODO: sync player model animation to weapon viewmodel animation
-- TODO: tags in model joints for vweps (error if animation duration from player model is different from animation duration in viewmodel?) also have a fire animation in both the player model and the vwep attached to it
--
-- =====================================================

-- ===================
-- PunchFire
-- 
-- Weapon fire function
-- ===================
function PunchFire(idx_self)
	HitscanFire(idx_self, entities[idx_self].forward, 10, 0.5, false, false)
end

-- ===================
-- Falcon2Fire
-- 
-- Weapon fire function
-- ===================
function Falcon2Fire(idx_self)
	HitscanFire(idx_self, entities[idx_self].forward, 15, 100, true, true)
end

-- ===================
-- CrossbowFire
-- 
-- Weapon fire function
-- ===================
function CrossbowFire(idx_self)
	-- TODO: particle effect to simulate trajectory
	HitscanFire(idx_self, entities[idx_self].forward, 50, 20, false, true)
end

-- ===================
-- SuperDragonFire
-- 
-- Weapon fire function
-- ===================
function SuperDragonFire(idx_self)
	local proj = {}

	proj.model = "energyball"
	proj.dir = entities[idx_self].forward:clone()
	proj.dir_fraction = 0.375
	proj.origin_adjust = Vector3()
	proj.angles = entities[idx_self].angles:clone()
	proj.initial_speed = 20
	proj.initial_avelocity = Vector3()
	proj.touchfn = "EnergyBallTouch"
	proj.diefn = "EnergyBallExplode"
	proj.thinkfn = nil
	proj.owner = idx_self
	proj.light_multiplier = 0
	proj.radius = 0.15
	proj.mass = 0.5
	proj.set_viewent_and_cmdent = false
	ProjectileFire(idx_self, proj)
end

-- ===================
-- CMP150Fire
-- 
-- Weapon fire function
-- ===================
function CMP150Fire(idx_self)
	HitscanFire(idx_self, entities[idx_self].forward, 10, 100, true, true)
end

-- ===================
-- NBombFire
-- 
-- Weapon fire function
-- ===================
function NBombFire(idx_self)
	local proj= {}

	proj.model = "energyball"
	proj.dir = entities[idx_self].forward:clone()
	proj.dir_fraction = 0.375
	proj.origin_adjust = Vector3()
	proj.angles = entities[idx_self].angles:clone()
	proj.initial_speed = 20
	proj.initial_avelocity = Vector3()
	proj.touchfn = nil
	proj.diefn = "EnergyBallExplode"
	proj.thinkfn = "ProjectileThink"
	proj.owner = idx_self
	proj.light_multiplier = 0
	proj.radius = 0.15
	proj.mass = 0.5
	proj.set_viewent_and_cmdent = false
	ProjectileFire(idx_self, proj)
end

-- ===================
-- SlayerFire
-- 
-- Weapon fire function
-- ===================
function SlayerFire(idx_self)
	local proj = {}

	proj.model = "energyball"
	proj.dir = entities[idx_self].forward:clone()
	proj.dir_fraction = 0.375
	proj.origin_adjust = Vector3()
	proj.angles = entities[idx_self].angles:clone()
	proj.initial_speed = 0
	proj.initial_avelocity = Vector3()
	proj.touchfn = "EnergyBallTouch"
	proj.diefn = "EnergyBallExplode"
	proj.thinkfn = "ProjectileThink"
	proj.owner = idx_self
	proj.light_multiplier = 0
	proj.radius = 0.1
	proj.mass = 0.5
	proj.set_viewent_and_cmdent = true
	ProjectileFire(idx_self, proj)
end

-- ===================
-- EnergyBallFire
-- 
-- Weapon fire function
-- ===================
function EnergyBallFire(idx_self)
	local proj = {}

	proj.model = "energyball"
	proj.dir = entities[idx_self].forward:clone()
	proj.dir_fraction = 0.375
	proj.origin_adjust = Vector3()
	proj.angles = entities[idx_self].angles:clone()
	proj.initial_speed = 0
	proj.initial_avelocity = Vector3()
	proj.touchfn = "EnergyBallTouch"
	proj.diefn = "EnergyBallExplode"
	proj.thinkfn = nil
	proj.owner = idx_self
	proj.light_multiplier = 1
	proj.radius = 0.1
	proj.mass = 0.1
	proj.set_viewent_and_cmdent = false
	ProjectileFire(idx_self, proj)
end

-- ===================
-- AxeFire
-- 
-- Weapon fire function
-- ===================
function AxeFire(idx_self)
	HitscanFire(idx_self, entities[idx_self].forward, 40, 0.8, false, false)
end

-- ===================
-- ShotgunFire
-- 
-- Weapon fire function
-- ===================
function ShotgunFire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	

	for i = 0, 5 do
		HitscanFire(idx_self, entities[idx_self].forward + entities[idx_self].right * (math.random(-10000, 10000) / 100000 + velocity_inaccuracy_x) + entities[idx_self].up * (math.random(-10000, 10000) / 100000 + velocity_inaccuracy_y), 10, 100, true, true)
	end
end

-- ===================
-- SuperShotgunFire
-- 
-- Weapon fire function
-- ===================
function SuperShotgunFire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	

	for i = 0, 13 do
		HitscanFire(idx_self, entities[idx_self].forward + entities[idx_self].right * (math.random(-10000, 10000) / 50000 + velocity_inaccuracy_x) + entities[idx_self].up * (math.random(-10000, 10000) / 50000 + velocity_inaccuracy_y), 10, 100, true, true)
	end
	-- TODO: remove two shells per fire
end

-- ===================
-- NailgunFire
-- 
-- Weapon fire function
-- ===================
function NailgunFire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	
	
	local proj = {}
	proj.model = "s_spike"
	proj.dir = entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * velocity_inaccuracy_y
	proj.dir_fraction = 0.375
	if IsPlayer(idx_self) and entities[idx_self].cameraent ~= nil then
		if entities[entities[idx_self].cameraent].state & 1 == 1 then
			proj.origin_adjust = entities[idx_self].right * 0.15
		else
			proj.origin_adjust = entities[idx_self].right * -0.15
		end
	else
		proj.origin_adjust = Vector3()
	end
	proj.angles = MathVecForwardToAngles(proj.dir)
	proj.initial_speed = 0
	proj.initial_avelocity = Vector3()
	proj.touchfn = "NailTouch"
	proj.diefn = "NailDie"
	proj.thinkfn = nil
	proj.owner = idx_self
	proj.light_multiplier = 0
	proj.radius = 0.05
	proj.mass = 0.1
	proj.set_viewent_and_cmdent = false
	ProjectileFire(idx_self, proj)
	-- TODO: should not die inside explosions, fire from the two sides, etc
end

-- ===================
-- SuperNailgunFire
-- 
-- Weapon fire function
-- ===================
function SuperNailgunFire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	
	
	local proj = {}
	proj.model = "s_spike"
	proj.dir = entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * velocity_inaccuracy_y
	proj.dir_fraction = 0.375
	proj.origin_adjust = Vector3()
	proj.angles = MathVecForwardToAngles(proj.dir)
	proj.initial_speed = 0
	proj.initial_avelocity = Vector3()
	proj.touchfn = "NailTouch"
	proj.diefn = "NailDie"
	proj.thinkfn = nil
	proj.owner = idx_self
	proj.light_multiplier = 0
	proj.radius = 0.05
	proj.mass = 0.1
	proj.set_viewent_and_cmdent = false
	ProjectileFire(idx_self, proj)
	-- TODO: should not die inside explosions, etc
end

-- ===================
-- GrenadeLauncherFire
-- 
-- Weapon fire function
-- ===================
function GrenadeLauncherFire(idx_self)
local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	
	
	local proj = {}
	proj.model = "grenade"
	proj.dir = entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * (velocity_inaccuracy_y + 0.3)
	proj.dir_fraction = 0.375
	proj.origin_adjust = Vector3()
	proj.angles = MathVecForwardToAngles(proj.dir)
	proj.initial_speed = 60
	proj.initial_avelocity = Vector3(-5, 0, 0)
	proj.touchfn = "GrenadeTouch"
	proj.diefn = "GrenadeExplode"
	proj.thinkfn = nil
	proj.owner = idx_self
	proj.light_multiplier = 0
	proj.radius = 0.15
	proj.mass = 0.5
	proj.set_viewent_and_cmdent = false
	ProjectileFire(idx_self, proj)
end

-- ===================
-- RocketLauncherFire
-- 
-- Weapon fire function
-- ===================
function RocketLauncherFire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	
	
	local proj = {}
	proj.model = "missile"
	proj.dir = entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * velocity_inaccuracy_y
	proj.dir_fraction = 0.375
	proj.origin_adjust = Vector3()
	proj.angles = MathVecForwardToAngles(proj.dir)
	proj.initial_speed = 0
	proj.initial_avelocity = Vector3()
	proj.touchfn = "RocketTouch"
	proj.diefn = "GrenadeExplode"
	proj.thinkfn = nil
	proj.owner = idx_self
	proj.light_multiplier = 1
	proj.radius = 0.1
	proj.mass = 0.1
	proj.set_viewent_and_cmdent = false
	ProjectileFire(idx_self, proj)
end

-- ===================
-- ThunderboltFire
-- 
-- Weapon fire function
-- ===================
function ThunderboltFire(idx_self)
end

-- ===================
-- AQ2MK23Fire
-- 
-- Weapon fire function
-- ===================
function AQ2MK23Fire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	

	HitscanFire(idx_self, entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * velocity_inaccuracy_y, 10, 100, true, true)
end

-- ===================
-- AQ2HandCannonFire
-- 
-- Weapon fire function
-- ===================
function AQ2HandCannonFire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	

	for i = 0, 19 do
		HitscanFire(idx_self, entities[idx_self].forward + entities[idx_self].right * (math.random(-10000, 10000) / 50000 + velocity_inaccuracy_x) + entities[idx_self].up * (math.random(-10000, 10000) / 50000 + velocity_inaccuracy_y), 10, 100, true, true)
	end
	-- TODO: remove two shells per fire
end

-- ===================
-- AQ2M61FragFire
-- 
-- Weapon fire function
-- ===================
function AQ2M61FragFire(idx_self)
local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	
	
	local proj = {}
	proj.model = "aq2grenade"
	proj.dir = entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * (velocity_inaccuracy_y + 0.3)
	proj.dir_fraction = 0.375
	proj.origin_adjust = Vector3()
	proj.angles = MathVecForwardToAngles(proj.dir)
	proj.initial_speed = 60
	proj.initial_avelocity = Vector3(-5, 0, 0)
	proj.touchfn = "GrenadeTouch"
	proj.diefn = "GrenadeExplode"
	proj.thinkfn = nil
	proj.owner = idx_self
	proj.light_multiplier = 0
	proj.radius = 0.15
	proj.mass = 0.5
	proj.set_viewent_and_cmdent = false
	ProjectileFire(idx_self, proj)
end

-- ===================
-- AQ2KnifeFire
-- 
-- Weapon fire function
-- ===================
function AQ2KnifeFire(idx_self)
	local ammo_idx = gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX].ammo_type
	if entities[idx_self].ammo[ammo_idx] == 1 then
		HitscanFire(idx_self, entities[idx_self].forward, 40, 0.8, false, false)
		entities[idx_self].ammo[ammo_idx] = entities[idx_self].ammo[ammo_idx] + 1
	else
		local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
		local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
		if velocity_inaccuracy_x < -0.1 then
			velocity_inaccuracy_x = -0.1
		elseif velocity_inaccuracy_x > 0.1 then
			velocity_inaccuracy_x = 0.1
		end
		if velocity_inaccuracy_y < -0.1 then
			velocity_inaccuracy_y = -0.1
		elseif velocity_inaccuracy_y > 0.1 then
			velocity_inaccuracy_y = 0.1
		end	
		
		local proj = {}
		proj.model = "g_aq2knife"
		proj.dir = entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * velocity_inaccuracy_y
		proj.dir_fraction = 0.375
		proj.origin_adjust = Vector3()
		proj.angles = MathVecForwardToAngles(proj.dir)
		proj.initial_speed = 0
		proj.initial_avelocity = Vector3()
		proj.touchfn = "RocketTouch"
		proj.diefn = "GrenadeExplode"
		proj.thinkfn = nil
		proj.owner = idx_self
		proj.light_multiplier = 1
		proj.radius = 0.1
		proj.mass = 0.1
		proj.set_viewent_and_cmdent = false
		ProjectileFire(idx_self, proj)
	end
end

-- ===================
-- AQ2M4Fire
-- 
-- Weapon fire function
-- ===================
function AQ2M4Fire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.04) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.04) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.15 then
		velocity_inaccuracy_x = -0.15
	elseif velocity_inaccuracy_x > 0.15 then
		velocity_inaccuracy_x = 0.15
	end
	if velocity_inaccuracy_y < -0.15 then
		velocity_inaccuracy_y = -0.15
	elseif velocity_inaccuracy_y > 0.15 then
		velocity_inaccuracy_y = 0.15
	end	

	HitscanFire(idx_self, entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * velocity_inaccuracy_y, 10, 100, true, true)
end

-- ===================
-- AQ2MP5Fire
-- 
-- Weapon fire function
-- ===================
function AQ2MP5Fire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	

	HitscanFire(idx_self, entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * velocity_inaccuracy_y, 5, 100, true, true)
end

-- ===================
-- AQ2Super90Fire
-- 
-- Weapon fire function
-- ===================
function AQ2Super90Fire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len() + 0.02) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	

	for i = 0, 9 do
		HitscanFire(idx_self, entities[idx_self].forward + entities[idx_self].right * (math.random(-10000, 10000) / 50000 + velocity_inaccuracy_x) + entities[idx_self].up * (math.random(-10000, 10000) / 50000 + velocity_inaccuracy_y), 10, 100, true, true)
	end
end

-- ===================
-- AQ2SniperFire
-- 
-- Weapon fire function
-- ===================
function AQ2SniperFire(idx_self)
	local velocity_inaccuracy_x = (entities[idx_self].velocity:len()) * math.random(-10000, 10000) / 750000
	local velocity_inaccuracy_y = (entities[idx_self].velocity:len()) * math.random(-10000, 10000) / 750000
	if velocity_inaccuracy_x < -0.1 then
		velocity_inaccuracy_x = -0.1
	elseif velocity_inaccuracy_x > 0.1 then
		velocity_inaccuracy_x = 0.1
	end
	if velocity_inaccuracy_y < -0.1 then
		velocity_inaccuracy_y = -0.1
	elseif velocity_inaccuracy_y > 0.1 then
		velocity_inaccuracy_y = 0.1
	end	

	HitscanFire(idx_self, entities[idx_self].forward + entities[idx_self].right * velocity_inaccuracy_x + entities[idx_self].up * velocity_inaccuracy_y, 200, 1000, true, true)
end

-- ===================
-- WeaponPrecaches
--
-- Precaches weapon data and inits vital structs about weapons (TODO: misnomer?)
-- ===================
function WeaponPrecaches()
	-- TODO: need a null weapon to avoid mangling the model when dying and respawning

	-- first, let's init weapon data TODO: put this in a config file

	gamestate.weapon_info = {}

	gamestate.weapon_info[WEAPON_PUNCH_IDX] = {}
	gamestate.weapon_info[WEAPON_PUNCH_IDX].v_model = "v_punch"
	gamestate.weapon_info[WEAPON_PUNCH_IDX].g_model = ""
	gamestate.weapon_info[WEAPON_PUNCH_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_PUNCH_IDX].ammo_type = -1
	gamestate.weapon_info[WEAPON_PUNCH_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_PUNCH_IDX].ammo_pickup_amount = 0
	gamestate.weapon_info[WEAPON_PUNCH_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_PUNCH_IDX].firefn = "PunchFire"

	gamestate.weapon_info[WEAPON_FALCON2_IDX] = {}
	gamestate.weapon_info[WEAPON_FALCON2_IDX].v_model = "v_falcon2"
	gamestate.weapon_info[WEAPON_FALCON2_IDX].g_model = "g_falcon2"
	gamestate.weapon_info[WEAPON_FALCON2_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_FALCON2_IDX].ammo_type = WEAPON_AMMO_TYPE_PISTOL
	gamestate.weapon_info[WEAPON_FALCON2_IDX].ammo_capacity = 10
	gamestate.weapon_info[WEAPON_FALCON2_IDX].ammo_pickup_amount = 10
	gamestate.weapon_info[WEAPON_FALCON2_IDX].semiauto = 50
	gamestate.weapon_info[WEAPON_FALCON2_IDX].firefn = "Falcon2Fire"

	gamestate.weapon_info[WEAPON_CROSSBOW_IDX] = {}
	gamestate.weapon_info[WEAPON_CROSSBOW_IDX].v_model = "v_crossbow"
	gamestate.weapon_info[WEAPON_CROSSBOW_IDX].g_model = "g_crossbow"
	gamestate.weapon_info[WEAPON_CROSSBOW_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_CROSSBOW_IDX].ammo_type = WEAPON_AMMO_TYPE_ARROW
	gamestate.weapon_info[WEAPON_CROSSBOW_IDX].ammo_capacity = 5
	gamestate.weapon_info[WEAPON_CROSSBOW_IDX].ammo_pickup_amount = 5
	gamestate.weapon_info[WEAPON_CROSSBOW_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_CROSSBOW_IDX].firefn = "CrossbowFire"

	gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX] = {}
	gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].v_model = "v_superdragon"
	gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].g_model = "g_superdragon"
	gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].ammo_type = WEAPON_AMMO_TYPE_GRENADE
	gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].ammo_capacity = 3
	gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].ammo_pickup_amount = 3
	gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_SUPERDRAGON_IDX].firefn = "SuperDragonFire"

	gamestate.weapon_info[WEAPON_CMP150_IDX] = {}
	gamestate.weapon_info[WEAPON_CMP150_IDX].v_model = "v_cmp150"
	gamestate.weapon_info[WEAPON_CMP150_IDX].g_model = "g_cmp150"
	gamestate.weapon_info[WEAPON_CMP150_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_CMP150_IDX].ammo_type = WEAPON_AMMO_TYPE_LIGHT
	gamestate.weapon_info[WEAPON_CMP150_IDX].ammo_capacity = 20
	gamestate.weapon_info[WEAPON_CMP150_IDX].ammo_pickup_amount = 20
	gamestate.weapon_info[WEAPON_CMP150_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_CMP150_IDX].firefn = "CMP150Fire"

	gamestate.weapon_info[WEAPON_NBOMB_IDX] = {}
	gamestate.weapon_info[WEAPON_NBOMB_IDX].v_model = "v_nbomb"
	gamestate.weapon_info[WEAPON_NBOMB_IDX].g_model = "g_nbomb"
	gamestate.weapon_info[WEAPON_NBOMB_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_NBOMB_IDX].ammo_type = WEAPON_AMMO_TYPE_NBOMB
	gamestate.weapon_info[WEAPON_NBOMB_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_NBOMB_IDX].ammo_pickup_amount = 1
	gamestate.weapon_info[WEAPON_NBOMB_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_NBOMB_IDX].firefn = "NBombFire"

	gamestate.weapon_info[WEAPON_SLAYER_IDX] = {}
	gamestate.weapon_info[WEAPON_SLAYER_IDX].v_model = "v_slayer"
	gamestate.weapon_info[WEAPON_SLAYER_IDX].g_model = "g_slayer"
	gamestate.weapon_info[WEAPON_SLAYER_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_SLAYER_IDX].ammo_type = WEAPON_AMMO_TYPE_ROCKET
	gamestate.weapon_info[WEAPON_SLAYER_IDX].ammo_capacity = 1
	gamestate.weapon_info[WEAPON_SLAYER_IDX].ammo_pickup_amount = 1
	gamestate.weapon_info[WEAPON_SLAYER_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_SLAYER_IDX].firefn = "SlayerFire"

	gamestate.weapon_info[WEAPON_ENERGYBALL_IDX] = {}
	gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].v_model = "v_energyball"
	gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].g_model = "g_energyball"
	gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].ammo_type = WEAPON_AMMO_TYPE_CELLS
	gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].ammo_capacity = 3
	gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].ammo_pickup_amount = 1
	gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_ENERGYBALL_IDX].firefn = "EnergyBallFire"

	gamestate.weapon_info[WEAPON_AXE_IDX] = {}
	gamestate.weapon_info[WEAPON_AXE_IDX].v_model = "v_axe"
	gamestate.weapon_info[WEAPON_AXE_IDX].g_model = "g_axe"
	gamestate.weapon_info[WEAPON_AXE_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_AXE_IDX].ammo_type = -1
	gamestate.weapon_info[WEAPON_AXE_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_AXE_IDX].ammo_pickup_amount = 0
	gamestate.weapon_info[WEAPON_AXE_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_AXE_IDX].firefn = "AxeFire"

	gamestate.weapon_info[WEAPON_SHOTGUN_IDX] = {}
	gamestate.weapon_info[WEAPON_SHOTGUN_IDX].v_model = "v_shot"
	gamestate.weapon_info[WEAPON_SHOTGUN_IDX].g_model = "g_shot"
	gamestate.weapon_info[WEAPON_SHOTGUN_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_SHOTGUN_IDX].ammo_type = WEAPON_AMMO_TYPE_QSHELLS
	gamestate.weapon_info[WEAPON_SHOTGUN_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_SHOTGUN_IDX].ammo_pickup_amount = 5
	gamestate.weapon_info[WEAPON_SHOTGUN_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_SHOTGUN_IDX].firefn = "ShotgunFire"

	gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX] = {}
	gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].v_model = "v_shot2"
	gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].g_model = "g_shot2"
	gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].ammo_type = WEAPON_AMMO_TYPE_QSHELLS
	gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].ammo_pickup_amount = 5
	gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_SUPERSHOTGUN_IDX].firefn = "SuperShotgunFire"
	
	gamestate.weapon_info[WEAPON_NAILGUN_IDX] = {}
	gamestate.weapon_info[WEAPON_NAILGUN_IDX].v_model = "v_nail"
	gamestate.weapon_info[WEAPON_NAILGUN_IDX].g_model = "g_nail"
	gamestate.weapon_info[WEAPON_NAILGUN_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_NAILGUN_IDX].ammo_type = WEAPON_AMMO_TYPE_QNAILS
	gamestate.weapon_info[WEAPON_NAILGUN_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_NAILGUN_IDX].ammo_pickup_amount = 10
	gamestate.weapon_info[WEAPON_NAILGUN_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_NAILGUN_IDX].firefn = "NailgunFire"

	gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX] = {}
	gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].v_model = "v_nail2"
	gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].g_model = "g_nail2"
	gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].ammo_type = WEAPON_AMMO_TYPE_QNAILS
	gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].ammo_pickup_amount = 10
	gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_SUPERNAILGUN_IDX].firefn = "SuperNailgunFire"

	gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX] = {}
	gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].v_model = "v_rock"
	gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].g_model = "g_rock"
	gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].ammo_type = WEAPON_AMMO_TYPE_QROCKETS
	gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].ammo_pickup_amount = 5
	gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_GRENADELAUNCHER_IDX].firefn = "GrenadeLauncherFire"

	gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX] = {}
	gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].v_model = "v_rock2"
	gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].g_model = "g_rock2"
	gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].ammo_type = WEAPON_AMMO_TYPE_QROCKETS
	gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].ammo_pickup_amount = 5
	gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_ROCKETLAUNCHER_IDX].firefn = "RocketLauncherFire"

	gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX] = {}
	gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].v_model = "v_light"
	gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].g_model = "g_light"
	gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].w_model = ""
	gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].ammo_type = WEAPON_AMMO_TYPE_CELLS
	gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].ammo_pickup_amount = 10
	gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_THUNDERBOLT_IDX].firefn = "ThunderboltFire"

	-- TODO: lots of aq2 weapons have unused frames for things that I should implement!

	gamestate.weapon_info[WEAPON_AQ2MK23_IDX] = {}
	gamestate.weapon_info[WEAPON_AQ2MK23_IDX].v_model = "v_aq2mk23"
	gamestate.weapon_info[WEAPON_AQ2MK23_IDX].g_model = "g_aq2mk23"
	gamestate.weapon_info[WEAPON_AQ2MK23_IDX].w_model = "player_w_aq2mk23"
	gamestate.weapon_info[WEAPON_AQ2MK23_IDX].ammo_type = WEAPON_AMMO_TYPE_AQ2MK23
	gamestate.weapon_info[WEAPON_AQ2MK23_IDX].ammo_capacity = 12
	gamestate.weapon_info[WEAPON_AQ2MK23_IDX].ammo_pickup_amount = 12
	gamestate.weapon_info[WEAPON_AQ2MK23_IDX].semiauto = 50
	gamestate.weapon_info[WEAPON_AQ2MK23_IDX].firefn = "AQ2MK23Fire"
	
	gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX] = {}
	gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].v_model = "v_aq2mk23dual"
	gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].g_model = "g_aq2mk23dual"
	gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].w_model = "player_w_aq2mk23dual"
	gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].ammo_type = WEAPON_AMMO_TYPE_AQ2MK23
	gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].ammo_capacity = 24 -- TODO: share ammo from one of the pistols
	gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].ammo_pickup_amount = 24
	gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].semiauto = 50 -- TODO: separate for each?
	gamestate.weapon_info[WEAPON_AQ2MK23DUAL_IDX].firefn = "AQ2MK23Fire"
	
	gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX] = {}
	gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].v_model = "v_aq2handcannon"
	gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].g_model = "g_aq2handcannon"
	gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].w_model = "player_w_aq2handcannon"
	gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].ammo_type = WEAPON_AMMO_TYPE_AQ2SHELLS
	gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].ammo_capacity = 1
	gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].ammo_pickup_amount = 1
	gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_AQ2HANDCANNON_IDX].firefn = "AQ2HandCannonFire"
	
	gamestate.weapon_info[WEAPON_AQ2M61FRAG_IDX] = {}
	gamestate.weapon_info[WEAPON_AQ2M61FRAG_IDX].v_model = "v_aq2m61frag"
	gamestate.weapon_info[WEAPON_AQ2M61FRAG_IDX].g_model = "g_aq2m61frag"
	gamestate.weapon_info[WEAPON_AQ2M61FRAG_IDX].w_model = "player_w_aq2m61frag"
	gamestate.weapon_info[WEAPON_AQ2M61FRAG_IDX].ammo_type = WEAPON_AMMO_TYPE_AQ2GRENADES
	gamestate.weapon_info[WEAPON_AQ2M61FRAG_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_AQ2M61FRAG_IDX].ammo_pickup_amount = 1
	gamestate.weapon_info[WEAPON_AQ2M61FRAG_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_AQ2M61FRAG_IDX].firefn = "AQ2M61FragFire"
	
	gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX] = {}
	gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX].v_model = "v_aq2knife"
	gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX].g_model = "g_aq2knife"
	gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX].w_model = "player_w_aq2knife"
	gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX].ammo_type = WEAPON_AMMO_TYPE_AQ2KNIFES
	gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX].ammo_capacity = 0
	gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX].ammo_pickup_amount = 1
	gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_AQ2KNIFE_IDX].firefn = "AQ2KnifeFire"
	
	gamestate.weapon_info[WEAPON_AQ2M4_IDX] = {}
	gamestate.weapon_info[WEAPON_AQ2M4_IDX].v_model = "v_aq2m4"
	gamestate.weapon_info[WEAPON_AQ2M4_IDX].g_model = "g_aq2m4"
	gamestate.weapon_info[WEAPON_AQ2M4_IDX].w_model = "player_w_aq2m4"
	gamestate.weapon_info[WEAPON_AQ2M4_IDX].ammo_type = WEAPON_AMMO_TYPE_AQ2M4
	gamestate.weapon_info[WEAPON_AQ2M4_IDX].ammo_capacity = 30
	gamestate.weapon_info[WEAPON_AQ2M4_IDX].ammo_pickup_amount = 30
	gamestate.weapon_info[WEAPON_AQ2M4_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_AQ2M4_IDX].firefn = "AQ2M4Fire"
	
	gamestate.weapon_info[WEAPON_AQ2MP5_IDX] = {}
	gamestate.weapon_info[WEAPON_AQ2MP5_IDX].v_model = "v_aq2mp5"
	gamestate.weapon_info[WEAPON_AQ2MP5_IDX].g_model = "g_aq2mp5"
	gamestate.weapon_info[WEAPON_AQ2MP5_IDX].w_model = "player_w_aq2mp5"
	gamestate.weapon_info[WEAPON_AQ2MP5_IDX].ammo_type = WEAPON_AMMO_TYPE_AQ2MP5
	gamestate.weapon_info[WEAPON_AQ2MP5_IDX].ammo_capacity = 30
	gamestate.weapon_info[WEAPON_AQ2MP5_IDX].ammo_pickup_amount = 30
	gamestate.weapon_info[WEAPON_AQ2MP5_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_AQ2MP5_IDX].firefn = "AQ2MP5Fire"

	gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX] = {}
	gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].v_model = "v_aq2super90"
	gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].g_model = "g_aq2super90"
	gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].w_model = "player_w_aq2super90"
	gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].ammo_type = WEAPON_AMMO_TYPE_AQ2SHELLS
	gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].ammo_capacity = 7
	gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].ammo_pickup_amount = 7
	gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_AQ2SUPER90_IDX].firefn = "AQ2Super90Fire"
	
	gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX] = {}
	gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].v_model = "v_aq2sniper"
	gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].g_model = "g_aq2sniper"
	gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].w_model = "player_w_aq2sniper"
	gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].ammo_type = WEAPON_AMMO_TYPE_AQ2SNIPER
	gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].ammo_capacity = 5
	gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].ammo_pickup_amount = 5
	gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].semiauto = 0
	gamestate.weapon_info[WEAPON_AQ2SNIPER_IDX].firefn = "AQ2SniperFire"
	
	-- weapon viewmodel precaches
	for i = 0, WEAPON_MAX_IDX - 1 do
		-- TODO: check more stuff
		if (gamestate.weapon_info[i] ~= nil) then
			-- checking string.len(gamestate.weapon_info[i].v_model) will be true, because 0 isn't nil
			if not isemptystring(gamestate.weapon_info[i].v_model) then
				local continue_checking = true
				PrecacheModel(gamestate.weapon_info[i].v_model)
				gamestate.weapon_info[i].v_modelindex = GetModelIndex(gamestate.weapon_info[i].v_model)
				
				gamestate.weapon_info[i].num_fire_animations = 0
				if AnimationExists(gamestate.weapon_info[i].v_modelindex, ANIMATION_FIRE) then
					continue_checking = true
					gamestate.weapon_info[i].num_fire_animations = gamestate.weapon_info[i].num_fire_animations + 1
				else
					continue_checking = false
				end
				if continue_checking and AnimationExists(gamestate.weapon_info[i].v_modelindex, ANIMATION_FIRE2) then
					continue_checking = true
					gamestate.weapon_info[i].num_fire_animations = gamestate.weapon_info[i].num_fire_animations + 1
				else
					continue_checking = false
				end
				if continue_checking and AnimationExists(gamestate.weapon_info[i].v_modelindex, ANIMATION_FIRE3) then
					continue_checking = true
					gamestate.weapon_info[i].num_fire_animations = gamestate.weapon_info[i].num_fire_animations + 1
				else
					continue_checking = false
				end
				if continue_checking and AnimationExists(gamestate.weapon_info[i].v_modelindex, ANIMATION_FIRE4) then
					continue_checking = true
					gamestate.weapon_info[i].num_fire_animations = gamestate.weapon_info[i].num_fire_animations + 1
				else
					continue_checking = false
				end
				if continue_checking and AnimationExists(gamestate.weapon_info[i].v_modelindex, ANIMATION_FIRE5) then
					continue_checking = true
					gamestate.weapon_info[i].num_fire_animations = gamestate.weapon_info[i].num_fire_animations + 1
				else
					continue_checking = false
				end
				if continue_checking and AnimationExists(gamestate.weapon_info[i].v_modelindex, ANIMATION_FIRE6) then
					continue_checking = true
					gamestate.weapon_info[i].num_fire_animations = gamestate.weapon_info[i].num_fire_animations + 1
				else
					continue_checking = false
				end
				if continue_checking and AnimationExists(gamestate.weapon_info[i].v_modelindex, ANIMATION_FIRE7) then
					continue_checking = true
					gamestate.weapon_info[i].num_fire_animations = gamestate.weapon_info[i].num_fire_animations + 1
				else
					continue_checking = false
				end
				if continue_checking and AnimationExists(gamestate.weapon_info[i].v_modelindex, ANIMATION_FIRE8) then
					continue_checking = true
					gamestate.weapon_info[i].num_fire_animations = gamestate.weapon_info[i].num_fire_animations + 1
				end
			end
			if not isemptystring(gamestate.weapon_info[i].g_model) then
				PrecacheModel(gamestate.weapon_info[i].g_model)
				gamestate.weapon_info[i].g_modelindex = GetModelIndex(gamestate.weapon_info[i].g_model)
			end
			if not isemptystring(gamestate.weapon_info[i].w_model) then
				PrecacheModel(gamestate.weapon_info[i].w_model)
				gamestate.weapon_info[i].w_modelindex = GetModelIndex(gamestate.weapon_info[i].w_model)
			end

			if gamestate.weapon_info[i].ammo_capacity > 0 and gamestate.weapon_info[i].ammo_pickup_amount > gamestate.weapon_info[i].ammo_capacity then
				error("WeaponPrecaches: weapon " .. i .. " has pickup amount " .. gamestate.weapon_info[i].ammo_pickup_amount .. " > ammo capacity " .. gamestate.weapon_info[i].ammo_capacity .. "\n")
			end
		end
	end

	-- weapon firing precaches
	PrecacheModel("energyball")
	PrecacheModel("energyexplosion")
	PrecacheModel("s_spike")
	PrecacheModel("grenade")
	PrecacheModel("missile")
	PrecacheModel("s_explod")
	PrecacheModel("aq2grenade")
end

-- ===================
-- WeaponBitToIdx
-- 
-- Turns a weapon bit into a weapon idx
-- ===================
function WeaponBitToIdx(idx_self, bit)
	-- TODO: better ways to select this?
	if bit == WEAPON_PUNCH_BIT then
		return WEAPON_PUNCH_IDX
	elseif bit == WEAPON_FALCON2_BIT then
		return WEAPON_FALCON2_IDX
	elseif bit == WEAPON_CROSSBOW_BIT then
		return WEAPON_CROSSBOW_IDX
	elseif bit == WEAPON_SUPERDRAGON_BIT then
		return WEAPON_SUPERDRAGON_IDX
	elseif bit == WEAPON_CMP150_BIT then
		return WEAPON_CMP150_IDX
	elseif bit == WEAPON_NBOMB_BIT then
		return WEAPON_NBOMB_IDX
	elseif bit == WEAPON_SLAYER_BIT then
		return WEAPON_SLAYER_IDX
	elseif bit == WEAPON_ENERGYBALL_BIT then
		return WEAPON_ENERGYBALL_IDX
	elseif bit == WEAPON_AXE_BIT then
		return WEAPON_AXE_IDX
	elseif bit == WEAPON_SHOTGUN_BIT then
		return WEAPON_SHOTGUN_IDX
	elseif bit == WEAPON_SUPERSHOTGUN_BIT then
		return WEAPON_SUPERSHOTGUN_IDX
	elseif bit == WEAPON_NAILGUN_BIT then
		return WEAPON_NAILGUN_IDX
	elseif bit == WEAPON_SUPERNAILGUN_BIT then
		return WEAPON_SUPERNAILGUN_IDX
	elseif bit == WEAPON_GRENADELAUNCHER_BIT then
		return WEAPON_GRENADELAUNCHER_IDX
	elseif bit == WEAPON_ROCKETLAUNCHER_BIT then
		return WEAPON_ROCKETLAUNCHER_IDX
	elseif bit == WEAPON_THUNDERBOLT_BIT then
		return WEAPON_THUNDERBOLT_IDX
	elseif bit == WEAPON_AQ2MK23_BIT then
		return WEAPON_AQ2MK23_IDX
	elseif bit == WEAPON_AQ2MK23DUAL_BIT then
		return WEAPON_AQ2MK23DUAL_IDX
	elseif bit == WEAPON_AQ2HANDCANNON_BIT then
		return WEAPON_AQ2HANDCANNON_IDX
	elseif bit == WEAPON_AQ2M61FRAG_BIT then
		return WEAPON_AQ2M61FRAG_IDX
	elseif bit == WEAPON_AQ2KNIFE_BIT then
		return WEAPON_AQ2KNIFE_IDX
	elseif bit == WEAPON_AQ2M4_BIT then
		return WEAPON_AQ2M4_IDX
	elseif bit == WEAPON_AQ2MP5_BIT then
		return WEAPON_AQ2MP5_IDX
	elseif bit == WEAPON_AQ2SUPER90_BIT then
		return WEAPON_AQ2SUPER90_IDX
	elseif bit == WEAPON_AQ2SNIPER_BIT then
		return WEAPON_AQ2SNIPER_IDX
	else
		error("WeaponBitToIdx: unhandled bit " .. bit .. " for entity " .. self_idx .. " (" .. entities[idx_self].classname .. ")\n")
	end

	return nil -- keep the compiler happy
end

-- ===================
-- WeaponGetAmmoIdx
--
-- Given a weapon idx, get ammo idx
-- ===================
function WeaponGetAmmoIdx(weapon_idx, no_ammo_ok)
	local ammo_idx

	if weapon_idx < 0 or weapon_idx >= WEAPON_MAX_IDX then
		error("WeaponGetAmmoIdx: weapon type " .. weapon_idx .. " is invalid\n")
	end

	ammo_idx = gamestate.weapon_info[weapon_idx].ammo_type
	if ammo_idx < -1 or ammo_idx >= AMMO_MAX_TYPES then
		error("WeaponGetAmmoIdx: ammo type " .. ammo_idx .. " is invalid\n")
	end

	if not no_ammo_ok and ammo_idx == -1 then
		error("WeaponGetAmmoIdx: ammo type " .. ammo_idx .. " is invalid for this call\n")
	end

	return ammo_idx
end

-- ===================
-- WeaponSetAmmoCapacities
-- 
-- Sets "self" ammo capacities, may be called at each spawn
-- ===================
function WeaponSetAmmoCapacities(idx_self)
	entities[idx_self].ammo_capacity = {}
	-- keep in sync with AMMO_MAX_TYPES
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_PISTOL] = WEAPON_AMMO_CAPACITY_PISTOL
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_ARROW] = WEAPON_AMMO_CAPACITY_ARROW
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_GRENADE] = WEAPON_AMMO_CAPACITY_GRENADE
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_LIGHT] = WEAPON_AMMO_CAPACITY_LIGHT
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_NBOMB] = WEAPON_AMMO_CAPACITY_NBOMB
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_ROCKET] = WEAPON_AMMO_CAPACITY_ROCKET
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_CELLS] = WEAPON_AMMO_CAPACITY_CELLS
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_QSHELLS] = WEAPON_AMMO_CAPACITY_QSHELLS
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_QNAILS] = WEAPON_AMMO_CAPACITY_QNAILS
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_QROCKETS] = WEAPON_AMMO_CAPACITY_QROCKETS
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_AQ2MK23] = WEAPON_AMMO_CAPACITY_AQ2MK23
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_AQ2GRENADES] = WEAPON_AMMO_CAPACITY_AQ2GRENADES
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_AQ2M4] = WEAPON_AMMO_CAPACITY_AQ2M4
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_AQ2MP5] = WEAPON_AMMO_CAPACITY_AQ2MP5
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_AQ2SHELLS] = WEAPON_AMMO_CAPACITY_AQ2SHELLS
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_AQ2SNIPER] = WEAPON_AMMO_CAPACITY_AQ2SNIPER
	entities[idx_self].ammo_capacity[WEAPON_AMMO_TYPE_AQ2KNIFES] = WEAPON_AMMO_CAPACITY_AQ2KNIFES
end

-- ===================
-- CheckItems
-- 
-- Check for items that are of continuous use for firing and timeout
-- Same observations as WeaponFire.
-- For players ONLY
-- TODO: get off this file?
-- ===================
function CheckItems(idx_self)
	-- check timeouts
	if entities[idx_self].items & ITEM_PEPPER_BIT ~= 0 and gamestate.time >= entities[idx_self].item_pepper_finished then
		entities[idx_self].items = entities[idx_self].items - (entities[idx_self].items & ITEM_PEPPER_BIT)
	end

	-- check refires
	if entities[idx_self].items & ITEM_PEPPER_BIT ~= 0 and gamestate.time >= entities[idx_self].item_pepper_attack_finished then
		local dir
		local angles
		local proj = {}

		dir = -entities[idx_self].forward
		angles = MathVecForwardToAngles(dir)

		proj.model = "energyball"
		proj.dir = dir
		proj.dir_fraction = 0.375
		proj.origin_adjust = Vector3()
		proj.angles = angles
		proj.initial_speed = 0
		proj.initial_avelocity = Vector3()
		proj.touchfn = "EnergyBallTouch"
		proj.diefn = "EnergyBallExplode"
		proj.thinkfn = nil
		proj.owner = idx_self
		proj.light_multiplier = 1
		proj.radius = 0.125
		proj.mass = 0.1
		proj.set_viewent_and_cmdent = false
		ProjectileFire(idx_self, proj)

		entities[idx_self].item_pepper_attack_finished = gamestate.time + 200
	end
end

-- ============================================================================
-- 
-- Generic weapon handling
-- 
-- TODO: sync player model animation to weapon viewmodel animation
-- 
-- ============================================================================

-- ===================
-- WeaponSetViewModel
-- 
-- Sets a new viewmodel for self's cameraent
-- ===================
function WeaponSetViewModel(idx_self)
	local viewweap = entities[idx_self].cameraent

	-- check if no view entity
	if viewweap == nil then
		return
	end
	if entities[viewweap] == nil then
		return
	end

	SetModel(viewweap, gamestate.weapon_info[WeaponBitToIdx(idx_self, entities[idx_self].current_weapon)].v_model)

	-- wait for it to be activated before allowing firing
	AnimationStart(viewweap, ANIMATION_WEAPONACTIVATE, ANIMATION_SLOT_ALLJOINTS, true, 0)
	entities[idx_self].attack_finished = gamestate.time + AnimationTimeLeft(viewweap, ANIMATION_SLOT_ALLJOINTS)
end

-- ===================
-- NextFireAnimation
-- 
-- Returns the number of the next animation to be used
-- ===================
function NextFireAnimation(idx_self, weapon_idx)

	local next
	if entities[entities[idx_self].cameraent].state then
		next = entities[entities[idx_self].cameraent].state
		entities[entities[idx_self].cameraent].state = entities[entities[idx_self].cameraent].state + 1
	else
		next = 0
		entities[entities[idx_self].cameraent].state = 0
	end
	entities[entities[idx_self].cameraent].state = entities[entities[idx_self].cameraent].state % gamestate.weapon_info[weapon_idx].num_fire_animations

	if next == 0 then
		return ANIMATION_FIRE
	elseif next == 1 then
		return ANIMATION_FIRE2
	elseif next == 2 then
		return ANIMATION_FIRE3
	elseif next == 3 then
		return ANIMATION_FIRE4
	elseif next == 4 then
		return ANIMATION_FIRE5
	elseif next == 5 then
		return ANIMATION_FIRE6
	elseif next == 6 then
		return ANIMATION_FIRE7
	elseif next == 7 then
		return ANIMATION_FIRE8
	else
		error("Invalid next fire function for entity " .. idx_self .. ": " .. next .. "\n")
	end
end

-- ===================
-- IsFireAnimation
-- 
-- Returns true if the given animation number is a fire animation
-- ===================
function IsFireAnimation(animation)
	if animation == ANIMATION_FIRE then
		return true
	elseif animation == ANIMATION_FIRE2 then
		return true
	elseif animation == ANIMATION_FIRE3 then
		return true
	elseif animation == ANIMATION_FIRE4 then
		return true
	elseif animation == ANIMATION_FIRE5 then
		return true
	elseif animation == ANIMATION_FIRE6 then
		return true
	elseif animation == ANIMATION_FIRE7 then
		return true
	elseif animation == ANIMATION_FIRE8 then
		return true
	else
		return false
	end
end

-- ===================
-- WeaponFire
-- 
-- Notes for all fire functions:
-- Fires the current weapon.
-- Do not forget that, depending on the movetype, calling or not SetAngles in this frame,
-- calling this function pre- or post-think and other factors, the current angle may not be
-- where entities[idx_self].aimcmd points! FIXME TODO
-- 
-- TODO's for all fire functions:
-- the current dir_fraction may make bullets appear beyond walls.
-- make hitscan and projective weapons treat corpses, etc the same way (currently projectiles will go through and hitscan will hit)
-- TODO: alternate fire
-- ===================
function WeaponFire(idx_self)
	local weapon_idx
	local ammo_idx
	local fire_ok
	local no_reload_fire

	if gamestate.time < (entities[idx_self].attack_finished or 0) then
		return
	end

	weapon_idx = WeaponBitToIdx(idx_self, entities[idx_self].current_weapon)
	ammo_idx = WeaponGetAmmoIdx(weapon_idx, true)

	if ammo_idx ~= -1 and gamestate.weapon_info[weapon_idx].ammo_capacity <= 0 and entities[idx_self].ammo[ammo_idx] > 0 then
		no_reload_fire = true
	else
		no_reload_fire = false
	end

	if no_reload_fire then
		fire_ok = true
	elseif ammo_idx == -1 then
		fire_ok = true
	elseif entities[idx_self].weapons_ammo[weapon_idx] > 0 then
		fire_ok = true
	else
		-- check if we should do a reload instead of firing
		if entities[idx_self].ammo ~= nil and entities[idx_self].ammo[ammo_idx] > 0 then
			WeaponReload(idx_self)
			return
		end
		fire_ok = false
	end

	if fire_ok then
		-- TODO: finish animations, allow specifying in which weapon frame the fire should occur
		if IsPlayer(idx_self) and entities[idx_self].cameraent ~= nil then
			AnimationStart(entities[idx_self].cameraent, NextFireAnimation(idx_self, weapon_idx), ANIMATION_SLOT_ALLJOINTS, true, 0)
			entities[idx_self].attack_finished = gamestate.time + AnimationTimeLeft(entities[idx_self].cameraent, ANIMATION_SLOT_ALLJOINTS)
		else
			-- TODO: correct attack_finished for enemies, their model's timeleft?
			entities[idx_self].attack_finished = gamestate.time + 1000
		end

		local firefunction = function_get(gamestate.weapon_info[weapon_idx].firefn)
		if firefunction == nil then
			error("Invalid fire function for entity " .. idx_self .. ": " .. entities[idx_self].firefn .. "\n")
		elseif type(firefunction) == "function" then
			firefunction(idx_self)
		else
			error("Invalid fire function for entity " .. idx_self .. ": " .. entities[idx_self].firefn .. "\n")
		end
		StartSoundB(GetModelSoundIndex(gamestate.weapon_info[weapon_idx].v_modelindex, SOUND_FIRE), idx_self, CHAN_WEAPON, 1, 4, 0.25, false)
		if no_reload_fire then
			entities[idx_self].ammo[ammo_idx] = entities[idx_self].ammo[ammo_idx] - 1
		else
			entities[idx_self].weapons_ammo[weapon_idx] = entities[idx_self].weapons_ammo[weapon_idx] - 1
		end
	else
		if IsPlayer(idx_self) and entities[idx_self].cameraent ~= nil then
			AnimationStart(entities[idx_self].cameraent, ANIMATION_FIREEMPTY, ANIMATION_SLOT_ALLJOINTS, true, 0)
			entities[idx_self].attack_finished = gamestate.time + AnimationTimeLeft(entities[idx_self].cameraent, ANIMATION_SLOT_ALLJOINTS)
		else
			-- TODO: correct attack_finished for enemies, their model's timeleft?
			entities[idx_self].attack_finished = gamestate.time + 1000
		end
		StartSoundB(GetModelSoundIndex(gamestate.weapon_info[weapon_idx].v_modelindex, SOUND_FIREEMPTY), idx_self, CHAN_WEAPON, 1, 1, 1, false)
	end
end

-- ===================
-- WeaponFireReleased
-- 
-- Call when releasing the trigger.
-- Do not forget that, depending on the movetype, calling or not SetAngles in this frame,
-- calling this function pre- or post-think and other factors, the current angle may not be
-- where entities[idx_self].aimcmd points! FIXME TODO
-- 
-- ===================
function WeaponFireReleased(idx_self)
	if entities[idx_self].cameraent ~= nil then
		if IsFireAnimation(entities[entities[idx_self].cameraent].cur_anim[ANIMATION_SLOT_ALLJOINTS]) or entities[entities[idx_self].cameraent].cur_anim[ANIMATION_SLOT_ALLJOINTS] == ANIMATION_FIREEMPTY then
			-- allow fast refiring for releasing the trigger
			if not isemptynumber(gamestate.weapon_info[WeaponBitToIdx(idx_self, entities[idx_self].current_weapon)].semiauto) then
				entities[idx_self].attack_finished = gamestate.time + gamestate.weapon_info[WeaponBitToIdx(idx_self, entities[idx_self].current_weapon)].semiauto
			end
		end
	end
end

-- ===================
-- WeaponReload
-- 
-- Reloads the entities[idx_self].current_weapon
-- ===================
function WeaponReload(idx_self)
	local weapon_idx
	local ammo_idx
	local value
	local needed_to_fill

	if gamestate.time < entities[idx_self].attack_finished then
		return
	end

	weapon_idx = WeaponBitToIdx(idx_self, entities[idx_self].current_weapon)
	ammo_idx = WeaponGetAmmoIdx(weapon_idx, true)

	if ammo_idx == -1 then
		return -- weapon doesn't use ammo
	end

	if entities[idx_self].weapons_ammo[weapon_idx] >= gamestate.weapon_info[weapon_idx].ammo_capacity then
		return -- weapon already fully loaded
	end

	if entities[idx_self].ammo[ammo_idx] < 1 then
		return -- no ammo in stock to fill this weapon
	end

	-- TODO: just min between entities[idx_self].ammo and needed to fill?
	value = math.min(entities[idx_self].ammo[ammo_idx], gamestate.weapon_info[weapon_idx].ammo_capacity)
	needed_to_fill = gamestate.weapon_info[weapon_idx].ammo_capacity - entities[idx_self].weapons_ammo[weapon_idx]
	value = math.min(value, needed_to_fill)

	entities[idx_self].weapons_ammo[weapon_idx] = entities[idx_self].weapons_ammo[weapon_idx] + value
	entities[idx_self].ammo[ammo_idx] = entities[idx_self].ammo[ammo_idx] - value

	-- TODO: finish animations
	if IsPlayer(idx_self) and entities[idx_self].cameraent ~= nil then
		AnimationStart(entities[idx_self].cameraent, ANIMATION_RELOAD, ANIMATION_SLOT_ALLJOINTS, true, 0)
		entities[idx_self].attack_finished = gamestate.time + AnimationTimeLeft(entities[idx_self].cameraent, ANIMATION_SLOT_ALLJOINTS)
	else
		-- TODO: correct attack_finished for enemies, their model's timeleft?
		entities[idx_self].attack_finished = gamestate.time + 1000
	end
	-- TODO: sound
end

-- ===================
-- WeaponCycle
-- 
-- Selects the next or previous weapon, according to "backwards"
-- Depends on entities[idx_self].current_weapon having only ONE bit set.
-- 
-- TODO: weapon changing animation
-- TODO: signed type stuff is here!
-- ===================
function WeaponCycle(idx_self, backwards)
	local newweap = entities[idx_self].current_weapon -- unsigned, or else the sign bit will be stuck and won't be shifted

	if gamestate.time < entities[idx_self].attack_finished then
		return
	end

	while true do -- keep shifting until we lose the bit
		if backwards then
			newweap = newweap >> 1 -- shift to a lower one
		else
			newweap = newweap << 1 -- shift to a higher one
		end

		if entities[idx_self].weapons & newweap == newweap then -- if we have this weapon, break
			break
		end

		if backwards and newweap <= 0 then
			newweap = 0
			break
		elseif newweap >= WEAPON_MAX_BIT then
			newweap = 0
			break
		end
	end

	if newweap ~= 0 then -- if we have found one
		WeaponSelect(idx_self, newweap, false)
		return
	end

	-- try overflowing the list
	if backwards then
		newweap = WEAPON_MAX_BIT
	else
		newweap = 1
	end

	while newweap ~= entities[idx_self].current_weapon do -- keep shifting until we reach the current weapon again
		if entities[idx_self].weapons & newweap == newweap then -- if we have this weapon, break
			break
		end

		if backwards then
			newweap = newweap >> 1 -- shift to a lower one
		else
			newweap = newweap << 1 -- shift to a higher one
		end
	end

	WeaponSelect(idx_self, newweap, false) -- select the new one or the current one
end

-- ===================
-- WeaponCheckImpulses
-- 
-- Misc weapon-related impulses
-- TODO: accumulate weapon change impulses for when we press too many times and the weapons are still changing OR abort changing operation and change to next
-- ===================
function WeaponCheckImpulses(idx_self)
	if entities[idx_self].impulse == 9 then -- TODO: temporary
		entities[idx_self].ammo[0] = entities[idx_self].ammo_capacity[0]
		entities[idx_self].ammo[1] = entities[idx_self].ammo_capacity[1]
		entities[idx_self].ammo[2] = entities[idx_self].ammo_capacity[2]
		entities[idx_self].ammo[3] = entities[idx_self].ammo_capacity[3]
		entities[idx_self].ammo[4] = entities[idx_self].ammo_capacity[4]
		entities[idx_self].ammo[5] = entities[idx_self].ammo_capacity[5]
		entities[idx_self].ammo[6] = entities[idx_self].ammo_capacity[6]
		entities[idx_self].ammo[7] = entities[idx_self].ammo_capacity[7]
		entities[idx_self].ammo[8] = entities[idx_self].ammo_capacity[8]
		entities[idx_self].ammo[9] = entities[idx_self].ammo_capacity[9]
		entities[idx_self].ammo[10] = entities[idx_self].ammo_capacity[10]
		entities[idx_self].ammo[11] = entities[idx_self].ammo_capacity[11]
		entities[idx_self].ammo[12] = entities[idx_self].ammo_capacity[12]
		entities[idx_self].ammo[13] = entities[idx_self].ammo_capacity[13]
		entities[idx_self].ammo[14] = entities[idx_self].ammo_capacity[14]
		entities[idx_self].ammo[15] = entities[idx_self].ammo_capacity[15]
		entities[idx_self].ammo[16] = entities[idx_self].ammo_capacity[16]
		entities[idx_self].weapons = entities[idx_self].weapons | WEAPON_FALCON2_BIT | WEAPON_CROSSBOW_BIT | WEAPON_SUPERDRAGON_BIT | WEAPON_CMP150_BIT | WEAPON_NBOMB_BIT | WEAPON_SLAYER_BIT | WEAPON_ENERGYBALL_BIT | WEAPON_AXE_BIT | WEAPON_SHOTGUN_BIT | WEAPON_SUPERSHOTGUN_BIT | WEAPON_NAILGUN_BIT | WEAPON_SUPERNAILGUN_BIT | WEAPON_GRENADELAUNCHER_BIT | WEAPON_ROCKETLAUNCHER_BIT | WEAPON_THUNDERBOLT_BIT | WEAPON_AQ2MK23_BIT | WEAPON_AQ2MK23DUAL_BIT | WEAPON_AQ2HANDCANNON_BIT | WEAPON_AQ2M61FRAG_BIT | WEAPON_AQ2KNIFE_BIT | WEAPON_AQ2M4_BIT | WEAPON_AQ2MP5_BIT | WEAPON_AQ2SUPER90_BIT | WEAPON_AQ2SNIPER_BIT
	elseif entities[idx_self].impulse == 10 then
		WeaponCycle(idx_self, false)
	elseif entities[idx_self].impulse == 12 then
		WeaponCycle(idx_self, true)
	end
end

-- ===================
-- WeaponFrame
-- 
-- Mostly to handle weapon sync stuff
-- ===================
function WeaponFrame(idx_self)
	WeaponCheckImpulses(idx_self)

	if IsPlayer(idx_self) then
		if entities[idx_self].cameraent ~= nil then
			local curanim = entities[entities[idx_self].cameraent].cur_anim[ANIMATION_SLOT_ALLJOINTS]
			if curanim == ANIMATION_WEAPONDEACTIVATE then
				-- if put away animation finished, handle weapon switch
				if AnimationTimeLeft(entities[idx_self].cameraent, ANIMATION_SLOT_ALLJOINTS) == 0 then
					WeaponSetViewModel(idx_self)
				end
			elseif curanim == ANIMATION_WEAPONACTIVATE or IsFireAnimation(curanim) or curanim == ANIMATION_FIREEMPTY or curanim == ANIMATION_RELOAD then
				-- after other actions, go to idle (which should have a looping flag)
				if AnimationTimeLeft(entities[idx_self].cameraent, ANIMATION_SLOT_ALLJOINTS) == 0 then
					AnimationStart(entities[idx_self].cameraent, ANIMATION_IDLE, ANIMATION_SLOT_ALLJOINTS, true, 0)
				end
			end
		end
	end
end

-- ===================
-- WeaponSelect
-- 
-- Selects a new weapon and also sets the cameraent's model, if applicable.
-- TODO: allow putting various switches in a queue, to allow pressing the weapon switch button a known number of times and waiting
-- TODO: also allow continuing to select weapons when the previous weapon is down without needing to bring the new up and down again for each selection
-- ===================
function WeaponSelect(idx_self, newweap, force)
	if (not force) and (gamestate.time < entities[idx_self].attack_finished) then
		return
	end

	if newweap == entities[idx_self].current_weapon then
		return
	end

	if entities[idx_self].weapons & newweap == 0 then
		if IsPlayer(idx_self) then
			PrintS(idx_self, "No weapon.\n")
		end
		return
	end

	entities[idx_self].current_weapon = newweap

	-- switch won't be instantaneous if we a have a cameraent and change isn't being forced
	if IsPlayer(idx_self) and entities[idx_self].cameraent ~= nil then
		entities[entities[idx_self].cameraent].state = 0 -- reset the multiple fire animations stuff
		if force then
			WeaponSetViewModel(idx_self) -- no wait switching
		else
			-- before switching, do a WeaponActivate and WeaponDeactivate animation
			AnimationStart(entities[idx_self].cameraent, ANIMATION_WEAPONDEACTIVATE, ANIMATION_SLOT_ALLJOINTS, true, 0)
			entities[idx_self].attack_finished = gamestate.time + AnimationTimeLeft(entities[idx_self].cameraent, ANIMATION_SLOT_ALLJOINTS)
		end
	end
end
