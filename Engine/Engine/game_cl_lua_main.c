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

/* TODO: not thread safe? */
lua_State *cl_lua_state = NULL;

/*
============================================================================

Progs built-in functions

Functions called from the VM for speed or scope reasons

============================================================================
*/

/*
===================
PR_ShutdownServer

Wrapper to SV_ShutdownServer
input: none
output: none
===================
*/
int PR_ShutdownServer(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_ShutdownServer: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	SV_ShutdownServer(false);

	return 0;
}

/*
===================
PR_DisconnectClient

Wrapper to CL_Disconnect
input: none
output: none
===================
*/
int PR_DisconnectClient(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_DisconnectClient: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	CL_Disconnect(false, true);

	return 0;
}

/*
===================
PR_VideoForceRefresh

To redraw the screen, in case something needs to be drawn and the refresh will take too long because of loading, etc
input: none
output: none
===================
*/
int PR_VideoForceRefresh(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_VideoForceRefresh: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	CL_VideoFrame();

	return 0;
}

/*
===================
PR_CL_MenuOpen

Wrapper to Game_CL_MenuOpen
input: none
output: none
===================
*/
int PR_CL_MenuOpen(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_CL_MenuOpen: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	Game_CL_MenuOpen();

	return 0;
}

/*
===================
PR_CL_MenuClose

Wrapper to Game_CL_MenuClose
input: none
output: none
===================
*/
int PR_CL_MenuClose(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_CL_MenuClose: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	Game_CL_MenuClose();

	return 0;
}

/*
===================
PR_CL_MenuTypeInGame

Wrapper to Game_CL_MenuTypeInGame
input: none
output: boolean menutypeingame
===================
*/
int PR_CL_MenuTypeInGame(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_CL_MenuTypeInGame: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	ProgsInsertBooleanIntoStack(L, Game_CL_MenuTypeInGame());

	return 1;
}

/*
===================
PR_CL_DialogEditText

Used as a clicking callback function
input: int slot
output: none
===================
*/
static char saved_text[MAX_GAME_STRING]; /* to hold the text being edited, for cancelling changes TODO FIXME not reentrant (splitscreen, etc) */
static char editing_text[MAX_GAME_STRING]; /* to hold the text being edited TODO FIXME not reentrant (splitscreen, etc) */
static int editing_text_slot; /* to hold the dialog slot of the text being edited TODO FIXME not reentrant (splitscreen, etc) */
static lua_State *editing_luastate;
void CL_DialogEditTextEvent(char *text, int text_modified, int confirm, int cancel, char *input_utf8, int key_index)
{
	/*
		Callback to be used when editing a text
		TODO: when finishing, update the width and height of the component! (always use automatic when creating?)
	*/
	if (confirm)
		CL_InputSetTextMode(false, NULL, 0, NULL);
	else if (cancel)
	{
		Sys_Snprintf(text, MAX_GAME_STRING, "%s", saved_text);
		CL_InputSetTextMode(false, NULL, 0, NULL);
	}

	lua_getglobal(editing_luastate, "dialog");
	lua_pushliteral(editing_luastate, "components");
	lua_gettable(editing_luastate, -2);
	lua_pushinteger(editing_luastate, editing_text_slot);
	lua_gettable(editing_luastate, -2);
	lua_pushstring(editing_luastate, text);
	lua_setfield(editing_luastate, -2, "text");
	lua_pop(editing_luastate, 3);
}
int PR_CL_DialogEditText(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int slot;

	const char *initialtext;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CL_DialogEditText: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	slot = ProgsGetIntegerFromStack(L, 1);

	lua_getglobal(L, "dialog");
	lua_pushliteral(L, "components");
	lua_gettable(L, -2);
	lua_pushinteger(L, slot);
	lua_gettable(L, -2);
	lua_pushstring(L, "text");
	lua_gettable(L, -2);

	initialtext = ProgsGetStringFromStack(L, -1, false);

	lua_pop(L, 4);

	Sys_Snprintf(saved_text, MAX_GAME_STRING, "%s", initialtext);
	Sys_Snprintf(editing_text, MAX_GAME_STRING, "%s", initialtext);
	editing_text_slot = slot;
	editing_luastate = L;
	CL_InputSetTextMode(true, editing_text, MAX_GAME_STRING, &CL_DialogEditTextEvent);

	return 0;
}

/*
===================
PR_ParticleAlloc

Creates a new particle with the given data
input: particle data
output: boolean success
===================
*/
int PR_ParticleAlloc(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	particle_t *newpart;

	int success;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_ParticleAlloc: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	newpart = CL_ParticleAlloc();
	if (newpart)
	{
		int color[4]; /* because of type mismatches */
		success = true;

		luaL_checktype(L, 1, LUA_TTABLE);
		lua_getfield(L, 1, "org");
		lua_getfield(L, 1, "vel");
		lua_getfield(L, 1, "color");
		lua_getfield(L, 1, "scale");
		lua_getfield(L, 1, "timelimit");

		ProgsGetVector3FromStack(L, -5, -1, newpart->org, false);
		ProgsGetVector3FromStack(L, -4, -1, newpart->vel, false);
		ProgsGetVector4IntegerFromStack(L, -3, -1, color, false);
		Math_Vector4Copy(color, newpart->color); /* because of unsigned char vs int converstion - just to be safe */
		newpart->scale = ProgsGetRealFromStack(L, -2);
		newpart->timelimit = ProgsGetDoubleFromStack(L, -1);

		lua_pop(L, 5);
	}
	else
	{
		success = false;
	}

	ProgsInsertBooleanIntoStack(L, success);
	return 1;
}

/*
===================
PR_InputReset

Wrapper to Game_CL_InputReset
input: none
output: none
===================
*/
int PR_InputReset(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_InputReset: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	Game_CL_InputReset();

	return 0;
}

/*
===================
PR_InputResetTriggersAndImpulse

Wrapper to Game_CL_InputResetTriggersAndImpulse
input: none
output: none
===================
*/
int PR_InputResetTriggersAndImpulse(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_InputResetTriggersAndImpulse: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	Game_CL_InputResetTriggersAndImpulse();

	return 0;
}

/*
===================
PR_InputGetState

Wrapper to Game_CL_InputGetState
input: none
output: table with client_game_input_state_t
===================
*/
int PR_InputGetState(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	const client_game_input_state_t *cur_data = Game_CL_InputGetState();
	const mstime_t frametime = Game_CL_InputGetFrameTime();
	const unsigned short seq = Game_CL_InputGetSequence();

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_InputGetState: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	lua_newtable(L);

	lua_pushliteral(L, "in_mov");
	ProgsInsertVector3IntoStack(L, cur_data->in_mov);
	lua_settable(L, -3);

	lua_pushliteral(L, "in_aim");
	ProgsInsertVector3IntoStack(L, cur_data->in_aim);
	lua_settable(L, -3);

	lua_pushliteral(L, "in_buttons");
	ProgsInsertIntegerIntoStack(L, cur_data->in_buttons);
	lua_settable(L, -3);

	lua_pushliteral(L, "in_triggerbuttons");
	ProgsInsertIntegerIntoStack(L, cur_data->in_triggerbuttons);
	lua_settable(L, -3);

	lua_pushliteral(L, "impulse");
	ProgsInsertIntegerIntoStack(L, cur_data->impulse);
	lua_settable(L, -3);

	lua_pushliteral(L, "sbar_showscores");
	ProgsInsertBooleanIntoStack(L, cur_data->sbar_showscores);
	lua_settable(L, -3);

	/* put extra data in this table */
	lua_pushliteral(L, "frametime");
	ProgsInsertDoubleIntoStack(L, frametime);
	lua_settable(L, -3);

	lua_pushliteral(L, "seq");
	ProgsInsertIntegerIntoStack(L, seq);
	lua_settable(L, -3);

	return 1;
}

/*
===================
PR_InputGetKeyDest

Wrapper to get the keydest variable
input: none
output: int keydest
===================
*/
int PR_InputGetKeyDest(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_InputGetKeyDest: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	ProgsInsertIntegerIntoStack(L, keydest);

	return 1;
}

/*
===================
PR_InputBindFromKey

Wrapper to CL_InputBindFromKey
input: int keyindex
output: char *bind
===================
*/
int PR_InputBindFromKey(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int keyindex;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_InputBindFromKey: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	keyindex = ProgsGetIntegerFromStack(L, 1);

	ProgsInsertStringIntoStack(L, CL_InputBindFromKey(keyindex));

	return 1;
}

/*
===================
PR_CLMessageWrite*

Various wrappers to send messages to the server
Unreliable or reliable
Buffer is cleared after the messages are sent
input: none OR message value
output: none
===================
*/
/* TODO: does not allow threading */
char game_cl_msg[MAX_NET_CMDSIZE];
int game_cl_msglen = 0;
int PR_MessageSendToServerUnreliable(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageSendToServerUnreliable: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	if (cls.connected)
		Host_NetchanQueueCommand(cls.netconn, game_cl_msg, game_cl_msglen, NET_CMD_UNRELIABLE);
	game_cl_msglen = 0;

	return 0;
}
int PR_MessageSendToServerReliable(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	nArgs = lua_gettop(L);
	if (nArgs != 0)
	{
		ProgsInsertStringIntoStack(L, "PR_MessageSendToServerReliable: invalid number of arguments, should be 0");
		lua_error(L);
		return 0;
	}

	if (cls.connected)
		Host_NetchanQueueCommand(cls.netconn, game_cl_msg, game_cl_msglen, NET_CMD_RELIABLE);
	game_cl_msglen = 0;

	return 0;
}
int PR_CLMessageWriteEntity(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	entindex_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWriteEntity: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (entindex_t)ProgsGetIntegerFromStack(L, 1);

	MSG_WriteEntity(game_cl_msg, &game_cl_msglen, value);

	return 0;
}
int PR_CLMessageWritePrecache(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	precacheindex_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWritePrecache: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (precacheindex_t)ProgsGetIntegerFromStack(L, 1);

	MSG_WritePrecache(game_cl_msg, &game_cl_msglen, value);

	return 0;
}
int PR_CLMessageWriteTime(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	mstime_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWriteTime: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = ProgsGetDoubleFromStack(L, 1);

	MSG_WriteTime(game_cl_msg, &game_cl_msglen, value);

	return 0;
}
int PR_CLMessageWriteByte(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	unsigned char value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWriteByte: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (unsigned char)ProgsGetIntegerFromStack(L, 1);

	MSG_WriteByte(game_cl_msg, &game_cl_msglen, value);

	return 0;
}
int PR_CLMessageWriteShort(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	short value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWriteShort: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (short)ProgsGetIntegerFromStack(L, 1);

	MSG_WriteShort(game_cl_msg, &game_cl_msglen, value);

	return 0;
}
int PR_CLMessageWriteInt(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	int value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWriteInt: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = ProgsGetIntegerFromStack(L, 1);

	MSG_WriteInt(game_cl_msg, &game_cl_msglen, value);

	return 0;
}
int PR_CLMessageWriteDouble(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	double value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWriteDouble: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = ProgsGetDoubleFromStack(L, 1);

	MSG_WriteDouble(game_cl_msg, &game_cl_msglen, value);

	return 0;
}
int PR_CLMessageWriteVec1(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	vec_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWriteVec1: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = (vec_t)ProgsGetRealFromStack(L, 1);

	MSG_WriteVec1(game_cl_msg, &game_cl_msglen, value);

	return 0;
}
int PR_CLMessageWriteVec3(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	vec3_t value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWriteVec3: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	ProgsGetVector3FromStack(L, 1, -1, value, false);

	MSG_WriteVec3(game_cl_msg, &game_cl_msglen, value);

	return 0;
}
int PR_CLMessageWriteString(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;
	const char *value;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_CLMessageWriteString: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	value = ProgsGetStringFromStack(L, 1, false);

	MSG_WriteString(game_cl_msg, &game_cl_msglen, value);

	return 0;
}

/*
===================
PR_LoadSound

Wrapper to CL_LoadSound
input: char *name, int local
output: table with sfx_t
===================
*/
int PR_LoadSound(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	const char *name;
	int local;

	sfx_t *newsound;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_LoadSound: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}

	name = ProgsGetStringFromStack(L, 1, false);
	local = ProgsGetBooleanFromStack(L, 2);

	newsound = CL_LoadSound(name, local);

	lua_newtable(L);

	lua_pushliteral(L, "name");
	ProgsInsertStringIntoStack(L, newsound->name);
	lua_settable(L, -3);

	lua_pushliteral(L, "data");
	ProgsInsertIntegerIntoStack(L, newsound->data);
	lua_settable(L, -3);

	return 1;
}

/*
===================
PR_StartLocalSound

Wrapper to CL_StartLocalSound
input: char *name, int local
output: none
===================
*/
int PR_StartLocalSound(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	const char *name;

	sfx_t playsound;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_StartLocalSound: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "name");
	lua_getfield(L, 1, "data");

	name = ProgsGetStringFromStack(L, -2, false);
	Sys_Snprintf(playsound.name, sizeof(playsound.name), "%s", name); /* TODO: this is a time waster - do only in debug builds? */
	playsound.data = ProgsGetIntegerFromStack(L, -1);

	lua_pop(L, 2);

	playsound.active = true;

	CL_StartLocalSound(&playsound);

	return 0;
}

/*
===================
PR_LoadTexture

Wrapper to CL_LoadTexture
TODO: game-created procedural textures (unsigned char *indata, int inwidth, int inheight) (shouldn't just pass as string because \0 will end it in c?)
input: char *name, int keep, int data_has_mipmaps, int mipmapuntilwidth, int mipmapuntilheight
output: table with texture_t
===================
*/
int PR_LoadTexture(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	const char *name;
	int keep, data_has_mipmaps;
	int mipmapuntilwidth, mipmapuntilheight;

	texture_t *newtexture;

	nArgs = lua_gettop(L);
	if (nArgs != 5)
	{
		ProgsInsertStringIntoStack(L, "PR_LoadTexture: invalid number of arguments, should be 5");
		lua_error(L);
		return 0;
	}

	name = ProgsGetStringFromStack(L, 1, false);
	keep = ProgsGetBooleanFromStack(L, 2);
	data_has_mipmaps = ProgsGetBooleanFromStack(L, 3);
	mipmapuntilwidth = ProgsGetIntegerFromStack(L, 4);
	mipmapuntilheight = ProgsGetIntegerFromStack(L, 5);

	newtexture = CL_LoadTexture(name, keep, NULL, 0, 0, data_has_mipmaps, mipmapuntilwidth, mipmapuntilheight);

	lua_newtable(L);

	lua_pushliteral(L, "id");
	ProgsInsertIntegerIntoStack(L, newtexture->id);
	lua_settable(L, -3);

	lua_pushliteral(L, "cl_id");
	ProgsInsertIntegerIntoStack(L, newtexture->cl_id);
	lua_settable(L, -3);

	lua_pushliteral(L, "name");
	ProgsInsertStringIntoStack(L, newtexture->name);
	lua_settable(L, -3);

	lua_pushliteral(L, "keep");
	ProgsInsertBooleanIntoStack(L, newtexture->keep);
	lua_settable(L, -3);

	lua_pushliteral(L, "width");
	ProgsInsertIntegerIntoStack(L, newtexture->width);
	lua_settable(L, -3);

	lua_pushliteral(L, "height");
	ProgsInsertIntegerIntoStack(L, newtexture->height);
	lua_settable(L, -3);

	return 1;
}

/*
===================
PR_Video2DAbsoluteFromUnitX

Wrapper to Sys_Video2DAbsoluteFromUnitX
input: vec_t unitcoord
output: vec_t basesizecoord
===================
*/
int PR_Video2DAbsoluteFromUnitX(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	vec_t unitcoord;

	vec_t basesizecoord;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_Video2DAbsoluteFromUnitX: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	unitcoord = ProgsGetRealFromStack(L, 1);

	Sys_Video2DAbsoluteFromUnitX(unitcoord, &basesizecoord);

	ProgsInsertRealIntoStack(L, basesizecoord);

	return 1;
}

/*
===================
PR_Video2DAbsoluteFromUnitY

Wrapper to Sys_Video2DAbsoluteFromUnitY
input: vec_t unitcoord
output: vec_t basesizecoord
===================
*/
int PR_Video2DAbsoluteFromUnitY(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	vec_t unitcoord;

	vec_t basesizecoord;

	nArgs = lua_gettop(L);
	if (nArgs != 1)
	{
		ProgsInsertStringIntoStack(L, "PR_Video2DAbsoluteFromUnitY: invalid number of arguments, should be 1");
		lua_error(L);
		return 0;
	}

	unitcoord = ProgsGetRealFromStack(L, 1);

	Sys_Video2DAbsoluteFromUnitY(unitcoord, &basesizecoord);

	ProgsInsertRealIntoStack(L, basesizecoord);

	return 1;
}

/*
===================
PR_VideoDraw2DFill

Wrapper to Sys_VideoDraw2DFill
input: int width, int height, int x, int y, vec4_t color
output: none
===================
*/
int PR_VideoDraw2DFill(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int width, height, x, y;
	vec4_t color;

	nArgs = lua_gettop(L);
	if (nArgs != 5)
	{
		ProgsInsertStringIntoStack(L, "PR_VideoDraw2DFill: invalid number of arguments, should be 5");
		lua_error(L);
		return 0;
	}

	width = (int)ProgsGetRealFromStack(L, 1);
	height = (int)ProgsGetRealFromStack(L, 2);
	x = (int)ProgsGetRealFromStack(L, 3);
	y = (int)ProgsGetRealFromStack(L, 4);
	ProgsGetVector4FromStack(L, 5, -1, color, false);

	/* TODO FIXME: some of these get repacked inside the function... see in the C code if it's better to keep everything packed */
	Sys_VideoDraw2DFill(width, height, x, y, color[0], color[1], color[2], color[3]);

	return 0;
}

/*
===================
PR_VideoDraw2DPic

Wrapper to Sys_VideoDraw2DPic
input: int id, int width, int height, int x, int y
output: none
===================
*/
int PR_VideoDraw2DPic(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	int id, width, height, x, y;

	nArgs = lua_gettop(L);
	if (nArgs != 5)
	{
		ProgsInsertStringIntoStack(L, "PR_VideoDraw2DPic: invalid number of arguments, should be 5");
		lua_error(L);
		return 0;
	}

	id = ProgsGetIntegerFromStack(L, 1);
	width = (int)ProgsGetRealFromStack(L, 2);
	height = (int)ProgsGetRealFromStack(L, 3);
	x = (int)ProgsGetRealFromStack(L, 4);
	y = (int)ProgsGetRealFromStack(L, 5);

	Sys_VideoDraw2DPic(&id, width, height, x, y);

	return 0;
}

/*
===================
PR_VideoDraw2DText

Wrapper to Sys_VideoDraw2DText
input: char *text, vec2_t pos, vec2_t scale, vec4_t color
output: none
===================
*/
int PR_VideoDraw2DText(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	const char *text;
	vec2_t pos, scale;
	vec4_t color;

	nArgs = lua_gettop(L);
	if (nArgs != 4)
	{
		ProgsInsertStringIntoStack(L, "PR_VideoDraw2DText: invalid number of arguments, should be 4");
		lua_error(L);
		return 0;
	}

	text = ProgsGetStringFromStack(L, 1, false);
	ProgsGetVector2FromStack(L, 2, -1, pos, false);
	ProgsGetVector2FromStack(L, 3, -1, scale, false);
	ProgsGetVector4FromStack(L, 4, -1, color, false);

	/* TODO FIXME: some of these get repacked inside the function... see in the C code if it's better to keep everything packed */
	Sys_VideoDraw2DText(text, pos[0], pos[1], scale[0], scale[1], color[0], color[1], color[2], color[3]);

	return 0;
}

/*
===================
PR_FontGetStringWidth

Wrapper to Sys_FontGetStringWidth
input: char *text, vec_t scalex
output: vec_t width
===================
*/
int PR_FontGetStringWidth(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	const char *text;
	vec_t scalex;

	vec_t width;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_FontGetStringWidth: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}

	text =  ProgsGetStringFromStack(L, 1, false);
	scalex = ProgsGetRealFromStack(L, 2);

	width = Sys_FontGetStringWidth(text, scalex);

	ProgsInsertRealIntoStack(L, width);

	return 1;
}

/*
===================
PR_FontGetStringHeight

Wrapper to Sys_FontGetStringheight
input: char *text, vec_t scaley
output: vec_t height
===================
*/
int PR_FontGetStringHeight(void *ls)
{
	lua_State *L = (lua_State *)ls;
	int nArgs;

	const char *text;
	vec_t scaley;

	vec_t height;

	nArgs = lua_gettop(L);
	if (nArgs != 2)
	{
		ProgsInsertStringIntoStack(L, "PR_FontGetStringHeight: invalid number of arguments, should be 2");
		lua_error(L);
		return 0;
	}

	text =  ProgsGetStringFromStack(L, 1, false);
	scaley = ProgsGetRealFromStack(L, 2);

	height = Sys_FontGetStringHeight(text, scaley);

	ProgsInsertRealIntoStack(L, height);

	return 1;
}

/*
============================================================================

Game-specific particle handling

============================================================================
*/

/*
===================
Game_CL_StartParticle

Should only be called by CL_StartParticle
===================
*/
void Game_CL_StartParticle(char *msg, int *read, int len)
{
	ProgsPrepareFunction(cl_lua_state, "StartParticle", true);
	ProgsInsertCPointerIntoStack(cl_lua_state, msg);
	ProgsInsertCPointerIntoStack(cl_lua_state, read);
	ProgsInsertIntegerIntoStack(cl_lua_state, len);
	ProgsRunFunction(cl_lua_state, 3, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

/*
============================================================================

Game client general code

============================================================================
*/

/*
===================
Game_CL_Newgame

Called when connecting to a server
===================
*/
void Game_CL_Newgame(void)
{
	ProgsPrepareFunction(cl_lua_state, "NewGame", true);
	ProgsRunFunction(cl_lua_state, 0, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

/*
===================
Game_CL_Frame

Called every frame when connected
===================
*/
void Game_CL_Frame(void)
{
	ProgsPrepareFunction(cl_lua_state, "Frame", true);
	ProgsInsertDoubleIntoStack(cl_lua_state, host_frametime);
	ProgsInsertIntegerIntoStack(cl_lua_state, (int)cls.stall_frames);
	ProgsInsertIntegerIntoStack(cl_lua_state, cls.snapshots[cls.current_snapshot_idx].my_ent);
	ProgsInsertIntegerIntoStack(cl_lua_state, cls.signon);
	ProgsInsertStringIntoStack(cl_lua_state, cls.remote_host);
	ProgsInsertBooleanIntoStack(cl_lua_state, cls.snapshots[cls.current_snapshot_idx].paused);
	ProgsRunFunction(cl_lua_state, 6, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

/*
===================
Game_CL_ParseServerMessages

Called to process extra state from the server
===================
*/
int Game_CL_ParseServerMessages(unsigned cmd, char *msg, int *read, int len)
{
	int result;
	ProgsPrepareFunction(cl_lua_state, "ParseServerMessages", true);
	ProgsInsertIntegerIntoStack(cl_lua_state, cmd);
	ProgsInsertCPointerIntoStack(cl_lua_state, msg);
	ProgsInsertCPointerIntoStack(cl_lua_state, read);
	ProgsInsertIntegerIntoStack(cl_lua_state, len);
	ProgsRunFunction(cl_lua_state, 4, 1, true);
	result = ProgsGetBooleanFromStack(cl_lua_state, -1);
	ProgsFinishFunction(cl_lua_state, 1);
	return result;
}

/*
===================
Game_CL_SendServerMessages

Called to send game-specific commands
===================
*/
void Game_CL_SendServerMessages(void)
{
	ProgsPrepareFunction(cl_lua_state, "SendServerMessages", true);
	ProgsRunFunction(cl_lua_state, 0, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

/*
============================================================================

General menu / user interface

============================================================================
*/

/*
===================
MenuInit
===================
*/
void MenuInit(void)
{
	if (cl_lua_state)
		Host_Error("MenuInit: client game already started!\n");
	cl_lua_state = luaL_newstate();
    luaL_openlibs(cl_lua_state);

	ProgsRegisterShared(cl_lua_state);

	lua_register(cl_lua_state, "ShutdownServer", PR_ShutdownServer);
	lua_register(cl_lua_state, "DisconnectClient", PR_DisconnectClient);
	lua_register(cl_lua_state, "VideoForceRefresh", PR_VideoForceRefresh);
	lua_register(cl_lua_state, "CL_MenuOpen", PR_CL_MenuOpen);
	lua_register(cl_lua_state, "CL_MenuClose", PR_CL_MenuClose);
	lua_register(cl_lua_state, "CL_MenuTypeInGame", PR_CL_MenuTypeInGame);
	lua_register(cl_lua_state, "CL_DialogEditText", PR_CL_DialogEditText);
	lua_register(cl_lua_state, "ParticleAlloc", PR_ParticleAlloc);
	lua_register(cl_lua_state, "InputReset", PR_InputReset);
	lua_register(cl_lua_state, "InputResetTriggersAndImpulse", PR_InputResetTriggersAndImpulse);
	lua_register(cl_lua_state, "InputGetState", PR_InputGetState);
	lua_register(cl_lua_state, "InputGetKeyDest", PR_InputGetKeyDest);
	lua_register(cl_lua_state, "InputBindFromKey", PR_InputBindFromKey);
	lua_register(cl_lua_state, "MessageSendToServerUnreliable", PR_MessageSendToServerUnreliable);
	lua_register(cl_lua_state, "MessageSendToServerReliable", PR_MessageSendToServerReliable);
	lua_register(cl_lua_state, "CLMessageWriteEntity", PR_CLMessageWriteEntity);
	lua_register(cl_lua_state, "CLMessageWritePrecache", PR_CLMessageWritePrecache);
	lua_register(cl_lua_state, "CLMessageWriteTime", PR_CLMessageWriteTime);
	lua_register(cl_lua_state, "CLMessageWriteByte", PR_CLMessageWriteByte);
	lua_register(cl_lua_state, "CLMessageWriteShort", PR_CLMessageWriteShort);
	lua_register(cl_lua_state, "CLMessageWriteInt", PR_CLMessageWriteInt);
	lua_register(cl_lua_state, "CLMessageWriteDouble", PR_CLMessageWriteDouble);
	lua_register(cl_lua_state, "CLMessageWriteVec1", PR_CLMessageWriteVec1);
	lua_register(cl_lua_state, "CLMessageWriteVec3", PR_CLMessageWriteVec3);
	lua_register(cl_lua_state, "CLMessageWriteString", PR_CLMessageWriteString);
	lua_register(cl_lua_state, "LoadSound", PR_LoadSound);
	lua_register(cl_lua_state, "StartLocalSound", PR_StartLocalSound);
	lua_register(cl_lua_state, "LoadTexture", PR_LoadTexture);
	lua_register(cl_lua_state, "Video2DAbsoluteFromUnitX", PR_Video2DAbsoluteFromUnitX);
	lua_register(cl_lua_state, "Video2DAbsoluteFromUnitY", PR_Video2DAbsoluteFromUnitY);
	lua_register(cl_lua_state, "VideoDraw2DFill", PR_VideoDraw2DFill);
	lua_register(cl_lua_state, "VideoDraw2DPic", PR_VideoDraw2DPic);
	lua_register(cl_lua_state, "VideoDraw2DText", PR_VideoDraw2DText);
	lua_register(cl_lua_state, "FontGetStringWidth", PR_FontGetStringWidth);
	lua_register(cl_lua_state, "FontGetStringHeight", PR_FontGetStringHeight);

#define lua_setConstReal(name) { lua_pushnumber(cl_lua_state, name ); lua_setglobal(cl_lua_state, #name ); }
#define lua_setConstInteger(name) { lua_pushinteger(cl_lua_state, name ); lua_setglobal(cl_lua_state, #name ); }
	lua_setConstInteger(KEY_INVALID)
	lua_setConstInteger(KEY_BACKSPACE)
	lua_setConstInteger(KEY_TAB)
	lua_setConstInteger(KEY_RETURN)
	lua_setConstInteger(KEY_ESC)
	lua_setConstInteger(KEY_SPACE)
	lua_setConstInteger(KEY_QUOTE)
	lua_setConstInteger(KEY_0)
	lua_setConstInteger(KEY_1)
	lua_setConstInteger(KEY_2)
	lua_setConstInteger(KEY_3)
	lua_setConstInteger(KEY_4)
	lua_setConstInteger(KEY_5)
	lua_setConstInteger(KEY_6)
	lua_setConstInteger(KEY_7)
	lua_setConstInteger(KEY_8)
	lua_setConstInteger(KEY_9)
	lua_setConstInteger(KEY_BACKQUOTE)
	lua_setConstInteger(KEY_A)
	lua_setConstInteger(KEY_B)
	lua_setConstInteger(KEY_C)
	lua_setConstInteger(KEY_D)
	lua_setConstInteger(KEY_E)
	lua_setConstInteger(KEY_F)
	lua_setConstInteger(KEY_G)
	lua_setConstInteger(KEY_H)
	lua_setConstInteger(KEY_I)
	lua_setConstInteger(KEY_J)
	lua_setConstInteger(KEY_K)
	lua_setConstInteger(KEY_L)
	lua_setConstInteger(KEY_M)
	lua_setConstInteger(KEY_N)
	lua_setConstInteger(KEY_O)
	lua_setConstInteger(KEY_P)
	lua_setConstInteger(KEY_Q)
	lua_setConstInteger(KEY_R)
	lua_setConstInteger(KEY_S)
	lua_setConstInteger(KEY_T)
	lua_setConstInteger(KEY_U)
	lua_setConstInteger(KEY_V)
	lua_setConstInteger(KEY_W)
	lua_setConstInteger(KEY_X)
	lua_setConstInteger(KEY_Y)
	lua_setConstInteger(KEY_Z)
	lua_setConstInteger(KEY_INSERT)
	lua_setConstInteger(KEY_DELETE)
	lua_setConstInteger(KEY_HOME)
	lua_setConstInteger(KEY_END)
	lua_setConstInteger(KEY_PAGEUP)
	lua_setConstInteger(KEY_PAGEDOWN)
	lua_setConstInteger(KEY_UP)
	lua_setConstInteger(KEY_DOWN)
	lua_setConstInteger(KEY_RIGHT)
	lua_setConstInteger(KEY_LEFT)
	lua_setConstInteger(KEY_F1)
	lua_setConstInteger(KEY_F2)
	lua_setConstInteger(KEY_F3)
	lua_setConstInteger(KEY_F4)
	lua_setConstInteger(KEY_F5)
	lua_setConstInteger(KEY_F6)
	lua_setConstInteger(KEY_F7)
	lua_setConstInteger(KEY_F8)
	lua_setConstInteger(KEY_F9)
	lua_setConstInteger(KEY_F10)
	lua_setConstInteger(KEY_F11)
	lua_setConstInteger(KEY_F12)
	lua_setConstInteger(KEY_LCONTROL)
	lua_setConstInteger(KEY_LSHIFT)
	lua_setConstInteger(KEY_LALT)
	lua_setConstInteger(KEY_RCONTROL)
	lua_setConstInteger(KEY_RSHIFT)
	lua_setConstInteger(KEY_RALT)
	lua_setConstInteger(KEY_PAUSE)
	lua_setConstInteger(MOUSE0_BUTTON4)
	lua_setConstInteger(MOUSE0_BUTTON3)
	lua_setConstInteger(MOUSE0_WHEELRIGHT)
	lua_setConstInteger(MOUSE0_WHEELLEFT)
	lua_setConstInteger(MOUSE0_WHEELDOWN)
	lua_setConstInteger(MOUSE0_WHEELUP)
	lua_setConstInteger(MOUSE0_BUTTON2)
	lua_setConstInteger(MOUSE0_BUTTON1)
	lua_setConstInteger(MOUSE0_BUTTON0)
	lua_setConstInteger(MOUSE0_HORIZONTAL)
	lua_setConstInteger(MOUSE0_VERTICAL)
	lua_setConstInteger(BASE_WIDTH)
	lua_setConstInteger(BASE_HEIGHT)

	lua_setConstInteger(MAX_CLIENTS)

	lua_setConstInteger(KEYDEST_INVALID)
	lua_setConstInteger(KEYDEST_GAME)
	lua_setConstInteger(KEYDEST_MENU)
	lua_setConstInteger(KEYDEST_TEXT)

	LoadProg(cl_lua_state, "cl_main", true, &std_mem, "clgameprog");

	ProgsPrepareFunction(cl_lua_state, "MenuInit", true);
	ProgsRunFunction(cl_lua_state, 0, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

/*
===================
MenuShutdown
===================
*/
void MenuShutdown(void)
{
	ProgsPrepareFunction(cl_lua_state, "MenuShutdown", true);
	ProgsRunFunction(cl_lua_state, 0, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);

	if (cl_lua_state)
	{
		lua_close(cl_lua_state);
		cl_lua_state = NULL;
	}
}

/*
===================
MenuDraw
===================
*/
void MenuDraw(void)
{
	ProgsPrepareFunction(cl_lua_state, "MenuDraw", true);
	ProgsRunFunction(cl_lua_state, 0, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);

	/* TODO CONSOLEDEBUG: do this in a better way, like a debug buffer
	{
	char text[MAX_GAME_STRING];
	extern int numvisiblefaces;
	Sys_Snprintf(text, MAX_GAME_STRING, "Faces desenhadas: %d", (int)numvisiblefaces);
	Sys_VideoDraw2DText(text, 10, 10, 0.5f, 0.5f, 1, 1, 1, 1);
	}*/
}

/*
===================
MenuDrawPriority
===================
*/
void MenuDrawPriority(void)
{
	ProgsPrepareFunction(cl_lua_state, "MenuDrawPriority", true);
	ProgsRunFunction(cl_lua_state, 0, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

/*
===================
MenuKey
===================
*/
void MenuKey(int keyindex, int down, vec_t analog_rel, vec_t analog_abs)
{
	ProgsPrepareFunction(cl_lua_state, "MenuKey", true);
	ProgsInsertIntegerIntoStack(cl_lua_state, keyindex);
	ProgsInsertIntegerIntoStack(cl_lua_state, down);
	ProgsInsertRealIntoStack(cl_lua_state, analog_rel);
	ProgsInsertRealIntoStack(cl_lua_state, analog_abs);
	ProgsRunFunction(cl_lua_state, 4, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

/*
===================
MenuFrame
===================
*/
void MenuFrame(void)
{
	ProgsPrepareFunction(cl_lua_state, "MenuFrame", true);
	ProgsInsertDoubleIntoStack(cl_lua_state, host_frametime);
	ProgsInsertDoubleIntoStack(cl_lua_state, host_realtime);
	ProgsInsertBooleanIntoStack(cl_lua_state, cls.ingame);
	ProgsInsertBooleanIntoStack(cl_lua_state, cls.connected);
	ProgsInsertBooleanIntoStack(cl_lua_state, cls.snapshots[cls.current_snapshot_idx].paused);
	ProgsInsertBooleanIntoStack(cl_lua_state, svs.loading);
	ProgsInsertBooleanIntoStack(cl_lua_state, svs.listening);
	ProgsRunFunction(cl_lua_state, 7, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

/*
===================
MenuClose
===================
*/
void MenuClose(void)
{
	if (cl_lua_state)
	{
		ProgsPrepareFunction(cl_lua_state, "MenuClose", true);
		ProgsRunFunction(cl_lua_state, 0, 0, true);
		ProgsFinishFunction(cl_lua_state, 0);
	}
}

/*
===================
MenuOpen
===================
*/
void MenuOpen(int show_splash)
{
	if (cl_lua_state)
	{
		ProgsPrepareFunction(cl_lua_state, "MenuOpen", true);
		ProgsInsertBooleanIntoStack(cl_lua_state, show_splash);
		ProgsRunFunction(cl_lua_state, 1, 0, true);
		ProgsFinishFunction(cl_lua_state, 0);
	}
}

/*
===================
MenuIsLoadingServer
===================
*/
const int MenuIsLoadingServer(void)
{
	int result;

	ProgsPrepareFunction(cl_lua_state, "MenuIsLoadingServer", true);
	ProgsRunFunction(cl_lua_state, 0, 1, true);
	result = ProgsGetBooleanFromStack(cl_lua_state, -1);
	ProgsFinishFunction(cl_lua_state, 1);

	return result;
}

/*
===================
MenuLoadingServerGetTimeout
===================
*/
const mstime_t MenuLoadingServerGetTimeout(void)
{
	mstime_t result;

	ProgsPrepareFunction(cl_lua_state, "MenuLoadingServerGetTimeout", true);
	ProgsRunFunction(cl_lua_state, 0, 1, true);
	result = ProgsGetDoubleFromStack(cl_lua_state, -1);
	ProgsFinishFunction(cl_lua_state, 1);

	return result;
}

/*
===================
MenuFinishLoadingServer
===================
*/
void MenuFinishLoadingServer(const int error_happened)
{
	ProgsPrepareFunction(cl_lua_state, "MenuFinishLoadingServer", true);
	ProgsInsertBooleanIntoStack(cl_lua_state, error_happened);
	ProgsRunFunction(cl_lua_state, 1, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

/*
===================
MenuIsLoadingClient
===================
*/
const int MenuIsLoadingClient(void)
{
	int result;

	ProgsPrepareFunction(cl_lua_state, "MenuIsLoadingClient", true);
	ProgsRunFunction(cl_lua_state, 0, 1, true);
	result = ProgsGetBooleanFromStack(cl_lua_state, -1);
	ProgsFinishFunction(cl_lua_state, 1);

	return result;
}

/*
===================
MenuLoadingClientGetTimeout
===================
*/
const mstime_t MenuLoadingClientGetTimeout(void)
{
	mstime_t result;

	ProgsPrepareFunction(cl_lua_state, "MenuLoadingClientGetTimeout", true);
	ProgsRunFunction(cl_lua_state, 0, 1, true);
	result = ProgsGetDoubleFromStack(cl_lua_state, -1);
	ProgsFinishFunction(cl_lua_state, 1);

	return result;
}

/*
===================
MenuFinishLoadingClient
===================
*/
void MenuFinishLoadingClient(void)
{
	ProgsPrepareFunction(cl_lua_state, "MenuFinishLoadingClient", true);
	ProgsRunFunction(cl_lua_state, 0, 0, true);
	ProgsFinishFunction(cl_lua_state, 0);
}

