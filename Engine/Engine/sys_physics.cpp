/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

#define BT_NO_PROFILE /* TODO: this needs to be set here or when compiling bullet?  Does this make any difference in performance? */

#include <btBulletDynamicsCommon.h>
#include <LinearMath/btGeometryUtil.h>
#include <BulletCollision/NarrowPhaseCollision/btRaycastCallback.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <BulletCollision/CollisionShapes/btTriangleShape.h>
#include <BulletCollision/CollisionShapes/btShapeHull.h>
#include <BulletWorldImporter/btBulletWorldImporter.h>

/* these are for stepping only one entity */
int saved_entity_to_simulate = -1;
int saved_activation_states[MAX_EDICTS]; /* TODO: again... use linked lists for iterating EVERYWHERE in this file... */
int saved_active[MAX_EDICTS];
int **saved_async_entities; /* only valid DURING a simulation step with saved_entity_to_simulate == -1 */
int cached_should_activate[MAX_EDICTS]; /* used to activate entities that were removed (because of async frames) */

int isAsync(int entity)
{
	if (saved_entity_to_simulate != -1)
	{
		if (entity == saved_entity_to_simulate)
			return true;
		return false;
	}

	int *i = *saved_async_entities;
	for (; *i != -1; i++)
		if (*i == entity)
			return true;
	return false;
}

/*
	=========================
	IMPORTANT!!!!!!!!!!!!!!!!

	When adding ANYTHING
	here, update the save/
	load code to handle the
	new properties
	=========================
*/
/* TODO: CACHE EVERY Game_SV_Get* WITH ONLY ONE FUNCTION CALL (TO REDUCE CALLING OVERHEAD) AT THE START OF THE FRAME! MAINLY FOR LUA GAMECODE */
/* TODO: splitimpulse for restitution problems? */
/* TODO: forceupdateallaabbs = false for performance? */
/* TODO: lots of places that we should check if a btCollisionObject is a btRigidBody before casting */
/* TODO: crouching and removing the voxels beneath the player in level2 (the voxelworld) won't make the player fall until he moves. In other maps this seen to work! Probably due to my forced sleep and/or relinking and activation! */
/* TODO: tracelines are 4cm off surfaces, because of the collision margin */
/* FIXME: bullet stuff is not using our memory pools, because of this we must take care about what we must free and what will be automatically freed */
/* TODO: really test CCD (all shapes against all shapes, swept sphere min and max size) and address the fact that really fast objects may reflect/get past the impact point before the touch function is called (player capsule vs cube convex shapes below causes tunneling with high speeds and release mode!) (I've seen cubes pass through doors when very fast after explosions in facility/felicity) (see if the new ccd code fix these two issues) */
/* TODO: when doing collision checks, try to limit the mass ratio between entities (to a maxium of 16, for example) */
/* FIXME: sync btScalar type to vec_t type */
/* TODO: investigate why it segfaults when forcing quit after a "too many shapes for entity %d" message */
/* TODO: using phys_substep_frametime when should be using TOTAL frametime? */
/*
	TODO:
	Make sure all bodies in an entity use the SAME and get THE SAME stimuli. This means that ragdolls should use one entity per bone?
	Or maybe have the FIRST always be the "control" body? Or remove the player body and spawn the ragdoll after the player dies?
	Whatever, just have a way of adding forces to multi-body entities!
	Make the rendering system use the bones the same way we use the bodies?
*/
/*
	TODO: world collision shape and other entities (and rays) collision shape
	For animated models!
	PHYSICS_SHAPE_RAGDOLL_MODEL:
	Just create the ragdoll model with the bones and joints from the model. For the collision shapes, use a capped cylinder or a convex hull (both defined by the vertexes attached to each bone)
	Per frame, send each bone position and angles (need origin and angles to be a array in game_edict_t!) Client just passes the multiple origins and angles to the sys_models.c renderer and it does the rest.
	PHYSICS_SHAPE_ANIMATED_MODEL:
	Create a collision model (using a capped cylinder or convex hull (both defined by all vertexes in the model in a base pose or let the code set? optionally adjust for each animation frame?)
	Fix all the collision model axis, angles will be set in phys_edict_t and won't be part of the physics simulation. (let the code decide if yaw will be free?)
	Create a projectile collision model (for accurate collision with small stuff) just like the ragdoll model, but the bone transform will be updated using the collision model origin, the phys_edict_t angles and then the animation info from the model.)
	Let the yaw angle affect all bones, the roll angle affect from the torso upwards or only the head, the pitch angle only the shoulders upwards or only the head. Let the game code decide which angles affect which bones or store this in a file?
	Per frame, send each bone position and angles (need origin and angles to be a array in game_edict_t!) Client just passes the multiple origins and angles to the sys_models.c renderer and it does the rest.
	PHYSICS_SHAPE_*:
	Document them and usage.
	Consolidate movetypes (some only allowed for some models? set while creating?)
*/
/*
TODO: auto-vectorization will have better performance? also, see if in the lib it was auto-vectorization or manual
Also may cause problems when we accesss components individually or pass pointers assuming btScalar/btVector3 <-> vec_t/vec3_t compatibility, or not?
#define BT_USE_SIMD_VECTOR3
#define BT_USE_SSE
#define BT_USE_SSE_IN_API
*/

#define USE_CONTINUOUS
/* #define FORCED_SLEEP */ /* TODO: this messes with kinematic bodies moving stuff */

#define PHYS_SUBSTEPSIZE			((1.0 / (double)HOST_MAX_FPS)) /* frequency of the simulated world */
#define PHYS_MAXSUBSTEPS			10

#define PHYS_ONGROUND_DIST		(0.1)	/* distance in centimeters for onground, when casting rays */
#define PHYS_SLOPE_COEFFICIENT	(0.5)	/*
											threshold for the onground setting, we are onground if we are above something else and the
											upward component of the contact normal of the contact plane is more than 30 degreess (sin 0.5)
											to the XZ plane, which means that slopes must have no more than 60 degrees.
										*/
#define PHYS_ONGROUND_CLEAR_THRESHOLD		50 /* time in miliseconds before clearing the onground flag due to no contacts */
#define SET_CONTACT_PROCESSING_THRESHOLD		body->setContactProcessingThreshold(CONVEX_DISTANCE_MARGIN)
/*#define SET_CONTACT_PROCESSING_THRESHOLD		body->setContactProcessingThreshold(1.)	/*
																						positive values increase stability but may cause
																						internal edges (edges shared between consecutive
																						coplanar faces in the same body or in different
																						bodies) to be hit and add jerkyness to the motion,
																						negative values decrease stability and may cause
																						"bouncing" when standing still.
																						Even with this option set, it's still possible
																						to hit internal edges when falling into the
																						scenery... (after jumping, etc) :(
																						Internal edge collision is especially bad when
																						dealing with stacked bodies, it may cause a false
																						hit with the top and bottom faces in the middle
																						of the stack.
																						TODO FIXME: do this right
																						Triangle meshes have a fix, at least for internal
																						edges on the same body/shape (see ConcavePhysicsDemo.cpp)
																					*/

/* bitfields for collision categories, when adding new categories, remember to set ALL interactions with it */
/* FIXME: the first few were already in use by Bullet, maybe some more will be added? */
#define COLLISION_CATEGORY_WORLD		64
#define COLLISION_CATEGORY_ENTITY		128
#define COLLISION_CATEGORY_TRIGGER		256

/*
	Collision data

	Because the physics code may run more steps per frame than the game logic code, we store
	the collisions in an array of the following struct.
	To speed up the checking it's an simple list, but we also have a matrix
	to see if a collision was already added to the list.
*/

typedef struct phys_collision_s
{
	entindex_t		self;
	entindex_t		other;

	vec3_t			pos;
	vec3_t			normal;
	vec_t			distance;
	int				reaction;
	vec_t			impulse;
} phys_collision_t;

/* additional data for vehicles */
typedef struct phys_edict_vehicle_extra_data_s {
	phys_edict_vehicle_info_t					construction_info;
	btRaycastVehicle::btVehicleTuning			m_tuning;
	btVehicleRaycaster							*m_vehicleRayCaster;
	btRaycastVehicle							*m_vehicle;
	btCollisionShape							*m_wheelShape;
} phys_edict_vehicle_extra_data_t;

/* entity representation data */
typedef struct phys_edict_s {
	int					active; /* the physics simulation, not the entity */
	entindex_t			index; /* easy way to verify who we are in callbacks */
	void				*myworld; /* pointer to our physics_world_t */
	vec_t				mass;
	unsigned int		solid, solid_group, solid_mask;
	int					onground; /* TODO: only when needed (then cache for one frame) */
	btVector3			onground_normal; /* TODO: pass this to the gamecode, average all contact points */
	int					ground_entity; /* TODO: make array, return empty when not onground */
	int					trace_onground; /* if we are going to use tracelines to better determine onground */
	mstime_t			lastonground;
	int					jumped_this_frame; /* to prevent long jumps during substeps */
	btCollisionShape	*shape; /* TODO: bullet recommends that we make sure to re-use collision shapes among rigid bodies whenever possible! This means caching per model and not per entity! */
	btRigidBody			*body;
	btQuaternion		globalspace_rotation; /* saved here because we may want to lock some angles in the physical object but still have them set at the game-level */
	int					relink; /* true if we need to relink and update all caches about this entity after changing stuff like transform and collision flags */
	int					type;
	double				sleep_counter; /* for manual sleeping TODO FIXME: shouldn't be needed, things are NOT sleeping and I don't know why! */
	int					sleep_state; /* for manual sleeping TODO FIXME: shouldn't be needed, things are NOT sleeping and I don't know why! */
	btScalar			collision_margin; /* cache for penetration depth for the shape */
	void				*extra_data; /* for stuff we must keep for bullet, like heightfield data */
	btScalar			radius; /* for some types, stores the amount the object can move per second without triggering ccd */
	vec3_t				creation_vecs;
	precacheindex_t		creation_model;
	btVector3			cached_linearvelocity; /* this should NOT be updated mid-simulation */
} phys_edict_t;

/* voxel representation data */
typedef struct phys_voxel_chunk_s {
	/* entity index will always be world, this is fine for now but BEWARE if someday we start returning which body/brush/triangle we hit, for example. for now it will collide with a world phys_ent with default values if it isn't spawned TODO */
	int					active;
	btCollisionShape	*shape;
	btRigidBody			*body;
	int					type;
} phys_voxel_chunk_t;

/*
===================
Sys_PhysicsCollisionDispatcher

Class for fine grained control over collisions and responses
TODO: better to use btOverlapFilterCallback?
===================
*/
int Physics_CheckPhysicalCollisionResponse(void *w_void, entindex_t e1, entindex_t e2);
class Sys_PhysicsCollisionDispatcher : public btCollisionDispatcher
{
public:
	Sys_PhysicsCollisionDispatcher(btDefaultCollisionConfiguration *dcc)
		:btCollisionDispatcher(dcc)
	{
	}
	virtual ~Sys_PhysicsCollisionDispatcher()
	{
	}

	/* TODO: use btCollisionObject::CF_NO_CONTACT_RESPONSE instead of this? Will it disable ccd? */
	bool needsResponse(const btCollisionObject *body0, const btCollisionObject *body1)
	{
		phys_edict_t *b0 = (phys_edict_t *)body0->getCollisionShape()->getUserPointer();
		phys_edict_t *b1 = (phys_edict_t *)body1->getCollisionShape()->getUserPointer();

		/* other cases will be handled by collision masks, but we want collision events to happen to triggers */
		if (b0->solid == SOLID_WORLD_TRIGGER || b1->solid == SOLID_WORLD_TRIGGER)
			return false;
		if (b0->solid == SOLID_ENTITY_TRIGGER && !(b1->solid_group & COLLISION_CATEGORY_WORLD))
			return false;
		if (b1->solid == SOLID_ENTITY_TRIGGER && !(b0->solid_group & COLLISION_CATEGORY_WORLD))
			return false;
		if (!Physics_CheckPhysicalCollisionResponse(b0->myworld, b0->index, b1->index))
			return false;

		return true;
	}
};

/* TODO IMPORTANT: keep these in sync between client and server, otherwise prediction will fail */
static cvar_t								*phys_gravity_y;
static cvar_t								*phys_maxspeed_linear;
static cvar_t								*phys_maxspeed_angular;
static cvar_t								*phys_debug_draw;
static cvar_t								*phys_axis_pitch_rate;
static cvar_t								*phys_axis_yaw_rate;
static cvar_t								*phys_axis_roll_rate;

/*
============================================================================

Physics debugging START

Needs a server and a client on the same machine to work

TODO: what if we have a server started and the client connects to another machine? check this (even though it should be checked in the client initialization too!)
TODO: move drawing code to the video code, only build command list here!
TODO: do not link GL code if we are building a dedicated executable

============================================================================
*/
#if 0
#include <GL/glew.h>
#endif

/*
===================
GLDebugDrawer

Class for debug drawing
===================
*/
class GLDebugDrawer : public btIDebugDraw
{
	int m_debugMode;

public:
	GLDebugDrawer();
	virtual ~GLDebugDrawer();

	virtual void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &fromColor, const btVector3 &toColor);
	virtual void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color);
	virtual void drawTriangle(const btVector3 &a, const btVector3 &b, const btVector3 &c, const btVector3 &color, btScalar alpha);
	virtual void drawContactPoint(const btVector3 &pointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color);
	virtual void draw3dText(const btVector3 &location, const char *textString);

	virtual void reportErrorWarning(const char *warningString);

	virtual void setDebugMode(int debugMode);
	virtual int getDebugMode() const
	{
		return m_debugMode;
	}
};

GLDebugDrawer::GLDebugDrawer()
:m_debugMode(0)
{
}

GLDebugDrawer::~GLDebugDrawer()
{
}

void GLDebugDrawer::drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &fromColor, const btVector3 &toColor)
{
#if 0
	glBegin(GL_LINES);
	glColor3f(fromColor.getX(), fromColor.getY(), fromColor.getZ());
	glVertex3d(from.getX(), from.getY(), from.getZ());
	glColor3f(toColor.getX(), toColor.getY(), toColor.getZ());
	glVertex3d(to.getX(), to.getY(), to.getZ());
	glEnd();
#endif
}

void GLDebugDrawer::drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color)
{
	drawLine(from, to, color, color);
}

void GLDebugDrawer::drawTriangle(const btVector3 &a, const btVector3 &b, const btVector3 &c, const btVector3 &color, btScalar alpha)
{
#if 0
	const btVector3	n = btCross(b - a, c - a).normalized();
	glBegin(GL_TRIANGLES);
	glColor4f(color.getX(), color.getY(), color.getZ(), alpha);
	glNormal3d(n.getX(), n.getY(), n.getZ());
	glVertex3d(a.getX(), a.getY(), a.getZ());
	glVertex3d(b.getX(), b.getY(), b.getZ());
	glVertex3d(c.getX(), c.getY(), c.getZ());
	glEnd();
#endif
}

void GLDebugDrawer::drawContactPoint(const btVector3 &pointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color)
{
#if 0
	btVector3 to = pointOnB + normalOnB * 1; /* distance; */
	const btVector3 &from = pointOnB;
	glColor4f(color.getX(), color.getY(), color.getZ(), 1.f);
	glBegin(GL_LINES);
	glVertex3d(from.getX(), from.getY(), from.getZ());
	glVertex3d(to.getX(), to.getY(), to.getZ());
	glEnd();
#endif
}

void GLDebugDrawer::draw3dText(const btVector3 &location, const char *textString)
{
#if 0
	glRasterPos3f(location.x(), location.y(), location.z());
	/* TODO */
#endif
}

void GLDebugDrawer::reportErrorWarning(const char *warningString)
{
	Sys_Printf("GLDebugDrawer::reportErrorWarning: %s\n", warningString);
}

void GLDebugDrawer::setDebugMode(int debugMode)
{
	m_debugMode = debugMode;
}

/*
============================================================================

Physics debugging END

============================================================================
*/

typedef struct physics_world_s {
	int									server;
	/* world stuff TODO: save/load for these */
	btDefaultCollisionConfiguration		*collision_configuration;
	Sys_PhysicsCollisionDispatcher		*dispatcher;
	btBroadphaseInterface				*overlapping_pair_cache;
	btSequentialImpulseConstraintSolver	*solver;
	btDiscreteDynamicsWorld				*dynamics_world;
	mstime_t							phys_time;
	double								phys_substep_frametime;
	GLDebugDrawer						*debug_drawer;

	int									phys_num_collisions;
	phys_collision_t					phys_collision_list[MAX_EDICTS * MAX_EDICTS];
	framenum_t							phys_collision_matrix[MAX_EDICTS][MAX_EDICTS];

	phys_edict_t						phys_entities[MAX_EDICTS]; /* same index as entities */

	phys_voxel_chunk_t					phys_voxel_chunks[VOXEL_MAX_CHUNKS]; /* same index as chunks */
} physics_world_t;

/*
===================
Sys_PhysicsDebugDraw

Debug drawing
===================
*/
void Sys_PhysicsDebugDraw(void *physworld)
{

	/*
		TODO:

		setDebugMode according to cvars

		Disabling debug drawing for a specific object:
		int f = obj->getCollisionFlags();
		obj->setCollisionFlags(f | btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT)
	*/
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	if (w->debug_drawer && phys_debug_draw->doublevalue) /* so that we only draw this if we have a local server. This function already only gets called if we have a local client */
		w->dynamics_world->debugDrawWorld();
}

/*
============================================================================

External data management

============================================================================
*/

/*
===================
Physics_UpdatePhysStats

Called by the physics code to update physics data for any entity with an active physical representation
===================
*/
void Physics_UpdatePhysStats(physics_world_t *w, entindex_t ent, vec3_t origin, vec3_t angles, vec3_t velocity, vec3_t avelocity, int onground)
{
	if (w->server)
		Game_SV_UpdatePhysStats(ent, origin, angles, velocity, avelocity, onground);
	else
		CL_PredUpdatePhysStats(ent, origin, angles, velocity, avelocity, onground);
}

/*
===================
Physics_UpdatePhysDirections

Called by the physics code to update physics data for any entity with an active physical representation
===================
*/
void Physics_UpdatePhysDirections(physics_world_t *w, entindex_t ent, vec3_t forward, vec3_t right, vec3_t up)
{
	if (w->server)
		Game_SV_UpdatePhysDirections(ent, forward, right, up);
	else
		CL_PredUpdatePhysDirections(ent, forward, right, up);
}

/*
===================
Physics_UpdateTraceResultStart

Called by the physics code to begin updating traceline results
===================
*/
void Physics_UpdateTraceResultStart(physics_world_t *w, entindex_t ent)
{
	if (w->server)
		Game_SV_UpdateTraceResultStart(ent);
	else
		CL_PredUpdateTraceResultStart(ent);
}

/*
===================
Physics_UpdateTraceResultStep

Called by the physics code to add a new traceline result for this entity
Returns the index of the last added result, -1 if it was ignored
===================
*/
int Physics_UpdateTraceResultStep(physics_world_t *w, entindex_t ent, entindex_t hit, vec3_t pos, vec3_t normal, vec_t fraction)
{
	if (w->server)
		return Game_SV_UpdateTraceResultStep(ent, hit, pos, normal, fraction);
	else
		return CL_PredUpdateTraceResultStep(ent, hit, pos, normal, fraction);
}

/*
===================
Physics_PostPhysics

Called by the physics code immediately after a frame is run
===================
*/
void Physics_PostPhysics(physics_world_t *w)
{
	if (w->server)
		Game_SV_PostPhysics();
	else
		CL_PredPostPhysics();
}

/*
===================
Physics_GetMoveType

Called by the physics code to know the movetype of an object
===================
*/
const int Physics_GetMoveType(physics_world_t *w, entindex_t ent)
{
	if (w->server)
		return Game_SV_GetMoveType(ent);
	else
		return CL_PredGetMoveType(ent);
}

/*
===================
Physics_GetAnglesFlags

Called by the physics code to know the anglesflags of an object
===================
*/
const unsigned int Physics_GetAnglesFlags(physics_world_t *w, entindex_t ent)
{
	if (w->server)
		return Game_SV_GetAnglesFlags(ent);
	else
		return CL_PredGetAnglesFlags(ent);
}

/*
===================
Physics_GetMoveCmd

Called by the physics code to know the move command of an object
===================
*/
void Physics_GetMoveCmd(physics_world_t *w, entindex_t ent, vec_t *dest)
{
	if (w->server)
		Game_SV_GetMoveCmd(ent, dest);
	else
		CL_PredGetMoveCmd(ent, dest);
}

/*
===================
Physics_GetAimCmd

Called by the physics code to know the aim command of an object
===================
*/
void Physics_GetAimCmd(physics_world_t *w, entindex_t ent, vec_t *dest)
{
	if (w->server)
		Game_SV_GetAimCmd(ent, dest);
	else
		CL_PredGetAimCmd(ent, dest);
}

/*
===================
Physics_GetMaxSpeed

Called by the physics code to know the max speed of an object in each axis
TODO: rotate this vector
===================
*/
void Physics_GetMaxSpeed(physics_world_t *w, entindex_t ent, vec_t *dest)
{
	if (w->server)
		Game_SV_GetMaxSpeed(ent, dest);
	else
		CL_PredGetMaxSpeed(ent, dest);
}

/*
===================
Physics_GetAcceleration

Called by the physics code to know the acceleration of an object in each axis
TODO: rotate this vector
===================
*/
void Physics_GetAcceleration(physics_world_t *w, entindex_t ent, vec_t *dest)
{
	if (w->server)
		Game_SV_GetAcceleration(ent, dest);
	else
		CL_PredGetAcceleration(ent, dest);
}

/*
===================
Physics_GetIgnoreGravity

Called by the physics code to know this entity wants to ignore gravity
===================
*/
int Physics_GetIgnoreGravity(physics_world_t *w, entindex_t ent)
{
	if (w->server)
		return Game_SV_GetIgnoreGravity(ent);
	else
		return CL_PredGetIgnoreGravity(ent);
}

/*
===================
Physics_CheckPhysicalCollisionResponse

Last check done by the physics code to see if two entities should generate
a collision response. (i.e. prevent inter-penetration)
===================
*/
int Physics_CheckPhysicalCollisionResponse(void *w_void, entindex_t e1, entindex_t e2)
{
	physics_world_t *w = (physics_world_t *)w_void;
	if (w->server)
		return Game_SV_CheckPhysicalCollisionResponse(e1, e2);
	else
		return CL_PredCheckPhysicalCollisionResponse(e1, e2);
}

/*
===================                                                                                                          *
Physics_Touchents
===================
*/
void Physics_TouchEnts(physics_world_t *w, entindex_t who, entindex_t by, vec3_t pos, vec3_t normal, vec_t distance, int reaction, vec_t impulse)
{
	if (w->server)
		Game_SV_TouchEnts(who, by, pos, normal, distance, reaction, impulse);
	else
		CL_PredTouchEnts(who, by, pos, normal, distance, reaction, impulse);
}

/*
===================
Physics_EntityGetData

Called to get lots of data about an entity faster. Any field can be null.
===================
*/
void Physics_EntityGetData(physics_world_t *w, const entindex_t ent, vec_t *origin, vec_t *velocity, vec_t *avelocity, vec_t *angles, vec_t *frames, precacheindex_t *modelindex, vec_t *lightintensity, int *anim_pitch)
{
	/* TODO FIXME: Sys Calling Game_SV and Game_CL? */
	if (w->server)
		Game_SV_EntityGetData(ent, origin, velocity, avelocity, angles, frames, modelindex, lightintensity, anim_pitch);
	else
		CL_PredEntityGetData(ent, origin, velocity, avelocity, angles, frames, modelindex, lightintensity, anim_pitch);
}

/*
===================
Physics_GetModelPhysicsTrimesh
===================
*/
void Physics_GetModelPhysicsTrimesh(physics_world_t *w, const precacheindex_t model, model_trimesh_t **trimesh)
{
	if (w->server)
		SV_GetModelPhysicsTrimesh(model, trimesh);
	else
		CL_GetModelPhysicsTrimesh(model, trimesh);
}

/*
===================
Physics_GetModelPhysicsBrushes
===================
*/
void Physics_GetModelPhysicsBrushes(physics_world_t *w, const precacheindex_t model, model_brushes_t **brushes)
{
	if (w->server)
		SV_GetModelPhysicsBrushes(model, brushes);
	else
		CL_GetModelPhysicsBrushes(model, brushes);
}

/*
===================
Physics_GetModelPhysicsHeightfield
===================
*/
void Physics_GetModelPhysicsHeightfield(physics_world_t *w, const precacheindex_t model, model_heightfield_t **heightfield)
{
	if (w->server)
		SV_GetModelPhysicsHeightfield(model, heightfield);
	else
		CL_GetModelPhysicsHeightfield(model, heightfield);
}

/*
===================
Sys_PhysicsGetEntityData

Any field can be NULL.
locked_angles will only be returned IF being used, otherwise no modification will be done.
TODO: only send other data (vehicle, etc) if being used? (and set the snapshot creator to use the old data when doing this? - just like the locked_angles)
===================
*/
int Sys_PhysicsGetEntityData(void *physworld, const int ent, vec_t *locked_angles, unsigned int *solid, int *type, vec_t *creation_vecs, int *creation_model, phys_edict_vehicle_info_t *creation_vehicle1, vec_t *mass, int *trace_onground)
{
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];

	if (!e->active)
		return false;

	/* only if being used */
	if (locked_angles && (Physics_GetAnglesFlags(w, ent) & (ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT | ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT | ANGLES_KINEMATICANGLES_LOCK_YAW_BIT)))
	{
		btTransform transform;
		vec3_t entforward, entright, entup;

		/* same notes as Sys_PhysicsUpdateGameStats* */
		e->body->getMotionState()->getWorldTransform(transform);
		const btMatrix3x3 &bodyM = transform.getBasis();

		/* convert to our coordinate system */
		btVector3 rightdir = bodyM.getColumn(0);
		btVector3 updir = bodyM.getColumn(1);
		btVector3 forwarddir = bodyM.getColumn(2);
		Math_Vector3Set(entright, rightdir[0], rightdir[1], rightdir[2]);
		Math_Vector3Set(entup, updir[0], updir[1], updir[2]);
		Math_Vector3Set(entforward, -forwarddir[0], -forwarddir[1], -forwarddir[2]); /* we look down into -Z */
		Math_Vector3Normalize(entright);
		Math_Vector3Normalize(entup);
		Math_Vector3Normalize(entforward);

		/* generate euler angles from direction vectors */
		Math_VecToAngles(entforward, entright, entup, locked_angles);
	}
	if (solid)
	{
		*solid = e->solid;
	}
	if (type)
	{
		*type = e->type;
	}
	if (creation_vecs)
	{
		Math_Vector3Copy(e->creation_vecs, creation_vecs);
	}
	if (creation_model)
	{
		*creation_model = e->creation_model;
	}
	if (creation_vehicle1)
	{
		if (e->type == PHYSICS_SHAPE_VEHICLE1)
		{
			phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)e->extra_data;
			*creation_vehicle1 = ed->construction_info;
		}
		else
		{
			memset(creation_vehicle1, 0, sizeof(phys_edict_vehicle_info_t));
		}
	}
	if (mass)
	{
		*mass = e->mass;
	}
	if (trace_onground)
	{
		*trace_onground = e->trace_onground;
	}

	return true;
}

/*
============================================================================

Game physics control code

Everything that should be changed for new movetypes is in this section

TODO: finish this
TODO: when falling into water, don't decellerate gravity because of density instantly, but depending on previous speed  (for a nice "the higher you fall, the deeper you get" effect)

============================================================================
*/

/*
===================
Sys_PhysicsClampSpeed

Since CCD is too expensive, clamp speeds
===================
*/
void Sys_PhysicsClampSpeed(void *physworld, const int ent)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];
	btScalar timeStep = (btScalar)w->phys_substep_frametime;

	if (!e->active)
		return; /* ignore silently entities who have no physical representation */

#ifdef USE_CONTINUOUS
	/* only allow controlled fly entities to trigger ccd to avoid excessive cpu usage */
	int movetype = Physics_GetMoveType(w, e->index);
	if (movetype == MOVETYPE_FLY)
		return;
#endif

	/* linear and angular speed cap */
	btVector3 velocity;
	btScalar speed;

	btScalar cap_lin;
	btScalar cap_ang;

	/* if any maxspeed is zero, use the ccd threshold to calculate maximum speed */

	if (phys_maxspeed_linear->doublevalue > 0)
		cap_lin = (btScalar)phys_maxspeed_linear->doublevalue;
	else if (e->radius > 0)
		cap_lin = e->radius / timeStep * 0.95f; /* 95% just to be sure we do not tunnel */
	else
		cap_lin = 0;

	if (phys_maxspeed_angular->doublevalue > 0)
		cap_ang = (btScalar)phys_maxspeed_angular->doublevalue;
	else if  (e->radius > 0)
		cap_ang = e->radius / timeStep * 0.95f; /* 95% just to be sure we do not tunnel TODO: better calculate this */
	else
		cap_ang = 0;

	if (cap_lin)
	{
		velocity = e->body->getLinearVelocity();
		speed = velocity.length();
		if (speed > cap_lin && speed > 0)
		{
			velocity *= cap_lin / speed;
			e->body->setLinearVelocity(velocity);
		}
	}

	if (cap_ang)
	{
		velocity = e->body->getAngularVelocity();
		speed = velocity.length();
		if (speed > cap_ang && speed > 0)
		{
			velocity *= cap_ang / speed;
			e->body->setAngularVelocity(velocity);
		}
	}
}

/*
===================
Sys_PhysicsApplyImpulse

Only works in dynamic objects (the ones which have mass)
Just applies an impulse to an entity
This should be the ONLY function to apply impulses.
"local_pos" is in relation to the body's origin (TODO: or is it the center of mass?)
===================
*/
void Sys_PhysicsApplyImpulse(void *physworld, const int ent, const vec3_t impulse, const vec3_t local_pos)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];

	if (!e->active)
		return; /* ignore silently entities who have no physical representation */

	e->body->activate(true);
	e->body->applyImpulse(btVector3(impulse[0], impulse[1], impulse[2]), btVector3(local_pos[0], local_pos[1], local_pos[2]));
	Sys_PhysicsClampSpeed(w, ent);
}

/*
===================
Sys_PhysicsApplyForces

Only works in dynamic objects (the ones which have mass)
Applies the necessary force to achieve final speed "max_speed" (in each axis, without surpassing it) in "dir", by applying a dosed "linear_force" in each axis
This should be the ONLY function to apply forces.
WARNING: if vertical_ok, we will probably cancel gravity! FIXME
TODO: probably will need lots of modifications or be removed
TODO: linear stuff to avoid diagonally moving entities to be sqrt(2) times faster
TODO: remove these deaccelerations! or have a custom friction argument
===================
*/
void Sys_PhysicsApplyForces(void *physworld, const int ent, vec3_t dir, const vec3_t linear_force, const vec3_t max_speed, const int vertical_ok, const int horizontal_ok, const int deaccel_vertical, const int deaccel_horizontal)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];
	vec3_t force;
	const btScalar *vel = e->body->getLinearVelocity();

	if (!e->active)
		return; /* ignore silently entities who have no physical representation */

	/* normalize it (no faster diagonal speed, sorry) */
	if (dir[0] || dir[1] || dir[2])
		Math_Vector3Normalize(dir);
	Math_ClearVector3(force);

	if (max_speed[0] == 0 || max_speed[1] == 0 || max_speed[2] == 0)
		Host_Error("Sys_PhysicsApplyForces: max_speed == 0 in an axis! (ent %d)\n", ent); /* avoid division by zero */

	/* compute forces */
	/* old version
	if (horizontal_ok)
	{
		if (dir[0])
			force[0] = (dir[0] - vel[0] / max_speed[0]) * linear_force[0];
		else if (deaccel_horizontal)
			force[0] = (-vel[0] / max_speed[0]) * linear_force[0];
		if (dir[2])
			force[2] = (dir[2] - vel[2] / max_speed[2]) * linear_force[2];
		else if (deaccel_horizontal)
			force[2] = (-vel[2] / max_speed[2]) * linear_force[2];
	}

	if (vertical_ok)
	{
		if (dir[1])
			force[1] = (dir[1] - vel[1] / max_speed[1]) * linear_force[1];
		else if (deaccel_vertical)
			force[1] = (-vel[1] / max_speed[1]) * linear_force[1];
	}
	*/
	if (horizontal_ok)
	{
		if (dir[0])
			force[0] = linear_force[0] * dir[0] - (vel[0] / max_speed[0]) * linear_force[0];
		else if (deaccel_horizontal)
			force[0] = -(vel[0] / max_speed[0]) * linear_force[0];
		if (dir[2])
			force[2] = linear_force[2] * dir[2] - (vel[2] / max_speed[2]) * linear_force[2];
		else if (deaccel_horizontal)
			force[2] = -(vel[2] / max_speed[2]) * linear_force[2];
	}

	if (vertical_ok)
	{
		if (dir[1])
			force[1] = linear_force[1] * dir[1] - (vel[1] / max_speed[1]) * linear_force[1];
		else if (deaccel_vertical)
			force[1] = -(vel[1] / max_speed[1]) * linear_force[1];
	}

	if (force[0] || force[1] || force[2])
	{
		/* TODO: do not use this ratio if calling from the game code - or only allow impulses from the game code? */
		vec_t ratio = HOST_MAX_FPS * (vec_t)w->phys_substep_frametime;
		e->body->activate(true);
		e->body->applyCentralForce(btVector3(force[0] * ratio, force[1] * ratio, force[2] * ratio));
	}

	Sys_PhysicsClampSpeed(w, ent);
}

/*
===================
Sys_PhysicsSetLinearVelocity
===================
*/
void Sys_PhysicsSetLinearVelocity(void *physworld, const int ent, vec3_t vel)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];

	if (!e->active)
		return; /* ignore silently entities who have no physical representation */

	e->body->activate(true);
	e->body->setLinearVelocity(btVector3(vel[0], vel[1], vel[2]));
	Sys_PhysicsClampSpeed(w, ent);
}

/*
===================
Sys_PhysicsSetAngularVelocity
===================
*/
void Sys_PhysicsSetAngularVelocity(void *physworld, const int ent, vec3_t avel)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];

	if (!e->active)
		return; /* ignore silently entities who have no physical representation */

	e->body->activate(true);
	e->body->setAngularVelocity(btVector3(avel[0], avel[1], avel[2]));
	Sys_PhysicsClampSpeed(w, ent);
}

/*
===================
Sys_PhysicsFreeControlFrame

Controls a free body.
===================
*/
void Sys_PhysicsFreeControlFrame(void *phyworld, entindex_t ent)
{
	/* TODO: doing nothing for now */
}

/* TODO: only calculate this when necessary, cache for a frame/subframe, use more rays and not only from origin, do a FAST check for voxels and ignore them in this check, etc */
class ClosestNotMe : public btCollisionWorld::ClosestRayResultCallback
{
	public:
		/* TODO: se from and to in the constructor? */
		ClosestNotMe(btRigidBody *me, int ignore_all_triggers) : btCollisionWorld::ClosestRayResultCallback(btVector3(0.0, 0.0, 0.0), btVector3(0.0, 0.0, 0.0))
		{
			m_me = me;
			m_ignore_all_triggers = ignore_all_triggers;
		}

		virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult &rayResult, bool normalInWorldSpace)
		{
			if (rayResult.m_collisionObject == m_me)
				return 1.0;

			/* also ignore "SOLID_ENTITY_WITHWORLDONLY" */
			if (m_ignore_all_triggers && (((phys_edict_t *)rayResult.m_collisionObject->getCollisionShape()->getUserPointer())->solid_group & COLLISION_CATEGORY_TRIGGER || ((phys_edict_t *)rayResult.m_collisionObject->getCollisionShape()->getUserPointer())->solid == SOLID_ENTITY_WITHWORLDONLY))
				return 1.0;

			return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
		}
	protected:
		btRigidBody *m_me;
		int m_ignore_all_triggers;
};

/*
===================
Sys_PhysicsQuickTrace

Quick raytracing function, returns the trace fraction only. From "src" to "dst", ignoring "ignore" and triggers (the later if "ignore_triggers" is true)
TOOD: still some false positives for staircase climbing
===================
*/
btScalar Sys_PhysicsQuickTrace(void *physworld, entindex_t ignore, btVector3 *src, btVector3 *dst, int ignore_all_triggers)
{
	physics_world_t *w = (physics_world_t *)physworld;
	btVector3 ray_source, ray_target;

	/* for alignment */
	ray_source = *src;
	ray_target = *dst;

	ClosestNotMe rayCallback(w->phys_entities[ignore].body, ignore_all_triggers);

	/* TODO: see effects of solid group and mask heritage */
	rayCallback.m_collisionFilterGroup |= w->phys_entities[ignore].solid_group;
	rayCallback.m_collisionFilterMask |= w->phys_entities[ignore].solid_mask;

	rayCallback.m_closestHitFraction = 1.0;
	w->dynamics_world->rayTest(ray_source, ray_target, rayCallback);

	if (rayCallback.hasHit())
	{
		return rayCallback.m_closestHitFraction;
	}
	else
	{
		return 1.0;
	}
}

/*
===================
Sys_PhysicsCharacterControlFrame

Controls a walking character.
TODO: pointcontents and waterlevel for liquid friction
TODO: time dependency? we use fixed steps now, but...
TODO: just finish this...
TODO: prevent sliding on ramps (do not add gravity if onground?)
TODO: see how to use rays/ghostobjects for steps in staircases, look into DynamicCharacterController.cpp from Bullet (maybe other ideas from there too?)
TODO: horizontal motion will make us climb steep hills easily
TODO: instant drop when going downstairs, to prevent floating over staircases
FIXME: sometimes a "super jump" happens? why?
===================
*/
void Sys_PhysicsCharacterControlFrame(void *physworld, entindex_t ent, const vec3_t movecmd, const int fly)
{
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];
	vec3_t max_speed;
	vec3_t acceleration;
	btMatrix3x3 bodyM;
	btTransform transform;
	vec3_t linear_force;
	vec3_t dir;
	vec3_t forward, right, up;

	Physics_GetMaxSpeed(w, ent, max_speed);
	Physics_GetAcceleration(w, ent, acceleration);
	Math_Vector3Scale(acceleration, linear_force, e->mass);

	/* go to where we are looking, get it from e->globalspace_rotation because some angles may be locked in the physical representation */
	transform.setIdentity();
	transform.setRotation(e->globalspace_rotation);

	/* direction of movement */
	bodyM = transform.getBasis();
	btVector3 rightdir = bodyM.getColumn(0);
	btVector3 updir = bodyM.getColumn(1);
	btVector3 forwarddir = bodyM.getColumn(2);
	Math_Vector3Set(right, rightdir[0], rightdir[1], rightdir[2]);
	Math_Vector3Set(up, updir[0], updir[1], updir[2]);
	Math_Vector3Set(forward, -forwarddir[0], -forwarddir[1], -forwarddir[2]); /* we look down into -Z */
	Math_Vector3Normalize(right);
	Math_Vector3Normalize(up);
	Math_Vector3Normalize(forward);
	Math_ClearVector3(dir);
	Math_Vector3ScaleAdd(forward, -movecmd[2], dir, dir); /* we look down into -Z */
	Math_Vector3ScaleAdd(right, movecmd[0], dir, dir);
	if (!fly)
	{
		dir[1] = 0; /* don't float. Will be normalized later */

		/* climb stairs */
		if (dir[0] || dir[2])
		{
			/* TODO FIXME: make this prettier and faster, is very ugly and doesn't work right */
	#define PHYSICS_STAIR_MARGIN_COMPENSATION (0.04f) /* TODO: why are capsules getting a very high collision margin of 0.25? */
	#define PHYSICS_STAIR_STEP_HEIGHT (0.25f)
	#define PHYSICS_STAIR_FORWARD_RADIUS (1)
	#define PHYSICS_STAIR_MAXSPEED_DIVIDER (2.5f)
			btVector3 AABB_min, AABB_max, ray_source, ray_target;
			w->phys_entities[ent].body->getAabb(AABB_min, AABB_max);

			/* TODO: use the right forward dist, see if these rays are touching triggers */
			btScalar fraction_footlevel;
			ray_source = w->phys_entities[ent].body->getWorldTransform().getOrigin();
			ray_source[1] = AABB_min.y() + PHYSICS_STAIR_MARGIN_COMPENSATION * 3; /* thrice the collision margin, to be sure we won't start in the floor */
			ray_target = ray_source + btVector3(dir[0], dir[1], dir[2]).normalized() * PHYSICS_STAIR_FORWARD_RADIUS;
			fraction_footlevel = Sys_PhysicsQuickTrace(physworld, ent, &ray_source, &ray_target, true);

			btScalar fraction_kneelevel;
			ray_source = w->phys_entities[ent].body->getWorldTransform().getOrigin();
			ray_source[1] = AABB_min.y() + PHYSICS_STAIR_MARGIN_COMPENSATION * 3 + PHYSICS_STAIR_STEP_HEIGHT; /* thrice the collision margin, to be sure we won't start in the floor */
			ray_target = ray_source + btVector3(dir[0], dir[1], dir[2]).normalized() * PHYSICS_STAIR_FORWARD_RADIUS;
			fraction_kneelevel = Sys_PhysicsQuickTrace(physworld, ent, &ray_source, &ray_target, true);

			/* TODO CONSOLEDEBUG Sys_Printf("%f %f\n", fraction_footlevel, fraction_kneelevel); */
			if (fraction_footlevel + 0.1f < fraction_kneelevel)
			{
				/* TODO CONSOLEDEBUG Sys_Printf("UP!\n"); */
				btVector3 jumpdir(0, 1, 0);
				vec3_t new_maxspeed;
				Math_Vector3Scale(max_speed, new_maxspeed, 1.0f / PHYSICS_STAIR_MAXSPEED_DIVIDER);
				Sys_PhysicsApplyForces(physworld, ent, jumpdir, linear_force, new_maxspeed, true, false, false, false);
			}
		}
	}
	else
	{
		Math_Vector3ScaleAdd(up, movecmd[1], dir, dir);
	}
	Sys_PhysicsApplyForces(physworld, ent, dir, linear_force, max_speed, fly, true, fly, true);

	/* jump */
	if (movecmd[1] > 0 && !fly && !e->jumped_this_frame) /* TODO: this makes is not go straight up when underwater */
	{
		btVector3 jumpveladd(0, 1, 0);
		Sys_PhysicsSetLinearVelocity(physworld, ent, e->body->getLinearVelocity() + jumpveladd);
		e->jumped_this_frame = true;
		/* TODO: reaction? (if we are midair on top of another entity and jump) */
		/* TODO: take actual vertical speed in consideration (going upstairs, for example) to prevent super-jumps (a common problem, because we may be moving up but NOT by our muscles) */
	}
	/* TODO: SWIM */
}

/*
===================
Sys_PhysicsPreFrame

Called by physics code to set any possible body/shape properties prior to the start of a frame.
Please note that setting ent->aimcmd may not change angles automatically, there may be a frame delay
between the time you set and the time you will be able to use the new angles.
===================
*/
void Sys_PhysicsPreFrame(void *physworld, entindex_t i)
{
	physics_world_t *w = (physics_world_t *)physworld;

	if (w->phys_entities[i].active)
	{
		/* we only deal with dynamic entities here */
		if (w->phys_entities[i].mass <= 0)
			return;

		Sys_PhysicsClampSpeed(w, i);

		const int globalspace_rotationflags = Physics_GetAnglesFlags(w, i);
		/* set this before moving TODO: by setting rotation directly, motionstates MAY get cancelled for rotation! this may be good for some game types (like first-person shooters and accurate control). Let the game decide when to use motionstates */
		if (globalspace_rotationflags & ANGLES_KINEMATICANGLES_BIT)
		{
			vec3_t aimcmd;
			Physics_GetAimCmd(w, i, aimcmd);

			btRigidBody *body = w->phys_entities[i].body;
			body->setAngularFactor(0); /* prevent angle modification if we want to set our own angles based on aimcmd every frame */

			if (aimcmd[0] || aimcmd[1] || aimcmd[2])
			{
				btTransform transform = body->getWorldTransform();

				/* set physical body rotation */
				/* TODO: store full rotation and create the full quaternion every time - this way we can limit angles (it's difficult to do that when creating quaternions with the delta angles and multiplying by the old quaternion) */
				/* TODO: do this somewhere else, maybe it doesn't work very well. Use real kinematic player!!!!! */
				btQuaternion qY(btVector3(0.0f, 1.0f, 0.0f), globalspace_rotationflags & ANGLES_KINEMATICANGLES_LOCK_YAW_BIT ? 0 : btScalar(w->phys_substep_frametime * aimcmd[ANGLES_YAW] * phys_axis_yaw_rate->doublevalue));
				btQuaternion qP(btVector3(1.0f, 0.0f, 0.0f), globalspace_rotationflags & ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT ? 0 : btScalar(w->phys_substep_frametime * aimcmd[ANGLES_PITCH] * phys_axis_pitch_rate->doublevalue));
				btQuaternion qR(btVector3(0.0f, 0.0f, 1.0f), globalspace_rotationflags & ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT ? 0 : btScalar(w->phys_substep_frametime * aimcmd[ANGLES_ROLL] * phys_axis_roll_rate->doublevalue));
				btQuaternion quat = transform.getRotation().normalized();
				/* btQuaternion q = quat * qR * qP * qY; /* airplane */
				btQuaternion q = qY * quat * qP; /* we are not an airplane! TODO: figure out why quaternions work in this multiplication order! */
				q = q * qR; /* we are not an airplane! TODO: multiplying it this way, pitch and roll may get into a pseudo gimbal lock */
				q.normalize();
				/* TODO: maybe we drift a little over time due to numerical innacurracies? */
				transform.setRotation(q);

				/* set game-visible rotation */
				btQuaternion gqY(btVector3(0.0f, 1.0f, 0.0f), btScalar(w->phys_substep_frametime * aimcmd[ANGLES_YAW] * phys_axis_yaw_rate->doublevalue));
				btQuaternion gqP(btVector3(1.0f, 0.0f, 0.0f), btScalar(w->phys_substep_frametime * aimcmd[ANGLES_PITCH] * phys_axis_pitch_rate->doublevalue));
				btQuaternion gqR(btVector3(0.0f, 0.0f, 1.0f), btScalar(w->phys_substep_frametime * aimcmd[ANGLES_ROLL] * phys_axis_roll_rate->doublevalue));
				/* btQuaternion gq = quat * gqR * gqP * gqY; /* airplane */
				btQuaternion gq = gqY * w->phys_entities[i].globalspace_rotation.normalized() * gqP; /* we are not an airplane! TODO: figure out why quaternions work in this multiplication order! */
				gq = gq * gqR; /* we are not an airplane! TODO: multiplying it this way, pitch and roll may get into a pseudo gimbal lock */
				w->phys_entities[i].globalspace_rotation = gq.normalized();
				/* TODO: maybe we drift a little over time due to numerical innacurracies? */

				body->setWorldTransform(transform);
				/* TODO: set motionstate transform here, like we do in Sys_PhysicsSetTransform? */

				/* TODO: see other places we are relinking to see if we should behave differently with kinematic/static bodies */
				/* TODO: these should be called when changing from static to kinematic and vice-versa and when moving a kinematic for the first time, not here */
				/* TODO: is this useful when teleporting dynamic entities? */
				body->setInterpolationAngularVelocity(btVector3(0,0,0));
			}
		}
		else
		{
			w->phys_entities[i].body->setAngularFactor(1);
		}

		/* TODO FIXME: this will fail if the body is waiting to be relinked? will all these stuff below fail??? */
		/* TODO FIXME: why turning while climbing will make the player fall a little? */
		if (Physics_GetIgnoreGravity(w, i))
			w->phys_entities[i].body->setGravity(btVector3(0, 0, 0));
		else
			w->phys_entities[i].body->setGravity(btVector3(0, (vec_t)phys_gravity_y->doublevalue, 0));

		/* update vehicle stuff */
		if (w->phys_entities[i].type == PHYSICS_SHAPE_VEHICLE1)
		{
			phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)w->phys_entities[i].extra_data;

			if (Physics_GetMoveType(w, i) != MOVETYPE_FREE)
				Host_Error("Sys_PhysicsPreFrame: vehicles must be MOVETYPE_FREE (ent %d)\n", i);

			vec3_t movecmd;
			Physics_GetMoveCmd(w, i, movecmd);

			/* TODO: this is keybord control, make continuous joystick control by mapping directly instead of incrementing */
			/* steering */
			if (movecmd[0])
			{
				ed->construction_info.gVehicleSteering += ed->construction_info.steeringIncrement * -movecmd[0];
				if (ed->construction_info.gVehicleSteering > ed->construction_info.steeringClamp)
					ed->construction_info.gVehicleSteering = ed->construction_info.steeringClamp;
				if (ed->construction_info.gVehicleSteering < -ed->construction_info.steeringClamp)
					ed->construction_info.gVehicleSteering = -ed->construction_info.steeringClamp;
			}
			else /* going back to center */
			{
				if (ed->construction_info.gVehicleSteering > 0)
				{
					ed->construction_info.gVehicleSteering -= ed->construction_info.steeringIncrement;
					if (ed->construction_info.gVehicleSteering < 0)
						ed->construction_info.gVehicleSteering = 0;
				}
				else if (ed->construction_info.gVehicleSteering < 0)
				{
					ed->construction_info.gVehicleSteering += ed->construction_info.steeringIncrement;
					if (ed->construction_info.gVehicleSteering > 0)
						ed->construction_info.gVehicleSteering = 0;
				}
			}
			/* accelerator */
			if (movecmd[2])
			{
				ed->construction_info.gEngineForce = ed->construction_info.maxEngineForce * movecmd[2];
				ed->construction_info.gBreakingForce = 0.f;
			}
			else
			{
				ed->construction_info.gEngineForce = 0.f;
				ed->construction_info.gBreakingForce = ed->construction_info.defaultBreakingForce;
			}

			int handbrake = false;
			/* handbrake */
			if (movecmd[1] > 0)
			{
				ed->construction_info.gEngineForce = 0.f;
				ed->construction_info.gBreakingForce = ed->construction_info.handBrakeBreakingForce * movecmd[1];
				handbrake = true;
			}

			/* this is here to not let all wheel drive have double the forces applied */
			btScalar force_multiplier;
			if (ed->construction_info.wheel_drive == VEHICLE1_ALL_WHEEL_DRIVE)
				force_multiplier = 0.5f;
			else
				force_multiplier = 1;


			int wheelIndex;

			if (ed->construction_info.wheel_drive == VEHICLE1_ALL_WHEEL_DRIVE || ed->construction_info.wheel_drive == VEHICLE1_REAR_WHEEL_DRIVE)
			{
				wheelIndex = 2;
				ed->m_vehicle->applyEngineForce(ed->construction_info.gEngineForce * force_multiplier, wheelIndex);
				ed->m_vehicle->setBrake(ed->construction_info.gBreakingForce * force_multiplier, wheelIndex);

				wheelIndex = 3;
				ed->m_vehicle->applyEngineForce(ed->construction_info.gEngineForce * force_multiplier, wheelIndex);
				ed->m_vehicle->setBrake(ed->construction_info.gBreakingForce * force_multiplier, wheelIndex);
			}


			wheelIndex = 0;
			ed->m_vehicle->setSteeringValue(ed->construction_info.gVehicleSteering, wheelIndex);
			if (ed->construction_info.wheel_drive == VEHICLE1_ALL_WHEEL_DRIVE || ed->construction_info.wheel_drive == VEHICLE1_FRONT_WHEEL_DRIVE)
			{
				ed->m_vehicle->applyEngineForce(ed->construction_info.gEngineForce * force_multiplier, wheelIndex);
				ed->m_vehicle->setBrake(handbrake ? 0 : ed->construction_info.gBreakingForce * force_multiplier, wheelIndex);
			}
			wheelIndex = 1;
			ed->m_vehicle->setSteeringValue(ed->construction_info.gVehicleSteering, wheelIndex);
			if (ed->construction_info.wheel_drive == VEHICLE1_ALL_WHEEL_DRIVE || ed->construction_info.wheel_drive == VEHICLE1_FRONT_WHEEL_DRIVE)
			{
				ed->m_vehicle->applyEngineForce(ed->construction_info.gEngineForce * force_multiplier, wheelIndex);
				ed->m_vehicle->setBrake(handbrake ? 0 : ed->construction_info.gBreakingForce * force_multiplier, wheelIndex);
			}

			w->phys_entities[i].body->activate();
		}

		switch (Physics_GetMoveType(w, i))
		{
			case MOVETYPE_FREE:
				Sys_PhysicsFreeControlFrame(physworld, i);
				break;
			case MOVETYPE_WALK:
				{
					vec3_t movecmd;
					Physics_GetMoveCmd(w, i, movecmd);
					Sys_PhysicsCharacterControlFrame(physworld, i, movecmd, false);
				}
				break;
			case MOVETYPE_FLY:
				{
					vec3_t movecmd;
					Physics_GetMoveCmd(w, i, movecmd);
					Sys_PhysicsCharacterControlFrame(physworld, i, movecmd, true);
				}
				break;
			case MOVETYPE_FOLLOW:
			case MOVETYPE_FOLLOWANGLES:
				break; /* done by game code */
			default:
				Host_Error("Sys_PhysicsPreFrame: bad movetype %d\n", Physics_GetMoveType(w, i));
				break;
		}

		/* TODO: this is being run BEFORE setting the onground flag (lots of places where it gets set! we may be one frame late in some cases. For trace_onground, it's probably ok! */
		if (i == saved_entity_to_simulate && w->phys_entities[i].onground) /* only for async entities */
		{
			phys_edict_t *ge = &w->phys_entities[w->phys_entities[i].ground_entity];
			if (ge->body)
			{
				/* TODO: this doesn't give inertia (for example, jumping won't work correctly) - setting velocity here to ent->velocity + groundent->velocity and removing int postframe wasn't working (because of friction?), but will work correctly for a kinematic character */
				/* TODO: angular too? */
				/* when doing async entities, everything else will have zero velocity. Solution: use cached values */
				w->phys_entities[i].body->translate(ge->cached_linearvelocity * w->phys_substep_frametime);
			}
		}
	}
}

/*
===================
Sys_PhysicsPostFrame

Called by physics code to set any possible body/shape properties after the end of a frame
===================
*/
void Sys_PhysicsPostFrame(void *physworld, entindex_t i)
{
	physics_world_t *w = (physics_world_t *)physworld;

	/* we only deal with dynamic entities here */
	if (w->phys_entities[i].mass <= 0)
		return;

	if (w->phys_entities[i].active)
	{
		Sys_PhysicsClampSpeed(w, i);
#ifdef FORCED_SLEEP
		/* for manual sleeping TODO FIXME: shouldn't be needed, things are NOT sleeping and I don't know why! also box shapes are sleeping in odd positions */
		if (w->phys_entities[i].type != PHYSICS_SHAPE_VEHICLE1)
		{
			/* TODO: unstable when starting VERY CLOSE, can be solved by making the shape smaller than the real size, but will cause interpenetration when finally resting */
			/* this workaround fixes two problems: bodies not sleeping and instability with small objects and big (but for me okay) subtimesteps. Maybe the later is caused by the former. */
			phys_edict_t *e = &w->phys_entities[i];

			btVector3 linear, angular;
			linear = e->body->getLinearVelocity();
			angular = e->body->getAngularVelocity();

			if (linear[0] < 0.04 && linear[1] < 0.04 && linear[2] < 0.04 &&
				linear[0] > -0.04 && linear[1] > -0.04 && linear[2] > -0.04 &&
				angular[0] < 0.04 && angular[1] < 0.04 && angular[2] < 0.04 &&
				angular[0] > -0.04 && angular[1] > -0.04 && angular[2] > -0.04 &&
				e->onground)
			{
				if (e->sleep_counter > 0)
					e->sleep_counter -= w->phys_substep_frametime;
				if (e->sleep_state != WANTS_DEACTIVATION && e->sleep_state != ISLAND_SLEEPING)
				{
					e->sleep_counter = 0.01;
					e->body->setActivationState(WANTS_DEACTIVATION);
					e->sleep_state = WANTS_DEACTIVATION;
				}
				if ((e->sleep_counter <= 0 && e->sleep_state == WANTS_DEACTIVATION) || e->sleep_state == ISLAND_SLEEPING)
				{
					e->body->setLinearVelocity(btVector3(0, 0, 0));
					e->body->setAngularVelocity(btVector3(0, 0, 0));
					e->body->setActivationState(ISLAND_SLEEPING);
					e->sleep_state = ISLAND_SLEEPING;
				}
			}
			else
			{
				e->sleep_counter = 0.01;
				e->sleep_state = ACTIVE_TAG;
			}
		}
#endif /* FORCED_SLEEP */

		switch (Physics_GetMoveType(w, i))
		{
			case MOVETYPE_FREE:
				break;
			case MOVETYPE_WALK:
				break;
			case MOVETYPE_FLY:
				break;
			case MOVETYPE_FOLLOW:
			case MOVETYPE_FOLLOWANGLES:
				break; /* done by game code */
			default:
				Host_Error("Sys_PhysicsPostFrame: bad movetype %d\n", Physics_GetMoveType(w, i));
				break;
		}

		/* if we aren't controlling rotation, get it from the physical body */
		if (!(Physics_GetAnglesFlags(w, i) & ANGLES_KINEMATICANGLES_BIT))
		{
			/* TODO: bypassing motionstates for angles here too because I was getting weird results with projectiles by using then! PROBABLY because the transform was initialized at identity and then changed for the initial transform AFTER the body was spawned. How can I cancel the interpolation in this case? wasn't removing and reinserting the body enough? was this not the cause? */
			/* btTransform transform = phys_entities[i].body->getWorldTransform(); */
			/* TODO now using motion states - wtf decide yourself (see Sys_PhysicsUpdateGameStats for when to use and when not to use motionstates) */
			btTransform transform;
			w->phys_entities[i].body->getMotionState()->getWorldTransform(transform);
			w->phys_entities[i].globalspace_rotation = transform.getRotation();
		}
	}
}

/*
============================================================================

Server-side physics simulation

TODO: create more cvars to configure various parameters

============================================================================
*/

/*
===================
Sys_PhysicsInternalTickPreCallback

Called before every FIXED subtick in Bullet's world
Great place to eliminate time-dependency

TODO: move here stuff that should execute between each fixed tick!
===================
*/
void Sys_PhysicsInternalTickPreCallback(btDynamicsWorld *world, btScalar timeStep)
{
	physics_world_t *w = (physics_world_t *)world->getWorldUserInfo();
	int i;
	if (saved_entity_to_simulate == -1) /* if only one entity is being simulated, whatever called the simulation must handle its time */
		w->phys_time += (mstime_t)(timeStep * 1000); /* FIXME: it's a double not mstime_t, floating point is bad for time :( */
	w->phys_substep_frametime = timeStep;

	/* TODO: set e->groundentity too (or a chain of them?), add velocity of groundentity (for platforms) (or do it while moving? maybe friction is enough to create this?) */
	/* set the onground flag where the contact method wasn't sufficient */
	for (i = 0; i < MAX_EDICTS; i++)
	{
		if (w->phys_entities[i].active && w->phys_entities[i].type == PHYSICS_SHAPE_VEHICLE1 && !w->phys_entities[i].onground)
		{
			/* see if any of the wheels is onground */
			phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)w->phys_entities[i].extra_data;
			for (int wheel = 0; wheel < ed->m_vehicle->getNumWheels(); wheel++)
			{
				const btWheelInfo::RaycastInfo ray = ed->m_vehicle->getWheelInfo(wheel).m_raycastInfo;

				if (ray.m_groundObject && ray.m_contactNormalWS.y() >= PHYS_SLOPE_COEFFICIENT)
				{
					w->phys_entities[i].onground = true;
					w->phys_entities[i].onground_normal = ray.m_contactNormalWS;
					w->phys_entities[i].ground_entity = ((btCollisionObject *)(ray.m_groundObject))->getUserIndex();
					w->phys_entities[i].lastonground = w->phys_time;
					break;
				}
			}
		}
		if (w->phys_entities[i].active && w->phys_entities[i].trace_onground && !w->phys_entities[i].onground) /* no point in retesting if we are already onground FIXME: this will cause different behavior from testing every frame if you leave ground just before the timeout for clearing the flag */
		{
			/* TODO: only calculate this when necessary, cache for a frame/subframe, use more rays and not only from origin, do a FAST check for voxels and ignore them in this check, etc */
			btVector3 ray_source, ray_target, ray_normal;
			btScalar ray_lambda;

			btVector3 AABB_min, AABB_max;
			w->phys_entities[i].body->getAabb(AABB_min, AABB_max);

			/* can't do from AABB_min to AABB_min - PHYS_ONGROUND_DIST because of collision margin (we may be already inside another body and there will be no hits) */
			ray_source = w->phys_entities[i].body->getWorldTransform().getOrigin();
			ray_target = ray_source;
			ray_target[1] = AABB_min.y() + btScalar(-PHYS_ONGROUND_DIST); /* AABB gets returned in world coordinates */

			ClosestNotMe rayCallback(w->phys_entities[i].body, true);

			rayCallback.m_closestHitFraction = 1.0;
			w->dynamics_world->rayTest(ray_source, ray_target, rayCallback);

			if (rayCallback.hasHit())
			{
				ray_lambda = rayCallback.m_closestHitFraction; /* TODO: do something with this fraction? */

				if (ray_lambda < 1.0)
				{
					/* TODO: make sure it returns the normal of the closest hit */
					if (rayCallback.m_hitNormalWorld.y() >= PHYS_SLOPE_COEFFICIENT)
					{
						w->phys_entities[i].onground = true;
						w->phys_entities[i].onground_normal = rayCallback.m_hitNormalWorld;
						w->phys_entities[i].ground_entity = rayCallback.m_collisionObject->getUserIndex();
						w->phys_entities[i].lastonground = w->phys_time;
					}
				}
			}
			else
			{
				ray_lambda = 1.0;
			}
		}
	}

	/* update entities' forced physics stuff each step! */
	if (saved_entity_to_simulate == -1)
	{
		for (i = 0; i < MAX_EDICTS; i++)
			Sys_PhysicsPreFrame(w, i);
	}
	else
		Sys_PhysicsPreFrame(w, saved_entity_to_simulate);


	/* relink what's needed TODO: bodies not relinked when physics pre-frame happens will have problems??? */
	for (i = 0; i < MAX_EDICTS; i++)
	{
		if (w->phys_entities[i].active && w->phys_entities[i].relink)
		{
			if (!w->phys_entities[i].body->isInWorld())
			{
				w->dynamics_world->addRigidBody(w->phys_entities[i].body, w->phys_entities[i].solid_group, w->phys_entities[i].solid_mask);
				if (w->phys_entities[i].mass == -1) /* TODO: not needed for kinematic bodies? */
					w->dynamics_world->updateSingleAabb(w->phys_entities[i].body); /* for running dynamics world with setForceUpdateAllAabbs(false) */
				if (w->phys_entities[i].type == PHYSICS_SHAPE_VEHICLE1)
				{
					phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)w->phys_entities[i].extra_data;

					w->dynamics_world->addVehicle(ed->m_vehicle);
				}
				w->phys_entities[i].body->activate(true); /* TODO: see if this is enough to reactivate the body when it's at rest and gets transported to a high location and must fall */
			}

			w->phys_entities[i].relink = false;
		}
	}
}

/*
===================
Sys_PhysicsInternalTickPostCallback

Called after every FIXED subtick in Bullet's world
Great place to eliminate time-dependency

TODO: move here stuff that should execute between each fixed tick!
TODO: ignore touchs by owner
===================
*/
void Sys_PhysicsInternalTickPostCallback(btDynamicsWorld *world, btScalar timeStep)
{
	physics_world_t *w = (physics_world_t *)world->getWorldUserInfo();
	int numManifolds = world->getDispatcher()->getNumManifolds();
	for (int i = 0; i < numManifolds; i++)
	{
		const btPersistentManifold *contactManifold = world->getDispatcher()->getManifoldByIndexInternal(i);
		const btCollisionObject *obA = (btCollisionObject *)contactManifold->getBody0();
		const btCollisionObject *obB = (btCollisionObject *)contactManifold->getBody1();

		int numContacts = contactManifold->getNumContacts();
		for (int j = 0; j < numContacts; j++)
		{
			const btManifoldPoint &pt = contactManifold->getContactPoint(j);

			/* FIXME: are these two redundant? */
			const btVector3 &ptA = pt.getPositionWorldOnA();
			const btVector3 &ptB = pt.getPositionWorldOnB();
			const btVector3 &normalOnB = pt.m_normalWorldOnB;
			btVector3 normalOnA(-normalOnB.getX(), -normalOnB.getY(), -normalOnB.getZ());
			entindex_t e1 = ((phys_edict_t *)obA->getCollisionShape()->getUserPointer())->index;
			entindex_t e2 = ((phys_edict_t *)obB->getCollisionShape()->getUserPointer())->index;

			/* if we are this far, collision event ocurred, but has a physical reaction been created? */
			int reaction_ocurred = false;
			if (w->dispatcher->needsResponse(obA, obB))
				reaction_ocurred = true;

			/* TODO: is it worth adding collisions where self == world to the list? */
			/* TODO: we are only storing the first contact between two bodies for now, but still iterating through them all. Stop iterating or implement manifolds in the game code */
			/* TODO: stuff like reaction and impulse may change in substeps? We may miss them by ignoring further contacts in a group of substeps */
			/* store the collision both ways */
			if (w->phys_collision_matrix[e1][e2] != host_framecount_notzero)
			{
				w->phys_collision_matrix[e1][e2] = host_framecount_notzero;
				w->phys_collision_list[w->phys_num_collisions].self = e1;
				w->phys_collision_list[w->phys_num_collisions].other = e2;
				Math_Vector3Copy(ptB, w->phys_collision_list[w->phys_num_collisions].pos);
				Math_Vector3Copy(normalOnB, w->phys_collision_list[w->phys_num_collisions].normal);
				w->phys_collision_list[w->phys_num_collisions].distance = pt.getDistance();
				w->phys_collision_list[w->phys_num_collisions].reaction = reaction_ocurred;
				w->phys_collision_list[w->phys_num_collisions].impulse = pt.getAppliedImpulse();
				w->phys_num_collisions++;
			}
			if (w->phys_collision_matrix[e2][e1] != host_framecount_notzero)
			{
				w->phys_collision_matrix[e2][e1] = host_framecount_notzero;
				w->phys_collision_list[w->phys_num_collisions].self = e2;
				w->phys_collision_list[w->phys_num_collisions].other = e1;
				Math_Vector3Copy(ptA, w->phys_collision_list[w->phys_num_collisions].pos);
				Math_Vector3Copy(normalOnA, w->phys_collision_list[w->phys_num_collisions].normal);
				w->phys_collision_list[w->phys_num_collisions].distance = pt.getDistance();
				w->phys_collision_list[w->phys_num_collisions].reaction = reaction_ocurred;
				w->phys_collision_list[w->phys_num_collisions].impulse = pt.getAppliedImpulse();
				w->phys_num_collisions++;
			}
		}
	}

	/* update entities' forced physics stuff each step! */
	if (saved_entity_to_simulate == -1)
	{
		entindex_t i;
		for (i = 0; i < MAX_EDICTS; i++)
			Sys_PhysicsPostFrame(w, i);
	}
	else
		Sys_PhysicsPostFrame(w, saved_entity_to_simulate);
}

/*
===================
Sys_PhysicsContactAddedCallback

Called when creating a new contact between two objects

TODO: custom materials, surfaceflags, etc... for friction, rolling friction, etc...
TODO: moving platforms here?
TODO: partId* and index* identify triangles, etc...
===================
*/
inline btScalar Sys_PhysicsCalculateCombinedRollingFriction(const btScalar rolling_friction0, const btScalar rolling_friction1)
{
	btScalar rolling_friction = rolling_friction0 * rolling_friction1;

	const btScalar MAX_FRICTION = btScalar(10.);
	if (rolling_friction < -MAX_FRICTION)
		rolling_friction = -MAX_FRICTION;
	if (rolling_friction > MAX_FRICTION)
		rolling_friction = MAX_FRICTION;
	return rolling_friction;
}
inline btScalar Sys_PhysicsCalculateCombinedFriction(const btScalar friction0, const btScalar friction1)
{
	btScalar friction = friction0 * friction1;

	const btScalar MAX_FRICTION = btScalar(10.);
	if (friction < -MAX_FRICTION)
		friction = -MAX_FRICTION;
	if (friction > MAX_FRICTION)
		friction = MAX_FRICTION;
	return friction;
}
inline btScalar Sys_PhysicsCalculateCombinedRestitution(const btScalar restitution0, const btScalar restitution1)
{
	return restitution0 * restitution1;
}
static bool Sys_PhysicsContactAddedCallback(btManifoldPoint &cp, const btCollisionObjectWrapper *colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper *colObj1Wrap, int partId1, int index1)
{
	btScalar rolling_friction0 = colObj0Wrap->getCollisionObject()->getRollingFriction();
	btScalar rolling_friction1 = colObj1Wrap->getCollisionObject()->getRollingFriction();
	btScalar friction0 = colObj0Wrap->getCollisionObject()->getFriction();
	btScalar friction1 = colObj1Wrap->getCollisionObject()->getFriction();
	btScalar restitution0 = colObj0Wrap->getCollisionObject()->getRestitution();
	btScalar restitution1 = colObj1Wrap->getCollisionObject()->getRestitution();
	phys_edict_t *e1 = (phys_edict_t *)colObj0Wrap->getCollisionObject()->getCollisionShape()->getUserPointer();
	phys_edict_t *e2 = (phys_edict_t *)colObj1Wrap->getCollisionObject()->getCollisionShape()->getUserPointer();

	/* fix to avoid colliding with backfaces of heightfields, etc! winding order MATTERS! */
	/* TODO: see if something breaks because of winding order */
	/* TODO: see interoperability with btAdjustInternalEdgeContacts (which is yet to be implemented as of the time of writing this comment */
	/* TODO: see if I can make this faster (cache normals instead of recalculating, etc */
	/* TODO: see if it's needed to mess with colObj0Wrap too - I don't know what's happening here */
	/* TODO: see if this messes with normal calculation */
	if (colObj1Wrap->getCollisionShape()->getShapeType() == TRIANGLE_SHAPE_PROXYTYPE)
    {
        const btTriangleShape* triShape = static_cast<const btTriangleShape*>( colObj1Wrap->getCollisionShape() );
        const btVector3* v = triShape->m_vertices1;
        btVector3 faceNormalLs = btCross(v[1] - v[0], v[2] - v[0]);
        faceNormalLs.normalize();
        btVector3 faceNormalWs = colObj1Wrap->getWorldTransform().getBasis() * faceNormalLs;
        float nDotF = btDot( faceNormalWs, cp.m_normalWorldOnB );
        if ( nDotF <= 0.0f )
        {
            /* flip the contact normal to be aligned with the face normal */
            cp.m_normalWorldOnB += -2.0f * nDotF * faceNormalWs;
        }
    }

	/* TODO CONSOLEDEBUG
	Sys_Printf("e1 == %d fric %.2f rfric %.2f res %.2f | e2 == %d fric %.2f rfric %.2f res %.2f\n", e1->index, friction0, rolling_friction0, restitution0, e2->index, friction1, rolling_friction1, restitution1);
	Sys_Printf("e1 == %d | e2 == %d | normalOnE2 %4.2f %4.2f %4.2f\n", e1->index, e2->index, cp.m_normalWorldOnB.getX(), cp.m_normalWorldOnB.getY(), cp.m_normalWorldOnB.getZ());
	*/

	bool isGround = false;
	/* set the onground flag here to avoid wasting cpu time with tracing TODO: check this, I wanted to set onground for a WORLD not activated (was voxels) here when everyone was on top of it!! (that's why I'm checking ->active below two times) */
	if (cp.m_normalWorldOnB.getY() >= PHYS_SLOPE_COEFFICIENT && e1->active)
	{
		e1->onground = true;
		e1->onground_normal = cp.m_normalWorldOnB;
		e1->ground_entity = e2->index;
		e1->lastonground = ((physics_world_t *)(e1->myworld))->phys_time;
		isGround = true;
	}
	if (cp.m_normalWorldOnB.getY() <= -PHYS_SLOPE_COEFFICIENT && e2->active) /* the normal is moving towards e1, invert it here */
	{
		e2->onground = true;
		e2->onground_normal = cp.m_normalWorldOnB * -1;
		e2->ground_entity = e1->index;
		e2->lastonground = ((physics_world_t *)(e2->myworld))->phys_time;
		isGround = true;
	}

	if ((Physics_GetMoveType(((physics_world_t *)(e1->myworld)), e1->index) != MOVETYPE_FREE || Physics_GetMoveType(((physics_world_t *)(e2->myworld)), e2->index) != MOVETYPE_FREE) && !isGround) /* currently apply this hack only to prevent sticking to walls, this will make airwalks FASTER than groundwalks TODO FIXME */
	{
		/* TODO: Entities not MOVETYPE_FREE should create their own friction and restitution, is this the right way? better to set friction based on contact normals to prevent "sticking" to walls? */
		/* TODO: this way we will also have to set different decceleration values for walking on ice, etc, based on the material friction used below. Also friction makes the camera look bad and shaky, don't know if because of the character controller functions (doing weird things) or because of the lack of motionstate correct usage */
		/* TODO: remove this when implementing kinematic controllers for everything not MOVETYPE_FREE */
		/* TODO: see cp.m_lateralFriction* stuff */
		friction0 = 0.f;
		friction1 = 0.f;
		rolling_friction0 = 0.f;
		rolling_friction1 = 0.f;
		restitution0 = 0;
		restitution1 = 0;
	}

	/* TODO: see if this won't make everything ALWAYS active */
	if (isAsync(e1->index) || isAsync(e2->index)) /* only for async entities */
	{
		if (e1->body)
			cached_should_activate[e1->index] = true;
		if (e2->body)
			cached_should_activate[e2->index] = true;
	}

	cp.m_combinedFriction = Sys_PhysicsCalculateCombinedFriction(friction0, friction1);
	cp.m_combinedRestitution = Sys_PhysicsCalculateCombinedRestitution(restitution0, restitution1);
	cp.m_combinedRollingFriction = Sys_PhysicsCalculateCombinedRollingFriction(rolling_friction0, rolling_friction1);

	/* this return value is currently ignored, but to be on the safe side: return false if you don't calculate friction */
	return true;
}

/*
===================
Sys_PhysicsUpdateGameStatsStep

To prevent code duplicates in Sys_PhysicsUpdateGameStats

Velocity, angular velocity and onground will be from base_ent instead of from
ent (for vehicle wheels, etc which do not have bodies but only a entity list
representing the wheels)

FIXME: slow?
TODO: velocity and avelocity should be per body
TODO: only set what the game code wants, when it wants...
TODO: set onground for each wheel, the info is there (see the chassis onground, which uses the wheels)
===================
*/
void Sys_PhysicsUpdateGameStatsStep(void *physworld, int ent, int base_ent, int only_direction_vectors, btTransform &transform)
{
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];
	phys_edict_t *base_e = &w->phys_entities[base_ent];
	vec3_t entorigin, entangles, entvel, entavel, entforward, entright, entup;
	const btScalar *bodypos, *bodyvel, *bodyavel;

	const btMatrix3x3 &bodyM = transform.getBasis();
	if (!only_direction_vectors)
	{
		bodypos = transform.getOrigin();
		bodyvel = base_e->body->getLinearVelocity();
		bodyavel = base_e->body->getAngularVelocity();
	}
	/* convert to our coordinate system */
	btVector3 rightdir = bodyM.getColumn(0);
	btVector3 updir = bodyM.getColumn(1);
	btVector3 forwarddir = bodyM.getColumn(2);
	Math_Vector3Set(entright, rightdir[0], rightdir[1], rightdir[2]);
	Math_Vector3Set(entup, updir[0], updir[1], updir[2]);
	Math_Vector3Set(entforward, -forwarddir[0], -forwarddir[1], -forwarddir[2]); /* we look down into -Z */
	Math_Vector3Normalize(entright);
	Math_Vector3Normalize(entup);
	Math_Vector3Normalize(entforward);
	if (!only_direction_vectors)
	{
		/* generate euler angles from direction vectors */
		Math_VecToAngles(entforward, entright, entup, entangles);

		Math_Vector3Copy((vec_t *)bodypos, entorigin);
		Math_Vector3Copy((vec_t *)bodyvel, entvel);
		Math_Vector3Copy((vec_t *)bodyavel, entavel);
	}

	if (!only_direction_vectors)
		Physics_UpdatePhysStats(w, ent, entorigin, entangles, entvel, entavel, base_e->onground);
	Physics_UpdatePhysDirections(w, ent, entforward, entright, entup);

	if (e->body)
		e->cached_linearvelocity = e->body->getLinearVelocity();
}

/*
===================
Sys_PhysicsUpdateGameStats

Updates the physics stats in a game entity
===================
*/
void Sys_PhysicsUpdateGameStats(void *physworld, int ent, int only_direction_vectors)
{
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];
	btTransform transform;

	/*
		TODO: only update if motionstate tells us so, see if this interpolation can cause us problems.
		Motion states were not being updated for static entities (for example, entities with bsp models)
		Also, fast moving entities can cause some weird issues with overshoots before exploding, etc
	*/
	/* if (e->mass > 0) TODO: see if setting the motionstate transform in Sys_PhysicsSetTransform solved this */
		e->body->getMotionState()->getWorldTransform(transform);
	/* else TODO: see if setting the motionstate transform in Sys_PhysicsSetTransform solved this
		transform = e->body->getWorldTransform(); */
	transform.setRotation(e->globalspace_rotation); /* use the saved rotation because some DOF may be locked in the physical bodies */

	Sys_PhysicsUpdateGameStatsStep(w, ent, ent, only_direction_vectors, transform);

	if (w->phys_entities[ent].type == PHYSICS_SHAPE_VEHICLE1)
	{
		phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)w->phys_entities[ent].extra_data;

		for (int i = 0; i < ed->m_vehicle->getNumWheels(); i++)
		{
			/* synchronize the wheels with the (interpolated) chassis worldtransform */
			ed->m_vehicle->updateWheelTransform(i, true);

			transform = ed->m_vehicle->getWheelInfo(i).m_worldTransform;
			/* TODO: return ed->m_vehicle->getWheelInfo(i).m_skidInfo !!! */
			Sys_PhysicsUpdateGameStatsStep(w, ed->construction_info.wheel_ents[i], ent, only_direction_vectors, transform);
		}
	}
}

/*
===================
Sys_PhysicsSimulate

Simulates up to frametime milliseconds in fixed steps

FIXME: Take care, in physics code that we simulate ourselves, about iteratively moving entities in discrete steps.
Explanation: hitscan weapons, for example, will benefit the player which will have physics evaluated first,
without giving a chance to another player evaluated last to shoot and kill the first player if he shot in
the same frame. This also causes various inconsistencies in movement, because one entity will move and an
entity that moves after it will evaluate his final position, not his initial nor if their paths incerpect
during the frametime.

If "entity_to_simulate" is different from "-1", only that entity will be simulated.
If "entity_to_simulate" is -1, "entities_ignore" must be an array (ending in -1) of entities to be ignored from this frame

TODO: interpolate to the server up to the frametime, because the fixed step may end earlier
TODO: if small objects don't come to rest, PHYS_STEPSIZE should be smaller :(
TODO: simulating only one entity may cause "jumps" in movement when pushing/being pushed
TODO: if entity wasn't simulated asynchronously for a frame, do it until it catches the sync world (with the last input received?)
TODO: see all problems caused by DISABLE_SIMULATION and removeRigidBody/removeRigidBody!!!
===================
*/
void Sys_PhysicsSimulate(void *physworld, mstime_t frametime, int entity_to_simulate, int *entities_ignore)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	int i, j, entities_ignore_idx;

	saved_entity_to_simulate = entity_to_simulate;
	if (entity_to_simulate != -1)
	{
		if (entities_ignore)
			Sys_Error("Sys_PhysicsSimulate: entity_to_simulate != -1 && entities_ignore\n");
		if (!w->phys_entities[entity_to_simulate].active)
			return;
		for (i = 0; i < MAX_EDICTS; i++)
		{
			cached_should_activate[i] = false;
			saved_active[i] = w->phys_entities[i].active;
			if (w->phys_entities[i].active && w->phys_entities[i].mass > 0 && i != entity_to_simulate)
			{
				saved_activation_states[i] = w->phys_entities[i].body->getActivationState();
				w->phys_entities[i].active = false;
				w->phys_entities[i].body->forceActivationState(DISABLE_SIMULATION); /* TODO: we want to collide with dynamic entities when they are not being simulated!!! */
			}
		}
		w->dynamics_world->setGravity(btVector3(0, 0, 0));
		w->phys_entities[entity_to_simulate].body->setGravity(btVector3(0, btScalar(phys_gravity_y->doublevalue), 0));
	}
	else
	{
		if (!entities_ignore)
			Sys_Error("Sys_PhysicsSimulate: entity_to_simulate == -1 && !entities_ignore\n");
		for (i = 0; i < MAX_EDICTS; i++)
			cached_should_activate[i] = false;
		saved_async_entities = &entities_ignore;
		for (entities_ignore_idx = 0; entities_ignore[entities_ignore_idx] != -1; entities_ignore_idx++)
		{
			i = entities_ignore[entities_ignore_idx];

			/* wheel entities have no physical representation, so no need to freeze */
			saved_active[i] = w->phys_entities[i].active;
			if (w->phys_entities[i].active && w->phys_entities[i].mass > 0)
			{
				/* reuse array for relinking */
				saved_activation_states[i] = w->phys_entities[i].body->isInWorld();
				w->phys_entities[i].active = false;
				if (saved_activation_states[i])
				{
					if (w->phys_entities[i].type == PHYSICS_SHAPE_VEHICLE1)
						w->dynamics_world->removeVehicle(((phys_edict_vehicle_extra_data_t *)w->phys_entities[i].extra_data)->m_vehicle);
					w->dynamics_world->removeRigidBody(w->phys_entities[i].body);
				}
			}
		}
	}

	w->dynamics_world->setForceUpdateAllAabbs(false); /* TODO: no need to set every frame? also see when saving/loading */
	/* run the world */
	w->dynamics_world->stepSimulation(btScalar((double)frametime / 1000.0), PHYS_MAXSUBSTEPS, btScalar(PHYS_SUBSTEPSIZE)); /* TODO: see if this really works (when calling with steps too small too!) and how it accumulates time */
	/* CProfileManager::dumpAll(); */

	for (i = 0; i < MAX_EDICTS; i++) /* TODO: motionstates for this (also useful for network!) */
		if (w->phys_entities[i].active)
		{
			Sys_PhysicsUpdateGameStats(physworld, i, false);
			if (w->phys_entities[i].lastonground + PHYS_ONGROUND_CLEAR_THRESHOLD < w->phys_time) /* depending on how jumping (for example) works, we may get different height jumps if we manage to keep the jump button pressed for different periods <= PHYS_ONGROUND_CLEAR_THRESHOLD, also delayed jumpings */
			{
				w->phys_entities[i].onground = false; /* clear it so that we may re-set it on the next iteration */
				w->phys_entities[i].ground_entity = 0;
			}
			w->phys_entities[i].jumped_this_frame = false;
		}

	/* run separately from Sys_PhysicsUpdateGameStats to make sure everyone has the most up to date values */
	for (i = 0; i < w->phys_num_collisions; i++)
	{
		/* TODO: flag to know if entities were already touched last frame? */
		Physics_TouchEnts(w, w->phys_collision_list[i].self, w->phys_collision_list[i].other, w->phys_collision_list[i].pos, w->phys_collision_list[i].normal, w->phys_collision_list[i].distance, w->phys_collision_list[i].reaction, w->phys_collision_list[i].impulse);
	}

	w->phys_num_collisions = 0;
	Physics_PostPhysics(w);

	if (entity_to_simulate != -1)
	{
		for (i = 0; i < MAX_EDICTS; i++)
		{
			if (saved_active[i] && w->phys_entities[i].mass > 0 && i != entity_to_simulate)
			{
				w->phys_entities[i].active = true;
				w->phys_entities[i].body->forceActivationState(saved_activation_states[i]);
				if (cached_should_activate[i])
					w->phys_entities[i].body->activate();
			}
		}
		w->dynamics_world->setGravity(btVector3(0, btScalar(phys_gravity_y->doublevalue), 0));
	}
	else
	{
		for (entities_ignore_idx = 0; entities_ignore[entities_ignore_idx] != -1; entities_ignore_idx++)
		{
			i = entities_ignore[entities_ignore_idx];

			/* wheel entities have no physical representation, so no need to freeze */
			if (saved_active[i] && w->phys_entities[i].mass > 0) /* updateSingleAabb not needed because we are only removing dynamic entities */
			{
				w->phys_entities[i].active = true;
				if (saved_activation_states[i])
				{
					w->dynamics_world->addRigidBody(w->phys_entities[i].body, w->phys_entities[i].solid_group, w->phys_entities[i].solid_mask);
					if (w->phys_entities[i].type == PHYSICS_SHAPE_VEHICLE1)
					{
						phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)w->phys_entities[i].extra_data;

						w->dynamics_world->addVehicle(ed->m_vehicle);
					}
				}
			}
		}
		for (i = 0; i < MAX_EDICTS; i++)
			if (cached_should_activate[i])
				w->phys_entities[i].body->activate();
	}
}

/*
===================
Sys_PhysicsIsDynamic

Called to know if an entity is part of the dynamic world
===================
*/
int Sys_PhysicsIsDynamic(void *physworld, int ent)
{
	if (!physworld)
		return false;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];

	if (!e->active) /* no physical representation */
		return false;

	if (e->mass > 0 && e->solid != SOLID_WORLD_NOT && e->solid != SOLID_WORLD_TRIGGER)
		return true;

	return false;
}

/*
===================
Sys_PhysicsSetTransform

Force origin and rotation on a physics representation of an entity
origin, angles or locked_angles may be NULL to keep the current value

IMPORTANT: This, Sys_PhysicsSetSolidState and Sys_PhysicsPreFrame should
be the only functions removing and adding a body to the collision world
other than the creation and destruction functions
===================
*/
void Sys_PhysicsSetTransform(void *physworld, int ent, const vec3_t origin, const vec3_t angles, const vec3_t locked_angles)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];
	vec3_t representation_angles;
	unsigned int anglesflags = Physics_GetAnglesFlags(w, ent);

	if (!e->active) /* no physical representation, no harm done */
		return;

	if (!origin && !angles && !locked_angles)
	{
		Sys_Printf("Sys_PhysicsSetTransform: !origin && !angles && !locked_angles for entity %d\n", ent);
		return;
	}

	/* decide which angles to use in the physical representation */
	if (locked_angles)
	{
		if (!(anglesflags & (ANGLES_KINEMATICANGLES_LOCK_PITCH_BIT | ANGLES_KINEMATICANGLES_LOCK_ROLL_BIT | ANGLES_KINEMATICANGLES_LOCK_YAW_BIT)))
			Host_Error("Sys_PhysicsSetTransform: locked_angles set but entity doesn't have any locked angles\n");

		Math_Vector3Copy(locked_angles, representation_angles);
	}
	else if (angles)
	{
		Math_Vector3Copy(angles, representation_angles);
	}

	/* body transform */
	btTransform new_transform = e->body->getWorldTransform();
	if (origin)
		new_transform.setOrigin(btVector3(origin[0], origin[1], origin[2]));
	if (angles || locked_angles)
	{
		btQuaternion quat(Math_Deg2Rad(representation_angles[ANGLES_YAW]), Math_Deg2Rad(representation_angles[ANGLES_PITCH]), Math_Deg2Rad(representation_angles[ANGLES_ROLL]));
		new_transform.setRotation(quat);
	}
	e->body->setWorldTransform(new_transform);
	/* motion state transform */
	e->body->getMotionState()->getWorldTransform(new_transform);
	if (origin)
		new_transform.setOrigin(btVector3(origin[0], origin[1], origin[2]));
	if (angles || locked_angles)
	{
		btQuaternion quat(Math_Deg2Rad(representation_angles[ANGLES_YAW]), Math_Deg2Rad(representation_angles[ANGLES_PITCH]), Math_Deg2Rad(representation_angles[ANGLES_ROLL]));
		new_transform.setRotation(quat);
	}
	e->body->getMotionState()->setWorldTransform(new_transform);

	/* send our new direction vectors back to the game code and set other entity-wide vars */
	if (angles)
	{
		/* default constructor for btQuaternion is in YAW-PITCH-ROLL order */
		e->globalspace_rotation = btQuaternion(Math_Deg2Rad(angles[ANGLES_YAW]), Math_Deg2Rad(angles[ANGLES_PITCH]), Math_Deg2Rad(angles[ANGLES_ROLL]));
		Sys_PhysicsUpdateGameStats(physworld, ent, true);
	}

	/* TODO: see other places we are relinking to see if we should behave differently with kinematic/static bodies */
	/* TODO: these should be called when changing from static to kinematic and vice-versa and when moving a kinematic for the first time, not here */
	/* TODO: is this useful when teleporting dynamic entities? */
	/* TODO: run these only if TELEPORTING!!! Also see if the cleanProxyFromPairs for vehicle is only useful when TELEPORTING / messes with kinematic platforms moving the vehicle */
#if 0
	if (origin)
	{
		e->body->setInterpolationWorldTransform();
		e->body->setInterpolationLinearVelocity(btVector3(0,0,0));
	}
	if (angles || locked_angles)
		e->body->setInterpolationAngularVelocity(btVector3(0,0,0));
#endif

	if (e->type == PHYSICS_SHAPE_VEHICLE1)
	{
		/* TODO: for other types too? as options? */
#if 0
		e->body->setCenterOfMassTransform(btTransform::getIdentity());
		e->body->setLinearVelocity(btVector3(0, 0, 0));
		e->body->setAngularVelocity(btVector3(0, 0, 0));
		if (m_vehicle)
		{
			m_vehicle->resetSuspension();
			for (int i = 0; i < m_vehicle->getNumWheels(); i++)
			{
				/* synchronize the wheels with the (interpolated) chassis worldtransform */
				m_vehicle->updateWheelTransform(i, true);
			}
		}
#endif
		w->dynamics_world->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(e->body->getBroadphaseHandle(), w->dynamics_world->getDispatcher());
	}
}

/*
===================
Sys_PhysicsSetSolidState

Set the various collision-related stuff of an entity.

IMPORTANT: This and Sys_PhysicsSetTransform should be the only functions
removing and adding a body to the collision world other than the creation
and destruction functions

TODO: test behavior for all values and see if they fit the description in game_sv.h!
TODO: default values when creating?
===================
*/
void Sys_PhysicsSetSolidState(void *physworld, int ent, const unsigned int value)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];
	unsigned int group, mask;

	if (!w->phys_entities[ent].active) /* no physical representation */
	{
		/* TODO CONSOLEDEBUG Sys_Printf("Trying to set a collision category on entity %d without an active physical representation\n", ent); */
		return;
	}

	/* TODO: use defaultfilter and allfilter? */
	group = 0; /* default to zero: if we pass invalid values, we will end up with no collision at all for this entity */
	mask = 0xFFFFFFFF;

	/* world-world will never collide, so let's maximize performance by never even testing them */
	if (value == SOLID_WORLD || value == SOLID_WORLD_NOT || value == SOLID_WORLD_TRIGGER)
	{
		group |= COLLISION_CATEGORY_WORLD;
		mask -= mask & COLLISION_CATEGORY_WORLD;
	}
	if (value == SOLID_ENTITY || value == SOLID_ENTITY_TRIGGER || value == SOLID_ENTITY_WITHWORLDONLY)
	{
		group |= COLLISION_CATEGORY_ENTITY;
	}
	/* trigger-trigger will never collide, so let's maximize performance by never even testing them */
	if (value == SOLID_WORLD_TRIGGER || value == SOLID_ENTITY_TRIGGER)
	{
		group |= COLLISION_CATEGORY_TRIGGER;
		mask -= mask & COLLISION_CATEGORY_TRIGGER;
	}
	/* these won't collide with anyone, but if it's an entity it will collide with world */
	if (value == SOLID_WORLD_NOT || value == SOLID_ENTITY_WITHWORLDONLY)
	{
		mask -= mask & (COLLISION_CATEGORY_ENTITY | COLLISION_CATEGORY_TRIGGER);
	}

	e->relink = true;
	if (e->body->isInWorld())
	{
		if (e->type == PHYSICS_SHAPE_VEHICLE1)
			w->dynamics_world->removeVehicle(((phys_edict_vehicle_extra_data_t *)e->extra_data)->m_vehicle);
		w->dynamics_world->removeRigidBody(e->body);
	}

	/* save for when we need to remove and add this entity again FIXME: search for all instances of addRigidBody in the code */
	e->solid = value;
	e->solid_group = group;
	e->solid_mask = mask;
}

class btMyVehicleRaycaster : public btVehicleRaycaster
{
	btDynamicsWorld	*m_dynamicsWorld;
public:
	btMyVehicleRaycaster(btDynamicsWorld *world) : m_dynamicsWorld(world) {}

	virtual void *castRay(const btVector3 &from, const btVector3 &to, btVehicleRaycasterResult &result);
};

void *btMyVehicleRaycaster::castRay(const btVector3 &from, const btVector3 &to, btVehicleRaycasterResult &result)
{
	/* TODO: only calculate this when necessary, cache for a frame/subframe, use more rays and not only from origin, do a FAST check for voxels and ignore them in this check, etc */
	class Closest : public btCollisionWorld::ClosestRayResultCallback
	{
		public:
			Closest(const btVector3 &rayFromWorld, const btVector3 &rayToWorld, int ignore_all_triggers) : btCollisionWorld::ClosestRayResultCallback(rayFromWorld, rayToWorld)
			{
				m_ignore_all_triggers = ignore_all_triggers;
			}

			virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult &rayResult, bool normalInWorldSpace)
			{
				/* also ignore "SOLID_ENTITY_WITHWORLDONLY" */
				if (m_ignore_all_triggers && (((phys_edict_t *)rayResult.m_collisionObject->getCollisionShape()->getUserPointer())->solid_group & COLLISION_CATEGORY_TRIGGER || ((phys_edict_t *)rayResult.m_collisionObject->getCollisionShape()->getUserPointer())->solid == SOLID_ENTITY_WITHWORLDONLY))
					return 1.0;

				return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
			}
		protected:
			int m_ignore_all_triggers;
	};

	Closest rayCallback(from, to, true);
	m_dynamicsWorld->rayTest(from, to, rayCallback);

	if (rayCallback.hasHit())
	{

		const btRigidBody *body = btRigidBody::upcast(rayCallback.m_collisionObject);
        if (body && body->hasContactResponse())
		{
			result.m_hitPointInWorld = rayCallback.m_hitPointWorld;
			result.m_hitNormalInWorld = rayCallback.m_hitNormalWorld;
			result.m_hitNormalInWorld.normalize();
			result.m_distFraction = rayCallback.m_closestHitFraction;
			return (void *)body;
		}
	}
	return 0;
}

/*
===================
Sys_PhysicsCreateObject

Creates a rigid body to represent the entity in the physics world.

Meaning of "data":
type == PHYSICS_SHAPE_BOX: 3 of type vec_t, half-extents of the box (half the side size). This means that the box is symmetrical.
type == PHYSICS_SHAPE_SPHERE: 1 of type vec_t, radius of the sphere.
type == PHYSICS_SHAPE_CAPSULE_Y: 2 of type vec_t, radius of the spheres and Y-separation between spheres (the result will be a convex hull of the two spheres, a capsule, with total height of Y-separation + 2 * radius)
type == PHYSICS_SHAPE_TRIMESH_FROM_MODEL: a precacheindex_t
type == PHYSICS_SHAPE_TRIMESH_FROM_DATA: a model_trimesh_t struct
type == PHYSICS_SHAPE_CONVEXHULLS_FROM_MODEL: a precacheindex_t
type == PHYSICS_SHAPE_CONVEXHULLS_FROM_DATA: a model_brushes_t struct
type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL: a precacheindex_t
type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA: a model_heightfield_t struct
type == PHYSICS_SHAPE_VEHICLE1: a phys_edict_vehicle_info_t struct

If mass == 0 or -1, then it's a kinematic (0) or static (-1) body (infinite mass), otherwise it's a dynamic body.

Returns "true" if the creation is successful

TODO FIXME: CCD IS SLOOOOW, try not to make things move too fast! Have a sv_maxspeed cvar? Because explosion forces may be out of control

TODO: kinematic if ent != 0 (world) for stuff that will REALLY move
TODO: callbacks for surfaceflags, contents, etc upon contact
TODO: bullet collision margin, scaling of rendering size or physical size
TODO: linear and angular damping, linear and angular max speed, contact max correcting velocity, contact penetration allowed
TODO: anisotropic friction? (linear and rolling)
TODO: droptofloor option (convex cast)
TODO: see which parameters apply to which types, for example some should cause an error if locked_angles is set?
TODO: for complex vehicle geometries, just create vehicle floor (with phantom bodies if necessary for balance) and then make the complex stuff movetype_follow with owner set to the vehicle floor for no collision between then?
===================
*/
int Sys_PhysicsCreateObject(void *physworld, int ent, int type, void *data, vec_t mass, vec3_t origin, vec3_t angles, vec3_t locked_angles, int trace_onground)
{
	if (!physworld)
		return false;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];

	/* clear any previous representation */
	if (e->active)
		Sys_PhysicsDestroyObject(physworld, ent);

	if (!data) /* no data, maybe everything is nonsolid */
		return false;

	e->active = true;
	/* store our index for callback purposes */
	e->index = ent;
	e->myworld = physworld;
	e->mass = mass;
	e->relink = true;
	e->type = type;
	e->trace_onground = trace_onground;

	/* transform */
	btTransform transform;
	transform.setIdentity();

	/* TODO: just set smaller margins using setMargin for smaller objects instead of upping the sizes to minimum_dimension? */
	btScalar minimum_dimension = CONVEX_DISTANCE_MARGIN * 4.f; /* TODO: Erwin Coumans says the minimum should be 0.2 units */
	if (type == PHYSICS_SHAPE_BOX)
	{
		e->creation_vecs[0] = ((vec_t *)data)[0];
		e->creation_vecs[1] = ((vec_t *)data)[1];
		e->creation_vecs[2] = ((vec_t *)data)[2];

		btVector3 halfsize = btVector3(((vec_t *)data)[0], ((vec_t *)data)[1], ((vec_t *)data)[2]);
		if (halfsize[0] < 0 || halfsize[1] < 0 || halfsize[2] < 0)
			Sys_Error("Sys_PhysicsCreateObject: Negative dimension for entity %d\n", ent);
		halfsize[0] = btMax(halfsize[0], minimum_dimension / 2.f);
		halfsize[1] = btMax(halfsize[1], minimum_dimension / 2.f);
		halfsize[2] = btMax(halfsize[2], minimum_dimension / 2.f);

		/* collision shape */
		btBoxShape *shape = new btBoxShape(halfsize);
		shape->setUserPointer(e);
		e->shape = shape;

		/* rigid body */
		btVector3 local_inertia(0, 0, 0);
		if (mass > 0)
			shape->calculateLocalInertia(btScalar(mass), local_inertia);

		/* using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects */
		btDefaultMotionState *motion_state = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rb_info(mass < 0 ? 0 : mass, motion_state, shape, local_inertia);
		btRigidBody *body = new btRigidBody(rb_info);
		body->setUserIndex(ent);

		/* set the smallest possible dimension radius for ccd */
		btScalar radius = btMin(halfsize[0], halfsize[1]);
		radius = btMin(radius, halfsize[2]);
		e->radius = radius;
#ifdef USE_CONTINUOUS
		body->setCcdMotionThreshold(radius * 2);
		body->setCcdSweptSphereRadius(radius / 5);
#endif
		/* TODO: CCD body->setCcdMotionThreshold((radius * 2) * (radius * 2)); /* bullet expects a squared value here to avoid square roots */
		/* TODO: CCD body->setCcdSweptSphereRadius(radius * 2 / 5); */
		/*
		this works better but is SLOWER
		btScalar radius = btMin(halfsize[0], halfsize[1]);
		radius = btMin(radius, halfsize[2]);
		btScalar maxradius = btSqrt(btDot(halfsize, halfsize));
		body->setCcdMotionThreshold(radius / 2);
		body->setCcdSweptSphereRadius(maxradius);
		*/
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);

		/* TODO: set these three correctly */
		body->setFriction(1);
		if (mass <= 0) /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
			body->setRollingFriction(1);
		body->setRestitution(0.5);
		if (mass <= 0) /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
			body->setAnisotropicFriction(shape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);

		SET_CONTACT_PROCESSING_THRESHOLD;
		e->body = body;
	}
	else if (type == PHYSICS_SHAPE_SPHERE)
	{
		e->creation_vecs[0] = ((vec_t *)data)[0];

		btScalar sphereradius = ((vec_t *)data)[0];
		if (sphereradius < 0)
			Sys_Error("Sys_PhysicsCreateObject: Negative dimension for entity %d\n", ent);
		sphereradius = btMax(sphereradius, minimum_dimension / 2.f);

		/* collision shape */
		btSphereShape *shape = new btSphereShape(sphereradius);
		shape->setUserPointer(e);
		e->shape = shape;

		/* rigid body */
		btVector3 local_inertia(0, 0, 0);
		if (mass > 0)
			shape->calculateLocalInertia(btScalar(mass), local_inertia);

		/* using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects */
		btDefaultMotionState *motion_state = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rb_info(mass < 0 ? 0 : mass, motion_state, shape, local_inertia);
		btRigidBody *body = new btRigidBody(rb_info);
		body->setUserIndex(ent);

		btScalar radius = sphereradius;
		e->radius = radius;
#ifdef USE_CONTINUOUS
		body->setCcdMotionThreshold(radius * 2);
		body->setCcdSweptSphereRadius(radius / 5);
#endif
		/* TODO: CCD body->setCcdMotionThreshold((radius * 2) * (radius * 2)); /* bullet expects a squared value here to avoid square roots */
		/* TODO: CCD body->setCcdSweptSphereRadius(radius * 2 / 5); */
		/*
		this works better but is SLOWER
		btScalar radius = sphereradius;
		body->setCcdMotionThreshold(radius);
		body->setCcdSweptSphereRadius(radius);
		*/
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
		body->setAnisotropicFriction(shape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);

		/* TODO: set these three correctly */
		body->setFriction(10);
		body->setRollingFriction(1);
		body->setRestitution(0.5);

		SET_CONTACT_PROCESSING_THRESHOLD;
		e->body = body;
	}
	else if (type == PHYSICS_SHAPE_CAPSULE_Y)
	{
		e->creation_vecs[0] = ((vec_t *)data)[0];
		e->creation_vecs[1] = ((vec_t *)data)[1];

		btScalar capsulesizes[2] = {((vec_t *)data)[0], ((vec_t *)data)[1]};
		/* no need to check for height, since if height == 0 it will degenerate into a sphere by using the radius */
		if (capsulesizes[0] < 0)
			Sys_Error("Sys_PhysicsCreateObject: Negative dimension for entity %d\n", ent);
		capsulesizes[0] = btMax(capsulesizes[0], minimum_dimension / 2.f);

		/* collision shape */
		btCapsuleShape *shape = new btCapsuleShape(capsulesizes[0], capsulesizes[1]);
		shape->setUserPointer(e);
		e->shape = shape;

		/* rigid body */
		btVector3 local_inertia(0, 0, 0);
		if (mass > 0)
			shape->calculateLocalInertia(btScalar(mass), local_inertia);

		/* using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects */
		btDefaultMotionState *motion_state = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rb_info(mass < 0 ? 0 : mass, motion_state, shape, local_inertia);
		btRigidBody *body = new btRigidBody(rb_info);
		body->setUserIndex(ent);

		/* set the smallest possible dimension radius for ccd, the radius of the caps */
		btScalar radius = capsulesizes[0];
		e->radius = radius;
#ifdef USE_CONTINUOUS
		body->setCcdMotionThreshold(radius * 2);
		body->setCcdSweptSphereRadius(radius / 5);
#endif
		/* TODO: CCD body->setCcdMotionThreshold((radius * 2) * (radius * 2)); /* bullet expects a squared value here to avoid square roots */
		/* TODO: CCD body->setCcdSweptSphereRadius(radius * 2 / 5); */
		/*
		this works better but is SLOWER
		btScalar radius = capsulesizes[0];
		btScalar maxradius = capsulesizes[1] / 2.f + capsulesizes[0];
		body->setCcdMotionThreshold(radius);
		body->setCcdSweptSphereRadius(maxradius);
		*/
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);

		/* TODO: set these three correctly */
		body->setFriction(1);
		body->setRollingFriction(1);
		body->setRestitution(0);
		body->setAnisotropicFriction(shape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);

		SET_CONTACT_PROCESSING_THRESHOLD;
		e->body = body;
	}
	else if (type == PHYSICS_SHAPE_TRIMESH_FROM_MODEL || type == PHYSICS_SHAPE_TRIMESH_FROM_DATA) /* TODO: analyze options for doing this, taking care of ccd-capable stuff */
	{
		/* TODO: check for minimum dimensions here? */
		/* TODO FIXME: btCompoundShape seems intended for convex child shapes only (or recursive btCompoundShape) FIX THIS HERE */
		int i;
		model_trimesh_t *trimesh;

		if (type == PHYSICS_SHAPE_TRIMESH_FROM_DATA)
		{
			trimesh = (model_trimesh_t *)data;
		}
		else
		{
			e->creation_model = *(precacheindex_t *)data;
			Physics_GetModelPhysicsTrimesh(w, *(precacheindex_t *)data, &trimesh);
			if (!trimesh)
			{
				Sys_PhysicsDestroyObject(physworld, ent);
				return false;
			}
		}

		if (trimesh->num_trimeshes < 1)
			Host_Error("Got %d trimeshes for entity %d\n", trimesh->num_trimeshes, ent);
		/* TODO CONSOLEDEBUG else
			Sys_Printf("Got %d trimeshes for entity %d\n", trimesh->num_trimeshes, ent); */

		/* collision shape */
		btCollisionShape *final_shape = NULL;
		btCompoundShape *compound_shape = NULL;
		if (trimesh->num_trimeshes > 1)
		{
			compound_shape = new btCompoundShape(true); /* TODO: false uses WAY less memory, but is slower */
			final_shape = compound_shape;
		}

		for (i = 0; i < trimesh->num_trimeshes; i++)
		{
			if(!trimesh->trimeshes[i].index_count || !trimesh->trimeshes[i].vert_count)
				continue;

			btVector3 aabb_min, aabb_max;
			btTriangleIndexVertexArray *index_vertex_arrays = new btTriangleIndexVertexArray(trimesh->trimeshes[i].index_count / 3, trimesh->trimeshes[i].indexes, trimesh->trimeshes[i].index_stride, trimesh->trimeshes[i].vert_count, (btScalar *)trimesh->trimeshes[i].verts, trimesh->trimeshes[i].vert_stride);
			index_vertex_arrays->calculateAabbBruteForce(aabb_min, aabb_max);
			/* TODO FIXME: set this correctly? */
			bool use_quantized_aabb_compression = true;
			btBvhTriangleMeshShape *trishape = new btBvhTriangleMeshShape(index_vertex_arrays, use_quantized_aabb_compression, aabb_min, aabb_max);
			trishape->setUserPointer(e);
			if (trimesh->num_trimeshes == 1)
				final_shape = trishape;
			else
				compound_shape->addChildShape(transform, trishape);
		}

		if (!final_shape)
			Host_Error("Empty trimesh for %d\n", ent);
		final_shape->setUserPointer(e); /* done twice if only one trimesh, it's ok */
		e->shape = final_shape;

		/* rigid body */
		btVector3 local_inertia(0, 0, 0);
		if (mass > 0)
			final_shape->calculateLocalInertia(btScalar(mass), local_inertia);

		/* using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects */
		btDefaultMotionState *motion_state = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rb_info(e->mass < 0 ? 0 : e->mass, motion_state, final_shape, local_inertia);
		btRigidBody *body = new btRigidBody(rb_info);
		body->setUserIndex(ent);

		/* TODO: set ccd? */
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);

		/* TODO: set these three correctly */
		body->setFriction(1);
		if (mass <= 0) /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
			body->setRollingFriction(1);
		body->setRestitution(0.5);
		if (mass <= 0) /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
			body->setAnisotropicFriction(final_shape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);

		SET_CONTACT_PROCESSING_THRESHOLD;
		e->body = body;
	}
	else if (type == PHYSICS_SHAPE_CONVEXHULLS_FROM_MODEL || type == PHYSICS_SHAPE_CONVEXHULLS_FROM_DATA) /* TODO: analyze other options for doing this, taking care of ccd-capable stuff */
	{
		/* TODO: check for minimum dimensions here? */
		model_brushes_t *brushes;

		if (type == PHYSICS_SHAPE_CONVEXHULLS_FROM_DATA)
		{
			brushes = (model_brushes_t *)data;
		}
		else
		{
			e->creation_model = *(precacheindex_t *)data;
			Physics_GetModelPhysicsBrushes(w, *(precacheindex_t *)data, &brushes);
			if (!brushes)
			{
				Sys_PhysicsDestroyObject(physworld, ent);
				return false;
			}
		}

		vec_t *cur_normal = brushes->normal;
		vec_t *cur_dist = brushes->dist;
		int i, j;

		if (brushes->num_brushes < 1)
			Host_Error("Got %d brushes in brush list for entity %d\n", brushes->num_brushes, ent);
		/* TODO CONSOLEDEBUG else
			Sys_Printf("Got %d brushes in brush list for entity %d\n", brushes->num_brushes, ent); */

		/* collision shape */
		btCollisionShape *final_shape = NULL;
		btCompoundShape *compound_shape = NULL;
		if (brushes->num_brushes > 1)
		{
			compound_shape = new btCompoundShape(true); /* TODO: false uses WAY less memory, but is slower */
			final_shape = compound_shape;
		}

		for (i = 0; i < brushes->num_brushes; i++)
		{
			btAlignedObjectArray<btVector3> plane_equations;

			for (j = 0; j < brushes->brush_sides[i]; j++)
			{
				btVector3 plane;

				plane.setValue(cur_normal[0], cur_normal[1], cur_normal[2]);
				plane[3] = -cur_dist[0]; /* the 4th unused component of btVector3 is useable by the user FIXME: in the future...? TODO: find out why we need to reverse this! this may get us in trouble! */
				plane_equations.push_back(plane);

				cur_normal += 3;
				cur_dist++;
			}

			btAlignedObjectArray<btVector3>	vertices;
			btGeometryUtil::getVerticesFromPlaneEquations(plane_equations, vertices);
			if (vertices.size() > 0)
			{
				/* this creates an internal copy of the vertices TODO: I've no idea how c++ allocation work, so make sure that there are no memory leaks when exiting this function */
				btConvexHullShape *originalbrushshape = new btConvexHullShape(&(vertices[0].getX()), vertices.size());

				/* create a hull approximation */
				btShapeHull *hull = new btShapeHull(originalbrushshape);
				btScalar margin = originalbrushshape->getMargin();
				hull->buildHull(margin);
				btConvexHullShape *brushshape = new btConvexHullShape((btScalar *)hull->getVertexPointer(), hull->numVertices()); /* TODO: dangerous casting */
				delete hull;
				delete originalbrushshape;

				brushshape->setUserPointer(e);
				brushshape->setMargin(0); /* TODO FIXME: this somewhat keeps objects spawning close to convex hulls from exploding, allowing pre-configured stacks - FIX THE BEHAVIOR WITHOUT REMOVING THE MARGIN */
				if (brushes->num_brushes == 1)
					final_shape = brushshape;
				else
					compound_shape->addChildShape(transform, brushshape);
			}
		}

		if (!final_shape)
			Host_Error("Empty brush set for %d\n", ent);
		final_shape->setUserPointer(e); /* done twice if only one brush, it's ok */
		e->shape = final_shape;

		/* rigid body */
		btVector3 local_inertia(0, 0, 0);
		if (mass > 0)
			final_shape->calculateLocalInertia(btScalar(mass), local_inertia);

		/* using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects */
		btDefaultMotionState *motion_state = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rb_info(mass < 0 ? 0 : mass, motion_state, final_shape, local_inertia);
		btRigidBody *body = new btRigidBody(rb_info);
		body->setUserIndex(ent);

		/* TODO: set ccd? */
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);

		/* TODO: set these three correctly */
		body->setFriction(1);
		if (mass <= 0) /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
			body->setRollingFriction(1);
		body->setRestitution(0.5);
		if (mass <= 0) /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
			body->setAnisotropicFriction(final_shape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);

		SET_CONTACT_PROCESSING_THRESHOLD;
		e->body = body;
	}
	else if (type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL || type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA) /* TODO: analyze other options for doing this, taking care of ccd-capable stuff */
	{
		/* TODO: check for minimum dimensions here? */
		model_heightfield_t *orig_heightfield;

		if (type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA)
		{
			orig_heightfield = (model_heightfield_t *)data;
		}
		else
		{
			e->creation_model = *(precacheindex_t *)data;
			Physics_GetModelPhysicsHeightfield(w, *(precacheindex_t *)data, &orig_heightfield);
			if (!orig_heightfield)
			{
				Sys_PhysicsDestroyObject(physworld, ent);
				return false;
			}
		}

		/* copy because bullet doesn't keep heightfield data */
		model_heightfield_t *heightfield = new model_heightfield_t;
		memcpy(heightfield, orig_heightfield, sizeof(model_heightfield_t));
		heightfield->data = new vec_t[heightfield->width * heightfield->length];
		memcpy(heightfield->data, orig_heightfield->data, sizeof(vec_t) * heightfield->width * heightfield->length);
		e->extra_data = heightfield;

		/* TODO CONSOLEDEBUG Sys_Printf("Got %d x %d heightfield for entity %d\n", heightfield->width, heightfield->length, ent); */

		/* collision shape */

		/* bullet centers the heightfield on the AABB defined with the mins/maxs, so we have to do this TODO: do faster*/
		btScalar absolutemax(heightfield->maxheight);
		btScalar absolutemin(heightfield->minheight);
		absolutemax = abs(absolutemax);
		absolutemin = abs(absolutemin);
		btScalar realabsolute;
		if (absolutemax > absolutemin)
			realabsolute = absolutemax;
		else
			realabsolute = absolutemin;

		/* TODO: check true/false for flipquadedges, shape->m_useDiamondSubdivision, etc to make the physical EQUAL to the graphical */
		btHeightfieldTerrainShape *shape = new btHeightfieldTerrainShape(heightfield->width, heightfield->length, heightfield->data, 1, -realabsolute, realabsolute, 1, PHY_FLOAT, false);
		btVector3 localScaling(heightfield->width_scale, 1, heightfield->length_scale);
		shape->setLocalScaling(localScaling);
		shape->setMargin(0.16f); /* TODO FIXME: do this for voxels too? */ /* TODO: this (or higher value) makes some pass-through not happen BUT IS UGLY VISUALLY. Adjusting scale for good size triangles seems to help. */

		shape->setUserPointer(e);
		e->shape = shape;

		/* rigid body */

		btVector3 local_inertia(0, 0, 0);
		if (mass > 0)
			shape->calculateLocalInertia(btScalar(mass), local_inertia);

		/* using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects */
		btDefaultMotionState *motion_state = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rb_info(mass < 0 ? 0 : mass, motion_state, shape, local_inertia);
		btRigidBody *body = new btRigidBody(rb_info);
		body->setUserIndex(ent);

		/* TODO: set ccd? */
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);

		/* TODO: set these three correctly, a new map for each one? */
		body->setFriction(1);
		if (mass <= 0) /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
			body->setRollingFriction(1);
		body->setRestitution(0.5);
		if (mass <= 0) /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
			body->setAnisotropicFriction(shape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);

		SET_CONTACT_PROCESSING_THRESHOLD;
		e->body = body;
	}
	else if (type == PHYSICS_SHAPE_VEHICLE1)
	{
		e->extra_data = new phys_edict_vehicle_extra_data_t;
		phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)e->extra_data;
		ed->construction_info = *(phys_edict_vehicle_info_t *)data;

		btCollisionShape* chassisShape = new btBoxShape(btVector3(ed->construction_info.chassis_box_half_extents[0], ed->construction_info.chassis_box_half_extents[1], ed->construction_info.chassis_box_half_extents[2]));
		btTransform localTrans;
		localTrans.setIdentity();
		/* localTrans effectively shifts the center of mass with respect to the chassis */
		localTrans.setOrigin(btVector3(ed->construction_info.chassis_box_localpos[0], ed->construction_info.chassis_box_localpos[1], ed->construction_info.chassis_box_localpos[2]));
		chassisShape->setUserPointer(e);

		btCollisionShape* suppShape = new btBoxShape(btVector3(ed->construction_info.suppchassis_box_half_extents[0], ed->construction_info.suppchassis_box_half_extents[1], ed->construction_info.suppchassis_box_half_extents[2]));
		btTransform suppLocalTrans;
		suppLocalTrans.setIdentity();
		/* suppLocalTrans effectively shifts the center of mass with respect to the chassis */
		localTrans.setOrigin(btVector3(ed->construction_info.suppchassis_box_localpos[0], ed->construction_info.suppchassis_box_localpos[1], ed->construction_info.suppchassis_box_localpos[2]));
		suppShape->setUserPointer(e);

		btCompoundShape* shape = new btCompoundShape();
		shape->addChildShape(localTrans,chassisShape);
		shape->addChildShape(suppLocalTrans, suppShape);

		shape->setUserPointer(e);
		e->shape = shape;

		btVector3 local_inertia(0, 0, 0);
		if (mass > 0)
			shape->calculateLocalInertia(btScalar(mass), local_inertia);

		/* using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects */
		btDefaultMotionState *motion_state = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rb_info(mass < 0 ? 0 : mass, motion_state, shape, local_inertia);
		btRigidBody *body = new btRigidBody(rb_info);
		body->setUserIndex(ent);

		/* TODO: set ccd? */
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);

		SET_CONTACT_PROCESSING_THRESHOLD;
		e->body = body;

		/* do extra data now */
		/* !!! THERE IS A MIRROR OF THE PARTS BELOW AT THE LOADING CODE */
		ed->m_wheelShape = new btCylinderShapeX(btVector3(ed->construction_info.wheelWidth, ed->construction_info.wheelRadius, ed->construction_info.wheelRadius));
		ed->m_wheelShape->setUserPointer(e);

		ed->m_vehicleRayCaster = new btMyVehicleRaycaster(w->dynamics_world);
		ed->m_vehicle = new btRaycastVehicle(ed->m_tuning, e->body, ed->m_vehicleRayCaster);

		/* never deactivate the vehicle */
		e->body->setActivationState(DISABLE_DEACTIVATION);

		bool isFrontWheel = true;

		btVector3 bt_wheelDirectionCS0(ed->construction_info.wheelDirectionCS0[0], ed->construction_info.wheelDirectionCS0[1], ed->construction_info.wheelDirectionCS0[2]);
		btVector3 bt_wheelAxleCS(ed->construction_info.wheelAxleCS[0], ed->construction_info.wheelAxleCS[1], ed->construction_info.wheelAxleCS[2]);

		/* choose coordinate system */
		ed->m_vehicle->setCoordinateSystem(0, 1, 2);

		btVector3 connectionPointCS0(ed->construction_info.chassis_box_half_extents[0] - (ed->construction_info.connectionStickLateralOutWheelWidthMultiplier * ed->construction_info.wheelWidth), ed->construction_info.connectionHeight, -(ed->construction_info.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier * ed->construction_info.chassis_box_half_extents[2]) + ed->construction_info.wheelRadius);
		ed->m_vehicle->addWheel(connectionPointCS0, bt_wheelDirectionCS0, bt_wheelAxleCS, ed->construction_info.suspensionRestLength, ed->construction_info.wheelRadius, ed->m_tuning, isFrontWheel);

		connectionPointCS0 = btVector3(-ed->construction_info.chassis_box_half_extents[0] + (ed->construction_info.connectionStickLateralOutWheelWidthMultiplier * ed->construction_info.wheelWidth), ed->construction_info.connectionHeight, -(ed->construction_info.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier * ed->construction_info.chassis_box_half_extents[2]) + ed->construction_info.wheelRadius);
		ed->m_vehicle->addWheel(connectionPointCS0, bt_wheelDirectionCS0, bt_wheelAxleCS, ed->construction_info.suspensionRestLength, ed->construction_info.wheelRadius, ed->m_tuning, isFrontWheel);

		connectionPointCS0 = btVector3(-ed->construction_info.chassis_box_half_extents[0] + (ed->construction_info.connectionStickLateralOutWheelWidthMultiplier * ed->construction_info.wheelWidth), ed->construction_info.connectionHeight, (ed->construction_info.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier * ed->construction_info.chassis_box_half_extents[2]) - ed->construction_info.wheelRadius);
		isFrontWheel = false;
		ed->m_vehicle->addWheel(connectionPointCS0, bt_wheelDirectionCS0, bt_wheelAxleCS, ed->construction_info.suspensionRestLength, ed->construction_info.wheelRadius, ed->m_tuning, isFrontWheel);

		connectionPointCS0 = btVector3(ed->construction_info.chassis_box_half_extents[0] - (ed->construction_info.connectionStickLateralOutWheelWidthMultiplier * ed->construction_info.wheelWidth), ed->construction_info.connectionHeight, (ed->construction_info.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier * ed->construction_info.chassis_box_half_extents[2]) - ed->construction_info.wheelRadius);
		ed->m_vehicle->addWheel(connectionPointCS0, bt_wheelDirectionCS0, bt_wheelAxleCS, ed->construction_info.suspensionRestLength, ed->construction_info.wheelRadius, ed->m_tuning, isFrontWheel);

		for (int i = 0; i < ed->m_vehicle->getNumWheels(); i++)
		{
			btWheelInfo &wheel = ed->m_vehicle->getWheelInfo(i);
			wheel.m_suspensionStiffness = ed->construction_info.suspensionStiffness;
			wheel.m_wheelsDampingRelaxation = ed->construction_info.suspensionDamping;
			wheel.m_wheelsDampingCompression = ed->construction_info.suspensionCompression;
			wheel.m_frictionSlip = ed->construction_info.wheelFriction;
			wheel.m_rollInfluence = ed->construction_info.rollInfluence;
			wheel.m_maxSuspensionTravelCm = ed->construction_info.maxSuspensionTravelCm;
			wheel.m_maxSuspensionForce = ed->construction_info.maxSuspensionForce;
		}
	}
	else
	{
		Host_Error("Sys_PhysicsCreateObject: unknown type %d\n", type);
	}

	if (!e->body || !e->shape)
	{
		Sys_Error("Sys_PhysicsCreateObject: Entity %d has no body or collision shape\n", ent);
		Sys_PhysicsDestroyObject(physworld, ent);
		return false;
	}

	e->collision_margin = e->shape->getMargin(); /* TODO FIXME: do this for voxels too? */
	/* e->body->setDamping(0.25f, 0.25f); /* TODO FIXME: only use this when we have non-ccd capable shapes (I activated this because I was falling through voxels if falling from too high and small items were falling through heightfields, but these issues seem to be fixed now) */
	if (!mass) /* for kinematic bodies */
	{
		/* TODO: should we do this for voxels? */
		btVector3 inertia(0, 0, 0);
		e->body->setMassProps(0.0, inertia);
		e->body->updateInertiaTensor();

		 /* TODO: do this only when moving? */
		e->body->setCollisionFlags(e->body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		 /* TODO: do this only when moving? */
		e->body->setActivationState(DISABLE_DEACTIVATION);
	}
	else if (mass == -1)
	{
		e->body->setCollisionFlags(e->body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
		e->body->setActivationState(ACTIVE_TAG);
	}

	/* set this stuff here instead of when creating the body to automatically send stuff like direction vectors to the engine */
	Sys_PhysicsSetTransform(physworld, ent, origin, angles, locked_angles);

	return true;
}

/*
===================
Sys_PhysicsDestroyObject

Destroys the physical representation of an entity
===================
*/
void Sys_PhysicsDestroyObject(void *physworld, int ent)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_edict_t *e = &w->phys_entities[ent];
	if (e->active)
	{
		e->active = false;
		e->index = 0;
		e->myworld = NULL;
		e->mass = 0;
		e->solid = 0;
		e->solid_group = btBroadphaseProxy::DefaultFilter;
		e->solid_mask = btBroadphaseProxy::AllFilter;
		e->onground = 0;
		e->onground_normal = btVector3(0, 0, 0);
		e->ground_entity = 0;
		e->trace_onground = 0;
		e->lastonground = 0;
		e->sleep_counter = 0;
		e->sleep_state = ACTIVE_TAG;
		e->collision_margin = 0;
		e->jumped_this_frame = false;

		if (e->body && e->body->getMotionState())
			delete e->body->getMotionState();
		if (e->body)
		{
			if (e->body->isInWorld())
			{
				if (e->type == PHYSICS_SHAPE_VEHICLE1)
					w->dynamics_world->removeVehicle(((phys_edict_vehicle_extra_data_t *)e->extra_data)->m_vehicle);
				w->dynamics_world->removeRigidBody(e->body);
			}
			delete e->body;
			e->body = NULL;
		}

		if (e->shape && (e->type == PHYSICS_SHAPE_TRIMESH_FROM_MODEL || e->type == PHYSICS_SHAPE_TRIMESH_FROM_DATA || e->type == PHYSICS_SHAPE_CONVEXHULLS_FROM_MODEL || e->type == PHYSICS_SHAPE_CONVEXHULLS_FROM_DATA || e->type == PHYSICS_SHAPE_VEHICLE1) && e->shape->isCompound())
		{
			btCompoundShape *compound = (btCompoundShape *)e->shape;
			for (int i = 0; i < compound->getNumChildShapes(); i++)
				if (compound->getChildShape(i))
				{
					if (e->type == PHYSICS_SHAPE_TRIMESH_FROM_MODEL || e->type == PHYSICS_SHAPE_TRIMESH_FROM_DATA)
					{
						btBvhTriangleMeshShape *trimesh = (btBvhTriangleMeshShape *)compound->getChildShape(i);
						if (trimesh->getMeshInterface())
							delete trimesh->getMeshInterface();
					}
					delete compound->getChildShape(i);
				}
		}
		if (e->shape)
		{
			if ((e->type == PHYSICS_SHAPE_TRIMESH_FROM_MODEL || e->type == PHYSICS_SHAPE_TRIMESH_FROM_DATA) && !e->shape->isCompound())
			{
				btBvhTriangleMeshShape *trimesh = (btBvhTriangleMeshShape *)e->shape;
				if (trimesh->getMeshInterface())
					delete trimesh->getMeshInterface();
			}
			delete e->shape;
			e->shape = NULL;
		}

		e->globalspace_rotation.setValue(0, 0, 0, 0);
		e->relink = false;
		e->type = -1;

		if (e->extra_data)
		{
			if (e->type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL || e->type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA)
			{
				model_heightfield_t *heightfield = (model_heightfield_t *)e->extra_data;
				if (heightfield->data)
					delete[] heightfield->data;
				delete heightfield;
			}
			else if (e->type == PHYSICS_SHAPE_VEHICLE1)
			{
				phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)e->extra_data;
				if (ed->m_vehicleRayCaster)
					delete ed->m_vehicleRayCaster;
				if (ed->m_vehicle)
					delete ed->m_vehicle;
				if (ed->m_wheelShape)
					delete ed->m_wheelShape;
				delete ed;
			}
		}
		e->extra_data = NULL;
		e->radius = 0;
		Math_ClearVector3(e->creation_vecs);
		e->creation_model = 0;
		e->cached_linearvelocity = btVector3(0, 0, 0);
	}
}

/*
===================
Sys_PhysicsCreateVoxelChunk

Creates a rigid body to represent the voxel chunk in the physics world.

Input are fields from a model_trimesh_part_t;

-Type PHYSICS_SHAPE_VOXEL_BOX allows for fast collision detection and rebuild,
uses parameter 3-5 (the "half_size" parameter is the same as on
Sys_PhysicsCreateObject, "box_origins" are 3-component origins packed
"num_boxes" times)
-Type PHYSICS_SHAPE_VOXEL_TRIMESH allows for smoothed terrains, uses parameters 6+;

TODO: decide which type to call based on input data
TODO: callbacks for surfaceflags, contents, etc upon contact
TODO: bullet collision margin, scaling of rendering size or physical size
TODO: anisotropic friction? (linear and rolling)
TODO: apparently bullet doesn't copy the trimesh data, is this true?
===================
*/
void Sys_PhysicsCreateVoxelChunk(void *physworld, int chunk, int type, vec3_t half_size, vec_t *box_origins, int num_boxes, vec_t *verts, int vert_stride, int vert_count, int *indexes, int index_count, int index_stride)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_voxel_chunk_t *c = &w->phys_voxel_chunks[chunk];

	/* clear any previous representation */
	if (c->active)
		Sys_PhysicsDestroyVoxelChunk(physworld, chunk);

	if(type == PHYSICS_SHAPE_VOXEL_TRIMESH && (!index_count || !vert_count))
		return; /* no data, maybe everything is nonsolid */

	if(type == PHYSICS_SHAPE_VOXEL_BOX && (!num_boxes))
		return; /* no data, maybe everything is nonsolid */

	if (type != PHYSICS_SHAPE_VOXEL_BOX && type != PHYSICS_SHAPE_VOXEL_TRIMESH)
		Host_Error("Sys_PhysicsCreateVoxelChunk: unsupported type for this operation: %d\n", type);

	c->active = true;
	c->type = type;

	/* transform */
	btTransform transform;
	transform.setIdentity();

	btRigidBody *body;
	if (type == PHYSICS_SHAPE_VOXEL_BOX)
	{
		/* TODO: check for minimum dimensions here? */
		/* collision shape */
		btCompoundShape *shape = new btCompoundShape(true); /* TODO: false uses WAY less memory, but is slower */

		for (int i = 0; i < num_boxes; i++)
		{
			btBoxShape *boxshape = new btBoxShape(btVector3(half_size[0], half_size[1], half_size[2]));
			boxshape->setUserPointer(&w->phys_entities[0]); /* point to world for now TODO */

			transform.setOrigin(btVector3(box_origins[i * 3 + 0], box_origins[i * 3 + 1], box_origins[i * 3 + 2]));

			shape->addChildShape(transform, boxshape);
		}

		transform.setIdentity(); /* reset for using with the body */

		shape->setUserPointer(&w->phys_entities[0]); /* point to world for now TODO */
		c->shape = shape;

		/* rigid body */
		btVector3 local_inertia(0, 0, 0);

		/* using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects */
		btDefaultMotionState *motion_state = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rb_info(0, motion_state, shape, local_inertia);
		body = new btRigidBody(rb_info);
		body->setUserIndex(-chunk - 1);

		/* TODO: set ccd? */
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);

		/* TODO: set these three correctly */
		body->setFriction(1);
		body->setRollingFriction(1); /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
		body->setRestitution(0.5);
		body->setAnisotropicFriction(shape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION); /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */

		SET_CONTACT_PROCESSING_THRESHOLD;
		c->body = body;
	}
	else if (type == PHYSICS_SHAPE_VOXEL_TRIMESH)
	{
		/* TODO: check for minimum dimensions here? */
		/* TODO: analyze options for doing this, taking care of ccd-capable stuff */
		/* collision shape */
		btVector3 aabb_min, aabb_max;
		btTriangleIndexVertexArray *index_vertex_arrays = new btTriangleIndexVertexArray(index_count / 3, indexes, index_stride, vert_count, (btScalar *)verts, vert_stride);
		index_vertex_arrays->calculateAabbBruteForce(aabb_min, aabb_max);
		/* TODO FIXME: set this correctly? */
		bool use_quantized_aabb_compression = true;
		btBvhTriangleMeshShape *shape = new btBvhTriangleMeshShape(index_vertex_arrays, use_quantized_aabb_compression, aabb_min, aabb_max);
		shape->setUserPointer(&w->phys_entities[0]); /* point to world for now TODO */
		c->shape = shape;

		/* rigid body */
		btVector3 local_inertia(0, 0, 0);

		/* using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects */
		btDefaultMotionState *motion_state = new btDefaultMotionState(transform);
		btRigidBody::btRigidBodyConstructionInfo rb_info(0, motion_state, shape, local_inertia);
		body = new btRigidBody(rb_info);
		body->setUserIndex(-chunk - 1);

		/* TODO: set ccd? */
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);

		/* TODO: set these three correctly */
		body->setFriction(1);
		body->setRollingFriction(1); /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */
		body->setRestitution(0.5);
		body->setAnisotropicFriction(shape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION); /* set rolling friction on if something will roll on us TODO FIXME: do we need to? */

		SET_CONTACT_PROCESSING_THRESHOLD;
		c->body = body;
	}

	/* emulate a Sys_PhysicsSetSolidState called with SOLID_WORLD TODO: change this if we change something in Sys_PhysicsSetSolidState */
	unsigned int solid_group = COLLISION_CATEGORY_WORLD;
	unsigned int solid_mask = 0xFFFFFFFF - COLLISION_CATEGORY_WORLD;
	w->dynamics_world->addRigidBody(body, solid_group, solid_mask);
	w->dynamics_world->updateSingleAabb(body); /* for running dynamics world with setForceUpdateAllAabbs(false) */
	/* TODO: if we do not have an entity zero (we currently point voxels to entity zero) with COLLISION_CATEGORY_WORLD/SOLID_WORLD then collisions with voxels may MISBEHAVE (I've seen items fall through) */
	/* TODO: THIS IS TEMPORARY UNTIL THE ENTITY 0 WITHOUT MODEL ISSUE IS SOLVED! for this to work, the voxels must be created after trying to spawn an empty worldspawn (because trying to spawn it will destroy entity 0 physics data) */
	if (!w->phys_entities[0].active)
	{
		w->phys_entities[0].myworld = w;
		w->phys_entities[0].solid = SOLID_WORLD;
		w->phys_entities[0].solid_group = solid_group;
		w->phys_entities[0].solid_mask = solid_mask;
	}

	c->body->setCollisionFlags(c->body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
	c->body->setActivationState(ACTIVE_TAG);

	/* TODO CONSOLEDEBUG Sys_Printf("Chunk %d physical representation created\n", chunk); */
}

/*
===================
Sys_PhysicsDestroyVoxelChunk

Destroys the physical representation of a voxel chunk
===================
*/
void Sys_PhysicsDestroyVoxelChunk(void *physworld, int chunk)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	phys_voxel_chunk_t *c = &w->phys_voxel_chunks[chunk];
	if (c->active)
	{
		c->active = false;

		if (c->body && c->body->getMotionState())
			delete c->body->getMotionState();
		if (c->body)
		{
			if (c->body->isInWorld())
				w->dynamics_world->removeRigidBody(c->body);
			delete c->body;
			c->body = NULL;
		}

		if (c->shape && c->type == PHYSICS_SHAPE_VOXEL_TRIMESH)
		{
			btBvhTriangleMeshShape *trimesh = (btBvhTriangleMeshShape *)c->shape;
			if (trimesh->getMeshInterface())
				delete trimesh->getMeshInterface();
		}
		else if (c->shape && c->type == PHYSICS_SHAPE_VOXEL_BOX)
		{
			btCompoundShape *compound = (btCompoundShape *)c->shape;
			for (int i = 0; i < compound->getNumChildShapes(); i++)
				if (compound->getChildShape(i))
					delete compound->getChildShape(i);
		}
		if (c->shape)
		{
			delete c->shape;
			c->shape = NULL;
		}

		c->type = -1;
	}
}

/*
===================
Sys_PhysicsTraceline

The line doesn't collide with "forent"
If "ignore_world_triggers", ignores hits with triggers that are SOLID_WORLD_TRIGGER
If set, "impulse_to_closest" will be applied as an impulse to the closest hit.
In Bullet, collision mask bit 0 (btBroadphaseProxy::DefaultFilter) is used to filter ray collision FIXME: implement this
Returns the index in the game_edict_t.trace_* arrays of the closest result, -1 if nothing was hit;

TODO: which triangle did we hit?
TODO: sort by trace_fraction
TODO: a nomonsters flag? ignore forent, forent->owner,, *->owner = forent and *->owner = forent->owner?
TODO: category bits interaction? get category bits from forent? and solid stuff interaction too! (what about entities we can pass through but can shoot???)
TODO: return hit face material, contents, flags, etc...
TODO: option to get only closest hit
===================
*/
int Sys_PhysicsTraceline(void *physworld, int forent, vec3_t origin, vec3_t forward, vec_t length, int ignore_world_triggers, vec_t impulse_to_closest)
{
	if (!physworld)
		return -1;
	physics_world_t *w = (physics_world_t *)physworld;
	int i;
	int closestidx_edicttrace = -1; /* index in the edict->trace_* struct */
	int closestidx_rayresult = -1; /* index in the ray struct */

	btVector3 start(origin[0], origin[1], origin[2]);
	btVector3 end(origin[0] + forward[0] * length, origin[1] + forward[1] * length, origin[2] + forward[2] * length);
	btCollisionWorld::AllHitsRayResultCallback ray_callback(start, end); /* return all hits */
	ray_callback.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces; /* cull backfaces TODO: only of trimeshes? what if we start tracing from inside but still want a hit? Make it possible to specify this */
	/*
	ray_callback.m_collisionFilterGroup |= COLLISION_CATEGORY_ENTITY | COLLISION_CATEGORY_WORLD | COLLISION_CATEGORY_TRIGGER;
	ray_callback.m_collisionFilterMask |= COLLISION_CATEGORY_ENTITY | COLLISION_CATEGORY_WORLD | COLLISION_CATEGORY_TRIGGER;
	*/
	/* TODO: see effects of solid group and mask heritage */
	ray_callback.m_collisionFilterGroup |= w->phys_entities[forent].solid_group;
	ray_callback.m_collisionFilterMask |= w->phys_entities[forent].solid_mask;

	w->dynamics_world->rayTest(start, end, ray_callback);

	Physics_UpdateTraceResultStart(w, forent);
	if (ray_callback.hasHit())
	{
		for (i = 0; i < ray_callback.m_collisionObjects.size(); i++)
		{
			phys_edict_t *hit = (phys_edict_t *)ray_callback.m_collisionObjects[i]->getCollisionShape()->getUserPointer();
			int current_edict_trace_idx; /* the index in the edcit->trace_* structure will be different than "i" */

			if (hit->index == forent)
				continue; /* ignore forent */

			/* TODO: finish solid checking with collision groups and masks heritage, etc */
			if (ignore_world_triggers && (hit->solid_group & COLLISION_CATEGORY_TRIGGER) && (hit->solid_group & COLLISION_CATEGORY_WORLD)) /* can't check TRIGGER | WORLD because that would kill ANY trigger and ANY world */
				continue;

			vec_t fraction = ray_callback.m_hitFractions[i];
			vec3_t normal = {ray_callback.m_hitNormalWorld[i].getX(), ray_callback.m_hitNormalWorld[i].getY(), ray_callback.m_hitNormalWorld[i].getZ()};
			vec3_t pos = {ray_callback.m_hitPointWorld[i].getX(), ray_callback.m_hitPointWorld[i].getY(), ray_callback.m_hitPointWorld[i].getZ()};

			/* TODO: this can return more than one hit for the same entity? (if a entity has multiple bodies, for example */
			current_edict_trace_idx = Physics_UpdateTraceResultStep(w, forent, hit->index, pos, normal, fraction);

			if (current_edict_trace_idx == -1)
				continue; /* nothing else to update if this was very far */

			if (closestidx_rayresult == -1)
			{
				/* initialize to the first hit (not necessarily the closest) */
				closestidx_edicttrace = current_edict_trace_idx;
				closestidx_rayresult = i;
			}
			else if (fraction < ray_callback.m_hitFractions[closestidx_rayresult])
			{
				/* update the closest hit (we can't just use ray_callback.m_closestHitFraction because it incluses ourselves TODO: flags to make it only collid what we want to consider) */
				closestidx_edicttrace = current_edict_trace_idx;
				closestidx_rayresult = i;
			}
		}

		/* apply impulse to the closest hit if wanted, except if it is world TODO: is this the fastest way? do world since it will be ignored? */
		if (closestidx_edicttrace != -1 && impulse_to_closest && ((phys_edict_t *)ray_callback.m_collisionObjects[closestidx_rayresult]->getCollisionShape()->getUserPointer())->index)
		{
			phys_edict_t *hit = (phys_edict_t *)ray_callback.m_collisionObjects[closestidx_rayresult]->getCollisionShape()->getUserPointer();
			btVector3 btimpulse = btVector3(end - start).normalized() * impulse_to_closest;
			btVector3 btorigin = ray_callback.m_hitPointWorld[closestidx_rayresult] - hit->body->getWorldTransform().getOrigin();
			vec3_t impulse = {btimpulse.getX(), btimpulse.getY(), btimpulse.getZ()};
			vec3_t origin = {btorigin.getX(), btorigin.getY(), btorigin.getZ()};
			Sys_PhysicsApplyImpulse(physworld, hit->index, impulse, origin);
		}
	}

	return closestidx_edicttrace;
}

/*
===================
Sys_CreatePhysicsWorld
===================
*/
void *Sys_CreatePhysicsWorld(uint8_t server)
{
	entindex_t i, j;

	/* TODO CONSOLEDEBUG Sys_Printf("Creating physics world...\n"); */
	physics_world_t *w = new physics_world_t;
	memset(w, 0, sizeof(physics_world_t));

	/* TODO: curiosity: when dealing with floats, is all-zeros really zero? or nan? or what? */
	w->server = server;
	w->phys_time = 0;
	w->phys_substep_frametime = 0;

	/* collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration. FIXME? */
	w->collision_configuration = new btDefaultCollisionConfiguration();
	/* use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded) TODO */
	w->dispatcher = new Sys_PhysicsCollisionDispatcher(w->collision_configuration);
	/* btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxisSweep3/bt32BitAxisSweep3. TODO: are these the best options? */
	w->overlapping_pair_cache = new btDbvtBroadphase();
	/* the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded) TODO */
	w->solver = new btSequentialImpulseConstraintSolver;
	w->dynamics_world = new btDiscreteDynamicsWorld(w->dispatcher, w->overlapping_pair_cache, w->solver, w->collision_configuration);

	w->dynamics_world->setGravity(btVector3(0, btScalar(phys_gravity_y->doublevalue), 0));
#ifdef USE_CONTINUOUS
	w->dynamics_world->getDispatchInfo().m_useContinuous = true;
#endif
	w->dynamics_world->setInternalTickCallback(Sys_PhysicsInternalTickPreCallback, w, true);
	w->dynamics_world->setInternalTickCallback(Sys_PhysicsInternalTickPostCallback, w, false);

	w->debug_drawer = new GLDebugDrawer();
	w->debug_drawer->setDebugMode(btIDebugDraw::DBG_DrawNormals | btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb | btIDebugDraw::DBG_DrawConstraints | btIDebugDraw::DBG_DrawConstraintLimits);
	w->dynamics_world->setDebugDrawer(w->debug_drawer);
	w->dynamics_world->setWorldUserInfo((void *)w);

	for (i = 0; i < MAX_EDICTS; i++)
	{
		w->phys_entities[i].active = false;
		w->phys_entities[i].index = 0; /* everyone is world by default */
		w->phys_entities[i].myworld = NULL;
		w->phys_entities[i].mass = 0;
		w->phys_entities[i].solid = 0;
		w->phys_entities[i].solid_group = btBroadphaseProxy::DefaultFilter;
		w->phys_entities[i].solid_mask = btBroadphaseProxy::AllFilter;
		w->phys_entities[i].onground = 0;
		w->phys_entities[i].onground_normal = btVector3(0, 0, 0);
		w->phys_entities[i].ground_entity = 0;
		w->phys_entities[i].trace_onground = 0;
		w->phys_entities[i].lastonground = 0;
		w->phys_entities[i].sleep_counter = 0;
		w->phys_entities[i].sleep_state = ACTIVE_TAG;
		w->phys_entities[i].collision_margin = 0;
		w->phys_entities[i].jumped_this_frame = false;
		w->phys_entities[i].shape = NULL;
		w->phys_entities[i].body = NULL;
		w->phys_entities[i].globalspace_rotation.setValue(0, 0, 0, 0);
		w->phys_entities[i].relink = false;
		w->phys_entities[i].type = -1;
		w->phys_entities[i].extra_data = NULL;
		w->phys_entities[i].radius = 0;
		Math_ClearVector3(w->phys_entities[i].creation_vecs);
		w->phys_entities[i].creation_model = 0;
		w->phys_entities[i].cached_linearvelocity = btVector3(0, 0, 0);
	}

	for (i = 0; i < VOXEL_MAX_CHUNKS; i++)
	{
		w->phys_voxel_chunks[i].active = false;
		w->phys_voxel_chunks[i].shape = NULL;
		w->phys_voxel_chunks[i].body = NULL;
		w->phys_voxel_chunks[i].type = -1;
	}

	/* reset collision data */
	w->phys_num_collisions = 0;
	for (i = 0; i < MAX_EDICTS; i++)
		for (j = 0; j < MAX_EDICTS; j++)
			w->phys_collision_matrix[i][j] = false;

	return w;
}

/*
===================
Sys_DestroyPhysicsWorld
===================
*/
void Sys_DestroyPhysicsWorld(void *physworld)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;
	int i;

	if (!physworld)
		return;

	/* TODO CONSOLEDEBUG Sys_Printf("Destroying physics world...\n"); */

	for (i = 0; i < MAX_EDICTS; i++)
		Sys_PhysicsDestroyObject(physworld, i);

	for (i = 0; i < VOXEL_MAX_CHUNKS; i++)
		Sys_PhysicsDestroyVoxelChunk(physworld, i);

	/* destroy stuff in inverse order */
	delete w->dynamics_world;
	delete w->solver;
	delete w->overlapping_pair_cache;
	delete w->dispatcher;
	delete w->collision_configuration;
	delete w->debug_drawer;

	/* TODO: is this done automatically? */
	w->dynamics_world = NULL;
	w->solver = NULL;
	w->overlapping_pair_cache = NULL;
	w->dispatcher = NULL;
	w->collision_configuration = NULL;
	w->debug_drawer = NULL;
}

/*
===================
Sys_PhysicsSaveFileNames

Little helper function
===================
*/
void Sys_PhysicsSaveFileNames(char *name, char *out1, char *out2)
{
	Sys_Snprintf(out1, FILENAME_MAX, "%s.phys", name);
	Sys_Snprintf(out2, FILENAME_MAX, "%s.physproperties", name);
}

/*
===================
Sys_LoadPhysicsWorld

TODO: ENDIANNESS, ALIGNMENT, ARCHITECTURE/ABI PADDING, KEEP IN SYNC
TODO: USE HOST_FS* FOR FILE I/O
NOTE: there is a bug in the bullet btBulletWorldImporter (fixed on
26 Aug 2014) that makes capsules bigger every save/load cycle
NOTE: child shapes are created two times in compound shapes? Has
this been fixed?
===================
*/
void Sys_LoadPhysicsWorld(void *physworld, char *name)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;

	char file1[FILENAME_MAX];
	char file2[FILENAME_MAX];
	Sys_PhysicsSaveFileNames(name, file1, file2);

	for (entindex_t i = 0; i < MAX_EDICTS; i++)
		Sys_PhysicsDestroyObject(physworld, i);
	for (int i = 0; i < VOXEL_MAX_CHUNKS; i++)
		Sys_PhysicsDestroyVoxelChunk(physworld, i);

	btBulletWorldImporter *fileLoader = new btBulletWorldImporter(w->dynamics_world);

	/* optionally enable the verbose mode to provide debugging information during file loading (a lot of data is generated, so this option is very slow) */
	/* fileLoader->setVerboseMode(true); */

	/* this will open a file directly from the filesystem */
	/* fileLoader->loadFile(file1); */

	/* read from memory instead */

	int lowmark = Sys_MemLowMark(&tmp_mem);
	char *memoryBuffer;
	int len;

	len = Host_FSLoadBinaryFile(file1, &tmp_mem, "savegamephys", (unsigned char **)&memoryBuffer, false);
	fileLoader->loadFileFromMemory(memoryBuffer, len);

	delete fileLoader;

	Sys_MemFreeToLowMark(&tmp_mem, lowmark);

	/* now load stuff not restored by the bullet internal tool */

	void *file = Host_FSFileHandleOpenBinaryRead(file2);
	Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->server, sizeof(uint8_t));
	Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_time, sizeof(mstime_t));
	entindex_t count;
	Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&count, sizeof(entindex_t));
	int num_heightfields = 0;
	for (entindex_t i = 0; i < count; i++)
	{
		entindex_t index;

		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&index, sizeof(entindex_t));

		w->phys_entities[index].active = true;
		w->phys_entities[index].index = index;

		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].collision_margin, sizeof(btScalar));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].globalspace_rotation, sizeof(btQuaternion));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].jumped_this_frame, sizeof(int));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].lastonground, sizeof(mstime_t));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].mass, sizeof(vec_t));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].onground, sizeof(int));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].onground_normal, sizeof(btVector3));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].ground_entity, sizeof(int));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].relink, sizeof(int));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].sleep_counter, sizeof(double));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].sleep_state, sizeof(int));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].solid, sizeof(unsigned int));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].solid_group, sizeof(unsigned int));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].solid_mask, sizeof(unsigned int));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].trace_onground, sizeof(int));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].type, sizeof(int));

		if (w->phys_entities[index].type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL || w->phys_entities[index].type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA)
		{
			model_heightfield_t *heightfield = new model_heightfield_t;
			/* save even the pointer to the data, just a little wasted space and is easier TODO FIXME */
			Host_FSFileHandleReadBinaryDest(file, (unsigned char *)heightfield, sizeof(model_heightfield_t));
			heightfield->data = new vec_t[heightfield->width * heightfield->length];
			Host_FSFileHandleReadBinaryDest(file, (unsigned char *)heightfield->data, sizeof(vec_t) * heightfield->width * heightfield->length);
			num_heightfields++;

			/*
				Heightfields are NOT saved (or loaded?) by the bullet default serializer (or world importer?)
				Let's just hope that all heightfields in the game are static and have no locked angles, because
				when recreating we lose lots of info on dynamic bodies. This is also OK with the
				btCollisionObject iterations further in the loading code because when creating we only set
				relink and not put the new object into the game world.
			*/
			int solid;
			solid = w->phys_entities[index].solid;
			vec3_t hf_origin, hf_angles;
			Physics_EntityGetData(w, index, hf_origin, NULL, NULL, hf_angles, NULL, NULL, NULL, NULL);
			Sys_PhysicsCreateObject(w, index, w->phys_entities[index].type, heightfield, w->phys_entities[index].mass, hf_origin, hf_angles, NULL, w->phys_entities[index].trace_onground);
			Sys_PhysicsSetSolidState(w, index, solid);

			delete[] heightfield->data;
			delete heightfield;
		}
		if (w->phys_entities[index].type == PHYSICS_SHAPE_VEHICLE1)
		{
			phys_edict_t *e = &w->phys_entities[index];
			phys_edict_vehicle_extra_data_t *ed = new phys_edict_vehicle_extra_data_t;
			/* save even the pointer to the data, just a little wasted space and is easier TODO FIXME */
			Host_FSFileHandleReadBinaryDest(file, (unsigned char *)ed, sizeof(phys_edict_vehicle_extra_data_t));

			/*
				Vehicles are NOT TOTALLY saved (or loaded?) by the bullet default serializer (or world importer?)
				This WILL create some discrepancies in loaded games compared to the game that was running,
				because when recreating we lose lots of info on dynamic bodies. This is OK with the
				btCollisionObject iterations further in the loading code because when creating we only set
				relink and not put the new object into the game world.
			*/
			e->extra_data = ed;

			/* EXTRA DATA WILL BE READ LATER - WE NEED SOME POINTERS SET */
		}
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].radius, sizeof(btScalar));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].creation_vecs, sizeof(vec3_t));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].creation_model, sizeof(precacheindex_t));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_entities[index].cached_linearvelocity, sizeof(btVector3));
	}
	int voxelcount;
	Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&voxelcount, sizeof(int));
	for (int i = 0; i < voxelcount; i++)
	{
		int index;
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&index, sizeof(int));
		w->phys_voxel_chunks[index].active = true;
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&w->phys_voxel_chunks[index].type, sizeof(int));
	}

	w->dynamics_world->setWorldUserInfo((void *)w);
	int colobjsize = w->dynamics_world->getCollisionObjectArray().size();
	int colobjsizesaved;
	Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&colobjsizesaved, sizeof(int));
	if (colobjsize + num_heightfields != colobjsizesaved) /* we know that heightfields are not saved */
		Sys_Error("colobjsize %d != colobjsizesaved %d\n", colobjsize, colobjsizesaved);
	for (int i = 0; i < colobjsize; i++)
	{
		btCollisionObject *obj = w->dynamics_world->getCollisionObjectArray()[i];
		int userindex;
		/* hope that the saved objects will have the SAME order in the loaded dynamics world! TODO FIXME */
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&userindex, sizeof(int));
		btTransform worldtransform;
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&worldtransform, sizeof(btTransform));
		obj->setWorldTransform(worldtransform);
		btTransform motionstateworldtransform;
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&motionstateworldtransform, sizeof(btTransform));
		btDefaultMotionState *motion_state = new btDefaultMotionState(motionstateworldtransform);
		((btRigidBody *)obj)->setMotionState(motion_state);
		int collisionflags;
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&collisionflags, sizeof(int));
		obj->setCollisionFlags(collisionflags);
		btVector3 avel, lvel;
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&avel, sizeof(btVector3));
		((btRigidBody *)obj)->setAngularVelocity(avel);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&lvel, sizeof(btVector3));
		((btRigidBody *)obj)->setLinearVelocity(lvel);
		btScalar scalar, scalar2;
		btVector3 vector3;
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&scalar, sizeof(btScalar));
		((btRigidBody *)obj)->setCcdMotionThreshold(scalar);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&scalar, sizeof(btScalar));
		((btRigidBody *)obj)->setCcdSweptSphereRadius(scalar);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&scalar, sizeof(btScalar));
		((btRigidBody *)obj)->setFriction(scalar);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&scalar, sizeof(btScalar));
		((btRigidBody *)obj)->setRollingFriction(scalar);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&scalar, sizeof(btScalar));
		((btRigidBody *)obj)->setRestitution(scalar);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&vector3, sizeof(btVector3));
		((btRigidBody *)obj)->setAnisotropicFriction(vector3, btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&scalar, sizeof(btScalar));
		((btRigidBody *)obj)->setContactProcessingThreshold(scalar);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&scalar, sizeof(btScalar));
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&scalar2, sizeof(btScalar));
		((btRigidBody *)obj)->setDamping(scalar, scalar2);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&vector3, sizeof(btVector3));
		((btRigidBody *)obj)->setAngularFactor(vector3);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&vector3, sizeof(btVector3));
		((btRigidBody *)obj)->setInterpolationAngularVelocity(vector3);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&vector3, sizeof(btVector3));
		((btRigidBody *)obj)->setGravity(vector3);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&scalar, sizeof(btScalar));
		((btRigidBody *)obj)->getCollisionShape()->setMargin(scalar);
		Host_FSFileHandleReadBinaryDest(file, (unsigned char *)&vector3, sizeof(btVector3));
		((btRigidBody *)obj)->getCollisionShape()->setLocalScaling(vector3);

		if (userindex >= 0)
		{
			phys_edict_t *e = &w->phys_entities[userindex];

			e->myworld = physworld;
			e->body = (btRigidBody *)obj;
			e->body->setUserIndex(userindex);

			btCollisionShape *objshape = obj->getCollisionShape();
			objshape->setUserPointer(e);
			e->shape = objshape;
			/* one level of compound shapes */
			if (e->shape->isCompound())
			{
				btCompoundShape *compound = (btCompoundShape *)e->shape;
				for (int j = 0; j < compound->getNumChildShapes(); j++)
				{
					if (compound->getChildShape(j))
					{
						compound->getChildShape(j)->setUserPointer(e);
					}
				}
			}
		}
		else
		{
			phys_voxel_chunk_t *c = &w->phys_voxel_chunks[-userindex - 1];

			c->body = (btRigidBody *)obj;
			c->body->setUserIndex(userindex);

			btCollisionShape *objshape = obj->getCollisionShape();
			objshape->setUserPointer(&w->phys_entities[0]); /* point to world for now TODO */
			c->shape = objshape;
			/* one level of compound shapes */
			if (c->shape->isCompound())
			{
				btCompoundShape *compound = (btCompoundShape *)c->shape;
				for (int j = 0; j < compound->getNumChildShapes(); j++)
				{
					if (compound->getChildShape(j))
					{
						compound->getChildShape(j)->setUserPointer(&w->phys_entities[0]); /* point to world for now TODO */
					}
				}
			}
		}
	}

	Host_FSFileHandleClose(file);

	for (entindex_t i = 0; i < MAX_EDICTS; i++)
	{
		if (w->phys_entities[i].active && w->phys_entities[i].type == PHYSICS_SHAPE_VEHICLE1)
		{
			phys_edict_t *e = &w->phys_entities[i];
			phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)w->phys_entities[i].extra_data;

			/* !!! EXTRA DATA RECREATION - HERE BECAUSE WE NEED SOME POINTERS - THIS IS A MIRROR OF THE FINAL PART OF Sys_PhysicsCreateObject */
			ed->m_wheelShape = new btCylinderShapeX(btVector3(ed->construction_info.wheelWidth, ed->construction_info.wheelRadius, ed->construction_info.wheelRadius));
			ed->m_wheelShape->setUserPointer(e);

			ed->m_vehicleRayCaster = new btMyVehicleRaycaster(w->dynamics_world);
			ed->m_vehicle = new btRaycastVehicle(ed->m_tuning, e->body, ed->m_vehicleRayCaster);

			/* never deactivate the vehicle */
			e->body->setActivationState(DISABLE_DEACTIVATION);

			bool isFrontWheel = true;

			btVector3 bt_wheelDirectionCS0(ed->construction_info.wheelDirectionCS0[0], ed->construction_info.wheelDirectionCS0[1], ed->construction_info.wheelDirectionCS0[2]);
			btVector3 bt_wheelAxleCS(ed->construction_info.wheelAxleCS[0], ed->construction_info.wheelAxleCS[1], ed->construction_info.wheelAxleCS[2]);

			/* choose coordinate system */
			ed->m_vehicle->setCoordinateSystem(0, 1, 2);

			btVector3 connectionPointCS0(ed->construction_info.chassis_box_half_extents[0] - (ed->construction_info.connectionStickLateralOutWheelWidthMultiplier * ed->construction_info.wheelWidth), ed->construction_info.connectionHeight, -(ed->construction_info.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier * ed->construction_info.chassis_box_half_extents[2]) + ed->construction_info.wheelRadius);
			ed->m_vehicle->addWheel(connectionPointCS0, bt_wheelDirectionCS0, bt_wheelAxleCS, ed->construction_info.suspensionRestLength, ed->construction_info.wheelRadius, ed->m_tuning, isFrontWheel);

			connectionPointCS0 = btVector3(-ed->construction_info.chassis_box_half_extents[0] + (ed->construction_info.connectionStickLateralOutWheelWidthMultiplier * ed->construction_info.wheelWidth), ed->construction_info.connectionHeight, -(ed->construction_info.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier * ed->construction_info.chassis_box_half_extents[2]) + ed->construction_info.wheelRadius);
			ed->m_vehicle->addWheel(connectionPointCS0, bt_wheelDirectionCS0, bt_wheelAxleCS, ed->construction_info.suspensionRestLength, ed->construction_info.wheelRadius, ed->m_tuning, isFrontWheel);

			connectionPointCS0 = btVector3(-ed->construction_info.chassis_box_half_extents[0] + (ed->construction_info.connectionStickLateralOutWheelWidthMultiplier * ed->construction_info.wheelWidth), ed->construction_info.connectionHeight, (ed->construction_info.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier * ed->construction_info.chassis_box_half_extents[2]) - ed->construction_info.wheelRadius);
			isFrontWheel = false;
			ed->m_vehicle->addWheel(connectionPointCS0, bt_wheelDirectionCS0, bt_wheelAxleCS, ed->construction_info.suspensionRestLength, ed->construction_info.wheelRadius, ed->m_tuning, isFrontWheel);

			connectionPointCS0 = btVector3(ed->construction_info.chassis_box_half_extents[0] - (ed->construction_info.connectionStickLateralOutWheelWidthMultiplier * ed->construction_info.wheelWidth), ed->construction_info.connectionHeight, (ed->construction_info.connectionStickFrontRearOutChassisBoxHalfExtentsZMultiplier * ed->construction_info.chassis_box_half_extents[2]) - ed->construction_info.wheelRadius);
			ed->m_vehicle->addWheel(connectionPointCS0, bt_wheelDirectionCS0, bt_wheelAxleCS, ed->construction_info.suspensionRestLength, ed->construction_info.wheelRadius, ed->m_tuning, isFrontWheel);

			for (int i = 0; i < ed->m_vehicle->getNumWheels(); i++)
			{
				btWheelInfo &wheel = ed->m_vehicle->getWheelInfo(i);
				wheel.m_suspensionStiffness = ed->construction_info.suspensionStiffness;
				wheel.m_wheelsDampingRelaxation = ed->construction_info.suspensionDamping;
				wheel.m_wheelsDampingCompression = ed->construction_info.suspensionCompression;
				wheel.m_frictionSlip = ed->construction_info.wheelFriction;
				wheel.m_rollInfluence = ed->construction_info.rollInfluence;
				wheel.m_maxSuspensionTravelCm = ed->construction_info.maxSuspensionTravelCm;
				wheel.m_maxSuspensionForce = ed->construction_info.maxSuspensionForce;
			}
		}
		if (w->phys_entities[i].active && w->phys_entities[i].body->isInWorld())
		{
			if (w->phys_entities[i].type == PHYSICS_SHAPE_VEHICLE1)
				w->dynamics_world->removeVehicle(((phys_edict_vehicle_extra_data_t *)w->phys_entities[i].extra_data)->m_vehicle);
			w->dynamics_world->removeRigidBody(w->phys_entities[i].body);
			w->phys_entities[i].relink = true;
		}
	}
	for (int i = 0; i < VOXEL_MAX_CHUNKS; i++)
	{
		if (w->phys_voxel_chunks[i].active)
		{
			unsigned int solid_group = COLLISION_CATEGORY_WORLD;
			unsigned int solid_mask = 0xFFFFFFFF - COLLISION_CATEGORY_WORLD;
			w->dynamics_world->removeRigidBody(w->phys_voxel_chunks[i].body);
			w->dynamics_world->addRigidBody(w->phys_voxel_chunks[i].body, solid_group, solid_mask);
			w->dynamics_world->updateSingleAabb(w->phys_voxel_chunks[i].body); /* for running dynamics world with setForceUpdateAllAabbs(false) */
		}
	}
}

/*
===================
Sys_SavePhysicsWorld

TODO: ENDIANNESS, ALIGNMENT, ARCHITECTURE/ABI PADDING, KEEP IN SYNC
TODO: USE HOST_FS* FOR FILE I/O
TODO: more properties we should save (friction, etc) I'M ALSO SURE I WILL FORGET SOMETHING WHEN I THINK THAT I'M DONE!
TODO: do not save the voxel physics data (they are static anyway), just recreate it! otherwise we will use too much memory and bullet will segfault
===================
*/
void Sys_SavePhysicsWorld(void *physworld, char *name)
{
	if (!physworld)
		return;
	physics_world_t *w = (physics_world_t *)physworld;

	char file1[FILENAME_MAX];
	char file2[FILENAME_MAX];
	Sys_PhysicsSaveFileNames(name, file1, file2);

	btDefaultSerializer *serializer = new btDefaultSerializer();
	w->dynamics_world->serialize(serializer);

	void *file = Host_FSFileHandleOpenBinaryWrite(file1);
	Host_FSFileHandleWriteBinary(file, serializer->getBufferPointer(), serializer->getCurrentBufferSize());
	Host_FSFileHandleClose(file);
	delete serializer;

	file = Host_FSFileHandleOpenBinaryWrite(file2);
	Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->server, sizeof(uint8_t));
	Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_time, sizeof(mstime_t));
	entindex_t count = 0;
	for (entindex_t i = 0; i < MAX_EDICTS; i++)
		if (w->phys_entities[i].active)
			count++;
	Host_FSFileHandleWriteBinary(file, (const unsigned char *)&count, sizeof(entindex_t));
	for (entindex_t i = 0; i < MAX_EDICTS; i++)
	{
		if (!w->phys_entities[i].active)
			continue;
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].index, sizeof(entindex_t));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].collision_margin, sizeof(btScalar));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].globalspace_rotation, sizeof(btQuaternion));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].jumped_this_frame, sizeof(int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].lastonground, sizeof(mstime_t));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].mass, sizeof(vec_t));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].onground, sizeof(int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].onground_normal, sizeof(btVector3));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].ground_entity, sizeof(int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].relink, sizeof(int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].sleep_counter, sizeof(double));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].sleep_state, sizeof(int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].solid, sizeof(unsigned int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].solid_group, sizeof(unsigned int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].solid_mask, sizeof(unsigned int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].trace_onground, sizeof(int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].type, sizeof(int));

		if (w->phys_entities[i].type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL || w->phys_entities[i].type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA)
		{
			if (!w->phys_entities[i].extra_data)
				Sys_Error("Heightfield for entity %d doesn't have extra data\n", i);
			model_heightfield_t *heightfield = (model_heightfield_t *)w->phys_entities[i].extra_data;

			/* save even the pointer to the data, just a little wasted space and is easier TODO FIXME */
			Host_FSFileHandleWriteBinary(file, (const unsigned char *)heightfield, sizeof(model_heightfield_t));
			Host_FSFileHandleWriteBinary(file, (const unsigned char *)heightfield->data, sizeof(vec_t) * heightfield->width * heightfield->length);
		}
		if (w->phys_entities[i].type == PHYSICS_SHAPE_VEHICLE1)
		{
			if (!w->phys_entities[i].extra_data)
				Sys_Error("Vehicle for entity %d doesn't have extra data\n", i);
			phys_edict_vehicle_extra_data_t *ed = (phys_edict_vehicle_extra_data_t *)w->phys_entities[i].extra_data;

			/* save even the pointer to the data, just a little wasted space and is easier TODO FIXME */
			Host_FSFileHandleWriteBinary(file, (const unsigned char *)ed, sizeof(phys_edict_vehicle_extra_data_t));
		}
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].radius, sizeof(btScalar));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].creation_vecs, sizeof(vec3_t));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].creation_model, sizeof(precacheindex_t));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_entities[i].cached_linearvelocity, sizeof(btVector3));
	}
	int voxelcount = 0;
	for (int i = 0; i < VOXEL_MAX_CHUNKS; i++)
		if (w->phys_voxel_chunks[i].active)
			voxelcount++;
	Host_FSFileHandleWriteBinary(file, (const unsigned char *)&voxelcount, sizeof(int));
	for (int i = 0; i < VOXEL_MAX_CHUNKS; i++)
	{
		if (!w->phys_voxel_chunks[i].active)
			continue;
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&i, sizeof(int));
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&w->phys_voxel_chunks[i].type, sizeof(int));
	}

	/* hope that the saved objects will have the SAME order in the loaded dynamics world! TODO FIXME */
	int colobjsize = w->dynamics_world->getCollisionObjectArray().size();
	Host_FSFileHandleWriteBinary(file, (const unsigned char *)&colobjsize, sizeof(int));
	for (int i = 0; i < colobjsize; i++)
	{
		btCollisionObject *obj = w->dynamics_world->getCollisionObjectArray()[i];
		int userindex = obj->getUserIndex();
		if (obj->getInternalType() & btCollisionObject::CO_GHOST_OBJECT)
		{
			 /* bullet doesn't serialize ghost objects  */
			continue;
		}

		if (userindex >=0)
		{
			if (w->phys_entities[userindex].type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_MODEL || w->phys_entities[userindex].type == PHYSICS_SHAPE_HEIGHTFIELD_FROM_DATA)
				continue; /* we know that heightfields are not saved */
		}

		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&userindex, sizeof(int));
		btTransform worldtransform = obj->getWorldTransform();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&worldtransform, sizeof(btTransform));
		btTransform motionstatetransform;
		((btRigidBody *)obj)->getMotionState()->getWorldTransform(motionstatetransform);
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&motionstatetransform, sizeof(btTransform));
		int collisionflags;
		collisionflags = obj->getCollisionFlags();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&collisionflags, sizeof(int));
		btVector3 avel, lvel;
		avel = ((btRigidBody *)obj)->getAngularVelocity();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&avel, sizeof(btVector3));
		lvel = ((btRigidBody *)obj)->getLinearVelocity();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&lvel, sizeof(btVector3));
		btScalar scalar;
		btVector3 vector3;
		scalar = ((btRigidBody *)obj)->getCcdMotionThreshold();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&scalar, sizeof(btScalar));
		scalar = ((btRigidBody *)obj)->getCcdSweptSphereRadius();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&scalar, sizeof(btScalar));
		scalar = ((btRigidBody *)obj)->getFriction();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&scalar, sizeof(btScalar));
		scalar = ((btRigidBody *)obj)->getRollingFriction();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&scalar, sizeof(btScalar));
		scalar = ((btRigidBody *)obj)->getRestitution();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&scalar, sizeof(btScalar));
		vector3 = ((btRigidBody *)obj)->getAnisotropicFriction();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&vector3, sizeof(btVector3));
		scalar = ((btRigidBody *)obj)->getContactProcessingThreshold();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&scalar, sizeof(btScalar));
		scalar = ((btRigidBody *)obj)->getLinearDamping();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&scalar, sizeof(btScalar));
		scalar = ((btRigidBody *)obj)->getAngularDamping();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&scalar, sizeof(btScalar));
		vector3 = ((btRigidBody *)obj)->getAngularFactor();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&vector3, sizeof(btVector3));
		vector3 = ((btRigidBody *)obj)->getInterpolationAngularVelocity();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&vector3, sizeof(btVector3));
		vector3 = ((btRigidBody *)obj)->getGravity();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&vector3, sizeof(btVector3));
		scalar = ((btRigidBody *)obj)->getCollisionShape()->getMargin();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&scalar, sizeof(btScalar));
		vector3 = ((btRigidBody *)obj)->getCollisionShape()->getLocalScaling();
		Host_FSFileHandleWriteBinary(file, (const unsigned char *)&vector3, sizeof(btVector3));
	}

	Host_FSFileHandleClose(file);
}

/*
===================
Sys_InitPhysics
===================
*/
void Sys_InitPhysics(void)
{
	phys_gravity_y = Host_CMDAddCvar("phys_gravity_y", "-9.81", 0);
	phys_maxspeed_linear = Host_CMDAddCvar("phys_maxspeed_linear", "0", 0);
	phys_maxspeed_angular = Host_CMDAddCvar("phys_maxspeed_angular", "0", 0);
	phys_debug_draw = Host_CMDAddCvar("phys_debug_draw", "0", 0); /* this is currently VERY slow for thousands of bodies */
	/* the server controls the turn rate, the client-side sensitivity should be adjusted by each axis of aimcmd[] (aimcmd[x] == 1 means the default turn rate) */
	phys_axis_pitch_rate = Host_CMDAddCvar("phys_axis_pitch_rate", "3.14", 0);
	phys_axis_yaw_rate = Host_CMDAddCvar("phys_axis_yaw_rate", "3.14", 0);
	phys_axis_roll_rate = Host_CMDAddCvar("phys_axis_roll_rate", "3.14", 0);

	gContactAddedCallback = Sys_PhysicsContactAddedCallback;
}

/*
===================
Sys_ShudownPhysics
===================
*/
void Sys_ShutdownPhysics(void)
{
}
