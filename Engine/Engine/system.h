/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef SYSTEM_H
#define SYSTEM_H

/* TODO: about for/while loops: it's always more efficient to count to zero, the processor can check for zeros faster */
/* TODO: I assumed almost everything in memory is tightly packed, without padding */
/* TODO: pass pointers (and not entire structs) as arguments, to minimize cache use (is this optimized by the compiler?) */

/* constants and typedefs */

#ifdef __GNUC__
#define CDECL __attribute__ ((__cdecl__))

#include <stdint.h>
#else
#define CDECL __cdecl

/* from the SDL2 source - START */
#if !defined(_STDINT_H_) && (!defined(HAVE_STDINT_H) || !_HAVE_STDINT_H)
#if defined(__GNUC__) || defined(__DMC__) || defined(__WATCOMC__)
#define HAVE_STDINT_H   1
#elif defined(_MSC_VER)
typedef signed __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef signed __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef signed __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#ifndef _UINTPTR_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64 uintptr_t;
#else
typedef unsigned int uintptr_t;
#endif
#define _UINTPTR_T_DEFINED
#endif
/* Older Visual C++ headers don't have the Win64-compatible typedefs... */
#if ((_MSC_VER <= 1200) && (!defined(DWORD_PTR)))
#define DWORD_PTR DWORD
#endif
#if ((_MSC_VER <= 1200) && (!defined(LONG_PTR)))
#define LONG_PTR LONG
#endif
#else /* !__GNUC__ && !_MSC_VER */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#ifndef _SIZE_T_DEFINED_
#define _SIZE_T_DEFINED_
typedef unsigned int size_t;
#endif
typedef unsigned int uintptr_t;
#endif /* __GNUC__ || _MSC_VER */
#endif /* !_STDINT_H_ && !HAVE_STDINT_H */

#ifdef _WIN64
# define SIZEOF_VOIDP 8
#else
# define SIZEOF_VOIDP 4
#endif
/* from the SDL2 source - END */

#endif

#ifndef false
#define false 0
#define true (!false)
#endif /* false */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4	0.785398163397448309616
#endif /* M_PI */

#ifndef MAX_PATH
#define MAX_PATH		260
#endif /* MAX_PATH */
#define MAX_LINE		4096

typedef uint64_t framenum_t; /* TODO: is this used where it should be used? */
typedef double mstime_t; /* TODO: is this used where it should be used? is unsigned long long multiplatform? */

typedef float vec_t; /* be aware of the formatting of string reading and printing functions, along WITH FILE FORMATS THAT HAVE THE FLOAT BYTE SIZE (if you change to double) */
typedef vec_t vec2_t[2]; /* be careful! vec2_t, etc, are ARRAYS */
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];

/* sys_***.c - system-dependent functions */
int Sys_Vsnprintf(char *buffer, size_t buffersize, const char *format, va_list args);
char *Sys_Strncat(char *dest, const char *source, size_t count);
EXTERNC int Sys_Snprintf(char *buffer, size_t buffersize, const char *format, ...);
#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
#define Sys_Sscanf_s sscanf
#define _strcmpi strcasecmp
#else
#define Sys_Sscanf_s sscanf_s
#endif /* __GNUC__ */

#if 0 /* linear memory pools - also change Host_FSLoadBinaryFile and Sys_LoadTextureData */
typedef struct memblock_type_s
{
	unsigned char *membuffer;
	int maxmb;
	int lowmark;
	int used;
} memblock_type_t;

EXTERNC memblock_type_t std_mem;
EXTERNC memblock_type_t tmp_mem;
EXTERNC memblock_type_t mdl_mem;
extern memblock_type_t snd_mem;
extern memblock_type_t svr_mem;

EXTERNC void *Sys_MemAlloc (memblock_type_t *memblock, int size, char *name);
EXTERNC void Sys_MemFreeToLowMark (memblock_type_t *memblock, int mark);
EXTERNC int Sys_MemLowMark (memblock_type_t *memblock);
#else

#define MEMBLOCK_MAX_NAME		32

typedef struct memblock_s {
	int		sentinel;
	int		size;
	char	name[MEMBLOCK_MAX_NAME];
} memblock_t;

#define MEMPOOL_MAX_MEMBLOCKS	(65536 * 16)

typedef struct mempool_s {
	memblock_t	*blocks[MEMPOOL_MAX_MEMBLOCKS];
	int			blocks_num;
} mempool_t;

EXTERNC mempool_t std_mem;
EXTERNC mempool_t tmp_mem;
EXTERNC mempool_t mdl_mem;
extern mempool_t snd_mem;
extern mempool_t svr_mem;

EXTERNC void *Sys_MemAlloc (mempool_t *mempool, int size, char *name);
EXTERNC void Sys_MemFreeToLowMark (mempool_t *mempool, int mark);
EXTERNC int Sys_MemLowMark (mempool_t *mempool);
#endif

int Sys_FileExists(char *path);
int Sys_FileReadBinary(char *path, unsigned char *buffer, int start, int count);
int Sys_FileWriteBinary(char *path, const unsigned char *buffer, int count);
void *Sys_FileHandleOpenRead(char *path, int start);
void *Sys_FileHandleOpenWrite(char *path);
void Sys_FileHandleClose(void *handle);
int Sys_FileHandleReadBinary(void *handle, unsigned char *buffer, int count);
int Sys_FileHandleWriteBinary(void *handle, const unsigned char *buffer, int count);

mstime_t Sys_Time(void);

/* key/axis indices TODO: complete this list */
/* TODO: combined x/y/z axis of the same input device and sent them as vec2_t/vec3_t and normalized? */
#define KEY_INVALID			0
#define KEY_BACKSPACE		8
#define KEY_TAB				9
#define KEY_RETURN			13
#define KEY_ESC				27
#define KEY_SPACE			32
#define KEY_QUOTE			39
#define KEY_0				48
#define KEY_1				49
#define KEY_2				50
#define KEY_3				51
#define KEY_4				52
#define KEY_5				53
#define KEY_6				54
#define KEY_7				55
#define KEY_8				56
#define KEY_9				57
#define KEY_BACKQUOTE		96
#define KEY_A				97
#define KEY_B				98
#define KEY_C				99
#define KEY_D				100
#define KEY_E				101
#define KEY_F				102
#define KEY_G				103
#define KEY_H				104
#define KEY_I				105
#define KEY_J				106
#define KEY_K				107
#define KEY_L				108
#define KEY_M				109
#define KEY_N				110
#define KEY_O				111
#define KEY_P				112
#define KEY_Q				113
#define KEY_R				114
#define KEY_S				115
#define KEY_T				116
#define KEY_U				117
#define KEY_V				118
#define KEY_W				119
#define KEY_X				120
#define KEY_Y				121
#define KEY_Z				122
#define KEY_INSERT			267
#define KEY_DELETE			268
#define KEY_HOME			269
#define KEY_END				270
#define KEY_PAGEUP			271
#define KEY_PAGEDOWN		272
#define KEY_UP				273
#define KEY_DOWN			274
#define KEY_RIGHT			275
#define KEY_LEFT			276
#define KEY_F1				282
#define KEY_F2				283
#define KEY_F3				284
#define KEY_F4				285
#define KEY_F5				286
#define KEY_F6				287
#define KEY_F7				288
#define KEY_F8				289
#define KEY_F9				290
#define KEY_F10				291
#define KEY_F11				292
#define KEY_F12				293
#define KEY_LCONTROL		306
#define KEY_LSHIFT			307
#define KEY_LALT			308
#define KEY_RCONTROL		309
#define KEY_RSHIFT			310
#define KEY_RALT			311
#define KEY_PAUSE			313
#define MOUSE0_BUTTON4		487
#define MOUSE0_BUTTON3		488
#define MOUSE0_WHEELRIGHT	489
#define MOUSE0_WHEELLEFT	490
#define MOUSE0_WHEELDOWN	491
#define MOUSE0_WHEELUP		492
#define MOUSE0_BUTTON2		493
#define MOUSE0_BUTTON1		494
#define MOUSE0_BUTTON0		495
#define MOUSE0_HORIZONTAL	496
#define MOUSE0_VERTICAL		497

const char *Sys_KeyIndexToKeyName(int index);
int Sys_KeyNameToKeyIndex(const char *name);
void Sys_ProcessEvents(void);
EXTERNC void Sys_ExclusiveInput(int value);
void Sys_InputSetTextMode(int value);

void Sys_SaveStackAndPC(void);
void Sys_RestoreStackAndPC(void);
EXTERNC void Sys_Printf(const char *msg, ...);
EXTERNC void Sys_Error(const char *error, ...);
double Sys_Random(int range_min, int range_max);
void Sys_KeepRandom(void);
void Sys_Shutdown(void);

/* sys_***_net.c - system-dependent network functions */
#define MAX_NET_PACKETSIZE					1200 /* WARNING: beware of fragmentation */

void Sys_NetInit(void);
void Sys_NetShutdown(void);

int Sys_NetSVWrite(char *data, int len, unsigned int id1, unsigned int id2);
int Sys_NetSVRead(char *pkt, unsigned int *id1, unsigned int *id2);
/* these two may be called once at each new game */
int Sys_NetSVStart(void);
void Sys_NetSVStop(void);
void Sys_NetSVInit(void);
void Sys_NetSVShutdown(void);

int Sys_NetCLServerIsLocal(void);
void Sys_NetCLServerSetToLocal(void);
int Sys_NetCLWrite(char *data, int len);
int Sys_NetCLRead(char *pkt);
/* these two may be called once at each new game */
int Sys_NetCLStart(void);
void Sys_NetCLStop(void);
void Sys_NetCLInit(void);
void Sys_NetCLShutdown(void);

/* sys_models.c - system-dependent model loading, parsing and drawing */
typedef struct model_vertex_s {
	/* FIXME: guarantee no padding, including in arrays of these */

	/* WARNING: DO NOT change this struct without a reason! This may (and will) be tied to file formats and the rendering flow */
	vec3_t origin;
	vec2_t texcoord0; /* two textures per face */
	vec2_t texcoord1;
	vec3_t normal;
	unsigned char color[4]; /* RGBA */
	vec4_t tangent;
	vec4_t weights; /* blend weights per bone, sum to 255 */
	unsigned char bones[4]; /* bones indexes */
} model_vertex_t;

/* structures for passing around data embedded into models */
#define MAX_ATTRIBS_PER_ENTITY		64
typedef struct model_entity_s {
	int num_attribs;
	char *attribs[MAX_ATTRIBS_PER_ENTITY]; /* TODO: see if we are not using this limit anywhere */
	char *values[MAX_ATTRIBS_PER_ENTITY];
} model_entity_t;

/* triangle soup defining a model */
typedef struct model_trimesh_part_s {
	model_vertex_t *verts;
	int vert_stride;
	unsigned int vert_count;

	/* this should be a unsigned int FIXME */
	int *indexes; /* 3-component, triangle */
	unsigned int index_count;
	int index_stride;
} model_trimesh_part_t;

typedef struct model_trimesh_s {
	model_trimesh_part_t *trimeshes;
	int num_trimeshes;
} model_trimesh_t;

/* convex brushes defining a model */
typedef struct model_brushes_s {
	int num_brushes;
	int *brush_sides; /* how many planes each brush has, indexed by num_brushes */

	/* these are indexed by num_brushes and each of their brush_sides[cur_brush], so they can't be directly adressed, only serially */
	vec_t *normal; /* 3-component, not really aligned because each brush may have a different number of planes */
	vec_t *dist; /* not really aligned because each brush may have a different number of planes */
} model_brushes_t;

typedef struct model_heightfield_s {
	int		width;
	vec_t	width_scale;
	int		length;
	vec_t	length_scale;
	vec_t	*data;
	vec_t	minheight, maxheight;
} model_heightfield_t;

void *Sys_LoadModel(const char *name, void *basemodel);
void Sys_LoadModelClientData(void *modeldata);
void Sys_LoadModelEntities(void *modeldata, int *num_entities, model_entity_t **entities);
void Sys_LoadModelPhysicsTrimesh(void *modeldata, model_trimesh_t **trimesh);
void Sys_LoadModelPhysicsBrushes(void *modeldata, model_brushes_t **brushes);
void Sys_LoadModelPhysicsHeightfield(void *modeldata, model_heightfield_t **heightfield);
void Sys_ModelAnimationInfo(void *data, const unsigned int animation_name, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation);
int Sys_ModelAnimationExists(void *data, const unsigned int animation_name);
void Sys_ModelStaticLightInPoint(void *data, const vec3_t point, vec3_t ambient, vec3_t directional, vec3_t direction);
EXTERNC void Sys_ModelAABB(void *data, const vec_t frame, vec3_t mins, vec3_t maxs);
unsigned int Sys_ModelPointContents(void *data, const vec3_t point);
void Sys_ModelTraceline(void *data, vec3_t start, vec3_t end, int *allsolid, int *startsolid, vec_t *fraction, vec3_t endpos, vec3_t plane_normal, vec_t *plane_dist);
void Sys_ModelTracesphere(void *data, vec3_t start, vec3_t end, vec_t radius, int *allsolid, int *startsolid, vec_t *fraction, vec3_t endpos, vec3_t plane_normal, vec_t *plane_dist);
void Sys_ModelTracebox(void *data, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int *allsolid, int *startsolid, vec_t *fraction, vec3_t endpos, vec3_t plane_normal, vec_t *plane_dist);
void Sys_ModelGetTagTransform(void *data, const unsigned int tag_idx, const int local_coords, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, const int ent);
void Sys_ModelAnimate(void *data, const int ent, vec3_t origin, vec3_t angles, vec_t *frames, const int anim_pitch);
EXTERNC int Sys_ModelIsStatic(void *data);
int Sys_ModelHasPVS(void *data);
void Sys_ModelPVSGetClustersBox(void *data, const vec3_t absmins, const vec3_t absmaxs, int *clusters, int *num_clusters, const int max_clusters);
void Sys_ModelPVSCreateFatPVSClusters(void *data, const vec3_t eyeorigin);
int Sys_ModelPVSTestFatPVSClusters(void *data, const int *clusters, const int num_clusters);
EXTERNC int Sys_VideoDraw3DModel(void *data, vec3_t eyeorigin, vec3_t eyeangles, vec3_t modelorigin, vec3_t modelangles, const int anim_pitch, vec_t *modelframes, int modelent, unsigned int desired_shader);

/* sys_***_video.c - system-dependent video functions */

EXTERNC vec_t Sys_FontGetStringWidth(const char *text, vec_t scalex);
EXTERNC vec_t Sys_FontGetStringHeight(const char *text, vec_t scaley);

/* base width and height, the size of the projected 2d screen (subpixels ok, width gets stretched or shrinked for different aspect ratios, exposing values beyond 0 to BASE_WIDTH or hiding values inside 0 to BASE_WIDTH) */
#define BASE_WIDTH	640
#define BASE_HEIGHT	480

EXTERNC void Sys_GenerateMipMaps(const unsigned char *name, unsigned char *data, const int width, const int height, const int level_count);
EXTERNC void Sys_LoadTextureData(const char *name, int *outwidth, int *outheight, unsigned char **outdata, mempool_t *mempool);
EXTERNC void Sys_LoadTexture(const char *name, int cl_id, unsigned int *id, int *outwidth, int *outheight, unsigned char *indata, int inwidth, int inheight, int data_has_mipmaps, int mipmapuntilwidth, int mipmapuntilheight);
EXTERNC void Sys_UnloadTexture(unsigned int *id);

EXTERNC void Sys_UpdateVBO(int id, model_trimesh_part_t *trimesh, int is_triangle_strip);
EXTERNC int Sys_UploadVBO(int id, model_trimesh_part_t *trimesh, int is_triangle_strip, int will_be_updated);
void Sys_DeleteVBO(int id);
EXTERNC void Sys_VBOsInit(void);
EXTERNC void Sys_CleanVBOs(int shutdown);

void Sys_VideoSet3D(const vec3_t origin, const vec3_t angles, const vec_t zfar, const int set_camera_frustum);
int Sys_Video3DPointAndDirectionFromUnitXY(const vec_t unit_x, const vec_t unit_y, vec3_t point, vec3_t direction);
EXTERNC void Sys_VideoSet2D(int use_base_size);
EXTERNC void Sys_Video2DAbsoluteFromUnitX(vec_t unit_x, vec_t *abs_x);
EXTERNC void Sys_Video2DAbsoluteFromUnitY(vec_t unit_y, vec_t *abs_y);

/* this index will restart the draw call (useful for drawing multiple triangle strips, etc with one call) */
/* TODO: indices as GL_UNSIGNED_INT (GLES2 platforms mostly won't work with this, use GL_UNSIGNED_SHORT for them), check indice limit when loading models */
/* TODO: needs gl 3.1 */
/* #define USE_PRIMITIVE_RESTART_INDEX */
#ifdef USE_PRIMITIVE_RESTART_INDEX
#define SYS_VIDEO_PRIMITIVE_RESTART_INDEX		UINT_MAX
#endif

/* TODO: move these to sys_*_video.c */
EXTERNC void Sys_VideoBindShaderProgram(unsigned int shader_id, const vec4_t light0_position, const vec4_t light0_diffuse, const vec4_t light0_ambient);
EXTERNC void Sys_VideoTransformFor3DModel(vec_t *ent_modelmatrix);
#define SHADER_MAX_BONES		40 /* TODO: keep this in sync with the shaders, unify */
/* some old gpus need a value as low as 2 or even 1 */
#define SHADER_MAX_LIGHTS		8  /* TODO: keep this in sync with the shaders, unify */
/* Uniform locations */
enum {
	SHADER_UNIFORM_PROJECTIONMATRIX = 0,
	SHADER_UNIFORM_VIEWMATRIX = 1,
	SHADER_UNIFORM_MODELMATRIX,
	SHADER_UNIFORM_MV,
	SHADER_UNIFORM_MV_NORMALS,
	SHADER_UNIFORM_MVP,
	SHADER_UNIFORM_COLOR,
    SHADER_UNIFORM_TEXTURE0,
    SHADER_UNIFORM_TEXTURE1,
	SHADER_UNIFORM_TEXTURE3,
	SHADER_UNIFORM_TEXTURE4,
	SHADER_UNIFORM_TEXTURE5,
	SHADER_UNIFORM_TEXTURE2,
	SHADER_UNIFORM_TEXTURE6,
	SHADER_UNIFORM_TEXTURE7,
	SHADER_UNIFORM_TEXTURE8,
	SHADER_UNIFORM_TEXTURE9,
	SHADER_UNIFORM_TEXTURE10,
	SHADER_UNIFORM_TEXTURE11,
	SHADER_UNIFORM_TEXTURE12,
	SHADER_UNIFORM_CAMERA_VIEW_MATRIX_INV,
	SHADER_UNIFORM_LIGHT_VIEW_MATRICES,
	SHADER_UNIFORM_LIGHT_PROJECTION_MATRICES,
	SHADER_UNIFORM_LIGHT_POSITIONS,
	SHADER_UNIFORM_LIGHT_INTENSITIES,
	SHADER_UNIFORM_LIGHT0_POSITION,
	SHADER_UNIFORM_LIGHT0_DIFFUSE,
	SHADER_UNIFORM_LIGHT0_AMBIENT,
	SHADER_UNIFORM_BONEMATS,
    SHADER_NUM_UNIFORMS
};
enum {
    SHADER_DEPTHSTORE = 0, /* generic depth storage and shadowmapping shader 1st stage - light point of view depth storing in a cubemap, this shader is dependent on depth testing being correctly set up */
	SHADER_DEPTHSTORE_SKINNING = 1,
	SHADER_DEPTHSTORE_TERRAIN,
    SHADER_SHADOWMAPPING, /* shadowmapping shader 2nd stage - drawing */
	SHADER_SHADOWMAPPING_SKINNING,
	SHADER_SHADOWMAPPING_TERRAIN,
	SHADER_LIGHTING_NO_SHADOWS, /* per-pixel lighting but without shadows */
	SHADER_LIGHTING_NO_SHADOWS_SKINNING,
	SHADER_LIGHTING_NO_SHADOWS_TERRAIN,
	SHADER_FIXED_LIGHT,
	SHADER_FIXED_LIGHT_SKINNING,
	SHADER_FIXED_LIGHT_TERRAIN,
	SHADER_LIGHTMAPPING,
	SHADER_PARTICLE,
	SHADER_SKYBOX,
	SHADER_2D,
	SHADER_2DTEXT,
    SHADER_NUM
};

/* currently, only these 5 public functions (and whichever functions they call) draw anything to the screen */
EXTERNC void Sys_VideoDraw2DPic(unsigned int *id, int width, int height, int x, int y);
EXTERNC void Sys_VideoDraw2DText(const char *text, vec_t x, vec_t y, vec_t scalex, vec_t scaley, vec_t r, vec_t g, vec_t b, vec_t a);
EXTERNC void Sys_VideoDraw2DFill(int width, int height, int x, int y, vec_t r, vec_t g, vec_t b, vec_t a);
EXTERNC int Sys_VideoDraw3DTriangles(const model_vertex_t *verts, const int vert_count, const int use_origins, const int use_colors, const int use_texcoords0, const int use_texcoords1, const int use_normals, const unsigned int *indices, const unsigned int indices_count, int texture_cl_id0, int texture_cl_id1, int texture_cl_id3, int texture_cl_id4, int texture_cl_id5, int is_triangle_strip_or_fan);
EXTERNC int Sys_VideoDrawVBO(const int id, int texture_cl_id0, int texture_cl_id1, int texture_cl_id3, int texture_cl_id4, int texture_cl_id5, int vertstartinclusive, int vertendinclusive, int idxcount, int idxstart);

EXTERNC void Sys_StartVideoFrame(void);
EXTERNC void Sys_EndVideoFrame(void);
EXTERNC void Sys_GetWidthHeight(int *width, int *height); /* for system module use only */
EXTERNC void Sys_InitVideo(void);
EXTERNC void Sys_ShutdownVideo(void);

EXTERNC void Sys_SkyboxLoad(unsigned char *name);
EXTERNC void Sys_SkyboxUnload(void);

EXTERNC void Sys_Draw3DFrame(vec3_t cameraorigin, vec3_t cameraangles, void *entities_ptr, void *particles_ptr, void *voxel_ptr);

/* sys_physics.c - system and/or middleware-dependent physics functions */

#define VEHICLE1_ALL_WHEEL_DRIVE	0
#define VEHICLE1_FRONT_WHEEL_DRIVE	1
#define VEHICLE1_REAR_WHEEL_DRIVE	2

typedef struct phys_edict_vehicle_info_s {
	vec3_t	wheelDirectionCS0;
	vec3_t	wheelAxleCS;

	vec_t	gEngineForce;

	vec_t	defaultBreakingForce;
	vec_t	handBrakeBreakingForce;
	vec_t	gBreakingForce;

	vec_t	maxEngineForce; /* this should be engine/velocity dependent */

	vec_t	gVehicleSteering;
	vec_t	steeringIncrement;
	vec_t	steeringClamp;
	vec_t	wheelRadius;
	vec_t	wheelWidth;
	vec_t	wheelFriction;
	vec_t	suspensionStiffness;
	vec_t	suspensionDamping;
	vec_t	suspensionCompression;
	vec_t	maxSuspensionTravelCm;
	vec_t	maxSuspensionForce;
	vec_t	rollInfluence;

	vec_t	suspensionRestLength;

	vec_t	connectionHeight;
	vec_t	connectionStickLateralOutWheelWidthMultiplier;
	vec_t	connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier;

	vec3_t	chassis_box_half_extents;
	vec3_t	chassis_box_localpos;
	vec3_t	suppchassis_box_half_extents;
	vec3_t	suppchassis_box_localpos;


	/* TODO: entindex_t here */
	int		wheel_ents[4]; /* entities which will have, despite having no physical representation, their properties set to the wheel properties */
	int		wheel_drive;
} phys_edict_vehicle_info_t;

#define PHYSICS_SHAPE_BOX						0
#define PHYSICS_SHAPE_SPHERE					1
#define PHYSICS_SHAPE_CAPSULE_Y					2
#define PHYSICS_SHAPE_TRIMESH_FROM_MODEL		3
#define PHYSICS_SHAPE_TRIMESH_FROM_DATA			4
#define PHYSICS_SHAPE_CONVEXHULLS_FROM_MODEL	5
#define PHYSICS_SHAPE_CONVEXHULLS_FROM_DATA		6
#define PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL	7
#define PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA		8
#define PHYSICS_SHAPE_VEHICLE1					9
#define PHYSICS_SHAPE_VOXEL_BOX					10
#define PHYSICS_SHAPE_VOXEL_TRIMESH				11

EXTERNC void Sys_PhysicsDebugDraw(void *physworld);

EXTERNC int Sys_PhysicsGetEntityData(void *physworld, const int ent, vec_t *locked_angles, unsigned int *solid, int *type, vec_t *creation_vecs, int *creation_model, phys_edict_vehicle_info_t *creation_vehicle1, vec_t *mass, int *trace_onground);

EXTERNC void Sys_PhysicsApplyImpulse(void *physworld, const int ent, const vec3_t impulse, const vec3_t local_pos);
EXTERNC void Sys_PhysicsApplyForces(void *physworld, const int ent, vec3_t dir, const vec3_t linear_force, const vec3_t max_speed, const int vertical_ok, const int horizontal_ok, const int deaccel_vertical, const int deaccel_horizontal);
EXTERNC void Sys_PhysicsSetLinearVelocity(void *physworld, const int ent, vec3_t vel);
EXTERNC void Sys_PhysicsSetAngularVelocity(void *physworld, const int ent, vec3_t avel);

EXTERNC void Sys_PhysicsSimulate(void *physworld, mstime_t frametime, int entity_to_simulate, int *entities_ignore);

EXTERNC int Sys_PhysicsIsDynamic(void *physworld, int ent);
EXTERNC void Sys_PhysicsSetTransform(void *physworld, int ent, const vec3_t origin, const vec3_t angles, const vec3_t locked_angles);
EXTERNC void Sys_PhysicsSetSolidState(void *physworld, int ent, const unsigned int value);
EXTERNC int Sys_PhysicsCreateObject(void *physworld, int ent, int type, void *data, vec_t mass, vec3_t origin, vec3_t angles, vec3_t locked_angles, int trace_onground);
EXTERNC void Sys_PhysicsDestroyObject(void *physworld, int ent);

EXTERNC void Sys_PhysicsCreateVoxelChunk(void *physworld, int chunk, int type, vec3_t half_size, vec_t *box_origins, int num_boxes, vec_t *verts, int vert_stride, int vert_count, int *indexes, int index_count, int index_stride);
EXTERNC void Sys_PhysicsDestroyVoxelChunk(void *physworld, int chunk);

EXTERNC int Sys_PhysicsTraceline(void *physworld, int forent, vec3_t origin, vec3_t forward, vec_t length, int ignore_world_triggers, vec_t impulse_to_closest);

EXTERNC void *Sys_CreatePhysicsWorld(uint8_t server);
EXTERNC void Sys_DestroyPhysicsWorld(void *physworld);
EXTERNC void Sys_LoadPhysicsWorld(void *physworld, char *name);
EXTERNC void Sys_SavePhysicsWorld(void *physworld, char *name);
EXTERNC void Sys_InitPhysics(void);
EXTERNC void Sys_ShutdownPhysics(void);

/* sys_win_sound.c - system and/or middleware-dependent sound system functions */
void Sys_SoundUpdateListener(vec3_t pos, vec3_t vel, vec3_t angles);
void Sys_SoundUpdateEntityAttribs(void *entindexptr, vec3_t pos, vec3_t vel);
void Sys_StopAllSounds(int stoplocal);
void Sys_PlaySound(unsigned int *snddata, void *entindex, vec3_t origin, vec3_t vel, int channel, vec_t pitch, vec_t gain, vec_t attenuation, int loop);
void Sys_StopSound(void *entindex, int channel);
void Sys_LoadSound(unsigned int *bufferptr, const char *name);
void Sys_UnloadSound(void *data);
void Sys_SoundsInit(void);
void Sys_SoundsShutdown(void);

#endif /* SYSTEM_H */
