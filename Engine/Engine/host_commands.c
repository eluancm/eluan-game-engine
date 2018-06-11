/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Commands and cvars definitions

Cvars should ONLY be used for variables that can change anytime without
any negative effect. In the other case, a command should be used and the cvar
should be marked readonly, to be updated only by the corresponding command.
It's possible to have cvars both "read-only" and "archive" at the same time.

Using commands to update cvar is also useful when you want to restrain the
values the cvar contains, like length and possible values.

Note: if you add a archived cvar before the subsystem has been initialized,
the archived value won't be available until the saved config has been
loaded. TODO: see if more problems arise with this.

TODO: clean the cvar mess: use set and seta in the config files to create,
and don't save the default values in the, only when they have been
initialized by the c-code! This means that setting a cvar to the default
shouldn't happen before all cvars have been initialized. Or maybe stop
storing the default value and just put a "delete cvars.cfg on quit" option
and don't forget to NOT save the cvars to cvars.cfg on exit. This will need
a restart to reset to defaults.

TODO: cvar bounds checking stored in the cvar struct (for easy checking of
saved values). But this may change initialization order (because we use
saved stuff before adding the cvars via code!!) After doing this, remove
the bounds checking stuff spread through the code.

TODO: also store which cvars will be broadcast to clients upon changes (make
them part of the game state snapshot?)

TODO: see everywhere we add new commands to the end of the buffer instead of
adding immediately after the current command.

============================================================================
*/

/* commands */
int commands_initialized = false;

int host_num_cmds;
cmd_t commands[MAX_COMMANDS];

int host_cmd_argc;
char *host_cmd_argv[MAX_CMD_ARGS];

/* command buffer */
#define MAX_COMMAND_BUFFER_SIZE		16384
int cmdbuffer_end;
char cmdbuffer[MAX_COMMAND_BUFFER_SIZE];
int cmdbuffer_num_waits;

/* cvars */
#define HOST_CMD_SET "set"
cvar_t cvar_head = {NULL, "", "", 0., 0, ""}; /* initialized here so that we may add cvars before initializing the cmd subsystem */

cvar_t *host_speeds = NULL;
cvar_t *host_netdelay = NULL;
cvar_t *host_netdelay_jitterlow = NULL;
cvar_t *host_netdelay_jitterhigh = NULL;
cvar_t *host_netloss = NULL;
cvar_t *temp1 = NULL;

/*
============================================================================

Host commands, valid for both client and server

============================================================================
*/

/*
===================
Host_CMD_Echo

Echoes all arguments
===================
*/
void Host_CMD_Echo(void)
{
	int i;

	for (i = 1; i < host_cmd_argc; i++)
	{
		if (i > 1)
			Sys_Printf(" ");
		Sys_Printf("%s", host_cmd_argv[i]); /* avoid % expansion exploit */
	}
	Sys_Printf("\n");
}

/*
===================
Host_CMD_Wait

Halts the command execution for one frame, or the specified number of frames
TODO: in some places in the engine we clear everything then execute a new command buffer, see if any may hurt
===================
*/
void Host_CMD_Wait(void)
{
	int i;

	if (host_cmd_argc > 2)
	{
		Sys_Printf("Wrong number of arguments for wait.\nUsage:\n:\"wait\" - waits for one frame before continuing command execution\n\"wait x\" - waits for \"x\" frames before continuing command execution\n");
		return;
	}

	if (host_cmd_argc == 1)
	{
		cmdbuffer_num_waits++;
		return;
	}

	Sys_Sscanf_s(host_cmd_argv[1], "%d", &i);
	if (i > 0)
		cmdbuffer_num_waits += i;
	else
		Sys_Printf("Can't wait for %d frames\n", i);
}

/*
===================
Host_CMD_Exec
===================
*/
void Host_CMD_Exec(void)
{
	int marker, size;
	unsigned char *config;
	unsigned char *ptr;
	char lcmd[MAX_LINE];

	if (host_cmd_argc != 2)
	{
		Sys_Printf("Wrong number of arguments for exec.\n");
		return;
	}

	Sys_Printf("Executing \"%s\"\n", host_cmd_argv[1]);

	marker = Sys_MemLowMark(&tmp_mem);

	if ((size = Host_FSLoadBinaryFile(host_cmd_argv[1], &tmp_mem, "scriptexec", &config, false)) != -1)
	{
		ptr = config;
		while (ptr < config + size && *ptr)
		{
#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
			if (Sys_Sscanf_s(ptr, "%[^\r\n]", lcmd) != 1)
#else
			if (Sys_Sscanf_s(ptr, "%[^\r\n]", lcmd, sizeof(lcmd)) != 1)
#endif /* __GNUC__ */
				break;

			Host_CMDBufferAdd(lcmd);

			ptr = Host_CMDSkipBlank(ptr);
		}
	}

	Sys_MemFreeToLowMark(&tmp_mem, marker);
}

/*
===================
Host_CMD_List
===================
*/
void Host_CMD_List(void)
{
	int i;

	Sys_Printf("Command list\n------------\n");

	for (i = 0; i < host_num_cmds; i++)
	{
		if (commands[i].active)
			Sys_Printf("%s\n", commands[i].name);
	}

	Sys_Printf("------------\nEnd of command list\n%d command(s) found\n", host_num_cmds);
}

/*
===================
Host_CMD_CvarList
===================
*/
void Host_CMD_CvarList(void)
{
	int i = 0;
	cvar_t *varptr = &cvar_head;

	Sys_Printf("Cvar list\n---------\n");

	while (varptr->next)
	{
		varptr = varptr->next;
		i++;
		Sys_Printf("\"%s\" with value \"%s\" (double: \"%lf\") Flags: %c%c\n", varptr->name, varptr->charvalue, varptr->doublevalue, varptr->flags & CVAR_ARCHIVE ? 'A' : '-', varptr->flags & CVAR_READONLY ? 'R' : '-');
	}

	Sys_Printf("---------\nEnd of cvar list\n%d cvars(s) found\n", i);
}

/*
===================
Host_CMD_Search
===================
*/
void Host_CMD_Search(void)
{
	int count = 0;
	int i;
	cvar_t *varptr = &cvar_head;

	if (host_cmd_argc != 2)
	{
		Sys_Printf("Wrong number of arguments for search.\n");
		return;
	}

	Sys_Printf("Searching for: \"%s\"\n", host_cmd_argv[1]);

	for (i = 0; i < host_num_cmds; i++)
	{
		if (commands[i].active)
		{
			if (strstr(commands[i].name, host_cmd_argv[1]))
			{
				Sys_Printf("Cmd : %s\n", commands[i].name);
				count++;
			}
		}
	}

	while (varptr->next)
	{
		int found = false;

		varptr = varptr->next;
		if (strstr(varptr->name, host_cmd_argv[1]))
			found = true;
		else if (strstr(varptr->charvalue, host_cmd_argv[1]))
			found = true;

		if (found)
		{
			Sys_Printf("Cvar: \"%s\" with value \"%s\" (double: \"%lf\") Flags: %c%c\n", varptr->name, varptr->charvalue, varptr->doublevalue, varptr->flags & CVAR_ARCHIVE ? 'A' : '-', varptr->flags & CVAR_READONLY ? 'R' : '-');
			count++;
		}
	}

	Sys_Printf("------------\nEnd of search\n%d result(s) found\n", count);
}

/*
===================
Host_CMD_Set

TODO: printing doubles as strings may cause precision loss (by default the entire mantissa isn't printed)
===================
*/
void Host_CMD_Set(void)
{
	cvar_t *varptr = &cvar_head;
	char value[MAX_CVAR_SIZE];
	int i, size, vsize;

	if (host_cmd_argc < 2)
	{
		Sys_Printf("Too few arguments for \"set\"\n");
	}
	else
	{
		if (strlen(host_cmd_argv[1]) + 1 >= MAX_CVAR_SIZE)
		{
			Sys_Printf("strlen(host_cmd_argv[1]) + 1 >= MAX_CVAR_SIZE, \"%s\" is too long for a cvar name\n", host_cmd_argv[1]);
			return;
		}

		while (varptr->next != NULL)
		{
			varptr = varptr->next;
			if (!strcmp(varptr->name, host_cmd_argv[1]))
			{
				if (host_cmd_argc == 2)
					Sys_Printf("\"%s\" is \"%s\"\n", host_cmd_argv[1], varptr->charvalue);
				else
				{
					if (varptr->flags & CVAR_READONLY)
					{
						Sys_Printf("\"%s\" is read-only\n", varptr->name);
						return;
					}

					/* TODO FIXME: why do we ignore multiple spaces and only set one? Only accept two arguments and quote the value if it needs more than one word. This will need a parser for quoted parameters */
					for (i = 2, size = 0; i < host_cmd_argc; i++)
					{
						size += strlen(host_cmd_argv[i]);
					}
					size += host_cmd_argc - 3; /* add blank spaces */

					Sys_Snprintf(value, sizeof(value), "%s", host_cmd_argv[2]);
					for(i = 3; i < host_cmd_argc; i++)
					{
						vsize = strlen(value);
						Sys_Strncat(value, " ", MAX_CVAR_SIZE - vsize);
						vsize = strlen(value);
						Sys_Strncat(value, host_cmd_argv[i], MAX_CVAR_SIZE - vsize);
					}
					if (strlen(value) != size) /* check here because we needed to transcode it */
					{
						/* TODO: see if it was truncated correctly and has \0 at the end */
						Sys_Printf("strlen(value) != size, \"%s\" is too long for a cvar value, truncating\n", value);
					}

					Host_CMDForceCvarSet(varptr, value, true);
				}
				return;
			}
		}

		Sys_Printf("Cvar \"%s\" not found\n", host_cmd_argv[1]);
	}
}

/*
===================
Host_CMD_CvarAdd

Create Cvars via config files
===================
*/
void Host_CMD_CvarAdd(void)
{
	if (host_cmd_argc != 4)
	{
		Sys_Printf("Usage: %s \"name\" \"default value\" flags\nFlags may be \"-\" for no flags or any combination of A for archive and R for read-only.\n");
	}
	else
	{
		int flags;
		char *ptr;

		flags = 0;
		ptr = host_cmd_argv[3];
		while (*ptr)
		{
			switch (*ptr)
			{
				case '-':
					break;
				case 'A':
					flags |= CVAR_ARCHIVE;
					break;
				case 'R':
					flags |= CVAR_READONLY;
					break;
				default:
					Sys_Printf("Unrecognized flag: %c\n", *ptr);
					return;
			}
			ptr++;
		}

		Host_CMDAddCvar(host_cmd_argv[1], host_cmd_argv[2], flags);
	}
}

/*
===================
Host_CMD_SaveCvarsConfig
===================
*/
#define MAX_CVARCONFIG_SIZE						65536
#define CVARCONFIG_FILENAME					"cvars.cfg"

void Host_CMD_SaveCvarsConfig(void)
{
	int marker, size = 0;
	unsigned char *config;
	cvar_t *cvarptr = cvar_head.next;

	marker = Sys_MemLowMark(&tmp_mem);
	config = Sys_MemAlloc(&tmp_mem, MAX_CVARCONFIG_SIZE, "savedcvarconfig");
	config[0] = 0;
	Sys_Strncat(config, "// this file is generated automatically, any edit may be lost\n", MAX_CVARCONFIG_SIZE - strlen(config));
	while (cvarptr)
	{
		/* save only cvars flagged as CVAR_ARCHIVE which have been used this session */
		if (cvarptr->flags & CVAR_ARCHIVE && !cvarptr->loadedfromconfig)
		{
			Sys_Strncat(config, cvarptr->name, MAX_CVARCONFIG_SIZE - strlen(config));
			Sys_Strncat(config, " ", MAX_CVARCONFIG_SIZE - strlen(config));
			Sys_Strncat(config, cvarptr->charvalue, MAX_CVARCONFIG_SIZE - strlen(config));
			Sys_Strncat(config, " ", MAX_CVARCONFIG_SIZE - strlen(config));
			/* we won't save the CVAR_ARCHIVE flag because that's redundant */
			Sys_Strncat(config, cvarptr->flags & CVAR_READONLY ? "R" : "-", MAX_CVARCONFIG_SIZE - strlen(config));
			Sys_Strncat(config, " ", MAX_CVARCONFIG_SIZE - strlen(config));
			Sys_Strncat(config, cvarptr->default_value, MAX_CVARCONFIG_SIZE - strlen(config));
			Sys_Strncat(config, "\n", MAX_CVARCONFIG_SIZE - strlen(config));
		}
		cvarptr = cvarptr->next;
	}

	size = strlen(config);
	if (Host_FSWriteBinaryFile(CVARCONFIG_FILENAME, config, size) == size)
		Sys_Printf("Cvar config saved.\n");
	else
		Sys_Printf("Error saving cvar config file.\n");

	Sys_MemFreeToLowMark(&tmp_mem, marker);
}

/*
===================
Host_CMD_LoadCvarsConfig

TODO: make this just a exec
===================
*/
void Host_CMD_LoadCvarsConfig(void)
{
	int marker, size;
	unsigned char *config;
	unsigned char *ptr;
	char lname[MAX_CVAR_SIZE];
	char lcharvalue[MAX_CVAR_SIZE];
	char lreadonly;
	char ldefault_value[MAX_CVAR_SIZE];
	cvar_t *newvar;

	if (host_initialized)
	{
		Sys_Printf("Can't load a config file after initalization phase is complete.\n");
		return;
	}

	marker = Sys_MemLowMark(&tmp_mem);

	if ((size = Host_FSLoadBinaryFile(CVARCONFIG_FILENAME, &tmp_mem, "loadedcvarconfig", &config, false)) != -1)
	{
		ptr = config;
		ptr = Host_CMDSkipBlank(ptr); /* skip first line, it's the header comment */
		while (ptr < config + size && *ptr)
		{
#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
			if (Sys_Sscanf_s(ptr, "%s %s %c %s", lname, lcharvalue, &lreadonly, ldefault_value) != 4)
#else
			if (Sys_Sscanf_s(ptr, "%s %s %c %s", lname, sizeof(lname), lcharvalue, sizeof(lcharvalue), &lreadonly, sizeof(lreadonly), ldefault_value, sizeof(ldefault_value)) != 4)
#endif /* __GNUC__ */
				break;

			newvar = Host_CMDAddCvar(lname, ldefault_value, CVAR_ARCHIVE | (lreadonly == 'R' ? CVAR_READONLY : 0));
			Host_CMDForceCvarSet(newvar, lcharvalue, true);
			newvar->loadedfromconfig = true;

			ptr = Host_CMDSkipBlank(ptr);
		}
		Sys_Printf("Cvar config loaded.\n");
	}
	else
	{
		Sys_Printf("Error loading cvar config file.\n");
	}

	Sys_MemFreeToLowMark(&tmp_mem, marker);
}

/*
===================
Host_CMD_Quit
===================
*/
void Host_CMD_Quit(void)
{
	/*
		finish the execution buffer before quiting!
		no other function should set host_quit
	*/
	host_quit = true;
	Host_CMDBufferAdd("disconnect"); /* this should execute immediatelly */
}

/*
===================
Host_CMD_Disconnect

Shuts down any running client and/or server. Useful for errors.
===================
*/
void Host_CMD_Disconnect(void)
{
	/* TODO CONSOLEDEBUG if (svs.listening || cls.ingame)
		Sys_Printf("Shutting down any local client and server...\n"); */

	CL_Disconnect(false, true);
	SV_ShutdownServer(false);
}

/*
===================
Host_CMD_Map

Proxy command.
If server listening: relay to changelevel
If server not listening: relay to startserver and tell client to connect locally
===================
*/
void Host_CMD_Map(void)
{
	char newcmd[MAX_LINE];

	if (host_cmd_argc < 2)
	{
		Sys_Printf("No map specified.\n");
		return;
	}
	if (host_cmd_argc > 2)
	{
		Sys_Printf("Too many arguments.\n");
		return;
	}

	if (svs.listening)
	{
		Sys_Snprintf(newcmd, sizeof(newcmd), "changelevel %s", host_cmd_argv[1]);
		Host_CMDBufferAdd(newcmd);
		return;
	}

	Sys_Snprintf(newcmd, sizeof(newcmd), "startserver %s", host_cmd_argv[1]);
	Host_CMDBufferAdd(newcmd);

	Sys_NetCLServerSetToLocal();
	Sys_Snprintf(newcmd, sizeof(newcmd), "connect");
	Host_CMDBufferAdd(newcmd);
}

typedef struct savegame_s {
	char			sv_name[MAX_MODEL_NAME];
	client_id_t		client_ids[MAX_CLIENTS];
	client_id_t		game_id;
	game_state_t	gs;
} savegame_t;

/*
===================
Host_CMD_Save

Saves the game

TODO: ENDIANNESS, ALIGNMENT, ARCHITECTURE/ABI PADDING, KEEP IN SYNC, CHECK ERRORS HERE AND IN ALL SUBFUNCTIONS
===================
*/
void Host_CMD_Save(void)
{
	savegame_t *savegame;
	int lowmark;
	int i;

	if (host_cmd_argc != 2)
	{
		Sys_Printf("Usage: save \"savefilename.sav\"\n");
		return;
	}

	if (!svs.listening)
	{
		Sys_Printf("Can't save game, not fully started.\n");
		return;
	}

	if (svs.loading_saved_game)
	{
		Sys_Printf("Can't save game, not finished loading a saved game.\n");
		return;
	}

	if (Host_CMDGetCvar("_sv_maxplayers", true)->doublevalue != 1)
	{
		Sys_Printf("Can't save game, not single player.\n");
		return;
	}

	Sys_Printf("Saving game to %s...\n", host_cmd_argv[1]);

	/* save voxel data */
	Host_VoxelSave(host_cmd_argv[1]);
	/* save physics data */
	Sys_SavePhysicsWorld(svs.physworld, host_cmd_argv[1]);
	/* save extra game data */
	Game_SV_SaveExtraGameData(host_cmd_argv[1]);
	lowmark = Sys_MemLowMark(&tmp_mem);
	savegame = Sys_MemAlloc(&tmp_mem, sizeof(savegame_t), "savegame");
	/* save the server name, so that we can later recreate the initial state of this game */
	Sys_Snprintf(savegame->sv_name, MAX_MODEL_NAME, "%s", svs.name);
	/* store the client ids */
	for (i = 0; i < MAX_CLIENTS; i++)
		savegame->client_ids[i] = svs.sv_clients[i].client_id;
	savegame->game_id = svs.game_id;
	/* save the game logic data */
	memcpy(&savegame->gs, &gs, sizeof(gs));
	if (Host_FSWriteBinaryFile(host_cmd_argv[1], (unsigned char *)savegame, sizeof(savegame_t)) == -1)
		Sys_Printf("Error!\n");
	else
		Sys_Printf("Done!\n");
	Sys_MemFreeToLowMark(&tmp_mem, lowmark);
}

/*
===================
Host_CMD_Load

Loads a saved game. The order of loading things and setting vars IS IMPORTANT!

TODO: ENDIANNESS, ALIGNMENT, ARCHITECTURE/ABI PADDING, KEEP IN SYNC, CHECK ERRORS HERE AND IN ALL SUBFUNCTIONS
TODO: Lots of stuff loaded and processed needlessy
===================
*/
extern void SV_StartServer(int changelevel); /*  TODO FIXME: Host Calling SV? */
extern void CL_Connect(int reconnect); /* TODO FIXME: Host Calling CL? */
void Host_CMD_Load(void)
{
	savegame_t *savegame;
	int lowmark;
	int i;
	int full_reload = true;

	if (host_cmd_argc != 2)
	{
		Sys_Printf("Usage: load \"savefilename.sav\"\n");
		return;
	}

	Host_CMDForceCvarSetValue(Host_CMDGetCvar("_sv_maxplayers", true), 1, true);

	Sys_Printf("Loading game from %s...\n", host_cmd_argv[1]);

	lowmark = Sys_MemLowMark(&tmp_mem);
	if (Host_FSLoadBinaryFile(host_cmd_argv[1], &tmp_mem, "loadgame", (unsigned char **)&savegame, false) != sizeof(savegame_t))
	{
		Sys_Printf("Error!\n");
		return;
	}

	svs.loading_saved_game = true; /* to wait for the client(s) */

	if (!strncmp(savegame->sv_name, svs.name, sizeof(savegame->sv_name)))
		full_reload = false;

	if (full_reload)
	{
		/* create a new server with the same initial data as the saved one */
		Sys_Snprintf(svs.name, MAX_MODEL_NAME, "%s", savegame->sv_name);
		SV_StartServer(false);
	}
	/* restore the game logic data */
	memcpy(&gs, &savegame->gs, sizeof(gs));

	/* TODO: this can cause problems in multiplayer games! When players are on other players' slots. Do a full reload in this case? */
	if (full_reload)
	{
		/* restore the client ids */
		svs.game_id = savegame->game_id;
		for (i = 0; i < MAX_CLIENTS; i++)
			svs.sv_clients[i].client_id = savegame->client_ids[i];
	}

	Sys_MemFreeToLowMark(&tmp_mem, lowmark);

	/* restore the extra game data */
	Game_SV_LoadExtraGameData(host_cmd_argv[1]);
	/* restore the physics */
	Sys_LoadPhysicsWorld(svs.physworld, host_cmd_argv[1]);
	/* save voxel data */
	Host_VoxelLoad(host_cmd_argv[1]);

	if (full_reload)
	{
		/* connect the client */
		Sys_NetCLServerSetToLocal();
		CL_Connect(false);
	}

	Sys_Printf("Done!\n");
}

/*
============================================================================

Command management

============================================================================
*/

/*
===================
Host_CMDValidateAlphanumericValue

Only checks for illegal characters, not size
===================
*/
int Host_CMDValidateAlphanumericValue(const char *value)
{
	int	size, i;

	size = strlen(value);
	for (i = 0; i < size; i++)
	{
		if ((value[i] >= 'a' && value[i] <= 'z') ||
			(value[i] >= 'A' && value[i] <= 'Z') ||
			(value[i] >= '0' && value[i] <= '9') ||
			(value[i] == '_') || (value[i] == '.') ||
			(value[i] == '-') || (value[i] == '+') || /* also allow these */
			(value[i] == ' '))
			continue;
		else
			return false;
	}

	return true;
}

/*
===================
Host_CMDSkipBlank

Will skip the last line (which should already have been processed), may return at \0
No bounds checking.
If data comes from an file, you may need a \n at the end
===================
*/
unsigned char *Host_CMDSkipBlank(unsigned char *ptr)
{
	/* skip end of lines */
	while (*ptr == '\n' || *ptr == '\r')
		ptr++;
	/* got a line? skip it */
	while(*ptr != '\n' && *ptr != '\r' && *ptr != 0)
		ptr++;
	/* skip the end of this line */
	while (*ptr == '\n' || *ptr == '\r')
		ptr++;

	return ptr;
}

/*
===================
Host_CMDAdd
===================
*/
void Host_CMDAdd(const char *name, void (*function)(void))
{
	int i;

	if (!commands_initialized)
		Sys_Error("Host_CMDAdd: Command subsystem not initialized!");

	if (strlen(name) + 1 >= MAX_CMD_SIZE)
		Sys_Printf("Error: strlen(name) + 1 >= MAX_CMD_SIZE, command name \"%s\" is too long\n", name);

	for (i = 0; i < host_num_cmds; i++)
		if (!strcmp(commands[i].name, name))
			Sys_Error("Tried to register command with already registered name \"%s\"\n", name);

	Sys_Snprintf(commands[host_num_cmds].name, sizeof(commands[host_num_cmds].name), "%s", name);
	commands[host_num_cmds].active = true;
	commands[host_num_cmds].cmd = function;

	host_num_cmds++;
}

/*
===================
Host_CMDExecute

Not reentrant. Really. Be careful.
Using "double quotes" ('\"') a command may have spaces inside the arguments.
===================
*/
void Host_CMDExecute(char *input)
{
	int i, j, size;
	char cmd[MAX_CMD_SIZE];
	int quoted;
	char *setcmd = HOST_CMD_SET; /* TODO: see if nothing is modified (for reentrancy) */
	int tried_cvar_set = false;
	int inside_quotes;

	if (!commands_initialized)
		Sys_Error("Command subsystem not initialized!");

	Sys_Snprintf(cmd, sizeof(cmd), "%s", input);

	size = strlen(cmd);

	if (!size)
		return;

	if (size != strlen(input))
		Sys_Printf("Warning: truncating command \"%s\"\n", cmd);

	inside_quotes = false;
	for (i = 0; i < size; i++)
	{
		/* skip comments FIXME: have generic token parsing for the entire engine */
		if (cmd[i] == '\"')
			inside_quotes = inside_quotes ? false : true;
		if (!inside_quotes)
			if (size > i + 1 && cmd[i] == '/' && cmd[i + 1] == '/')
				for (j = i; j < size; j++)
					cmd[j] = 0;
	}

	host_cmd_argc = 0;
	for (i = 0; i < size; i++)
	{
		/* do the real parsing now */
		if (host_cmd_argc == MAX_CMD_ARGS)
		{
			Sys_Printf("Error: host_cmd_argc == MAX_CMD_ARGS, truncating argument list for command \"%s\"\n", host_cmd_argv[0]);
			break;
		}
		if (cmd[i] == 0)
			break;

		if (cmd[i] == ' ')
		{
			cmd[i] = 0;
			continue;
		}

		if (cmd[i] == '\"')
		{
			quoted = true;
			i++;

			if (cmd[i] == 0) /* reached the end? */
			{
				Sys_Printf("Error: unclosed double quotes!\n");
				return;
			}
		}
		else
			quoted = false;

		/* parse the entire arg */
		host_cmd_argv[host_cmd_argc++] = cmd + i;

		if (quoted)
		{
			while (cmd[i] != '\"')
			{
				if (cmd[i] == 0) /* reached the end? */
				{
					Sys_Printf("Error: unclosed double quotes!\n");
					return;
				}

				i++;
			}
			cmd[i] = 0;
		}

		while (cmd[i] != ' ' && cmd[i] != 0)
		{
			i++;
		}
		cmd[i] = 0;
	}

	/* comment line, etc */
	if (!host_cmd_argc)
		return;

trycmdagain:

	for (i = 0; i < host_num_cmds; i++)
	{
		if (!strcmp(commands[i].name, host_cmd_argv[0]))
		{
			commands[i].cmd();
			break;
		}
	}

	if (tried_cvar_set)
		return;	/* alreay tried as a cvar */

	if (i == host_num_cmds)
	{
		cvar_t *cvarexists = Host_CMDGetCvar(host_cmd_argv[0], false);
		if (cvarexists)
			tried_cvar_set = true; /* try as a cvar */
	}
	else
		return; /* if we found a cmd, dismiss the following code */

	if (!tried_cvar_set)
	{
		Sys_Printf("Error: command or cvar \"%s\" not found.\n", host_cmd_argv[0]);
		return;
	}

	if (host_cmd_argc == MAX_CMD_ARGS)
	{
		Sys_Printf("Error: command \"%s\" not found and too many args to call \"%s\"\n", host_cmd_argv[0], HOST_CMD_SET);
		return;
	}

	for (i = host_cmd_argc; i > 0; i--)
		host_cmd_argv[i] = host_cmd_argv[i - 1];
	host_cmd_argv[0] = setcmd;
	host_cmd_argc++;

	goto trycmdagain;
}

/*
===================
Host_CMDBufferClear
===================
*/
void Host_CMDBufferClear(void)
{
	cmdbuffer[0] = 0;
	cmdbuffer_end = 0; /* location of the last zero */
	cmdbuffer_num_waits = 0;
}

/*
===================
Host_CMDBufferAdd

Allow '\n', '\r' and ';' to separate commands
===================
*/
void Host_CMDBufferAdd(const char *input)
{
	int size;
	char cmd[MAX_CMD_SIZE];
	char *cmdptr;
	int inside_quotes;

	if (!commands_initialized)
		Sys_Error("Command subsystem not initialized!");

	Sys_Snprintf(cmd, sizeof(cmd), "%s", input);

	size = strlen(cmd);

	if (!size)
		return;

	if (size != strlen(input))
		Sys_Printf("Warning: truncating command \"%s\"\n", cmd);
	if (size + cmdbuffer_end + 2 >= MAX_COMMAND_BUFFER_SIZE) /* + 2 to be sure we have space for a final separator, if necessary */
	{
		Sys_Printf("Warning: Command buffer overflow, discarded command: \"%s\"\n", cmd);
		return;
	}

	inside_quotes = false;
	for (cmdptr = cmd; *cmdptr != 0; cmdptr++)
	{
		if (*cmdptr == '\"')
			inside_quotes = inside_quotes ? false : true;

		if (!inside_quotes)
			if (*cmdptr == '\n' || *cmdptr == '\r' || *cmdptr == ';')
			{
				if (cmdbuffer[cmdbuffer_end] != 0) /* avoid various zeros when repeating the separation chars or even the \r\n from windows */
					cmdbuffer[++cmdbuffer_end] = 0;
				continue;
			}

		cmdbuffer[++cmdbuffer_end] = *cmdptr;
	}
	if (cmdbuffer[cmdbuffer_end] != 0) /* if the last character wasn't a stray separation */
		cmdbuffer[++cmdbuffer_end] = 0; /* we use zero for separation to avoid exploits */
}

/*
===================
Host_CMDBufferExecute

A anything added during the current execution will be executed immediately
===================
*/
void Host_CMDBufferExecute(void)
{
	int i, j;

	if (!commands_initialized)
		Sys_Error("Command subsystem not initialized!");

	if (cmdbuffer_num_waits)
	{
		cmdbuffer_num_waits--;
		return;
	}

	for (i = 0; i <= cmdbuffer_end; i++) /* FIXME: hopefully cmdbuffer[cmdbuffer_end] == 0 */
	{
		if (cmdbuffer[i] == 0) /* yes, it starts with a zero to make this easy to implement */
		{
			if (cmdbuffer_num_waits)
				break; /* found the next command, but we will wait to act */

			if ((i + 1) != MAX_COMMAND_BUFFER_SIZE && i != cmdbuffer_end)
				Host_CMDExecute(cmdbuffer + i + 1);
		}
	}

	if (!cmdbuffer_num_waits)
		Host_CMDBufferClear();
	else
	{
		for (j = 0; i <= cmdbuffer_end; i++, j++) /* cmdbufer_end is inclusive (note the + 2 check in Host_CMDBufferAdd) */
		{
			cmdbuffer[j] = cmdbuffer[i];
		}
		cmdbuffer_end = j - 1; /* j - 1 because it will have been incremented when breaking the loop */
	}
}

/*
============================================================================

Cvar management

============================================================================
*/

/*
===================
Host_CMDAddCvar
===================
*/
cvar_t *Host_CMDAddCvar(const char *name, const char *value, const int flags)
{
	int i, size;
	cvar_t *varptr = &cvar_head;

	if (strlen(name) + 1 >= MAX_CVAR_SIZE)
		Sys_Error("Host_CMDAddCvar: strlen(name) + 1 >= MAX_CVAR_SIZE, \"%s\" is too long for a cvar name\n", name);
	if (strlen(value) + 1 >= MAX_CVAR_SIZE)
		Sys_Error("Host_CMDAddCvar: strlen(value) + 1 >= MAX_CVAR_SIZE, \"%s\" is too long for a cvar default value\n", value);

	size = strlen(name);
	for (i = 0; i < size; i++)
	{
		if ((name[i] >= 'a' && name[i] <= 'z') ||
			(name[i] >= 'A' && name[i] <= 'Z') ||
			(name[i] >= '0' && name[i] <= '9') ||
			(name[i] == '_'))
			continue;
		else
			Sys_Error("Host_CMDAddCvar: Variable names should only use 0-9, a-z and A-Z for the name. \"%s\" disqualifies (found \"%c\")\n", name, name[i]);
	}

	if (!Host_CMDValidateAlphanumericValue(value))
		Sys_Error("Host_CMDAddCvar: Invalid characters in default value \"%s\" for cvar \"%s\"\n", value, name);

	while (varptr->next != NULL)
	{
		varptr = varptr->next; /* first one is bogus */
		if (!strncmp(varptr->name, name, MAX_CVAR_SIZE))
		{
			if (!varptr->loadedfromconfig)
			{
				Sys_Error("Host_CMDAddCvar: Tried to register cvar with already registered name \"%s\"\nAre you trying to register cvars before the saved config is loaded?\n", name);
			}
			else
			{
				if (strcmp(varptr->default_value, value) || varptr->flags != flags)
					Sys_Error("Host_CMDAddCvar: Inconsistent config file for cvar \"%s\"\n", name);

				varptr->loadedfromconfig = false;
				return varptr;
			}
		}
	}

	varptr->next = Sys_MemAlloc(&std_mem, sizeof(cvar_t), "Cvars");
	varptr = varptr->next;
	varptr->next = NULL;

	Sys_Snprintf(varptr->name, sizeof(varptr->name), "%s", name);
	Host_CMDForceCvarSet(varptr, value, false);
	varptr->flags = flags;
	Sys_Snprintf(varptr->default_value, sizeof(varptr->default_value), "%s", value);
	varptr->loadedfromconfig = false; /* will be set by config loading code if that's the case */

	return varptr;
}

/*
===================
Host_CMDGetCvar
===================
*/
cvar_t *Host_CMDGetCvar(const char *name, int error_if_not_found)
{
	cvar_t *varptr = &cvar_head;

	while (varptr->next != NULL)
	{
		varptr = varptr->next; /* first one is bogus */
		if (!strncmp(varptr->name, name, MAX_CVAR_SIZE))
			return varptr;
	}

	if (error_if_not_found)
		Host_Error("Host_CMDGetCvar: Cvar \"%s\" not found.\n", name);

	return NULL;
}

/*
===================
Host_CMDForceCvarSet

"warn" is mostly to avoing printing the default value
===================
*/
void Host_CMDForceCvarSet(cvar_t *varptr, const char *value, int warn)
{
	if (strlen(value) + 1 >= MAX_CVAR_SIZE)
		Sys_Printf("\"%s\" is too long for a cvar value, truncating\n", value);

	if (!Host_CMDValidateAlphanumericValue(value))
	{
		Sys_Printf("Invalid characters in value \"%s\" for cvar \"%s\"\n", value, varptr->name);
		return;
	}

	if (warn && !strncmp(value, varptr->charvalue, MAX_CVAR_SIZE))
		warn = false;

	Sys_Snprintf(varptr->charvalue, sizeof(varptr->charvalue), "%s", value);
	varptr->doublevalue = atof(varptr->charvalue);

	if (warn)
	{
		/* TODO CONSOLEDEBUG Sys_Printf("\"%s\" changed to \"%s\" (double: \"%lf\")\n", varptr->name, varptr->charvalue, varptr->doublevalue); */
		Sys_Printf("\"%s\" changed to \"%s\"\n", varptr->name, varptr->charvalue);
	}
}

/*
===================
Host_CMDForceCvarSetValue

"warn" is mostly to avoing printing the default value
===================
*/
void Host_CMDForceCvarSetValue(cvar_t *varptr, const double value, int warn)
{
	if (warn && varptr->doublevalue == value)
		warn = false;

	varptr->doublevalue = value;
	Sys_Snprintf(varptr->charvalue, sizeof(varptr->charvalue), "%lf", value);

	if (warn)
	{
		/* TODO CONSOLEDEBUG Sys_Printf("\"%s\" changed to \"%s\" (double: \"%lf\")\n", varptr->name, varptr->charvalue, varptr->doublevalue); */
		Sys_Printf("\"%s\" changed to \"%s\"\n", varptr->name, varptr->charvalue);
	}
}

/*
============================================================================

Main command and cvar routines

============================================================================
*/

/*
===================
Host_CMDInit
===================
*/
void Host_CMDInit(void)
{
	int i;

	/* zero cmds */
	host_num_cmds = 0;
	for (i = 0; i < MAX_COMMANDS; i++)
		commands[i].active = false;

	/* empty command buffer */
	Host_CMDBufferClear();

	/* I know, everything above is on the heap and was already zero from the start... */
	commands_initialized = true;

	Host_CMDAdd("echo", Host_CMD_Echo);
	Host_CMDAdd("wait", Host_CMD_Wait);
	Host_CMDAdd("exec", Host_CMD_Exec);
	Host_CMDAdd("cmdlist", Host_CMD_List);
	Host_CMDAdd("cvarlist", Host_CMD_CvarList);
	Host_CMDAdd("search", Host_CMD_Search);
	Host_CMDAdd(HOST_CMD_SET, Host_CMD_Set);
	Host_CMDAdd("cvaradd", Host_CMD_CvarAdd);
	Host_CMDAdd("savecvarsconfig", Host_CMD_SaveCvarsConfig);
	Host_CMDAdd("loadcvarsconfig", Host_CMD_LoadCvarsConfig);
	Host_CMDAdd("quit", Host_CMD_Quit);
	Host_CMDAdd("disconnect", Host_CMD_Disconnect);
	Host_CMDAdd("map", Host_CMD_Map);
	Host_CMDAdd("save", Host_CMD_Save);
	Host_CMDAdd("load", Host_CMD_Load);

	host_speeds = Host_CMDAddCvar("host_speeds", "0", 0);
	host_netdelay = Host_CMDAddCvar("host_netdelay", "0", 0);
	host_netdelay_jitterlow = Host_CMDAddCvar("host_netdelay_jitterlow", "0", 0);
	host_netdelay_jitterhigh = Host_CMDAddCvar("host_netdelay_jitterhigh", "0", 0);
	host_netloss = Host_CMDAddCvar("host_netloss", "0", 0);
	/* temporary cvar that can be used to toggle test stuff while developing */
	temp1 = Host_CMDAddCvar("temp1", "0", 0);

	/*
		load our configuration file before anything to make sure the command line arguments and
		initialization code using saved cvars work properly
	*/
	Host_CMDBufferAdd("loadcvarsconfig"); /* TODO: change this to an exec? */
	Host_CMDBufferExecute(); /* no problem, we're in an init function */
}

/*
===================
Host_CMDShutdown
===================
*/
void Host_CMDShutdown(void)
{
	Host_CMDBufferAdd("savecvarsconfig");
	Host_CMDBufferExecute(); /* no problem, we're in a shutdown function */

	commands_initialized = false;
}
