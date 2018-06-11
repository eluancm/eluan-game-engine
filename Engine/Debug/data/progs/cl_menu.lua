-- cl_menu.lua

-- ============================================================================
-- 
-- Game-specific menu routines
-- 
-- ============================================================================

-- ============================================================================
-- 
-- Helpers start
-- 
-- ============================================================================

-- must call MenuHelpers_PutClientServerStatus to set these before calling MenuHelpers_UpdateClientServerStatus
server_status = nil
client_status = nil

-- ===================
-- MenuHelpers_UpdateClientServerStatus
-- ===================
function MenuHelpers_UpdateClientServerStatus()
	local green = Vector4(0.1, 1, 0.1, 1)

	-- update info TODO: not all may be visible with the current code
	if sv_loading then
		CL_DialogUpdateText(server_status, 10, BASE_HEIGHT - 70, -1, -1, 0.2, 0.2, "Server: loading", green, nil, nil, nil, false)
	elseif sv_listening then
		CL_DialogUpdateText(server_status, 10, BASE_HEIGHT - 70, -1, -1, 0.2, 0.2, "Server: started", green, nil, nil, nil, false)
	else
		CL_DialogUpdateText(server_status, 10, BASE_HEIGHT - 70, -1, -1, 0.2, 0.2, "Server: not started", green, nil, nil, nil, false)
	end

	if cl_connected and not cl_ingame then
		CL_DialogUpdateText(client_status, 10, BASE_HEIGHT - 90, -1, -1, 0.2, 0.2, "Client: connecting", green, nil, nil, nil, false)
	elseif cl_ingame then
		CL_DialogUpdateText(client_status, 10, BASE_HEIGHT - 90, -1, -1, 0.2, 0.2, "Client: connected", green, nil, nil, nil, false)
	else
		CL_DialogUpdateText(client_status, 10, BASE_HEIGHT - 90, -1, -1, 0.2, 0.2, "Client: not connected", green, nil, nil, nil, false)
	end
end

-- ===================
-- MenuHelpers_PutClientServerStatus
-- ===================
function MenuHelpers_PutClientServerStatus()
	local green = Vector4(0.1, 1, 0.1, 1)

	-- since this function is called only when entering the menu, the following info must be updated if changed while the menu is open
	server_status = CL_DialogAddText(10, BASE_HEIGHT - 70, -1, -1, 0.2, 0.2, "Server: no info...", green, nil, nil, nil, false)
	client_status = CL_DialogAddText(10, BASE_HEIGHT - 90, -1, -1, 0.2, 0.2, "Client: no info...", green, nil, nil, nil, false)
end

-- ============================================================================
-- 
-- Helpers end
-- 
-- ============================================================================

m_rgba_title = Vector4(0.8, 0.8, 0.5, 1)
m_rgba = Vector4(0.5, 0.5, 0.5, 1)
m_rgba_selected = Vector4(0, 0, 1, 1)
m_rgba_editing = Vector4(0.8, 0.8, 0.1, 1)

m_clicksnd = nil

LOADING_TIMEOUT = 1000 -- not a real timeout, it only checks that we are REALLY loading after this time, then drop to console if not
m_loading_server = false
m_loading_server_timeout = 0
m_loading_client = false
m_loading_client_timeout = 0

-- ============================================================================
-- 
-- Splash screen
-- 
-- ============================================================================

SPLASH_TIME = 4000 -- after four seconds

splashscreen = nil
m_counter = nil

-- ===================
-- Menu_Splash_Frame
-- ===================
function Menu_Splash_Frame()
	if m_counter >= SPLASH_TIME then
		Menu_Main()
	else
		m_counter = m_counter + host_frametime
	end
end

-- ===================
-- Menu_Splash_Exit
-- ===================
function Menu_Splash_Exit()
	m_counter = SPLASH_TIME -- quit splash screen
end

-- ===================
-- Menu_Splash
-- ===================
function Menu_Splash(void)
	m_counter = 0

	CL_DialogClear()
	CL_DialogAddPicture(0, 0, -1, -1, splashscreen, nil, nil, false)
	CL_DialogFrameCallback(Menu_Splash_Frame)
	CL_DialogDefaultClickCallback(Menu_Splash_Exit)
	CL_DialogExitCallback(Menu_Splash_Exit)
	CL_DialogSetShowCursor(false)
end

-- ============================================================================
-- 
-- Main Menu
-- 
-- ============================================================================

m_mainback = nil

-- ===================
-- Menu_Main_Frame
-- ===================
function Menu_Main_Frame()
	MenuHelpers_UpdateClientServerStatus()
end

-- ===================
-- Menu_Main_NewGame
-- ===================
function Menu_Main_NewGame(slot)
	StartLocalSound(m_clicksnd)

	LocalCmd("disconnect")
	LocalCmd("sv_maxplayers 1") -- TODO: remember to set this when loading games
	LocalCmd("map start")
	m_loading_server = true
	m_loading_server_timeout = host_realtime + LOADING_TIMEOUT
	m_loading_client = true
	m_loading_client_timeout = host_realtime + LOADING_TIMEOUT
	VideoForceRefresh() -- force updating to guarantee the loading messages
end

-- ===================
-- Menu_Main_Multiplayer
-- ===================
function Menu_Main_Multiplayer(slot)
	StartLocalSound(m_clicksnd)
	Menu_Multiplayer()
end

-- ===================
-- Menu_Main_VideoOptions
-- ===================
function Menu_Main_VideoOptions(slot)
	StartLocalSound(m_clicksnd)
	Menu_Video()
end

-- ===================
-- Menu_Main_Quit
-- ===================
function Menu_Main_Quit(slot)
	StartLocalSound(m_clicksnd)
	LocalCmd("quit")
end

-- ===================
-- Menu_Main_Exit
-- ===================
function Menu_Main_Exit()
	-- never allow the user to exit, to avoid black screen - unless we have a server running
	if sv_listening then
		StartLocalSound(m_clicksnd)
		LocalCmd("toggleconsole")
		LocalCmd("togglemenu")
	end
end

-- ===================
-- Menu_Main
-- ===================
function Menu_Main()
	local red = Vector4(1, 0.1, 0.1, 1)

	if CL_MenuTypeInGame() then
		Menu_InGame()
		return
	end

	CL_DialogClear()
	CL_DialogAddPicture(0, 0, -1, -1, m_mainback, nil, nil, false)

	CL_DialogAddText(15, 5, -1, -1, 0.4, 0.4, "Main Menu", m_rgba_title, nil, nil, nil, false)

	CL_DialogSelectComponent(CL_DialogAddText(45, 185, -1, -1, 0.3, 0.3, "Single Player", m_rgba, m_rgba_selected, nil, Menu_Main_NewGame, true))
	CL_DialogAddText(45, 185 + 30, -1, -1, 0.3, 0.3, "Multiplayer", m_rgba, m_rgba_selected, nil, Menu_Main_Multiplayer, true)
	CL_DialogAddText(45, 185 + 60, -1, -1, 0.3, 0.3, "Video Options", m_rgba, m_rgba_selected, nil, Menu_Main_VideoOptions, true)
	CL_DialogAddText(45, 185 + 90, -1, -1, 0.3, 0.3, "Quit", m_rgba, m_rgba_selected, nil, Menu_Main_Quit, true)

	MenuHelpers_PutClientServerStatus()

	CL_DialogAddText(10, BASE_HEIGHT - 30, -1, -1, 0.2, 0.2, "Engine Test (c) 2013-2016 Eluan Costa Miranda", red, nil, nil, nil, false)
	CL_DialogFrameCallback(Menu_Main_Frame)
	CL_DialogDefaultClickCallback(nil)
	CL_DialogExitCallback(Menu_Main_Exit)
	CL_DialogSetShowCursor(true)
end

-- ============================================================================
-- 
-- Multiplayer Menu
-- 
-- ============================================================================

-- ===================
-- Menu_Multiplayer_Frame
-- ===================
function Menu_Multiplayer_Frame()
	MenuHelpers_UpdateClientServerStatus()
end

-- ===================
-- Menu_Multiplayer_NewGame
-- ===================
function Menu_Multiplayer_NewGame(slot)
	StartLocalSound(m_clicksnd)
	Menu_MultiplayerStart()
end

-- ===================
-- Menu_Multiplayer_Joingame
-- ===================
function Menu_Multiplayer_JoinGame(slot)
	StartLocalSound(m_clicksnd)
	Menu_MultiplayerConnect()
end

-- ===================
-- Menu_Multiplayer_Back
-- ===================
function Menu_Multiplayer_Back(slot)
	StartLocalSound(m_clicksnd)
	Menu_Main()
end

-- ===================
-- Menu_Multiplayer_Exit
-- ===================
function Menu_Multiplayer_Exit()
	StartLocalSound(m_clicksnd)
	Menu_Main()
end

-- ===================
-- Menu_Multiplayer
-- ===================
function Menu_Multiplayer()
	local red = Vector4(1, 0.1, 0.1, 1)

	CL_DialogClear()
	CL_DialogAddPicture(0, 0, -1, -1, m_mainback, nil, nil, false)

	CL_DialogAddText(15, 5, -1, -1, 0.4, 0.4, "Multiplayer", m_rgba_title, nil, nil, nil, false)

	CL_DialogSelectComponent(CL_DialogAddText(45, 185, -1, -1, 0.3, 0.3, "New Game (Start Server)", m_rgba, m_rgba_selected, nil, Menu_Multiplayer_NewGame, true))
	CL_DialogAddText(45, 185 + 30, -1, -1, 0.3, 0.3, "Join Game (Connect to Existing Server)", m_rgba, m_rgba_selected, nil, Menu_Multiplayer_JoinGame, true)
	CL_DialogAddText(45, 185 + 60, -1, -1, 0.3, 0.3, "Back", m_rgba, m_rgba_selected, nil, Menu_Multiplayer_Back, true)

	MenuHelpers_PutClientServerStatus()

	CL_DialogAddText(10, BASE_HEIGHT - 30, -1, -1, 0.2, 0.2, "Engine Test (c) 2013-2016 Eluan Costa Miranda", red, nil, nil, nil, false)
	CL_DialogFrameCallback(Menu_Multiplayer_Frame)
	CL_DialogDefaultClickCallback(nil)
	CL_DialogExitCallback(Menu_Multiplayer_Exit)
	CL_DialogSetShowCursor(true)
end

-- ============================================================================
-- 
-- Multiplayer Start Server Menu
-- 
-- ============================================================================

server_maxplayers = 0
server_listenport = 0
server_map = 0

-- ===================
-- Menu_MultiplayerStart_Frame
-- ===================
function Menu_MultiplayerStart_Frame()
	MenuHelpers_UpdateClientServerStatus()
end

-- ===================
-- Menu_MultiplayerStart_CreateServer
-- ===================
function Menu_MultiplayerStart_CreateServer(slot)
	StartLocalSound(m_clicksnd)

	ShutdownServer() -- TODO: cmd
	LocalCmd("sv_maxplayers " .. dialog.components[server_maxplayers].text)
	LocalCmd("sv_listenport " .. dialog.components[server_listenport].text)
	LocalCmd("startserver " .. dialog.components[server_map].text)
	m_loading_server = true
	m_loading_server_timeout = host_realtime + LOADING_TIMEOUT
	VideoForceRefresh() -- force updating to guarantee the loading messages
end

-- ===================
-- Menu_MultiplayerStart_Back
-- ===================
function Menu_MultiplayerStart_Back(slot)
	StartLocalSound(m_clicksnd)
	Menu_Multiplayer()
end

-- ===================
-- Menu_MultiplayerStart_Exit
-- ===================
function Menu_MultiplayerStart_Exit()
	StartLocalSound(m_clicksnd)
	Menu_Multiplayer()
end

-- ===================
-- Menu_MultiplayerStart
-- ===================
function Menu_MultiplayerStart()
	local red = Vector4(1, 0.1, 0.1, 1)

	CL_DialogClear()
	CL_DialogAddPicture(0, 0, -1, -1, m_mainback, nil, nil, false)

	-- title will change if we are connected to a server
	CL_DialogAddText(15, 5, -1, -1, 0.4, 0.4, "Multiplayer", m_rgba_title, nil, nil, nil, false)

	-- TODO: validate these after editing (have filters for inputs in the dialog code)
	CL_DialogAddText(45, 185, -1, -1, 0.25, 0.25, "Listen Port:", m_rgba_title, nil, nil, nil, false)
	server_listenport = CL_DialogAddText(45, 185 + 20, -1, -1, 0.25, 0.25, CvarGetString("_sv_listenport"), m_rgba, m_rgba_selected, m_rgba_editing, CL_DialogEditText, true)
	CL_DialogAddText(45, 185 + 40, -1, -1, 0.25, 0.25, "Map:", m_rgba_title, nil, nil, nil, false)
	server_map = CL_DialogAddText(45, 185 + 60, -1, -1, 0.25, 0.25, "start", m_rgba, m_rgba_selected, m_rgba_editing, CL_DialogEditText, true) -- TODO: default start map from config
	CL_DialogAddText(45, 185 + 80, -1, -1, 0.25, 0.25, "Max Players:", m_rgba_title, nil, nil, nil, false)
	server_maxplayers = CL_DialogAddText(45, 185 + 100, -1, -1, 0.25, 0.25, CvarGetString("_sv_maxplayers"), m_rgba, m_rgba_selected, m_rgba_editing, CL_DialogEditText, true)

	CL_DialogSelectComponent(CL_DialogAddText(45, 185 + 130, -1, -1, 0.3, 0.3, "Create Server", m_rgba, m_rgba_selected, nil, Menu_MultiplayerStart_CreateServer, true))
	CL_DialogAddText(45, 185 + 160, -1, -1, 0.3, 0.3, "Back", m_rgba, m_rgba_selected, nil, Menu_MultiplayerStart_Back, true)

	MenuHelpers_PutClientServerStatus()

	CL_DialogAddText(10, BASE_HEIGHT - 30, -1, -1, 0.2, 0.2, "Engine Test (c) 2013-2016 Eluan Costa Miranda", red, nil, nil, nil, false)
	CL_DialogFrameCallback(Menu_MultiplayerStart_Frame)
	CL_DialogDefaultClickCallback(nil)
	CL_DialogExitCallback(Menu_MultiplayerStart_Exit)
	CL_DialogSetShowCursor(true)
end

-- ============================================================================
-- 
-- Multiplayer Connect to Server Menu
-- 
-- ============================================================================

client_remotehost = 0
client_remoteport = 0
client_name = ""

-- ===================
-- Menu_MultiplayerConnect_Frame
-- ===================
function Menu_MultiplayerConnect_Frame()
	MenuHelpers_UpdateClientServerStatus()
end

-- ===================
-- Menu_MultiplayerConnect_JoinGame
-- ===================
function Menu_MultiplayerConnect_JoinGame(slot)
	StartLocalSound(m_clicksnd)

	DisconnectClient() -- TODO: cmd
	LocalCmd("cl_remoteserver " .. dialog.components[client_remotehost].text)
	LocalCmd("cl_remoteport " .. dialog.components[client_remoteport].text)
	LocalCmd("name " .. dialog.components[client_name].text)
	LocalCmd("connect")
	m_loading_client = true
	m_loading_client_timeout = host_realtime + LOADING_TIMEOUT
end

-- ===================
-- Menu_MultiplayerConnect_Back
-- ===================
function Menu_MultiplayerConnect_Back(slot)
	StartLocalSound(m_clicksnd)
	Menu_Multiplayer()
end

-- ===================
-- Menu_MultiplayerConnect_Exit
-- ===================
function Menu_MultiplayerConnect_Exit()
	StartLocalSound(m_clicksnd)
	Menu_Multiplayer()
end

-- ===================
-- Menu_MultiplayerConnect
-- ===================
function Menu_MultiplayerConnect()
	local red = Vector4(1, 0.1, 0.1, 1)

	CL_DialogClear()
	CL_DialogAddPicture(0, 0, -1, -1, m_mainback, nil, nil, false)

	-- title will change if we are connected to a server
	CL_DialogAddText(15, 5, -1, -1, 0.4, 0.4, "Multiplayer", m_rgba_title, nil, nil, nil, false)

	-- TODO: validate these after editing
	CL_DialogAddText(45, 185, -1, -1, 0.25, 0.25, "Address:", m_rgba_title, nil, nil, nil, false)
	client_remotehost = CL_DialogAddText(45, 185 + 20, -1, -1, 0.25, 0.25, CvarGetString("_cl_remoteserver"), m_rgba, m_rgba_selected, m_rgba_editing, CL_DialogEditText, true)
	CL_DialogAddText(45, 185 + 40, -1, -1, 0.25, 0.25, "Port:", m_rgba_title, nil, nil, nil, false)
	client_remoteport = CL_DialogAddText(45, 185 + 60, -1, -1, 0.25, 0.25, CvarGetString("_cl_remoteport"), m_rgba, m_rgba_selected, m_rgba_editing, CL_DialogEditText, true)
	CL_DialogAddText(45, 185 + 80, -1, -1, 0.25, 0.25, "Your Name:", m_rgba_title, nil, nil, nil, false)
	client_name = CL_DialogAddText(45, 185 + 100, -1, -1, 0.25, 0.25, CvarGetString("_cl_name"), m_rgba, m_rgba_selected, m_rgba_editing, CL_DialogEditText, true)

	CL_DialogSelectComponent(CL_DialogAddText(45, 185 + 130, -1, -1, 0.3, 0.3, "Join Game", m_rgba, m_rgba_selected, nil, Menu_MultiplayerConnect_JoinGame, true))
	CL_DialogAddText(45, 185 + 160, -1, -1, 0.3, 0.3, "Back", m_rgba, m_rgba_selected, nil, Menu_MultiplayerConnect_Back, true)

	MenuHelpers_PutClientServerStatus()

	CL_DialogAddText(10, BASE_HEIGHT - 30, -1, -1, 0.2, 0.2, "Engine Test (c) 2013-2016 Eluan Costa Miranda", red, nil, nil, nil, false)
	CL_DialogFrameCallback(Menu_MultiplayerConnect_Frame)
	CL_DialogDefaultClickCallback(nil)
	CL_DialogExitCallback(Menu_MultiplayerConnect_Exit)
	CL_DialogSetShowCursor(true)
end

-- ============================================================================
-- 
-- Video Menu
-- 
-- ============================================================================

current_winres = nil
current_mode = nil
mod_windowed640x480 = nil
mod_windowed800x600 = nil
mod_windowed768x480 = nil
mod_fullscreen = nil

-- ===================
-- Menu_VideoFrame
-- ===================
function Menu_VideoFrame()
	local green = Vector4(0.1, 1, 0.1, 1)

	-- update info
	CL_DialogUpdateText(current_winres, 10, BASE_HEIGHT - 70, -1, -1, 0.2, 0.2, "Current windowed resolution: " .. CvarGetInteger("_vid_windowedwidth") .. "x" .. CvarGetInteger("_vid_windowedheight"), green, nil, nil, nil, false)
	if CvarGetBoolean("_vid_fullscreen") then
		CL_DialogUpdateText(current_mode, 10, BASE_HEIGHT - 90, -1, -1, 0.2, 0.2, "Current video mode: " .. "fullscreen (desktop resolution)", green, nil, nil, nil, false)
	else
		CL_DialogUpdateText(current_mode, 10, BASE_HEIGHT - 90, -1, -1, 0.2, 0.2, "Current video mode: " .. "windowed", green, nil, nil, nil, false)
	end
end

-- ===================
-- Menu_VideoSetResolution
-- 
-- TODO: maximized window
-- ===================
function Menu_VideoSetResolution(slot)
	StartLocalSound(m_clicksnd)

	if slot == mod_windowed640x480 then
		LocalCmd("vid_setwindowed 640 480")
		return
	elseif slot == mod_windowed800x600 then
		LocalCmd("vid_setwindowed 800 600")
		return
	elseif slot == mod_windowed768x480 then
		LocalCmd("vid_setwindowed 768 480")
		return
	elseif slot == mod_fullscreen then
		LocalCmd("vid_setfullscreen")
		return
	end

	error("Menu_VideoSetResolution: unknown mode " .. slot .. "\n")
end

-- ===================
-- Menu_VideoBack
-- ===================
function Menu_VideoBack(slot)
	StartLocalSound(m_clicksnd)
	Menu_Main()
end

-- ===================
-- Menu_VideoExit
-- ===================
function Menu_VideoExit()
	StartLocalSound(m_clicksnd)
	Menu_Main()
end

-- ===================
-- Menu_Video
-- ===================
function Menu_Video()
	local red = Vector4(1, 0.1, 0.1, 1)
	local green = Vector4(0.1, 1, 0.1, 1)

	CL_DialogClear()
	CL_DialogAddPicture(0, 0, -1, -1, m_mainback, nil, nil, false)

	-- title will change if we are connected to a server
	CL_DialogAddText(15, 5, -1, -1, 0.4, 0.4, "Video Options", m_rgba_title, nil, nil, nil, false)

	mod_windowed640x480 = CL_DialogAddText(45, 185, -1, -1, 0.3, 0.3, "Set 640x480 windowed", m_rgba, m_rgba_selected, nil, Menu_VideoSetResolution, true)
	CL_DialogSelectComponent(mod_windowed640x480)
	mod_windowed800x600 = CL_DialogAddText(45, 185 + 30, -1, -1, 0.3, 0.3, "Set 800x600 windowed", m_rgba, m_rgba_selected, nil, Menu_VideoSetResolution, true)
	mod_windowed768x480 = CL_DialogAddText(45, 185 + 60, -1, -1, 0.3, 0.3, "Set 768x480 windowed", m_rgba, m_rgba_selected, nil, Menu_VideoSetResolution, true)
	mod_fullscreen = CL_DialogAddText(45, 185 + 90, -1, -1, 0.3, 0.3, "Set fullscreen", m_rgba, m_rgba_selected, nil, Menu_VideoSetResolution, true)
	CL_DialogAddText(45, 185 + 120, -1, -1, 0.3, 0.3, "Back", m_rgba, m_rgba_selected, nil, Menu_VideoBack, true)

	-- since this function is called only when entering the menu, the following info must be updated if changed while the menu is open
	current_winres = CL_DialogAddText(10, BASE_HEIGHT - 70, -1, -1, 0.2, 0.2, "Current windowed resolution: no info...", green, nil, nil, nil, false)
	current_mode = CL_DialogAddText(10, BASE_HEIGHT - 90, -1, -1, 0.2, 0.2, "Current video mode: no info...", green, nil, nil, nil, false)

	CL_DialogAddText(10, BASE_HEIGHT - 30, -1, -1, 0.2, 0.2, "Engine Test (c) 2013-2016 Eluan Costa Miranda", red, nil, nil, nil, false)
	CL_DialogFrameCallback(Menu_VideoFrame)
	CL_DialogDefaultClickCallback(nil)
	CL_DialogExitCallback(Menu_VideoExit)
	CL_DialogSetShowCursor(true)
end

-- ============================================================================
-- 
-- In Game Menu
-- 
-- ============================================================================

-- ===================
-- Menu_InGame_Frame
-- ===================
function Menu_InGame_Frame()
	MenuHelpers_UpdateClientServerStatus()
end

-- ===================
-- Menu_InGame_Disconnect
-- ===================
function Menu_InGame_Disconnect(slot)
	StartLocalSound(m_clicksnd)
	LocalCmdForce("disconnect")
	Menu_Main() -- TODO: make it not instantaneous
end

-- ===================
-- Menu_InGame_VideoOptions
-- ===================
function Menu_InGame_VideoOptions(slot)
	StartLocalSound(m_clicksnd)
	Menu_Video()
end

-- ===================
-- Menu_InGame_Quit
-- ===================
function Menu_InGame_Quit(slot)
	StartLocalSound(m_clicksnd)
	LocalCmd("quit")
end

-- ===================
-- Menu_InGame__Exit
-- ===================
function Menu_InGame_Exit()
	StartLocalSound(m_clicksnd)
	CL_MenuClose()
end

-- ===================
-- Menu_InGame
-- ===================
function Menu_InGame()
	local red = Vector4(1, 0.1, 0.1, 1)

	CL_DialogClear()
	CL_DialogAddPicture(0, 0, -1, -1, m_mainback, nil, nil, false)

	-- title will change if we are connected to a server
	CL_DialogAddText(15, 5, -1, -1, 0.4, 0.4, "In-Game Menu", m_rgba_title, nil, nil, nil, false)

	if sv_listening or sv_loading then
		if CvarGetInteger("_sv_maxplayers") == 1 then
			CL_DialogSelectComponent(CL_DialogAddText(45, 185, -1, -1, 0.3, 0.3, "End Game", m_rgba, m_rgba_selected, nil, Menu_InGame_Disconnect, true))
		else
			CL_DialogSelectComponent(CL_DialogAddText(45, 185, -1, -1, 0.3, 0.3, "Shutdown Server (and kick all clients)", m_rgba, m_rgba_selected, nil, Menu_InGame_Disconnect, true))
		end
	elseif cl_connected then
		CL_DialogSelectComponent(CL_DialogAddText(45, 185, -1, -1, 0.3, 0.3, "Disconnect", m_rgba, m_rgba_selected, nil, Menu_InGame_Disconnect, true))
	else
		CL_MenuClose() -- TODO: it may get opened inadvertly
		return
	end
	CL_DialogAddText(45, 185 + 30, -1, -1, 0.3, 0.3, "Video Options", m_rgba, m_rgba_selected, nil, Menu_InGame_VideoOptions, true)
	CL_DialogAddText(45, 185 + 60, -1, -1, 0.3, 0.3, "Quit", m_rgba, m_rgba_selected, nil, Menu_InGame_Quit, true)

	MenuHelpers_PutClientServerStatus()

	CL_DialogAddText(10, BASE_HEIGHT - 30, -1, -1, 0.2, 0.2, "Engine Test (c) 2013-2016 Eluan Costa Miranda", red, nil, nil, nil, false)
	CL_DialogFrameCallback(Menu_InGame_Frame)
	CL_DialogDefaultClickCallback(nil)
	CL_DialogExitCallback(Menu_InGame_Exit)
	CL_DialogSetShowCursor(true)
end

-- ===================
-- MenuInfoBox
-- 
-- Draws an information box at the center of the screen
-- ===================
function MenuInfoBox(text, scale)
	local textwidth, textheight

	textwidth = FontGetStringWidth(text, scale)
	textheight = FontGetStringHeight(text, scale)

	VideoDraw2DFill(textwidth + 15, textheight + 15, (BASE_WIDTH - (textwidth + 15)) / 2, (BASE_HEIGHT - (textheight + 15)) / 2, Vector4(0, 0, 0, 0.8))
	VideoDraw2DFill(textwidth + 10, textheight + 10, (BASE_WIDTH - (textwidth + 10)) / 2, (BASE_HEIGHT - (textheight + 10)) / 2, Vector4(0.3, 0.3, 0.3, 0.8))
	VideoDraw2DText(text, Vector2((BASE_WIDTH - textwidth) / 2.0, (BASE_HEIGHT - textheight - 32 * scale) / 2.0), Vector2(scale, scale), Vector4(1, 1, 1, 1))
end