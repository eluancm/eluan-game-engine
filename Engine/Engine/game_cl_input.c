/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Game specific input mapping and command generation to the server

============================================================================
*/

static client_game_input_state_t clgis;


#define NEXT_SAVED_COMMAND ((saved_command_current + 1) & MAX_SAVED_SNAPSHOTS_MASK)

typedef struct saved_commands_s {
	client_game_input_state_t	cmds;
	unsigned short				seq; /* 0 == invalid */
	mstime_t					frametime; /* TODO: check this, sync with server, see if using the right frametime, see if working properly, sanitize (server doesn't trust client), etc */
} saved_commands_t;
static saved_commands_t saved_commands[MAX_SAVED_SNAPSHOTS];
static int saved_command_current;

static cvar_t *in_invertpitch = NULL;
static cvar_t *in_sensitivity_digital_pitch = NULL, *in_sensitivity_digital_yaw = NULL, *in_sensitivity_digital_roll = NULL;
static cvar_t *in_sensitivity_analog_pitch = NULL, *in_sensitivity_analog_yaw = NULL, *in_sensitivity_analog_roll = NULL;
/* these macros simplify the code */
#define PITCH(a)		(in_invertpitch->doublevalue ? -(a) : (a))
#define DIGITAL_PITCH_SENS(a)	((vec_t)(a) * (vec_t)in_sensitivity_digital_pitch->doublevalue)
#define DIGITAL_YAW_SENS(a)		((vec_t)(a) * (vec_t)in_sensitivity_digital_yaw->doublevalue)
#define DIGITAL_ROLL_SENS(a)	((vec_t)(a) * (vec_t)in_sensitivity_digital_roll->doublevalue)
#define ANALOG_PITCH_SENS(a)	((vec_t)(a) * (vec_t)in_sensitivity_analog_pitch->doublevalue)
#define ANALOG_YAW_SENS(a)		((vec_t)(a) * (vec_t)in_sensitivity_analog_yaw->doublevalue)
#define ANALOG_ROLL_SENS(a)		((vec_t)(a) * (vec_t)in_sensitivity_analog_roll->doublevalue)
static int in_moveleft, in_moveright, in_moveup, in_movedown, in_moveforward, in_movebackward;
static int in_aimup, in_aimdown, in_aimleft, in_aimright, in_aimrollleft, in_aimrollright;

/*
===================
Game_CL_CMD_Move*Press
===================
*/
void Game_CL_CMD_MoveLeftPress(void)		{ in_moveleft     = true; clgis.in_mov[0] = -1; }
void Game_CL_CMD_MoveRightPress(void)		{ in_moveright    = true; clgis.in_mov[0] =  1; }
void Game_CL_CMD_MoveUpPress(void)			{ in_moveup       = true; clgis.in_mov[1] =  1; }
void Game_CL_CMD_MoveDownPress(void)		{ in_movedown     = true; clgis.in_mov[1] = -1; }
void Game_CL_CMD_MoveForwardPress(void)		{ in_moveforward  = true; clgis.in_mov[2] = -1; }
void Game_CL_CMD_MoveBackwardPress(void)	{ in_movebackward = true; clgis.in_mov[2] =  1; }

/*
===================
Game_CL_CMD_Move*Unpress
===================
*/
void Game_CL_CMD_MoveLeftUnpress(void)		{ in_moveleft     = false; if (in_moveright)    clgis.in_mov[0] =  1; else clgis.in_mov[0] = 0; }
void Game_CL_CMD_MoveRightUnpress(void)		{ in_moveright    = false; if (in_moveleft)     clgis.in_mov[0] = -1; else clgis.in_mov[0] = 0; }
void Game_CL_CMD_MoveUpUnpress(void)		{ in_moveup       = false; if (in_movedown)     clgis.in_mov[1] = -1; else clgis.in_mov[1] = 0; }
void Game_CL_CMD_MoveDownUnpress(void)		{ in_movedown     = false; if (in_moveup)       clgis.in_mov[1] =  1; else clgis.in_mov[1] = 0; }
void Game_CL_CMD_MoveForwardUnpress(void)	{ in_moveforward  = false; if (in_movebackward) clgis.in_mov[2] =  1; else clgis.in_mov[2] = 0; }
void Game_CL_CMD_MoveBackwardUnpress(void)	{ in_movebackward = false; if (in_moveforward)  clgis.in_mov[2] = -1; else clgis.in_mov[2] = 0; }

/*
===================
Game_CL_CMD_MoveAnalog*
===================
*/
void Game_CL_CMD_MoveAnalogSidestep(void)
{
	vec_t rel;

	if (host_cmd_argc != 3)
	{
		Sys_Printf("Wrong number of arguments for Game_CL_CMD_MoveAnalogSidestep\n");
		return;
	}

	rel = (vec_t)atof(host_cmd_argv[1]);
	clgis.in_mov[0] = rel;
}
void Game_CL_CMD_MoveAnalogUp(void)
{
	vec_t rel;

	if (host_cmd_argc != 3)
	{
		Sys_Printf("Wrong number of arguments for Game_CL_CMD_MoveAnalogUp\n");
		return;
	}

	rel = (vec_t)atof(host_cmd_argv[1]);
	clgis.in_mov[1] = rel;
}
void Game_CL_CMD_MoveAnalogForward(void)
{
	vec_t rel;

	if (host_cmd_argc != 3)
	{
		Sys_Printf("Wrong number of arguments for Game_CL_CMD_MoveAnalogForward\n");
		return;
	}

	rel = (vec_t)atof(host_cmd_argv[1]);
	clgis.in_mov[2] = rel;
}

/*
===================
Game_CL_CMD_Aim*Press
===================
*/
void Game_CL_CMD_AimUpPress(void)		{ in_aimup        = true; clgis.in_aim[ANGLES_PITCH]  = DIGITAL_PITCH_SENS(PITCH( 1)); }
void Game_CL_CMD_AimDownPress(void)		{ in_aimdown      = true; clgis.in_aim[ANGLES_PITCH]  = DIGITAL_PITCH_SENS(PITCH(-1)); }
void Game_CL_CMD_AimLeftPress(void)		{ in_aimleft      = true; clgis.in_aim[ANGLES_YAW]    = DIGITAL_YAW_SENS( 1); }
void Game_CL_CMD_AimRightPress(void)	{ in_aimright     = true; clgis.in_aim[ANGLES_YAW]    = DIGITAL_YAW_SENS(-1); }
void Game_CL_CMD_AimRollLeftPress(void)	{ in_aimrollleft  = true; clgis.in_aim[ANGLES_ROLL]   = DIGITAL_ROLL_SENS( 1); }
void Game_CL_CMD_AimRollRightPress(void){ in_aimrollright = true; clgis.in_aim[ANGLES_ROLL]   = DIGITAL_ROLL_SENS(-1); }

/*
===================
Game_CL_CMD_Aim*Unpress
===================
*/
void Game_CL_CMD_AimUpUnpress(void)			{ in_aimup        = false; if (in_aimdown)      clgis.in_aim[ANGLES_PITCH]  = DIGITAL_PITCH_SENS(PITCH(-1));  else clgis.in_aim[ANGLES_PITCH] = 0; }
void Game_CL_CMD_AimDownUnpress(void)		{ in_aimdown      = false; if (in_aimup)        clgis.in_aim[ANGLES_PITCH]  = DIGITAL_PITCH_SENS(PITCH( 1));  else clgis.in_aim[ANGLES_PITCH] = 0; }
void Game_CL_CMD_AimLeftUnpress(void)		{ in_aimleft      = false; if (in_aimright)     clgis.in_aim[ANGLES_YAW]    = DIGITAL_YAW_SENS(-1);           else clgis.in_aim[ANGLES_YAW]   = 0; }
void Game_CL_CMD_AimRightUnpress(void)		{ in_aimright     = false; if (in_aimleft)      clgis.in_aim[ANGLES_YAW]    = DIGITAL_YAW_SENS( 1);           else clgis.in_aim[ANGLES_YAW]   = 0; }
void Game_CL_CMD_AimRollLeftUnpress(void)	{ in_aimrollleft  = false; if (in_aimrollright) clgis.in_aim[ANGLES_ROLL]   = DIGITAL_ROLL_SENS(-1);          else clgis.in_aim[ANGLES_ROLL]  = 0; }
void Game_CL_CMD_AimRollRightUnpress(void)	{ in_aimrollright = false; if (in_aimrollleft)  clgis.in_aim[ANGLES_ROLL]   = DIGITAL_ROLL_SENS( 1);          else clgis.in_aim[ANGLES_ROLL]  = 0; }

/*
===================
Game_CL_CMD_AimAnalog*
===================
*/
void Game_CL_CMD_AimAnalogPitch(void)
{
	vec_t rel;

	if (host_cmd_argc != 3)
	{
		Sys_Printf("Wrong number of arguments for Game_CL_CMD_AimAnalogPitch\n");
		return;
	}

	rel = (vec_t)atof(host_cmd_argv[1]);
	clgis.in_aim[ANGLES_PITCH] = ANALOG_PITCH_SENS(PITCH(-rel));
}
void Game_CL_CMD_AimAnalogYaw(void)
{
	vec_t rel;

	if (host_cmd_argc != 3)
	{
		Sys_Printf("Wrong number of arguments for Game_CL_CMD_AimAnalogYaw\n");
		return;
	}

	rel = (vec_t)atof(host_cmd_argv[1]);
	clgis.in_aim[ANGLES_YAW] = ANALOG_YAW_SENS(-rel);
}
void Game_CL_CMD_AimAnalogRoll(void)
{
	vec_t rel;

	if (host_cmd_argc != 3)
	{
		Sys_Printf("Wrong number of arguments for Game_CL_CMD_AimAnalogRoll\n");
		return;
	}

	rel = (vec_t)atof(host_cmd_argv[1]);
	clgis.in_aim[ANGLES_ROLL] = ANALOG_ROLL_SENS(-rel);
}

/*
===================
Game_CL_CMD_Button*Press
===================
*/
void Game_CL_CMD_Button0Press(void)	{ clgis.in_buttons |= 1; }
void Game_CL_CMD_Button1Press(void)	{ clgis.in_buttons |= 2; }
void Game_CL_CMD_Button2Press(void)	{ clgis.in_buttons |= 4; }

/*
===================
Game_CL_CMD_Button*Unpress
===================
*/
void Game_CL_CMD_Button0Unpress(void)	{ clgis.in_buttons &= 255 - 1; }
void Game_CL_CMD_Button1Unpress(void)	{ clgis.in_buttons &= 255 - 2; }
void Game_CL_CMD_Button2Unpress(void)	{ clgis.in_buttons &= 255 - 4; }

/*
===================
Game_CL_CMD_TriggerButton*
===================
*/
void Game_CL_CMD_TriggerButton0(void)	{ clgis.in_triggerbuttons |= 1; }
void Game_CL_CMD_TriggerButton1(void)	{ clgis.in_triggerbuttons |= 2; }
void Game_CL_CMD_TriggerButton2(void)	{ clgis.in_triggerbuttons |= 4; }
void Game_CL_CMD_TriggerButton3(void)	{ clgis.in_triggerbuttons |= 8; }

/*
===================
Game_CL_CMD_Impulse
===================
*/
void Game_CL_CMD_Impulse(void)
{
	unsigned char impulse;

	if (host_cmd_argc != 2)
	{
		Sys_Printf("Wrong number of arguments for Game_CL_CMD_Impulse\n");
		return;
	}

	impulse = (unsigned char)atoi(host_cmd_argv[1]);
	clgis.impulse = impulse;
}

/*
===================
Game_CL_CMD_ShowScores*
===================
*/
void Game_CL_CMD_ShowScoresPress(void)		{ clgis.sbar_showscores = true; }
void Game_CL_CMD_ShowScoresUnpress(void)	{ clgis.sbar_showscores = false; }

/*
===================
Game_CL_InputReset
===================
*/
void Game_CL_InputReset(void)
{
	memset(&clgis, 0, sizeof(clgis));
	memset(&saved_commands, 0, sizeof(saved_commands));
	saved_command_current = 0;
}

/*
===================
Game_CL_InputResetTriggersAndImpulse
===================
*/
void Game_CL_InputResetTriggersAndImpulse(void)
{
	clgis.in_triggerbuttons = 0; /* trigger buttons are reset after getting sent */
	clgis.impulse = 0; /* impulse reset after getting sent */
}

/*
===================
Game_CL_InputSaveState
===================
*/
void Game_CL_InputSaveState(void)
{
	saved_commands[NEXT_SAVED_COMMAND].cmds = clgis;
	saved_commands[NEXT_SAVED_COMMAND].frametime = host_frametime;
	saved_commands[NEXT_SAVED_COMMAND].seq = saved_commands[saved_command_current].seq + 1;
	if (!saved_commands[NEXT_SAVED_COMMAND].seq)
		saved_commands[NEXT_SAVED_COMMAND].seq++;
	saved_command_current = NEXT_SAVED_COMMAND;
}

/*
===================
Game_CL_InputGetState
===================
*/
const client_game_input_state_t *Game_CL_InputGetState(void)
{
	return &saved_commands[saved_command_current].cmds;
}

/*
===================
Game_CL_InputGetFrameTime
===================
*/
const mstime_t Game_CL_InputGetFrameTime(void)
{
	return saved_commands[saved_command_current].frametime;
}

/*
===================
Game_CL_InputGetSequence
===================
*/
const unsigned short Game_CL_InputGetSequence(void)
{
	return saved_commands[saved_command_current].seq;
}

/*
===================
Game_CL_InputSetCurrentBySequence

Returns "true" if succesful
===================
*/
const int Game_CL_InputSetCurrentBySequence(unsigned short seq)
{
	int i;

	for (i = 0; i < MAX_SAVED_SNAPSHOTS; i++)
		if (saved_commands[i].seq == seq)
		{
			saved_command_current = i;
			return true;
		}

	return false;
}

/*
===================
Game_CL_InputSetNext
===================
*/
void Game_CL_InputSetNext(void)
{
	saved_command_current = NEXT_SAVED_COMMAND;
}

/*
===================
Game_CL_InputInit
===================
*/
void Game_CL_InputInit(void)
{
	int i;

	in_moveleft = in_moveright = in_moveup = in_movedown = in_moveforward = in_movebackward = 0;
	Math_ClearVector3(clgis.in_mov);
	Host_CMDAdd("+moveleft", Game_CL_CMD_MoveLeftPress);
	Host_CMDAdd("-moveleft", Game_CL_CMD_MoveLeftUnpress);
	Host_CMDAdd("+moveright", Game_CL_CMD_MoveRightPress);
	Host_CMDAdd("-moveright", Game_CL_CMD_MoveRightUnpress);
	Host_CMDAdd("+moveup", Game_CL_CMD_MoveUpPress);
	Host_CMDAdd("-moveup", Game_CL_CMD_MoveUpUnpress);
	Host_CMDAdd("+movedown", Game_CL_CMD_MoveDownPress);
	Host_CMDAdd("-movedown", Game_CL_CMD_MoveDownUnpress);
	Host_CMDAdd("+moveforward", Game_CL_CMD_MoveForwardPress);
	Host_CMDAdd("-moveforward", Game_CL_CMD_MoveForwardUnpress);
	Host_CMDAdd("+movebackward", Game_CL_CMD_MoveBackwardPress);
	Host_CMDAdd("-movebackward", Game_CL_CMD_MoveBackwardUnpress);
	Host_CMDAdd("moveanalog_sidestep", Game_CL_CMD_MoveAnalogSidestep);
	Host_CMDAdd("moveanalog_up", Game_CL_CMD_MoveAnalogUp);
	Host_CMDAdd("moveanalog_forward", Game_CL_CMD_MoveAnalogForward);

	in_aimup = in_aimdown = in_aimleft = in_aimright = in_aimrollleft = in_aimrollright = 0;
	Math_ClearVector3(clgis.in_aim);
	Host_CMDAdd("+aimup", Game_CL_CMD_AimUpPress);
	Host_CMDAdd("-aimup", Game_CL_CMD_AimUpUnpress);
	Host_CMDAdd("+aimdown", Game_CL_CMD_AimDownPress);
	Host_CMDAdd("-aimdown", Game_CL_CMD_AimDownUnpress);
	Host_CMDAdd("+aimleft", Game_CL_CMD_AimLeftPress);
	Host_CMDAdd("-aimleft", Game_CL_CMD_AimLeftUnpress);
	Host_CMDAdd("+aimright", Game_CL_CMD_AimRightPress);
	Host_CMDAdd("-aimright", Game_CL_CMD_AimRightUnpress);
	Host_CMDAdd("+aimrollleft", Game_CL_CMD_AimRollLeftPress);
	Host_CMDAdd("-aimrollleft", Game_CL_CMD_AimRollLeftUnpress);
	Host_CMDAdd("+aimrollright", Game_CL_CMD_AimRollRightPress);
	Host_CMDAdd("-aimrollright", Game_CL_CMD_AimRollRightUnpress);
	Host_CMDAdd("aimanalog_pitch", Game_CL_CMD_AimAnalogPitch);
	Host_CMDAdd("aimanalog_yaw", Game_CL_CMD_AimAnalogYaw);
	Host_CMDAdd("aimanalog_roll", Game_CL_CMD_AimAnalogRoll);

	clgis.in_buttons = 0;
	Host_CMDAdd("+button0", Game_CL_CMD_Button0Press);
	Host_CMDAdd("-button0", Game_CL_CMD_Button0Unpress);
	Host_CMDAdd("+button1", Game_CL_CMD_Button1Press);
	Host_CMDAdd("-button1", Game_CL_CMD_Button1Unpress);
	Host_CMDAdd("+button2", Game_CL_CMD_Button2Press);
	Host_CMDAdd("-button2", Game_CL_CMD_Button2Unpress);

	clgis.in_triggerbuttons = 0;
	Host_CMDAdd("triggerbutton0", Game_CL_CMD_TriggerButton0);
	Host_CMDAdd("triggerbutton1", Game_CL_CMD_TriggerButton1);
	Host_CMDAdd("triggerbutton2", Game_CL_CMD_TriggerButton2);
	Host_CMDAdd("triggerbutton3", Game_CL_CMD_TriggerButton3);

	clgis.impulse = 0;
	Host_CMDAdd("impulse", Game_CL_CMD_Impulse);

	Host_CMDAdd("+showscores", Game_CL_CMD_ShowScoresPress);
	Host_CMDAdd("-showscores", Game_CL_CMD_ShowScoresUnpress);

	in_invertpitch = Host_CMDAddCvar("in_invertpitch", "0", CVAR_ARCHIVE);
	in_sensitivity_digital_pitch = Host_CMDAddCvar("in_sensitivity_digital_pitch", "1", CVAR_ARCHIVE);
	in_sensitivity_digital_yaw = Host_CMDAddCvar("in_sensitivity_digital_yaw", "1", CVAR_ARCHIVE);
	in_sensitivity_digital_roll = Host_CMDAddCvar("in_sensitivity_digital_roll", "1", CVAR_ARCHIVE);
	in_sensitivity_analog_pitch = Host_CMDAddCvar("in_sensitivity_analog_pitch", "96", CVAR_ARCHIVE);
	in_sensitivity_analog_yaw = Host_CMDAddCvar("in_sensitivity_analog_yaw", "96", CVAR_ARCHIVE);
	in_sensitivity_analog_roll = Host_CMDAddCvar("in_sensitivity_analog_roll", "96", CVAR_ARCHIVE);

	for (i = 0; i < MAX_SAVED_SNAPSHOTS; i++)
	{
		saved_commands[i].cmds = clgis;
		saved_commands[i].frametime = 0;
		saved_commands[i].seq = 0;
	}
	saved_command_current = 0;
}

/*
===================
Game_CL_InputShutdown
===================
*/
void Game_CL_InputShutdown(void)
{
}
