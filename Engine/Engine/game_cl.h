/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#ifndef GAME_CL_H
#define GAME_CL_H

#include "game_cl_lua.h"

/* game specific defines start */

typedef struct client_game_input_state_s
{
	/* stuff sent to the server */
	/* move direction */
	vec3_t			in_mov;
	/* aim direction (vector, not angles) */
	vec3_t			in_aim;
	/* general buttons */
	unsigned char	in_buttons; /* bitfield sent every frame */
	unsigned char	in_triggerbuttons; /* bitfield sent only when pressing */
	unsigned char	impulse; /* value sent every frame then reset */

	/* local stuff */
	int				sbar_showscores;

} client_game_input_state_t;

/* game specific defines end */

/* game_cl_menu.c */
void MenuInit(void);
void MenuShutdown(void);
void MenuDraw(void);
void MenuDrawPriority(void);
void MenuKey(int keyindex, int down, vec_t analog_rel, vec_t analog_abs);
void MenuFrame(void);
void MenuClose(void);
void MenuOpen(int show_splash);
const int MenuIsLoadingServer(void);
const mstime_t MenuLoadingServerGetTimeout(void);
void MenuFinishLoadingServer(const int error_happened);
const int MenuIsLoadingClient(void);
const mstime_t MenuLoadingClientGetTimeout(void);
void MenuFinishLoadingClient(void);

void Game_CL_MenuClose(void);
void Game_CL_MenuOpen(void);
const int Game_CL_MenuTypeInGame(void);

/* game_cl_input.c */
void Game_CL_InputReset(void);
void Game_CL_InputResetTriggersAndImpulse(void);
void Game_CL_InputSaveState(void);
const client_game_input_state_t *Game_CL_InputGetState(void);
const mstime_t Game_CL_InputGetFrameTime(void);
const unsigned short Game_CL_InputGetSequence(void);
const int Game_CL_InputSetCurrentBySequence(unsigned short seq);
void Game_CL_InputSetNext(void);

/* Below here only functions called by engine code */

/* game_cl_menu.c */
void Game_CL_MenuInit(void);
void Game_CL_MenuShutdown(void);
void Game_CL_MenuDraw(void);
void Game_CL_MenuKey(int keyindex, int down, vec_t analog_rel, vec_t analog_abs);
void Game_CL_MenuFrame(void);
void Game_CL_MenuConsolePrint(char *text);

/* game_cl_input.c */
void Game_CL_InputInit(void);
void Game_CL_InputShutdown(void);

/* game_cl_main.c */
void Game_CL_Newgame(void);
void Game_CL_Frame(void);
int Game_CL_ParseServerMessages(unsigned cmd, char *msg, int *read, int len);
void Game_CL_SendServerMessages(void);

/* game_cl_particles.c */
void Game_CL_StartParticle(char *msg, int *read, int len);

#endif /* GAME_CL_H */
