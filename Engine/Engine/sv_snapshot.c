/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2016 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Server game view snapshot management

Snapshots run on top of the netchannel. Adding a new field requires changes
in: get data, compare if visible or copy from base if not, send.

TODO: use instrospection so that we don't even know what we are sending (and
send only the real bytes that changed, not a whole struct or int64, for example)
TODO: there may BE STILL some issues with padding/etc with cross-platform
play

============================================================================
*/

/*
===================
SV_SnapshotCreate

Creates a new differential snapshot for a client
TODO: do not create if equal to the last one?
===================
*/
void SV_SnapshotCreate(int slot)
{
	int j;
	entindex_t i;
	int newindex;
	snapshot_data_t *snapshot;
	snapshot_bitfields_t *snapshot_bitfield;
	const snapshot_data_t *base_snapshot;
	entindex_t newviewent = Game_SV_ClientGetViewEnt(slot);
	vec3_t eyeorigin;
	uint32_t crc32;
	precacheindex_t ent_modelindex;
	vec_t ent_lightintensity;

	/* TODO FIXME: this all of this data is needed, just send anything */
	Game_SV_EntityGetData(newviewent, eyeorigin, NULL, NULL, NULL, NULL, &ent_modelindex, &ent_lightintensity, NULL);

	Game_SV_VisibilityPrepare(eyeorigin);
	Game_SV_EntityGetData(newviewent, NULL, NULL, NULL, NULL, NULL, &ent_modelindex, &ent_lightintensity, NULL);
	if (!Game_SV_EntityIsVisible(newviewent, slot, eyeorigin, newviewent, ent_modelindex, ent_lightintensity))
		Host_Error("Client %d viewent (%d) is not valid for sending to client\n", slot, newviewent);

	/* allocate a new one in the circular buffer */
	/* TODO research this: using last_created_snapshot uses less bandwith but makes packet losses worse? Using last_acknowledged_snapshot makes the current PL calculation less meaningful */
	base_snapshot = &svs.sv_clients[slot].snapshots[svs.sv_clients[slot].last_acknowledged_snapshot]; /* this is the base */
	do
	{
		newindex = (svs.sv_clients[slot].last_created_snapshot + 1) & MAX_SAVED_SNAPSHOTS_MASK; /* this is the new one */
		svs.sv_clients[slot].last_created_snapshot = newindex;
	} while (newindex == svs.sv_clients[slot].last_acknowledged_snapshot); /* protect our base snapshot from overwriting if the network is bad */
	snapshot = &svs.sv_clients[slot].snapshots[newindex];
	snapshot_bitfield = &svs.sv_clients[slot].snapshots_modified_bitfields[newindex];
	svs.sv_clients[slot].snapshots_times[newindex] = host_realtime;

	/* basic client data */
	svs.sv_clients[slot].snapshot_counter++;
	if (!svs.sv_clients[slot].snapshot_counter)
		svs.sv_clients[slot].snapshot_counter++; /* never use zero, it's special for blank states TODO FIXME: lot's of places in the engine where we overflow back to zero and use it, but shouldn't. After doing this, see where we check + 1 to ignore the zero if we are in the max value before overflow!!! (in this case, the PL counter) */
	snapshot->id = svs.sv_clients[slot].snapshot_counter;
	snapshot->base_id = base_snapshot->id;
	snapshot->viewent = newviewent;
	snapshot->cameraent = Game_SV_ClientGetCameraEnt(slot);
	snapshot->my_ent = slot + 1; /* (starts at 1, zero is world) - can't just ignore the entity because we need origin, velocity, etc for sound and the model for rendering from other points of view */
	snapshot->paused = (unsigned char)svs.paused;
	snapshot->time = Game_SV_GetTime();
	snapshot->cmdent = Game_SV_ClientGetCmdEnt(slot);

	/* calculating manually because of possible padding TODO FIXME: out of order messages INSIDE the update (like swapped entity updates) should not cause errors, but they will (update: since now the modified bitfields are cached and the crc checks done AFTER the end is received, a error will only happen if begin and/or end is received in the wrong order */
	crc32 = Host_CRC32(0xffffffff, &snapshot->id, sizeof(snapshot->id));
	/* see what has changed, do a binary compare because some different floating point data may compare equally */
	if (!Host_BinaryCompare(&snapshot->viewent, &base_snapshot->viewent, sizeof(base_snapshot->viewent)))
	{
		crc32 = Host_CRC32(crc32, &snapshot->viewent, sizeof(snapshot->viewent));
		snapshot_bitfield->bitfield_client |= MODIFIED_VIEWENT;
	}
	else
		snapshot_bitfield->bitfield_client &= (~MODIFIED_VIEWENT);
	if (!Host_BinaryCompare(&snapshot->cameraent, &base_snapshot->cameraent, sizeof(base_snapshot->cameraent)))
	{
		crc32 = Host_CRC32(crc32, &snapshot->cameraent, sizeof(snapshot->cameraent));
		snapshot_bitfield->bitfield_client |= MODIFIED_CAMERAENT;
	}
	else
		snapshot_bitfield->bitfield_client &= (~MODIFIED_CAMERAENT);
	if (!Host_BinaryCompare(&snapshot->my_ent, &base_snapshot->my_ent, sizeof(base_snapshot->my_ent)))
	{
		crc32 = Host_CRC32(crc32, &snapshot->my_ent, sizeof(snapshot->my_ent));
		snapshot_bitfield->bitfield_client |= MODIFIED_MY_ENT;
	}
	else
		snapshot_bitfield->bitfield_client &= (~MODIFIED_MY_ENT);
	if (!Host_BinaryCompare(&snapshot->paused, &base_snapshot->paused, sizeof(base_snapshot->paused)))
	{
		crc32 = Host_CRC32(crc32, &snapshot->paused, sizeof(snapshot->paused));
		snapshot_bitfield->bitfield_client |= MODIFIED_PAUSED;
	}
	else
		snapshot_bitfield->bitfield_client &= (~MODIFIED_PAUSED);
	if (!Host_BinaryCompare(&snapshot->time, &base_snapshot->time, sizeof(base_snapshot->time)))
	{
		crc32 = Host_CRC32(crc32, &snapshot->time, sizeof(snapshot->time));
		snapshot_bitfield->bitfield_client |= MODIFIED_TIME;
	}
	else
		snapshot_bitfield->bitfield_client &= (~MODIFIED_TIME);
	if (!Host_BinaryCompare(&snapshot->cmdent, &base_snapshot->cmdent, sizeof(base_snapshot->cmdent)))
	{
		crc32 = Host_CRC32(crc32, &snapshot->cmdent, sizeof(snapshot->cmdent));
		snapshot_bitfield->bitfield_client |= MODIFIED_CMDENT;
	}
	else
		snapshot_bitfield->bitfield_client &= (~MODIFIED_CMDENT);

	for (i = 0; i < MAX_EDICTS; i++) /* TODO: should not refer to that structure directly? */
	{
		/* we need to cache some data first */
		Game_SV_EntityGetData(i, NULL, NULL, NULL, NULL, NULL, &ent_modelindex, &ent_lightintensity, NULL);

		/* a inactive/invisible entity will just be set to inactive and all data will be mirrored from the last acknowledged snapshot to avoid sending superfulous data */
		if (Game_SV_EntityIsVisible(newviewent, slot, eyeorigin, i, ent_modelindex, ent_lightintensity))
		{
			int anim_pitch;
			Game_SV_EntityGetData(i, snapshot->edicts[i].origin, snapshot->edicts[i].velocity, snapshot->edicts[i].avelocity, snapshot->edicts[i].angles, snapshot->edicts[i].frame, NULL, NULL, &anim_pitch);
			snapshot->edicts[i].model = ent_modelindex;
			snapshot->edicts[i].light_intensity = ent_lightintensity;
			snapshot->edicts[i].anim_pitch = anim_pitch; /* cast it TODO FIXME: will we ever have bigger values here? */

			if (Sys_PhysicsGetEntityData(svs.physworld, i, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
			{
				int trace_onground, creation_model;

				Sys_PhysicsGetEntityData(svs.physworld, i, snapshot->edicts[i].phys_locked_angles, &snapshot->edicts[i].phys_solid, &snapshot->edicts[i].phys_type, snapshot->edicts[i].phys_creation_vecs, &creation_model, &snapshot->edicts[i].phys_creation_vehicle1, &snapshot->edicts[i].phys_mass, &trace_onground);

				Game_SV_GetMoveCmd(i, snapshot->edicts[i].phys_movecmd);
				Game_SV_GetAimCmd(i, snapshot->edicts[i].phys_aimcmd);
				snapshot->edicts[i].phys_creation_model = creation_model; /* cast it TODO FIXME: will we ever have bigger values here? */
				snapshot->edicts[i].phys_trace_onground = trace_onground; /* cast it TODO FIXME: will we ever have bigger values here? */
				snapshot->edicts[i].phys_movetype = Game_SV_GetMoveType(i);
				Game_SV_GetAimCmd(i, snapshot->edicts[i].phys_aimcmd);
				snapshot->edicts[i].phys_anglesflags = Game_SV_GetAnglesFlags(i);
				Game_SV_GetMaxSpeed(i, snapshot->edicts[i].phys_maxspeed);
				Game_SV_GetAcceleration(i, snapshot->edicts[i].phys_acceleration);
				snapshot->edicts[i].phys_ignore_gravity = Game_SV_GetIgnoreGravity(i); /* cast it TODO FIXME: will we ever have bigger values here? */

				/* if not used, it wasn't updated by Sys_PhysicsGetEntityData, so get the last good value */
				if (!(snapshot->edicts[i].phys_anglesflags & (ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT | ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT | ANGLES_KINEMATICANGLES_LOCK_YAW_BIT)))
					Math_Vector3Copy(base_snapshot->edicts[i].phys_locked_angles, snapshot->edicts[i].phys_locked_angles);

				if (!Host_BinaryCompare(snapshot->edicts[i].phys_locked_angles, base_snapshot->edicts[i].phys_locked_angles, sizeof(base_snapshot->edicts[i].phys_locked_angles)))
				{
					crc32 = Host_CRC32(crc32, snapshot->edicts[i].phys_locked_angles, sizeof(snapshot->edicts[i].phys_locked_angles));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_LOCKED_ANGLES;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_LOCKED_ANGLES);

				if (!Host_BinaryCompare(snapshot->edicts[i].phys_movecmd, base_snapshot->edicts[i].phys_movecmd, sizeof(base_snapshot->edicts[i].phys_movecmd)))
				{
					crc32 = Host_CRC32(crc32, snapshot->edicts[i].phys_movecmd, sizeof(snapshot->edicts[i].phys_movecmd));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_MOVECMD;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_MOVECMD);

				if (!Host_BinaryCompare(snapshot->edicts[i].phys_aimcmd, base_snapshot->edicts[i].phys_aimcmd, sizeof(base_snapshot->edicts[i].phys_aimcmd)))
				{
					crc32 = Host_CRC32(crc32, snapshot->edicts[i].phys_aimcmd, sizeof(snapshot->edicts[i].phys_aimcmd));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_AIMCMD;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_AIMCMD);

				if (!Host_BinaryCompare(&snapshot->edicts[i].phys_solid, &base_snapshot->edicts[i].phys_solid, sizeof(base_snapshot->edicts[i].phys_solid)))
				{
					crc32 = Host_CRC32(crc32, &snapshot->edicts[i].phys_solid, sizeof(snapshot->edicts[i].phys_solid));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_SOLID;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_SOLID);

				if (!Host_BinaryCompare(&snapshot->edicts[i].phys_type, &base_snapshot->edicts[i].phys_type, sizeof(base_snapshot->edicts[i].phys_type)))
				{
					crc32 = Host_CRC32(crc32, &snapshot->edicts[i].phys_type, sizeof(snapshot->edicts[i].phys_type));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_TYPE;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_TYPE);

				if (!Host_BinaryCompare(snapshot->edicts[i].phys_creation_vecs, base_snapshot->edicts[i].phys_creation_vecs, sizeof(base_snapshot->edicts[i].phys_creation_vecs)))
				{
					crc32 = Host_CRC32(crc32, snapshot->edicts[i].phys_creation_vecs, sizeof(snapshot->edicts[i].phys_creation_vecs));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_CREATION_VECS;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_CREATION_VECS);

				if (!Host_BinaryCompare(&snapshot->edicts[i].phys_creation_model, &base_snapshot->edicts[i].phys_creation_model, sizeof(base_snapshot->edicts[i].phys_creation_model)))
				{
					crc32 = Host_CRC32(crc32, &snapshot->edicts[i].phys_creation_model, sizeof(snapshot->edicts[i].phys_creation_model));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_CREATION_MODEL;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_CREATION_MODEL);

				/* TODO: assuming this struct is tightly packed (only vecs and ints as of this writing) */
				if (!Host_BinaryCompare(&snapshot->edicts[i].phys_creation_vehicle1, &base_snapshot->edicts[i].phys_creation_vehicle1, sizeof(base_snapshot->edicts[i].phys_creation_vehicle1)))
				{
					crc32 = Host_CRC32(crc32, &snapshot->edicts[i].phys_creation_vehicle1, sizeof(snapshot->edicts[i].phys_creation_vehicle1));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_CREATION_VEHICLE1;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_CREATION_VEHICLE1);

				if (!Host_BinaryCompare(&snapshot->edicts[i].phys_mass, &base_snapshot->edicts[i].phys_mass, sizeof(base_snapshot->edicts[i].phys_mass)))
				{
					crc32 = Host_CRC32(crc32, &snapshot->edicts[i].phys_mass, sizeof(snapshot->edicts[i].phys_mass));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_MASS;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_MASS);

				if (!Host_BinaryCompare(&snapshot->edicts[i].phys_trace_onground, &base_snapshot->edicts[i].phys_trace_onground, sizeof(base_snapshot->edicts[i].phys_trace_onground)))
				{
					crc32 = Host_CRC32(crc32, &snapshot->edicts[i].phys_trace_onground, sizeof(snapshot->edicts[i].phys_trace_onground));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_TRACE_ONGROUND;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_TRACE_ONGROUND);

				if (!Host_BinaryCompare(&snapshot->edicts[i].phys_movetype, &base_snapshot->edicts[i].phys_movetype, sizeof(base_snapshot->edicts[i].phys_movetype)))
				{
					crc32 = Host_CRC32(crc32, &snapshot->edicts[i].phys_movetype, sizeof(snapshot->edicts[i].phys_movetype));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_MOVETYPE;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_MOVETYPE);

				if (!Host_BinaryCompare(&snapshot->edicts[i].phys_anglesflags, &base_snapshot->edicts[i].phys_anglesflags, sizeof(base_snapshot->edicts[i].phys_anglesflags)))
				{
					crc32 = Host_CRC32(crc32, &snapshot->edicts[i].phys_anglesflags, sizeof(snapshot->edicts[i].phys_anglesflags));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_ANGLESFLAGS;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_ANGLESFLAGS);

				if (!Host_BinaryCompare(snapshot->edicts[i].phys_maxspeed, base_snapshot->edicts[i].phys_maxspeed, sizeof(base_snapshot->edicts[i].phys_maxspeed)))
				{
					crc32 = Host_CRC32(crc32, snapshot->edicts[i].phys_maxspeed, sizeof(snapshot->edicts[i].phys_maxspeed));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_MAXSPEED;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_MAXSPEED);

				if (!Host_BinaryCompare(snapshot->edicts[i].phys_acceleration, base_snapshot->edicts[i].phys_acceleration, sizeof(base_snapshot->edicts[i].phys_acceleration)))
				{
					crc32 = Host_CRC32(crc32, snapshot->edicts[i].phys_acceleration, sizeof(snapshot->edicts[i].phys_acceleration));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_ACCELERATION;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_ACCELERATION);

				if (!Host_BinaryCompare(&snapshot->edicts[i].phys_ignore_gravity, &base_snapshot->edicts[i].phys_ignore_gravity, sizeof(base_snapshot->edicts[i].phys_ignore_gravity)))
				{
					crc32 = Host_CRC32(crc32, &snapshot->edicts[i].phys_ignore_gravity, sizeof(snapshot->edicts[i].phys_ignore_gravity));
					snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_PHYS_IGNORE_GRAVITY;
				}
				else
					snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_PHYS_IGNORE_GRAVITY);

				snapshot->edicts[i].active = 2;
			}
			else
			{
				Math_Vector3Copy(base_snapshot->edicts[i].phys_locked_angles, snapshot->edicts[i].phys_locked_angles);
				Math_Vector3Copy(base_snapshot->edicts[i].phys_movecmd, snapshot->edicts[i].phys_movecmd);
				Math_Vector3Copy(base_snapshot->edicts[i].phys_aimcmd, snapshot->edicts[i].phys_aimcmd);
				snapshot->edicts[i].phys_solid = base_snapshot->edicts[i].phys_solid;
				snapshot->edicts[i].phys_type = base_snapshot->edicts[i].phys_type;
				Math_Vector3Copy(base_snapshot->edicts[i].phys_creation_vecs, snapshot->edicts[i].phys_creation_vecs);
				snapshot->edicts[i].phys_creation_model = base_snapshot->edicts[i].phys_creation_model;
				snapshot->edicts[i].phys_creation_vehicle1 = base_snapshot->edicts[i].phys_creation_vehicle1;
				snapshot->edicts[i].phys_mass = base_snapshot->edicts[i].phys_mass;
				snapshot->edicts[i].phys_trace_onground = base_snapshot->edicts[i].phys_trace_onground;
				snapshot->edicts[i].phys_movetype = base_snapshot->edicts[i].phys_movetype;
				snapshot->edicts[i].phys_anglesflags = base_snapshot->edicts[i].phys_anglesflags;
				Math_Vector3Copy(base_snapshot->edicts[i].phys_maxspeed, snapshot->edicts[i].phys_maxspeed);
				Math_Vector3Copy(base_snapshot->edicts[i].phys_acceleration, snapshot->edicts[i].phys_acceleration);
				snapshot->edicts[i].phys_ignore_gravity = base_snapshot->edicts[i].phys_ignore_gravity;

				/* no need to check for diferences in the above, except for the active flag which will be checked out of this if-else-block */
				snapshot_bitfield->bitfield_edicts[i] &= (~(MODIFIED_PHYS_LOCKED_ANGLES & MODIFIED_PHYS_MOVECMD & MODIFIED_PHYS_AIMCMD & MODIFIED_PHYS_SOLID & MODIFIED_PHYS_TYPE & MODIFIED_PHYS_CREATION_VECS & MODIFIED_PHYS_CREATION_MODEL & MODIFIED_PHYS_CREATION_VEHICLE1 & MODIFIED_PHYS_MASS & MODIFIED_PHYS_TRACE_ONGROUND & MODIFIED_PHYS_MOVETYPE & MODIFIED_PHYS_ANGLESFLAGS & MODIFIED_PHYS_MAXSPEED & MODIFIED_PHYS_ACCELERATION & MODIFIED_PHYS_IGNORE_GRAVITY));

				snapshot->edicts[i].active = 1;
			}

			/* see what has changed, do a binary compare because some different floating point data may compare equally */
			if (!Host_BinaryCompare(&snapshot->edicts[i].active, &base_snapshot->edicts[i].active, sizeof(base_snapshot->edicts[i].active)))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].active, sizeof(snapshot->edicts[i].active));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_ACTIVE;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_ACTIVE);

			if (!Host_BinaryCompare(snapshot->edicts[i].origin, base_snapshot->edicts[i].origin, sizeof(base_snapshot->edicts[i].origin)))
			{
				crc32 = Host_CRC32(crc32, snapshot->edicts[i].origin, sizeof(snapshot->edicts[i].origin));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_ORIGIN;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_ORIGIN);

			if (!Host_BinaryCompare(snapshot->edicts[i].velocity, base_snapshot->edicts[i].velocity, sizeof(base_snapshot->edicts[i].velocity)))
			{
				crc32 = Host_CRC32(crc32, snapshot->edicts[i].velocity, sizeof(snapshot->edicts[i].velocity));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_VELOCITY;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_VELOCITY);

			if (!Host_BinaryCompare(snapshot->edicts[i].avelocity, base_snapshot->edicts[i].avelocity, sizeof(base_snapshot->edicts[i].avelocity)))
			{
				crc32 = Host_CRC32(crc32, snapshot->edicts[i].avelocity, sizeof(snapshot->edicts[i].avelocity));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_AVELOCITY;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_AVELOCITY);

			if (!Host_BinaryCompare(snapshot->edicts[i].angles, base_snapshot->edicts[i].angles, sizeof(base_snapshot->edicts[i].angles)))
			{
				crc32 = Host_CRC32(crc32, snapshot->edicts[i].angles, sizeof(snapshot->edicts[i].angles));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_ANGLES;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_ANGLES);

			if (!Host_BinaryCompare(&snapshot->edicts[i].frame[0], &base_snapshot->edicts[i].frame[0], sizeof(base_snapshot->edicts[i].frame[0])))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].frame[0], sizeof(snapshot->edicts[i].frame[0]));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_FRAME0;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_FRAME0);

			if (!Host_BinaryCompare(&snapshot->edicts[i].frame[1], &base_snapshot->edicts[i].frame[1], sizeof(base_snapshot->edicts[i].frame[1])))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].frame[1], sizeof(snapshot->edicts[i].frame[1]));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_FRAME1;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_FRAME1);

			if (!Host_BinaryCompare(&snapshot->edicts[i].frame[2], &base_snapshot->edicts[i].frame[2], sizeof(base_snapshot->edicts[i].frame[2])))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].frame[2], sizeof(snapshot->edicts[i].frame[2]));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_FRAME2;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_FRAME2);

			if (!Host_BinaryCompare(&snapshot->edicts[i].frame[3], &base_snapshot->edicts[i].frame[3], sizeof(base_snapshot->edicts[i].frame[3])))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].frame[3], sizeof(snapshot->edicts[i].frame[3]));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_FRAME3;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_FRAME3);

			if (!Host_BinaryCompare(&snapshot->edicts[i].frame[4], &base_snapshot->edicts[i].frame[4], sizeof(base_snapshot->edicts[i].frame[4])))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].frame[4], sizeof(snapshot->edicts[i].frame[4]));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_FRAME4;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_FRAME4);

			if (!Host_BinaryCompare(&snapshot->edicts[i].model, &base_snapshot->edicts[i].model, sizeof(base_snapshot->edicts[i].model)))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].model, sizeof(snapshot->edicts[i].model));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_MODEL;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_MODEL);

			if (!Host_BinaryCompare(&snapshot->edicts[i].light_intensity, &base_snapshot->edicts[i].light_intensity, sizeof(base_snapshot->edicts[i].light_intensity)))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].light_intensity, sizeof(snapshot->edicts[i].light_intensity));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_LIGHT_INTENSITY;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_LIGHT_INTENSITY);

			if (!Host_BinaryCompare(&snapshot->edicts[i].anim_pitch, &base_snapshot->edicts[i].anim_pitch, sizeof(base_snapshot->edicts[i].anim_pitch)))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].anim_pitch, sizeof(snapshot->edicts[i].anim_pitch));
				snapshot_bitfield->bitfield_edicts[i] |= MODIFIED_ANIM_PITCH;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] &= (~MODIFIED_ANIM_PITCH);
		}
		else
		{
			snapshot->edicts[i].active = 0;
			Math_Vector3Copy(base_snapshot->edicts[i].origin, snapshot->edicts[i].origin);
			Math_Vector3Copy(base_snapshot->edicts[i].velocity, snapshot->edicts[i].velocity);
			Math_Vector3Copy(base_snapshot->edicts[i].avelocity, snapshot->edicts[i].avelocity);
			Math_Vector3Copy(base_snapshot->edicts[i].angles, snapshot->edicts[i].angles);
			for (j = 0; j < ANIMATION_MAX_BLENDED_FRAMES; j++)
				snapshot->edicts[i].frame[j] = base_snapshot->edicts[i].frame[j];
			snapshot->edicts[i].model = base_snapshot->edicts[i].model;
			snapshot->edicts[i].light_intensity = base_snapshot->edicts[i].light_intensity;
			snapshot->edicts[i].anim_pitch = base_snapshot->edicts[i].anim_pitch;

			Math_Vector3Copy(base_snapshot->edicts[i].phys_locked_angles, snapshot->edicts[i].phys_locked_angles);
			Math_Vector3Copy(base_snapshot->edicts[i].phys_movecmd, snapshot->edicts[i].phys_movecmd);
			Math_Vector3Copy(base_snapshot->edicts[i].phys_aimcmd, snapshot->edicts[i].phys_aimcmd);
			snapshot->edicts[i].phys_solid = base_snapshot->edicts[i].phys_solid;
			snapshot->edicts[i].phys_type = base_snapshot->edicts[i].phys_type;
			Math_Vector3Copy(base_snapshot->edicts[i].phys_creation_vecs, snapshot->edicts[i].phys_creation_vecs);
			snapshot->edicts[i].phys_creation_model = base_snapshot->edicts[i].phys_creation_model;
			snapshot->edicts[i].phys_creation_vehicle1 = base_snapshot->edicts[i].phys_creation_vehicle1;
			snapshot->edicts[i].phys_mass = base_snapshot->edicts[i].phys_mass;
			snapshot->edicts[i].phys_trace_onground = base_snapshot->edicts[i].phys_trace_onground;
			snapshot->edicts[i].phys_movetype = base_snapshot->edicts[i].phys_movetype;
			snapshot->edicts[i].phys_anglesflags = base_snapshot->edicts[i].phys_anglesflags;
			Math_Vector3Copy(base_snapshot->edicts[i].phys_maxspeed, snapshot->edicts[i].phys_maxspeed);
			Math_Vector3Copy(base_snapshot->edicts[i].phys_acceleration, snapshot->edicts[i].phys_acceleration);
			snapshot->edicts[i].phys_ignore_gravity = base_snapshot->edicts[i].phys_ignore_gravity;

			/* only the active flag may be different */
			if (!Host_BinaryCompare(&snapshot->edicts[i].active, &base_snapshot->edicts[i].active, sizeof(base_snapshot->edicts[i].active)))
			{
				crc32 = Host_CRC32(crc32, &snapshot->edicts[i].active, sizeof(snapshot->edicts[i].active));
				snapshot_bitfield->bitfield_edicts[i] = MODIFIED_ACTIVE;
			}
			else
				snapshot_bitfield->bitfield_edicts[i] = 0;
		}
	}

	svs.sv_clients[slot].snapshots_crc32[newindex] = crc32;
}

/*
===================
SV_SnapshotSend

Sends a differential snapshot to a client.
Always send the last one created, old data isn't important to the client,
it may ha ve already changed. We only keep old data in case it's confirmed,
then we create diffs from it.
TODO: do not create if equal to the last one?
===================
*/
void SV_SnapshotSend(int slot)
{
	char msg[MAX_NET_CMDSIZE];
	int len = 0;
	entindex_t i;
	snapshot_data_t *snapshot;
	snapshot_bitfields_t *snapshot_bitfields;

	snapshot = &svs.sv_clients[slot].snapshots[svs.sv_clients[slot].last_created_snapshot];
	snapshot_bitfields = &svs.sv_clients[slot].snapshots_modified_bitfields[svs.sv_clients[slot].last_created_snapshot];

	MSG_WriteByte(msg, &len, SVC_BEGIN);
	MSG_WriteShort(msg, &len, snapshot->base_id);
	MSG_WriteShort(msg, &len, snapshot->id);
	MSG_WriteShort(msg, &len, Game_SV_ClientGetInputSequence(slot));
	MSG_WriteByte(msg, &len, snapshot_bitfields->bitfield_client);
	if (snapshot_bitfields->bitfield_client & MODIFIED_VIEWENT)
		MSG_WriteEntity(msg, &len, snapshot->viewent);
	if (snapshot_bitfields->bitfield_client & MODIFIED_CAMERAENT)
		MSG_WriteEntity(msg, &len, snapshot->cameraent);
	if (snapshot_bitfields->bitfield_client & MODIFIED_MY_ENT)
		MSG_WriteEntity(msg, &len, snapshot->my_ent);
	if (snapshot_bitfields->bitfield_client & MODIFIED_PAUSED)
		MSG_WriteByte(msg, &len, snapshot->paused);
	if (snapshot_bitfields->bitfield_client & MODIFIED_TIME)
		MSG_WriteTime(msg, &len, snapshot->time);
	if (snapshot_bitfields->bitfield_client & MODIFIED_CMDENT)
		MSG_WriteEntity(msg, &len, snapshot->cmdent);
	Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, msg, len, NET_CMD_UNRELIABLE);

	for (i = 0; i < MAX_EDICTS; i++) /* TODO: should not refer to that structure directly? */
	{
		if (snapshot_bitfields->bitfield_edicts[i]) /* only send if one of the fields has changed */
		{
			len = 0;  /* reset buffer, it's already copied by the net subsystem */
			MSG_WriteByte(msg, &len, SVC_ENTITY);
			MSG_WriteShort(msg, &len, snapshot->id);
			MSG_WriteEntity(msg, &len, i); /* edict number */
			MSG_WriteInt(msg, &len, snapshot_bitfields->bitfield_edicts[i]);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_ACTIVE)
				MSG_WriteByte(msg, &len, snapshot->edicts[i].active);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_ORIGIN)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].origin);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_VELOCITY)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].velocity);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_AVELOCITY)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].avelocity);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_ANGLES)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].angles);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_FRAME0)
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].frame[0]);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_FRAME1)
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].frame[1]);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_FRAME2)
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].frame[2]);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_FRAME3)
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].frame[3]);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_FRAME4)
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].frame[4]);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_MODEL)
				MSG_WritePrecache(msg, &len, snapshot->edicts[i].model);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_LIGHT_INTENSITY)
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].light_intensity);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_ANIM_PITCH)
				MSG_WriteByte(msg, &len, snapshot->edicts[i].anim_pitch);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_LOCKED_ANGLES)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_locked_angles);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_MOVECMD)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_movecmd);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_AIMCMD)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_aimcmd);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_SOLID)
				MSG_WriteInt(msg, &len, snapshot->edicts[i].phys_solid);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_TYPE)
				MSG_WriteInt(msg, &len, snapshot->edicts[i].phys_type);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_CREATION_VECS)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_creation_vecs);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_CREATION_MODEL)
				MSG_WritePrecache(msg, &len, snapshot->edicts[i].phys_creation_model);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_CREATION_VEHICLE1)
			{
				/* TODO: this is getting sent when steering the vehicle -- apparently bullet does NOT copy the structure */
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheelDirectionCS0);
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheelAxleCS);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.gEngineForce);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.defaultBreakingForce);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.handBrakeBreakingForce);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.gBreakingForce);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.maxEngineForce);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.gVehicleSteering);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.steeringIncrement);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.steeringClamp);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheelRadius);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheelWidth);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheelFriction);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.suspensionStiffness);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.suspensionDamping);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.suspensionCompression);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.maxSuspensionTravelCm);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.maxSuspensionForce);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.rollInfluence);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.suspensionRestLength);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.connectionHeight);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.connectionStickLateralOutWheelWidthMultiplier);
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier);
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.chassis_box_half_extents);
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.chassis_box_localpos);
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.suppchassis_box_half_extents);
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.suppchassis_box_localpos);
				MSG_WriteInt(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheel_ents[0]);
				MSG_WriteInt(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheel_ents[1]);
				MSG_WriteInt(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheel_ents[2]);
				MSG_WriteInt(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheel_ents[3]);
				MSG_WriteInt(msg, &len, snapshot->edicts[i].phys_creation_vehicle1.wheel_drive);
			}
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_MASS)
				MSG_WriteVec1(msg, &len, snapshot->edicts[i].phys_mass);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_TRACE_ONGROUND)
				MSG_WriteByte(msg, &len, snapshot->edicts[i].phys_trace_onground);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_MOVETYPE)
				MSG_WriteInt(msg, &len, snapshot->edicts[i].phys_movetype);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_ANGLESFLAGS)
				MSG_WriteInt(msg, &len, snapshot->edicts[i].phys_anglesflags);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_MAXSPEED)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_maxspeed);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_ACCELERATION)
				MSG_WriteVec3(msg, &len, snapshot->edicts[i].phys_acceleration);
			if (snapshot_bitfields->bitfield_edicts[i] & MODIFIED_PHYS_IGNORE_GRAVITY)
				MSG_WriteByte(msg, &len, snapshot->edicts[i].phys_ignore_gravity);
			Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, msg, len, NET_CMD_UNRELIABLE);
		}
	}

	len = 0;
	MSG_WriteByte(msg, &len, SVC_END);
	MSG_WriteShort(msg, &len, snapshot->id);
	MSG_WriteInt(msg, &len, svs.sv_clients[slot].snapshots_crc32[svs.sv_clients[slot].last_created_snapshot]);
	Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, msg, len, NET_CMD_UNRELIABLE);
}

/*
===================
SV_SnapshotReceivedAck

Used to set a new reference snapshot depending on what the client confirmed, then cancel
all data and set up the confirmed one to create new differential snapshots.
TODO: do not create if equal to the last one?
===================
*/
void SV_SnapshotReceivedAck(int slot, unsigned short ack_id)
{
	int i;
	int found = 0;
	int index;
	char msg[MAX_NET_CMDSIZE];
	int len = 0;

	if (!ack_id)
		return; /* no data yet */
	if (svs.sv_clients[slot].snapshots[svs.sv_clients[slot].last_acknowledged_snapshot].id == ack_id)
		return; /* already acked */

	for (i = 0; i < MAX_SAVED_SNAPSHOTS; i++)
	{
		if (svs.sv_clients[slot].snapshots[i].id == ack_id)
		{
			found++;
			index = i;
		}
	}

	if (!found || found > 1)
	{
		/*
			something bad happened, the client acknowledged either a packet from
			long ago (relatively) that we don't know how to make diffs from or
			a repeated packet is in the cache (unlikely), so start fresh with
			full data the next time, a kind of self-healing because we can't
			be sure of which snapshot to base the next differential update
		*/
		svs.sv_clients[slot].last_acknowledged_snapshot = 0;
		svs.sv_clients[slot].snapshot_counter = 0;
		if (svs.sv_clients[slot].packet_loss < 100)
			svs.sv_clients[slot].packet_loss++;

		/* start over */
		for (i = 0; i < MAX_SAVED_SNAPSHOTS; i++)
			svs.sv_clients[slot].snapshots[i].id = 0;

		svs.sv_clients[slot].last_created_snapshot = 0;
		memset(&svs.sv_clients[slot].snapshots[0], 0, sizeof(svs.sv_clients[slot].snapshots[0]));
		svs.sv_clients[slot].snapshots_times[0] = host_realtime;

		/* tell the client to start over */
		len = 0;
		MSG_WriteByte(msg, &len, SVC_SNAPSHOTRESET);
		Host_NetchanQueueCommand(svs.sv_clients[slot].netconn, msg, len, NET_CMD_UNRELIABLE);
	}
	else if (found == 1)
	{
		int nextinline = false;
		svs.sv_clients[slot].ping = host_realtime - svs.sv_clients[slot].snapshots_times[index];
		/* if acked the next packet, subtract from packet loss. If acked the same packet, was already ignored. If acked anything else, add to packet loss */
		if (svs.sv_clients[slot].snapshots[svs.sv_clients[slot].last_acknowledged_snapshot].id + 1 == 0) /* overflow? */
		{
			if (svs.sv_clients[slot].snapshots[index].id == 1) /* first one from zero */
				nextinline = true;
		}
		else
		{
			if (svs.sv_clients[slot].snapshots[svs.sv_clients[slot].last_acknowledged_snapshot].id + 1 == svs.sv_clients[slot].snapshots[index].id)
				nextinline = true;
		}
		if (nextinline)
		{
			 if (svs.sv_clients[slot].packet_loss > 0)
				svs.sv_clients[slot].packet_loss--;
		}
		else if (svs.sv_clients[slot].packet_loss < 100)
			svs.sv_clients[slot].packet_loss++;

		svs.sv_clients[slot].last_acknowledged_snapshot = index;

		for (i = 0; i < MAX_SAVED_SNAPSHOTS; i++)
		{
			if (svs.sv_clients[slot].snapshots_times[i] < svs.sv_clients[slot].snapshots_times[index]) /* TODO FIXME: currently no time overflow allowed here, also depends on only one snapshot per player per frame */
				svs.sv_clients[slot].snapshots[i].id = 0; /* set to this id to invalidate older snapshots! */
		}
	}
	else
	{
		Sys_Printf("SV_SnapshotReceivedAck: the impossible happened, disconnecting client %d...\n", slot);
		SV_DropClient(slot, false, true, SVC_ERROR);
	}
}

/*
===================
SV_SnapshotReset

Client requesting reset
===================
*/
void SV_SnapshotReset(int slot)
{
	int i;

	/* reset to the last acked */
	svs.sv_clients[slot].last_created_snapshot = svs.sv_clients[slot].last_acknowledged_snapshot;
	svs.sv_clients[slot].snapshot_counter = svs.sv_clients[slot].snapshots[svs.sv_clients[slot].last_acknowledged_snapshot].id;

	for (i = 0; i < MAX_SAVED_SNAPSHOTS; i++)
		if (i != svs.sv_clients[slot].last_acknowledged_snapshot)
			svs.sv_clients[slot].snapshots[i].id = 0;
}
