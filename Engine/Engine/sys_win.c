/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

#ifdef __WINDOWS__
#include <windows.h>
#endif /* __WINDOWS__ */
#include <time.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <SDL.h>

/*
============================================================================

System replacement functions

============================================================================
*/

#ifdef __GNUC__  /* TODO FIXME: SECURITY HAZARD */

/*
===================
fopen_s
===================
*/
#include <errno.h>
int fopen_s(FILE **handle, const char *path, const char *mode)
{
	*handle = fopen(path, mode);
	if (*handle)
		return 0;

        return errno;
}

/*
===================
Sys_Vsnprintf
===================
*/
int Sys_Vsnprintf(char *buffer, size_t buffersize, const char *format, va_list args)
{
	int result;

	result = vsnprintf(buffer, buffersize, format, args);

	if (result < 0 || (size_t)result >= buffersize)
	{
		buffer[buffersize - 1] = '\0';
		return -1;
	}

	return result;
}

/*
===================
Sys_Strncat

TODO: fix this len stuff
===================
*/
char *Sys_Strncat(char *dest, const char *source, size_t count)
{
	size_t len = count + strlen(dest);

	strncat(dest, source, len);

	return dest;
}

#else

/*
===================
Sys_Vsnprintf
===================
*/
int Sys_Vsnprintf(char *buffer, size_t buffersize, const char *format, va_list args)
{
	int result;

	result = _vsnprintf_s(buffer, buffersize, _TRUNCATE, format, args);

	if (result < 0 || (size_t)result >= buffersize)
	{
		buffer[buffersize - 1] = '\0';
		return -1;
	}

	return result;
}

/*
===================
Sys_Strncat

TODO: fix this len stuff
===================
*/
char *Sys_Strncat(char *dest, const char *source, size_t count)
{
	size_t len = count + strlen(dest);

	strncat_s(dest, len, source, _TRUNCATE);

	return dest;
}

#endif /* __GNUC__ */

/*
===================
Sys_Snprintf
===================
*/
int Sys_Snprintf(char *buffer, size_t buffersize, const char *format, ...)
{
	va_list args;
	int result;

	va_start(args, format);
	result = Sys_Vsnprintf(buffer, buffersize, format, args);
	va_end(args);

	return result;
}

#if 0 /* linear memory pools */
/*
============================================================================

Memory allocation subsystem, based on the Quake system

std_mem should be used for permanent data!

TODO: search for memory leaks
TODO: option to not zero-fill, for faster allocations

============================================================================
*/

#define MEMBLOCK_SENTINEL		0xdeadbeef
#define MEMBLOCK_MAX_NAME		32

typedef struct memblock_s
{
	int		sentinel;
	int		size;
	char	name[MEMBLOCK_MAX_NAME];
} memblock_t;

/* TODO: lots of unnecessary data being kept on memory after loading is done... */

/* TODO: anything not allocated here (mainly in libraries) will probably segfault if out of memory! Happens a lot with huge voxel worlds */
/* host/shared data */
memblock_type_t std_mem = {NULL, 64 * 1024 * 1024, 0, 0}; /* Alloc 64MB of virtual memory for misc permanent game/engine data */
memblock_type_t tmp_mem = {NULL, 64 * 1024 * 1024, 0, 0}; /* Alloc 64MB of virtual memory for scratch pad data*/
memblock_type_t mdl_mem = {NULL, 768 * 1024 * 1024, 0, 0}; /* Alloc 768MB of virtual memory for model data */
/* client data */
memblock_type_t snd_mem = {NULL, 256 * 1024 * 1024, 0, 0}; /* Alloc 256MB of virtual memory for sound data */
/* server data */
memblock_type_t svr_mem = {NULL, 128 * 1024 * 1024, 0, 0}; /* Alloc 128MB of virtual memory for server data */

/*
===================
Sys_MemAlloc
===================
*/
void *Sys_MemAlloc (memblock_type_t *memblock, int size, char *name)
{
	memblock_t *mempointer;

	if (strlen(name) > MEMBLOCK_MAX_NAME - 1)
		Sys_Error("Sys_MemAlloc: name too big: %s\n", name);

	if (!memblock->membuffer)
	{
		memblock->membuffer = VirtualAlloc (NULL, memblock->maxmb, MEM_RESERVE, PAGE_NOACCESS);
		memblock->lowmark = 0;
		memblock->used = 0;
		if (!memblock->membuffer)
			Sys_Error("Sys_MemAlloc: couldn't allocate membuffer with size %d for: %s\n", size, name);
	}

	size = sizeof(memblock_t) + ((size + 15) & ~15);

	/* TODO CONSOLEDEBUG
	Sys_Printf("Allocating %d (lowmark %d and maxsize %d) with name %s\n", size, memblock->lowmark, memblock->maxmb, name);
	*/

	if (memblock->lowmark + size >= memblock->maxmb)
		Sys_Error("Sys_MemAlloc: out of memory in membuffer while allocating %s\n", name);

	while (memblock->lowmark + size >= memblock->used)
	{
		VirtualAlloc (memblock->membuffer + memblock->used, 65536, MEM_COMMIT, PAGE_READWRITE);
		memblock->used += 65536;
	}

	mempointer = (memblock_t *) (memblock->membuffer + memblock->lowmark);
	memblock->lowmark += size;

	memcpy (mempointer->name, name, MEMBLOCK_MAX_NAME);
	mempointer->sentinel = MEMBLOCK_SENTINEL;
	mempointer->size = size;

	/* this is important, lots of parts of the code assume zero-filled allocations */
	memset((mempointer + 1), 0, size - sizeof (memblock_t));
	return (mempointer + 1);
}

/*
===================
Sys_MemFreeToLowMark
===================
*/
void Sys_MemFreeToLowMark (memblock_type_t *memblock, int mark)
{
	memset (memblock->membuffer + mark, 0, memblock->used - mark);
	memblock->lowmark = mark;
}

/*
===================
Sys_MemLowMark
===================
*/
int Sys_MemLowMark (memblock_type_t *memblock)
{
	return memblock->lowmark;
}

/*
===================
Sys_MemInit
===================
*/
void Sys_MemInit(void)
{
}

/*
===================
Sys_MemShutdown
===================
*/
void Sys_MemShutdown(void)
{
}
#else

/*
============================================================================

Stack-organized memory allocation subsystem

std_mem should be used for permanent data!

TODO: search for memory leaks

============================================================================
*/

static int mem_inited = false;

#define MEMBLOCK_SENTINEL		0xdeadbeef

/* TODO: lots of unnecessary data being kept on memory after loading is done... */

/* TODO: anything not allocated here (mainly in libraries) will probably segfault if out of memory! Happens a lot with huge voxel worlds */
/* host/shared data */
mempool_t std_mem; /* misc permanent game/engine data */
mempool_t tmp_mem; /* scratch pad data*/
mempool_t mdl_mem; /* model data */
/* client data */
mempool_t snd_mem; /* sound data */
/* server data */
mempool_t svr_mem; /* server data */

/*
===================
Sys_MemAlloc

TODO: ALIGNED ALLOCATION! (take care because the header (memblock_t) will be aligned, so add padding)
===================
*/
void *Sys_MemAlloc (mempool_t *mempool, int size, char *name)
{
	memblock_t *memblockpointer;
	unsigned char *bytepointer;

	if (!mem_inited)
		Sys_Error("Sys_MemAlloc: memory subsystem not initialized.\n");

	if (strlen(name) > MEMBLOCK_MAX_NAME - 1)
		Sys_Error("Sys_MemAlloc: name too big: %s\n", name);

	if (mempool->blocks_num == MEMPOOL_MAX_MEMBLOCKS)
		Sys_Error("Sys_MemAlloc: too many blocks allocated error while allocating \"%s\".\n", name);

	/* TODO CONSOLEDEBUG
	Sys_Printf("Allocating %d (numblocks %d) with name %s\n", size, mempool->blocks_num, name);
	*/

	/* + header + sentinel at tail */
	memblockpointer = malloc(size + sizeof(memblock_t) + sizeof(int));
	if (!memblockpointer)
		Sys_Error("Sys_MemAlloc: couldn't allocate memory with size %d for: %s\n", size, name);
	bytepointer = (unsigned char *)memblockpointer;

	/* this is important, lots of parts of the code assume zero-filled allocations */
	memset(bytepointer + sizeof(memblock_t), 0, size);

	memcpy (memblockpointer->name, name, MEMBLOCK_MAX_NAME);
	memblockpointer->sentinel = MEMBLOCK_SENTINEL;
	memblockpointer->size = size;

	*(int *)(bytepointer + sizeof(memblock_t) + size) = MEMBLOCK_SENTINEL;

	mempool->blocks[mempool->blocks_num] = memblockpointer;
	mempool->blocks_num++;

	/* use cast to unsigned char to jump BYTES with sizeof */
	return bytepointer + sizeof(memblock_t);
}

/*
===================
Sys_MemFreeToLowMark
===================
*/
void Sys_MemFreeToLowMark (mempool_t *mempool, int mark)
{
	if (!mem_inited)
		Sys_Error("Sys_MemFreeToLowMark: memory subsystem not initialized.\n");

	if (mark < 0)
		Sys_Error("Sys_MemFreeToLowMark: mark < 0\n");

	while (mempool->blocks_num != mark)
	{
		if (mempool->blocks[mempool->blocks_num - 1]->sentinel != MEMBLOCK_SENTINEL)
			Sys_Error("Sys_MemFreeToLowMark: memory subsystem corruption at header - bug detected.\n");
		if (*(int *)((unsigned char *)mempool->blocks[mempool->blocks_num - 1] + sizeof(memblock_t) + mempool->blocks[mempool->blocks_num - 1]->size) != MEMBLOCK_SENTINEL)
			Sys_Error("Sys_MemFreeToLowMark: memory subsystem corruption at tail - bug detected.\n");
		free(mempool->blocks[mempool->blocks_num - 1]);
		mempool->blocks_num--;
	}
}

/*
===================
Sys_MemLowMark
===================
*/
int Sys_MemLowMark (mempool_t *mempool)
{
	if (!mem_inited)
		Sys_Error("Sys_MemLowMark: memory subsystem not initialized.\n");

	return mempool->blocks_num;
}

/*
===================
Sys_MemInit
===================
*/
void Sys_MemInit(void)
{
	if (mem_inited)
		Sys_Error("Sys_MemInit: memory subsystem already initialized.\n");

	memset(&std_mem, 0, sizeof(std_mem));
	memset(&tmp_mem, 0, sizeof(tmp_mem));
	memset(&mdl_mem, 0, sizeof(mdl_mem));
	memset(&snd_mem, 0, sizeof(snd_mem));
	memset(&svr_mem, 0, sizeof(svr_mem));

	mem_inited = true;
}

/*
===================
Sys_MemShutdown
===================
*/
void Sys_MemShutdown(void)
{
	if (!mem_inited)
		Sys_Error("Sys_MemShutdown: memory subsystem not initialized.\n");

	Sys_MemFreeToLowMark(&std_mem, 0);
	Sys_MemFreeToLowMark(&tmp_mem, 0);
	Sys_MemFreeToLowMark(&mdl_mem, 0);
	Sys_MemFreeToLowMark(&snd_mem, 0);
	Sys_MemFreeToLowMark(&svr_mem, 0);

	mem_inited = false;
}
#endif

/*
============================================================================

Filesystem related system functions


TODO: use size_t instead of ints for big files, but take into account our
usage of -1 as failure indication

============================================================================
*/

/*
===================
Sys_FileExists

Returns -1 if file doesn't exist, filesize otherwise.
===================
*/
int Sys_FileExists(char *path)
{
#ifdef __GNUC__
	struct stat buf;

	if(stat(path, &buf))
#else
	struct _stat buf;

	if(_stat(path, &buf))
#endif /* __GNUC__ */
		return -1;

	return buf.st_size;
}

/*
===================
Sys_FileReadBinary

Reads "count" bytes, starting from from "start" (inclusive), from file in "path" to "buffer"
Make sure that the bytes read will fit into buffer before calling
Returns -1 on failure, read bytes otherwise.
===================
*/
int Sys_FileReadBinary(char *path, unsigned char *buffer, int start, int count)
{
	int size;
	FILE *f;

	if (fopen_s(&f, path, "rb"))
		return -1;

	if (fseek(f, start, SEEK_SET))
	{
		fclose(f);
		return -1;
	}

	size = fread(buffer, sizeof(unsigned char), count, f);

	if (size != count)
		Sys_Printf("Warning: wanted to read %d bytes from \"%s\", starting at byte %d, but only read %d bytes\n", count, path, start, size);

	fclose(f);

	return size;
}

/*
===================
Sys_FileWriteBinary

Writes "count" bytes from buffer to the file specified by "path"
Returns -1 on failure, written bytes otherwise.
===================
*/
int Sys_FileWriteBinary(char *path, const unsigned char *buffer, int count)
{
	int size;
	FILE *f;

	if (fopen_s(&f, path, "wb"))
		return -1;

	if (fseek(f, 0, SEEK_SET)) /* redundant but... oh, well! */
	{
		fclose(f);
		return -1;
	}

	size = fwrite(buffer, sizeof(unsigned char), count, f);

	if (size != count)
		Sys_Printf("Warning: wanted to write %d bytes to \"%s\" but only wrote %d bytes\n", count, path, size);

	fclose(f);

	return size;
}

/*
===================
Sys_FileHandleOpenRead

Opens a file handle for interactive reading at the specific byte
Returns NULL on failure, file handle otherwise.
===================
*/
void *Sys_FileHandleOpenRead(char *path, int start)
{
	FILE *f;

	if (fopen_s(&f, path, "rb"))
		return NULL;

	if (fseek(f, start, SEEK_SET))
	{
		fclose(f);
		return NULL;
	}

	return f;
}

/*
===================
Sys_FileHandleOpenWrite

Opens a file handle for interactive writing
Returns NULL on failure, file handle otherwise.
===================
*/
void *Sys_FileHandleOpenWrite(char *path)
{
	FILE *f;

	if (fopen_s(&f, path, "wb"))
		return NULL;

	if (fseek(f, 0, SEEK_SET)) /* redundant but... oh, well! */
	{
		fclose(f);
		return NULL;
	}

	return f;
}

/*
===================
Sys_FileHandleClose

Closes a file handle
===================
*/
void Sys_FileHandleClose(void *handle)
{
	FILE *f = handle;

	if (!handle)
		Sys_Error("Sys_FileHandleClose: NULL handle\n");

	fclose(f);
}

/*
===================
Sys_FileHandleReadBinary

Reads "count" bytes from a file handle to "buffer"
Make sure that the bytes read will fit into buffer before calling
Returns read bytes.
===================
*/
int Sys_FileHandleReadBinary(void *handle, unsigned char *buffer, int count)
{
	int size;
	FILE *f = handle;

	if (!handle)
		Sys_Error("Sys_FileHandleReadBinary: NULL handle\n");

	size = fread(buffer, sizeof(unsigned char), count, f);

	if (size != count)
		Sys_Printf("Warning: wanted to read %d bytes from file but only read %d bytes\n", count, size); /* TODO: filename */

	return size;
}

/*
===================
Sys_FileHandleWriteBinary

Writes "count" bytes from "buffer" to the file handle
Returns written bytes.
===================
*/
int Sys_FileHandleWriteBinary(void *handle, const unsigned char *buffer, int count)
{
	int size;
	FILE *f = handle;

	if (!handle)
		Sys_Error("Sys_FileHandleWriteBinary: NULL handle\n");

	size = fwrite(buffer, sizeof(unsigned char), count, f);

	if (size != count)
		Sys_Printf("Warning: wanted to write %d bytes to file but only wrote %d bytes\n", count, size); /* TODO: filename */

	return size;
}

/*
============================================================================

Timer-related functions

============================================================================
*/

#if 0
/*
===================
Sys_Time

Returns time in milliseconds. Should wrap around every ~49 days, be careful!

FIXME: 64-bit high-resolution time in milliseconds
===================
*/
mstime_t Sys_Time(void)
{
	/* No wrap around checking here, SDL returns value since SDL initialization, not the OS */
	/* TODO: SDL_GetPerformanceCounter? Is it platform specific? */
	return SDL_GetTicks();
}
#endif

/*
===================
Sys_Time

Returns time in milliseconds. TODO: when will wrapping occur? will be different depending on system, so calculate!
===================
*/
mstime_t Sys_Time(void)
{
	/* No wrap around checking here */
	/* TODO: any quirks? */
	return (mstime_t)SDL_GetPerformanceCounter() / ((mstime_t)SDL_GetPerformanceFrequency() / 1000.);
}

/*
============================================================================

Event handling functions

============================================================================
*/

/*
===================
Sys_KeyIndexToKeyName

Converts from the engine keyindex to a key name
TODO: use hashes
===================
*/
const char *Sys_KeyIndexToKeyName(int index)
{
	switch (index)
	{
		case KEY_INVALID:
			return "INVALID";
		case KEY_BACKSPACE:
			return "BACKSPACE";
		case KEY_TAB:
			return "TAB";
		case KEY_RETURN:
			return "RETURN";
		case KEY_ESC:
			return "ESC";
		case KEY_SPACE:
			return "SPACE";
		case KEY_QUOTE:
			return "QUOTE";
		case KEY_0:
			return "0";
		case KEY_1:
			return "1";
		case KEY_2:
			return "2";
		case KEY_3:
			return "3";
		case KEY_4:
			return "4";
		case KEY_5:
			return "5";
		case KEY_6:
			return "6";
		case KEY_7:
			return "7";
		case KEY_8:
			return "8";
		case KEY_9:
			return "9";
		case KEY_BACKQUOTE:
			return "BACKQUOTE";
		case KEY_A:
			return "A";
		case KEY_B:
			return "B";
		case KEY_C:
			return "C";
		case KEY_D:
			return "D";
		case KEY_E:
			return "E";
		case KEY_F:
			return "F";
		case KEY_G:
			return "G";
		case KEY_H:
			return "H";
		case KEY_I:
			return "I";
		case KEY_J:
			return "J";
		case KEY_K:
			return "K";
		case KEY_L:
			return "L";
		case KEY_M:
			return "M";
		case KEY_N:
			return "N";
		case KEY_O:
			return "O";
		case KEY_P:
			return "P";
		case KEY_Q:
			return "Q";
		case KEY_R:
			return "R";
		case KEY_S:
			return "S";
		case KEY_T:
			return "T";
		case KEY_U:
			return "U";
		case KEY_V:
			return "V";
		case KEY_W:
			return "W";
		case KEY_X:
			return "X";
		case KEY_Y:
			return "Y";
		case KEY_Z:
			return "Z";
		case KEY_INSERT:
			return "INSERT";
		case KEY_DELETE:
			return "DELETE";
		case KEY_HOME:
			return "HOME";
		case KEY_END:
			return "END";
		case KEY_PAGEUP:
			return "PAGEUP";
		case KEY_PAGEDOWN:
			return "PAGEDOWN";
		case KEY_UP:
			return "UP";
		case KEY_DOWN:
			return "DOWN";
		case KEY_RIGHT:
			return "RIGHT";
		case KEY_LEFT:
			return "LEFT";
		case KEY_F1:
			return "F1";
		case KEY_F2:
			return "F2";
		case KEY_F3:
			return "F3";
		case KEY_F4:
			return "F4";
		case KEY_F5:
			return "F5";
		case KEY_F6:
			return "F6";
		case KEY_F7:
			return "F7";
		case KEY_F8:
			return "F8";
		case KEY_F9:
			return "F9";
		case KEY_F10:
			return "F10";
		case KEY_F11:
			return "F11";
		case KEY_F12:
			return "F12";
		case KEY_LCONTROL:
			return "LCONTROL";
		case KEY_LSHIFT:
			return "LSHIFT";
		case KEY_LALT:
			return "LALT";
		case KEY_RCONTROL:
			return "RCONTROL";
		case KEY_RSHIFT:
			return "RSHIFT";
		case KEY_RALT:
			return "RALT";
		case KEY_PAUSE:
			return "PAUSE";
		case MOUSE0_HORIZONTAL:
			return "MOUSE0_HORIZONTAL";
		case MOUSE0_VERTICAL:
			return "MOUSE0_VERTICAL";
		case MOUSE0_BUTTON0:
			return "MOUSE0_BUTTON0";
		case MOUSE0_BUTTON1:
			return "MOUSE0_BUTTON1";
		case MOUSE0_BUTTON2:
			return "MOUSE0_BUTTON2";
		case MOUSE0_WHEELUP:
			return "MOUSE0_WHEELUP";
		case MOUSE0_WHEELDOWN:
			return "MOUSE0_WHEELDOWN";
		case MOUSE0_WHEELLEFT:
			return "MOUSE0_WHEELLEFT";
		case MOUSE0_WHEELRIGHT:
			return "MOUSE0_WHEELRIGHT";
		case MOUSE0_BUTTON3:
			return "MOUSE0_BUTTON3";
		case MOUSE0_BUTTON4:
			return "MOUSE0_BUTTON4";
		default:
			Sys_Printf("Sys_KeyIndexToKeyName: keyindex %d untreated by this function\n", index);
			return "INVALID";
	}
}

/*
===================
Sys_KeyNameToKeyIndex

Converts from the key name to the engine keyindex. Don't be case-sensitive.
TODO: use hashes
===================
*/
int Sys_KeyNameToKeyIndex(const char *name)
{
	if (!_strcmpi(name, "INVALID"))
		return KEY_INVALID;
	if (!_strcmpi(name, "BACKSPACE"))
		return KEY_BACKSPACE;
	if (!_strcmpi(name, "TAB"))
		return KEY_TAB;
	if (!_strcmpi(name, "RETURN"))
		return KEY_RETURN;
	if (!_strcmpi(name, "ESC"))
		return KEY_ESC;
	if (!_strcmpi(name, "SPACE"))
		return KEY_SPACE;
	if (!_strcmpi(name, "QUOTE"))
		return KEY_QUOTE;
	if (!_strcmpi(name, "0"))
		return KEY_0;
	if (!_strcmpi(name, "1"))
		return KEY_1;
	if (!_strcmpi(name, "2"))
		return KEY_2;
	if (!_strcmpi(name, "3"))
		return KEY_3;
	if (!_strcmpi(name, "4"))
		return KEY_4;
	if (!_strcmpi(name, "5"))
		return KEY_5;
	if (!_strcmpi(name, "6"))
		return KEY_6;
	if (!_strcmpi(name, "7"))
		return KEY_7;
	if (!_strcmpi(name, "8"))
		return KEY_8;
	if (!_strcmpi(name, "9"))
		return KEY_9;
	if (!_strcmpi(name, "BACKQUOTE"))
		return KEY_BACKQUOTE;
	if (!_strcmpi(name, "A"))
		return KEY_A;
	if (!_strcmpi(name, "B"))
		return KEY_B;
	if (!_strcmpi(name, "C"))
		return KEY_C;
	if (!_strcmpi(name, "D"))
		return KEY_D;
	if (!_strcmpi(name, "E"))
		return KEY_E;
	if (!_strcmpi(name, "F"))
		return KEY_F;
	if (!_strcmpi(name, "G"))
		return KEY_G;
	if (!_strcmpi(name, "H"))
		return KEY_H;
	if (!_strcmpi(name, "I"))
		return KEY_I;
	if (!_strcmpi(name, "J"))
		return KEY_J;
	if (!_strcmpi(name, "K"))
		return KEY_K;
	if (!_strcmpi(name, "L"))
		return KEY_L;
	if (!_strcmpi(name, "M"))
		return KEY_M;
	if (!_strcmpi(name, "N"))
		return KEY_N;
	if (!_strcmpi(name, "O"))
		return KEY_O;
	if (!_strcmpi(name, "P"))
		return KEY_P;
	if (!_strcmpi(name, "Q"))
		return KEY_Q;
	if (!_strcmpi(name, "R"))
		return KEY_R;
	if (!_strcmpi(name, "S"))
		return KEY_S;
	if (!_strcmpi(name, "T"))
		return KEY_T;
	if (!_strcmpi(name, "U"))
		return KEY_U;
	if (!_strcmpi(name, "V"))
		return KEY_V;
	if (!_strcmpi(name, "W"))
		return KEY_W;
	if (!_strcmpi(name, "X"))
		return KEY_X;
	if (!_strcmpi(name, "Y"))
		return KEY_Y;
	if (!_strcmpi(name, "Z"))
		return KEY_Z;
	if (!_strcmpi(name, "INSERT"))
		return KEY_INSERT;
	if (!_strcmpi(name, "DELETE"))
		return KEY_DELETE;
	if (!_strcmpi(name, "HOME"))
		return KEY_HOME;
	if (!_strcmpi(name, "END"))
		return KEY_END;
	if (!_strcmpi(name, "PAGEUP"))
		return KEY_PAGEUP;
	if (!_strcmpi(name, "PAGEDOWN"))
		return KEY_PAGEDOWN;
	if (!_strcmpi(name, "UP"))
		return KEY_UP;
	if (!_strcmpi(name, "DOWN"))
		return KEY_DOWN;
	if (!_strcmpi(name, "RIGHT"))
		return KEY_RIGHT;
	if (!_strcmpi(name, "LEFT"))
		return KEY_LEFT;
	if (!_strcmpi(name, "F1"))
		return KEY_F1;
	if (!_strcmpi(name, "F2"))
		return KEY_F2;
	if (!_strcmpi(name, "F3"))
		return KEY_F3;
	if (!_strcmpi(name, "F4"))
		return KEY_F4;
	if (!_strcmpi(name, "F5"))
		return KEY_F5;
	if (!_strcmpi(name, "F6"))
		return KEY_F6;
	if (!_strcmpi(name, "F7"))
		return KEY_F7;
	if (!_strcmpi(name, "F8"))
		return KEY_F8;
	if (!_strcmpi(name, "F9"))
		return KEY_F9;
	if (!_strcmpi(name, "F10"))
		return KEY_F10;
	if (!_strcmpi(name, "F11"))
		return KEY_F11;
	if (!_strcmpi(name, "F12"))
		return KEY_F12;
	if (!_strcmpi(name, "LCONTROL"))
		return KEY_LCONTROL;
	if (!_strcmpi(name, "LSHIFT"))
		return KEY_LSHIFT;
	if (!_strcmpi(name, "LALT"))
		return KEY_LALT;
	if (!_strcmpi(name, "RCONTROL"))
		return KEY_RCONTROL;
	if (!_strcmpi(name, "RSHIFT"))
		return KEY_RSHIFT;
	if (!_strcmpi(name, "RALT"))
		return KEY_RALT;
	if (!_strcmpi(name, "PAUSE"))
		return KEY_PAUSE;
	if (!_strcmpi(name, "MOUSE0_HORIZONTAL"))
		return MOUSE0_HORIZONTAL;
	if (!_strcmpi(name, "MOUSE0_VERTICAL"))
		return MOUSE0_VERTICAL;
	if (!_strcmpi(name, "MOUSE0_BUTTON0"))
		return MOUSE0_BUTTON0;
	if (!_strcmpi(name, "MOUSE0_BUTTON1"))
		return MOUSE0_BUTTON1;
	if (!_strcmpi(name, "MOUSE0_BUTTON2"))
		return MOUSE0_BUTTON2;
	if (!_strcmpi(name, "MOUSE0_WHEELUP"))
		return MOUSE0_WHEELUP;
	if (!_strcmpi(name, "MOUSE0_WHEELDOWN"))
		return MOUSE0_WHEELDOWN;
	if (!_strcmpi(name, "MOUSE0_WHEELLEFT"))
		return MOUSE0_WHEELLEFT;
	if (!_strcmpi(name, "MOUSE0_WHEELRIGHT"))
		return MOUSE0_WHEELRIGHT;
	if (!_strcmpi(name, "MOUSE0_BUTTON3"))
		return MOUSE0_BUTTON3;
	if (!_strcmpi(name, "MOUSE0_BUTTON4"))
		return MOUSE0_BUTTON4;

	Sys_Printf("Sys_KeyNameToKeyIndex: key name %s untreated by this function\n", name);
	return KEY_INVALID;
}

/*
===================
Sys_KeyCodeToIndex

Converts from a SDL keycode to the engine's keyindex.
TODO: use hashes
===================
*/
int Sys_KeyCodeToKeyIndex(SDL_Keycode code)
{
	if (code == SDLK_UNKNOWN)
		return KEY_INVALID;
	if (code == SDLK_BACKSPACE)
		return KEY_BACKSPACE;
	if (code == SDLK_TAB)
		return KEY_TAB;
	if (code == SDLK_RETURN)
		return KEY_RETURN;
	if (code == SDLK_ESCAPE)
		return KEY_ESC;
	if (code == SDLK_SPACE)
		return KEY_SPACE;
	if (code == SDLK_QUOTE)
		return KEY_QUOTE;
	if (code == SDLK_0)
		return KEY_0;
	if (code == SDLK_1)
		return KEY_1;
	if (code == SDLK_2)
		return KEY_2;
	if (code == SDLK_3)
		return KEY_3;
	if (code == SDLK_4)
		return KEY_4;
	if (code == SDLK_5)
		return KEY_5;
	if (code == SDLK_6)
		return KEY_6;
	if (code == SDLK_7)
		return KEY_7;
	if (code == SDLK_8)
		return KEY_8;
	if (code == SDLK_9)
		return KEY_9;
	if (code == SDLK_BACKQUOTE)
		return KEY_BACKQUOTE;
	if (code == SDLK_a)
		return KEY_A;
	if (code == SDLK_b)
		return KEY_B;
	if (code == SDLK_c)
		return KEY_C;
	if (code == SDLK_d)
		return KEY_D;
	if (code == SDLK_e)
		return KEY_E;
	if (code == SDLK_f)
		return KEY_F;
	if (code == SDLK_g)
		return KEY_G;
	if (code == SDLK_h)
		return KEY_H;
	if (code == SDLK_i)
		return KEY_I;
	if (code == SDLK_j)
		return KEY_J;
	if (code == SDLK_k)
		return KEY_K;
	if (code == SDLK_l)
		return KEY_L;
	if (code == SDLK_m)
		return KEY_M;
	if (code == SDLK_n)
		return KEY_N;
	if (code == SDLK_o)
		return KEY_O;
	if (code == SDLK_p)
		return KEY_P;
	if (code == SDLK_q)
		return KEY_Q;
	if (code == SDLK_r)
		return KEY_R;
	if (code == SDLK_s)
		return KEY_S;
	if (code == SDLK_t)
		return KEY_T;
	if (code == SDLK_u)
		return KEY_U;
	if (code == SDLK_v)
		return KEY_V;
	if (code == SDLK_w)
		return KEY_W;
	if (code == SDLK_x)
		return KEY_X;
	if (code == SDLK_y)
		return KEY_Y;
	if (code == SDLK_z)
		return KEY_Z;
	if (code == SDLK_INSERT)
		return KEY_INSERT;
	if (code == SDLK_DELETE)
		return KEY_DELETE;
	if (code == SDLK_HOME)
		return KEY_HOME;
	if (code == SDLK_END)
		return KEY_END;
	if (code == SDLK_PAGEUP)
		return KEY_PAGEUP;
	if (code == SDLK_PAGEDOWN)
		return KEY_PAGEDOWN;
	if (code == SDLK_UP)
		return KEY_UP;
	if (code == SDLK_DOWN)
		return KEY_DOWN;
	if (code == SDLK_RIGHT)
		return KEY_RIGHT;
	if (code == SDLK_LEFT)
		return KEY_LEFT;
	if (code == SDLK_F1)
		return KEY_F1;
	if (code == SDLK_F2)
		return KEY_F2;
	if (code == SDLK_F3)
		return KEY_F3;
	if (code == SDLK_F4)
		return KEY_F4;
	if (code == SDLK_F5)
		return KEY_F5;
	if (code == SDLK_F6)
		return KEY_F6;
	if (code == SDLK_F7)
		return KEY_F7;
	if (code == SDLK_F8)
		return KEY_F8;
	if (code == SDLK_F9)
		return KEY_F9;
	if (code == SDLK_F10)
		return KEY_F10;
	if (code == SDLK_F11)
		return KEY_F11;
	if (code == SDLK_F12)
		return KEY_F12;
	if (code == SDLK_LCTRL)
		return KEY_LCONTROL;
	if (code == SDLK_LSHIFT)
		return KEY_LSHIFT;
	if (code == SDLK_LALT)
		return KEY_LALT;
	if (code == SDLK_RCTRL)
		return KEY_RCONTROL;
	if (code == SDLK_RSHIFT)
		return KEY_RSHIFT;
	if (code == SDLK_RALT)
		return KEY_RALT;
	if (code == SDLK_PAUSE)
		return KEY_PAUSE;

	Sys_Printf("Sys_KeyCodeToKeyIndex: keycode %d untreated by this function\n", code);
	return KEY_INVALID;
}

/*
===================
Sys_ProcessEvents

TODO: get rid of mouse acceleration!!!
===================
*/
extern int ignore_window_resize_event;
void Sys_ProcessEvents(void)
{
	SDL_Event sdlevent;
	/* For absolute coordinates scaling */
	int cur_w;
	int cur_h;

	Sys_GetWidthHeight(&cur_w, &cur_h);

	/* Grab all the events off the queue. */
	while (SDL_PollEvent(&sdlevent))
	{
		SDL_bool state = SDL_IsTextInputActive();

		if (state == SDL_FALSE)
		{
			switch (sdlevent.type)
			{
				case SDL_WINDOWEVENT:
					if (sdlevent.window.event == SDL_WINDOWEVENT_RESIZED && !Host_CMDGetCvar("_vid_fullscreen", true)->doublevalue)
					{
						if (!ignore_window_resize_event)
						{
							char cmd[MAX_CMD_SIZE];

							int width, height;
							width = Math_Max(BASE_WIDTH, sdlevent.window.data1);
							height = Math_Max(BASE_HEIGHT, sdlevent.window.data2);
							Sys_Snprintf(cmd, sizeof(cmd), "vid_setwindowed %d %d\n", width, height);
							Host_CMDBufferAdd(cmd);
						}
						else
						{
							ignore_window_resize_event = false;
						}
					}
					break;
				case SDL_KEYDOWN:
					/* Handle key presses. */
					if (!sdlevent.key.repeat)
						CL_InputProcessKeyUpDown(Sys_KeyCodeToKeyIndex(sdlevent.key.keysym.sym), 1, 0, 0); /* TODO: sys calling cl? */
					break;
				case SDL_KEYUP:
					/* Handle key releases. */
					if (!sdlevent.key.repeat)
						CL_InputProcessKeyUpDown(Sys_KeyCodeToKeyIndex(sdlevent.key.keysym.sym), 0, 0, 0); /* TODO: sys calling cl? */
					break;
				case SDL_MOUSEMOTION:
					/* TODO: support for more devices (motion.which), joysticks/gamepads get separated jaxis? */
					if (sdlevent.motion.xrel)
						CL_InputProcessKeyUpDown(MOUSE0_HORIZONTAL, -1, (vec_t)sdlevent.motion.xrel / (vec_t)cur_w, (vec_t)sdlevent.motion.x / (vec_t)cur_w); /* TODO: sys calling cl? */
					if (sdlevent.motion.yrel)
						CL_InputProcessKeyUpDown(MOUSE0_VERTICAL, -1, (vec_t)sdlevent.motion.yrel / (vec_t)cur_h, (vec_t)sdlevent.motion.y / (vec_t)cur_h); /* TODO: sys calling cl? */
					break;
				case SDL_MOUSEWHEEL: /* we are turning analog to digital FIXME? */
					/* TODO: support for more devices (motion.which), joysticks/gamepads get separated jaxis? */
					if (sdlevent.wheel.y > 0)
					{
						CL_InputProcessKeyUpDown(MOUSE0_WHEELUP, 1, 0, 0); /* TODO: sys calling cl? */
						CL_InputProcessKeyUpDown(MOUSE0_WHEELUP, 0, 0, 0); /* TODO: sys calling cl? */
					}
					else if (sdlevent.wheel.y < 0)
					{
						CL_InputProcessKeyUpDown(MOUSE0_WHEELDOWN, 1, 0, 0); /* TODO: sys calling cl? */
						CL_InputProcessKeyUpDown(MOUSE0_WHEELDOWN, 0, 0, 0); /* TODO: sys calling cl? */
					}
					if (sdlevent.wheel.x > 0) /* TODO: test this */
					{
						Sys_Printf("MOUSERIGHT\n");
						CL_InputProcessKeyUpDown(MOUSE0_WHEELRIGHT, 1, 0, 0); /* TODO: sys calling cl? */
						CL_InputProcessKeyUpDown(MOUSE0_WHEELRIGHT, 0, 0, 0); /* TODO: sys calling cl? */
					}
					else if (sdlevent.wheel.x < 0) /* TODO: test this */
					{
						Sys_Printf("MOUSELEFT\n");
						CL_InputProcessKeyUpDown(MOUSE0_WHEELLEFT, 1, 0, 0); /* TODO: sys calling cl? */
						CL_InputProcessKeyUpDown(MOUSE0_WHEELLEFT, 0, 0, 0); /* TODO: sys calling cl? */
					}
					break;
				case SDL_MOUSEBUTTONDOWN:
					/* TODO: support for more devices (motion.which), joysticks/gamepads get separated jaxis? */
					if (sdlevent.button.button == SDL_BUTTON_LEFT)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON0, 1, 0, 0); /* TODO: sys calling cl? */
					else if (sdlevent.button.button == SDL_BUTTON_RIGHT)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON1, 1, 0, 0); /* TODO: sys calling cl? */
					else if (sdlevent.button.button == SDL_BUTTON_MIDDLE)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON2, 1, 0, 0); /* TODO: sys calling cl? */
					else if (sdlevent.button.button == SDL_BUTTON_X1)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON3, 1, 0, 0); /* TODO: sys calling cl? */
					else if (sdlevent.button.button == SDL_BUTTON_X2)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON4, 1, 0, 0); /* TODO: sys calling cl? */
					break;
				case SDL_MOUSEBUTTONUP:
					/* TODO: support for more devices (motion.which), joysticks/gamepads get separated jaxis? */
					if (sdlevent.button.button == SDL_BUTTON_LEFT)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON0, 0, 0, 0); /* TODO: sys calling cl? */
					else if (sdlevent.button.button == SDL_BUTTON_RIGHT)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON1, 0, 0, 0); /* TODO: sys calling cl? */
					else if (sdlevent.button.button == SDL_BUTTON_MIDDLE)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON2, 0, 0, 0); /* TODO: sys calling cl? */
					else if (sdlevent.button.button == SDL_BUTTON_X1)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON3, 0, 0, 0); /* TODO: sys calling cl? */
					else if (sdlevent.button.button == SDL_BUTTON_X2)
						CL_InputProcessKeyUpDown(MOUSE0_BUTTON4, 0, 0, 0); /* TODO: sys calling cl? */
					break;
				default:
					break;
			}
		}
		else if (state == SDL_TRUE)
		{
			/* TODO: composition is not very good (mainly when deleting utf-8 extended characters) */
			switch (sdlevent.type)
			{
				case SDL_TEXTINPUT:
					/* add new text onto the end of our text */
					CL_InputProcessText(sdlevent.text.text, 0); /* TODO: sys calling cl? */
					break;
				case SDL_TEXTEDITING:
					/*
						update the composition text.
						update the cursor position.
						update the selection length (if any).

						composition = sdlevent.edit.text;
						cursor = sdlevent.edit.start;
						selection_len = sdlevent.edit.length;
					*/
					Sys_Printf("FIXME SDL_TEXTEDITING\n");
					break;
				case SDL_KEYDOWN:
					/* Handle key presses, sending repetitions too; */
					CL_InputProcessText(NULL, Sys_KeyCodeToKeyIndex(sdlevent.key.keysym.sym)); /* TODO: sys calling cl? */
					break;
				default:
					break;
			}
		}

		switch (sdlevent.type)
		{
			case SDL_QUIT:
				/* Handle quit requests (like Ctrl-c). */
				Host_CMDBufferAdd("quit");
				break;
			default:
				break;
		}
	}
}

/*
===================
Sys_ExclusiveInput

If value == true, grab all input exclusively
If value == false, let the system handle input

Useful for grabbing mouse input ingame and freeing it when at a menu
TODO: does this change mouse relative input sensitivity??
TODO: always set when fullscreen?
===================
*/
void Sys_ExclusiveInput(int value)
{
	SDL_bool state = SDL_GetRelativeMouseMode();

	if (value && state != SDL_TRUE)
		SDL_SetRelativeMouseMode(SDL_TRUE);
	else if (!value && state != SDL_FALSE)
		SDL_SetRelativeMouseMode(SDL_FALSE);
}

/*
===================
Sys_InputSetTextMode

If value == true, go into text composition mode
If value == false, go into individual key events mode
===================
*/
void Sys_InputSetTextMode(int value)
{
	SDL_bool state = SDL_IsTextInputActive();

	/*
		TODO: use:
		extern DECLSPEC void SDLCALL SDL_SetTextInputRect(SDL_Rect *rect); (important for IME and OSK)
		extern DECLSPEC SDL_bool SDLCALL SDL_HasScreenKeyboardSupport(void);
		extern DECLSPEC SDL_bool SDLCALL SDL_IsScreenKeyboardShown(SDL_Window *window);
	*/

	if (value && state != SDL_TRUE)
		SDL_StartTextInput();
	else if (!value && state != SDL_FALSE)
		SDL_StopTextInput();
}

/*
============================================================================

Main system functions

============================================================================
*/

/* TODO: see what havoc this can wreak with multithreading! */
static int jmp_set = false;
static jmp_buf environment;

/*
===================
Sys_SaveStackAndPC
===================
*/
void Sys_SaveStackAndPC(void)
{
	jmp_set = true;
	if (setjmp(environment))
		Host_CMDBufferClear(); /* do not re-execute commands if we abort a frame */
}

/*
===================
Sys_RestoreStackAndPC

Useful to abort frames
TODO: see if this work if we have some modules in C++
===================
*/
void Sys_RestoreStackAndPC(void)
{
	if (!jmp_set)
		Sys_Error("Sys_RestoreStackAndPC: jump point not set\n");
	longjmp(environment, 1); /* send parameter to reset the cmdbuffer */
}

/*
===================
Sys_Printf

TODO FIXME: calling various size integers (8, 16, 64 bit) with only %d in the same format string will cause misalignment and wrong printed values. Use u, h and l qualifiers where applicable.
===================
*/
void Sys_Printf(const char *msg, ...)
{
	va_list		argptr;
	char		text[MAX_LINE];

	va_start(argptr, msg);
	Sys_Vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	/* TODO: debuglevels and/or sys_dprint/cl_consoledprint */
	/* TODO: put these on the client notify lines? (currently it's the other way around, notify lines are sent here) */
	printf("%s", text);
	CL_MenuConsolePrint(text); /* TODO: sys calling cl */
}

/*
===================
Sys_Error

Quits the engine. This function will never return.
===================
*/
void Sys_Error(const char *error, ...)
{
	va_list		argptr;
	char		text[MAX_LINE];
	char		text2[MAX_LINE];
	static int	in_error = false;

	va_start(argptr, error);
	Sys_Vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	/* reset video mode so that we can see the message box */
	Sys_ShutdownVideo();

	if (!in_error)
	{
		Sys_Snprintf(text2, sizeof(text2), "System error, trying to shutdown gracefully...\n\nDetails:\n%s", text);
		Sys_Printf(text2);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "System error", text2, NULL);
		in_error = true;
		Host_CMDBufferClear();
		Host_CMDBufferAdd("quit");
		Host_CMDBufferExecute();
		Sys_RestoreStackAndPC();
	}
	else
	{
		Sys_Snprintf(text2, sizeof(text2), "Double error! Forcing exit...\n\nDetails:\n%s", text);
		Sys_Printf(text2);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Double system error", text2, NULL);
		exit(1);
	}
}

/*
===================
Sys_Random

Generate random numbers in the half-closed interval [range_min, range_max).
In other words: range_min <= random number < range_max
===================
*/
double Sys_Random(int range_min, int range_max)
{
	/* rand returns a number between [0,RAND_MAX) */
	return (double)rand() / (RAND_MAX) * (range_max - range_min) + range_min;
}

/*
===================
Sys_Random

Should be called each frame to keep random time dependent
===================
*/
void Sys_KeepRandom(void)
{
	rand();
}

/*
===================
Sys_Init
===================
*/
void Sys_Init(void)
{
	Sys_MemInit();

    Sys_Printf("Initializing SDL...\n");

    if (SDL_Init(SDL_INIT_TIMER) == -1)
        Sys_Error("Could not initialize SDL: %s\n", SDL_GetError());

	Sys_NetInit();

    Sys_Printf("Done.\n");
	/* TODO: SDL2 doesn't have a parachute anymore, so we should catch signals and call SDL_Quit (or the console cmd quit or the SDL_QUIT event) */
}

/*
===================
Sys_Shutdown
===================
*/
void Sys_Shutdown(void)
{
	/*
		Since the system will free up our resources, every shutdown function up to this point
		should only reset whatever states we changed on the system that may cause trouble.
		(For example: restore resolution, free the mouse cursor, etc)
	*/
    Sys_Printf("Shutting down... ");
	Sys_NetShutdown();
    /* Shutdown all subsystems */
    SDL_Quit();
 	Sys_MemShutdown();
	Sys_Printf("Done.\n");

	exit(0);
}

/*
===================
main
===================
*/
int main(int argc, char *argv[])
{
	srand((unsigned int)time(NULL));

	Sys_Init();
	Host_Init(argc, argv);

	Host_CMDBufferExecute(); /* execute any pending commands from initialization */

	while (1)
		Host_Frame();

	/* never reached */
	return 0;
}
