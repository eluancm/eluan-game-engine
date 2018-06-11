-- sv_vehicle.lua

-- ============================================================================
-- 
-- Four wheeled vehicle
-- 
-- TODO: a vehicle where a player drives and another controls the turret, easy to do
-- TODO: when a player inside the vehicle disconnects, the vehicle becomes unusable
-- TODO: when framerate goes down, vehicle movemente stutters
-- TODO: when the player is pushing the vehicle, damage may occur to the player
-- 
-- ============================================================================

-- ===================
-- vehicle_fourwheels_die
-- ===================
function vehicle_fourwheels_die(idx_self, idx_other)
	local turret
	local turret_radius = 0.3 -- TODO: use model size
	local turret_mass = 20 -- TODO: use model data

	entities[idx_self].movecmd:clear()
	entities[idx_self].takedamage = false
	entities[idx_self].nextthink1 = gamestate.time + 15000
	entities[idx_self].think1 = "SUB_Remove"
	entities[idx_self].nextthink2 = 0
	entities[idx_self].think2 = nil
	entities[idx_self].touch = nil
	entities[idx_self].use = nil

	-- TODO: let the wheels go
	entities[entities[idx_self].attachment1].nextthink1 = gamestate.time + 15000
	entities[entities[idx_self].attachment1].think1 = "SUB_Remove"
	entities[entities[idx_self].attachment2].nextthink1 = gamestate.time + 15000
	entities[entities[idx_self].attachment2].think1 = "SUB_Remove"
	entities[entities[idx_self].attachment3].nextthink1 = gamestate.time + 15000
	entities[entities[idx_self].attachment3].think1 = "SUB_Remove"
	entities[entities[idx_self].attachment4].nextthink1 = gamestate.time + 15000
	entities[entities[idx_self].attachment4].think1 = "SUB_Remove"

	turret = entities[idx_self].attachment5
	entities[turret].movetype = MOVETYPE_FREE
	PhysicsCreateFromData(entities[idx_self].attachment5, PHYSICS_SHAPE_SPHERE, turret_radius, turret_mass, entities[turret].origin, entities[turret].angles, nil, false)
	entities[turret].nextthink1 = gamestate.time + 15000
	entities[turret].think1 = "SUB_Remove"

	-- if a player is inside, kick him out and do some damage
	if not isemptynumber(entities[idx_self].counter) and IsPlayer(entities[idx_self].owner) then
		local oldowner = entities[idx_self].owner

		vehicle_fourwheels_use(idx_self, oldowner)
		DealDamage(oldowner, idx_self, 50 * math.random(10000, 30000) / 10000)
	end
end

-- ===================
-- vehicle_fourwheels_pain
-- ===================
function vehicle_fourwheels_pain(idx_self, idx_other)
end

-- ===================
-- vehicle_fourwheels_touch
-- ===================
function vehicle_fourwheels_touch(idx_self, idx_other)
	if not entities[idx_self].contact_reaction then
		return -- this also prevents damage to our own projectiles when they are launched from a moving vehicle
	end

	if entities[idx_other].takedamage then
		local velocity_differential
		local damage

		-- even if something hits us at full speed, do no damage through here, to be consistent with the world
		-- TODO: this prevents the second if below from running - was this intended when I wrote it?
		if entities[idx_self].velocity:lenSq() < 25 then
			return
		end

		velocity_differential = entities[idx_other].velocity - entities[idx_self].velocity
		damage = velocity_differential:lenSq() * 5
		DealDamage(idx_other, idx_self, damage)
	end
	if entities[idx_self].takedamage then
		local impact_impulse
		local damage

		-- TODO: if only taking into account one point of contact, may the physics engine choose the weakest one?
		impact_impulse = entities[idx_self].contact_normal * (-entities[idx_self].contact_impulse) -- negative because the normal points towards self
		damage = impact_impulse:len() / 100

		-- only do normal damage if not a downwards impact (because vehicles have suspension to take care of that!)
		if entities[idx_self].up:dot(impact_impulse) < 0 then
			damage = damage - 100
		end

		-- the division will cause a series of very small numbers, avoid them
		if damage > 5 then
			DealDamage(idx_self, idx_other, damage)
		end
	end
end

-- ===================
-- vehicle_fourwheels_use
-- ===================
function vehicle_fourwheels_use(idx_self, idx_other)
	if not IsPlayer(idx_other) then
		return
	end

	if isemptynumber(entities[idx_self].counter) then -- nobody inside
		entities[idx_self].counter = entities[idx_self].counter + 1
		entities[idx_other].viewent = entities[idx_self].cameraent
		entities[idx_other].cmdent = idx_self
		entities[idx_self].owner = idx_other

		player_hide(idx_other)
	else -- someone is inside
		-- only a player inside may "use" this vehicle
		if entities[idx_self].cameraent == entities[idx_other].viewent then
			local newangles
			-- TODO: do a convex cast and see if there is empty space at the side of the vehicle, depending on there the vehicle door is
			entities[idx_self].counter = entities[idx_self].counter - 1
			entities[idx_other].viewent = entities[idx_other].cameraent
			newangles = entities[idx_self].angles:clone()
			newangles:setAngle(ANGLES_ROLL, 0) -- remove any roll from the vehicle camera before re-creating the player eyes
			SetTransform(idx_other, entities[idx_self].origin, newangles, nil)
			-- player_unhide will fix kinematic angles
			player_unhide(idx_other)
			PhysicsSetLinearVelocity(idx_other, entities[idx_self].velocity)
			-- TODO: set linear velocity? here and in other places?

			entities[idx_self].owner = nil
		end
		if entities[idx_other].cmdent == idx_self then
			entities[idx_other].cmdent = idx_other
		end
	end
end

-- ===================
-- vehicle_fourwheels_think
-- ===================
function vehicle_fourwheels_think(idx_self)
	if entities[idx_self].health <= 0 then
		return
	end

	local turret

	local forward, right, up
	local vehiclerotationmatrix
	local resultforward, resultright, resultup
	local newangles

	-- player code will pass the movement (*cmd entity fields of the player) to ourself

	-- TODO: entities[idx_self].forward is done post physics or when settings transform (turret will have been set by MOVETYPE_FOLLOW at endframe)
	turret = entities[idx_self].attachment5
	-- no physical shape, so we can't update entities[turret].forward and friends with SetTransform
	forward, right, up = MathAnglesToVec(entities[turret].moveangles)
	-- transform with the parent's angles (angles->direction vectors->multiply by parent's rotation->transformed vectors->new angles)
	vehiclerotationmatrix = Matrix4x4()
	vehiclerotationmatrix = vehiclerotationmatrix:RotateY(entities[idx_self].angles:getAngle(ANGLES_YAW))
	vehiclerotationmatrix = vehiclerotationmatrix:RotateX(entities[idx_self].angles:getAngle(ANGLES_PITCH))
	vehiclerotationmatrix = vehiclerotationmatrix:RotateZ(entities[idx_self].angles:getAngle(ANGLES_ROLL))

	resultforward = vehiclerotationmatrix:applyToVector3(forward)
	resultright = vehiclerotationmatrix:applyToVector3(right)
	resultup = vehiclerotationmatrix:applyToVector3(up)

	-- get the angles transformed with the parent's angles
	newangles = MathVecToAngles(resultforward, resultright, resultup)

	-- store the results in our entity (weapon firing need angles and direction vectors, but by not having a physical representation the turret doesn't get those updated automatically */
	-- TODO: see other properties that the weapon firing code needs
	if entities[idx_self].velocity then
		entities[turret].velocity = entities[idx_self].velocity:clone()
	end
	if entities[idx_self].avelocity then
		entities[turret].avelocity = entities[idx_self].avelocity:clone()
	end
	entities[turret].forward = resultforward:clone()
	entities[turret].right = resultright:clone()
	entities[turret].up = resultup:clone()
	entities[turret].angles = newangles:clone()

	if not isemptynumber(entities[idx_self].counter) then
		if entities[idx_self].buttoncmd & BUTTONCMD_FIRE == BUTTONCMD_FIRE then
			WeaponFire(turret)
		else
			WeaponFireReleased(turret)
		end
		if entities[idx_self].triggerbuttoncmd & TRIGGERBUTTONCMD_USE == TRIGGERBUTTONCMD_USE then
			create_instant_use_event(entities[idx_self].owner, idx_self)
		end
	else
		entities[idx_self].movecmd:clear()
		entities[idx_self].movecmd.y = 1 -- handbrake, parked
	end

	entities[idx_self].nextthink1 = gamestate.time + 1
	entities[idx_self].think1 = "vehicle_fourwheels_think"
end

-- ===================
-- vehicle_fourwheels
-- 
-- SPAWN FUNCTION
-- Basic vehicle.
-- Set "origin"
-- Set "angle"
-- ===================
function vehicle_fourwheels(idx_self)
	local turret
	local v = {}

	-- PrecacheModel("zombie")
	PrecacheModel("vehicle1_wheel")
	PrecacheModel("vehicle1_chassis")

	-- TODO: use a file acompanying the models for most of these
	-- TODO: there are other properties in bullet to verify
	-- TODO: bullet vehicle code uses raycasting for the wheels - a convex cast would make them more natural (they "jump around" sometimes)
	-- TODO: if the car is by its side and you push the wheels with the player entity body, the suspension will compress and push the car, not you!
	-- TODO: wheels as dynamic cylinders that collide (with chassis as owner so no collision with it) when they stick out
	v.wheelDirectionCS0 = Vector3(0, -1, 0)
	v.wheelAxleCS = Vector3(-1, 0, 0)
	v.gEngineForce = 0.0
	v.defaultBreakingForce = 20.0 -- "engine brake" force
	v.handBrakeBreakingForce = 600.0
	v.gBreakingForce = 0.0
	v.maxEngineForce = 4000.0
	v.gVehicleSteering = 0.0
	v.steeringIncrement = 0.04
	v.steeringClamp = 0.6
	v.wheelRadius = 0.5
	v.wheelWidth = 0.4
	v.wheelFriction = 25 -- 1000
	v.suspensionStiffness = 20.0
	v.suspensionDamping = 2.3
	v.suspensionCompression = 4.4
	v.maxSuspensionTravelCm = 25 -- 500
	v.maxSuspensionForce = 10000 -- 6000
	v.rollInfluence = 0.02 -- 0.1
	v.suspensionRestLength = 0.6
	v.connectionHeight = -0.2 -- 0.2
	v.connectionStickLateralOutWheelWidthMultiplier = -0.5 -- 0.3
	v.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier = 1
	v.chassis_box_half_extents = Vector3(1.0, 0.5, 2.0)
	v.chassis_box_localpos = Vector3(0, 1, 0)
	v.suppchassis_box_half_extents = Vector3(0.5, 0.1, 0.5)
	v.suppchassis_box_localpos = Vector3(0, 0, 0)
	-- TODO: spawn these at the right location? or no need because the first simulation frame will put them at the right place?
	-- v.wheel_ents0 = Spawn({["classname"] = "vehicle_fourwheels_wheel", ["origin"] = tostring(entities[idx_self].origin), ["angles"] = tostring(entities[idx_self].angles), ["model"] = "zombie"})
	-- v.wheel_ents1 = Spawn({["classname"] = "vehicle_fourwheels_wheel", ["origin"] = tostring(entities[idx_self].origin), ["angles"] = tostring(entities[idx_self].angles), ["model"] = "zombie"})
	-- v.wheel_ents2 = Spawn({["classname"] = "vehicle_fourwheels_wheel", ["origin"] = tostring(entities[idx_self].origin), ["angles"] = tostring(entities[idx_self].angles), ["model"] = "zombie"})
	-- v.wheel_ents3 = Spawn({["classname"] = "vehicle_fourwheels_wheel", ["origin"] = tostring(entities[idx_self].origin), ["angles"] = tostring(entities[idx_self].angles), ["model"] = "zombie"})
	v.wheel_ents0 = Spawn({["classname"] = "vehicle_fourwheels_wheel", ["origin"] = tostring(entities[idx_self].origin), ["angles"] = tostring(entities[idx_self].angles), ["model"] = "vehicle1_wheel"})
	v.wheel_ents1 = Spawn({["classname"] = "vehicle_fourwheels_wheel", ["origin"] = tostring(entities[idx_self].origin), ["angles"] = tostring(entities[idx_self].angles), ["model"] = "vehicle1_wheel"})
	v.wheel_ents2 = Spawn({["classname"] = "vehicle_fourwheels_wheel", ["origin"] = tostring(entities[idx_self].origin), ["angles"] = tostring(entities[idx_self].angles), ["model"] = "vehicle1_wheel"})
	v.wheel_ents3 = Spawn({["classname"] = "vehicle_fourwheels_wheel", ["origin"] = tostring(entities[idx_self].origin), ["angles"] = tostring(entities[idx_self].angles), ["model"] = "vehicle1_wheel"})
	entities[v.wheel_ents0].owner = idx_self
	entities[v.wheel_ents1].owner = idx_self
	entities[v.wheel_ents2].owner = idx_self
	entities[v.wheel_ents3].owner = idx_self

	entities[idx_self].attachment1 = v.wheel_ents0
	entities[idx_self].attachment2 = v.wheel_ents1
	entities[idx_self].attachment3 = v.wheel_ents2
	entities[idx_self].attachment4 = v.wheel_ents3
	v.wheel_drive = VEHICLE1_ALL_WHEEL_DRIVE

	-- SetModel(idx_self, "zombie")
	SetModel(idx_self, "vehicle1_chassis")
	-- PhysicsCreateFromData(idx_self, PHYSICS_SHAPE_VEHICLE1, v, 800, entities[idx_self].origin, entities[idx_self].angles, nil, false)
	PhysicsCreateFromData(idx_self, PHYSICS_SHAPE_VEHICLE1, v, 1200, entities[idx_self].origin, entities[idx_self].angles, nil, false)
	PhysicsSetSolidState(idx_self, SOLID_ENTITY)

	entities[idx_self].counter = 0
	entities[idx_self].use = "vehicle_fourwheels_use"
	entities[idx_self].nextthink1 = gamestate.time + 1
	entities[idx_self].think1 = "vehicle_fourwheels_think"

	entities[idx_self].cameraent = Spawn({["classname"] = "camera"})
	entities[entities[idx_self].cameraent].owner = idx_self

	entities[idx_self].takedamage = true
	entities[idx_self].health = 500
	entities[idx_self].max_health = 500
	entities[idx_self].die = "vehicle_fourwheels_die"
	entities[idx_self].pain = "vehicle_fourwheels_pain"
	entities[idx_self].touch = "vehicle_fourwheels_touch"

	turret = Spawn({["classname"] = "vehicle_fourwheels_turret", ["model"] = "v_falcon2"})
	entities[turret].owner = idx_self
	entities[turret].movetype = MOVETYPE_FOLLOW
	entities[turret].followup = 1 -- TODO: at this position we are out of the vehicle body, we may shoot through the terrain when upside-down
	entities[turret].moveangles = Vector3()
	entities[idx_self].attachment5 = turret

	entities[turret].weapons = WEAPON_ENERGYBALL_BIT
	entities[turret].weapons_ammo = {}
	entities[turret].weapons_ammo[WEAPON_ENERGYBALL_IDX] = 100
	WeaponSelect(turret, WEAPON_ENERGYBALL_BIT, true)
end

-- ===================
-- vehicle_fourwheels_wheel
-- 
-- SPAWN FUNCTION
-- Wheel for basic vehicle.
-- ===================
function vehicle_fourwheels_wheel(idx_self)
end

-- ===================
-- vehicle_fourwheels_turret
-- 
-- SPAWN FUNCTION
-- Turret for basic vehicle.
-- ===================
function vehicle_fourwheels_turret(idx_self)
end

-- ============================================================================
-- 
-- Vertiglider vehicle
-- 
-- TODO: a vehicle where a player drives and another controls the turret, easy to do
-- 
-- ============================================================================

-- ===================
-- vehicle_fourwheels_die
-- ===================
function vehicle_vertiglider_die(idx_self, idx_other)
	entities[idx_self].movecmd:clear()
	entities[idx_self].aimcmd:clear()
	entities[idx_self].movetype = MOVETYPE_FREE
	entities[idx_self].anglesflags = 0 -- TODO FIXME: this line causes sudden jumps in camera angles (setting transform here to the kinematic angles set in entities[idx_self].angles could make the collision object stuck/pass through some walls
	entities[idx_self].takedamage = false
	entities[idx_self].nextthink1 = gamestate.time + 15000
	entities[idx_self].think1 = "SUB_Remove"
	entities[idx_self].nextthink2 = 0
	entities[idx_self].think2 = nil
	entities[idx_self].touch = nil
	entities[idx_self].use = nil

	-- if a player is inside, kick him out and do some damage
	if not isemptynumber(entities[idx_self].counter) and IsPlayer(entities[idx_self].owner) then
		local oldowner = entities[idx_self].owner

		vehicle_vertiglider_use(idx_self, oldowner)
		DealDamage(oldowner, idx_self, 50 * math.random(10000, 30000) / 10000)
	end
end

-- ===================
-- vehicle_vertiglider_pain
-- ===================
function vehicle_vertiglider_pain(idx_self, idx_other)
end

-- ===================
-- vehicle_vertiglider_touch
-- ===================
function vehicle_vertiglider_touch(idx_self, idx_other)
	if not entities[idx_self].contact_reaction then
		return -- this also prevents damage to our own projectiles when they are launched from a moving vehicle
	end

	if entities[idx_other].takedamage then
		local velocity_differential
		local damage

		-- even if something hits us at full speed, do no damage through here, to be consistent with the world
		-- TODO: this prevents the second if below from running - was this intended when I wrote it?
		if entities[idx_self].velocity:lenSq() < 25 then
			return
		end

		velocity_differential = entities[idx_other].velocity - entities[idx_self].velocity
		damage = velocity_differential:lenSq() * 5
		DealDamage(idx_other, idx_self, damage)
	end
	if entities[idx_self].takedamage then
		local impact_impulse
		local damage

		-- TODO: if only taking into account one point of contact, may the physics engine choose the weakest one?
		impact_impulse = entities[idx_self].contact_normal * (-entities[idx_self].contact_impulse) -- negative because the normal points towards self

		damage = impact_impulse:len() / 100

		-- for soft landing, absorb damage from impacts below
		if entities[idx_self].up:dot(impact_impulse) < 0 then
			damage = damage - 50
		end

		-- the division will cause a series of very small numbers, avoid them
		if damage > 5 then
			DealDamage(idx_self, idx_other, damage)
		end
	end
end

-- ===================
-- vehicle_vertiglider_use
-- ===================
function vehicle_vertiglider_use(idx_self, idx_other)
	if not IsPlayer(idx_other) then
		return
	end

	if isemptynumber(entities[idx_self].counter) then -- nobody inside
		entities[idx_self].counter = entities[idx_self].counter + 1
		entities[idx_other].viewent = entities[idx_self].cameraent
		entities[idx_other].cmdent = idx_self
		entities[idx_self].owner = idx_other

		player_hide(idx_other)
	else -- someone is inside
		-- only a player inside may "use" this vehicle
		if entities[idx_self].cameraent == entities[idx_other].viewent then
			local newangles
			-- TODO: do a convex cast and see if there is empty space at the side of the vehicle, depending on there the vehicle door is
			entities[idx_self].counter = entities[idx_self].counter - 1
			entities[idx_other].viewent = entities[idx_other].cameraent
			newangles = entities[idx_self].angles:clone()
			newangles:setAngle(ANGLES_ROLL, 0) -- remove any roll from the vehicle camera before re-creating the player eyes
			SetTransform(idx_other, entities[idx_self].origin, newangles, nil)
			-- player_unhide will fix kinematic angles
			player_unhide(idx_other)
			PhysicsSetLinearVelocity(idx_other, entities[idx_self].velocity)
			-- TODO: set linear velocity? here and in other places?

			entities[idx_self].owner = nil
		end
		if entities[idx_other].cmdent == idx_self then
			entities[idx_other].cmdent = idx_other
		end
	end
end

-- ===================
-- vehicle_vertiglider_think
-- ===================
function vehicle_vertiglider_think(idx_self)
	if entities[idx_self].health <= 0 then
		return
	end

	if entities[idx_self].onground then
		entities[idx_self].anglesflags = 0 -- TODO FIXME: this line causes sudden jumps in camera angles (setting transform here to the kinematic angles set in entities[idx_self].angles could make the collision object stuck/pass through some walls
	else
		entities[idx_self].anglesflags = ANGLES_KINEMATICANGLES_BIT
	end

	if not isemptynumber(entities[idx_self].counter) then
		-- player code will pass the movement (*cmd entity fields of the player) to ourself

		-- TODO: entities[idx_self].forward is done post physics or when settings transform (turret will have been set by MOVETYPE_FOLLOW at endframe)
		if entities[idx_self].buttoncmd & BUTTONCMD_FIRE == BUTTONCMD_FIRE then
			WeaponFire(idx_self)
		else
			WeaponFireReleased(idx_self)
		end

		if entities[idx_self].triggerbuttoncmd & TRIGGERBUTTONCMD_USE == TRIGGERBUTTONCMD_USE then
			create_instant_use_event(entities[idx_self].owner, idx_self)
		end
	else
		entities[idx_self].movecmd:clear()
		entities[idx_self].aimcmd:clear()
	end

	entities[idx_self].nextthink1 = gamestate.time + 1
	entities[idx_self].think1 = "vehicle_vertiglider_think"
end

-- ===================
-- vehicle_vertiglider
-- 
-- SPAWN FUNCTION
-- Basic vehicle.
-- Set "origin"
-- Set "angle"
-- ===================
function vehicle_vertiglider(idx_self)
	local size = Vector3(1.0, 0.5, 2.0)

	PrecacheModel("vehicle1_chassis")

	-- TODO: use a file acompanying the models for most of these
	SetModel(idx_self, "vehicle1_chassis")
	PhysicsCreateFromData(idx_self, PHYSICS_SHAPE_BOX, size, 600, entities[idx_self].origin, entities[idx_self].angles, nil, false)
	PhysicsSetSolidState(idx_self, SOLID_ENTITY)

	entities[idx_self].counter = 0
	entities[idx_self].use = "vehicle_vertiglider_use"
	entities[idx_self].nextthink1 = gamestate.time + 1
	entities[idx_self].think1 = "vehicle_vertiglider_think"

	entities[idx_self].cameraent = Spawn({["classname"] = "camera"})
	entities[entities[idx_self].cameraent].owner = idx_self

	entities[idx_self].takedamage = true
	entities[idx_self].health = 200
	entities[idx_self].max_health = 200
	entities[idx_self].die = "vehicle_vertiglider_die"
	entities[idx_self].pain = "vehicle_vertiglider_pain"
	entities[idx_self].touch = "vehicle_vertiglider_touch"

	entities[idx_self].weapons = WEAPON_ENERGYBALL_BIT
	entities[idx_self].weapons_ammo = {}
	entities[idx_self].weapons_ammo[WEAPON_ENERGYBALL_IDX] = 100
	WeaponSelect(idx_self, WEAPON_ENERGYBALL_BIT, true)

	entities[idx_self].movetype = MOVETYPE_FLY
	entities[idx_self].maxspeed = Vector3(32, 32, 32)
	entities[idx_self].acceleration = Vector3(32, 32, 32)
end
