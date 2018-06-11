/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

World Voxel Server-side Management

============================================================================
*/

/* TODO: if we keep queuing up stuff, old stuff won't ever get sent. Use a real queue, not a stack! */
/* TODO: do this before client finishes loading, so that he sees a loading screen! */
#define MAX_CHUNK_BYTES_SENT_PER_PACKET		(MAX_NET_CMDSIZE - sizeof(int) * 2 - 3) /* index and start position get sent as int, -3 is a safeguard */
#define MAX_PACKETS_LEFT_AFTER_QUEUEING		128 /* arbitrary FIXME tune */
int sv_voxel_chunk_queue[MAX_CLIENTS][VOXEL_MAX_CHUNKS];
int sv_voxel_chunk_queue_num[MAX_CLIENTS];
int sv_voxel_chunk_queue_progress[MAX_CLIENTS];
int sv_voxel_block_queue[MAX_CLIENTS][VOXEL_MAX_CHUNKS][4]; /* x, y, z, type (type as int) */ /* TODO: do not use VOXEL_MAX_CHUNKS here */
int sv_voxel_block_queue_num[MAX_CLIENTS];

/*
===================
SV_VoxelQueueToNewClient

When a new client connects, he must receive all the blocks in his immediate area
===================
*/
void SV_VoxelQueueToNewClient(int slot)
{
	int i;
	const model_voxel_t *voxel = Host_VoxelGetData();

	if (!svs.sv_clients[slot].connected)
		Host_Error("SV_VoxelSendToNewClient: client isn't connected (slot %d)\n", slot);

	if (sv_voxel_chunk_queue_num[slot] == VOXEL_MAX_CHUNKS)
		Host_Error("SV_VoxelSendToNewClient: queue full\n");

	/*  this is the only place were we reset this data */
	sv_voxel_chunk_queue_num[slot] = 0;
	sv_voxel_chunk_queue_progress[slot] = 0; /* progress for chunk in the front of the queue */
	sv_voxel_block_queue_num[slot] = 0;

	for (i = voxel->num_chunks - 1; i >= 0; i--) /* send them in order */
	{
		/* TODO: send chunks closer to player first */
		sv_voxel_chunk_queue[slot][sv_voxel_chunk_queue_num[slot]++] = i;
	}
}

/*
===================
SV_VoxelQueueSendPartial

TODO: minimize data sent to clients (compression, removing dupes,
removing more than one update to the same block in the same frame,
etc...

TODO: do not send if not in clients "visible" area, send all visible
chunks to client when he connects.

TODO: relationship of this with server stats/baselines (stuff that
gets sent to all connecting players and updated as they change
to all players as reliables (scores, etc...)

TODO: slow
===================
*/
void SV_VoxelQueueSendPartial(int slot)
{
	char msg[MAX_NET_CMDSIZE];
	int len;
	int i;
	const model_voxel_t *voxel = Host_VoxelGetData();
	int packetsleft = Host_NetchanCommandsLeft(svs.sv_clients[slot].netconn, NET_CMD_RELIABLE);

	if (!svs.sv_clients[slot].connected)
		Host_Error("SV_VoxelQueueSendPartial: client isn't connected (slot %d)\n", slot);

	while (sv_voxel_chunk_queue_num[slot])
	{
		int bytes_sent;
		int bytes_left = VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z - sv_voxel_chunk_queue_progress[slot];
		unsigned char *start_address;

		if (packetsleft < MAX_PACKETS_LEFT_AFTER_QUEUEING)
			return; /* cmd queue too full */

		/* decide how many bytes we will send now */
		if (bytes_left > MAX_CHUNK_BYTES_SENT_PER_PACKET)
			bytes_sent = MAX_CHUNK_BYTES_SENT_PER_PACKET;
		else
			bytes_sent = bytes_left;

		/* if at the start of a chunk, get space for sending the coordinates */
		if (sv_voxel_chunk_queue_progress[slot] == 0)
			bytes_sent -= sizeof(int) * 3;

		/* hoping to have the data structures in the same way on both ends FIXME */
		len = 0;
		MSG_WriteByte(msg, &len, SVC_VOXELCHUNKPART);
		MSG_WriteInt(msg, &len, sv_voxel_chunk_queue[slot][sv_voxel_chunk_queue_num[slot] - 1]); /* TODO: other places where I forgot this -1 */
		MSG_WriteInt(msg, &len, sv_voxel_chunk_queue_progress[slot]);
		/* start of a chunk? send coordinates */
		if (sv_voxel_chunk_queue_progress[slot] == 0)
		{
			MSG_WriteInt(msg, &len, voxel->chunklist[sv_voxel_chunk_queue[slot][sv_voxel_chunk_queue_num[slot] - 1]]->origin[0]);
			MSG_WriteInt(msg, &len, voxel->chunklist[sv_voxel_chunk_queue[slot][sv_voxel_chunk_queue_num[slot] - 1]]->origin[1]);
			MSG_WriteInt(msg, &len, voxel->chunklist[sv_voxel_chunk_queue[slot][sv_voxel_chunk_queue_num[slot] - 1]]->origin[2]);
		}
		MSG_WriteByte(msg, &len, bytes_sent); /* TODO FIXME: this fails if > 256 bytes are sent (e.g.: big MAX_NET_CMDSIZE) */
		start_address = (unsigned char *)voxel->chunklist[sv_voxel_chunk_queue[slot][sv_voxel_chunk_queue_num[slot] - 1]]->blocks + (sv_voxel_chunk_queue_progress[slot]);
		for (i = 0; i < bytes_sent; i++)
			MSG_WriteByte(msg, &len, *(start_address + i));
		Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, msg, len, NET_CMD_RELIABLE); /* reliable, not much harm done in this case */

		sv_voxel_chunk_queue_progress[slot] += bytes_sent;

		if (sv_voxel_chunk_queue_progress[slot] == VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z)
		{
			sv_voxel_chunk_queue_num[slot]--;
			sv_voxel_chunk_queue_progress[slot] = 0;
		}

		packetsleft--;
	}

	if (!sv_voxel_chunk_queue_num[slot]) /* only start sending updates after all chunks have been sent TODO: still problems with stuff that changes while in queue (double updates causing flickering because the new state was sent earlier, etc) */
	{
		while (sv_voxel_block_queue_num[slot])
		{
			if (packetsleft < MAX_PACKETS_LEFT_AFTER_QUEUEING)
				return; /* cmd queue too full */

			len = 0;
			MSG_WriteByte(msg, &len, SVC_VOXELBLOCK);
			MSG_WriteInt(msg, &len, sv_voxel_block_queue[slot][sv_voxel_block_queue_num[slot] - 1][0]);
			MSG_WriteInt(msg, &len, sv_voxel_block_queue[slot][sv_voxel_block_queue_num[slot] - 1][1]);
			MSG_WriteInt(msg, &len, sv_voxel_block_queue[slot][sv_voxel_block_queue_num[slot] - 1][2]);
			MSG_WriteByte(msg, &len, (unsigned char)sv_voxel_block_queue[slot][sv_voxel_block_queue_num[slot] - 1][3]);

			/* TODO: revisit all the engine code and drop connection when reliable queueing fails! */
			Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, msg, len, NET_CMD_RELIABLE); /* reliable, not much harm done in this case */

			sv_voxel_block_queue_num[slot]--;
			packetsleft--;
		}
	}
}

/*
============================================================================

World Voxel Server-side Management Game Interface

============================================================================
*/

/*
===================
SV_VoxelSetChunk

Updates the type of all blocks in a chunk and warns clients

"type" must have a size of least VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z

This function should only be called during loading, because it doesn't
send neighboring chunks to the update queue (because they will ALL need update)

TODO: minimize data sent to clients (compression, removing dupes,
removing more than one update to the same block in the same frame,
etc...

TODO: do not send if not in clients "visible" area, send all visible
chunks to client when he connects.

TODO: relationship of this with server stats/baselines (stuff that
gets sent to all connecting players and updated as they change
to all players as reliables (scores, etc...)
===================
*/
void SV_VoxelSetChunk(int chunkoriginx, int chunkoriginy, int chunkoriginz, unsigned char *type)
{
	int i, updatedchunk;

	if (!svs.loading)
		Host_Error("Host_VoxelSetChunk: only valid during loading\n");

	updatedchunk = Host_VoxelSetChunk(chunkoriginx, chunkoriginy, chunkoriginz, type);

	if (updatedchunk == -1)
		return;

	/* inform clients TODO: make sure that there are no clients connected when we are filling the initial world with this function - this report protocol is slow for large modifications */
	for (i = 0; i < MAX_CLIENTS; i++)
		if (svs.sv_clients[i].ingame)
		{
			if (sv_voxel_chunk_queue_num[i] == VOXEL_MAX_CHUNKS)
				Host_Error("SV_VoxelSetChunk: queue full\n");

			/*
				Send absolute values to let the chunk indices be different
				between client and server (but with the current code, they
				are the same.
			*/
			sv_voxel_chunk_queue[i][sv_voxel_chunk_queue_num[i]++] = updatedchunk; /* TODO: check dupes in queue */
		}
}

/*
===================
SV_VoxelSetBlock

Updates the type of a block and warns clients

TODO: minimize data sent to clients (compression, removing dupes,
removing more than one update to the same block in the same frame,
etc...

TODO: do not send if not in clients "visible" area, send all visible
chunks to client when he connects.

TODO: relationship of this with server stats/baselines (stuff that
gets sent to all connecting players and updated as they change
to all players as reliables (scores, etc...)
===================
*/
void SV_VoxelSetBlock(int absx, int absy, int absz, unsigned char type)
{
	int i;

	if (!Host_VoxelSetBlock(absx, absy, absz, type))
		return;

	/* inform clients TODO: make sure that there are no clients connected when we are filling the initial world with this function - this report protocol is slow for large modifications */
	for (i = 0; i < MAX_CLIENTS; i++)
		if (svs.sv_clients[i].ingame)
		{
			if (sv_voxel_block_queue_num[i] == VOXEL_MAX_CHUNKS)
				Host_Error("SV_VoxelSetBlock: queue full\n");

			/*
				Send absolute values to let the chunk indices be different
				between client and server (but with the current code, they
				are the same.
			*/
			sv_voxel_block_queue[i][sv_voxel_block_queue_num[i]][0] = absx;
			sv_voxel_block_queue[i][sv_voxel_block_queue_num[i]][1] = absy;
			sv_voxel_block_queue[i][sv_voxel_block_queue_num[i]][2] = absz;
			sv_voxel_block_queue[i][sv_voxel_block_queue_num[i]++][3] = type; /* TODO: check dupes in queue then update type */
		}
}

/*
===================
SV_VoxelPointContents

TODO: implement for sphere and aabbox? makes sense?
TODO: do this for entities and submodels (because we may have a water brush inside a entity in the map
TODO: test this
===================
*/
int SV_VoxelPointContents(const vec3_t point)
{
	int i, contents, absindex[3], chunkpos[3], blockpos[3];
	model_voxel_t *data = Host_VoxelGetData();

	if (!svs.listening)
		Host_Error("Sys_ModelPointContentsVoxel: server not listening!\n");

	contents = 0; /* empty */
	/* find the chunk the point is in TODO: optimize */
	for (i = 0; i < data->num_chunks; i++)
	{
		vec3_t box_mins;
		vec3_t box_maxs;

		/* TODO: check if this is right */
		Math_Vector3Set(box_mins, (vec_t)(data->chunklist[i]->origin[0] * VOXEL_CHUNK_SIZE_X * VOXEL_SIZE_X - VOXEL_SIZE_X_2), (vec_t)(data->chunklist[i]->origin[1] * VOXEL_CHUNK_SIZE_Y * VOXEL_SIZE_Y - VOXEL_SIZE_Y_2), (vec_t)(data->chunklist[i]->origin[2] * VOXEL_CHUNK_SIZE_Z * VOXEL_SIZE_Z - VOXEL_SIZE_Z_2));
		Math_Vector3Set(box_maxs, (vec_t)(data->chunklist[i]->origin[0] * VOXEL_CHUNK_SIZE_X * VOXEL_SIZE_X + VOXEL_CHUNK_SIZE_X * VOXEL_SIZE_X - VOXEL_SIZE_X_2), (vec_t)(data->chunklist[i]->origin[1] * VOXEL_CHUNK_SIZE_Y * VOXEL_SIZE_Y + VOXEL_CHUNK_SIZE_Y * VOXEL_SIZE_Y - VOXEL_SIZE_Y_2), (vec_t)(data->chunklist[i]->origin[2] * VOXEL_CHUNK_SIZE_Z * VOXEL_SIZE_Z + VOXEL_CHUNK_SIZE_Z * VOXEL_SIZE_Z - VOXEL_SIZE_Z_2));
		if (Math_PointInsideBox(point, box_mins, box_maxs))
			break;
	}

	if (i == data->num_chunks)
		return 0; /* it's empty for this model at least! TODO: other models, entities, etc */

	/* convert the point to the unitary indexes of the chunk positions, this will make it so that the maxs are an open interval, to avoid a point being in two adjacent boxes */
	absindex[0] = (int)floor(point[0]);
	absindex[1] = (int)floor(point[1]);
	absindex[2] = (int)floor(point[2]);
	Host_VoxelDecomposeAbsCoord(absindex, blockpos, chunkpos);

	if (blockpos[0] < 0 || blockpos[0] >= VOXEL_CHUNK_SIZE_X ||
		blockpos[1] < 0 || blockpos[1] >= VOXEL_CHUNK_SIZE_Y ||
		blockpos[2] < 0 || blockpos[2] >= VOXEL_CHUNK_SIZE_Z)
		Host_Error("Sys_ModelPointContentsVoxel: invalid blockpos: point %f %f %f inside chunk %d %d %d, block %d %d %d\n", point[0], point[1], point[2], data->chunklist[i]->origin[0], data->chunklist[i]->origin[1], data->chunklist[i]->origin[2], blockpos[0], blockpos[1], blockpos[2]);

	/* TODO CONSOLEDEBUG
		Sys_Printf("Point %f %f %f inside chunk %d, block %d %d %d\n", point[0], point[1], point[2], i, blockpos[0], blockpos[1], blockpos[2]);
	*/

	/* TODO: other content types that matter for external stuff */
	if (data->chunklist[i]->blocks[blockpos[0]][blockpos[1]][blockpos[2]].type != VOXEL_BLOCKTYPE_EMPTY)
		contents |= CONTENTS_SOLID_BIT;

	return contents;
}
