/*
 *  gl3Glue.cpp
 *
 *  Created by david on 5/11/09.
 *  Copyright 2009 n/a. All rights reserved.
 *
 */

#include "gl3Glue.h"

struct Vertex 
{
	float xyz[3];
	float st[2];
	float c[4];
};

#define MAX_VERTS 16384

typedef struct Vertex Vertex;
Vertex immediate[ MAX_VERTS ];
Vertex vab;
short quad_indexes[MAX_VERTS * 3 / 2 ];
int curr_vertex;
GLenum curr_prim;
bool initted = false;
int iPositionLocation;
int iTexCoordLocation;
int iColorLocation;

void ftglSetAttributeLocations(int iPosition, int iTexCoord, int iColor)
{
	iPositionLocation = iPosition;
	iTexCoordLocation = iTexCoord;
	iColorLocation = iColor;
}

/* TODO ELUAN: maintain in sync with engine */
enum {
    SHADER_ATTRIB_VERTEX = 0,
    SHADER_ATTRIB_COLOR,
	SHADER_ATTRIB_TEXCOORD0,
	SHADER_ATTRIB_TEXCOORD1,
	SHADER_ATTRIB_NORMAL,
    SHADER_NUM_ATTRIBUTES
};

void ftglBegin( GLenum prim ) 
{
	curr_vertex = 0;
	curr_prim = prim;

	/* TODO ELUAN: hack */
	ftglSetAttributeLocations(SHADER_ATTRIB_VERTEX, SHADER_ATTRIB_TEXCOORD0, SHADER_ATTRIB_COLOR);
}


void ftglVertex3f( float x, float y, float z ) 
{
	assert( curr_vertex < MAX_VERTS );
	vab.xyz[ 0 ] = x;
	vab.xyz[ 1 ] = y;
	vab.xyz[ 2 ] = z;
	immediate[ curr_vertex ] = vab;
	curr_vertex++;
}


void ftglVertex2f( float x, float y ) 
{
	assert( curr_vertex < MAX_VERTS );
	vab.xyz[ 0 ] = x;
	vab.xyz[ 1 ] = y;
	vab.xyz[ 2 ] = 0.0f;
	immediate[ curr_vertex ] = vab;
	curr_vertex++;
}


void ftglTexCoord2f( GLfloat s, GLfloat t ) 
{
	vab.st[ 0 ] = s;
	vab.st[ 1 ] = t;

	/* TODO ELUAN: hack */
	vab.c[ 0 ] = 1;
	vab.c[ 1 ] = 1;
	vab.c[ 2 ] = 1;
	vab.c[ 3 ] = 1;
}


void ftglEnd() 
{
	const Vertex *v = NULL; /* for pointers */

	if (curr_vertex == 0) 
	{
		curr_prim = 0;
		return;
	}
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * curr_vertex, immediate, GL_STREAM_DRAW);
	if (iPositionLocation >= 0)
	{
		glEnableVertexAttribArray(iPositionLocation);
		glVertexAttribPointer(iPositionLocation, 3, GL_FLOAT, false, sizeof( Vertex ), &v->xyz);
	}

	if (iTexCoordLocation >= 0)
	{
		glEnableVertexAttribArray(iTexCoordLocation);
		glVertexAttribPointer(iTexCoordLocation, 2, GL_FLOAT, false, sizeof( Vertex ), &v->st);
	}

	if (iColorLocation >= 0)
	{
		glEnableVertexAttribArray(iColorLocation);
		glVertexAttribPointer(iColorLocation, 4, GL_FLOAT, false, sizeof( Vertex ), &v->c);
	}

	glDrawArrays( curr_prim, 0, curr_vertex );

	if (iPositionLocation >= 0)
		glDisableVertexAttribArray(iPositionLocation);
	if (iTexCoordLocation >= 0)
		glDisableVertexAttribArray(iTexCoordLocation);
	if (iColorLocation >= 0)
		glDisableVertexAttribArray(iColorLocation);

	curr_vertex = 0;
	curr_prim = 0;
}


void ftglError(const char* source)
{
	GLenum error = glGetError();
	 
	switch (error) {
		case GL_NO_ERROR:
			break;
		case GL_INVALID_ENUM:
			printf("GL Error (%d): GL_INVALID_ENUM. %s\n\n", error, source);
			break;
		case GL_INVALID_VALUE:
			printf("GL Error (%d): GL_INVALID_VALUE. %s\n\n", error, source);
			break;
		case GL_INVALID_OPERATION:
			printf("GL Error (%d): GL_INVALID_OPERATION. %s\n\n", error, source);
			break;
		case GL_OUT_OF_MEMORY:
			printf("GL Error (%d): GL_OUT_OF_MEMORY. %s\n\n", error, source);
			break;
		default:
			break;
	}
}
