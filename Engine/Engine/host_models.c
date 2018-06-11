/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Model management

Same observations as with texture loading.
TODO: lots of memory could be saved if we don't store some temporary data

============================================================================
*/

model_t		models[MAX_PRECACHES];
int			modelmarker;

/*
===================
Host_CleanModels

Everything will be cleared, system should take care of caching disk data.
TODO: for clearing models, sounds, textures and all other resources, use reference counts!! This avoids lots of mess for shared data in local clients and servers. Clear reference count and add to them when loading, them free up what is still zero.
===================
*/
void Host_CleanModels(void)
{
	int i;

	/* stuff loaded by Sys_LoadModelClientData and part of voxel data */
	if (cls.active)
	{
		Sys_SkyboxUnload();
		Sys_CleanVBOs(false);
		CL_CleanTextures(false);
	}

	for (i = 0; i < MAX_PRECACHES; i++)
	{
		models[i].active = false;
		models[i].name[0] = 0;
		models[i].data = NULL;
	}

	Host_CleanVoxelsByProxy();

	/* clear everything that models[i].data pointed at */
	Sys_MemFreeToLowMark(&mdl_mem, modelmarker);
}

/*
===================
Host_LoadModel

May be called at will to get the id of a previously loaded model
Model names that begin with '*SUBMODELINDEX*BASEMODELPRECACHEINDEX' are SUBMODELS of the BASEMODEL
===================
*/
model_t *Host_LoadModel(const char *name, int server)
{
	int i;

	if (strlen (name) + 1 >= MAX_MODEL_NAME)
		Host_Error("Host_LoadModel: name too big: %s\n", name);

	/* see if it's already loaded */
	for (i = 0; i < MAX_PRECACHES; i++)
	{
		if (models[i].active && !strcmp(name, models[i].name))
		{
			return models + i;
		}
	}

	if ((!server && cls.ingame) || (server && svs.listening))
		Host_Error("Host_LoadModel: model \"%s\" not precached.\n", name);

	/* load it */
	for (i = 0; i < MAX_PRECACHES; i++)
	{
		if (!models[i].active)
		{
			models[i].active = true;
			if (name[0] != '*')
			{
				models[i].data = Sys_LoadModel(name, NULL);
			}
			else
			{
				precacheindex_t basemodel;

				/* TODO FIXME: anywhere where I scan something for shorts or bytes, use qualifiers instead of scanning for 32-bit %d and %u */
				Sys_Sscanf_s(name, "*%*d*%hd", &basemodel); /* ugly: *, ignore, *, precacheindex_t */

				if (!models[basemodel].active)
					Host_Error("Trying to load submodel %s without having loaded the basemodel.\n", name);

				models[i].data = Sys_LoadModel(name, models[basemodel].data);
			}
			Sys_Snprintf(models[i].name, MAX_MODEL_NAME, "%s", name);
			CL_CheckLoadingSlowness(); /* force updating to guarantee the loading messages */

			return models + i;
		}
	}

	Sys_Error ("Host_LoadModel: Couldn't load \"%s\", i == MAX_PRECACHES\n", name);
	return NULL; /* never reached */
}

/*
===================
Host_ModelsInit
===================
*/
void Host_ModelsInit(void)
{
	modelmarker = Sys_MemLowMark(&mdl_mem);
	Host_CleanModels();
}

/*
===================
Host_ModelsShutdown
===================
*/
void Host_ModelsShutdown(void)
{
	Host_CleanModels();
}
