/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2016 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Client game view snapshot management

Snapshots run on top of the netchannel. Adding a new field requires changes
in: receive if from current snapshot or ignore if not, crc check.

============================================================================
*/

#define RECEIVING_SNAPSHOT_IDX ((cls.current_snapshot_idx + 1) & MAX_SAVED_SNAPSHOTS_MASK)

/*
===================
CL_SnapshotParseData

Parses a snapshot fragment from the server
===================
*/
void CL_SnapshotParseData(unsigned cmd, char *msg, int *read, int len)
{
	unsigned char byte_bitfield;
	unsigned int int_bitfield;
	uint32_t received_crc32;
	entindex_t i;
	unsigned char tmp_ubyte;
	unsigned short tmp_ushort, tmp_ushort2, tmp_ushort3;
	int tmp_int;
	vec_t tmp_vec;
	vec3_t tmp_vec3;
	precacheindex_t tmp_precache;
	mstime_t tmp_time;
	int should_accept;
	unsigned short snap_iterator_idx, snap_iterator_idx2;

	switch (cmd)
	{
		case SVC_BEGIN:
			if (cls.snapshots[RECEIVING_SNAPSHOT_IDX].id != cls.snapshots[cls.current_snapshot_idx].id) /* was receiving another */
			{
				/* reset */
				if (cls.snapshots[cls.current_snapshot_idx].id != 0)
					memcpy(&cls.snapshots[RECEIVING_SNAPSHOT_IDX], &cls.snapshots[cls.current_snapshot_idx], sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX]));
				else
					memset(&cls.snapshots[RECEIVING_SNAPSHOT_IDX], 0, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX]));
			}
			MSG_ReadShort(msg, read, len, &tmp_ushort);
			MSG_ReadShort(msg, read, len, &tmp_ushort2);
			MSG_ReadShort(msg, read, len, &tmp_ushort3);
			should_accept = false;
			for (snap_iterator_idx = 0; snap_iterator_idx < MAX_SAVED_SNAPSHOTS; snap_iterator_idx++) /* accept if based on one of the last ones TODO: this is done iteratively, think a little and do this directly. Fail cases: a frame takes TOO MUCH to arrive and the index overflows enouugh to be recognized as one of the last ones AND the CRC32 verifies. Almost impossible. Also, when reseting at the end just before the id overflow, a CRC32 may pass in the worst case, because the new 0 id is not far from the old before the overflow. */
			{
				/* to force the overflow to happen - summing directly in the comparison won't work! they will be treated like regular ints TODO: other places where we need the overflow */
				unsigned short snapshot_ushort1, snapshot_ushort2;
				snapshot_ushort1 = cls.snapshots[cls.current_snapshot_idx].id - snap_iterator_idx;
				if (tmp_ushort == snapshot_ushort1)
				{
					for (snap_iterator_idx2 = 0; snap_iterator_idx2 < MAX_SAVED_SNAPSHOTS; snap_iterator_idx2++)
					{
						snapshot_ushort2 = cls.snapshots[cls.current_snapshot_idx].id + snap_iterator_idx2;
						if (tmp_ushort2 == snapshot_ushort2)
						{
							should_accept = true;
							cls.snapshots[RECEIVING_SNAPSHOT_IDX].base_id = tmp_ushort;
							cls.snapshots[RECEIVING_SNAPSHOT_IDX].id = tmp_ushort2;
							cls.receiving_acked_input = tmp_ushort3;
							break;
						}
					}
				}
			}
			if (!should_accept)
			{
				char replymsg[MAX_NET_CMDSIZE];
				int replylen = 0;

				/* tell the server to start over */
				replylen = 0;
				MSG_WriteByte(replymsg, &replylen, CLC_SNAPSHOTRESET);
				Host_NetchanQueueCommand(cls.netconn, replymsg, replylen, NET_CMD_UNRELIABLE);
				Sys_Printf("Received out of order snapshot. Debug: cur %hu recv %hu recv_base %hu\n", cls.snapshots[cls.current_snapshot_idx].id, tmp_ushort2, tmp_ushort);
			}

			if (should_accept)
			{
				memset(&cls.receiving_snapshot_bitfields, 0, sizeof(cls.receiving_snapshot_bitfields));

				MSG_ReadByte(msg, read, len, &cls.receiving_snapshot_bitfields.bitfield_client);
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_VIEWENT)
					MSG_ReadEntity(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].viewent);
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_CAMERAENT)
					MSG_ReadEntity(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].cameraent);
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_MY_ENT)
					MSG_ReadEntity(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].my_ent);
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_PAUSED)
					MSG_ReadByte(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].paused);
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_TIME)
					MSG_ReadTime(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].time);
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_CMDENT)
					MSG_ReadEntity(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].cmdent);
			}
			else
			{
				MSG_ReadByte(msg, read, len, &byte_bitfield);
				if (byte_bitfield & MODIFIED_VIEWENT)
					MSG_ReadEntity(msg, read, len, &i);
				if (byte_bitfield & MODIFIED_CAMERAENT)
					MSG_ReadEntity(msg, read, len, &i);
				if (byte_bitfield & MODIFIED_MY_ENT)
					MSG_ReadEntity(msg, read, len, &i);
				if (byte_bitfield & MODIFIED_PAUSED)
					MSG_ReadByte(msg, read, len, &tmp_ubyte);
				if (byte_bitfield & MODIFIED_TIME)
					MSG_ReadTime(msg, read, len, &tmp_time);
				if (byte_bitfield & MODIFIED_CMDENT)
					MSG_ReadEntity(msg, read, len, &i);
			}
			break;
		case SVC_ENTITY:
			MSG_ReadShort(msg, read, len, &tmp_ushort);
			if (tmp_ushort == cls.snapshots[RECEIVING_SNAPSHOT_IDX].id) /* ignore out of order data from other snapshots */
			{
				MSG_ReadEntity(msg, read, len, &i);

				/* TODO: cleaning a edict after freeing is useful, but affects snapshots */
				MSG_ReadInt(msg, read, len, &cls.receiving_snapshot_bitfields.bitfield_edicts[i]);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_ACTIVE)
					MSG_ReadByte(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].active);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_ORIGIN)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].origin);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_VELOCITY)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].velocity);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_AVELOCITY)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].avelocity);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_ANGLES)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].angles);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME0)
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[0]);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME1)
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[1]);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME2)
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[2]);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME3)
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[3]);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME4)
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[4]);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_MODEL)
					MSG_ReadPrecache(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].model);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_LIGHT_INTENSITY)
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].light_intensity);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_ANIM_PITCH)
					MSG_ReadByte(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].anim_pitch);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_LOCKED_ANGLES)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_locked_angles);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_MOVECMD)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_movecmd);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_AIMCMD)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_aimcmd);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_SOLID)
					MSG_ReadInt(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_solid);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_TYPE)
					MSG_ReadInt(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_type);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_CREATION_VECS)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vecs);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_CREATION_MODEL)
					MSG_ReadPrecache(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_model);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_CREATION_VEHICLE1)
				{
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheelDirectionCS0);
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheelAxleCS);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.gEngineForce);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.defaultBreakingForce);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.handBrakeBreakingForce);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.gBreakingForce);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.maxEngineForce);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.gVehicleSteering);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.steeringIncrement);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.steeringClamp);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheelRadius);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheelWidth);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheelFriction);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.suspensionStiffness);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.suspensionDamping);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.suspensionCompression);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.maxSuspensionTravelCm);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.maxSuspensionForce);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.rollInfluence);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.suspensionRestLength);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.connectionHeight);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.connectionStickLateralOutWheelWidthMultiplier);
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier);
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.chassis_box_half_extents);
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.chassis_box_localpos);
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.suppchassis_box_half_extents);
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.suppchassis_box_localpos);
					MSG_ReadInt(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheel_ents[0]);
					MSG_ReadInt(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheel_ents[1]);
					MSG_ReadInt(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheel_ents[2]);
					MSG_ReadInt(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheel_ents[3]);
					MSG_ReadInt(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1.wheel_drive);
				}
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_MASS)
					MSG_ReadVec1(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_mass);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_TRACE_ONGROUND)
					MSG_ReadByte(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_trace_onground);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_MOVETYPE)
					MSG_ReadInt(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_movetype);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_ANGLESFLAGS)
					MSG_ReadInt(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_anglesflags);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_MAXSPEED)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_maxspeed);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_ACCELERATION)
					MSG_ReadVec3(msg, read, len, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_acceleration);
				if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_IGNORE_GRAVITY)
					MSG_ReadByte(msg, read, len, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_ignore_gravity);
			}
			else
			{
				MSG_ReadEntity(msg, read, len, &i);
				MSG_ReadInt(msg, read, len, &int_bitfield);
				if (int_bitfield & MODIFIED_ACTIVE)
					MSG_ReadByte(msg, read, len, &tmp_ubyte);
				if (int_bitfield & MODIFIED_ORIGIN)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_VELOCITY)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_AVELOCITY)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_ANGLES)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_FRAME0)
					MSG_ReadVec1(msg, read, len, &tmp_vec);
				if (int_bitfield & MODIFIED_FRAME1)
					MSG_ReadVec1(msg, read, len, &tmp_vec);
				if (int_bitfield & MODIFIED_FRAME2)
					MSG_ReadVec1(msg, read, len, &tmp_vec);
				if (int_bitfield & MODIFIED_FRAME3)
					MSG_ReadVec1(msg, read, len, &tmp_vec);
				if (int_bitfield & MODIFIED_FRAME4)
					MSG_ReadVec1(msg, read, len, &tmp_vec);
				if (int_bitfield & MODIFIED_MODEL)
					MSG_ReadPrecache(msg, read, len, &tmp_precache);
				if (int_bitfield & MODIFIED_LIGHT_INTENSITY)
					MSG_ReadVec1(msg, read, len, &tmp_vec);
				if (int_bitfield & MODIFIED_ANIM_PITCH)
					MSG_ReadByte(msg, read, len, &tmp_ubyte);
				if (int_bitfield & MODIFIED_PHYS_LOCKED_ANGLES)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_PHYS_MOVECMD)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_PHYS_AIMCMD)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_PHYS_SOLID)
					MSG_ReadInt(msg, read, len, &tmp_int);
				if (int_bitfield & MODIFIED_PHYS_TYPE)
					MSG_ReadInt(msg, read, len, &tmp_int);
				if (int_bitfield & MODIFIED_PHYS_CREATION_VECS)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_PHYS_CREATION_MODEL)
					MSG_ReadPrecache(msg, read, len, &tmp_precache);
				if (int_bitfield & MODIFIED_PHYS_CREATION_VEHICLE1)
				{
					MSG_ReadVec3(msg, read, len, tmp_vec3);
					MSG_ReadVec3(msg, read, len, tmp_vec3);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec1(msg, read, len, &tmp_vec);
					MSG_ReadVec3(msg, read, len, tmp_vec3);
					MSG_ReadVec3(msg, read, len, tmp_vec3);
					MSG_ReadVec3(msg, read, len, tmp_vec3);
					MSG_ReadVec3(msg, read, len, tmp_vec3);
					MSG_ReadInt(msg, read, len, &tmp_int);
					MSG_ReadInt(msg, read, len, &tmp_int);
					MSG_ReadInt(msg, read, len, &tmp_int);
					MSG_ReadInt(msg, read, len, &tmp_int);
					MSG_ReadInt(msg, read, len, &tmp_int);
				}
				if (int_bitfield & MODIFIED_PHYS_MASS)
					MSG_ReadVec1(msg, read, len, &tmp_vec);
				if (int_bitfield & MODIFIED_PHYS_TRACE_ONGROUND)
					MSG_ReadByte(msg, read, len, &tmp_ubyte);
				if (int_bitfield & MODIFIED_PHYS_MOVETYPE)
					MSG_ReadInt(msg, read, len, &tmp_int);
				if (int_bitfield & MODIFIED_PHYS_ANGLESFLAGS)
					MSG_ReadInt(msg, read, len, &tmp_int);
				if (int_bitfield & MODIFIED_PHYS_MAXSPEED)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_PHYS_ACCELERATION)
					MSG_ReadVec3(msg, read, len, tmp_vec3);
				if (int_bitfield & MODIFIED_PHYS_IGNORE_GRAVITY)
					MSG_ReadByte(msg, read, len, &tmp_ubyte);
			}
			break;
		case SVC_END:
			MSG_ReadShort(msg, read, len, &tmp_ushort);
			if (tmp_ushort == cls.snapshots[RECEIVING_SNAPSHOT_IDX].id) /* ignore out of order data from other snapshots */
			{
				MSG_ReadInt(msg, read, len, &received_crc32);

				/* calculating manually because of possible padding */
				cls.receiving_snapshot_crc32 = Host_CRC32(0xffffffff, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].id, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].id));
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_VIEWENT)
					cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].viewent, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].viewent));
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_CAMERAENT)
					cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].cameraent, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].cameraent));
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_MY_ENT)
					cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].my_ent, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].my_ent));
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_PAUSED)
					cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].paused, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].paused));
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_TIME)
					cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].time, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].time));
				if (cls.receiving_snapshot_bitfields.bitfield_client & MODIFIED_CMDENT)
					cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].cmdent, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].cmdent));

				for (i = 0; i < MAX_EDICTS; i++)
				{
					/* in the server code, these get put into the crc32 first */
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_LOCKED_ANGLES)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_locked_angles, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_locked_angles));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_MOVECMD)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_movecmd, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_movecmd));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_AIMCMD)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_aimcmd, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_aimcmd));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_SOLID)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_solid, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_solid));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_TYPE)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_type, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_type));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_CREATION_VECS)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vecs, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vecs));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_CREATION_MODEL)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_model, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_model));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_CREATION_VEHICLE1)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_creation_vehicle1));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_MASS)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_mass, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_mass));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_TRACE_ONGROUND)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_trace_onground, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_trace_onground));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_MOVETYPE)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_movetype, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_movetype));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_ANGLESFLAGS)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_anglesflags, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_anglesflags));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_MAXSPEED)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_maxspeed, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_maxspeed));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_ACCELERATION)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_acceleration, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_acceleration));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_PHYS_IGNORE_GRAVITY)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_ignore_gravity, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].phys_ignore_gravity));

					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_ACTIVE)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].active, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].active));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_ORIGIN)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].origin, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].origin));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_VELOCITY)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].velocity, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].velocity));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_AVELOCITY)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].avelocity, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].avelocity));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_ANGLES)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].angles, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].angles));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME0)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[0], sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[0]));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME1)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[1], sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[1]));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME2)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[2], sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[2]));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME3)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[3], sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[3]));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_FRAME4)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[4], sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].frame[4]));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_MODEL)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].model, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].model));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_LIGHT_INTENSITY)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].light_intensity, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].light_intensity));
					if (cls.receiving_snapshot_bitfields.bitfield_edicts[i] & MODIFIED_ANIM_PITCH)
						cls.receiving_snapshot_crc32 = Host_CRC32(cls.receiving_snapshot_crc32, &cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].anim_pitch, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX].edicts[i].anim_pitch));
				}

				/* checksumming will prevent aplying wrong updates - we don't check the differential update itself, we check the end result in the game state */
				if (received_crc32 == cls.receiving_snapshot_crc32)
				{
					if (cls.snapshots[cls.current_snapshot_idx].paused != cls.snapshots[RECEIVING_SNAPSHOT_IDX].paused)
						Sys_Printf("%s\n", cls.snapshots[RECEIVING_SNAPSHOT_IDX].paused ? "Server paused" : "Server unpaused");

					cls.current_acked_input = cls.receiving_acked_input;

					CL_PredParseFromServer(&cls.snapshots[RECEIVING_SNAPSHOT_IDX], &cls.snapshots[cls.current_snapshot_idx]);

					cls.current_snapshot_idx = RECEIVING_SNAPSHOT_IDX;
					cls.stall_frames = 0;
					cls.current_snapshot_valid = true;
				}
				else
				{
					Sys_Printf("Snapshot CRC32 error.\n");

					/* reset */
					if (cls.snapshots[cls.current_snapshot_idx].id != 0)
						memcpy(&cls.snapshots[RECEIVING_SNAPSHOT_IDX], &cls.snapshots[cls.current_snapshot_idx], sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX]));
					else
						memset(&cls.snapshots[RECEIVING_SNAPSHOT_IDX], 0, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX]));
				}
			}
			else
			{
				MSG_ReadInt(msg, read, len, &tmp_int);
			}
			break;
		case SVC_SNAPSHOTRESET:
			Sys_Printf("Network sync error.\n");
			cls.snapshots[cls.current_snapshot_idx].id = 0; /* keep the data for appearances sake */
			memset(&cls.snapshots[RECEIVING_SNAPSHOT_IDX], 0, sizeof(cls.snapshots[RECEIVING_SNAPSHOT_IDX])); /* TODO: this is slow - just have a pointer to a empty state? the whole snapshot stuff is slow! */
			break;
		default:
			Host_Error("CL_SnapshotParseData: unknown command (%02X) received\n");
	}
}
