/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

World Voxel Client-side Management

============================================================================
*/

/*
===================
CL_ParseVoxelBlock
===================
*/
void CL_ParseVoxelBlock(char *msg, int *read, int len)
{
	int absx, absy, absz;
	unsigned char type;

	/* we must read even if we will ignore it */
	MSG_ReadInt(msg, read, len, &absx);
	MSG_ReadInt(msg, read, len, &absy);
	MSG_ReadInt(msg, read, len, &absz);
	MSG_ReadByte(msg, read, len, &type);

	/* if we have a local server, we will be sharing data with it */
	if (svs.listening)
		return;

	Host_VoxelSetBlock(absx, absy, absz, type);
}

/*
===================
CL_ParseVoxelChunkPart
===================
*/
void CL_ParseVoxelChunkPart(char *msg, int *read, int len)
{
	int chunkindex, start;
	unsigned char datasize;
	int chunkcoord[3];
	int i;
	unsigned char *start_address;
	model_voxel_t *voxel = Host_VoxelGetData();

	/* we must read even if we will ignore it */
	MSG_ReadInt(msg, read, len, &chunkindex);
	MSG_ReadInt(msg, read, len, &start);

	if (chunkindex < 0)
		Host_Error("CL_ParseVoxelChunkPart: chunkindex < 0! (start == %d, chunk = %d\n", start, chunkindex);
	if (start < 0)
		Host_Error("CL_ParseVoxelChunkPart: start < 0! (start == %d, chunk = %d\n", start, chunkindex);

	/* chunk not yet allocated */
	if (start == 0)
	{
		MSG_ReadInt(msg, read, len, chunkcoord);
		MSG_ReadInt(msg, read, len, chunkcoord + 1);
		MSG_ReadInt(msg, read, len, chunkcoord + 2);

		if (voxel->num_chunks != chunkindex + 1)
		{
			/* if we have a local server, we will be sharing data with it */
			if (!svs.listening)
			{
				if(Host_VoxelNewChunk(chunkcoord) == -1)
				{
					Host_Error("CL_ParseVoxelChunkPart: unable to create chunkidx %d at %d %d %d, corrupted data from the server?\n", chunkindex, chunkcoord[0], chunkcoord[1], chunkcoord[2]);
				}
			}
		}
		else
		{
			if (!svs.listening)
				Host_Error("CL_ParseVoxelChunkPart: Chunk already allocated but start == 0, network data corrupt (voxel->num_chunks == %d, chunkindex == %d, start == %d", voxel->num_chunks, chunkindex, start);
		}
	}
	else if (voxel->num_chunks != chunkindex + 1)
	{
		if (!svs.listening)
			Host_Error("CL_ParseVoxelChunkPart: Chunk not yet allocated but start != 0, network data corrupt (voxel->num_chunks == %d, chunkindex == %d, start == %d", voxel->num_chunks, chunkindex, start);
	}

	if ((voxel->num_chunks != chunkindex + 1) && !svs.listening)
		Host_Error("CL_ParseVoxelChunkPart: Chunk allocation failed or network data corrupt, voxel->num_chunks == %d, chunkindex == %d", voxel->num_chunks, chunkindex);

	MSG_ReadByte(msg, read, len, &datasize);
	start_address = (unsigned char *)voxel->chunklist[chunkindex]->blocks + start;

	if (start + datasize > VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z)
		Host_Error("CL_ParseVoxelChunkPart: chunk overflow! (start == %d, datasize == %d, limit == %d\n", start, datasize, VOXEL_CHUNK_SIZE_X * VOXEL_CHUNK_SIZE_Y * VOXEL_CHUNK_SIZE_Z);

	for (i = 0; i < datasize; i++)
	{
		if (!svs.listening)
			MSG_ReadByte(msg, read, len, start_address + i); /* bypass Host_VoxelSetChunk, this is a controlled environment */
		else /* if we have a local server, we will be sharing data with it */
		{
			unsigned char tmp;
			MSG_ReadByte(msg, read, len, &tmp); /* just sink it */
		}
	}

	/* this will update chunks more than once before they are fully received, but the experience will be better */
	if (!svs.listening)
	{
		if (voxel->num_updated_chunks == VOXEL_MAX_CHUNKS)
			Sys_Error("CL_ParseVoxelChunkPart: voxel->num_updated_chunks == %d\n", voxel->num_updated_chunks);
		/* bypass Host_VoxelQueueChunkUpdate, this is a controlled environment  */
		voxel->updated_chunks_list[voxel->num_updated_chunks++] = chunkindex;
	}
}
