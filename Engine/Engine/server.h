/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef SERVER_H
#define SERVER_H

/* server to client command protocol, sent as a byte */
#define SVC_BEGIN				0 /* sent to begin snapshot update */
#define SVC_ENTITY				1
#define SVC_END					2 /* sent to end snapshot update */
#define SVC_SERVERQUIT			3 /* it's better to clear the queue and send this as a unreliable TODO: define what happens if we send this during signon */
#define SVC_ERROR				4 /* it's better to clear the queue and send this as a unreliable TODO: define what happens if we send this during signon */
#define SVC_RECONNECT			5 /* it's better to clear the queue and send this as a unreliable TODO: define what happens if we send this during signon */
#define SVC_VOXELBLOCK			6
#define SVC_VOXELCHUNKPART		7
#define SVC_PARTICLE			8 /* svc is engine-defined but contents are game-defined */
#define SVC_SOUND				9
#define SVC_STOPSOUND			10
#define SVC_SNAPSHOTRESET		11
#define SVC_NOPAUSE				12

/* sv_snapshot.c */

/* TODO: compress all the booleans into shared bytes, etc */
/* TODO: endianess issues, also with crc32 byte by byte */
/* TODO: when sending a FULL snapshot, do not re-send it too often, wait a little while for confirmation - DO THIS BEFORE SETTING THE CLIENT INGAME? */

/* hold 32 frames of data. Players with ping times > ~533 (60fps server) will experience network sync errors  */
#define MAX_SAVED_SNAPSHOTS			32 /* because of the below mask, must be a power of 2 */
#define MAX_SAVED_SNAPSHOTS_MASK	31 /* for cyclic buffering */

/* TODO: calculate to see if a packet overflow is possible */
typedef struct snapshot_edict_s {
	unsigned char				active; /* 0 = inactive, 1 = active, 2 = active with physical representation */
	vec3_t						origin;
	vec3_t						velocity;
	vec3_t						avelocity;
	vec3_t						angles;
	vec_t						frame[ANIMATION_MAX_BLENDED_FRAMES];
	precacheindex_t				model;
	vec_t						light_intensity; /* a negative intensity will be treated like a positive one but without casting shadows */
	unsigned char				anim_pitch; /* if true, use animation for ANGLES_PITCH TODO: not the best place for this? */

	/* for client side prediction */
	/* TODO: kinematic moves and player movement type changes (like going underwater) won't be automatically predicted because they are done in the server gamecode - need client gamecode to emulate it (just create a shared script) */
	vec3_t						phys_locked_angles;
	vec3_t						phys_movecmd;
	vec3_t						phys_aimcmd;
	unsigned int				phys_solid;
	int							phys_type;
	vec3_t						phys_creation_vecs;
	precacheindex_t				phys_creation_model;
	phys_edict_vehicle_info_t	phys_creation_vehicle1;
	vec_t						phys_mass;
	uint8_t						phys_trace_onground;
	int							phys_movetype;
	unsigned int				phys_anglesflags;
	vec3_t						phys_maxspeed;
	vec3_t						phys_acceleration;
	uint8_t						phys_ignore_gravity;
} snapshot_edict_t;

typedef struct snapshot_data_s {
	unsigned short		id; /* !!! should be the same type as sv_client_t.snapshot_counter  TODO: see if rollover is okay (probably is, only a few are cached) */
	unsigned short		base_id; /* id of the snapshot used as base */
	snapshot_edict_t	edicts[MAX_EDICTS];
	entindex_t			viewent; /* index of the entity with the client camera origin/angles */
	entindex_t			cameraent; /* index of the entity which represents the player's eyes camera */
	entindex_t			my_ent; /* client slot in the entity list */
	unsigned char		paused; /* TODO: Converting to a unsigned char, I don't think any game would have more than 256 pause states? Also, this is a flag, so it belongs in a bitfield to represent the server status, se we would have a MODIFIED_* bit to represent a modified bitfield, heh */
	mstime_t			time; /* for interpolation, etc */
	entindex_t			cmdent; /* entity who is receiving the client input */
} snapshot_data_t;

/* a bitfield is before each struct, indicating which fields we will be sending */
#define MODIFIED_VIEWENT				1
#define MODIFIED_CAMERAENT				2
#define MODIFIED_MY_ENT					4
#define MODIFIED_PAUSED					8
#define MODIFIED_TIME					16
#define MODIFIED_CMDENT					32
/* the following are sent for each entity - if we send the entity at all */
#define MODIFIED_ACTIVE					1
#define MODIFIED_ORIGIN					2
#define MODIFIED_VELOCITY				4
#define MODIFIED_AVELOCITY				8
#define MODIFIED_ANGLES					16
#define MODIFIED_FRAME0					32 /* KEEP IN SYNC WITH ANIMATION_MAX_BLENDED_FRAMES */
#define MODIFIED_FRAME1					64
#define MODIFIED_FRAME2					128
#define MODIFIED_FRAME3					256
#define MODIFIED_FRAME4					512
#define MODIFIED_MODEL					1024
#define MODIFIED_LIGHT_INTENSITY		2048
#define MODIFIED_ANIM_PITCH				4096
#define MODIFIED_PHYS_LOCKED_ANGLES		8192
#define MODIFIED_PHYS_MOVECMD			16384
#define MODIFIED_PHYS_AIMCMD			32768
#define MODIFIED_PHYS_SOLID				65536
#define MODIFIED_PHYS_TYPE				(1 << 17)
#define MODIFIED_PHYS_CREATION_VECS		(1 << 18)
#define MODIFIED_PHYS_CREATION_MODEL	(1 << 19)
#define MODIFIED_PHYS_CREATION_VEHICLE1	(1 << 20)
#define MODIFIED_PHYS_MASS				(1 << 21)
#define MODIFIED_PHYS_TRACE_ONGROUND	(1 << 22)
#define MODIFIED_PHYS_MOVETYPE			(1 << 23)
#define MODIFIED_PHYS_ANGLESFLAGS		(1 << 24)
#define MODIFIED_PHYS_MAXSPEED			(1 << 25)
#define MODIFIED_PHYS_ACCELERATION		(1 << 26)
#define MODIFIED_PHYS_IGNORE_GRAVITY	(1 << 27)


typedef struct snapshot_bitfields_s {
	unsigned char		bitfield_client;
	unsigned int		bitfield_edicts[MAX_EDICTS];
} snapshot_bitfields_t;

void SV_SnapshotCreate(int slot);
void SV_SnapshotSend(int slot);
void SV_SnapshotReceivedAck(int slot, unsigned short ack_id);
void SV_SnapshotReset(int slot);

/* sv_main.c */

#define MAX_CLIENTS			16 /* should obviously be way smaller than MAX_EDICTS */
#define MAX_CLIENT_NAME		32
#define MAX_HOST_NAME		64

/*
	clients get "connected" the moment they request a connection
	they get disconnected with the drop function (hard disconection,
	something should do a soft disconnect before)
*/
typedef struct sv_client_s
{
	/*
		this entire struct should be cleared when we drop a client, but be careful with the netconn pointer
		use SV_CleanClientSlot for clearing any new field you add below, and only it.
	*/
	int						connected;	/* communicating */
	int						signon;		/* handshake progress */
	int						ingame;		/* playing */
	packetqueue_t			*netconn;
	client_id_t				client_id;	/* uniquely identify this client */

	snapshot_data_t			snapshots[MAX_SAVED_SNAPSHOTS];
	snapshot_bitfields_t	snapshots_modified_bitfields[MAX_SAVED_SNAPSHOTS]; /* for each snapshot, which entity fields we modified */
	mstime_t				snapshots_times[MAX_SAVED_SNAPSHOTS];
	uint32_t				snapshots_crc32[MAX_SAVED_SNAPSHOTS];
	int						last_created_snapshot;
	int						last_acknowledged_snapshot;
	unsigned short			snapshot_counter; /* !!! should be the same type as snapshot_data_t.id */

	/* these two are derived from the snapshot subsystem TODO: test their implementation */
	mstime_t				ping;
	int						packet_loss;
} sv_client_t;

typedef struct sv_state_s
{
	int					active;
	int					loading;
	int					loading_saved_game;
	int					listening;
	int					paused;
	char				name[MAX_MODEL_NAME]; /* current map */
	sv_client_t			sv_clients[MAX_CLIENTS];
	client_id_t			game_id;	/* uniquely identify the current game */

	/* TODO: integrate this with the client */
	/* TODO: the names are already in the data... just cache the data */
	precacheindex_t		precached_models_num;
	char				precached_models[MAX_PRECACHES][MAX_MODEL_NAME]; /* to send to the client */
	model_t				*precached_models_data[MAX_PRECACHES]; /* for server-side processing */
	precacheindex_t		precached_sounds_num;
	char				precached_sounds[MAX_PRECACHES][MAX_MODEL_NAME]; /* to send to the client */
	/* sfx_t				*precached_sounds_data[MAX_PRECACHES]; */ /* for server-side processing */ /* TODO: will we want someday to do anything with sounds server-side? */

	void				*physworld;
} sv_state_t;

EXTERNC sv_state_t	svs;

void SV_Init(void);
void SV_Shutdown(void);
void SV_DropClient(int slot, int reconnect, int warn, unsigned char svc_reason);
void SV_ShutdownServer(int changelevel);
void SV_Frame(void);

/* sv_resources.c */

precacheindex_t SV_GetModelIndex(const char *name);
void SV_GetModelEntities(const precacheindex_t model, int *num_entities, model_entity_t **entities);
EXTERNC void SV_GetModelPhysicsTrimesh(const precacheindex_t model, model_trimesh_t **trimesh);
EXTERNC void SV_GetModelPhysicsBrushes(const precacheindex_t model, model_brushes_t **brushes);
EXTERNC void SV_GetModelPhysicsHeightfield(const precacheindex_t model, model_heightfield_t **heightfield);
void SV_GetModelAABB(const precacheindex_t model, const vec_t frame, vec3_t mins, vec3_t maxs);
void SV_GetModelTagTransform(const precacheindex_t model, const unsigned int tag_idx, const int local_coords, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, const int ent);
void SV_Animate(const precacheindex_t model, const int ent, vec3_t origin, vec3_t angles, vec_t *frames, const int anim_pitch);
void SV_AnimationInfo(const precacheindex_t model, const unsigned int animation, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation);
int SV_AnimationExists(const precacheindex_t model, const unsigned int animation);
int SV_PointContents(const precacheindex_t model, const vec3_t point);
int SV_ModelHasPVS(const precacheindex_t model);
void SV_ModelPVSGetClustersBox(const precacheindex_t model, const vec3_t absmins, const vec3_t absmaxs, int *clusters, int *num_clusters, const int max_clusters);
void SV_ModelPVSCreateFatPVSClusters(const precacheindex_t model, const vec3_t eyeorigin);
int SV_ModelPVSTestFatPVSClusters(const precacheindex_t model, const int *clusters, const int num_clusters);
void SV_Precache_Model(const char *name);
precacheindex_t SV_GetSoundIndex(const char *name);
void SV_Precache_Sound(const char *name);
void SV_ClearAllPrecaches(void);

/* sv_voxels.c */

void SV_VoxelQueueToNewClient(int slot);
void SV_VoxelQueueSendPartial(int slot);

/* game interface */
void SV_VoxelSetChunk(int chunkoriginx, int chunkoriginy, int chunkoriginz, unsigned char *type);
void SV_VoxelSetBlock(int absx, int absy, int absz, unsigned char type);
int SV_VoxelPointContents(const vec3_t point);

#endif /* SERVER_H */
