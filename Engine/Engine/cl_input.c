/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Generic digital key/button and analog input mapping

State tracking

============================================================================
*/

/* key 0 is invalid, so digital keys are [1 - 495] */
#define MAX_KEYS			496
/* analog inputs are [496-511] */
#define FIRST_ANALOG_INPUT	496
#define MAX_ANALOG_INPUTS	16

typedef struct keystate_s {
	/* 0 = released, 1 = pressed */
	int digital_pressed;
	/* frame in which the key was pressed */
	framenum_t frame;
	/*
		if the key was released on the same frame in which it was pressed,
		only release it on the next frame. This technique won't capture those
		moments when you press and release the key various times at warp speeds!
		Network lag would also contribute to this, anyway :-)
	*/
	int digital_release;

	/*
		for analog inputs, we only keep track of the last frame in which we had a relative motion, then send
		a "null" update with relative motion zero if we didn't update this input in the next frame (to
		effectively cancel the last action). This requires that we keep the absolute position for re-sending
	*/
	vec_t analog_abs;
	int analog_release; /* send a rel == zero if true */
} keystate_t;

static keystate_t keystates[MAX_KEYS + MAX_ANALOG_INPUTS];

/*
============================================================================

Key/Button binding

Key indices are not guaranteed to be the same from platform to platform

For digital keys, if a bind command starts with '+', the same command will
be executed when the key is released, but with a '-' instead of the '+'.

For analog inputs, the relative motion since the last call is passed as
an vec_t in the first argument for the binded command. The absolute position
is passed as the second argument as an vec_t too, normalized from -1 to 1.
The relative motion is also normalized in relation to the absolute motion (this
does not mean that you can add the relative motion to the old absolute position
and get the new absolute position, because a mouse, for example, will still
have an relative motion but will not change its absolute position if we try
to move it past the screen). Be aware that some inputs (like a mouse, again)
can have the absolute position between 0 and 1 instead of between -1 and 1.
When we don't change the relative motion in a new frame, we send an event
with the relative motion of zero and the old absolute position if it wasn't
already sent by the system.

TODO: support + and - commands for analog inputs? support calling digital bind commands only
after a treshold, suport deadzones in the engine or let it to the driver?

With joysticks/gamepads, it's best to use the absolute position for game input
(aiming, for example) and the relative motion for cursor movement (a mouse
cursor).
With a mouse, it's the reverse: it's best to use the relative motion for game
input and the absolute position for cursor movement.

TODO: do we reverse their order before passing to the game or let
the game decide how to use them? THIS IS A MAJOR PROBLEM! FIXME! Make absolute
== relative for joysticks/gamepads?

============================================================================
*/

#define MAX_BIND_LINE		128

static char keybindings[MAX_KEYS + MAX_ANALOG_INPUTS][MAX_BIND_LINE];

/*
===================
CL_CMD_BindList
===================
*/
void CL_CMD_BindList(void)
{
	int i, j;

	Sys_Printf("Key/axis binding list\n---------\n");

	for(i = 1, j = 0; i < MAX_KEYS + MAX_ANALOG_INPUTS; i++)
	{
		if (keybindings[i][0])
		{
			Sys_Printf("Key/axis \"%s\" bound to \"%s\"\n", Sys_KeyIndexToKeyName(i), keybindings[i]);
			j++;
		}
	}

	Sys_Printf("---------\nEnd of key/axis binding list\n%d binds(s) found\n", j);
}

/*
===================
CL_CMD_Bind
===================
*/
void CL_CMD_Bind(void)
{
	int keyval;

	if (host_cmd_argc < 2)
	{
		Sys_Printf("Too few arguments for \"bind\"\n");
	}
	else if (host_cmd_argc > 3)
	{
		Sys_Printf("Too many arguments for \"bind\"\n");
	}
	else
	{
		keyval = Sys_KeyNameToKeyIndex(host_cmd_argv[1]);
		if (keyval <= 0 || keyval >= MAX_KEYS + MAX_ANALOG_INPUTS)
		{
			Sys_Printf("Invalid key/axis index specified for \"bind\", \"%d\" is out of range\n", keyval);
			return;
		}

		if (host_cmd_argc == 2)
			Sys_Printf("\"%s\" is bound to \"%s\"\n", host_cmd_argv[1], keybindings[keyval]);
		else
		{

			if (strlen(host_cmd_argv[2]) + 1 >= MAX_BIND_LINE)
			{
				Sys_Printf("Error: \"%s\" is too long for a bind command\n", host_cmd_argv[2]);
				return;
			}

			if (!Host_CMDValidateAlphanumericValue(host_cmd_argv[2]))
			{
				Sys_Printf("Error: \"%s\" contains invalid characters\n", host_cmd_argv[2]);
				return;
			}

			Sys_Snprintf(keybindings[keyval], sizeof(keybindings[keyval]), "%s", host_cmd_argv[2]);
		}
	}
}

/*
===================
CL_CMD_SaveBindsConfig
===================
*/
#define MAX_BINDCONFIG_SIZE					65536
#define BINDCONFIG_FILENAME					"keybindings.cfg"
#define DEFAULTBINDCONFIG_FILENAME			"keybindings_default.cfg"

void CL_CMD_SaveBindsConfig(void)
{
	int i, marker, size = 0;
	unsigned char *config;
	char line[MAX_BIND_LINE + 32]; /* to guarantee space for the cmd, key index, space and \n, overkill */

	marker = Sys_MemLowMark(&tmp_mem);
	config = Sys_MemAlloc(&tmp_mem, MAX_BINDCONFIG_SIZE, "savedbindconfig");
	config[0] = 0;
	Sys_Snprintf(line, sizeof(line), "// this file is generated automatically, any edit may be lost\n");
	Sys_Strncat(config, line, MAX_BINDCONFIG_SIZE - strlen(config));
	for (i = 1; i < MAX_KEYS + MAX_ANALOG_INPUTS; i++)
	{
		if (keybindings[i][0])
		{
			Sys_Snprintf(line, sizeof(line), "bind %s \"%s\"\n", Sys_KeyIndexToKeyName(i), keybindings[i]);
			Sys_Strncat(config, line, MAX_BINDCONFIG_SIZE - strlen(config));
		}
	}

	size = strlen(config);
	if (Host_FSWriteBinaryFile(BINDCONFIG_FILENAME, config, size) == size)
		Sys_Printf("Bind config saved.\n");
	else
		Sys_Printf("Error saving bind config file.\n");

	Sys_MemFreeToLowMark(&tmp_mem, marker);
}

/*
===================
CL_CMD_LoadDefaultBindsConfig
===================
*/
void CL_CMD_LoadDefaultBindsConfig(void)
{
	char lcmd[MAX_LINE];

	Sys_Snprintf(lcmd, sizeof(lcmd), "exec %s", DEFAULTBINDCONFIG_FILENAME);
	Host_CMDBufferAdd(lcmd);
}

/*
===================
CL_CMD_LoadBindsConfig
===================
*/
void CL_CMD_LoadBindsConfig(void)
{
	char lcmd[MAX_LINE];

	Sys_Snprintf(lcmd, sizeof(lcmd), "exec %s", BINDCONFIG_FILENAME);
	Host_CMDBufferAdd(lcmd);
}

/*
============================================================================

Main input routines

============================================================================
*/

/* the destination for the text, may already contain text, set when enabling text input TODO FIXME not reentrant, splitscreen, etc */
static char *textinput_dest;
static size_t textinput_maxsize = 0;
void (*textinput_callback)(char *text, int text_modified, int confirm, int cancel, char *input_utf8, int key_index);

/*
===================
CL_InputProcessText

TODO: better editing (left, right, home, end, etc)
===================
*/
void CL_InputProcessText(char *input_utf8, int key_index)
{
	if (!cls.active)
		return;

	if (input_utf8 && key_index)
		Sys_Error("CL_InputProcessText: text input and key press set at the same time! %s and %d\n", input_utf8, key_index);

	if (keydest != KEYDEST_TEXT)
	{
		Sys_Printf("WARNING: Not KEYDEST_TEXT but called CL_InputProcessText\n"); /* FIXME */
		return;
	}

	if (input_utf8)
	{
		Sys_Strncat(textinput_dest, input_utf8, textinput_maxsize - strlen(textinput_dest));
		textinput_callback(textinput_dest, true, false, false, input_utf8, key_index);
	}
	
	/* CL_InputProcessKeyUpDown doesn't get called when we are editing text */
	switch (key_index)
	{
		/* TODO: condense these and just pass the key_index? */
		case KEY_ESC:
			textinput_callback(textinput_dest, false, false, true, input_utf8, key_index);
			break;
		case KEY_RETURN:
			textinput_callback(textinput_dest, false, true, false, input_utf8, key_index);
			break;
		case KEY_BACKSPACE:
			if (strlen(textinput_dest))
			{
				textinput_dest[strlen(textinput_dest) - 1] = 0;
				textinput_callback(textinput_dest, true, false, false, input_utf8, key_index);
			}
			break;
		default:
			textinput_callback(textinput_dest, false, false, false, input_utf8, key_index);
			break;
	}
}

/*
===================
CL_InputProcessKeyUpDown

down == 0 -> key released
down == 1 -> key pressed
down == -1 -> analog input

TODO: clear all key inputs when changing windows, keydest, etc, to prevent stuck keys
===================
*/
void CL_InputProcessKeyUpDown(int keyindex, int down, vec_t analog_rel, vec_t analog_abs)
{
	char upcmd[MAX_LINE];

	if (!cls.active)
		return;

	if (keydest == KEYDEST_TEXT)
	{
		Sys_Printf("WARNING: KEYDEST_TEXT but called CL_InputProcessKeyUpDown\n"); /* FIXME */
		return;
	}

	/* validate input */
	if ((keyindex <= 0 || keyindex >= MAX_KEYS) && (down == 0 || down == 1))
	{
		Sys_Printf("Key index \"%d\" is out of range.\n", keyindex);
		return;
	}
	if ((keyindex < MAX_KEYS || keyindex >= MAX_KEYS + MAX_ANALOG_INPUTS) && down == -1)
	{
		Sys_Printf("Axis index \"%d\" is out of range.\n", keyindex);
		return;
	}

	/* commenting this out as window management may make us lose some events. Hopefully no problems arise.
	if (keystates[keyindex].digital_pressed && down == 1)
	{
		Sys_Printf("Warning: pressing already pressed key %d\n", keyindex);
		return;
	}
	else if (!keystates[keyindex].digital_pressed && down == 0)
	{
		Sys_Printf("Warning: releasing not pressed key %d\n", keyindex);
		return;
	} */

	/* handle key state and releasing on the same frame as pressing, to make sure each key spends at least 1 frame being pressed */
	if (down == 1)
	{
		keystates[keyindex].digital_pressed = true;
		keystates[keyindex].frame = host_framecount;
		keystates[keyindex].digital_release = false;
	}
	else if (down == 0)
	{
		if (keystates[keyindex].digital_release == false && keystates[keyindex].frame == host_framecount)
		{
			keystates[keyindex].digital_release = true; /* unpressed on the same frame, delay the release! */
			return;
		}

		keystates[keyindex].digital_pressed = false;
	}
	else if (down == -1)
	{
		if (analog_rel)
		{
			keystates[keyindex].frame = host_framecount;
			keystates[keyindex].analog_abs = analog_abs;
			keystates[keyindex].analog_release = true;
		}
		else
		{
			/* this will also make it compatible with the system sending an zero relative motion when the movement stops */
			keystates[keyindex].analog_release = false;
		}
	}

	/* Proccess input */
	switch (keydest)
	{
		case KEYDEST_INVALID:
			Sys_Error("Invalid keydest!\n");
			break;
		case KEYDEST_MENU:
			/* handle menu */
			CL_MenuKey(keyindex,  down, analog_rel, analog_abs);
			break;
		case KEYDEST_GAME:
			/* handle in-game bindings */
			if (keybindings[keyindex][0])
			{
				if (down == 1)
				{
					Host_CMDBufferAdd(keybindings[keyindex]);
				}
				else if (down == 0)
				{
					if (keybindings[keyindex][0] == '+')
					{
						Sys_Snprintf(upcmd, sizeof(upcmd), "%s", keybindings[keyindex]);
						upcmd[0] = '-';
						Host_CMDBufferAdd(upcmd);
					}
				}
				else if (down == -1)
				{
					/* reusing upcmd here */
					Sys_Snprintf(upcmd, sizeof(upcmd), "%s %f %f", keybindings[keyindex], analog_rel, analog_abs);
					Host_CMDBufferAdd(upcmd);
				}
			}
			else
			{
				if (down == 1)
					Sys_Printf("Unbound key %d (%s) pressed\n", keyindex, Sys_KeyIndexToKeyName(keyindex));
				if (down == -1)
					Sys_Printf("Unbound axis %d (%s) (%f, %f) moved\n", keyindex, Sys_KeyIndexToKeyName(keyindex), analog_rel, analog_abs);
			}
			break;
		default:
			Sys_Printf("Unknown keydest\n");
			break;
	}
}

/*
===================
CL_InputBindFromKey

Returns the bind command associated with a key
TODO: will fail when a bind is multiple commands (once that's implemented) or has arguments
TODO: too slow to call every frame, use hashes or similar
===================
*/
char *CL_InputBindFromKey(int keyindex)
{
	if ((keyindex <= 0 || keyindex >= MAX_KEYS) && (keyindex < MAX_KEYS || keyindex >= MAX_KEYS + MAX_ANALOG_INPUTS))
	{
		Sys_Printf("Key/Axis index \"%d\" is out of range.\n", keyindex);
		return NULL;
	}

	return keybindings[keyindex];
}

/*
===================
CL_InputPostFrame

TODO: these won't get caught by the right keydest if we change keydest mid-frame
===================
*/
void CL_InputPostFrame(void)
{
	/* TODO: release anyway to prevent stuck keys when opening console? remove warning message in CL_InputProcessKeyUpDown */
	if (keydest != KEYDEST_TEXT)
	{
		int i;

		/* Check delayed releases, but do not check for analog inputs, only up to MAX_KEYS */
		for (i = 0; i < MAX_KEYS; i++)
			if (keystates[i].digital_release)
				CL_InputProcessKeyUpDown(i, 0, 0, 0);

		/* Check analog relative motion stopping */
		for (i = FIRST_ANALOG_INPUT; i < FIRST_ANALOG_INPUT + MAX_ANALOG_INPUTS; i++)
			if (keystates[i].frame == host_framecount - 1 && keystates[i].analog_release)
				CL_InputProcessKeyUpDown(i, -1, 0, keystates[i].analog_abs);
	}
}

/*
===================
CL_InputSetTextMode

If textmode is true, set keydest to KEYDEST_TEXT, otherwise
restores the previous value.
===================
*/
void CL_InputSetTextMode(int textmode, char *existing_text, size_t text_maxsize, void (*text_callback)(char *text, int text_modified, int confirm, int cancel, char *input_utf8, int key_index))
{
	static int oldkeydest = KEYDEST_INVALID;

	if (text_maxsize >= MAX_LINE)
		Sys_Error("CL_InputSetTextMode: text_maxsize (%d) >= MAX_LINE (%d)\n", text_maxsize, MAX_LINE);

	textinput_dest = existing_text;
	textinput_maxsize = text_maxsize;
	textinput_callback = text_callback;

	if (textmode)
	{
		if (!existing_text || !text_maxsize || !text_callback)
			Sys_Error("CL_InputSetTextMode: trying to enable with existing_text == NULL, text_maxsize == 0 or text_callback == NULL\n");

		oldkeydest = keydest;
		keydest = KEYDEST_TEXT;
		Sys_InputSetTextMode(true);
	}
	else
	{
		if (existing_text || text_maxsize || text_callback)
			Sys_Error("CL_InputSetTextMode: to disable text input, call with existing_text == NULL, text_maxsize == 0 and text_callback == NULL\n");

		keydest = oldkeydest;
		Sys_InputSetTextMode(false);
	}
}

/*
===================
CL_InputInit
===================
*/
void CL_InputInit(void)
{
	int i;

	CL_InputSetTextMode(false, NULL, 0, NULL); /* will set keydest to KEYDEST_INVALID */

	/* clear key states and bindings */
	for (i = 0; i < MAX_KEYS; i++)
	{
		keybindings[i][0] = 0;
		keystates[i].analog_abs = 0;
		keystates[i].analog_release = false;
		keystates[i].digital_pressed = false;
		keystates[i].digital_release = false;
		keystates[i].frame = 0;
	}

	Host_CMDAdd("bindlist", CL_CMD_BindList);
	Host_CMDAdd("bind", CL_CMD_Bind);
	Host_CMDAdd("savebindsconfig", CL_CMD_SaveBindsConfig);
	Host_CMDAdd("loaddefaultbindsconfig", CL_CMD_LoadDefaultBindsConfig);
	Host_CMDAdd("loadbindsconfig", CL_CMD_LoadBindsConfig);

	Game_CL_InputInit();

	/* load defaults here too, in case the saved config ins't available */
	Host_CMDBufferAdd("loaddefaultbindsconfig");
	Host_CMDBufferAdd("loadbindsconfig");
	Host_CMDBufferExecute(); /* no problem, we're in an init function */
}

/*
===================
CL_InputShutdown
===================
*/
void CL_InputShutdown(void)
{
	Host_CMDBufferAdd("savebindsconfig");
	Host_CMDBufferExecute(); /* no problem, we're in a shutdown function */

	Game_CL_InputShutdown();
}
