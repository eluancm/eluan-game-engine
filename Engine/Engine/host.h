/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef HOST_H
#define HOST_H

/* structures shared between client and server */

/* should be >= 1 to prevent division by zero */
#define HOST_MAX_FPS	60
#define HOST_MIN_FPS	10 /* if below this, time will slow down */

/* for vec3_t angles structures */
#define ANGLES_PITCH	0 /* + up - down */
#define ANGLES_YAW		1 /* + left - right */
#define ANGLES_ROLL		2 /* + left - right */
/*
	for origin, with a camera with angles {0, 0, 0}:
	origin[0] - left + right
	origin[1] - down + up
	origin[2] - forward + backward

	in other words, camera at {0, 0, 0} with angles {0, 0, 0} looks into -z, with +y up and +x right
*/

#define MAX_EDICTS		512 /* be careful to check the number of bits actually sent through the network before raising this up */
typedef short entindex_t; /* WARNING: please allow negative values FIXME: maybe I'm not using this somewhere... */

#define MAX_PRECACHES	512 /* be careful to check the number of bits actually sent through the network before raising this up */
typedef short precacheindex_t; /* FIXME: maybe I'm not using this somewhere... */
#define MAX_MODEL_NAME		64
#define MAX_SOUND_NAME		64

/* host.c */
#define MAX_ARGS		64

extern int host_initialized;
extern int host_argc;
extern char *host_argkey[MAX_ARGS];
extern char *host_argvalue[MAX_ARGS];

extern int host_quit;

extern framenum_t host_framecount;
EXTERNC framenum_t host_framecount_notzero; /* will never be zero, so it will jump from the maximum value to 1 - useful to avoid memsets to zero in some stuff */
/* TODO: use Sys_Time() in framerate independent sections instead of host_realtime */
extern mstime_t host_realtime; /* doesn't take paused games, breakpoints, etc into consideration so a breakpoint will add a huge time */
extern mstime_t host_frametime;

int Host_GetArg(const char *arg, char **dest);
EXTERNC void Host_Error(const char *error, ...);
void Host_Init(int argc, char *argv[]);
void Host_Frame(void);
void Host_Shutdown(void);

/* host_math.c */

EXTERNC const vec3_t null_vec3; /* TODO: use this where necessary */

#define Math_Vector2Copy(src, dest) ((dest)[0] = (src)[0], (dest)[1] = (src)[1])

/* FIXME: create Math_Vector3Subtract and search in the code where I used Math_Vector3ScaleAdd for this */
#define Math_ClearVector3(v) ((v)[0] = 0, (v)[1] = 0, (v)[2] = 0)
#define Math_Vector3Add(src1, src2, dest) ((dest)[0] = (src1)[0] + (src2)[0], (dest)[1] = (src1)[1] + (src2)[1], (dest)[2] = (src1)[2] + (src2)[2])
#define Math_Vector3Compare(v1, v2) ((v1)[0] == (v2)[0] && (v1)[1] == (v2)[1] && (v1)[2] == (v2)[2]) /* TODO: epsilon */
#define Math_Vector3Copy(src, dest) ((dest)[0] = (src)[0], (dest)[1] = (src)[1], (dest)[2] = (src)[2])
/* FIXME: this (and others like this) will break if-blocks (and while, etc) that have no curly braces */
#define Math_Vector3Normalize(v) {vec_t ilength = (vec_t)Math_Vector3Length((v)); if (ilength) ilength = 1.0f / ilength; (v)[0] *= ilength; (v)[1] *= ilength; (v)[2] *= ilength;}
#define Math_Vector3IsZero(v) (!((v)[0]) && !((v)[1]) && !((v)[2])) /* TODO: check for NaN? */
#define Math_Vector3Length(v) (sqrt(Math_DotProduct3((v),(v))))
#define Math_Vector3LengthSquared(v) (Math_DotProduct3((v),(v)))
#define Math_Vector3Scale(src, dest, scale) ((dest)[0] = (src)[0] * (scale), (dest)[1] = (src)[1] * (scale), (dest)[2] = (src)[2] * (scale))
#define Math_Vector3ScaleAdd(srca, scalea, srcb, dest) ((dest)[0] = (srca)[0] * (scalea) + (srcb)[0], (dest)[1] = (srca)[1] * (scalea) + (srcb)[1], (dest)[2] = (srca)[2] * (scalea) + (srcb)[2])
#define Math_Vector3Set(v, a, b, c) ((v)[0] = (a), (v)[1] = (b), (v)[2] = (c))
#define Math_DotProduct3(a, b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2])
#define Math_CrossProduct3(a, b, dest) ((dest)[0] = (a)[1] * (b)[2] - (a)[2] * (b)[1], (dest)[1] = (a)[2] * (b)[0] - (a)[0] * (b)[2], (dest)[2] = (a)[0] * (b)[1] - (a)[1] * (b)[0]) /* dest must not be one of the operands */

void Math_AABB3Center(vec3_t destcenter, vec3_t maxs, vec3_t mins);
void Math_AABB3EnclosePoints(const vec3_t point1, const vec3_t point2, vec3_t maxs, vec3_t mins);

#define Math_Vector4Compare(v1, v2) ((v1)[0] == (v2)[0] && (v1)[1] == (v2)[1] && (v1)[2] == (v2)[2] && (v1)[3] == (v2)[3]) /* TODO: epsilon */
#define Math_Vector4Copy(src, dest) ((dest)[0] = (src)[0], (dest)[1] = (src)[1], (dest)[2] = (src)[2], (dest)[3] = (src)[3])

EXTERNC void Math_PlaneNormalAndPointToPlaneEquation(vec3_t innormal, vec3_t inpoint, vec4_t outequation);
EXTERNC void Math_PlaneFromThreePoints(vec3_t in_point0, vec3_t in_point1, vec3_t in_point2, vec4_t outequation);
EXTERNC vec_t Math_PlaneDistanceToPoint(const vec4_t equation, const vec3_t point);
EXTERNC int Math_PlaneIntersectionPoint(const vec4_t in_equation1, const vec4_t in_equation2, const vec4_t in_equation3, vec3_t outpoint);
int Math_BoxTestAgainstPlaneSides(const vec3_t boxmins, const vec3_t boxmaxs, const vec3_t planenormal, const vec_t planedist);

#define Math_Deg2Rad(deg) (deg / 180.f * (vec_t)M_PI)
#define Math_Rad2Deg(rad) (rad * 180.f / (vec_t)M_PI)
void Math_VecForwardToAngles(vec3_t forward, vec3_t angles);
EXTERNC int Math_VecToAngles(vec3_t forward, vec3_t right, vec3_t up, vec3_t angles); /* TODO: clean up the EXTERNC's */
EXTERNC void Math_AnglesToVec(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void Math_ReflectVectorAroundNormal(vec3_t invector, vec3_t normal, vec3_t outvector);
int Math_CheckIfClose(vec3_t origin1, vec3_t origin2, vec_t distance);
int Math_PointInsideBox(const vec3_t point, const vec3_t box_mins, const vec3_t box_maxs);
EXTERNC void Math_PerspectiveToFrustum(const double fovy, const double aspect, const double znear, const double zfar, double *xmin, double *xmax, double *ymin, double *ymax);
unsigned int Math_PopCount(unsigned int i);
EXTERNC void Math_Matrix3x3From4x4Top(vec_t *destmatrix, const vec_t *srcmatrix);
EXTERNC int Math_MatrixIsEqual3x3(const vec_t *a, const vec_t *b);
EXTERNC void Math_MatrixCopy3x3(vec_t *destmatrix, const vec_t *srcmatrix);
EXTERNC int Math_MatrixIsEqual4x4(const vec_t *a, const vec_t *b);
EXTERNC void Math_MatrixCopy4x4(vec_t *destmatrix, const vec_t *srcmatrix);
void Math_Matrix4x4ApplyToVector4(vec_t *destvector, vec_t *a, vec_t *b);
EXTERNC void Math_MatrixMultiply4x4(vec_t *destmatrix, vec_t *a, vec_t *b);
EXTERNC void Math_MatrixInverse4x4(vec_t *destmatrix, vec_t *m);
EXTERNC void Math_MatrixTranspose4x4(vec_t *destmatrix, vec_t *m);
EXTERNC void Math_MatrixPerspectiveFrustum4x4(vec_t *destmatrix, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far);
EXTERNC void Math_MatrixOrtho4x4(vec_t *destmatrix, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far);
EXTERNC void Math_MatrixLookAt4x4(vec_t *destmatrix, vec_t eyex, vec_t eyey, vec_t eyez, vec_t centerx, vec_t centery, vec_t centerz, vec_t upx, vec_t upy, vec_t upz);
EXTERNC void Math_MatrixIdentity4x4(vec_t *destmatrix);
EXTERNC void Math_MatrixTranslate4x4(vec_t *destmatrix, vec_t x, vec_t y, vec_t z);
EXTERNC void Math_MatrixScale4x4(vec_t *destmatrix, vec_t x, vec_t y, vec_t z);
EXTERNC void Math_MatrixRotateX4x4(vec_t *destmatrix, vec_t angle);
EXTERNC void Math_MatrixRotateY4x4(vec_t *destmatrix, vec_t angle);
EXTERNC void Math_MatrixRotateZ4x4(vec_t *destmatrix, vec_t angle);
void Math_MatrixRotateFromVectors4x4(vec_t *destmatrix, vec3_t forward, vec3_t right, vec3_t up);
EXTERNC void Math_MatrixModel4x4(vec_t *destmatrix, vec3_t origin, vec3_t angles, vec3_t scale); /* TODO: clean up the EXTERNC's */

#define Math_Max(a, b) ((a) > (b) ? (a) : (b))
#define Math_Min(a, b) ((a) < (b) ? (a) : (b))
#define Math_Bound(min, var, max) {if ((var) < (min)) (var) = (min);if ((var) > (max)) (var) = (max);}

/* TODO: AABB/Sphere/Point on plane side, for culling */
/* TODO: reduce number of divisions in the code by storing inverses of stuff */

/* host_commands.c */
#define MAX_COMMANDS		256
#define MAX_CMD_SIZE		256
#define MAX_CMD_ARGS		16

EXTERNC int host_cmd_argc;
EXTERNC char *host_cmd_argv[MAX_CMD_ARGS];

typedef struct cmd_s
{
	int active;
	char name[MAX_CMD_SIZE];
	void (*cmd)(void);
} cmd_t;

#define MAX_CVAR_SIZE	64

#define	CVAR_ARCHIVE	1 /* save to config */
#define	CVAR_READONLY	2 /* can't change with the cmd defined by the macro HOST_CMD_SET */

typedef struct cvar_s
{
	struct cvar_s	*next;
	char			name[MAX_CVAR_SIZE];
	char			charvalue[MAX_CVAR_SIZE];
	double			doublevalue; /* updated automatically */
	int				flags;
	char			default_value[MAX_CVAR_SIZE];
	int				loadedfromconfig;	/*
											The config loader will set this,
											further initialization will clear it.
										*/
} cvar_t;

extern cvar_t *host_speeds;
extern cvar_t *host_netdelay; /* TODO: this causes connection erros on changelevel */
extern cvar_t *host_netdelay_jitterlow; /* TODO: this causes connection erros on changelevel */
extern cvar_t *host_netdelay_jitterhigh; /* TODO: this causes connection erros on changelevel */
extern cvar_t *host_netloss;

int Host_CMDValidateAlphanumericValue(const char *value);
EXTERNC unsigned char *Host_CMDSkipBlank(unsigned char *ptr);
EXTERNC void Host_CMDAdd(const char *name, void (*function)(void));
void Host_CMDBufferClear(void);
void Host_CMDBufferAdd(const char *input);
void Host_CMDBufferExecute(void);
EXTERNC cvar_t *Host_CMDAddCvar(const char *name, const char *value, const int flags);
cvar_t *Host_CMDGetCvar(const char *name, int error_if_not_found);
void Host_CMDForceCvarSet(cvar_t *varptr, const char *value, int warn);
EXTERNC void Host_CMDForceCvarSetValue(cvar_t *varptr, const double value, int warn);
void Host_CMDInit(void);
void Host_CMDShutdown(void);

/* host_filesystem.c */
EXTERNC int Host_FSFileExists(const char *path);
EXTERNC void Host_FSFileGetPath(const char *path, char *dst_fullpath, size_t dst_size);
EXTERNC int Host_FSLoadBinaryFile(char *path, mempool_t *mempool, char *id, unsigned char **result, int do_not_warn_on_failure);
EXTERNC int Host_FSWriteBinaryFile(char *path, const unsigned char *buffer, int count);
EXTERNC void *Host_FSFileHandleOpenBinaryRead(char *path);
EXTERNC void *Host_FSFileHandleOpenBinaryWrite(char *path);
EXTERNC void Host_FSFileHandleClose(void *handle);
int Host_FSFileHandleReadBinaryMemPool(void *handle, mempool_t *mempool, char *id, unsigned char **result, int count);
EXTERNC int Host_FSFileHandleReadBinaryDest(void *handle, unsigned char *result, int count);
EXTERNC int Host_FSFileHandleWriteBinary(void *handle, const unsigned char *buffer, int count);
void Host_FSInit(void);
void Host_FSShutdown(void);

/* host_netchan.c */
#define NET_PROTOCOL		1 /* if you update this, change the description in host_netchan.c too */
#define NET_PROTOCOL_INFO	2 /* if you update this, change the description in host_netchan.c too */
#define NET_MAGIC			0x1988

#define NET_ERR				-1
#define NET_OK				0

/* network timeout, in milliseconds */
#define		NET_MSTIMEOUT				10000 /* 10 seconds */

typedef unsigned short seqnum_t;
typedef unsigned int client_id_t;

/* be sure to keep the sizes in sync */
#define		NET_HEADERLEN				(sizeof(unsigned short)*2 + sizeof(seqnum_t)*2 + sizeof(client_id_t)*2)
#define		MAX_NETCHAN_DATASIZE		(MAX_NET_PACKETSIZE - NET_HEADERLEN) /* whole packet */
#define		MAX_NET_CMDSIZE				384 /* individual commands */

/* command types */
#define		NET_CMD_UNRELIABLE			0
#define		NET_CMD_RELIABLE			1

/*
	A full command
	will be created as an circular linked list, if next->active, then it's an overflow
	they will be sent only if the full command fits into the outgoing packet
	if a command doesn't fit on an empty packet, then we have a problem. Disconnect/drop client
*/
typedef struct packetcmd_s
{
	struct		packetcmd_s *next;
	int			active;
	char		*data; /* not zero-terminated, specification has changed */
	int			len;
	mstime_t	created_time;
} packetcmd_t;

typedef struct packetqueue_s {
	/* outgoing */
	packetcmd_t		*reliable; /* slower, but will get through for sure, in order too */
	packetcmd_t		*unreliable; /* may get lost or arrive out of order */
	char			*last_reliable;	/*
										Last reliable cmd sent, outseq refers to this and is included into this.
										It's also already removed from the reliable packet list.
									*/
	int				last_reliable_len;
	seqnum_t		outseq;	/*
								this sequence number refer to the packet we already sent through the network
								(last_reliable), not the one waiting to be sent
							*/
	seqnum_t		outseq_ack;	/*
									we get this back on EVERY packet we receive and set it here, if valid.
									It's the last reliable packet the other end has received from us

									if outseq == outseq_ack, we may send the next reliable packet
								*/

	/* these two are only valid for the server code */
	unsigned int				dest_id1;
	unsigned int				dest_id2;

	seqnum_t		inseq; /* if the incoming packet has a reliable bit, set this (if valid) and send it back in EVERY packet we send */
	mstime_t		last_received_time; /* absolute time the last packet was received. For timeout purposes TODO: convert to relative time to avoid overflows (will timeout before overflowing - do this everywhere) */
} packetqueue_t;

int Host_NetchanReceiveCommands(char *out, int *len, unsigned int *id1, unsigned int *id2, int *reliable, seqnum_t *inseq, seqnum_t *myseq, int server, client_id_t *client_id, client_id_t *game_id);
void Host_NetchanUpdateNC(packetqueue_t *nc, int reliable, seqnum_t r_inseq, seqnum_t r_myseq, mstime_t r_time);
int Host_NetchanCommandsLeft(packetqueue_t *nc, int type);
int Host_NetchanQueueCommand(packetqueue_t *nc, char *cmd, int len, int type);
int Host_NetchanDispatchCommands(packetqueue_t *nc, int server, client_id_t client_id, client_id_t game_id);
void Host_NetchanCleanQueue(packetqueue_t *nc);
void Host_NetchanClean(packetqueue_t *nc, int reconnect);
packetqueue_t *Host_NetchanCreate(void);

void MSG_WriteEntity(char *msg, int *len, const short data);
void MSG_WritePrecache(char *msg, int *len, const short data);
void MSG_WriteTime(char *msg, int *len, const double data);
void MSG_WriteByte(char *msg, int *len, const unsigned char data);
void MSG_WriteShort(char *msg, int *len, const short data);
void MSG_WriteInt(char *msg, int *len, const int data);
void MSG_WriteDouble(char *msg, int *len, const double data);
void MSG_WriteVec1(char *msg, int *len, const vec_t data);
void MSG_WriteVec3(char *msg, int *len, const vec3_t data);
void MSG_WriteString(char *msg, int *len, const char *data);
void MSG_ReadEntity(char *msg, int *len, unsigned int maxlen, short *data);
void MSG_ReadPrecache(char *msg, int *len, unsigned int maxlen, short *data);
void MSG_ReadTime(char *msg, int *len, unsigned int maxlen, double *data);
void MSG_ReadByte(char *msg, int *len, unsigned int maxlen, unsigned char *data);
void MSG_ReadShort(char *msg, int *len, unsigned int maxlen, short *data);
void MSG_ReadInt(char *msg, int *len, unsigned int maxlen, int *data);
void MSG_ReadDouble(char *msg, int *len, unsigned int maxlen, double *data);
void MSG_ReadVec1(char *msg, int *len, unsigned int maxlen, vec_t *data);
void MSG_ReadVec3(char *msg, int *len, unsigned int maxlen, vec3_t data);
void MSG_ReadString(char *msg, int *len, unsigned int maxlen, char *data, int datamaxsize);

/* host_models.c */
#define ANIMATION_MAX_BLENDED_FRAMES	5 /* should be kept in sync with the definition of animation_slot_names and should be kept in sync with the definitions in game_sv.h, should be >= 1. Also keep in sync with the snapshot definitions in server.h */

typedef struct model_s {
	int			active;
	char		name[MAX_MODEL_NAME];
	void		*data; /* system will take care of this */
} model_t;

model_t *Host_LoadModel(const char *name, int server);
void Host_CleanModels(void);
void Host_ModelsInit(void);
void Host_ModelsShutdown(void);

/* host_voxels.c */

/*
	Please define only one of these. Trimeshes allow for smoothed terrain but
	uses more memory and is slower to rebuild. Boxes won't be smoothed (for
	physics at least) but will rebuild faster.

	Trimeshes also make it easir to get trapped inside chunks.
*/
/* #define CHUNKS_AS_TRIMESHES */
#define CHUNKS_AS_BOXES

/* TODO: lots of this is system (implementation) specific data, we should not have them here */
/* block sizes */
#define VOXEL_SIZE_X					1.f
#define VOXEL_SIZE_Y					1.f
#define VOXEL_SIZE_Z					1.f
#define VOXEL_SIZE_X_2					0.5f
#define VOXEL_SIZE_Y_2					0.5f
#define VOXEL_SIZE_Z_2					0.5f

#define VOXEL_BLOCKTYPE_EMPTY			0
#define VOXEL_BLOCKTYPE_MAX				16 /* types 1-15 will be user-defined */

/* chunk sizes in blocks (bigger means faster rendering but slower rebuilding times) */
#define VOXEL_CHUNK_SIZE_X				16
#define VOXEL_CHUNK_SIZE_Y				16
#define VOXEL_CHUNK_SIZE_Z				16
#define VOXEL_CHUNK_MAX_VERTICES		65536 /* TODO: optimize this, make dynamic, tune according to VOXEL_CHUNK_SIZE_* */
#define VOXEL_CHUNK_MAX_TRIANGLESVERTS	(32768 * 3) /* TODO: optimize this, make dynamic, tune according to VOXEL_CHUNK_SIZE_* */
#define VOXEL_MAX_CHUNKS				8192 /* TODO: CHUNK LEVEL OF DETAIL, tune according to VOXEL_(MIN|MAX)_CHUNK_* */
/* TODO: limit X and Y too before crashing? */
#define VOXEL_MAX_CHUNK_Y				15
#define VOXEL_MIN_CHUNK_Y				0

typedef struct model_voxel_block_s {
	unsigned char type;
} model_voxel_block_t;

typedef struct model_voxel_chunk_s {
	/* TODO: network requires no padding between these two */
	int origin[3]; /* chunk position in the chunk grid (not real world coordinates), center of block (0, 0, 0) */
	model_voxel_block_t blocks[VOXEL_CHUNK_SIZE_X][VOXEL_CHUNK_SIZE_Y][VOXEL_CHUNK_SIZE_Z];
	vec3_t mins, maxs;

	/* data below here must be regenerated when data above is updated */

	int vbo_id;

#ifdef CHUNKS_AS_TRIMESHES
	vec3_t physvertices[VOXEL_CHUNK_MAX_VERTICES]; /* TODO: still means to occupy less space, join triangles */
	unsigned int physnumvertices;
	unsigned int phystrianglesverts[VOXEL_CHUNK_MAX_TRIANGLESVERTS];
	unsigned int physnumtrianglesverts;
#endif /* CHUNKS_AS_TRIMESHES */
} model_voxel_chunk_t;

typedef struct model_voxel_s {
	/* this data should only be set while loading/connecting */
	void *texture; /* texture atlas, for client only, type zero isn't used, will just occupy space for now? FIXME */

	/* blocks are divided into chunks to maximize performance */
	/* TODO: organize chunks in a better way? */
	int num_chunks;
	model_voxel_chunk_t		*chunklist[VOXEL_MAX_CHUNKS];

	/* used as a list to indicate when we have updated chunk contents and need to recreate cached data inside them */
	int num_updated_chunks;
	int updated_chunks_list[VOXEL_MAX_CHUNKS];

	/* TODO: async streaming and saving to disk! do not keep everything in memory! what about entities? just pause them? :( */
	/* TODO: visibility list (loaded chunks?) and render list (after culling, also do not put completely empty chunks in this list (not empty in blocks, but empty in triangles to render!)) */
	/* TODO: async updating too */
	/* TODO: save alterations made and load them on loadgame. */
	/* TODO: do not load for client, (send trimesh copy to client instead of blocks?), warn client when trimeshes are created/loaded, updated and destroyed/saved) */
} model_voxel_t;

void Host_CleanVoxelsByProxy(void);
model_voxel_t *Host_VoxelGetData(void);
void Host_VoxelComposeAbsCoord(model_voxel_chunk_t *chunk, int *blockpos, int *out);
void Host_VoxelDecomposeAbsCoord(int *absindex, int *outblock, int *outchunk);
int Host_VoxelGetChunk(int *chunkcoord);
int Host_VoxelNewChunk(int *chunkcoord);
int Host_VoxelSetChunk(int chunkoriginx, int chunkoriginy, int chunkoriginz, unsigned char *type);
int Host_VoxelSetBlock(int absx, int absy, int absz, unsigned char type);
void Host_VoxelCommitUpdates(void);
void Host_VoxelCreate(int loading_saved_game);
void Host_VoxelLoad(char *name);
void Host_VoxelSave(char *name);

/* host_utils.c */
uint32_t Host_CRC32(uint32_t crc, const void *buf, size_t size);
int Host_BinaryCompare(const void *buf1, const void *buf2, size_t size);

/* host_cpputils.cpp */

EXTERNC void Host_UtilMapInsertInt3ToInt(void *data, int key[3], int pair);
EXTERNC void Host_UtilMapInsertCharPToVoidPVoid(void *data, char *key, void (*pair)(void));
EXTERNC int Host_UtilMapRetrieveInt3ToInt(void *data, int key[3]);
EXTERNC void *Host_UtilMapRetrieveCharPToVoidPVoid(void *data, char *key);
EXTERNC void *Host_UtilMapCreateInt3ToInt(void);
EXTERNC void *Host_UtilMapCreateCharPToVoidPVoid(void);
EXTERNC void Host_UtilMapDestroyInt3ToInt(void *data);
EXTERNC void Host_UtilMapDestroyCharPToVoidPVoid(void *data);

#endif /* HOST_H */
