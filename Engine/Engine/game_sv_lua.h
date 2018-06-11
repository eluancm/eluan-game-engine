/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef GAME_SV_LUA_H
#define GAME_SV_LUA_H

/* game_edict_t.visible VALUES */ /* TODO: use this for notarget, etc? */
#define VISIBLE_NO				0 /* do not send this entity through the network */
#define VISIBLE_TEST			1 /* test for visibility using various methods */
#define VISIBLE_ALWAYS			2 /* always send this entity */
#define VISIBLE_ALWAYS_OWNER	3 /* always send this entity, but only to the direct owner and nobody else */
#define VISIBLE_NEVER_OWNER		4 /* test for everybody, but never send to the client which is viewing the world through the owner of this entity */

#define MAX_GAME_ENTITY_CLUSTERS	32 /* for visibility testing, how many clusters of the tested model (likely the world model) the entity can touch concurrently */

typedef struct game_edict_s
{
	/* used only for caching */
	int					trace_numhits;
	entindex_t			trace_ent[MAX_GAME_TRACEHITS]; /* entity hit */
	vec3_t				trace_pos[MAX_GAME_TRACEHITS]; /* point of contact, global coordinates */
	vec3_t				trace_normal[MAX_GAME_TRACEHITS]; /* the normal of the face which the ray contacted */
	vec_t				trace_fraction[MAX_GAME_TRACEHITS]; /* the distance from the original position to the contact point */

	/* for server-side PVS calculation */
	vec3_t				clusters_myoldorigin; /* for validating the cache */
	vec3_t				clusters_myoldangles; /* for validating the cache */
	int					clusters[MAX_GAME_ENTITY_CLUSTERS]; /* where the entity is, when comparing it to a world model which has PVS */
	int					clusters_num; /* how many are in the list */
	int					clusters_valid; /* if the cache is valid */
} game_edict_t;

/* inside a struct for loading and saving games */
typedef struct game_state_s
{
	game_edict_t	entities[MAX_EDICTS];

	entindex_t		worldent;
	precacheindex_t	worldmodelindex;
	vec3_t			worldorigin;

	precacheindex_t	model_sounds[MAX_PRECACHES][NUM_MODEL_SOUNDS];
	int				model_sounds_loaded[MAX_PRECACHES][NUM_MODEL_SOUNDS];
	int				model_sounds_fileloaded[MAX_PRECACHES];
} game_state_t;

/* game_sv_main.c */
extern game_state_t gs;

#endif /* GAME_SV_LUA_H */

