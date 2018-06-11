/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

World Voxel Management Shared by Client and Server

Voxels are separate from other models for a number or reasons, but mainly
because they must have updateable data and only one "voxel model" may
exist per server. (Decompositing voxels into free blocks in physics code
would mean creating entities, not new voxel models)

Only one world voxel model at a time, may be shared by a local client-server
pair, changes will be propagated over network for remote clients.

All allocated memory must be on mdl_mem, which means that Host_CleanModels
will free it.

TODO: server send to client which chunks are visible, use as basis for
not generating vbos, having a visibility list, frustum culling, empty chunk
ignoring, etc

TODO: water fluids/pressure (block has water height X and it flood fills)
TODO: lots of instances of host calling sv/cl here

============================================================================
*/

static void *chunk_map; /* mapping from chunkcoord to chunkindex */
static model_voxel_t		*voxelworld = NULL;

/*
	these are used as temporary containers for creating VBOs, they may be
	overwritten after the VBO is created
*/
/* TODO: textures */
static model_vertex_t rendervertices[VOXEL_CHUNK_MAX_VERTICES]; /* TODO: still means to occupy less space, join triangles? remove color component? */
static unsigned int rendernumvertices;
static unsigned int rendertrianglesverts[VOXEL_CHUNK_MAX_TRIANGLESVERTS];
static unsigned int rendernumtrianglesverts;

#ifdef CHUNKS_AS_BOXES
/*
	these are used as temporary containers for creating physical chunks,
	they may be overwritten after the physical chunk is created
*/
static vec3_t physorigins[VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z]; /* TODO: still means to occupy less space, join boxes */
static unsigned int physnumboxes;
static vec3_t physhalfsize;
#endif /* CHUNKS_AS_BOXES */

/* coordinates for the texture coordinates in the texture atlas TODO: FIX THIS MESS */
/* TODO: get rid of these four and do it 0-1? (see about half-pixel correction then) */
#define TEXTURE_SIZE_X					64
#define TEXTURE_SIZE_Y					64
#define VOXEL_ATLAS_WIDTH				768
#define VOXEL_ATLAS_HEIGHT				256

#define BLOCK_SIDES_X					3
#define BLOCK_SIDES_Y					1

#define TEXTURE_MAX_X_IDX				(VOXEL_ATLAS_WIDTH / TEXTURE_SIZE_X / BLOCK_SIDES_X)
#define TEXTURE_X_POS(linearidx)		((TEXTURE_SIZE_X * BLOCK_SIDES_X * linearidx) % VOXEL_ATLAS_WIDTH)
#define TEXTURE_MAX_Y_IDX				(VOXEL_ATLAS_HEIGHT / TEXTURE_SIZE_Y / BLOCK_SIDES_Y)
#define TEXTURE_Y_POS(linearidx)		((TEXTURE_SIZE_Y * BLOCK_SIDES_Y * (linearidx / TEXTURE_MAX_X_IDX)) % VOXEL_ATLAS_HEIGHT)

/* TODO: custom mipmapping to avoid blending */
/* TODO: some blocktypes may need two different textures, for example, the lateral "merging" texture between grass and dirt on the top block of a pile */
/*
	Calculate everything as int, then convert to vec_t to do half-pixel correction
	and have the coords at the center of the texels to prevend bleeding.
	Remember to use ((vec_t)TEXTURE_SIZE_X - 1.0) / (vec_t)VOXEL_ATLAS_WIDTH and
	((vec_t)TEXTURE_SIZE_Y - 1.0) / (vec_t)VOXEL_ATLAS_HEIGHT when setting the
	other corners of the coordinates, for the full effect of half-pixel correction
*/
#define TEXTURESIDE_POS_X(tex_x, side)	(((vec_t)(tex_x + ((side % BLOCK_SIDES_X) * TEXTURE_SIZE_X)) + 0.5f) / (vec_t)VOXEL_ATLAS_WIDTH)
#define TEXTURESIDE_POS_Y(tex_y, side)	(((vec_t)(tex_y + ((side / BLOCK_SIDES_X) * TEXTURE_SIZE_Y)) + 0.5f) / (vec_t)VOXEL_ATLAS_HEIGHT)

#if ((VOXEL_ATLAS_WIDTH / TEXTURE_SIZE_X / BLOCK_SIDES_X) * (VOXEL_ATLAS_HEIGHT / TEXTURE_SIZE_Y / BLOCK_SIDES_Y) != VOXEL_BLOCKTYPE_MAX)
#error "Please adjust the voxel texturing constants, they are out of sync! Check block_texture_st_coords_* too!"
#endif

static const vec_t block_texture_st_coords_top[VOXEL_BLOCKTYPE_MAX][2] =
{
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(0), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(0), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(1), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(1), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(2), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(2), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(3), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(3), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(4), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(4), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(5), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(5), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(6), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(6), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(7), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(7), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(8), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(8), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(9), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(9), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(10), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(10), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(11), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(11), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(12), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(12), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(13), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(13), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(14), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(14), 0)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(15), 0), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(15), 0)}
};

static const vec_t block_texture_st_coords_laterals[VOXEL_BLOCKTYPE_MAX][2] =
{
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(0), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(0), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(1), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(1), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(2), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(2), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(3), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(3), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(4), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(4), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(5), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(5), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(6), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(6), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(7), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(7), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(8), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(8), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(9), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(9), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(10), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(10), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(11), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(11), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(12), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(12), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(13), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(13), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(14), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(14), 1)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(15), 1), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(15), 1)}
};

static const vec_t block_texture_st_coords_bottom[VOXEL_BLOCKTYPE_MAX][2] =
{
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(0), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(0), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(1), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(1), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(2), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(2), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(3), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(3), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(4), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(4), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(5), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(5), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(6), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(6), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(7), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(7), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(8), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(8), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(9), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(9), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(10), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(10), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(11), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(11), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(12), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(12), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(13), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(13), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(14), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(14), 2)},
	{TEXTURESIDE_POS_X(TEXTURE_X_POS(15), 2), TEXTURESIDE_POS_Y(TEXTURE_Y_POS(15), 2)}
};

/*
===================
Host_CleanVoxelByProxy

Should only be called by Host_CleanModels.
===================
*/
void Host_CleanVoxelsByProxy(void)
{
	if (!voxelworld)
		return;

	voxelworld = NULL;
	Host_UtilMapDestroyInt3ToInt(chunk_map);
	chunk_map = NULL;
}

/*
===================
Host_VoxelGetData

Gets the current structure
===================
*/
model_voxel_t *Host_VoxelGetData(void)
{
	return voxelworld;
}

/*
===================
Host_VoxelComposeAbsCoord

Gets an absolute 3-component index number from a chunk/block combination
===================
*/
void Host_VoxelComposeAbsCoord(model_voxel_chunk_t *chunk, int *blockpos, int *out)
{
	out[0] = chunk->origin[0] * VOXEL_CHUNK_SIZE_X + blockpos[0];
	out[1] = chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + blockpos[1];
	out[2] = chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + blockpos[2];
}

/*
===================
Host_VoxelDecomposeAbsCoord

Gets an chunk/block (both 3-component) combination from an absolute
3-component index number (not world coordinates)
===================
*/
void Host_VoxelDecomposeAbsCoord(int *absindex, int *outblock, int *outchunk)
{
	outchunk[0] = (int)floor((vec_t)absindex[0] / (vec_t)VOXEL_CHUNK_SIZE_X);
	outchunk[1] = (int)floor((vec_t)absindex[1] / (vec_t)VOXEL_CHUNK_SIZE_Y);
	outchunk[2] = (int)floor((vec_t)absindex[2] / (vec_t)VOXEL_CHUNK_SIZE_Z);
	outblock[0] = absindex[0] - (outchunk[0] * VOXEL_CHUNK_SIZE_X);
	outblock[1] = absindex[1] - (outchunk[1] * VOXEL_CHUNK_SIZE_Y);
	outblock[2] = absindex[2] - (outchunk[2] * VOXEL_CHUNK_SIZE_Z);
}

/*
===================
Host_VoxelGetChunk

Returns the chunk index for a given chunk coordinate
(not absolute or world coord) or -1 if no chunk is loaded in that coord

TODO: will it someday need to be faster?
===================
*/
int Host_VoxelGetChunk(int *chunkcoord)
{
	int chunkidx;

	if (!voxelworld)
		Host_Error("Host_VoxelGetChunk: voxel world not initialized\n");

	chunkidx = Host_UtilMapRetrieveInt3ToInt(chunk_map, chunkcoord);

	if (chunkidx < -1 || chunkidx >= VOXEL_MAX_CHUNKS)
		Host_Error("Host_VoxelGetChunk: invalid index %d\n", chunkidx);
	return chunkidx;
}

/*
===================
Host_VoxelQueueChunkUpdate

Puts a chunk in the update list, if it exists.
Returns true if it was added.

TODO: SLOW
===================
*/
int Host_VoxelQueueChunkUpdate(int chunkidx)
{
	int i;

	if (!voxelworld)
		Host_Error("Host_VoxelQueueChunkUpdate: voxel world not initialized\n");

	if (chunkidx < 0 || chunkidx > VOXEL_MAX_CHUNKS)
		Host_Error("Host_VoxelQueueChunkUpdate: chunkidx < 0 || chunkidx > VOXEL_MAX_CHUNKS: %d\n", chunkidx);

	for (i = 0; i < voxelworld->num_updated_chunks; i++)
		if (voxelworld->updated_chunks_list[i] == chunkidx) /* already in queue TODO: do this faster */
			return false; /* TODO: should I return true or false here? */

	if (voxelworld->num_updated_chunks == VOXEL_MAX_CHUNKS)
		Host_Error("Host_VoxelQueueChunkUpdate: too many chunks in queue!\n");

	voxelworld->updated_chunks_list[voxelworld->num_updated_chunks++] = chunkidx;
	/* TODO: flag for async update of chunks (also update neighboring chunks when smoothing terrain someday...) */

	return true;
}

/*
===================
Host_VoxelNewChunk

Returns a new chunk index for a given chunk coordinate (not world coordinates)
Returns -1 if the new chunk was out of bounds (going too high or too low, for example)
TODO: do unloading/save and unload if a chunk has no visible blocks?
===================
*/
int Host_VoxelNewChunk(int *chunkcoord)
{
	if (!voxelworld)
		Host_Error("SV_VoxelNewChunk: voxel world not created\n");

	if (chunkcoord[1] > VOXEL_MAX_CHUNK_Y)
		return -1; /* too high */
	if (chunkcoord[1] < VOXEL_MIN_CHUNK_Y)
		return -1; /* too low */

	/* see if this coord is already allocated */
	if (Host_VoxelGetChunk(chunkcoord) != -1)
		Host_Error("SV_VoxelNewChunk: chunk %d %d %d already allocated!\n", chunkcoord[0], chunkcoord[1], chunkcoord[2]);

	if (voxelworld->num_chunks == VOXEL_MAX_CHUNKS)
		Host_Error("SV_VoxelNewChunk: VOXEL_MAX_CHUNKS\n");

	/* Sys_MemAlloc returns zeroed memory */
	voxelworld->chunklist[voxelworld->num_chunks] = Sys_MemAlloc(&mdl_mem, sizeof(model_voxel_chunk_t), "voxelchunks");

	voxelworld->chunklist[voxelworld->num_chunks]->origin[0] = chunkcoord[0];
	voxelworld->chunklist[voxelworld->num_chunks]->origin[1] = chunkcoord[1];
	voxelworld->chunklist[voxelworld->num_chunks]->origin[2] = chunkcoord[2];

	Host_UtilMapInsertInt3ToInt(chunk_map, chunkcoord, voxelworld->num_chunks);

	/* VBO not created yet */
	voxelworld->chunklist[voxelworld->num_chunks]->vbo_id = -1;

	voxelworld->chunklist[voxelworld->num_chunks]->mins[0] = (chunkcoord[0] * VOXEL_CHUNK_SIZE_X) * VOXEL_SIZE_X - VOXEL_SIZE_X_2;
	voxelworld->chunklist[voxelworld->num_chunks]->mins[1] = (chunkcoord[1] * VOXEL_CHUNK_SIZE_Y) * VOXEL_SIZE_X - VOXEL_SIZE_Y_2;
	voxelworld->chunklist[voxelworld->num_chunks]->mins[2] = (chunkcoord[2] * VOXEL_CHUNK_SIZE_Z) * VOXEL_SIZE_X - VOXEL_SIZE_Z_2;
	voxelworld->chunklist[voxelworld->num_chunks]->maxs[0] = (chunkcoord[0] * VOXEL_CHUNK_SIZE_X + VOXEL_CHUNK_SIZE_X) * VOXEL_SIZE_X + VOXEL_SIZE_X_2;
	voxelworld->chunklist[voxelworld->num_chunks]->maxs[1] = (chunkcoord[1] * VOXEL_CHUNK_SIZE_Y + VOXEL_CHUNK_SIZE_Y) * VOXEL_SIZE_X + VOXEL_SIZE_Y_2;
	voxelworld->chunklist[voxelworld->num_chunks]->maxs[2] = (chunkcoord[2] * VOXEL_CHUNK_SIZE_Z + VOXEL_CHUNK_SIZE_Z) * VOXEL_SIZE_X + VOXEL_SIZE_Z_2;

	voxelworld->num_chunks++;

	return voxelworld->num_chunks - 1;
	/* no need to add this to the updated chunk lists now: its all empty, so it's already updated! */
}

/*
===================
Host_VoxelSetChunk

Should only be called by SV_VoxelSetChunk, because of updating.
Updates the type of all blocks in a chunk and sets a flag for updating it.
Allocates a new chunk if necessary, returns the index of the updated chunk,
or -1 if no updating took place.
This function should only be called during loading, because it doesn't
send neighboring chunks to the update queue (because they will ALL need update)

"type" must have a size of least VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z

TODO: do this faster
TODO: only called by server for now, remove from here?
===================
*/
int Host_VoxelSetChunk(int chunkoriginx, int chunkoriginy, int chunkoriginz, unsigned char *type)
{
	int i;
	int chunkidx;
	int chunkorigin[3];
	int updated;

	if (!voxelworld)
		Host_Error("Host_VoxelSetChunk: voxel world not created\n");

	if (!svs.loading)
		Host_Error("Host_VoxelSetChunk: only valid during loading\n");

	chunkorigin[0] = chunkoriginx;
	chunkorigin[1] = chunkoriginy;
	chunkorigin[2] = chunkoriginz;
	chunkidx = Host_VoxelGetChunk(chunkorigin);
	if (chunkidx == -1) /* chunk doesn't exist yet */
	{
		int has_not_empty = false;
		for (i = 0; i < VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z; i++)
			if (type[i] != VOXEL_BLOCKTYPE_EMPTY)
			{
				has_not_empty = true;
				break;
			}

		if (has_not_empty)
		{
			chunkidx = Host_VoxelNewChunk(chunkorigin);
			if (chunkidx == -1)
				return -1; /* out of bounds */
		}
		else
			return -1; /* do not create a new empty chunk */
	}

	/* TODO: isn't it better to just do a memcpy and assume updated == true? */
	updated = false;
	for (i = 0; i < VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z; i++)
		if (((unsigned char *)voxelworld->chunklist[chunkidx]->blocks)[i] != type[i])
		{
			updated = true;
			((unsigned char *)voxelworld->chunklist[chunkidx]->blocks)[i] = type[i];
		}

	if (!updated)
		return -1; /* all blocks are equal */

	if (Host_VoxelQueueChunkUpdate(chunkidx))
		return chunkidx;
	else
		return -1; /* already in queue TODO: should we return chunkidx here? */
}

/*
===================
Host_VoxelSetBlock

Should only be called by CL_ParseVoxelBlock and SV_VoxelSetBlock,
because of updating.
Updates the type of a block and sets a flag for updating its chunk.
Allocates a new chunk if necessary and sends neighboring chunks to
the update list if necessary too.
Returns true if the setting was succesful, false if out of bounds, etc...
TODO: do this faster (currently doing one lookup for each block!)
===================
*/
int Host_VoxelSetBlock(int absx, int absy, int absz, unsigned char type)
{
	int destabs[3], destchunk[3], destblock[3];
	int chunkidx;

	if (!voxelworld)
		Host_Error("SV_VoxelSetBlock: voxel world not created\n");

	destabs[0] = absx;
	destabs[1] = absy;
	destabs[2] = absz;

	Host_VoxelDecomposeAbsCoord(destabs, destblock, destchunk);
	chunkidx = Host_VoxelGetChunk(destchunk);
	if (chunkidx == -1) /* chunk doesn't exist yet */
	{
		if (type != VOXEL_BLOCKTYPE_EMPTY)
		{
			chunkidx = Host_VoxelNewChunk(destchunk);
			if (chunkidx == -1)
				return false; /* out of bounds */
		}
		else
			return false; /* do not create a new chunk for an empty block */
	}

	if (voxelworld->chunklist[chunkidx]->blocks[destblock[0]][destblock[1]][destblock[2]].type == type)
		return false; /* already at that type */

	voxelworld->chunklist[chunkidx]->blocks[destblock[0]][destblock[1]][destblock[2]].type = type;

	Host_VoxelQueueChunkUpdate(chunkidx);

	/* TODO: is this enough? (think about transparency, etc) */
	/* send neighboring chunks to the update list, if necessary (we [un]covered one of their faces, etc) */
	if (destblock[2] == VOXEL_CHUNK_SIZE_Z - 1) /* front */
	{
		int nextchunk[3], nextchunkidx;

		nextchunk[0] = voxelworld->chunklist[chunkidx]->origin[0];
		nextchunk[1] = voxelworld->chunklist[chunkidx]->origin[1];
		nextchunk[2] = voxelworld->chunklist[chunkidx]->origin[2] + 1;
		nextchunkidx = Host_VoxelGetChunk(nextchunk);
		if (nextchunkidx != -1 && voxelworld->chunklist[nextchunkidx]->blocks[destblock[0]][destblock[1]][0].type != VOXEL_BLOCKTYPE_EMPTY)
			Host_VoxelQueueChunkUpdate(nextchunkidx);
	}
	if (destblock[2] == 0) /* back */
	{
		int nextchunk[3], nextchunkidx;

		nextchunk[0] = voxelworld->chunklist[chunkidx]->origin[0];
		nextchunk[1] = voxelworld->chunklist[chunkidx]->origin[1];
		nextchunk[2] = voxelworld->chunklist[chunkidx]->origin[2] - 1;
		nextchunkidx = Host_VoxelGetChunk(nextchunk);
		if (nextchunkidx != -1 && voxelworld->chunklist[nextchunkidx]->blocks[destblock[0]][destblock[1]][VOXEL_CHUNK_SIZE_Z - 1].type != VOXEL_BLOCKTYPE_EMPTY)
			Host_VoxelQueueChunkUpdate(nextchunkidx);
	}
	if (destblock[0] == VOXEL_CHUNK_SIZE_X - 1) /* right */
	{
		int nextchunk[3], nextchunkidx;

		nextchunk[0] = voxelworld->chunklist[chunkidx]->origin[0] + 1;
		nextchunk[1] = voxelworld->chunklist[chunkidx]->origin[1];
		nextchunk[2] = voxelworld->chunklist[chunkidx]->origin[2];
		nextchunkidx = Host_VoxelGetChunk(nextchunk);
		if (nextchunkidx != -1 && voxelworld->chunklist[nextchunkidx]->blocks[0][destblock[1]][destblock[2]].type != VOXEL_BLOCKTYPE_EMPTY)
			Host_VoxelQueueChunkUpdate(nextchunkidx);
	}
	if (destblock[0] == 0) /* left */
	{
		int nextchunk[3], nextchunkidx;

		nextchunk[0] = voxelworld->chunklist[chunkidx]->origin[0] - 1;
		nextchunk[1] = voxelworld->chunklist[chunkidx]->origin[1];
		nextchunk[2] = voxelworld->chunklist[chunkidx]->origin[2];
		nextchunkidx = Host_VoxelGetChunk(nextchunk);
		if (nextchunkidx != -1 && voxelworld->chunklist[nextchunkidx]->blocks[VOXEL_CHUNK_SIZE_X - 1][destblock[1]][destblock[2]].type != VOXEL_BLOCKTYPE_EMPTY)
			Host_VoxelQueueChunkUpdate(nextchunkidx);
	}
	if (destblock[1] == VOXEL_CHUNK_SIZE_Y - 1) /* top */
	{
		int nextchunk[3], nextchunkidx;

		nextchunk[0] = voxelworld->chunklist[chunkidx]->origin[0];
		nextchunk[1] = voxelworld->chunklist[chunkidx]->origin[1] + 1;
		nextchunk[2] = voxelworld->chunklist[chunkidx]->origin[2];
		nextchunkidx = Host_VoxelGetChunk(nextchunk);
		if (nextchunkidx != -1 && voxelworld->chunklist[nextchunkidx]->blocks[destblock[0]][0][destblock[2]].type != VOXEL_BLOCKTYPE_EMPTY)
			Host_VoxelQueueChunkUpdate(nextchunkidx);
	}
	if (destblock[1] == 0) /* bottom */
	{
		int nextchunk[3], nextchunkidx;

		nextchunk[0] = voxelworld->chunklist[chunkidx]->origin[0];
		nextchunk[1] = voxelworld->chunklist[chunkidx]->origin[1] - 1;
		nextchunk[2] = voxelworld->chunklist[chunkidx]->origin[2];
		nextchunkidx = Host_VoxelGetChunk(nextchunk);
		if (nextchunkidx != -1 && voxelworld->chunklist[nextchunkidx]->blocks[destblock[0]][VOXEL_CHUNK_SIZE_Y - 1][destblock[2]].type != VOXEL_BLOCKTYPE_EMPTY)
			Host_VoxelQueueChunkUpdate(nextchunkidx);
	}

	return true;
}

/*
===================
Host_VoxelCommitUpdates

Recreates cached data for all chunks in queue for this
TODO: async, there may be MANY updated chunks in a queue
TODO: use atlas for textures
TODO: lots of stuff in this file should be in a sys_*.c file!
TODO: see what happens if we interpolate the duplicate vertex normals for smoothing
TODO: triangle order is not counter-clockwise in all cases, but we have normals, right?
===================
*/
void Host_VoxelCommitUpdates(void)
{
	int i;

	if (!voxelworld)
		return;

#ifdef CHUNKS_AS_BOXES
	physhalfsize[0] = VOXEL_SIZE_X_2;
	physhalfsize[1] = VOXEL_SIZE_Y_2;
	physhalfsize[2] = VOXEL_SIZE_Z_2;
#endif /* CHUNKS_AS_BOXES */

	/*
		We will create two meshes for each chunk, this will be faster
		if we have client and server on separate machines. It will be
		a little slower if client and server are on the same machine
		but we will use less ram anyway, because of reusing of render
		buffers after uploading to GPU.
	*/
	for (i = 0; i < voxelworld->num_updated_chunks; i++)
	{
		model_voxel_chunk_t *chunk = voxelworld->chunklist[voxelworld->updated_chunks_list[i]];
		int x, y, z; /* cube positions inside the chunk */
		mstime_t t1, t2;
		rendernumvertices = 0;
		rendernumtrianglesverts = 0;
#ifdef CHUNKS_AS_TRIMESHES
		chunk->physnumvertices = 0;
		chunk->physnumtrianglesverts = 0;
#endif /* CHUNKS_AS_TRIMESHES */
#ifdef CHUNKS_AS_BOXES
		physnumboxes = 0;
#endif /* CHUNKS_AS_BOXES */
		t1 = Sys_Time();

		for (x = 0; x < VOXEL_CHUNK_SIZE_X; x++)
			for (y = 0; y < VOXEL_CHUNK_SIZE_Y; y++)
				for (z = 0; z < VOXEL_CHUNK_SIZE_Z; z++)
				{
					if (chunk->blocks[x][y][z].type != VOXEL_BLOCKTYPE_EMPTY)
					{
						/* TODO: optimize this, join coplanar faces where possible (also join brushes when possible), checkered pattern will generate lots of inside faces */
						vec3_t p1, p2, p3, p4, p5, p6, p7, p8;
						int okfront = false;
						int okback = false;
						int okright = false;
						int okleft = false;
						int oktop = false;
						int okbottom = false;

						/* TODO: do not calculate those for the server if we are going to use boxes and there is no client */
						p1[0] = (vec_t)((chunk->origin[0] * VOXEL_CHUNK_SIZE_X + x) * VOXEL_SIZE_X - VOXEL_SIZE_X_2);
						p1[1] = (vec_t)((chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + y) * VOXEL_SIZE_Y - VOXEL_SIZE_Y_2);
						p1[2] = (vec_t)((chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + z) * VOXEL_SIZE_Z + VOXEL_SIZE_Z_2);

						p2[0] = (vec_t)((chunk->origin[0] * VOXEL_CHUNK_SIZE_X + x) * VOXEL_SIZE_X + VOXEL_SIZE_X_2);
						p2[1] = (vec_t)((chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + y) * VOXEL_SIZE_Y - VOXEL_SIZE_Y_2);
						p2[2] = (vec_t)((chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + z) * VOXEL_SIZE_Z + VOXEL_SIZE_Z_2);

						p3[0] = (vec_t)((chunk->origin[0] * VOXEL_CHUNK_SIZE_X + x) * VOXEL_SIZE_X + VOXEL_SIZE_X_2);
						p3[1] = (vec_t)((chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + y) * VOXEL_SIZE_Y + VOXEL_SIZE_Y_2);
						p3[2] = (vec_t)((chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + z) * VOXEL_SIZE_Z + VOXEL_SIZE_Z_2);

						p4[0] = (vec_t)((chunk->origin[0] * VOXEL_CHUNK_SIZE_X + x) * VOXEL_SIZE_X - VOXEL_SIZE_X_2);
						p4[1] = (vec_t)((chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + y) * VOXEL_SIZE_Y + VOXEL_SIZE_Y_2);
						p4[2] = (vec_t)((chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + z) * VOXEL_SIZE_Z + VOXEL_SIZE_Z_2);

						p5[0] = (vec_t)((chunk->origin[0] * VOXEL_CHUNK_SIZE_X + x) * VOXEL_SIZE_X + VOXEL_SIZE_X_2);
						p5[1] = (vec_t)((chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + y) * VOXEL_SIZE_Y - VOXEL_SIZE_Y_2);
						p5[2] = (vec_t)((chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + z) * VOXEL_SIZE_Z - VOXEL_SIZE_Z_2);

						p6[0] = (vec_t)((chunk->origin[0] * VOXEL_CHUNK_SIZE_X + x) * VOXEL_SIZE_X - VOXEL_SIZE_X_2);
						p6[1] = (vec_t)((chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + y) * VOXEL_SIZE_Y - VOXEL_SIZE_Y_2);
						p6[2] = (vec_t)((chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + z) * VOXEL_SIZE_Z - VOXEL_SIZE_Z_2);

						p7[0] = (vec_t)((chunk->origin[0] * VOXEL_CHUNK_SIZE_X + x) * VOXEL_SIZE_X - VOXEL_SIZE_X_2);
						p7[1] = (vec_t)((chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + y) * VOXEL_SIZE_Y + VOXEL_SIZE_Y_2);
						p7[2] = (vec_t)((chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + z) * VOXEL_SIZE_Z - VOXEL_SIZE_Z_2);

						p8[0] = (vec_t)((chunk->origin[0] * VOXEL_CHUNK_SIZE_X + x) * VOXEL_SIZE_X + VOXEL_SIZE_X_2);
						p8[1] = (vec_t)((chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + y) * VOXEL_SIZE_Y + VOXEL_SIZE_Y_2);
						p8[2] = (vec_t)((chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + z) * VOXEL_SIZE_Z - VOXEL_SIZE_Z_2);

						if (chunk->blocks[x][y][z].type >= VOXEL_BLOCKTYPE_MAX || chunk->blocks[x][y][z].type < 0)
							Host_Error("Host_VoxelCommitUpdates: unknown BLOCKTYPE: chunk %d %d %d, block %d %d %d, type %d\n", chunk->origin[0], chunk->origin[1], chunk->origin[2], x, y, z, chunk->blocks[x][y][z].type);

						/* checking as if all faces/boxes/etc will be generated without culling */
						if (rendernumvertices + 24 >= VOXEL_CHUNK_MAX_VERTICES)
							Host_Error("Host_VoxelCommitUpdates: too many render vertices: chunk %d %d %d, block %d %d %d, type %d\n", chunk->origin[0], chunk->origin[1], chunk->origin[2], x, y, z, chunk->blocks[x][y][z].type);
						if (rendernumtrianglesverts + 36 >= VOXEL_CHUNK_MAX_TRIANGLESVERTS)
							Host_Error("Host_VoxelCommitUpdates: too many render triangles: chunk %d %d %d, block %d %d %d, type %d\n", chunk->origin[0], chunk->origin[1], chunk->origin[2], x, y, z, chunk->blocks[x][y][z].type);
#ifdef CHUNKS_AS_TRIMESHES
						if (chunk->physnumvertices + 24 >= VOXEL_CHUNK_MAX_VERTICES)
							Host_Error("Host_VoxelCommitUpdates: too many physics vertices: chunk %d %d %d, block %d %d %d, type %d\n", chunk->origin[0], chunk->origin[1], chunk->origin[2], x, y, z, chunk->blocks[x][y][z].type);
						if (chunk->physnumtrianglesverts + 36 >= VOXEL_CHUNK_MAX_TRIANGLESVERTS)
							Host_Error("Host_VoxelCommitUpdates: too many physics triangles: chunk %d %d %d, block %d %d %d, type %d\n", chunk->origin[0], chunk->origin[1], chunk->origin[2], x, y, z, chunk->blocks[x][y][z].type);
#endif /* CHUNKS_AS_TRIMESHES */
#ifdef CHUNKS_AS_BOXES
						if (physnumboxes + 1 >= VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z)
							Host_Error("Host_VoxelCommitUpdates: too many physics boxes: chunk %d %d %d, block %d %d %d, type %d\n", chunk->origin[0], chunk->origin[1], chunk->origin[2], x, y, z, chunk->blocks[x][y][z].type);
#endif /* CHUNKS_AS_BOXES */

						 /* TODO: transparent blocks should not occlude other blocks */
						/* in-chunk occlusions */
						if (z < VOXEL_CHUNK_SIZE_Z - 1 && chunk->blocks[x][y][z + 1].type == VOXEL_BLOCKTYPE_EMPTY)
							okfront = true;
						if (z > 0 && chunk->blocks[x][y][z - 1].type == VOXEL_BLOCKTYPE_EMPTY)
							okback = true;
						if (x < VOXEL_CHUNK_SIZE_X - 1 && chunk->blocks[x + 1][y][z].type == VOXEL_BLOCKTYPE_EMPTY)
							okright = true;
						if (x > 0 && chunk->blocks[x - 1][y][z].type == VOXEL_BLOCKTYPE_EMPTY)
							okleft = true;
						if (y < VOXEL_CHUNK_SIZE_Y - 1 && chunk->blocks[x][y + 1][z].type == VOXEL_BLOCKTYPE_EMPTY)
							oktop = true;
						if (y > 0 && chunk->blocks[x][y - 1][z].type == VOXEL_BLOCKTYPE_EMPTY)
							okbottom = true;
						/* occlusion by blocks in other chunks TODO: slow? */
						if (z == VOXEL_CHUNK_SIZE_Z - 1)
						{
							int nextchunk[3], nextchunkidx;

							nextchunk[0] = chunk->origin[0];
							nextchunk[1] = chunk->origin[1];
							nextchunk[2] = chunk->origin[2] + 1;
							nextchunkidx = Host_VoxelGetChunk(nextchunk);
							if (nextchunkidx == -1)
								okfront = true;
							else if (voxelworld->chunklist[nextchunkidx]->blocks[x][y][0].type == VOXEL_BLOCKTYPE_EMPTY)
								okfront = true;
						}
						if (z == 0)
						{
							int nextchunk[3], nextchunkidx;

							nextchunk[0] = chunk->origin[0];
							nextchunk[1] = chunk->origin[1];
							nextchunk[2] = chunk->origin[2] - 1;
							nextchunkidx = Host_VoxelGetChunk(nextchunk);
							if (nextchunkidx == -1)
								okback = true;
							else if (voxelworld->chunklist[nextchunkidx]->blocks[x][y][VOXEL_CHUNK_SIZE_Z - 1].type == VOXEL_BLOCKTYPE_EMPTY)
								okback = true;
						}
						if (x == VOXEL_CHUNK_SIZE_X - 1)
						{
							int nextchunk[3], nextchunkidx;

							nextchunk[0] = chunk->origin[0] + 1;
							nextchunk[1] = chunk->origin[1];
							nextchunk[2] = chunk->origin[2];
							nextchunkidx = Host_VoxelGetChunk(nextchunk);
							if (nextchunkidx == -1)
								okright = true;
							else if (voxelworld->chunklist[nextchunkidx]->blocks[0][y][z].type == VOXEL_BLOCKTYPE_EMPTY)
								okright = true;
						}
						if (x == 0)
						{
							int nextchunk[3], nextchunkidx;

							nextchunk[0] = chunk->origin[0] - 1;
							nextchunk[1] = chunk->origin[1];
							nextchunk[2] = chunk->origin[2];
							nextchunkidx = Host_VoxelGetChunk(nextchunk);
							if (nextchunkidx == -1)
								okleft = true;
							else if (voxelworld->chunklist[nextchunkidx]->blocks[VOXEL_CHUNK_SIZE_X - 1][y][z].type == VOXEL_BLOCKTYPE_EMPTY)
								okleft = true;
						}
						if (y == VOXEL_CHUNK_SIZE_Y - 1)
						{
							int nextchunk[3], nextchunkidx;

							nextchunk[0] = chunk->origin[0];
							nextchunk[1] = chunk->origin[1] + 1;
							nextchunk[2] = chunk->origin[2];
							nextchunkidx = Host_VoxelGetChunk(nextchunk);
							if (nextchunkidx == -1)
								oktop = true;
							else if (voxelworld->chunklist[nextchunkidx]->blocks[x][0][z].type == VOXEL_BLOCKTYPE_EMPTY)
								oktop = true;
						}
						if (y == 0)
						{
							int nextchunk[3], nextchunkidx;

							nextchunk[0] = chunk->origin[0];
							nextchunk[1] = chunk->origin[1] - 1;
							nextchunk[2] = chunk->origin[2];
							nextchunkidx = Host_VoxelGetChunk(nextchunk);
							if (nextchunkidx == -1)
								okbottom = true;
							else if (voxelworld->chunklist[nextchunkidx]->blocks[x][VOXEL_CHUNK_SIZE_Y - 1][z].type == VOXEL_BLOCKTYPE_EMPTY)
								okbottom = true;
						}

						/* TODO: reuse vertices below! currently we copy then when needed this will make things faster and use less memory */

						/* front face */
						if (okfront)
						{
							if (cls.active)
							{
								Math_Vector3Set(rendervertices[rendernumvertices    ].normal, 0, 0, 1);
								Math_Vector3Set(rendervertices[rendernumvertices + 1].normal, 0, 0, 1);
								Math_Vector3Set(rendervertices[rendernumvertices + 2].normal, 0, 0, 1);
								Math_Vector3Set(rendervertices[rendernumvertices + 3].normal, 0, 0, 1);

								rendervertices[rendernumvertices    ].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices    ].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 1].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 1].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 2].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 2].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1];
								rendervertices[rendernumvertices + 3].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices + 3].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1];

								Math_Vector3Copy(p1, rendervertices[rendernumvertices    ].origin);
								Math_Vector3Copy(p2, rendervertices[rendernumvertices + 1].origin);
								Math_Vector3Copy(p3, rendervertices[rendernumvertices + 2].origin);
								Math_Vector3Copy(p4, rendervertices[rendernumvertices + 3].origin);

								rendertrianglesverts[rendernumtrianglesverts    ] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 1] = rendernumvertices + 1;
								rendertrianglesverts[rendernumtrianglesverts + 2] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 3] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 4] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 5] = rendernumvertices + 3;

								rendernumvertices += 4;
								rendernumtrianglesverts += 6;
							}

							if (svs.loading || svs.listening) /* allow updating chunks during the loading phase */
							{
#ifdef CHUNKS_AS_TRIMESHES
								Math_Vector3Copy(p1, chunk->physvertices[chunk->physnumvertices    ]);
								Math_Vector3Copy(p2, chunk->physvertices[chunk->physnumvertices + 1]);
								Math_Vector3Copy(p3, chunk->physvertices[chunk->physnumvertices + 2]);
								Math_Vector3Copy(p4, chunk->physvertices[chunk->physnumvertices + 3]);

								chunk->phystrianglesverts[chunk->physnumtrianglesverts    ] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 1] = chunk->physnumvertices + 1;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 2] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 3] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 4] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 5] = chunk->physnumvertices + 3;

								chunk->physnumvertices += 4;
								chunk->physnumtrianglesverts += 6;
#endif /* CHUNKS_AS_TRIMESHES */
							}
						}

						/* back face */
						if (okback)
						{
							if (cls.active)
							{
								Math_Vector3Set(rendervertices[rendernumvertices    ].normal, 0, 0, -1);
								Math_Vector3Set(rendervertices[rendernumvertices + 1].normal, 0, 0, -1);
								Math_Vector3Set(rendervertices[rendernumvertices + 2].normal, 0, 0, -1);
								Math_Vector3Set(rendervertices[rendernumvertices + 3].normal, 0, 0, -1);

								rendervertices[rendernumvertices    ].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices    ].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 1].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 1].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 2].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 2].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1];
								rendervertices[rendernumvertices + 3].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices + 3].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1];

								Math_Vector3Copy(p5, rendervertices[rendernumvertices    ].origin);
								Math_Vector3Copy(p6, rendervertices[rendernumvertices + 1].origin);
								Math_Vector3Copy(p7, rendervertices[rendernumvertices + 2].origin);
								Math_Vector3Copy(p8, rendervertices[rendernumvertices + 3].origin);

								rendertrianglesverts[rendernumtrianglesverts    ] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 1] = rendernumvertices + 1;
								rendertrianglesverts[rendernumtrianglesverts + 2] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 3] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 4] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 5] = rendernumvertices + 3;

								rendernumvertices += 4;
								rendernumtrianglesverts += 6;
							}

							if (svs.loading || svs.listening) /* allow updating chunks during the loading phase */
							{
#ifdef CHUNKS_AS_TRIMESHES
								Math_Vector3Copy(p5, chunk->physvertices[chunk->physnumvertices    ]);
								Math_Vector3Copy(p6, chunk->physvertices[chunk->physnumvertices + 1]);
								Math_Vector3Copy(p7, chunk->physvertices[chunk->physnumvertices + 2]);
								Math_Vector3Copy(p8, chunk->physvertices[chunk->physnumvertices + 3]);

								chunk->phystrianglesverts[chunk->physnumtrianglesverts    ] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 1] = chunk->physnumvertices + 1;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 2] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 3] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 4] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 5] = chunk->physnumvertices + 3;

								chunk->physnumvertices += 4;
								chunk->physnumtrianglesverts += 6;
#endif /* CHUNKS_AS_TRIMESHES */
							}
						}

						/* right face */
						if (okright)
						{
							if (cls.active)
							{
								Math_Vector3Set(rendervertices[rendernumvertices    ].normal, 1, 0, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 1].normal, 1, 0, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 2].normal, 1, 0, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 3].normal, 1, 0, 0);

								rendervertices[rendernumvertices    ].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices    ].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 1].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 1].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 2].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 2].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1];
								rendervertices[rendernumvertices + 3].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices + 3].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1];

								Math_Vector3Copy(p2, rendervertices[rendernumvertices    ].origin);
								Math_Vector3Copy(p5, rendervertices[rendernumvertices + 1].origin);
								Math_Vector3Copy(p8, rendervertices[rendernumvertices + 2].origin);
								Math_Vector3Copy(p3, rendervertices[rendernumvertices + 3].origin);

								rendertrianglesverts[rendernumtrianglesverts    ] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 1] = rendernumvertices + 1;
								rendertrianglesverts[rendernumtrianglesverts + 2] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 3] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 4] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 5] = rendernumvertices + 3;

								rendernumvertices += 4;
								rendernumtrianglesverts += 6;
							}

							if (svs.loading || svs.listening) /* allow updating chunks during the loading phase */
							{
#ifdef CHUNKS_AS_TRIMESHES
								Math_Vector3Copy(p2, chunk->physvertices[chunk->physnumvertices    ]);
								Math_Vector3Copy(p5, chunk->physvertices[chunk->physnumvertices + 1]);
								Math_Vector3Copy(p8, chunk->physvertices[chunk->physnumvertices + 2]);
								Math_Vector3Copy(p3, chunk->physvertices[chunk->physnumvertices + 3]);

								chunk->phystrianglesverts[chunk->physnumtrianglesverts    ] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 1] = chunk->physnumvertices + 1;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 2] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 3] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 4] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 5] = chunk->physnumvertices + 3;

								chunk->physnumvertices += 4;
								chunk->physnumtrianglesverts += 6;
#endif /* CHUNKS_AS_TRIMESHES */
							}
						}

						/* left face */
						if (okleft)
						{
							if (cls.active)
							{
								Math_Vector3Set(rendervertices[rendernumvertices    ].normal, -1, 0, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 1].normal, -1, 0, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 2].normal, -1, 0, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 3].normal, -1, 0, 0);

								rendervertices[rendernumvertices    ].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices    ].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 1].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 1].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 2].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 2].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1];
								rendervertices[rendernumvertices + 3].texcoord0[0] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices + 3].texcoord0[1] = block_texture_st_coords_laterals[chunk->blocks[x][y][z].type][1];

								Math_Vector3Copy(p6, rendervertices[rendernumvertices    ].origin);
								Math_Vector3Copy(p1, rendervertices[rendernumvertices + 1].origin);
								Math_Vector3Copy(p4, rendervertices[rendernumvertices + 2].origin);
								Math_Vector3Copy(p7, rendervertices[rendernumvertices + 3].origin);

								rendertrianglesverts[rendernumtrianglesverts    ] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 1] = rendernumvertices + 1;
								rendertrianglesverts[rendernumtrianglesverts + 2] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 3] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 4] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 5] = rendernumvertices + 3;

								rendernumvertices += 4;
								rendernumtrianglesverts += 6;
							}

							if (svs.loading || svs.listening) /* allow updating chunks during the loading phase */
							{
#ifdef CHUNKS_AS_TRIMESHES
								Math_Vector3Copy(p6, chunk->physvertices[chunk->physnumvertices    ]);
								Math_Vector3Copy(p1, chunk->physvertices[chunk->physnumvertices + 1]);
								Math_Vector3Copy(p4, chunk->physvertices[chunk->physnumvertices + 2]);
								Math_Vector3Copy(p7, chunk->physvertices[chunk->physnumvertices + 3]);

								chunk->phystrianglesverts[chunk->physnumtrianglesverts    ] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 1] = chunk->physnumvertices + 1;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 2] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 3] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 4] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 5] = chunk->physnumvertices + 3;

								chunk->physnumvertices += 4;
								chunk->physnumtrianglesverts += 6;
#endif /* CHUNKS_AS_TRIMESHES */
							}
						}

						/* top face */
						if (oktop)
						{
							if (cls.active)
							{
								Math_Vector3Set(rendervertices[rendernumvertices    ].normal, 0, 1, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 1].normal, 0, 1, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 2].normal, 0, 1, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 3].normal, 0, 1, 0);

								rendervertices[rendernumvertices    ].texcoord0[0] = block_texture_st_coords_top[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices    ].texcoord0[1] = block_texture_st_coords_top[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 1].texcoord0[0] = block_texture_st_coords_top[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 1].texcoord0[1] = block_texture_st_coords_top[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 2].texcoord0[0] = block_texture_st_coords_top[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 2].texcoord0[1] = block_texture_st_coords_top[chunk->blocks[x][y][z].type][1];
								rendervertices[rendernumvertices + 3].texcoord0[0] = block_texture_st_coords_top[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices + 3].texcoord0[1] = block_texture_st_coords_top[chunk->blocks[x][y][z].type][1];

								Math_Vector3Copy(p4, rendervertices[rendernumvertices    ].origin);
								Math_Vector3Copy(p3, rendervertices[rendernumvertices + 1].origin);
								Math_Vector3Copy(p8, rendervertices[rendernumvertices + 2].origin);
								Math_Vector3Copy(p7, rendervertices[rendernumvertices + 3].origin);

								rendertrianglesverts[rendernumtrianglesverts    ] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 1] = rendernumvertices + 1;
								rendertrianglesverts[rendernumtrianglesverts + 2] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 3] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 4] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 5] = rendernumvertices + 3;

								rendernumvertices += 4;
								rendernumtrianglesverts += 6;
							}

							if (svs.loading || svs.listening) /* allow updating chunks during the loading phase */
							{
#ifdef CHUNKS_AS_TRIMESHES
								Math_Vector3Copy(p4, chunk->physvertices[chunk->physnumvertices    ]);
								Math_Vector3Copy(p3, chunk->physvertices[chunk->physnumvertices + 1]);
								Math_Vector3Copy(p8, chunk->physvertices[chunk->physnumvertices + 2]);
								Math_Vector3Copy(p7, chunk->physvertices[chunk->physnumvertices + 3]);

								chunk->phystrianglesverts[chunk->physnumtrianglesverts    ] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 1] = chunk->physnumvertices + 1;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 2] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 3] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 4] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 5] = chunk->physnumvertices + 3;

								chunk->physnumvertices += 4;
								chunk->physnumtrianglesverts += 6;
#endif /* CHUNKS_AS_TRIMESHES */
							}
						}

						/* bottom face */
						if (okbottom)
						{
							if (cls.active)
							{
								Math_Vector3Set(rendervertices[rendernumvertices    ].normal, 0, -1, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 1].normal, 0, -1, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 2].normal, 0, -1, 0);
								Math_Vector3Set(rendervertices[rendernumvertices + 3].normal, 0, -1, 0);

								rendervertices[rendernumvertices    ].texcoord0[0] = block_texture_st_coords_bottom[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices    ].texcoord0[1] = block_texture_st_coords_bottom[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 1].texcoord0[0] = block_texture_st_coords_bottom[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 1].texcoord0[1] = block_texture_st_coords_bottom[chunk->blocks[x][y][z].type][1] + ((vec_t)(TEXTURE_SIZE_Y - 1.0f) / (vec_t)VOXEL_ATLAS_HEIGHT);
								rendervertices[rendernumvertices + 2].texcoord0[0] = block_texture_st_coords_bottom[chunk->blocks[x][y][z].type][0] + ((vec_t)(TEXTURE_SIZE_X - 1.0f) / (vec_t)VOXEL_ATLAS_WIDTH);
								rendervertices[rendernumvertices + 2].texcoord0[1] = block_texture_st_coords_bottom[chunk->blocks[x][y][z].type][1];
								rendervertices[rendernumvertices + 3].texcoord0[0] = block_texture_st_coords_bottom[chunk->blocks[x][y][z].type][0];
								rendervertices[rendernumvertices + 3].texcoord0[1] = block_texture_st_coords_bottom[chunk->blocks[x][y][z].type][1];

								Math_Vector3Copy(p6, rendervertices[rendernumvertices    ].origin);
								Math_Vector3Copy(p5, rendervertices[rendernumvertices + 1].origin);
								Math_Vector3Copy(p2, rendervertices[rendernumvertices + 2].origin);
								Math_Vector3Copy(p1, rendervertices[rendernumvertices + 3].origin);

								rendertrianglesverts[rendernumtrianglesverts    ] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 1] = rendernumvertices + 1;
								rendertrianglesverts[rendernumtrianglesverts + 2] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 3] = rendernumvertices;
								rendertrianglesverts[rendernumtrianglesverts + 4] = rendernumvertices + 2;
								rendertrianglesverts[rendernumtrianglesverts + 5] = rendernumvertices + 3;

								rendernumvertices += 4;
								rendernumtrianglesverts += 6;
							}

							if (svs.loading || svs.listening) /* allow updating chunks during the loading phase */
							{
#ifdef CHUNKS_AS_TRIMESHES
								Math_Vector3Copy(p6, chunk->physvertices[chunk->physnumvertices    ]);
								Math_Vector3Copy(p5, chunk->physvertices[chunk->physnumvertices + 1]);
								Math_Vector3Copy(p2, chunk->physvertices[chunk->physnumvertices + 2]);
								Math_Vector3Copy(p1, chunk->physvertices[chunk->physnumvertices + 3]);

								chunk->phystrianglesverts[chunk->physnumtrianglesverts    ] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 1] = chunk->physnumvertices + 1;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 2] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 3] = chunk->physnumvertices;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 4] = chunk->physnumvertices + 2;
								chunk->phystrianglesverts[chunk->physnumtrianglesverts + 5] = chunk->physnumvertices + 3;

								chunk->physnumvertices += 4;
								chunk->physnumtrianglesverts += 6;
#endif /* CHUNKS_AS_TRIMESHES */
							}
						}
#ifdef CHUNKS_AS_BOXES
						if (okfront || okback || okright || okleft || oktop || okbottom)
						{
							/* calculate the center of the box */
							physorigins[physnumboxes][0] = (vec_t)((chunk->origin[0] * VOXEL_CHUNK_SIZE_X + x) * VOXEL_SIZE_X);
							physorigins[physnumboxes][1] = (vec_t)((chunk->origin[1] * VOXEL_CHUNK_SIZE_Y + y) * VOXEL_SIZE_Y);
							physorigins[physnumboxes][2] = (vec_t)((chunk->origin[2] * VOXEL_CHUNK_SIZE_Z + z) * VOXEL_SIZE_Z);
							physnumboxes++;
						}
#endif /* CHUNKS_AS_BOXES */
					}
				}
		t2 = Sys_Time();
		if (!svs.loading) /* do not spam */
			Sys_Printf("Chunk %d update  in %u ms\n", voxelworld->updated_chunks_list[i], t2 - t1);
		t1 = Sys_Time();
		if (svs.loading || svs.listening) /* allow updating chunks during the loading phase */
		{
			if (!svs.loading_saved_game) /* if loading, do not create because the data was already saved with the physics saving */
			{
				/* if we are a server, update physics data */
#ifdef CHUNKS_AS_TRIMESHES
				Sys_PhysicsCreateVoxelChunk(svs.physworld, voxelworld->updated_chunks_list[i], PHYSICS_SHAPE_VOXEL_TRIMESH, NULL, NULL, 0, (vec_t *)chunk->physvertices, sizeof(vec3_t), chunk->physnumvertices, chunk->phystrianglesverts, chunk->physnumtrianglesverts, sizeof(unsigned int) * 3);
#endif /* CHUNKS_AS_TRIMESHES */
#ifdef CHUNKS_AS_BOXES
				Sys_PhysicsCreateVoxelChunk(svs.physworld, voxelworld->updated_chunks_list[i], PHYSICS_SHAPE_VOXEL_BOX, physhalfsize, (vec_t *)physorigins, physnumboxes, NULL, 0, 0, NULL, 0, 0);
#endif /* CHUNKS_AS_BOXES */
			}
		}
		/* TODO: when doing the client-side prediction, also update the client physworld's voxelworld here (if the server is not local, of course! Physics should not be ran two times on the same machine */

		t2 = Sys_Time();
		if (!svs.loading) /* do not spam */
			Sys_Printf("Chunk %d physics in %u ms\n", voxelworld->updated_chunks_list[i], t2 - t1);
		t1 = Sys_Time();
		if (cls.active)
		{
			model_trimesh_part_t trimesh;

			trimesh.verts = rendervertices;
			trimesh.vert_stride = sizeof(model_vertex_t);
			trimesh.vert_count = rendernumvertices;
			trimesh.indexes = rendertrianglesverts;
			trimesh.index_count = rendernumtrianglesverts;
			trimesh.index_stride = sizeof(unsigned int) * 3;
			if (voxelworld->chunklist[voxelworld->updated_chunks_list[i]]->vbo_id != -1)
				Sys_UploadVBO(voxelworld->chunklist[voxelworld->updated_chunks_list[i]]->vbo_id, &trimesh, false, false);
			else
				voxelworld->chunklist[voxelworld->updated_chunks_list[i]]->vbo_id = Sys_UploadVBO(-1, &trimesh, false, false);
		}

		t2 = Sys_Time();
		if (!svs.loading) /* do not spam */
			Sys_Printf("Chunk %d VBO     in %u ms\n", voxelworld->updated_chunks_list[i], t2 - t1);
	}

	voxelworld->num_updated_chunks = 0;
}

/*
===================
Host_VoxelCreate

Prepare for using voxels
===================
*/
void Host_VoxelCreate(int loading_saved_game)
{
	if ((svs.active && !svs.loading) && (cls.active && cls.ingame))
		Host_Error("Host_VoxelCreate should only be called while loading\n");

	if (voxelworld) /* already created - client and server on the same machine? */
		return;

	chunk_map = Host_UtilMapCreateInt3ToInt();

	voxelworld = Sys_MemAlloc(&mdl_mem, sizeof(model_voxel_t), "voxelheader");
	voxelworld->num_chunks = 0;
	voxelworld->num_updated_chunks = 0;

	/* TODO: load texture atlas from game code, this shouldn't be mandatory for the engine to decide, also, send from server to client! search for other cases like this in voxel code */
	/* load the voxel atlas texture with separate mipmaps because auto-mipmapping would mix the different textures in the atlas */
	if (cls.active)
		voxelworld->texture = (void *)CL_LoadTexture("voxelatlas", true, NULL, 0, 0, true, VOXEL_ATLAS_WIDTH / TEXTURE_SIZE_X, VOXEL_ATLAS_HEIGHT / TEXTURE_SIZE_Y);

	/* TODO CONSOLEDEBUG Sys_Printf("Voxel world ready.\n"); */
}

/*
===================
Host_VoxelSaveFileName

Little helper function
===================
*/
void Host_VoxelSaveFileName(char *name, char *out)
{
	Sys_Snprintf(out, FILENAME_MAX, "%s.voxels", name);
}

/*
===================
Host_VoxelLoad

Loads voxel data

TODO: ENDIANNESS, ALIGNMENT, ARCHITECTURE/ABI PADDING, KEEP IN SYNC, CHECK ERRORS HERE AND IN ALL SUBFUNCTIONS
TODO: currently needs all voxels in voxelworld->chunklist to be sequential
===================
*/
void Host_VoxelLoad(char *name)
{
	void *handle;
	int i;
	char file[FILENAME_MAX];

	if (!voxelworld)
		return;

	Host_VoxelSaveFileName(name, file);

	handle = Host_FSFileHandleOpenBinaryRead(file);
	if (!handle)
	{
		Sys_Printf("Host_VoxelLoad: couldn't load voxel data\n");
		return;
	}

	Host_FSFileHandleReadBinaryDest(handle, (unsigned char *)&voxelworld->num_chunks, sizeof(voxelworld->num_chunks));
	for (i = 0; i < voxelworld->num_chunks; i++)
	{
		int created = false;
		if (!voxelworld->chunklist[i])
		{
			created = true;
			voxelworld->chunklist[i] = Sys_MemAlloc(&mdl_mem, sizeof(model_voxel_chunk_t), "voxelchunks");
		}

		/* TODO: currently saving even vbo data to do this easier... */
		Host_FSFileHandleReadBinaryDest(handle, (unsigned char *)voxelworld->chunklist[i], sizeof(model_voxel_chunk_t));
		if (created) /* we need to have a valid origin, so do this only after reading if we just created this chunk */
			Host_UtilMapInsertInt3ToInt(chunk_map, voxelworld->chunklist[i]->origin, i);

		voxelworld->chunklist[i]->vbo_id = -1;
	}
	Host_FSFileHandleClose(handle);

	for (i = 0; i < voxelworld->num_chunks; i++)
		voxelworld->updated_chunks_list[i] = i;
	voxelworld->num_updated_chunks = voxelworld->num_chunks;
	Host_VoxelCommitUpdates();
}

/*
===================
Host_VoxelSave

Saves voxel data

TODO: ENDIANNESS, ALIGNMENT, ARCHITECTURE/ABI PADDING, KEEP IN SYNC, CHECK ERRORS HERE AND IN ALL SUBFUNCTIONS
TODO: currently needs all voxels in voxelworld->chunklist to be sequential
===================
*/
void Host_VoxelSave(char *name)
{
	void *handle;
	int i;
	char file[FILENAME_MAX];

	if (!voxelworld)
		return;

	Host_VoxelSaveFileName(name, file);

	handle = Host_FSFileHandleOpenBinaryWrite(file);
	if (!handle)
	{
		Sys_Printf("Host_VoxelSave: couldn't save voxel data\n");
		return;
	}

	Host_FSFileHandleWriteBinary(handle, (unsigned char *)&voxelworld->num_chunks, sizeof(voxelworld->num_chunks));
	for (i = 0; i < voxelworld->num_chunks; i++)
	{
		/* TODO: currently saving even vbo data to do this easier... */
		Host_FSFileHandleWriteBinary(handle, (unsigned char *)voxelworld->chunklist[i], sizeof(model_voxel_chunk_t));
	}

	Host_FSFileHandleClose(handle);
}
