/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef ENGINE_H
#define ENGINE_H

/* #define DEDICATED_SERVER */

/* standard headers */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* custom headers */

#ifdef __cplusplus
/* be careful when using this, it's easy to mess things up (specially with externs inside functions) */
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include "system.h"
#include "host.h"
#include "server.h"
#include "client.h"
#include "game_shared.h"
#include "game_sv.h"
#include "game_cl.h"

#endif /* ENGINE_H */
