/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alut.h> /* TODO FIXME: is alut deprecated? */

/*
============================================================================

System Specific Sound management

Channel = one of the sources of an entity
It makes more sense to use mono samples

TODO: alSpeedOfSound
TODO: possible to have weird sound volumes by firing, running and turning at the same time

============================================================================
*/

ALCdevice *device = NULL;
ALCcontext *context = NULL;

#define DEFAULT_LISTENER_POS {0, 0, 0}
#define DEFAULT_LISTENER_VEL {0, 0, 0}
#define DEFAULT_LISTENER_ANG {0, 0, 0}

/* OpenAL has a limited number of sources, we won't be able to map entities[channels] 1:1 */
#define MAX_SOUND_SOURCES		256

typedef struct sndsource_s {
	ALuint		source;
	int			ent_owner;
	int			ent_channel;
} sndsource_t;

sndsource_t sources[MAX_SOUND_SOURCES];

#define NUM_ENTITY_CHANNELS	4 /* channel meanings are game-defined (voice, weapon, etc...) - currently sent as a byte through the network */

typedef struct entsource_s {
	ALuint		sources[NUM_ENTITY_CHANNELS]; /* the allocated source for this entity */
} entsource_t;

entsource_t entsources[MAX_EDICTS + 1]; /* the extra is for local sounds */

/*
===================
Sys_SoundUpdateListener
===================
*/
void Sys_SoundUpdateListener(vec3_t pos, vec3_t vel, vec3_t angles)
{
	vec3_t forward, right, up;
	vec_t forwardup[6];

	alListenerfv(AL_POSITION, pos);
	alListenerfv(AL_VELOCITY, vel);

	Math_AnglesToVec(angles, forward, right, up);
	forwardup[0] = forward[0];
	forwardup[1] = forward[1];
	forwardup[2] = forward[2];
	forwardup[3] = up[0];
	forwardup[4] = up[1];
	forwardup[5] = up[2];
	alListenerfv(AL_ORIENTATION, forwardup);

	/* local sound TODO: better way to do this? */
	if (alIsSource(sources[MAX_EDICTS].source))
	{
		ALint state;
		alGetSourcei(sources[MAX_EDICTS].source, AL_SOURCE_STATE, &state);
		if (state == AL_PLAYING || state == AL_PAUSED)
		{
			alSourcefv(sources[MAX_EDICTS].source, AL_POSITION, pos);
			alSourcefv(sources[MAX_EDICTS].source, AL_VELOCITY, vel);
		}
	}

	/* TODO CONSOLEDEBUG Sys_Printf("org %f %f %f fwd %f %f %f up %f %f %f\n", pos[0], pos[1], pos[2], forwardup[0], forwardup[1], forwardup[2], forwardup[3], forwardup[4], forwardup[5]); */
}

/*
===================
Sys_SoundUpdateEntityAttribs

TODO: update more stuff (gain, etc)
===================
*/
void Sys_SoundUpdateEntityAttribs(void *entindexptr, vec3_t pos, vec3_t vel)
{
	entindex_t ent = *(entindex_t *)entindexptr;
	int i;

	if (ent < 0 || ent >= MAX_EDICTS)
		Host_Error("Sys_SoundUpdateEntityAttribs: invalid entity %d\n", ent);

	for (i = 0; i < NUM_ENTITY_CHANNELS; i++)
		if (alIsSource(entsources[ent].sources[i]))
		{
			ALint state;
			alGetSourcei(entsources[ent].sources[i], AL_SOURCE_STATE, &state);
			if (state == AL_PLAYING || state == AL_PAUSED)
			{
				alSourcefv(entsources[ent].sources[i], AL_POSITION, pos);
				alSourcefv(entsources[ent].sources[i], AL_VELOCITY, vel);
			}
		}
	/* TODO CONSOLEDEBUG Sys_Printf("ent %d org %f %f %f vel %f %f %f\n", ent, pos[0], pos[1], pos[2], vel[0], vel[1], vel[2]); */
}

/*
===================
Sys_StopAllSounds

Stops all sounds, including local sounds if desired
===================
*/
void Sys_StopAllSounds(int stoplocal)
{
	int i, j, maxedicts;

	for (i = 0; i < MAX_SOUND_SOURCES; i++)
	{
		if (alIsSource(sources[i].source))
		{
			if (!stoplocal && sources[i].ent_owner == MAX_EDICTS)
				continue; /* do not stop local sounds */
			alSourceStop(sources[i].source);
			alSourcei(sources[i].source, AL_BUFFER, 0);
			sources[i].ent_owner = -1;
			sources[i].ent_channel = -1;
		}
	}

	if (stoplocal)
		maxedicts = MAX_EDICTS + 1; /* stop local sounds */
	else
		maxedicts = MAX_EDICTS; /* do not stop local sounds */
	for (i = 0; i < maxedicts; i++)
		for (j = 0; j < NUM_ENTITY_CHANNELS; j++)
			entsources[i].sources[j] = 0; /* TODO: hope genSources never returns this */
}

/*
===================
Sys_PlaySound

Should only be called by CL_StartSound and CL_StartLocalSound
Entity -1 is local (client-side) source (origin, vel and channel are ignored then)
TODO: only 1 channel is enough for local sounds?
===================
*/
void Sys_PlaySound(unsigned int *snddata, void *entindex, vec3_t origin, vec3_t vel, int channel, vec_t pitch, vec_t gain, vec_t attenuation, int loop)
{
	int i;
	ALuint source;
	ALuint buffer = *snddata;
	entindex_t ent = *(entindex_t *)entindex;

	if (channel < 0 || channel >= NUM_ENTITY_CHANNELS)
		Host_Error("Sys_PlaySound: invalid channel %d for entity %d\n", channel, ent);

	if (ent < -1 || ent >= MAX_EDICTS)
		Host_Error("Sys_PlaySound: invalid entity %d\n", ent);

	if (ent == -1)
	{
		ent = MAX_EDICTS; /* TODO: hope it doesn't overflow */
		channel = 0; /* FIXME: only 1 enough for local? allocate a new channel for every new sound? */
	}

	if (entsources[ent].sources[channel])
	{
		source = entsources[ent].sources[channel];
		alSourceStop(source); /* need to stop it before setting a buffer, otherwise we will get an error */
	}
	else
	{
		for (i = 0; i < MAX_SOUND_SOURCES; i++)
		{
			ALint state;
			alGetSourcei(sources[i].source, AL_SOURCE_STATE, &state);
			if (state != AL_PLAYING && state != AL_PAUSED)
			{
				source = sources[i].source;

				/* remove from the old entity, if any */
				if (sources[i].ent_owner != -1)
					entsources[sources[i].ent_owner].sources[sources[i].ent_channel] = 0;

				/* assign to new entity */
				sources[i].ent_owner = ent;
				sources[i].ent_channel = channel;
				entsources[ent].sources[channel] = source;
				break;
			}
		}

		if (i == MAX_SOUND_SOURCES)
		{
			Sys_Printf("Sys_PlaySound: too many sounds, out of sound sources\n");
			return;
		}
	}

	alSourcef(source, AL_PITCH, pitch);
	alSourcef(source, AL_GAIN, gain);
	if (ent != MAX_EDICTS)
	{
		alSourcefv(source, AL_POSITION, origin);
		alSourcefv(source, AL_VELOCITY, vel);
	}
	else /* local sound TODO: better way to do this? */
	{
		vec3_t list_pos, list_vel;
		alGetListenerfv(AL_POSITION, list_pos);
		alGetListenerfv(AL_VELOCITY, list_vel);
		alSourcefv(source, AL_POSITION, list_pos);
		alSourcefv(source, AL_VELOCITY, list_vel);
	}
	if (loop)
		alSourcei(source, AL_LOOPING, AL_TRUE);
	else
		alSourcei(source, AL_LOOPING, AL_FALSE);
	alSourcef(source, AL_ROLLOFF_FACTOR, attenuation);
	alSourcei(source, AL_BUFFER, buffer);

	alSourcePlay(source);
}

/*
===================
Sys_StopSound

Should only be called by CL_StopSound and CL_StopLocalSound
Entity -1 is local (client-side) source (channel is ignored then)
TODO: only 1 channel is enough for local sounds?
===================
*/
void Sys_StopSound(void *entindex, int channel)
{
	entindex_t ent = *(entindex_t *)entindex;

	if (channel < 0 || channel >= NUM_ENTITY_CHANNELS)
		Host_Error("Sys_PlaySound: invalid channel %d for entity %d\n", channel, ent);

	if (ent < -1 || ent >= MAX_EDICTS)
		Host_Error("Sys_PlaySound: invalid entity %d\n", ent);

	if (ent == -1)
	{
		ent = MAX_EDICTS; /* TODO: hope it doesn't overflow */
		channel = 0; /* FIXME: only 1 enough for local? allocate a new channel for every new sound? */
	}

	if (entsources[ent].sources[channel])
		alSourceStop(entsources[ent].sources[channel]);
}

/*
===================
Sys_LoadSound

TODO: endianness
TODO: error if stereo sample!
===================
*/
void Sys_LoadSound(unsigned int *bufferptr, const char *name)
{
	int marker;
	char path[MAX_PATH];
	unsigned char *buffer;
	int size;

	marker = Sys_MemLowMark(&tmp_mem);

	Sys_Snprintf(path, MAX_PATH, "sounds/%s.wav", name);
	if ((size = Host_FSLoadBinaryFile(path, &tmp_mem, "sound", &buffer, false)) == -1)
		Host_Error("Sys_LoadSound: couldn't load %s\n", path);

	*bufferptr = alutCreateBufferFromFileImage(buffer, size); /* TODO: size + 1 returned from Host_FSLoadBinaryFile? The null-terminator should cause no problems */

	if (*bufferptr == AL_NONE)
		Sys_Error("Sys_LoadSound: error parsing or allocating %s: %s\n", path, alutGetErrorString(alutGetError()));

	Sys_MemFreeToLowMark(&tmp_mem, marker);
}

/*
===================
Sys_UnloadSound
===================
*/
void Sys_UnloadSound(void *data)
{
	ALuint *buffer = data;

	alDeleteBuffers(1, data);
}

/*
===================
Sys_SoundsInit
===================
*/
void Sys_SoundsInit(void)
{
	int i;
	vec3_t pos = DEFAULT_LISTENER_POS;
	vec3_t vel = DEFAULT_LISTENER_VEL;
	vec3_t angles = DEFAULT_LISTENER_ANG;
	const ALCubyte* device_specifier;
	int error;

	if (device || context)
		return;

	device = alcOpenDevice(NULL); /* open default audio device */
	context = alcCreateContext(device, NULL);
	alcMakeContextCurrent(context);

	Sys_SoundUpdateListener(pos, vel, angles);

	for (i = 0; i < MAX_SOUND_SOURCES; i++)
	{
		error = alGetError();
		if (error)
			Sys_Error("Sys_SoundsInit: OpenAL Error: %x\n", error);
		alGenSources(1, &sources[i].source);
	}

	alutInitWithoutContext(NULL, NULL);

	Sys_StopAllSounds(true); /* to clean up */

	error = alcGetError(device);
	if (error != ALC_NO_ERROR)
		Sys_Error("Sys_SoundsInit: OpenAL Context Error: %x\n", error);

    device_specifier = alcGetString(device, ALC_DEVICE_SPECIFIER);
	Sys_Printf("OpenAL initialized using: %s\n", device_specifier);
}

/*
===================
Sys_SoundsShutdown

All sounds must be unloaded before calling this
===================
*/
void Sys_SoundsShutdown(void)
{
	int i;

	if (!context || !device)
		return;

	Sys_StopAllSounds(true);

	alutExit();

	if (context)
	{
		alcMakeContextCurrent(NULL);
		alcDestroyContext(context);
		context = NULL;
	}
	if (device)
	{
		alcCloseDevice(device);
		device = NULL;
	}

	for (i = 0; i < MAX_SOUND_SOURCES; i++)
		alDeleteSources(1, &sources[i].source);
}
