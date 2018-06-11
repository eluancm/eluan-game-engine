/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
	When dealing with connections, only the commands connect, startserver and disconnect should be used! FIXME?
*/

/*
============================================================================

General host constants and variables

============================================================================
*/

int host_initialized = false;
int host_quit = false;

/* host_framecount, host_framecount_notzero and host_realtime may overlap, be careful when using them! Mainly casting after "+1" cases to prevent undetected overflows, etc */
framenum_t host_framecount; /* for frame counting around the engine */
framenum_t host_framecount_notzero; /* for frame counting around the engine - will never be zero */
mstime_t host_realtime; /* for timekeeping in miliseconds around the engine */
mstime_t host_frametime; /* very bad things may happen with this one if time overlaps */

/*
============================================================================

Command line parameters management

============================================================================
*/

int host_argc;
char *host_argkey[MAX_ARGS];
char *host_argvalue[MAX_ARGS];

/*
===================
Host_CheckArg

Checks if an given argument has been passed on the command line, returns 0 on failure, otherwise
returns 1-based argument position
===================
*/
int Host_CheckArg(const char *arg)
{
		int i;

		for (i = 0; i < host_argc; i++)
		{
			if (!strcmp(host_argkey[i], arg))
				return i + 1;
		}

		return false;
}

/*
===================
Host_GetArg

Sets a pointer to the value of the given argument, returns 0 and sets NULL on failure, othewise
returns argument length
===================
*/
int Host_GetArg(const char *arg, char **dest)
{
	int argnum = Host_CheckArg(arg);

	if (!argnum)
	{
		*dest = NULL;
		return false;
	}
	else
	{
		argnum--;
		if (!host_argvalue[argnum])
			Sys_Error("!host_argvalue[argnum], command line argument \"%s\" should have a value specified\n", host_argkey[argnum]);

		*dest = host_argvalue[argnum];
		return strlen(host_argvalue[argnum]);
	}
}

/*
===================
Host_InitArgs

There are two types of args:
-Those that are prefixed with '-'. They will be read directly by the engine (usually when the
subsystems are starting).
-Those that are prefixed with '+'. These will be executed right after the command subsystem starts.

Args that have more than one parameter or have a parameter with spaces in it generally should be
enclosed as quotes on the system command line so that the structure is perfectly like this:

./game -basedir test +set "name The Mad Man"

The command shell will take care of ignoring spaces between the quotes.

A "-basedir" key with an "test" value
And a "+set" command with a "name The Mad Man" value

The system, as it is now, does not allow inputting command parameters that start with + or -.
===================
*/
void Host_InitArgs(int argc, char *argv[])
{
	int i;

	if (argc - 1 > MAX_ARGS) /* exclude the program name */
		Sys_Error("argc > MAX_ARGS (is %d, should not exceed %d)\n", argc, MAX_ARGS);

	host_argc = 0;

	/* copy them */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-' || argv[i][0] == '+')
		{
			host_argkey[host_argc] = argv[i];

			i++;

			if (i == argc)
			{
				host_argvalue[host_argc] = NULL;
			}
			else if (argv[i][0] != '-' && argv[i][0] != '+')
			{
				host_argvalue[host_argc] = argv[i];
			}
			else
			{
				host_argvalue[host_argc] = NULL;
				i--;
			}

			host_argc++;
		}
		else
		{
			Sys_Printf("Warning: Ignoring lone parameter \"%s\"\n", argv[i]);
		}
	}

	Sys_Printf("%d command line arguments\n", host_argc);
}

/*
============================================================================

Main host functions

============================================================================
*/

/*
===================
Host_Error

End currently running client and/or server, abort the frame and continue execution.
BEWARE: calling it from the renderer may leave it in a unconsistent state!
TODO: See where we should call Sys_Error instead of Host_Error because we would leave open file handles, etc
===================
*/
void Host_Error(const char *error, ...)
{
	va_list		argptr;
	char		text[MAX_LINE];
	char		text2[MAX_LINE];

	va_start(argptr, error);
	Sys_Vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	Sys_Printf("*** Host Error Occurred ***\n");

	Host_CMDBufferClear();
	Host_CMDBufferAdd("disconnect");
	Host_CMDBufferAdd("openconsole");
	Host_CMDBufferExecute();

	Sys_Snprintf(text2, sizeof(text2), "*** Host error: %s\n", text);
	Sys_Printf(text2);

	Sys_RestoreStackAndPC();
}

/*
===================
Host_Init
===================
*/
void Host_Init(int argc, char *argv[])
{
	int i;
	char *empty = "";
	char cmd[MAX_CMD_SIZE];

	host_framecount = 0;
	host_framecount_notzero = 1;
	host_realtime = Sys_Time(); /* initialize time */
	host_frametime = 0; /* TODO? */
	svs.active = false;
	cls.active = false;

	Host_InitArgs(argc, argv);

	Host_FSInit();
	Host_CMDInit();
	Host_ModelsInit();
	SV_Init();
	CL_Init();

	host_initialized = true;

	/* first autoexec, then command line statements */
	Host_CMDBufferAdd("openconsole");
	Host_CMDBufferAdd("exec autoexec.cfg");
	/* execute everything first to clear the buffer (and allow stuff placed at the end of the buffer to execute before the cmdline */
	/* TODO: get rid of all waits? */
	Host_CMDBufferExecute(); /* no problem, we're in an init function */
	/* execute commands line statements */
	for (i = 0; i < host_argc; i++)
	{
		if (host_argkey[i][0] == '+')
		{
			if (host_argvalue[i] == NULL)
				host_argvalue[i] = empty;

			if (strlen(host_argkey[i] + 1) + strlen(host_argvalue[i]) + 2 >= MAX_CMD_SIZE)
			{
				Sys_Printf("Warning: truncating command \"%s\" from command line\n", host_argkey[i]);
			}

			Sys_Snprintf(cmd, sizeof(cmd), "%s %s", host_argkey[i] + 1, host_argvalue[i]); /* TODO: fix this */
			Host_CMDBufferAdd(cmd);
		}
	}
	Host_CMDBufferExecute(); /* no problem, we're in an init function */

	if (cls.connected)
	{
		Host_CMDBufferAdd("toggleconsole");
	}
	else if (!svs.listening)
	{
		Host_CMDBufferAdd("toggleconsole");
		Host_CMDBufferAdd("opensplashmenu");
	}
}

/*
===================
Host_FilterTime

Gets absolute time value in milliseconds as a parameter
Returns TRUE if we should run this frame

Always updates host_realtime
Only updates host_frametime if TRUE

TODO: ms resolution is not enough! there may still be some quirks here
===================
*/
int Host_FilterTime(mstime_t time)
{
	mstime_t checktime;
	static mstime_t oldrealtime = 0;

	host_realtime = time;
	checktime = host_realtime - oldrealtime;

	if (checktime < (1000. / HOST_MAX_FPS)) /* don't let a frame be too short */
		return false;

	host_frametime = checktime;
	oldrealtime = host_realtime;

	/* host_frametime = (1000. / HOST_MAX_FPS); */ /* fixed timestep */

	if (host_frametime > (1000. / HOST_MIN_FPS)) /* and don't let it be too long (10fps minimum, even if it means slowing down time */
		host_frametime = (1000. / HOST_MIN_FPS);

	return true;
}

/*
===================
Host_Frame
===================
*/
void Host_Frame(void)
{
	mstime_t start_time, end_time;
	Sys_KeepRandom();

	/* TODO: hope this doesn't cause much performance issues. If it just saves SP and PC, no problem. */
	/* TODO: be sure that everything that needs to be destroyed is destroyed upon disconnection in Host_Error(), otherwise memory will leak if not checking when creating new instances of stuff (like physics world, etc) */
	Sys_SaveStackAndPC(); /* save stack frame so that we can bail out in case of errors by aborting the entire frame */

	Sys_ProcessEvents();

	if (Host_FilterTime(Sys_Time())) /* TODO: ignore this if no server and no client is running (so that we can keep updating the screen when loading without making loading slower by limiting framerate) */
	{
		if (host_speeds->doublevalue)
		{
			Sys_Printf("-----------\nframe start\n");
			start_time = Sys_Time();
		}

		if (host_quit)
		{
			Host_Shutdown();
			return; /* FIXME: do we really reach this? */
		}

		Host_CMDBufferExecute();
		SV_Frame();
		CL_Frame();

		host_framecount++;
		host_framecount_notzero++;
		if (!host_framecount_notzero)
			host_framecount_notzero++; /* TODO: reset everything that uses this to zero after overflow? almost impossible to happen if this is a 64-bit number and we have 60 frames per second anyway... */
		if (host_speeds->doublevalue)
		{
			end_time = Sys_Time();
			Sys_Printf("frame total: % 3f\n", end_time - start_time);
		}
	}
}

/*
===================
Host_Shutdown

This function is able to fully shut down the engine properly, but this isn't guaranteed if it's called mid-frame FIXME
===================
*/
void Host_Shutdown(void)
{
	CL_Shutdown();
	SV_Shutdown();
	Host_ModelsShutdown();
	Host_CMDShutdown();
	Host_FSShutdown();

	Sys_Shutdown();
}
