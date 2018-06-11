/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Loading and precaching of resources

These functions should always load models in order, no blank spaces in the array.

============================================================================
*/

/*
===================
SV_GetModelIndex

Returns the model index. It's better for the game code to cache the result to avoid too many seeks.
===================
*/
precacheindex_t SV_GetModelIndex(const char *name)
{
	precacheindex_t	i;

	for (i = 0; i < svs.precached_models_num; i++)
		if (!strcmp(svs.precached_models[i], name))
			return i;

	Host_Error("SV_GetModelIndex: \"%s\" is not precached\n", name);
	return -1; /* never reached */
}

/*
===================
SV_GetModelEntities

Gets an entity list from inside the model.
Memory allocated from this function will only be
freed when a new server starts.
===================
*/
void SV_GetModelEntities(const precacheindex_t model, int *num_entities, model_entity_t **entities)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_GetModelEntities: model %d not precached\n", model);

	Sys_LoadModelEntities(svs.precached_models_data[model]->data, num_entities, entities);
}

/*
===================
SV_GetModelPhysicsTrimesh

Gets a trimesh from the model.
Memory allocated from this function will only be
freed when a new server starts.
===================
*/
void SV_GetModelPhysicsTrimesh(const precacheindex_t model, model_trimesh_t **trimesh)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_GetModelPhysicsTrimesh: model %d not precached\n", model);

	Sys_LoadModelPhysicsTrimesh(svs.precached_models_data[model]->data, trimesh);
}

/*
===================
SV_GetModelPhysicsBrushes

Gets a list of convex brushes from the model.
Memory allocated from this function will only be
freed when a new server starts.
===================
*/
void SV_GetModelPhysicsBrushes(const precacheindex_t model, model_brushes_t **brushes)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_GetModelPhysicsBrushes: model %d not precached\n", model);

	Sys_LoadModelPhysicsBrushes(svs.precached_models_data[model]->data, brushes);
}

/*
===================
SV_GetModelPhysicsHeightfield

Gets a heightfield from the model.
Memory allocated from this function will only be
freed when a new server starts.
===================
*/
void SV_GetModelPhysicsHeightfield(const precacheindex_t model, model_heightfield_t **heightfield)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_GetModelPhysicsHeightfield: model %d not precached\n", model);

	Sys_LoadModelPhysicsHeightfield(svs.precached_models_data[model]->data, heightfield);
}

/*
===================
SV_GetModelAABB

Gets the axis-aligned bounding boxes for the given model
===================
*/
void SV_GetModelAABB(const precacheindex_t model, const vec_t frame, vec3_t mins, vec3_t maxs)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_GetModelAABB: model %d not precached\n", model);

	Sys_ModelAABB(svs.precached_models_data[model]->data, frame, mins, maxs);
}

/*
===================
SV_GetModelTagTransform

Gets the transform data for the given model tag
===================
*/
void SV_GetModelTagTransform(const precacheindex_t model, const unsigned int tag_idx, const int local_coords, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, const int ent)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_GetModelTagTransform: model %d not precached\n", model);

	Sys_ModelGetTagTransform(svs.precached_models_data[model]->data, tag_idx, local_coords, origin, forward, right, up, ent);
}

/*
===================
SV_Animate

Animates a model
===================
*/
void SV_Animate(const precacheindex_t model, const int ent, vec3_t origin, vec3_t angles, vec_t *frames, const int anim_pitch)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_Animate: model %d not precached\n", model);

	Sys_ModelAnimate(svs.precached_models_data[model]->data, ent, origin, angles, frames, anim_pitch);
}

/*
===================
SV_AnimationInfo

Gets animation info from a model
===================
*/
void SV_AnimationInfo(const precacheindex_t model, const unsigned int animation, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_AnimationInfo: model %d not precached\n", model);

	Sys_ModelAnimationInfo(svs.precached_models_data[model]->data, animation, start_frame, num_frames, loop, frames_per_second, multiple_slots, vertex_animation);
}

/*
===================
SV_AnimationExists

Returns true if the animation exists in the model
===================
*/
int SV_AnimationExists(const precacheindex_t model, const unsigned int animation)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_AnimationExists: model %d not precached\n", model);

	return Sys_ModelAnimationExists(svs.precached_models_data[model]->data, animation);
}

/*
===================
SV_PointContents

Returns a bitfield with the contents of a point inside the volume defined by "ent"
===================
*/
int SV_PointContents(const precacheindex_t model, const vec3_t point)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_PointContents: model %d not precached\n", model);

	return Sys_ModelPointContents(svs.precached_models_data[model]->data, point);
}

/*
===================
SV_ModelHasPVS

Returns true if the model has a PVS implementation
===================
*/
int SV_ModelHasPVS(const precacheindex_t model)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_ModelHasPVS: model %d not precached\n", model);

	return Sys_ModelHasPVS(svs.precached_models_data[model]->data);
}

/*
===================
SV_ModelPVSGetClustersBox

Gets a list of clusters from the model that a box intercepts
===================
*/
void SV_ModelPVSGetClustersBox(const precacheindex_t model, const vec3_t absmins, const vec3_t absmaxs, int *clusters, int *num_clusters, const int max_clusters)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_ModelPVSGetClustersBox: model %d not precached\n", model);

	Sys_ModelPVSGetClustersBox(svs.precached_models_data[model]->data, absmins, absmaxs, clusters, num_clusters, max_clusters);
}

/*
===================
SV_ModelPVSCreateFatPVSClusters

Prepare visibility calculation for a given viewpoint
===================
*/
void SV_ModelPVSCreateFatPVSClusters(const precacheindex_t model, const vec3_t eyeorigin)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_ModelPVSCreateFatPVSClusters: model %d not precached\n", model);

	Sys_ModelPVSCreateFatPVSClusters(svs.precached_models_data[model]->data, eyeorigin);
}

/*
===================
SV_ModelPVSTestFatPVSClusters

Tests a cluster list for visibility, after creating the visibility data with SV_ModelPVSCreateFatPVSClusters
===================
*/
int SV_ModelPVSTestFatPVSClusters(const precacheindex_t model, const int *clusters, const int num_clusters)
{
	if (model < 0 || model >= svs.precached_models_num)
		Host_Error("SV_ModelPVSTestFatPVSClusters: model %d not precached\n", model);

	return Sys_ModelPVSTestFatPVSClusters(svs.precached_models_data[model]->data, clusters, num_clusters);
}

/*
===================
SV_Precache_Model

Loads the server-side data and stores the name in a list to send to the client. Should only be called by Game_SV_Precache_Model
===================
*/
void SV_Precache_Model(const char *name)
{
	precacheindex_t	i;

	if (name[0] == 0)
		Host_Error("SV_Precache_Model: empty name\n");

	if (svs.precached_models_num == MAX_PRECACHES)
		Host_Error("SV_Precache_Model: Too many precaches\n");

	for (i = 0; i < svs.precached_models_num; i++)
		if (!strcmp(svs.precached_models[i], name))
			return; /* already precached */

	if (svs.listening)
		Host_Error("SV_Precache_Model: loading already finished but tried to precache %s\n", name);

	Sys_Snprintf(svs.precached_models[svs.precached_models_num], MAX_MODEL_NAME, "%s", name);

	/* TODO CONSOLEDEBUG Sys_Printf("SV_Precache_Model: %s (%d)\n", name, svs.precached_models_num); */

	svs.precached_models_data[svs.precached_models_num] = Host_LoadModel(name, true);

	svs.precached_models_num++;
}

/*
===================
SV_GetSoundIndex

Returns the sound index. It's better for the game code to cache the result to avoid too many seeks.
===================
*/
precacheindex_t SV_GetSoundIndex(const char *name)
{
	precacheindex_t	i;

	for (i = 0; i < svs.precached_sounds_num; i++)
		if (!strcmp(svs.precached_sounds[i], name))
			return i;

	Host_Error("SV_GetSoundIndex: \"%s\" is not precached\n", name);
	return -1; /* never reached */
}

/*
===================
SV_Precache_Sound

Loads the server-side data and stores the name in a list to send to the client. Should only be called by Game_SV_Precache_Sound
===================
*/
void SV_Precache_Sound(const char *name)
{
	precacheindex_t i;

	if (name[0] == 0)
		Host_Error("SV_Precache_Sound: empty name\n");

	if (svs.precached_sounds_num == MAX_PRECACHES)
		Host_Error("SV_Precache_Sound: Too many precaches\n");

	for (i = 0; i < svs.precached_sounds_num; i++)
		if (!strcmp(svs.precached_sounds[i], name))
			return; /* already precached */

	if (svs.listening)
		Host_Error("SV_Precache_Sound: loading already finished but tried to precache %s\n", name);

	Sys_Snprintf(svs.precached_sounds[svs.precached_sounds_num], MAX_SOUND_NAME, "%s", name);

	/* TODO CONSOLEDEBUG Sys_Printf("SV_Precache_Sound: %s (%d)\n", name, svs.precached_sounds_num); */

	/* TODO: will we ever need server-side sound data? */

	svs.precached_sounds_num++;
}

/*
===================
SV_ClearAllPrecaches

Cleans the precache list in preparation for a new game
===================
*/
void SV_ClearAllPrecaches(void)
{
	svs.precached_models_num = 0;
	svs.precached_sounds_num = 0;
}
