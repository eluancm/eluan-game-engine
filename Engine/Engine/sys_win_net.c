/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

#include <SDL_net.h>

/*
============================================================================

System-dependent common network functions

This is low level code, not used directly by the engine, only by
host_netchan.c to deliver the high-level interface

The general setup should be done once per run, the client/server specific
code should be done every time they are (re)initialized (once per new game,
for example). This initialization isn't done in netchan because netchan is
generic and this initialization is different for client and server.

FIXME: random chance we randomly select the same port for two clients
inside NAT?!

For UDP/IP, id1 = host, id2 = port

============================================================================
*/

#define DEFAULT_PORT	"26260"

/*
===================
Sys_NetInit
===================
*/
void Sys_NetInit(void)
{
	if (SDLNet_Init() == -1)
		Sys_Error("SDLNet_Init: %s\n", SDLNet_GetError());
}

/*
===================
Sys_NetShutdown
===================
*/
void Sys_NetShutdown(void)
{
	SDLNet_Quit();
}

/*
============================================================================

System-dependent server network functions

============================================================================
*/

cvar_t *sv_listenport = NULL;

/* FIXME: really ugly when you think about threading */
UDPpacket *net_svpacket = NULL;
UDPsocket server_socket = NULL;

/*
===================
Sys_CMD_SV_ListenPort
===================
*/
void Sys_CMD_SV_ListenPort(void)
{
	if (host_cmd_argc > 2)
	{
		Sys_Printf("Too many arguments for \"sv_listenport\"\n");
	}
	else
	{
		if (host_cmd_argc == 1)
			Sys_Printf("\"sv_listenport\" is \"%s\"\n", sv_listenport->charvalue);
		else
		{
			Host_CMDForceCvarSet(sv_listenport, host_cmd_argv[1], true);
		}
	}
}

/*
===================
Sys_NetSVWrite

Returns 1 on success, 0 on failure
Please make sure the buffer length is not bigger than MAX_NET_PACKETSIZE
===================
*/
int Sys_NetSVWrite(char *data, int len, unsigned int id1, unsigned int id2)
{
	int i;

	if (len > MAX_NET_PACKETSIZE)
	{
		Sys_Printf("Warning: Server tried to send a packet too big (%d) to fit in its buffers! Discarding...\n", len);
		return 0;
	}

	net_svpacket->address.host = id1;
	net_svpacket->address.port = id2;

	for (i = 0; i < len; i++)
		(net_svpacket->data)[i] = data[i];
	net_svpacket->len = len;
	if (SDLNet_UDP_Send(server_socket, -1, net_svpacket) == 0)
	{
		Sys_Printf("Server couldn't send packet to client, SDLNet_UDP_Send: %s\n", SDLNet_GetError());
		return 0;
	}
	else
		return 1;
}

/*
===================
Sys_NetSVRead

Returns the packet size, if any
Be sure that "pkt" holds at least MAX_NET_PACKETSIZE bytes
===================
*/
int Sys_NetSVRead(char *pkt, unsigned int *id1, unsigned int *id2)
{
	int i, res;

start:
	res = SDLNet_UDP_Recv(server_socket, net_svpacket);

	if (res == 1)
	{
		if (net_svpacket->len > MAX_NET_PACKETSIZE)
		{
			Sys_Printf("Warning: Server received a packet too big to fit in its buffers! Discarding...\n");
			goto start;
		}

		for (i = 0; i < net_svpacket->len; i++)
			pkt[i] = ((char *)net_svpacket->data)[i];
		*id1 = net_svpacket->address.host;
		*id2 = net_svpacket->address.port;
		return net_svpacket->len;
	}
	else if (res == 0)
	{
		return 0;
	}
	else
	{
		Sys_Printf("Couldn't poll for incoming server packets, SDLNet_UDP_Recv: %s\n", SDLNet_GetError());
		return 0;
	}
}

/*
===================
Sys_NetSVStart
===================
*/
int Sys_NetSVStart(void)
{
	if (!(server_socket = SDLNet_UDP_Open((Uint16)sv_listenport->doublevalue)))
	{
		Sys_Printf("Couldn't alloc server socket, SDLNet_UDP_Open: %s\n", SDLNet_GetError());
		return false;
	}

	Sys_Printf("Listening on port %d\n", (Uint16)sv_listenport->doublevalue);

	return true;
}

/*
===================
Sys_NetSVStop
===================
*/
void Sys_NetSVStop(void)
{
	SDLNet_UDP_Close(server_socket);
	server_socket = NULL;
}

/*
===================
Sys_NetSVInit
===================
*/
void Sys_NetSVInit(void)
{
	/* these cvars should be changed by cmds, to prevent their change while a server is started */
	sv_listenport = Host_CMDAddCvar("_sv_listenport", DEFAULT_PORT, CVAR_READONLY);

	Host_CMDAdd("sv_listenport", Sys_CMD_SV_ListenPort);

	if (!(net_svpacket = SDLNet_AllocPacket(MAX_NET_PACKETSIZE)))
		Sys_Error("SDLNet_AllocPacket: %s\n", SDLNet_GetError());
}

/*
===================
Sys_NetSVShutdown
===================
*/
void Sys_NetSVShutdown(void)
{
	SDLNet_FreePacket(net_svpacket);
}

/*
============================================================================

System-dependent client network functions

============================================================================
*/

cvar_t *cl_remoteserver = NULL;
cvar_t *cl_remoteport = NULL;

/* FIXME: really ugly when you think about threading */
UDPpacket *net_clpacket = NULL;
UDPsocket client_socket = NULL;
IPaddress client_serveraddress;

/*
===================
Sys_CMD_CL_RemoteServer
===================
*/
void Sys_CMD_CL_RemoteServer(void)
{
	if (host_cmd_argc > 2)
	{
		Sys_Printf("Too many arguments for \"cl_remoteserver\"\n");
	}
	else
	{
		if (host_cmd_argc == 1)
			Sys_Printf("\"cl_remoteserver\" is \"%s\"\n", cl_remoteserver->charvalue);
		else
		{
			Host_CMDForceCvarSet(cl_remoteserver, host_cmd_argv[1], true);
		}
	}
}

/*
===================
Sys_CMD_CL_RemotePort
===================
*/
void Sys_CMD_CL_RemotePort(void)
{
	if (host_cmd_argc > 2)
	{
		Sys_Printf("Too many arguments for \"cl_remoteport\"\n");
	}
	else
	{
		if (host_cmd_argc == 1)
			Sys_Printf("\"cl_remoteport\" is \"%s\"\n", cl_remoteport->charvalue);
		else
		{
			Host_CMDForceCvarSet(cl_remoteport, host_cmd_argv[1], true);
		}
	}
}

/*
===================
Sys_NetCLServerIsLocal

Returns 1 if the configured server is a local one, 0 otherwise
TODO: we may use our own internet address, etc.
===================
*/
int Sys_NetCLServerIsLocal(void)
{
	if (cls.active)
	{
		if (sv_listenport->doublevalue == cl_remoteport->doublevalue && (!strcmp(cl_remoteserver->charvalue, "localhost") || !strcmp(cl_remoteserver->charvalue, "127.0.0.1")))
			return true;
	}
	return false;
}

/*
===================
Sys_NetCLServerSetToLocal

Sets the client to connect to a local server
TODO: we may use our own internet address, etc.
===================
*/
void Sys_NetCLServerSetToLocal(void)
{
	if (cls.active)
	{
		Host_CMDForceCvarSet(cl_remoteport, sv_listenport->charvalue, true);
		Host_CMDForceCvarSet(cl_remoteserver, "localhost", true);
	}
}

/*
===================
Sys_NetCLWrite

Returns 1 on success, 0 on failure
Please make sure the buffer length is not bigger than MAX_NET_PACKETSIZE
===================
*/
int Sys_NetCLWrite(char *data, int len)
{
	int i;

	if (len > MAX_NET_PACKETSIZE)
	{
		Sys_Printf("Warning: Client tried to send a packet too big (%d) to fit in its buffers! Discarding...\n", len);
		return 0;
	}

	net_clpacket->address.host = client_serveraddress.host;
	net_clpacket->address.port = client_serveraddress.port;

	for (i = 0; i < len; i++)
		(net_clpacket->data)[i] = data[i];
	net_clpacket->len = len;
	if (SDLNet_UDP_Send(client_socket, -1, net_clpacket) == 0)
	{
		Sys_Printf("Client couldn't send packet to server, SDLNet_UDP_Send: %s\n", SDLNet_GetError());
		return 0;
	}
	else
		return 1;
}

/*
===================
Sys_NetCLRead

Returns the packet size, if any
Be sure that "pkt" holds at least MAX_NET_PACKETSIZE bytes
===================
*/
int Sys_NetCLRead(char *pkt)
{
	int i, res;

start:
	res = SDLNet_UDP_Recv(client_socket, net_clpacket);

	if (res == 1)
	{
		if (net_clpacket->len > MAX_NET_PACKETSIZE)
		{
			Sys_Printf("Warning: Client received a packet too big to fit in its buffers! Discarding...\n");
			goto start;
		}
		if (net_clpacket->address.host != client_serveraddress.host && net_clpacket->address.port != client_serveraddress.port)
			goto start; /* ignore silently if from other host:port pair */

		for (i = 0; i < net_clpacket->len; i++)
			pkt[i] = ((char *)net_clpacket->data)[i];
		return net_clpacket->len;
	}
	else if (res == 0)
	{
		return 0;
	}
	else
	{
		Sys_Printf("Couldn't poll for incoming client packets, SDLNet_UDP_Recv: %s\n", SDLNet_GetError());
		return 0;
	}
}

/*
===================
Sys_NetCLStart
===================
*/
int Sys_NetCLStart(void)
{
	/* use a random outgoing port */
	if (!(client_socket = SDLNet_UDP_Open(0)))
	{
		Sys_Printf("Couldn't alloc client socket, SDLNet_UDP_Open: %s\n", SDLNet_GetError());
		return false;
	}

	if (SDLNet_ResolveHost(&client_serveraddress, cl_remoteserver->charvalue, (Uint16)cl_remoteport->doublevalue) == -1)
	{
		Sys_Printf("SDLNet_ResolveHost(%s, %d): %s\n", cl_remoteserver->charvalue, (Uint16)cl_remoteport->doublevalue, SDLNet_GetError());
		return false;
	}

	Sys_Printf("Connecting to %s:%s...\n", cl_remoteserver->charvalue, cl_remoteport->charvalue);

	return true;
}

/*
===================
Sys_NetCLStop
===================
*/
void Sys_NetCLStop(void)
{
	SDLNet_UDP_Close(client_socket);
	client_socket = NULL;
}

/*
===================
Sys_NetCLInit
===================
*/
void Sys_NetCLInit(void)
{
	/* these cvars should be changed by cmds, to prevent their change while a client is started */
	cl_remoteserver = Host_CMDAddCvar("_cl_remoteserver", "localhost", CVAR_READONLY);
	cl_remoteport = Host_CMDAddCvar("_cl_remoteport", DEFAULT_PORT, CVAR_READONLY);

	Host_CMDAdd("cl_remoteserver", Sys_CMD_CL_RemoteServer);
	Host_CMDAdd("cl_remoteport", Sys_CMD_CL_RemotePort);

	if (!(net_clpacket = SDLNet_AllocPacket(MAX_NET_PACKETSIZE)))
		Sys_Error("SDLNet_AllocPacket: %s\n", SDLNet_GetError());
}

/*
===================
Sys_NetCLShutdown
===================
*/
void Sys_NetCLShutdown(void)
{
	SDLNet_FreePacket(net_clpacket);
}
