/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef GAME_SHARED_LUA_H
#define GAME_SHARED_LUA_H

void LoadProg(void *ls, const char *name, int abort_on_failure, mempool_t *mempool, char *id_for_mempool);

void ProgsPrepareFunction(void *ls, char *name, int abort_on_failure);

void ProgsInsertIntegerIntoStack(void *ls, const int value);
void ProgsInsertInteger64IntoStack(void *ls, const int64_t value);
void ProgsInsertDoubleIntoStack(void *ls, const double value);
void ProgsInsertRealIntoStack(void *ls, const vec_t value);
void ProgsInsertVector2IntoStack(void *ls, const vec2_t value);
void ProgsInsertVector3IntoStack(void *ls, const vec3_t value);
void ProgsInsertBooleanIntoStack(void *ls, const int value);
void ProgsInsertStringIntoStack(void *ls, const char *str);
void ProgsInsertCPointerIntoStack(void *ls, void *ptr);

void ProgsRunFunction(void *ls, int args, int results, int abort_on_failure);

int ProgsGetIntegerFromStack(void *ls, int idx);
int64_t ProgsGetInteger64FromStack(void *ls, int idx);
double ProgsGetDoubleFromStack(void *ls, int idx);
vec_t ProgsGetRealFromStack(void *ls, int idx);
vec_t *ProgsGetVector2FromStack(void *ls, int idx, int unpack_idx_start, vec2_t out, int accept_nil);
vec_t *ProgsGetVector3FromStack(void *ls, int idx, int unpack_idx_start, vec3_t out, int accept_nil);
int *ProgsGetVector3IntegerFromStack(void *ls, int idx, int unpack_idx_start, int out[3], int accept_nil);
vec_t *ProgsGetVector4FromStack(void *ls, int idx, int unpack_idx_start, vec4_t out, int accept_nil);
int *ProgsGetVector4IntegerFromStack(void *ls, int idx, int unpack_idx_start, int out[4], int accept_nil);
int ProgsGetBooleanFromStack(void *ls, int idx);
const char *ProgsGetStringFromStack(void *ls, int idx, int accept_nil);
void *ProgsGetCPointerFromStack(void *ls, int idx);

void ProgsFinishFunction(void *ls, int num_return_args);

void ProgsRegisterShared(void *ls);

#endif /* GAME_SHARED_LUA_H */

