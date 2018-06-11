-- cl_sbar.lua

-- ============================================================================
-- 
-- Game client ingame status bar draw
-- 
-- ============================================================================

-- TODO: configure
-- TODO: centerprint queue for not overwriting (after all, centerprint is for stuff you should read!) - OR MAYBE show many at the same time, like if it was various lines with different disappearing times
-- TODO: support icons (for example, only show the corresponding ammo icon when you get it - or maybe along with text: "[pic] shells")
-- TODO: centerprints in savegames?
CENTERPRINT_DURATION_MS		= 3000
CENTERPRINT_Y				= BASE_HEIGHT / 2.0
CENTERPRINT_SCALEX			= 0.3
CENTERPRINT_SCALEY			= 0.3
CENTERPRINT_R				= 1.0
CENTERPRINT_G				= 1.0
CENTERPRINT_B				= 1.0

MAX_NOTIFY_MESSAGES			= 8
PRINT_DURATION_MS			= 3000
PRINT_Y_START				= BASE_HEIGHT - 64
PRINT_X						= 10
PRINT_SCALEX				= 0.3
PRINT_SCALEY				= 0.3
PRINT_R						= 0.25
PRINT_G						= 0.25
PRINT_B						= 0.75

fps = 0
accumulated_time = 0
accumulated_frames = 0
slow_frames = 0

-- ===================
-- Sbar_Draw
-- 
-- TODO: animations when these change
-- TODO: see what needs to go to Sbar_Frame()
-- ===================
function Sbar_Draw()
	local i, pnum, pidx
	local text
	local meter, wx, py
	local meter_int
	local instant_fps = 0
	if not cl_ingame then
		return
	end

	-- fps TODO FIXME: won't go below 10 because of scaling to avoid too long frames
	if not isemptynumber(host_frametime) then
		instant_fps = 1.0 / host_frametime * 1000.0
		if accumulated_frames == 0 then
			fps = instant_fps
		end
		if accumulated_time < 250 then
			accumulated_time = accumulated_time + host_frametime
			accumulated_frames = accumulated_frames + 1
		else
			fps = accumulated_frames / accumulated_time * 1000.0
			accumulated_time = host_frametime
			accumulated_frames = 1
		end
	end

	if instant_fps < 50 then
		slow_frames = slow_frames + 1
		if slow_frames > 10 then
			slow_frames = 10
		end
		PrintC("WARNING: this frame was very slow: " .. string.format("% 4.2f", instant_fps) .. " fps\n")
	else
		if slow_frames > 0 then
			slow_frames = slow_frames - 1
		end
	end

	text = "framerate: " .. string.format("% 4.2f", fps) .. " fps"
	if slow_frames >= 10 then
		text = text .. " (SLOW) "
	else
		text = text .. "        "
	end
	if clgs.stall_frames >= 10 then
		text = text .. " (NET STALL) "
	else
		text = text .. "             "
	end
	VideoDraw2DText(text, Vector2(16, 16), Vector2(0.2, 0.2), Vector4(1, 1, 0, 1))
	
	-- score bar
	if clgs.sbar_showscores or clgs.gameover then
		local sx = 64
		local sy = 32
		local sorted_indices = {}
		local j

		for i = 0, MAX_CLIENTS - 1 do
			sorted_indices[i] = i
		end

		-- Insertion sort O(n^2), hope that the we never have that many clients
		-- TODO: test this sort
		for i = 1, MAX_CLIENTS - 1 do
			for j = i, 1, -1 do
				local tmp

				if clgs.scorefrags[sorted_indices[j - 1]] <= clgs.scorefrags[sorted_indices[j]] then
					break
				end
				
				tmp = sorted_indices[j - 1]
				sorted_indices[j - 1] = sorted_indices[j]
				sorted_indices[j] = tmp
			end
		end

		VideoDraw2DText("Server: " .. clgs.remote_host, Vector2(sx, sy), Vector2(0.2, 0.2), Vector4(1, 1, 1, 1))
		sy = sy + 16
		VideoDraw2DText("--------------------------------", Vector2(sx, sy), Vector2(0.2, 0.2), Vector4(1, 1, 1, 1))
		sy = sy + 16
		VideoDraw2DText("Scores | Time | Ping | PL | Name", Vector2(sx, sy), Vector2(0.2, 0.2), Vector4(1, 1, 1, 1))
		sy = sy + 16
		VideoDraw2DText("------ | ---- | ---- | -- | ----", Vector2(sx, sy), Vector2(0.2, 0.2), Vector4(1, 1, 1, 1))
		for i = MAX_CLIENTS - 1, 0, -1 do
			if not isemptystring(clgs.scorenames[sorted_indices[i]]) then
				sy = sy + 16

				text = ""
				-- TODO: sort, only show connected, etc
				if clgs.my_ent and sorted_indices[i] == clgs.my_ent - 1 then -- -1 because entity slot is 1-based
					text = text .. "["
				else
					text = text .. " "
				end
				text = text .. string.format("%4d", clgs.scorefrags[sorted_indices[i]])
				if clgs.my_ent and sorted_indices[i] == clgs.my_ent - 1 then -- -1 because entity slot is 1-based
					text = text .. "]"
				else
					text = text .. " "
				end
				local shownping, shownpl
				if clgs.scorepings[sorted_indices[i]] < 10000 then
					shownping = clgs.scorepings[sorted_indices[i]]
				else
					shownping = 9999
				end
				if clgs.scorepls[sorted_indices[i]] < 100 then
					shownpl = clgs.scorepls[sorted_indices[i]]
				else
					shownpl = 99
				end
				text = text .. string.format(" | %4d | %4d | %2d | %s", clgs.scoretimes[sorted_indices[i]], shownping, shownpl, clgs.scorenames[sorted_indices[i]])
				VideoDraw2DText(text, Vector2(sx, sy), Vector2(0.2, 0.2), Vector4(1, 1, 1, 1))
			end
		end
	end

	if not clgs.gameover then
		-- crosshair
		if clgs.health and clgs.health > 0 then
			local x, y
			x = (BASE_WIDTH - FontGetStringWidth("+", 0.2)) / 2.0
			y = (BASE_HEIGHT - FontGetStringHeight("+", 0.2)) / 2.0
			VideoDraw2DText("+", Vector2(x, y), Vector2(0.2, 0.2), Vector4(0.1, 0.9, 0.1, 0.5))
		end

		-- health
		meter = clgs.health
		if not meter or meter < 0 then
			meter = 0
		end
		VideoDraw2DFill(104, 14, 498, 8, Vector4(0, 0, 0, 0.5))
		VideoDraw2DFill(meter, 10, 500, 10, Vector4(1 - meter/100, meter/100, 0, 1))
		VideoDraw2DText(string.format("%3d", math.ceil(meter)), Vector2(468, 6), Vector2(0.2, 0.2), Vector4(1, 1, 0, 0.5)) -- ceil for preventing "zero life but alive"

		-- armor
		meter = clgs.armor
		if not meter or meter < 0 then
			meter = 0
		end
		VideoDraw2DFill(104, 14, 498, 24, Vector4(0, 0, 0, 0.5))
		VideoDraw2DFill(meter, 10, 500, 26, Vector4((1 - meter/100) * 0.3, (1 - meter/100) * 0.3, 0.5 + (1 - meter/100) * 0.5, 1))
		VideoDraw2DText(string.format("%3d", math.ceil(meter)), Vector2(468, 22), Vector2(0.2, 0.2), Vector4(1, 1, 0, 0.5)) -- ceil for preventing "zero life but alive"

		-- weapons
		wx = 0
		for i = 0, 31 do
			local weap = 1 << i

			if clgs.current_weapon == weap then
				VideoDraw2DText("*", Vector2(wx, 462), Vector2(0.2, 0.2), Vector4(1, 1, 0, 0.5))
			end

			if clgs.weapons and clgs.weapons & weap == weap then
				VideoDraw2DText(tostring(i), Vector2(wx, 446), Vector2(0.2, 0.2), Vector4(1, 1, 0, 0.5))
				wx = wx + 16
			end
		end

		-- ammo
		if (clgs.current_weapon_ammo_capacity or 0) > 0 then
			-- TODO: make this bar smaller if the capacity is small
			meter_int = clgs.current_weapon_ammo * 100 / clgs.current_weapon_ammo_capacity
			VideoDraw2DFill(14, 104, 618, 312, Vector4(0.25, 0.5, 0.25, 0.5))
			VideoDraw2DFill(10, meter_int, 620, 314 + (100 - meter_int), Vector4(0.5, 1, 0.5, 1))
			VideoDraw2DText(string.format("%3d", clgs.current_weapon_ammo), Vector2(588, 398), Vector2(0.2, 0.2), Vector4(1, 1, 0, 0.5))
		end
		if (clgs.current_ammo_capacity or 0) > 0 then
			meter_int = clgs.current_ammo * 50 / clgs.current_ammo_capacity
			VideoDraw2DFill(14, 54, 618, 424, Vector4(0.25, 0.25, 0.5, 0.5))
			VideoDraw2DFill(10, meter_int, 620, 426, Vector4(0.5, 0.5, 1, 1))
			VideoDraw2DText(string.format("%3d", clgs.current_ammo), Vector2(588, 422), Vector2(0.2, 0.2), Vector4(1, 1, 0, 0.5))
		end

		-- items
		wx = 0
		for i = 0, 31 do
			local item = 1 << i

			if clgs.items and clgs.items & item == item then
				VideoDraw2DText(tostring(i), Vector2(wx, 414), Vector2(0.2, 0.2), Vector4(1, 1, 0, 0.5))
				wx = wx + 16
			end
		end
	end

	-- centerprint
	if clgs.centerprint_received_time + CENTERPRINT_DURATION_MS > clgs.time and not isemptystring(clgs.centerprint_message) then
		local y = CENTERPRINT_Y -- initial position
		local alpha -- for fading out

		if clgs.centerprint_received_time + CENTERPRINT_DURATION_MS - clgs.time <= 1000 then -- the last second
			alpha = (clgs.centerprint_received_time + CENTERPRINT_DURATION_MS - clgs.time) / 1000.0
		else
			alpha = 1.0
		end

		-- draw each line
		local strlines = split_string(clgs.centerprint_message, "\n")
		for k,strline in ipairs(strlines) do
			-- calculate x position
			local x = BASE_WIDTH / 2.0
			x = x - FontGetStringWidth(strline, CENTERPRINT_SCALEX) / 2.0

			VideoDraw2DText(strline, Vector2(x, y), Vector2(CENTERPRINT_SCALEX, CENTERPRINT_SCALEY), Vector4(CENTERPRINT_R, CENTERPRINT_G, CENTERPRINT_B, alpha))

			y = y + FontGetStringHeight(strline, CENTERPRINT_SCALEY)
		end
	end

	-- TODO: messages with too many newlines will overflow the screen?
	-- print
	py = PRINT_Y_START -- initial position

	pnum = 0
	pidx = clgs.print_message_tail
	while pnum < MAX_NOTIFY_MESSAGES do
		if clgs.print_received_time[pidx] + PRINT_DURATION_MS > clgs.time and not isemptystring(clgs.print_message[pidx])  then
			local alpha -- for fading out

			if clgs.print_received_time[pidx] + PRINT_DURATION_MS - clgs.time <= 1000 then -- the last second
				alpha = (clgs.print_received_time[pidx] + PRINT_DURATION_MS - clgs.time) / 1000.0
			else
				alpha = 1.0
			end

			-- draw each line
			local strlines = split_string(clgs.print_message[pidx], "\n")
			for k,strline in ipairs(strlines) do
				VideoDraw2DText(strline, Vector2(PRINT_X, py), Vector2(PRINT_SCALEX, PRINT_SCALEY), Vector4(PRINT_R, PRINT_G, PRINT_B, alpha))

				py = py - FontGetStringHeight(strline, PRINT_SCALEY)				
			end
		end
		
		pnum = pnum + 1
		pidx = (pidx - 1 + MAX_NOTIFY_MESSAGES) % MAX_NOTIFY_MESSAGES -- we are doing it likes this because -1 % MAX_NOTIFY_MESSAGES is 1, % is division rest and not a real modulus
	end
end

-- ===================
-- Sbar_Frame
-- ===================
function Sbar_Frame()
	if not cl_ingame then
		return
	end
end

-- ===================
-- Sbar_Init
-- ===================
function Sbar_Init()
end

-- ===================
-- Sbar_Shutdown
-- ===================
function Sbar_Shutdown()
end