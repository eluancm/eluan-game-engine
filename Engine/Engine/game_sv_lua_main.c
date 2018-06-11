/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

#include "lua/src/lua.h"
#include "lua/src/lauxlib.h"
#include "lua/src/lualib.h"

/* should be kept in sync with the definitions in game_shared.h/animation.lua */
const char *animation_names[NUM_ANIMATIONS] =
{
	"Base",
	"Idle",
	"Fire",
	"Fire2",
	"Fire3",
	"Fire4",
	"Fire5",
	"Fire6",
	"Fire7",
	"Fire8",
	"FireEmpty",
	"Reload",
	"WeaponActivate",
	"WeaponDeactivate",
	"Run",
	"RunRight",
	"RunLeft",
};

/* should be kept in sync with the definitions in game_shared.h/animation.lua and ANIMATION_MAX_BLENDED_FRAMES in host.h */
const char *animation_slot_names[ANIMATION_MAX_BLENDED_FRAMES] =
{
	"AllJoints",
	"Arms",
	"Legs",
	"Pelvis",
	"Torso"
};

/* should be kept in sync with the definitions in game_shared.h/animation.lua TODO: does this belong in this file? */
const char *model_tag_names[NUM_MODEL_TAGS] =
{
	"RightHand",
	"LeftHand"
};

/* TODO: not thread safe? */
lua_State *sv_lua_state = NULL;

/*
============================================================================

Progs built-in functions

Functions called from the VM for speed or scope reasons

============================================================================
*/

/*
===================
PR_PrecacheModel

Proxy for SV_Precache_Model.
input: char *name
output: none
===================
*/
int PR_PrecacheModel(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *name;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_PrecacheModel: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	name = ProgsGetStringFromStack(L, 1, false);

	SV_Precache_Model(name);
	model_sound_precache(name); /* also load model sounds, if any */

	return 0;
}

/*
===================
PR_PrecacheSound

Proxy for SV_Precache_Sound.
input: char *name
output: none
===================
*/
int PR_PrecacheSound(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *name;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_PrecacheSound: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	name = ProgsGetStringFromStack(L, 1, false);

	SV_Precache_Sound(name);

	return 0;
}

/*
===================
PR_GetModelIndex

Wrapper to SV_GetModelIndex
input: char *model
output: int modelindex
===================
*/
int PR_GetModelIndex(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *model;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_GetModelIndex: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	model = ProgsGetStringFromStack(L, 1, false);

	ProgsInsertIntegerIntoStack(L, SV_GetModelIndex(model));
	return 1; /* number of return values in the stack */
}

/*
===================
PR_GetSoundIndex

Wrapper to SV_GetSoundIndex
input: char *sound
output: int soundindex
===================
*/
int PR_GetSoundIndex(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *sound;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_GetSoundIndex: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	sound = ProgsGetStringFromStack(L, 1, false);

	ProgsInsertIntegerIntoStack(L, SV_GetSoundIndex(sound));
	return 1; /* number of return values in the stack */
}

/*
===================
PR_GetModelSoundIndex

Wrapper to model_sound
input: int modelindex, int soundtype
output: int soundindex
===================
*/
int PR_GetModelSoundIndex(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	int modelindex;
	int soundtype;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_GetSoundIndex: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	modelindex = ProgsGetIntegerFromStack(L, 1);
	soundtype = ProgsGetIntegerFromStack(L, 2);

	ProgsInsertIntegerIntoStack(L, model_sound(modelindex, soundtype));
	return 1; /* number of return values in the stack */
}

/*
===================
PR_GetModelEntities

Get the entities embedded into a model
input: int modelindex
output: table containing (key, value) string pairs for each entity
===================
*/
int PR_GetModelEntities(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	precacheindex_t modelindex;

	int i, j, num_entities;
	model_entity_t *entities;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_GetModelEntities: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	modelindex = (precacheindex_t)ProgsGetIntegerFromStack(L, 1);

	SV_GetModelEntities(modelindex, &num_entities, &entities);

	/* create table */
	lua_newtable(L);
	for (i = 0; i < num_entities; i++)
	{
		/* push index and create subtable */
		lua_pushinteger(L, i + 1); /* start at 1, to use ipairs (iterate from 1 to the first nil) in lua */
		lua_newtable(L);
		/* TODO CONSOLEDEBUG Sys_Printf("Entity %d:\n", i); */
		for (j = 0; j < entities[i].num_attribs; j++)
		{
			int k, len;

			/* remove quotes from attirb and value TODO: only if there are quotes */
			len = strlen(entities[i].values[j]);
			for (k = 0; k < len - 2; k++)
				entities[i].values[j][k] = entities[i].values[j][k + 1];
			entities[i].values[j][k] = 0;

			len = strlen(entities[i].attribs[j]);
			for (k = 0; k < len - 2; k++)
				entities[i].attribs[j][k] = entities[i].attribs[j][k + 1];
			entities[i].attribs[j][k] = 0;

			/* key */
			lua_pushstring(L, entities[i].attribs[j]);
			/* value */
			lua_pushstring(L, entities[i].values[j]);
			/* set and pop */
			lua_settable(L, -3);
			/* TODO CONSOLEDEBUG Sys_Printf(" %s = %s\n", entities[i].attribs[j], entities[i].values[j]); */
		}
		/* put the entity in the master table */
		lua_settable(L, -3);
	}

	Sys_Printf("Model %d has %d entities.\n", modelindex, num_entities);

	return 1;
}

/*
===================
PR_GetModelAABB

Get the AABB of a given frame of the model
input: int modelindex, vec_t frame
output: vec3_t mins, vec3_t maxs
===================
*/
int PR_GetModelAABB(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	precacheindex_t modelindex;
	vec_t frame;

	vec3_t mins, maxs;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_GetModelAABB: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	modelindex = (precacheindex_t)ProgsGetIntegerFromStack(L, 1);
	frame = ProgsGetRealFromStack(L, 2);

	SV_GetModelAABB(modelindex, frame, mins, maxs);

	ProgsInsertVector3IntoStack(L, mins);
	ProgsInsertVector3IntoStack(L, maxs);

	return 2;
}

/*
===================
PR_GetModelTagTransform

Wrapper to SV_GetModelTagTransform
input: int modelindex, unsigned int tag_idx, int local_coords, boolean origin, boolean forward, boolean right, boolean up, int ent
output: variable, depends on each of origin, forward, right and up being true or false. For the true ones, return in the same order
===================
*/
int PR_GetModelTagTransform(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	precacheindex_t modelindex;
	unsigned int tag_idx;
	int local_coords, origin, forward, right, up;
	entindex_t ent;

	int num_outs = 0;
	vec3_t origin_out, forward_out, right_out, up_out;

	nArgs = lua_gettop(L);
	if (nArgs != 8)
	{
		ProgsInsertStringIntoStack(L, "PR_GetModelTagTransform: invalid number of arguments, should be 8");
		lua_error(L);
		return 0;
	}
	modelindex = (precacheindex_t)ProgsGetIntegerFromStack(L, 1);
	tag_idx = ProgsGetIntegerFromStack(L, 2);
	local_coords = ProgsGetBooleanFromStack(L, 3);
	origin = ProgsGetBooleanFromStack(L, 4);
	forward = ProgsGetBooleanFromStack(L, 5);
	right = ProgsGetBooleanFromStack(L, 6);
	up = ProgsGetBooleanFromStack(L, 7);
	ent = (entindex_t)ProgsGetIntegerFromStack(L, 8);

	SV_GetModelTagTransform(modelindex, tag_idx, local_coords, origin ? origin_out : NULL, forward ? forward_out : NULL, right ? right_out : NULL, up ? up_out : NULL, ent);

	if (origin)
	{
		num_outs++;
		ProgsInsertVector3IntoStack(L, origin_out);
	}
	if (forward)
	{
		num_outs++;
		ProgsInsertVector3IntoStack(L, forward_out);
	}
	if (right)
	{
		num_outs++;
		ProgsInsertVector3IntoStack(L, right_out);
	}
	if (up)
	{
		num_outs++;
		ProgsInsertVector3IntoStack(L, up_out);
	}

	return num_outs;
}

/*
===================
PR_Animate

Animates a model (to get up-to-date tag locations, etc)
input: int modelindex, entindex_t ent, vec3_t origin, vec3_t angles, vec_t *frames
output: none
===================
*/
int PR_Animate(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int i;

	precacheindex_t modelindex;
	entindex_t ent;
	vec3_t origin, angles;
	vec_t frames[ANIMATION_MAX_BLENDED_FRAMES];
	int anim_pitch;

	nArgs = lua_gettop(L);
	if (nArgs != 6)
	{
		ProgsInsertStringIntoStack(L, "PR_Animate: invalid number of arguments, should be 6");
		lua_error(L);
		return 0;
	}
	modelindex = (precacheindex_t)ProgsGetIntegerFromStack(L, 1);
	ent = (entindex_t)ProgsGetIntegerFromStack(L, 2);
	ProgsGetVector3FromStack(L, 3, -1, origin, false);
	ProgsGetVector3FromStack(L, 4, -1, angles, false);

	for (i = 0; i < ANIMATION_MAX_BLENDED_FRAMES; i++)
	{
		if (lua_istable(sv_lua_state, 5))
		{
			lua_pushinteger(sv_lua_state, i);
			lua_gettable(sv_lua_state, 5);
			if (lua_isnumber(sv_lua_state, -1))
			{
				vec_t value = (vec_t)lua_tonumber(sv_lua_state, -1);
				lua_pop(sv_lua_state, 1);
				frames[i] = value;
			}
			else
			{
				lua_pop(sv_lua_state, 1);
				frames[i] = 0;
			}
		}
		else
		{
			frames[i] = 0;
		}
	}

	anim_pitch = ProgsGetBooleanFromStack(L, 5);

	SV_Animate(modelindex, ent, origin, angles, frames, anim_pitch);

	return 0;
}

/*
===================
PR_AnimationInfo

Get the information about a specific animation of the model
input: int modelindex, int animation
output: int start_frame, int num_frames, boolean loop, vec_t fps, boolean multiple_slots, boolean vertex_animation
===================
*/
int PR_AnimationInfo(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	precacheindex_t modelindex;
	unsigned int animation;

	unsigned int start_frame, num_frames;
	int loop;
	vec_t frames_per_second;
	int multiple_slots;
	int vertex_animation;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_AnimationInfo: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	modelindex = (precacheindex_t)ProgsGetIntegerFromStack(L, 1);
	animation = ProgsGetIntegerFromStack(L, 2);

	SV_AnimationInfo(modelindex, animation, &start_frame, &num_frames, &loop, &frames_per_second, &multiple_slots, &vertex_animation);

	ProgsInsertIntegerIntoStack(L, start_frame);
	ProgsInsertIntegerIntoStack(L, num_frames);
	ProgsInsertBooleanIntoStack(L, loop);
	ProgsInsertRealIntoStack(L, frames_per_second);
	ProgsInsertBooleanIntoStack(L, multiple_slots);
	ProgsInsertBooleanIntoStack(L, vertex_animation);

	return 6;
}

/*
===================
PR_AnimationExists

Check if a specific animation exists in the model
input: int modelindex, int animation
output: boolean exists
===================
*/
int PR_AnimationExists(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	precacheindex_t modelindex;
	unsigned int animation;

	int exists;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_AnimationExists: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	modelindex = (precacheindex_t)ProgsGetIntegerFromStack(L, 1);
	animation = ProgsGetIntegerFromStack(L, 2);

	exists = SV_AnimationExists(modelindex, animation);

	ProgsInsertBooleanIntoStack(L, exists);

	return 1;
}

/*
===================
PR_PointContents

Wrapper to SV_PointContents
input: int modelindex, vec3_t origin
output: int pointcontents
===================
*/
int PR_PointContents(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	precacheindex_t modelindex;
	vec3_t origin;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_PointContents: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	modelindex = (precacheindex_t)ProgsGetIntegerFromStack(L, 1);
	ProgsGetVector3FromStack(L, 2, -1, origin, false);

	ProgsInsertIntegerIntoStack(L, SV_PointContents(modelindex, origin));

	return 1; /* number of return values in the stack */
}

/*
===================
PR_PhysicsApplyImpulse

Wrapper to Sys_PhysicsApplyImpulse
input: int entity, vec3_t impulse, vec3_t origin_local_space
output: none
===================
*/
int PR_PhysicsApplyImpulse(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t entity;
	vec3_t impulse, origin_localspace;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsApplyImpulse: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 1);
	ProgsGetVector3FromStack(L, 2, -1, impulse, false);
	ProgsGetVector3FromStack(L, 3, -1, origin_localspace, false);

	Sys_PhysicsApplyImpulse(svs.physworld, entity, impulse, origin_localspace);

	return 0; /* number of return values in the stack */
}

/*
===================
PR_PhysicsSetLinearVelocity

Wrapper to Sys_PhysicsSetLinearVelocity
input: int entity, vec3_t velocity
output: none
===================
*/
int PR_PhysicsSetLinearVelocity(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t entity;
	vec3_t velocity;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsSetLinearVelocity: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 1);
	ProgsGetVector3FromStack(L, 2, -1, velocity, false);

	Sys_PhysicsSetLinearVelocity(svs.physworld, entity, velocity);

	return 0; /* number of return values in the stack */
}

/*
===================
PR_PhysicsSetAngularVelocity

Wrapper to Sys_PhysicsSetAngularVelocity
input: int entity, vec3_t avelocity
output: none
===================
*/
int PR_PhysicsSetAngularVelocity(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t entity;
	vec3_t avelocity;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsSetAngularVelocity: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 1);
	ProgsGetVector3FromStack(L, 2, -1, avelocity, false);

	Sys_PhysicsSetAngularVelocity(svs.physworld, entity, avelocity);

	return 0; /* number of return values in the stack */
}

/*
===================
PR_PhysicsCreateFromModel

Creates a physical representation for this entity from the data in the model
input: int entity, int modelindex, vec_t mass, vec3_t origin, vec3_t angles, vec3_t locked_angles, int trace_onground
output: boolean success
===================
*/
int PR_PhysicsCreateFromModel(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t entity;
	precacheindex_t modelindex;
	vec_t mass;
	vec3_t origin, angles, locked_angles;
	vec_t *origin_ptr, *angles_ptr, *locked_angles_ptr;
	int trace_onground;

	int success = false;

	nArgs = lua_gettop(L);
	if (nArgs != 7)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsCreateFromModel: invalid number of arguments, should be 7");
		lua_error(L);
		return 0;
	}
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 1);
	modelindex = (precacheindex_t)ProgsGetIntegerFromStack(L, 2);
	mass = ProgsGetRealFromStack(L, 3);
	origin_ptr = ProgsGetVector3FromStack(L, 4, -1, origin, true);
	angles_ptr = ProgsGetVector3FromStack(L, 5, -1, angles, true);
	locked_angles_ptr = ProgsGetVector3FromStack(L, 6, -1, locked_angles, true);
	trace_onground = ProgsGetBooleanFromStack(L, 7);

	/* order of preference: heightfield, brushes, trimeshes */
	if (Sys_PhysicsCreateObject(svs.physworld, entity, PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL, &modelindex, mass, origin_ptr, angles_ptr, locked_angles_ptr, trace_onground))
	{
		success = true;
		goto physicsdone;
	}

	if (Sys_PhysicsCreateObject(svs.physworld, entity, PHYSICS_SHAPE_CONVEXHULLS_FROM_MODEL, &modelindex, mass, origin_ptr, angles_ptr, locked_angles_ptr, trace_onground))
	{
		success = true;
		goto physicsdone;
	}

	if (Sys_PhysicsCreateObject(svs.physworld, entity, PHYSICS_SHAPE_TRIMESH_FROM_MODEL, &modelindex, mass, origin_ptr, angles_ptr, locked_angles_ptr, trace_onground))
	{
		success = true;
		goto physicsdone;
	}
physicsdone:

	ProgsInsertBooleanIntoStack(L, success);
	return 1; /* number of return values in the stack */
}

/*
===================
PR_PhysicsCreateFromData

Creates a physical representation for this entity from the data
input: int entity, int type, * data, vec_t mass, vec3_t origin, vec3_t angles, vec3_t locked_angles, int trace_onground
output: none
===================
*/
int PR_PhysicsCreateFromData(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t entity;
	int type;
	void *data;
	vec3_t data123; /* 123 = vec, vec2, vec3 */
	phys_edict_vehicle_info_t datavehicle1;
	vec_t mass;
	vec3_t origin, angles, locked_angles;
	vec_t *origin_ptr, *angles_ptr, *locked_angles_ptr;
	int trace_onground;

	nArgs = lua_gettop(L);
	if (nArgs != 8)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsCreateFromData: invalid number of arguments, should be 8");
		lua_error(L);
		return 0;
	}
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 1);
	type = ProgsGetIntegerFromStack(L, 2);
	switch (type)
	{
		case PHYSICS_SHAPE_BOX:
			data = ProgsGetVector3FromStack(L, 3, -1, data123, false);
			break;
		case PHYSICS_SHAPE_SPHERE:
			data123[0] = ProgsGetRealFromStack(L, 3);
			data = &data123[0];
			break;
		case PHYSICS_SHAPE_CAPSULE_Y:
			data = ProgsGetVector2FromStack(L, 3, -1, data123, false);
			break;
		case PHYSICS_SHAPE_VEHICLE1:
			luaL_checktype(L, 3, LUA_TTABLE);
			lua_getfield(L, 3, "wheelDirectionCS0");
			lua_getfield(L, 3, "wheelAxleCS");
			lua_getfield(L, 3, "gEngineForce");
			lua_getfield(L, 3, "defaultBreakingForce");
			lua_getfield(L, 3, "handBrakeBreakingForce");
			lua_getfield(L, 3, "gBreakingForce");
			lua_getfield(L, 3, "maxEngineForce");
			lua_getfield(L, 3, "gVehicleSteering");
			lua_getfield(L, 3, "steeringIncrement");
			lua_getfield(L, 3, "steeringClamp");
			lua_getfield(L, 3, "wheelRadius");
			lua_getfield(L, 3, "wheelWidth");
			lua_getfield(L, 3, "wheelFriction");
			lua_getfield(L, 3, "suspensionStiffness");
			lua_getfield(L, 3, "suspensionDamping");
			lua_getfield(L, 3, "suspensionCompression");
			lua_getfield(L, 3, "maxSuspensionTravelCm");
			lua_getfield(L, 3, "maxSuspensionForce");
			lua_getfield(L, 3, "rollInfluence");
			lua_getfield(L, 3, "suspensionRestLength");
			lua_getfield(L, 3, "connectionHeight");
			lua_getfield(L, 3, "connectionStickLateralOutWheelWidthMultiplier");
			lua_getfield(L, 3, "connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier");
			lua_getfield(L, 3, "chassis_box_half_extents");
			lua_getfield(L, 3, "chassis_box_localpos");
			lua_getfield(L, 3, "suppchassis_box_half_extents");
			lua_getfield(L, 3, "suppchassis_box_localpos");
			lua_getfield(L, 3, "wheel_ents0");
			lua_getfield(L, 3, "wheel_ents1");
			lua_getfield(L, 3, "wheel_ents2");
			lua_getfield(L, 3, "wheel_ents3");
			lua_getfield(L, 3, "wheel_drive");

			ProgsGetVector3FromStack(L, -32, -1, datavehicle1.wheelDirectionCS0, false);
			ProgsGetVector3FromStack(L, -31, -1, datavehicle1.wheelAxleCS, false);
			datavehicle1.gEngineForce = ProgsGetRealFromStack(L, -30);
			datavehicle1.defaultBreakingForce = ProgsGetRealFromStack(L, -29);
			datavehicle1.handBrakeBreakingForce = ProgsGetRealFromStack(L, -28);
			datavehicle1.gBreakingForce = ProgsGetRealFromStack(L, -27);
			datavehicle1.maxEngineForce = ProgsGetRealFromStack(L, -26);
			datavehicle1.gVehicleSteering = ProgsGetRealFromStack(L, -25);
			datavehicle1.steeringIncrement = ProgsGetRealFromStack(L, -24);
			datavehicle1.steeringClamp = ProgsGetRealFromStack(L, -23);
			datavehicle1.wheelRadius = ProgsGetRealFromStack(L, -22);
			datavehicle1.wheelWidth = ProgsGetRealFromStack(L, -21);
			datavehicle1.wheelFriction = ProgsGetRealFromStack(L, -20);
			datavehicle1.suspensionStiffness = ProgsGetRealFromStack(L, -19);
			datavehicle1.suspensionDamping = ProgsGetRealFromStack(L, -18);
			datavehicle1.suspensionCompression = ProgsGetRealFromStack(L, -17);
			datavehicle1.maxSuspensionTravelCm = ProgsGetRealFromStack(L, -16);
			datavehicle1.maxSuspensionForce = ProgsGetRealFromStack(L, -15);
			datavehicle1.rollInfluence = ProgsGetRealFromStack(L, -14);
			datavehicle1.suspensionRestLength = ProgsGetRealFromStack(L, -13);
			datavehicle1.connectionHeight = ProgsGetRealFromStack(L, -12);
			datavehicle1.connectionStickLateralOutWheelWidthMultiplier = ProgsGetRealFromStack(L, -11);
			datavehicle1.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier = ProgsGetRealFromStack(L, -10);
			ProgsGetVector3FromStack(L, -9, -1, datavehicle1.chassis_box_half_extents, false);
			ProgsGetVector3FromStack(L, -8, -1, datavehicle1.chassis_box_localpos, false);
			ProgsGetVector3FromStack(L, -7, -1, datavehicle1.suppchassis_box_half_extents, false);
			ProgsGetVector3FromStack(L, -6, -1, datavehicle1.suppchassis_box_localpos, false);
			datavehicle1.wheel_ents[0] = ProgsGetIntegerFromStack(L, -5);
			datavehicle1.wheel_ents[1] = ProgsGetIntegerFromStack(L, -4);
			datavehicle1.wheel_ents[2] = ProgsGetIntegerFromStack(L, -3);
			datavehicle1.wheel_ents[3] = ProgsGetIntegerFromStack(L, -2);
			datavehicle1.wheel_drive = ProgsGetIntegerFromStack(L, -1);

			lua_pop(L, 32);

			data = &datavehicle1;
			break;
		case PHYSICS_SHAPE_TRIMESH_FROM_DATA:
		case PHYSICS_SHAPE_CONVEXHULLS_FROM_DATA:
		case PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA:
		case PHYSICS_SHAPE_VOXEL_BOX:
		case PHYSICS_SHAPE_VOXEL_TRIMESH:
		default:
			ProgsInsertStringIntoStack(L, "PR_PhysicsCreateFromData: unsupported type");
			lua_error(L);
			return 0;
	}
	mass = ProgsGetRealFromStack(L, 4);
	origin_ptr = ProgsGetVector3FromStack(L, 5, -1, origin, true);
	angles_ptr = ProgsGetVector3FromStack(L, 6, -1, angles, true);
	locked_angles_ptr = ProgsGetVector3FromStack(L, 7, -1, locked_angles, true);
	trace_onground = ProgsGetBooleanFromStack(L, 8);

	Sys_PhysicsCreateObject(svs.physworld, entity, type, data, mass, origin_ptr, angles_ptr, locked_angles_ptr, trace_onground);

	return 0; /* number of return values in the stack */
}

/*
===================
PR_PhysicsDestroy

Destroys a physical representation for this entity
input: int entity
output: none
===================
*/
int PR_PhysicsDestroy(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t entity;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsDestroy: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 1);

	Sys_PhysicsDestroyObject(svs.physworld, entity);

	return 0; /* number of return values in the stack */
}

/*
===================
PR_PhysicsSetSolidState

Set values for collision detection
input: int entity, unsigned int solid_value
output: none
===================
*/
int PR_PhysicsSetSolidState(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t entity;
	unsigned int solid_value;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsSetSolidState: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 1);
	solid_value = (unsigned int)ProgsGetIntegerFromStack(L, 2);

	Sys_PhysicsSetSolidState(svs.physworld, entity, solid_value);

	return 0;
}

/*
===================
PR_PhysicsSetTransform

Origins and angles should ONLY be changed through this function. It takes care of everything.
input: int entity, vec3_t origin, vec3_t angles, vec3_t locked_angles
output: none
===================
*/
int PR_PhysicsSetTransform(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t entity;
	vec3_t origin, angles, locked_angles;
	vec_t *origin_ptr, *angles_ptr, *locked_angles_ptr;

	nArgs = lua_gettop(L);
	if (nArgs != 4)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsSetTransform: invalid number of arguments, should be 4");
		lua_error(L);
		return 0;
	}
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 1);
	origin_ptr = ProgsGetVector3FromStack(L, 2, -1, origin, true);
	angles_ptr = ProgsGetVector3FromStack(L, 3, -1, angles, true);
	locked_angles_ptr = ProgsGetVector3FromStack(L, 4, -1, locked_angles, true);

	if (!origin_ptr && !angles_ptr && !locked_angles_ptr)
	{
		Sys_Printf("PR_PhysicsSetTransform: !origin && !angles && !locked_angles for entity %d\n", entity);
		return 0;
	}

	Sys_PhysicsSetTransform(svs.physworld, entity, origin_ptr, angles_ptr, locked_angles_ptr);

	return 0;
}

/*
===================
PR_PhysicsIsDynamic

To know if an entity is dynamic or static
input: int entity
output: boolean is_dynamic
===================
*/
int PR_PhysicsIsDynamic(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t entity;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsIsDynamic: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 1);

	ProgsInsertBooleanIntoStack(L, Sys_PhysicsIsDynamic(svs.physworld, entity));

	return 1;
}

/*
===================
PR_PhysicsTraceline

Origins and angles should ONLY be changed through this function. It takes care of everything.
input: int forent, vec3_t origin, vec3_t forward, vec_t length, int ignore_world_triggers, vec_t impulse_to_closest
output: table with subtables containing the keys int ent, vec_t posx/posy/posz, vec_t normalx/normaly/normalz, vec_t fraction for each entity that was hit, int closest_hit_idx containing the closest hit in the table
===================
*/
int PR_PhysicsTraceline(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	entindex_t forent;
	vec3_t origin, forward;
	vec_t length;
	int ignore_world_triggers;
	vec_t impulse_to_closest;
	int closest_hit_idx;

	int i;

	nArgs = lua_gettop(L);
	if (nArgs != 6)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsTraceline: invalid number of arguments, should be 6");
		lua_error(L);
		return 0;
	}
	forent = (entindex_t)ProgsGetIntegerFromStack(L, 1);
	ProgsGetVector3FromStack(L, 2, -1, origin, false);
	ProgsGetVector3FromStack(L, 3, -1, forward, false);
	length = ProgsGetRealFromStack(L, 4);
	ignore_world_triggers = ProgsGetBooleanFromStack(L, 5);
	impulse_to_closest = ProgsGetRealFromStack(L, 6);

	closest_hit_idx = Sys_PhysicsTraceline(svs.physworld, forent, origin, forward, length, ignore_world_triggers, impulse_to_closest);

	/* create table */
	lua_newtable(L);
	for (i = 0; i < gs.entities[forent].trace_numhits; i++)
	{
		/* push index and create subtable */
		lua_pushinteger(L, i + 1); /* start at 1, to use ipairs (iterate from 1 to the first nil) in lua */
		lua_newtable(L);

		lua_pushliteral(L, "ent");
		lua_pushinteger(L, gs.entities[forent].trace_ent[i]);
		lua_settable(L, -3);

		lua_pushliteral(L, "posx");
		lua_pushnumber(L, gs.entities[forent].trace_pos[i][0]);
		lua_settable(L, -3);
		lua_pushliteral(L, "posy");
		lua_pushnumber(L, gs.entities[forent].trace_pos[i][1]);
		lua_settable(L, -3);
		lua_pushliteral(L, "posz");
		lua_pushnumber(L, gs.entities[forent].trace_pos[i][2]);
		lua_settable(L, -3);

		lua_pushliteral(L, "normalx");
		lua_pushnumber(L, gs.entities[forent].trace_normal[i][0]);
		lua_settable(L, -3);
		lua_pushliteral(L, "normaly");
		lua_pushnumber(L, gs.entities[forent].trace_normal[i][1]);
		lua_settable(L, -3);
		lua_pushliteral(L, "normalz");
		lua_pushnumber(L, gs.entities[forent].trace_normal[i][2]);
		lua_settable(L, -3);

		lua_pushliteral(L, "fraction");
		lua_pushnumber(L, gs.entities[forent].trace_fraction[i]);
		lua_settable(L, -3);

		/* put this result in the master table */
		lua_settable(L, -3);
	}

	if (closest_hit_idx == -1)
		lua_pushnil(L);
	else
		ProgsInsertIntegerIntoStack(L, closest_hit_idx + 1);

	return 2;
}

/*
===================
PR_PhysicsSimulateEntity

Disables every other entity and runs a physics simulation only for this entity for the amount of time given
input: mstime_t frametime, int entity
output: none
===================
*/
int PR_PhysicsSimulateEntity(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	mstime_t frametime;
	entindex_t entity;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_PhysicsSimulateEntity: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	frametime = ProgsGetDoubleFromStack(L, 1);
	entity = (entindex_t)ProgsGetIntegerFromStack(L, 2);

	Sys_PhysicsSimulate(svs.physworld, frametime, entity, NULL);

	return 0;
}

/*
===================
PR_MessageWrite*

Various wrappers to send messages to specific client slots or all clients
Unreliable or reliable
Buffer is cleared after the messages are sent
input: int player slot OR none OR message value
output: none
===================
*/
/* TODO: does not allow threading */
char game_sv_msg[MAX_NET_CMDSIZE];
int game_sv_msglen = 0;
int PR_MessageSendToClientUnreliable(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	int slot;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageSendToClientUnreliable: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	slot = ProgsGetIntegerFromStack(L, 1);

	if (slot < 0 || slot >= MAX_CLIENTS)
		Sys_Printf("PR_MessageSendToClientUnreliable: invalid slot: %d\n", slot);

	if (svs.sv_clients[slot].ingame)
		Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, game_sv_msg, game_sv_msglen, NET_CMD_UNRELIABLE);
	game_sv_msglen = 0;

	return 0;
}
int PR_MessageSendBroadcastUnreliable(void *ls)
{
	int i;
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageSendBroadcastUnreliable: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
		if (svs.sv_clients[i].ingame)
			Host_NetchanQueueCommand(svs.sv_clients[i].netconn, game_sv_msg, game_sv_msglen, NET_CMD_UNRELIABLE);

	game_sv_msglen = 0;

	return 0;
}
int PR_MessageSendToClientReliable(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	int slot;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageSendToClientReliable: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	slot = ProgsGetIntegerFromStack(L, 1);

	if (slot < 0 || slot >= MAX_CLIENTS)
		Sys_Printf("PR_MessageSendToClientReliable: invalid slot: %d\n", slot);

	if (svs.sv_clients[slot].ingame)
		Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, game_sv_msg, game_sv_msglen, NET_CMD_RELIABLE);
	game_sv_msglen = 0;

	return 0;
}
int PR_MessageSendBroadcastReliable(void *ls)
{
	int i;
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageSendBroadcastReliable: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
		if (svs.sv_clients[i].ingame)
			Host_NetchanQueueCommand(svs.sv_clients[i].netconn, game_sv_msg, game_sv_msglen, NET_CMD_RELIABLE);

	game_sv_msglen = 0;

	return 0;
}
int PR_MessageWriteEntity(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	entindex_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWriteEntity: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (entindex_t)ProgsGetIntegerFromStack(L, 1);

	MSG_WriteEntity(game_sv_msg, &game_sv_msglen, value);

	return 0;
}
int PR_MessageWritePrecache(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	precacheindex_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWritePrecache: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (precacheindex_t)ProgsGetIntegerFromStack(L, 1);

	MSG_WritePrecache(game_sv_msg, &game_sv_msglen, value);

	return 0;
}
int PR_MessageWriteTime(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	mstime_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWriteTime: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = ProgsGetDoubleFromStack(L, 1);

	MSG_WriteTime(game_sv_msg, &game_sv_msglen, value);

	return 0;
}
int PR_MessageWriteByte(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	unsigned char value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWriteByte: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (unsigned char)ProgsGetIntegerFromStack(L, 1);

	MSG_WriteByte(game_sv_msg, &game_sv_msglen, value);

	return 0;
}
int PR_MessageWriteShort(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	short value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWriteShort: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (short)ProgsGetIntegerFromStack(L, 1);

	MSG_WriteShort(game_sv_msg, &game_sv_msglen, value);

	return 0;
}
int PR_MessageWriteInt(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	int value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWriteInt: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = ProgsGetIntegerFromStack(L, 1);

	MSG_WriteInt(game_sv_msg, &game_sv_msglen, value);

	return 0;
}
int PR_MessageWriteDouble(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	double value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWriteDouble: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = ProgsGetDoubleFromStack(L, 1);

	MSG_WriteDouble(game_sv_msg, &game_sv_msglen, value);

	return 0;
}
int PR_MessageWriteVec1(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	vec_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWriteVec1: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (vec_t)ProgsGetRealFromStack(L, 1);

	MSG_WriteVec1(game_sv_msg, &game_sv_msglen, value);

	return 0;
}
int PR_MessageWriteVec3(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	vec3_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWriteVec3: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	ProgsGetVector3FromStack(L, 1, -1, value, false);

	MSG_WriteVec3(game_sv_msg, &game_sv_msglen, value);

	return 0;
}
int PR_MessageWriteString(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageWriteString: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = ProgsGetStringFromStack(L, 1, false);

	MSG_WriteString(game_sv_msg, &game_sv_msglen, value);

	return 0;
}

/*
===================
PR_NetworkGetPing

Wrapper to read network ping data
input: int player_slot
output: mstime_t ping
===================
*/
int PR_NetworkGetPing(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int player_slot;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_NetworkGetPing: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	player_slot = ProgsGetIntegerFromStack(L, 1);

	if (player_slot < 0 || player_slot >= MAX_CLIENTS)
	{
		ProgsInsertStringIntoStack(L, "PR_NetworkGetPing: player slot out of bounds");
		lua_error(L);
		return 0;
	}

	ProgsInsertIntegerIntoStack(L, (int)svs.sv_clients[player_slot].ping);

	return 1;
}

/*
===================
PR_NetworkGetPacketLoss

Wrapper to read network packet loss data
input: int player_slot
output: int packet_loss
===================
*/
int PR_NetworkGetPacketLoss(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int player_slot;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_NetworkGetPacketLoss: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	player_slot = ProgsGetIntegerFromStack(L, 1);

	if (player_slot < 0 || player_slot >= MAX_CLIENTS)
	{
		ProgsInsertStringIntoStack(L, "PR_NetworkGetPacketLoss: player slot out of bounds");
		lua_error(L);
		return 0;
	}

	ProgsInsertIntegerIntoStack(L, svs.sv_clients[player_slot].packet_loss);

	return 1;
}

/*
===================
PR_MathVecForwardToAngles

Wrapper to Math_VecForwardToAngles
input: vec3_t forward
output: vec3_t angles
===================
*/
int PR_MathVecForwardToAngles(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	vec3_t forward;

	vec3_t angles;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MathVecForwardToAngles: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	ProgsGetVector3FromStack(L, 1, -1, forward, false);

	Math_VecForwardToAngles(forward, angles);

	ProgsInsertVector3IntoStack(L, angles);

	return 1;
}

/*
===================
PR_MathAnglesToVec

Wrapper to Math_AnglesToVec
input: vec3_t angles
output: vec3_t forward, vec3_t right, vec3_t up
===================
*/
int PR_MathAnglesToVec(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	vec3_t angles;

	vec3_t forward, right, up;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MathAnglesToVec: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	ProgsGetVector3FromStack(L, 1, -1, angles, false);

	Math_AnglesToVec(angles, forward, right, up);

	ProgsInsertVector3IntoStack(L, forward);
	ProgsInsertVector3IntoStack(L, right);
	ProgsInsertVector3IntoStack(L, up);

	return 3;
}

/*
===================
PR_MathVecToAngles

Wrapper to Math_VecToAngles
input: vec3_t forward, vec3_t right, vec3_t up
output: vec3_t angles
===================
*/
int PR_MathVecToAngles(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	vec3_t forward, right, up;

	vec3_t angles;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MathVecToAngles: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}
	ProgsGetVector3FromStack(L, 1, -1, forward, false);
	ProgsGetVector3FromStack(L, 2, -1, right, false);
	ProgsGetVector3FromStack(L, 3, -1, up, false);

	Math_VecToAngles(forward, right, up, angles);

	ProgsInsertVector3IntoStack(L, angles);

	return 1;
}

/*
===================
PR_MathPopCount

Wrapper to Math_PopCount
input: unsigned int value
output: unsigned int popcount
===================
*/
int PR_MathPopCount(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	unsigned int value;

	unsigned int popcount;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_MathPopCount: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	value = ProgsGetIntegerFromStack(L, 1);

	popcount = Math_PopCount(value);

	ProgsInsertIntegerIntoStack(L, popcount);

	return 1;
}

/*
===================
PR_VoxelSet

Point is in absolute voxel coordinates
Returns true if succesful, false if out of bounds, coord already occupied, etc
TODO: do not let a voxel be put if there is someone in there (telefragging?)
TODO: create a Host_VoxelWorldToVoxelAbsolute and use it whenever we need! To keep consistency
input: int absblock[3], unsigned char type
output: boolean success
===================
*/
int PR_VoxelSet(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int absblock[3];
	int type;

	int success;

	int block[3];
	int chunk[3];
	int chunkidx;
	unsigned char currenttype;
	model_voxel_t *voxel = Host_VoxelGetData();

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_VoxelSet: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}
	ProgsGetVector3IntegerFromStack(L, 1, -1, absblock, false);
	type = ProgsGetIntegerFromStack(L, 2);

	if (type == VOXEL_BLOCKTYPE_EMPTY)
	{
		ProgsInsertStringIntoStack(L, "PR_VoxelSet: can't set an empty block, use VoxelRemove");
		lua_error(L);
		return 0;
	}

	Host_VoxelDecomposeAbsCoord(absblock, block, chunk);
	chunkidx = Host_VoxelGetChunk(chunk);

	/* if it didn't exist, try to create it */
	if (chunkidx == -1)
	{
		chunkidx = Host_VoxelNewChunk(chunk);
		if (chunkidx == -1) /* invalid coord for a new chunk */
		{
			success = false;
		}
		else /* created okay, came empty, just set */
		{
			SV_VoxelSetBlock(absblock[0], absblock[1], absblock[2], type);
			success = true;
		}
	}
	else /* chunk already exists */
	{
		currenttype = voxel->chunklist[chunkidx]->blocks[block[0]][block[1]][block[2]].type;

		/* if it was empty, set */
		if (currenttype == VOXEL_BLOCKTYPE_EMPTY)
		{
			SV_VoxelSetBlock(absblock[0], absblock[1], absblock[2], type);
			success = true;
		}
		else /* if it was occupied, fail */
		{
			success = false;
		}
	}

	ProgsInsertBooleanIntoStack(L, success);

	return 1;
}

/*
===================
PR_VoxelRemove

Point is in absolute voxel coordinates
Returns the removed type (VOXEL_BLOCKTYPE_EMPTY if none)
input: int absblock[3]
output: unsigned char removedtype
===================
*/
int PR_VoxelRemove(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int absblock[3];

	int removedtype = VOXEL_BLOCKTYPE_EMPTY; /* if not a chunk in memory, return empty FIXME: hope we don't have "faraway voxel catcher" hehe */

	int block[3];
	int chunk[3];
	int chunkidx;
	model_voxel_t *voxel = Host_VoxelGetData();

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_VoxelRemove: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	ProgsGetVector3IntegerFromStack(L, 1, -1, absblock, false);

	Host_VoxelDecomposeAbsCoord(absblock, block, chunk);
	chunkidx = Host_VoxelGetChunk(chunk);
	if (chunkidx != -1)
	{
		removedtype = voxel->chunklist[chunkidx]->blocks[block[0]][block[1]][block[2]].type;

		if (removedtype != VOXEL_BLOCKTYPE_EMPTY)
			SV_VoxelSetBlock(absblock[0], absblock[1], absblock[2], VOXEL_BLOCKTYPE_EMPTY);
	}

	ProgsInsertIntegerIntoStack(L, removedtype);

	return 1;
}

/*
===================
PR_VoxelChunkBufferClear

Empties the chunk buffer, only useful if you're not going to overwrite the entire buffer
input: none
output: none
===================
*/
static unsigned char newchunk[VOXEL_CHUNK_SIZE_X][VOXEL_CHUNK_SIZE_Y][VOXEL_CHUNK_SIZE_Z]; /* TODO FIXME: if VOXEL_CHUNK_SIZE_* are not powers of 2, padding problems when using a pointer to this */
int PR_VoxelChunkBufferClear(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_VoxelChunkBufferClear: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	memset(newchunk, 0, sizeof(newchunk));

	return 0;
}

/*
===================
PR_VoxelChunkSetBlock

Sets a block in the chunk buffer, using local (internal to the chunk) coordinates
input: int localblock[3], unsigned char type
output: none
===================
*/
int PR_VoxelChunkSetBlock(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int localblock[3];
	int type;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_VoxelChunkSetBlock: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}

	ProgsGetVector3IntegerFromStack(L, 1, -1, localblock, false);
	type = ProgsGetIntegerFromStack(L, 2);

	if (localblock[0] < 0 || localblock[1] < 0 || localblock[2] < 0 || localblock[0] >= VOXEL_CHUNK_SIZE_X || localblock[1] >= VOXEL_CHUNK_SIZE_Y || localblock[1] >= VOXEL_CHUNK_SIZE_Y)
	{
		ProgsInsertStringIntoStack(L, "PR_VoxelChunkSetBlock: block coord out of bounds");
		lua_error(L);
		return 0;
	}
	if (type < 0 || type >= VOXEL_BLOCKTYPE_MAX)
	{
		ProgsInsertStringIntoStack(L, "PR_VoxelChunkSetBlock: block type out of bounds");
		lua_error(L);
		return 0;
	}

	newchunk[localblock[0]][localblock[1]][localblock[2]] = type;

	return 0;
}

/*
===================
PR_VoxelChunkCommit

Commits the chunk buffer to the chunk coordinate
input: int chunkcoord[3]
output: none
===================
*/
int PR_VoxelChunkCommit(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int chunkcoord[3];

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_VoxelChunkCommit: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	ProgsGetVector3IntegerFromStack(L, 1, -1, chunkcoord, false);

	SV_VoxelSetChunk(chunkcoord[0], chunkcoord[1], chunkcoord[2], (unsigned char *)newchunk);

	return 0;
}

/*
===================
PR_VoxelCommitUpdates

Wrapper to Host_VoxelCommitUpdates
input: none
output: none
===================
*/
int PR_VoxelCommitUpdates(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_VoxelCommitUpdates: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	Host_VoxelCommitUpdates();

	return 0;
}

/*
============================================================================

Game logic entity dictionary server support/interface functions

============================================================================
*/

/*
===================
Game_SV_EntityIsActive

Called to know if an entity is active
===================
*/
int Game_SV_EntityIsActive(const entindex_t ent)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	if (!lua_istable(sv_lua_state, -1)) /* entity not active */
	{
		lua_pop(sv_lua_state, 2);
		return false;
	}
	lua_pop(sv_lua_state, 2);
	return true;
}

/*
===================
Game_SV_EntityGetOwner

Called to get the index of the owner of the entity
===================
*/
const entindex_t Game_SV_EntityGetOwner(const entindex_t ent)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	if (!lua_istable(sv_lua_state, -1)) /* entity not active */
	{
		lua_pop(sv_lua_state, 2);
		return gs.worldent;
	}
	lua_pushstring(sv_lua_state, "owner");
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1))
	{
		entindex_t owner = (precacheindex_t)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return owner;
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		return gs.worldent;
	}
}

/*
===================
Game_SV_EntityGetVisibleFlag

Called to get the visibility flag of the entity
===================
*/
const int Game_SV_EntityGetVisibleFlag(const entindex_t ent)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	if (!lua_istable(sv_lua_state, -1)) /* entity not active */
	{
		lua_pop(sv_lua_state, 2);
		return VISIBLE_NO;
	}
	lua_pushstring(sv_lua_state, "visible");
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1))
	{
		int visible = (int)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return visible;
	}
	else
	{
		lua_pop(sv_lua_state, 1);

		lua_pushstring(sv_lua_state, "modelindex");
		lua_gettable(sv_lua_state, -2);
		if (lua_isnumber(sv_lua_state, -1))
		{
			int modelindex = (int)lua_tointeger(sv_lua_state, -1);
			lua_pop(sv_lua_state, 3);
			if (modelindex == SV_GetModelIndex("null"))
				return VISIBLE_NO;
			else
				return VISIBLE_TEST;
		}
		else
		{
			lua_pop(sv_lua_state, 3);
			return VISIBLE_NO;
		}
	}
}

/*
===================
Game_SV_VisibilityPrepare

Creates visibility data for a given eyeorigin
===================
*/
void Game_SV_VisibilityPrepare(const vec3_t eyeorigin)
{
	if (SV_ModelHasPVS(gs.worldmodelindex))
	{
		vec3_t adjusted_eyeorigin;

		/* TODO: WORLD MODEL ANGLE CHANGE NOT HANDLED!!! */
		/* adjust for possible world movement */
		if (!Math_Vector3IsZero(gs.worldorigin))
		{
			Math_Vector3ScaleAdd(gs.worldorigin, -1, eyeorigin, adjusted_eyeorigin);
			SV_ModelPVSCreateFatPVSClusters(gs.worldmodelindex, adjusted_eyeorigin);
			return;
		}
		SV_ModelPVSCreateFatPVSClusters(gs.worldmodelindex, eyeorigin);
	}
}

/*
===================
Game_SV_EntityIsVisible

Called to know if an entity is active and visible to a camera at eyeorigin, from client "slot".
Visibility data for "slot" should be already created with Game_SV_VisibilityPrepare before calling
this function.
===================
*/
const int Game_SV_EntityIsVisible(const entindex_t viewent, const int slot, const vec3_t eyeorigin, const entindex_t ent, const precacheindex_t ent_modelindex, const vec_t ent_lightintensity)
{
	game_edict_t *e = &gs.entities[ent];
	int visible;

	/* easy case: inactive, not visible */
	if (!Game_SV_EntityIsActive(ent))
		return false;

	/* TODO: decide what to do with the players' self->model */
	/* always send the viewent for the current player */
	if (viewent == ent)
		return true;

	/* TODO: entities that emit light should be sent when their light or the shadows cast by their light would be visible to the player, have a test for that. Also, SOUNDS */
	if (ent_lightintensity)
		return true;

	visible = Game_SV_EntityGetVisibleFlag(ent);
	/* all other cases */
	switch (visible)
	{
		case VISIBLE_ALWAYS_OWNER:
			if (Game_SV_EntityGetOwner(ent) == slot + 1) /* TODO: depends of slot order being right */
				return true;
			else
				return false;
		case VISIBLE_ALWAYS:
			return true;
		case VISIBLE_TEST: /* TODO: test for visibility for a frustum of a light we can view! */
		case VISIBLE_NEVER_OWNER:
			if (visible == VISIBLE_NEVER_OWNER && Game_SV_EntityGetOwner(ent) == viewent)
				return false;
			if (!ent_modelindex)
				return false;
			if (SV_ModelHasPVS(gs.worldmodelindex) && ent != gs.worldent)
				return SV_ModelPVSTestFatPVSClusters(gs.worldmodelindex, e->clusters, e->clusters_num);
			return true;
		case VISIBLE_NO:
		default:
			return false;
	}
}

/*
===================
Game_SV_EntityGetData

Called to get lots of data about an entity faster. Any field can be null.
===================
*/
void Game_SV_EntityGetData(const entindex_t ent, vec_t *origin, vec_t *velocity, vec_t *avelocity, vec_t *angles, vec_t *frames, precacheindex_t *modelindex, vec_t *lightintensity, int *anim_pitch)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	if (!lua_istable(sv_lua_state, -1)) /* entity not active */
	{
		lua_pop(sv_lua_state, 2);
		return; /* return anything */
	}

	if (origin)
	{
		lua_pushstring(sv_lua_state, "origin");
		lua_gettable(sv_lua_state, -2);
		if (lua_istable(sv_lua_state, -1))
		{
			lua_getfield(sv_lua_state, -1, "x");
			lua_getfield(sv_lua_state, -2, "y");
			lua_getfield(sv_lua_state, -3, "z");
			origin[0] = (vec_t)luaL_checknumber(sv_lua_state, -3);
			origin[1] = (vec_t)luaL_checknumber(sv_lua_state, -2);
			origin[2] = (vec_t)luaL_checknumber(sv_lua_state, -1);
			lua_pop(sv_lua_state, 4);
		}
		else
		{
			lua_pop(sv_lua_state, 1);
			origin[0] = 0;
			origin[1] = 0;
			origin[2] = 0;
		}
	}

	if (velocity)
	{
		lua_pushstring(sv_lua_state, "velocity");
		lua_gettable(sv_lua_state, -2);
		if (lua_istable(sv_lua_state, -1))
		{
			lua_getfield(sv_lua_state, -1, "x");
			lua_getfield(sv_lua_state, -2, "y");
			lua_getfield(sv_lua_state, -3, "z");
			velocity[0] = (vec_t)luaL_checknumber(sv_lua_state, -3);
			velocity[1] = (vec_t)luaL_checknumber(sv_lua_state, -2);
			velocity[2] = (vec_t)luaL_checknumber(sv_lua_state, -1);
			lua_pop(sv_lua_state, 4);
		}
		else
		{
			lua_pop(sv_lua_state, 1);
			velocity[0] = 0;
			velocity[1] = 0;
			velocity[2] = 0;
		}
	}

	if (avelocity)
	{
		lua_pushstring(sv_lua_state, "avelocity");
		lua_gettable(sv_lua_state, -2);
		if (lua_istable(sv_lua_state, -1))
		{
			lua_getfield(sv_lua_state, -1, "x");
			lua_getfield(sv_lua_state, -2, "y");
			lua_getfield(sv_lua_state, -3, "z");
			avelocity[0] = (vec_t)luaL_checknumber(sv_lua_state, -3);
			avelocity[1] = (vec_t)luaL_checknumber(sv_lua_state, -2);
			avelocity[2] = (vec_t)luaL_checknumber(sv_lua_state, -1);
			lua_pop(sv_lua_state, 4);
		}
		else
		{
			lua_pop(sv_lua_state, 1);
			avelocity[0] = 0;
			avelocity[1] = 0;
			avelocity[2] = 0;
		}
	}

	if (angles)
	{
		lua_pushstring(sv_lua_state, "angles");
		lua_gettable(sv_lua_state, -2);
		if (lua_istable(sv_lua_state, -1))
		{
			lua_getfield(sv_lua_state, -1, "x");
			lua_getfield(sv_lua_state, -2, "y");
			lua_getfield(sv_lua_state, -3, "z");
			angles[0] = (vec_t)luaL_checknumber(sv_lua_state, -3);
			angles[1] = (vec_t)luaL_checknumber(sv_lua_state, -2);
			angles[2] = (vec_t)luaL_checknumber(sv_lua_state, -1);
			lua_pop(sv_lua_state, 4);
		}
		else
		{
			lua_pop(sv_lua_state, 1);
			angles[0] = 0;
			angles[1] = 0;
			angles[2] = 0;
		}
	}

	if (frames)
	{
		int i;

		lua_pushstring(sv_lua_state, "frame");
		lua_gettable(sv_lua_state, -2);
		for (i = 0; i < ANIMATION_MAX_BLENDED_FRAMES; i++)
		{
			if (lua_istable(sv_lua_state, -1))
			{
				lua_pushinteger(sv_lua_state, i);
				lua_gettable(sv_lua_state, -2);
				if (lua_isnumber(sv_lua_state, -1))
				{
					vec_t value = (vec_t)lua_tonumber(sv_lua_state, -1);
					lua_pop(sv_lua_state, 1);
					frames[i] = value;
				}
				else
				{
					lua_pop(sv_lua_state, 1);
					frames[i] = 0;
				}
			}
			else
			{
				frames[i] = 0;
			}
		}
		lua_pop(sv_lua_state, 1);
	}

	if (modelindex)
	{
		lua_pushstring(sv_lua_state, "modelindex");
		lua_gettable(sv_lua_state, -2);
		if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
		{
			precacheindex_t return_modelindex = (precacheindex_t)lua_tointeger(sv_lua_state, -1);
			lua_pop(sv_lua_state, 1);
			*modelindex = return_modelindex;
		}
		else
		{
			lua_pop(sv_lua_state, 1);
			*modelindex = 0;
		}
	}

	if (lightintensity)
	{
		lua_pushstring(sv_lua_state, "light_intensity");
		lua_gettable(sv_lua_state, -2);
		if (lua_isnumber(sv_lua_state, -1))
		{
			vec_t return_lightintensity = (vec_t)lua_tonumber(sv_lua_state, -1);
			lua_pop(sv_lua_state, 1);
			*lightintensity = return_lightintensity;
		}
		else
		{
			lua_pop(sv_lua_state, 1);
			*lightintensity = 0;
		}
	}

	if (anim_pitch)
	{
		lua_pushstring(sv_lua_state, "anim_pitch");
		lua_gettable(sv_lua_state, -2);
		if (lua_isboolean(sv_lua_state, -1))
		{
			int return_anim_pitch = lua_toboolean(sv_lua_state, -1);
			lua_pop(sv_lua_state, 1);
			*anim_pitch = return_anim_pitch;
		}
		else
		{
			lua_pop(sv_lua_state, 1);
			*anim_pitch = false;
		}
	}

	lua_pop(sv_lua_state, 2);
}

/*
============================================================================

Server-side game client handling

============================================================================
*/

/*
===================
Game_SV_ClientGetInputSequence

Called to get the sequence number of the last input the server received from the client
===================
*/
const unsigned short Game_SV_ClientGetInputSequence(const int slot)
{
	entindex_t ent;

	lua_getglobal(sv_lua_state, "gamestate");
	lua_pushliteral(sv_lua_state, "players");
	lua_gettable(sv_lua_state, -2);
	lua_pushinteger(sv_lua_state, slot);
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		ent = (entindex_t)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetInputSequence: Couldn't find player in slot %d\n", slot);
	}

	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "input_seq");
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		unsigned short seq = (unsigned short)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return seq;
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetInputSequence: Couldn't find input_seq for player in slot %d\n", slot);
		return 0;
	}
}

/*
===================
Game_SV_ClientGetNetname

Called to get the netname of the client
===================
*/
const char *Game_SV_ClientGetNetname(const int slot)
{
	entindex_t ent;

	lua_getglobal(sv_lua_state, "gamestate");
	lua_pushliteral(sv_lua_state, "players");
	lua_gettable(sv_lua_state, -2);
	lua_pushinteger(sv_lua_state, slot);
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		ent = (entindex_t)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetNetname: Couldn't find player in slot %d\n", slot);
	}

	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "netname");
	lua_gettable(sv_lua_state, -2);
	if (lua_isstring(sv_lua_state, -1))
	{
		const char *netname = lua_tostring(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return netname;
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetNetname: Couldn't find netname for player in slot %d\n", slot);
		return NULL;
	}
}

/*
===================
Game_SV_ClientGetViewEnt

Called to get the index of the view entity
===================
*/
const entindex_t Game_SV_ClientGetViewEnt(const int slot)
{
	entindex_t ent;

	lua_getglobal(sv_lua_state, "gamestate");
	lua_pushliteral(sv_lua_state, "players");
	lua_gettable(sv_lua_state, -2);
	lua_pushinteger(sv_lua_state, slot);
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		ent = (entindex_t)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetViewEnt: Couldn't find player in slot %d\n", slot);
	}

	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "viewent");
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		entindex_t viewent = (entindex_t)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return viewent;
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetViewEnt: Couldn't find viewent for player in slot %d\n", slot);
		return 0;
	}
}

/*
===================
Game_SV_ClientGetCameraEnt

Called to get the index of the camera entity
===================
*/
const entindex_t Game_SV_ClientGetCameraEnt(const int slot)
{
	entindex_t ent;

	lua_getglobal(sv_lua_state, "gamestate");
	lua_pushliteral(sv_lua_state, "players");
	lua_gettable(sv_lua_state, -2);
	lua_pushinteger(sv_lua_state, slot);
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		ent = (entindex_t)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetCameraEnt: Couldn't find player in slot %d\n", slot);
	}

	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "cameraent");
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		entindex_t cameraent = (entindex_t)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return cameraent;
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetCameraEnt: Couldn't find cameraent for player in slot %d\n", slot);
		return 0;
	}
}

/*
===================
Game_SV_ClientGetCmdEnt

Called to get the index of the input cmd entity
===================
*/
const entindex_t Game_SV_ClientGetCmdEnt(const int slot)
{
	entindex_t ent;

	lua_getglobal(sv_lua_state, "gamestate");
	lua_pushliteral(sv_lua_state, "players");
	lua_gettable(sv_lua_state, -2);
	lua_pushinteger(sv_lua_state, slot);
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		ent = (entindex_t)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetCmdEnt: Couldn't find player in slot %d\n", slot);
	}

	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "cmdent");
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		entindex_t cmdent = (entindex_t)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return cmdent;
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Host_Error("Game_SV_ClientGetCmdEnt: Couldn't find cmdent for player in slot %d\n", slot);
		return 0;
	}
}

/*
===================
Game_SV_GetTime

Returns the current game time
===================
*/
const mstime_t Game_SV_GetTime(void)
{
	mstime_t value;

	lua_getglobal(sv_lua_state, "gamestate");
	lua_pushliteral(sv_lua_state, "time");
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		value = luaL_checknumber(sv_lua_state, -1);
		lua_pop(sv_lua_state, 2);
	}
	else
	{
		value = 0;
		lua_pop(sv_lua_state, 2);
	}

	return value;
}

/*
===================
Game_SV_ClientPreThink

Called before physics code is run
===================
*/
void Game_SV_ClientPreThink(int slot)
{
	ProgsPrepareFunction(sv_lua_state, "ClientPreThink", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, slot);
	ProgsRunFunction(sv_lua_state, 1, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_ClientPostThink

Called after physics code is run
===================
*/
void Game_SV_ClientPostThink(int slot)
{
	ProgsPrepareFunction(sv_lua_state, "ClientPostThink", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, slot);
	ProgsRunFunction(sv_lua_state, 1, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_ClientSetName

Prevent null and repeated names
Will make game_edict_t *self valid.
TODO: see if an overhaul is needed when permiting changing names mid-game
===================
*/
void Game_SV_ClientSetName(const int slot, const char *intended_netname, int broadcast)
{
	ProgsPrepareFunction(sv_lua_state, "ClientSetName", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, slot);
	if (intended_netname)
		ProgsInsertStringIntoStack(sv_lua_state, intended_netname);
	else
		ProgsInsertStringIntoStack(sv_lua_state, "");
	ProgsInsertBooleanIntoStack(sv_lua_state, broadcast);
	ProgsRunFunction(sv_lua_state, 3, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_ClientConnect

Called when an client finishes his signon phase and is now in game.
The edict is ALWAYS active, even if the client is not connected.
Use the field "connected" to see if a player is online.
TODO: if a player builds a sentry gun, leaves the server, the sentry stays, someone elses logs in place of
this player, the sentry gun kill someone. Will the sentry give frags to this new player? reset owner of EVERY entity
a player owns to "world" when he quits?
Will make game_edict_t *self valid.
TODO: do NOT call ClientConnect and ClientDisconnect when changing levels?
===================
*/
void Game_SV_ClientConnect(int slot, char *netname, int loading_saved_game)
{
	ProgsPrepareFunction(sv_lua_state, "ClientConnect", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, slot);
	if (netname)
		ProgsInsertStringIntoStack(sv_lua_state, netname);
	else
		ProgsInsertStringIntoStack(sv_lua_state, "");
	ProgsInsertBooleanIntoStack(sv_lua_state, loading_saved_game);
	ProgsRunFunction(sv_lua_state, 3, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_ClientDisconnect

Called when disconnecting a client
Will make game_edict_t *self valid.

TODO: save player information, for Game_SV_ClientConnect to load
if the player is coming back? :) needs authentication from the
engine first, them passing the ID to Game_SV_ClientConnect, which
will load the saved data OR create a new one if a new player.
But be careful with things such as: saving a single player game,
losing, doing a disconnect and saving player data, loading game
and finding your data as it was when you lost, not when you saved!
===================
*/
void Game_SV_ClientDisconnect(int slot)
{
	ProgsPrepareFunction(sv_lua_state, "ClientDisconnect", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, slot);
	ProgsRunFunction(sv_lua_state, 1, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_ParseClientMessages

Called to process extra commands from the client
===================
*/
int Game_SV_ParseClientMessages(int slot, unsigned cmd, char *msg, int *read, int len)
{
	int result;
	ProgsPrepareFunction(sv_lua_state, "ParseClientMessages", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, slot);
	ProgsInsertIntegerIntoStack(sv_lua_state, cmd);
	ProgsInsertCPointerIntoStack(sv_lua_state, msg);
	ProgsInsertCPointerIntoStack(sv_lua_state, read);
	ProgsInsertIntegerIntoStack(sv_lua_state, len);
	ProgsRunFunction(sv_lua_state, 5, 1, true);
	result = ProgsGetBooleanFromStack(sv_lua_state, -1);
	ProgsFinishFunction(sv_lua_state, 1);
	return result;
}

/*
===================
Game_SV_SendClientMessages

Called to send game-specific state updates
TODO: send fixangle here and make sure it gets sent at the same time as teletransports and spawns
===================
*/
void Game_SV_SendClientMessages(int slot)
{
	ProgsPrepareFunction(sv_lua_state, "SendClientMessages", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, slot);
	ProgsRunFunction(sv_lua_state, 1, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
============================================================================

Game logic physics management/interfaces

============================================================================
*/

/*
===================
Game_SV_UpdatePhysStats

Called by the physics code to update physics data for any entity with an active physical representation
===================
*/
void Game_SV_UpdatePhysStats(entindex_t ent, vec3_t origin, vec3_t angles, vec3_t velocity, vec3_t avelocity, int onground)
{
	ProgsPrepareFunction(sv_lua_state, "UpdatePhysStats", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, ent);
	ProgsInsertRealIntoStack(sv_lua_state, origin[0]);
	ProgsInsertRealIntoStack(sv_lua_state, origin[1]);
	ProgsInsertRealIntoStack(sv_lua_state, origin[2]);
	ProgsInsertRealIntoStack(sv_lua_state, angles[0]);
	ProgsInsertRealIntoStack(sv_lua_state, angles[1]);
	ProgsInsertRealIntoStack(sv_lua_state, angles[2]);
	ProgsInsertRealIntoStack(sv_lua_state, velocity[0]);
	ProgsInsertRealIntoStack(sv_lua_state, velocity[1]);
	ProgsInsertRealIntoStack(sv_lua_state, velocity[2]);
	ProgsInsertRealIntoStack(sv_lua_state, avelocity[0]);
	ProgsInsertRealIntoStack(sv_lua_state, avelocity[1]);
	ProgsInsertRealIntoStack(sv_lua_state, avelocity[2]);
	ProgsInsertBooleanIntoStack(sv_lua_state, onground);
	ProgsRunFunction(sv_lua_state, 14, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_UpdatePhysDirections

Called by the physics code to update physics data for any entity with an active physical representation
===================
*/
void Game_SV_UpdatePhysDirections(entindex_t ent, vec3_t forward, vec3_t right, vec3_t up)
{
	ProgsPrepareFunction(sv_lua_state, "UpdatePhysDirections", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, ent);
	ProgsInsertRealIntoStack(sv_lua_state, forward[0]);
	ProgsInsertRealIntoStack(sv_lua_state, forward[1]);
	ProgsInsertRealIntoStack(sv_lua_state, forward[2]);
	ProgsInsertRealIntoStack(sv_lua_state, right[0]);
	ProgsInsertRealIntoStack(sv_lua_state, right[1]);
	ProgsInsertRealIntoStack(sv_lua_state, right[2]);
	ProgsInsertRealIntoStack(sv_lua_state, up[0]);
	ProgsInsertRealIntoStack(sv_lua_state, up[1]);
	ProgsInsertRealIntoStack(sv_lua_state, up[2]);
	ProgsRunFunction(sv_lua_state, 10, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_UpdateTraceResultStart

Called by the physics code to begin updating traceline results
===================
*/
void Game_SV_UpdateTraceResultStart(entindex_t ent)
{
	gs.entities[ent].trace_numhits = 0;
}

/*
===================
Game_SV_UpdateTraceResultStep

Called by the physics code to add a new traceline result for this entity
Returns the index of the last added result, -1 if it was ignored
===================
*/
int Game_SV_UpdateTraceResultStep(entindex_t ent, entindex_t hit, vec3_t pos, vec3_t normal, vec_t fraction)
{
	int i, idx;

	idx = gs.entities[ent].trace_numhits;
	if (gs.entities[ent].trace_numhits == MAX_GAME_TRACEHITS)
	{
		for (i = 0; i < MAX_GAME_TRACEHITS; i++)
		{
			if (gs.entities[ent].trace_fraction[i] > fraction)
			{
				idx = i;
				Sys_Printf("Warning: trace had too many results (%d max), replaced a farther hit\n", MAX_GAME_TRACEHITS);
				goto doit;
			}
		}
		Sys_Printf("Warning: trace had too many results (%d max), discarding a far hit\n", MAX_GAME_TRACEHITS);
		return -1;
	}
	gs.entities[ent].trace_numhits++;

doit:

	gs.entities[ent].trace_ent[idx] = hit;
	Math_Vector3Copy(pos, gs.entities[ent].trace_pos[idx]);
	Math_Vector3Copy(normal, gs.entities[ent].trace_normal[idx]);
	gs.entities[ent].trace_fraction[idx] = fraction;

	return idx;
}

/*
===================
Game_SV_PostPhysics

Called by the physics code immediately after a frame is run
===================
*/
void Game_SV_PostPhysics(void)
{
	ProgsPrepareFunction(sv_lua_state, "PostPhysics", true);
	ProgsRunFunction(sv_lua_state, 0, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_GetMoveType

Called by the physics code to know the movetype of an object
===================
*/
const int Game_SV_GetMoveType(entindex_t ent)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "movetype");
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		int movetype = (unsigned int)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return movetype;
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		return 0;
	}
}

/*
===================
Game_SV_GetAnglesFlags

Called by the physics code to know the anglesflags of an object
===================
*/
const unsigned int Game_SV_GetAnglesFlags(entindex_t ent)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "anglesflags");
	lua_gettable(sv_lua_state, -2);
	if (lua_isnumber(sv_lua_state, -1)) /* TODO FIXME: for some reason, when getting the player viewent, lua_isinteger wasn't working and printing the value from lua was printing (for example) 99.0 instead of 99, but this isn't an floating point value! */
	{
		unsigned int anglesflags = (unsigned int)lua_tointeger(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return anglesflags;
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		return 0;
	}
}

/*
===================
Game_SV_GetMoveCmd

Called by the physics code to know the move command of an object
===================
*/
void Game_SV_GetMoveCmd(entindex_t ent, vec_t *dest)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "movecmd");
	lua_gettable(sv_lua_state, -2);
	if (lua_istable(sv_lua_state, -1))
	{
		lua_getfield(sv_lua_state, -1, "x");
		lua_getfield(sv_lua_state, -2, "y");
		lua_getfield(sv_lua_state, -3, "z");
		dest[0] = (vec_t)luaL_checknumber(sv_lua_state, -3);
		dest[1] = (vec_t)luaL_checknumber(sv_lua_state, -2);
		dest[2] = (vec_t)luaL_checknumber(sv_lua_state, -1);
		lua_pop(sv_lua_state, 6);
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		dest[0] = 0;
		dest[1] = 0;
		dest[2] = 0;
	}
}

/*
===================
Game_SV_GetAimCmd

Called by the physics code to know the aim command of an object
===================
*/
void Game_SV_GetAimCmd(entindex_t ent, vec_t *dest)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "aimcmd");
	lua_gettable(sv_lua_state, -2);
	if (lua_istable(sv_lua_state, -1))
	{
		lua_getfield(sv_lua_state, -1, "x");
		lua_getfield(sv_lua_state, -2, "y");
		lua_getfield(sv_lua_state, -3, "z");
		dest[0] = (vec_t)luaL_checknumber(sv_lua_state, -3);
		dest[1] = (vec_t)luaL_checknumber(sv_lua_state, -2);
		dest[2] = (vec_t)luaL_checknumber(sv_lua_state, -1);
		lua_pop(sv_lua_state, 6);
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		dest[0] = 0;
		dest[1] = 0;
		dest[2] = 0;
	}
}

/*
===================
Game_SV_GetMaxSpeed

Called by the physics code to know the max speed of an object in each axis
TODO: rotate this vector
===================
*/
void Game_SV_GetMaxSpeed(entindex_t ent, vec_t *dest)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "maxspeed");
	lua_gettable(sv_lua_state, -2);
	if (lua_istable(sv_lua_state, -1))
	{
		lua_getfield(sv_lua_state, -1, "x");
		lua_getfield(sv_lua_state, -2, "y");
		lua_getfield(sv_lua_state, -3, "z");
		dest[0] = (vec_t)luaL_checknumber(sv_lua_state, -3);
		dest[1] = (vec_t)luaL_checknumber(sv_lua_state, -2);
		dest[2] = (vec_t)luaL_checknumber(sv_lua_state, -1);
		lua_pop(sv_lua_state, 6);
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Math_Vector3Copy(null_vec3, dest);
	}
}

/*
===================
Game_SV_GetAcceleration

Called by the physics code to know the acceleration of an object in each axis
TODO: rotate this vector
===================
*/
void Game_SV_GetAcceleration(entindex_t ent, vec_t *dest)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "acceleration");
	lua_gettable(sv_lua_state, -2);
	if (lua_istable(sv_lua_state, -1))
	{
		lua_getfield(sv_lua_state, -1, "x");
		lua_getfield(sv_lua_state, -2, "y");
		lua_getfield(sv_lua_state, -3, "z");
		dest[0] = (vec_t)luaL_checknumber(sv_lua_state, -3);
		dest[1] = (vec_t)luaL_checknumber(sv_lua_state, -2);
		dest[2] = (vec_t)luaL_checknumber(sv_lua_state, -1);
		lua_pop(sv_lua_state, 6);
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		Math_Vector3Copy(null_vec3, dest);
	}
}

/*
===================
Game_SV_GetIgnoreGravity

Called by the physics code to know this entity wants to ignore gravity
===================
*/
int Game_SV_GetIgnoreGravity(entindex_t ent)
{
	lua_getglobal(sv_lua_state, "entities");
	lua_pushinteger(sv_lua_state, ent);
	lua_gettable(sv_lua_state, -2);
	lua_pushstring(sv_lua_state, "ignore_gravity");
	lua_gettable(sv_lua_state, -2);
	if (lua_isboolean(sv_lua_state, -1))
	{
		int ignore_gravity = lua_toboolean(sv_lua_state, -1);
		lua_pop(sv_lua_state, 3);
		return ignore_gravity;
	}
	else
	{
		lua_pop(sv_lua_state, 3);
		return false;
	}
}

/*
============================================================================

Main game logic

Globals should only be used when it's not important to save/load games.

TODO:
REMOVE THESE DEPENDENCIES:
gs.entities[0] is ALWAYS assumed to belong to game WORLD entit in various parts of the engine.
The null model is assumed to be the model ZERO always too. (mainly for drawing and for having entity model == 0 meaning no model)

TODO:
IN-GAME SAVEGAMES
-See if we have any state variables outside the gs structure (already looks
clean as of 2014/07/14, with the exception of voxels)
-Physics state saving
-When loading, Game_SV_NewGame should be called to be able to set the
precaches in the same order (this function and everything it calls should
ALWAYS do the SAME things when loading the same level)
-Then we PAUSE the server without sending any updates to the client and
wait for him to log in (Not calling Game_SV_ClientConnect and others,
because the game logic data would already be loaded by then and this would
cause a respawn)
-Multiplayer like Hexen 2? Put all connecting clients in their rightful slot
and force pause until all are connected
-While the server is paused waiting for the clients to connect, send NOPs to
them.
-Load the state structures from the saved game
-Unpause and begin sending updates
This all means that ANYTHING meaningful for the game state shouldn't be:
-Set and forgot (like sending a message to the client and not having it
set locally for sending again (when comparing deltas) or resending as
baseline, like frags and ambient sounds
-Long-lasting particle effects should be send as flags of entities
-Client-side temporary entities should not be long-lasting as well
-Be careful about client-side UIs and other stuff! (server sending a UI and
waiting for an answer - wrong! should send it every time?)
-See about when it's possible to save the game (mid-changelevels, waiting
for players to reconnect, etc)
-What to do about voxel data structures (commit all changes, etc)
-Use the same method used to save player data for loading games to changelevels
and returning players in multiplayer?
-Save precaches, to avoid going throught the normal level load before loading
the saved data?
-Get rid of pointers in game code (be sure to not rely on game_edict_t * being
used after being set in another frame)
-Games like Fallout 3 always respawn the entities physically, good for
recovering from physics errors.
-See about cvars which should be saved too. (any cvar created in the game_sv*.c files?)
-Which data that we pass to the physics engine isn't mirrored int it? (heightfield
data, for example)

============================================================================
*/

game_state_t gs;

/*
===================
Game_SV_StartFrame

Called at the start of each frame
===================
*/
void Game_SV_StartFrame(mstime_t frametime)
{
	ProgsPrepareFunction(sv_lua_state, "StartFrame", true);
	ProgsInsertDoubleIntoStack(sv_lua_state, frametime);
	ProgsRunFunction(sv_lua_state, 1, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_EndFrame

Called at the end of each frame
===================
*/
void Game_SV_EndFrame(void)
{
	entindex_t i;

	ProgsPrepareFunction(sv_lua_state, "EndFrame", true);
	ProgsRunFunction(sv_lua_state, 0, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);

	Game_SV_EntityGetData(gs.worldent, gs.worldorigin, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	for (i = 0; i < MAX_EDICTS; i++)
	{
		if (!Game_SV_EntityIsActive(i))
			continue;

		/* handle origins for server-side PVS calculations, to ease culling on the clients and prevent some cheats */
		if (SV_ModelHasPVS(gs.worldmodelindex) && (i != gs.worldent))
		{
			vec3_t entityorigin, entityangles;
			precacheindex_t entmodelindex;

			Game_SV_EntityGetData(i, entityorigin, NULL, NULL, entityangles, NULL, &entmodelindex, NULL, NULL);

			/* TODO: changing world or entity models midframe may leave us with invalid cluster caches */
			/* TODO: WORLD MODEL ANGLE CHANGE NOT HANDLED!!! */
			/* TODO: TEST ENTITY ANGLE CHANGES!!! OTHER CULLING METHODS DO NOT HANDLE THIS? CHECK AABBs GIVEN TO FRUSTUM CULLING, ETC TOO (a door is a good example, it is VERY flat, so when rotated it will probably be into another set of clusters!) */
			/* TODO: invisible entities also cause problems with shadows, sound velocity updating, etc */
			if (!Math_Vector3Compare(gs.worldorigin, gs.entities[gs.worldent].clusters_myoldorigin))
				gs.entities[i].clusters_valid = false;
			if (!Math_Vector3Compare(entityorigin, gs.entities[i].clusters_myoldorigin) || !entmodelindex)
				gs.entities[i].clusters_valid = false;
			if (!Math_Vector3Compare(entityangles, gs.entities[i].clusters_myoldangles) || !entmodelindex)
				gs.entities[i].clusters_valid = false;

			if (!gs.entities[i].clusters_valid && entmodelindex)
			{
				vec_t matrix[16];
				vec4_t point1, point2;
				vec4_t outpoint1, outpoint2;

				vec3_t absmin, absmax;
				vec3_t minus = {-0.01f, -0.01f, -0.01f};
				vec3_t plus = {0.01f, 0.01f, 0.01f};
				/* TODO: will this force animation to happen? also, only using the base frame - make it big enough to encompass all others */
				SV_GetModelAABB(entmodelindex, ANIMATION_BASE_FRAME, absmin, absmax);

				Math_MatrixModel4x4(matrix, NULL, (vec_t *)entityangles, NULL); /* TODO FIXME: const casting */
				point1[0] = absmin[0];
				point1[1] = absmin[1];
				point1[2] = absmin[2];
				point1[3] = 1;
				point2[0] = absmax[0];
				point2[1] = absmax[1];
				point2[2] = absmax[2];
				point2[3] = 1;
				Math_Matrix4x4ApplyToVector4(outpoint1, matrix, point1);
				Math_Matrix4x4ApplyToVector4(outpoint2, matrix, point2);
				Math_AABB3EnclosePoints(outpoint1, outpoint2, absmax, absmin);

				/* make the sizes absolute, adjust for possible world movement and make slightly bigger to avoid edge cases */
				Math_Vector3Add(absmin, entityorigin, absmin);
				Math_Vector3ScaleAdd(gs.worldorigin, -1, absmin, absmin);
				Math_Vector3Add(absmin, minus, absmin);
				Math_Vector3Add(absmax, entityorigin, absmax);
				Math_Vector3ScaleAdd(gs.worldorigin, -1, absmax, absmax);
				Math_Vector3Add(absmax, plus, absmax);
				SV_ModelPVSGetClustersBox(gs.worldmodelindex, absmin, absmax, gs.entities[i].clusters, &gs.entities[i].clusters_num, MAX_GAME_ENTITY_CLUSTERS);
				gs.entities[i].clusters_valid = true;
				/* TODO CONSOLEDEBUG Sys_Printf("entity %d is on %d clusters\n", i, gs.entities[i].clusters_num); */
			}
			Math_Vector3Copy(entityorigin, gs.entities[i].clusters_myoldorigin);
			Math_Vector3Copy(entityangles, gs.entities[i].clusters_myoldangles);
		}
	}
}

/*
===================
Game_SV_RunThinks

Called to wake up entities at a predetermined time

See code in Host_FilterTime to see the minimum and maximum
frametimes, to take into account when setting nextthinks.
===================
*/
void Game_SV_RunThinks(void)
{
	ProgsPrepareFunction(sv_lua_state, "RunThinks", true);
	ProgsRunFunction(sv_lua_state, 0, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_Touchents

Called by physics code to run the touch function of an entity
Touched entities should not be removed inside touch functions because other entities
may want to touch it in the same frame, or worse: new entities may occupy it's slot
and get touched!
To remove, set a next think for the next millisecond. The think functions will
run before the next frame's physics code. (Use SUB_Remove for the think function)
===================
*/
void Game_SV_TouchEnts(entindex_t who, entindex_t by, vec3_t pos, vec3_t normal, vec_t distance, int reaction, vec_t impulse)
{
	if (who < 0 || who >= MAX_EDICTS)
		Host_Error("Invalid entity %d touched!\n", who);
	if (by < 0 || by >= MAX_EDICTS)
		Host_Error("Invalid entity %d touching!\n", by);

	ProgsPrepareFunction(sv_lua_state, "Touchents", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, who);
	ProgsInsertIntegerIntoStack(sv_lua_state, by);
	ProgsInsertVector3IntoStack(sv_lua_state, pos);
	ProgsInsertVector3IntoStack(sv_lua_state, normal);
	ProgsInsertRealIntoStack(sv_lua_state, distance);
	ProgsInsertBooleanIntoStack(sv_lua_state, reaction);
	ProgsInsertRealIntoStack(sv_lua_state, impulse);
	ProgsRunFunction(sv_lua_state, 7, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_CheckPhysicalCollisionResponse

Last check done by the physics code to see if two entities should generate
a collision response. (i.e. prevent inter-penetration)
===================
*/
int Game_SV_CheckPhysicalCollisionResponse(entindex_t e1, entindex_t e2)
{
	int result;

	if (e1 < 0 || e1 >= MAX_EDICTS)
		Host_Error("Game_SV_CheckPhysicalCollisionResponse: Invalid entity e1 %d!\n", e1);
	if (e2 < 0 || e2 >= MAX_EDICTS)
		Host_Error("Game_SV_CheckPhysicalCollisionResponse: Invalid entity e2 %d!\n", e2);

	ProgsPrepareFunction(sv_lua_state, "CheckPhysicalCollisionResponse", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, e1);
	ProgsInsertIntegerIntoStack(sv_lua_state, e2);
	ProgsRunFunction(sv_lua_state, 2, 1, true);

	result = ProgsGetBooleanFromStack(sv_lua_state, -1);
	ProgsFinishFunction(sv_lua_state, 1);
	return result;
}

/*
===================
Game_SV_NewGame

Called to start a new game, will load all resources
===================
*/
void Game_SV_NewGame(int loading_saved_game)
{
	Sys_Printf("\n===================\n\nNew Game\n\n===================\n\n");

	game_sv_msglen = 0;

	/* it's important to zero out everything, the code relies on default and sane properties to be zero */
	memset(&gs, 0, sizeof(game_state_t));

	if (sv_lua_state)
		Host_Error("Game_SV_NewGame: game already started!\n");
	sv_lua_state = luaL_newstate();
    luaL_openlibs(sv_lua_state);

	ProgsRegisterShared(sv_lua_state);

	lua_register(sv_lua_state, "PrecacheModel", PR_PrecacheModel);
	lua_register(sv_lua_state, "PrecacheSound", PR_PrecacheSound);
	lua_register(sv_lua_state, "GetModelIndex", PR_GetModelIndex);
	lua_register(sv_lua_state, "GetSoundIndex", PR_GetSoundIndex);
	lua_register(sv_lua_state, "GetModelSoundIndex", PR_GetModelSoundIndex);
	lua_register(sv_lua_state, "GetModelEntities", PR_GetModelEntities);
	lua_register(sv_lua_state, "GetModelAABB", PR_GetModelAABB);
	lua_register(sv_lua_state, "GetModelTagTransform", PR_GetModelTagTransform);
	lua_register(sv_lua_state, "Animate", PR_Animate);
	lua_register(sv_lua_state, "AnimationInfo", PR_AnimationInfo);
	lua_register(sv_lua_state, "AnimationExists", PR_AnimationExists);
	lua_register(sv_lua_state, "PointContents", PR_PointContents);
	lua_register(sv_lua_state, "PhysicsApplyImpulse", PR_PhysicsApplyImpulse);
	lua_register(sv_lua_state, "PhysicsSetLinearVelocity", PR_PhysicsSetLinearVelocity);
	lua_register(sv_lua_state, "PhysicsSetAngularVelocity", PR_PhysicsSetAngularVelocity);
	lua_register(sv_lua_state, "PhysicsCreateFromModel", PR_PhysicsCreateFromModel);
	lua_register(sv_lua_state, "PhysicsCreateFromData", PR_PhysicsCreateFromData);
	lua_register(sv_lua_state, "PhysicsDestroy", PR_PhysicsDestroy);
	lua_register(sv_lua_state, "PhysicsSetSolidState", PR_PhysicsSetSolidState);
	lua_register(sv_lua_state, "PhysicsSetTransform", PR_PhysicsSetTransform);
	lua_register(sv_lua_state, "PhysicsIsDynamic", PR_PhysicsIsDynamic);
	lua_register(sv_lua_state, "PhysicsTraceline", PR_PhysicsTraceline);
	lua_register(sv_lua_state, "PhysicsSimulateEntity", PR_PhysicsSimulateEntity);
	lua_register(sv_lua_state, "MessageSendToClientUnreliable", PR_MessageSendToClientUnreliable);
	lua_register(sv_lua_state, "MessageSendBroadcastUnreliable", PR_MessageSendBroadcastUnreliable);
	lua_register(sv_lua_state, "MessageSendToClientReliable", PR_MessageSendToClientReliable);
	lua_register(sv_lua_state, "MessageSendBroadcastReliable", PR_MessageSendBroadcastReliable);
	lua_register(sv_lua_state, "MessageWriteEntity", PR_MessageWriteEntity);
	lua_register(sv_lua_state, "MessageWritePrecache", PR_MessageWritePrecache);
	lua_register(sv_lua_state, "MessageWriteTime", PR_MessageWriteTime);
	lua_register(sv_lua_state, "MessageWriteByte", PR_MessageWriteByte);
	lua_register(sv_lua_state, "MessageWriteShort", PR_MessageWriteShort);
	lua_register(sv_lua_state, "MessageWriteInt", PR_MessageWriteInt);
	lua_register(sv_lua_state, "MessageWriteDouble", PR_MessageWriteDouble);
	lua_register(sv_lua_state, "MessageWriteVec1", PR_MessageWriteVec1);
	lua_register(sv_lua_state, "MessageWriteVec3", PR_MessageWriteVec3);
	lua_register(sv_lua_state, "MessageWriteString", PR_MessageWriteString);
	lua_register(sv_lua_state, "NetworkGetPing", PR_NetworkGetPing);
	lua_register(sv_lua_state, "NetworkGetPacketLoss", PR_NetworkGetPacketLoss);
	lua_register(sv_lua_state, "MathVecForwardToAngles", PR_MathVecForwardToAngles);
	lua_register(sv_lua_state, "MathAnglesToVec", PR_MathAnglesToVec);
	lua_register(sv_lua_state, "MathVecToAngles", PR_MathVecToAngles);
	lua_register(sv_lua_state, "MathPopCount", PR_MathPopCount);
	lua_register(sv_lua_state, "VoxelSet", PR_VoxelSet);
	lua_register(sv_lua_state, "VoxelRemove", PR_VoxelRemove);
	lua_register(sv_lua_state, "VoxelChunkBufferClear", PR_VoxelChunkBufferClear);
	lua_register(sv_lua_state, "VoxelChunkSetBlock", PR_VoxelChunkSetBlock);
	lua_register(sv_lua_state, "VoxelChunkCommit", PR_VoxelChunkCommit);
	lua_register(sv_lua_state, "VoxelCommitUpdates", PR_VoxelCommitUpdates);

	/*
		TODO FIXME: see which ones have the highest 32-bit bit integer set (like WEAPON_MAX_BIT had)
		here and in all parameter passing, forwards and backwards. This is because it will be cast
		to a negative signed 32-bit number in some cases, and this is bad. Passing unsigned integers
		back and forth with higher bits set should be tested througly. FIND A SOLUTION TO THIS!
	*/
#define lua_setConstReal(name) { lua_pushnumber(sv_lua_state, name ); lua_setglobal(sv_lua_state, #name ); }
#define lua_setConstInteger(name) { lua_pushinteger(sv_lua_state, name ); lua_setglobal(sv_lua_state, #name ); }
	lua_setConstInteger(ANGLES_PITCH)
	lua_setConstInteger(ANGLES_YAW)
	lua_setConstInteger(ANGLES_ROLL)
	lua_setConstInteger(MAX_EDICTS)
	lua_setConstInteger(ANIMATION_MAX_BLENDED_FRAMES)
	lua_setConstReal(VOXEL_SIZE_X)
	lua_setConstReal(VOXEL_SIZE_Y)
	lua_setConstReal(VOXEL_SIZE_Z)
	lua_setConstReal(VOXEL_SIZE_X_2)
	lua_setConstReal(VOXEL_SIZE_Y_2)
	lua_setConstReal(VOXEL_SIZE_Z_2)
	lua_setConstInteger(VOXEL_BLOCKTYPE_EMPTY)
	lua_setConstInteger(VOXEL_BLOCKTYPE_MAX)
	lua_setConstInteger(VOXEL_CHUNK_SIZE_X)
	lua_setConstInteger(VOXEL_CHUNK_SIZE_Y)
	lua_setConstInteger(VOXEL_CHUNK_SIZE_Z)
	lua_setConstInteger(SVC_PARTICLE)
	lua_setConstInteger(SVC_SOUND)
	lua_setConstInteger(SVC_STOPSOUND)
	lua_setConstInteger(MAX_CLIENTS)

	lua_setConstInteger(SOLID_WORLD_NOT)
	lua_setConstInteger(SOLID_WORLD_TRIGGER)
	lua_setConstInteger(SOLID_WORLD)
	lua_setConstInteger(SOLID_ENTITY_WITHWORLDONLY)
	lua_setConstInteger(SOLID_ENTITY_TRIGGER)
	lua_setConstInteger(SOLID_ENTITY)
	lua_setConstInteger(CONTENTS_SOLID_BIT)
	lua_setConstInteger(CONTENTS_WATER_BIT)
	lua_setConstInteger(MOVETYPE_FREE)
	lua_setConstInteger(MOVETYPE_WALK)
	lua_setConstInteger(MOVETYPE_FLY)
	lua_setConstInteger(MOVETYPE_FOLLOW)
	lua_setConstInteger(MOVETYPE_FOLLOWANGLES)
	lua_setConstInteger(ANGLES_KINEMATICANGLES_BIT)
	lua_setConstInteger(ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT)
	lua_setConstInteger(ANGLES_KINEMATICANGLES_LOCK_YAW_BIT)
	lua_setConstInteger(ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT)
	lua_setConstInteger(VISIBLE_NO)
	lua_setConstInteger(VISIBLE_TEST)
	lua_setConstInteger(VISIBLE_ALWAYS)
	lua_setConstInteger(VISIBLE_ALWAYS_OWNER)
	lua_setConstInteger(VISIBLE_NEVER_OWNER)
	lua_setConstInteger(ANIMATION_BASE)
	lua_setConstInteger(ANIMATION_IDLE)
	lua_setConstInteger(ANIMATION_FIRE)
	lua_setConstInteger(ANIMATION_FIRE2)
	lua_setConstInteger(ANIMATION_FIRE3)
	lua_setConstInteger(ANIMATION_FIRE4)
	lua_setConstInteger(ANIMATION_FIRE5)
	lua_setConstInteger(ANIMATION_FIRE6)
	lua_setConstInteger(ANIMATION_FIRE7)
	lua_setConstInteger(ANIMATION_FIRE8)
	lua_setConstInteger(ANIMATION_FIREEMPTY)
	lua_setConstInteger(ANIMATION_RELOAD)
	lua_setConstInteger(ANIMATION_WEAPONACTIVATE)
	lua_setConstInteger(ANIMATION_WEAPONDEACTIVATE)
	lua_setConstInteger(ANIMATION_RUN)
	lua_setConstInteger(ANIMATION_RUNRIGHT)
	lua_setConstInteger(ANIMATION_RUNLEFT)
	lua_setConstInteger(NUM_ANIMATIONS)
	lua_setConstInteger(ANIMATION_BASE_FRAME)
	lua_setConstInteger(ANIMATION_SLOT_ALLJOINTS)
	lua_setConstInteger(ANIMATION_SLOT_ARMS)
	lua_setConstInteger(ANIMATION_SLOT_LEGS)
	lua_setConstInteger(ANIMATION_SLOT_PELVIS)
	lua_setConstInteger(ANIMATION_SLOT_TORSO)
	lua_setConstInteger(MODEL_TAG_RIGHTHAND)
	lua_setConstInteger(MODEL_TAG_LEFTHAND)
	lua_setConstInteger(NUM_MODEL_TAGS)
	lua_setConstInteger(SOUND_FIRE)
	lua_setConstInteger(SOUND_FIREEMPTY)
	lua_setConstInteger(SOUND_PAIN)
	lua_setConstInteger(SOUND_DIE)

	lua_setConstInteger(VEHICLE1_ALL_WHEEL_DRIVE)
	lua_setConstInteger(VEHICLE1_FRONT_WHEEL_DRIVE)
	lua_setConstInteger(VEHICLE1_REAR_WHEEL_DRIVE)
	lua_setConstInteger(PHYSICS_SHAPE_BOX)
	lua_setConstInteger(PHYSICS_SHAPE_SPHERE)
	lua_setConstInteger(PHYSICS_SHAPE_CAPSULE_Y)
	lua_setConstInteger(PHYSICS_SHAPE_TRIMESH_FROM_MODEL)
	lua_setConstInteger(PHYSICS_SHAPE_TRIMESH_FROM_DATA)
	lua_setConstInteger(PHYSICS_SHAPE_CONVEXHULLS_FROM_MODEL)
	lua_setConstInteger(PHYSICS_SHAPE_CONVEXHULLS_FROM_DATA)
	lua_setConstInteger(PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL)
	lua_setConstInteger(PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA)
	lua_setConstInteger(PHYSICS_SHAPE_VEHICLE1)
	lua_setConstInteger(PHYSICS_SHAPE_VOXEL_BOX)
	lua_setConstInteger(PHYSICS_SHAPE_VOXEL_TRIMESH)

	LoadProg(sv_lua_state, "sv_main", true, &svr_mem, "svgameprog");

	ProgsPrepareFunction(sv_lua_state, "NewGame", true);
	ProgsInsertStringIntoStack(sv_lua_state, svs.name);
	ProgsInsertBooleanIntoStack(sv_lua_state, loading_saved_game);
	ProgsRunFunction(sv_lua_state, 2, 1, true);
	gs.worldent = ProgsGetIntegerFromStack(sv_lua_state, -1);
	ProgsFinishFunction(sv_lua_state, 1);
	Game_SV_EntityGetData(gs.worldent, NULL, NULL, NULL, NULL, NULL, &gs.worldmodelindex, NULL, NULL);
}

/*
===================
Game_SV_EndGame

Called to end a current game, will unload everything that's not unloaded automatically
via memory pools
===================
*/
void Game_SV_EndGame(void)
{
	if (svs.listening)
		Sys_Printf("\n===================\n\nEnd Game\n\n===================\n\n");

	if (sv_lua_state)
	{
		lua_close(sv_lua_state);
		sv_lua_state = NULL;
	}
}

/*
===================
Game_SV_ExtraGameDataSaveFileName

Little helper function
===================
*/
void Game_SV_ExtraGameDataSaveFileName(const char *name, char *out)
{
	Sys_Snprintf(out, FILENAME_MAX, "%s.lua", name);
}

/*
===================
Game_SV_SaveExtraGameData

Saves extra game data data

TODO: ENDIANNESS, ALIGNMENT, ARCHITECTURE/ABI PADDING, KEEP IN SYNC, CHECK ERRORS HERE AND IN ALL SUBFUNCTIONS
===================
*/
void Game_SV_SaveExtraGameData(const char *name)
{
	void *handle;
	char file[FILENAME_MAX];
	const unsigned char *data;

	if (!sv_lua_state)
		return;

	Game_SV_ExtraGameDataSaveFileName(name, file);

	handle = Host_FSFileHandleOpenBinaryWrite(file);
	if (!handle)
	{
		Sys_Printf("Game_SV_SaveExtraGameData: couldn't save extra game data\n");
		return;
	}

	ProgsPrepareFunction(sv_lua_state, "SaveGame", true);
	ProgsRunFunction(sv_lua_state, 0, 1, true);
	data = ProgsGetStringFromStack(sv_lua_state, -1, false);

	Host_FSFileHandleWriteBinary(handle, data, strlen(data) + 1); /* + 1 to include the null terminator */

	ProgsFinishFunction(sv_lua_state, 1);

	Host_FSFileHandleClose(handle);
}

/*
===================
Game_SV_LoadExtraGameData

Loads extra game data data

TODO: ENDIANNESS, ALIGNMENT, ARCHITECTURE/ABI PADDING, KEEP IN SYNC, CHECK ERRORS HERE AND IN ALL SUBFUNCTIONS
===================
*/
void Game_SV_LoadExtraGameData(const char *name)
{
	char file[FILENAME_MAX];
	unsigned char *data;
	int lowmark;

	if (!sv_lua_state)
		return;

	Game_SV_ExtraGameDataSaveFileName(name, file);

	lowmark = Sys_MemLowMark(&tmp_mem);
	if (Host_FSLoadBinaryFile(file, &tmp_mem, "loadgame", &data, true) == -1)
	{
		Sys_Printf("Game_SV_LoadExtraGameData: couldn't load extra game data\n");
		return;
	}

	if(luaL_dostring(sv_lua_state, data))
	{
		Sys_MemFreeToLowMark(&tmp_mem, lowmark);
		Host_Error("Game_SV_LoadExtraGameData: error: %s\n", lua_tostring(sv_lua_state, -1));
	}

	Sys_MemFreeToLowMark(&tmp_mem, lowmark);
}

#define MAX_SPAWNPARMS_SIZE	1024*1024 /* TODO: this max size is arbitrary, beware of runtime errors! */
unsigned char spawnparms[MAX_SPAWNPARMS_SIZE];

/*
===================
Game_SV_SaveSpawnParms

Called during changelevels to save data that should be carried between server instances
===================
*/
void Game_SV_SaveSpawnParms(void)
{
	const unsigned char *data;

	if (!sv_lua_state)
		return;

	ProgsPrepareFunction(sv_lua_state, "SaveSpawnParms", true);
	ProgsRunFunction(sv_lua_state, 0, 1, true);
	data = ProgsGetStringFromStack(sv_lua_state, -1, false);

	if (strlen(data) + 1 > MAX_SPAWNPARMS_SIZE) /* + 1 to include the null terminator */
		Host_Error("Game_SV_SaveSpawnParms: saved spawnparms data size %d exceeds maximum value of %d\n", strlen(data) + 1, MAX_SPAWNPARMS_SIZE);

	spawnparms[0] = 0;
	Sys_Strncat(spawnparms, data, MAX_SPAWNPARMS_SIZE);

	ProgsFinishFunction(sv_lua_state, 1);
}

/*
===================
Game_SV_LoadSpawnParms

Called during changelevels to load data that should be carried between server instances
===================
*/
void Game_SV_LoadSpawnParms(void)
{
	if (!sv_lua_state)
		return;

	if(luaL_dostring(sv_lua_state, spawnparms))
		Host_Error("Game_SV_LoadSpawnParms: error: %s\n", lua_tostring(sv_lua_state, -1));
}

/*
===================
Game_SV_SetNewSpawnParms

Called during a new game to reset the data storage that should be carried between server instances
===================
*/
void Game_SV_SetNewSpawnParms(void)
{
	if (!sv_lua_state)
		return;

	ProgsPrepareFunction(sv_lua_state, "SetNewSpawnParms", true);
	ProgsRunFunction(sv_lua_state, 0, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_ClientSetNewSpawnParms

Called to clear the data storage that should be carried between server instances to avoid a new player using the old player's data
===================
*/
void Game_SV_ClientSetNewSpawnParms(int slot)
{
	if (!sv_lua_state)
		return;

	ProgsPrepareFunction(sv_lua_state, "ClientSetNewSpawnParms", true);
	ProgsInsertIntegerIntoStack(sv_lua_state, slot);
	ProgsRunFunction(sv_lua_state, 1, 0, true);
	ProgsFinishFunction(sv_lua_state, 0);
}

/*
===================
Game_SV_InitGame

Called when the server subsystem of the engine is initializing
===================
*/
void Game_SV_InitGame(void)
{
}

/*
===================
Game_SV_ShutdownGame

Called when the server subsystem of the engine is shutting down
===================
*/
void Game_SV_ShutdownGame(void)
{
}

