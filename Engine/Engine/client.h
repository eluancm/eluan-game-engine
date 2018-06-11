/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef CLIENT_H
#define CLIENT_H

/* client to server command protocol, sent as a byte */
#define CLC_DISCONNECT			0 /* it's better to clear the queue and send this as a unreliable TODO: define what happens if we send this during signon */
#define CLC_SNAPSHOTACK			1 /* last snapshot we received */
#define CLC_SNAPSHOTRESET		2 /* request for reset */
#define CLC_PAUSE				3 /* pause request */
#define CLC_NAME				4 /* change client name after initial signon */

/* cl_snapshot.c */
void CL_SnapshotParseData(unsigned cmd, char *msg, int *read, int len);

/* cl_predict.c */
void CL_PredFrame(void);
void CL_PredParseFromServer(snapshot_data_t *data, snapshot_data_t *old_data);
EXTERNC void CL_PredUpdatePhysStats(entindex_t ent, vec3_t origin, vec3_t angles, vec3_t velocity, vec3_t avelocity, int onground);
EXTERNC void CL_PredUpdatePhysDirections(entindex_t ent, vec3_t forward, vec3_t right, vec3_t up);
EXTERNC void CL_PredUpdateTraceResultStart(entindex_t ent);
EXTERNC int CL_PredUpdateTraceResultStep(entindex_t ent, entindex_t hit, vec3_t pos, vec3_t normal, vec_t fraction);
EXTERNC void CL_PredPostPhysics(void);
EXTERNC const int CL_PredGetMoveType(entindex_t ent);
EXTERNC const unsigned int CL_PredGetAnglesFlags(entindex_t ent);
EXTERNC void CL_PredGetMoveCmd(entindex_t ent, vec_t *dest);
EXTERNC void CL_PredGetAimCmd(entindex_t ent, vec_t *dest);
EXTERNC void CL_PredGetMaxSpeed(entindex_t ent, vec_t *dest);
EXTERNC void CL_PredGetAcceleration(entindex_t ent, vec_t *dest);
EXTERNC int CL_PredGetIgnoreGravity(entindex_t ent);
EXTERNC int CL_PredCheckPhysicalCollisionResponse(entindex_t e1, entindex_t e2);
EXTERNC void CL_PredTouchEnts(entindex_t who, entindex_t by, vec3_t pos, vec3_t normal, vec_t distance, int reaction, vec_t impulse);
EXTERNC void CL_PredEntityGetData(const entindex_t ent, vec_t *origin, vec_t *velocity, vec_t *avelocity, vec_t *angles, vec_t *frames, precacheindex_t *modelindex, vec_t *lightintensity, int *anim_pitch);
/* TODO: put these three somewhere else */
EXTERNC void CL_GetModelPhysicsTrimesh(const precacheindex_t model, model_trimesh_t **trimesh);
EXTERNC void CL_GetModelPhysicsBrushes(const precacheindex_t model, model_brushes_t **brushes);
EXTERNC void CL_GetModelPhysicsHeightfield(const precacheindex_t model, model_heightfield_t **heightfield);

/* cl_sound.c */

typedef struct sfx_s {
	int				active;
	char			name[MAX_SOUND_NAME];
	unsigned int	data; /* just an index, the mening is given by the system code */
} sfx_t;

extern sfx_t		server_sounds[MAX_PRECACHES];

void CL_CleanSounds(int clean_local);
void CL_StartSound(sfx_t *snd, entindex_t ent, vec3_t origin, vec3_t vel, int channel, vec_t pitch, vec_t gain, vec_t attenuation, int loop);
void CL_StopSound(entindex_t ent, int channel);
void CL_StartLocalSound(sfx_t *snd);
void CL_StopLocalSound(void);
sfx_t *CL_LoadSound(const char *name, int local);
void CL_SoundsInit(void);
void CL_SoundsShutdown(void);

/* cl_particles.c */

typedef struct particle_s {
	/* ALL properties must be set when creating a new particle - they won't be cleaned */
	vec3_t			org;
	vec3_t			vel;
	unsigned char	color[4]; /* rgba */
	vec_t			scale;
	mstime_t		timelimit;
	/* int affected_by_gravity; TODO - other physical effects too (requires clint-side physics), color/alpha ramping/decay */

	struct particle_s *next;
} particle_t;

extern particle_t *used_particles;

void CL_CleanParticles(void);
void CL_UpdateParticles(void);
particle_t *CL_ParticleAlloc(void);
void CL_StartParticle(char *msg, int *read, int len);
void CL_ParticlesInit(void);
void CL_ParticlesShutdown(void);

/* cl_video.c */

/* texture related constants and structures */

#define MAX_TEXTURE_NAME		64
#define MAX_TEXTURES			1024

typedef struct texture_s {
	int				active;
	unsigned int	id;
	int				cl_id;
	char			name[MAX_TEXTURE_NAME];
	int				keep; /* don't let it be unloaded from video memory between level loads, etc. Used for menu graphics, for example */
	int				width, height;
} texture_t;

void CL_CleanTextures(int forcenokeep);
EXTERNC texture_t *CL_LoadTexture(const char *name, int keep, unsigned char *indata, int inwidth, int inheight, int data_has_mipmaps, int mipmapuntilwidth, int mipmapuntilheight);

/* rendering stuff */

void CL_VideoDataClean (void);
void CL_VideoSet2D(void);
void CL_VideoInit(void);
void CL_VideoShutdown(void);
void CL_VideoFrame(void);

/* cl_input.c */

/* key destination - this will be used also to keep track of which of these layers is active */
EXTERNC enum {KEYDEST_INVALID = -1, KEYDEST_GAME, KEYDEST_MENU, KEYDEST_TEXT} keydest;

/* all input commands should come through these two functions */
void CL_InputProcessText(char *input_utf8, int key_index);
void CL_InputProcessKeyUpDown(int keyindex, int down, vec_t analog_rel, vec_t analog_abs);

char *CL_InputBindFromKey(int keyindex);
void CL_InputPostFrame(void);
void CL_InputSetTextMode(int textmode, char *existing_text, size_t text_maxsize, void (*text_callback)(char *text, int text_modified, int confirm, int cancel, char *input_utf8, int key_index));
void CL_InputInit(void);
void CL_InputShutdown(void);

/* cl_menu.c */
void CL_MenuInit(void);
void CL_MenuShutdown(void);
void CL_MenuDraw(void);
void CL_MenuKey(int keyindex, int down, vec_t analog_rel, vec_t analog_abs);
void CL_MenuFrame(void);
void CL_MenuConsolePrint(char *text);

/* cl_main.c */

typedef struct cl_state_s
{
	/* use CL_CleanState to clean any field you add below */

	int				active;		/* subsystem initialized */
	int				connected;	/* communicating */
	int				signon;		/* handshake progress */
	int				ingame;		/* playing */
	mstime_t		time;		/* incremented with host_frametime each frame while the client is connected */
	packetqueue_t	*netconn;
	mstime_t		netsendtime;/* last send time for the network - for waiting between connection retries */
	int				conn_tries; /* how many times we already tried to connect */
	client_id_t		client_id;	/* our identifier, sent by the server upon accepting our connection request, carried across consecutive games */
	client_id_t		game_id;	/* identifier of the current game, sent by the server upon accepting our connection request */
	/* both identifiers on top should prevent late data from another client and late data from a previous game from interfering with the current game */
	char			remote_host[MAX_HOST_NAME];
	framenum_t		stall_frames; /* how many frames have passed since current_snapshot was updated succestully */

	/* mirrored from server, we only read from it */
	snapshot_data_t			snapshots[MAX_SAVED_SNAPSHOTS];
	int						current_snapshot_idx; /* (current_snapshot_idx + 1) & MAX_SAVED_SNAPSHOTS_MASK is used for receiving the next snapshot and should not be considered valid */
	int						current_snapshot_valid;
	unsigned short			current_acked_input;
	snapshot_data_t			prediction_snapshot;
	snapshot_bitfields_t	receiving_snapshot_bitfields;
	uint32_t				receiving_snapshot_crc32;
	unsigned short			receiving_acked_input;
	precacheindex_t			precached_models_num;
	model_t					*precached_models[MAX_PRECACHES];
	precacheindex_t			precached_sounds_num;
	sfx_t					*precached_sounds[MAX_PRECACHES];

	void					*physworld; /* for prediction */
} cl_state_t;

EXTERNC cl_state_t	cls; /* TODO: turn EXTERNC back into extern when fixing iqm */
extern cvar_t *cl_fullpredict;

void CL_CheckLoadingSlowness(void);
void CL_Init(void);
void CL_Shutdown(void);
void CL_Disconnect(int reconnect, int warn);
void CL_Frame(void);

/* cl_voxels.c */

void CL_ParseVoxelBlock(char *msg, int *read, int len);
void CL_ParseVoxelChunkPart(char *msg, int *read, int len);

#endif /* CLIENT_H */
