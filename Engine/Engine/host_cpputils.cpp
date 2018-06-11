/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

Misc utils used by the engine

TODO: just convert the engine to SANE c++?
TODO: lots of n (or even n^2 or n^3) searches in the code that can be
replaced with more efficient algorithms
TODO: can the string lookup be faster? (test tree?)

============================================================================
*/

#include <map>
#include <string>

/*
============================================================================

Classes for overloading operators on types that do not have them

============================================================================
*/
class int3_s {
public:
	int value[3];
	bool operator<(const int3_s b) const
	{
		if (value[0] < b.value[0])
			return true;
		if (value[0] > b.value[0])
			return false;
		if (value[1] < b.value[1])
			return true;
		if (value[1] > b.value[1])
			return false;
		if (value[2] < b.value[2])
			return true;
		if (value[2] > b.value[2])
			return false;

		return false;
	}
};

/*
============================================================================

Map:
int[3] -> int
char * -> void (CDECL *)(void)

============================================================================
*/

typedef void (*funcptr)(void);

typedef std::map<int3_s, int, std::less<int3_s> > int3_to_int;
typedef std::map<std::string, void (CDECL *)(void), std::less<std::string> > charp_to_voidpvoid;

/*
===================
Host_UtilMapInsert*

Inserts a key-pair value in a map
===================
*/
void Host_UtilMapInsertInt3ToInt(void *data, int key[3], int pair)
{
	int3_to_int *the_map = (int3_to_int *)data;

	int3_s thevec;
	thevec.value[0] = key[0];
	thevec.value[1] = key[1];
	thevec.value[2] = key[2];
	(*the_map).insert(int3_to_int::value_type(thevec, pair));
}
void Host_UtilMapInsertCharPToVoidPVoid(void *data, char *key, void (*pair)(void))
{
	charp_to_voidpvoid *the_map = (charp_to_voidpvoid *)data;

	(*the_map).insert(charp_to_voidpvoid::value_type(key, pair));
}

/*
===================
Host_UtilMapRetrieve*

Retrieves a key-pair value in a map. Returns -1 if not found.
===================
*/
int Host_UtilMapRetrieveInt3ToInt(void *data, int key[3])
{
	int3_to_int *the_map = (int3_to_int *)data;
	int3_to_int::iterator the_iterator;

	int3_s thevec;
	thevec.value[0] = key[0];
	thevec.value[1] = key[1];
	thevec.value[2] = key[2];
	the_iterator = (*the_map).find(thevec);
	if (the_iterator != (*the_map).end())
		return (*the_iterator).second;
	else
		return -1;
}
void *Host_UtilMapRetrieveCharPToVoidPVoid(void *data, char *key) /* the function pointers have no parameters, let's just return the address then */
{
	charp_to_voidpvoid *the_map = (charp_to_voidpvoid *)data;
	charp_to_voidpvoid::iterator the_iterator;

	the_iterator = (*the_map).find(key);
	if (the_iterator != (*the_map).end())
		return (void *)(*the_iterator).second;
	else
		return (void *)-1;
}

/*
===================
Host_UtilMapCreate*

Creates a new map
===================
*/
void *Host_UtilMapCreateInt3ToInt(void)
{
	int3_to_int *new_map = new int3_to_int;

	return new_map;
}
void *Host_UtilMapCreateCharPToVoidPVoid(void)
{
	charp_to_voidpvoid *new_map = new charp_to_voidpvoid;

	return new_map;
}

/*
===================
Host_UtilMapDestroy*

Destroys a map
===================
*/
void Host_UtilMapDestroyInt3ToInt(void *data)
{
	int3_to_int *old_map = (int3_to_int *)data;
	delete old_map;
}
void Host_UtilMapDestroyCharPToVoidPVoid(void *data)
{
	charp_to_voidpvoid *old_map = (charp_to_voidpvoid *)data;
	delete old_map;
}
