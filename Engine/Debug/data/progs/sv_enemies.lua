-- sv_enemies.lua

-- ============================================================================
-- 
-- Generic enemy functions
-- 
-- ============================================================================

-- ===================
-- enemies_commonfindplayer
-- 
-- Searches for the nearest player in a direct line
-- ===================
function enemies_commonfindplayer(idx_self, mindist)
	local target
	local distancesquared = -1
	local distanceent = nil
	local distance3

	target = Find(0, "classname", "player", true, true) -- only first
	while target ~= nil do
		distance3 = entities[target].origin - entities[idx_self].origin
		if distancesquared == -1 then -- first one?
			distancesquared = distance3:lenSq()
			distanceent = target
		else
			local newdistancesquared = distance3:lenSq()
			if newdistancesquared < distancesquared then -- less than the first one?
				distancesquared = newdistancesquared
				distanceent = target
			end
		end

		target = Find(target, "classname", "player", true, false) -- find next
	end

	if distancesquared > (mindist * mindist) then
		return nil
	else
		return distanceent -- may be nil
	end
end

-- ===================
-- enemies_commondie
-- 
-- Die procedures for all enemies
-- ===================
function enemies_commondie(idx_self)
	entities[idx_self].movetype = MOVETYPE_FREE
	entities[idx_self].anglesflags = 0 -- TODO FIXME: this line causes sudden jumps in camera angles (setting transform here to the kinematic angles set in entities[idx_self].angles could make the collision object stuck/pass through some walls
	entities[idx_self].aimcmd:clear()
	entities[idx_self].movecmd:clear()
	PhysicsSetSolidState(idx_self, SOLID_ENTITY_WITHWORLDONLY)
	entities[idx_self].takedamage = false
	entities[idx_self].nextthink1 = gamestate.time + 15000
	entities[idx_self].think1 = "SUB_Remove"
	entities[idx_self].nextthink2 = 0
	entities[idx_self].think2 = nil
	entities[idx_self].touch = nil

	ItemsDrop(idx_self)

	gamestate.killed_enemies = gamestate.killed_enemies + 1

	SUB_UseTargets(idx_self)
	-- TODO: usar um trigger_counter para isso... tipo quake! Pro trigger counter seria: trigger_counter->count, trigger_counter->targetname, trigger_counter->use
end

-- ===================
-- enemies_commonpain
-- 
-- Pain procedures for all enemies
-- ===================
function enemies_commonpain(idx_self)
end

-- ===================
-- enemies_commonprespawn
-- 
-- Pre-Spawn procedures for all enemies
-- 
-- Returns true if the enemy is still valid.
-- ===================
function enemies_commonprespawn(idx_self)
	-- TODO: SLOW. We may also want enemies in some multiplayer gamemodes
	if CvarGetInteger("_sv_maxplayers") > 1 then
		Free(idx_self)
		return false
	end
	return true
end

-- ===================
-- enemies_commonspawn
-- 
-- Spawn procedures for all enemies
-- ===================
function enemies_commonspawn(idx_self)
	entities[idx_self].movetype = MOVETYPE_WALK
	PhysicsSetSolidState(idx_self, SOLID_ENTITY)
	entities[idx_self].takedamage = true
	entities[idx_self].attack_finished = gamestate.time + 2000 -- don't let them shoot upon spawning

	gamestate.num_enemies = gamestate.num_enemies + 1
end
