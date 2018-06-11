/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

/*
============================================================================

General math-related functions

TODO: did I somewhere in the engine code use maxs - mins wrongly to calculate the center of a bounding box? This just passed through my head

============================================================================
*/

const vec3_t null_vec3 = {0, 0, 0};

/*
===================
Math_AABB3Center

Calculates the center point for a given axis aligned bounding box
center = mins + (maxs - mins) / 2
===================
*/
void Math_AABB3Center(vec3_t destcenter, vec3_t maxs, vec3_t mins)
{
	vec3_t tmp;

	/* FIXME: optimize to (mins + maxs)/2 */
	Math_Vector3ScaleAdd(mins, -1, maxs, tmp);
	Math_Vector3Scale(tmp, tmp, 0.5f);
	Math_Vector3Add(tmp, mins, destcenter);
}

/*
===================
Math_AABB3EnclosePoints

Calculates the AABB which encloses two given points
===================
*/
void Math_AABB3EnclosePoints(const vec3_t point1, const vec3_t point2, vec3_t maxs, vec3_t mins)
{
	mins[0] = Math_Min(point1[0], point2[0]);
	mins[1] = Math_Min(point1[1], point2[1]);
	mins[2] = Math_Min(point1[2], point2[2]);
	maxs[0] = Math_Max(point1[0], point2[0]);
	maxs[1] = Math_Max(point1[1], point2[1]);
	maxs[2] = Math_Max(point1[2], point2[2]);
}

/*
===================
Math_PlaneNormalAndPointToPlaneEquation

Converts from cross(innormal, pointxyz - inpoint) = 0 to apointx + bpointy + cpointz + d = 0
===================
*/
void Math_PlaneNormalAndPointToPlaneEquation(vec3_t innormal, vec3_t inpoint, vec4_t outequation)
{
	outequation[0] = innormal[0];
	outequation[1] = innormal[1];
	outequation[2] = innormal[2];
	outequation[3] = Math_DotProduct3(innormal, inpoint);
}

/*
===================
Math_PlaneFromThreePoints

Creates a plane from three distinct points in space
They must not be colinear.
===================
*/
void Math_PlaneFromThreePoints(vec3_t in_point0, vec3_t in_point1, vec3_t in_point2, vec4_t outequation)
{
	vec3_t v1m0, v2m0;
	Math_Vector3ScaleAdd(in_point0, -1, in_point1, v1m0);
	Math_Vector3ScaleAdd(in_point0, -1, in_point2, v2m0);
	Math_CrossProduct3(v1m0, v2m0, outequation); /* create normal */
	Math_Vector3Normalize(outequation);
	outequation[3] = Math_DotProduct3(outequation, in_point0); /* create dist */
}

/*
===================
Math_PlaneDistanceToPoint

Be sure to create the plane with a NORMALIZED direction vector.
Result:
positive: on the front side of the plane
zero: on the plane:
negative: on the back side of the plane

TODO: does this give the real distance? we use mostly to see sides anyway
===================
*/
vec_t Math_PlaneDistanceToPoint(const vec4_t equation, const vec3_t point)
{
	/* TODO: inline this */
	return Math_DotProduct3(equation, point) - equation[3]; /* FIXME: optimize trivial cases */
}

/*
===================
Math_PlaneIntersectionPoint

Be sure to create the plane with a NORMALIZED direction vector.
Result:
outpoint will be the intersection between the three input planes

returns false if there's no single point of intersection, true otherwise

===================
*/
int Math_PlaneIntersectionPoint(const vec4_t in_equation1, const vec4_t in_equation2, const vec4_t in_equation3, vec3_t outpoint)
{
	vec3_t vU;
	vec_t fDenom;
	vec3_t aux, aux2, aux3;

	Math_CrossProduct3(in_equation2, in_equation3, vU);
	fDenom = Math_DotProduct3(in_equation1, vU);
	
	if (fabsf(fDenom) <= 0) /* TODO: use epsilon here */
		return false;

	Math_Vector3Scale(vU, aux, in_equation1[3]);
	Math_Vector3Scale(in_equation2, aux2, in_equation3[3]);
	Math_Vector3Scale(in_equation3, aux3, in_equation2[3]);
	Math_Vector3ScaleAdd(aux3, -1, aux2, aux2);
	Math_CrossProduct3(in_equation1, aux2, aux3);
	Math_Vector3Add(aux, aux3, aux);
	Math_Vector3Scale(aux, outpoint, 1.f / fDenom);
	/* legible: _vRet = (vU * _pP0.dist + _pP0.n.Cross( _pP1.n * _pP2.dist - _pP2.n * _pP1.dist )) / fDenom; */
	return true;
}

/*
===================
Math_BoxTestAgainstPlaneSides

Be sure to create the plane with a NORMALIZED direction vector.
Result:
will return 1 if the box is on the positive side of the plane, 2 if on the negative side and 3 if it crosses the plane.

TODO: this is ultra-slow! Do a faster test and also make cases for axial planes
TODO: test correctly
TODO: make my own implementation!! This is GPL CODE!
===================
*/
int Math_BoxTestAgainstPlaneSides(const vec3_t boxmins, const vec3_t boxmaxs, const vec3_t planenormal, const vec_t planedist)
{
	int		i;
	float	dist1, dist2;
	int		sides;
	vec3_t	corners[2]; /* 0 = maxs, 1 = mins */

	for (i = 0; i < 3; i++)
	{
		if (planenormal[i] < 0)
		{
			corners[0][i] = boxmins[i];
			corners[1][i] = boxmaxs[i];
		}
		else
		{
			corners[1][i] = boxmins[i];
			corners[0][i] = boxmaxs[i];
		}
	}
	dist1 = Math_DotProduct3(planenormal, corners[0]) - planedist;
	dist2 = Math_DotProduct3(planenormal, corners[1]) - planedist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

	return sides;
}

/*
===================
Math_VecForwardToAngles

No ANGLES_ROLL will result!
===================
*/
void Math_VecForwardToAngles(vec3_t forward, vec3_t angles)
{
	Math_Vector3Normalize(forward);

	angles[ANGLES_ROLL] = 0;
	if (forward[0] == 0 && forward[2] == 0)
	{
		if(forward[1] > 0)
		{
			angles[ANGLES_PITCH] = (vec_t)M_PI_2;
			angles[ANGLES_YAW] = 0;
		}
		else
		{
			angles[ANGLES_PITCH] = (vec_t)-M_PI_2;
			angles[ANGLES_YAW] = 0;
		}
	}
	else
	{
		angles[ANGLES_YAW] = (vec_t)-atan2(forward[0], -forward[2]);
		angles[ANGLES_PITCH] = (vec_t)atan2(forward[1], sqrt(forward[0]*forward[0] + forward[2]*forward[2]));
	}

	/* now convert radians to degrees, and make all values positive */
	Math_Vector3Scale(angles, angles, 180.0f / (vec_t)M_PI);
	if (angles[ANGLES_PITCH] < 0) angles[ANGLES_PITCH] += 360;
	if (angles[ANGLES_YAW] < 0) angles[ANGLES_YAW] += 360;
	if (angles[ANGLES_ROLL] < 0) angles[ANGLES_ROLL] += 360;
}

/*
===================
Math_VecToAngles

YXZ order
Returns true if the result is unique, false otherwise (and then return a solution with roll zero)
forward[] are all negated from the default use because we look into -Z
TODO: see if this and Math_AnglesToVec return the same values for the unique solutions
TODO: test for values outside of (-M_PI_2, M_PI_2) for pitch and roll
===================
*/
int Math_VecToAngles(vec3_t forward, vec3_t right, vec3_t up, vec3_t angles)
{
	int is_unique;
	vec_t gimballock_vertical; /* for gimbal locks with roll and yaw */
	/*
		rotYXZ is the same as in Math_AnglesToVec:
			rot[0][0] = Cy * Cz + Sx * Sy * Sz;
			rot[0][1] = Cz * Sx * Sy - Cy * Sz;
			rot[0][2] = Cx * Sy;
			rot[1][0] = Cx * Sz;
			rot[1][1] = Cx * Cz;
			rot[1][2] = -Sx;
			rot[2][0] = -Cz * Sy + Cy * Sx * Sz;
			rot[2][1] = Cy * Cz * Sx + Sy * Sz;
			rot[2][2] = Cx * Cy;
	*/

	Math_Vector3Normalize(forward);
	Math_Vector3Normalize(right);
	Math_Vector3Normalize(up);

	angles[ANGLES_PITCH] = (vec_t)asin(forward[1]);
	if (angles[ANGLES_PITCH] < M_PI_2)
	{
		if (angles[ANGLES_PITCH] > -M_PI_2)
		{
			angles[ANGLES_YAW] = (vec_t)atan2(-forward[0], -forward[2]);
			angles[ANGLES_ROLL] = (vec_t)atan2(right[1], up[1]);
			is_unique = true;
		}
		else
		{
			/* not unique, gimbal lock */
			gimballock_vertical = (vec_t)atan2(-up[0], right[0]);
			angles[ANGLES_ROLL] = 0; /* all values will work */
			angles[ANGLES_YAW] = angles[ANGLES_ROLL] + gimballock_vertical;
			is_unique = false;
		}
	}
	else
	{
		/* not unique, gimbal lock */
		gimballock_vertical = (vec_t)atan2(-up[0], right[0]);
		angles[ANGLES_ROLL] = 0; /* all values will work */
		angles[ANGLES_YAW] = -gimballock_vertical - angles[ANGLES_ROLL];
		is_unique = false;
	}

	/* we use degrees, so convert our radians */
	angles[ANGLES_PITCH] = Math_Rad2Deg(angles[ANGLES_PITCH]);
	angles[ANGLES_YAW] = Math_Rad2Deg(angles[ANGLES_YAW]);
	angles[ANGLES_ROLL] = Math_Rad2Deg(angles[ANGLES_ROLL]);
	return is_unique;
}

/*
===================
Math_AnglesToVec

YXZ order
forward[] are all negated from the default use because we look into -Z
forward, right and up are all optional, NULL pointers can be passed.
===================
*/
void Math_AnglesToVec(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	/* rotation matrix */
	vec_t rot[3][3];
	/*
		x = pitch
		y = yaw
		z = roll
		C = cos
		S = sin
	*/
	vec_t Cx = (vec_t)cos(Math_Deg2Rad(angles[ANGLES_PITCH]));
	vec_t Cy = (vec_t)cos(Math_Deg2Rad(angles[ANGLES_YAW]));
	vec_t Cz = (vec_t)cos(Math_Deg2Rad(angles[ANGLES_ROLL]));
	vec_t Sx = (vec_t)sin(Math_Deg2Rad(angles[ANGLES_PITCH]));
	vec_t Sy = (vec_t)sin(Math_Deg2Rad(angles[ANGLES_YAW]));
	vec_t Sz = (vec_t)sin(Math_Deg2Rad(angles[ANGLES_ROLL]));

	/*
		The standard one-axis rotation matrices:

		Rx = {{1, 0, 0}, {0, Cx, -Sx}, {0, Sx, Cx}}
		Ry = {{Cy, 0, Sy}, {0, 1, 0}, {-Sy, 0, Cy}}
		Rz = {{Cz, -Sz, 0}, {Sz, Cz, 0}, {0, 0, 1}}

		Concatenating them, in the YXZ order (YAW, PITCH, ROLL) we get rot[][]:
		rot = Ry*Rx*Rz (equivalent to Ry*(Rx*Rz))
	*/

	rot[0][0] = Cy * Cz + Sx * Sy * Sz;
	rot[0][1] = Cz * Sx * Sy - Cy * Sz;
	rot[0][2] = Cx * Sy;
	rot[1][0] = Cx * Sz;
	rot[1][1] = Cx * Cz;
	rot[1][2] = -Sx;
	rot[2][0] = -Cz * Sy + Cy * Sx * Sz;
	rot[2][1] = Cy * Cz * Sx + Sy * Sz;
	rot[2][2] = Cx * Cy;

	/* now we multiply axis vectors by the matrix to get new vectors */
	/* TODO: check this, I've changed some stuff about the coordinate system */
	/* normalize just to be safe */
	if (forward)
	{
		Math_Vector3Set(forward, -rot[0][2], -rot[1][2], -rot[2][2]);
		Math_Vector3Normalize(forward);
	}
	if (right)
	{
		Math_Vector3Set(right, rot[0][0], rot[1][0], rot[2][0]);
		Math_Vector3Normalize(right);
	}
	if (up)
	{
		Math_Vector3Set(up, rot[0][1], rot[1][1], rot[2][1]);
		Math_Vector3Normalize(up);
	}
}

/*
===================
Math_ReflectVectorAroundNormal

Calculates a reflection for an "invector" touching a plane with "normal"

r = in - 2*dot(in, normal) * normal / length(normal)

TODO: fail sometimes? or is it because the entity becomes stuck inside something solid? (the projectiles being reflected on other projectiles or inside the world may cause this)
===================
*/
void Math_ReflectVectorAroundNormal(vec3_t invector, vec3_t normal, vec3_t outvector)
{
	vec3_t tempv;
	vec_t tempe;


	tempe = 2.f * Math_DotProduct3(invector, normal) / (vec_t)Math_Vector3Length(normal); /* TODO: normal should be normalized already */
	Math_Vector3Scale(normal, tempv, tempe);
	Math_Vector3ScaleAdd(tempv, -1, invector, outvector);
}

/*
===================
Math_CheckIfClose

Direct line.
===================
*/
int Math_CheckIfClose(vec3_t origin1, vec3_t origin2, vec_t distance)
{
	vec3_t dir;

	Math_Vector3ScaleAdd(origin1, -1, origin2, dir);
	if (Math_Vector3LengthSquared(dir) <= distance * distance)
		return true;
	else
		return false;
}

/*
===================
Math_PointInsideBox

Checks if a point is inside the box is inside the box defined by mins and maxs.
The interval is closed in mins and open in maxs, to avoid a point being inside
two adjacent boxes.

TODO: check inverted box
===================
*/
int Math_PointInsideBox(const vec3_t point, const vec3_t box_mins, const vec3_t box_maxs)
{
	if (point[1] >= box_mins[1] && point[1] < box_maxs[1])
		if (point[0] >= box_mins[0] && point[0] < box_maxs[0])
			if (point[2] >= box_mins[2] && point[2] < box_maxs[2])
				return true;

	return false;
}

/*
===================
Math_PerspectiveToFrustum

Given a znear and zfar, converts from fovy and aspect to xmin, xmax, ymin and ymax
===================
*/
void Math_PerspectiveToFrustum(const double fovy, const double aspect, const double znear, const double zfar, double *xmin, double *xmax, double *ymin, double *ymax)
{
	*ymax = znear * tan(fovy * M_PI / 360.0);
	*ymin = -(*ymax);

	*xmin = (*ymin) * aspect;
	*xmax = (*ymax) * aspect;
}


/*
===================
Math_PopCount

"Variable-precision SWAR algorithm" for counting the number of set bits.
===================
*/
unsigned int Math_PopCount(unsigned int i)
{
     i = i - ((i >> 1) & 0x55555555);
     i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
     return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/*
============================================================================
Matrix Math

Note: After some operations, it's wise to do stuff like re-normalize normals, etc.
============================================================================
*/

/*
===================
Math_Matrix3x3From4x4Top

Copies the top-left of a 4x4 srcmatrix into a 3x3 destmatrix in column-major (OpenGL) order
TODO: SIMD
===================
*/
void Math_Matrix3x3From4x4Top(vec_t *destmatrix, const vec_t *srcmatrix)
{
	destmatrix[0] = srcmatrix[0];
	destmatrix[1] = srcmatrix[1];
	destmatrix[2] = srcmatrix[2];
	destmatrix[3] = srcmatrix[4];
	destmatrix[4] = srcmatrix[5];
	destmatrix[5] = srcmatrix[6];
	destmatrix[6] = srcmatrix[8];
	destmatrix[7] = srcmatrix[9];
	destmatrix[8] = srcmatrix[10];
}

/*
===================
Math_MatrixIsEqual3x3

Compares matrix a to matrix b
TODO: SIMD
===================
*/
int Math_MatrixIsEqual3x3(const vec_t *a, const vec_t *b)
{
	int i;

	for (i = 0; i < 9; i++)
		if (a[i] != b[i])
			return false;
	return true;
}

/*
===================
Math_MatrixCopy3x3

Copies matrix srcmatrix to matrix destmatrix in column-major (OpenGL) order
TODO: SIMD
===================
*/
void Math_MatrixCopy3x3(vec_t *destmatrix, const vec_t *srcmatrix)
{
	int i;

	for (i = 0; i < 9; i++)
		destmatrix[i] = srcmatrix[i];
}

/*
===================
Math_MatrixIsEqual4x4

Compares matrix a to matrix b
TODO: SIMD
===================
*/
int Math_MatrixIsEqual4x4(const vec_t *a, const vec_t *b)
{
	int i;

	for (i = 0; i < 16; i++)
		if (a[i] != b[i])
			return false;
	return true;
}

/*
===================
Math_MatrixCopy4x4

Copies matrix srcmatrix to matrix destmatrix in column-major (OpenGL) order
TODO: SIMD
===================
*/
void Math_MatrixCopy4x4(vec_t *destmatrix, const vec_t *srcmatrix)
{
	int i;

	for (i = 0; i < 16; i++)
		destmatrix[i] = srcmatrix[i];
}

/*
===================
Math_Matrix4x4ApplyToVector4

Multiplies matrix A by vector B, placing the results in vector destvector in column-major (OpenGL) order
TODO: SIMD
TODO: when using vec3_t, should the last value be zero or 1 by default? normalize by the last value when?
===================
*/
void Math_Matrix4x4ApplyToVector4(vec_t *destvector, vec_t *a, vec_t *b)
{
	/* column 0 */
	destvector[0] = a[0] * b[0] + a[4] * b[1] + a[8 ] * b[2] + a[12] * b[3];
	destvector[1] = a[1] * b[0] + a[5] * b[1] + a[9 ] * b[2] + a[13] * b[3];
	destvector[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
	destvector[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];
}

/*
===================
Math_MatrixMultiply4x4

Multiplies matrix A by matrix B, placing the results in matrix destmatrix in column-major (OpenGL) order
TODO: SIMD
===================
*/
void Math_MatrixMultiply4x4(vec_t *destmatrix, vec_t *a, vec_t *b)
{
	int i;
	vec_t tmp[16];

	/* column 0 */
	tmp[0] = a[0] * b[0] + a[4] * b[1] + a[8 ] * b[2] + a[12] * b[3];
	tmp[1] = a[1] * b[0] + a[5] * b[1] + a[9 ] * b[2] + a[13] * b[3];
	tmp[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
	tmp[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];

	/* column 1 */
	tmp[4] = a[0] * b[4] + a[4] * b[5] + a[8 ] * b[6] + a[12] * b[7];
	tmp[5] = a[1] * b[4] + a[5] * b[5] + a[9 ] * b[6] + a[13] * b[7];
	tmp[6] = a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7];
	tmp[7] = a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7];

	/* column 2 */
	tmp[8 ] = a[0] * b[8] + a[4] * b[9] + a[8 ] * b[10] + a[12] * b[11];
	tmp[9 ] = a[1] * b[8] + a[5] * b[9] + a[9 ] * b[10] + a[13] * b[11];
	tmp[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11];
	tmp[11] = a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11];

	/* column 3 */
	tmp[12] = a[0] * b[12] + a[4] * b[13] + a[8 ] * b[14] + a[12] * b[15];
	tmp[13] = a[1] * b[12] + a[5] * b[13] + a[9 ] * b[14] + a[13] * b[15];
	tmp[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15];
	tmp[15] = a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15];

	for (i = 0; i < 16; i++)
		destmatrix[i] = tmp[i];
}

/*
===================
Math_MatrixInverse4x4

Inverts matrix M, placing the result in matrix destmatrix in column-major (OpenGL) order
TODO: SIMD
===================
*/
void Math_MatrixInverse4x4(vec_t *destmatrix, vec_t *m)
{
	vec_t inv[16], det;
    int i;

    inv[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] + 
              m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - 
              m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + 
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] - 
             m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + 
             m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - 
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] + 
              m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - 
              m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + 
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] - 
              m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + 
              m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - 
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] - 
             m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + 
             m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - 
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] + 
              m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - 
              m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + 
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] - 
              m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + 
              m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - 
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] + 
               m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - 
               m[4]  * m[2] * m[13] - 
               m[12] * m[1] * m[6] + 
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + 
              m[1] * m[7] * m[10] + 
              m[5] * m[2] * m[11] - 
              m[5] * m[3] * m[10] - 
              m[9] * m[2] * m[7] + 
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - 
             m[0] * m[7] * m[10] - 
             m[4] * m[2] * m[11] + 
             m[4] * m[3] * m[10] + 
             m[8] * m[2] * m[7] - 
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + 
               m[0] * m[7] * m[9] + 
               m[4] * m[1] * m[11] - 
               m[4] * m[3] * m[9] - 
               m[8] * m[1] * m[7] + 
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - 
              m[0] * m[6] * m[9] - 
              m[4] * m[1] * m[10] + 
              m[4] * m[2] * m[9] + 
              m[8] * m[1] * m[6] - 
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (!det)
		Host_Error("Math_MatrixInverse4x4: determinant == zero, column-major: %f %f %f %f, %f %f %f %f, %f %f %f %f, %f %f %f %f\n", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);

    det = 1.0f / det;

    for (i = 0; i < 16; i++)
        destmatrix[i] = inv[i] * det;
}

/*
===================
Math_MatrixTranspose4x4

Transposes matrix M, placing the result in matrix destmatrix in column-major (OpenGL) order
TODO: SIMD
===================
*/
void Math_MatrixTranspose4x4(vec_t *destmatrix, vec_t *m)
{
	int i;
	vec_t tmp[16];

	tmp[0] = m[0];	tmp[1] = m[4];	tmp[2] = m[8];		tmp[3] = m[12];
	tmp[4] = m[1];	tmp[5] = m[5];	tmp[6] = m[9];		tmp[7] = m[13];
	tmp[8] = m[2];	tmp[9] = m[6];	tmp[10] = m[10];	tmp[11] = m[14];
	tmp[12] = m[3];	tmp[13] = m[7];	tmp[14] = m[11];	tmp[15] = m[15];

	for (i = 0; i < 16; i++)
		destmatrix[i] = tmp[i];
}

/*
===================
Math_MatrixPerspectiveFrustum4x4

Creates an perspective projection matrix in column-major (OpenGL) order
===================
*/
void Math_MatrixPerspectiveFrustum4x4(vec_t *destmatrix, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far)
{
	/* column 0 */
	destmatrix[0] = 2.f * near / (right - left); destmatrix[1] = 0; destmatrix[2] = 0; destmatrix[3] = 0;

	/* column 1 */
	destmatrix[4] = 0; destmatrix[5] = 2.f * near / (top - bottom); destmatrix[6] = 0; destmatrix[7] = 0;

	/* column 2 */
	destmatrix[8] = (right + left) / (right - left); destmatrix[9] = (top + bottom) / (top - bottom);
	destmatrix[10]= -((far + near) / (far - near)); destmatrix[11]= -1;

	/* column 3 */
	destmatrix[12]= 0; destmatrix[13]= 0; destmatrix[14]= -(2.f * far * near) / (far - near); destmatrix[15]= 0;
}

/*
===================
Math_MatrixOrtho4x4

Creates an orthogonal projection matrix in column-major (OpenGL) order
===================
*/
void Math_MatrixOrtho4x4(vec_t *destmatrix, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far)
{
	/* column 0 */
	destmatrix[0] = 2.f / (right - left); destmatrix[1] = 0; destmatrix[2] = 0; destmatrix[3] = 0;

	/* column 1 */
	destmatrix[4] = 0; destmatrix[5] = 2.f / (top - bottom); destmatrix[6] = 0; destmatrix[7] = 0;

	/* column 2 */
	destmatrix[8] = 0; destmatrix[9] = 0; destmatrix[10]= -2.f / (far - near); destmatrix[11]= 0;

	/* column 3 */
	destmatrix[12]= -((right + left) / (right - left)); destmatrix[13]= -((top + bottom) / (top - bottom));
	destmatrix[14]= -((far + near) / (far - near)); destmatrix[15]= 1;
}

/*
===================
Math_MatrixLookAt4x4

Creates a view matrix in column-major (OpenGL) order
===================
*/
void Math_MatrixLookAt4x4(vec_t *destmatrix, vec_t eyex, vec_t eyey, vec_t eyez, vec_t centerx, vec_t centery, vec_t centerz, vec_t upx, vec_t upy, vec_t upz)
{
    vec3_t forward, side, up;

    forward[0] = centerx - eyex;
    forward[1] = centery - eyey;
    forward[2] = centerz - eyez;

    up[0] = upx;
    up[1] = upy;
    up[2] = upz;

	Math_Vector3Normalize(forward);

    /* side = forward x up */
	Math_CrossProduct3(forward, up, side);
	Math_Vector3Normalize(side);

    /* recompute up as: up = side x forward */
	Math_CrossProduct3(side, forward, up);

	/* since we need the inverse transformation for the view matrix and this matrix is orthonormal, the transpose will be the inverse */
    destmatrix[0] = side[0];
    destmatrix[4] = side[1];
    destmatrix[8] = side[2];
	destmatrix[12] = 0;

    destmatrix[1] = up[0];
    destmatrix[5] = up[1];
    destmatrix[9] = up[2];
	destmatrix[13] = 0;

    destmatrix[2] = -forward[0];
    destmatrix[6] = -forward[1];
    destmatrix[10] = -forward[2];
	destmatrix[14] = 0;

	destmatrix[3] = 0;
	destmatrix[7] = 0;
	destmatrix[11] = 0;
	destmatrix[15] = 1;

	Math_MatrixTranslate4x4(destmatrix, -eyex, -eyey, -eyez);
}

/*
===================
Math_MatrixIdentity4x4

Creates an identity matrix in column-major (OpenGL) order
===================
*/
void Math_MatrixIdentity4x4(vec_t *destmatrix)
{
	/* column 0 */
	destmatrix[0] = 1; destmatrix[1] = 0; destmatrix[2] = 0; destmatrix[3] = 0;

	/* column 1 */
	destmatrix[4] = 0; destmatrix[5] = 1; destmatrix[6] = 0; destmatrix[7] = 0;

	/* column 2 */
	destmatrix[8] = 0; destmatrix[9] = 0; destmatrix[10]= 1; destmatrix[11]= 0;

	/* column 3 */
	destmatrix[12]= 0; destmatrix[13]= 0; destmatrix[14]= 0; destmatrix[15]= 1;
}

/*
===================
Math_MatrixTranslate4x4

Multiplies a matrix by a translation matrix in column-major (OpenGL) order from translation vector xyz
===================
*/
void Math_MatrixTranslate4x4(vec_t *destmatrix, vec_t x, vec_t y, vec_t z)
{
	vec_t tmp[16];

	Math_MatrixIdentity4x4(tmp);

	/* translation */
	tmp[12]= x;
	tmp[13]= y;
	tmp[14]= z;

	Math_MatrixMultiply4x4(destmatrix, destmatrix, tmp);
}

/*
===================
Math_MatrixScale4x4

Multiplies a matrix by a scaling matrix in column-major (OpenGL) order from scaling factor vector xyz
===================
*/
void Math_MatrixScale4x4(vec_t *destmatrix, vec_t x, vec_t y, vec_t z)
{
	vec_t tmp[16];

	Math_MatrixIdentity4x4(tmp);

	/* scaling */
	tmp[0] = x;
	tmp[5] = y;
	tmp[10]= z;

	Math_MatrixMultiply4x4(destmatrix, destmatrix, tmp);
}

/*
===================
Math_MatrixRotateX4x4

Multiplies a matrix by a rotation around the X axis matrix in column-major (OpenGL) order from rotation angle in degress
===================
*/
void Math_MatrixRotateX4x4(vec_t *destmatrix, vec_t angle)
{
	vec_t tmp[16];
	vec_t rad = Math_Deg2Rad(angle);

	Math_MatrixIdentity4x4(tmp);

	/* rotation around X */
	tmp[5] = cosf(rad);
	tmp[6] = sinf(rad);
	tmp[9] = -tmp[6];
	tmp[10] = tmp[5];

	Math_MatrixMultiply4x4(destmatrix, destmatrix, tmp);
}

/*
===================
Math_MatrixRotateY4x4

Multiplies a matrix by a rotation around the Y axis matrix in column-major (OpenGL) order from rotation angle in degress
===================
*/
void Math_MatrixRotateY4x4(vec_t *destmatrix, vec_t angle)
{
	vec_t tmp[16];
	vec_t rad = Math_Deg2Rad(angle);

	Math_MatrixIdentity4x4(tmp);

	/* rotation around Y */
	tmp[0] = cosf(rad);
	tmp[2] = -sinf(rad);
	tmp[8] = -tmp[2];
	tmp[10] = tmp[0];

	Math_MatrixMultiply4x4(destmatrix, destmatrix, tmp);
}

/*
===================
Math_MatrixRotateZ4x4

Multiplies a matrix by a rotation around the Z axis matrix in column-major (OpenGL) order from rotation angle in degress
===================
*/
void Math_MatrixRotateZ4x4(vec_t *destmatrix, vec_t angle)
{
	vec_t tmp[16];
	vec_t rad = Math_Deg2Rad(angle);

	Math_MatrixIdentity4x4(tmp);

	/* rotation around Z */
	tmp[0] = cosf(rad);
	tmp[1] = sinf(rad);
	tmp[4] = -tmp[1];
	tmp[5] = tmp[0];

	Math_MatrixMultiply4x4(destmatrix, destmatrix, tmp);
}

/*
===================
Math_MatrixRotateFromVectors4x4

Multiplies a matrix in column-major (OpenGL) order by a rotation defined by three normalized direction vectors
===================
*/
void Math_MatrixRotateFromVectors4x4(vec_t *destmatrix, vec3_t forward, vec3_t right, vec3_t up)
{
	vec_t tmp[16];

	Math_MatrixIdentity4x4(tmp);

	/* rotation */
	tmp[8] = -forward[0];
	tmp[9] = -forward[1];
	tmp[10] = -forward[2];

	tmp[0] = right[0];
	tmp[1] = right[1];
	tmp[2] = right[2];

	tmp[4] = up[0];
	tmp[5] = up[1];
	tmp[6] = up[2];

	Math_MatrixMultiply4x4(destmatrix, destmatrix, tmp);
}

/*
===================
Math_MatrixModel4x4

Creates a model transformation matrix in column-major (OpenGL) order
TODO: if using scale, also remember to take into consideration when getting aabb's, radius, setting a real normalmatrix when needed, etc
===================
*/
void Math_MatrixModel4x4(vec_t *destmatrix, vec3_t origin, vec3_t angles, vec3_t scale)
{
	Math_MatrixIdentity4x4(destmatrix);
	if (origin)
		Math_MatrixTranslate4x4(destmatrix, origin[0], origin[1], origin[2]);
	if (angles)
	{
		Math_MatrixRotateY4x4(destmatrix, angles[ANGLES_YAW]);
		Math_MatrixRotateX4x4(destmatrix, angles[ANGLES_PITCH]);
		Math_MatrixRotateZ4x4(destmatrix, angles[ANGLES_ROLL]);
	}
	if (scale)
		Math_MatrixScale4x4(destmatrix, scale[0], scale[1], scale[2]);
}