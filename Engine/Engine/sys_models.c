/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2017 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

#define BRUTEFORCE_BATCH_QUAKE3BSP

/*
============================================================================

Model loading and parsing

TODO: USE POINTERS TO STRUCTS TO AVOID PASSING LOTS OF ARGUMENTS
TODO: see how q3bsp terrains are made
TODO: delete intermediary data where we can (search for Sys_MemAllloc and file loads)
TODO: error if loading clientdata more than once for a given model
TODO: for stuff that may get loaded more than once per server, cache on loading and
avoid recreation everytime the gamecode or something else asks for them (like brushes
lists)

============================================================================
*/

extern cvar_t *r_lodbias;
extern cvar_t *r_lodhysteresis;

/*
============================================================================

Model type: custom facemesh

============================================================================
*/

#define MODELTYPE_FACEMESH_TYPE_ID		0x1def

#define FACEMESH_MAX_FRAMES				16

typedef struct model_facemesh_animation_s {
	int						exists;
	unsigned int			start_frame;
	unsigned int			num_frames;
	int						loop;
	vec_t					frames_per_second;
} model_facemesh_animation_t;

typedef struct model_facemesh_s {
	int							type; /* MODELTYPE_*, should be the first */

	int							num_frames;
	texture_t					**textures;
	char						**texturenames;

	model_trimesh_part_t		sprite;

	int							vbo_id;

	vec3_t						mins, maxs;

	model_facemesh_animation_t	animations[NUM_ANIMATIONS];

	char						name[MAX_MODEL_NAME];
} model_facemesh_t;

/*
===================
Sys_LoadModelFaceMesh

Should only be called by Sys_LoadModel.
Returns data pointer for the model data

It's better if the ascii files end with a newline

TODO FIXME: replace this function ENTIRELY
TODO: make this the sprite type, make always-facing-camera and oriented types, make a clipping function for oriented sprites use as bullet marks, etc, on walls
===================
*/
void *Sys_LoadModelFaceMesh(const char *name, char *path, unsigned char *data, int size)
{
	int i;
	unsigned int v;
	int type;
	/* processing stuff: */
	float x, y, xmin, xmax, ymin, ymax;
	model_facemesh_t *mdl;

	/* start reading */
	if (!Sys_Sscanf_s(data, "%d", &type))
		Sys_Error("Sys_LoadModelFaceMesh: \"%s\" truncated\n", path);
	data = Host_CMDSkipBlank(data);

	switch (type)
	{
		case 0:
		case 1:
			/* sprite model */

			mdl = Sys_MemAlloc(&mdl_mem, sizeof(model_facemesh_t), "model");
			mdl->type = MODELTYPE_FACEMESH_TYPE_ID;
			/*
				When the model is oriented looking at -Z (the default), these are the normals:
				0 = XY plane, face normal points toward +Z
				1 = XZ plane, face normal poitns torward +Y TODO TEST
			*/

			/* size */
			if (Sys_Sscanf_s(data, "%f %f", &x, &y) != 2)
				Sys_Error("Sys_LoadModelFaceMesh: \"%s\" truncated\n", path);
			data = Host_CMDSkipBlank(data);

			/* create triangles from scratch */
			mdl->sprite.vert_count = 4;
			mdl->sprite.vert_stride = sizeof(model_vertex_t);
			mdl->sprite.verts = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * mdl->sprite.vert_count, "model");

			xmin = -x/2;
			xmax = x/2;
			ymin = -y/2;
			ymax = y/2;

			/* swap Y for Z based on type */
			mdl->sprite.verts[0].origin[0] = xmax; mdl->sprite.verts[0].origin[1] = (type == 0 ? ymax : 0); mdl->sprite.verts[0].origin[2] = (type == 0 ? 0 : ymin); mdl->sprite.verts[0].texcoord0[0] = 1; mdl->sprite.verts[0].texcoord0[1] = 0;
			mdl->sprite.verts[1].origin[0] = xmin; mdl->sprite.verts[1].origin[1] = (type == 0 ? ymax : 0); mdl->sprite.verts[1].origin[2] = (type == 0 ? 0 : ymin); mdl->sprite.verts[1].texcoord0[0] = 0; mdl->sprite.verts[1].texcoord0[1] = 0;
			mdl->sprite.verts[2].origin[0] = xmax; mdl->sprite.verts[2].origin[1] = (type == 0 ? ymin : 0); mdl->sprite.verts[2].origin[2] = (type == 0 ? 0 : ymax); mdl->sprite.verts[2].texcoord0[0] = 1; mdl->sprite.verts[2].texcoord0[1] = 1;
			mdl->sprite.verts[3].origin[0] = xmin; mdl->sprite.verts[3].origin[1] = (type == 0 ? ymin : 0); mdl->sprite.verts[3].origin[2] = (type == 0 ? 0 : ymax); mdl->sprite.verts[3].texcoord0[0] = 0; mdl->sprite.verts[3].texcoord0[1] = 1;

			mdl->sprite.verts[0].normal[0] = 0; mdl->sprite.verts[0].normal[1] = (type == 0 ? 0 : 1.f); mdl->sprite.verts[0].normal[2] = (type == 0 ? 1.f : 0);
			mdl->sprite.verts[1].normal[0] = 0; mdl->sprite.verts[1].normal[1] = (type == 0 ? 0 : 1.f); mdl->sprite.verts[1].normal[2] = (type == 0 ? 1.f : 0);
			mdl->sprite.verts[2].normal[0] = 0; mdl->sprite.verts[2].normal[1] = (type == 0 ? 0 : 1.f); mdl->sprite.verts[2].normal[2] = (type == 0 ? 1.f : 0);
			mdl->sprite.verts[3].normal[0] = 0; mdl->sprite.verts[3].normal[1] = (type == 0 ? 0 : 1.f); mdl->sprite.verts[3].normal[2] = (type == 0 ? 1.f : 0);

			mdl->sprite.index_count = 6;
			mdl->sprite.index_stride = sizeof(unsigned int) * 3;
			mdl->sprite.indexes = Sys_MemAlloc(&mdl_mem, sizeof(int) * mdl->sprite.index_count * 3, "model");

			/* TODO: do a better order, if necessary? */
			mdl->sprite.indexes[0] = 0; mdl->sprite.indexes[1] = 1; mdl->sprite.indexes[2] = 2;
			mdl->sprite.indexes[3] = 1; mdl->sprite.indexes[4] = 3; mdl->sprite.indexes[5] = 2;

			/* load skins */
			if (!Sys_Sscanf_s(data, "%d", &mdl->num_frames))
				Sys_Error("Sys_LoadModelFaceMesh: \"%s\" truncated\n", path);
			data = Host_CMDSkipBlank(data);

			if (mdl->num_frames > FACEMESH_MAX_FRAMES)
				Sys_Error("Sys_LoadModelFaceMesh: \"%s\" has too many frames (%d, max %d)\n", path, mdl->num_frames, FACEMESH_MAX_FRAMES);

			mdl->texturenames = Sys_MemAlloc(&mdl_mem, sizeof(char *) * mdl->num_frames, "model");
			for (i = 0; i < mdl->num_frames; i++)
			{
				mdl->texturenames[i] = Sys_MemAlloc(&mdl_mem, sizeof(char) * MAX_TEXTURE_NAME, "model");

#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
				if (!Sys_Sscanf_s(data, "%s", mdl->texturenames[i]))
#else
				if (!Sys_Sscanf_s(data, "%s", mdl->texturenames[i], MAX_TEXTURE_NAME))
#endif /* __GNUC__ */
					Sys_Error("Sys_LoadModelFaceMesh: \"%s\" truncated\n", path);
				data = Host_CMDSkipBlank(data);
			}

			/* load animation info */
			{
				int lowmark = Sys_MemLowMark(&tmp_mem);

				char filename[MAX_PATH];
				unsigned char *buffer, *bufferend;
				int buffersize;

				char animation_name[MAX_GAME_STRING];
				unsigned int start_frame;
				unsigned int num_frames;
				int loop;
				vec_t frames_per_second;

				/* per line: name startframe numframes loop0or1 fps */
				Sys_Snprintf(filename, MAX_PATH, "%s_animations.txt", path);
				if ((buffersize = Host_FSLoadBinaryFile(filename, &tmp_mem, "model", &buffer, true)) != -1)
				{
					int bufferlines = 0;
					bufferend = buffer + buffersize;
					while (1)
					{
						if (buffer >= bufferend)
							break;
#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
						if (!Sys_Sscanf_s((const char *)buffer, "%s %u %u %d %f", animation_name, &start_frame, &num_frames, &loop, &frames_per_second))
#else
						if (!Sys_Sscanf_s((const char *)buffer, "%s %u %u %d %f", animation_name, sizeof(animation_name), &start_frame, &num_frames, &loop, &frames_per_second))
#endif /* __GNUC__ */
							break;
						buffer = Host_CMDSkipBlank(buffer);
						bufferlines++;

						if (loop != 0 && loop != 1)
						{
							Host_Error("%s: property loop = %d is out of range (should be zero or 1) name = \"%s\"\n at line %d\n", filename, loop, animation_name, bufferlines); /* TODO: line counting NOT accurate */
						}
						for (i = 0; i < NUM_ANIMATIONS; i++)
						{
							if (!strncmp(animation_name, animation_names[i], MAX_GAME_STRING))
							{
								if (mdl->animations[i].exists)
									Host_Error("%s: animation name = \"%s\" redefined at line %d\n", filename, animation_name, bufferlines); /* TODO: line counting NOT accurate */
								mdl->animations[i].exists = true;
								mdl->animations[i].start_frame = start_frame;
								mdl->animations[i].num_frames = num_frames;
								mdl->animations[i].loop = loop;
								mdl->animations[i].frames_per_second = frames_per_second;
								break;
							}
						}
					}
				}

				Sys_MemFreeToLowMark(&tmp_mem, lowmark);
			}
			break;
		default:
			Sys_Error("Sys_LoadModelFaceMesh: \"%s\": unknown type %d\n", path, type);
	}

	Math_ClearVector3(mdl->mins);
	Math_ClearVector3(mdl->maxs);
	for (v = 0; v < mdl->sprite.vert_count; v++)
	{
		mdl->mins[0] = Math_Min(mdl->mins[0], mdl->sprite.verts[v].origin[0]);
		mdl->mins[1] = Math_Min(mdl->mins[1], mdl->sprite.verts[v].origin[1]);
		mdl->mins[2] = Math_Min(mdl->mins[2], mdl->sprite.verts[v].origin[2]);
		mdl->maxs[0] = Math_Max(mdl->maxs[0], mdl->sprite.verts[v].origin[0]);
		mdl->maxs[1] = Math_Max(mdl->maxs[1], mdl->sprite.verts[v].origin[1]);
		mdl->maxs[2] = Math_Max(mdl->maxs[2], mdl->sprite.verts[v].origin[2]);
	}

	Sys_Snprintf(mdl->name, MAX_MODEL_NAME, "%s", name);

	/* TODO CONSOLEDEBUG Sys_Printf("Loaded %s\n", path); */

	return mdl;
}

/*
===================
Sys_LoadModelClientDataFaceMesh

Should only be called by Sys_LoadModelClientData, passing
model_t->data to load any textures, VBOs, etc that
the model uses.
===================
*/
void Sys_LoadModelClientDataFaceMesh(model_facemesh_t *mdl)
{
	int i;

	mdl->textures = Sys_MemAlloc(&mdl_mem, sizeof(texture_t *) * mdl->num_frames, "model");
	for (i = 0; i < mdl->num_frames; i++)
	{
		/* TODO FIXME: Sys Calling CL? */
		mdl->textures[i] = CL_LoadTexture(mdl->texturenames[i], false, NULL, 0, 0, false, 1, 1);
	}

	mdl->vbo_id = Sys_UploadVBO(-1, &mdl->sprite, false, false);
}

/*
===================
Sys_ModelAnimationInfoFaceMesh

Should only be called by Sys_ModelAnimationInfo
===================
*/
void Sys_ModelAnimationInfoFaceMesh(model_facemesh_t *model, const unsigned int animation, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation)
{
	if (animation < 0 || animation >= NUM_ANIMATIONS)
		Sys_Error("Sys_ModelAnimationInfoFaceMesh: animation %d out of range in model %s\n", animation, model->name);

	if (!model->animations[animation].exists)
		Sys_Error("Sys_ModelAnimationInfoFaceMesh: animation %d not found in model %s\n", animation, model->name);

	if (start_frame)
		*start_frame = model->animations[animation].start_frame;
	if (num_frames)
		*num_frames = model->animations[animation].num_frames;
	if (loop)
		*loop = model->animations[animation].loop;
	if (frames_per_second)
		*frames_per_second = model->animations[animation].frames_per_second;
	if (multiple_slots)
		*multiple_slots = false;
	if (vertex_animation)
		*vertex_animation = true; /* get treated as if it was a vertex animation */
}

/*
===================
Sys_ModelAnimationExistsFaceMesh

Should only be called by Sys_ModelAnimationExists
===================
*/
int Sys_ModelAnimationExistsFaceMesh(model_facemesh_t *model, const unsigned int animation)
{
	if (animation < 0 || animation >= NUM_ANIMATIONS)
		Sys_Error("Sys_ModelAnimationExistsFaceMesh: animation %d out of range in model %s\n", animation, model->name);

	return model->animations[animation].exists;
}

/*
===================
Sys_ModelAABBFaceMesh

Should only be called by Sys_ModelAABB
TODO: test
===================
*/
void Sys_ModelAABBFaceMesh(model_facemesh_t *mdl, vec3_t mins, vec3_t maxs)
{
	Math_Vector3Copy(mdl->mins, mins);
	Math_Vector3Copy(mdl->maxs, maxs);
}

/*
===================
Sys_VideoDraw3DModelFaceMesh

Should only be called by Sys_VideoDraw3DModel
"frame[0]" is the sprite frame
===================
*/
#if (ANIMATION_SLOT_ALLJOINTS != 0)
#error "ANIMATION_SLOT_ALLJOINTS should be == 0"
#endif
int Sys_VideoDraw3DModelFaceMesh(model_facemesh_t *mdl, vec_t *frames, int modelent, vec3_t origin, vec3_t angles, unsigned int desired_shader)
{
	vec_t ent_modelmatrix[16];
	int frame_integer = (int)frames[0]; /* TODO: round this or interpolate vertex data */

	if (frame_integer < 0 || frame_integer >= mdl->num_frames)
		Sys_Error("Sys_VideoDraw3DModelFaceMesh: %s: frame %f is out of bounds", mdl->name, frames[0]);

	Math_MatrixModel4x4(ent_modelmatrix, origin, angles, NULL);
	Sys_VideoTransformFor3DModel(ent_modelmatrix);
	Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL); /* TODO: make possible to use fixed lights? */
	return Sys_VideoDrawVBO(mdl->vbo_id, mdl->textures[frame_integer]->cl_id, -1, -1, -1, -1, -1, -1, -1, -1);
}

/*
============================================================================

Model type: Quake 3 BSP

Please see a Quake 3 BSP spec for more details.
There ARE differences between getting trimeshes and brushes for a Quake 3 BSP
Trimeshes are for rendering and brushes are for collision. For example, "clip"
brushes will be compiled to the brushes list and not the trimeshes list,
creating an invisible wall.

TODO: complete implementation: areaportals, etc. Do this
by checking distance to nearest planes in the tree?
TODO: check key-value pairs that are used during map compilation (we
may use them wrongly for other purposes in the game)
FIXME: should Q3BSP_FACETYPE_POLYGON and Q3BSP_FACETYPE_MESH really be
treated equally?
FIXME: lots of info missing, can we use q3map2 GPL headers?
TODO: lots of places I should've used epsilons, specially regarding plane distances
TODO: The hull expansion (increased distance to plane) done in various places (volume tracing, fat pvs) is wrong? I think it only works right for 90-degree plane intersections. This way we can implement the mins/maxs as a cube in which the expansion works, then clip against it too. Probably wouldn't be completely right but would be a better aproximation.
TODO: relation between absmin/absmax, mins/maxs and origin on brushes WHICH HAVE ORIGIN! (rotating brushes for example)

Quake coordinate system:

1/32 scale

X = -Y
Y = Z
Z = -X

I think:
Faces = information from outside leafs (convex volumes)
Brushes = information from inside leafs (convex volumes)
Is this correct? And how does "Brushes" store information from each of it's brushplanes? (The textures, which contain the flags) Does it only store if they are all the same?
And is it possible to have more than one brush inside a leaf? Maybe only if the contents are the same? This is important for PointContents!
The [mins],[maxs] pairs can cause problem after swizzling, because we negate the individual vec3_t, in turn having to consider the
entire pair and swapping the components that we inverted between mins and maxs.


============================================================================
*/

#define Q3BSP_MAGIC					('P' << 24 | 'S' << 16 | 'B' << 8 | 'I') /* little endian */
#define Q3BSP_VERSION				0x2e
#define Q3BSP_NUM_DIRENTRIES		17
#define Q3BSP_DIRENTRY_ENTITIES		0
#define Q3BSP_DIRENTRY_TEXTURES		1
#define Q3BSP_DIRENTRY_PLANES		2
#define Q3BSP_DIRENTRY_NODES		3
#define Q3BSP_DIRENTRY_LEAFS		4
#define Q3BSP_DIRENTRY_LEAFFACES	5
#define Q3BSP_DIRENTRY_LEAFBRUSHES	6
#define Q3BSP_DIRENTRY_MODELS		7
#define Q3BSP_DIRENTRY_BRUSHES		8
#define Q3BSP_DIRENTRY_BRUSHSIDES	9
#define Q3BSP_DIRENTRY_VERTEXES		10
#define Q3BSP_DIRENTRY_MESHVERTS	11
#define Q3BSP_DIRENTRY_EFFECTS		12
#define Q3BSP_DIRENTRY_FACES		13
#define Q3BSP_DIRENTRY_LIGHTMAPS	14
#define Q3BSP_DIRENTRY_LIGHTVOLS	15
#define Q3BSP_DIRENTRY_VISDATA		16

#define	Q3BSP_MAX_LEAFS				0x20000
#define	Q3BSP_MAX_LEAFFACES			0x20000
#define Q3BSP_MAX_VISDATASIZE		0x200000

#define Q3BSP_LIGHTMAPWIDTH			128
#define Q3BSP_LIGHTMAPHEIGHT		128

 /* TODO: check if the grid size is altays 64x64x128, swizzle this data */
#define Q3BSP_LIGHTVOL_GRIDSIZE_X	64.f
#define Q3BSP_LIGHTVOL_GRIDSIZE_Y	64.f
#define Q3BSP_LIGHTVOL_GRIDSIZE_Z	128.f

#define Q3BSP_FACETYPE_POLYGON		1
#define Q3BSP_FACETYPE_PATCH		2
#define Q3BSP_FACETYPE_MESH			3
#define Q3BSP_FACETYPE_BILLBOARD	4

/* TODO: add the rest and check EVERYWHERE we will need them */
#define Q3BSP_CONTENTS_SOLID		1
#define Q3BSP_CONTENTS_WATER		(1 << 5) /* TODO: surface warp, underwater camera warp */
#define Q3BSP_CONTENTS_DETAIL		(1 << 27) /* TODO: is this ever set in the bsp file? */

/* TODO: add the rest and check EVERYWHERE we will need them */
#define Q3BSP_SURF_NONSOLID			(1 << 14)

/* TODO: we are ignoring this in lots of places. Make this the epsilon for the entire engine? */
#define Q3BSP_DIST_EPSILON			(1.f/1024.f)

#define Q3BSP_NODE_FRONTSIDE		0
#define Q3BSP_NODE_BACKSIDE			1

typedef struct q3bsp_direntry_entities_s {
	char	ents; /* size is the lump size itself */
} q3bsp_direntry_entities_t;

typedef struct q3bsp_direntry_textures_s {
	char	name[64];
	int		flags; /* TODO: use SURF_* */
	int		contents; /* TODO: use CONTENTS_*, need to be unsigned int? */
} q3bsp_direntry_textures_t;

typedef struct q3bsp_direntry_planes_s {
	/* planes are paired, the pair of planes with indices i and i ^ 1 are coincident planes with opposing normals. */
	vec3_t	normal;
	vec_t	dist;
} q3bsp_direntry_planes_t;

typedef struct q3bsp_direntry_nodes_s {
	/* the first node is the root node for the bsp tree */
	int		plane;
	int		children[2]; /* negative numbers are leaf indices: -(leaf + 1) */
	int		mins[3];
	int		maxs[3];
} q3bsp_direntry_nodes_t;

typedef struct q3bsp_direntry_leafs_s {
	int		cluster; /* for PVS. If negative, outside map or invalid */
	int		area;	/* areaportal */
	int		mins[3];
	int		maxs[3];
	int		leafface;
	int		n_leaffaces;
	int		leafbrush;
	int		n_leafbrushes;
} q3bsp_direntry_leafs_t;

typedef struct q3bsp_direntry_leaffaces_s {
	int		face;
} q3bsp_direntry_leaffaces_t;

typedef struct q3bsp_direntry_leafbrushes_s {
	int		brush;
} q3bsp_direntry_leafbrushes_t;

typedef struct q3bsp_direntry_models_s {
	/* only the first one has a bsp tree, the others are movers/etc */
	vec3_t	mins;
	vec3_t	maxs;
	int		face;
	int		n_faces;
	int		brush;
	int		n_brushes;
} q3bsp_direntry_models_t;

typedef struct q3bsp_direntry_brushes_s {
	int		brushside;
	int		n_brushsides;
	int		texture;
} q3bsp_direntry_brushes_t;

typedef struct q3bsp_direntry_brushsides_s {
	int		plane;
	int		texture;
} q3bsp_direntry_brushsides_t;

typedef struct q3bsp_direntry_vertexes_s {
	vec3_t origin;
	vec2_t texcoord0; /* two textures per face */
	vec2_t texcoord1;
	vec3_t normal;
	unsigned char color[4]; /* RGBA */
} q3bsp_direntry_vertexes_t;

typedef struct q3bsp_direntry_meshverts_s {
	int		offset;
} q3bsp_direntry_meshverts_t;

typedef struct q3bsp_direntry_effects_s {
	/* FIXME: this entry makes no sense, look this up! */
	char	name[64];
	int		brush;
	int		unknown; /* TODO: look this up */
} q3bsp_direntry_effects_t;

typedef struct q3bsp_direntry_faces_s {
	int		texture;
	int		effect; /* -1 = no effect */
	int		type; /* 1 = polygon, 2 = patch, 3 = mesh, 4 = billboard */
	int		vertex; /* first vertex in the vertex array */
	int		n_vertexes;
	int		meshvert; /* first meshvert (index) in the meshvert array */
	int		n_meshverts;
	int		lm_index; /* lightmap */
	int		lm_start[2];
	int		lm_size[2];
	vec3_t	lm_origin;
	vec3_t	lm_vecs0;
	vec3_t	lm_vecs1;
	vec3_t	normal;
	int		size[2]; /* patch dimensions */

	/*
		Excerpt from a unofficial spec:

		Several components have different meanings depending on the face type.

		For type 1 faces (polygons), vertex and n_vertexes describe a set of vertices that form a polygon.
		The set always contains a loop of vertices, and sometimes also includes an additional vertex near
		the center of the polygon. For these faces, meshvert and n_meshverts describe a valid polygon
		triangulation. Every three meshverts describe a triangle. Each meshvert is an offset from the first
		vertex of the face, given by vertex.

		For type 2 faces (patches), vertex and n_vertexes describe a 2D rectangular grid of control vertices
		with dimensions given by size. Within this rectangular grid, regions of 3x3 vertices represent
		biquadratic Bezier patches. Adjacent patches share a line of three vertices. There are a total of
		(size[0] - 1) / 2 by (size[1] - 1) / 2 patches. Patches in the grid start at (i, j) given by:

			i = 2n, n in [ 0 .. (size[0] - 1) / 2 ), and
			j = 2m, m in [ 0 .. (size[1] - 1) / 2 ).

		For type 3 faces (meshes), meshvert and n_meshverts are used to describe the independent triangles
		that form the mesh. As with type 1 faces, every three meshverts describe a triangle, and each
		meshvert is an offset from the first vertex of the face, given by vertex.

		For type 4 faces (billboards), vertex describes the single vertex that determines the location of
		the billboard. Billboards are used for effects such as flares. Exactly how each billboard vertex is
		to be interpreted has not been investigated.

		The lm_ variables are primarily used to deal with lightmap data. A face that has a lightmap has a
		non-negative lm_index. For such a face, lm_index is the index of the image in the lightmaps lump
		that contains the lighting data for the face. The data in the lightmap image can be located using
		the rectangle specified by lm_start and lm_size.

		For type 1 faces (polygons) only, lm_origin and lm_vecs can be used to compute the world-space
		positions corresponding to lightmap samples. These positions can in turn be used to compute dynamic
		lighting across the face.

		None of the lm_ variables are used to compute texture coordinates for indexing into lightmaps.
		In fact, lightmap coordinates need not be computed. Instead, lightmap coordinates are simply stored
		with the vertices used to describe each face.
	*/
} q3bsp_direntry_faces_t;

typedef struct q3bsp_direntry_lightmaps_s {
	unsigned char	map[Q3BSP_LIGHTMAPWIDTH][Q3BSP_LIGHTMAPHEIGHT][3]; /* RGB */
} q3bsp_direntry_lightmaps_t;

typedef struct q3bsp_direntry_lightvols_s {
	unsigned char	ambient[3]; /* RGB */
	unsigned char	directional[3]; /* RGB */
	unsigned char	dir[2]; /* 0 = phi, 1 = theta */
	/*
		Excerpt from a unofficial spec:

		The lightvols lump stores a uniform grid of lighting information used to illuminate non-map objects.

		Lightvols make up a 3D grid whose dimensions are:

		nx = floor(models[0].maxs[0] / 64) - ceil(models[0].mins[0] / 64) + 1
		ny = floor(models[0].maxs[1] / 64) - ceil(models[0].mins[1] / 64) + 1
		nz = floor(models[0].maxs[2] / 128) - ceil(models[0].mins[2] / 128) + 1

		TODO: can lightgrids be used to correctly light moving submodels? lightmaps are static, you know
	*/
} q3bsp_direntry_lightvols_t;

typedef struct q3bsp_direntry_visdata_s {
	int				n_vecs; /* number of vectors */
	int				sz_vecs; /* size of each vector, in bytes */
	unsigned char	vecs; /* visibility data, one bit per cluster per vector. Size: n_vecs * sz_vecs */

	/*
		Excerpt from a unofficial spec:

		The visdata lump stores bit vectors that provide cluster-to-cluster visibility information.
		There is exactly one visdata record, with a length equal to that specified in the lump directory.

		Cluster x is visible from cluster y if the (1 << y % 8) bit of vecs[x * sz_vecs + y / 8] is set.

		Note that clusters are associated with leaves.
	*/
} q3bsp_direntry_visdata_t;

typedef struct q3bsp_direntry_s {
	int		offset;
	int		length;
} q3bsp_direntry_t;

typedef struct q3bsp_header_s {
	char				magic[4];
	int					version;
	q3bsp_direntry_t	direntries[Q3BSP_NUM_DIRENTRIES];
} q3bsp_header_t;

#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
typedef struct q3bsp_renderbatch_s {
	int								numbatches;
	int								*batchstart;
	int								*batchcount;
	int								*batchfirstvertex;
	int								*batchlastvertex;
	int								*batchlightmap;
	int								*batchtexture;
} q3bsp_renderbatch_t;
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */

typedef struct q3bsp_model_s {
	char							magic[4];

	q3bsp_header_t					*header;

	q3bsp_direntry_entities_t		*entities;
	q3bsp_direntry_textures_t		*textures;
	q3bsp_direntry_planes_t			*planes;
	q3bsp_direntry_nodes_t			*nodes;
	q3bsp_direntry_leafs_t			*leafs;
	q3bsp_direntry_leaffaces_t		*leaffaces;
	q3bsp_direntry_leafbrushes_t	*leafbrushes;
	q3bsp_direntry_models_t			*models;
	q3bsp_direntry_brushes_t		*brushes;
	q3bsp_direntry_brushsides_t		*brushsides;
	q3bsp_direntry_vertexes_t		*vertexes;
	q3bsp_direntry_meshverts_t		*meshverts;
	q3bsp_direntry_effects_t		*effects;
	q3bsp_direntry_faces_t			*faces;
	q3bsp_direntry_lightmaps_t		*lightmaps;
	q3bsp_direntry_lightvols_t		*lightvols;
	q3bsp_direntry_visdata_t		*visdata;

	int								entitiessize;
	int								numtextures;
	int								numplanes;
	int								numnodes;
	int								numleafs;
	int								numleaffaces;
	int								numleafbrushes;
	int								nummodels;
	int								numbrushes;
	int								numbrushsides;
	int								numvertexes;
	int								nummeshverts;
	int								numeffects;
	int								numfaces;
	int								numlightmaps;
	int								numlightvols;
	int								visdatasize;

	char							name[MAX_MODEL_NAME];

	/* same index as textures */
	texture_t						**texture_ids;
	/* same index as lightmaps */
	texture_t						**lightmap_ids;

	int								vbo_id;

	int								num_lightvols_x;
	int								num_lightvols_y;
	int								num_lightvols_z;

	/* used when rendering */
#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
	q3bsp_renderbatch_t				*renderbatchs; /* one for each model */
#else /* BRUTEFORCE_BATCH_QUAKE3BSP */
	/* same index as faces */
	int								*face_ids;
	int								maxviscluster;
	/* FIXME: not being reentrant is the lesser problem of these globals */
	/* TODO: using current frame number to avoid clearing these arrays. Zero reserved for never-seen leafs and faces */
	framenum_t						*visiblenodesarray;
	framenum_t						*visibleleafsarray;
	int								numvisiblefaces;
	int								*visiblefacesstack; /* FIXME: what is the value of MAX_FACES? not MAX_LEAFFACES, possible overflow */
	framenum_t						*visiblefacesarray; /* FIXME: what is the value of MAX_FACES? not MAX_LEAFFACES, possible overflow */
	framenum_t						cache_frame;
	int								cache_cluster; /* TODO: anything more we want to cache? */
	/* reconstructed for reverse traversal */
	int								*nodeparent;
	int								*leafparent; /* is a node */
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */

	/* for Sys_LoadModelPhysicsBrushes */
	model_brushes_t					**brush_cache;
	/* for Sys_LoadModelPhysicsTrimesh */
	model_trimesh_t					**trimesh_cache;
} q3bsp_model_t;

/*
===================
Sys_LoadModelQuake3BSPVec3SwizzleMinsMaxs

Fix [mins],[maxs] after swizzling, the reason is above in the general comments about Quake 3 BSP
===================
*/
void Sys_LoadModelQuake3BSPVec3SwizzleMinsMaxs(vec3_t mins, vec3_t maxs)
{
	int i;
	vec_t tmp;

	/* TODO: see what really needs swizzling */
	for (i = 0; i < 3; i++)
	{
		if (mins[i] > maxs[i])
		{
			tmp = mins[i];
			mins[i] = maxs[i];
			maxs[i] = tmp;
		}
	}
}

/*
===================
Sys_LoadModelQuake3BSPVec3SwizzleMinsMaxsInt

Fix [mins],[maxs] after swizzling, the reason is above in the general comments about Quake 3 BSP
===================
*/
void Sys_LoadModelQuake3BSPVec3SwizzleMinsMaxsInt(int *mins, int *maxs)
{
	int i;
	int tmp;

	/* TODO: see what really needs swizzling */
	for (i = 0; i < 3; i++)
	{
		if (mins[i] > maxs[i])
		{
			tmp = mins[i];
			mins[i] = maxs[i];
			maxs[i] = tmp;
		}
	}
}

/*
===================
Sys_LoadModelQuake3BSPVec3Swizzle

Converts from the Quake coordinate system
===================
*/
void Sys_LoadModelQuake3BSPVec3Swizzle(vec3_t input)
{
	vec_t tmp;

	tmp = input[1];
	input[1] = input[2];
	input[2] = -tmp;

	tmp = input[0];
	input[0] = input[2];
	input[2] = -tmp;
}

/*
===================
Sys_LoadModelQuake3BSPVec3SwizzleInt

Converts from the Quake coordinate system
===================
*/
void Sys_LoadModelQuake3BSPVec3SwizzleInt(int *input)
{
	int tmp;

	tmp = input[1];
	input[1] = input[2];
	input[2] = -tmp;

	tmp = input[0];
	input[0] = input[2];
	input[2] = -tmp;
}

/*
===================
Sys_LoadModelQuake3BSPVec3Scale

1 unit = 32 Q3BSP units
===================
*/
void Sys_LoadModelQuake3BSPVec3Scale(vec3_t input)
{
	input[0] /= 32.f;
	input[1] /= 32.f;
	input[2] /= 32.f;
}

/*
===================
Sys_LoadModelQuake3BSPVec3ScaleIntCeil

1 unit = 32 Q3BSP units
===================
*/
void Sys_LoadModelQuake3BSPVec3ScaleIntCeil(int *input)
{
	/* TODO FIXME: loss of precision? make this a float? */
	input[0] = (int)ceil((vec_t)input[0] / 32.f);
	input[1] = (int)ceil((vec_t)input[1] / 32.f);
	input[2] = (int)ceil((vec_t)input[2] / 32.f);
}

/*
===================
Sys_LoadModelQuake3BSPVec3ScaleIntFloor

1 unit = 32 Q3BSP units
===================
*/
void Sys_LoadModelQuake3BSPVec3ScaleIntFloor(int *input)
{
	/* TODO FIXME: loss of precision? make this a float? */
	input[0] = (int)floor((vec_t)input[0] / 32.f);
	input[1] = (int)floor((vec_t)input[1] / 32.f);
	input[2] = (int)floor((vec_t)input[2] / 32.f);
}

/*
===================
Sys_LoadModelQuake3BSPVecScale

1 unit = 32 Q3BSP units
===================
*/
void Sys_LoadModelQuake3BSPVecScale(vec_t *input)
{
	input[0] /= 32.f;
}

/*
===================
Sys_LoadModelQuake3BSP

Should only be called by Sys_LoadModel.
Returns data pointer for the model data
===================
*/
void *Sys_LoadModelQuake3BSP(const char *name, char *path, unsigned char *data, int size)
{
	int i;
	int temp;

	q3bsp_model_t	*bspmodel = Sys_MemAlloc(&mdl_mem, sizeof(q3bsp_model_t), "model");

	/* TODO: whatever wasn't used in this code isn't indexed below!! */
	bspmodel->header = (q3bsp_header_t *)data;
	bspmodel->planes = (q3bsp_direntry_planes_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_PLANES].offset);
	bspmodel->nodes = (q3bsp_direntry_nodes_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_NODES].offset);
	bspmodel->leafs = (q3bsp_direntry_leafs_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_LEAFS].offset);
	bspmodel->models = (q3bsp_direntry_models_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_MODELS].offset);
	bspmodel->vertexes = (q3bsp_direntry_vertexes_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_VERTEXES].offset);
	bspmodel->faces = (q3bsp_direntry_faces_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_FACES].offset);
	bspmodel->meshverts = (q3bsp_direntry_meshverts_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_MESHVERTS].offset);
	bspmodel->textures = (q3bsp_direntry_textures_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_TEXTURES].offset);
	bspmodel->lightmaps = (q3bsp_direntry_lightmaps_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_LIGHTMAPS].offset);
	bspmodel->entities = (q3bsp_direntry_entities_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_ENTITIES].offset);
	bspmodel->brushes = (q3bsp_direntry_brushes_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_BRUSHES].offset);
	bspmodel->brushsides = (q3bsp_direntry_brushsides_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_BRUSHSIDES].offset);
	bspmodel->visdata = (q3bsp_direntry_visdata_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_VISDATA].offset);
	bspmodel->leaffaces = (q3bsp_direntry_leaffaces_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_LEAFFACES].offset);
	bspmodel->leafbrushes = (q3bsp_direntry_leafbrushes_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_LEAFBRUSHES].offset);
	bspmodel->effects = (q3bsp_direntry_effects_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_EFFECTS].offset);
	bspmodel->lightvols = (q3bsp_direntry_lightvols_t *)((char *)data + bspmodel->header->direntries[Q3BSP_DIRENTRY_LIGHTVOLS].offset);

	/* TODO: whatever wasn't used in this code isn't indexed below!! */
	bspmodel->nummeshverts = bspmodel->header->direntries[Q3BSP_DIRENTRY_MESHVERTS].length / sizeof(q3bsp_direntry_meshverts_t);
	bspmodel->numplanes = bspmodel->header->direntries[Q3BSP_DIRENTRY_PLANES].length / sizeof(q3bsp_direntry_planes_t);
	bspmodel->numnodes = bspmodel->header->direntries[Q3BSP_DIRENTRY_NODES].length / sizeof(q3bsp_direntry_nodes_t);
	bspmodel->numleafs = bspmodel->header->direntries[Q3BSP_DIRENTRY_LEAFS].length / sizeof(q3bsp_direntry_leafs_t); /* FIXME: this seems to include too much leafs (leafs not in the bsp tree: for submodels, etc...) */
	bspmodel->nummodels = bspmodel->header->direntries[Q3BSP_DIRENTRY_MODELS].length / sizeof(q3bsp_direntry_models_t);
	bspmodel->numvertexes = bspmodel->header->direntries[Q3BSP_DIRENTRY_VERTEXES].length / sizeof(q3bsp_direntry_vertexes_t);
	bspmodel->numfaces = bspmodel->header->direntries[Q3BSP_DIRENTRY_FACES].length / sizeof(q3bsp_direntry_faces_t);
	bspmodel->numtextures = bspmodel->header->direntries[Q3BSP_DIRENTRY_TEXTURES].length / sizeof(q3bsp_direntry_textures_t);
	bspmodel->numlightmaps = bspmodel->header->direntries[Q3BSP_DIRENTRY_LIGHTMAPS].length / sizeof(q3bsp_direntry_lightmaps_t);
	bspmodel->entitiessize = bspmodel->header->direntries[Q3BSP_DIRENTRY_ENTITIES].length / sizeof(q3bsp_direntry_entities_t);
	bspmodel->nummodels = bspmodel->header->direntries[Q3BSP_DIRENTRY_MODELS].length / sizeof(q3bsp_direntry_models_t);
	bspmodel->visdatasize = bspmodel->header->direntries[Q3BSP_DIRENTRY_VISDATA].length;
	bspmodel->numlightvols = bspmodel->header->direntries[Q3BSP_DIRENTRY_LIGHTVOLS].length / sizeof(q3bsp_direntry_lightvols_t);

	/* TODO: deal with endianess */
	if (*((int *)bspmodel->header->magic) != Q3BSP_MAGIC)
		Sys_Error("Sys_LoadModelQuake3BSP: %s has unknown BSP type %c%c%c%c\n", path, bspmodel->header->magic[0], bspmodel->header->magic[1], bspmodel->header->magic[2], bspmodel->header->magic[3]);

	/* copy for future identification */
	memcpy(bspmodel->magic, bspmodel->header->magic, 4);

	if (bspmodel->header->version != Q3BSP_VERSION)
		Sys_Error("Sys_LoadModelQuake3BSP: %s has unknown version %d\n", path, bspmodel->header->version);

	for (i = 0; i < Q3BSP_NUM_DIRENTRIES; i++)
	{
		if (i && bspmodel->header->direntries[i].length % 4) /* ignore the entity lump */ /* FIXME: check if (length % direntrysizeof) too */
			Sys_Error("Sys_LoadModelQuake3BSP: %s has a funny lump %d size: %d\n", path, i, bspmodel->header->direntries[i].length);
		if (bspmodel->header->direntries[i].offset + bspmodel->header->direntries[i].length > size)
			Sys_Error("Sys_LoadModelQuake3BSP: %s truncated\n", path);
	}

	bspmodel->texture_ids = NULL;
	bspmodel->lightmap_ids = NULL;
	bspmodel->vbo_id = -1;
#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
	bspmodel->face_ids = NULL;
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
	/* TODO: swizzle this lightvol data */
	/* calculating values before swizzling the AABBs */
	bspmodel->num_lightvols_x = (int)floor(bspmodel->models[0].maxs[0] / Q3BSP_LIGHTVOL_GRIDSIZE_X) - (int)ceil(bspmodel->models[0].mins[0] / Q3BSP_LIGHTVOL_GRIDSIZE_X) + 1;
	bspmodel->num_lightvols_y = (int)floor(bspmodel->models[0].maxs[1] / Q3BSP_LIGHTVOL_GRIDSIZE_Y) - (int)ceil(bspmodel->models[0].mins[1] / Q3BSP_LIGHTVOL_GRIDSIZE_Y) + 1;
	bspmodel->num_lightvols_z = (int)floor(bspmodel->models[0].maxs[2] / Q3BSP_LIGHTVOL_GRIDSIZE_Z) - (int)ceil(bspmodel->models[0].mins[2] / Q3BSP_LIGHTVOL_GRIDSIZE_Z) + 1 ;

	if (bspmodel->numlightvols && (bspmodel->numlightvols != bspmodel->num_lightvols_x * bspmodel->num_lightvols_y * bspmodel->num_lightvols_z))
		Sys_Error("Sys_LoadModelQuake3BSP: %s has %d lightvols but must have %d\n", path, bspmodel->numlightvols, bspmodel->num_lightvols_x * bspmodel->num_lightvols_y * bspmodel->num_lightvols_z);

	/* as always, assume we get zero-filled memory */
	bspmodel->brush_cache = Sys_MemAlloc(&mdl_mem, sizeof(model_brushes_t *) * bspmodel->nummodels, "modelbrushes");
	bspmodel->trimesh_cache = Sys_MemAlloc(&mdl_mem, sizeof(model_trimesh_t *) * bspmodel->nummodels, "modeltrimesh");

	/* TODO: q3map2 lies about world mins/maxs because of lightgrid? */
	/* Convert the coordinate system PART 1/2, be careful about this when using this format! */
	/* Quake has a strange inconsistency: coords indexes are swizzled but angles indexes are not! FIXME? */
	/* TODO: what about lightmaps and lightvols? Do they need swizzling? */
	/* TODO: use lightvols for entities */
	for (i = 0; i < bspmodel->numplanes; i++)
	{
		Sys_LoadModelQuake3BSPVec3Swizzle(bspmodel->planes[i].normal);
		Sys_LoadModelQuake3BSPVecScale(&bspmodel->planes[i].dist);
	}
	for (i = 0; i < bspmodel->numnodes; i++)
	{
		Sys_LoadModelQuake3BSPVec3SwizzleInt(bspmodel->nodes[i].mins);
		Sys_LoadModelQuake3BSPVec3SwizzleInt(bspmodel->nodes[i].maxs);
		Sys_LoadModelQuake3BSPVec3SwizzleMinsMaxsInt(bspmodel->nodes[i].mins, bspmodel->nodes[i].maxs);
		/* scale later because some of the mins/maxs have a different meaning now we don't want to do a ceil/floor on the wrong value */
		Sys_LoadModelQuake3BSPVec3ScaleIntFloor(bspmodel->nodes[i].mins);
		Sys_LoadModelQuake3BSPVec3ScaleIntCeil(bspmodel->nodes[i].maxs);
	}
#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
	for (i = 0; i < bspmodel->numleafs; i++)
#else /* BRUTEFORCE_BATCH_QUAKE3BSP */
	for (i = 0, bspmodel->maxviscluster = 0; i < bspmodel->numleafs; i++)
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP  */
	{
		Sys_LoadModelQuake3BSPVec3SwizzleInt(bspmodel->leafs[i].mins);
		Sys_LoadModelQuake3BSPVec3SwizzleInt(bspmodel->leafs[i].maxs);
		Sys_LoadModelQuake3BSPVec3SwizzleMinsMaxsInt(bspmodel->leafs[i].mins, bspmodel->leafs[i].maxs);
		Sys_LoadModelQuake3BSPVec3ScaleIntFloor(bspmodel->leafs[i].mins);
		Sys_LoadModelQuake3BSPVec3ScaleIntCeil(bspmodel->leafs[i].maxs);
#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
		if (bspmodel->leafs[i].cluster > bspmodel->maxviscluster)
			bspmodel->maxviscluster = bspmodel->leafs[i].cluster;
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
	}
	for (i = 0; i < bspmodel->nummodels; i++)
	{
		Sys_LoadModelQuake3BSPVec3Swizzle(bspmodel->models[i].mins);
		Sys_LoadModelQuake3BSPVec3Swizzle(bspmodel->models[i].maxs);
		Sys_LoadModelQuake3BSPVec3SwizzleMinsMaxs(bspmodel->models[i].mins, bspmodel->models[i].maxs);
		Sys_LoadModelQuake3BSPVec3Scale(bspmodel->models[i].mins);
		Sys_LoadModelQuake3BSPVec3Scale(bspmodel->models[i].maxs);
	}
	for (i = 0; i < bspmodel->numvertexes; i++)
	{
		Sys_LoadModelQuake3BSPVec3Swizzle(bspmodel->vertexes[i].normal);
		Sys_LoadModelQuake3BSPVec3Swizzle(bspmodel->vertexes[i].origin);
		Sys_LoadModelQuake3BSPVec3Scale(bspmodel->vertexes[i].origin);
	}
	for (i = 0; i < bspmodel->numfaces; i++)
	{
		Sys_LoadModelQuake3BSPVec3Swizzle(bspmodel->faces[i].lm_origin);
		Sys_LoadModelQuake3BSPVec3Swizzle(bspmodel->faces[i].lm_vecs0);
		Sys_LoadModelQuake3BSPVec3Swizzle(bspmodel->faces[i].lm_vecs1);
		Sys_LoadModelQuake3BSPVec3Swizzle(bspmodel->faces[i].normal);
		Sys_LoadModelQuake3BSPVec3Scale(bspmodel->faces[i].lm_origin);
	}
	/* invert triangle order (different handedness) TODO: why do I need to do this? */
	for (i = 0; i < bspmodel->nummeshverts; i += 3)
	{
		temp = bspmodel->meshverts[i].offset;
		bspmodel->meshverts[i].offset = bspmodel->meshverts[i + 2].offset;
		bspmodel->meshverts[i + 2].offset = temp;
	}

	/* TODO: > or >= ? */
	if (bspmodel->numleafs > Q3BSP_MAX_LEAFS)
		Host_Error("Sys_LoadModelQuake3BSP: bspmodel->numleafs > Q3BSP_MAX_LEAFS\n");
	if (bspmodel->numleaffaces > Q3BSP_MAX_LEAFFACES)
		Host_Error("Sys_LoadModelQuake3BSP: bspmodel->numleaffaces > Q3BSP_MAX_LEAFFACES\n");
	if (bspmodel->visdatasize > Q3BSP_MAX_VISDATASIZE)
		Host_Error("Sys_LoadModelQuake3BSP: bspmodel->visdatasize > Q3BSP_MAX_VISDATASIZE\n");

	Sys_Snprintf(bspmodel->name, MAX_MODEL_NAME, "%s", name);
	/* TODO: check more basic stuff like nummodels, numleafs, numfaces, etc >= MAX, check indices, check if any texture indice == -1? */
	/* TODO CONSOLEDEBUG Sys_Printf("Loaded %s\n", path); */

	return bspmodel;
}

#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
/*
===================
Sys_LoadModelClientDataQuake3BSP_ReconstructParents

Reconstructs the parentes for each leaf and node
===================
*/
void Sys_LoadModelClientDataQuake3BSP_ReconstructParents(q3bsp_model_t *bspmodel, int index)
{
	int children;
	q3bsp_direntry_nodes_t *curnode = &bspmodel->nodes[index];

	children = curnode->children[Q3BSP_NODE_FRONTSIDE];
	if (children < 0)
	{
		bspmodel->leafparent[-children - 1] = index;
	}
	else
	{
		bspmodel->nodeparent[children] = index;
		Sys_LoadModelClientDataQuake3BSP_ReconstructParents(bspmodel, children);
	}

	children = curnode->children[Q3BSP_NODE_BACKSIDE];
	if (children < 0)
	{
		bspmodel->leafparent[-children - 1] = index;
	}
	else
	{
		bspmodel->nodeparent[children] = index;
		Sys_LoadModelClientDataQuake3BSP_ReconstructParents(bspmodel, children);
	}
}
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */

/*
===================
Sys_LoadModelClientDataQuake3BSP

Should only be called by Sys_LoadModelClientData, passing
model_t->data to load any textures, VBOs, etc that
the model uses.

FIXME: darker than on Quake 3, is any overbright or gamma correction going on? (also check vertex colors loaded from the .bsp file for this
===================
*/
void Sys_LoadModelClientDataQuake3BSP(q3bsp_model_t *bspmodel)
{
	int i, j, k;
	char texturename[MAX_TEXTURE_NAME];
	unsigned char *convertedlightmap;
	unsigned char *convlmptr;

	bspmodel->texture_ids = Sys_MemAlloc(&mdl_mem, sizeof(texture_t *) * bspmodel->numtextures, "model");
	bspmodel->lightmap_ids = Sys_MemAlloc(&mdl_mem, sizeof(texture_t *) * bspmodel->numlightmaps, "model");

	for(i = 0; i < bspmodel->numtextures; i++)
	{
		/* TODO: do not draw a surface if texture == noshader? will these surfaces ever get into the rendering queue? */
		/* since we will be having lots os textures in these models, make them separate */
		Sys_Snprintf(texturename, MAX_TEXTURE_NAME, "bsp%s", bspmodel->textures[i].name);
		/* TODO: use texture textures[i].contents and textures[i].flags */
		/* TODO FIXME: Sys Calling CL? */
		bspmodel->texture_ids[i] = CL_LoadTexture(texturename, false, NULL, 0, 0, false, 1, 1);
	}

	/* this will only run if we have lightmaps */
	for (i = 0; i < bspmodel->numlightmaps; i++)
	{
		convertedlightmap = Sys_MemAlloc(&mdl_mem, Q3BSP_LIGHTMAPWIDTH * Q3BSP_LIGHTMAPHEIGHT * 4, "lightmap");
		Sys_Snprintf(texturename, MAX_TEXTURE_NAME, "%slightmap%d", bspmodel->name, i); /* TODO: OTHER PLACES WHERE WE NEED A NAME TO AVOID ALLOWING ONLY ONE OF TYPE OF RESOURCE */

		/* convert to our texture format */
		convlmptr = convertedlightmap;
		for (j = 0; j < Q3BSP_LIGHTMAPWIDTH; j++)
		{
			for (k = 0; k < Q3BSP_LIGHTMAPHEIGHT; k++)
			{
				int r, g, b, overbright_shift;
				/* TODO: look at this and at the blend mode (with the "255 - l..." the GL_TEXTURE_ENV_MODE is GL_BLEND, without the "255 - l..." it's GL_MODULATE) */
				/* TODO: see if this overbrighting is right, implement overbright in the rest of the engine! */
				overbright_shift = 2;
				r = (bspmodel->lightmaps[i].map[j][k][2] << overbright_shift) | 0x3;
				g = (bspmodel->lightmaps[i].map[j][k][1] << overbright_shift) | 0x3;
				b = (bspmodel->lightmaps[i].map[j][k][0] << overbright_shift) | 0x3;
				if ((r | g | b) > 255)
				{
					int max;

					max = Math_Max(r, g);
					max = Math_Max(max, b);
					r = r * 255 / max;
					g = g * 255 / max;
					b = b * 255 / max;
				}
				convlmptr[0] = r;
				convlmptr[1] = g;
				convlmptr[2] = b;
				convlmptr[3] = 255;
				convlmptr += 4;
			}
		}

		/* TODO FIXME: Sys Calling CL? */
		bspmodel->lightmap_ids[i] = CL_LoadTexture(texturename, false, convertedlightmap, Q3BSP_LIGHTMAPWIDTH, Q3BSP_LIGHTMAPHEIGHT, false, 1, 1);
	}

	/* TODO CONSOLEDEBUG Sys_Printf("Uploaded %d textures and %d lightmaps from Q3BSP to video memory\n", bspmodel->numtextures, bspmodel->numlightmaps); */

	{
		/* TODO FIXME: do vbos by brush instead of by face? do not store these stuff, they will be sent to the vbo and never updated! (for now, but q3bsp shaders...?) */
		int modelidx;
#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
		int textureidx, lightmapidx;
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
		int i, j;
		int numtrisidx = 0;
		model_trimesh_part_t *trimesh;

#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
		bspmodel->face_ids = Sys_MemAlloc(&mdl_mem, sizeof(int) * bspmodel->numfaces, "modeltrimesh");
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
		trimesh = Sys_MemAlloc(&mdl_mem, sizeof(model_trimesh_part_t), "modeltrimesh");

		trimesh->verts = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * bspmodel->numvertexes, "modeltrimesh");
		trimesh->vert_stride = sizeof(model_vertex_t);
		trimesh->vert_count = bspmodel->numvertexes;

		for (i = 0; i < bspmodel->numvertexes; i++)
		{
			Math_Vector3Copy(bspmodel->vertexes[i].origin, trimesh->verts[i].origin);
			Math_Vector2Copy(bspmodel->vertexes[i].texcoord0, trimesh->verts[i].texcoord0);
			Math_Vector2Copy(bspmodel->vertexes[i].texcoord1, trimesh->verts[i].texcoord1);
			Math_Vector3Copy(bspmodel->vertexes[i].normal, trimesh->verts[i].normal);
			Math_Vector4Copy(bspmodel->vertexes[i].color, trimesh->verts[i].color);
		}

		for (modelidx = 0; modelidx < bspmodel->nummodels; modelidx++)
		{
			/* count the faces on model */
			for (i = 0; i < bspmodel->models[modelidx].n_faces; i++)
			{
				switch (bspmodel->faces[bspmodel->models[modelidx].face + i].type)
				{
					case Q3BSP_FACETYPE_POLYGON:
					case Q3BSP_FACETYPE_MESH:
						numtrisidx += bspmodel->faces[bspmodel->models[modelidx].face + i].n_meshverts;
						break;
					case Q3BSP_FACETYPE_PATCH:
					case Q3BSP_FACETYPE_BILLBOARD:
						/* TODO */
						break;
					default:
						Sys_Error("Sys_LoadModelClientDataQuake3BSP: Unknown face type %d for Q3BSP in model %d\n", bspmodel->faces[bspmodel->models[modelidx].face + i].type, modelidx);
				}
			}
		}

		trimesh->indexes = Sys_MemAlloc(&mdl_mem, sizeof(int) * numtrisidx, "modeltrimesh");
		trimesh->index_count = 0; /* we will increase this iteratively */
		trimesh->index_stride = sizeof(q3bsp_direntry_meshverts_t) * 3;

#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
		bspmodel->renderbatchs = Sys_MemAlloc(&mdl_mem, sizeof(q3bsp_renderbatch_t) * bspmodel->nummodels, "q3bsp_render");
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
		for (modelidx = 0; modelidx < bspmodel->nummodels; modelidx++)
		{
#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
			/* TODO: count this (per model) and optimize */
			int batchcombinations = bspmodel->numlightmaps * bspmodel->numtextures;

			bspmodel->renderbatchs[modelidx].numbatches = batchcombinations;
			bspmodel->renderbatchs[modelidx].batchstart = Sys_MemAlloc(&mdl_mem, sizeof(int) * batchcombinations, "q3bsp_render");
			bspmodel->renderbatchs[modelidx].batchcount = Sys_MemAlloc(&mdl_mem, sizeof(int) * batchcombinations, "q3bsp_render");
			bspmodel->renderbatchs[modelidx].batchfirstvertex = Sys_MemAlloc(&mdl_mem, sizeof(int) * batchcombinations, "q3bsp_render");
			bspmodel->renderbatchs[modelidx].batchlastvertex = Sys_MemAlloc(&mdl_mem, sizeof(int) * batchcombinations, "q3bsp_render");
			bspmodel->renderbatchs[modelidx].batchlightmap = Sys_MemAlloc(&mdl_mem, sizeof(int) * batchcombinations, "q3bsp_render");
			bspmodel->renderbatchs[modelidx].batchtexture = Sys_MemAlloc(&mdl_mem, sizeof(int) * batchcombinations, "q3bsp_render");

			/* group by lightmap and then by texture */
			for (lightmapidx = 0; lightmapidx < bspmodel->numlightmaps; lightmapidx++)
			{
				for (textureidx = 0; textureidx < bspmodel->numtextures; textureidx++)
				{
#define CURRENT_BATCH (bspmodel->numtextures * lightmapidx + textureidx)
					bspmodel->renderbatchs[modelidx].batchstart[CURRENT_BATCH] = trimesh->index_count;
					bspmodel->renderbatchs[modelidx].batchfirstvertex[CURRENT_BATCH] = (bspmodel->meshverts + bspmodel->faces[bspmodel->models[modelidx].face + 0].meshvert + 0)->offset + bspmodel->faces[bspmodel->models[modelidx].face + 0].vertex;
					bspmodel->renderbatchs[modelidx].batchlastvertex[CURRENT_BATCH] = (bspmodel->meshverts + bspmodel->faces[bspmodel->models[modelidx].face + 0].meshvert + 0)->offset + bspmodel->faces[bspmodel->models[modelidx].face + 0].vertex;;
					bspmodel->renderbatchs[modelidx].batchlightmap[CURRENT_BATCH] = lightmapidx;
					bspmodel->renderbatchs[modelidx].batchtexture[CURRENT_BATCH] = textureidx;
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
					/* copy and adjust data */
					for (i = 0; i < bspmodel->models[modelidx].n_faces; i++)
					{
#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
						if (bspmodel->faces[bspmodel->models[modelidx].face + i].lm_index != lightmapidx)
							continue;
						if (bspmodel->faces[bspmodel->models[modelidx].face + i].texture != textureidx)
							continue;
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
						switch (bspmodel->faces[bspmodel->models[modelidx].face + i].type)
						{
							case Q3BSP_FACETYPE_POLYGON:
							case Q3BSP_FACETYPE_MESH:
#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
								bspmodel->face_ids[bspmodel->models[modelidx].face + i] = trimesh->index_count;
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
								for (j = 0; j < bspmodel->faces[bspmodel->models[modelidx].face + i].n_meshverts; j++)
								{
									int curvertex = (bspmodel->meshverts + bspmodel->faces[bspmodel->models[modelidx].face + i].meshvert + j)->offset + bspmodel->faces[bspmodel->models[modelidx].face + i].vertex;
#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
									if (curvertex < bspmodel->renderbatchs[modelidx].batchfirstvertex[CURRENT_BATCH])
										bspmodel->renderbatchs[modelidx].batchfirstvertex[CURRENT_BATCH] = curvertex;
									if (curvertex > bspmodel->renderbatchs[modelidx].batchlastvertex[CURRENT_BATCH])
										bspmodel->renderbatchs[modelidx].batchlastvertex[CURRENT_BATCH] = curvertex;
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
									trimesh->indexes[(trimesh->index_count)] = curvertex;
									(trimesh->index_count)++;
								}
								break;
							case Q3BSP_FACETYPE_PATCH:
							case Q3BSP_FACETYPE_BILLBOARD:
								/* TODO */
								break;
							default:
								Sys_Error("Sys_LoadModelClientDataQuake3BSP: Unknown face type %d for Q3BSP in model %d\n", bspmodel->faces[bspmodel->models[modelidx].face + i].type, modelidx);
						}
					}
#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
					bspmodel->renderbatchs[modelidx].batchcount[CURRENT_BATCH] = trimesh->index_count - bspmodel->renderbatchs[modelidx].batchstart[CURRENT_BATCH];
				}
			}
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
		}

		bspmodel->vbo_id = Sys_UploadVBO(-1, trimesh, false, false);

		/* TODO CONSOLEDEBUG Sys_Printf("Uploaded %d triangles from Q3BSP to video memory\n", trimesh->index_count / 3); */
	}
#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
	bspmodel->visiblenodesarray = Sys_MemAlloc(&mdl_mem, sizeof(framenum_t) * bspmodel->numnodes, "q3bsp_render");
	bspmodel->visibleleafsarray = Sys_MemAlloc(&mdl_mem, sizeof(framenum_t) * bspmodel->numleafs, "q3bsp_render");
	bspmodel->numvisiblefaces = 0;
	bspmodel->visiblefacesstack = Sys_MemAlloc(&mdl_mem, sizeof(int) * bspmodel->numfaces, "q3bsp_render");
	bspmodel->visiblefacesarray = Sys_MemAlloc(&mdl_mem, sizeof(framenum_t) * bspmodel->numfaces, "q3bsp_render");
	bspmodel->cache_frame = 0;
	bspmodel->cache_cluster = -1;

	bspmodel->nodeparent = Sys_MemAlloc(&mdl_mem, sizeof(int) * bspmodel->numnodes, "q3bsp_parents");
	bspmodel->leafparent = Sys_MemAlloc(&mdl_mem, sizeof(int) * bspmodel->numleafs, "q3bsp_parents");
	Sys_LoadModelClientDataQuake3BSP_ReconstructParents(bspmodel, 0);
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
}

/*
===================
Sys_LoadModelEntitiesQuake3BSP_GetToken

Gets a space-separated token and returns the new head
TODO: review this, may not be safe
===================
*/
char *Sys_LoadModelEntitiesQuake3BSP_GetToken(char *entity_list, char *token, int token_size)
{
	int i, j, listlen, quotes;

	while (entity_list[0] == ' ' || entity_list[0] == '\r' || entity_list[0] == '\n' || entity_list[0] == '\t') /* FIXME: any other delimiters? */
		entity_list++;

	/* special treatment, quotes run until another one is found and then break (independent of spaces after the closing quote) */
	if (entity_list[0] == '"')
		quotes = true;
	else
		quotes = false;

	listlen = strlen(entity_list);
	for (i = 0; i < listlen; i++)
	{
		if (quotes)
		{
			if (entity_list[i] == 0)
				break;
			if (entity_list[i] == '"' && i) /* closing quote? */
			{
				i++; /* include it */
				break;
			}
		}
		else
		{
			if (entity_list[i] == ' ' || entity_list[i] == '\r' || entity_list[i] == '\n' || entity_list[i] == '\t' || entity_list[i] == 0) /* FIXME: any other delimiters? */
				break;
		}
	}

	for (j = 0; j < i; j++)
	{
		if (j == token_size - 1) /* reserve space for \0 */
			Sys_Error("Sys_LoadModelEntitiesQuake3BSP_GetToken: Token bigger than %d characters\n");

		token[j] = entity_list[j];
	}

	token[j] = 0;
	return entity_list + i;
}

/*
===================
Sys_LoadModelEntitiesQuake3BSP

Should only be called by Sys_LoadModelEntities, passing
model_t->data and destination pointers.

Swizzling can be turned off if not loading entity file from a Q3BSP but from somewhere else using the same syntax but our coordinates
===================
*/
#define Q3BSP_ENTITY_STATEMACHINE_START					0
#define Q3BSP_ENTITY_STATEMACHINE_GETATTRIB				1
#define Q3BSP_ENTITY_STATEMACHINE_GETVALUE				2
void Sys_LoadModelEntitiesQuake3BSP(void *modeldata, int *num_entities, model_entity_t **entities, int modeldata_is_bspheader, int swizzle)
{
	q3bsp_direntry_entities_t *entity_lump;
	char *entity_list;
	int state, curattrib;
	char token[MAX_LINE];

	if (modeldata_is_bspheader)
		entity_lump = ((q3bsp_model_t *)modeldata)->entities;
	else
		entity_lump = (q3bsp_direntry_entities_t *)modeldata;
	entity_list = &entity_lump->ents;

	/* allocate our array */
	*entities = Sys_MemAlloc(&mdl_mem, sizeof(model_entity_t) * MAX_EDICTS, "modelentities"); /* TODO: allocate only what we need */
	*num_entities = 0;

	/* use a state machine to parse the entities file */
	state = Q3BSP_ENTITY_STATEMACHINE_START;
	while (entity_list = Sys_LoadModelEntitiesQuake3BSP_GetToken(entity_list, token, sizeof(token)))
	{
		if (token[0] == 0)
			break;

		switch(state)
		{
			/* initial state */
			case Q3BSP_ENTITY_STATEMACHINE_START:
				if (!strcmp(token, "{")) /* we've got a new entity! */
				{
					if ((*num_entities) == MAX_EDICTS) /* TODO: allocate only what we need */
						Sys_Error("Sys_LoadModelEntitiesQuake3BSP: Too many entities! (%d)\n", (*num_entities));

					state = Q3BSP_ENTITY_STATEMACHINE_GETATTRIB;
					curattrib = 0;
				}
				else
					Sys_Error("Sys_LoadModelEntitiesQuake3BSP: Unexpected token %s, should have been {.\n", token);
				break;
			/* waiting for an entity attribute */
			case Q3BSP_ENTITY_STATEMACHINE_GETATTRIB:
				if (!strcmp(token, "}")) /* end of this entity */
				{
					state = Q3BSP_ENTITY_STATEMACHINE_START;

					(*num_entities)++;
				}
				else if (token[0] == '"' && token[strlen(token) - 1] == '"') /* an attribute! */
				{
					if ((*entities)[*num_entities].num_attribs == MAX_ATTRIBS_PER_ENTITY)
						Sys_Error("Sys_LoadModelEntitiesQuake3BSP: Entity has too many attribs, stopped at %s\n", token);

					state = Q3BSP_ENTITY_STATEMACHINE_GETVALUE;

					(*entities)[*num_entities].attribs[curattrib] = Sys_MemAlloc(&mdl_mem, sizeof(char) * MAX_GAME_STRING, "modelentities");
					(*entities)[*num_entities].values[curattrib] = Sys_MemAlloc(&mdl_mem, sizeof(char) * MAX_GAME_STRING, "modelentities");
					Sys_Snprintf((*entities)[*num_entities].attribs[curattrib], MAX_GAME_STRING, "%s", token);
				}
				else
					Sys_Error("Sys_LoadModelEntitiesQuake3BSP: Unexpected token %s, should have been a quoted attribute.\n", token);
				break;
			/* waiting for an entity attribute value */
			case Q3BSP_ENTITY_STATEMACHINE_GETVALUE:
				if (token[0] == '"' && token[strlen(token) - 1] == '"') /* got it */
				{
					state = Q3BSP_ENTITY_STATEMACHINE_GETATTRIB;

					/* Convert the coordinate system PART 2/2, be careful about this when using this format! */
					/* Quake has a strange inconsistency: coords indexes are swizzled but angles indexes are not! FIXME? */
					/* TODO: convert stuff other than origins? */
					if (!strcmp((*entities)[*num_entities].attribs[curattrib], "\"origin\""))
					{
						vec3_t convertedorigin;

						Sys_Sscanf_s(token, "\"%f %f %f\"", &convertedorigin[0], &convertedorigin[1], &convertedorigin[2]);
						if (swizzle)
						{
							Sys_LoadModelQuake3BSPVec3Swizzle(convertedorigin);
							Sys_LoadModelQuake3BSPVec3Scale(convertedorigin);
						}
						Sys_Snprintf((*entities)[*num_entities].values[curattrib], MAX_GAME_STRING, "\"%f %f %f\"", convertedorigin[0], convertedorigin[1], convertedorigin[2]);
					}
					else
					{
						Sys_Snprintf((*entities)[*num_entities].values[curattrib], MAX_GAME_STRING, "%s", token);
					}

					curattrib++;
					(*entities)[*num_entities].num_attribs++;

					if (curattrib == sizeof((*entities)[*num_entities].attribs)) /* TODO: see if this code is right */
						Sys_Error("Sys_LoadModelEntitiesQuake3BSP: Entity %d has too many properties!\n", *num_entities);
				}
				else
					Sys_Error("Sys_LoadModelEntitiesQuake3BSP: Unexpected token %s, should have been a quoted value.\n", token);
				break;
			default:
				Sys_Error("Sys_LoadModelEntitiesQuake3BSP: Unknown state %d!\n", state);
		}
	}

	/* still parsing some entity? error */
	if (state != Q3BSP_ENTITY_STATEMACHINE_START)
		Sys_Error("Sys_LoadModelEntitiesQuake3BSP: entity lump truncated\n");
}

/*
===================
Sys_LoadModelPhysicsTrimeshQuake3BSP

Should only be called by Sys_LoadModelPhysicsTrimesh, passing
model_t->data and destination pointers.

TODO FIXME: these faces are for rendering, we should use the brushes planes for collision! This is important for clipbrushes (invisible walls), etc
===================
*/
void Sys_LoadModelPhysicsTrimeshQuake3BSP(q3bsp_model_t *bspmodel, int modelidx, model_trimesh_t **trimeshlist)
{
	if (bspmodel->trimesh_cache[modelidx])
	{
		*trimeshlist = bspmodel->trimesh_cache[modelidx];
	}
	else
	{
		int i, j;
		int numtrisidx = 0;
		model_trimesh_part_t *trimesh;

		/* TODO CONSOLEDEBUG Sys_Printf("Processing %d faces for collision detection in model %d out of %d\n", bspmodel->models[modelidx].n_faces, modelidx, bspmodel->nummodels); */

		/* we will just point at the original vertexes */
		*trimeshlist = Sys_MemAlloc(&mdl_mem, sizeof(model_trimesh_t), "modeltrimesh");
		bspmodel->trimesh_cache[modelidx] = *trimeshlist;

		/* TODO: separate into different trimeshes instead of a big soup for the entire map? */
		(*trimeshlist)->num_trimeshes = 1;
		(*trimeshlist)->trimeshes = Sys_MemAlloc(&mdl_mem, sizeof(model_trimesh_part_t), "modeltrimesh");
		trimesh = (*trimeshlist)->trimeshes;

		trimesh->verts = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * bspmodel->numvertexes, "modeltrimesh");
		trimesh->vert_stride = sizeof(model_vertex_t);
		trimesh->vert_count = bspmodel->numvertexes;

		for (i = 0; i < bspmodel->numvertexes; i++)
		{
			Math_Vector3Copy(bspmodel->vertexes[i].origin, trimesh->verts[i].origin);
			Math_Vector2Copy(bspmodel->vertexes[i].texcoord0, trimesh->verts[i].texcoord0);
			Math_Vector2Copy(bspmodel->vertexes[i].texcoord1, trimesh->verts[i].texcoord1);
			Math_Vector3Copy(bspmodel->vertexes[i].normal, trimesh->verts[i].normal);
			Math_Vector4Copy(bspmodel->vertexes[i].color, trimesh->verts[i].color);
		}

		/* count the faces on model */
		for (i = 0; i < bspmodel->models[modelidx].n_faces; i++)
		{
			/* ignore faces we should pass through */
			if ((bspmodel->textures[bspmodel->faces[bspmodel->models[modelidx].face + i].texture].flags & Q3BSP_SURF_NONSOLID))
				continue;
			switch (bspmodel->faces[bspmodel->models[modelidx].face + i].type)
			{
				case Q3BSP_FACETYPE_POLYGON:
				case Q3BSP_FACETYPE_MESH:
					numtrisidx += bspmodel->faces[bspmodel->models[modelidx].face + i].n_meshverts;
					break;
				case Q3BSP_FACETYPE_PATCH:
				case Q3BSP_FACETYPE_BILLBOARD:
					/* TODO */
					break;
				default:
					Sys_Error("Sys_LoadModelPhysicsTrimeshQuake3BSP: Unknown face type %d for Q3BSP in model %d\n", bspmodel->faces[bspmodel->models[modelidx].face + i].type, modelidx);
			}
		}

		/* TODO CONSOLEDEBUG Sys_Printf("%d triangles detected in model %d.\n", numtrisidx / 3, modelidx); */

		trimesh->indexes = Sys_MemAlloc(&mdl_mem, sizeof(int) * numtrisidx, "modeltrimesh");
		trimesh->index_count = 0; /* we will increase this iteratively */
		trimesh->index_stride = sizeof(q3bsp_direntry_meshverts_t) * 3;

		/* copy and adjust data */
		for (i = 0; i < bspmodel->models[modelidx].n_faces; i++)
		{
			/* ignore faces we should pass through */
			if ((bspmodel->textures[bspmodel->faces[bspmodel->models[modelidx].face + i].texture].flags & Q3BSP_SURF_NONSOLID))
				continue;
			switch (bspmodel->faces[bspmodel->models[modelidx].face + i].type)
			{
				case Q3BSP_FACETYPE_POLYGON:
				case Q3BSP_FACETYPE_MESH:
					for (j = 0; j < bspmodel->faces[bspmodel->models[modelidx].face + i].n_meshverts; j++)
					{
						/* TODO FIXME: USE NORMALS TOO */
						trimesh->indexes[(trimesh->index_count)] = (bspmodel->meshverts + bspmodel->faces[bspmodel->models[modelidx].face + i].meshvert + j)->offset + bspmodel->faces[bspmodel->models[modelidx].face + i].vertex;
						(trimesh->index_count)++;
					}
					break;
				case Q3BSP_FACETYPE_PATCH:
				case Q3BSP_FACETYPE_BILLBOARD:
					/* TODO */
					break;
				default:
					Sys_Error("Sys_LoadModelPhysicsTrimeshQuake3BSP: Unknown face type %d for Q3BSP in model %d\n", bspmodel->faces[bspmodel->models[modelidx].face + i].type, modelidx);
			}
		}
	}
}

/*
===================
Sys_LoadModelPhysicsBrushesQuake3BSP

Should only be called by Sys_LoadModelPhysicsBrushes, passing
model_t->data and destination pointers.

TODO: see what happens with brushes for the different facetypes (Q3BSP_FACETYPE_PATCH, Q3BSP_FACETYPE_BILLBOARD, etc)
===================
*/
void Sys_LoadModelPhysicsBrushesQuake3BSP(q3bsp_model_t *bspmodel, int modelidx, model_brushes_t **outbrushes)
{
	if (bspmodel->brush_cache[modelidx])
	{
		*outbrushes = bspmodel->brush_cache[modelidx];
	}
	else
	{
		int i, j;
		int num_brushes = 0;
		int num_brushsides = 0;
		/* these are for iteratively setting stuff */
		int cur_brush;
		vec_t *cur_normal;
		vec_t *cur_dist;

		/* TODO CONSOLEDEBUG Sys_Printf("Processing %d brushes for collision detection in model %d out of %d\n", bspmodel->models[modelidx].n_brushes, modelidx, bspmodel->nummodels); */

		*outbrushes = Sys_MemAlloc(&mdl_mem, sizeof(model_brushes_t), "modelbrushes");
		bspmodel->brush_cache[modelidx] = *outbrushes;

		/* count how many planes we have */
		for (i = 0; i < bspmodel->models[modelidx].n_brushes; i++)
		{
			/* ignore brushes we should pass through TODO: what do we do about brushes with different contents in different faces? Also, pass brush/plane information for callbacks */
			if (!(bspmodel->textures[bspmodel->brushes[bspmodel->models[modelidx].brush + i].texture].contents & (Q3BSP_CONTENTS_SOLID | Q3BSP_CONTENTS_DETAIL)))
				continue;

			num_brushes++;
			num_brushsides += bspmodel->brushes[bspmodel->models[modelidx].brush + i].n_brushsides;
		}

		(*outbrushes)->num_brushes = num_brushes;
		(*outbrushes)->brush_sides = Sys_MemAlloc(&mdl_mem, sizeof(int) * num_brushes, "modelbrushes");
		(*outbrushes)->normal = Sys_MemAlloc(&mdl_mem, sizeof(vec3_t) * num_brushsides, "modelbrushes");
		(*outbrushes)->dist = Sys_MemAlloc(&mdl_mem, sizeof(vec_t) * num_brushsides, "modelbrushes");

		/* copy and adjust data */
		cur_brush = 0;
		cur_normal = (*outbrushes)->normal;
		cur_dist = (*outbrushes)->dist;
		for (i = 0; i < bspmodel->models[modelidx].n_brushes; i++)
		{
			/* ignore brushes we should pass through TODO: what do we do about brushes with different contents in different faces? Also, pass brush/plane information for callbacks */
			if (!(bspmodel->textures[bspmodel->brushes[bspmodel->models[modelidx].brush + i].texture].contents & (Q3BSP_CONTENTS_SOLID | Q3BSP_CONTENTS_DETAIL)))
				continue;

			(*outbrushes)->brush_sides[cur_brush] = bspmodel->brushes[bspmodel->models[modelidx].brush + i].n_brushsides;
			for (j = 0; j < bspmodel->brushes[bspmodel->models[modelidx].brush + i].n_brushsides; j++)
			{
				q3bsp_direntry_planes_t *curplane = &bspmodel->planes[bspmodel->brushsides[bspmodel->brushes[bspmodel->models[modelidx].brush + i].brushside + j].plane];

				Math_Vector3Copy(curplane->normal, cur_normal);
				*cur_dist = curplane->dist;

				cur_normal += 3;
				cur_dist++;
			}

			cur_brush++;
		}
	}
}

/*
===================
Sys_ModelStaticLightInPointQuake3BSP

Should only be called by Sys_ModelStaticLightInPoint
TODO: TEST THIS
TODO: "direction" will be in WORLD SPACE! FIX THIS?!
TODO: do this per vertex or per pixel! needs gpu
===================
*/
void Sys_ModelStaticLightInPointQuake3BSP(q3bsp_model_t *bspmodel, const vec3_t point, vec3_t ambient, vec3_t directional, vec3_t direction)
{
	int i, j;
	vec3_t gridsize = {Q3BSP_LIGHTVOL_GRIDSIZE_X, Q3BSP_LIGHTVOL_GRIDSIZE_Y, Q3BSP_LIGHTVOL_GRIDSIZE_Z};
	vec3_t realpoint;
	vec3_t realmins;
	vec3_t realmaxs;

	int pointidx[3];
	vec3_t pointfrac;

	int step[3];
	q3bsp_direntry_lightvols_t *floor_lightvol;
	vec_t lightvol_fraction_sum;

	if (!bspmodel->numlightvols)
	{
		/* no data, so fullbright */
		goto fullbright_point;
	}

	/* TODO: swizzle the lightvol data to avoid having to unswizzle the point */
	realpoint[0] = -point[2] * 32.f;
	realpoint[1] = -point[0] * 32.f;
	realpoint[2] = point[1] * 32.f;
	realmins[0] = -bspmodel->models[0].maxs[2] * 32.f;
	realmins[1] = -bspmodel->models[0].maxs[0] * 32.f;
	realmins[2] = bspmodel->models[0].mins[1] * 32.f;
	realmaxs[0] = -bspmodel->models[0].mins[2] * 32.f;
	realmaxs[1] = -bspmodel->models[0].mins[0] * 32.f;
	realmaxs[2] = bspmodel->models[0].maxs[1] * 32.f;

	for (i = 0; i < 3; i++)
	{
		/* snap to the grid */
		Math_Bound(realmins[i], realpoint[i], realmaxs[i] - 1); /* -1 because we will interpolate with +1 in every axis */

		/* go to the origin of the lightgrid array */
		realpoint[i] -= realmins[i];

		/* scale to the grid size */
		realpoint[i] /= gridsize[i];

		/* create an index using lower-bounds */
		pointidx[i] = (int)realpoint[i];

		/* define the fractions */
		pointfrac[i] = realpoint[i] - (vec_t)floor(realpoint[i]);
	}

	/* step multipliers for advacing x, y and z in the flat array */
	step[0] = 1;
	step[1] = bspmodel->num_lightvols_x;
	step[2] = bspmodel->num_lightvols_x * bspmodel->num_lightvols_y;

	/* initial lightvol for the interpolation. We begin by going down in each axis (floor isn't from ground, it's the math function) */
	floor_lightvol = bspmodel->lightvols + pointidx[0] + pointidx[1] * step[1] + pointidx[2] * step[2];

	/* do a trilinear interpolation using 8 samples from the neighbooring lightvols */
	lightvol_fraction_sum = 0;
	Math_ClearVector3(ambient);
	Math_ClearVector3(directional);
	Math_ClearVector3(direction);

	for (i = 0; i < 8; i++)
	{
		vec_t lightvol_frac = 1.0; /* how much the lightvol from this step will influence our result */
		q3bsp_direntry_lightvols_t *lightvol = floor_lightvol; /* the initial lightvol, when each bit of the following counter is zero */
		vec_t phi_longitude, theta_latitude; /* for unencoding q3bsp's 0-255 values into zero to 2*pi */
		vec3_t lightvol_dir_normal; /* for transforming phi/theta in a direction vector */
		for (j = 0; j < 3; j++)
		{
			/*
				Binary counter to determine the lightvol for this step and it's eight.
				Since 2^3 == 8, the two outcomes define if the bits of xyz are 0 or 1.
				0 meaning floor and 1 meaning ceil in determining the lightvols we will use in this step.
				Then we will use the fraction in each axis in linear interpolation for determining the lightvol's weight.
			*/
			if (i & (1 << j))
			{
				lightvol_frac *= pointfrac[j]; /* pointfrac[j] means how much we are close to the next lightvol (frac: 0.f = in the floor_lightvol in that axis, 1.f = in the next lightvol in that axis) */
				lightvol += step[j]; /* since this bit is 1, go in that direction */
			}
			else
			{
				lightvol_frac *= 1.f - pointfrac[j]; /* pointfrac[j] means how much we are far from the floor_lighvol (frac: 0.f = in the floor_lightvol in that axis, 1.f = in the next lightvol in that axis) */
			}
		}

		if (!lightvol->ambient[0] && !lightvol->ambient[1] && !lightvol->ambient[2])
			continue; /* do not use this lightvol for merging, it's all black */

		/* let this lightvol contribute */
		lightvol_fraction_sum += lightvol_frac;
		for (j = 0; j < 3; j++)
		{
			ambient[j] += lightvol_frac * lightvol->ambient[j];
			directional[j] += lightvol_frac * lightvol->directional[j];
		}

		/* TODO: interpolate everything, then transform to direction vector at the last moment? */
		/* transform from the encoded state to [zero, 2*PI) */
		phi_longitude = lightvol->dir[0] * (vec_t)M_PI / 128.f;
		theta_latitude = lightvol->dir[1] * (vec_t)M_PI / 128.f;

		/* transform to a direction vector */
		lightvol_dir_normal[0] = (vec_t)cos(theta_latitude) * (vec_t)sin(phi_longitude);
		lightvol_dir_normal[1] = (vec_t)sin(theta_latitude) * (vec_t)sin(phi_longitude);
		lightvol_dir_normal[2] = (vec_t)cos(phi_longitude);

		/* finally contribute to the normal */
		Math_Vector3ScaleAdd(lightvol_dir_normal, lightvol_frac, direction, direction);
	}

	/* normalize values if some lightvols didn't contribute, but do nothing if none contributed TODO FIXME: is this right? do other scalings? overbrights? like we do in the lightmaps? */
	if (lightvol_fraction_sum > 0 && lightvol_fraction_sum < 1)
	{
		vec_t lightvol_fraction_sum_inverse = 1.f / lightvol_fraction_sum;
		Math_Vector3Scale(ambient, ambient, lightvol_fraction_sum_inverse);
		Math_Vector3Scale(directional, directional, lightvol_fraction_sum_inverse);
	}
	/* normalize the direction vector and swizzle the result for the engine to use */
	Math_Vector3Normalize(direction);
	Sys_LoadModelQuake3BSPVec3Swizzle(direction);
	/* apply overbright and make it 0-1 TODO: see if this overbrighting is right, implement overbright in the rest of the engine! */
	ambient[0] = ambient[0] * 4.f / 255.f;
	ambient[1] = ambient[1] * 4.f / 255.f;
	ambient[2] = ambient[2] * 4.f / 255.f;
	directional[0] = directional[0] * 4.f / 255.f;
	directional[1] = directional[1] * 4.f / 255.f;
	directional[2] = directional[2] * 4.f / 255.f;
	return;

fullbright_point:
	ambient[0] = 1;
	ambient[1] = 1;
	ambient[2] = 1;
	directional[0] = 0;
	directional[1] = 0;
	directional[2] = 0;
	direction[0] = 0;
	direction[1] = 0;
	direction[2] = -1;
	return;
}

/*
===================
Sys_ModelAABBQuake3BSP

Should only be called by Sys_ModelAABB
TODO: we can also get AABB per leaf, would it be useful? or only if we did our own physics engine
TODO: test
===================
*/
void Sys_ModelAABBQuake3BSP(q3bsp_model_t *bspmodel, vec3_t mins, vec3_t maxs)
{
	Math_Vector3Copy(bspmodel->models[0].mins, mins);
	Math_Vector3Copy(bspmodel->models[0].maxs, maxs);
}

#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
/*
===================
Sys_ModelQuake3BSP_IsClusterVisible

Returns true if a cluster is visible from another cluster.

Excerpt from a unofficial spec:
Recall that a Quake 3 map is divided into convex spaces called leaves. Adjacent
leaves are joined into clusters. The map file contains precomputed visibility
information at the cluster level, which is stored in the visData bit array.

TODO: adapt for network PVS, etc...
===================
*/
int Sys_ModelQuake3BSP_IsClusterVisible(q3bsp_model_t *bspmodel, int viscluster, int testcluster)
{
	int i;
	unsigned char vis_set;

	if (!bspmodel->visdatasize || (bspmodel->visdata->n_vecs == 0) || (viscluster < 0)) /* FIXME: should we return visible when viscluster < 0? */
		return true;

	i = (viscluster * bspmodel->visdata->sz_vecs) + (testcluster >> 3);
	vis_set = *((&bspmodel->visdata->vecs) + i);

	return (vis_set & (1 << (testcluster & 7))) != 0;

	/* TODO: we may also perform culling to the frustum planes here */
	/* TODO: when culling ANYTHING, remember to add the model "radius" (biggest dist from point to vert) to the AABB, if the model is rotated */
}
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */

/*
===================
Sys_ModelQuake3BSP_FindLeaf

Returns the leaf index for "origin"
TODO: adapt for network PVS, etc...
TODO: implement for sphere and aabbox? uses: entity leaf placement/linking, etc
===================
*/
int Sys_ModelQuake3BSP_FindLeaf(q3bsp_model_t *bspmodel, const vec3_t origin)
{
	int index = 0;

	while (index >= 0)
	{
		q3bsp_direntry_nodes_t *curnode = &bspmodel->nodes[index];
		q3bsp_direntry_planes_t *curplane = &bspmodel->planes[curnode->plane];

		/* distance from a point to a plane */
		vec_t distance = Math_DotProduct3(curplane->normal, origin) - curplane->dist; /* FIXME: optimize trivial cases */

		if (distance >= 0)
		{
			index = curnode->children[Q3BSP_NODE_FRONTSIDE];
		}
		else
		{
			index = curnode->children[Q3BSP_NODE_BACKSIDE];
		}
	}

	return -index - 1;
}

/*
===================
Sys_ModelPointContentsQuake3BSP

Should only be called by Sys_ModelPointContents
TODO: What about Q3BSP_FACETYPE_PATCH and Q3BSP_FACETYPE_BILLBOARD? Specially the former!
TODO: implement for sphere and aabbox? makes sense?
TODO: do this for entities and submodels (because we may have a water brush inside a entity in the map
===================
*/
unsigned int Sys_ModelPointContentsQuake3BSP(q3bsp_model_t *bspmodel, const vec3_t point)
{
	int pointleaf, i;
	unsigned int contents;

	/* find the leaf the point is in */
	pointleaf = Sys_ModelQuake3BSP_FindLeaf(bspmodel, point);

	/* TODO CONSOLEDEBUG
	for (i = 0; i < bspmodel->leafs[pointleaf].n_leaffaces; i++)
		Sys_Printf("leaf %d face %d texture %d contents %d flags %d\n", pointleaf, i, bspmodel->faces[bspmodel->leaffaces[bspmodel->leafs[pointleaf].leafface + i].face].texture, bspmodel->textures[bspmodel->faces[bspmodel->leaffaces[bspmodel->leafs[pointleaf].leafface + i].face].texture].contents, bspmodel->textures[bspmodel->faces[bspmodel->leaffaces[bspmodel->leafs[pointleaf].leafface + i].face].texture].flags);
	for (i = 0; i < bspmodel->leafs[pointleaf].n_leafbrushes; i++)
		Sys_Printf("leaf %d brsh %d texture %d contents %d flags %d\n", pointleaf, i, bspmodel->brushes[bspmodel->leafbrushes[bspmodel->leafs[pointleaf].leafbrush + i].brush].texture, bspmodel->textures[bspmodel->brushes[bspmodel->leafbrushes[bspmodel->leafs[pointleaf].leafbrush + i].brush].texture].contents, bspmodel->textures[bspmodel->brushes[bspmodel->leafbrushes[bspmodel->leafs[pointleaf].leafbrush + i].brush].texture].flags);
	*/

	contents = 0;
	for (i = 0; i < bspmodel->leafs[pointleaf].n_leafbrushes; i++) /* TODO FIXME: is this right? check my ramblings about brush contents here in this same source file */
	{
		/* TODO: other content types that matter for external stuff */
		if (bspmodel->textures[bspmodel->brushes[bspmodel->leafbrushes[bspmodel->leafs[pointleaf].leafbrush + i].brush].texture].contents & Q3BSP_CONTENTS_WATER)
			contents |= CONTENTS_WATER_BIT;
		if (bspmodel->textures[bspmodel->brushes[bspmodel->leafbrushes[bspmodel->leafs[pointleaf].leafbrush + i].brush].texture].contents & (Q3BSP_CONTENTS_SOLID | Q3BSP_CONTENTS_DETAIL)) /* TODO: verify what will happen if we call this with a traceline endpos that ends in a wall */
			contents |= CONTENTS_SOLID_BIT;
	}

	return contents;
}

/*
===================
Sys_ModelTraceQuake3BSP_ClipAgainstBrush

Checks lines against convex brushes defined by planes, setting the collision points where applicable.
Using this definition, we can know that a point is inside such brush if its distance to each of
the brush planes is < 0.

If a line segment intersects more than one plane and one of the endpoints of the segment is inside the brush,
the distance from the inside point to the brush surface is the smaller distance from this point to all the intersected
brush planes. For the outside point, it's the reverse: the distance from the outside point to the brush surface is the
biggest of the distances from this point to all the intersected brush planes.

The analog occurs for a line segment's distances when intercepting planes: if the start is outside (in front) of the
plane, then the entering point is the biggest distance. If the start if outside of the plane, then the leaving point is the
shortest distance

TODO: create planes from entities AABB's and test them with this, for consistency?
TODO: check for entities AABB's
TODO: apparently, brushes have their AABB as its first six planes, for quick testing!
FIXME: Q3BSP_TRACE_BOX macro is a waste of processing time
===================
*/
#define Q3BSP_TRACE_BOX		(!sphere_radius && (box_mins ? (box_mins[0] || box_mins[1] || box_mins[2]) : false) && (box_maxs ? (box_maxs[0] || box_maxs[1] || box_maxs[2]) : false))
void Sys_ModelTraceQuake3BSP_ClipAgainstBrush(q3bsp_model_t *bspmodel, vec3_t inputstart, vec3_t inputend, vec_t sphere_radius, vec3_t box_mins, vec3_t box_maxs, vec_t *outputfraction, int *startsolid, int *allsolid, vec3_t plane_normal, vec_t *plane_dist, q3bsp_direntry_brushes_t *curbrush)
{
	q3bsp_direntry_brushsides_t *curbrushside;
	q3bsp_direntry_planes_t *curplane;
	vec_t startdist, enddist;
	vec3_t offset3, offseted_pos;
	int i, j;
	/* because of this variable, this function should ONLY be called when a leaf has brushes and brush planes */
	int start_outside = false;
	int end_outside = false;
	/* initialize these to values smaller than the smallest possible for easy first set */
	vec_t enter_fraction = -1.f; /* fraction where we enter the brush */
	vec_t leave_fraction = 1.f; /* fraction where we leave the brush */
	 /* plane that we hit to enter the brush */
	vec_t enter_plane_dist;
	vec3_t enter_plane_normal;

	for (i = 0; i < curbrush->n_brushsides; i++)
	{
		curbrushside = &bspmodel->brushsides[curbrush->brushside + i];
		curplane = &bspmodel->planes[curbrushside->plane];
		if (Q3BSP_TRACE_BOX) /* FIXME: is this right? */
		{
			for (j = 0; j < 3; j++) /* create an offset for each axis */
			{
				if (curplane->normal[j] < 0) /* see which side of the AABB should collide with this plane depending on the plane orientation (no need to check the other side if we will be fully immersed anyway) TODO: consistency in the >, >=, etc, checks and epsilon usage? */
					offset3[j] = box_maxs[j];
				else
					offset3[j] = box_mins[j];
			}

			/* offset the point by the AABB */
			Math_Vector3Add(inputstart, offset3, offseted_pos);
			/* calculate distance from the offseted point */
			startdist = Math_DotProduct3(curplane->normal, offseted_pos) - curplane->dist; /* FIXME: optimize trivial cases */

			/* offset the point by the AABB */
			Math_Vector3Add(inputend, offset3, offseted_pos);
			/* calculate distance from the offseted point */
			enddist = Math_DotProduct3(curplane->normal, offseted_pos) - curplane->dist; /* FIXME: optimize trivial cases */
		}
		else /* the same code may be used for lines and spheres */
		{
			startdist = Math_DotProduct3(curplane->normal, inputstart) - (curplane->dist + sphere_radius); /* FIXME: optimize trivial cases */
			enddist = Math_DotProduct3(curplane->normal, inputend) - (curplane->dist + sphere_radius); /* FIXME: optimize trivial cases */
		}

		if (startdist > 0) /* this means that this point is outside the brush */
			start_outside = true;
		if (enddist > 0) /* this means that this point is outside the brush */
			end_outside = true;
		if (startdist > 0 && enddist > 0) /* TODO: consistency in the >, >=, etc, checks and epsilon usage? */
			return; /* if both are on the front of a plane, this means that the line segment is completely outside the brush */
		if (startdist <= 0 && enddist <= 0) /* TODO: consistency in the >, >=, etc, checks and epsilon usage? */
			continue; /* this means that both points are on the back side of this plane. Try another plane to see if a crossing happens */

		/* this plane was crossed, so there's a chance that this line will cross this brush. Save parameters to use after all planes have been tested */
		if (startdist > enddist) /* entering the brush */
		{
			vec_t fraction = (startdist - Q3BSP_DIST_EPSILON) / (startdist - enddist); /* calculate fraction from start to entering spot, slightly outside the brush FIXME? */
			if (fraction > enter_fraction) /* better result */
			{
				enter_fraction = fraction;
				Math_Vector3Copy(curplane->normal, enter_plane_normal);
				enter_plane_dist = curplane->dist;
			}
		}
		else /* leaving the brush */
		{
			vec_t fraction = (startdist + Q3BSP_DIST_EPSILON) / (startdist - enddist); /* calculate fraction from start to leaving spot, slightly inside the brush FIXME? */
			if (fraction < leave_fraction) /* better result */
				leave_fraction = fraction;
		}
	}

	/* update output. For these to work, the convex brushes shouldn't overlap! */
	if (!start_outside)
	{
		*startsolid = true;
		if (!end_outside)
		{
			*allsolid = true;
			*outputfraction = 0.f;
			/* TODO: return more contents, etc for the hit face */
		}
	}
	if (enter_fraction < leave_fraction) /* TODO FIXME: what to do with the other case? when enter_fraction >= leave_fraction */
	{
		/* see if we have a better result */
		if (enter_fraction > -1.f && enter_fraction < (*outputfraction))
		{
			Math_Bound(0.f, enter_fraction, 1.f);
			*outputfraction = enter_fraction;
			/* TODO: return more material, contents, flags, etc for the hit face */
			Math_Vector3Copy(enter_plane_normal, plane_normal);
			*plane_dist = enter_plane_dist;
		}
	}
}

/*
===================
Sys_ModelTraceQuake3BSP_RecursiveNodeClip

Checks lines agains nodes, splitting them if necessary

TODO: OPTIMIZE! Stop if trace_allsolid, etc...
===================
*/
void Sys_ModelTraceQuake3BSP_RecursiveNodeClip(q3bsp_model_t *bspmodel, vec3_t inputstart, vec3_t inputend, vec_t sphere_radius, vec3_t box_mins, vec3_t box_maxs, vec3_t box_extents, vec_t *outputfraction, vec3_t start, vec3_t end, int *allsolid, int *startsolid, vec_t start_fraction, vec_t end_fraction, vec3_t plane_normal, vec_t *plane_dist, int nodeindex)
{
	q3bsp_direntry_nodes_t *curnode = &bspmodel->nodes[nodeindex];
	q3bsp_direntry_planes_t *curplane = &bspmodel->planes[curnode->plane];
	q3bsp_direntry_leafs_t *curleaf;
	q3bsp_direntry_brushes_t *curbrush;
	vec_t startdist, enddist, offset;
	int i;

	/* we already hit something that is closer, don't bother continuing, HUGE speedup since we stop walking the entire potential collision tree */
	if ((*outputfraction) < start_fraction)
		return;

	if (nodeindex < 0) /* reached a leaf TODO: what if this leaf has no brushes? */
	{
		curleaf = &bspmodel->leafs[-nodeindex - 1];
		for (i = 0; i < curleaf->n_leafbrushes; i++)  /* TODO FIXME: is this right? check my ramblings about brush contents here in this same source file */
		{
			curbrush = &bspmodel->brushes[bspmodel->leafbrushes[curleaf->leafbrush + i].brush];
			/* TODO: brushes may be shared between leafs, ignore if a given brush was already tested for this trace */

			/* TODO: will this give us all the contents flags for all the brushsides? is it possible that they will be different? Also, check for other flags/contents, if ever necessary */
			if ((curbrush->n_brushsides > 0) && (bspmodel->textures[curbrush->texture].contents & (Q3BSP_CONTENTS_SOLID | Q3BSP_CONTENTS_DETAIL)))
				Sys_ModelTraceQuake3BSP_ClipAgainstBrush(bspmodel, inputstart, inputend, sphere_radius, box_mins, box_maxs, outputfraction, startsolid, allsolid, plane_normal, plane_dist, curbrush);

			/* get out if we decide we are completely inside solids */
			if (*allsolid)
				return;
		}

		/* done with this leaf */
		return;
	}

	startdist = Math_DotProduct3(curplane->normal, start) - curplane->dist; /* FIXME: optimize trivial cases */
	enddist = Math_DotProduct3(curplane->normal, end) - curplane->dist; /* FIXME: optimize trivial cases */
	if (Q3BSP_TRACE_BOX) /* TODO: do not use a symmetrical AABB in this part? Test which option is faster (walk more nodes or process more per node) */
	{
		/* box_extents is already in absolute values, so we do a DotProduct with absolute values for the plane normal too */
		offset = 0.f;
		for (i = 0; i < 3; i++) /* FIXME: optimize trivial cases */
			offset += (vec_t)fabs(box_extents[i] * curplane->normal[i]); /* both box_extents and curplane->normal are in local coordinates! The offset will be the distance to the plane from one of the symmetrical ends of the AABB. FIXME: is this right? */
	}
	else
		offset = sphere_radius; /* offsetting for tracing spheres, if 0 it will turn into a line */

	/* TODO: use a epsilon for these first two ifs? */
	if (startdist >= offset && enddist >= offset) /* both in front FIXME: >=? this should be consistent in the entire code, otherwise entities sitting inside planes can cause problems (I've seen it happen) */
		Sys_ModelTraceQuake3BSP_RecursiveNodeClip(bspmodel, inputstart, inputend, sphere_radius, box_mins, box_maxs, box_extents, outputfraction, start, end, allsolid, startsolid, start_fraction, end_fraction, plane_normal, plane_dist, curnode->children[Q3BSP_NODE_FRONTSIDE]);
	else if (startdist < -offset && enddist < -offset) /* both in the back side */
		Sys_ModelTraceQuake3BSP_RecursiveNodeClip(bspmodel, inputstart, inputend, sphere_radius, box_mins, box_maxs, box_extents, outputfraction, start, end, allsolid, startsolid, start_fraction, end_fraction, plane_normal, plane_dist, curnode->children[Q3BSP_NODE_BACKSIDE]);
	else /* we will need to split the line TODO FIXME: check this math! do we want to clip the line one epsilon before or after the plane has been hit? */
	{
		int side; /* where start->middle is */
		vec_t fraction1, fraction2; /* splitting fractions in relation to this recursion-level's sub-traceline */
		vec_t middle_fraction; /* splitting fraction in relation to the global traceline */
		vec3_t middle; /* middle point / point of plane intersection */

		if (startdist < enddist) /* since the points are in opposite sides, this means that start is in the backside and end is in the frontside */
		{
			vec_t inversedist = 1.0f / (startdist - enddist); /* this length will be negative */
			side = Q3BSP_NODE_BACKSIDE; /* start->middle is in the backside */
			fraction1 = (startdist - offset - Q3BSP_DIST_EPSILON) * inversedist; /* start -> start + (end - start) * fraction1 crosses the plane */
			fraction2 = (startdist + offset + Q3BSP_DIST_EPSILON) * inversedist; /* start -> start + (end - start) * fraction2 doesn't cross the plane */
		}
		else if (enddist < startdist) /* since the points are in opposite sides, this means that start is in the frontside and end is in the backside */
		{
			vec_t inversedist = 1.0f / (startdist - enddist); /* this length will be positive */
			side = Q3BSP_NODE_FRONTSIDE; /* start->middle is in the frontside */
			fraction1 = (startdist + offset + Q3BSP_DIST_EPSILON) * inversedist; /* start -> start + (end - start) * fraction1 crosses the plane */
			fraction2 = (startdist - offset - Q3BSP_DIST_EPSILON) * inversedist; /* start -> start + (end - start) * fraction2 doesn't cross the plane */
		}
		else
		{
			side = Q3BSP_NODE_FRONTSIDE;
			fraction1 = 1.0f;
			fraction2 = 0.0f;
		}

		/* keep everything tight */
		Math_Bound(0.0f, fraction1, 1.0f);
		Math_Bound(0.0f, fraction2, 1.0f);

		/* find out the end point for the first side with our already calculated fraction with epsilon */
		middle_fraction = start_fraction + (end_fraction - start_fraction) * fraction1;
		for (i = 0; i < 3; i++)
			middle[i] = start[i] + fraction1 * (end[i] - start[i]);

		/* recurse down the start->middle side */
		Sys_ModelTraceQuake3BSP_RecursiveNodeClip(bspmodel, inputstart, inputend, sphere_radius, box_mins, box_maxs, box_extents, outputfraction, start, middle, allsolid, startsolid, start_fraction, middle_fraction, plane_normal, plane_dist, curnode->children[side]);

		/* find out the start point for the second side with our already calculated fraction with epsilon */
		middle_fraction = start_fraction + (end_fraction - start_fraction) * fraction2;
		for (i = 0; i < 3; i++)
			middle[i] = start[i] + fraction2 * (end[i] - start[i]);

		/* recurse down the middle->end side */
		Sys_ModelTraceQuake3BSP_RecursiveNodeClip(bspmodel, inputstart, inputend, sphere_radius, box_mins, box_maxs, box_extents, outputfraction, middle, end, allsolid, startsolid, middle_fraction, end_fraction, plane_normal, plane_dist, curnode->children[side == Q3BSP_NODE_FRONTSIDE ? Q3BSP_NODE_BACKSIDE : Q3BSP_NODE_FRONTSIDE]);
	}
}

/*
===================
Sys_ModelTraceQuake3BSP

Should only be called by Sys_ModelTrace*

If tracing a line, call with sphere_radius zero, box_mins, box_maxs and box_extents NULL
If tracing a sphere, call with sphere_radius set, box_mins, box_maxs and box_extents NULL
If tracing a box, call with sphere_radius zero, box_mins and box_maxs set to form an AABB, box_extents as the biggest absolute value in each axis

If we call this function with a sphere with zero radius or a box with zero extents, then it's a line tracing

TODO: What about Q3BSP_FACETYPE_PATCH and Q3BSP_FACETYPE_BILLBOARD? Specially the former!
TODO: use pointcontents if start == end?
TODO: in some cases, we may get plane_normal of the plane marked with X below:

        \<- RAY
         \____ <- FLOOR
         /X
RAMP -> / X <- the plane we hit!
       /  X

Another (worse) example:

        \<- RAY
_________\____ <- FLOOR
         X
         X <- the plane we hit!
         X

This is the dreaded "internal edge" problem! Also occurs with physics engines like bullet and ode because of penetration depth for stability
===================
*/
void Sys_ModelTraceQuake3BSP(q3bsp_model_t *bspmodel, vec3_t start, vec3_t end, vec_t sphere_radius, vec3_t box_mins, vec3_t box_maxs, int *allsolid, int *startsolid, vec_t *fraction, vec3_t endpos, vec3_t plane_normal, vec_t *plane_dist)
{
	vec3_t box_extents;
	int i;

	*startsolid = false;
	*allsolid = false;
	*fraction = 1.f;

	if (Q3BSP_TRACE_BOX)
	{
		if (box_mins[0] > box_maxs[0] || box_mins[1] > box_maxs[1] || box_mins[2] > box_maxs[2])
			Host_Error("Sys_ModelTraceQuake3BSP: mins[i] > maxs[i]\n");

		for (i = 0; i < 3; i++) /* TODO: need box_mins <= 0 for each axis */
			box_extents[i] = Math_Max(-box_mins[i], box_maxs[i]); /* store the absolute maximum in each axis, to simulate a symmetrical AABB */

		Sys_ModelTraceQuake3BSP_RecursiveNodeClip(bspmodel, start, end, sphere_radius, box_mins, box_maxs, box_extents, fraction, start, end, allsolid, startsolid, 0, 1, plane_normal, plane_dist, 0);
	}
	else
		Sys_ModelTraceQuake3BSP_RecursiveNodeClip(bspmodel, start, end, sphere_radius, box_mins, box_maxs, NULL, fraction, start, end, allsolid, startsolid, 0, 1, plane_normal, plane_dist, 0);

	if (*fraction == 1.f) /* nothing blocked */
	{
		Math_Vector3Copy(end, endpos);
	}
	else /* we hit something */
	{
		for (i = 0; i < 3; i++)
			endpos[i] = start[i] + (*fraction) * (end[i] - start[i]);
	}
}

/*
===================
Sys_ModelHasPVSQuake3BSP

TODO: use point checking if mins == maxs
===================
*/
int Sys_ModelHasPVSQuake3BSP(q3bsp_model_t *bspmodel)
{
	if (!bspmodel->visdatasize || (bspmodel->visdata->n_vecs == 0))
		return false;

	return true;
}

/*
===================
Sys_ModelPVSGetClustersBoxQuake3BSP

Only call if Sys_ModelHasPVS() is true for the model
TODO: use point checking if mins == maxs
===================
*/
void Sys_ModelPVSGetClustersBoxQuake3BSP(q3bsp_model_t *bspmodel, const vec3_t absmins, const vec3_t absmaxs, int *clusters, int *num_clusters, const int max_clusters, int start_node)
{
	q3bsp_direntry_planes_t *curplane;
	q3bsp_direntry_nodes_t *curnode;
	q3bsp_direntry_leafs_t *leaf;

	while (1)
	{
		int sides;

		if (start_node < 0)
		{
			int i;
			int leafnum;

			leafnum = -1 - start_node;
			leaf = &bspmodel->leafs[leafnum];

			/* TODO: do this faster */
			for (i = 0; i < *num_clusters; i++)
				if (clusters[i] == leaf->cluster)
					return;
			if (*num_clusters == max_clusters)
			{
				Sys_Printf("Sys_ModelPVSGetClustersBoxQuake3BSP: too many clusters!\n");
				return;
			}
			if (leaf->cluster == -1)
				return; /* outside model */
			clusters[*num_clusters] = leaf->cluster;
			(*num_clusters)++;
			return;
		}

		curnode = &bspmodel->nodes[start_node];
		curplane = &bspmodel->planes[curnode->plane];

		sides = Math_BoxTestAgainstPlaneSides(absmins, absmaxs, curplane->normal, curplane->dist);
		if (sides == 1)
			start_node = curnode->children[0];
		else if (sides == 2)
			start_node = curnode->children[1];
		else
		{
			/* intersection */
			Sys_ModelPVSGetClustersBoxQuake3BSP(bspmodel, absmins, absmaxs, clusters, num_clusters, max_clusters, curnode->children[0]);
			start_node = curnode->children[1];
		}
	}
}

/*
===================
Sys_ModelPVSAddToFatPVS

Adds a PVS set to a fat PVS
===================
*/
#define FAT_PVS_MAX_CLUSTERS	128
#define FAT_PVS_EYE_RADIUS		0.25f

/* FIXME: not being reentrant is the lesser problem of these globals */
int fatpvs_clusters[FAT_PVS_MAX_CLUSTERS];
int fatpvs_num_clusters;
unsigned char fatpvs[Q3BSP_MAX_VISDATASIZE]; /* too big, calculate the right value */

void Sys_ModelPVSAddToFatPVS(q3bsp_model_t *bspmodel, const int cluster)
{
	int i;
	unsigned char *vis_set;

	i = (cluster * bspmodel->visdata->sz_vecs);
	vis_set = (&bspmodel->visdata->vecs) + i;
	for (i = 0; i < bspmodel->visdata->sz_vecs; i++)
		fatpvs[i] |= vis_set[i];
}

/*
===================
Sys_ModelPVSCreateFatPVSClustersRecursive

Checks which clusters should be added to the fat PVS, recursive step
===================
*/
void Sys_ModelPVSCreateFatPVSClustersRecursive(q3bsp_model_t *bspmodel, const vec3_t eyeorigin, int index)
{
	while (1)
	{
		if (index < 0)
		{
			int i;
			int leafnum;
			q3bsp_direntry_leafs_t *leaf;

			leafnum = -1 - index;
			leaf = &bspmodel->leafs[leafnum];

			/* TODO: do this faster */
			for (i = 0; i < fatpvs_num_clusters; i++)
				if (fatpvs_clusters[i] == leaf->cluster)
					return;
			if (fatpvs_num_clusters == FAT_PVS_MAX_CLUSTERS)
			{
				Sys_Printf("Sys_ModelPVSCreateFatPVSClustersRecursive: too many clusters!\n");
				return;
			}
			if (leaf->cluster == -1)
				return; /* outside model */
			fatpvs_clusters[fatpvs_num_clusters] = leaf->cluster;
			fatpvs_num_clusters++;
			Sys_ModelPVSAddToFatPVS(bspmodel, leaf->cluster);
			return;
		}
		else
		{
			q3bsp_direntry_nodes_t *curnode = &bspmodel->nodes[index];
			q3bsp_direntry_planes_t *curplane = &bspmodel->planes[curnode->plane];

			/* distance from a point to a plane */
			vec_t distance = Math_DotProduct3(curplane->normal, eyeorigin) - curplane->dist; /* FIXME: optimize trivial cases */

			if (distance >= FAT_PVS_EYE_RADIUS)
			{
				index = curnode->children[Q3BSP_NODE_FRONTSIDE];
			}
			else if (distance < -FAT_PVS_EYE_RADIUS)
			{
				index = curnode->children[Q3BSP_NODE_BACKSIDE];
			}
			else
			{
				/* intersect */
				Sys_ModelPVSCreateFatPVSClustersRecursive(bspmodel, eyeorigin, curnode->children[Q3BSP_NODE_FRONTSIDE]);
				index = curnode->children[Q3BSP_NODE_BACKSIDE];
			}
		}
	}
}

/*
===================
Sys_ModelPVSCreateFatPVSClustersQuake3BSP

Checks which clusters should be added to the fat PVS
===================
*/
void Sys_ModelPVSCreateFatPVSClustersQuake3BSP(q3bsp_model_t *bspmodel, const vec3_t eyeorigin)
{
	if (!bspmodel->visdatasize || (bspmodel->visdata->n_vecs == 0))
		return;

	memset(fatpvs, 0, bspmodel->visdata->sz_vecs);
	fatpvs_num_clusters = 0;

	Sys_ModelPVSCreateFatPVSClustersRecursive(bspmodel, eyeorigin, 0);

	/* TODO CONSOLEDEBUG Sys_Printf("fatpvs_num_clusters = %d\n", fatpvs_num_clusters); */
}

/*
===================
Sys_ModelPVSTestFatPVSClustersQuake3BSP

Tests a cluster list against a fat PVS
===================
*/
int Sys_ModelPVSTestFatPVSClustersQuake3BSP(q3bsp_model_t *bspmodel, const int *clusters, const int num_clusters)
{
	int i, j;
	unsigned char vis_set;
	int testcluster;

	if (!bspmodel->visdatasize || (bspmodel->visdata->n_vecs == 0))
		return true;

	for (j = 0; j < num_clusters; j++)
	{
		if (clusters[j] < 0) /* FIXME: should we return visible when viscluster < 0? */
			return true;
		testcluster = clusters[j];
		i = testcluster >> 3;
		vis_set = *(fatpvs + i);

		if ((vis_set & (1 << (testcluster & 7))) != 0)
			return true;
	}

	return false;

	/* TODO: when culling ANYTHING, remember to add the model "radius" (biggest dist from point to vert) to the AABB, if the model is rotated */
}

#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
/*
===================
Sys_ModelQuake3BSP_MarkLeafs

Marks linearly in "visibleleafsarray" and "visiblenodesarray" the potentially visible leafs/nodes from where we are,
because traversing tree nodes is fast, but traversing them ALL is not!
TODO: see if this is really working correctly, by reversing order, disabling depth test, etc
===================
*/
void Sys_ModelQuake3BSP_MarkLeafs(q3bsp_model_t *bspmodel, int camleaf)
{
	int curleaf, i;
	int solidleaf;
	/* TODO: cull entire nodes based on their [mins, maxs] (remembering that all subnodes will be on one of two sides) and if all their subnodes are not visible (by marking them upwards when visible)! This way we can prevent goind down a branching when recursively drawing */

	for (curleaf = 0; curleaf < bspmodel->numleafs; curleaf++)
	{
		if (bspmodel->visdatasize > 0)
		{
			if (bspmodel->leafs[curleaf].cluster < 0 || bspmodel->leafs[curleaf].cluster > bspmodel->maxviscluster)
				continue; /* just a node TODO: other places where we should do this */
		}
		else
		{
			/* no vis data TODO: doesn't work with detail brushes */
			/* TODO: only use this if using a r_novis-like approach */
			solidleaf = false;
			/* TODO: have this cached upwards if all of the subnodes have the same contents */
			/* a camera can't be inside a solid brush */
			for (i = 0; i < bspmodel->leafs[curleaf].n_leafbrushes; i++) /* TODO FIXME: is this right? check my ramblings about brush contents here in this same source file */
			{
				if (bspmodel->textures[bspmodel->brushes[bspmodel->leafbrushes[bspmodel->leafs[curleaf].leafbrush + i].brush].texture].contents & (Q3BSP_CONTENTS_SOLID | Q3BSP_CONTENTS_DETAIL))
				{
					solidleaf = true;
					break;
				}
			}
			if (solidleaf)
				continue;
		}

		if (Sys_ModelQuake3BSP_IsClusterVisible(bspmodel, bspmodel->leafs[camleaf].cluster, bspmodel->leafs[curleaf].cluster)) /* TODO: cache cluster results for minimal speed gains? */
		{
			int curnode;

			/* FIXME: anything else we want to do per leaf? */
			/* if a leaf is potentially visible, add it to our set */
			bspmodel->visibleleafsarray[curleaf] = host_framecount_notzero;

			curnode = bspmodel->leafparent[curleaf];
			while (1)
			{
				if (bspmodel->visiblenodesarray[curnode] == host_framecount_notzero) /* already set visible up from here */
					break;

				bspmodel->visiblenodesarray[curnode] = host_framecount_notzero;

				if (!curnode) /* root node */
					break;
				curnode = bspmodel->nodeparent[curnode];
			}
		}
	}
}

/*
===================
Sys_ModelQuake3BSP_RecursiveBuildDrawLists

Returns the set of potentially visible faces in "numvisiblefaces", "visiblefacesstack" and "visiblefacessarray"
TODO: adapt for network PVS? store the valid entities when traversing the tree? relink entities when they move? etc...
===================
*/
void Sys_ModelQuake3BSP_RecursiveBuildDrawLists(q3bsp_model_t *bspmodel, int index, vec3_t entorigin, vec3_t entangles, vec3_t eyeorigin)
{
	q3bsp_direntry_nodes_t *curnode = &bspmodel->nodes[index];
	q3bsp_direntry_planes_t *curplane = &bspmodel->planes[curnode->plane];
	vec_t distance;
	int i, curface;
	/* reached a leaf? */
	if (index < 0)
	{
		index = -index - 1;

		/* if this leaf wasn't marked by MarkLeafs, ignore it */
		if (bspmodel->visibleleafsarray[index] != host_framecount_notzero)
			return;

		/*
			TODO:
			Here we can cull a leaf from rendering if it isn't within the camera frustum, even if within the PVS.
			It's also possible to put this check before reaching a leaf, to cull entire subtrees based on their mins-maxs

		vec3_t mins, maxs;
		mins[0] = (vec_t)bspmodel->leafs[index].mins[0];
		mins[1] = (vec_t)bspmodel->leafs[index].mins[1];
		mins[2] = (vec_t)bspmodel->leafs[index].mins[2];
		maxs[0] = (vec_t)bspmodel->leafs[index].maxs[0];
		maxs[1] = (vec_t)bspmodel->leafs[index].maxs[1];
		maxs[2] = (vec_t)bspmodel->leafs[index].maxs[2];
		if (Sys_Video3DFrustumCullingTestAABB(entorigin, entangles, mins, maxs) == FRUSTUM_CULL_OUTSIDE)
			return;
		*/

		/* FIXME: anything else we want to do per leaf? */
		/* if a leaf is potentially visible, add its faces to our set */
		for (i = 0; i < bspmodel->leafs[index].n_leaffaces; i++)
		{
			curface = bspmodel->leaffaces[bspmodel->leafs[index].leafface + i].face;
			if (bspmodel->visiblefacesarray[curface] != host_framecount_notzero) /* rendering faces may be shared between leafs */
			{
				bspmodel->visiblefacesarray[curface] = host_framecount_notzero;
				bspmodel->visiblefacesstack[(bspmodel->numvisiblefaces)++] = curface;
			}
		}

		return;
	}
	else
	{
		if (bspmodel->visiblenodesarray[index] != host_framecount_notzero)
			return; /* no children visible */
	}

	/* distance from a point to a plane */
	distance = Math_DotProduct3(curplane->normal, eyeorigin) - curplane->dist;  /* FIXME: optimize trivial cases */

	/* add the current side first, for front-to-back order TODO: test without z-testing */
	if (distance >= 0) /* we are on the front side of the plane */
	{
		/* then recurse front first, then back */
		Sys_ModelQuake3BSP_RecursiveBuildDrawLists(bspmodel, curnode->children[Q3BSP_NODE_FRONTSIDE], entorigin, entangles, eyeorigin);
		Sys_ModelQuake3BSP_RecursiveBuildDrawLists(bspmodel, curnode->children[Q3BSP_NODE_BACKSIDE], entorigin, entangles, eyeorigin);
	}
	else /* we are on the back side of the plane */
	{
		/* then recurse back first, then front */
		Sys_ModelQuake3BSP_RecursiveBuildDrawLists(bspmodel, curnode->children[Q3BSP_NODE_BACKSIDE], entorigin, entangles, eyeorigin);
		Sys_ModelQuake3BSP_RecursiveBuildDrawLists(bspmodel, curnode->children[Q3BSP_NODE_FRONTSIDE], entorigin, entangles, eyeorigin);
	}
}
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */

/*
===================
Sys_VideoDraw3DModelQuake3BSP

Should only be called by Sys_VideoDraw3DModel
TODO: VERY SMALL BATCHES KILL PERFORMANCE IN MODERN GPU's! IN THIS CASE THE
VERTEX PROCESSING POWER IS SO HIGH THAT IT'S BETTER TO JUST BRUTE FORCE
EVERYTHING!
===================
*/
#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
extern cvar_t *r_lockpvs;
/* FIXME: not being reentrant is the lesser problem of these globals */
q3bsp_model_t *curmodel;
int texture0cmp(const void * a, const void * b)
{
	return (curmodel->faces[*(int *)a].texture - curmodel->faces[*(int *)b].texture);
}
int texture1cmp(const void * a, const void * b)
{
	return (curmodel->faces[*(int *)a].lm_index - curmodel->faces[*(int *)b].lm_index);
}
int Sys_VideoDraw3DModelQuake3BSP(q3bsp_model_t *bspmodel, int modelent, vec3_t eyeorigin, vec3_t origin, vec3_t angles, unsigned int desired_shader)
#else /* BRUTEFORCE_BATCH_QUAKE3BSP */
int Sys_VideoDraw3DModelQuake3BSP(q3bsp_model_t *bspmodel, int modelent, vec3_t origin, vec3_t angles, unsigned int desired_shader)
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
{
	int draw_calls_issued = false;
#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
	int camleaf, curface;
	vec3_t adjusted_eyeorigin;
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
	int i;
	/* TODO: view frustum culling, patch rendering */
	vec_t ent_modelmatrix[16];

	Math_MatrixModel4x4(ent_modelmatrix, origin, angles, NULL);
	Sys_VideoTransformFor3DModel(ent_modelmatrix);
#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
	/* if the bsp origin is not {0, 0, 0}, adjust eyeorigin because the bsp code expect a centered bsp */
	/* TODO: BSP MODEL ANGLE CHANGE NOT HANDLED!!! */
	if (!Math_Vector3IsZero(origin))
		Math_Vector3ScaleAdd(origin, -1, eyeorigin, adjusted_eyeorigin);
	else
		Math_Vector3Copy(eyeorigin, adjusted_eyeorigin);
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
	Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL); /* TODO: make possible to use fixed lights? */
#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
	/* find the leaf the camera is in */
	camleaf = Sys_ModelQuake3BSP_FindLeaf(bspmodel, adjusted_eyeorigin);
	/* Sys_Printf("Camleaf = %d\n", camleaf); TODO CONSOLEDEBUG */

	if ((bspmodel->cache_frame != host_framecount_notzero || bspmodel->cache_cluster != bspmodel->leafs[camleaf].cluster) && !r_lockpvs->doublevalue)
	{
		/* determine which leafs are potentially visible from here using visclusters */
		Sys_ModelQuake3BSP_MarkLeafs(bspmodel, camleaf);

		bspmodel->numvisiblefaces = 0;
		/* start searching for stuff to draw from the root node */
		Sys_ModelQuake3BSP_RecursiveBuildDrawLists(bspmodel, 0, origin, angles, adjusted_eyeorigin);
		/* Sys_Printf("%d faces visible\n", numvisiblefaces); TODO CONSOLEDEBUG */

		if (bspmodel->numvisiblefaces)
		{
			if (bspmodel->numvisiblefaces > 1)
			{
				/* sort */
				/* TODO: test, create an already sorted list, also sort bspsubmodels if they have lots of textures and faces */
				curmodel = bspmodel;
				/* sort by lightmap */
				if (desired_shader != SHADER_DEPTHSTORE)
				{
					qsort(bspmodel->visiblefacesstack, bspmodel->numvisiblefaces, sizeof(bspmodel->visiblefacesstack[0]), texture1cmp);
					{
						/* then sort by texture */
						int start = 0, end;
						for (i = 0; i < bspmodel->numvisiblefaces; i++)
						{
							if (bspmodel->visiblefacesstack[i] != bspmodel->visiblefacesstack[start])
							{
								end = i - 1;
								qsort (bspmodel->visiblefacesstack + start, end - start, sizeof(bspmodel->visiblefacesstack[0]), texture0cmp);
								start = i;
							}
						}
						end = i - 1;
						qsort (bspmodel->visiblefacesstack + start, end - start, sizeof(bspmodel->visiblefacesstack[0]), texture0cmp);
					}
				}
			}
		}

		bspmodel->cache_cluster = bspmodel->leafs[camleaf].cluster;
		bspmodel->cache_frame = host_framecount_notzero;
	}

	/*
		Because of the BSP tree, there would be no need to z-test anything here. But since in Quake3 BSP faces are not split between leafs anymore, this
		is not really a good idea. By traversing the BSP tree from front-to-back we try to make the most use of early z-culling on the hardware
		and drivers. Without z-testing, we would have to perform a separate z-only pass to fill the z-buffer to not waste time rendering hidden
		faces if we have heavy shader programs. The only problem are with transparent surfaces, which need to be back-to-front and are handled
		separately.
	*/
	/* render each face marked as potentially visible */
	for (i = 0; i < bspmodel->numvisiblefaces; i++)
	{
		/* TODO: partition between transparent and opaque faces, skybox, draw opaque front-to-back, draw transparent back-to-front */
		/*
			TODO: excerpt from a unofficial spec:

			Although handling the shaders and effects that can be stored in Quake 3 maps is more complicated, simple alpha blending
			can be supported to render translucent surfaces correctly. When a texture contains an alpha channel, enable blending and
			select the glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) blending mode. Alpha blended faces should not be backfaced
			culled; they appear to have only one polygon for both sides (there is probably a two-sided polygon flag somewhere that is
			the correct place to obtain such information).
		*/
		/* TODO: use just one draw call for consecutive surfaces with the same texture (sort on loading if necessary */
		curface = bspmodel->visiblefacesstack[i];
		switch (bspmodel->faces[curface].type)
		{
			case Q3BSP_FACETYPE_POLYGON:
			case Q3BSP_FACETYPE_MESH:
				/* TODO: OPTION TO USE VERTEX COLORS */
				if (Sys_VideoDrawVBO(bspmodel->vbo_id, bspmodel->texture_ids[bspmodel->faces[curface].texture]->cl_id, (bspmodel->numlightmaps && bspmodel->faces[curface].lm_index >= 0) ? (bspmodel->lightmap_ids[bspmodel->faces[curface].lm_index]->cl_id) : -1, -1, -1, -1, bspmodel->faces[curface].vertex, bspmodel->faces[curface].vertex + bspmodel->faces[curface].n_vertexes - 1, bspmodel->faces[curface].n_meshverts, bspmodel->face_ids[curface]))
					draw_calls_issued = true;
				break;
			case Q3BSP_FACETYPE_PATCH:
			case Q3BSP_FACETYPE_BILLBOARD:
				/* TODO */
				break;
			default:
				Sys_Error("Sys_VideoDraw3DModelQuake3BSP: Unknown face type %d for Q3BSP\n", bspmodel->faces[curface].type);
		}
	}
#else /* BRUTEFORCE_BATCH_QUAKE3BSP */
	for (i = 0; i < bspmodel->renderbatchs[0].numbatches; i++)
	{
		if (!bspmodel->renderbatchs[0].batchcount[i])
			continue;

		if (Sys_VideoDrawVBO(bspmodel->vbo_id, bspmodel->texture_ids[bspmodel->renderbatchs[0].batchtexture[i]]->cl_id, (bspmodel->numlightmaps && bspmodel->renderbatchs[0].batchlightmap[i] >= 0) ? (bspmodel->lightmap_ids[bspmodel->renderbatchs[0].batchlightmap[i]]->cl_id) : -1, -1, -1, -1, bspmodel->renderbatchs[0].batchfirstvertex[i], bspmodel->renderbatchs[0].batchlastvertex[i], bspmodel->renderbatchs[0].batchcount[i], bspmodel->renderbatchs[0].batchstart[i]))
			draw_calls_issued = true;
	}
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
	return draw_calls_issued;
}

/*
============================================================================

Model type: Quake 3 BSP Submodel

This is just a submodel from a .bsp file organized for easy dealing as
a separate model. Points to data on the base model.

It's important to note that the coordinates are global, which means that
entities that use them will be at their starting position when their origin
is '0 0 0'

TODO: see if this will cause problems when we try to rotate entities that use submodels (translate matrix to maxs-mins before rotating?) Or MAYBE convert to local coordinates and set origin to maxs-mins or to a value specified in the map editor (maybe a an entity like info_null?)
TODO: quake 3 dealt with this for sure, see docs for info on this! origin brushes? origin entities? hint textures? find out
TODO: frames for this and the base model (to change textures)

============================================================================
*/

#define Q3BSPSM_MAGIC					('M' << 24 | 'S' << 16 | 'B' << 8 | 'I') /* little endian */

typedef struct q3bsp_submodel_s {
	char						magic[4];

	q3bsp_model_t				*base_bspmodel; /* for knowing the sizes of the stuff below */

	/* the point to all the data from the basemodel */
	q3bsp_direntry_brushes_t	*brushes;
	q3bsp_direntry_faces_t		*faces;
	q3bsp_direntry_vertexes_t	*vertexes;
	q3bsp_direntry_textures_t	*textures;
	q3bsp_direntry_planes_t		*planes;
	q3bsp_direntry_brushsides_t	*brushsides;
	q3bsp_direntry_meshverts_t	*meshverts;
	q3bsp_direntry_effects_t	*effects; /* TODO */
	q3bsp_direntry_lightmaps_t	*lightmaps;

	/* this one points directly to our submodel, not to the direntry like the others above */
	q3bsp_direntry_models_t		*model;
	/* this one is to remember what our index was, it doesn't index the above variable */
	int							modelidx_in_base;
} q3bsp_submodel_t;

/*
===================
Sys_LoadModelQuake3BSPSubModel

Should only be called by Sys_LoadModel.
Returns data pointer for the model data
===================
*/
void *Sys_LoadModelQuake3BSPSubModel(const char *name, void *basemodel)
{
	q3bsp_submodel_t *data;
	int modelidx;

	data = Sys_MemAlloc(&mdl_mem, sizeof(q3bsp_submodel_t), "submodel");
	*(int *)data->magic = Q3BSPSM_MAGIC;
	data->base_bspmodel = basemodel;

	/* copy pointers for general stuff */
	data->brushes = data->base_bspmodel->brushes;
	data->faces = data->base_bspmodel->faces;
	data->vertexes = data->base_bspmodel->vertexes;
	data->textures = data->base_bspmodel->textures;
	data->planes = data->base_bspmodel->planes;
	data->brushsides = data->base_bspmodel->brushsides;
	data->meshverts = data->base_bspmodel->meshverts;
	data->effects = data->base_bspmodel->effects;
	data->lightmaps = data->base_bspmodel->lightmaps;

	/* copy pointer to our model */
	Sys_Sscanf_s(name, "*%d", &modelidx);
	if (modelidx >= data->base_bspmodel->nummodels)
		Host_Error("Sys_LoadModelQuake3BSPSubModel: Trying to load submodel %s but the basemodel only has %d submodels.\n", name, data->base_bspmodel->nummodels);
	data->model = data->base_bspmodel->models + modelidx;
	data->modelidx_in_base = modelidx;

	/* TODO CONSOLEDEBUG Sys_Printf("Loaded submodel %s\n", name); */

	return data;
}

/*
===================
Sys_LoadModelPhysicsTrimeshQuake3BSPSubModel

Should only be called by Sys_LoadModelPhysicsTrimesh, passing
model_t->data and destination pointers.
===================
*/
void Sys_LoadModelPhysicsTrimeshQuake3BSPSubModel(q3bsp_submodel_t *bspsubmodel, model_trimesh_t **trimesh)
{
	/* use the standard procedure */
	Sys_LoadModelPhysicsTrimeshQuake3BSP(bspsubmodel->base_bspmodel, bspsubmodel->modelidx_in_base, trimesh);
}

/*
===================
Sys_LoadModelPhysicsBrushesQuake3BSPSubModel

Should only be called by Sys_LoadModelPhysicsBrushes, passing
model_t->data and destination pointers.
===================
*/
void Sys_LoadModelPhysicsBrushesQuake3BSPSubModel(q3bsp_submodel_t *bspsubmodel, model_brushes_t **brushes)
{
	/* use the standard procedure */
	Sys_LoadModelPhysicsBrushesQuake3BSP(bspsubmodel->base_bspmodel, bspsubmodel->modelidx_in_base, brushes);
}

/*
===================
Sys_ModelAABBQuake3BSPSubModel

Should only be called by Sys_ModelAABB
TODO: test
===================
*/
void Sys_ModelAABBQuake3BSPSubModel(q3bsp_submodel_t *bspsubmodel, vec3_t mins, vec3_t maxs)
{
	Math_Vector3Copy(bspsubmodel->model->mins, mins);
	Math_Vector3Copy(bspsubmodel->model->maxs, maxs);
}

/*
===================
Sys_VideoDraw3DModelQuake3BSPSubModel

Should only be called by Sys_VideoDraw3DModel
This is a lot easiser, since submodels do not have BSP trees
===================
*/
int Sys_VideoDraw3DModelQuake3BSPSubModel(q3bsp_submodel_t *bspsubmodel, int modelent, vec3_t origin, vec3_t angles, unsigned int desired_shader)
{
	int draw_calls_issued = false;
	int i;
	/* TODO: view frustum culling, patch rendering  */
	vec_t ent_modelmatrix[16];

	Math_MatrixModel4x4(ent_modelmatrix, origin, angles, NULL);
	Sys_VideoTransformFor3DModel(ent_modelmatrix);
	Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL); /* TODO: make possible to use fixed lights? */
#ifndef BRUTEFORCE_BATCH_QUAKE3BSP
	for (i = 0; i < bspsubmodel->model->n_faces; i++)
	{
		/* TODO: partition between transparent and opaque faces, draw opaque front-to-back, draw transparent back-to-front */
		/*
			TODO: excerpt from a unofficial spec:

			Although handling the shaders and effects that can be stored in Quake 3 maps is more complicated, simple alpha blending
			can be supported to render translucent surfaces correctly. When a texture contains an alpha channel, enable blending and
			select the glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) blending mode. Alpha blended faces should not be backfaced
			culled; they appear to have only one polygon for both sides (there is probably a two-sided polygon flag somewhere that is
			the correct place to obtain such information).
		*/
		/* TODO: use just one draw call for consecutive surfaces with the same texture (sort on loading if necessary */
		switch (bspsubmodel->faces[bspsubmodel->model->face + i].type)
		{
			case Q3BSP_FACETYPE_POLYGON:
			case Q3BSP_FACETYPE_MESH:
				/* TODO: OPTION TO USE VERTEX COLORS */
				if (Sys_VideoDrawVBO(bspsubmodel->base_bspmodel->vbo_id, bspsubmodel->base_bspmodel->texture_ids[bspsubmodel->faces[bspsubmodel->model->face + i].texture]->cl_id, (bspsubmodel->base_bspmodel->numlightmaps && bspsubmodel->faces[bspsubmodel->model->face + i].lm_index >= 0) ? (bspsubmodel->base_bspmodel->lightmap_ids[bspsubmodel->faces[bspsubmodel->model->face + i].lm_index]->cl_id) : -1, -1, -1, -1, bspsubmodel->faces[bspsubmodel->model->face + i].vertex, bspsubmodel->faces[bspsubmodel->model->face + i].vertex + bspsubmodel->faces[bspsubmodel->model->face + i].n_vertexes - 1, bspsubmodel->faces[bspsubmodel->model->face + i].n_meshverts, bspsubmodel->base_bspmodel->face_ids[bspsubmodel->model->face + i]))
					draw_calls_issued = true;
				break;
			case Q3BSP_FACETYPE_PATCH:
			case Q3BSP_FACETYPE_BILLBOARD:
				/* TODO */
				break;
			default:
				Sys_Error("Sys_VideoDraw3DModelQuake3BSPSubModel: Unknown face type %d for Q3BSP\n", bspsubmodel->faces[bspsubmodel->model->face + i].type);
		}
	}
#else /* BRUTEFORCE_BATCH_QUAKE3BSP */
	for (i = 0; i < bspsubmodel->base_bspmodel->renderbatchs[bspsubmodel->modelidx_in_base].numbatches; i++)
	{
		if (!bspsubmodel->base_bspmodel->renderbatchs[bspsubmodel->modelidx_in_base].batchcount[i])
			continue;

		if (Sys_VideoDrawVBO(bspsubmodel->base_bspmodel->vbo_id, bspsubmodel->base_bspmodel->texture_ids[bspsubmodel->base_bspmodel->renderbatchs[bspsubmodel->modelidx_in_base].batchtexture[i]]->cl_id, (bspsubmodel->base_bspmodel->numlightmaps && bspsubmodel->base_bspmodel->renderbatchs[bspsubmodel->modelidx_in_base].batchlightmap[i] >= 0) ? (bspsubmodel->base_bspmodel->lightmap_ids[bspsubmodel->base_bspmodel->renderbatchs[bspsubmodel->modelidx_in_base].batchlightmap[i]]->cl_id) : -1, -1, -1, -1, bspsubmodel->base_bspmodel->renderbatchs[bspsubmodel->modelidx_in_base].batchfirstvertex[i], bspsubmodel->base_bspmodel->renderbatchs[bspsubmodel->modelidx_in_base].batchlastvertex[i], bspsubmodel->base_bspmodel->renderbatchs[bspsubmodel->modelidx_in_base].batchcount[i], bspsubmodel->base_bspmodel->renderbatchs[bspsubmodel->modelidx_in_base].batchstart[i]))
			draw_calls_issued = true;
	}
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
	return draw_calls_issued;
}

/*
============================================================================

Model type: Empty - Entity list only

============================================================================
*/

#define ENTITIESONLY_MAGIC						('I' << 24 | 'T' << 16 | 'N' << 8 | 'E') /* little endian */

typedef struct model_entitiesonly_s {
	unsigned char magic[4];
	unsigned char *data;
} model_entitiesonly_t;

/*
===================
Sys_LoadModelEntitiesOnly

Should only be called by Sys_LoadModel.
Returns data pointer for the model data
===================
*/
void *Sys_LoadModelEntitiesOnly(char *path, unsigned char *data)
{
	model_entitiesonly_t *model;
	model = Sys_MemAlloc(&mdl_mem, sizeof(model_entitiesonly_t), "modelheader");

	/* TODO: deal with endianess */
	*((int *)model->magic) = ENTITIESONLY_MAGIC;
	model->data = data;

	/* TODO CONSOLEDEBUG Sys_Printf("Loaded %s\n", path); */

	return (void *)model;
}

/*
===================
Sys_LoadModelEntitiesEntitiesOnly

Should only be called by Sys_LoadModelEntities, passing
model_t->data and destination pointers.
===================
*/
void Sys_LoadModelEntitiesEntitiesOnly(model_entitiesonly_t *modeldata, int *num_entities, model_entity_t **entities)
{
	/* use the Quake 3 BSP entity format in a separate file, but using our coordinate system, not Quake's */
	Sys_LoadModelEntitiesQuake3BSP(modeldata->data, num_entities, entities, false, false);
}

/*
============================================================================

Model type: IQM - Inter Quake Model

============================================================================
*/

#define IQM_LE_MAGIC						('M' << 24 | 'Q' << 16 | 'N' << 8 | 'I') /* little endian */

typedef struct model_iqm_s {
	unsigned char magic[4];
	unsigned char *data;
} model_iqm_t;

/* TODO: fix wrappers on top of wrappers */
void *Sys_LoadModelIQMDo(const char *path, unsigned char *rawdata);
void Sys_LoadModelClientDataIQMDo(void *modeldata);
void Sys_ModelAnimationInfoIQMDo(void *modeldata, const unsigned int animation, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation);
int Sys_ModelAnimationExistsIQMDo(void *modeldata, const unsigned int animation);
void Sys_ModelAABBIQMDo(void *modeldata, const vec_t frame, vec3_t mins, vec3_t maxs);
void Sys_ModelGetTagTransformIQMDo(void *modeldata, const unsigned int tag_idx, const int local_coords, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, const int ent);
void Sys_ModelAnimateIQMDo(void *modeldata, const int ent, vec3_t origin, vec3_t angles, vec_t *frames, const int anim_pitch);
int Sys_VideoDraw3DModelIQMDo(void *modeldata, vec_t *frames, const int ent, vec3_t origin, vec3_t angles, const int anim_pitch, unsigned int desired_shader, const vec3_t ambient, const vec3_t directional, const vec3_t direction);

/*
===================
Sys_LoadModelIQM

Should only be called by Sys_LoadModel.
Returns data pointer for the model data
===================
*/
void *Sys_LoadModelIQM(const char *path, unsigned char *data)
{
	model_iqm_t *model;
	model = Sys_MemAlloc(&mdl_mem, sizeof(model_iqm_t), "modelheader");

	/* TODO: deal with endianess */
	*((int *)model->magic) = IQM_LE_MAGIC;
	model->data = Sys_LoadModelIQMDo(path, data);

	/* TODO CONSOLEDEBUG Sys_Printf("Loaded %s\n", path); */

	return (void *)model;
}

/*
===================
Sys_LoadModelClientDataIQM

Should only be called by Sys_LoadModelClientData, passing
model_t->data to load any textures, VBOs, etc that
the model uses.

FIXME: overbright and/or gamma correction? (also check vertex colors, etc)
===================
*/
void Sys_LoadModelClientDataIQM(model_iqm_t *model)
{
	Sys_LoadModelClientDataIQMDo(model->data);
}

/*
===================
Sys_ModelAnimationInfoIQM

Should only be called by Sys_ModelAnimationInfo
===================
*/
void Sys_ModelAnimationInfoIQM(model_iqm_t *model, const unsigned int animation, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation)
{
	Sys_ModelAnimationInfoIQMDo(model->data, animation, start_frame, num_frames, loop, frames_per_second, multiple_slots, vertex_animation);
}

/*
===================
Sys_ModelAnimationExistsIQM

Should only be called by Sys_ModelAnimationExists
===================
*/
int Sys_ModelAnimationExistsIQM(model_iqm_t *model, const unsigned int animation)
{
	return Sys_ModelAnimationExistsIQMDo(model->data, animation);
}

/*
===================
Sys_ModelAABBIQM

Should only be called by Sys_ModelAABB
===================
*/
void Sys_ModelAABBIQM(model_iqm_t *model, const vec_t frame, vec3_t mins, vec3_t maxs)
{
	Sys_ModelAABBIQMDo(model->data, frame, mins, maxs);
}

/*
===================
Sys_ModelGetTagTransformIQM

Should only be called by Sys_ModelGetTagTransform
===================
*/
void Sys_ModelGetTagTransformIQM(model_iqm_t *model, const unsigned int tag_idx, const int local_coords, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, const int ent)
{
	Sys_ModelGetTagTransformIQMDo(model->data, tag_idx, local_coords, origin, forward, right, up, ent);
}

/*
===================
Sys_ModelAnimateIQM

Should only be called by Sys_ModelAnimate
===================
*/
void Sys_ModelAnimateIQM(model_iqm_t *model, const int ent, vec3_t origin, vec3_t angles, vec_t *frames, const int anim_pitch)
{
	Sys_ModelAnimateIQMDo(model->data, ent, origin, angles, frames, anim_pitch);
}

/*
===================
Sys_VideoDraw3DModelIQM

Should only be called by Sys_VideoDraw3DModel
Non-integer frames are for interpolation
===================
*/
int Sys_VideoDraw3DModelIQM(model_iqm_t *model, vec_t *frames, const int ent, vec3_t origin, vec3_t angles, const int anim_pitch, unsigned int desired_shader, const vec3_t ambient, const vec3_t directional, const vec3_t direction)
{
	return Sys_VideoDraw3DModelIQMDo(model->data, frames, ent, origin, angles, anim_pitch, desired_shader, ambient, directional, direction);
}

/*
============================================================================

Model type: custom heightmap

============================================================================
*/

#define MODELTYPE_HEIGHTMAP_TYPE_ID			0xe174
#define MODELTYPE_HEIGHTMAP_MAX_LOD_LEVELS	16

typedef struct model_heightmap_s {
	int						type; /* MODELTYPE_*, should be the first */

	vec_t					texscale_x, texscale_y;
	vec3_t					heightmap_scale;

	model_heightfield_t		heightfield; /* TODO: dynamic allocation? */

	unsigned char			*entities;

	texture_t				*texturelow;
	texture_t				*texturemid;
	texture_t				*texturehigh;
	texture_t				*texturepath;
	texture_t				*texturepathmask;
	char					*texturelowname;
	char					*texturemidname;
	char					*texturehighname;
	char					*texturepathname;
	char					*texturepathmaskname;
	model_trimesh_part_t	trimesh[MODELTYPE_HEIGHTMAP_MAX_LOD_LEVELS]; /* TODO: dynamic allocation? */
	int						vbo_id[MODELTYPE_HEIGHTMAP_MAX_LOD_LEVELS]; /* TODO: dynamic allocation? */

	vec3_t					mins, maxs;

	int						max_lod; /* indexing stuff with MODELTYPE_HEIGHTMAP_MAX_LOD_LEVELS */
	vec_t					lod_distances[MODELTYPE_HEIGHTMAP_MAX_LOD_LEVELS]; /* TODO: dynamic allocation? */
} model_heightmap_t;

/*
===================
Sys_LoadModelHeightmap

Should only be called by Sys_LoadModel.
Returns data pointer for the model data

It's better if the ascii files end with a newline
TODO: deal with endianess
TODO: better lod creation, not mipmapping the heightmap texture
TODO: last lod is an almost empty model, should it be this way? limit lods to avoid this?
===================
*/
void *Sys_LoadModelHeightmap(const char *name, char *path, unsigned char *data)
{
	int i, marker;
	model_heightmap_t *model;
	char properties_path[MAX_PATH];
	unsigned char *properties_data;
	char heightmap_path[MAX_PATH];
	int orig_heightmap_width; /* columns */
	int orig_heightmap_height; /* rows */
	unsigned char *orig_heightmap_data;
	unsigned char *mipmapped_heightmap_data, *mipmapped_heightmap_data_ptr;
	int miplevels_width, miplevels_height, miplevels_size, current_miplevel;
	int heightmap_inc;
	vec_t texture0to1, texture1to3, textureblendwindowsize;

	marker = Sys_MemLowMark(&tmp_mem);

	model = Sys_MemAlloc(&mdl_mem, sizeof(model_heightmap_t), "modelheader");
	model->type = MODELTYPE_HEIGHTMAP_TYPE_ID;
	model->entities = data;

	/* textures and scales */
	model->texturelowname = Sys_MemAlloc(&mdl_mem, sizeof(char) * MAX_TEXTURE_NAME, "model");
	model->texturemidname = Sys_MemAlloc(&mdl_mem, sizeof(char) * MAX_TEXTURE_NAME, "model");
	model->texturehighname = Sys_MemAlloc(&mdl_mem, sizeof(char) * MAX_TEXTURE_NAME, "model");
	model->texturepathname = Sys_MemAlloc(&mdl_mem, sizeof(char) * MAX_TEXTURE_NAME, "model");
	model->texturepathmaskname = Sys_MemAlloc(&mdl_mem, sizeof(char) * MAX_TEXTURE_NAME, "model");
	Sys_Snprintf(properties_path, MAX_PATH, "%s.properties", path);
	if (Host_FSLoadBinaryFile(properties_path, &tmp_mem, "modeldata", &properties_data, false) == -1)
		Sys_Error("Sys_LoadModelHeightmap: couldn't load \"%s\"\n", properties_path);

#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
	if (!Sys_Sscanf_s(properties_data, "%s", model->texturelowname))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (!Sys_Sscanf_s(properties_data, "%s", model->texturemidname))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (!Sys_Sscanf_s(properties_data, "%s", model->texturehighname))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (!Sys_Sscanf_s(properties_data, "%s", model->texturepathname))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (!Sys_Sscanf_s(properties_data, "%s", model->texturepathmaskname))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);
#else
	if (!Sys_Sscanf_s(properties_data, "%s", model->texturelowname, MAX_TEXTURE_NAME))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (!Sys_Sscanf_s(properties_data, "%s", model->texturemidname, MAX_TEXTURE_NAME))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (!Sys_Sscanf_s(properties_data, "%s", model->texturehighname, MAX_TEXTURE_NAME))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (!Sys_Sscanf_s(properties_data, "%s", model->texturepathname, MAX_TEXTURE_NAME))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (!Sys_Sscanf_s(properties_data, "%s", model->texturepathmaskname, MAX_TEXTURE_NAME))
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);
#endif /* __GNUC__ */

	if (Sys_Sscanf_s(properties_data, "%f %f", &model->texscale_x, &model->texscale_y) != 2)
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (Sys_Sscanf_s(properties_data, "%f %f %f", &texture0to1, &texture1to3, &textureblendwindowsize) != 3)
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);
	properties_data = Host_CMDSkipBlank(properties_data);

	if (Sys_Sscanf_s(properties_data, "%f %f %f", &model->heightmap_scale[0], &model->heightmap_scale[1], &model->heightmap_scale[2]) != 3)
		Sys_Error("Sys_LoadModelHeightmap: \"%s\" truncated\n", path);

	/* load the heightmap */
	Sys_Snprintf(heightmap_path, MAX_PATH, "heightmaps/%s", name);
	Sys_LoadTextureData(heightmap_path, &orig_heightmap_width, &orig_heightmap_height, &orig_heightmap_data, &tmp_mem);
	/* TODO: use other ways to calculate miplevels? */
	model->max_lod = 0;
	miplevels_width = orig_heightmap_width;
	miplevels_height = orig_heightmap_height;
	miplevels_size = miplevels_width * miplevels_height * 4; /* 4 == four-component data */
	while (1)
	{
		if (model->max_lod == MODELTYPE_HEIGHTMAP_MAX_LOD_LEVELS - 1)
			break;
		miplevels_width >>= 1;
		miplevels_height >>= 1;
		if (miplevels_width < 1 || miplevels_height < 1)
			break;
		model->max_lod++;
		miplevels_size += miplevels_width * miplevels_height * 4; /* 4 == four-component data */
	}
	mipmapped_heightmap_data = Sys_MemAlloc(&tmp_mem, miplevels_size, "model");
	memcpy(mipmapped_heightmap_data, orig_heightmap_data, orig_heightmap_width * orig_heightmap_height * 4); /* 4 == four-component data */
	Sys_GenerateMipMaps(name, mipmapped_heightmap_data, orig_heightmap_width, orig_heightmap_height, model->max_lod);

	/* BGRA data, using only B (0-255) (if modifying the range, search for 255 and etc below.) */
	heightmap_inc = 4;

	miplevels_width = orig_heightmap_width;
	miplevels_height = orig_heightmap_height;
	mipmapped_heightmap_data_ptr = mipmapped_heightmap_data;
	for (current_miplevel = 0; current_miplevel <= model->max_lod; current_miplevel++)
	{
		int heightmap_step;

		if (current_miplevel)
		{
			mipmapped_heightmap_data_ptr += miplevels_width * miplevels_height * heightmap_inc;
			miplevels_width >>= 1;
			miplevels_height >>= 1;
		}
		heightmap_step = heightmap_inc * miplevels_width;

		model->trimesh[current_miplevel].vert_count = miplevels_width * miplevels_height;
		model->trimesh[current_miplevel].vert_stride = sizeof(model_vertex_t);
		model->trimesh[current_miplevel].verts = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * model->trimesh[current_miplevel].vert_count, "model");
		{
			int i, j, k, curindex;
			vec_t *trinormals;

			vec_t fTextureU = (vec_t)orig_heightmap_width * model->texscale_x;
			vec_t fTextureV = (vec_t)orig_heightmap_height * model->texscale_y;

			/* scale X and Z to (-0.5, 0.5) for easy scaling and placement later. Y will be (0, 1). */
			for (i = 0; i < miplevels_height; i++)
			{
				for (j = 0; j < miplevels_width; j++)
				{
					vec_t fScaleC = (vec_t)j / ((vec_t)(miplevels_width - 1));
					vec_t fScaleR = (vec_t)i / ((vec_t)(miplevels_height - 1));
					vec_t fVertexHeight = (vec_t)(*(mipmapped_heightmap_data_ptr + heightmap_step * i + j * heightmap_inc)) / 255.0f;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].origin[0] = -0.5f + fScaleC;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].origin[1] = fVertexHeight;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].origin[2] = -0.5f + fScaleR;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].texcoord0[0] = fTextureU * fScaleC;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].texcoord0[1] = fTextureV * fScaleR;
					/* store the texture scaling in the second texcoord */
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].texcoord1[0] = fTextureU;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].texcoord1[1] = fTextureV;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].color[0] = 255;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].color[1] = 255;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].color[2] = 255;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].color[3] = 255;
					/* store the Y scale in the first component, to put it back into 0..1 scale if needed */
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].weights[0] = model->heightmap_scale[1];
					/* 0..1 Y value for texture transitions to start TODO: per vertex */
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].weights[1] = texture0to1;
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].weights[2] = texture1to3;
					/* size of the blending window TODO: per vertex */
					model->trimesh[current_miplevel].verts[i + j * miplevels_width].weights[3] = textureblendwindowsize;
				}
			}

			/* triangle normals ((heightmap_width - 1) * (heightmap_height - 1) quads each with 2 triangles */
			trinormals = Sys_MemAlloc(&tmp_mem, sizeof(vec3_t) * (miplevels_width - 1) * (miplevels_height - 1) * 2, "model");
			for (i = 0; i < miplevels_height - 1; i++)
			{
				for (j = 0; j < miplevels_width - 1; j++)
				{
					vec3_t tri0[3], tri1[3], tri0veca, tri0vecb, tri1veca, tri1vecb, tri0norm, tri1norm;
					Math_Vector3Copy(model->trimesh[current_miplevel].verts[i     +  j      * miplevels_width].origin, tri0[0]);
					Math_Vector3Copy(model->trimesh[current_miplevel].verts[i + 1 +  j      * miplevels_width].origin, tri0[1]);
					Math_Vector3Copy(model->trimesh[current_miplevel].verts[i + 1 + (j + 1) * miplevels_width].origin, tri0[2]);
					Math_Vector3Copy(model->trimesh[current_miplevel].verts[i + 1 + (j + 1) * miplevels_width].origin, tri1[0]);
					Math_Vector3Copy(model->trimesh[current_miplevel].verts[i     + (j + 1) * miplevels_width].origin, tri1[1]);
					Math_Vector3Copy(model->trimesh[current_miplevel].verts[i     +  j      * miplevels_width].origin, tri1[2]);
					Math_Vector3ScaleAdd(tri0[1], -1, tri0[0], tri0veca);
					Math_Vector3ScaleAdd(tri0[2], -1, tri0[1], tri0vecb);
					Math_Vector3ScaleAdd(tri1[1], -1, tri1[0], tri1veca);
					Math_Vector3ScaleAdd(tri1[2], -1, tri1[1], tri1vecb);
					Math_CrossProduct3(tri0veca, tri0vecb, tri0norm);
					Math_CrossProduct3(tri1veca, tri1vecb, tri1norm);
					Math_Vector3Copy(tri0norm, trinormals + (i + j * (miplevels_width - 1)) * 3);
					Math_Vector3Copy(tri1norm, trinormals + (i + j * (miplevels_width - 1) + (miplevels_width - 1) * (miplevels_height - 1)) * 3);
					Math_Vector3Normalize(trinormals + (i + j * (miplevels_width - 1)) * 3);
					Math_Vector3Normalize(trinormals + (i + j * (miplevels_width - 1) + (miplevels_width - 1) * (miplevels_height - 1)) * 3);
				}
			}

			/* smooth vertex normals, seek the triangles the vertex is part of and average their normals */
			for (i = 0; i < miplevels_height; i++)
			{
				for (j = 0; j < miplevels_width; j++)
				{
					vec_t *finalvert = model->trimesh[current_miplevel].verts[i + j * miplevels_width].normal;
					Math_ClearVector3(finalvert);

					/* upper-left */
					if (j != 0 && i != 0)
					{
						Math_Vector3Add(trinormals + ((i - 1) + (j - 1) * (miplevels_width - 1)) * 3, finalvert, finalvert);
						Math_Vector3Add(trinormals + ((i - 1) + (j - 1) * (miplevels_width - 1) + (miplevels_width - 1) * (miplevels_height - 1)) * 3, finalvert, finalvert);
					}
					/* upper-right */
					if (i != 0 && j != miplevels_width - 1)
						Math_Vector3Add(trinormals + ((i - 1) + (j    ) * (miplevels_width - 1)) * 3, finalvert, finalvert);
					/* bottom-right */
					if (i != miplevels_height - 1 && j != miplevels_width - 1)
					{
						Math_Vector3Add(trinormals + ((i    ) + (j    ) * (miplevels_width - 1)) * 3, finalvert, finalvert);
						Math_Vector3Add(trinormals + ((i    ) + (j    ) * (miplevels_width - 1) + (miplevels_width - 1) * (miplevels_height - 1)) * 3, finalvert, finalvert);
					}
					/* bottom-left */
					if (i != miplevels_height - 1 && j != 0)
						Math_Vector3Add(trinormals + ((i    ) + (j - 1) * (miplevels_width - 1) + (miplevels_width - 1) * (miplevels_height - 1)) * 3, finalvert, finalvert);

					Math_Vector3Normalize(finalvert);
				}
			}

			/* indices */
			/* TODO: calculate right array size for when USE_PRIMITIVE_RESTART_INDEX is set and when it's not */
			/* TODO: see if the index generation is right for !USE_PRIMITIVE_RESTART_INDEX */
			model->trimesh[current_miplevel].index_count = (miplevels_height - 1) * miplevels_width * 2 + miplevels_height - 1;
			model->trimesh[current_miplevel].index_stride = sizeof(unsigned int) * 3;
			model->trimesh[current_miplevel].indexes = Sys_MemAlloc(&mdl_mem, sizeof(int) * model->trimesh[current_miplevel].index_count, "model");

			curindex = 0;
			for (i = 0; i < miplevels_height - 1; i++)
			{
				for (j = 0; j < miplevels_width; j++)
				{
#ifndef USE_PRIMITIVE_RESTART_INDEX
					if (i & 1)
					{
#endif
						for (k = 1; k >= 0; k--) /* invert order for my engine */
						{
							int row = i + (1 - k);
							int index = row + j * miplevels_width;

							model->trimesh[current_miplevel].indexes[curindex++] = index;
						}
#ifndef USE_PRIMITIVE_RESTART_INDEX
					}
					else
					{
						for (k = 0; k < 2; k++) /* invert order for my engine */
						{
							int row = i + (1 - k);
							int index = row + (miplevels_width - j - 1) * miplevels_width;

							model->trimesh[current_miplevel].indexes[curindex++] = index;
						}
					}
#endif
				}

				/* TODO: not for the last one? remove from the index count if so */
#ifdef USE_PRIMITIVE_RESTART_INDEX
				model->trimesh[current_miplevel].indexes[curindex++] = SYS_VIDEO_PRIMITIVE_RESTART_INDEX;
#endif
			}
#ifndef USE_PRIMITIVE_RESTART_INDEX
			if (curindex) /* because last miplevel is empty */
				model->trimesh[current_miplevel].indexes[curindex++] = model->trimesh[current_miplevel].indexes[curindex - 1];
#endif
		}
	}

	model->heightfield.width = orig_heightmap_width;
	model->heightfield.width_scale = model->heightmap_scale[0] / ((vec_t)orig_heightmap_width - 1);
	model->heightfield.length = orig_heightmap_height;
	model->heightfield.length_scale = model->heightmap_scale[2] / ((vec_t)orig_heightmap_height - 1);
	model->heightfield.data = Sys_MemAlloc(&mdl_mem, sizeof(vec_t) * orig_heightmap_width * orig_heightmap_height, "model");
	for (i = 0; i < orig_heightmap_width * orig_heightmap_height; i++)
	{
		model->heightfield.data[i] = (vec_t)(*(orig_heightmap_data + i * heightmap_inc)) / 255.f * model->heightmap_scale[1];
		if (!i)
		{
			model->heightfield.maxheight = model->heightfield.data[i];
			model->heightfield.minheight = model->heightfield.data[i];
		}
		else
		{
			if (model->heightfield.maxheight < model->heightfield.data[i])
				model->heightfield.maxheight = model->heightfield.data[i];
			if (model->heightfield.minheight > model->heightfield.data[i])
				model->heightfield.minheight = model->heightfield.data[i];
		}
	}

	model->mins[0] = -model->heightfield.width * model->heightfield.width_scale / 2.f;
	model->mins[1] = model->heightfield.minheight;
	model->mins[2] = -model->heightfield.length * model->heightfield.length_scale / 2.f;
	model->maxs[0] = model->heightfield.width * model->heightfield.width_scale / 2.f;
	model->maxs[1] = model->heightfield.maxheight;
	model->maxs[2] = model->heightfield.length * model->heightfield.length_scale / 2.f;

	/* TODO: calc these correctly, this is just a placeholder - also be careful with overflows */
	{
		vec_t max_axis;
		max_axis = (vec_t)Math_Max(0, fabs(model->mins[0]));
		max_axis = (vec_t)Math_Max(max_axis, fabs(model->mins[1]));
		max_axis = (vec_t)Math_Max(max_axis, fabs(model->mins[2]));
		max_axis = (vec_t)Math_Max(max_axis, fabs(model->maxs[0]));
		max_axis = (vec_t)Math_Max(max_axis, fabs(model->maxs[1]));
		max_axis = (vec_t)Math_Max(max_axis, fabs(model->maxs[2]));

		model->lod_distances[0] = max_axis * 2;
		for (current_miplevel = 1; current_miplevel <= model->max_lod; current_miplevel++)
			model->lod_distances[current_miplevel] = model->lod_distances[current_miplevel - 1] * 2;
	}

	/* TODO CONSOLEDEBUG Sys_Printf("%f %f %f - %f %f %f\n", model->mins[0], model->mins[1], model->mins[2], model->maxs[0], model->maxs[1], model->maxs[2]); */

	Sys_MemFreeToLowMark(&tmp_mem, marker);
	/* TODO CONSOLEDEBUG Sys_Printf("Loaded %s, %d lods\n", path, model->max_lod); */
	return (void *)model;
}

/*
===================
Sys_LoadModelClientDataHeightmap

Should only be called by Sys_LoadModelClientData, passing
model_t->data to load any textures, VBOs, etc that
the model uses.
===================
*/
void Sys_LoadModelClientDataHeightmap(model_heightmap_t *mdl)
{
	int current_miplevel;

	/* TODO FIXME: Sys Calling CL? */
	mdl->texturelow = CL_LoadTexture(mdl->texturelowname, false, NULL, 0, 0, false, 1, 1);
	mdl->texturemid = CL_LoadTexture(mdl->texturemidname, false, NULL, 0, 0, false, 1, 1);
	mdl->texturehigh = CL_LoadTexture(mdl->texturehighname, false, NULL, 0, 0, false, 1, 1);
	mdl->texturepath = CL_LoadTexture(mdl->texturepathname, false, NULL, 0, 0, false, 1, 1);
	mdl->texturepathmask = CL_LoadTexture(mdl->texturepathmaskname, false, NULL, 0, 0, false, 1, 1);
	for (current_miplevel = 0; current_miplevel <= mdl->max_lod; current_miplevel++)
		mdl->vbo_id[current_miplevel] = Sys_UploadVBO(-1, &mdl->trimesh[current_miplevel], true, true);
}

/*
===================
Sys_LoadModelEntitiesHeightmap

Should only be called by Sys_LoadModelEntities, passing
model_t->data and destination pointers.
===================
*/
void Sys_LoadModelEntitiesHeightmap(model_heightmap_t *modeldata, int *num_entities, model_entity_t **entities)
{
	/* use the Quake 3 BSP entity format in a separate file, but using our coordinate system, not Quake's */
	Sys_LoadModelEntitiesQuake3BSP(modeldata->entities, num_entities, entities, false, false);
}

/*
===================
Sys_LoadModelPhysicsHeightfieldHeightmap

Should only be called by Sys_LoadModelPhysicsHeightfield, passing
model_t->data and destination pointers.
===================
*/
void Sys_LoadModelPhysicsHeightfieldHeightmap(model_heightmap_t *heightmap, model_heightfield_t **heightfield)
{
	*heightfield = &heightmap->heightfield;
}

/*
===================
Sys_ModelAABBHeightmap

Should only be called by Sys_ModelAABB
TODO: test
===================
*/
void Sys_ModelAABBHeightmap(model_heightmap_t *heightmap, vec3_t mins, vec3_t maxs)
{
	Math_Vector3Copy(heightmap->mins, mins);
	Math_Vector3Copy(heightmap->maxs, maxs);
}

/*
===================
Sys_VideoDraw3DModelHeightmapCalcLOD

Should only be called by Sys_VideoDraw3DModelHeightmap
TODO: do a Math_LOD function that calculates this in a good way
===================
*/
int Sys_VideoDraw3DModelHeightmapCalcLOD(const vec3_t eyeorigin, const vec3_t origin, const int max_lod, const vec_t *distances)
{
	int lod, i;
	vec_t bias = (vec_t)r_lodbias->doublevalue;
	vec_t hysteresis = (vec_t)r_lodhysteresis->doublevalue;
	vec_t dist;
	vec3_t tmp;
	Math_Vector3ScaleAdd(eyeorigin, -1, origin, tmp);
	dist = (vec_t)Math_Vector3Length(tmp) * bias;
	for (lod = max_lod, i = max_lod; i >= 0; i--)
	{
		if (dist < distances[i])
			lod = i;
		else
			break;
	}

	/* TODO: use hysteresis (need to store the values from previous frames - so that a 100 distance with 10 hysteresis will change up with 105 and down with 95, to avoid flickering) */
	/* TODO: lod blending */

	Math_Bound(0, lod, max_lod); /* just to be sure - TODO: tidy the code and remove */
	return lod;
}

/*
===================
Sys_VideoDraw3DModelHeightmap

Should only be called by Sys_VideoDraw3DModel
===================
*/
int Sys_VideoDraw3DModelHeightmap(model_heightmap_t *mdl, vec3_t eyeorigin, int modelent, vec3_t origin, vec3_t angles, unsigned int desired_shader, const vec3_t ambient, const vec3_t directional, const vec3_t direction)
{
	int lod;
	vec_t ent_modelmatrix[16];

	Math_MatrixModel4x4(ent_modelmatrix, origin, angles, mdl->heightmap_scale); /* TODO: what if we want a entity scale? */
	Sys_VideoTransformFor3DModel(ent_modelmatrix);
	if (desired_shader == SHADER_LIGHTMAPPING)
	{
		/* set lights before transforming the matrix TODO: even the view matrix? seems ok right now, maybe because of glMatrixMode()? */
		const vec_t ambientcol[4] = { ambient[0], ambient[1], ambient[2], 1 },
					diffusecol[4] = { directional[0], directional[1], directional[2], 1 },
					lightdir[4] = { direction[0], direction[1], direction[2], 0 };
			Sys_VideoBindShaderProgram(SHADER_FIXED_LIGHT_TERRAIN, lightdir, diffusecol, ambientcol);
	}
	else
	{
		int terrain_shader;
		switch (desired_shader)
		{
			case SHADER_LIGHTING_NO_SHADOWS:
				terrain_shader = SHADER_LIGHTING_NO_SHADOWS_TERRAIN;
				break;
			case SHADER_DEPTHSTORE:
				terrain_shader = SHADER_DEPTHSTORE_TERRAIN;
				break;
			case SHADER_SHADOWMAPPING:
				terrain_shader = SHADER_SHADOWMAPPING_TERRAIN;
				break;
			default:
				Sys_Error("Sys_VideoDraw3DModelIQMDo: can't find terrain shader for shader %d\n", desired_shader);
				terrain_shader = desired_shader; /* shut up the compiler */
		}
		Sys_VideoBindShaderProgram(terrain_shader, NULL, NULL, NULL);
	}

	/* TODO: doing a Host_Error here made the display dark - gl state problem? */
	lod = Sys_VideoDraw3DModelHeightmapCalcLOD(eyeorigin, origin, mdl->max_lod, mdl->lod_distances);

	return Sys_VideoDrawVBO(mdl->vbo_id[lod], mdl->texturelow->cl_id, mdl->texturemid->cl_id, mdl->texturehigh->cl_id, mdl->texturepath->cl_id, mdl->texturepathmask->cl_id, -1, -1, -1, -1);
}

/*
============================================================================

Model type: Quake 1 Model

============================================================================
*/

#define QUAKE1MDL_MAGIC						('O' << 24 | 'P' << 16 | 'D' << 8 | 'I') /* little endian */
#define QUAKE1MDL_VERSION					6

#define QUAKE1MDL_SYNCTYPE_SYNC				0
#define QUAKE1MDL_SYNCTYPE_RAND				1

#define QUAKE1MDL_FRAMETYPE_SINGLE			0
#define QUAKE1MDL_FRAMETYPE_GROUP			1

#define QUAKE1MDL_SKINTYPE_SINGLE			0
#define QUAKE1MDL_SKINTYPE_GROUP			1

#define QUAKE1MDL_FRAME_MAX_NAME			16

typedef struct model_q1mdl_texcoord_s {
	int						onseam;
	int						s;
	int						t;
} model_q1mdl_texcoord_t;

typedef struct model_q1mdl_triangle_s {
	int						front_face;
	int						vertindex[3];
} model_q1mdl_triangle_t;

typedef struct model_q1mdl_frame_s {
	vec3_t					mins;
	vec3_t					maxs;
	char					*name;

	model_vertex_t			*verts;
} model_q1mdl_frame_t;

typedef struct model_q1mdl_header_s {
	int						magic;
	int						ver;
	vec3_t					scale;
	vec3_t					scale_origin;
	vec_t					bounding_radius;
	vec3_t					eye_position; /* TODO: is this used in the id models? - also implement this as an extension to IQM (or as a tag in IQM?) */
	int						num_skins;
	int						skin_width;
	int						skin_height;
	int						num_verts;
	int						num_tris;
	int						num_frames;
	int						sync_type;
	int						flags; /* TODO: see flags */
	vec_t					size; /* TODO: what is this? */
} model_q1mdl_header_t;

typedef struct model_q1mdl_animation_s {
	int						exists;
	unsigned int			start_frame;
	unsigned int			num_frames;
	int						loop;
	vec_t					frames_per_second;
} model_q1mdl_animation_t;

typedef struct model_q1mdl_s {
	int						magic; /* replicate for easy verifies */
	model_q1mdl_header_t	*header;

	char					name[MAX_MODEL_NAME];

	unsigned char			**skinraw;

	/* same index as skins */
	texture_t				**skin_ids;

	model_q1mdl_texcoord_t	*skin_vertices;
	model_q1mdl_triangle_t	*triangles;

	model_q1mdl_frame_t		*frameinfo;
	model_trimesh_part_t	data; /* data.verts will be NULL. Because they are per-frame, they can be any of the ones in frameinfo[] */

	model_q1mdl_animation_t	animations[NUM_ANIMATIONS];

	/* for drawing */
	model_vertex_t			*interpolation; /* TODO: not reentrant / not multithread safe */
	model_trimesh_part_t	vbo_data;
	int						vbo;

	/* for Sys_LoadModelPhysicsTrimesh - assume we get zero-filled memory */
	model_trimesh_t			*trimesh_cache;
} model_q1mdl_t;

/* TODO: allow other palettes */
/* Quake 1 default palette. Format: 256 entries, 3 bytes each (R, G, B) */
const unsigned char model_q1mdl_pal[768] =
{
	0,0,0,15,15,15,31,31,31,47,47,47,63,63,63,75,75,75,91,91,91,107,107,107,
	123,123,123,139,139,139,155,155,155,171,171,171,187,187,187,203,203,203,
	219,219,219,235,235,235,15,11,7,23,15,11,31,23,11,39,27,15,47,35,19,55,
	43,23,63,47,23,75,55,27,83,59,27,91,67,31,99,75,31,107,83,31,115,87,31,
	123,95,35,131,103,35,143,111,35,11,11,15,19,19,27,27,27,39,39,39,51,47,
	47,63,55,55,75,63,63,87,71,71,103,79,79,115,91,91,127,99,99,139,107,107,
	151,115,115,163,123,123,175,131,131,187,139,139,203,0,0,0,7,7,0,11,11,0,
	19,19,0,27,27,0,35,35,0,43,43,7,47,47,7,55,55,7,63,63,7,71,71,7,75,75,
	11,83,83,11,91,91,11,99,99,11,107,107,15,7,0,0,15,0,0,23,0,0,31,0,0,39,
	0,0,47,0,0,55,0,0,63,0,0,71,0,0,79,0,0,87,0,0,95,0,0,103,0,0,111,0,0,
	119,0,0,127,0,0,19,19,0,27,27,0,35,35,0,47,43,0,55,47,0,67,55,0,75,59,7,
	87,67,7,95,71,7,107,75,11,119,83,15,131,87,19,139,91,19,151,95,27,163,
	99,31,175,103,35,35,19,7,47,23,11,59,31,15,75,35,19,87,43,23,99,47,31,
	115,55,35,127,59,43,143,67,51,159,79,51,175,99,47,191,119,47,207,143,43,
	223,171,39,239,203,31,255,243,27,11,7,0,27,19,0,43,35,15,55,43,19,71,51,
	27,83,55,35,99,63,43,111,71,51,127,83,63,139,95,71,155,107,83,167,123,
	95,183,135,107,195,147,123,211,163,139,227,179,151,171,139,163,159,127,
	151,147,115,135,139,103,123,127,91,111,119,83,99,107,75,87,95,63,75,87,
	55,67,75,47,55,67,39,47,55,31,35,43,23,27,35,19,19,23,11,11,15,7,7,187,
	115,159,175,107,143,163,95,131,151,87,119,139,79,107,127,75,95,115,67,
	83,107,59,75,95,51,63,83,43,55,71,35,43,59,31,35,47,23,27,35,19,19,23,
	11,11,15,7,7,219,195,187,203,179,167,191,163,155,175,151,139,163,135,
	123,151,123,111,135,111,95,123,99,83,107,87,71,95,75,59,83,63,51,67,51,
	39,55,43,31,39,31,23,27,19,15,15,11,7,111,131,123,103,123,111,95,115,
	103,87,107,95,79,99,87,71,91,79,63,83,71,55,75,63,47,67,55,43,59,47,35,
	51,39,31,43,31,23,35,23,15,27,19,11,19,11,7,11,7,255,243,27,239,223,23,
	219,203,19,203,183,15,187,167,15,171,151,11,155,131,7,139,115,7,123,99,
	7,107,83,0,91,71,0,75,55,0,59,43,0,43,31,0,27,15,0,11,7,0,0,0,255,11,
	11,239,19,19,223,27,27,207,35,35,191,43,43,175,47,47,159,47,47,143,47,
	47,127,47,47,111,47,47,95,43,43,79,35,35,63,27,27,47,19,19,31,11,11,15,
	43,0,0,59,0,0,75,7,0,95,7,0,111,15,0,127,23,7,147,31,7,163,39,11,183,
	51,15,195,75,27,207,99,43,219,127,59,227,151,79,231,171,95,239,191,119,
	247,211,139,167,123,59,183,155,55,199,195,55,231,227,87,127,191,255,
	171,231,255,215,255,255,103,0,0,139,0,0,179,0,0,215,0,0,255,0,0,255,
	243,147,255,247,199,255,255,255,159,91,83
};

const double model_q1mdl_anorms[162][3] = {
	/* In Quake's coordinate system! Swizzle before using. */
	{-0.525731, 0.000000, 0.850651},
	{-0.442863, 0.238856, 0.864188},
	{-0.295242, 0.000000, 0.955423},
	{-0.309017, 0.500000, 0.809017},
	{-0.162460, 0.262866, 0.951056},
	{0.000000, 0.000000, 1.000000},
	{0.000000, 0.850651, 0.525731},
	{-0.147621, 0.716567, 0.681718},
	{0.147621, 0.716567, 0.681718},
	{0.000000, 0.525731, 0.850651},
	{0.309017, 0.500000, 0.809017},
	{0.525731, 0.000000, 0.850651},
	{0.295242, 0.000000, 0.955423},
	{0.442863, 0.238856, 0.864188},
	{0.162460, 0.262866, 0.951056},
	{-0.681718, 0.147621, 0.716567},
	{-0.809017, 0.309017, 0.500000},
	{-0.587785, 0.425325, 0.688191},
	{-0.850651, 0.525731, 0.000000},
	{-0.864188, 0.442863, 0.238856},
	{-0.716567, 0.681718, 0.147621},
	{-0.688191, 0.587785, 0.425325},
	{-0.500000, 0.809017, 0.309017},
	{-0.238856, 0.864188, 0.442863},
	{-0.425325, 0.688191, 0.587785},
	{-0.716567, 0.681718, -0.147621},
	{-0.500000, 0.809017, -0.309017},
	{-0.525731, 0.850651, 0.000000},
	{0.000000, 0.850651, -0.525731},
	{-0.238856, 0.864188, -0.442863},
	{0.000000, 0.955423, -0.295242},
	{-0.262866, 0.951056, -0.162460},
	{0.000000, 1.000000, 0.000000},
	{0.000000, 0.955423, 0.295242},
	{-0.262866, 0.951056, 0.162460},
	{0.238856, 0.864188, 0.442863},
	{0.262866, 0.951056, 0.162460},
	{0.500000, 0.809017, 0.309017},
	{0.238856, 0.864188, -0.442863},
	{0.262866, 0.951056, -0.162460},
	{0.500000, 0.809017, -0.309017},
	{0.850651, 0.525731, 0.000000},
	{0.716567, 0.681718, 0.147621},
	{0.716567, 0.681718, -0.147621},
	{0.525731, 0.850651, 0.000000},
	{0.425325, 0.688191, 0.587785},
	{0.864188, 0.442863, 0.238856},
	{0.688191, 0.587785, 0.425325},
	{0.809017, 0.309017, 0.500000},
	{0.681718, 0.147621, 0.716567},
	{0.587785, 0.425325, 0.688191},
	{0.955423, 0.295242, 0.000000},
	{1.000000, 0.000000, 0.000000},
	{0.951056, 0.162460, 0.262866},
	{0.850651, -0.525731, 0.000000},
	{0.955423, -0.295242, 0.000000},
	{0.864188, -0.442863, 0.238856},
	{0.951056, -0.162460, 0.262866},
	{0.809017, -0.309017, 0.500000},
	{0.681718, -0.147621, 0.716567},
	{0.850651, 0.000000, 0.525731},
	{0.864188, 0.442863, -0.238856},
	{0.809017, 0.309017, -0.500000},
	{0.951056, 0.162460, -0.262866},
	{0.525731, 0.000000, -0.850651},
	{0.681718, 0.147621, -0.716567},
	{0.681718, -0.147621, -0.716567},
	{0.850651, 0.000000, -0.525731},
	{0.809017, -0.309017, -0.500000},
	{0.864188, -0.442863, -0.238856},
	{0.951056, -0.162460, -0.262866},
	{0.147621, 0.716567, -0.681718},
	{0.309017, 0.500000, -0.809017},
	{0.425325, 0.688191, -0.587785},
	{0.442863, 0.238856, -0.864188},
	{0.587785, 0.425325, -0.688191},
	{0.688191, 0.587785, -0.425325},
	{-0.147621, 0.716567, -0.681718},
	{-0.309017, 0.500000, -0.809017},
	{0.000000, 0.525731, -0.850651},
	{-0.525731, 0.000000, -0.850651},
	{-0.442863, 0.238856, -0.864188},
	{-0.295242, 0.000000, -0.955423},
	{-0.162460, 0.262866, -0.951056},
	{0.000000, 0.000000, -1.000000},
	{0.295242, 0.000000, -0.955423},
	{0.162460, 0.262866, -0.951056},
	{-0.442863, -0.238856, -0.864188},
	{-0.309017, -0.500000, -0.809017},
	{-0.162460, -0.262866, -0.951056},
	{0.000000, -0.850651, -0.525731},
	{-0.147621, -0.716567, -0.681718},
	{0.147621, -0.716567, -0.681718},
	{0.000000, -0.525731, -0.850651},
	{0.309017, -0.500000, -0.809017},
	{0.442863, -0.238856, -0.864188},
	{0.162460, -0.262866, -0.951056},
	{0.238856, -0.864188, -0.442863},
	{0.500000, -0.809017, -0.309017},
	{0.425325, -0.688191, -0.587785},
	{0.716567, -0.681718, -0.147621},
	{0.688191, -0.587785, -0.425325},
	{0.587785, -0.425325, -0.688191},
	{0.000000, -0.955423, -0.295242},
	{0.000000, -1.000000, 0.000000},
	{0.262866, -0.951056, -0.162460},
	{0.000000, -0.850651, 0.525731},
	{0.000000, -0.955423, 0.295242},
	{0.238856, -0.864188, 0.442863},
	{0.262866, -0.951056, 0.162460},
	{0.500000, -0.809017, 0.309017},
	{0.716567, -0.681718, 0.147621},
	{0.525731, -0.850651, 0.000000},
	{-0.238856, -0.864188, -0.442863},
	{-0.500000, -0.809017, -0.309017},
	{-0.262866, -0.951056, -0.162460},
	{-0.850651, -0.525731, 0.000000},
	{-0.716567, -0.681718, -0.147621},
	{-0.716567, -0.681718, 0.147621},
	{-0.525731, -0.850651, 0.000000},
	{-0.500000, -0.809017, 0.309017},
	{-0.238856, -0.864188, 0.442863},
	{-0.262866, -0.951056, 0.162460},
	{-0.864188, -0.442863, 0.238856},
	{-0.809017, -0.309017, 0.500000},
	{-0.688191, -0.587785, 0.425325},
	{-0.681718, -0.147621, 0.716567},
	{-0.442863, -0.238856, 0.864188},
	{-0.587785, -0.425325, 0.688191},
	{-0.309017, -0.500000, 0.809017},
	{-0.147621, -0.716567, 0.681718},
	{-0.425325, -0.688191, 0.587785},
	{-0.162460, -0.262866, 0.951056},
	{0.442863, -0.238856, 0.864188},
	{0.162460, -0.262866, 0.951056},
	{0.309017, -0.500000, 0.809017},
	{0.147621, -0.716567, 0.681718},
	{0.000000, -0.525731, 0.850651},
	{0.425325, -0.688191, 0.587785},
	{0.587785, -0.425325, 0.688191},
	{0.688191, -0.587785, 0.425325},
	{-0.955423, 0.295242, 0.000000},
	{-0.951056, 0.162460, 0.262866},
	{-1.000000, 0.000000, 0.000000},
	{-0.850651, 0.000000, 0.525731},
	{-0.955423, -0.295242, 0.000000},
	{-0.951056, -0.162460, 0.262866},
	{-0.864188, 0.442863, -0.238856},
	{-0.951056, 0.162460, -0.262866},
	{-0.809017, 0.309017, -0.500000},
	{-0.864188, -0.442863, -0.238856},
	{-0.951056, -0.162460, -0.262866},
	{-0.809017, -0.309017, -0.500000},
	{-0.681718, 0.147621, -0.716567},
	{-0.681718, -0.147621, -0.716567},
	{-0.850651, 0.000000, -0.525731},
	{-0.688191, 0.587785, -0.425325},
	{-0.587785, 0.425325, -0.688191},
	{-0.425325, 0.688191, -0.587785},
	{-0.425325, -0.688191, -0.587785},
	{-0.587785, -0.425325, -0.688191},
	{-0.688191, -0.587785, -0.425325}
};

/*
===================
Sys_LoadModelQuake1MDL

Should only be called by Sys_LoadModel.
Returns data pointer for the model data
===================
*/
void *Sys_LoadModelQuake1MDL(const char *name, const char *path, unsigned char *data, const size_t size)
{
	int i, j;
	model_q1mdl_t *model;
	model = Sys_MemAlloc(&mdl_mem, sizeof(model_q1mdl_t), "q1model");

	/* TODO: deal with endianess */
	model->header = (model_q1mdl_header_t *)data;
	if (model->header->magic != QUAKE1MDL_MAGIC)
		Sys_Error("Model %s magic number mismatch (%u should be %u)\n", path, model->header->magic, QUAKE1MDL_MAGIC);
	if (model->header->ver != QUAKE1MDL_VERSION)
		Sys_Error("Model %s version mismatch (%u should be %u)\n", path, model->header->ver, QUAKE1MDL_VERSION);

	/* TODO: check limits, check if model size in bytes has all the data it claims to have (to avoid overflows - do this for ALL models and file types, this is a security problem with downloads from a server!) */

	data += sizeof(model_q1mdl_header_t);

	model->skinraw = Sys_MemAlloc(&mdl_mem, sizeof(unsigned char *) * model->header->num_skins, "q1model");
	for (i = 0; i < model->header->num_skins; i++)
	{
		int type = *((int *)data);
		data += 4;
		if (type == QUAKE1MDL_SKINTYPE_GROUP)
		{
			Sys_Error("Sys_LoadModelQuake1MDL: %s: skin groups not supported\n", model->name);
		}
		else if (type == QUAKE1MDL_SKINTYPE_SINGLE)
		{
			model->skinraw[i] = data;
			data += model->header->skin_width * model->header->skin_height;
		}
		else
		{
			Sys_Error("Sys_LoadModelQuake1MDL: %s: skin type %d not supported\n", model->name, type);
		}
	}

	model->skin_vertices = (model_q1mdl_texcoord_t *)data;
	data += sizeof(model_q1mdl_texcoord_t) * model->header->num_verts;

	model->triangles = (model_q1mdl_triangle_t *)data;
	data += sizeof(model_q1mdl_triangle_t) * model->header->num_tris;

	model->frameinfo = Sys_MemAlloc(&mdl_mem, sizeof(model_q1mdl_frame_t) * model->header->num_frames, "q1model");
	for (i = 0; i < model->header->num_frames; i++)
	{
		int type = *((int *)data);
		data += 4;
		if (type == QUAKE1MDL_FRAMETYPE_GROUP)
		{
			Sys_Error("Sys_LoadModelQuake1MDL: %s: frame groups not supported\n", model->name);
		}
		else if (type == QUAKE1MDL_FRAMETYPE_SINGLE)
		{
			/* swizzle coords */
			model->frameinfo[i].mins[0] = (vec_t)(*((unsigned char *)data)) * model->header->scale[0] + model->header->scale_origin[0];
			data++;
			model->frameinfo[i].mins[1] = (vec_t)(*((unsigned char *)data)) * model->header->scale[1] + model->header->scale_origin[1];
			data++;
			model->frameinfo[i].mins[2] = (vec_t)(*((unsigned char *)data)) * model->header->scale[2] + model->header->scale_origin[2];
			data++;
			data++; /* unused lightnormal */

			model->frameinfo[i].maxs[0] = (vec_t)(*((unsigned char *)data)) * model->header->scale[0] + model->header->scale_origin[0];
			data++;
			model->frameinfo[i].maxs[1] = (vec_t)(*((unsigned char *)data)) * model->header->scale[1] + model->header->scale_origin[1];
			data++;
			model->frameinfo[i].maxs[2] = (vec_t)(*((unsigned char *)data)) * model->header->scale[2] + model->header->scale_origin[2];
			data++;
			data++; /* unused lightnormal */

			Sys_LoadModelQuake3BSPVec3Swizzle(model->frameinfo[i].mins);
			Sys_LoadModelQuake3BSPVec3Swizzle(model->frameinfo[i].maxs);
			Sys_LoadModelQuake3BSPVec3Scale(model->frameinfo[i].mins);
			Sys_LoadModelQuake3BSPVec3Scale(model->frameinfo[i].maxs);
			Sys_LoadModelQuake3BSPVec3SwizzleMinsMaxs(model->frameinfo[i].mins, model->frameinfo[i].maxs);

			model->frameinfo[i].name = data;
			data += QUAKE1MDL_FRAME_MAX_NAME;

			/* double the number of verts because we have verts on seams TODO FIXME: do this in a better way, this wastes memory */
			model->frameinfo[i].verts = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * model->header->num_verts * 2, "q1model");
			for (j = 0; j < model->header->num_verts; j++)
			{
				/* swizzle coords */
				model->frameinfo[i].verts[j].origin[0] = (vec_t)(*((unsigned char *)data)) * model->header->scale[0] + model->header->scale_origin[0];
				model->frameinfo[i].verts[j + model->header->num_verts].origin[0] = (vec_t)(*((unsigned char *)data)) * model->header->scale[0] + model->header->scale_origin[0];
				data++;
				model->frameinfo[i].verts[j].origin[1] = (vec_t)(*((unsigned char *)data)) * model->header->scale[1] + model->header->scale_origin[1];
				model->frameinfo[i].verts[j + model->header->num_verts].origin[1] = (vec_t)(*((unsigned char *)data)) * model->header->scale[1] + model->header->scale_origin[1];
				data++;
				model->frameinfo[i].verts[j].origin[2] = (vec_t)(*((unsigned char *)data)) * model->header->scale[2] + model->header->scale_origin[2];
				model->frameinfo[i].verts[j + model->header->num_verts].origin[2] = (vec_t)(*((unsigned char *)data)) * model->header->scale[2] + model->header->scale_origin[2];
				data++;

				Sys_LoadModelQuake3BSPVec3Swizzle(model->frameinfo[i].verts[j].origin);
				Sys_LoadModelQuake3BSPVec3Swizzle(model->frameinfo[i].verts[j + model->header->num_verts].origin);
				Sys_LoadModelQuake3BSPVec3Scale(model->frameinfo[i].verts[j].origin);
				Sys_LoadModelQuake3BSPVec3Scale(model->frameinfo[i].verts[j + model->header->num_verts].origin);

				model->frameinfo[i].verts[j].normal[0] = (vec_t)model_q1mdl_anorms[(*((unsigned char *)data))][0];
				model->frameinfo[i].verts[j + model->header->num_verts].normal[0] = (vec_t)model_q1mdl_anorms[(*((unsigned char *)data))][0];
				model->frameinfo[i].verts[j].normal[1] = (vec_t)model_q1mdl_anorms[(*((unsigned char *)data))][1];
				model->frameinfo[i].verts[j + model->header->num_verts].normal[1] = (vec_t)model_q1mdl_anorms[(*((unsigned char *)data))][1];
				model->frameinfo[i].verts[j].normal[2] = (vec_t)model_q1mdl_anorms[(*((unsigned char *)data))][2];;
				model->frameinfo[i].verts[j + model->header->num_verts].normal[2] = (vec_t)model_q1mdl_anorms[(*((unsigned char *)data))][2];;
				data++;

				Sys_LoadModelQuake3BSPVec3Swizzle(model->frameinfo[i].verts[j].normal);
				Sys_LoadModelQuake3BSPVec3Swizzle(model->frameinfo[i].verts[j + model->header->num_verts].normal);

				model->frameinfo[i].verts[j].texcoord0[0] = (((vec_t)model->skin_vertices[j].s) + 0.5f) / model->header->skin_width;
				model->frameinfo[i].verts[j + model->header->num_verts].texcoord0[0] = (((vec_t)model->skin_vertices[j].s + model->header->skin_width / 2) + 0.5f) / model->header->skin_width;
				model->frameinfo[i].verts[j].texcoord0[1] = (((vec_t)model->skin_vertices[j].t) + 0.5f) / model->header->skin_height;
				model->frameinfo[i].verts[j + model->header->num_verts].texcoord0[1] = (((vec_t)model->skin_vertices[j].t) + 0.5f) / model->header->skin_height;
			}
		}
		else
		{
			Sys_Error("Sys_LoadModelQuake1MDL: %s: frame type %d not supported\n", model->name, type);
		}
	}

	model->data.vert_stride = sizeof(model_vertex_t);
	model->data.vert_count = model->header->num_verts * 2; /* double the number of verts because we have verts on seams TODO FIXME: do this in a better way, this wastes memory */

	model->data.indexes = Sys_MemAlloc(&mdl_mem, sizeof(int) * model->header->num_tris * 3, "q1model");
	model->data.index_count = model->header->num_tris * 3;
	model->data.index_stride = sizeof(int) * 3;

	for (i = 0; i < model->header->num_tris; i++)
	{
		/* rever the triangle order because of the different handness */
		if (model->skin_vertices[model->triangles[i].vertindex[2]].onseam && !model->triangles[i].front_face)
			model->data.indexes[i * 3 + 0] = model->triangles[i].vertindex[2] + model->header->num_verts;
		else
			model->data.indexes[i * 3 + 0] = model->triangles[i].vertindex[2];
		if (model->skin_vertices[model->triangles[i].vertindex[1]].onseam && !model->triangles[i].front_face)
			model->data.indexes[i * 3 + 1] = model->triangles[i].vertindex[1] + model->header->num_verts;
		else
			model->data.indexes[i * 3 + 1] = model->triangles[i].vertindex[1];
		if (model->skin_vertices[model->triangles[i].vertindex[0]].onseam && !model->triangles[i].front_face)
			model->data.indexes[i * 3 + 2] = model->triangles[i].vertindex[0] + model->header->num_verts;
		else
			model->data.indexes[i * 3 + 2] = model->triangles[i].vertindex[0];
	}

	Sys_Snprintf(model->name, MAX_MODEL_NAME, "%s", name);
	model->magic = model->header->magic;
	/* TODO CONSOLEDEBUG Sys_Printf("%s has %d verts %d tris %d skins %d frames \n", path, model->header->num_verts, model->header->num_tris, model->header->num_skins, model->header->num_frames); */

	/* load animation info */
	{
		int lowmark = Sys_MemLowMark(&tmp_mem);

		char filename[MAX_PATH];
		unsigned char *buffer, *bufferend;
		int buffersize;

		char animation_name[MAX_GAME_STRING];
		unsigned int start_frame;
		unsigned int num_frames;
		int loop;
		vec_t frames_per_second;

		/* per line: name startframe numframes loop0or1 fps */
		Sys_Snprintf(filename, MAX_PATH, "%s_animations.txt", path);
		if ((buffersize = Host_FSLoadBinaryFile(filename, &tmp_mem, "q1model", &buffer, true)) != -1)
		{
			int bufferlines = 0;
			bufferend = buffer + buffersize;
			while (1)
			{
				if (buffer >= bufferend)
					break;
#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
				if (!Sys_Sscanf_s((const char *)buffer, "%s %u %u %d %f", animation_name, &start_frame, &num_frames, &loop, &frames_per_second))
#else
				if (!Sys_Sscanf_s((const char *)buffer, "%s %u %u %d %f", animation_name, sizeof(animation_name), &start_frame, &num_frames, &loop, &frames_per_second))
#endif /* __GNUC__ */
					break;
				buffer = Host_CMDSkipBlank(buffer);
				bufferlines++;

				if (loop != 0 && loop != 1)
				{
					Host_Error("%s: property loop = %d is out of range (should be zero or 1) name = \"%s\"\n at line %d\n", filename, loop, animation_name, bufferlines); /* TODO: line counting NOT accurate */
				}
				for (i = 0; i < NUM_ANIMATIONS; i++)
				{
					if (!strncmp(animation_name, animation_names[i], MAX_GAME_STRING))
					{
						if (model->animations[i].exists)
							Host_Error("%s: animation name = \"%s\" redefined at line %d\n", filename, animation_name, bufferlines); /* TODO: line counting NOT accurate */
						model->animations[i].exists = true;
						model->animations[i].start_frame = start_frame;
						model->animations[i].num_frames = num_frames;
						model->animations[i].loop = loop;
						model->animations[i].frames_per_second = frames_per_second;
						break;
					}
				}
			}
		}

		Sys_MemFreeToLowMark(&tmp_mem, lowmark);
	}

	/* TODO CONSOLEDEBUG Sys_Printf("Loaded %s\n", path); */

	return (void *)model;
}

/*
===================
Sys_LoadModelClientDataQuake1MDL

Should only be called by Sys_LoadModelClientData, passing
model_t->data to load any textures, VBOs, etc that
the model uses.
===================
*/
void Sys_LoadModelClientDataQuake1MDL(model_q1mdl_t *model)
{
	int i, j, k;
	char texturename[MAX_TEXTURE_NAME];
	unsigned char *convertedskin;
	unsigned char *convptr;
	unsigned char *mdlptr;

	model->skin_ids = Sys_MemAlloc(&mdl_mem, sizeof(texture_t *) * model->header->num_skins, "q1model");

	mdlptr = ((unsigned char *)model->header) + sizeof(model_q1mdl_header_t);
	for (i = 0; i < model->header->num_skins; i++)
	{
		int type = *((int *)mdlptr);
		mdlptr += 4;
		if (type == QUAKE1MDL_SKINTYPE_GROUP)
		{
			Sys_Error("Sys_LoadModelClientDataQuake1MDL: %s: skin groups not supported\n", model->name);
		}
		else if (type == QUAKE1MDL_SKINTYPE_SINGLE)
		{
			convertedskin = Sys_MemAlloc(&mdl_mem, model->header->skin_width * model->header->skin_height * 4, "q1modelskin");
			Sys_Snprintf(texturename, MAX_TEXTURE_NAME, "%sskin%d", model->name, i);

			/* convert to our texture format */
			convptr = convertedskin;
			for (j = 0; j < model->header->skin_width; j++)
			{
				for (k = 0; k < model->header->skin_height; k++)
				{
					convptr[0] = model_q1mdl_pal[2 + (*mdlptr) * 3];
					convptr[1] = model_q1mdl_pal[1 + (*mdlptr) * 3];
					convptr[2] = model_q1mdl_pal[0 + (*mdlptr) * 3];
					convptr[3] = 255;
					convptr += 4;
					mdlptr++;
				}
			}

			/* TODO FIXME: Sys Calling CL? */
			model->skin_ids[i] = CL_LoadTexture(texturename, false, convertedskin, model->header->skin_width, model->header->skin_height, false, 1, 1);
		}
		else
		{
			Sys_Error("Sys_LoadModelClientDataQuake1MDL: %s: skin type %d not supported\n", model->name, type);
		}
	}

	model->interpolation = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * model->header->num_verts * 2, "q1modelinterp");
	for (i = 0; i < model->header->num_verts * 2; i++) /* *2 because of skin seams */
	{
		/* these are the same for all frames, so just copy them */
		model->interpolation[i].texcoord0[0] = model->frameinfo[0].verts[i].texcoord0[0];
		model->interpolation[i].texcoord0[1] = model->frameinfo[0].verts[i].texcoord0[1];

		model->interpolation[i].color[0] = 255;
		model->interpolation[i].color[1] = 255;
		model->interpolation[i].color[2] = 255;
		model->interpolation[i].color[3] = 255;
	}

	/* create a complete structure for a vbo that will be updated each frame */
	model->vbo_data = model->data;
	model->vbo_data.verts = model->interpolation;
	model->vbo = Sys_UploadVBO(-1, &model->vbo_data, false, true);

	/* TODO CONSOLEDEBUG Sys_Printf("Uploaded %d skins from Q1MDL to video memory\n", model->header->num_skins); */

	/* TODO do some sort of VBO (all frames in the same vbo, then select which by drawing a range? or separate vbos? */
}

/*
===================
Sys_LoadModelPhysicsTrimeshQuake1MDL

Should only be called by Sys_LoadModelPhysicsTrimesh, passing
model_t->data and destination pointers.
===================
*/
void Sys_LoadModelPhysicsTrimeshQuake1MDL(model_q1mdl_t *model, vec_t frame, model_trimesh_t **trimeshlist)
{
	if (model->trimesh_cache)
	{
		*trimeshlist = model->trimesh_cache;
	}
	else
	{
		unsigned int i;
		model_trimesh_part_t *trimesh;
		int frame_integer = (int)frame; /* TODO: round this or interpolate vertex data */
		if (frame_integer < 0 || frame_integer > model->header->num_frames)
			Host_Error("Sys_LoadModelPhysicsTrimeshQuake1MDL: %s: frame %f is out of bounds", model->name, frame);

		/* we will just copy the original vertexes */
		*trimeshlist = Sys_MemAlloc(&mdl_mem, sizeof(model_trimesh_t), "q1modeltrimesh");
		model->trimesh_cache = *trimeshlist;

		(*trimeshlist)->num_trimeshes = 1;
		(*trimeshlist)->trimeshes = Sys_MemAlloc(&mdl_mem, sizeof(model_trimesh_part_t), "q1modeltrimesh");
		trimesh = (*trimeshlist)->trimeshes;

		trimesh->verts = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * model->data.vert_count, "q1modeltrimesh");
		trimesh->vert_stride = model->data.vert_stride;
		trimesh->vert_count = model->data.vert_count;

		for (i = 0; i < trimesh->vert_count; i++)
		{
			Math_Vector3Copy(model->data.verts[i].origin, trimesh->verts[i].origin);
			Math_Vector2Copy(model->data.verts[i].texcoord0, trimesh->verts[i].texcoord0);
			Math_Vector3Copy(model->data.verts[i].normal, trimesh->verts[i].normal);
		}

		trimesh->indexes = Sys_MemAlloc(&mdl_mem, sizeof(int) * model->data.index_count, "q1modeltrimesh");
		trimesh->index_count = model->data.index_count;
		trimesh->index_stride = model->data.index_stride;

		for (i = 0; i < trimesh->index_count; i++)
			trimesh->indexes[i] = model->data.indexes[i];
	}
}

/*
===================
Sys_ModelAnimationInfoQuake1MDL

Should only be called by Sys_ModelAnimationInfo
===================
*/
void Sys_ModelAnimationInfoQuake1MDL(model_q1mdl_t *model, const unsigned int animation, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation)
{
	if (animation < 0 || animation >= NUM_ANIMATIONS)
		Sys_Error("Sys_ModelAnimationInfoQuake1MDL: animation %d out of range in model %s\n", animation, model->name);

	if (!model->animations[animation].exists)
		Sys_Error("Sys_ModelAnimationInfoQuake1MDL: animation %d not found in model %s\n", animation, model->name);

	if (start_frame)
		*start_frame = model->animations[animation].start_frame;
	if (num_frames)
		*num_frames = model->animations[animation].num_frames;
	if (loop)
		*loop = model->animations[animation].loop;
	if (frames_per_second)
		*frames_per_second = model->animations[animation].frames_per_second;
	if (multiple_slots)
		*multiple_slots = false;
	if (vertex_animation)
		*vertex_animation = true;
}

/*
===================
Sys_ModelAnimationExistsQuake1MDL

Should only be called by Sys_ModelAnimationExists
===================
*/
int Sys_ModelAnimationExistsQuake1MDL(model_q1mdl_t *model, const unsigned int animation)
{
	if (animation < 0 || animation >= NUM_ANIMATIONS)
		Sys_Error("Sys_ModelAnimationExistsQuake1MDL: animation %d out of range in model %s\n", animation, model->name);

	return model->animations[animation].exists;
}

/*
===================
Sys_ModelAABBQuake1MDL

Should only be called by Sys_ModelAABB
===================
*/
void Sys_ModelAABBQuake1MDL(model_q1mdl_t *model, const vec_t frame, vec3_t mins, vec3_t maxs)
{
	int frame_integer = (int)frame; /* TODO: round this or interpolate aabb data */
	if (frame_integer < 0 || frame_integer > model->header->num_frames)
		Host_Error("Sys_ModelAABBQuake1MDL: %s: frame %f is out of bounds", model->name, frame);

	Math_Vector3Copy(model->frameinfo[frame_integer].mins, mins);
	Math_Vector3Copy(model->frameinfo[frame_integer].maxs, maxs);
}

/*
===================
Sys_VideoDraw3DModelQuake1MDL

Should only be called by Sys_VideoDraw3DModel
Non-integer frames are for interpolation

frames[0] is the animation frame
frames[1] is the skin
frames[2] is the next animation frame (needed because we may want to interpolate from the last to the first frame)
===================
*/
#if (ANIMATION_MAX_BLENDED_FRAMES < 2)
#error "ANIMATION_MAX_BLENDED_FRAMES should be >= 2"
#endif
#if (ANIMATION_SLOT_ALLJOINTS != 0)
#error "ANIMATION_SLOT_ALLJOINTS should be == 0"
#endif
int Sys_VideoDraw3DModelQuake1MDL(model_q1mdl_t *model, vec_t *frames, const int modelent, vec3_t origin, vec3_t angles, const int anim_pitch, unsigned int desired_shader, const vec3_t ambient, const vec3_t directional, const vec3_t direction)
{
	vec3_t newangles;
	vec_t ent_modelmatrix[16];
	int frame_integer_floor = (int)floor(frames[0]);
	int frame_next_floor = (int)floor(frames[2]);
	vec_t interp;
	int skin_integer = (int)frames[1];

	if (frame_integer_floor < 0 || frame_integer_floor >= model->header->num_frames)
		Sys_Error("Sys_VideoDraw3DModelQuake1MDL: %s: base frame %f is out of bounds", model->name, frames[0]);
	if (frame_next_floor < 0 || frame_next_floor >= model->header->num_frames)
		Sys_Error("Sys_VideoDraw3DModelQuake1MDL: %s: interp frame %f is out of bounds", model->name, frames[2]);
	if (skin_integer < 0 || skin_integer >= model->header->num_skins)
		Sys_Error("Sys_VideoDraw3DModelQuake1MDL: %s: skin %f is out of bounds", model->name, frames[1]);

	Math_Vector3Copy(angles, newangles);
	if (anim_pitch)
		newangles[ANGLES_PITCH] /= 3; /* TODO: this is temporary */

	{
		int i;
		model_vertex_t *v1, *v2, *vlerp;

		Math_MatrixModel4x4(ent_modelmatrix, origin, newangles, NULL);
		Sys_VideoTransformFor3DModel(ent_modelmatrix);
		if (desired_shader == SHADER_LIGHTMAPPING)
		{
			/* set lights before transforming the matrix TODO: even the view matrix? seems ok right now, maybe because of glMatrixMode()? */
			const float ambientcol[4] = { ambient[0], ambient[1], ambient[2], 1 },
				diffusecol[4] = { directional[0], directional[1], directional[2], 1 },
				lightdir[4] = { direction[0], direction[1], direction[2], 0 };

			Sys_VideoBindShaderProgram(SHADER_FIXED_LIGHT, lightdir, diffusecol, ambientcol);
		}
		else
			Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL);

		interp = frames[0] - frame_integer_floor;
		v1 = model->frameinfo[frame_integer_floor].verts;
		v2 = model->frameinfo[frame_next_floor].verts;
		vlerp = model->interpolation;
		for (i = 0; i < model->header->num_verts * 2; i++) /* *2 because of skin seams */
		{
			vlerp->origin[0] = v1->origin[0] + interp * (v2->origin[0] - v1->origin[0]);
			vlerp->origin[1] = v1->origin[1] + interp * (v2->origin[1] - v1->origin[1]);
			vlerp->origin[2] = v1->origin[2] + interp * (v2->origin[2] - v1->origin[2]);

			vlerp->normal[0] = v1->normal[0] + interp * (v2->normal[0] - v1->normal[0]);
			vlerp->normal[1] = v1->normal[1] + interp * (v2->normal[1] - v1->normal[1]);
			vlerp->normal[2] = v1->normal[2] + interp * (v2->normal[2] - v1->normal[2]);

			v1++;
			v2++;
			vlerp++;
		}
		Sys_UpdateVBO(model->vbo, &model->vbo_data, false);
		return Sys_VideoDrawVBO(model->vbo, model->skin_ids[skin_integer]->cl_id, -1, -1, -1, -1, -1, -1, -1, -1);
		/* without vbos
		return Sys_VideoDraw3DTriangles(model->interpolation, model->data.vert_count, true, false, true, false, true, model->data.indexes, model->data.index_count, model->skin_ids[skin_integer]->cl_id, -1, -1, -1, -1, 0); */
	}
}

/*
============================================================================

Model type: Quake 2 Model

============================================================================
*/

#define QUAKE2MDL_MAGIC						('2' << 24 | 'P' << 16 | 'D' << 8 | 'I') /* little endian */
#define QUAKE2MDL_VERSION					8

#define QUAKE2MDL_TEXTURE_MAX_NAME			64
#define QUAKE2MDL_FRAME_MAX_NAME			16

typedef struct model_q2mdl_texcoord_s {
	short					s;
	short					t;
} model_q2mdl_texcoord_t;

typedef struct model_q2mdl_triangle_s {
	unsigned short			vertindex[3];
	unsigned short			texcoordindex[3];
} model_q2mdl_triangle_t;

typedef struct model_q2mdl_gltris_s {
	vec_t					s;
	vec_t					t;
	int						index;
} model_q2mdl_gltris_t;

typedef struct model_q2mdl_frame_s {
	vec3_t					scale;
	vec3_t					scale_origin;
	char					*name;

	model_vertex_t			*verts;
	vec3_t					mins;
	vec3_t					maxs;
} model_q2mdl_frame_t;

typedef struct model_q2mdl_header_s {
	int						magic;
	int						ver;
	int						skin_width;
	int						skin_height;
	int						framesize;

	int						num_skins;
	int						num_verts;
	int						num_texcoords;
	int						num_tris;
	int						num_gltris;
	int						num_frames;

	int						offset_skins;
	int						offset_texcoords;
	int						offset_tris;
	int						offset_frames;
	int						offset_gltris;
	int						offset_eof;
} model_q2mdl_header_t;

typedef struct model_q2mdl_animation_s {
	int						exists;
	unsigned int			start_frame;
	unsigned int			num_frames;
	int						loop;
	vec_t					frames_per_second;
} model_q2mdl_animation_t;

typedef struct model_q2mdl_s {
	int						magic; /* replicate for easy verifies */
	model_q2mdl_header_t	*header;

	char					name[MAX_MODEL_NAME];

	/* same index as skins */
	texture_t				**skin_ids;
	char					**skin_names;

	model_q2mdl_texcoord_t	*skin_vertices;
	model_q2mdl_triangle_t	*triangles;
	unsigned char			*gltriangles_start;
	model_trimesh_t			gltriangles; /* verts will be NULL. Because they are per-frame, they can be any of the ones in frameinfo[] */
	int						*gltriangles_is_fan;

	model_q2mdl_frame_t		*frameinfo;
	model_trimesh_part_t	data; /* data.verts will be NULL. Because they are per-frame, they can be any of the ones in frameinfo[] */

	model_q2mdl_animation_t	animations[NUM_ANIMATIONS];

	/* for drawing */
	model_vertex_t			*interpolation; /* TODO: not reentrant / not multithread safe */
	model_trimesh_part_t	vbo_data;
	int						vbo;

	/* for Sys_LoadModelPhysicsTrimesh - assume we get zero-filled memory */
	model_trimesh_t			*trimesh_cache;
} model_q2mdl_t;

const double model_q2mdl_anorms[162][3] = {
	/* In Quake's coordinate system! Swizzle before using. */
	{-0.525731, 0.000000, 0.850651},
	{-0.442863, 0.238856, 0.864188},
	{-0.295242, 0.000000, 0.955423},
	{-0.309017, 0.500000, 0.809017},
	{-0.162460, 0.262866, 0.951056},
	{0.000000, 0.000000, 1.000000},
	{0.000000, 0.850651, 0.525731},
	{-0.147621, 0.716567, 0.681718},
	{0.147621, 0.716567, 0.681718},
	{0.000000, 0.525731, 0.850651},
	{0.309017, 0.500000, 0.809017},
	{0.525731, 0.000000, 0.850651},
	{0.295242, 0.000000, 0.955423},
	{0.442863, 0.238856, 0.864188},
	{0.162460, 0.262866, 0.951056},
	{-0.681718, 0.147621, 0.716567},
	{-0.809017, 0.309017, 0.500000},
	{-0.587785, 0.425325, 0.688191},
	{-0.850651, 0.525731, 0.000000},
	{-0.864188, 0.442863, 0.238856},
	{-0.716567, 0.681718, 0.147621},
	{-0.688191, 0.587785, 0.425325},
	{-0.500000, 0.809017, 0.309017},
	{-0.238856, 0.864188, 0.442863},
	{-0.425325, 0.688191, 0.587785},
	{-0.716567, 0.681718, -0.147621},
	{-0.500000, 0.809017, -0.309017},
	{-0.525731, 0.850651, 0.000000},
	{0.000000, 0.850651, -0.525731},
	{-0.238856, 0.864188, -0.442863},
	{0.000000, 0.955423, -0.295242},
	{-0.262866, 0.951056, -0.162460},
	{0.000000, 1.000000, 0.000000},
	{0.000000, 0.955423, 0.295242},
	{-0.262866, 0.951056, 0.162460},
	{0.238856, 0.864188, 0.442863},
	{0.262866, 0.951056, 0.162460},
	{0.500000, 0.809017, 0.309017},
	{0.238856, 0.864188, -0.442863},
	{0.262866, 0.951056, -0.162460},
	{0.500000, 0.809017, -0.309017},
	{0.850651, 0.525731, 0.000000},
	{0.716567, 0.681718, 0.147621},
	{0.716567, 0.681718, -0.147621},
	{0.525731, 0.850651, 0.000000},
	{0.425325, 0.688191, 0.587785},
	{0.864188, 0.442863, 0.238856},
	{0.688191, 0.587785, 0.425325},
	{0.809017, 0.309017, 0.500000},
	{0.681718, 0.147621, 0.716567},
	{0.587785, 0.425325, 0.688191},
	{0.955423, 0.295242, 0.000000},
	{1.000000, 0.000000, 0.000000},
	{0.951056, 0.162460, 0.262866},
	{0.850651, -0.525731, 0.000000},
	{0.955423, -0.295242, 0.000000},
	{0.864188, -0.442863, 0.238856},
	{0.951056, -0.162460, 0.262866},
	{0.809017, -0.309017, 0.500000},
	{0.681718, -0.147621, 0.716567},
	{0.850651, 0.000000, 0.525731},
	{0.864188, 0.442863, -0.238856},
	{0.809017, 0.309017, -0.500000},
	{0.951056, 0.162460, -0.262866},
	{0.525731, 0.000000, -0.850651},
	{0.681718, 0.147621, -0.716567},
	{0.681718, -0.147621, -0.716567},
	{0.850651, 0.000000, -0.525731},
	{0.809017, -0.309017, -0.500000},
	{0.864188, -0.442863, -0.238856},
	{0.951056, -0.162460, -0.262866},
	{0.147621, 0.716567, -0.681718},
	{0.309017, 0.500000, -0.809017},
	{0.425325, 0.688191, -0.587785},
	{0.442863, 0.238856, -0.864188},
	{0.587785, 0.425325, -0.688191},
	{0.688191, 0.587785, -0.425325},
	{-0.147621, 0.716567, -0.681718},
	{-0.309017, 0.500000, -0.809017},
	{0.000000, 0.525731, -0.850651},
	{-0.525731, 0.000000, -0.850651},
	{-0.442863, 0.238856, -0.864188},
	{-0.295242, 0.000000, -0.955423},
	{-0.162460, 0.262866, -0.951056},
	{0.000000, 0.000000, -1.000000},
	{0.295242, 0.000000, -0.955423},
	{0.162460, 0.262866, -0.951056},
	{-0.442863, -0.238856, -0.864188},
	{-0.309017, -0.500000, -0.809017},
	{-0.162460, -0.262866, -0.951056},
	{0.000000, -0.850651, -0.525731},
	{-0.147621, -0.716567, -0.681718},
	{0.147621, -0.716567, -0.681718},
	{0.000000, -0.525731, -0.850651},
	{0.309017, -0.500000, -0.809017},
	{0.442863, -0.238856, -0.864188},
	{0.162460, -0.262866, -0.951056},
	{0.238856, -0.864188, -0.442863},
	{0.500000, -0.809017, -0.309017},
	{0.425325, -0.688191, -0.587785},
	{0.716567, -0.681718, -0.147621},
	{0.688191, -0.587785, -0.425325},
	{0.587785, -0.425325, -0.688191},
	{0.000000, -0.955423, -0.295242},
	{0.000000, -1.000000, 0.000000},
	{0.262866, -0.951056, -0.162460},
	{0.000000, -0.850651, 0.525731},
	{0.000000, -0.955423, 0.295242},
	{0.238856, -0.864188, 0.442863},
	{0.262866, -0.951056, 0.162460},
	{0.500000, -0.809017, 0.309017},
	{0.716567, -0.681718, 0.147621},
	{0.525731, -0.850651, 0.000000},
	{-0.238856, -0.864188, -0.442863},
	{-0.500000, -0.809017, -0.309017},
	{-0.262866, -0.951056, -0.162460},
	{-0.850651, -0.525731, 0.000000},
	{-0.716567, -0.681718, -0.147621},
	{-0.716567, -0.681718, 0.147621},
	{-0.525731, -0.850651, 0.000000},
	{-0.500000, -0.809017, 0.309017},
	{-0.238856, -0.864188, 0.442863},
	{-0.262866, -0.951056, 0.162460},
	{-0.864188, -0.442863, 0.238856},
	{-0.809017, -0.309017, 0.500000},
	{-0.688191, -0.587785, 0.425325},
	{-0.681718, -0.147621, 0.716567},
	{-0.442863, -0.238856, 0.864188},
	{-0.587785, -0.425325, 0.688191},
	{-0.309017, -0.500000, 0.809017},
	{-0.147621, -0.716567, 0.681718},
	{-0.425325, -0.688191, 0.587785},
	{-0.162460, -0.262866, 0.951056},
	{0.442863, -0.238856, 0.864188},
	{0.162460, -0.262866, 0.951056},
	{0.309017, -0.500000, 0.809017},
	{0.147621, -0.716567, 0.681718},
	{0.000000, -0.525731, 0.850651},
	{0.425325, -0.688191, 0.587785},
	{0.587785, -0.425325, 0.688191},
	{0.688191, -0.587785, 0.425325},
	{-0.955423, 0.295242, 0.000000},
	{-0.951056, 0.162460, 0.262866},
	{-1.000000, 0.000000, 0.000000},
	{-0.850651, 0.000000, 0.525731},
	{-0.955423, -0.295242, 0.000000},
	{-0.951056, -0.162460, 0.262866},
	{-0.864188, 0.442863, -0.238856},
	{-0.951056, 0.162460, -0.262866},
	{-0.809017, 0.309017, -0.500000},
	{-0.864188, -0.442863, -0.238856},
	{-0.951056, -0.162460, -0.262866},
	{-0.809017, -0.309017, -0.500000},
	{-0.681718, 0.147621, -0.716567},
	{-0.681718, -0.147621, -0.716567},
	{-0.850651, 0.000000, -0.525731},
	{-0.688191, 0.587785, -0.425325},
	{-0.587785, 0.425325, -0.688191},
	{-0.425325, 0.688191, -0.587785},
	{-0.425325, -0.688191, -0.587785},
	{-0.587785, -0.425325, -0.688191},
	{-0.688191, -0.587785, -0.425325}
};

/*
===================
Sys_LoadModelQuake2MDL

Should only be called by Sys_LoadModel.
Returns data pointer for the model data
===================
*/
void *Sys_LoadModelQuake2MDL(const char *name, const char *path, unsigned char *data, const size_t size)
{
	int i, j;
	unsigned char *data_start = data;
	model_q2mdl_t *model;
	model = Sys_MemAlloc(&mdl_mem, sizeof(model_q2mdl_t), "q2model");

	/* TODO: deal with endianess */
	model->header = (model_q2mdl_header_t *)data;
	if (model->header->magic != QUAKE2MDL_MAGIC)
		Sys_Error("Model %s magic number mismatch (%u should be %u)\n", path, model->header->magic, QUAKE2MDL_MAGIC);
	if (model->header->ver != QUAKE2MDL_VERSION)
		Sys_Error("Model %s version mismatch (%u should be %u)\n", path, model->header->ver, QUAKE2MDL_VERSION);

	/* TODO: check limits, check if model size in bytes has all the data it claims to have (to avoid overflows - do this for ALL models and file types, this is a security problem with downloads from a server!) */

	data = data_start + model->header->offset_skins;

	model->skin_names = Sys_MemAlloc(&mdl_mem, sizeof(char *) * model->header->num_skins, "q2model");
	for (i = 0; i < model->header->num_skins; i++)
	{
		model->skin_names[i] = data;
		data += QUAKE2MDL_TEXTURE_MAX_NAME;
	}

	model->skin_vertices = (model_q2mdl_texcoord_t *)(data_start + model->header->offset_texcoords);
	model->triangles = (model_q2mdl_triangle_t *)(data_start + model->header->offset_tris);
	model->gltriangles_start = data_start + model->header->offset_gltris;

	data = data_start + model->header->offset_frames;
	model->frameinfo = Sys_MemAlloc(&mdl_mem, sizeof(model_q2mdl_frame_t) * model->header->num_frames, "q2model");
	for (i = 0; i < model->header->num_frames; i++)
	{
		/* will be swizzled when used */
		model->frameinfo[i].scale[0] = (vec_t)(*((vec_t *)data));
		data += 4;
		model->frameinfo[i].scale[1] = (vec_t)(*((vec_t *)data));
		data += 4;
		model->frameinfo[i].scale[2] = (vec_t)(*((vec_t *)data));
		data += 4;

		model->frameinfo[i].scale_origin[0] = (vec_t)(*((vec_t *)data));
		data += 4;
		model->frameinfo[i].scale_origin[1] = (vec_t)(*((vec_t *)data));
		data += 4;
		model->frameinfo[i].scale_origin[2] = (vec_t)(*((vec_t *)data));
		data += 4;

		model->frameinfo[i].name = data;
		data += QUAKE2MDL_FRAME_MAX_NAME;

		/* just like the quake 1 model, expect now allocate triple the number of verts to account for front/back st vertices on the seams. TODO FIXME wastes lots of memory */
		model->frameinfo[i].verts = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * model->header->num_verts * 3, "q2model");
		for (j = 0; j < model->header->num_verts; j++)
		{
			/* swizzle coords */
			model->frameinfo[i].verts[j].origin[0] = (vec_t)(*((unsigned char *)data)) * model->frameinfo[i].scale[0] + model->frameinfo[i].scale_origin[0];
			data++;
			model->frameinfo[i].verts[j].origin[1] = (vec_t)(*((unsigned char *)data)) * model->frameinfo[i].scale[1] + model->frameinfo[i].scale_origin[1];
			data++;
			model->frameinfo[i].verts[j].origin[2] = (vec_t)(*((unsigned char *)data)) * model->frameinfo[i].scale[2] + model->frameinfo[i].scale_origin[2];
			data++;

			Sys_LoadModelQuake3BSPVec3Swizzle(model->frameinfo[i].verts[j].origin);
			Sys_LoadModelQuake3BSPVec3Scale(model->frameinfo[i].verts[j].origin);

			/* AABB */
			if (!j)
			{
				Math_Vector3Copy(model->frameinfo[i].verts[j].origin, model->frameinfo[i].mins);
				Math_Vector3Copy(model->frameinfo[i].verts[j].origin, model->frameinfo[i].maxs);
			}
			else
			{
				if (model->frameinfo[i].verts[j].origin[0] < model->frameinfo[i].mins[0])
					model->frameinfo[i].mins[0] = model->frameinfo[i].verts[j].origin[0];
				if (model->frameinfo[i].verts[j].origin[1] < model->frameinfo[i].mins[1])
					model->frameinfo[i].mins[1] = model->frameinfo[i].verts[j].origin[1];
				if (model->frameinfo[i].verts[j].origin[2] < model->frameinfo[i].mins[2])
					model->frameinfo[i].mins[2] = model->frameinfo[i].verts[j].origin[2];
				if (model->frameinfo[i].verts[j].origin[0] > model->frameinfo[i].maxs[0])
					model->frameinfo[i].maxs[0] = model->frameinfo[i].verts[j].origin[0];
				if (model->frameinfo[i].verts[j].origin[1] > model->frameinfo[i].maxs[1])
					model->frameinfo[i].maxs[1] = model->frameinfo[i].verts[j].origin[1];
				if (model->frameinfo[i].verts[j].origin[2] > model->frameinfo[i].maxs[2])
					model->frameinfo[i].maxs[2] = model->frameinfo[i].verts[j].origin[2];
			}

			model->frameinfo[i].verts[j].normal[0] = (vec_t)model_q2mdl_anorms[(*((unsigned char *)data))][0];
			model->frameinfo[i].verts[j].normal[1] = (vec_t)model_q2mdl_anorms[(*((unsigned char *)data))][1];
			model->frameinfo[i].verts[j].normal[2] = (vec_t)model_q2mdl_anorms[(*((unsigned char *)data))][2];;
			data++;

			Sys_LoadModelQuake3BSPVec3Swizzle(model->frameinfo[i].verts[j].normal);

			/* deal with st coordinates when parsing the triangles */
		}
	}

	/* since the texcoords are per triangle, make them per vertex (filling the original vertex we allocated and creating duplicates as needed for vertices that may have two or more st coordinates because of the seams */
	/* TODO FIXME: apparently 2 * num_verts is not enough for some custom models (which are broken) */
#define Q2MDL_MAX_ST_PER_VERTEX	4
/* #define Q2MDL_TEXCOORD_COMPARE(a, b) ((((a) - (b)) * ((a) - (b))) < 0.00001f) /* some floating point errors were happening */
	/* TODO FIXME: see if the below range doesn't make the models ugly */
#define Q2MDL_TEXCOORD_COMPARE(a, b) ((((a) - (b)) * ((a) - (b))) < 0.01f) /* some floating point errors were happening and the gltris data was slightly off - this high value minimizes gltris st comparison mismatches */
	{
		int lowmark = Sys_MemLowMark(&mdl_mem);
		int **filled_pos; /* for each vertex */
		int **filled_flag; /* for each vertex */
		int newvert_pos = model->header->num_verts; /* starting position for where to store duplicated verts but with different st coordinates */
		int k, st_slot;
		filled_pos = Sys_MemAlloc(&mdl_mem, sizeof(int *) * model->header->num_verts, "q2model_parse");
		filled_flag = Sys_MemAlloc(&mdl_mem, sizeof(int *) * model->header->num_verts, "q2model_parse");

		for (i = 0; i < model->header->num_tris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				int curvert = model->triangles[i].vertindex[j];
				int curst = model->triangles[i].texcoordindex[j];

				if (!filled_pos[curvert])
				{
					filled_pos[curvert] = Sys_MemAlloc(&mdl_mem, sizeof(int) * Q2MDL_MAX_ST_PER_VERTEX, "q2model_parse");
					filled_flag[curvert] = Sys_MemAlloc(&mdl_mem, sizeof(int) * Q2MDL_MAX_ST_PER_VERTEX, "q2model_parse");
					for (k = 0; k < model->header->num_frames; k++)
					{
						filled_pos[curvert][0] = curvert;
						filled_flag[curvert][0] = true;
						model->frameinfo[k].verts[curvert].texcoord0[0] = ((vec_t)model->skin_vertices[curst].s) / model->header->skin_width;
						model->frameinfo[k].verts[curvert].texcoord0[1] = ((vec_t)model->skin_vertices[curst].t) / model->header->skin_height;
					}
				}
				else
				{
					for (st_slot = 0; st_slot < Q2MDL_MAX_ST_PER_VERTEX; st_slot++)
					{
						if (filled_flag[curvert][st_slot])
						{
							if (Q2MDL_TEXCOORD_COMPARE((((vec_t)model->skin_vertices[curst].s) / model->header->skin_width), model->frameinfo[0].verts[filled_pos[curvert][st_slot]].texcoord0[0])
								&& Q2MDL_TEXCOORD_COMPARE((((vec_t)model->skin_vertices[curst].t) / model->header->skin_height), model->frameinfo[0].verts[filled_pos[curvert][st_slot]].texcoord0[1]))
							{
								model->triangles[i].vertindex[j] = filled_pos[curvert][st_slot];
								break;
							}
						}
						else
						{
							filled_pos[curvert][st_slot] = newvert_pos;
							filled_flag[curvert][st_slot] = true;
							for (k = 0; k < model->header->num_frames; k++)
							{
								Math_Vector3Copy(model->frameinfo[k].verts[curvert].origin, model->frameinfo[k].verts[newvert_pos].origin);
								Math_Vector3Copy(model->frameinfo[k].verts[curvert].normal, model->frameinfo[k].verts[newvert_pos].normal);
								model->frameinfo[k].verts[newvert_pos].texcoord0[0] = ((vec_t)model->skin_vertices[curst].s) / model->header->skin_width;
								model->frameinfo[k].verts[newvert_pos].texcoord0[1] = ((vec_t)model->skin_vertices[curst].t) / model->header->skin_height;
							}
							if (newvert_pos == model->header->num_verts * 3)
								Host_Error("%s: out of spare verts!\n", path);
							model->triangles[i].vertindex[j] = newvert_pos;
							newvert_pos++;
							break;
						}
					}

					if (st_slot == Q2MDL_MAX_ST_PER_VERTEX)
						Host_Error("%s: too many different ST coords for a single vertex!\n", path);
				}
			}
		}

		/* create vertex st data and triangle glcmds */
		{
			int curgltris;
			model->gltriangles.num_trimeshes = 0;
			data = model->gltriangles_start;
			while ((i = *((int *)data)) != 0)
			{
				model->gltriangles.num_trimeshes++;
				data += sizeof(int);
				if (i < 0)
					i = -i;

				for(; i > 0; i--)
				{
					model_q2mdl_gltris_t *gltris = (model_q2mdl_gltris_t *)data;

					int curvert = gltris->index;

					if (!filled_pos[curvert])
					{
						filled_pos[curvert] = Sys_MemAlloc(&mdl_mem, sizeof(int) * Q2MDL_MAX_ST_PER_VERTEX, "q2model_parse");
						filled_flag[curvert] = Sys_MemAlloc(&mdl_mem, sizeof(int) * Q2MDL_MAX_ST_PER_VERTEX, "q2model_parse");
						for (k = 0; k < model->header->num_frames; k++)
						{
							filled_pos[curvert][0] = curvert;
							filled_flag[curvert][0] = true;
							model->frameinfo[k].verts[curvert].texcoord0[0] = gltris->s;
							model->frameinfo[k].verts[curvert].texcoord0[1] = gltris->t;
						}
					}
					else
					{
						for (st_slot = 0; st_slot < Q2MDL_MAX_ST_PER_VERTEX; st_slot++)
						{
							if (filled_flag[curvert][st_slot])
							{
								if (Q2MDL_TEXCOORD_COMPARE(gltris->s, model->frameinfo[0].verts[filled_pos[curvert][st_slot]].texcoord0[0])
									&& Q2MDL_TEXCOORD_COMPARE(gltris->t, model->frameinfo[0].verts[filled_pos[curvert][st_slot]].texcoord0[1]))
								{
									gltris->index = filled_pos[curvert][st_slot];
									break;
								}
							}
							else
							{
								filled_pos[curvert][st_slot] = newvert_pos;
								filled_flag[curvert][st_slot] = true;
								for (k = 0; k < model->header->num_frames; k++)
								{
									Math_Vector3Copy(model->frameinfo[k].verts[curvert].origin, model->frameinfo[k].verts[newvert_pos].origin);
									Math_Vector3Copy(model->frameinfo[k].verts[curvert].normal, model->frameinfo[k].verts[newvert_pos].normal);
									model->frameinfo[k].verts[newvert_pos].texcoord0[0] = gltris->s;
									model->frameinfo[k].verts[newvert_pos].texcoord0[1] = gltris->t;
								}
								if (newvert_pos == model->header->num_verts * 3)
									Host_Error("%s: out of spare verts!\n", path);
								gltris->index = newvert_pos;
								newvert_pos++;
								break;
							}
						}

						if (st_slot == Q2MDL_MAX_ST_PER_VERTEX)
							Host_Error("%s: too many different ST coords for a single vertex!\n", path);
					}

					data += sizeof(model_q2mdl_gltris_t);
				}
			}

			Sys_MemFreeToLowMark(&mdl_mem, lowmark);

			curgltris = 0;
			model->gltriangles.trimeshes = Sys_MemAlloc(&mdl_mem, sizeof(model_trimesh_part_t) * model->gltriangles.num_trimeshes, "q2model_gltris");
			model->gltriangles_is_fan = Sys_MemAlloc(&mdl_mem, sizeof(int) * model->gltriangles.num_trimeshes, "q2model_gltris");
			data = model->gltriangles_start;
			while ((i = *((int *)data)) != 0)
			{
				int *index_ptr;
				data += sizeof(int);
				model->gltriangles_is_fan[curgltris] = (i < 0) ? true : false;
				if (i < 0)
					i = -i;

				model->gltriangles.trimeshes[curgltris].vert_stride = sizeof(model_vertex_t);
				model->gltriangles.trimeshes[curgltris].vert_count = model->header->num_verts * 3; /* double the number of verts because we have verts on seams TODO FIXME: do this in a better way, this wastes memory */

				model->gltriangles.trimeshes[curgltris].indexes = Sys_MemAlloc(&mdl_mem, sizeof(int) * i, "q2model");
				model->gltriangles.trimeshes[curgltris].index_count = i;
				model->gltriangles.trimeshes[curgltris].index_stride = sizeof(int) * 3;

				index_ptr = model->gltriangles.trimeshes[curgltris].indexes;
				for(; i > 0; i--)
				{
					model_q2mdl_gltris_t *gltris = (model_q2mdl_gltris_t *)data;

					*index_ptr = gltris->index;

					data += sizeof(model_q2mdl_gltris_t);
					index_ptr++;
				}

				curgltris++;
			}
		}
	}

	model->data.vert_stride = sizeof(model_vertex_t);
	model->data.vert_count = model->header->num_verts * 3; /* double the number of verts because we have verts on seams TODO FIXME: do this in a better way, this wastes memory */

	model->data.indexes = Sys_MemAlloc(&mdl_mem, sizeof(int) * model->header->num_tris * 3, "q2model");
	model->data.index_count = model->header->num_tris * 3;
	model->data.index_stride = sizeof(int) * 3;

	for (i = 0; i < model->header->num_tris; i++)
	{
		/* rever the triangle order because of the different handness */
		model->data.indexes[i * 3 + 0] = model->triangles[i].vertindex[2];
		model->data.indexes[i * 3 + 1] = model->triangles[i].vertindex[1];
		model->data.indexes[i * 3 + 2] = model->triangles[i].vertindex[0];
	}

	Sys_Snprintf(model->name, MAX_MODEL_NAME, "%s", name);
	model->magic = model->header->magic;
	/* TODO CONSOLEDEBUG Sys_Printf("%s has %d verts %d tris %d skins %d frames \n", path, model->header->num_verts, model->header->num_tris, model->header->num_skins, model->header->num_frames); */

	/* load animation info */
	{
		int lowmark = Sys_MemLowMark(&tmp_mem);

		char filename[MAX_PATH];
		unsigned char *buffer, *bufferend;
		int buffersize;

		char animation_name[MAX_GAME_STRING];
		unsigned int start_frame;
		unsigned int num_frames;
		int loop;
		vec_t frames_per_second;

		/* per line: name startframe numframes loop0or1 fps */
		Sys_Snprintf(filename, MAX_PATH, "%s_animations.txt", path);
		if ((buffersize = Host_FSLoadBinaryFile(filename, &tmp_mem, "q2model", &buffer, true)) != -1)
		{
			int bufferlines = 0;
			bufferend = buffer + buffersize;
			while (1)
			{
				if (buffer >= bufferend)
					break;
#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
				if (!Sys_Sscanf_s((const char *)buffer, "%s %u %u %d %f", animation_name, &start_frame, &num_frames, &loop, &frames_per_second))
#else
				if (!Sys_Sscanf_s((const char *)buffer, "%s %u %u %d %f", animation_name, sizeof(animation_name), &start_frame, &num_frames, &loop, &frames_per_second))
#endif /* __GNUC__ */
					break;
				buffer = Host_CMDSkipBlank(buffer);
				bufferlines++;

				if (loop != 0 && loop != 1)
				{
					Host_Error("%s: property loop = %d is out of range (should be zero or 1) name = \"%s\"\n at line %d\n", filename, loop, animation_name, bufferlines); /* TODO: line counting NOT accurate */
				}
				for (i = 0; i < NUM_ANIMATIONS; i++)
				{
					if (!strncmp(animation_name, animation_names[i], MAX_GAME_STRING))
					{
						if (model->animations[i].exists)
							Host_Error("%s: animation name = \"%s\" redefined at line %d\n", filename, animation_name, bufferlines); /* TODO: line counting NOT accurate */
						model->animations[i].exists = true;
						model->animations[i].start_frame = start_frame;
						model->animations[i].num_frames = num_frames;
						model->animations[i].loop = loop;
						model->animations[i].frames_per_second = frames_per_second;
						break;
					}
				}
			}
		}

		Sys_MemFreeToLowMark(&tmp_mem, lowmark);
	}

	/* TODO CONSOLEDEBUG Sys_Printf("Loaded %s\n", path); */

	return (void *)model;
}

/*
===================
Sys_LoadModelClientDataQuake2MDL

Should only be called by Sys_LoadModelClientData, passing
model_t->data to load any textures, VBOs, etc that
the model uses.
===================
*/
void Sys_LoadModelClientDataQuake2MDL(model_q2mdl_t *model)
{
	char texture[MAX_TEXTURE_NAME];
	int i;

	model->skin_ids = Sys_MemAlloc(&mdl_mem, sizeof(texture_t *) * model->header->num_skins, "q2model");

	for (i = 0; i < model->header->num_skins; i++)
	{
		/* TODO CONSOLEDEBUG Sys_Printf("%s %d %s\n", model->name, i, model->skin_names[i]); */
		/* TODO FIXME: Sys Calling CL? */
		Sys_Snprintf(texture, sizeof(texture), "models/%s/%s", model->name, model->skin_names[i]);
		model->skin_ids[i] = CL_LoadTexture(texture, false, NULL, 0, 0, false, 1, 1);
	}

	model->interpolation = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * model->header->num_verts * 3, "q2modelinterp");
	for (i = 0; i < model->header->num_verts * 3; i++) /* * 3 because of skin seams */
	{
		/* these are the same for all frames, so just copy them */
		model->interpolation[i].texcoord0[0] = model->frameinfo[0].verts[i].texcoord0[0];
		model->interpolation[i].texcoord0[1] = model->frameinfo[0].verts[i].texcoord0[1];

		model->interpolation[i].color[0] = 255;
		model->interpolation[i].color[1] = 255;
		model->interpolation[i].color[2] = 255;
		model->interpolation[i].color[3] = 255;
	}

	/* create a complete structure for a vbo that will be updated each frame */
	model->vbo_data = model->data;
	model->vbo_data.verts = model->interpolation;
	model->vbo = Sys_UploadVBO(-1, &model->vbo_data, false, true);

	/* TODO CONSOLEDEBUG Sys_Printf("Uploaded %d skins from Q2MDL to video memory\n", model->header->num_skins); */

	/* TODO do some sort of VBO (all frames in the same vbo, then select which by drawing a range? or separate vbos? */
}

/*
===================
Sys_LoadModelPhysicsTrimeshQuake2MDL

Should only be called by Sys_LoadModelPhysicsTrimesh, passing
model_t->data and destination pointers.
===================
*/
void Sys_LoadModelPhysicsTrimeshQuake2MDL(model_q2mdl_t *model, vec_t frame, model_trimesh_t **trimeshlist)
{
	if (model->trimesh_cache)
	{
		*trimeshlist = model->trimesh_cache;
	}
	else
	{
		unsigned int i;
		model_trimesh_part_t *trimesh;
		int frame_integer = (int)frame; /* TODO: round this or interpolate vertex data */
		if (frame_integer < 0 || frame_integer > model->header->num_frames)
			Host_Error("Sys_LoadModelPhysicsTrimeshQuake2MDL: %s: frame %f is out of bounds", model->name, frame);

		/* we will just copy the original vertexes */
		*trimeshlist = Sys_MemAlloc(&mdl_mem, sizeof(model_trimesh_t), "q2modeltrimesh");
		model->trimesh_cache = *trimeshlist;

		(*trimeshlist)->num_trimeshes = 1;
		(*trimeshlist)->trimeshes = Sys_MemAlloc(&mdl_mem, sizeof(model_trimesh_part_t), "q2modeltrimesh");
		trimesh = (*trimeshlist)->trimeshes;

		trimesh->verts = Sys_MemAlloc(&mdl_mem, sizeof(model_vertex_t) * model->data.vert_count, "q2modeltrimesh");
		trimesh->vert_stride = model->data.vert_stride;
		trimesh->vert_count = model->data.vert_count;

		for (i = 0; i < trimesh->vert_count; i++)
		{
			Math_Vector3Copy(model->data.verts[i].origin, trimesh->verts[i].origin);
			Math_Vector2Copy(model->data.verts[i].texcoord0, trimesh->verts[i].texcoord0);
			Math_Vector3Copy(model->data.verts[i].normal, trimesh->verts[i].normal);
		}

		trimesh->indexes = Sys_MemAlloc(&mdl_mem, sizeof(int) * model->data.index_count, "q2modeltrimesh");
		trimesh->index_count = model->data.index_count;
		trimesh->index_stride = model->data.index_stride;

		for (i = 0; i < trimesh->index_count; i++)
			trimesh->indexes[i] = model->data.indexes[i];
	}
}

/*
===================
Sys_ModelAnimationInfoQuake2MDL

Should only be called by Sys_ModelAnimationInfo
===================
*/
void Sys_ModelAnimationInfoQuake2MDL(model_q2mdl_t *model, const unsigned int animation, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation)
{
	if (animation < 0 || animation >= NUM_ANIMATIONS)
		Sys_Error("Sys_ModelAnimationInfoQuake2MDL: animation %d out of range in model %s\n", animation, model->name);

	if (!model->animations[animation].exists)
		Sys_Error("Sys_ModelAnimationInfoQuake2MDL: animation %d not found in model %s\n", animation, model->name);

	if (start_frame)
		*start_frame = model->animations[animation].start_frame;
	if (num_frames)
		*num_frames = model->animations[animation].num_frames;
	if (loop)
		*loop = model->animations[animation].loop;
	if (frames_per_second)
		*frames_per_second = model->animations[animation].frames_per_second;
	if (multiple_slots)
		*multiple_slots = false;
	if (vertex_animation)
		*vertex_animation = true;
}

/*
===================
Sys_ModelAnimationExistsQuake2MDL

Should only be called by Sys_ModelAnimationExists
===================
*/
int Sys_ModelAnimationExistsQuake2MDL(model_q2mdl_t *model, const unsigned int animation)
{
	if (animation < 0 || animation >= NUM_ANIMATIONS)
		Sys_Error("Sys_ModelAnimationExistsQuake2MDL: animation %d out of range in model %s\n", animation, model->name);

	return model->animations[animation].exists;
}

/*
===================
Sys_ModelAABBQuake2MDL

Should only be called by Sys_ModelAABB
===================
*/
void Sys_ModelAABBQuake2MDL(model_q2mdl_t *model, const vec_t frame, vec3_t mins, vec3_t maxs)
{
	int frame_integer = (int)frame; /* TODO: round this or interpolate aabb data */
	if (frame_integer < 0 || frame_integer > model->header->num_frames)
		Host_Error("Sys_ModelAABBQuake2MDL: %s: frame %f is out of bounds", model->name, frame);

	Math_Vector3Copy(model->frameinfo[frame_integer].mins, mins);
	Math_Vector3Copy(model->frameinfo[frame_integer].maxs, maxs);
}

/*
===================
Sys_VideoDraw3DModelQuake2MDL

Should only be called by Sys_VideoDraw3DModel
Non-integer frames are for interpolation

frames[0] is the animation frame
frames[1] is the skin
frames[2] is the next animation frame (needed because we may want to interpolate from the last to the first frame)
===================
*/
#if (ANIMATION_MAX_BLENDED_FRAMES < 2)
#error "ANIMATION_MAX_BLENDED_FRAMES should be >= 2"
#endif
#if (ANIMATION_SLOT_ALLJOINTS != 0)
#error "ANIMATION_SLOT_ALLJOINTS should be == 0"
#endif
int Sys_VideoDraw3DModelQuake2MDL(model_q2mdl_t *model, vec_t *frames, const int modelent, vec3_t origin, vec3_t angles, const int anim_pitch, unsigned int desired_shader, const vec3_t ambient, const vec3_t directional, const vec3_t direction)
{
	vec3_t newangles;
	int draw_calls_issued = false;
	vec_t ent_modelmatrix[16];
	int frame_integer_floor = (int)floor(frames[0]);
	int frame_next_floor = (int)floor(frames[2]);
	vec_t interp;
	int skin_integer = (int)frames[1];

	if (frame_integer_floor < 0 || frame_integer_floor >= model->header->num_frames)
		Sys_Error("Sys_VideoDraw3DModelQuake2MDL: %s: base frame %f is out of bounds", model->name, frames[0]);
	if (frame_next_floor < 0 || frame_next_floor >= model->header->num_frames)
		Sys_Error("Sys_VideoDraw3DModelQuake2MDL: %s: interp frame %f is out of bounds", model->name, frames[2]);
	if (skin_integer < 0 || skin_integer >= model->header->num_skins)
		Sys_Error("Sys_VideoDraw3DModelQuake2MDL: %s: skin %f is out of bounds", model->name, frames[1]);

	Math_Vector3Copy(angles, newangles);
	if (anim_pitch)
		newangles[ANGLES_PITCH] /= 3; /* TODO: this is temporary */

	{
		int i;
		model_vertex_t *v1, *v2, *vlerp;

		Math_MatrixModel4x4(ent_modelmatrix, origin, newangles, NULL);
		Sys_VideoTransformFor3DModel(ent_modelmatrix);
		if (desired_shader == SHADER_LIGHTMAPPING)
		{
			/* set lights before transforming the matrix TODO: even the view matrix? seems ok right now, maybe because of glMatrixMode()? */
			const float ambientcol[4] = { ambient[0], ambient[1], ambient[2], 1 },
				diffusecol[4] = { directional[0], directional[1], directional[2], 1 },
				lightdir[4] = { direction[0], direction[1], direction[2], 0 };

			Sys_VideoBindShaderProgram(SHADER_FIXED_LIGHT, lightdir, diffusecol, ambientcol);
		}
		else
			Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL);

		interp = frames[0] - frame_integer_floor;
		v1 = model->frameinfo[frame_integer_floor].verts;
		v2 = model->frameinfo[frame_next_floor].verts;
		vlerp = model->interpolation;
		for (i = 0; i < model->header->num_verts * 3; i++) /* * 3 because of skin seams */
		{
			vlerp->origin[0] = v1->origin[0] + interp * (v2->origin[0] - v1->origin[0]);
			vlerp->origin[1] = v1->origin[1] + interp * (v2->origin[1] - v1->origin[1]);
			vlerp->origin[2] = v1->origin[2] + interp * (v2->origin[2] - v1->origin[2]);

			vlerp->normal[0] = v1->normal[0] + interp * (v2->normal[0] - v1->normal[0]);
			vlerp->normal[1] = v1->normal[1] + interp * (v2->normal[1] - v1->normal[1]);
			vlerp->normal[2] = v1->normal[2] + interp * (v2->normal[2] - v1->normal[2]);

			v1++;
			v2++;
			vlerp++;
		}

		Sys_UpdateVBO(model->vbo, &model->vbo_data, false);
		if (Sys_VideoDrawVBO(model->vbo, model->skin_ids[skin_integer]->cl_id, -1, -1, -1, -1, -1, -1, -1, -1))
			draw_calls_issued = true;
		/* without vbos
		if (Sys_VideoDraw3DTriangles(model->interpolation, model->data.vert_count, true, false, true, false, true, model->data.indexes, model->data.index_count, model->skin_ids[skin_integer]->cl_id, -1, -1, -1, -1, 0))
			draw_calls_issued = true;
		*/
		/* TODO FIXME: reverse winding order (if odd number of vertices, abcde becomes edcba, if even, just add a degenerate triangle at the end them do the the reversal
		for (i = 0; i < model->gltriangles.num_trimeshes; i++)
		{
			if (model->gltriangles_is_fan[i])
			{
			if (Sys_VideoDraw3DTriangles(model->interpolation, model->data.vert_count, true, false, true, false, true, model->gltriangles.trimeshes[i].indexes, model->gltriangles.trimeshes[i].index_count, model->skin_ids[skin_integer]->cl_id, -1, -1, -1, -1, 2))
					draw_calls_issued = true;
			}
			else
			{
			if (Sys_VideoDraw3DTriangles(model->interpolation, model->data.vert_count, true, false, true, false, true, model->gltriangles.trimeshes[i].indexes, model->gltriangles.trimeshes[i].index_count, model->skin_ids[skin_integer]->cl_id, -1, -1, -1, -1, 1))
					draw_calls_issued = true;
			}
		}
		*/
	}

	return draw_calls_issued;
}

/*
============================================================================

Wrapper functions - this is the public interface, nothing up from here
may be called by external code!

TODO: in addition to verifying if client or server are eligible to call
the functions here, verify if it's their frame that's running!

============================================================================
*/

/*
===================
Sys_LoadModel

Should only be called by Host_LoadModel.
Returns data pointer for the model data.
basemodel is for loading submodels.
===================
*/
void *Sys_LoadModel(const char *name, void *basemodel)
{
	int size;
	char path[MAX_PATH];
	unsigned char *data;

	/* TODO: load a file with the name of the model being loaded, containing the skybox name. Since the skybox will only be loaded once per map, this is ok! */
	Sys_SkyboxLoad("daylight");

	/* the model we want is a submodel */
	if (name[0] == '*')
	{
		if (!basemodel)
			Host_Error("Sys_LoadModel: Trying to load submodel %s but basemodel == NULL!\n", name);

		switch (*(int *)basemodel) /* FIXME: do this better? */
		{
			case MODELTYPE_FACEMESH_TYPE_ID:
				Host_Error("Sys_LoadModel: Modeltype %d doesn't have submodels!\n", *(int *)basemodel); /* TODO FIXME */
				break;
			case Q3BSP_MAGIC:
				return Sys_LoadModelQuake3BSPSubModel(name, basemodel);
				break;
			case Q3BSPSM_MAGIC:
				Host_Error("Sys_LoadModel: A submodel cannot have more submodels!\n");
				break;
			case ENTITIESONLY_MAGIC:
				Host_Error("Sys_LoadModel: Modeltype %d doesn't have submodels!\n", *(int *)basemodel);
				break;
			case IQM_LE_MAGIC:
				Host_Error("Sys_LoadModel: Modeltype %d doesn't have submodels!\n", *(int *)basemodel);
				break;
			case MODELTYPE_HEIGHTMAP_TYPE_ID:
				Host_Error("Sys_LoadModel: Modeltype %d doesn't have submodels!\n", *(int *)basemodel);
				break;
			case QUAKE1MDL_MAGIC:
				Host_Error("Sys_LoadModel: Modeltype %d doesn't have submodels!\n", *(int *)basemodel);
				break;
			case QUAKE2MDL_MAGIC:
				Host_Error("Sys_LoadModel: Modeltype %d doesn't have submodels!\n", *(int *)basemodel);
				break;
			default:
				Host_Error("Sys_LoadModel: Basemodel %d for submodel %s is of unknown type!\n", *(int *)basemodel, name);
		}
	}
	else
	{
		/* search both into the models and maps subdirs, for better organization */

		/* custom facemesh */
		Sys_Snprintf(path, MAX_PATH, "models/%s.spr", name);
		/* TODO: endianness for binary formats */
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelFaceMesh(name, path, data, size);
		Sys_Snprintf(path, MAX_PATH, "maps/%s.spr", name);
		/* TODO: endianness for binary formats */
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelFaceMesh(name, path, data, size);

		/* quake 3 arena bsp */
		Sys_Snprintf(path, MAX_PATH, "models/%s.bsp", name);
		/* TODO: endianness for binary formats */
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelQuake3BSP(name, path, data, size);
		Sys_Snprintf(path, MAX_PATH, "maps/%s.bsp", name);
		/* TODO: endianness for binary formats */
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelQuake3BSP(name, path, data, size);

		/* entity file description */
		Sys_Snprintf(path, MAX_PATH, "models/%s.ent", name);
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelEntitiesOnly(path, data);
		Sys_Snprintf(path, MAX_PATH, "maps/%s.ent", name);
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelEntitiesOnly(path, data);

		/* iqm - inter quake model */
		Sys_Snprintf(path, MAX_PATH, "models/%s.iqm", name);
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelIQM(path, data);
		Sys_Snprintf(path, MAX_PATH, "maps/%s.iqm", name);
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelIQM(path, data);

		/* heightmap/heightfield */
		Sys_Snprintf(path, MAX_PATH, "models/%s.hmp", name);
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelHeightmap(name, path, data);
		Sys_Snprintf(path, MAX_PATH, "maps/%s.hmp", name);
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelHeightmap(name, path, data);

		/* quake 1 mdl */
		Sys_Snprintf(path, MAX_PATH, "models/%s.mdl", name);
		/* TODO: endianness for binary formats */
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelQuake1MDL(name, path, data, size);
		Sys_Snprintf(path, MAX_PATH, "maps/%s.mdl", name);
		/* TODO: endianness for binary formats */
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelQuake1MDL(name, path, data, size);

		/* quake 2 mdl */
		Sys_Snprintf(path, MAX_PATH, "models/%s.md2", name);
		/* TODO: endianness for binary formats */
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelQuake2MDL(name, path, data, size);
		Sys_Snprintf(path, MAX_PATH, "maps/%s.md2", name);
		/* TODO: endianness for binary formats */
		if ((size = Host_FSLoadBinaryFile(path, &mdl_mem, "modeldata", &data, true)) != -1)
			return Sys_LoadModelQuake2MDL(name, path, data, size);
	}

	Host_Error("Sys_LoadModel: Model %s not found!\n", name);

	return NULL; /* keep the compiler happy */
}

/*
===================
Sys_LoadModelClientData

Should only be called after Host_LoadModel, passing
model_t->data to load any textures, VBOs, etc that
the model uses.

Materials should be set up when loading the model itself,
this function is for setting up the materials for drawing
(GPU upload, etc)
===================
*/
void Sys_LoadModelClientData(void *modeldata)
{
	if (!cls.active)
		Host_Error("Sys_LoadModelClientData: client not active!\n");
	switch (*(int *)modeldata) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Sys_LoadModelClientDataFaceMesh(modeldata);
			break;
		case Q3BSP_MAGIC:
			Sys_LoadModelClientDataQuake3BSP(modeldata);
			break;
		case Q3BSPSM_MAGIC: /* loaded by the base model */
			break;
		case ENTITIESONLY_MAGIC:
			break;
		case IQM_LE_MAGIC:
			Sys_LoadModelClientDataIQM(modeldata);
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Sys_LoadModelClientDataHeightmap(modeldata);
			break;
		case QUAKE1MDL_MAGIC:
			Sys_LoadModelClientDataQuake1MDL(modeldata);
			break;
		case QUAKE2MDL_MAGIC:
			Sys_LoadModelClientDataQuake2MDL(modeldata);
			break;
		default:
			Host_Error("Sys_LoadModelClientData: Model %d is of unknown type!\n", *(int *)modeldata);
	}
}

/*
===================
Sys_LoadModelEntities

Should only be called after Host_LoadModel, passing
model_t->data to load any entities inside the model.
===================
*/
void Sys_LoadModelEntities(void *modeldata, int *num_entities, model_entity_t **entities)
{
	if (!svs.loading)
		Host_Error("Sys_LoadModelEntities: server not loading!\n");
	switch (*(int *)modeldata) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have entities!\n", *(int *)modeldata); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			Sys_LoadModelEntitiesQuake3BSP(modeldata, num_entities, entities, true, true);
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Submodels do not have entities!\n");
			break;
		case ENTITIESONLY_MAGIC:
			Sys_LoadModelEntitiesEntitiesOnly(modeldata, num_entities, entities);
			break;
		case IQM_LE_MAGIC:
			Host_Error("Modeltype %d doesn't have entities!\n", *(int *)modeldata);
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Sys_LoadModelEntitiesHeightmap(modeldata, num_entities, entities);
			break;
		case QUAKE1MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have entities!\n", *(int *)modeldata);
			break;
		case QUAKE2MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have entities!\n", *(int *)modeldata);
			break;
		default:
			Host_Error("Sys_LoadModelEntities: Model %d is of unknown type!\n", *(int *)modeldata);
	}
}

/*
===================
Sys_LoadModelPhysicsValidateTrimesh

Validates a trimesh, may be called at any time
===================
*/
int Sys_LoadModelPhysicsValidateTrimesh(model_trimesh_t *trimesh)
{
	int i;

	/* no trimesh */
	if (!trimesh)
		return false;

	for (i = 0; i < trimesh->num_trimeshes; i++)
		if (!(trimesh->trimeshes + i))
			return false;

	/* no data */
	for (i = 0; i < trimesh->num_trimeshes; i++)
	{
		if (!trimesh->trimeshes[i].index_count || !trimesh->trimeshes[i].vert_count)
			return false;

		/* invalid, should not happen ever, so don't print out too much info */
		if (trimesh->trimeshes[i].index_count % 3)
			Host_Error("Sys_LoadModelPhysicsValidateTrimesh: trimesh->index_count % 3\n");
	}

	return true;
}

/*
===================
Sys_LoadModelPhysicsTrimesh

Should only be called after Host_LoadModel, passing
model_t->data to load the physics data from the model.
===================
*/
void Sys_LoadModelPhysicsTrimesh(void *modeldata, model_trimesh_t **trimesh)
{
	/* we now have it client-side for prediction, so comment this...
	if (!svs.loading && !svs.listening)
		Host_Error("Sys_LoadModelPhysicsTrimesh: server not listening or loading!\n"); */
	switch (*(int *)modeldata) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Sys_LoadModelPhysicsTrimesh: Modeltype %d doesn't have a physics trimesh!\n", *(int *)modeldata); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			Sys_LoadModelPhysicsTrimeshQuake3BSP(modeldata, 0, trimesh);
			break;
		case Q3BSPSM_MAGIC:
			Sys_LoadModelPhysicsTrimeshQuake3BSPSubModel(modeldata, trimesh);
			break;
		case ENTITIESONLY_MAGIC:
			*trimesh = NULL;
			break;
		case IQM_LE_MAGIC:
			Host_Error("Sys_LoadModelPhysicsTrimesh: Modeltype %d doesn't have a physics trimesh TODO FIXME NOW!\n", *(int *)modeldata);
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID: /* TODO: we have triangle strips */
			*trimesh = NULL;
			break;
		case QUAKE1MDL_MAGIC:
			Sys_LoadModelPhysicsTrimeshQuake1MDL(modeldata, 0, trimesh); /* TODO FIXME: pass frame here! */
			break;
		case QUAKE2MDL_MAGIC:
			Sys_LoadModelPhysicsTrimeshQuake2MDL(modeldata, 0, trimesh); /* TODO FIXME: pass frame here! */
			break;
		default:
			Host_Error("Sys_LoadModelPhysicsTrimesh: Model %d is of unknown type!\n", *(int *)modeldata);
	}

	/* if the entity doesn't have any solid triangles or is otherwise invalid */
	if (!Sys_LoadModelPhysicsValidateTrimesh(*trimesh))
	{
		/* TODO CONSOLEDEBUG Sys_Printf("Sys_LoadModelPhysicsTrimesh: Model doesn't have a valid trimesh!\n"); */
		*trimesh = NULL;
	}
}

/*
===================
Sys_LoadModelPhysicsValidateBrushes

Validates a brush list, may be called at any time
===================
*/
int Sys_LoadModelPhysicsValidateBrushes(model_brushes_t *brushes)
{
	/* no brush list */
	if (!brushes)
		return false;

	/* no data */
	if (!brushes->num_brushes || !brushes->dist || !brushes->normal)
		return false;

	return true;
}

/*
===================
Sys_LoadModelPhysicsBrushes

Should only be called after Host_LoadModel, passing
model_t->data to load the physics data from the model.
===================
*/
void Sys_LoadModelPhysicsBrushes(void *modeldata, model_brushes_t **brushes)
{
	/* we now have it client-side for prediction, so comment this...
	if (!svs.loading && !svs.listening)
		Host_Error("Sys_LoadModelPhysicsBrushes: server not listening or loading!\n"); */
	switch (*(int *)modeldata) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Sys_LoadModelPhysicsBrushes: Modeltype %d doesn't have a physics brush list!\n", *(int *)modeldata); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			Sys_LoadModelPhysicsBrushesQuake3BSP(modeldata, 0, brushes);
			break;
		case Q3BSPSM_MAGIC:
			Sys_LoadModelPhysicsBrushesQuake3BSPSubModel(modeldata, brushes);
			break;
		case ENTITIESONLY_MAGIC:
			*brushes = NULL;
			break;
		case IQM_LE_MAGIC:
			Host_Error("Sys_LoadModelPhysicsBrushes: Modeltype %d doesn't have a physics brush list TODO FIXME NOW!\n", *(int *)modeldata);
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			*brushes = NULL;
			break;
		case QUAKE1MDL_MAGIC:
			Host_Error("Sys_LoadModelPhysicsBrushes: Modeltype %d doesn't have a physics brush list TODO FIXME NOW!\n", *(int *)modeldata);
			break;
		case QUAKE2MDL_MAGIC:
			Host_Error("Sys_LoadModelPhysicsBrushes: Modeltype %d doesn't have a physics brush list TODO FIXME NOW!\n", *(int *)modeldata);
			break;
		default:
			Host_Error("Sys_LoadModelPhysicsBrushes: Model %d is of unknown type!\n", *(int *)modeldata);
	}

	/* if the entity doesn't have any solid brushes or is otherwise invalid */
	if (!Sys_LoadModelPhysicsValidateBrushes(*brushes))
	{
		/* TODO CONSOLEDEBUG Sys_Printf("Sys_LoadModelPhysicsBrushes: Model doesn't have a valid brush list!\n"); */
		*brushes = NULL;
	}
}

/*
===================
Sys_LoadModelPhysicsValidateHeightfield

Validates a heightfield, may be called at any time
===================
*/
int Sys_LoadModelPhysicsValidateHeightfield(model_heightfield_t *heightfield)
{
	/* no brush list */
	if (!heightfield)
		return false;

	/* no data */
	if (!heightfield->width || !heightfield->length || !heightfield->data)
		return false;

	return true;
}

/*
===================
Sys_LoadModelPhysicsHeightfield

Should only be called after Host_LoadModel, passing
model_t->data to load the physics data from the model.
===================
*/
void Sys_LoadModelPhysicsHeightfield(void *modeldata, model_heightfield_t **heightfield)
{
	/* we now have it client-side for prediction, so comment this...
	if (!svs.loading && !svs.listening)
		Host_Error("Sys_LoadModelPhysicsHeightfield: server not listening or loading!\n"); */
	switch (*(int *)modeldata) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Sys_LoadModelPhysicsHeightfield: Modeltype %d doesn't have a heightfield!\n", *(int *)modeldata); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			*heightfield = NULL;
			break;
		case Q3BSPSM_MAGIC:
			*heightfield = NULL;
			break;
		case ENTITIESONLY_MAGIC:
			*heightfield = NULL;
			break;
		case IQM_LE_MAGIC:
			Host_Error("Sys_LoadModelPhysicsHeightfield: Modeltype %d doesn't have a heightfield!\n", *(int *)modeldata); /* TODO FIXME */
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Sys_LoadModelPhysicsHeightfieldHeightmap(modeldata, heightfield);
			break;
		case QUAKE1MDL_MAGIC:
			Host_Error("Sys_LoadModelPhysicsHeightfield: Modeltype %d doesn't have a heightfield!\n", *(int *)modeldata); /* TODO FIXME */
			break;
		case QUAKE2MDL_MAGIC:
			Host_Error("Sys_LoadModelPhysicsHeightfield: Modeltype %d doesn't have a heightfield!\n", *(int *)modeldata); /* TODO FIXME */
			break;
		default:
			Host_Error("Sys_LoadModelPhysicsHeightfield: Model %d is of unknown type!\n", *(int *)modeldata);
	}

	/* if the entity doesn't have any solid brushes or is otherwise invalid */
	if (!Sys_LoadModelPhysicsValidateHeightfield(*heightfield))
	{
		/* TODO CONSOLEDEBUG Sys_Printf("Sys_LoadModelPhysicsHeightfield: Model doesn't have a valid heightfield!\n"); */
		*heightfield = NULL;
	}
}

/*
===================
Sys_ModelAnimationInfo

Should only be called after Host_LoadModel, passing
model_t->data to return the animation info for "animation_name" in the other parameters (which are optional)
===================
*/
void Sys_ModelAnimationInfo(void *data, const unsigned int animation, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation)
{
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Sys_ModelAnimationInfoFaceMesh(data, animation, start_frame, num_frames, loop, frames_per_second, multiple_slots, vertex_animation);
			break;
		case Q3BSP_MAGIC:
			Host_Error("Modeltype %d doesn't have a animationinfo implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a animationinfo implementation!\n", *(int *)data); /* TODO FIXME: frame-changing in bmodels for pressed buttons, etc */
			break;
		case ENTITIESONLY_MAGIC:
			Host_Error("Modeltype %d doesn't have a animationinfo implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case IQM_LE_MAGIC:
			Sys_ModelAnimationInfoIQM(data, animation, start_frame, num_frames, loop, frames_per_second, multiple_slots, vertex_animation);
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a animationinfo implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE1MDL_MAGIC:
			Sys_ModelAnimationInfoQuake1MDL(data, animation, start_frame, num_frames, loop, frames_per_second, multiple_slots, vertex_animation);
			break;
		case QUAKE2MDL_MAGIC:
			Sys_ModelAnimationInfoQuake2MDL(data, animation, start_frame, num_frames, loop, frames_per_second, multiple_slots, vertex_animation);
			break;
		default:
			Host_Error("Sys_ModelAnimationInfo: Model %d is of unknown type!\n", *(int *)data);
	}
}

/*
===================
Sys_ModelAnimationExists

Should only be called after Host_LoadModel, passing
model_t->data to return true if the animation exists in the model and false otherwise
===================
*/
int Sys_ModelAnimationExists(void *data, const unsigned int animation)
{
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			return Sys_ModelAnimationExistsFaceMesh(data, animation);
			break;
		case Q3BSP_MAGIC:
			Host_Error("Modeltype %d doesn't have a animationexists implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a animationexists implementation!\n", *(int *)data); /* TODO FIXME: frame-changing in bmodels for pressed buttons, etc */
			break;
		case ENTITIESONLY_MAGIC:
			Host_Error("Modeltype %d doesn't have a animationexists implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case IQM_LE_MAGIC:
			return Sys_ModelAnimationExistsIQM(data, animation);
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a animationexists implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE1MDL_MAGIC:
			return Sys_ModelAnimationExistsQuake1MDL(data, animation);
			break;
		case QUAKE2MDL_MAGIC:
			return Sys_ModelAnimationExistsQuake2MDL(data, animation);
			break;
		default:
			Host_Error("Sys_ModelAnimationExists: Model %d is of unknown type!\n", *(int *)data);
	}

	return false; /* keep the compiler happy */
}

/*
===================
Sys_ModelStaticLightInPoint

Should only be called after Host_LoadModel, passing
model_t->data to return the static light in a point on the model in "ambient" and "directional" (both in RGB) and the direction vector to the directional light in "direction"
TODO: "point" should be carefully chosen because a point inside solid stuff may ruin the lighting for parts of the model outside the solid TODO: fix this (also being at the side of a 100% dark room may get into the lerping)
TODO FIXME: remember to take into account origin, angles, frame(?), etc of models! Currently it's okay to assume all-zeros for the world map
===================
*/
void Sys_ModelStaticLightInPoint(void *data, const vec3_t point, vec3_t ambient, vec3_t directional, vec3_t direction)
{
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a lightinpoint implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			Sys_ModelStaticLightInPointQuake3BSP(data, point, ambient, directional, direction);
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a lightinpoint implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case ENTITIESONLY_MAGIC:
			/* everything empty, so fullbright TODO: should be dark */
			ambient[0] = 1;
			ambient[1] = 1;
			ambient[2] = 1;
			directional[0] = 0;
			directional[1] = 0;
			directional[2] = 0;
			direction[0] = 0;
			direction[1] = 0;
			direction[2] = -1;
			break;
		case IQM_LE_MAGIC:
			Host_Error("Modeltype %d doesn't have a lightinpoint implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID: /* TODO: possible to have - also, what about voxels? */
			/* everything empty, so fullbright TODO: should be dark */
			ambient[0] = 1;
			ambient[1] = 1;
			ambient[2] = 1;
			directional[0] = 0;
			directional[1] = 0;
			directional[2] = 0;
			direction[0] = 0;
			direction[1] = 0;
			direction[2] = -1;
			break;
		case QUAKE1MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a lightinpoint implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE2MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a lightinpoint implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		default:
			Host_Error("Sys_ModelStaticLightInPoint: Model %d is of unknown type!\n", *(int *)data);
	}
}

/*
===================
Sys_ModelAABB

Should only be called after Host_LoadModel, passing
model_t->data to return the untransformed AABB of the model at frame "frame" (if applicable) in "mins" and "maxs"

TODO: frame is floating point to allow AABBs of interpolations, do it?
===================
*/
void Sys_ModelAABB(void *data, const vec_t frame, vec3_t mins, vec3_t maxs)
{
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			/* ignore the "frame" parameter, they are all the same size */
			Sys_ModelAABBFaceMesh(data, mins, maxs);
			break;
		case Q3BSP_MAGIC:
			if (frame)
				Host_Error("Sys_ModelAABB: Modeltype %d doesn't have frames, but a frame argument was passed.\n", *(int *)data); /* TODO FIXME: texture animation */
			Sys_ModelAABBQuake3BSP(data, mins, maxs);
			break;
		case Q3BSPSM_MAGIC:
			if (frame)
				Host_Error("Sys_ModelAABB: Modeltype %d doesn't have frames, but a frame argument was passed.\n", *(int *)data); /* TODO FIXME: texture animation */
			Sys_ModelAABBQuake3BSPSubModel(data, mins, maxs);
			break;
		case ENTITIESONLY_MAGIC:
			/* everything empty */
			if (frame)
				Host_Error("Sys_ModelAABB: Modeltype %d doesn't have frames, but a frame argument was passed.\n", *(int *)data);
			Math_ClearVector3(mins);
			Math_ClearVector3(maxs);
			break;
		case IQM_LE_MAGIC:
			Sys_ModelAABBIQM(data, frame, mins, maxs);
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			if (frame)
				Host_Error("Sys_ModelAABB: Modeltype %d doesn't have frames, but a frame argument was passed.\n", *(int *)data); /* TODO FIXME: texture animation */
			Sys_ModelAABBHeightmap(data, mins, maxs);
			break;
		case QUAKE1MDL_MAGIC:
			Sys_ModelAABBQuake1MDL(data, frame, mins, maxs);
			break;
		case QUAKE2MDL_MAGIC:
			Sys_ModelAABBQuake2MDL(data, frame, mins, maxs);
			break;
		default:
			Host_Error("Sys_ModelAABB: Model %d is of unknown type!\n", *(int *)data);
	}
}

/*
===================
Sys_ModelPointContents

Should only be called after Host_LoadModel, passing
model_t->data to return the contents of a given point inside a convex volume on the model, NOT a surface flag!
TODO FIXME: remember to take into account origin, angles, frame(?), etc of models! Currently it's okay to assume all-zeros for the world map
===================
*/
unsigned int Sys_ModelPointContents(void *data, const vec3_t point)
{
	if (!svs.listening)
		Host_Error("Sys_ModelPointContents: server not listening!\n");
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a pointcontents implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			return Sys_ModelPointContentsQuake3BSP(data, point);
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a pointcontents implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case ENTITIESONLY_MAGIC:
			return 0; /* everything empty */
		case IQM_LE_MAGIC:
			Host_Error("Modeltype %d doesn't have a pointcontents implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID: /* TODO: over and under the heightmap */
			return 0; /* everything empty */
		case QUAKE1MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a pointcontents implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE2MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a pointcontents implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		default:
			Host_Error("Sys_ModelPointContents: Model %d is of unknown type!\n", *(int *)data);
	}

	/* keep the compiler happy */
	return 0;
}

/*
===================
Sys_ModelTrace*

Should only be called after Host_LoadModel, passing
model_t->data to set a group of variables about the trace result.

input line/sphere/box:
data: the model data
start, end: the starting and ending point of the trace

input sphere:
radius: the sphere radius

input box:
mins, maxs: the mins and maxs of the axis aligned bounding box

output:
allsolid: true if the trace starts and ends inside the same solid brush (TODO: test this)
startsolid: true if the trace starts inside a solid brush
fraction: fraction of the "end" - "start" vector that was traced before hitting an obstacle
endpos: trace end. If something was hit, it's the hit point in the hit plane, if nothing was hit, it's "end"
plane_normal: normal of the plane that was hit by the trace, if fraction < 1.0
plane_dist: distance to origin of the plane that was hit by the trace, if fraction < 1.0

TODO FIXME:
remember to take into account origin, angles, frame(?), etc of models! Currently it's okay to assume all-zeros for the world map, but when you start thinking of rotating entities (like doors) things get ugly
make lots of test cases for these functions, I only did limited testing
put outputs inside a struct
can false collision positives happen with traceboxes and very BIG objects being outside brushes where no part of the AABB is completely outside the brush? This happens with most frustum culling code. See if the BSP tree makes this not happen here.
===================
*/
void Sys_ModelTraceline(void *data, vec3_t start, vec3_t end, int *allsolid, int *startsolid, vec_t *fraction, vec3_t endpos, vec3_t plane_normal, vec_t *plane_dist)
{
	if (!svs.listening)
		Host_Error("Sys_ModelTraceline: server not listening!\n");
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			Sys_ModelTraceQuake3BSP(data, start, end, 0, NULL, NULL, allsolid, startsolid, fraction, endpos, plane_normal, plane_dist);
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case ENTITIESONLY_MAGIC: /* all empty */
			*allsolid = false;
			*startsolid = false;
			*fraction = 1.0f;
			Math_Vector3Copy(end, endpos);
			break;
		case IQM_LE_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE1MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE2MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		default:
			Host_Error("Sys_ModelTraceline: Model %d is of unknown type!\n", *(int *)data);
	}
}
void Sys_ModelTracesphere(void *data, vec3_t start, vec3_t end, vec_t radius, int *allsolid, int *startsolid, vec_t *fraction, vec3_t endpos, vec3_t plane_normal, vec_t *plane_dist)
{
	if (!svs.listening)
		Host_Error("Sys_ModelTracesphere: server not listening!\n");
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			Sys_ModelTraceQuake3BSP(data, start, end, radius, NULL, NULL, allsolid, startsolid, fraction, endpos, plane_normal, plane_dist);
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case ENTITIESONLY_MAGIC: /* all empty */
			*allsolid = false;
			*startsolid = false;
			*fraction = 1.0f;
			Math_Vector3Copy(end, endpos);
			break;
		case IQM_LE_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE1MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE2MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		default:
			Host_Error("Sys_ModelTracesphere: Model %d is of unknown type!\n", *(int *)data);
	}
}
void Sys_ModelTracebox(void *data, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int *allsolid, int *startsolid, vec_t *fraction, vec3_t endpos, vec3_t plane_normal, vec_t *plane_dist)
{
	if (!svs.listening)
		Host_Error("Sys_ModelTracebox: server not listening!\n");
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			Sys_ModelTraceQuake3BSP(data, start, end, 0, mins, maxs, allsolid, startsolid, fraction, endpos, plane_normal, plane_dist);
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case ENTITIESONLY_MAGIC: /* all empty */
			*allsolid = false;
			*startsolid = false;
			*fraction = 1.0f;
			Math_Vector3Copy(end, endpos);
			break;
		case IQM_LE_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE1MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE2MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a traceline implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		default:
			Host_Error("Sys_ModelTracebox: Model %d is of unknown type!\n", *(int *)data);
	}
}

/*
===================
Sys_ModelGetTagTransform

Should only be called after Host_LoadModel, passing
model_t->data to get a tag transform in local or global coordinates (at least one of them must not be NULL)
Because of possible animations, Sys_ModelAnimate() must be called before calling this
TODO: tag scale?
===================
*/
void Sys_ModelGetTagTransform(void *data, const unsigned int tag_idx, const int local_coords, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, const int ent)
{
	if (!origin && !forward && !right && !up)
		Host_Error("Sys_ModelGetTagTransform: all outputs are NULL!\n");

	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a tag implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSP_MAGIC:
			Host_Error("Modeltype %d doesn't have a tag implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a tag implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case ENTITIESONLY_MAGIC:
			Host_Error("Modeltype %d doesn't have a tag implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case IQM_LE_MAGIC:
			Sys_ModelGetTagTransformIQM(data, tag_idx, local_coords, origin, forward, right, up, ent);
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a tag implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE1MDL_MAGIC:
			/* since we are vertex animated, the tagged model should be animated too TODO FIXME: implement tags/direction vectors per-frame*/
			Host_Error("Modeltype %d doesn't have a tag implementation!\n", *(int *)data);
			break;
		case QUAKE2MDL_MAGIC:
			/* since we are vertex animated, the tagged model should be animated too TODO FIXME: implement tags/direction vectors per-frame*/
			Host_Error("Modeltype %d doesn't have a tag implementation!\n", *(int *)data);
			break;
		default:
			Host_Error("Sys_ModelGetTagTransform: Model %d is of unknown type!\n", *(int *)data);
	}
}

/*
===================
Sys_ModelAnimate

Should only be called after Host_LoadModel, passing
model_t->data to animate the model according to modelorigin, modelangles and modelframes, for entity ent
To conserve CPU time, be sure to ONLY call this function when origin, angles and frames won't be updated
for the current frame anymore. Also try to not call if ent will be removed before drawing!
===================
*/
void Sys_ModelAnimate(void *data, const int ent, vec3_t origin, vec3_t angles, vec_t *frames, const int anim_pitch)
{
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID: /* sprites, so no need to calculate anything */
			break;
		case Q3BSP_MAGIC: /* TODO: no animations yet (texture animation do in shaders?) */
			Host_Error("Modeltype %d doesn't have a animation implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case Q3BSPSM_MAGIC: /* TODO: no animations yet (submodels being "activated", like buttons: do in shaders? */
			Host_Error("Modeltype %d doesn't have a animation implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case ENTITIESONLY_MAGIC:
			break;
		case IQM_LE_MAGIC:
			Sys_ModelAnimateIQM(data, ent, origin, angles, frames, anim_pitch);
			break;
		case MODELTYPE_HEIGHTMAP_TYPE_ID: /* TODO: no animations yet */
			Host_Error("Modeltype %d doesn't have a animation implementation!\n", *(int *)data); /* TODO FIXME */
			break;
		case QUAKE1MDL_MAGIC: /* vertex animation, so no need to calculate anything */
			break;
		case QUAKE2MDL_MAGIC: /* vertex animation, so no need to calculate anything */
			break;
		default:
			Host_Error("Sys_ModelAnimate: Model %d is of unknown type!\n", *(int *)data);
	}
}

/*
===================
Sys_ModelIsStatic

Should only be called after Host_LoadModel, passing model_t->data
Called to know if a model is part of scenery, for better occlusion queries

TODO: better ways to classify models as static or not!
===================
*/
int Sys_ModelIsStatic(void *data)
{
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			return false;
		case Q3BSP_MAGIC:
			return true;
		case Q3BSPSM_MAGIC:
			return true;
		case ENTITIESONLY_MAGIC:
			return true; /* whatever */
		case IQM_LE_MAGIC:
			return false;
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			return true;
		case QUAKE1MDL_MAGIC:
			return false;
		case QUAKE2MDL_MAGIC:
			return false;
		default:
			Host_Error("Sys_ModelIsStatic: Model %d is of unknown type!\n", *(int *)data);
	}

	/* keep the compiler happy */
	return 0;
}

/*
===================
Sys_ModelHasPVS

Should only be called after Host_LoadModel, passing model_t->data
Called to know if a model has PVS data
===================
*/
int Sys_ModelHasPVS(void *data)
{
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			return false;
		case Q3BSP_MAGIC:
			return Sys_ModelHasPVSQuake3BSP(data);
		case Q3BSPSM_MAGIC:
			return false;
		case ENTITIESONLY_MAGIC:
			return false;
		case IQM_LE_MAGIC:
			return false;
		case MODELTYPE_HEIGHTMAP_TYPE_ID: /* TODO: implement */
			return false;
		case QUAKE1MDL_MAGIC:
			return false;
		case QUAKE2MDL_MAGIC:
			return false;
		default:
			Host_Error("Sys_ModelHasPVS: Model %d is of unknown type!\n", *(int *)data);
		 /* TODO: also implement for voxels, currently culling is done with the gpu client-side */
	}

	/* keep the compiler happy */
	return 0;
}

/*
===================
Sys_ModelPVSGetClustersBox

Should only be called after Host_LoadModel, passing model_t->data
Called to get a list of all the clusters a box touches

"mins" and "maxs" are absolute (if the entity with the bsp model is at origin with no rotation and scale)
"clusters" is an array
"num_clusters" is a pointer to the above array size
===================
*/
void Sys_ModelPVSGetClustersBox(void *data, const vec3_t absmins, const vec3_t absmaxs, int *clusters, int *num_clusters, const int max_clusters)
{
	*num_clusters = 0;

	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case Q3BSP_MAGIC:
			Sys_ModelPVSGetClustersBoxQuake3BSP(data, absmins, absmaxs, clusters, num_clusters, max_clusters, 0);
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case ENTITIESONLY_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case IQM_LE_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data); /* TODO FIXME */
		case QUAKE1MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case QUAKE2MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		default:
			Host_Error("Sys_ModelHasPVS: Model %d is of unknown type!\n", *(int *)data);
	}
}

/*
===================
Sys_ModelPVSCreateFatPVSClusters

Checks which clusters should be added to the fat PVS.
One of the reasons for a fat PVS is head bobbing.
===================
*/
void Sys_ModelPVSCreateFatPVSClusters(void *data, const vec3_t eyeorigin)
{
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case Q3BSP_MAGIC:
			Sys_ModelPVSCreateFatPVSClustersQuake3BSP(data, eyeorigin);
			break;
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case ENTITIESONLY_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case IQM_LE_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data); /* TODO FIXME */
		case QUAKE1MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case QUAKE2MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		default:
			Host_Error("Sys_ModelPVSCreateFatPVSClusters: Model %d is of unknown type!\n", *(int *)data);
	}
}


/*
===================
Sys_ModelPVSTestFatPVSClusters

Tests a cluster list against a fat PVS
===================
*/
int Sys_ModelPVSTestFatPVSClusters(void *data, const int *clusters, const int num_clusters)
{
	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case Q3BSP_MAGIC:
			return Sys_ModelPVSTestFatPVSClustersQuake3BSP(data, clusters, num_clusters);
		case Q3BSPSM_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case ENTITIESONLY_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case IQM_LE_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data); /* TODO FIXME */
		case QUAKE1MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		case QUAKE2MDL_MAGIC:
			Host_Error("Modeltype %d doesn't have a PVS implementation!\n", *(int *)data);
		default:
			Host_Error("Sys_ModelPVSTestFatPVSClusters: Model %d is of unknown type!\n", *(int *)data);
	}

	/* keep the compiler happy */
	return 0;
}

/*
===================
Sys_VideoDraw3DModel

Should only be called after Sys_VideoSet3D
TODO FIXME: replace this function ENTIRELY
TODO: batch and sort ALL polygons (for early-z and transparent polygon correct order) (also sort by texture?)
TODO: need frustum planes for culling
FIXME: need to err if a face texture has alpha but the face isn't marked as transparent (in models that have PVS or anything like that)
TODO: PVS
Returns true if draw calls were issued.
===================
*/
int Sys_VideoDraw3DModel(void *data, vec3_t eyeorigin, vec3_t eyeangles, vec3_t modelorigin, vec3_t modelangles, const int anim_pitch, vec_t *modelframes, int modelent, unsigned int desired_shader)
{
	if (!cls.active)
		Host_Error("Sys_VideoDraw3DModel: client not active!\n");

	if (!data)
		Sys_Error("Sys_VideoDraw3DModel: null data!\n");

	switch (*(int *)data) /* FIXME: do this better? */
	{
		case MODELTYPE_FACEMESH_TYPE_ID:
			return Sys_VideoDraw3DModelFaceMesh(data, modelframes, modelent, modelorigin, eyeangles, desired_shader); /* TODO: make an oriented type for decals */
		case Q3BSP_MAGIC:
#ifdef BRUTEFORCE_BATCH_QUAKE3BSP
			return Sys_VideoDraw3DModelQuake3BSP(data, modelent, modelorigin, modelangles, desired_shader);
#else /* BRUTEFORCE_BATCH_QUAKE3BSP */
			return Sys_VideoDraw3DModelQuake3BSP(data, modelent, eyeorigin, modelorigin, modelangles, desired_shader);
#endif /* BRUTEFORCE_BATCH_QUAKE3BSP */
		case Q3BSPSM_MAGIC:
			return Sys_VideoDraw3DModelQuake3BSPSubModel(data, modelent, modelorigin, modelangles, desired_shader);
		case ENTITIESONLY_MAGIC: /* empty */
			return false;
		case IQM_LE_MAGIC:
			{
				/* TODO: use these lights for quake 3 bsp submodels? */
				/* TODO FIXME: do this right, pass lights through network, see about direction vector coordinate space: model, view, world? convert? */
				vec3_t ambient, directional, direction;
				if (desired_shader == SHADER_LIGHTMAPPING)
					Sys_ModelStaticLightInPoint(cls.precached_models[1]->data, modelorigin, ambient, directional, direction); /* TODO: sys calling cl, get basemodel from server - we may have more than one "lightmapped" source for lights? (closed bsp locatins in heightfields, etc) */
				return Sys_VideoDraw3DModelIQM(data, modelframes, modelent, modelorigin, modelangles, anim_pitch, desired_shader, ambient, directional, direction);
			}
		case MODELTYPE_HEIGHTMAP_TYPE_ID:
			{
				vec_t rate = (((vec_t)host_realtime) / 10000.f);
				/* TODO: use these lights for quake 3 bsp submodels? */
				/* TODO FIXME: do this right, pass lights through network, see about direction vector coordinate space: model, view, world? convert? */
				vec3_t ambient = {0.3f, 0.3f, 0.3f};
				/*vec3_t directional = {0.7f, 0.7f, 1}; /* morning */
				/*vec3_t directional = {1, 0.5, 0.5}; /* afternoon */
				vec3_t directional = {1, 1, 1}; /* day */
				vec3_t direction;
				while (rate >= 2.f * (vec_t)M_PI)
					rate -=  2.f * (vec_t)M_PI;
				direction[0] = sinf(rate);
				direction[1] = 0.5;
				direction[2] = cosf(rate);
				Math_Vector3Normalize(direction);
				return Sys_VideoDraw3DModelHeightmap(data, eyeorigin, modelent, modelorigin, modelangles, desired_shader, ambient, directional, direction);
			}
		case QUAKE1MDL_MAGIC:
			{
				/* TODO: use these lights for quake 3 bsp submodels? */
				/* TODO FIXME: do this right, pass lights through network, see about direction vector coordinate space: model, view, world? convert? */
				vec3_t ambient, directional, direction;
				if (desired_shader == SHADER_LIGHTMAPPING)
					Sys_ModelStaticLightInPoint(cls.precached_models[1]->data, modelorigin, ambient, directional, direction); /* TODO: sys calling cl, get basemodel from server - we may have more than one "lightmapped" source for lights? (closed bsp locatins in heightfields, etc) */
				return Sys_VideoDraw3DModelQuake1MDL(data, modelframes, modelent, modelorigin, modelangles, anim_pitch, desired_shader, ambient, directional, direction);
			}
		case QUAKE2MDL_MAGIC:
			{
				/* TODO: use these lights for quake 3 bsp submodels? */
				/* TODO FIXME: do this right, pass lights through network, see about direction vector coordinate space: model, view, world? convert? */
				vec3_t ambient, directional, direction;
				if (desired_shader == SHADER_LIGHTMAPPING)
					Sys_ModelStaticLightInPoint(cls.precached_models[1]->data, modelorigin, ambient, directional, direction); /* TODO: sys calling cl, get basemodel from server - we may have more than one "lightmapped" source for lights? (closed bsp locatins in heightfields, etc) */
				return Sys_VideoDraw3DModelQuake2MDL(data, modelframes, modelent, modelorigin, modelangles, anim_pitch, desired_shader, ambient, directional, direction);
			}
		default:
			Sys_Error("Sys_VideoDraw3DModel: Trying to draw an invalid model %d\n", *(int *)data);
	}

	/* keep the compiler happy */
	return false;
}
