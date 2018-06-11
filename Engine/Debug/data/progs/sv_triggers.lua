-- sv_triggers.lua

-- ============================================================================
-- 
-- Triggers
-- 
-- ============================================================================

-- ===================
-- trigger_teleport_touch
-- ===================
function trigger_teleport_touch(idx_self, idx_other)
	local dest

	if not IsPlayer(idx_other) then
		return
	end

	-- TODO: reset velocity? adjust velocity according to the new orientation by using the scalar speed?

	dest = Find(0, "targetname", entities[idx_self].target, true, true)
	if dest == nil then
		error("Can't find teleport destination for entity " .. idx_self .. " at " .. tostring(entities[idx_self].origin) .. "\n")
	end
	PlayerPlaceAtSpawnPoint(idx_other, dest)

	CenterPrintS(idx_other, "Teleported!")
end

-- ===================
-- trigger_teleport
-- 
-- SPAWN FUNCTION
-- Teleports a player to the location defined by "target"
-- Set "target"
-- Set "model"
-- Set "origin" if "model" isn't in absolute coordinates
-- Set "angle" if "model" isn't in absolute coordinates
-- ===================
function trigger_teleport(idx_self)
	if isemptynumber(entities[idx_self].modelindex) then
		error("trigger_teleport needs a model, entity " .. idx_self .. "\n")
	end
	if isemptystring(entities[idx_self].target) then
		error("trigger_teleport with no target set, entity " .. idx_self .. "\n")
	end

	PhysicsSetSolidState(idx_self, SOLID_WORLD_TRIGGER)
	entities[idx_self].touch = "trigger_teleport_touch"
end

-- ===================
-- trigger_once_use
-- ===================
function trigger_once_use(idx_self, idx_other)
	-- FIXME: if target is in wait time, do not remove trigger_once then! Make SUB_UseTargets return a value? But them we may re-use one or more targets if we have multiple targets, delete their targetname? but they might be targeted by something else!
	SUB_UseTargets(idx_self)
	entities[idx_self].touch = nil
	entities[idx_self].use = nil
	entities[idx_self].nextthink1 = gamestate.time + 1
	entities[idx_self].think1 = "SUB_Remove"
end

-- ===================
-- trigger_once_touch
-- ===================
function trigger_once_touch(idx_self, idx_other)
	if not IsPlayer(idx_other) then
		return
	end

	trigger_once_use(idx_self, idx_other) -- in this case we can use the same function
end

-- ===================
-- trigger_once
-- 
-- SPAWN FUNCTION
-- When a player touchs or some event uses it, will use all targets only once and remove itself
-- Set "target"
-- Set "model" and/or set "targetname", at least one of them
-- Set "origin" if "model" isn't in absolute coordinates
-- Set "angle" if "model" isn't in absolute coordinates
-- ===================
function trigger_once(idx_self)
	if isemptystring(entities[idx_self].target) then
		error("trigger_once with no target set, entity " .. idx_self .. "\n")
	end
	if isemptynumber(entities[idx_self].modelindex) and isemptystring(entities[idx_self].targetname) then
		error("trigger_once must have at least one of model or targetname set, entity " .. idx_self .. "\n")
	end

	if entities[idx_self].modelindex ~= nil then
		PhysicsSetSolidState(idx_self, SOLID_WORLD_TRIGGER)
		entities[idx_self].touch = "trigger_once_touch"
	end
	entities[idx_self].use = "trigger_once_use"
end

-- ===================
-- trigger_changelevel_touch
-- ===================
NOEXIT_DAMAGE	= 1000
function trigger_changelevel_touch(idx_self, idx_other)
	if not IsPlayer(idx_other) then
		return
	end

	if gamestate.noexit ~= nil then
		if gamestate.noexit == 1 then
			DealDamage(other, self, NOEXIT_DAMAGE)
			return
		elseif gamestate.noexit == 2 then
			return
		end
	end

	-- TODO: only print this in multiplayer, see if it arrives even after changelevel being issued
	PrintB(entities[idx_other].netname .. " exited the level.\n")

	LocalCmd("changelevel " .. entities[idx_self].map)
end

-- ===================
-- trigger_changelevel
-- 
-- SPAWN FUNCTION
-- Changes the server to map entities[idx_self].map, carrying all clients with it
-- Set "map"
-- Set "model"
-- Set "origin" if "model" isn't in absolute coordinates
-- Set "angle" if "model" isn't in absolute coordinates
-- ===================
function trigger_changelevel(idx_self)
	if isemptynumber(entities[idx_self].modelindex) then
		error("trigger_changelevel needs a model, entity " .. idx_self .. "\n")
	end
	if isemptystring(entities[idx_self].map) then
		error("trigger_changelevel with no map set, entity " .. idx_self .. "\n")
	end

	PhysicsSetSolidState(idx_self, SOLID_WORLD_TRIGGER)
	entities[idx_self].touch = "trigger_changelevel_touch"
end