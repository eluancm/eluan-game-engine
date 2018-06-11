-- sv_worldspawn.lua

-- =====================================================
--
-- Game world entity
--
-- =====================================================

-- ===================
-- worldspawn
-- 
-- SPAWN FUNCTION
-- Entity that manages the game world.
-- Be sure to never set origin and angles for the world entity. Drawing and physics code may assume this. FIXME?
-- ===================
function worldspawn(idx_self)
	-- see if entity 0 is already spawned
	if idx_self ~= gamestate.world then
		error ("World already spawned or something spawned before it! worldspawn is entity " .. tostring(idx_self) .. "\n")
	end
	
	-- there's no need to precache the world map here since it was already loaded by the server code to load entities
	SetModel(idx_self, gamestate.name)

	local ret = PhysicsCreateFromModel(idx_self, entities[idx_self].modelindex, -1, entities[idx_self].origin, entities[idx_self].angles, nil, false)
	-- TODO: create something anyway to allow voxels to have something to refer to?
	if ret then
		PhysicsSetSolidState(idx_self, SOLID_WORLD)
	end

	-- players are special entities. Always allocated, no spawn functions. Done here so the world is also the first one in the linked list of allocated entities.
	gamestate.players = {}
	for i = 0, MAX_CLIENTS - 1 do
		gamestate.players[i] = Spawn(nil, i + 1)
	end
end
