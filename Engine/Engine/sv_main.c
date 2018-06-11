/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Server commands

Added after SV_Init(), so no need to check svs.active

TODO: connecting slots != connected slots!! Slots separados pra evitar DoS

============================================================================
*/

cvar_t *sv_hostname = NULL;
cvar_t *sv_maxplayers = NULL;
cvar_t *sv_pausable = NULL;

/*
===================
SV_CMD_Hostname

Sets the server host name
===================
*/
void SV_CMD_Hostname(void)
{
	if (svs.listening || svs.loading)
	{
		Sys_Printf("\"sv_hostname\": server running, can't change hostname\n");
		return;
	}

	if (host_cmd_argc == 1)
	{
		Sys_Printf("\"sv_hostname\" is %s\n", sv_hostname->charvalue);
	}
	else if (host_cmd_argc == 2)
	{
		if (strlen(host_cmd_argv[1]) >= MAX_HOST_NAME) /* >= because of zero termination */
		{
			Sys_Printf("\"sv_hostname\": too big, maximum %d characters.\n", MAX_HOST_NAME);
			return;
		}

		Host_CMDForceCvarSet(sv_hostname, host_cmd_argv[1], true);
		Sys_Printf("\"sv_hostname\": is now %s\n", sv_hostname->charvalue);
	}
	else
	{
		Sys_Printf("\"sv_hostname\": too many arguments.\n");
	}
}

/*
===================
SV_CMD_MaxPlayers

Sets the maximum number of clients allowed to connect to the current server
===================
*/
void SV_CMD_MaxPlayers(void)
{
	int newvalue;

	if (svs.listening || svs.loading)
	{
		Sys_Printf("\"sv_maxplayers\": server running, can't change maxplayers\n");
		return;
	}

	if (host_cmd_argc == 1)
	{
		Sys_Printf("\"sv_maxplayers\" is %d\n", (int)sv_maxplayers->doublevalue);
	}
	else if (host_cmd_argc == 2)
	{
		newvalue = atoi(host_cmd_argv[1]);
		if (newvalue < 1 || newvalue > MAX_CLIENTS)
		{
			Sys_Printf("\"sv_maxplayers\": %d is out of range.\n", newvalue);
			return;
		}

		Host_CMDForceCvarSet(sv_maxplayers, host_cmd_argv[1], true);
		Sys_Printf("\"sv_maxplayers\": is now %d\n", (int)sv_maxplayers->doublevalue);
	}
	else
	{
		Sys_Printf("\"sv_maxplayers\": too many arguments.\n");
	}
}

/*
===================
SV_CMD_StartServer

Sets up server parameters then calls SV_StartServer
===================
*/
void SV_StartServer(int changelevel);
void SV_CMD_StartServer(void)
{
	if (host_cmd_argc < 2)
	{
		Sys_Printf("No map specified.\n");
		return;
	}
	if (host_cmd_argc > 2)
	{
		Sys_Printf("Too many arguments.\n");
		return;
	}

	Sys_Printf("Starting server with map \"%s\"...\n", host_cmd_argv[1]);
	Sys_Snprintf(svs.name, MAX_MODEL_NAME, "%s", host_cmd_argv[1]);
	SV_StartServer(false);
}

/*
===================
SV_CMD_ChangeLevel

Sets up server parameters then calls SV_StartServer without dropping clients,
but merely reseting them to signon stage.

TODO: spawnparms system for carrying client info between server instances, identifying by id1 and id2 (or client_id)
===================
*/
void SV_CMD_ChangeLevel(void)
{
	if (host_cmd_argc < 2)
	{
		Sys_Printf("No map specified.\n");
		return;
	}
	if (host_cmd_argc > 2)
	{
		Sys_Printf("Too many arguments.\n");
		return;
	}

	if (!svs.listening)
	{
		Sys_Printf("Can't changelevel, not listening.\n");
		return;
	}

	Sys_Printf("Changing server to map \"%s\"...\n", host_cmd_argv[1]);
	Sys_Snprintf(svs.name, MAX_MODEL_NAME, "%s", host_cmd_argv[1]);
	SV_StartServer(true);

	/* TODO: this is a cmd, but it still is a SV calling a CL? Is this the right way to do it? sometimes the reconnect packet doesn't arrive in time and, well, NULL pointers for unloaded models... */
	if (cls.connected)
	{
		CL_Disconnect(true, false); /* TODO: make sure that SVC_RECONNECT is not sent to the local client (probably already assured because of sending the game id with the SVC_RECONNECT packet) */
		Host_CMDBufferAdd("reconnect"); /* execute only when the frame ends TODO: redundant disconnect above, but necessary to not use data a local server will have freed */
	}

	/* TODO: if the local client doesn't receive the reconnect packet, it may be stuck in a "connected" state with new models and old entities, this is bad! */
}

/*
===================
SV_CMD_Ping

Displays the connection quality of clients
===================
*/
void SV_CMD_Ping(void)
{
	int i;

	if (host_cmd_argc > 1)
	{
		Sys_Printf("Too many arguments.\n");
		return;
	}

	if (!svs.listening)
	{
		Sys_Printf("Server not started.\n");
		return;
	}

	Sys_Printf("Client ping times and packet loss: (game %-08X)\n", svs.game_id);
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (svs.sv_clients[i].ingame) /* TODO FIXME: keep this table aligned */
			Sys_Printf("%2d %16s: ID %-05X PL %-3d Ping %-5ums\n", i, Game_SV_ClientGetNetname(i), svs.sv_clients[i].client_id, svs.sv_clients[i].packet_loss, svs.sv_clients[i].ping);
	}
}

/*
===================
SV_CMD_Pause

Pauses the server
===================
*/
void SV_CMD_Pause(void)
{
	if (host_cmd_argc > 1)
	{
		Sys_Printf("Too many arguments.\n");
		return;
	}

	if (!svs.listening)
	{
		Sys_Printf("Server not started.\n");
		return;
	}

	/* on the server console, ignore sv_pausable */
	if (svs.paused)
		svs.paused = false;
	else
		svs.paused = true;
}

/*
============================================================================

Main server routines

============================================================================
*/

int			servermarker; /* for clearing server memory after a game */
sv_state_t	svs;

/*
===================
SV_GenerateClientId

Creates a new unique client id
===================
*/
client_id_t SV_GenerateClientId(void)
{
	client_id_t new_id;

	while (1)
	{
		int repeat = false;
		int i;

		new_id = (client_id_t)floor(Sys_Random(1, 65535));

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (svs.sv_clients[i].client_id == new_id)
			{
				repeat = true;
				break;
			}
		}

		if (!repeat)
			return new_id;
	}
}

/*
===================
SV_CheckFinishedLoading

Sees if the server can resume operations after loading a saved game.
===================
*/
void SV_CheckFinishedLoading(void)
{
	int i, count;

	/* TODO FIXME: have multiplayer saves */
	for (i = 0, count = 0; i < MAX_CLIENTS; i++)
		if (svs.sv_clients[i].ingame)
			count++;

	if (count == 1)
		svs.loading_saved_game = false; /* continue the game */
}

/*
===================
SV_CleanClientSlot
===================
*/
void SV_CleanClientSlot(int slot, int reconnect)
{
	if (!reconnect)
		svs.sv_clients[slot].connected = false;
	svs.sv_clients[slot].signon = 0;
	svs.sv_clients[slot].ingame = false;
	Host_NetchanClean(svs.sv_clients[slot].netconn, reconnect);
	if (!reconnect)
		svs.sv_clients[slot].client_id = 0;

	memset(svs.sv_clients[slot].snapshots, 0, sizeof(svs.sv_clients[slot].snapshots));
	memset(svs.sv_clients[slot].snapshots_modified_bitfields, 0, sizeof(svs.sv_clients[slot].snapshots_modified_bitfields));
	memset(svs.sv_clients[slot].snapshots_times, 0, sizeof(svs.sv_clients[slot].snapshots_times));
	memset(svs.sv_clients[slot].snapshots_crc32, 0, sizeof(svs.sv_clients[slot].snapshots_crc32));
	svs.sv_clients[slot].last_created_snapshot = 0;
	svs.sv_clients[slot].last_acknowledged_snapshot = 0;
	svs.sv_clients[slot].snapshot_counter = 0;

	svs.sv_clients[slot].ping = 0;
	svs.sv_clients[slot].packet_loss = 0;
}

/*
===================
SV_Init
===================
*/
void SV_Init(void)
{
	int i;

	Sys_NetSVInit();

	memset(&svs, 0, sizeof(svs));

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		svs.sv_clients[i].netconn = Host_NetchanCreate();
		SV_CleanClientSlot(i, false); /* even if some thing were already cleared up by the memset above, we need defaults! */
	}

	sv_pausable = Host_CMDAddCvar("sv_pausable", "1", 0);

	/* these cvars should be changed by proxy cmds, to prevent their change while a server is started or because of having to broadcast new data TODO: by checking only when a client connects and having the structs always allocated and checked, we may be able to allow mid-game changes to this (without kicking people off if we have more players than the new limit, but be aware of what happens on changelevel) */
	sv_hostname = Host_CMDAddCvar("_sv_hostname", "noname", CVAR_ARCHIVE | CVAR_READONLY);
	sv_maxplayers = Host_CMDAddCvar("_sv_maxplayers", "1", CVAR_READONLY);

	Host_CMDAdd("sv_hostname", SV_CMD_Hostname);
	Host_CMDAdd("sv_maxplayers", SV_CMD_MaxPlayers);

	Host_CMDAdd("startserver", SV_CMD_StartServer);
	Host_CMDAdd("changelevel", SV_CMD_ChangeLevel);
	Host_CMDAdd("ping", SV_CMD_Ping);
	Host_CMDAdd("sv_pause", SV_CMD_Pause);

	Sys_InitPhysics();
	Game_SV_InitGame();
	/* store our low-memory pointer, from here, everything in this memory pool shall be game-specific */
	servermarker = Sys_MemLowMark(&svr_mem);
	svs.active = true;
}

/*
===================
SV_Shutdown
===================
*/
void SV_Shutdown(void)
{
	svs.active = false;

	Game_SV_ShutdownGame();
	Sys_ShutdownPhysics();

	Sys_NetSVShutdown();
}

/*
===================
SV_DropClient

Drops any given client.
reconnect = keep the slot populated but reset connection state
warn = if we tell the client we are dropping
svc_reason = the SVC to send as the dropping reason (sent without any argument)

TODO: keep client slot intact for some time to let it reconnect and get his ongoing game
back if his connection went bad.
===================
*/
void SV_DropClient(int slot, int reconnect, int warn, unsigned char svc_reason)
{
	char sendmsg[MAX_NET_CMDSIZE];
	int sendlen;

	if (svs.sv_clients[slot].connected)
	{
		Sys_Printf("Server: client %d dropped\n", slot);

		if (svs.sv_clients[slot].ingame)
			Game_SV_ClientDisconnect(slot);

		if (warn)
		{
			/* TODO: sometimes the server loads a new map before the client receives the reconnect command, them the client tries to draw old entities but the server has already cleaned the models! :( */
			Host_NetchanCleanQueue(svs.sv_clients[slot].netconn);
			sendlen = 0;
			MSG_WriteByte(sendmsg, &sendlen, svc_reason);
			Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, sendmsg, sendlen, NET_CMD_UNRELIABLE); /* need to send as unreliable because of reliable acks and other stuff */
			Host_NetchanDispatchCommands(svs.sv_clients[slot].netconn, true, svs.sv_clients[slot].client_id, svs.game_id); /* send immediately and don't care if it reaches the client or not, it will timeout anyway TODO: see if this makes the final messages of why he was disconnected reach him */
		}

		if (!reconnect) /* clear changelevel data to avoid a new player using data from other player that was present during a changelevel */
			Game_SV_ClientSetNewSpawnParms(slot);
	}

	SV_CleanClientSlot(slot, reconnect);
}

/*
===================
SV_StartServer

Starts a new game, should only be called by the proxy commands "startserver" and "changelevel"
===================
*/
void SV_StartServer(int changelevel)
{
	if (!svs.active)
	{
		Sys_Printf("Server: Can't start a server if the server subsystem isn't initialized\n");
		return;
	}

	if (!svs.name[0])
	{
		Sys_Printf("SV_StartServer: can't start server, no map specified.\n");
		return;
	}

	if (changelevel)
		Game_SV_SaveSpawnParms();

	SV_ShutdownServer(changelevel); /* this will inform all clients, including local clients */

	/* free all extra memory used by the last game */
	Sys_MemFreeToLowMark(&svr_mem, servermarker);
	/*
		Since local servers always start before local clients, it's safe to do this here. Because
		of this, the message to the local client should arrive without any delay on the next frame
		and the disconnection should happen at the same time, to prevet any use of the now freed
		models. TODO FIXME: use local message passing buffers to GUARANTEE zero-delay delivery.
	*/
	Host_CleanModels();

	/* end of housekeeping, start initializing and loading stuff */

	if (!Sys_NetSVStart())
	{
		Sys_Printf("Server: Can't create netconn\n");
		return;
	}

	svs.loading = true;
	svs.paused = false;
	svs.game_id++;
	if (!svs.game_id) /* prevent a null game_id */
		svs.game_id++;
	svs.physworld = Sys_CreatePhysicsWorld(true);
	Host_VoxelCreate(svs.loading_saved_game);
	Game_SV_NewGame(svs.loading_saved_game);
	if (changelevel)
		Game_SV_LoadSpawnParms();
	else
		Game_SV_SetNewSpawnParms();
	svs.loading = false;

	svs.listening = true;

	/* TODO: run some frames to let everything settle (except if loading a game, because everything is already initialized) */

	/* TODO CONSOLEDEBUG Sys_Printf("Server started. (%s)\n", svs.name); */
}

/*
===================
SV_ShutdownServer

Ends the current game
===================
*/
void SV_ShutdownServer(int changelevel)
{
	int i;
	int was_listening = svs.listening;

	if (!svs.active)
		return;

	/* TODO: test what happens with clients on the signon stage */
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (changelevel)
			SV_DropClient(i, true, true, SVC_RECONNECT);
		else
			SV_DropClient(i, false, true, SVC_SERVERQUIT);
	}

	Game_SV_EndGame();
	svs.loading = false; /* because loading may fail */
	svs.listening = false;
	/* if (!changelevel)
		svs.name[0] = 0; TODO: this is called when starting a new one, so do not clear the name */

	Sys_DestroyPhysicsWorld(svs.physworld);
	svs.physworld = NULL;
	Sys_NetSVStop();
	SV_ClearAllPrecaches();

	if (was_listening && !changelevel)
		Sys_Printf("Server shutdown\n");
}

/*
===================
SV_ParseClientMessages

Called every frame to get client commands
===================
*/
void SV_ParseClientMessages(int slot, char *msg, int len)
{
	int read = 0;
	unsigned char cmd;

	unsigned short ack_id;
	char new_name[MAX_CLIENT_NAME];

	if (svs.loading_saved_game)
		return; /* ignore while waiting */

	while (read != len)
	{
		MSG_ReadByte(msg, &read, len, &cmd);

		switch (cmd)
		{
			case CLC_DISCONNECT:
				SV_DropClient(slot, false, false, 0);
				return; /* ignore other messages */
			case CLC_SNAPSHOTACK:
				MSG_ReadShort(msg, &read, len, &ack_id);
				SV_SnapshotReceivedAck(slot, ack_id);
				break;
			case CLC_SNAPSHOTRESET:
				SV_SnapshotReset(slot);
				break;
			case CLC_PAUSE:
				if (!sv_pausable->doublevalue)
				{
					char sendmsg[MAX_NET_CMDSIZE];
					int sendlen;

					sendlen = 0;
					MSG_WriteByte(sendmsg, &sendlen, SVC_NOPAUSE);
					Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, sendmsg, sendlen, NET_CMD_RELIABLE);
					break;
				}

				Host_CMDBufferAdd("sv_pause");
				break;
			case CLC_NAME:
				MSG_ReadString(msg, &read, len, new_name, MAX_CLIENT_NAME);
				Game_SV_ClientSetName(slot, new_name, true);
				break;
			default:
				if (!Game_SV_ParseClientMessages(slot, cmd, msg, &read, len))
				{
					Sys_Printf("Unknown CLC (%02X) received, disconnecting client %d...\n", cmd, slot);
					SV_DropClient(slot, false, true, SVC_ERROR);
					return; /* stop trying to read */
				}
				else
					break;
		}
	}
}

/*
===================
SV_ParseClientSignonMessages

Connection handshaking
===================
*/
void SV_ParseClientSignonMessages(int slot, char *msg, int len)
{
	char sendmsg[MAX_NET_CMDSIZE];
	int sendlen;
	int read = 0;
	short cmd, cmd2;
	precacheindex_t i;

	while (read != len)
	{
		if (svs.sv_clients[slot].signon == 0)
		{
			/* these two were already parsed by the slot-allocation code, ignore them */
			MSG_ReadShort(msg, &read, len, &cmd);
			MSG_ReadShort(msg, &read, len, &cmd2);

			Sys_Printf("SGNS: Server welcoming new client %d...\n", slot);

			sendlen = 0;
			MSG_WriteShort(sendmsg, &sendlen, 2);
			MSG_WriteInt(sendmsg, &sendlen, svs.sv_clients[slot].client_id);
			MSG_WriteInt(sendmsg, &sendlen, svs.game_id);
			MSG_WriteString(sendmsg, &sendlen, sv_hostname->charvalue);

			/* TODO CONSOLEDEBUG Sys_Printf("SGNS: Server sending %d model precaches...\n", svs.precached_models_num); */

			MSG_WritePrecache(sendmsg, &sendlen, svs.precached_models_num);
			Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, sendmsg, sendlen, NET_CMD_RELIABLE);

			for (i = 0; i < svs.precached_models_num; i++)
			{
				/* TODO CONSOLEDEBUG Sys_Printf("SGNS: Server sending model precache \"%s\"\n", svs.precached_models[i]); */
				Sys_Snprintf(sendmsg, MAX_MODEL_NAME, "%s", svs.precached_models[i]);
				/* not forgeting the zero terminator... */
				Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, sendmsg, strlen(sendmsg) + 1, NET_CMD_RELIABLE);
			}

			/* TODO CONSOLEDEBUG Sys_Printf("SGNS: Server sending %d sound precaches...\n", svs.precached_sounds_num); */
			sendlen = 0;
			MSG_WritePrecache(sendmsg, &sendlen, svs.precached_sounds_num);
			Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, sendmsg, sendlen, NET_CMD_RELIABLE);

			for (i = 0; i < svs.precached_sounds_num; i++)
			{
				/* TODO CONSOLEDEBUG Sys_Printf("SGNS: Server sending sound precache \"%s\"\n", svs.precached_sounds[i]); */
				Sys_Snprintf(sendmsg, MAX_SOUND_NAME, "%s", svs.precached_sounds[i]);
				/* not forgeting the zero terminator... */
				Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, sendmsg, strlen(sendmsg) + 1, NET_CMD_RELIABLE);
			}

			/* TODO CONSOLEDEBUG Sys_Printf("SGNS: Server sending baseline...\n"); */
			/* TODO: SEND BASELINE HERE */

			/* TODO CONSOLEDEBUG Sys_Printf("SGNS: End of baseline\n"); */
			sendlen = 0;
			MSG_WriteShort(sendmsg, &sendlen, NET_MAGIC + 1);
			Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, sendmsg, sendlen, NET_CMD_RELIABLE);
			svs.sv_clients[slot].signon = 1;
		}
		else if (svs.sv_clients[slot].signon == 1)
		{
			MSG_ReadShort(msg, &read, len, &cmd);

			if (cmd == NET_MAGIC + 2)
			{
				char netname[MAX_CLIENT_NAME];
				MSG_ReadString(msg, &read, len, netname, sizeof(netname));

				svs.sv_clients[slot].signon = 2;
				svs.sv_clients[slot].ingame = true;
				/* TODO CONSOLEDEBUG Sys_Printf("SGNS: Server acknowledging client...\n"); */
				SV_VoxelQueueToNewClient(slot);
				Game_SV_ClientConnect(slot, netname, svs.loading_saved_game);
			}
			else
			{
				Sys_Printf("SGNS: Server rejecting client %d with corrupted data...\n", slot);
				SV_DropClient(slot, false, true, SVC_ERROR);
			}
		}
	}
}

/*
===================
SV_SendClientMessages

Called every frame to update the client state
===================
*/
void SV_SendClientMessages(int slot)
{
	if (svs.loading_saved_game)
		return; /* ignore while waiting */

	SV_SnapshotCreate(slot);
	SV_SnapshotSend(slot);

	/* send anything game-specific */
	if (svs.sv_clients[slot].ingame)
		Game_SV_SendClientMessages(slot);
}

/*
===================
SV_ParseMessages

Parses the clients messages
===================
*/
void SV_ParseMessages(void)
{
	char msg[MAX_NETCHAN_DATASIZE];
	int len, reliable, i;
	unsigned int id1, id2;
	seqnum_t inseq, myseq;
	/* for unconnected clients TODO: see if sharing this won't cause problems */
	static packetqueue_t *tmp_netconn = NULL;
	char sendmsg[MAX_NET_CMDSIZE];
	int sendlen;
	client_id_t from_client_id;
	client_id_t game_id;

	/* TODO: possible denial of services in lots of places? Limit incoming packets per client per frame */
	/* TODO: is it possible to keep receiving infinite messages with a fast enough connection and never leave this loop? */
	while (Host_NetchanReceiveCommands(msg, &len, &id1, &id2, &reliable, &inseq, &myseq, true, &from_client_id, &game_id))
	{
		/* see if it's a message from someone already connected/connecting */
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			/* make sure the client is the right one and that no leftover message from the last game will get in */
			/* if (svs.sv_clients[i].connected && svs.sv_clients[i].netconn->dest_id1 == id1 && svs.sv_clients[i].netconn->dest_id2 == id2 && (svs.sv_clients[i].client_id == from_client_id || svs.sv_clients[i].signon == 0) && (svs.game_id == game_id || svs.sv_clients[i].signon == 0)) */
			if (svs.sv_clients[i].connected && svs.sv_clients[i].netconn->dest_id1 == id1 && svs.sv_clients[i].netconn->dest_id2 == id2 && svs.sv_clients[i].client_id == from_client_id  && (svs.game_id == game_id || svs.sv_clients[i].signon == 0))
				break;

			/* if (svs.sv_clients[i].connected && svs.sv_clients[i].netconn->dest_id1 == id1 && (svs.sv_clients[i].client_id == from_client_id || svs.sv_clients[i].signon == 0) && (svs.game_id == game_id || svs.sv_clients[i].signon == 0)) */
			if (svs.sv_clients[i].connected && svs.sv_clients[i].netconn->dest_id1 == id1 && svs.sv_clients[i].client_id == from_client_id && (svs.game_id == game_id || svs.sv_clients[i].signon == 0))
			{
				/* if only id2 changed but the rest is OK, update id2 */
				svs.sv_clients[i].netconn->dest_id2 = id2;
				/*
					TODO FIXME: THIS IS A NAT FIX FOR CHANGING OUTGOUND UDP PORTS ON ROUTERS AND SHOULD BE ON A SYS MODULE.
					NO UDP/IP/NAT/ROUTER IDIOSYNCHRACIES SHOULD BE HERE!
				*/
				break;
			}
		}

		/* if not, see if we have free slots */
		if (i == MAX_CLIENTS)
		{
			if (!tmp_netconn)
				tmp_netconn = Host_NetchanCreate();
			Host_NetchanClean(tmp_netconn, false);
			if (len == sizeof(short) * 2) /* see if the connection command fits exactly in this packet */
			{
				/* did we receive the magic packet? */
				if (((short *)msg)[0] == NET_PROTOCOL && ((short *)msg)[1] == NET_MAGIC)
				{
					int num_connected_clients = 0;

					/* check how many clients are connected */
					for (i = 0; i < MAX_CLIENTS; i++)
					{
						if (svs.sv_clients[i].connected)
							num_connected_clients++;
					}

					if (num_connected_clients >= (int)sv_maxplayers->doublevalue)
					{
						/* we don't have any free slots */
						Sys_Printf("SGNS: Client rejected, server is full.\n");
						tmp_netconn->dest_id1 = id1;
						tmp_netconn->dest_id2 = id2;
						sendlen = 0;
						MSG_WriteShort(sendmsg, &sendlen, 1);
						Host_NetchanQueueCommand(tmp_netconn, sendmsg, sendlen, NET_CMD_RELIABLE);
						Host_NetchanDispatchCommands(tmp_netconn, true, 0, svs.game_id); /* don't care about errors */
						continue; /* ignore this client, unfortunately */
					}

					/* then find a free slot */
					for (i = 0; i < MAX_CLIENTS; i++)
					{
						if (!svs.sv_clients[i].connected)
						{
							svs.sv_clients[i].connected = true;
							svs.sv_clients[i].netconn->dest_id1 = id1;
							svs.sv_clients[i].netconn->dest_id2 = id2;
							svs.sv_clients[i].netconn->last_received_time = host_realtime;
							if (!svs.sv_clients[i].client_id) /* new connection, new id - zero is invalid */
								svs.sv_clients[i].client_id = SV_GenerateClientId();
							break;
						}
					}

					if (i == MAX_CLIENTS) /* this should never happen, but... */
						Host_Error("SGNS: had free slots, but didn't find any!\n");
				}
				/* did we receive the magic packet for server info ? */
				else if (((short *)msg)[0] == NET_PROTOCOL_INFO && ((short *)msg)[1] == NET_MAGIC)
				{
					int numplayers, curplayer;
					/* TODO: do not do this, maintain this number elsewhere */
					for (numplayers = 0, curplayer = 0; curplayer < MAX_CLIENTS; curplayer++)
						if (svs.sv_clients[curplayer].ingame)
							numplayers++;

					Sys_Printf("SGNS: Server accepting info request from (%x,%x)\n", id1, id2);
					tmp_netconn->dest_id1 = id1;
					tmp_netconn->dest_id2 = id2;
					sendlen = 0;
					MSG_WriteInt(sendmsg, &sendlen, numplayers);
					Host_NetchanQueueCommand(tmp_netconn, sendmsg, sendlen, NET_CMD_RELIABLE);
					Host_NetchanDispatchCommands(tmp_netconn, true, 0, svs.game_id); /* don't care about errors */
					continue; /* ignore, not a player */
				}
				else
				{
					Sys_Printf("SGNS: Server rejecting client with wrong protocol number... (%x,%x)\n", ((short *)msg)[0], ((short *)msg)[1]);
					tmp_netconn->dest_id1 = id1;
					tmp_netconn->dest_id2 = id2;
					sendlen = 0;
					MSG_WriteShort(sendmsg, &sendlen, 0);
					Host_NetchanQueueCommand(tmp_netconn, sendmsg, sendlen, NET_CMD_RELIABLE);
					Host_NetchanDispatchCommands(tmp_netconn, true, 0, svs.game_id); /* don't care about errors */
					continue; /* ignore this client */
				}
			}
			else
				continue; /* junk data */
		}

		if (!reliable && !svs.sv_clients[i].ingame) /* do not accept unreliables until fully connected */
		{
			Host_NetchanUpdateNC(svs.sv_clients[i].netconn, reliable, inseq, myseq, host_realtime);  /* update acks and timeout anyway */
			continue;
		}
		if (reliable && (inseq != (seqnum_t)(svs.sv_clients[i].netconn->inseq + 1))) /* ignore duplicated and out of order reliables */
		{
			Host_NetchanUpdateNC(svs.sv_clients[i].netconn, reliable, inseq, myseq, host_realtime);  /* update acks and timeout anyway */
			continue;
		}

		if (svs.sv_clients[i].ingame == true)
		{
			SV_ParseClientMessages(i, msg, len);
		}
		else
		{
			/* deal with the connection messages here */
			SV_ParseClientSignonMessages(i, msg, len);
		}

		if (!svs.sv_clients[i].connected) /* client disconnected after we received certain messages */
			continue; /* we may get messages from other clients */

		/* all ok */
		Host_NetchanUpdateNC(svs.sv_clients[i].netconn, reliable, inseq, myseq, host_realtime);
	}
}

/*
===================
SV_Frame

TODO: RUN ALL FRAMES WITH A FIXED TIMESTEP (like the physics code). We may even run each gamecode frame inside the physics
pre/post fixed frames! Or just deal with the fixed frames here and send a fixed timestep to the physics code. As of the time
of wrinting this commentary, the physics code may run more than one frame (while buffering feedback data for the gamecode - or not).
TODO: sleep if no clients?
TODO: run at least ONE frame with svs.loading_saved_game to make sure everything falls in their place without any simulation happening?
===================
*/
void SV_Frame(void)
{
	mstime_t start_time, start_time2, end_time, end_time2;
	int i;

	if (!svs.active)
		return;

	if (svs.listening)
	{
		if (host_speeds->doublevalue)
			start_time = Sys_Time();
		SV_CheckFinishedLoading();

		if (host_speeds->doublevalue)
			start_time2 = Sys_Time();
		if (!svs.loading_saved_game && !svs.paused) /* ignore while waiting or paused */
			Game_SV_StartFrame(host_frametime);
		SV_ParseMessages();
		if (host_speeds->doublevalue)
		{
			end_time2 = Sys_Time();
			Sys_Printf("server recv: % 3f\n", end_time2 - start_time2);
			start_time2 = Sys_Time();
		}
		if (!svs.loading_saved_game && !svs.paused) /* ignore while waiting or paused */
		{
			/*
				Think order:
				PlayerPre
				EverythingElse (including players, if think pointers set)
				Physics code runs
				PlayerPost

				This makes it easy to "pass" control data from players to entities, who
				will in turn check and adjust the information themselves before the
				physics code does anything with them.
			*/
			for (i = 0; i < MAX_CLIENTS; i++)
				if (svs.sv_clients[i].ingame)
					Game_SV_ClientPreThink(i);

			Game_SV_RunThinks();

			if (host_speeds->doublevalue)
			{
				end_time2 = Sys_Time();
				Sys_Printf("server gpre: % 3f\n", end_time2 - start_time2);
				start_time2 = Sys_Time();
			}

			{
				/* call physics simulation with a list of the entities that are asynchronous */
				int cmdents[MAX_CLIENTS + 1];
				int cmdents_idx = 0;
				for (i = 0; i < MAX_CLIENTS; i++)
					if (svs.sv_clients[i].ingame)
						cmdents[cmdents_idx++] = Game_SV_ClientGetCmdEnt(i);

				cmdents[cmdents_idx] = -1;
				Sys_PhysicsSimulate(svs.physworld, host_frametime, -1, cmdents);
			}

			if (host_speeds->doublevalue)
			{
				end_time2 = Sys_Time();
				Sys_Printf("server gphy: % 3f\n", end_time2 - start_time2);
				start_time2 = Sys_Time();
			}

			for (i = 0; i < MAX_CLIENTS; i++)
				if (svs.sv_clients[i].ingame)
					Game_SV_ClientPostThink(i);

			Game_SV_EndFrame();
		}

		Host_VoxelCommitUpdates();
		if (host_speeds->doublevalue)
		{
			end_time2 = Sys_Time();
			Sys_Printf("server gpos: % 3f\n", end_time2 - start_time2);
			start_time2 = Sys_Time();
		}
		for (i = 0; i < MAX_CLIENTS; i++)
			if (svs.sv_clients[i].connected)
			{
				if (svs.sv_clients[i].ingame)
				{
					SV_SendClientMessages(i);
					SV_VoxelQueueSendPartial(i);
				}

				if (Host_NetchanDispatchCommands(svs.sv_clients[i].netconn, true, svs.sv_clients[i].client_id, svs.game_id) == NET_ERR)
				{
					Sys_Printf("Server error sending data to client %d\n", i);
					SV_DropClient(i, false, false, 0);
				}
				else if (svs.sv_clients[i].netconn->last_received_time + NET_MSTIMEOUT <= host_realtime)
				{
					/* TODO FIXME: some strange timeouts happening */
					Sys_Printf("Server timed out waiting for client %d\n", i);
					SV_DropClient(i, false, false, 0);
				}
			}
		if (host_speeds->doublevalue)
		{
			end_time2 = Sys_Time();
			Sys_Printf("server send: % 3f\n", end_time2 - start_time2);
			end_time = Sys_Time();
			Sys_Printf("server totl: % 3f\n", end_time - start_time);
		}
	}
}

#ifdef DEDICATED_SERVER
void CL_PredUpdatePhysStats(entindex_t ent, vec3_t origin, vec3_t angles, vec3_t velocity, vec3_t avelocity, int onground) {}
void CL_PredUpdatePhysDirections(entindex_t ent, vec3_t forward, vec3_t right, vec3_t up) {}
void CL_PredUpdateTraceResultStart(entindex_t ent) {}
int CL_PredUpdateTraceResultStep(entindex_t ent, entindex_t hit, vec3_t pos, vec3_t normal, vec_t fraction) {}
void CL_PredPostPhysics(void) {}
const int CL_PredGetMoveType(entindex_t ent) {}
const unsigned int CL_PredGetAnglesFlags(entindex_t ent) {}
void CL_PredGetMoveCmd(entindex_t ent, vec_t *dest) {}
void CL_PredGetAimCmd(entindex_t ent, vec_t *dest) {}
void CL_PredGetMaxSpeed(entindex_t ent, vec_t *dest) {}
void CL_PredGetAcceleration(entindex_t ent, vec_t *dest) {}
int CL_PredGetIgnoreGravity(entindex_t ent) {}
int CL_PredCheckPhysicalCollisionResponse(entindex_t e1, entindex_t e2) {}
void CL_PredTouchEnts(entindex_t who, entindex_t by, vec3_t pos, vec3_t normal, vec_t distance, int reaction, vec_t impulse) {}
void CL_PredEntityGetData(const entindex_t ent, vec_t *origin, vec_t *velocity, vec_t *avelocity, vec_t *angles, vec_t *frames, precacheindex_t *modelindex, vec_t *lightintensity, int *anim_pitch) {}
void CL_GetModelPhysicsTrimesh(const precacheindex_t model, model_trimesh_t **trimesh) {}
void CL_GetModelPhysicsBrushes(const precacheindex_t model, model_brushes_t **brushes) {}
void CL_GetModelPhysicsHeightfield(const precacheindex_t model, model_heightfield_t **heightfield) {}

#include "lua/src/lua.h"
lua_State *cl_lua_state;

void CL_MenuConsolePrint(char *text) {}

void Sys_UpdateVBO(int id, model_trimesh_part_t *trimesh, int is_triangle_strip) {}
int Sys_UploadVBO(int id, model_trimesh_part_t *trimesh, int is_triangle_strip, int will_be_updated) {}
void Sys_CleanVBOs(int shutdown) {}
void Sys_VideoBindShaderProgram(unsigned int shader_id, const vec4_t light0_position, const vec4_t light0_diffuse, const vec4_t light0_ambient) {}
void Sys_VideoTransformFor3DModel(vec_t *ent_modelmatrix) {}
int Sys_VideoDrawVBO(const int id, int texture_cl_id0, int texture_cl_id1, int texture_cl_id3, int texture_cl_id4, int texture_cl_id5, int vertstartinclusive, int vertendinclusive, int idxcount, int idxstart) {}
int ignore_window_resize_event = true;
void Sys_GetWidthHeight(int *width, int *height) {}
void Sys_ShutdownVideo(void) {}
void Sys_SkyboxUnload() {}

cvar_t *r_lodbias;
cvar_t *r_lodhysteresis;

void CL_InputProcessText(char *input_utf8, int key_index) {}
void CL_InputProcessKeyUpDown(int keyindex, int down, vec_t analog_rel, vec_t analog_abs) {}

void CL_CheckLoadingSlowness(void) {}
void CL_Init(void) {}
void CL_Shutdown(void) {}
void CL_Connect(int reconnect) {}
void CL_Disconnect(int reconnect, int warn) {}
void CL_Frame(void) {}

void CL_CleanTextures(int forcenokeep) {}
texture_t *CL_LoadTexture(const char *name, int keep, unsigned char *indata, int inwidth, int inheight, int data_has_mipmaps, int mipmapuntilwidth, int mipmapuntilheight) {}
#endif /* DEDICATED_SERVER */
