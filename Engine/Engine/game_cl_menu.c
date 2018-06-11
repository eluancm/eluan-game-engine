/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

static int m_menuopened = false;
static int m_consoleopened = false;
static int m_menutype_ingame = false;

/*
============================================================================

Console

TODO: when starting the game with +connect, for example, keydest == keydest_game. Why?
TODO: cutting messages when wraping lines

============================================================================
*/

static int console_initialized = false;

#define MAX_CONSOLE_LINES		16384
#define MAX_CONSOLE_HISTORY		32

static cvar_t *cl_conspeed = NULL;
static cvar_t *cl_conheight = NULL;
static texture_t *cl_conback;

/* TODO: is MAX_LINE to big, causing a VERY big console buffer? */
static char console_input[MAX_LINE];
static char console_input_history[MAX_CONSOLE_HISTORY][MAX_LINE];
static int console_input_history_num;
static int console_input_history_current;
static char console_input_history_saved_line[MAX_LINE]; /* to save what we are typing while browsing the history */
static char console_buffer[MAX_CONSOLE_LINES][MAX_LINE];
static int console_buffer_wrapped = false; /* if the console buffer was wrapped at least once */
static int console_current_new_line = 0;
static int console_current_new_line_pos = 0; /* column position */
static int console_scroll_start_line = 0; /* if this == console_current_new_line, no scrolling. Scrolling up decreases and scrolling down increases (with mod MAX_CONSOLE_LINES), should never reach console_current_new_line again. */
static vec_t console_y = 0;
static mstime_t console_opening_timer = 0;
static mstime_t console_closing_timer = 0;

/* TODO FIXME quick hack for some legibility gains - proportional to width even though the console is 4:3 in all resolutions as of this writing */
extern cvar_t *vid_windowedwidth;

#define CONSOLE_FONT_SCALEX			(BASE_WIDTH / (vec_t)vid_windowedwidth->doublevalue * 0.24f)
#define CONSOLE_FONT_SCALEY			(BASE_WIDTH / (vec_t)vid_windowedwidth->doublevalue * 0.24f)
#define CONSOLE_FONT_NORMAL_R		0.8f
#define CONSOLE_FONT_NORMAL_G		0.8f
#define CONSOLE_FONT_NORMAL_B		0.8f
#define CONSOLE_FONT_NORMAL_A		1.f
#define CONSOLE_FONT_WRAP_EPSILON	1.f
#define CONSOLE_PROMPT				"] "

/*
===================
CL_CMD_ConHistory
===================
*/
void CL_CMD_ConHistory(void)
{
	int i;

	if (host_cmd_argc > 1)
	{
		Sys_Printf("Too many parameters.\n");
		return;
	}

	for (i = 0; i < console_input_history_num - 1; i++) /* cut ourselves out */
		Sys_Printf("%d: %s\n", i, console_input_history[i]);
}

/*
===================
CL_ConsoleInit
===================
*/
void CL_ConsoleInit(void)
{
	int i;

	cl_conback = CL_LoadTexture("menu/conback", true, NULL, 0, 0, false, 1, 1); /* TODO: do not mipmap any 2d stuff */

	console_input[0] = 0;
	for (i = 0; i < MAX_CONSOLE_HISTORY; i++)
		console_input_history[i][0] = 0;
	console_input_history_num = 0;
	console_input_history_current = 0;
	console_input_history_saved_line[0] = 0;
	for (i = 0; i < MAX_CONSOLE_LINES; i++)
		console_buffer[i][0] = 0;
	console_buffer_wrapped = false;
	console_current_new_line = 0;
	console_current_new_line_pos = 0;
	console_scroll_start_line = 0;
	console_y = 0;
	console_opening_timer = 0;
	console_closing_timer = 0;

	cl_conspeed = Host_CMDAddCvar("cl_conspeed", "250", 0);
	cl_conheight = Host_CMDAddCvar("cl_conheight", "0.5", 0);

	Host_CMDAdd("cl_conhistory", CL_CMD_ConHistory);

	console_initialized = true;
	Sys_Printf("Console initialized.\n"); /* TODO: initalize at least the buffer earlier to catch get more log messages */
}

/*
===================
CL_ConsoleShutdown
===================
*/
void CL_ConsoleShutdown(void)
{
	console_initialized = false;
}

/*
===================
CL_ConsoleIsFullScreen
===================
*/
int CL_ConsoleIsFullScreen(void)
{
	if (!cls.ingame && !m_menuopened)
		return true;
	return false;
}

/*
===================
CL_ConsoleDraw
===================
*/
void CL_ConsoleDraw(void)
{
	char line[MAX_LINE];
	vec_t y = console_y;
	vec_t x = 0;
	int i;
	vec_t extra_hack; /* TODO: hack for some more space because of characters that go under writing lines (g, q, etc...) */

	if (!console_initialized)
		return;

	extra_hack = Sys_FontGetStringHeight(CONSOLE_PROMPT, CONSOLE_FONT_SCALEY) / 2;

	Sys_VideoDraw2DPic(&cl_conback->id, cl_conback->width, cl_conback->height, (int)x, (int)(console_y - BASE_HEIGHT + Sys_FontGetStringHeight(CONSOLE_PROMPT, CONSOLE_FONT_SCALEY) + (CL_ConsoleIsFullScreen() ? 0 : extra_hack))); /* TODO: use console_input along with CONSOLE_PROMPT for size calculation */

	if (CL_ConsoleIsFullScreen())
		y -= extra_hack;

	Sys_Snprintf(line, sizeof(line), "%s%s%c", CONSOLE_PROMPT, console_input, (uint64_t)(host_realtime / 250) % 2 ? '|' : ' ');
	Sys_VideoDraw2DText(line, x, y, CONSOLE_FONT_SCALEX, CONSOLE_FONT_SCALEY, CONSOLE_FONT_NORMAL_R, CONSOLE_FONT_NORMAL_G, CONSOLE_FONT_NORMAL_B, CONSOLE_FONT_NORMAL_A);
	y -= Sys_FontGetStringHeight(line, CONSOLE_FONT_SCALEY);

	/* only print finished lines */
	for (i = (console_scroll_start_line - 1 + MAX_CONSOLE_LINES) % MAX_CONSOLE_LINES; i != console_current_new_line; i--, i = (i + MAX_CONSOLE_LINES) % MAX_CONSOLE_LINES) /* must modify i here to be circular, otherwise the stop condition won't catch the modified value! */
	{
		if (y < 0 - Sys_FontGetStringHeight(console_buffer[i], CONSOLE_FONT_SCALEY))
			break;
		Sys_VideoDraw2DText(console_buffer[i], x, y, CONSOLE_FONT_SCALEX, CONSOLE_FONT_SCALEY, CONSOLE_FONT_NORMAL_R, CONSOLE_FONT_NORMAL_G, CONSOLE_FONT_NORMAL_B, CONSOLE_FONT_NORMAL_A);
		y -= Sys_FontGetStringHeight(console_buffer[i], CONSOLE_FONT_SCALEY);
	}
}

/*
===================
CL_ConsoleKey
===================
*/
void CL_ConsoleKey(int keyindex, int down, vec_t analog_rel, vec_t analog_abs)
{
	if (!console_initialized)
		return;
	if (MenuIsLoadingServer() || MenuIsLoadingClient())
		return; /* ignore keypresses while loading - TODO: allow loading cancelation */

	Sys_Printf("CL_ConsoleKey: console opened but not in textediting mode!\n");
}

/*
===================
CL_ConsoleFrame
===================
*/
void CL_ConsoleFrame(void)
{
	if (!console_initialized)
		return;

	/* set sane values TODO: can Game_CL_ConsoleOpen/Close be called before this sanityzing is applied? */
	if (cl_conspeed->doublevalue < 1)
		Host_CMDForceCvarSetValue(cl_conspeed, 1, true);
	if (cl_conspeed->doublevalue > 5000)
		Host_CMDForceCvarSetValue(cl_conspeed, 5000, true);

	/* force fullscren console */
	if (CL_ConsoleIsFullScreen())
		console_y = (vec_t)BASE_HEIGHT - Sys_FontGetStringHeight(CONSOLE_PROMPT, CONSOLE_FONT_SCALEY); /* TODO: use console_input along with CONSOLE_PROMPT for size calculation */
	else
	{
		if (m_consoleopened)
		{
			if (console_opening_timer > 0)
			{
				/* unsigned type, so we can't just check for negative */
				if (host_frametime > console_opening_timer)
					console_opening_timer = 0;
				else
					console_opening_timer -= host_frametime;
				console_y = 1.f - (vec_t)(console_opening_timer) / (vec_t)cl_conspeed->doublevalue;
				console_y *= (vec_t)cl_conheight->doublevalue * (vec_t)BASE_HEIGHT;
			}
			else
			{
				console_y = (vec_t)cl_conheight->doublevalue * (vec_t)BASE_HEIGHT;
			}
		}
		else
		{
			if (console_closing_timer > 0)
			{
				if (host_frametime > console_closing_timer)
					console_closing_timer = 0;
				else
					console_closing_timer -= host_frametime;
				console_y = (vec_t)(console_closing_timer) / (vec_t)cl_conspeed->doublevalue;
				console_y *= (vec_t)cl_conheight->doublevalue * (vec_t)BASE_HEIGHT;
			}
		}
	}

	/* set sane values again TODO: can Game_CL_ConsoleOpen/Close be called before this sanityzing is applied? */
	if (console_opening_timer < 0)
		console_opening_timer = 0;
	if (console_closing_timer < 0)
		console_closing_timer = 0;
}

/*
===================
CL_ConsoleCheckLineWidth

TODO: test this for security bugs
TODO: test for edge cases where it possibly fails
===================
*/
void CL_ConsoleCheckLineWidth(void)
{
	int origend;
	int curstart;
	int curend;
	char save;
	int possible;

	/* empty line */
	if (!console_buffer[console_current_new_line][0])
		return;

again:
	curstart = 0;
	curend = strlen(console_buffer[console_current_new_line]);
	origend = curend;
	possible = false;
	while (curstart < curend)
	{
		vec_t result;
		int mid;

		mid = curstart + (curend - curstart - 1) / 2;

		save = console_buffer[console_current_new_line][mid + 1];
		console_buffer[console_current_new_line][mid + 1] = 0;
		result = Sys_FontGetStringWidth(console_buffer[console_current_new_line], CONSOLE_FONT_SCALEX);
		console_buffer[console_current_new_line][mid + 1] = save;

		/* fits exactly */
		if (result > (vec_t)BASE_WIDTH - CONSOLE_FONT_WRAP_EPSILON && result < (vec_t)BASE_WIDTH + CONSOLE_FONT_WRAP_EPSILON)
		{
			possible = true;
			break;
		}
		else if (result < BASE_WIDTH)
		{
			possible = true;
			curstart = mid + 1;
		}
		else if (result > BASE_WIDTH)
		{
			curend = mid;
		}
	}

	if (!possible)
	{
		Sys_Printf("CL_ConsoleCheckLineWidth: not a single character from the line fits in BASE_WIDTH\n");
		return;
	}

	if (origend != curend) /* character wrap */
	{
		int newlineindex = (console_current_new_line + 1) % MAX_CONSOLE_LINES;

		memmove(console_buffer[newlineindex], console_buffer[console_current_new_line] + curend, origend - curend);
		console_buffer[console_current_new_line][curend] = 0;
		console_buffer[newlineindex][origend - curend] = 0;

		console_current_new_line++;
		if (console_current_new_line == MAX_CONSOLE_LINES)
			console_buffer_wrapped = true;
		console_current_new_line %= MAX_CONSOLE_LINES;
		console_scroll_start_line++;
		console_scroll_start_line %= MAX_CONSOLE_LINES;
		goto again;
	}
}

/*
===================
CL_ConsolePrint
===================
*/
void CL_ConsolePrint(char *text)
{
	/* TODO: handle horizontal overflow in console_input */
	int inputpos = 0;

	if (!console_initialized)
		return;

	if (!text)
		return;

	while (text[inputpos])
	{
		/* TODO: filter input (\r, \t, etc...) */
		if (text[inputpos] == '\n')
		{
			console_buffer[console_current_new_line][console_current_new_line_pos] = 0;
			CL_ConsoleCheckLineWidth();
			console_current_new_line++;
			if (console_current_new_line == MAX_CONSOLE_LINES)
				console_buffer_wrapped = true;
			console_current_new_line %= MAX_CONSOLE_LINES;
			console_scroll_start_line++;
			console_scroll_start_line %= MAX_CONSOLE_LINES;
			console_buffer[console_current_new_line][0] = 0;
			console_current_new_line_pos = 0;
			inputpos++;
			continue;
		}

		if (inputpos >= MAX_LINE - 1)
			Sys_Error("CL_ConsolePrint: inputpos >= MAX_LINE - 1\n");
		if (console_current_new_line_pos >= MAX_LINE - 1)
			Sys_Error("CL_ConsolePrint: console_current_new_line_pos >= MAX_LINE - 1\n");

		console_buffer[console_current_new_line][console_current_new_line_pos++] = text[inputpos++];
	}

	/* finish this line and allow appeding later */
	console_buffer[console_current_new_line][console_current_new_line_pos] = 0;
}

/*
===================
CL_ConsoleEditTextEvent
===================
*/
void Game_CL_ConsoleClose(void);
void CL_ConsoleEditTextEvent(char *text, int text_modified, int confirm, int cancel, char *input_utf8, int key_index)
{
	int i;
	char tmp[MAX_LINE];

	if (confirm)
	{
		/* TODO: do a circular buffer or a linked list to avoid copying input history */
		if (console_input_history_num < MAX_CONSOLE_HISTORY)
			console_input_history_num++;
		else
		{
			for (i = 0; i < console_input_history_num - 1; i++)
			{
				Sys_Snprintf(console_input_history[i], sizeof(console_input_history[i]), "%s", console_input_history[i + 1]);
			}
		}

		Sys_Snprintf(console_input_history[console_input_history_num - 1], sizeof(console_input_history[console_input_history_num - 1]), "%s", text);
		console_input_history_current = console_input_history_num;
		console_input_history_saved_line[0] = 0;

		Host_CMDBufferAdd(text);
		Sys_Printf("%s%s\n", CONSOLE_PROMPT, text);
		text[0] = 0;
	}
	else if (cancel)
	{
		Game_CL_ConsoleClose();
	}
	else if (key_index)
	{
		switch (key_index)
		{
			case KEY_HOME:
				if (console_buffer_wrapped)
				{
					console_scroll_start_line = console_current_new_line + 1;
					console_scroll_start_line %= MAX_CONSOLE_LINES;
				}
				else
					console_scroll_start_line = 0; /* avoid having pages of blank lines in the circular buffer */
				break;
			case KEY_END:
				console_scroll_start_line = console_current_new_line;
				break;
			case KEY_PAGEUP:
				if (console_buffer_wrapped)
				{
					console_scroll_start_line--;
					console_scroll_start_line = (console_scroll_start_line + MAX_CONSOLE_LINES) % MAX_CONSOLE_LINES;
					if (console_scroll_start_line == console_current_new_line) /* circular wrap */
						goto force_down; /* just to avoid a modular -1 comparison */
				}
				else
				{
					if (console_scroll_start_line > 0) /* avoid scrolling up pages of blank lines in the circular buffer */
						console_scroll_start_line--;
				}
				break;
			case KEY_PAGEDOWN:
				if (console_scroll_start_line != console_current_new_line) /* prevent wrap */
				{
force_down:
					console_scroll_start_line++;
					console_scroll_start_line %= MAX_CONSOLE_LINES;
				}
				break;
			case KEY_TAB:
				if (text[0])
				{
					Sys_Snprintf(tmp, sizeof(tmp), "search %s", text);
					Host_CMDBufferAdd(tmp);
				}
				break;
			case KEY_UP:
				if (console_input_history_num)
				{
					/* save the new input */
					if (console_input_history_current == console_input_history_num)
						Sys_Snprintf(console_input_history_saved_line, sizeof(console_input_history_saved_line), text);

					console_input_history_current--;
					if (console_input_history_current < 0)
						console_input_history_current = 0;

					Sys_Snprintf(text, sizeof(console_input), "%s", console_input_history[console_input_history_current]);
				}
				break;
			case KEY_DOWN:
				if (console_input_history_num)
				{
					if (console_input_history_current < console_input_history_num)
					{
						console_input_history_current++;

						if (console_input_history_current == console_input_history_num)
							Sys_Snprintf(text, sizeof(console_input), "%s", console_input_history_saved_line);
						else
							Sys_Snprintf(text, sizeof(console_input), "%s", console_input_history[console_input_history_current]);
					}
				}
				break;
			default:
				break;
		}
	}
}

/*
============================================================================

General

============================================================================
*/

/*
===================
Game_CL_ConsoleClose
===================
*/
void Game_CL_ConsoleClose(void)
{
	if (!console_initialized)
	{
		Sys_Printf("Game_CL_ConsoleClose: Console not initialized.\n");
		return;
	}

	if (!m_consoleopened)
		return;

	console_input_history_current = console_input_history_num;
	console_input_history_saved_line[0] = 0;

	console_input[0] = 0;
	CL_InputSetTextMode(false, NULL, 0, NULL);

	if (!m_menuopened)
		keydest = KEYDEST_GAME;
	m_consoleopened = false;
	if (!CL_ConsoleIsFullScreen())
		console_closing_timer = (mstime_t)floor(cl_conspeed->doublevalue) - console_opening_timer;
	else
		console_closing_timer = 0;
}

/*
===================
Game_CL_ConsoleOpen
===================
*/
void Game_CL_ConsoleOpen(void)
{
	if (!console_initialized)
	{
		Sys_Printf("Game_CL_ConsoleOpen: Console not initialized.\n");
		return;
	}

	if (m_consoleopened)
		return;

	keydest = KEYDEST_MENU;
	m_consoleopened = true;
	console_scroll_start_line = console_current_new_line;
	if (!CL_ConsoleIsFullScreen())
		console_opening_timer = (mstime_t)floor(cl_conspeed->doublevalue) - console_closing_timer;
	else
		console_opening_timer = 0;

	CL_InputSetTextMode(true, console_input, MAX_GAME_STRING, &CL_ConsoleEditTextEvent);
}

/*
===================
CL_CMD_ToggleConsole
===================
*/
void CL_CMD_ToggleConsole(void)
{
	if (!m_consoleopened)
		Game_CL_ConsoleOpen();
	else
		Game_CL_ConsoleClose();
}

/*
===================
CL_CMD_OpenConsole
===================
*/
void CL_CMD_OpenConsole(void)
{
	if (!m_consoleopened)
		Game_CL_ConsoleOpen();
}

/*
===================
CL_CMD_ConDump

TODO: if no parameter, create sequential files (condump0000.txt, condump0001.txt, etc)
TODO: TEST WITH A HEX EDITOR TO SEE IF WE HAVE NO NULL TERMINATORS, REALLY TEST THE CIRCULAR BUFFER, ETC
TODO: using binary file writing functions makes \n not get converted to \n\r under windows
===================
*/
void CL_CMD_ConDump(void)
{
	void *handle;
	int i;

	if (!console_initialized)
	{
		Sys_Printf("Can't dump console if it isn't initialized.\n");
		return;
	}

	if (host_cmd_argc != 2)
	{
		Sys_Printf("Usage: condump \"filename.txt\"\n");
		return;
	}

	Sys_Printf("Dumping console buffer to %s... ", host_cmd_argv[1]);

	handle = Host_FSFileHandleOpenBinaryWrite(host_cmd_argv[1]);
	if (!handle)
	{
		Sys_Printf("Couldn't open file for writing.\n");
		return;
	}

	if (console_buffer_wrapped)
	{
		i = (console_current_new_line + 1) % MAX_CONSOLE_LINES; /* the message from this command is on the current new line, so start from the next */
		while (1)
		{
			Host_FSFileHandleWriteBinary(handle, console_buffer[i], strlen(console_buffer[i])); /* no "+ 1" here to ignore the null terminator */
			Host_FSFileHandleWriteBinary(handle, "\n", strlen("\n"));
			i = (i + 1) % MAX_CONSOLE_LINES;
			if (i == (console_current_new_line + 1) % MAX_CONSOLE_LINES)
				break;
		}
	}
	else
	{
		for (i = 0; i < console_current_new_line; i++)
		{
			Host_FSFileHandleWriteBinary(handle, console_buffer[i], strlen(console_buffer[i])); /* no "+ 1" here to ignore the null terminator */
			Host_FSFileHandleWriteBinary(handle, "\n", strlen("\n"));
		}
	}

	Host_FSFileHandleClose(handle);
	Sys_Printf("Done.\n");
}

/*
===================
Game_CL_MenuClose
===================
*/
void Game_CL_MenuClose(void)
{
	if (!m_menuopened)
		return;

	MenuClose();
	if (!m_consoleopened)
		keydest = KEYDEST_GAME;
	m_menuopened = false;
}

/*
===================
Game_CL_MenuOpen
===================
*/
void Game_CL_MenuOpen(void)
{
	if (m_menuopened)
		return;

	if (!m_consoleopened) /* preserve the console keydest */
		keydest = KEYDEST_MENU;
	m_menuopened = true;
	if (cls.ingame)
		m_menutype_ingame = true;
	else
		m_menutype_ingame = false;
}

/*
===================
Game_CL_MenuTypeInGame
===================
*/
const int Game_CL_MenuTypeInGame(void)
{
	return m_menutype_ingame;
}

/*
===================
CL_CMD_ToggleMenu
===================
*/
void CL_CMD_ToggleMenu(void)
{
	if (!m_menuopened)
	{
		Game_CL_MenuOpen();
		MenuOpen(false);
	}
	else
	{
		Game_CL_MenuClose();
	}
}

/*
===================
CL_CMD_OpenSplashMenu
===================
*/
void CL_CMD_OpenSplashMenu(void)
{
	if (m_menuopened)
		Game_CL_MenuClose();

	Game_CL_MenuOpen();
	MenuOpen(true);
}

/*
===================
Game_CL_MenuInit

TODO: make precaches of the stuff we load here (and EVERYWHERE in the engine) at least precache_file() to create some autocopying system
or maybe just print them after running a game session and then parse
===================
*/
void Game_CL_MenuInit(void)
{
	m_menuopened = false;
	m_consoleopened = false;
	m_menutype_ingame = false;

	CL_ConsoleInit();
	MenuInit();

	Host_CMDAdd("togglemenu", CL_CMD_ToggleMenu);
	Host_CMDAdd("opensplashmenu", CL_CMD_OpenSplashMenu);
	Host_CMDAdd("toggleconsole", CL_CMD_ToggleConsole);
	Host_CMDAdd("openconsole", CL_CMD_OpenConsole);
	Host_CMDAdd("condump", CL_CMD_ConDump);
}

/*
===================
Game_CL_MenuShutdown
===================
*/
void Game_CL_MenuShutdown(void)
{
	m_menuopened = false;
	m_consoleopened = false;

	MenuShutdown();

	CL_ConsoleShutdown();
}

/*
===================
Game_CL_MenuDraw
===================
*/
void Game_CL_MenuDraw(void)
{
	MenuDraw();

	if (m_consoleopened || console_closing_timer || (!m_menuopened && !cls.ingame))
		CL_ConsoleDraw(); /* priority, on top */

	MenuDrawPriority();
}

/*
===================
Game_CL_MenuKey
===================
*/
void Game_CL_MenuKey(int keyindex, int down, vec_t analog_rel, vec_t analog_abs)
{
	if (MenuIsLoadingServer() || MenuIsLoadingClient())
		return; /* ignore keypresses while loading - TODO: allow loading cancelation TODO: this ignores the mouse updates also */

	if (m_consoleopened) /* priority, on top */
		CL_ConsoleKey(keyindex, down, analog_rel, analog_abs);
	else if (m_menuopened)
		MenuKey(keyindex, down, analog_rel, analog_abs);
	else /* TODO: multithreading failure here? (if setting keydest before m_*open in another thread) */
		Sys_Error("Game_CL_MenuKey: called without active menu or console.\n");
}

/*
===================
Game_CL_MenuFrame
===================
*/
void Game_CL_MenuFrame(void)
{
	int error_happened = false;

	if (MenuIsLoadingServer() && svs.listening)
	{
		MenuFinishLoadingServer(false);
	}
	/* if finished loading, close menu */
	if (MenuIsLoadingClient() && cls.ingame)
	{
		MenuFinishLoadingClient();
		Game_CL_ConsoleClose();
		Game_CL_MenuClose();
	}

	/* if the loading flags are still set but we are not loading, bail out on errors */
	if (MenuIsLoadingServer() && !svs.loading && !svs.listening && MenuLoadingServerGetTimeout() < host_realtime)
	{
		MenuFinishLoadingServer(true);
		MenuFinishLoadingClient();
		error_happened = true;
	}
	if (MenuIsLoadingClient() && !cls.connected && !cls.ingame && MenuLoadingClientGetTimeout() < host_realtime)
	{
		MenuFinishLoadingServer(true);
		MenuFinishLoadingClient();
		error_happened = true;
	}
	if (error_happened)
	{
		/* drop to console to show the error */
		if (keydest == KEYDEST_GAME || !m_consoleopened)
			Host_CMDBufferAdd("toggleconsole");
			/* Host_CMDBufferAdd("togglemenu"); TODO: option to drop to console or menu, then display the error there */
	}
	/* hide innapropriate menus for the current connection status */
	if (m_menuopened)
	{
		if (m_menutype_ingame && !cls.ingame)
			Game_CL_MenuClose(); /* not ingame anymore, so this menu doesn't make sense */
		else if (!m_menutype_ingame && cls.ingame)
			Game_CL_MenuClose(); /* not disconnected anymore, so this menu doesn't make sense */
	}

	MenuFrame();
	CL_ConsoleFrame();
}

/*
===================
Game_CL_MenuConsolePrint
===================
*/
void Game_CL_MenuConsolePrint(char *text)
{
	CL_ConsolePrint(text);
}
