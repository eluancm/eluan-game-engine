/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Customizable sound data

============================================================================
*/

/*
===================
model_sound

Return a sound effect depending on the material of the model surface
TODO: determine materials and change sound effects, precache only what's needed, change this to the function actually playing the sound to force some settings?
TODO: return default when material doesn't specify the requested sound type?
TODO: modelinternid (for passing a id for mesh, face, brush, etc returned from physics code) to see get the material in multimaterial models
like maps? or maybe just pass a point? which is faster?
TODO: different sound with and without armor for pain and die? or put these in the ricochet/spark sound?
===================
*/
precacheindex_t model_sound(int modelindex, int sound_type)
{
	if (!gs.model_sounds_fileloaded[modelindex])
		Host_Error("model sounds for model %d not loaded (sound_type %d)\n", modelindex, sound_type);

	if (sound_type < 0 || sound_type >= NUM_MODEL_SOUNDS)
		Host_Error("model sound %d is out of range (modelindex %d)\n", sound_type, modelindex);

	if (!gs.model_sounds_loaded[modelindex][sound_type])
		Host_Error("model sound unknown sound_type: %d (modelindex %d)\n", sound_type, modelindex);

	return gs.model_sounds[modelindex][sound_type];
}

/*
===================
model_sound_precache

Precaches the customizable sounds
TODO: use pathname instead of modelname, to have everything in the same folder (and allow a map and model with the same name)
TODO: in the file, use SOUND_* instead of the raw index! this is somewhat important
===================
*/
void model_sound_precache(const char *modelname)
{
	precacheindex_t modelindex = SV_GetModelIndex(modelname);
	int marker = Sys_MemLowMark(&tmp_mem);
	char modelsoundspath[MAX_PATH];
	unsigned char *modelsoundsdata;
	unsigned char *modelsoundsdataend;
	int modelsoundssize;

	if (gs.model_sounds_fileloaded[modelindex])
		return;

	Sys_Snprintf(modelsoundspath, MAX_PATH, "sounds/%s_sounds.txt", modelname);

	if ((modelsoundssize = Host_FSLoadBinaryFile(modelsoundspath, &tmp_mem, "modelsounddata", &modelsoundsdata, true)) != -1)
	{
		int modelsoundslines = 0;
		modelsoundsdataend = modelsoundsdata + modelsoundssize;
		while (1)
		{
			int modelsounds_index;
			char modelsounds_name[MAX_SOUND_NAME];

			if (modelsoundsdata >= modelsoundsdataend)
				break;
#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
			if (!Sys_Sscanf_s((const char *)modelsoundsdata, "%d %s", &modelsounds_index, modelsounds_name))
#else
			if (!Sys_Sscanf_s((const char *)modelsoundsdata, "%d %s", &modelsounds_index, modelsounds_name, sizeof(modelsounds_name)))
#endif /* __GNUC__ */
				break;
			modelsoundsdata = Host_CMDSkipBlank(modelsoundsdata);
			modelsoundslines++;

			if (modelsounds_index < 0 || modelsounds_index >= NUM_MODEL_SOUNDS)
			{
				Host_Error("%s: model sound %d is out of range (max %d) name = \"%s\"\n at line %d\n", modelsoundspath, modelsounds_index, NUM_MODEL_SOUNDS, modelsounds_name, modelsoundslines); /* TODO: line counting NOT accurate */
			}
			else
			{
				if (gs.model_sounds_loaded[modelindex][modelsounds_index]) /* avoid wasting time */
					continue;
				SV_Precache_Sound(modelsounds_name);
				gs.model_sounds[modelindex][modelsounds_index] = SV_GetSoundIndex(modelsounds_name);
				gs.model_sounds_loaded[modelindex][modelsounds_index] = true;
			}
		}
		/* TODO CONSOLEDEBUG Sys_Printf("%s: loaded modelsounds in \"%s\": %d lines\n", modelname, modelsoundspath, modelsoundslines); */
	}
	else
	{
		/* TODO CONSOLEDEBUG Sys_Printf("%s: no modelsounds found (\"%s\" doesn't exist)\n", modelname, modelsoundspath); */
		/* already cleared */
	}
	Sys_MemFreeToLowMark(&tmp_mem, marker);

	gs.model_sounds_fileloaded[modelindex] = true;
}
