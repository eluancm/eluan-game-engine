/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Network communication channel management

This is a wrapper that implements packet-based communication with reliable
(guaranteed and in-order delivery, slow) and unreliable (may get lost or
arrive out-of-order, fast) channels on top of the low-level system network
code in sys_win_net.c

It should be initialized once per run and cleaned once every new game.

FIXME: dispatch commands if we have enough to fill a packet?

Here is a quick and dirty overview of the handshake process used by
the higher level client-server code for NET_PROTOCOL 1: (TODO: document NET_PROTOCOL_INFO)

Client sends short protocol + short MAGIC
											server receives and replies with:
											short 0 -> invalid protocol and/or magic
											short 1 -> server is full
											short 2 then a client_id_t for the client_id and
											another for the game_id then a string with the
											server hostname and a short X -> X = number of
											model precaches
											then, server sends all model precaches as strings
client receives reply, it it's 0 or 1, drop
if 2, then client receives client_id,
game_id, hostname and short X and set it as
model precaches total and start reading all
model precaches. sendings NOPS between each
load (to prevent timeout)
											server sends short X -> number of sound precaches
											server starts sending all sound precaches
client receives short x, sets total
number of sound precaches and starts
reading all sound precaches, sends
a NOP between each one to prevent
timeouts
											server sends baseline
client parses baseline
											baseline ends with MAGIC + 1
client acknowledges magic + 1 and is
ingame, client sends magic + 2 to ack,
followed by the client name
											server receives magic + 2, the client name and and
											sets client ingame


============================================================================
*/

/* how many complete commands we may buffer */
#define		MAX_NET_BUFFEREDCMDS		8192

#define		NET_RELIABLE_BIT			(1<<15)
#define		NET_PACKET_ID				0xA55A

/* packet structure */
typedef struct packet_s {
	unsigned short	id;
	unsigned short	len;		/* RELIABLE_BIT may be set there, the rest must be less than MAX_NETCHAN_DATASIZE, points to the next free position. Invalid if == MAX_NETCHAN_DATASIZE */
	seqnum_t		ack;		/* last reliable received from the other side */
	seqnum_t		reliableid;	/* if the RELIABLE_BIT is set, this is the id of the reliable packet we are sending */
	client_id_t		client_id;	/* this is the identifier the server sends the client on receiving a connection request */
	client_id_t		game_id;	/* this is the game identifier the server sends the client on receiving a connection request */
	char			data[MAX_NETCHAN_DATASIZE];
} packet_t;

/*
===================
Host_NetchanReceiveCommands

Receives ONE packet with one or more commands each time it's called
returns false if there's nothing else to receive this frame

server = true if the server is calling this function
server = false if the client is calling this function

The client code will automatically check if it comes from the server we're connected.
The server code will set the id1 and id2 parameters, for new connection purposes.

"out" must be at least as big as MAX_NETCHAN_DATASIZE
===================
*/
int Host_NetchanReceiveCommands(char *out, int *len, unsigned int *id1, unsigned int *id2, int *reliable, seqnum_t *inseq, seqnum_t *myseq, int server, client_id_t *client_id, client_id_t *game_id)
{
	int res, i;
	packet_t pkt;

	if ((id1 || id2) && !server)
	{
		Sys_Printf("Netchan: client shouldn't set id1 || id2\n");
		return false;
	}

	if (!(id1 && id2) && server)
	{
		Sys_Printf("Netchan: server should set id1 && id2\n");
		return false;
	}

	do
	{
		if (server)
			res = Sys_NetSVRead((char *)&pkt, id1, id2);
		else
			res = Sys_NetCLRead((char *)&pkt);

		if (!res)
			return false;

		/* simulate packet loss */
		if (host_netloss->doublevalue > Sys_Random(0,1))
			continue;

		*reliable = false;
		if (pkt.len & NET_RELIABLE_BIT)
		{
			pkt.len -= NET_RELIABLE_BIT;
			*reliable = true;
		}
	} while ((pkt.len > MAX_NETCHAN_DATASIZE) || /* ignore and discard these stuff silently: probably bugs, attacks or the like, the engine wouldn't have send them FIXME: when debugging, warn? */
			 (pkt.id != NET_PACKET_ID));

	if (*reliable)
		*inseq = pkt.reliableid;

	*myseq = pkt.ack;
	/* copy, just to be safe */
	for (i = 0; i < pkt.len; i++)
		out[i] = pkt.data[i];
	*len = pkt.len;

	*client_id = pkt.client_id;
	*game_id = pkt.game_id;

	if (!server && !pkt.len)
		Sys_Printf("<--- server to client keepalive\n");

	return true;
}

/*
===================
Host_NetchanUpdateNC

Updates data that we will need later
===================
*/
void Host_NetchanUpdateNC(packetqueue_t *nc, int reliable, seqnum_t r_inseq, seqnum_t r_myseq, mstime_t r_time)
{
	/* TODO: look other places where I forgot to cast and a overflow didn't happen as expected!! Damnit!! */
	if (reliable && (r_inseq == (seqnum_t)(nc->inseq + 1)))
		nc->inseq = r_inseq;
	if (r_myseq == (seqnum_t)(nc->outseq_ack + 1))
		nc->outseq_ack = r_myseq;
	nc->last_received_time = r_time;
}

/*
===================
Host_NetchanCommandsLeft

Returns how many commands still fit in the queue
===================
*/
int Host_NetchanCommandsLeft(packetqueue_t *nc, int type)
{
	int i;
	int count = 0;
	packetcmd_t		*pktqueue;

	if (type == NET_CMD_UNRELIABLE)
		pktqueue = nc->unreliable;
	else if (type == NET_CMD_RELIABLE)
		pktqueue = nc->reliable;
	else
		Sys_Error("Netchan: Invalid command type specified!\n");

	for (i = 0; i < MAX_NET_BUFFEREDCMDS; i++)
	{
		if (pktqueue->active)
		{
			pktqueue = pktqueue->next;
			count++;
		}
		else
			break;
	}

	return MAX_NET_BUFFEREDCMDS - count;
}

/*
===================
Host_NetchanQueueCommand

Queue a single command
===================
*/
int Host_NetchanQueueCommand(packetqueue_t *nc, char *cmd, int len, int type)
{
	int i;
	packetcmd_t		*pktqueue;

	if (type == NET_CMD_UNRELIABLE)
		pktqueue = nc->unreliable;
	else if (type == NET_CMD_RELIABLE)
		pktqueue = nc->reliable;
	else
		Sys_Error("Netchan: Invalid command type specified!\n");

	if (len >= MAX_NET_CMDSIZE)
	{
		if (type == NET_CMD_RELIABLE)
		{
			Sys_Printf("Netchan: buffer overflow on reliable stream, command too big\n");
			return NET_ERR;
		}
		else /* NET_CMD_UNRELIABLE */
		{
			Sys_Printf("Netchan: buffer overflow on unreliable stream, command too big, continuing...\n");
			return NET_OK; /* don't care, may only cause gameplay jitter */
		}
	}

	for (i = 0; i < MAX_NET_BUFFEREDCMDS; i++)
	{
		if (pktqueue->active)
			pktqueue = pktqueue->next;
		else
			break;
	}

	if (pktqueue->active)
	{
		if (type == NET_CMD_RELIABLE)
		{
			Sys_Printf("Netchan: buffer overflow on reliable stream\n");
			return NET_ERR;
		}
		else /* NET_CMD_UNRELIABLE */
		{
			Sys_Printf("Netchan: buffer overflow on unreliable stream, continuing...\n");
			return NET_OK; /* don't care, may only cause gameplay jitter */
		}
	}

	pktqueue->active = true;
	pktqueue->len = len;
	pktqueue->created_time = Sys_Time();
	for (i = 0; i < len; i++)
		pktqueue->data[i] = cmd[i];

	return NET_OK;
}

/*
===================
Host_NetchanDispatchCommands

Sends all buffered commands in a single shot

server = true if the server is calling this function
server = false if the client is calling this function
===================
*/
int Host_NetchanDispatchCommands(packetqueue_t *nc, int server, client_id_t client_id, client_id_t game_id)
{
	int i;
	packet_t pkt;

	/* always send this data */
	pkt.id = NET_PACKET_ID;
	pkt.ack = nc->inseq;
	pkt.client_id = client_id;
	pkt.game_id = game_id;

	/* nothing to send? send a nop for ack purposes and to prevent timeouts */
	if (!nc->last_reliable_len && !nc->reliable->active && !nc->unreliable->active)
	{
		pkt.data[0] = 0;
		pkt.len = 0;
		pkt.reliableid = 0;

		if (server)
		{
			if (!Sys_NetSVWrite((char *)&pkt, NET_HEADERLEN + pkt.len, nc->dest_id1, nc->dest_id2))
				return NET_ERR;
		}
		else
		{
			Sys_Printf("---> client to server keepalive\n");
			if (!Sys_NetCLWrite((char *)&pkt, NET_HEADERLEN + pkt.len))
				return NET_ERR;
		}

		return NET_OK;
	}

	/* simulate network latency */
	if (Sys_Time() >= nc->reliable->created_time + host_netdelay->doublevalue + Sys_Random(host_netdelay_jitterlow->doublevalue, host_netdelay_jitterhigh->doublevalue))
	{
		/* see if we have reliables to send */
		if (nc->outseq_ack == nc->outseq) /* also true when creating a new connection */
		{
			if (nc->reliable->active) /* new reliable packet */
				nc->outseq++;
			nc->last_reliable[0] = 0;
			nc->last_reliable_len = 0;

			while (nc->reliable->active)
			{

				if (nc->reliable->len + nc->last_reliable_len >= MAX_NETCHAN_DATASIZE - 2)
				{
					if (nc->last_reliable_len == 0)
					{
						Sys_Printf("Netchan: Reliable stream cmd doesn't fit! To: %d %d\n", nc->dest_id1, nc->dest_id2);
						return NET_ERR;
					}

					break;
				}

				for (i = 0; i < nc->reliable->len; i++)
					nc->last_reliable[nc->last_reliable_len++] = nc->reliable->data[i];

				nc->reliable->active = false;
				nc->reliable = nc->reliable->next;
			}
		}
	}

	if (nc->last_reliable_len)
	{
		for (i = 0; i < nc->last_reliable_len; i++)
			pkt.data[i] = nc->last_reliable[i];

		pkt.len = nc->last_reliable_len;
		pkt.len |= NET_RELIABLE_BIT;
		pkt.reliableid = nc->outseq;

		/* send, stripping the unreliable bit for the net subsystem, of course */
		if (server)
		{
			if (!Sys_NetSVWrite((char *)&pkt, NET_HEADERLEN + pkt.len - NET_RELIABLE_BIT, nc->dest_id1, nc->dest_id2)) /* FIXME: enviar unreliables aqui também? */
				return NET_ERR;
		}
		else
		{
			if (!Sys_NetCLWrite((char *)&pkt, NET_HEADERLEN + pkt.len - NET_RELIABLE_BIT))
				return NET_ERR;
		}
	}

	/* now send all the unreliable cmds, breaking them down if we can't send everything in one packet */
	pkt.data[0] = 0;
	pkt.len = 0;
	pkt.reliableid = 0;
	while (nc->unreliable->active)
	{
		/* simulate network latency */
		if (Sys_Time() < nc->unreliable->created_time + host_netdelay->doublevalue + Sys_Random(host_netdelay_jitterlow->doublevalue, host_netdelay_jitterhigh->doublevalue))
			break;

		if (nc->unreliable->len + pkt.len >= MAX_NETCHAN_DATASIZE - 2)
		{
			if (pkt.len == 0)
			{
				Sys_Printf("Netchan: Unreliable stream cmd doesn't fit! To: %d %d\n", nc->dest_id1, nc->dest_id2);
				return NET_ERR; /* since this shouldn't happen, we return an error even though it's an unreliable packet.  There's also no need to unqueue the packet since this will cause a disconnection anyway. */
			}
			else
			{
				if (server)
				{
					if (!Sys_NetSVWrite((char *)&pkt, NET_HEADERLEN + pkt.len, nc->dest_id1, nc->dest_id2))
						return NET_ERR;
				}
				else
				{
					if (!Sys_NetCLWrite((char *)&pkt, NET_HEADERLEN + pkt.len))
						return NET_ERR;
				}

				pkt.data[0] = 0;
				pkt.len = 0;
			}
		}
		else
		{
			for (i = 0; i < nc->unreliable->len; i++)
				pkt.data[pkt.len++] = nc->unreliable->data[i];

			nc->unreliable->active = false;
			nc->unreliable = nc->unreliable->next;
		}
	}

	if (pkt.len)
	{
		if (server)
		{
			if (!Sys_NetSVWrite((char *)&pkt, NET_HEADERLEN + pkt.len, nc->dest_id1, nc->dest_id2))
				return NET_ERR;
		}
		else
		{
			if (!Sys_NetCLWrite((char *)&pkt, NET_HEADERLEN + pkt.len))
				return NET_ERR;
		}
	}

	return NET_OK;
}

/*
===================
Host_NetchanCleanQueue
===================
*/
void Host_NetchanCleanQueue(packetqueue_t *nc)
{
	packetcmd_t		*pktcmd1, *pktcmd2;

	pktcmd1 = nc->reliable;
	pktcmd2 = nc->unreliable;

	pktcmd1->active = false;
	pktcmd2->active = false;
	memset(pktcmd1->data, 0, MAX_NET_CMDSIZE);
	memset(pktcmd2->data, 0, MAX_NET_CMDSIZE);
	pktcmd1->len = 0;
	pktcmd2->len = 0;
	pktcmd1->created_time = 0;
	pktcmd2->created_time = 0;

	pktcmd1 = pktcmd1->next;
	pktcmd2 = pktcmd2->next;

	/* assume both are the same size */
	while (pktcmd1 != nc->reliable)
	{
		pktcmd1->active = false;
		pktcmd2->active = false;
		memset(pktcmd1->data, 0, MAX_NET_CMDSIZE);
		memset(pktcmd2->data, 0, MAX_NET_CMDSIZE);
		pktcmd1->len = 0;
		pktcmd2->len = 0;
		pktcmd1->created_time = 0;
		pktcmd2->created_time = 0;

		pktcmd1 = pktcmd1->next;
		pktcmd2 = pktcmd2->next;
	}
}

/*
===================
Host_NetchanClean
===================
*/
void Host_NetchanClean(packetqueue_t *nc, int reconnect)
{
	Host_NetchanCleanQueue(nc);

	memset(nc->last_reliable, 0, MAX_NET_CMDSIZE);
	nc->last_reliable_len = 0;
	nc->outseq = 0;
	nc->outseq_ack = 0;
	if (!reconnect) /* keep the id */
	{
		nc->dest_id1 = 0;
		nc->dest_id2 = 0;
	}

	nc->inseq = 0;
	if (!reconnect) /* prevent instant timeout */
		nc->last_received_time = Sys_Time(); /* TODO: is this right? */
}

/*
===================
Host_NetchanCreate
===================
*/
packetqueue_t *Host_NetchanCreate(void)
{
	int i;
	packetqueue_t	*newnc;
	packetcmd_t		*pktcmd1, *pktcmd2;

	newnc = Sys_MemAlloc(&std_mem, sizeof(packetqueue_t), "netchan");

	newnc->last_reliable = Sys_MemAlloc(&std_mem, MAX_NETCHAN_DATASIZE, "netchan");

	/* create the packet cmd queues */
	newnc->reliable = Sys_MemAlloc(&std_mem, sizeof(packetcmd_t), "netchan");
	newnc->reliable->data = Sys_MemAlloc(&std_mem, MAX_NET_CMDSIZE, "netchan");
	pktcmd1 = newnc->reliable;

	newnc->unreliable = Sys_MemAlloc(&std_mem, sizeof(packetcmd_t), "netchan");
	newnc->unreliable->data = Sys_MemAlloc(&std_mem, MAX_NET_CMDSIZE, "netchan");
	pktcmd2 = newnc->unreliable;

	for (i = 1; i < MAX_NET_BUFFEREDCMDS; i++)
	{
		pktcmd1->next = Sys_MemAlloc(&std_mem, sizeof(packetcmd_t), "netchan");
		pktcmd1->next->data = Sys_MemAlloc(&std_mem, MAX_NET_CMDSIZE, "netchan");
		pktcmd1 = pktcmd1->next;

		pktcmd2->next = Sys_MemAlloc(&std_mem, sizeof(packetcmd_t), "netchan");
		pktcmd2->next->data = Sys_MemAlloc(&std_mem, MAX_NET_CMDSIZE, "netchan");
		pktcmd2 = pktcmd2->next;
	}

	/* make the buffers circular */
	pktcmd1->next = newnc->reliable;
	pktcmd2->next = newnc->unreliable;

	return newnc;
}

/*
============================================================================

Wrappers to write to a message buffer

Be sure that *msg is at least as big as MAX_NET_CMDSIZE
Be also careful with the output for MSG_ReadString, no checks are performed

TODO: endianness, guarantee data sizes

============================================================================
*/

/*
===================
MSG_WriteEntity
===================
*/
void MSG_WriteEntity(char *msg, int *len, const short data)
{
	MSG_WriteShort(msg, len, data);
}

/*
===================
MSG_WritePrecache
===================
*/
void MSG_WritePrecache(char *msg, int *len, const short data)
{
	MSG_WriteShort(msg, len, data);
}

/*
===================
MSG_WriteTime
===================
*/
void MSG_WriteTime(char *msg, int *len, const double data)
{
	MSG_WriteDouble(msg, len, data);
}

/*
===================
MSG_WriteByte
===================
*/
void MSG_WriteByte(char *msg, int *len, const unsigned char data)
{
	if (*len + sizeof(unsigned char) > MAX_NET_CMDSIZE)
	{
		Sys_Printf("MSG_WriteByte: Buffer overflow!\n"); /* TODO: hope it's not a reliable */
		return;
	}

	msg[*len] = data;
	(*len)++;
}

/*
===================
MSG_WriteShort
===================
*/
void MSG_WriteShort(char *msg, int *len, const short data)
{
	short *dest = (short *)(msg + *len);
	if (*len + sizeof(short) > MAX_NET_CMDSIZE)
	{
		Sys_Printf("MSG_WriteShort: Buffer overflow!\n"); /* TODO: hope it's not a reliable */
		return;
	}

	dest[0] = data;

	(*len) += sizeof(short);
}

/*
===================
MSG_WriteInt
===================
*/
void MSG_WriteInt(char *msg, int *len, const int data)
{
	int *dest = (int *)(msg + *len);
	if (*len + sizeof(int) > MAX_NET_CMDSIZE)
	{
		Sys_Printf("MSG_WriteInt: Buffer overflow!\n"); /* TODO: hope it's not a reliable */
		return;
	}

	dest[0] = data;

	(*len) += sizeof(int);
}

/*
===================
MSG_WriteDouble
===================
*/
void MSG_WriteDouble(char *msg, int *len, const double data)
{
	double *dest = (double *)(msg + *len);
	if (*len + sizeof(double) > MAX_NET_CMDSIZE)
	{
		Sys_Printf("MSG_WriteDouble: Buffer overflow!\n"); /* TODO: hope it's not a reliable */
		return;
	}

	dest[0] = data;

	(*len) += sizeof(double);
}

/*
===================
MSG_WriteVec1
===================
*/
void MSG_WriteVec1(char *msg, int *len, const vec_t data)
{
	vec_t *dest = (vec_t *)(msg + *len);
	if (*len + sizeof(vec_t) > MAX_NET_CMDSIZE)
	{
		Sys_Printf("MSG_WriteVec1: Buffer overflow!\n"); /* TODO: hope it's not a reliable */
		return;
	}

	dest[0] = data;

	(*len) += sizeof(vec_t);
}

/*
===================
MSG_WriteVec3
===================
*/
void MSG_WriteVec3(char *msg, int *len, const vec3_t data)
{
	vec_t *dest = (vec_t *)(msg + *len);
	if (*len + sizeof(vec_t) * 3 > MAX_NET_CMDSIZE)
	{
		Sys_Printf("MSG_WriteVec3: Buffer overflow!\n"); /* TODO: hope it's not a reliable */
		return;
	}

	dest[0] = data[0];
	dest[1] = data[1];
	dest[2] = data[2];

	(*len) += sizeof(vec_t) * 3;
}

/*
===================
MSG_WriteString

TODO: overflows???
===================
*/
void MSG_WriteString(char *msg, int *len, const char *data)
{
	char *dest = msg + *len;
	if (*len + strlen(data) + 1 > MAX_NET_CMDSIZE)
	{
		Sys_Printf("MSG_WriteString: Buffer overflow!\n"); /* TODO: hope it's not a reliable */
		return;
	}

	while (1)
	{
		*dest = *data;
		(*len)++;
		if (*dest == 0)
			break;

		dest++;
		data++;
	}
}

/*
===================
MSG_ReadEntity
===================
*/
void MSG_ReadEntity(char *msg, int *len, unsigned int maxlen, short *data)
{
	MSG_ReadShort(msg, len, maxlen, data);
}

/*
===================
MSG_ReadPrecache
===================
*/
void MSG_ReadPrecache(char *msg, int *len, unsigned int maxlen, short *data)
{
	MSG_ReadShort(msg, len, maxlen, data);
}

/*
===================
MSG_ReadTime
===================
*/
void MSG_ReadTime(char *msg, int *len, unsigned int maxlen, double *data)
{
	MSG_ReadDouble(msg, len, maxlen, data);
}

/*
===================
MSG_ReadByte
===================
*/
void MSG_ReadByte(char *msg, int *len, unsigned int maxlen, unsigned char *data)
{
	if (*len + sizeof(unsigned char) > maxlen)
	{
		Sys_Printf("MSG_ReadByte: Buffer underflow!\n"); /* TODO: hope it's not a reliable */
		*data = 0;
		return;
	}

	*data = msg[*len];
	(*len)++;
}

/*
===================
MSG_ReadShort
===================
*/
void MSG_ReadShort(char *msg, int *len, unsigned int maxlen, short *data)
{
	short *src = (short *)(msg + *len);
	if (*len + sizeof(short) > maxlen)
	{
		Sys_Printf("MSG_ReadShort: Buffer underflow!\n"); /* TODO: hope it's not a reliable */
		*data = 0;
		return;
	}

	data[0] = src[0];

	(*len) += sizeof(short);
}

/*
===================
MSG_ReadInt
===================
*/
void MSG_ReadInt(char *msg, int *len, unsigned int maxlen, int *data)
{
	int *src = (int *)(msg + *len);
	if (*len + sizeof(int) > maxlen)
	{
		Sys_Printf("MSG_ReadInt: Buffer underflow!\n"); /* TODO: hope it's not a reliable */
		*data = 0;
		return;
	}

	data[0] = src[0];

	(*len) += sizeof(int);
}

/*
===================
MSG_ReadDouble
===================
*/
void MSG_ReadDouble(char *msg, int *len, unsigned int maxlen, double *data)
{
	double *src = (double *)(msg + *len);
	if (*len + sizeof(double) > maxlen)
	{
		Sys_Printf("MSG_ReadDouble: Buffer underflow!\n"); /* TODO: hope it's not a reliable */
		*data = 0;
		return;
	}

	data[0] = src[0];

	(*len) += sizeof(double);
}

/*
===================
MSG_ReadVec1
===================
*/
void MSG_ReadVec1(char *msg, int *len, unsigned int maxlen, vec_t *data)
{
	vec_t *src = (vec_t *)(msg + *len);
	if (*len + sizeof(vec_t) > maxlen)
	{
		Sys_Printf("MSG_ReadVec1: Buffer underflow!\n"); /* TODO: hope it's not a reliable */
		data[0] = 0;
		return;
	}

	data[0] = src[0];

	(*len) += sizeof(vec_t);
}

/*
===================
MSG_ReadVec3
===================
*/
void MSG_ReadVec3(char *msg, int *len, unsigned int maxlen, vec3_t data)
{
	vec_t *src = (vec_t *)(msg + *len);
	if (*len + sizeof(vec_t) * 3 > maxlen)
	{
		Sys_Printf("MSG_ReadVec3: Buffer underflow!\n"); /* TODO: hope it's not a reliable */
		data[0] = 0;
		data[1] = 0;
		data[2] = 0;
		return;
	}

	data[0] = src[0];
	data[1] = src[1];
	data[2] = src[2];

	(*len) += sizeof(vec_t) * 3;
}

/*
===================
MSG_ReadString
===================
*/
void MSG_ReadString(char *msg, int *len, unsigned int maxlen, char *data, int datamaxsize)
{
	char *src = msg + *len;
	int cursize = 0;

	if (datamaxsize < 1)
		Host_Error("MSG_ReadString: datamaxsize < 1\n");

	while (1)
	{
		if (src == msg + maxlen)
		{
			Sys_Printf("MSG_ReadString: Buffer underflow!\n"); /* TODO: hope it's not a reliable */
			*data = 0;
			return;
		}

		if (cursize >= datamaxsize)
		{
			Sys_Printf("MSG_ReadString: Buffer overflow!\n"); /* TODO: hope it's not a reliable */
			*(data - 1) = 0; /* at least one character will have been written when we get here, so it's okay to do this */
			return;
		}

		*data = *src;
		(*len)++;
		if (*src == 0)
			break;

		src++;
		data++;
		cursize++;
	}
}
