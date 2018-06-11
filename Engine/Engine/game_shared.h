/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef GAME_SHARED_H
#define GAME_SHARED_H

/* Units for physics considerations: meters, kilograms, seconds. (or milliseconds if dealing with mstime_t variables!) */

#define MAX_GAME_STRING			256 /* please use this for strings in-game as much as possible */

/* game specific defines start */

enum { /* should be kept in sync with the definition of animation_names TODO: document which models need which animations */
	ANIMATION_BASE = 0,
	ANIMATION_IDLE = 1,
	ANIMATION_FIRE,
	ANIMATION_FIRE2,
	ANIMATION_FIRE3,
	ANIMATION_FIRE4,
	ANIMATION_FIRE5,
	ANIMATION_FIRE6,
	ANIMATION_FIRE7,
	ANIMATION_FIRE8,
	ANIMATION_FIREEMPTY,
	ANIMATION_RELOAD,
	ANIMATION_WEAPONACTIVATE,
	ANIMATION_WEAPONDEACTIVATE,
	ANIMATION_RUN,
	ANIMATION_RUNRIGHT,
	ANIMATION_RUNLEFT,
	NUM_ANIMATIONS					/* should be >= 1 */
};
EXTERNC const char *animation_names[NUM_ANIMATIONS];
#define ANIMATION_BASE_FRAME				0 /* this frame should have info for getting default AABB, etc... It will also NOT be blended unless it's in the FIRST of the ANIMATION_SLOT_*, works as "animation slot disabled", so ANIMATION_BASE should include it for disabling animation slots. Having it blended in the first slot is useful for models with only the base frame */

enum { /* should be kept in sync with the definition of animation_slot_names and ANIMATION_MAX_BLENDED_FRAMES TODO: document which models need which slots defined */
	ANIMATION_SLOT_ALLJOINTS = 0,	/* entire model, still user-definable because there may be some special joints */
	ANIMATION_SLOT_ARMS = 1,		/* taunt, fire, etc */
	ANIMATION_SLOT_LEGS,			/* walk, run, jump, etc */
	ANIMATION_SLOT_PELVIS,			/* walk direction */
	ANIMATION_SLOT_TORSO,			/* pitch */
};
EXTERNC const char *animation_slot_names[ANIMATION_MAX_BLENDED_FRAMES];

enum { /* should be kept in sync with the definition of model_tag_names TODO: document which models need which animations */
	MODEL_TAG_RIGHTHAND = 0,
    MODEL_TAG_LEFTHAND = 1,
    NUM_MODEL_TAGS					/* should be >= 1? TODO */
};
EXTERNC const char *model_tag_names[NUM_MODEL_TAGS];

/* game specific defines end */

#include "game_shared_lua.h"

#endif /* GAME_SHARED_H */
