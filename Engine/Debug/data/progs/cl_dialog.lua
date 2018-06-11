-- cl_dialog.lua

-- ============================================================================
-- 
-- GUI dialog generation routines
-- 
-- The lower the index, the higher the priority for cursor selection while
-- stacked to other components.
-- 
-- Arrow keys (Up-down) use the index as the selection order
-- 
-- TODO:
-- Real arrow keys selection (based on position, including left-right.)
-- To do this (example for right arrow):
-- -Check the components at the RIGHT of the current selection
-- -Check the distance to the component and select the smaller distance (x^2 + x^y)
-- 
-- TODO: callback for when a component is selected?
-- 
-- ============================================================================

dialog = {}

cursor_pic = nil
show_cursor = false
cursor_x = 0
cursor_y = 0

-- ===================
-- CL_DialogClear
-- ===================
function CL_DialogClear()
	dialog = {}
	dialog.selected_component = -1
	dialog.components = {}
end

-- ===================
-- CL_DialogGetFreeSlot
-- ===================
function CL_DialogGetFreeSlot()
	local newelement_slot = #(dialog.components) + 1

	dialog.components[newelement_slot] = {}

	return newelement_slot
end

-- ===================
-- CL_DialogValidateSlot
-- ===================
function CL_DialogValidateSlot(slot)
	if dialog.components[slot] == nil then
		error("CL_DialogValidateSlot: invalid slot: " .. slot .. "\n")
	end
end

-- ===================
-- CL_DialogShowCursor
-- 
-- Call to show or hide the mouse/pointer/etc cursor
-- 
-- TODO: cursor offset, for cursors where the tip isn't in the top-left side (x == 0, y == 0)
-- ===================
function CL_DialogShowCursor(value)
	show_cursor = value
end

-- ===================
-- CL_DialogFrameCallback
-- ===================
function CL_DialogFrameCallback(framecallback)
	dialog.framecallback = framecallback
end

-- ===================
-- CL_DialogDefaultClickCallback
-- ===================
function CL_DialogDefaultClickCallback(defaultclickcallback)
	dialog.defaultclickcallback = defaultclickcallback
end

-- ===================
-- CL_DialogExitCallback
-- ===================
function CL_DialogExitCallback(exitcallback)
	dialog.exitcallback = exitcallback
end

-- ===================
-- CL_DialogSelectComponent
-- 
-- Forces selection of component "i"
-- ===================
function CL_DialogSelectComponent(i)
	CL_DialogValidateSlot(i)

	if not dialog.components[i] then
		error("Component " .. i .. " inactive!\n")
	end

	if not dialog.components[i].selectable then
		error("Component " .. i .. " not selectable!\n")
	end

	dialog.selected_component = i
end

-- ===================
-- CL_DialogSetShowCursor
-- 
-- Set if the cursor should appear for the current dialog
-- ===================
function CL_DialogSetShowCursor(value)
	dialog.show_cursor = value
end

-- ===================
-- CL_DialogAddPicture
-- 
-- If width == -1 or height == -1, will get the sizes from pic
-- selected_pic may be nil
-- Returns the index of the component
-- TODO: allow insivible components if pic == nil?
-- ===================
function CL_DialogAddPicture(x, y, width, height, pic, selected_pic, clickcallback, selectable)
	local slot

	slot = CL_DialogGetFreeSlot()
	dialog.components[slot].selectable = selectable
	dialog.components[slot].x = x
	dialog.components[slot].y = y

	if not pic then
		error("CL_DialogAddPicture: pic == nil\n")
	end

	if width == -1 then
		dialog.components[slot].width = pic.width
	elseif width > 0 then
		dialog.components[slot].width = width
	else
		error("CL_DialogAddPicture: Invalid width for picture " .. pic.name .. ": " .. width .. "\n")
	end

	if height == -1 then
		dialog.components[slot].height = pic.height
	elseif height > 0 then
		dialog.components[slot].height = height
	else
		error("CL_DialogAddPicture: Invalid height for picture " .. pic.name .. ": " .. height .. "\n")
	end

	dialog.components[slot].pic = pic
	dialog.components[slot].selected_pic = selected_pic
	dialog.components[slot].clickcallback = clickcallback

	return slot
end

-- ===================
-- CL_DialogAddText
-- 
-- If width == -1 or height == -1, will get the sizes from the text
-- rgba_selected and/or rgba_editing may be nil
-- Returns the index of the component
-- TODO: allow insivible components if text == nil?
-- ===================
function CL_DialogAddText(x, y, width, height, scalex, scaley, text, rgba, rgba_selected, rgba_editing, clickcallback, selectable)
	local slot

	slot = CL_DialogGetFreeSlot()
	dialog.components[slot].selectable = selectable
	dialog.components[slot].x = x
	dialog.components[slot].y = y

	if not text then
		error("CL_DialogAddText: text == nil\n")
	end

	if text == "" then
		error("CL_DialogAddText: text == \"\"\n")
	end

	if width == -1 then
		dialog.components[slot].width = FontGetStringWidth(text, scalex)
	elseif width > 0 then
		dialog.components[slot].width = width
	else
		error("CL_DialogAddText: Invalid width for text " .. text .. ": " .. width .. "\n")
	end

	if height == -1 then
		dialog.components[slot].height = FontGetStringHeight(text, scaley)
	elseif height > 0 then
		dialog.components[slot].height = height
	else
		error("CL_DialogAddText: Invalid height for text " .. text .. ": " .. height .. "\n")
	end

	dialog.components[slot].text = text
	dialog.components[slot].text_color = rgba:clone()
	if rgba_selected then
		dialog.components[slot].text_selectedcolor = rgba_selected:clone()
	else
		dialog.components[slot].text_selectedcolor = rgba:clone()
	end
	if rgba_editing then
		dialog.components[slot].text_editingcolor = rgba_editing:clone()
	else
		dialog.components[slot].text_editingcolor = rgba:clone()
	end
	dialog.components[slot].text_scalex = scalex
	dialog.components[slot].text_scaley = scaley
	dialog.components[slot].clickcallback = clickcallback

	return slot
end

-- ===================
-- CL_DialogUpdateText
-- 
-- If width == -1 or height == -1, will get the sizes from the text
-- rgba_selected and/or rgba_editing may be nil
-- TODO: allow insivible components if text == nil?
-- ===================
function CL_DialogUpdateText(slot, x, y, width, height, scalex, scaley, text, rgba, rgba_selected, rgba_editing, clickcallback, selectable)
	CL_DialogValidateSlot(slot)

	dialog.components[slot].selectable = selectable
	dialog.components[slot].x = x
	dialog.components[slot].y = y

	if not text then
		error("CL_DialogAddText: text == nil\n")
	end

	if text == "" then
		Sys_Error("CL_DialogAddText: text == \"\"\n")
	end

	if width == -1 then
		dialog.components[slot].width = FontGetStringWidth(text, scalex)
	elseif width > 0 then
		dialog.components[slot].width = width
	else
		error("CL_DialogAddText: Invalid width for text " .. text .. ": " .. width .. "\n")
	end

	if height == -1 then
		dialog.components[slot].height = FontGetStringHeight(text, scaley)
	elseif height > 0 then
		dialog.components[slot].height = height
	else
		error("CL_DialogAddText: Invalid height for text " .. text .. ": " .. height .. "\n")
	end

	dialog.components[slot].text = text
	dialog.components[slot].text_color = rgba:clone()
	if rgba_selected then
		dialog.components[slot].text_selectedcolor = rgba_selected:clone()
	else
		dialog.components[slot].text_selectedcolor = rgba:clone()
	end
	if rgba_editing then
		dialog.components[slot].text_editingcolor = rgba_editing:clone()
	else
		dialog.components[slot].text_editingcolor = rgba:clone()
	end
	dialog.components[slot].text_scalex = scalex
	dialog.components[slot].text_scaley = scaley
	dialog.components[slot].clickcallback = clickcallback
end

-- ===================
-- CL_DialogInit
-- ===================
function CL_DialogInit()
	CL_DialogClear()

	show_cursor = false
	cursor_x = Video2DAbsoluteFromUnitX(0.5)
	cursor_y = Video2DAbsoluteFromUnitY(0.5)
	cursor_pic = LoadTexture("menu/cursor", true, false, 1, 1)
end

-- ===================
-- CL_DialogShutdown
-- ===================
function CL_DialogShutdown()
end

-- ===================
-- CL_DialogDraw
-- ===================
function CL_DialogDraw()
	for i, v in ipairs(dialog.components) do
		if dialog.selected_component == i and v.selected_pic then
			VideoDraw2DPic(v.selected_pic.id, v.width, v.height, v.x, v.y)
		elseif v.pic then
			VideoDraw2DPic(v.pic.id, v.width, v.height, v.x, v.y)
		end

		if v.text then
			if dialog.selected_component ~= i then
				VideoDraw2DText(v.text, Vector2(v.x, v.y), Vector2(v.text_scalex, v.text_scaley), v.text_color)
			else
				-- component is selected, are we editing it?
				if InputGetKeyDest() == KEYDEST_TEXT then
					VideoDraw2DText(v.text, Vector2(v.x, v.y), Vector2(v.text_scalex, v.text_scaley), v.text_editingcolor)
				else
					VideoDraw2DText(v.text, Vector2(v.x, v.y), Vector2(v.text_scalex, v.text_scaley), v.text_selectedcolor)
				end
			end
		end
	end

	if show_cursor then
		VideoDraw2DPic(cursor_pic.id, cursor_pic.width, cursor_pic.height, cursor_x, cursor_y)
	end
end

-- ===================
-- CL_DialogKey
-- ===================
function CL_DialogKey(keyindex, down, analog_rel, analog_abs)
	if keyindex == MOUSE0_HORIZONTAL then
		cursor_x = Video2DAbsoluteFromUnitX(analog_abs)
	end

	if keyindex == MOUSE0_VERTICAL then
		cursor_y = Video2DAbsoluteFromUnitY(analog_abs)
	end

	if keyindex == MOUSE0_VERTICAL or keyindex == MOUSE0_HORIZONTAL then
		dialog.selected_component = -1
		for i, v in pairs(dialog.components) do
			if v.selectable and cursor_x >= v.x and cursor_x < v.x + v.width and cursor_y >= v.y and cursor_y < v.y + v.height then
				dialog.selected_component = i
				break
			end
		end
	end

	-- do not continue if we are not pressing a button
	if down ~= 1 then
		return
	end

	-- TODO: rollover, simplify?
	if keyindex == KEY_UP then
		for i = dialog.selected_component - 1, 1, -1 do -- because of the condition, there's no problem when selected_component == -1
			if dialog.components[i] and dialog.components[i].selectable then
				dialog.selected_component = i
				break
			end
		end
	end
	-- TODO: rollover, simplify?
	if keyindex == KEY_DOWN then
		for i = dialog.selected_component + 1, #dialog.components do -- because of the condition, there's no problem when selected_component == -1
			if dialog.components[i] and dialog.components[i].selectable then
				dialog.selected_component = i
				break
			end
		end
	end

	if keyindex == MOUSE0_BUTTON0 or keyindex == KEY_RETURN then -- FIXME: simplify this
		if dialog.selected_component ~= -1 then
			if dialog.components[dialog.selected_component].clickcallback and
				(keyindex == KEY_RETURN or (keyindex == MOUSE0_BUTTON0 and
				cursor_x >= dialog.components[dialog.selected_component].x and
				cursor_x <  dialog.components[dialog.selected_component].x + dialog.components[dialog.selected_component].width and
				cursor_y >= dialog.components[dialog.selected_component].y and
				cursor_y <  dialog.components[dialog.selected_component].y + dialog.components[dialog.selected_component].height)) then
				dialog.components[dialog.selected_component].clickcallback(dialog.selected_component)
			elseif dialog.defaultclickcallback then
				dialog.defaultclickcallback()
			end
		elseif dialog.defaultclickcallback then
			dialog.defaultclickcallback()
		end
	end

	if keyindex == KEY_ESC and dialog.exitcallback then
		dialog.exitcallback()
	end

	-- TODO: only works if the bind is exactly this way
	local consolebind = InputBindFromKey(keyindex)
	if consolebind == "toggleconsole" then
		LocalCmd("toggleconsole")
	end
end

-- ===================
-- CL_DialogFrame
-- ===================
function CL_DialogFrame()
	-- handle cursor showing
	CL_DialogShowCursor(dialog.show_cursor)

	if dialog.framecallback then
		dialog.framecallback()
	end
end