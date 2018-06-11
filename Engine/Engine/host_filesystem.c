/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Host filesystem management

TODO: sound updates while loading

============================================================================
*/

#define DEFAULT_SEARCHPATH		"data"

#define MAX_SEARCHPATHS			16
typedef struct searchpath_s
{
	int active;
	char path[MAX_PATH];
} searchpath_t;

searchpath_t	searchpaths[MAX_SEARCHPATHS]; /* TODO: have a variable with the number of added searchpaths to avoid parsing through the entire array */

/*
===================
Host_ValidatePath

Will error on ".." and ":". Remember to call on every path that the user can input.
===================
*/
void Host_ValidatePath(const char *path)
{
	int i, size;

	size = strlen(path);

	if (size + 1 >= MAX_PATH)
		Sys_Error("size + 1 >= MAX_PATH, path too long (%s)\n", path);

	if (!path[0])
		Sys_Error("Empty path specified\n");
	if (path[0] == '/')
		Sys_Error("Attempted to access root directory (%s)\n", path);
	if (path[size - 1] == '/')
		Sys_Error("Directories should not have a trailing slash (%s)\n", path);
	for (i = 0; i < size; i++)
	{
		/* we can check i + 1 because of the \0 */
		if (path[i] == '.' && path[i + 1] == '.')
			Sys_Error("path[i] == '.' && path[i + 1] == '.', invalid relative path specified (%s)\n", path);
		else if (path[i] == '.' && path[i + 1] == '/')
			Sys_Error("path[i] == '.' && path[i + 1] == '/', invalid relative path specified (%s)\n", path);
		else if (path[i] == '/' && path[i + 1] == '/')
			Sys_Error("path[i] == '/' && path[i + 1] == '/', invalid relative path specified (%s)\n", path);
		else if (path[i] == ':')
			Sys_Error("path[i] == ':', drive path should not be specified (%s)\n", path);
		else if (path[i] == '\\')
			Sys_Error("path[i] == '\\', character \"\\\" is not allowed (%s)\n", path);
	}
}

/*
===================
Host_FSFileExists

Returns -1 if file doesn't exist, filesize otherwise.
TODO: use size_t here and in Sys_FileExists
===================
*/
int Host_FSFileExists(const char *path)
{
	int i;
	char fullpath[MAX_PATH];

	for (i = MAX_SEARCHPATHS - 1; i >= 0; i--)
	{
		if (searchpaths[i].active)
		{
			int result;

			if (strlen(searchpaths[i].path) + strlen(path) + 3 >= MAX_PATH)
				Sys_Error("Final path too long for \"%s\"\n", path);

			Sys_Snprintf(fullpath, sizeof(fullpath), "%s/%s", searchpaths[i].path, path);
			Host_ValidatePath(fullpath);
			result = Sys_FileExists(fullpath);
			if (result != -1)
				return result;
		}
	}

	return -1;
}

/*
===================
Host_FSFileGetPath

Gets a filepath inside the highest searchpath for a specific archive, empty string otherwise
===================
*/
void Host_FSFileGetPath(const char *path, char *dst_fullpath, size_t dst_size)
{
	int i;
	char fullpath[MAX_PATH];

	for (i = MAX_SEARCHPATHS - 1; i >= 0; i--)
	{
		if (searchpaths[i].active)
		{
			if (strlen(searchpaths[i].path) + strlen(path) + 3 >= MAX_PATH)
				Sys_Error("Final path too long for \"%s\"\n", path);

			Sys_Snprintf(fullpath, sizeof(fullpath), "%s/%s", searchpaths[i].path, path);
			Host_ValidatePath(fullpath);
			if (Sys_FileExists(fullpath) != -1)
			{
				Sys_Snprintf(dst_fullpath, dst_size, "%s", fullpath);
				return;
			}
		}
	}

	dst_fullpath[0] = 0;
}

/*
===================
Host_FSLoadBinaryFile

Returns -1 on failure, filesize otherwise

TODO: endianness converting for parsing anything loaded by this
===================
*/
int Host_FSLoadBinaryFile(char *path, mempool_t *mempool, char *id, unsigned char **result, int do_not_warn_on_failure)
{
	unsigned char *nullfile = "(null)";
	int i, size;
	char fullpath[MAX_PATH];
	unsigned char *buffer;

	for (i = MAX_SEARCHPATHS - 1; i >= 0; i--)
	{
		if (searchpaths[i].active)
		{
			if (strlen(searchpaths[i].path) + strlen(path) + 3 >= MAX_PATH)
				Sys_Error("Final path too long for \"%s\"\n", path);

			Sys_Snprintf(fullpath, sizeof(fullpath), "%s/%s", searchpaths[i].path, path);
			Host_ValidatePath(fullpath);
			size = Sys_FileExists(fullpath);

			if (size == 0) /* TODO: do not do this, set result to null and return zero then treat correctly on the calling code */
			{
				*result = nullfile;
				return strlen(nullfile) + 1; /* include \0 terminator */
			}
			else if (size != -1)
			{
				buffer = Sys_MemAlloc(mempool, size + 1, id); /*
																	size + 1
																	even if this is called "LoadBINARYFile", we parse many
																	things loaded by this as text. So, having a nice zero (the
																	engine's mem alloc routines fill allocated buffers with
																	zero) at the end is nice!
																	FIXME? LoadAsciiFile?
																	TODO: loading and saving a file multiple times may result
																	in lots of zero bytes added to the end?
																*/

				size = Sys_FileReadBinary(fullpath, buffer, 0, size);
				if (size == -1)
				{
					Sys_Printf("Error reading from \"%s\"\n", path);
					return -1;
				}
				else
				{
					*result = buffer;
					return size;
				}
			}
		}
	}

	if (!do_not_warn_on_failure)
		Sys_Printf("Couldn't load \"%s\"\n", path);
	return -1;
}

/*
===================
Host_FSWriteBinaryFile

Returns -1 on failure, written bytes otherwise
Will write to the latest active searchpath
===================
*/
int Host_FSWriteBinaryFile(char *path, const unsigned char *buffer, int count)
{
	int i, size;
	char fullpath[MAX_PATH];

	/* FIXME: will break if we add binary archives as searchpaths */

	for (i = MAX_SEARCHPATHS - 1; i >= 0; i--)
	{
		if (searchpaths[i].active)
		{
			if (strlen(searchpaths[i].path) + strlen(path) + 3 >= MAX_PATH)
				Sys_Error("Final path too long for \"%s\"\n", path);

			Sys_Snprintf(fullpath, sizeof(fullpath), "%s/%s", searchpaths[i].path, path);
			Host_ValidatePath(fullpath);

			size = Sys_FileWriteBinary(fullpath, buffer, count);
			if (size == -1)
				Sys_Printf("Error writing to \"%s\"\n", path);

			return size;
		}
	}

	Sys_Printf("Couldn't find a searchpath to write \"%s\"\n", path);
	return -1;
}

/*
===================
Host_FSFileHandleOpenBinaryRead

Returns NULL on failure, handle otherwise

TODO: endianness converting for parsing anything loaded by this
===================
*/
void *Host_FSFileHandleOpenBinaryRead(char *path)
{
	int i, size;
	char fullpath[MAX_PATH];

	for (i = MAX_SEARCHPATHS - 1; i >= 0; i--)
	{
		if (searchpaths[i].active)
		{
			if (strlen(searchpaths[i].path) + strlen(path) + 3 >= MAX_PATH)
				Sys_Error("Final path too long for \"%s\"\n", path);

			Sys_Snprintf(fullpath, sizeof(fullpath), "%s/%s", searchpaths[i].path, path);
			Host_ValidatePath(fullpath);
			size = Sys_FileExists(fullpath);

			if (size == -1)
			{
				goto openreadfailure;
			}
			else
			{
				return Sys_FileHandleOpenRead(fullpath, 0);
			}
		}
	}

openreadfailure:
	Sys_Printf("Couldn't load \"%s\"\n", path);
	return NULL;
}

/*
===================
Host_FSFileHandleOpenBinaryWrite

Returns NULL on failure, handle otherwise
Will write to the latest active searchpath
===================
*/
void *Host_FSFileHandleOpenBinaryWrite(char *path)
{
	int i;
	void *handle;
	char fullpath[MAX_PATH];

	/* FIXME: will break if we add binary archives as searchpaths */

	for (i = MAX_SEARCHPATHS - 1; i >= 0; i--)
	{
		if (searchpaths[i].active)
		{
			if (strlen(searchpaths[i].path) + strlen(path) + 3 >= MAX_PATH)
				Sys_Error("Final path too long for \"%s\"\n", path);

			Sys_Snprintf(fullpath, sizeof(fullpath), "%s/%s", searchpaths[i].path, path);
			Host_ValidatePath(fullpath);

			handle = Sys_FileHandleOpenWrite(fullpath);
			if (!handle)
				Sys_Printf("Error writing to \"%s\"\n", path);

			return handle;
		}
	}

	Sys_Printf("Couldn't find a searchpath to write \"%s\"\n", path);
	return NULL;
}

/*
===================
Host_FSFileHandleClose

Closes a file handle
===================
*/
void Host_FSFileHandleClose(void *handle)
{
	Sys_FileHandleClose(handle);
}

/*
===================
Host_FSFileHandleReadBinaryMemPool

Reads "count" bytes from "handle" into "result", creating data in "mempool" with name "id"
Returns read bytes

TODO: endianness converting for parsing anything loaded by this
===================
*/
int Host_FSFileHandleReadBinaryMemPool(void *handle, mempool_t *mempool, char *id, unsigned char **result, int count)
{
	unsigned char *buffer;

	buffer = Sys_MemAlloc(mempool, count, id);
	*result = buffer;

	return Sys_FileHandleReadBinary(handle, buffer, count);
}

/*
===================
Host_FSFileHandleReadBinaryDest

Reads "count" bytes from "handle" into "result", please make sure that the data fits before reading
Returns read bytes

TODO: endianness converting for parsing anything loaded by this
===================
*/
int Host_FSFileHandleReadBinaryDest(void *handle, unsigned char *result, int count)
{
	return Sys_FileHandleReadBinary(handle, result, count);
}

/*
===================
Host_FSFileHandleWriteBinary

Writes "count" bytes from "buffer" to "handle"
Returns written bytes.
===================
*/
int Host_FSFileHandleWriteBinary(void *handle, const unsigned char *buffer, int count)
{
	return Sys_FileHandleWriteBinary(handle, buffer, count);
}

/*
===================
Host_FSAddSearchpath
===================
*/
void Host_FSAddSearchpath(char *path)
{
	int i;

	Host_ValidatePath(path);

	for (i = 0; i < MAX_SEARCHPATHS; i++)
	{
		if (!searchpaths[i].active)
		{
			Sys_Snprintf(searchpaths[i].path, sizeof(searchpaths[i].path), "%s", path);
			searchpaths[i].active = true;
			Sys_Printf("Added search path: %s\n", searchpaths[i].path);
			return;
		}
	}

	if (i == MAX_SEARCHPATHS)
		Sys_Error("Host_FSAddSearchpath: too many paths\n");
}

/*
===================
Host_FSInit
===================
*/
void Host_FSInit(void)
{
	int i;
	char *path;

	for (i = 0; i < MAX_SEARCHPATHS; i++)
		searchpaths[i].active = false;

	/* first the base files */
	if (!Host_GetArg("-basedir", &path))
		Host_FSAddSearchpath(DEFAULT_SEARCHPATH);
	else
		Host_FSAddSearchpath(path);

	/* then additional mod files */
	if (Host_GetArg("-game", &path))
		Host_FSAddSearchpath(path);

	 /* TODO: add other searchpaths for as many -game parameters as we have. Or maybe a SAVE PATH in the user home dir as the last one. */
}

/*
===================
Host_FSShutdown
===================
*/
void Host_FSShutdown(void)
{
}