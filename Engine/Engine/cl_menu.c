/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Menu system routines (includes everything 2D: status bars, etc)

============================================================================
*/

/*
===================
CL_MenuInit
===================
*/
void CL_MenuInit(void)
{
	Game_CL_MenuInit();
}

/*
===================
CL_MenuShutdown
===================
*/
void CL_MenuShutdown(void)
{
	Game_CL_MenuShutdown();
}

/*
===================
CL_MenuDraw
===================
*/
void CL_MenuDraw(void)
{
	Game_CL_MenuDraw();
}

/*
===================
CL_MenuKey

We check for keydest in cl_input.c before entering this function
===================
*/
void CL_MenuKey(int keyindex, int down, vec_t analog_rel, vec_t analog_abs)
{
	Game_CL_MenuKey(keyindex, down, analog_rel, analog_abs);
}

/*
===================
CL_MenuFrame
===================
*/
void CL_MenuFrame(void)
{
	Game_CL_MenuFrame();
}

/*
===================
CL_MenuConsolePrint
===================
*/
void CL_MenuConsolePrint(char *text)
{
	Game_CL_MenuConsolePrint(text);
}