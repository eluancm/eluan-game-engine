/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/* TODO: reload 2d textures based on display resolution (on system code) to avoid undersampling or oversampling */

/*
============================================================================

Texture management

Texture are loaded from the filesystem according to "name"
We don't care about file formats, paths etc! sys_***_video.c will take
care of that.

============================================================================
*/
texture_t	textures[MAX_TEXTURES];

/*
===================
CL_CleanTextures
===================
*/
void CL_CleanTextures(int forcenokeep)
{
	int i;

	for (i = 0; i < MAX_TEXTURES; i++)
	{
		if (textures[i].active && (!textures[i].keep || forcenokeep))
		{
			Sys_UnloadTexture(&textures[i].id);
			textures[i].active = false;
		}
	}
}

/*
===================
CL_LoadTexture

May be called at will to get the id of a previously loaded texture
indata, inwidth and inheight are optional, for generated textures
===================
*/
texture_t *CL_LoadTexture(const char *name, int keep, unsigned char *indata, int inwidth, int inheight, int data_has_mipmaps, int mipmapuntilwidth, int mipmapuntilheight)
{
	int i;

	if (strlen (name) + 1 >= MAX_TEXTURE_NAME)
		Host_Error("CL_LoadTexture: name too big: %s\n", name);

	/* see if it's already loaded */
	for (i = 0; i < MAX_TEXTURES; i++)
	{
		if (textures[i].active && !strcmp(name, textures[i].name))
		{
			if (keep) /* may be reused in a location we want to keep */
				textures[i].keep = true;

			return textures + i;
		}
	}

	if (cls.ingame) /* TODO: have a local parameter, just like sound playing, to allow for local loads even after engine initialization? */
		Host_Error("CL_LoadTexture: texture \"%s\" not precached.\n", name);

	/* load it */
	for (i = 0; i < MAX_TEXTURES; i++)
	{
		if (!textures[i].active)
		{
			textures[i].active = true;
			Sys_LoadTexture(name, i, &textures[i].id, &textures[i].width, &textures[i].height, indata, inwidth, inheight, data_has_mipmaps, mipmapuntilwidth, mipmapuntilheight);
			Sys_Snprintf(textures[i].name, MAX_TEXTURE_NAME, "%s", name);
			textures[i].keep = keep;
			textures[i].cl_id = i;

			/* TODO CONSOLEDEBUG Sys_Printf("Loaded texture %d (sysid %d) \"%s\"\n", i, textures[i].id, name); */

			return textures + i;
		}
	}

	Sys_Error ("CL_LoadTexture: Couldn't load \"%s\", i == MAX_TEXTURES\n", name);
	return NULL; /* never reached */
}

/*
===================
CL_TexturesInit
===================
*/
void CL_TexturesInit(void)
{
	int i;

	for (i = 0; i < MAX_TEXTURES; i++)
	{
		textures[i].active = false;
		/* textures[i].id will be set by the system */
		textures[i].cl_id = -1;
		textures[i].name[0] = 0;
		textures[i].keep = false;
		textures[i].width = 0;
		textures[i].height = 0;
	}
}

/*
===================
CL_TexturesShutdown
===================
*/
void CL_TexturesShutdown(void)
{
	CL_CleanTextures(true);
}

/*
============================================================================

Main video routines

============================================================================
*/

/*
===================
CL_VideoDataClean

Between levels, etc
===================
*/
void CL_VideoDataClean (void)
{
	if (!svs.listening)
		Host_CleanModels(); /* only clear models here if we do not have a listen server, otherwise let the server handle this. FIXME: Stupid things like having a local server and connecting to a remote server will prevent memory from being freed. */
}

/*
===================
CL_VideoSet2D

Simulate a BASE_WIDTHxBASE_HIGHT display with X going right and Y going down, starting at the top-left corner
Vertical resolution is always the same, horizontal resolution stretchs or shrinks to adapt to different aspect ratios
without stretching the screen.
===================
*/
void CL_VideoSet2D(void)
{
	Sys_VideoSet2D(true);
}

/*
===================
CL_VideoInit
===================
*/
void CL_VideoInit(void)
{
	Sys_InitVideo();
	CL_TexturesInit();
	Sys_VBOsInit();
}

/*
===================
CL_VideoShutdown
===================
*/
void CL_VideoShutdown(void)
{
	Sys_SkyboxUnload();
	Sys_CleanVBOs(true); /* nothing to uninitialize, just free video memory */
	CL_TexturesShutdown();
	Sys_ShutdownVideo();
}

/*
===================
CL_VideoFrame

This may be called at will to update the screen during process-freezing operations (loading screen, etc)
===================
*/
void CL_VideoFrame(void)
{
	Sys_StartVideoFrame();

	if (cls.ingame && !svs.loading && cls.current_snapshot_valid) /* received at least the baseline and the server isn't changing maps, etc (the reconnect/disconect message may not have arrived in the local client yet) */ /* TODO: MAY CAUSE SEGFAULTS, BASELINE NOT IMPLEMENTED */
		Sys_Draw3DFrame(cls.prediction_snapshot.edicts[cls.prediction_snapshot.viewent].origin, cls.prediction_snapshot.edicts[cls.prediction_snapshot.viewent].angles, cls.prediction_snapshot.edicts, used_particles, Host_VoxelGetData());

	if (keydest == KEYDEST_GAME && !cls.connected) /* do not compare with cls.ingame because this will take us back to the menu while changing levels */
		Host_CMDBufferAdd("togglemenu"); /* do not look at a blank screen */

	CL_VideoSet2D();
	CL_MenuDraw();

	Sys_EndVideoFrame();
}
