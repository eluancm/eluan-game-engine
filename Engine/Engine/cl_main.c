/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/* TODO: demo saving/playing */

#define CLIENT_MAX_CONNECTION_RETRIES		3

/*
============================================================================

Client commands

Added after CL_Init(), so no need to check cls.active

============================================================================
*/

cvar_t *cl_name = NULL;

/*
===================
CL_CMD_Name

Sets the client name
===================
*/
void CL_CMD_Name(void)
{
	if (cls.connected && !cls.ingame)
	{
		Sys_Printf("\"name\": client still connecting, can't change name\n");
		return;
	}

	if (host_cmd_argc == 1)
	{
		Sys_Printf("\"name\" is %s\n", cl_name->charvalue);
	}
	else if (host_cmd_argc == 2)
	{
		if (strlen(host_cmd_argv[1]) >= MAX_CLIENT_NAME) /* >= because of zero termination */
		{
			Sys_Printf("\"name\": too big, maximum %d characters.\n", MAX_CLIENT_NAME);
			return;
		}

		Host_CMDForceCvarSet(cl_name, host_cmd_argv[1], true);
		if (cls.ingame)
		{
			char msg[MAX_NET_CMDSIZE];
			int len = 0;

			MSG_WriteByte(msg, &len, CLC_NAME);
			MSG_WriteString(msg, &len, cl_name->charvalue);
			Host_NetchanQueueCommand(cls.netconn, msg, len, NET_CMD_RELIABLE);
		}
	}
	else
	{
		Sys_Printf("\"name\": too many arguments.\n");
	}
}

/*
===================
CL_CMD_Connect

Sets up client parameters then calls CL_Connect
===================
*/
void CL_Connect(int reconnect);
void CL_CMD_Connect(void)
{
	/* if not connecting to an active local server, shut it down */
	if (svs.listening && !Sys_NetCLServerIsLocal()) /* TODO: TEST THIS */
	{
		/* TODO FIXME: CL calling SV, need a way to execute a command while executing another, or just break this execution, put the disconnect command in front of it in the queue then restart executing */
		Sys_Printf("Not connecting to the locally started server, shutting it down...\n");
		SV_ShutdownServer(false);
	}
	CL_Connect(false);
}

/*
===================
CL_CMD_Reconnect

Sets up client parameters then calls CL_Connect, but without
resetting the connection, just going back to signon stage.
===================
*/
void CL_CMD_Reconnect(void)
{
	if (!cls.connected)
	{
		Sys_Printf("Can't reconnect, not connected.\n");
		return;
	}
	CL_Connect(true);
}

/*
===================
CL_CMD_Pause

Sends a pause request to the server - it does not makes sense on the client
===================
*/
void CL_CMD_Pause(void)
{
	char msg[MAX_NET_CMDSIZE];
	int len = 0;

	if (!cls.connected)
	{
		Sys_Printf("Can't pause, not connected.\n");
		return;
	}

	MSG_WriteByte(msg, &len, CLC_PAUSE);
	Host_NetchanQueueCommand(cls.netconn, msg, len, NET_CMD_RELIABLE);
}

/*
============================================================================

Main client routines

============================================================================
*/

cl_state_t	cls;
static cvar_t *cl_exclusiveinputingame = NULL;
cvar_t *cl_fullpredict = NULL;

/*
===================
CL_CheckLoadingSlowness

Perform extra updates to show that we are not frozen, just loading lots of data.
TODO: lots of places in the rendering loop we should use Sys_Time() instead of host_realtime because host_realtime is only updated once per full frame (BUT this may cause "rollbacks" in some types of effects - use with caution)
===================
*/
void CL_CheckLoadingSlowness(void)
{
	static mstime_t last_update = 0;
	mstime_t current = Sys_Time();

	if (!last_update)
		last_update = host_realtime;

	if (current > last_update + 100) /* update each 1/10 of a second */
	{
		CL_VideoFrame();
		/* TODO: is sound already multithreaded? */
		last_update = current;
	}
}

/*
===================
CL_CleanState
===================
*/
void CL_CleanState(int reconnect)
{
	if (!reconnect)
		cls.connected = false;
	cls.signon = 0;
	cls.ingame = false;
	cls.time = 0;
	Host_NetchanClean(cls.netconn, reconnect);
	cls.netsendtime = 0;
	cls.conn_tries = 0;
	if (!reconnect)
		cls.client_id = 0;
	cls.game_id = 0;
	cls.remote_host[0] = 0;
	cls.stall_frames = 0;

	memset(&cls.snapshots, 0, sizeof(cls.snapshots));
	memset(&cls.prediction_snapshot, 0, sizeof(cls.prediction_snapshot));
	cls.current_snapshot_idx = 0;
	cls.current_snapshot_valid = false;
	cls.current_acked_input = 0;
	memset(&cls.receiving_snapshot_bitfields, 0, sizeof(cls.receiving_snapshot_bitfields));
	cls.receiving_snapshot_crc32 = 0;
	cls.receiving_acked_input = 0;
	cls.precached_models_num = 0;
	cls.precached_sounds_num = 0;

	cls.physworld = NULL;
}

/*
===================
CL_Init
===================
*/
void CL_Init(void)
{
	Sys_NetCLInit();

	memset(&cls, 0, sizeof(cls));

	CL_VideoInit();
	CL_ParticlesInit();
	CL_SoundsInit();
	CL_InputInit();
	CL_MenuInit();

	cls.netconn = Host_NetchanCreate();
	CL_CleanState(false); /* some initial values may not be zero */

	cls.active = true;

	/* these cvars should be changed by proxy cmds, to prevent their change while a client is connected or because of having to send new data to the server */
	cl_name = Host_CMDAddCvar("_cl_name", "Player", CVAR_ARCHIVE | CVAR_READONLY);

	Host_CMDAdd("name", CL_CMD_Name);

	Host_CMDAdd("connect", CL_CMD_Connect);
	Host_CMDAdd("reconnect", CL_CMD_Reconnect);
	Host_CMDAdd("pause", CL_CMD_Pause);

	cl_exclusiveinputingame = Host_CMDAddCvar("cl_exclusiveinputingame", "1", 0);
	cl_fullpredict = Host_CMDAddCvar("cl_fullpredict", "1", CVAR_ARCHIVE);
}

/*
===================
CL_Shutdown
===================
*/
void CL_Shutdown(void)
{
	cls.active = false;

	CL_MenuShutdown();
	CL_InputShutdown();
	CL_SoundsShutdown();
	CL_ParticlesShutdown();
	CL_VideoShutdown();

	Sys_NetCLShutdown();
}

/*
===================
CL_Connect

Connects to an already running server, should only be called from the proxy command "connect"
===================
*/
void CL_Connect(int reconnect)
{
	char msg[MAX_NET_CMDSIZE];
	int len = 0;

	if (!cls.active)
	{
		Sys_Printf("Client: Can't connect to a server if the client subsystem isn't initialized\n");
		return;
	}

	CL_Disconnect(reconnect, reconnect ? false : true);

	/* clean up */
	CL_VideoDataClean();
	CL_CleanParticles();
	CL_CleanSounds(false);

	/* okay, let's start */
	if (!reconnect) /* only recreate the socket if we are connecting to another server */
	{
		if (!Sys_NetCLStart())
		{
			Sys_Printf("Client: Can't create netconn\n");
			return;
		}
	}

	if (!Sys_NetCLServerIsLocal())
		cls.physworld = Sys_CreatePhysicsWorld(false);
	Host_VoxelCreate(false);
	Game_CL_Newgame();

	cls.netconn->last_received_time = host_realtime;
	cls.connected = true; /* stateless */

	/* TODO CONSOLEDEBUG Sys_Printf("Client connecting...\n"); */

	/* send handshake signal */
	MSG_WriteShort(msg, &len, NET_PROTOCOL);
	MSG_WriteShort(msg, &len, NET_MAGIC);
	Host_NetchanQueueCommand(cls.netconn, msg, len, NET_CMD_RELIABLE);
}

/*
===================
CL_Disconnect

Disconnects to the server we are currently connected to
TODO: show message on the menu system when a error is the cause
===================
*/
void CL_Disconnect(int reconnect, int warn)
{
	char sendmsg[MAX_NET_CMDSIZE];
	int sendlen;
	int was_ingame = cls.ingame;

	if (!cls.active)
		return;
	if (!cls.connected)
		return;

	Sys_StopAllSounds(false);

	if (warn)
	{
		Host_NetchanCleanQueue(cls.netconn);
		sendlen = 0;
		MSG_WriteByte(sendmsg, &sendlen, CLC_DISCONNECT);
		Host_NetchanQueueCommand(cls.netconn, sendmsg, sendlen, NET_CMD_UNRELIABLE); /* need to send as unreliable because of reliable acks and other stuff */
		Host_NetchanDispatchCommands(cls.netconn, false, cls.client_id, cls.game_id); /* send immediately and don't care if it reaches the server or not, it will timeout anyway TODO: see if this makes the final messages of why he was disconnected reach the server */
	}

	if (cls.physworld)
		Sys_DestroyPhysicsWorld(cls.physworld);
	if (!reconnect) /* keep everything if we are reconnecting */
		Sys_NetCLStop();

	CL_CleanState(reconnect);

	if (was_ingame && !reconnect)
		Sys_Printf("Client disconnected\n");
}

/*
===================
CL_ParseServerMessages

Called every frame to get state from the server
===================
*/
void CL_ParseServerMessages(char *msg, int len)
{
	int read = 0;
	unsigned char cmd;

	/* TODO: when server makes things static, add them to the baseline!!! */
	/* TODO: interpolate */
	/* TODO: lots of place where linked lists would improve performance... */
	/* TODO: when a packet causes a return, read all messages in queue before returning */

	while (read != len)
	{
		MSG_ReadByte(msg, &read, len, &cmd);

		switch (cmd)
		{
			case SVC_BEGIN: /* new differential snapshot */
			case SVC_ENTITY:
			case SVC_END: /* end of new differential snapshot */
			case SVC_SNAPSHOTRESET:
				CL_SnapshotParseData(cmd, msg, &read, len);
				break;
			case SVC_SERVERQUIT: /* server quits */
				Sys_Printf("Server quit.\n");
				CL_Disconnect(false, false);
				Host_CMDBufferAdd("openconsole");
				return; /* ignore other messages */
			case SVC_ERROR: /* generic network error */
				Sys_Printf("Protocol, network or another error.\n");
				CL_Disconnect(false, false);
				Host_CMDBufferAdd("openconsole");
				return; /* ignore other messages */
			case SVC_RECONNECT: /* server tells us to reconnect */
				Sys_Printf("Server requested a reconnect.\n");
				CL_Disconnect(true, false);
				Host_CMDBufferAdd("reconnect"); /* execute only when the frame ends TODO: redundant disconnect above, but necessary to not use data a local server will have freed */
				return; /* we will go to signon parsing again */
			case SVC_VOXELBLOCK:
				CL_ParseVoxelBlock(msg, &read, len);
				break;
			case SVC_VOXELCHUNKPART:
				CL_ParseVoxelChunkPart(msg, &read, len);
				break;
			case SVC_PARTICLE:
				CL_StartParticle(msg, &read, len);
				break;
			case SVC_SOUND:
				{
					precacheindex_t snd;
					entindex_t ent;
					vec3_t origin, vel;
					unsigned char channel;
					vec_t pitch, gain, attenuation;
					unsigned char loop;

					MSG_ReadPrecache(msg, &read, len, &snd);
					MSG_ReadEntity(msg, &read, len, &ent);
					MSG_ReadVec3(msg, &read, len, origin);
					MSG_ReadVec3(msg, &read, len, vel);
					MSG_ReadByte(msg, &read, len, &channel);
					MSG_ReadVec1(msg, &read, len, &pitch);
					MSG_ReadVec1(msg, &read, len, &gain);
					MSG_ReadVec1(msg, &read, len, &attenuation);
					MSG_ReadByte(msg, &read, len, &loop);
					CL_StartSound(&server_sounds[snd], ent, origin, vel, channel, pitch, gain, attenuation, loop);
				}
				break;
			case SVC_STOPSOUND:
				{
					entindex_t ent;
					unsigned char channel;

					MSG_ReadEntity(msg, &read, len, &ent);
					MSG_ReadByte(msg, &read, len, &channel);
					CL_StopSound(ent, channel);
				}
				break;
			case SVC_NOPAUSE:
				Sys_Printf("Server is not pausable\n");
				break;
			default:
				if (!Game_CL_ParseServerMessages(cmd, msg, &read, len))
				{
					Sys_Printf("Unknown SVC (%02X) received, disconnecting...\n", cmd);
					Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
					Host_CMDBufferAdd("openconsole");
					return; /* stop trying to read */
				}
				else
					break;
		}
	}
}

/*
===================
CL_ParseServerSignonMessages

Connection handshaking
TODO: visual feedback when loading resources?
===================
*/
void CL_ParseServerSignonMessages(char *msg, int len)
{
	static precacheindex_t loaded = 0; /* for models AND sounds */
	char loadprecache[MAX_NET_CMDSIZE]; /* for models AND sounds */
	char sendmsg[MAX_NET_CMDSIZE];
	int sendlen;
	int read = 0;
	short cmd;
	mstime_t lastkeepalivesent; /* if the function returns, the netchannel will be re-set, but it won't if we receive lots of big models to load during this single function call and we need to keep track of time for keepalives */
	mstime_t currentrealtime;

	lastkeepalivesent = cls.netconn->last_received_time;
	/* TODO: wait for the server on changelevels by making the server send keepalives if loading is taking too much time, see if keepalives work */
	while (read != len)
	{
		if (cls.signon == 0)
		{
			MSG_ReadShort(msg, &read, len, &cmd);
			if (cmd == 0)
			{
				Sys_Printf("SGNC: Connection refused: protocol version mismatch.\n");
				Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
				Host_CMDBufferAdd("openconsole");
				return;
			}
			else if (cmd == 1)
			{
				Sys_Printf("SGNC: Connection refused: server is full.\n");
				Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
				Host_CMDBufferAdd("openconsole");
				return;
			}
			else if (cmd == 2)
			{
				client_id_t new_id;

				MSG_ReadInt(msg, &read, len, &new_id);
				if (cls.client_id != 0 && new_id != cls.client_id)
				{
					Sys_Printf("SGNC: Connection error: reconnecting but received wrong client id, %-05X should be %-05X\n", new_id, cls.client_id);
					Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
					Host_CMDBufferAdd("openconsole");
					return;
				}
				cls.client_id = new_id;
				MSG_ReadInt(msg, &read, len, &cls.game_id);
				MSG_ReadString(msg, &read, len, cls.remote_host, sizeof(cls.remote_host));
				Sys_Printf("SGNC: Connection accepted. Remote host: %s\n", cls.remote_host);
				MSG_ReadPrecache(msg, &read, len, &cmd);
				cls.signon = 1;
				loaded = 0;
				cls.precached_models_num = cmd;
				/* TODO CONSOLEDEBUG Sys_Printf("SGNC: Client precaching %d models\n", cmd); */

				CL_VideoFrame(); /* force updating to guarantee the loading messages */
			}
			else
			{
				Sys_Printf("SGNC: Connection refused: unknown error, cmd = %04X\n", cmd);
				Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
				Host_CMDBufferAdd("openconsole");
				return;
			}
		}
		else if (cls.signon == 1)
		{
			/* already loaded all last time? */
			if (loaded == cls.precached_models_num)
			{
				/* TODO CONSOLEDEBUG Sys_Printf("SGNC: Client finished precaching all models!\n"); */
				MSG_ReadPrecache(msg, &read, len, &cmd);
				cls.signon = 2;
				loaded = 0;
				cls.precached_sounds_num = cmd;
				/* TODO CONSOLEDEBUG Sys_Printf("SGNC: Client precaching %d sounds\n", cmd); */
			}
			else
			{
				MSG_ReadString(msg, &read, len, loadprecache, sizeof(loadprecache));
				/* TODO CONSOLEDEBUG Sys_Printf("SGNC: Client precaching model \"%s\"...\n", loadprecache); */
				cls.precached_models[loaded] = Host_LoadModel(loadprecache, false);
				CL_CheckLoadingSlowness(); /* force updating to guarantee the loading messages */
				/* load stuff unneeded for the server separately */
				Sys_LoadModelClientData(cls.precached_models[loaded]->data);
				loaded++;
				/* to acknowledge our progress right now if half of the timeout has passed */
				currentrealtime = Sys_Time();
				if (currentrealtime > lastkeepalivesent + NET_MSTIMEOUT / 2)
				{
					if (Host_NetchanDispatchCommands(cls.netconn, false, cls.client_id, cls.game_id) == NET_ERR)
					{
						Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
						Host_CMDBufferAdd("openconsole");
					}
					lastkeepalivesent = currentrealtime;
				}
				CL_CheckLoadingSlowness(); /* force updating to guarantee the loading messages */
			}
		}
		else if (cls.signon == 2)
		{
			/* just finished loading them */
			if (loaded == cls.precached_sounds_num)
			{
				/* TODO CONSOLEDEBUG Sys_Printf("SGNC: Client finished precaching all sounds!\n"); */
				cls.signon = 3;
			}
			else
			{
				MSG_ReadString(msg, &read, len, loadprecache, sizeof(loadprecache));
				/* TODO CONSOLEDEBUG Sys_Printf("SGNC: Client precaching sound \"%s\"...\n", loadprecache); */
				cls.precached_sounds[loaded++] = CL_LoadSound(loadprecache, false);
				/* to acknowledge our progress right now if half of the timeout has passed */
				currentrealtime = Sys_Time();
				if (currentrealtime > lastkeepalivesent + NET_MSTIMEOUT / 2)
				{
					if (Host_NetchanDispatchCommands(cls.netconn, false, cls.client_id, cls.game_id) == NET_ERR)
					{
						Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
						Host_CMDBufferAdd("openconsole");
					}
					lastkeepalivesent = currentrealtime;
				}
				CL_CheckLoadingSlowness(); /* force updating to guarantee the loading messages */
			}
		}
		else if (cls.signon == 3)
		{
			MSG_ReadShort(msg, &read, len, &cmd);

			if (cmd == NET_MAGIC + 1)
			{
				sendlen = 0;
				MSG_WriteShort(sendmsg, &sendlen, NET_MAGIC + 2);
				MSG_WriteString(sendmsg, &sendlen, cl_name->charvalue);
				Host_NetchanQueueCommand(cls.netconn, sendmsg, sendlen, NET_CMD_RELIABLE);

				cls.ingame = true;
				cls.signon = 4;
				/* TODO CONSOLEDEBUG Sys_Printf("SGNC: Client connected.\n"); */
			}
			else
			{
				/* TODO: read baseline! */
			}
		}
	}
}

/*
===================
CL_SendServerMessages

Called every frame to send commands to the server
===================
*/
void CL_SendServerMessages(void)
{
	char msg[MAX_NET_CMDSIZE];
	int len = 0;

	MSG_WriteByte(msg, &len, CLC_SNAPSHOTACK);
	MSG_WriteShort(msg, &len, cls.snapshots[cls.current_snapshot_idx].id);
	Host_NetchanQueueCommand(cls.netconn, msg, len, NET_CMD_UNRELIABLE);

	/* send anything game-specific */
	Game_CL_SendServerMessages();
}

/*
===================
CL_ParseMessages

Parses the server messages
===================
*/
void CL_ParseMessages(void)
{
	char msg[MAX_NETCHAN_DATASIZE];
	int len, reliable;
	seqnum_t inseq, myseq;
	client_id_t to_client_id;
	client_id_t game_id;

	/* TODO: is it possible to keep receiving infinite messages with a fast enough connection and never leave this loop? */
	while (Host_NetchanReceiveCommands(msg, &len, NULL, NULL, &reliable, &inseq, &myseq, false, &to_client_id, &game_id))
	{
		if (to_client_id != cls.client_id && cls.signon != 0)
			continue; /* after first signon stage, must use the correct client_id */

		if (game_id != cls.game_id && cls.signon != 0) /* to make sure that no leftover message from the last game will get in */
			continue; /* after first signon stage, messages must match the game_id */

		if (!reliable && !cls.ingame) /* do not accept unreliables until fully connected */
		{
			Host_NetchanUpdateNC(cls.netconn, reliable, inseq, myseq, host_realtime); /* update acks and timeout anyway */
			continue;
		}
		if (reliable && (inseq != (seqnum_t)(cls.netconn->inseq + 1))) /* ignore duplicated and out of order reliables */
		{
			Host_NetchanUpdateNC(cls.netconn, reliable, inseq, myseq, host_realtime); /* update acks and timeout anyway */
			continue;
		}

		if (cls.ingame)
		{
			CL_ParseServerMessages(msg, len);
		}
		else
		{
			/* deal with the connection messages here */
			CL_ParseServerSignonMessages(msg, len);
		}

		if (!cls.connected) /* we disconnected after receiving a message */
			break;

		Host_NetchanUpdateNC(cls.netconn, reliable, inseq, myseq, host_realtime);
	}
}

/*
===================
CL_Frame
===================
*/
void CL_Frame(void)
{
	mstime_t start_time, start_time2, end_time, end_time2;
	int send_data_to_server_now = false;
	if (!cls.active)
		return;

	if (host_speeds->doublevalue)
		start_time = Sys_Time();

	/* don't let anything get in the way of gameplay but let the system get a chance when we are in menus, etc FIXME: move this to cl_input.c or sys_***.c? */
	if (keydest == KEYDEST_GAME)
		Sys_ExclusiveInput(cl_exclusiveinputingame->doublevalue ? true : false);
	else
		Sys_ExclusiveInput(false);

	if (host_speeds->doublevalue)
		start_time2 = Sys_Time();
	/* these are in different loops because we may disconnect after receiving certain messages */
	if (cls.connected)
	{
		cls.stall_frames++;
		if (cls.stall_frames > 10) /* just to give an upper bound... */
			cls.stall_frames = 10;
		CL_ParseMessages();
	}
	if (host_speeds->doublevalue)
	{
		end_time2 = Sys_Time();
		Sys_Printf("client recv: % 3f\n", end_time2 - start_time2);
		start_time2 = Sys_Time();
	}
	if (cls.connected && !cls.snapshots[cls.current_snapshot_idx].paused)
		cls.time += host_frametime;
	if (cls.connected)
	{
		/* TODO: sync entities here if doing client side prediction, but only if the server is not local */
		/* make a snapshot of the input state for prediction and reconciliation */
		/* TODO: since input are regular console commands, what happens if we execute the buffer in the wrong times, getting the stored commands out of sync? */
		Game_CL_InputSaveState();

		if (cls.ingame) /* TODO: option to have a different viewent but still hear the world from the ears of the cameraent in the player head - this may have problems with visibility? */
			Sys_SoundUpdateListener(cls.prediction_snapshot.edicts[cls.prediction_snapshot.viewent].origin, cls.prediction_snapshot.edicts[cls.prediction_snapshot.viewent].velocity, cls.prediction_snapshot.edicts[cls.prediction_snapshot.viewent].angles);
		if (!cls.snapshots[cls.current_snapshot_idx].paused)
			CL_UpdateParticles();
		Host_VoxelCommitUpdates();
		Game_CL_Frame();
		if (!cls.snapshots[cls.current_snapshot_idx].paused)
			CL_PredFrame();

		if (host_speeds->doublevalue)
		{
			end_time2 = Sys_Time();
			Sys_Printf("client game+phys: % 3f\n", end_time2 - start_time2);
			start_time2 = Sys_Time();
		}

		if (cls.ingame)
			CL_SendServerMessages();

		if (host_speeds->doublevalue)
		{
			end_time2 = Sys_Time();
			Sys_Printf("client send: % 3f\n", end_time2 - start_time2);
			start_time2 = Sys_Time();
		}

		if (cls.signon == 0) /* still trying to connect */
		{
			int try_now = false;
			/* do not overflow the server with connection requests because it may create a new connection with a new client_id for each try */
			if (!cls.conn_tries)
				try_now = true; /* first try */
			if (cls.conn_tries <= CLIENT_MAX_CONNECTION_RETRIES && (cls.netsendtime + NET_MSTIMEOUT) <= host_realtime)
				try_now = true; /* more tries after waiting for the timeout counter */

			if (try_now)
			{
				cls.conn_tries++;
				if (Host_NetchanDispatchCommands(cls.netconn, false, cls.client_id, cls.game_id) == NET_ERR)
				{
					Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
					Host_CMDBufferAdd("openconsole");
				}
				if (cls.conn_tries == 1)
					Sys_Printf("Trying...\n");
				else
					Sys_Printf("Still trying...\n");

				cls.netsendtime = host_realtime;
			}
			else if (cls.conn_tries >= CLIENT_MAX_CONNECTION_RETRIES)
			{
				Sys_Printf("No response.\n");
				Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
				Host_CMDBufferAdd("openconsole");
			}
		}
		else
		{
			if (Host_NetchanDispatchCommands(cls.netconn, false, cls.client_id, cls.game_id) == NET_ERR)
			{
				Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
				Host_CMDBufferAdd("openconsole");
			}
			else if (cls.netconn->last_received_time + NET_MSTIMEOUT <= host_realtime)
			{
				Sys_Printf("Client timed out waiting for server\n");
				Host_CMDBufferAdd("disconnect"); /* execute only when the frame ends */
				Host_CMDBufferAdd("openconsole");
			}
		}
	}

	CL_MenuFrame();

	if (host_speeds->doublevalue)
	{
		end_time2 = Sys_Time();
		Sys_Printf("client menu: % 3f\n", end_time2 - start_time2);
		start_time2 = Sys_Time();
	}

	CL_VideoFrame();

	if (host_speeds->doublevalue)
	{
		end_time2 = Sys_Time();
		Sys_Printf("client vid : % 3f\n", end_time2 - start_time2);
	}

	CL_InputPostFrame();

	if (host_speeds->doublevalue)
	{
		end_time = Sys_Time();
		Sys_Printf("client totl: % 3f\n", end_time - start_time);
	}
}
