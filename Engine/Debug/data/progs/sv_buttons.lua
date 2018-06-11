-- sv_buttons.lua

-- ============================================================================
-- 
-- Buttons
-- 
-- ============================================================================

-- ===================
-- func_button_reset
-- ===================
function func_button_reset(idx_self)
	-- TODO: some fancy animation returning to state
	entities[idx_self].touch = "func_button_touch"
end

-- ===================
-- func_button_touch
-- ===================
function func_button_touch(idx_self, idx_other)
	SUB_UseTargets(idx_self)

	-- TODO: some fancy animation

	entities[idx_self].touch = nil
	entities[idx_self].nextthink1 = gamestate.time + 1000 -- TODO: use entities[idx_self].wait for this? then we will need to change the description in the game_edict_t struct
	entities[idx_self].think1 = "func_button_reset"

	PrintC("Button touched by: " .. entities[idx_other].classname .. "\n")
end

-- ===================
-- func_button
-- 
-- SPAWN FUNCTION
-- When touched, fires targets.
-- Set "target"
-- Set "model"
-- Set "origin" if "model" isn't in absolute coordinates
-- Set "angle" if "model" isn't in absolute coordinates
-- ===================
function func_button(idx_self)
	if isemptynumber(entities[idx_self].modelindex) then
		error("func_button needs a model, entity " .. idx_self .. "\n")
	end

	if isemptystring(entities[idx_self].target) then
		error("func_button with no target set, entity " .. idx_self .. "\n")
	end

	PhysicsSetSolidState(idx_self, SOLID_WORLD)
	entities[idx_self].touch = "func_button_touch"
end
