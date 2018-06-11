/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Sound management

Same observations as with texture loading.
TODO: spatial sound, effects for when on another room, underwater, etc
TODO: doppler effect (need to set velocity for each source via network is enough?), sound origin updating (need id system, see below)
TODO: output/input device selection? or leave that to the operating system?
TODO: listener at camera position or player position? configurable?
TODO: have some means of detecting if a entity associated with a sound was deleted and replaced by another with the same index (for position/velocity updates in sound)
TODO: pause all sounds when the game is paused (server needs to say so!)
TODO: altering pitch, effects, etc...
TODO: if an entity occupies the same index as another removed entity, the new sounds may overwrite the old ones - WAIT FOR THE SOUND TO FINISH BEFORE REMOVING AN ENTITY!!! (make it invisible if needed)
TODO: sounds may be cached twice: one in local sounds and another in server sounds
TODO: extra updates if we are running slow (is it multithreaded and done automatically?)
TODO: UPDATE SOUND ORIGINS FOR SERVER ENTITIES NOT ON PVS? PHS?

============================================================================
*/

/*
	local sounds never get cleared (menu sounds, etc)
	server sounds are cleared at each new connection
	both structs should have the SAME size
*/
sfx_t		local_sounds[MAX_PRECACHES];
sfx_t		server_sounds[MAX_PRECACHES];
int			server_soundmarker;

/*
===================
CL_CleanSounds

Everything loaded by instruction of the server will be cleared, system should take care of caching disk data.
===================
*/
void CL_CleanSounds(int clean_local)
{
	precacheindex_t i;

	for (i = 0; i < MAX_PRECACHES; i++)
	{
		if (server_sounds[i].active)
			Sys_UnloadSound(&server_sounds[i].data);

		server_sounds[i].active = false;
		server_sounds[i].name[0] = 0;
		server_sounds[i].data = 0;
	}
	Sys_MemFreeToLowMark(&snd_mem, server_soundmarker);

	if (clean_local)
		for (i = 0; i < MAX_PRECACHES; i++)
		{
			if (local_sounds[i].active)
				Sys_UnloadSound(&local_sounds[i].data);

			local_sounds[i].active = false;
			local_sounds[i].name[0] = 0;
			local_sounds[i].data = 0;
		}
}

/*
===================
CL_StartSound
===================
*/
void CL_StartSound(sfx_t *snd, entindex_t ent, vec3_t origin, vec3_t vel, int channel, vec_t pitch, vec_t gain, vec_t attenuation, int loop)
{
	if (ent < -1 || ent >= MAX_EDICTS)
		Host_Error("CL_StartSound: invalid entity %d\n", ent);

	if (!snd)
		Host_Error("CL_StartSound: invalid sound for entity %d\n", ent);

	if (!snd->active)
		Host_Error("CL_StartSound: sound for entity %d not active\n", ent);

	Sys_PlaySound(&snd->data, &ent, origin, vel, channel, pitch, gain, attenuation, loop);
}

/*
===================
CL_StopSound
===================
*/
void CL_StopSound(entindex_t ent, int channel)
{
	if (ent < -1 || ent >= MAX_EDICTS)
		Host_Error("CL_StartSound: invalid entity %d\n", ent);

	Sys_StopSound(&ent, channel);
}

/*
===================
CL_StartLocalSound
===================
*/
void CL_StartLocalSound(sfx_t *snd)
{
	CL_StartSound(snd, -1, NULL, NULL, 0, 1, 1, 0, false);
}

/*
===================
CL_StopLocalSound

TODO: test
===================
*/
void CL_StopLocalSound(void)
{
	CL_StopSound(-1, 0);
}

/*
===================
CL_LoadSound

May be called at will to get the id of a previously loaded sound
===================
*/
sfx_t *CL_LoadSound(const char *name, int local)
{
	precacheindex_t i;
	sfx_t *soundptr;

	if (strlen (name) + 1 >= MAX_SOUND_NAME)
		Host_Error("CL_LoadSound: name too big: %s\n", name);

	if (local)
		soundptr = local_sounds;
	else
		soundptr = server_sounds;

	/* see if it's already loaded */
	for (i = 0; i < MAX_PRECACHES; i++)
	{
		if (soundptr[i].active && !strcmp(name, soundptr[i].name))
		{
			return soundptr + i;
		}
	}

	if (cls.ingame && !local) /* change this if the server will load it TODO: look for other resources being loaded mid-game! */
		Host_Error("CL_LoadSound: sound \"%s\" not precached.\n", name); /* TODO: require local preload? */

	/* load it */
	for (i = 0; i < MAX_PRECACHES; i++)
	{
		if (!soundptr[i].active)
		{
			soundptr[i].active = true;
			Sys_LoadSound(&soundptr[i].data, name);
			Sys_Snprintf(soundptr[i].name, MAX_SOUND_NAME, "%s", name);

			/* TODO CONSOLEDEBUG Sys_Printf("Loaded sound %d (sysid %d) \"%s\"\n", i, soundptr[i].data, name); */

			return soundptr + i;
		}
	}

	Sys_Error ("CL_LoadSound: Couldn't load \"%s\", i == MAX_PRECACHES\n", name);
	return NULL; /* never reached */
}

/*
===================
CL_SoundsInit
===================
*/
void CL_SoundsInit(void)
{
	Sys_SoundsInit();
	server_soundmarker = Sys_MemLowMark(&snd_mem);
	memset(local_sounds, 0, sizeof(local_sounds));
	memset(server_sounds, 0, sizeof(server_sounds));
	CL_CleanSounds(true);
}

/*
===================
CL_SoundsShutdown
===================
*/
void CL_SoundsShutdown(void)
{
	CL_CleanSounds(true);
	Sys_SoundsShutdown();
}