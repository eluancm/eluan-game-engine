-- sv_misc.lua

-- =====================================================
--
-- Misc entities
--
-- =====================================================

-- ===================
-- info_player_start
-- 
-- SPAWN FUNCTION
-- Initial player position upon entering a map, used according to gametype
-- Set "origin"
-- Set "angle"
-- ===================
function info_player_start(idx_self)
end

-- ===================
-- info_player_deathmatch
-- 
-- SPAWN FUNCTION
-- Initial player position upon entering a map, used according to gametype
-- Set "origin"
-- Set "angle"
-- ===================
function info_player_deathmatch(idx_self)
end

-- ===================
-- info_teleport_destination
-- 
-- SPAWN FUNCTION
-- Use as destination for teleports
-- Set "origin"
-- Set "angle"
-- Set "targetname"
-- ===================
function info_teleport_destination(idx_self)
	if isemptystring(entities[idx_self].targetname) then
		error("info_teleport_destination with no targetname set, entity " .. tonumber(self) .. "\n")
	end
end

-- ===================
-- misc_climbable_touch
-- ===================
function misc_climbable_touch(idx_self, idx_other)
	if not IsPlayer(idx_other) then
		return
	end

	entities[idx_other].climbing = true
end

-- ===================
-- misc_climbable
-- 
-- SPAWN FUNCTION
-- Defines an invisible area where the player will have free vertical movement
-- Set "model" to the invisible area where climbing will be allowed (must be a little far from walls to permit climbing if player loses touch of the wall by a small fraction)
-- Set "origin" if "model" isn't in absolute coordinates
-- Set "angle" if "model" isn't in absolute coordinates
-- TODO: is this the right file/section for this entity?
-- ===================
function misc_climbable(idx_self)
	if entities[idx_self].modelindex == nil then
		error("misc_climbable needs a model, entity " .. tonumber(self) .. "\n")
	end

	PhysicsSetSolidState(idx_self, SOLID_WORLD_TRIGGER)
	entities[idx_self].touch = "misc_climbable_touch"
	entities[idx_self].visible = VISIBLE_NO
end

-- ===================
-- misc_movable
-- 
-- SPAWN FUNCTION
-- Defines an model which will be free for pushing around
-- Set "model" to the model
-- Set "origin" if "model" isn't in absolute coordinates
-- Set "angle" if "model" isn't in absolute coordinates
-- Set "counter" to the mass of the model
-- TODO: is this the right file/section for this entity?
-- TODO: angles locking, mass setting automatic by volume if mass = -1, or maybe setting a density and calculating automatically?
-- TODO: pushing sounds, touch sounds, fall sounds (if entities[idx_self].velocity* && entities[idx_self].onground and sound not playing, play loop! if not velocity sound stop)
-- TODO: if no brushes or trimesh, try AABB box like items, also cylinder etc
-- 
-- Remember that rotation and AABB must be in local coordinates, so use the right tool in the mapping program to set an origin (in Quake3BSP/GtkRadiant/Q3MAP2, just use origin brushes)
-- ===================
function misc_movable(idx_self)
	local mins, maxs
	local model_size, mass

	if entities[idx_self].modelindex == nil then
		error("misc_movable needs a model, entity " .. tonumber(self) .. "\n")
	end

	-- can't be static or kinematic
	if isemptynumber(entities[idx_self].counter) then
		mass = 20 -- TODO: set this appropriately (density?)
	else
		mass = entities[idx_self].counter
		if mass < 0 then
			mass = 20 -- TODO: set this appropriately (density?)
		end
	end

	-- TODO FIXME: commenting these four lines makes the world non-solid! WTF?
	-- TODO FIXME: stacking is unstable after updating the physics engine! (problem with the convex hulls below) WTF?
	mins, maxs = GetModelAABB(entities[idx_self].modelindex, ANIMATION_BASE_FRAME)
	model_size = AABBToBoxHalfExtents(mins, maxs)
	PhysicsCreateFromData(idx_self, PHYSICS_SHAPE_BOX, model_size, mass, entities[idx_self].origin, entities[idx_self].angles, nil, false)
	PhysicsSetSolidState(idx_self, SOLID_ENTITY)
end

-- ===================
-- misc_static
-- 
-- SPAWN FUNCTION
-- Defines an model which will be static and part of the game world
-- Set "model" to the model
-- Set "origin" if "model" isn't in absolute coordinates
-- Set "angle" if "model" isn't in absolute coordinates
-- TODO: is this the right file/section for this entity?
-- TODO: pushing sounds, touch sounds, fall sounds (if entities[idx_self].velocity* && entities[idx_self].onground and sound not playing, play loop! if not velocity sound stop)
-- TODO: if no brushes or trimesh, try also cylinder etc
-- 
-- Remember that rotation and AABB must be in local coordinates, so use the right tool in the mapping program to set an origin (in Quake3BSP/GtkRadiant/Q3MAP2, just use origin brushes)
-- TODO: test this
-- ===================
function misc_static(idx_self)
	if entities[idx_self].modelindex == nil then
		error("misc_static needs a model, entity " .. tonumber(self) .. "\n")
	end

	
	if not PhysicsCreateFromModel(idx_self, entities[idx_self].modelindex, -1, entities[idx_self].origin, entities[idx_self].angles, nil, false)  then
		-- fallback
		local mins, maxs
		local model_size

		mins, maxs = GetModelAABB(entities[idx_self].modelindex, ANIMATION_BASE_FRAME)
		model_size = AABBToBoxHalfExtents(mins, maxs)
		PhysicsCreateFromData(idx_self, PHYSICS_SHAPE_BOX, model_size, -1, entities[idx_self].origin, entities[idx_self].angles, nil, false)
	end

	PhysicsSetSolidState(idx_self, SOLID_WORLD)
end
