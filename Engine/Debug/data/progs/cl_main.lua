-- cl_main.lua

-- TODO: here and in server code: too many redundant initializations leftover from the C port (because LUA has a GC, etc)

-- no state from the client is saved, but maintaining everything in this table makes it easy to clear leftovers
clgs = {}

LoadProg("shared_defs")
LoadProg("shared_lib")
LoadProg("shared_pred")
LoadProg("cl_dialog")
LoadProg("cl_menu")
LoadProg("cl_sbar")

-- =====================================================
--
-- FUNCTIONS CALLED EXCLUSIVELY BY C CODE
--
-- =====================================================

-- ===================
-- StartParticle
-- 
-- Reads a particle effect from the network buffer and starts it
-- ===================
function StartParticle(msg, readdata, datalen)
	local parttype = MessageReadByte(msg, readdata, datalen)
	local origin = MessageReadVec3(msg, readdata, datalen)
	local newpart = {}

	if parttype == PARTICLE_EXPLOSION then
		for i = 0, 63 do
			newpart.org = origin -- no need to clone here, pointer is fine
			newpart.vel = Vector3(math.random(-30000, 30000) / 10000, math.random(-30000, 30000) / 10000, math.random(-30000, 30000) / 10000)
			newpart.color = Vector4(math.random(240, 244), math.random(138, 198), math.random(15, 128), 255)
			newpart.scale = 2.0
			newpart.timelimit = clgs.time + math.random(400, 650)

			if not ParticleAlloc(newpart) then
				return
			end
		end
	elseif parttype == PARTICLE_BLOOD then
		for i = 0,31 do
			newpart.org = origin -- no need to clone here, pointer is fine
			newpart.vel = Vector3(math.random(-10000, 10000) / 10000, math.random(-10000, 10000) / 10000, math.random(-10000, 10000) / 10000)
			newpart.color = Vector4(math.random(128, 255), math.random(0, 16), math.random(0, 16), 255)
			newpart.scale = 1.0
			newpart.timelimit = clgs.time + math.random(50, 500)

			if not ParticleAlloc(newpart) then
				return
			end
		end
	elseif parttype == PARTICLE_GUNSHOT then
		for i = 0, 15 do
			newpart.org = origin -- no need to clone here, pointer is fine
			newpart.vel = Vector3(math.random(-10000, 10000) / 10000, math.random(-10000, 10000) / 10000, math.random(-10000, 10000) / 10000)
			newpart.color = Vector4(math.random(240, 244), math.random(138, 198), math.random(15, 128), 255)
			newpart.scale = 0.5
			newpart.timelimit = clgs.time + math.random(50, 250)

			if not ParticleAlloc(newpart) then
				return
			end
		end
	else
		error("StartParticle: unknown type " .. parttype .. "\n")
	end
end

-- ===================
-- NewGame
-- 
-- Called when connecting to a server
-- ===================
function NewGame()
	clgs = {}
	
	InputReset()

	clgs.time = 0
	clgs.frametime = 0
	clgs.stall_frames = 0
	clgs.remote_host = ""
	clgs.my_ent = nil
	clgs.signon = 0
	clgs.paused = false
	
	clgs.centerprint_received_time = 0
	clgs.centerprint_message = nil

	clgs.print_received_time = {}
	clgs.print_message = {}
	for i = 0, MAX_NOTIFY_MESSAGES - 1 do
		clgs.print_received_time[i] = 0
		clgs.print_message[i] = nil
	end
	clgs.print_message_head = 0
	clgs.print_message_tail = 0

	clgs.scorenames = {}
	clgs.scorefrags = {}
	clgs.scoretimes = {}
	clgs.scorepings = {}
	clgs.scorepls = {}
end

-- ===================
-- Frame
-- 
-- Called every frame when connected
-- ===================
function Frame(frametime, stall_frames, my_ent, signon, remote_host, paused)
	if not paused then
		clgs.time = clgs.time + frametime
	end
	clgs.frametime = frametime
	clgs.stall_frames = stall_frames
	clgs.my_ent = my_ent
	clgs.signon = signon
	clgs.remote_host = remote_host
	clgs.paused = paused
end

-- ===================
-- PredPreThink
-- 
-- Called before each prediction step (including when doing reconciliation)
-- ===================
function PredPreThink(frametime)
end

-- ===================
-- PredPostThink
-- 
-- Called after each prediction step (including when doing reconciliation)
-- ===================
function PredPreThink(frametime)
end

-- ===================
-- ParseServerMessages
-- 
-- Called to process extra state from the server
-- ===================
function ParseServerMessages(cmd, msg, readdata, datalen)
	if cmd == SVC_UPDATESTATS then
		clgs.health = MessageReadVec1(msg, readdata, datalen)
		clgs.armor = MessageReadVec1(msg, readdata, datalen)
		clgs.weapons = MessageReadInt(msg, readdata, datalen)
		clgs.current_weapon_ammo = MessageReadShort(msg, readdata, datalen)
		clgs.current_weapon_ammo_capacity = MessageReadShort(msg, readdata, datalen)
		clgs.current_ammo = MessageReadShort(msg, readdata, datalen)
		clgs.current_ammo_capacity = MessageReadShort(msg, readdata, datalen)
		clgs.current_weapon = MessageReadInt(msg, readdata, datalen)
		clgs.items = MessageReadInt(msg, readdata, datalen)
		for i = 0, MAX_CLIENTS - 1 do
			clgs.scorenames[i] = MessageReadString(msg, readdata, datalen)
			clgs.scorefrags[i] = MessageReadInt(msg, readdata, datalen)
			clgs.scoretimes[i] = MessageReadShort(msg, readdata, datalen)
			clgs.scorepings[i] = MessageReadShort(msg, readdata, datalen)
			clgs.scorepls[i] = MessageReadShort(msg, readdata, datalen)
		end
	elseif cmd == SVC_CENTERPRINT then
		clgs.centerprint_message = MessageReadString(msg, readdata, datalen)
		clgs.centerprint_received_time = clgs.time
		PrintC("--------------------------\n" .. clgs.centerprint_message .. "\n--------------------------\n")
	elseif cmd == SVC_PRINT then
		clgs.print_message[clgs.print_message_tail] = MessageReadString(msg, readdata, datalen)
		clgs.print_received_time[clgs.print_message_tail] = clgs.time
		PrintC(clgs.print_message[clgs.print_message_tail])
		clgs.print_message_tail = (clgs.print_message_tail + 1) % MAX_NOTIFY_MESSAGES
		if clgs.print_message_tail == clgs.print_message_head then
			clgs.print_message_head = (clgs.print_message_head + 1) % MAX_NOTIFY_MESSAGES
		end
	elseif cmd == SVC_ENDGAME then
		clgs.gameover = true
	else
		return false -- not processed
	end

	return true -- processed
end

-- ===================
-- SendServerMessages
-- 
-- Called to send game-specific commands
-- ===================
function SendServerMessages()
	clgis = InputGetState()

	CLMessageWriteByte(CLC_MOVE)
	CLMessageWriteVec3(clgis.in_mov)
	CLMessageWriteVec3(clgis.in_aim)
	CLMessageWriteByte(clgis.in_buttons)
	CLMessageWriteByte(clgis.in_triggerbuttons)
	CLMessageWriteByte(clgis.impulse)
	-- TODO: too many bits for a small frametime
	CLMessageWriteTime(clgis.frametime)
	CLMessageWriteShort(clgis.seq)
	MessageSendToServerUnreliable() -- TODO FIXME: sent triggerbuttons and impulse as a reliable message separately, because it's not re-sent but reliable is slow for everything else

	InputResetTriggersAndImpulse()
	
	clgs.sbar_showscores = clgis.sbar_showscores
end

-- ===================
-- MenuInit
-- ===================
function MenuInit()
	host_frametime = 0
	host_realtime = 0
	cl_ingame = false
	cl_connected = false
	paused = false
	sv_loading = false
	sv_listening = false

	CL_DialogInit()

	splashscreen = LoadTexture("menu/splashscreen", true, false, 1, 1)

	m_mainback = LoadTexture("menu/mainback", true, false, 1, 1)

	m_clicksnd = LoadSound("menu/menu1", true)

	m_loading_server = false
	m_loading_server_timeout = 0
	m_loading_client = false
	m_loading_client_timeout = 0

	Sbar_Init()
end

-- ===================
-- MenuShutdown
-- ===================
function MenuShutdown()
	Sbar_Shutdown()
	CL_DialogShutdown()
end

-- ===================
-- MenuDraw
-- ===================
function MenuDraw()
	Sbar_Draw()
	if paused then
		MenuInfoBox("Paused", 0.5)
	end
	CL_DialogDraw()
end

-- ===================
-- MenuDrawPriority
-- ===================
function MenuDrawPriority()
	if m_loading_server or m_loading_client then -- TODO: is this drawing loading plaque, even if menu is CLOSED? (change m_loading to clgs.loading?)
		if m_loading_server then
			MenuInfoBox("Loading server...", 0.5)
		else
			if clgs.signon == 0 then
				local signs = {[0] = "|", [1] = "/", [2] = "-", [3] = "\\"}
				local msg = "  Connecting... " .. signs[math.floor((host_realtime / 500) % 4)] -- TODO: host_realtime not update if we are updating mid-frame
				MenuInfoBox(msg, 0.5)
			else
				MenuInfoBox("Loading client...", 0.5)
			end
		end
	end
end

-- ===================
-- MenuKey
-- ===================
function MenuKey(keyindex, down, analog_rel, analog_abs)
	CL_DialogKey(keyindex, down, analog_rel, analog_abs)
end

-- ===================
-- MenuFrame
-- ===================
function MenuFrame(new_host_frametime, new_host_realtime, new_ingame, new_connected, new_paused, new_loading, new_listening)
	host_frametime = new_host_frametime
	host_realtime = new_host_realtime
	cl_ingame = new_ingame
	cl_connected = new_connected
	paused = new_paused
	sv_loading = new_loading
	sv_listening = new_listening

	Sbar_Frame()
	CL_DialogFrame()
end

-- ===================
-- MenuClose
-- ===================
function MenuClose()
	CL_DialogClear()
end

-- ===================
-- MenuOpen
-- ===================
function MenuOpen(show_splash)
	if show_splash then
		Menu_Splash()
	else
		Menu_Main()
	end
end

-- ===================
-- MenuIsLoadingServer
-- ===================
function MenuIsLoadingServer()
	return m_loading_server
end

-- ===================
-- MenuLoadingServerGetTimeout
-- ===================
function MenuLoadingServerGetTimeout()
	return m_loading_server_timeout
end

-- ===================
-- MenuFinishLoadingServer
-- ===================
function MenuFinishLoadingServer(error_happened)
	m_loading_server = false
	-- give more time to the client
	if not error_happened and m_loading_client then
		m_loading_client_timeout = host_realtime + LOADING_TIMEOUT
	end
end

-- ===================
-- MenuIsLoadingClient
-- ===================
function MenuIsLoadingClient()
	return m_loading_client
end

-- ===================
-- MenuLoadingClientGetTimeout
-- ===================
function MenuLoadingClientGetTimeout()
	return m_loading_client_timeout
end

-- ===================
-- MenuFinishLoadingClient
-- ===================
function MenuFinishLoadingClient()
	m_loading_client = false
end
