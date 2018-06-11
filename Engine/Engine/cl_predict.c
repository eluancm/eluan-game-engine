/*
This code was written by me, Eluan Costa Miranda, unless otherwise noted.
Use or distribution of this code must have explict authorization by me.
This code is copyright 2011-2018 Eluan Costa Miranda <eluancm@gmail.com>
No warranties.
*/

#include "engine.h"

/*
============================================================================

Client-side prediction

The client runs a local copy of the physics code to try to reduce the impact
of latency in the gameplay. Not everything is predicted.

TODO: voxels, more actions (shooting, etc), common lua code for movement (and aiming limits!!!)
TODO: kinematic movement prediction for non-player entities? at least player-controlled ones?
TODO: inputs get sent at 60fps, server reads them at 60fps, sometimes a frame starts in the server
with the old input because of a delay that can be <1ms! So we should repeat an input and ignore the
next one, but there is NO way to inform that (because of lag). What's the solution?

============================================================================
*/

/*
===================
CL_PredFrame

Runs a prediction frame
===================
*/
void CL_PredFrame(void)
{
	mstime_t ft = Game_CL_InputGetFrameTime();
	vec3_t saved_aimcmd;

	const client_game_input_state_t *is = Game_CL_InputGetState();
	Math_Vector3Copy(is->in_aim, cls.prediction_snapshot.edicts[cls.prediction_snapshot.cmdent].phys_aimcmd);
	Math_Vector3Copy(is->in_mov, cls.prediction_snapshot.edicts[cls.prediction_snapshot.cmdent].phys_movecmd);

	cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].angles[ANGLES_PITCH] += ft * cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].phys_aimcmd[ANGLES_PITCH] * 180.0 / 1000.0;
	cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].angles[ANGLES_YAW] += ft * cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].phys_aimcmd[ANGLES_YAW] * 180.0 / 1000.0;
	Math_Bound(-85, cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].angles[ANGLES_PITCH], 85);
	Math_Vector3Copy(cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].angles, cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].phys_locked_angles);
	cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].phys_locked_angles[ANGLES_PITCH] = 0;
	Sys_PhysicsSetTransform(cls.physworld, cls.prediction_snapshot.my_ent, NULL, cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].angles, (cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].phys_anglesflags & (ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT | ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT | ANGLES_KINEMATICANGLES_LOCK_YAW_BIT)) ? cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].phys_locked_angles : NULL);
	Math_Vector3Copy(cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].phys_aimcmd, saved_aimcmd);
	Math_ClearVector3(cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].phys_aimcmd);

	/* client pre-think */
	//ProgsPrepareFunction(cl_lua_state, "PredPreThink", true);
	//ProgsInsertDoubleIntoStack(cl_lua_state, (int)ft);
	//ProgsRunFunction(cl_lua_state, 1, 0, true);
	//ProgsFinishFunction(cl_lua_state, 0);

	/* do physics */
	if (cls.physworld)
	{
		int ignore[2];
		ignore[0] = cls.prediction_snapshot.cmdent;
		ignore[1] = -1;
		Sys_PhysicsSimulate(cls.physworld, ft, cls.prediction_snapshot.cmdent, NULL);
		//Sys_PhysicsSimulate(cls.physworld, ft, -1, ignore);
		cls.prediction_snapshot.time += ft;
	}

	Math_Vector3Copy(saved_aimcmd, cls.prediction_snapshot.edicts[cls.prediction_snapshot.my_ent].phys_aimcmd);

	Math_Vector3Copy(cls.prediction_snapshot.edicts[cls.prediction_snapshot.cmdent].angles, cls.prediction_snapshot.edicts[cls.prediction_snapshot.viewent].angles);
	Math_Vector3Copy(cls.prediction_snapshot.edicts[cls.prediction_snapshot.cmdent].origin, cls.prediction_snapshot.edicts[cls.prediction_snapshot.viewent].origin);
	cls.prediction_snapshot.edicts[cls.prediction_snapshot.viewent].origin[1] += 0.8;

	/* client post-think */
	//ProgsPrepareFunction(cl_lua_state, "PredPostThink", true);
	//ProgsInsertDoubleIntoStack(cl_lua_state, (int)ft);
	//ProgsRunFunction(cl_lua_state, 1, 0, true);
	//ProgsFinishFunction(cl_lua_state, 0);
}

/*
===================
CL_PredParseEntityFromServer

Parses a entity that was just received from the server

TODO: vehicle being recreated randomly (steering changes are triggering!!!)
TODO: lots of stuff should be forwarded to the physics subsystem and are not, causing desyncs during reconciliation!
===================
*/
void CL_PredParseEntityFromServer(snapshot_data_t *data, snapshot_data_t *old_data, entindex_t ent)
{
	vec3_t saved_origin, saved_angles;
	int interp = false;

	/* first, update the non-interpolated data */
//	if (cls.prediction_snapshot.edicts[ent].active)
//	{
//		Math_Vector3Copy(cls.prediction_snapshot.edicts[ent].origin, saved_origin);
//		Math_Vector3Copy(cls.prediction_snapshot.edicts[ent].angles, saved_angles);
//		interp = true;
//	}
	memcpy(&cls.prediction_snapshot.edicts[ent], &data->edicts[ent], sizeof(snapshot_edict_t));
//	if (interp)
//	{
//		if (ent != cls.prediction_snapshot.my_ent)
//		{
			/*Math_Vector3Add(saved_origin, data->edicts[ent].origin, cls.prediction_snapshot.edicts[ent].origin);
			Math_Vector3Scale(cls.prediction_snapshot.edicts[ent].origin, cls.prediction_snapshot.edicts[ent].origin, 0.5);
			Math_Vector3Add(saved_angles, data->edicts[ent].angles, cls.prediction_snapshot.edicts[ent].angles);
			Math_Vector3Scale(cls.prediction_snapshot.edicts[ent].angles, cls.prediction_snapshot.edicts[ent].angles, 0.5);*/
//		}
//		else
//		{
//			Math_Vector3Copy(saved_origin, cls.prediction_snapshot.edicts[ent].origin);
//			Math_Vector3Copy(saved_angles, cls.prediction_snapshot.edicts[ent].angles);
//		}
//	}

	if (!cls.physworld)
		return;

	if (cls.prediction_snapshot.edicts[ent].active != 2)
	{
		if (cls.prediction_snapshot.edicts[ent].active != old_data->edicts[ent].active)
			Sys_Printf("ent %d inactive\n", ent);
		if (cls.prediction_snapshot.edicts[ent].active == 1)
			Sys_PhysicsDestroyObject(cls.physworld, ent);
		else
		{
			/* cls.prediction_snapshot.edicts[ent].active == 0 but we don't know if invisible or destroyed, so just make not solid (and force setting the solid status when going active again) */
			/* TODO: an invisible flag? */
			/* TODO: this makes stuff just fall invisibly - must remove recreating below in the create = true for this case */
			/* Sys_PhysicsSetSolidState(cls.physworld, ent, SOLID_WORLD_NOT); */
			/* TODO: this makes it costly to get into view again) */
			Sys_PhysicsDestroyObject(cls.physworld, ent);
		}
	}
	else
	{
		int usemass = true;
		int use_lockedangles = false;
		int create = false;
		int transform = true;
		int solid = false;

		/* TODO: this must be set before connecting, otherwise it will only have effect when entities transition from invisible to visible */
		/* TODO: this assumes players have these fixed entity numbers */
		/* TODO: more stuff to predict  when not doing full prediction? */
		if (!cl_fullpredict->doublevalue && ((ent == 0 || ent > MAX_CLIENTS) && cls.prediction_snapshot.edicts[ent].phys_type != PHYSICS_SHAPE_VEHICLE1))
			usemass = false;

		if (cls.prediction_snapshot.edicts[ent].phys_anglesflags & (ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT | ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT | ANGLES_KINEMATICANGLES_LOCK_YAW_BIT))
			use_lockedangles = true;

		if (cls.prediction_snapshot.edicts[ent].active != old_data->edicts[ent].active)
		{
			Sys_Printf("ent %d active\n", ent);
            create = true;
            solid = true;
		}
		if (cls.prediction_snapshot.edicts[ent].phys_solid != old_data->edicts[ent].phys_solid)
		{
			Sys_Printf("ent %d solid\n", ent);
			solid = true;
			}
		if (cls.prediction_snapshot.edicts[ent].phys_type != old_data->edicts[ent].phys_type)
		{
			Sys_Printf("ent %d type\n", ent);
			create = true;
			}
		/*if (bitfields->bitfield_edicts[ent] & MODIFIED_PHYS_CREATION_VECS && (cls.prediction_snapshot.edicts[ent].phys_type == PHYSICS_SHAPE_BOX || cls.prediction_snapshot.edicts[ent].phys_type == PHYSICS_SHAPE_SPHERE || cls.prediction_snapshot.edicts[ent].phys_type == PHYSICS_SHAPE_CAPSULE_Y))
		{
			Sys_Printf("ent %d creavecs\n", ent);
			create = true;
			}
		if (bitfields->bitfield_edicts[ent] & MODIFIED_PHYS_CREATION_MODEL && (cls.prediction_snapshot.edicts[ent].phys_type == PHYSICS_SHAPE_TRIMESH_FROM_MODEL || cls.prediction_snapshot.edicts[ent].phys_type == PHYSICS_SHAPE_CONVEXHULLS_FROM_MODEL || cls.prediction_snapshot.edicts[ent].phys_type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL))
		{
			Sys_Printf("ent %d creamodel\n", ent);
			create = true;
			}*/
		if (!Host_BinaryCompare(&cls.prediction_snapshot.edicts[ent].phys_creation_vehicle1, &old_data->edicts[ent].phys_creation_vehicle1, sizeof(phys_edict_vehicle_info_t)) && cls.prediction_snapshot.edicts[ent].phys_type == PHYSICS_SHAPE_VEHICLE1)
		{
			Sys_Printf("ent %d creavehi\n", ent);
			create = true;
		}
		/*if (bitfields->bitfield_edicts[ent] & MODIFIED_PHYS_MASS)
		{
			Sys_Printf("ent %d mass\n", ent);
			create = true;
		}
		if (bitfields->bitfield_edicts[ent] & MODIFIED_PHYS_TRACE_ONGROUND)
		{
			Sys_Printf("ent %d ngroudn\n", ent);
			create = true;
		}*/

        if (create)
        {
        	Sys_Printf("creating %d\n", ent);
            transform = false;
            switch (cls.prediction_snapshot.edicts[ent].phys_type)
            {
				case PHYSICS_SHAPE_BOX:
				case PHYSICS_SHAPE_SPHERE:
				case PHYSICS_SHAPE_CAPSULE_Y:
					Sys_PhysicsCreateObject(cls.physworld, ent, cls.prediction_snapshot.edicts[ent].phys_type, cls.prediction_snapshot.edicts[ent].phys_creation_vecs, usemass ? cls.prediction_snapshot.edicts[ent].phys_mass : -1, cls.prediction_snapshot.edicts[ent].origin, cls.prediction_snapshot.edicts[ent].angles, use_lockedangles ? cls.prediction_snapshot.edicts[ent].phys_locked_angles : NULL, cls.prediction_snapshot.edicts[ent].phys_trace_onground);
					break;
				case PHYSICS_SHAPE_TRIMESH_FROM_MODEL:
				case PHYSICS_SHAPE_CONVEXHULLS_FROM_MODEL:
				case PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL:
					Sys_PhysicsCreateObject(cls.physworld, ent, cls.prediction_snapshot.edicts[ent].phys_type, &cls.prediction_snapshot.edicts[ent].phys_creation_model, usemass ? cls.prediction_snapshot.edicts[ent].phys_mass : -1, cls.prediction_snapshot.edicts[ent].origin, cls.prediction_snapshot.edicts[ent].angles, use_lockedangles ? cls.prediction_snapshot.edicts[ent].phys_locked_angles : NULL, cls.prediction_snapshot.edicts[ent].phys_trace_onground);
					break;
				case PHYSICS_SHAPE_VEHICLE1:
					Sys_PhysicsCreateObject(cls.physworld, ent, cls.prediction_snapshot.edicts[ent].phys_type, &cls.prediction_snapshot.edicts[ent].phys_creation_vehicle1, usemass ? cls.prediction_snapshot.edicts[ent].phys_mass : -1, cls.prediction_snapshot.edicts[ent].origin, cls.prediction_snapshot.edicts[ent].angles, use_lockedangles ? cls.prediction_snapshot.edicts[ent].phys_locked_angles : NULL, cls.prediction_snapshot.edicts[ent].phys_trace_onground);
					break;
				case PHYSICS_SHAPE_TRIMESH_FROM_DATA:
				case PHYSICS_SHAPE_CONVEXHULLS_FROM_DATA:
				case PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA:
					Host_Error("CL_PredParseEntityFromServer: creation information for type %d not synced yet (ent %d)", cls.prediction_snapshot.edicts[ent].phys_type, ent);
					break;
				case PHYSICS_SHAPE_VOXEL_BOX:
				case PHYSICS_SHAPE_VOXEL_TRIMESH:
				default:
					Host_Error("CL_PredParseEntityFromServer: physics type %d not valid for entities (ent %d)", cls.prediction_snapshot.edicts[ent].phys_type, ent);
					break;
            }
        }
        if (transform)
			Sys_PhysicsSetTransform(cls.physworld, ent, cls.prediction_snapshot.edicts[ent].origin, cls.prediction_snapshot.edicts[ent].angles, use_lockedangles ? cls.prediction_snapshot.edicts[ent].phys_locked_angles : NULL);
		if (solid)
		{
			if (usemass)
				Sys_PhysicsSetSolidState(cls.physworld, ent, cls.prediction_snapshot.edicts[ent].phys_solid);
			else
			{
				switch (cls.prediction_snapshot.edicts[ent].phys_solid)
				{
					case SOLID_ENTITY_WITHWORLDONLY:
						Sys_PhysicsSetSolidState(cls.physworld, ent, SOLID_WORLD_NOT);
					case SOLID_ENTITY_TRIGGER:
						Sys_PhysicsSetSolidState(cls.physworld, ent, SOLID_WORLD_TRIGGER);
					case SOLID_ENTITY:
						Sys_PhysicsSetSolidState(cls.physworld, ent, SOLID_WORLD);
					default:
						Sys_PhysicsSetSolidState(cls.physworld, ent, cls.prediction_snapshot.edicts[ent].phys_solid);
				}
			}
		}

		Sys_PhysicsSetLinearVelocity(cls.physworld, ent, cls.prediction_snapshot.edicts[ent].velocity);
		Sys_PhysicsSetAngularVelocity(cls.physworld, ent, cls.prediction_snapshot.edicts[ent].avelocity);
	}
}

/*
===================
CL_PredParseFromServer

Parses general data that was just received from the server
===================
*/
void CL_PredParseFromServer(snapshot_data_t *data, snapshot_data_t *old_data)
{
	entindex_t i;
	unsigned short reconciliate_from_input, current_input;
	int reconciliation_input_found = false;

	cls.prediction_snapshot.id = data->id;
	cls.prediction_snapshot.base_id = data->base_id;
	cls.prediction_snapshot.viewent = data->viewent;
	cls.prediction_snapshot.cameraent = data->cameraent;
	cls.prediction_snapshot.my_ent = data->my_ent;
	cls.prediction_snapshot.paused = data->paused;
	cls.prediction_snapshot.time = data->time;
	cls.prediction_snapshot.cmdent = data->cmdent;

	reconciliate_from_input = cls.current_acked_input + 1; /* the current acked was already processed in this snapshot */
	current_input = Game_CL_InputGetSequence();
	if (Game_CL_InputSetCurrentBySequence(reconciliate_from_input))
		reconciliation_input_found = true;
	for (i = 0; i < MAX_EDICTS; i++)
	{
		if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & (MODIFIED_ORIGIN | MODIFIED_VELOCITY)) /* TODO: update for the ones we receive no data because of visibility */
			Sys_SoundUpdateEntityAttribs(&i, data->edicts[i].origin, data->edicts[i].velocity); /* TODO: being updated by prediction code already */
		CL_PredParseEntityFromServer(data, old_data, i);
	}
//	if (!reconciliation_input_found)
//		Sys_Printf("Not reconciliating!!! cur = %5u, ack = %5u\n", current_input, reconciliate_from_input);
	while (reconciliation_input_found)
	{
		CL_PredFrame();
		if (Game_CL_InputGetSequence() == current_input)
			break;
		Game_CL_InputSetNext();
	}
}

/*
===================
CL_PredUpdatePhysStats

Called by the physics code to update physics data for any entity with an active physical representation
===================
*/
void CL_PredUpdatePhysStats(entindex_t ent, vec3_t origin, vec3_t angles, vec3_t velocity, vec3_t avelocity, int onground)
{
	Math_Vector3Copy(origin, cls.prediction_snapshot.edicts[ent].origin);
	Math_Vector3Copy(angles, cls.prediction_snapshot.edicts[ent].angles);
	Math_Vector3Copy(velocity, cls.prediction_snapshot.edicts[ent].velocity);
	Math_Vector3Copy(avelocity, cls.prediction_snapshot.edicts[ent].avelocity);
	/* cls.prediction_snapshot.edicts[ent].onground = onground; */

	Sys_SoundUpdateEntityAttribs(&ent, cls.prediction_snapshot.edicts[ent].origin, cls.prediction_snapshot.edicts[ent].velocity);
}

/*
===================
CL_PredUpdatePhysDirections

Called by the physics code to update physics data for any entity with an active physical representation
===================
*/
void CL_PredUpdatePhysDirections(entindex_t ent, vec3_t forward, vec3_t right, vec3_t up)
{
	/* Math_Vector3Copy(forward, cls.prediction_snapshot.edicts[ent].forward); */
	/* Math_Vector3Copy(right, cls.prediction_snapshot.edicts[ent].right); */
	/* Math_Vector3Copy(up, cls.prediction_snapshot.edicts[ent].up); */
}

/*
===================
CL_PredUpdateTraceResultStart

Called by the physics code to begin updating traceline results
===================
*/
void CL_PredUpdateTraceResultStart(entindex_t ent)
{
	Sys_Error("CL_PredUpdateTraceResultStart: Not implemented");
}

/*
===================
CL_PredUpdateTraceResultStep

Called by the physics code to add a new traceline result for this entity
Returns the index of the last added result, -1 if it was ignored
===================
*/
int CL_PredUpdateTraceResultStep(entindex_t ent, entindex_t hit, vec3_t pos, vec3_t normal, vec_t fraction)
{
	Sys_Error("CL_PredUpdateTraceResultStep: Not implemented");
	return 0;
}

/*
===================
CL_PredPostPhysics

Called by the physics code immediately after a frame is run
===================
*/
void CL_PredPostPhysics(void)
{
}

/*
===================
CL_PredGetMoveType

Called by the physics code to know the movetype of an object
===================
*/
const int CL_PredGetMoveType(entindex_t ent)
{
	return cls.prediction_snapshot.edicts[ent].phys_movetype;
}

/*
===================
CL_PredGetAnglesFlags

Called by the physics code to know the anglesflags of an object
===================
*/
const unsigned int CL_PredGetAnglesFlags(entindex_t ent)
{
	return cls.prediction_snapshot.edicts[ent].phys_anglesflags;
}

/*
===================
CL_PredGetMoveCmd

Called by the physics code to know the move command of an object
===================
*/
void CL_PredGetMoveCmd(entindex_t ent, vec_t *dest)
{
	Math_Vector3Copy(cls.prediction_snapshot.edicts[ent].phys_movecmd, dest);
}

/*
===================
CL_PredGetAimCmd

Called by the physics code to know the aim command of an object
===================
*/
void CL_PredGetAimCmd(entindex_t ent, vec_t *dest)
{
	Math_Vector3Copy(cls.prediction_snapshot.edicts[ent].phys_aimcmd, dest);
}

/*
===================
CL_PredGetMaxSpeed

Called by the physics code to know the max speed of an object in each axis
TODO: rotate this vector
===================
*/
void CL_PredGetMaxSpeed(entindex_t ent, vec_t *dest)
{
	Math_Vector3Copy(cls.prediction_snapshot.edicts[ent].phys_maxspeed, dest);
}

/*
===================
CL_PredGetAcceleration

Called by the physics code to know the acceleration of an object in each axis
TODO: rotate this vector
===================
*/
void CL_PredGetAcceleration(entindex_t ent, vec_t *dest)
{
	Math_Vector3Copy(cls.prediction_snapshot.edicts[ent].phys_acceleration, dest);
}

/*
===================
CL_PredGetIgnoreGravity

Called by the physics code to know this entity wants to ignore gravity
===================
*/
int CL_PredGetIgnoreGravity(entindex_t ent)
{
	return cls.prediction_snapshot.edicts[ent].phys_ignore_gravity;
}

/*
===================
CL_PredCheckPhysicalCollisionResponse

Last check done by the physics code to see if two entities should generate
a collision response. (i.e. prevent inter-penetration)
===================
*/
int CL_PredCheckPhysicalCollisionResponse(entindex_t e1, entindex_t e2)
{
	/* not enought data yet, so return TRUE always */
	return true;
}

/*
===================
CL_PredTouchEnts

Called by physics code to run the touch function of an entity
Touched entities should not be removed inside touch functions because other entities
may want to touch it in the same frame, or worse: new entities may occupy it's slot
and get touched!
To remove, set a next think for the next millisecond. The think functions will
run before the next frame's physics code. (Use SUB_Remove for the think function)
===================
*/
void CL_PredTouchEnts(entindex_t who, entindex_t by, vec3_t pos, vec3_t normal, vec_t distance, int reaction, vec_t impulse)
{
}

/*
===================
CL_PredEntityGetData

Called to get lots of data about an entity faster. Any field can be null.
===================
*/
void CL_PredEntityGetData(const entindex_t ent, vec_t *origin, vec_t *velocity, vec_t *avelocity, vec_t *angles, vec_t *frames, precacheindex_t *modelindex, vec_t *lightintensity, int *anim_pitch)
{
	Sys_Error("CL_PredEntityGetData: Not implemented");
}

/*
===================
CL_GetModelPhysicsTrimesh

Gets a trimesh from the model.
Memory allocated from this function will only be
freed when a new client starts.
===================
*/
void CL_GetModelPhysicsTrimesh(const precacheindex_t model, model_trimesh_t **trimesh)
{
	if (model < 0 || model >= cls.precached_models_num)
		Host_Error("CL_GetModelPhysicsTrimesh: model %d not precached\n", model);

	Sys_LoadModelPhysicsTrimesh(cls.precached_models[model]->data, trimesh);
}

/*
===================
CL_GetModelPhysicsBrushes

Gets a list of convex brushes from the model.
Memory allocated from this function will only be
freed when a new client starts.
===================
*/
void CL_GetModelPhysicsBrushes(const precacheindex_t model, model_brushes_t **brushes)
{
	if (model < 0 || model >= cls.precached_models_num)
		Host_Error("CL_GetModelPhysicsBrushes: model %d not precached\n", model);

	Sys_LoadModelPhysicsBrushes(cls.precached_models[model]->data, brushes);
}

/*
===================
CL_GetModelPhysicsHeightfield

Gets a heightfield from the model.
Memory allocated from this function will only be
freed when a new client starts.
===================
*/
void CL_GetModelPhysicsHeightfield(const precacheindex_t model, model_heightfield_t **heightfield)
{
	if (model < 0 || model >= cls.precached_models_num)
		Host_Error("CL_GetModelPhysicsHeightfield: model %d not precached\n", model);

	Sys_LoadModelPhysicsHeightfield(cls.precached_models[model]->data, heightfield);
}
