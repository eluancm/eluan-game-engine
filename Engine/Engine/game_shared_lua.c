/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

#include "lua/src/lua.h"
#include "lua/src/lauxlib.h"
#include "lua/src/lualib.h"

/*
============================================================================

Progs helper code

Functions for handling the VM

TODO: CHECK VALUES, SPECIALLY INDICES BECAUSE WE READ THEM AND BLINDLY ACCESS DATA WITH data[read_index]
TODO: stack not cleared correctly after erros. This is problematic with the client because it doesn't get reloaded
TODO: for client, support calling lua functions from the console (lua_exec cmd? parameters?)
TODO: reload server/client code on the fly? currently, only server code is reloaded and only when starting the server

============================================================================
*/

/*
===================
LoadProg

Adds a new prog file into the VM
===================
*/
void LoadProg(void *ls, const char *name, int abort_on_failure, mempool_t *mempool, char *id_for_mempool)
{
	char vm[MAX_GAME_STRING];
	char path[MAX_PATH];
	char *contents;
	int iStatus;
	lua_State *L = (lua_State *)ls;

	if (!strncmp(id_for_mempool, "svgameprog", MEMBLOCK_MAX_NAME))
		Sys_Snprintf(vm, MAX_GAME_STRING, "server");
	else if (!strncmp(id_for_mempool, "clgameprog", MEMBLOCK_MAX_NAME))
		Sys_Snprintf(vm, MAX_GAME_STRING, "client");
	else
		Sys_Error("LoadProg: Unknown Lua VM with mempool block name %s - please name it.\n", id_for_mempool);

	Sys_Snprintf(path, MAX_PATH, "progs/%s.lua", name);
	if (Host_FSLoadBinaryFile(path, mempool, id_for_mempool, &contents, true) == -1)
	{
		if (abort_on_failure)
		{
			Host_Error("LoadProg: %s couldn't load %s\n", vm, path);
		}
		else
		{
			Sys_Printf("LoadProg: %s couldn't load %s\n", vm, path);
			return;
		}
	}

	/* luaL_dostring instead of luaL_loadstring, so that only the contents of this file is ran for the initialization of functions, etc, instead of having to re-parse everything */
	iStatus = luaL_dostring(L, contents);
	if(iStatus)
		Host_Error("LoadProg: %s error: %s\n", vm, lua_tostring(L, -1));

	Sys_Printf("LoadProg: %s loaded %s\n", vm, path);
}

/*
===================
ProgsPrepareFunction

Prepares to run a function in the VM
===================
*/
static int traceback(lua_State *L)
{
	lua_getglobal(L, "debug");
	lua_getfield(L, -1, "traceback");
	lua_remove(L, -2); /* remove the "debug" */
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 2);
	lua_call(L, 2, 1);
	Sys_Printf("%s\n", lua_tostring(L, -1));
	return 1;
}
void ProgsPrepareFunction(void *ls, char *name, int abort_on_failure)
{
	lua_State *L = (lua_State *)ls;
	lua_pushcfunction(L, traceback);
	lua_getglobal(L, name);
	if (!lua_isfunction(L, -1))
	{
		if (abort_on_failure)
		{
			Host_Error("ProgsPrepareFunction: error: %s not found\n", name);
		}
		else
		{
			Sys_Printf("ProgsPrepareFunction: error: %s not found\n", name);
		}
		/* do not pop it: let the invalid function run and generate a traceback if abort_on_failure is false */
		/* lua_pop(L, 1); */
		return;
	}
}

/*
===================
ProgsInsert*IntoStack

Inserts arguments for the function to be called, use after ProgsPrepareFunction
===================
*/
void ProgsInsertIntegerIntoStack(void *ls, const int value)
{
	lua_State *L = (lua_State *)ls;
	lua_pushinteger(L, value);
}
void ProgsInsertInteger64IntoStack(void *ls, const int64_t value)
{
	lua_State *L = (lua_State *)ls;
	lua_pushinteger(L, value);
}
void ProgsInsertDoubleIntoStack(void *ls, const double value)
{
	lua_State *L = (lua_State *)ls;
	lua_pushnumber(L, value);
}
void ProgsInsertRealIntoStack(void *ls, const vec_t value)
{
	lua_State *L = (lua_State *)ls;
	lua_pushnumber(L, value);
}
void ProgsInsertVector2IntoStack(void *ls, const vec2_t value)
{
	lua_State *L = (lua_State *)ls;
	lua_newtable(L);
	lua_pushliteral(L, "x");
	lua_pushnumber(L, value[0]);
	lua_settable(L, -3);
	lua_pushliteral(L, "y");
	lua_pushnumber(L, value[1]);
	lua_settable(L, -3);
	lua_getglobal(L, "Vector2");
	lua_setmetatable(L, -2);
}
void ProgsInsertVector3IntoStack(void *ls, const vec3_t value)
{
	lua_State *L = (lua_State *)ls;
	lua_newtable(L);
	lua_pushliteral(L, "x");
	lua_pushnumber(L, value[0]);
	lua_settable(L, -3);
	lua_pushliteral(L, "y");
	lua_pushnumber(L, value[1]);
	lua_settable(L, -3);
	lua_pushliteral(L, "z");
	lua_pushnumber(L, value[2]);
	lua_settable(L, -3);
	lua_getglobal(L, "Vector3");
	lua_setmetatable(L, -2);
}
void ProgsInsertBooleanIntoStack(void *ls, const int value)
{
	lua_State *L = (lua_State *)ls;
	lua_pushboolean(L, value);
}
void ProgsInsertStringIntoStack(void *ls, const char *str)
{
	lua_State *L = (lua_State *)ls;
	lua_pushstring(L, str);
}
void ProgsInsertCPointerIntoStack(void *ls, void *ptr)
{
	lua_State *L = (lua_State *)ls;
	lua_pushlightuserdata(L, ptr);
}

/*
===================
ProgsRunFunction

Runs a previously prepared function in the VM
===================
*/
void ProgsRunFunction(void *ls, int args, int results, int abort_on_failure)
{
	lua_State *L = (lua_State *)ls;
	int iStatus;

	iStatus = lua_pcall(L, args, results, lua_gettop(L) - args - 1);

	if (iStatus)
	{
		if (abort_on_failure)
		{
			Host_Error("ProgsRunFunction: error: %s\n", lua_tostring(L, -1));
		}
		else
		{
			Sys_Printf("ProgsRunFunction: error: %s\n", lua_tostring(L, -1));
			return;
		}
	}
}

/*
===================
ProgsGet*FromStack

Useful for getting lua functions return values
0 is the top of the stack (points to empty new area)
-1 is the last position on the stack with a valid element
-2 is the second to last position, etc
When getting a pointer/reference in return, be sure to copy it if you want do keep it
Lua has 64-bit integers and double precision floating point numbers by default
===================
*/
int ProgsGetIntegerFromStack(void *ls, int idx)
{
	lua_State *L = (lua_State *)ls;
	return (int)luaL_checkinteger(L, idx);
}
int64_t ProgsGetInteger64FromStack(void *ls, int idx)
{
	lua_State *L = (lua_State *)ls;
	return luaL_checkinteger(L, idx);
}
double ProgsGetDoubleFromStack(void *ls, int idx)
{
	lua_State *L = (lua_State *)ls;
	return luaL_checknumber(L, idx);
}
vec_t ProgsGetRealFromStack(void *ls, int idx)
{
	lua_State *L = (lua_State *)ls;
	return (vec_t)luaL_checknumber(L, idx);
}
vec_t *ProgsGetVector2FromStack(void *ls, int idx, int unpack_idx_start, vec2_t out, int accept_nil)
{
	lua_State *L = (lua_State *)ls;

	if (lua_isnil(L, idx) && accept_nil)
		return NULL; /* in this case, "out" won't be modified */

	luaL_checktype(L, idx, LUA_TTABLE);
	/*
		Now to get the data out of the table:
		'unpack' the table by putting the values onto
		the stack first. Then convert those stack values
		into an appropriate C type.
	*/
	if (idx < 0)
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx - 1, "y");
	}
	else
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx, "y");
	}
	/*
		stack now has following:
		idx  = {x, y}
		unpack_idx_start - 1 = x
		unpack_idx_start     = y
	*/
	out[0] = (vec_t)luaL_checknumber(L, unpack_idx_start - 1);
	out[1] = (vec_t)luaL_checknumber(L, unpack_idx_start);
	/*
		we can all pop both off the stack because
		we got them by value, but if there was a string, etc
		we should keep it in the stack!
	*/
	lua_pop(L, 2);

	return out;
}
vec_t *ProgsGetVector3FromStack(void *ls, int idx, int unpack_idx_start, vec3_t out, int accept_nil)
{
	lua_State *L = (lua_State *)ls;

	if (lua_isnil(L, idx) && accept_nil)
		return NULL; /* in this case, "out" won't be modified */

	luaL_checktype(L, idx, LUA_TTABLE);
	/*
		Now to get the data out of the table:
		'unpack' the table by putting the values onto
		the stack first. Then convert those stack values
		into an appropriate C type.
	*/
	if (idx < 0)
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx - 1, "y");
		lua_getfield(L, idx - 2, "z");
	}
	else
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx, "y");
		lua_getfield(L, idx, "z");
	}
	/*
		stack now has following:
		idx  = {x, y, z}
		unpack_idx_start - 2 = x
		unpack_idx_start - 1 = y
		unpack_idx_start     = z
	*/
	out[0] = (vec_t)luaL_checknumber(L, unpack_idx_start - 2);
	out[1] = (vec_t)luaL_checknumber(L, unpack_idx_start - 1);
	out[2] = (vec_t)luaL_checknumber(L, unpack_idx_start);
	/*
		we can all pop all three off the stack because
		we got them by value, but if there was a string, etc
		we should keep it in the stack!
	*/
	lua_pop(L, 3);

	return out;
}
int *ProgsGetVector3IntegerFromStack(void *ls, int idx, int unpack_idx_start, int out[3], int accept_nil)
{
	lua_State *L = (lua_State *)ls;

	if (lua_isnil(L, idx) && accept_nil)
		return NULL; /* in this case, "out" won't be modified */

	luaL_checktype(L, idx, LUA_TTABLE);
	/*
		Now to get the data out of the table:
		'unpack' the table by putting the values onto
		the stack first. Then convert those stack values
		into an appropriate C type.
	*/
	if (idx < 0)
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx - 1, "y");
		lua_getfield(L, idx - 2, "z");
	}
	else
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx, "y");
		lua_getfield(L, idx, "z");
	}
	/*
		stack now has following:
		idx  = {x, y, z}
		unpack_idx_start - 2 = x
		unpack_idx_start - 1 = y
		unpack_idx_start     = z
	*/
	out[0] = (int)luaL_checkinteger(L, unpack_idx_start - 2);
	out[1] = (int)luaL_checkinteger(L, unpack_idx_start - 1);
	out[2] = (int)luaL_checkinteger(L, unpack_idx_start);
	/*
		we can all pop all three off the stack because
		we got them by value, but if there was a string, etc
		we should keep it in the stack!
	*/
	lua_pop(L, 3);

	return out;
}
vec_t *ProgsGetVector4FromStack(void *ls, int idx, int unpack_idx_start, vec4_t out, int accept_nil)
{
	lua_State *L = (lua_State *)ls;

	if (lua_isnil(L, idx) && accept_nil)
		return NULL; /* in this case, "out" won't be modified */

	luaL_checktype(L, idx, LUA_TTABLE);
	/*
		Now to get the data out of the table:
		'unpack' the table by putting the values onto
		the stack first. Then convert those stack values
		into an appropriate C type.
	*/
	if (idx < 0)
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx - 1, "y");
		lua_getfield(L, idx - 2, "z");
		lua_getfield(L, idx - 3, "w");
	}
	else
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx, "y");
		lua_getfield(L, idx, "z");
		lua_getfield(L, idx, "w");
	}
	/*
		stack now has following:
		idx  = {x, y, z, w}
		unpack_idx_start - 3 = x
		unpack_idx_start - 2 = y
		unpack_idx_start - 1 = z
		unpack_idx_start     = w
	*/
	out[0] = (vec_t)luaL_checknumber(L, unpack_idx_start - 3);
	out[1] = (vec_t)luaL_checknumber(L, unpack_idx_start - 2);
	out[2] = (vec_t)luaL_checknumber(L, unpack_idx_start - 1);
	out[3] = (vec_t)luaL_checknumber(L, unpack_idx_start);
	/*
		we can all pop all three off the stack because
		we got them by value, but if there was a string, etc
		we should keep it in the stack!
	*/
	lua_pop(L, 4);

	return out;
}
int *ProgsGetVector4IntegerFromStack(void *ls, int idx, int unpack_idx_start, int out[4], int accept_nil)
{
	lua_State *L = (lua_State *)ls;

	if (lua_isnil(L, idx) && accept_nil)
		return NULL; /* in this case, "out" won't be modified */

	luaL_checktype(L, idx, LUA_TTABLE);
	/*
		Now to get the data out of the table:
		'unpack' the table by putting the values onto
		the stack first. Then convert those stack values
		into an appropriate C type.
	*/
	if (idx < 0)
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx - 1, "y");
		lua_getfield(L, idx - 2, "z");
		lua_getfield(L, idx - 3, "w");
	}
	else
	{
		lua_getfield(L, idx, "x");
		lua_getfield(L, idx, "y");
		lua_getfield(L, idx, "z");
		lua_getfield(L, idx, "w");
	}
	/*
		stack now has following:
		idx  = {x, y, z, w}
		unpack_idx_start - 3 = x
		unpack_idx_start - 2 = y
		unpack_idx_start - 1 = z
		unpack_idx_start     = w
	*/
	out[0] = (int)luaL_checkinteger(L, unpack_idx_start - 3);
	out[1] = (int)luaL_checkinteger(L, unpack_idx_start - 2);
	out[2] = (int)luaL_checkinteger(L, unpack_idx_start - 1);
	out[3] = (int)luaL_checkinteger(L, unpack_idx_start);
	/*
		we can all pop all three off the stack because
		we got them by value, but if there was a string, etc
		we should keep it in the stack!
	*/
	lua_pop(L, 4);

	return out;
}
int ProgsGetBooleanFromStack(void *ls, int idx)
{
	lua_State *L = (lua_State *)ls;
	return lua_toboolean(L, idx);
}
const char *ProgsGetStringFromStack(void *ls, int idx, int accept_nil)
{
	lua_State *L = (lua_State *)ls;

	if (lua_isnil(L, idx) && accept_nil)
		return NULL;

	return luaL_checkstring(L, idx);
}
void *ProgsGetCPointerFromStack(void *ls, int idx)
{
	lua_State *L = (lua_State *)ls;
	return lua_touserdata(L, idx);
}

/*
===================
ProgsFinishFunction

Just for cleaning up the stack
===================
*/
void stackdump_g(lua_State *L)
{
	int i;
	int top = lua_gettop(L);

	Sys_Printf("total in stack %d\n", top);

	for (i = 1; i <= top; i++)
	{
		/* repeat for each level */
		int t = lua_type(L, i);
		switch (t)
		{
			case LUA_TSTRING:  /* strings */
				Sys_Printf("string: '%s'\n", lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:  /* booleans */
				Sys_Printf("boolean %s\n", lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNUMBER:  /* numbers */
				Sys_Printf("number: %g\n", lua_tonumber(L, i));
				break;
			default:  /* other values */
				Sys_Printf("%s\n", lua_typename(L, t));
				break;
		}
		printf("  ");  /* put a separator */
	}
	printf("\n");  /* end the listing */
}
void ProgsFinishFunction(void *ls, int num_return_args)
{
	lua_State *L = (lua_State *)ls;

	/* the input arguments were cosumed, so we just clean the return args and the traceback function */
	lua_pop(L, 1 + num_return_args);

	/* TODO FIXME: this won't work with nested calls (like C->LUA->C->LUA, the last LUA finishing will obviously have the first's values into the stack
	if (lua_gettop(L) != 0)
	{
		stackdump_g(L);
		Host_Error("Lua stack leak detected!\n");
	}
	*/
}

/*
============================================================================

Progs built-in functions

Functions called from the VM for speed or scope reasons

============================================================================
*/

/*
===================
PR_LoadProg

Wrapper to LoadProg
input: char *name
output: none
===================
*/
extern lua_State *sv_lua_state;
extern lua_State *cl_lua_state;
int PR_LoadProg(void *ls)
{
	char error[MAX_GAME_STRING];
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *name;

	if (ls == sv_lua_state && svs.listening)
	{
		Sys_Snprintf(error, MAX_GAME_STRING, "PR_LoadProg: server already listening and tried to load \"%s\"", ProgsGetStringFromStack(L, 1, false));
		ProgsInsertStringIntoStack(L, error);
		lua_error(L);
		return 0;
	}

	if (ls == cl_lua_state && cls.ingame)
	{
		Sys_Snprintf(error, MAX_GAME_STRING, "PR_LoadProg: client already in game and tried to load \"%s\"", ProgsGetStringFromStack(L, 1, false));
		ProgsInsertStringIntoStack(L, error);
		lua_error(L);
		return 0;
	}

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_LoadProg: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	name = ProgsGetStringFromStack(L, 1, false);

	if (ls == sv_lua_state)
		LoadProg(L, name, false, &svr_mem, "svgameprog");
	else if (ls == cl_lua_state)
		LoadProg(L, name, false, &svr_mem, "clgameprog");
	else
		LoadProg(L, name, false, &svr_mem, "unkowngameprog");

	return 0; /* number of return values in the stack */
}

/*
===================
PR_PrintC

Wrapper to Sys_Printf
input: char *msg
output: none
===================
*/
int PR_PrintC(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *msg;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_PrintC: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetStringFromStack(L, 1, false);

	Sys_Printf("%s", msg);

	return 0; /* number of return values in the stack */
}

/*
===================
PR_LocalCmd

Wrapper to Host_CMDBufferAdd
input: char *cmd
output: none
===================
*/
int PR_LocalCmd(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *cmd;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_LocalCmd: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	cmd = ProgsGetStringFromStack(L, 1, false);

	Host_CMDBufferAdd(cmd);

	return 0; /* number of return values in the stack */
}

/*
===================
PR_LocalCmdForce

Wrapper to Host_CMDBufferAdd, but clearing the buffer of previous cmds and forcing the immediate execution of the new one(s)
input: char *cmd
output: none
===================
*/
int PR_LocalCmdForce(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *cmd;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_LocalCmdForce: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	cmd = ProgsGetStringFromStack(L, 1, false);

	Host_CMDBufferClear();
	Host_CMDBufferAdd(cmd);
	Host_CMDBufferExecute(); /* TODO: is this safe? */

	return 0; /* number of return values in the stack */
}

/*
===================
PR_CvarGet*

Wrappers to pass cvar values to the progs
input: char *cvarname
output: {int, vec_t, char *} value
===================
*/
int PR_CvarGetInteger(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *cvarname;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CvarGetInteger: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	cvarname = ProgsGetStringFromStack(L, 1, false);

	ProgsInsertIntegerIntoStack(L, (int)(Host_CMDGetCvar(cvarname, true)->doublevalue));
	return 1; /* number of return values in the stack */
}
int PR_CvarGetReal(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *cvarname;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CvarGetReal: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	cvarname = ProgsGetStringFromStack(L, 1, false);

	ProgsInsertDoubleIntoStack(L, Host_CMDGetCvar(cvarname, true)->doublevalue);
	return 1; /* number of return values in the stack */
}
int PR_CvarGetBoolean(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *cvarname;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CvarGetBoolean: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	cvarname = ProgsGetStringFromStack(L, 1, false);

	ProgsInsertBooleanIntoStack(L, (int)Host_CMDGetCvar(cvarname, true)->doublevalue);
	return 1; /* number of return values in the stack */
}
int PR_CvarGetString(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *cvarname;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CvarGetString: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}
	cvarname = ProgsGetStringFromStack(L, 1, false);

	ProgsInsertStringIntoStack(L, (Host_CMDGetCvar(cvarname, true)->charvalue));
	return 1; /* number of return values in the stack */
}

/*
===================
PR_MessageRead*

Various wrappers to read messages from data passed by the engine
input: message data
output: message read
===================
*/
int PR_MessageReadEntity(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	entindex_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadEntity: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadEntity(msg, read, len, &value);

	ProgsInsertIntegerIntoStack(L, value);

	return 1;
}
int PR_MessageReadPrecache(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	precacheindex_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadPrecache: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadPrecache(msg, read, len, &value);

	ProgsInsertIntegerIntoStack(L, value);

	return 1;
}
int PR_MessageReadTime(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	mstime_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadTime: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadTime(msg, read, len, &value);

	ProgsInsertDoubleIntoStack(L, value);

	return 1;
}
int PR_MessageReadByte(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	unsigned char value;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadByte: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadByte(msg, read, len, &value);

	ProgsInsertIntegerIntoStack(L, value);

	return 1;
}
int PR_MessageReadShort(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	short value;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadShort: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadShort(msg, read, len, &value);

	ProgsInsertIntegerIntoStack(L, value);

	return 1;
}
int PR_MessageReadInt(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	int value;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadInt: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadInt(msg, read, len, &value);

	ProgsInsertIntegerIntoStack(L, value);

	return 1;
}
int PR_MessageReadDouble(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	double value;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadDouble: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadDouble(msg, read, len, &value);

	ProgsInsertDoubleIntoStack(L, value);

	return 1;
}
int PR_MessageReadVec1(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	vec_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadVec1: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadVec1(msg, read, len, &value);

	ProgsInsertRealIntoStack(L, value);

	return 1;
}
int PR_MessageReadVec3(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	vec3_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadVec3: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadVec3(msg, read, len, value);

	ProgsInsertVector3IntoStack(L, value);

	return 1;
}
int PR_MessageReadString(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	char *msg;
	int *read;
	int len;

	char value[MAX_GAME_STRING];

	nArgs = lua_gettop(L);
	if (nArgs != 3)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageReadString: invalid number of arguments, should be 3");
		lua_error(L);
		return 0;
	}

	msg = ProgsGetCPointerFromStack(L, 1);
	read = ProgsGetCPointerFromStack(L, 2);
	len = ProgsGetIntegerFromStack(L, 3);

	MSG_ReadString(msg, read, len, value, MAX_GAME_STRING);

	ProgsInsertStringIntoStack(L, value);

	return 1;
}


/*
===================
ProgsRegisterShared

Registers the various PR_* builtins shared by client and server
===================
*/
void ProgsRegisterShared(void *ls)
{
	lua_State *L = (lua_State *)ls;

	lua_register(L, "LoadProg", PR_LoadProg);
	lua_register(L, "PrintC", PR_PrintC);
	lua_register(L, "LocalCmd", PR_LocalCmd);
	lua_register(L, "LocalCmdForce", PR_LocalCmdForce);
	lua_register(L, "CvarGetInteger", PR_CvarGetInteger);
	lua_register(L, "CvarGetReal", PR_CvarGetReal);
	lua_register(L, "CvarGetBoolean", PR_CvarGetBoolean);
	lua_register(L, "CvarGetString", PR_CvarGetString);
	lua_register(L, "MessageReadEntity", PR_MessageReadEntity);
	lua_register(L, "MessageReadPrecache", PR_MessageReadPrecache);
	lua_register(L, "MessageReadTime", PR_MessageReadTime);
	lua_register(L, "MessageReadByte", PR_MessageReadByte);
	lua_register(L, "MessageReadShort", PR_MessageReadShort);
	lua_register(L, "MessageReadInt", PR_MessageReadInt);
	lua_register(L, "MessageReadDouble", PR_MessageReadDouble);
	lua_register(L, "MessageReadVec1", PR_MessageReadVec1);
	lua_register(L, "MessageReadVec3", PR_MessageReadVec3);
	lua_register(L, "MessageReadString", PR_MessageReadString);
}

