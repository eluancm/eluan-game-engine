/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef GAME_SV_H
#define GAME_SV_H

#define MAX_GAME_TRACEHITS		16	/*
										maximum number of ents to hit during a traceline (TODO: verify that we only get the closest ones)
									*/

/*
	Game_SV_SetSolidState VALUES
	Physics stuff, will determine how entities react to each other in physics code.
	Choose smartly, the right values can help reduce CPU usage.
	In addition to the collision table below, triggers won't ever react to other triggers in any way (collision events or physical reactions)
*/
/* part of the game world, collision events and physical events between themselves should be filtered early */
#define SOLID_WORLD_NOT				0 /* Collision events: NO ONE                Physical reactions: NO ONE                 */
#define SOLID_WORLD_TRIGGER			1 /* Collision events: EVERYONE EXCEPT WORLD Physical reactions: NO ONE                 */
#define SOLID_WORLD					2 /* Collision events: EVERYONE EXCEPT WORLD Physical reactions: EVERYONE EXCEPT WORLD. */
/* other entities */
#define SOLID_ENTITY_WITHWORLDONLY	3 /* Collision events: WORLD ONLY            Physical reactions: WORLD ONLY             */
#define SOLID_ENTITY_TRIGGER		4 /* Collision events: EVERYONE              Physical reactions: WORLD ONLY             */
#define SOLID_ENTITY				5 /* Collision events: EVERYONE              Physical reactions: EVERYONE               */

/*
	Game_SV_PointContents FLAGS
	model stuff, bitfield determining contents of the volume a point is in
*/
#define CONTENTS_SOLID_BIT		1
#define CONTENTS_WATER_BIT		(1 << 5)

/* game_edict_t.movetype VALUES */
#define MOVETYPE_FREE			0 /* move according to physics */
#define MOVETYPE_WALK			1 /* move like a player. Needs game_edict_t.movecmd, game_edict_t.maxspeed and game_edict_t.acceleration */
#define MOVETYPE_FLY			2 /* just like MOVETYPE_WALK, but allows free vertical movement (without needing to jump) */
#define MOVETYPE_FOLLOW			3 /* will adjust the origin to the self->owner origin at the start of every frame */
#define MOVETYPE_FOLLOWANGLES	4 /* will adjust the origin and angles to the self->owner origin and angles at the start of every frame */

/* game_edict_t.anglesflags FLAGS */
/* if the angles are locked, the locked value must be sent to Sys_PhysicsSetTransform (Game_SV_SetTransform) and Sys_PhysicsCreateObject */
#define ANGLES_KINEMATICANGLES_BIT				1 /* angles are modified by aimcmd (to make a dynamic object have kinematic angles) */
#define ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT	2 /* keep the pitch value that was used to create the physical object (but return the angle modified by aimcmd) */
#define ANGLES_KINEMATICANGLES_LOCK_YAW_BIT		4 /* keep the yaw value that was used to create the physical object (but return the angle modified by aimcmd) */
#define ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT	8 /* keep the roll value that was used to create the physical object (but return the angle modified by aimcmd) */

/* game specific defines start */

enum { /* model_sound() sound_type VALUES TODO: document which models need which sound types */
	SOUND_FIRE = 0,
    SOUND_FIREEMPTY = 1,
	SOUND_PAIN,
	SOUND_DIE,
    NUM_MODEL_SOUNDS					/* should be >= 1? TODO */
};

/* game specific defines end */

/* game_sv_materialresources.c */
precacheindex_t model_sound(int modelindex, int sound_type);
void model_sound_precache(const char *modelname);

#include "game_sv_lua.h"

/* Below here only functions called by engine code */

/* game_sv_edict.c */
void Game_SV_VisibilityPrepare(const vec3_t eyeorigin);
const int Game_SV_EntityIsVisible(const entindex_t viewent, const int slot, const vec3_t eyeorigin, const entindex_t ent, const precacheindex_t ent_modelindex, const vec_t ent_lightintensity);
EXTERNC void Game_SV_EntityGetData(const entindex_t ent, vec_t *origin, vec_t *velocity, vec_t *avelocity, vec_t *angles, vec_t *frames, precacheindex_t *modelindex, vec_t *lightintensity, int *anim_pitch);

/* game_sv_client.c */
const char *Game_SV_ClientGetNetname(const int slot);
const entindex_t Game_SV_ClientGetViewEnt(const int slot);
const entindex_t Game_SV_ClientGetCameraEnt(const int slot);
const entindex_t Game_SV_ClientGetCmdEnt(const int slot);
const mstime_t Game_SV_GetTime(void);
const unsigned short Game_SV_ClientGetInputSequence(const int slot);
void Game_SV_ClientPreThink(int slot);
void Game_SV_ClientPostThink(int slot);
void Game_SV_ClientSetName(const int slot, const char *intended_netname, int broadcast);
void Game_SV_ClientConnect(int slot, char *netname, int loading_saved_game);
void Game_SV_ClientDisconnect(int slot);
int Game_SV_ParseClientMessages(int slot, unsigned cmd, char *msg, int *read, int len);
void Game_SV_SendClientMessages(int slot);

/* game_sv_main.c */
void Game_SV_StartFrame(mstime_t frametime);
void Game_SV_EndFrame(void);
void Game_SV_RunThinks(void);
EXTERNC void Game_SV_TouchEnts(entindex_t who, entindex_t by, vec3_t pos, vec3_t normal, vec_t distance, int reaction, vec_t impulse);
EXTERNC int Game_SV_CheckPhysicalCollisionResponse(entindex_t e1, entindex_t e2);
void Game_SV_NewGame(int loading_saved_game);
void Game_SV_EndGame(void);
void Game_SV_SaveExtraGameData(const char *name);
void Game_SV_LoadExtraGameData(const char *name);
void Game_SV_SaveSpawnParms(void);
void Game_SV_LoadSpawnParms(void);
void Game_SV_SetNewSpawnParms(void);
void Game_SV_ClientSetNewSpawnParms(int slot);
void Game_SV_InitGame(void);
void Game_SV_ShutdownGame(void);

/* game_sv_resources.c */
EXTERNC void Game_SV_UpdatePhysStats(entindex_t ent, vec3_t origin, vec3_t angles, vec3_t velocity, vec3_t avelocity, int onground);
EXTERNC void Game_SV_UpdatePhysDirections(entindex_t ent, vec3_t forward, vec3_t right, vec3_t up);
EXTERNC void Game_SV_UpdateTraceResultStart(entindex_t ent);
EXTERNC int Game_SV_UpdateTraceResultStep(entindex_t ent, entindex_t hit, vec3_t pos, vec3_t normal, vec_t fraction);
EXTERNC void Game_SV_PostPhysics(void);
EXTERNC const int Game_SV_GetMoveType(entindex_t ent);
EXTERNC const unsigned int Game_SV_GetAnglesFlags(entindex_t ent);
EXTERNC void Game_SV_GetMoveCmd(entindex_t ent, vec_t *dest);
EXTERNC void Game_SV_GetAimCmd(entindex_t ent, vec_t *dest);
EXTERNC void Game_SV_GetMaxSpeed(entindex_t ent, vec_t *dest);
EXTERNC void Game_SV_GetAcceleration(entindex_t ent, vec_t *dest);
EXTERNC int Game_SV_GetIgnoreGravity(entindex_t ent);

#endif /* GAME_SV_H */
