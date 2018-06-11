/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"
#ifndef DEDICATED_SERVER
#include <GL/glew.h>
#endif /* DEDICATED_SERVER */

/* based on IQM git at 2014-07-17, compare new revisions to that one for upgrading */
/* TODO: load mesh from one file and animations from another */
/* TODO: refactor this code */
/* TODO: see about lights and shadows in various modes (static, various dynamic) */
/* TODO: do not animate if not requested directly by server and bounding box for the frame is not on view (when requesting by the server, SEE IF REALLY NECESSARY if no player can see the model...) */
/* TODO: hide meshes on demand, to allow for various types of clothing and accessories (or even remove them all) */
/* TODO: vertex collapsing for LODS: automatic or pre-made */
/* TODO: collapse bones too when collapsing verts for LOD */

#define IQM_GPU_ANIMATION /* TODO: see why this doesn't work on some intel GPUs */

/*
============================================================================

IQM HEADER

============================================================================
*/

#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION 2

struct iqmheader
{
    char magic[16];
    unsigned int version;
    unsigned int filesize;
    unsigned int flags;
    unsigned int num_text, ofs_text;
    unsigned int num_meshes, ofs_meshes;
    unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
    unsigned int num_triangles, ofs_triangles, ofs_adjacency;
    unsigned int num_joints, ofs_joints;
    unsigned int num_poses, ofs_poses;
    unsigned int num_anims, ofs_anims;
    unsigned int num_frames, num_framechannels, ofs_frames, ofs_bounds;
    unsigned int num_comment, ofs_comment;
    unsigned int num_extensions, ofs_extensions;
};

struct iqmmesh
{
    unsigned int name;
    unsigned int material;
    unsigned int first_vertex, num_vertexes;
    unsigned int first_triangle, num_triangles;
};

enum
{
    IQM_POSITION     = 0,
    IQM_TEXCOORD     = 1,
    IQM_NORMAL       = 2,
    IQM_TANGENT      = 3,
    IQM_BLENDINDEXES = 4,
    IQM_BLENDWEIGHTS = 5,
    IQM_COLOR        = 6,
    IQM_CUSTOM       = 0x10
};

enum
{
    IQM_BYTE   = 0,
    IQM_UBYTE  = 1,
    IQM_SHORT  = 2,
    IQM_USHORT = 3,
    IQM_INT    = 4,
    IQM_UINT   = 5,
    IQM_HALF   = 6,
    IQM_FLOAT  = 7,
    IQM_DOUBLE = 8,
};

struct iqmtriangle
{
    unsigned int vertex[3];
};

struct iqmadjacency
{
    unsigned int triangle[3];
};

struct iqmjoint
{
    unsigned int name;
    int parent;
    float translate[3], rotate[4], scale[3];
};

struct iqmpose
{
    int parent;
    unsigned int mask;
    float channeloffset[10];
    float channelscale[10];
};

struct iqmanim
{
    unsigned int name;
    unsigned int first_frame, num_frames;
    float framerate;
    unsigned int flags;
};

enum
{
    IQM_LOOP = 1<<0
};

struct iqmvertexarray
{
    unsigned int type;
    unsigned int flags;
    unsigned int format;
    unsigned int size;
    unsigned int offset;
};

struct iqmbounds
{
    float bbmin[3], bbmax[3];
    float xyradius, radius; /* TODO FIXME: xyradius is not useful for us because IQM was created for the Quake Z-UP coordinate system, we should hack this in the exporter! */
};

/*
============================================================================

IQM utils - TODO: integrato into my stuff

============================================================================
*/

#ifdef NULL
#undef NULL
#endif
#define NULL 0

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef signed long long int llong;
typedef unsigned long long int ullong;

#ifdef swap
#undef swap
#endif
template<class T>
static inline void swap(T &a, T &b)
{
    T t = a;
    a = b;
    b = t;
}
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
template<class T>
static inline T max(T a, T b)
{
    return a > b ? a : b;
}
template<class T>
static inline T min(T a, T b)
{
    return a < b ? a : b;
}

template<class T> static inline T radians(T x) { return (x*M_PI)/180; }
template<class T> static inline T degrees(T x) { return (x*180)/M_PI; }

template<class T>
static inline T clamp(T val, T minval, T maxval)
{
    return max(minval, min(val, maxval));
}

static inline bool islittleendian() { union { int i; uchar b[sizeof(int)]; } conv; conv.i = 1; return conv.b[0] != 0; }
inline ushort endianswap16(ushort n) { return (n<<8) | (n>>8); }
inline uint endianswap32(uint n) { return (n<<24) | (n>>24) | ((n>>8)&0xFF00) | ((n<<8)&0xFF0000); }
template<class T> inline T endianswap(T n) { union { T t; uint i; } conv; conv.t = n; conv.i = endianswap32(conv.i); return conv.t; }
template<> inline ushort endianswap<ushort>(ushort n) { return endianswap16(n); }
template<> inline short endianswap<short>(short n) { return endianswap16(n); }
template<> inline uint endianswap<uint>(uint n) { return endianswap32(n); }
template<> inline int endianswap<int>(int n) { return endianswap32(n); }
template<class T> inline void endianswap(T *buf, int len) { for(T *end = &buf[len]; buf < end; buf++) *buf = endianswap(*buf); }
template<class T> inline T lilswap(T n) { return islittleendian() ? n : endianswap(n); }
template<class T> inline void lilswap(T *buf, int len) { if(!islittleendian()) endianswap(buf, len); }
template<class T> inline T bigswap(T n) { return islittleendian() ? endianswap(n) : n; }
template<class T> inline void bigswap(T *buf, int len) { if(islittleendian()) endianswap(buf, len); }

template<class T> T getval(FILE *f) { T n; return fread(&n, 1, sizeof(n), f) == sizeof(n) ? n : 0; }
template<class T> T getlil(FILE *f) { return lilswap(getval<T>(f)); }
template<class T> T getbig(FILE *f) { return bigswap(getval<T>(f)); }

/*
============================================================================

IQM math stuff - TODO: integrato into my math stuff

============================================================================
*/

struct Vec4;

struct Vec3
{
    union
    {
        struct { float x, y, z; };
        float v[3];
    };

    Vec3() {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    explicit Vec3(const float *v) : x(v[0]), y(v[1]), z(v[2]) {}
    explicit Vec3(const Vec4 &v);

    float &operator[](int i) { return v[i]; }
    float operator[](int i) const { return v[i]; }

    bool operator==(const Vec3 &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3 &o) const { return x != o.x || y != o.y || z != o.z; }

    Vec3 operator+(const Vec3 &o) const { return Vec3(x+o.x, y+o.y, z+o.z); }
    Vec3 operator-(const Vec3 &o) const { return Vec3(x-o.x, y-o.y, z-o.z); }
    Vec3 operator+(float k) const { return Vec3(x+k, y+k, z+k); }
    Vec3 operator-(float k) const { return Vec3(x-k, y-k, z-k); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    Vec3 operator*(const Vec3 &o) const { return Vec3(x*o.x, y*o.y, z*o.z); }
    Vec3 operator/(const Vec3 &o) const { return Vec3(x/o.x, y/o.y, z/o.z); }
    Vec3 operator*(float k) const { return Vec3(x*k, y*k, z*k); }
    Vec3 operator/(float k) const { return Vec3(x/k, y/k, z/k); }

    Vec3 &operator+=(const Vec3 &o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3 &operator-=(const Vec3 &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3 &operator+=(float k) { x += k; y += k; z += k; return *this; }
    Vec3 &operator-=(float k) { x -= k; y -= k; z -= k; return *this; }
    Vec3 &operator*=(const Vec3 &o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
    Vec3 &operator/=(const Vec3 &o) { x /= o.x; y /= o.y; z /= o.z; return *this; }
    Vec3 &operator*=(float k) { x *= k; y *= k; z *= k; return *this; }
    Vec3 &operator/=(float k) { x /= k; y /= k; z /= k; return *this; }

    float dot(const Vec3 &o) const { return x*o.x + y*o.y + z*o.z; }
    float magnitude() const { return sqrtf(dot(*this)); }
    float squaredlen() const { return dot(*this); }
    float dist(const Vec3 &o) const { return (*this - o).magnitude(); }
    Vec3 normalize() const { return *this * (1.0f / magnitude()); }
    Vec3 cross(const Vec3 &o) const { return Vec3(y*o.z-z*o.y, z*o.x-x*o.z, x*o.y-y*o.x); }
    Vec3 reflect(const Vec3 &n) const { return *this - n*2.0f*dot(n); }
    Vec3 project(const Vec3 &n) const { return *this - n*dot(n); }
};

struct Vec4
{
    union
    {
        struct { float x, y, z, w; };
        float v[4];
    };

    Vec4() {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    explicit Vec4(const Vec3 &p, float w = 0) : x(p.x), y(p.y), z(p.z), w(w) {}
    explicit Vec4(const float *v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {}

    float &operator[](int i)       { return v[i]; }
    float  operator[](int i) const { return v[i]; }

    bool operator==(const Vec4 &o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=(const Vec4 &o) const { return x != o.x || y != o.y || z != o.z || w != o.w; }

    Vec4 operator+(const Vec4 &o) const { return Vec4(x+o.x, y+o.y, z+o.z, w+o.w); }
    Vec4 operator-(const Vec4 &o) const { return Vec4(x-o.x, y-o.y, z-o.z, w-o.w); }
    Vec4 operator+(float k) const { return Vec4(x+k, y+k, z+k, w+k); }
    Vec4 operator-(float k) const { return Vec4(x-k, y-k, z-k, w-k); }
    Vec4 operator-() const { return Vec4(-x, -y, -z, -w); }
    Vec4 neg3() const { return Vec4(-x, -y, -z, w); }
    Vec4 operator*(float k) const { return Vec4(x*k, y*k, z*k, w*k); }
    Vec4 operator/(float k) const { return Vec4(x/k, y/k, z/k, w/k); }
    Vec4 addw(float f) const { return Vec4(x, y, z, w + f); }

    Vec4 &operator+=(const Vec4 &o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
    Vec4 &operator-=(const Vec4 &o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
    Vec4 &operator+=(float k) { x += k; y += k; z += k; w += k; return *this; }
    Vec4 &operator-=(float k) { x -= k; y -= k; z -= k; w -= k; return *this; }
    Vec4 &operator*=(float k) { x *= k; y *= k; z *= k; w *= k; return *this; }
    Vec4 &operator/=(float k) { x /= k; y /= k; z /= k; w /= k; return *this; }

    float dot3(const Vec4 &o) const { return x*o.x + y*o.y + z*o.z; }
    float dot3(const Vec3 &o) const { return x*o.x + y*o.y + z*o.z; }
    float dot(const Vec4 &o) const { return dot3(o) + w*o.w; }
    float dot(const Vec3 &o) const  { return x*o.x + y*o.y + z*o.z + w; }
    float magnitude() const  { return sqrtf(dot(*this)); }
    float magnitude3() const { return sqrtf(dot3(*this)); }
    Vec4 normalize() const { return *this * (1.0f / magnitude()); }
    Vec3 cross3(const Vec4 &o) const { return Vec3(y*o.z-z*o.y, z*o.x-x*o.z, x*o.y-y*o.x); }
    Vec3 cross3(const Vec3 &o) const { return Vec3(y*o.z-z*o.y, z*o.x-x*o.z, x*o.y-y*o.x); }
};

inline Vec3::Vec3(const Vec4 &v) : x(v.x), y(v.y), z(v.z) {}

struct Matrix3x3;
struct Matrix3x4;

struct Quat : Vec4
{
    Quat() {}
    Quat(float x, float y, float z, float w) : Vec4(x, y, z, w) {}
    Quat(float angle, const Vec3 &axis)
    {
        float s = sinf(0.5f*angle);
        x = s*axis.x;
        y = s*axis.y;
        z = s*axis.z;
        w = cosf(0.5f*angle);
    }
    explicit Quat(const Vec3 &v) : Vec4(v.x, v.y, v.z, -sqrtf(max(1.0f - v.squaredlen(), 0.0f))) {}
    explicit Quat(const float *v) : Vec4(v[0], v[1], v[2], v[3]) {}
    explicit Quat(const Matrix3x3 &m) { convertmatrix(m); }
    explicit Quat(const Matrix3x4 &m) { convertmatrix(m); }

    void restorew()
    {
        w = -sqrtf(max(1.0f - dot3(*this), 0.0f));
    }

    Quat normalize() const { return *this * (1.0f / magnitude()); }

    Quat operator*(float k) const { return Quat(x*k, y*k, z*k, w*k); }
    Quat &operator*=(float k) { return (*this = *this * k); }

    Quat operator*(const Quat &o) const
    {
        return Quat(w*o.x + x*o.w + y*o.z - z*o.y,
                    w*o.y - x*o.z + y*o.w + z*o.x,
                    w*o.z + x*o.y - y*o.x + z*o.w,
                    w*o.w - x*o.x - y*o.y - z*o.z);
    }
    Quat &operator*=(const Quat &o) { return (*this = *this * o); }

    Quat operator+(const Vec4 &o) const { return Quat(x+o.x, y+o.y, z+o.z, w+o.w); }
    Quat &operator+=(const Vec4 &o) { return (*this = *this + o); }
    Quat operator-(const Vec4 &o) const { return Quat(x-o.x, y-o.y, z-o.z, w-o.w); }
    Quat &operator-=(const Vec4 &o) { return (*this = *this - o); }

    Quat operator-() const { return Quat(-x, -y, -z, w); }

    void flip() { x = -x; y = -y; z = -z; w = -w; }

    Vec3 transform(const Vec3 &p) const
    {
        return p + cross3(cross3(p) + p*w)*2.0f;
    }

    void calcangleaxis(float &angle, Vec3 &axis)
    {
        float rr = dot3(*this);
        if(rr > 0)
        {
            angle = 2*acosf(w);
            axis = Vec3(*this) * (1 / rr);
        }
        else { angle = 0; axis = Vec3(0, 0, 1); }
    }

    template<class M>
    void convertmatrix(const M &m)
    {
        float trace = m.a.x + m.b.y + m.c.z;
        if(trace>0)
        {
            float r = sqrtf(1 + trace), inv = 0.5f/r;
            w = 0.5f*r;
            x = (m.c.y - m.b.z)*inv;
            y = (m.a.z - m.c.x)*inv;
            z = (m.b.x - m.a.y)*inv;
        }
        else if(m.a.x > m.b.y && m.a.x > m.c.z)
        {
            float r = sqrtf(1 + m.a.x - m.b.y - m.c.z), inv = 0.5f/r;
            x = 0.5f*r;
            y = (m.b.x + m.a.y)*inv;
            z = (m.a.z + m.c.x)*inv;
            w = (m.c.y - m.b.z)*inv;
        }
        else if(m.b.y > m.c.z)
        {
            float r = sqrtf(1 + m.b.y - m.a.x - m.c.z), inv = 0.5f/r;
            x = (m.b.x + m.a.y)*inv;
            y = 0.5f*r;
            z = (m.c.y + m.b.z)*inv;
            w = (m.a.z - m.c.x)*inv;
        }
        else
        {
            float r = sqrtf(1 + m.c.z - m.a.x - m.b.y), inv = 0.5f/r;
            x = (m.a.z + m.c.x)*inv;
            y = (m.c.y + m.b.z)*inv;
            z = 0.5f*r;
            w = (m.b.x - m.a.y)*inv;
        }
    }
};

struct Matrix3x3
{
    Vec3 a, b, c;

    Matrix3x3() {}
    Matrix3x3(const Vec3 &a, const Vec3 &b, const Vec3 &c) : a(a), b(b), c(c) {}
    explicit Matrix3x3(const Quat &q) { convertquat(q); }
    explicit Matrix3x3(const Quat &q, const Vec3 &scale)
    {
        convertquat(q);
        a *= scale;
        b *= scale;
        c *= scale;
    }

    void convertquat(const Quat &q)
    {
        float x = q.x, y = q.y, z = q.z, w = q.w,
              tx = 2*x, ty = 2*y, tz = 2*z,
              txx = tx*x, tyy = ty*y, tzz = tz*z,
              txy = tx*y, txz = tx*z, tyz = ty*z,
              twx = w*tx, twy = w*ty, twz = w*tz;
        a = Vec3(1 - (tyy + tzz), txy - twz, txz + twy);
        b = Vec3(txy + twz, 1 - (txx + tzz), tyz - twx);
        c = Vec3(txz - twy, tyz + twx, 1 - (txx + tyy));
    }

    Matrix3x3 operator*(const Matrix3x3 &o) const
    {
        return Matrix3x3(
            o.a*a.x + o.b*a.y + o.c*a.z,
            o.a*b.x + o.b*b.y + o.c*b.z,
            o.a*c.x + o.b*c.y + o.c*c.z);
    }
    Matrix3x3 &operator*=(const Matrix3x3 &o) { return (*this = *this * o); }

    void transpose(const Matrix3x3 &o)
    {
        a = Vec3(o.a.x, o.b.x, o.c.x);
        b = Vec3(o.a.y, o.b.y, o.c.y);
        c = Vec3(o.a.z, o.b.z, o.c.z);
    }
    void transpose() { transpose(Matrix3x3(*this)); }

    Vec3 transform(const Vec3 &o) const { return Vec3(a.dot(o), b.dot(o), c.dot(o)); }
    Vec3 transposedtransform(const Vec3 &o) const { return a*o.x + b*o.y + c*o.z; }
};

struct Matrix3x4
{
    Vec4 a, b, c;

    Matrix3x4() {}
    Matrix3x4(const Vec4 &a, const Vec4 &b, const Vec4 &c) : a(a), b(b), c(c) {}
    explicit Matrix3x4(const Matrix3x3 &rot, const Vec3 &trans)
        : a(Vec4(rot.a, trans.x)), b(Vec4(rot.b, trans.y)), c(Vec4(rot.c, trans.z))
    {
    }
    explicit Matrix3x4(const Quat &rot, const Vec3 &trans)
    {
        *this = Matrix3x4(Matrix3x3(rot), trans);
    }
    explicit Matrix3x4(const Quat &rot, const Vec3 &trans, const Vec3 &scale)
    {
        *this = Matrix3x4(Matrix3x3(rot, scale), trans);
    }

    Matrix3x4 operator*(float k) const { return Matrix3x4(*this) *= k; }
    Matrix3x4 &operator*=(float k)
    {
        a *= k;
        b *= k;
        c *= k;
        return *this;
    }

    Matrix3x4 operator+(const Matrix3x4 &o) const { return Matrix3x4(*this) += o; }
    Matrix3x4 &operator+=(const Matrix3x4 &o)
    {
        a += o.a;
        b += o.b;
        c += o.c;
        return *this;
    }

    void invert(const Matrix3x4 &o)
    {
        Matrix3x3 invrot(Vec3(o.a.x, o.b.x, o.c.x), Vec3(o.a.y, o.b.y, o.c.y), Vec3(o.a.z, o.b.z, o.c.z));
        invrot.a /= invrot.a.squaredlen();
        invrot.b /= invrot.b.squaredlen();
        invrot.c /= invrot.c.squaredlen();
        Vec3 trans(o.a.w, o.b.w, o.c.w);
        a = Vec4(invrot.a, -invrot.a.dot(trans));
        b = Vec4(invrot.b, -invrot.b.dot(trans));
        c = Vec4(invrot.c, -invrot.c.dot(trans));
    }
    void invert() { invert(Matrix3x4(*this)); }

    Matrix3x4 operator*(const Matrix3x4 &o) const
    {
        return Matrix3x4(
            (o.a*a.x + o.b*a.y + o.c*a.z).addw(a.w),
            (o.a*b.x + o.b*b.y + o.c*b.z).addw(b.w),
            (o.a*c.x + o.b*c.y + o.c*c.z).addw(c.w));
    }
    Matrix3x4 &operator*=(const Matrix3x4 &o) { return (*this = *this * o); }

    Vec3 transform(const Vec3 &o) const { return Vec3(a.dot(o), b.dot(o), c.dot(o)); }
    Vec3 transformnormal(const Vec3 &o) const { return Vec3(a.dot3(o), b.dot3(o), c.dot3(o)); }
};

/*
============================================================================

IQM Model Functions

============================================================================
*/

#ifdef IQM_GPU_ANIMATION
bool UBOtested = false;
bool hasUBO = false; /* TODO: TEST WITHOUT UBOs */

void loadexts()
{
#ifndef DEDICATED_SERVER
    const char *version = (const char *)glGetString(GL_VERSION);
    if(strcmp(version, "2.1") < 0) Sys_Error("OpenGL version 2.1 required, found version: %s", version);

	if (!UBOtested)
	{
		if (glewIsSupported("GL_ARB_uniform_buffer_object"))
		{
			hasUBO = true;
			Sys_Printf("OpenGL extension GL_ARB_uniform_buffer_object supported by this system.\n");
		}
		else
		{
			hasUBO = false;
			Sys_Printf("OpenGL extension GL_ARB_uniform_buffer_object not supported by this system, disabling dependencies...\n");
		}
		UBOtested = true;
	}
#endif /* DEDICATED_SERVER */
}
#endif /* IQM_GPU_ANIMATION */

/* m == in-memory */
typedef struct m_model_iqm_s {
	unsigned char	*rawdata;
	iqmheader		*hdr;
	char			name[MAX_MODEL_NAME];

	/* TODO: meshdata and animdata separated because they can be from different files? right now both will point at rawdata */
	// Note that while this demo stores pointer directly into mesh data in a buffer
	// of the entire IQM file's data, it is recommended that you copy the data and
	// convert it into a more suitable internal representation for whichever 3D
	// engine you use.
	unsigned char *meshdata, *animdata;
	float *inposition, *innormal, *intangent, *intexcoord;
	unsigned char *inblendindex, *inblendweight;
#ifndef IQM_GPU_ANIMATION
	unsigned char *incolor; /* TODO: why */
#endif
	int nummeshes, numtris, numverts, numjoints, numframes, numanims;
	iqmtriangle *tris;
#ifndef IQM_GPU_ANIMATION
	iqmtriangle *adjacency;
#endif
	iqmmesh *meshes;
	texture_t **textures;
	iqmjoint *joints;
	iqmpose *poses;
	iqmanim *anims;
	iqmbounds *bounds;
	Matrix3x4 *baseframe, *inversebaseframe, *frames;

	int *jointmasks[ANIMATION_MAX_BLENDED_FRAMES]; /* TODO: use bitfields */

	int modeltags_enabled[NUM_MODEL_TAGS]; /* if this model has this tag */
	Vec3 modeltags_base[NUM_MODEL_TAGS]; /* model tag origin in the base frame */
	unsigned int modeltags_joint[NUM_MODEL_TAGS]; /* joint which transforms this model tag */

	unsigned int animation_cache[NUM_ANIMATIONS]; /* for indexing with a int instead of a string */
	int torso_joint; /* rotate model torso only if we have pitch and a Torso joint */

	/* entity specific stuff, only valid if the model has been animated at least once for a given entity TODO: using too much memory and allocating memory on-the-fly! specify which entities will use which models and allocate everything at precache time? */
	vec_t lastframes[MAX_EDICTS][ANIMATION_MAX_BLENDED_FRAMES];
	float *outposition[MAX_EDICTS], *outnormal[MAX_EDICTS], *outtangent[MAX_EDICTS], *outbitangent[MAX_EDICTS];
	Matrix3x4 *outframe[MAX_EDICTS];
	vec_t *lastmodelmatrix[MAX_EDICTS];
	vec_t *modelmatrix_origin[MAX_EDICTS], *modelmatrix_angles[MAX_EDICTS];
	/* TODO: frame number cache (even if animation didn't change for the current frame - just to know if the animation function was called to let the render and gettag error! */
#ifndef DEDICATED_SERVER
	int vbo_id;
	GLuint ubo;
	GLint ubosize, bonematsoffset;

	/* TODO: unloading:
	if(ubo) glDeleteBuffers_(1, &ubo);
	*/
#endif /* DEDICATED_SERVER */
} m_model_iqm_t;

void loadiqmmeshes(const char *path, m_model_iqm_t *iqm)
{
	iqmheader *hdr = iqm->hdr;
	unsigned char *buf = iqm->rawdata;

    lilswap((uint *)&buf[hdr->ofs_vertexarrays], hdr->num_vertexarrays*sizeof(iqmvertexarray)/sizeof(uint));
    lilswap((uint *)&buf[hdr->ofs_triangles], hdr->num_triangles*sizeof(iqmtriangle)/sizeof(uint));
	lilswap((uint *)&buf[hdr->ofs_meshes], hdr->num_meshes*sizeof(iqmmesh)/sizeof(uint));
    lilswap((uint *)&buf[hdr->ofs_joints], hdr->num_joints*sizeof(iqmjoint)/sizeof(uint));
#ifndef IQM_GPU_ANIMATION
    if(hdr->ofs_adjacency) lilswap((uint *)&buf[hdr->ofs_adjacency], hdr->num_triangles*sizeof(iqmtriangle)/sizeof(uint));
#endif

	iqm->meshdata = buf;
    iqm->nummeshes = hdr->num_meshes;
    iqm->numtris = hdr->num_triangles;
    iqm->numverts = hdr->num_vertexes;
    iqm->numjoints = hdr->num_joints;
#ifdef IQM_GPU_ANIMATION
	if (iqm->numjoints >= SHADER_MAX_BONES)
		Sys_Error("%s: too many joints: %d (MAX %d)\n", path, iqm->numjoints, SHADER_MAX_BONES);
#endif
    iqm->textures = (texture_t **)Sys_MemAlloc(&mdl_mem, sizeof(texture_t *) * iqm->nummeshes, "modeldata");
    memset(iqm->textures, 0, iqm->nummeshes*sizeof(texture_t *));

    const char *str = hdr->ofs_text ? (char *)&buf[hdr->ofs_text] : "";
    iqmvertexarray *vas = (iqmvertexarray *)&buf[hdr->ofs_vertexarrays];
    for(int i = 0; i < (int)hdr->num_vertexarrays; i++)
    {
        iqmvertexarray &va = vas[i];
        switch(va.type)
        {
		case IQM_POSITION: if(va.format != IQM_FLOAT || va.size != 3) Sys_Error("loadiqmmeshes: %s: va.format != IQM_FLOAT || va.size != 3\n", path); iqm->inposition = (float *)&buf[va.offset]; lilswap(iqm->inposition, 3*hdr->num_vertexes); break;
        case IQM_NORMAL: if(va.format != IQM_FLOAT || va.size != 3) Sys_Error("loadiqmmeshes: %s: va.format != IQM_FLOAT || va.size != 3\n", path); iqm->innormal = (float *)&buf[va.offset]; lilswap(iqm->innormal, 3*hdr->num_vertexes); break;
        case IQM_TANGENT: if(va.format != IQM_FLOAT || va.size != 4) Sys_Error("loadiqmmeshes: %s: va.format != IQM_FLOAT || va.size != 4\n", path); iqm->intangent = (float *)&buf[va.offset]; lilswap(iqm->intangent, 4*hdr->num_vertexes); break;
        case IQM_TEXCOORD: if(va.format != IQM_FLOAT || va.size != 2) Sys_Error("loadiqmmeshes: %s: va.format != IQM_FLOAT || va.size != 2\n", path); iqm->intexcoord = (float *)&buf[va.offset]; lilswap(iqm->intexcoord, 2*hdr->num_vertexes); break;
        case IQM_BLENDINDEXES: if(va.format != IQM_UBYTE || va.size != 4) Sys_Error("loadiqmmeshes: %s: va.format != IQM_UBYTE || va.size != 4\n", path); iqm->inblendindex = (uchar *)&buf[va.offset]; break;
        case IQM_BLENDWEIGHTS: if(va.format != IQM_UBYTE || va.size != 4) Sys_Error("loadiqmmeshes: %s: va.format != IQM_UBYTE || va.size != 4\n", path); iqm->inblendweight = (uchar *)&buf[va.offset]; break;
#ifndef IQM_GPU_ANIMATION /* TODO: have this in GPU animation */
        case IQM_COLOR: if(va.format != IQM_UBYTE || va.size != 4) Sys_Error("loadiqmmeshes: %s: va.format != IQM_UBYTE || va.size != 4\n", path); iqm->incolor = (uchar *)&buf[va.offset]; break;
#endif
        }
    }
    iqm->tris = (iqmtriangle *)&buf[hdr->ofs_triangles];
    iqm->meshes = (iqmmesh *)&buf[hdr->ofs_meshes];
    iqm->joints = (iqmjoint *)&buf[hdr->ofs_joints];
#ifndef IQM_GPU_ANIMATION
    if(hdr->ofs_adjacency) iqm->adjacency = (iqmtriangle *)&buf[hdr->ofs_adjacency];
#endif

	/* invert triangle vertex order TODO FIXME: FIND OUT WHY WE ARE DOING THIS AND IF ANYTHING ELSE IS AFFECTED AND NEEDS INVERTING!!! */
	for (int i = 0; i < (int)iqm->numtris; i++)
	{
		unsigned int temp = iqm->tris[i].vertex[0];
		iqm->tris[i].vertex[0] = iqm->tris[i].vertex[2];
		iqm->tris[i].vertex[2] =  temp;
	}

    iqm->baseframe = (Matrix3x4 *)Sys_MemAlloc(&mdl_mem, sizeof(Matrix3x4) * hdr->num_joints, "modeldata");
    iqm->inversebaseframe = (Matrix3x4 *)Sys_MemAlloc(&mdl_mem, sizeof(Matrix3x4) * hdr->num_joints, "modeldata");
    for(int i = 0; i < (int)hdr->num_joints; i++)
    {
        iqmjoint &j = iqm->joints[i];
        iqm->baseframe[i] = Matrix3x4(Quat(j.rotate).normalize(), Vec3(j.translate), Vec3(j.scale));
        iqm->inversebaseframe[i].invert(iqm->baseframe[i]);
        if(j.parent >= 0)
        {
            iqm->baseframe[i] = iqm->baseframe[j.parent] * iqm->baseframe[i];
            iqm->inversebaseframe[i] *= iqm->inversebaseframe[j.parent];
        }
    }

	int marker = Sys_MemLowMark(&tmp_mem);
	/* load joint masks */
	for (int i = 0; i < ANIMATION_MAX_BLENDED_FRAMES; i++)
		iqm->jointmasks[i] = (int *)Sys_MemAlloc(&mdl_mem, sizeof(int) * hdr->num_joints, "modeldata"); /* comes with all zeros from the allocation function */
	char maskpath[MAX_PATH];
	unsigned char *maskdata;
	unsigned char *maskdataend;
	int masksize;
	Sys_Snprintf(maskpath, MAX_PATH, "%s_jointmasks.txt", path);

	if ((masksize = Host_FSLoadBinaryFile(maskpath, &tmp_mem, "modeldata", &maskdata, true)) != -1)
	{
		int masklines = 0;
		maskdataend = maskdata + masksize;
		while (1)
		{
			char joint[MAX_MODEL_NAME]; /* TODO: which is the biggest string size in iqm? unlimited? */
			char slot_name[MAX_MODEL_NAME]; /* animation slot in which this joint will be enabled TODO: size spec */
			if (maskdata >= maskdataend)
				break;
#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
			if (!Sys_Sscanf_s((const char *)maskdata, "%s %s", joint, slot_name)) /* TODO: permit blending ratio */
#else
			if (!Sys_Sscanf_s((const char *)maskdata, "%s %s", joint, sizeof(joint), slot_name, sizeof(slot_name))) /* TODO: permit blending ratio */
#endif /* __GNUC__ */
				break;
			maskdata = Host_CMDSkipBlank(maskdata);
			masklines++;

			int slot_enabled;
			for (slot_enabled = 0; slot_enabled < ANIMATION_MAX_BLENDED_FRAMES; slot_enabled++)
			{
				if (!strncmp(slot_name, animation_slot_names[slot_enabled], sizeof(slot_name))) /* TODO: sizes, overflows */
					break;
			}
			if (slot_enabled == ANIMATION_MAX_BLENDED_FRAMES)
				Sys_Error("%s: animation masks: %s, animation slot %s (for joint %s) not found at line %d\n", path, maskpath, slot_name, joint, masklines); /* TODO: line counting NOT accurate */

			if (slot_enabled < 0 || slot_enabled >= ANIMATION_MAX_BLENDED_FRAMES)
				Sys_Error("%s: animation masks: %s, animation slot %s (%d) (for joint %s) out of range at line %d\n", path, maskpath, slot_name, slot_enabled, joint, masklines); /* TODO: line counting NOT accurate */

			/* TODO: slow */
			unsigned int i;
			for (i = 0; i < hdr->num_joints; i++)
			{
				if (!strncmp(joint, &str[iqm->joints[i].name], sizeof(joint))) /* TODO: sizes, overflows */
				{
					/* TODO CONSOLEDEBUG Sys_Printf("%s: animation masks: %s, line %d, joint %s enabled for slot %d\n", path, maskpath, masklines, joint, slot_enabled); */ /* TODO: line counting NOT accurate */
					iqm->jointmasks[slot_enabled][i] = true;
					break;
				}
			}
			/* TODO CONSOLEDEBUG if (i == hdr->num_joints)
				Sys_Error("%s: animation masks: %s, joint %s not found at line %d\n", path, maskpath, joint, masklines); */ /* TODO: line counting NOT accurate */
		}
		/* TODO CONSOLEDEBUG Sys_Printf("%s: loaded animation masks: %s, %d lines\n", path, maskpath, masklines); */
	}
	else
	{
		/* TODO CONSOLEDEBUG Sys_Printf("%s: no animation masks found (%s doesn't exist), defaulting to all enabled for the first slot (%s) only\n", path, maskpath, animation_slot_names[0]); */
		for (unsigned int i = 0; i < hdr->num_joints; i++)
			iqm->jointmasks[0][i] = true;
	}
	/* load tags */
	char tagpath[MAX_PATH];
	unsigned char *tagdata;
	unsigned char *tagdataend;
	int tagsize;
	Sys_Snprintf(tagpath, MAX_PATH, "%s_tags.txt", path);

	if ((tagsize = Host_FSLoadBinaryFile(tagpath, &tmp_mem, "modeldata", &tagdata, true)) != -1)
	{
		int taglines = 0;
		tagdataend = tagdata + tagsize;
		while (1)
		{
			char tag_name[MAX_MODEL_NAME]; /* which tag is defined by the current line in the file TODO: size spec */
			vec3_t tag_origin;
			char tag_joint[MAX_MODEL_NAME]; /* joint to which this tag is attached TODO: which is the biggest string size in iqm? unlimited? */

			if (tagdata >= tagdataend)
				break;
#ifdef __GNUC__ /* TODO FIXME: SECURITY HAZARD */
			if (!Sys_Sscanf_s((const char *)tagdata, "%s %f %f %f %s", tag_name, &tag_origin[0], &tag_origin[1], &tag_origin[2], tag_joint)) /* TODO: permit tag weight, permit weight to more than one joint */
#else
			if (!Sys_Sscanf_s((const char *)tagdata, "%s %f %f %f %s", tag_name, sizeof(tag_name), &tag_origin[0], &tag_origin[1], &tag_origin[2], tag_joint, sizeof(tag_joint))) /* TODO: permit tag weight, permit weight to more than one joint */
#endif /* __GNUC__ */
				break;
			tagdata = Host_CMDSkipBlank(tagdata);
			taglines++;

			int tag_idx;
			for (tag_idx = 0; tag_idx < NUM_MODEL_TAGS; tag_idx++)
			{
				if (!strncmp(tag_name, model_tag_names[tag_idx], sizeof(tag_name))) /* TODO: sizes, overflows */
					break;
			}
			if (tag_idx == NUM_MODEL_TAGS)
				Sys_Error("%s: tags: %s, tag %s (at %f %f %f, for joint %s) not found at line %d\n", path, tagpath, tag_name, tag_origin[0], tag_origin[1], tag_origin[2], tag_joint, taglines); /* TODO: line counting NOT accurate */

			if (tag_idx < 0 || tag_idx >= NUM_MODEL_TAGS)
				Sys_Error("%s: tags: %s, tag %s (%d) (at %f %f %f, for joint %s) out of range at line %d\n", path, tagpath, tag_name, tag_idx, tag_origin[0], tag_origin[1], tag_origin[2], tag_joint, taglines); /* TODO: line counting NOT accurate */

			/* TODO: slow */
			unsigned int i;
			for (i = 0; i < hdr->num_joints; i++)
			{
				if (!strncmp(tag_joint, &str[iqm->joints[i].name], sizeof(tag_joint))) /* TODO: sizes, overflows */
				{
					/* TODO CONSOLEDEBUG Sys_Printf("%s: tags: %s, tag %s (%d) (at %f %f %f, for joint %s) enabled at line %d\n", path, tagpath, tag_name, tag_idx, tag_origin[0], tag_origin[1], tag_origin[2], tag_joint, taglines); */ /* TODO: line counting NOT accurate */
					iqm->modeltags_enabled[tag_idx] = true;
					iqm->modeltags_base[tag_idx].x = tag_origin[0];
					iqm->modeltags_base[tag_idx].y = tag_origin[1];
					iqm->modeltags_base[tag_idx].z = tag_origin[2];
					iqm->modeltags_joint[tag_idx] = i;
					break;
				}
			}
			/* TODO CONSOLEDEBUG if (i == hdr->num_joints)
				Sys_Error("%s: tags: %s, tag %s (%d) (at %f %f %f, for joint %s) joint not found at line %d\n", path, tagpath, tag_name, tag_idx, tag_origin[0], tag_origin[1], tag_origin[2], tag_joint, taglines); */ /* TODO: line counting NOT accurate */
		}
		/* TODO CONSOLEDEBUG Sys_Printf("%s: loaded tags: %s, %d lines\n", path, tagpath, taglines); */
	}
	else
	{
		/* TODO CONSOLEDEBUG Sys_Printf("%s: no tags found (%s doesn't exist), disabling tags for this model\n", path, maskpath); */
		/* already cleared */
	}
	Sys_MemFreeToLowMark(&tmp_mem, marker);

    for(int i = 0; i < (int)hdr->num_meshes; i++)
    {
        iqmmesh &m = iqm->meshes[i];
		/* TODO CONSOLEDEBUG Sys_Printf("%s: loaded mesh: %s (%u vertexes, %u triangles)\n", path, &str[m.name], m.num_vertexes, m.num_triangles); */
    }

	/* TODO CONSOLEDEBUG Sys_Printf("%s: %u joints:", path, hdr->num_joints); */
	iqm->torso_joint = -1;
	for(int i = 0; i < (int)hdr->num_joints; i++)
    {
		/* TODO CONSOLEDEBUG Sys_Printf(" %s", &str[iqm->joints[i].name]); */

		/* rotate model torso only if we have pitch and a Torso joint */
		if (!strcmp(&str[iqm->joints[i].name], "TorsoPitchJoint")) /* TODO: sizes, overflows */
			iqm->torso_joint = i;
    }
	/* TODO CONSOLEDEBUG Sys_Printf("\n"); */
}

void loadiqmanims(const char *path, m_model_iqm_t *iqm)
{
	iqmheader *hdr = iqm->hdr;
	unsigned char *buf = iqm->rawdata;

	if((int)hdr->num_poses != iqm->numjoints) Sys_Error("loadiqmanims: %s: (int)hdr->num_poses != iqm->numjoints\n", path);

	/* TODO: what? deal with multi-file animations
    if(animdata)
    {
        if(animdata != meshdata) delete[] animdata;
        delete[] frames;
        animdata = NULL;
        anims = NULL;
        frames = 0;
        numframes = 0;
        numanims = 0;
    } */

    lilswap((uint *)&buf[hdr->ofs_poses], hdr->num_poses*sizeof(iqmpose)/sizeof(uint));
    lilswap((uint *)&buf[hdr->ofs_anims], hdr->num_anims*sizeof(iqmanim)/sizeof(uint));
    lilswap((ushort *)&buf[hdr->ofs_frames], hdr->num_frames*hdr->num_framechannels);
	if(hdr->ofs_bounds) lilswap((uint *)&buf[hdr->ofs_bounds], hdr->num_frames*sizeof(iqmbounds)/sizeof(uint));

    iqm->animdata = buf;
    iqm->numanims = hdr->num_anims;
    iqm->numframes = hdr->num_frames;

    const char *str = hdr->ofs_text ? (char *)&buf[hdr->ofs_text] : "";
    iqm->anims = (iqmanim *)&buf[hdr->ofs_anims];
    iqm->poses = (iqmpose *)&buf[hdr->ofs_poses];
    iqm->frames = (Matrix3x4 *)Sys_MemAlloc(&mdl_mem, sizeof(Matrix3x4) * hdr->num_frames * hdr->num_poses, "modeldata");
    ushort *framedata = (ushort *)&buf[hdr->ofs_frames];
    if(hdr->ofs_bounds) iqm->bounds = (iqmbounds *)&buf[hdr->ofs_bounds];

    for(int i = 0; i < (int)hdr->num_frames; i++)
    {
        for(int j = 0; j < (int)hdr->num_poses; j++)
        {
            iqmpose &p = iqm->poses[j];
            Quat rotate;
            Vec3 translate, scale;
            translate.x = p.channeloffset[0]; if(p.mask&0x01) translate.x += *framedata++ * p.channelscale[0];
            translate.y = p.channeloffset[1]; if(p.mask&0x02) translate.y += *framedata++ * p.channelscale[1];
            translate.z = p.channeloffset[2]; if(p.mask&0x04) translate.z += *framedata++ * p.channelscale[2];
            rotate.x = p.channeloffset[3]; if(p.mask&0x08) rotate.x += *framedata++ * p.channelscale[3];
            rotate.y = p.channeloffset[4]; if(p.mask&0x10) rotate.y += *framedata++ * p.channelscale[4];
            rotate.z = p.channeloffset[5]; if(p.mask&0x20) rotate.z += *framedata++ * p.channelscale[5];
            rotate.w = p.channeloffset[6]; if(p.mask&0x40) rotate.w += *framedata++ * p.channelscale[6];
            scale.x = p.channeloffset[7]; if(p.mask&0x80) scale.x += *framedata++ * p.channelscale[7];
            scale.y = p.channeloffset[8]; if(p.mask&0x100) scale.y += *framedata++ * p.channelscale[8];
            scale.z = p.channeloffset[9]; if(p.mask&0x200) scale.z += *framedata++ * p.channelscale[9];
            // Concatenate each pose with the inverse base pose to avoid doing this at animation time.
            // If the joint has a parent, then it needs to be pre-concatenated with its parent's base pose.
            // Thus it all negates at animation time like so:
            //   (parentPose * parentInverseBasePose) * (parentBasePose * childPose * childInverseBasePose) =>
            //   parentPose * (parentInverseBasePose * parentBasePose) * childPose * childInverseBasePose =>
            //   parentPose * childPose * childInverseBasePose
            Matrix3x4 m(rotate.normalize(), translate, scale);
            if(p.parent >= 0) iqm->frames[i*hdr->num_poses + j] = iqm->baseframe[p.parent] * m * iqm->inversebaseframe[j];
            else iqm->frames[i*hdr->num_poses + j] = m * iqm->inversebaseframe[j];
        }
    }

    for(int animidx = 0; animidx < NUM_ANIMATIONS; animidx++)
	{
		int i;
		for(i = 0; i < (int)iqm->hdr->num_anims; i++)
		{
			iqmanim &a = iqm->anims[i];
			/* TODO FIXME: should use strncmp, what is the max string name for iqm? to be honest strncmp itself is UNSAFE in most of the engine */
			if (!strcmp(animation_names[animidx], &str[a.name]))
			{
				iqm->animation_cache[animidx] = i;
				/* TODO CONSOLEDEBUG Sys_Printf("%s: found anim %s: (first frame %u, %u frames, %s, framerate %f\n", iqm->name, &str[a.name], a.first_frame, a.num_frames, a.flags & IQM_LOOP ? "LOOP" : "NO LOOP", a.framerate); */
				break;
			}
		}

		if (i == (int)iqm->hdr->num_anims)
		{
			iqm->animation_cache[animidx] = NUM_ANIMATIONS; /* tag it as not found */
			/* TODO CONSOLEDEBUG Sys_Printf("%s: anim %s not found\n", iqm->name, animation_names[animidx]); */
		}
	}

    for(int i = 0; i < (int)hdr->num_anims; i++)
    {
        iqmanim &a = iqm->anims[i];
		/* TODO CONSOLEDEBUG Sys_Printf("%s: loaded anim: %s (first frame %u, %u frames, %s, framerate %f)\n", path, &str[a.name], a.first_frame, a.num_frames, a.flags & IQM_LOOP ? "LOOP" : "NO LOOP", a.framerate); */
    }
}

// Note that this animates all attributes (position, normal, tangent, bitangent)
// for expository purposes, even though this demo does not use all of them for rendering.
/* TODO: do we blend with whatever was animated earlier or just between the defined frames? useful for seamless transition between animations and animation blending */
/* TODO: this function is becoming very slow */
void animateiqm_generatematrices(m_model_iqm_t *iqm, float *curframes, const int ent, const int anim_pitch)
{
    if(iqm->numframes <= 0) return;

	/*
		This blending means that an animation should have a last frame for blending as we get closer to it, but curframe should never be it,
		otherwise we will blend with the next frame which will be the first for another animation. This also means
		that animations with only one pose should have 2 frames.
		(in a looped animation, the last frame should then be equal to the first one, since the last one should not ever be set but will
		be used for blending as if it were the first frame.
	*/
    int frame1[ANIMATION_MAX_BLENDED_FRAMES],
        frame2[ANIMATION_MAX_BLENDED_FRAMES];
    float frameoffset[ANIMATION_MAX_BLENDED_FRAMES];
    Matrix3x4 *mat1[ANIMATION_MAX_BLENDED_FRAMES],
              *mat2[ANIMATION_MAX_BLENDED_FRAMES];
	for (int i = 0; i < ANIMATION_MAX_BLENDED_FRAMES; i++)
	{
		frame1[i] = (int)floor(curframes[i]);
		frame2[i] = frame1[i] + 1;
		frameoffset[i] = curframes[i] - frame1[i];
		/* TODO: if out of bounds, error instead of module? */
		frame1[i] %= iqm->numframes;
		frame2[i] %= iqm->numframes;
		if ((frame1[i] == ANIMATION_BASE_FRAME || frame2[i] == ANIMATION_BASE_FRAME) && i != 0) /* only allow the base frame in the first animation slot */
			continue;
		mat1[i] = &iqm->frames[frame1[i] * iqm->numjoints];
		mat2[i] = &iqm->frames[frame2[i] * iqm->numjoints];
	}
    // Interpolate matrixes between the two closest frames and concatenate with parent matrix if necessary.
    // Concatenate the result with the inverse of the base pose.
    // You would normally do animation blending and inter-frame blending here in a 3D engine. TODO both right
    for(int i = 0; i < iqm->numjoints; i++)
    {
		iqm->outframe[ent][i] = Matrix3x4(Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0)); /* for linear interpolation blending */

		/* rotate model torso only if we have pitch and a Torso joint */
		if (iqm->torso_joint == i)
		{
			/* TODO: use an additive animation for this? or allow modyfing more than one bone? do this faster? which slot? */
			Matrix3x4 mat = mat1[ANIMATION_SLOT_ALLJOINTS][i]*(1 - frameoffset[ANIMATION_SLOT_ALLJOINTS]) + mat2[ANIMATION_SLOT_ALLJOINTS][i]*frameoffset[ANIMATION_SLOT_ALLJOINTS];

			if (anim_pitch)
			{
				Matrix3x4 matpitch(Quat(Math_Deg2Rad(iqm->modelmatrix_angles[ent][ANGLES_PITCH]), Vec3(1, 0, 0)), Vec3(0, 0, 0));
				Matrix3x4 mattrans1(Quat(0, Vec3(1, 0, 0)), Vec3(iqm->joints[i].translate));
				Matrix3x4 mattrans2(Quat(0, Vec3(1, 0, 0)), Vec3(iqm->joints[i].translate) * -1);
				if(iqm->joints[i].parent >= 0)
					iqm->outframe[ent][i] += (iqm->outframe[ent][iqm->joints[i].parent] * mat * mattrans1 * matpitch * mattrans2);
				else
					iqm->outframe[ent][i] += mat * mattrans1 * matpitch * mattrans2;
			}
			else
			{
				if(iqm->joints[i].parent >= 0)
					iqm->outframe[ent][i] += (iqm->outframe[ent][iqm->joints[i].parent] * mat);
				else
					iqm->outframe[ent][i] += mat;
			}
			continue;
		}

		/* TODO: see about dual quaternion blending, for preserving volume */
		/* higher slot number for animations have priority over bones TODO: use weights when implementing blending */
		int joint_is_animated = false;
		for (int slot = ANIMATION_MAX_BLENDED_FRAMES - 1; slot >= 0; slot--) /* can't do each slot separately because parents must be in the final blended position before attempting to blend their children */
		{
			float slot_weight = 1; /* TODO: for blending between slots, define this, when loading, see if the slot weights sum up to 1 (better to use uchars from 0 to 255 and see if they sum up to 255) then divide by 255 for use here */
			if (iqm->jointmasks[slot][i])
			{
				joint_is_animated = true;
				if ((frame1[slot] == ANIMATION_BASE_FRAME || frame2[slot] == ANIMATION_BASE_FRAME) && slot != 0) /* only allow the base frame in the first animation slot */
					continue;

				Matrix3x4 mat = mat1[slot][i]*(1 - frameoffset[slot]) + mat2[slot][i]*frameoffset[slot];
				if(iqm->joints[i].parent >= 0) iqm->outframe[ent][i] += (iqm->outframe[ent][iqm->joints[i].parent] * mat) * slot_weight; /* TODO: see if the right is applying to mat or to parent * mat */
				else iqm->outframe[ent][i] += mat * slot_weight;

				break; /* TODO: for blending between slots, apply the weights by slot above and CONTINUE this loop, do not break here. */
			}
		}

		if (!joint_is_animated)
		{
			const char *str = iqm->hdr->ofs_text ? (char *)&iqm->rawdata[iqm->hdr->ofs_text] : "";
			Sys_Error("animateiqm_generatematrices: joint \"%s\" of model \"%s\" isn't animated by any slot, if this is okay, consider deleting or merging it!\n", &str[iqm->joints[i].name], iqm->name);
		}
    }
}
void animateiqm_generateattributes(m_model_iqm_t *iqm, const int ent)
{
#ifndef IQM_GPU_ANIMATION
	if(iqm->numframes <= 0) return;

    // The actual vertex generation based on the matrixes follows...
    const Vec3 *srcpos = (const Vec3 *)iqm->inposition, *srcnorm = (const Vec3 *)iqm->innormal;
    const Vec4 *srctan = (const Vec4 *)iqm->intangent;
    Vec3 *dstpos = (Vec3 *)iqm->outposition[ent], *dstnorm = (Vec3 *)iqm->outnormal[ent], *dsttan = (Vec3 *)iqm->outtangent[ent], *dstbitan = (Vec3 *)iqm->outbitangent[ent];
    const uchar *index = iqm->inblendindex, *weight = iqm->inblendweight;
    for(int i = 0; i < iqm->numverts; i++)
    {
        // Blend matrixes for this vertex according to its blend weights.
        // the first index/weight is always present, and the weights are
        // guaranteed to add up to 255. So if only the first weight is
        // presented, you could optimize this case by skipping any weight
        // multiplies and intermediate storage of a blended matrix.
        // There are only at most 4 weights per vertex, and they are in
        // sorted order from highest weight to lowest weight. Weights with
        // 0 values, which are always at the end, are unused.
        Matrix3x4 mat = iqm->outframe[ent][index[0]] * (weight[0]/255.0f);
        for(int j = 1; j < 4 && weight[j]; j++)
            mat += iqm->outframe[ent][index[j]] * (weight[j]/255.0f);

        // Transform attributes by the blended matrix.
        // Position uses the full 3x4 transformation matrix.
        // Normals and tangents only use the 3x3 rotation part
        // of the transformation matrix.
        *dstpos = mat.transform(*srcpos);

        // Note that if the matrix includes non-uniform scaling, normal vectors
        // must be transformed by the inverse-transpose of the matrix to have the
        // correct relative scale. Note that invert(mat) = adjoint(mat)/determinant(mat),
        // and since the absolute scale is not important for a vector that will later
        // be renormalized, the adjoint-transpose matrix will work fine, which can be
        // cheaply generated by 3 cross-products.  TODO use this in the opengl shaders
        //
        // If you don't need to use joint scaling in your models, you can simply use the
        // upper 3x3 part of the position matrix instead of the adjoint-transpose shown
        // here.
        Matrix3x3 matnorm(mat.b.cross3(mat.c), mat.c.cross3(mat.a), mat.a.cross3(mat.b));

        *dstnorm = matnorm.transform(*srcnorm);
        // Note that input tangent data has 4 coordinates,
        // so only transform the first 3 as the tangent vector.
        *dsttan = matnorm.transform(Vec3(*srctan));
        // Note that bitangent = cross(normal, tangent) * sign,
        // where the sign is stored in the 4th coordinate of the input tangent data.
        *dstbitan = dstnorm->cross(*dsttan) * srctan->w;

        srcpos++;
        srcnorm++;
        srctan++;
        dstpos++;
        dstnorm++;
        dsttan++;
        dstbitan++;

        index += 4;
        weight += 4;
    }
#endif
}

/* TODO FIXME: move these to sys_*_video.c */
/* TODO: make int video_3d_alpha_pass an static and remove the EXTERNC where it's declared when moving this rendering to the proper file */
extern int video_3d_alpha_pass;
extern void Sys_VideoUpdateUniform(const unsigned int uniform_id, const void *data, const int count);
int renderiqm(m_model_iqm_t *iqm, const int ent)
{
	int draw_calls_issued = false;
#ifndef DEDICATED_SERVER
#ifndef IQM_GPU_ANIMATION
    for(int i = 0; i < iqm->nummeshes; i++)
    {
        iqmmesh &m = iqm->meshes[i];

        /* draw them TODO: use GL_UNSIGNED_SHORT or GL_UNSIGNED_BYTE when applicable */
#error "fix this"
		if (Sys_VideoDraw3DTriangles(iqm->numframes > 0 ? iqm->outposition[ent] : iqm->inposition, iqm->incolor, iqm->intexcoord, NULL, iqm->numframes > 0 ? iqm->outnormal[ent] : iqm->innormal, 0, (unsigned int *)&iqm->tris[m.first_triangle], 3 * m.num_triangles, iqm->textures[i]->cl_id, -1, -1, -1, -1, 0))
			draw_calls_issued = true;
    }
#else
    if(iqm->numframes > 0)
    {
        if(hasUBO)
        {
            glBindBuffer(GL_UNIFORM_BUFFER, iqm->ubo);
            glBufferData(GL_UNIFORM_BUFFER, iqm->ubosize, NULL, GL_STREAM_DRAW);
            glBufferSubData(GL_UNIFORM_BUFFER, iqm->bonematsoffset, iqm->numjoints*sizeof(Matrix3x4), iqm->outframe[ent][0].a.v);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            glBindBufferBase(GL_UNIFORM_BUFFER, 0, iqm->ubo);
        }
        else
        {
            Sys_VideoUpdateUniform(SHADER_UNIFORM_BONEMATS, iqm->outframe[ent][0].a.v, iqm->numjoints);
        }
    }

	for(int i = 0; i < iqm->nummeshes; i++)
	{
		/* TODO: only enable blend weight and bone index if iqm->numframes > 0 */
		iqmmesh &m = iqm->meshes[i];
		if (Sys_VideoDrawVBO(iqm->vbo_id, iqm->textures[i] ? iqm->textures[i]->cl_id : -1, -1, -1, -1, -1, -1, -1, m.num_triangles * 3, m.first_triangle * 3))
			draw_calls_issued = true;
	}
#endif
#endif /* DEDICATED_SERVER */
	return draw_calls_issued;
}

/*
===================
Sys_LoadModelIQMDo

Should only be called by Sys_LoadModelIQM.
===================
*/
EXTERNC void *Sys_LoadModelIQMDo(const char *path, unsigned char *rawdata)
{
	m_model_iqm_t *iqm;
    iqmheader *hdr;
	int i, j;

	/* TODO: check if we have at least sizeof(iqmheader) at rawdata */
	hdr = (iqmheader *)rawdata;
    if (memcmp(hdr->magic, IQM_MAGIC, sizeof(hdr->magic))) /* TODO: sizeof with pointer? */
		Sys_Error("Sys_LoadModelIQMDo: header mismatch for %s, %s was expected\n", path, IQM_MAGIC);

    lilswap(&hdr->version, (sizeof(iqmheader) - sizeof(hdr->magic)) / sizeof(unsigned int));

    if (hdr->version != IQM_VERSION)
        Sys_Error("Sys_LoadModelIQMDo: version mismatch for %s, %d was expected\n", path, IQM_VERSION);

    if (hdr->filesize > (16 << 20))
        Sys_Error("Sys_LoadModelIQMDo: %s is too big!\n", path); /* sanity check... don't load files bigger than 16 MB */

	iqm = (m_model_iqm_t *)Sys_MemAlloc(&mdl_mem, sizeof(m_model_iqm_t), "modeldata"); /* assume that it comes memsetted to zero */
	iqm->rawdata = rawdata;
	iqm->hdr = hdr;
	Sys_Snprintf(iqm->name, MAX_MODEL_NAME, "%s", path);

    if (hdr->num_meshes > 0)
		loadiqmmeshes(path, iqm);
    if (hdr->num_anims > 0)
		loadiqmanims(path, iqm);

	for (j = 0; j < MAX_EDICTS; j++)
		for (i = 0; i < ANIMATION_MAX_BLENDED_FRAMES; i++)
			iqm->lastframes[j][i] = -1;

	return iqm;
}

/*
===================
Sys_LoadModelClientDataIQMDo

Should only be called by Sys_LoadModelClientDataIQM.
===================
*/
#ifndef DEDICATED_SERVER
extern GLuint shader_program_ids[SHADER_NUM];
#endif /* DEDICATED_SERVER */
EXTERNC void Sys_LoadModelClientDataIQMDo(m_model_iqm_t *model)
{
#ifndef DEDICATED_SERVER
	char texturename[MAX_TEXTURE_NAME];
	char modelname[MAX_TEXTURE_NAME];
	size_t size;
	const char *str = model->hdr->ofs_text ? (char *)&model->rawdata[model->hdr->ofs_text] : "";

    for(int i = 0; i < (int)model->hdr->num_meshes; i++)
    {
        iqmmesh &m = model->meshes[i];

		Sys_Snprintf(modelname, MAX_MODEL_NAME, "%s", model->name);
		size = strlen(modelname);
		if (modelname[size - 4] == '.') /* remove any embedded extesion TODO: DO THIS RIGHT! */
			modelname[size - 4] = 0;

		Sys_Snprintf(texturename, MAX_TEXTURE_NAME, "%s/%s", modelname, &str[m.material]); /* TODO: models may share textures! */
		size = strlen(texturename);
		if (texturename[size - 4] == '.') /* remove any embedded extesion TODO: DO THIS RIGHT! */
			texturename[size - 4] = 0;

		/* TODO FIXME: Sys Calling CL? */
        model->textures[i] = CL_LoadTexture(texturename, false, NULL, 0, 0, false, 1, 1);
		/* TODO CONSOLEDEBUG if(model->textures[i]) Sys_Printf("loaded material: %s\n", &str[m.material]); */
    }

#ifdef IQM_GPU_ANIMATION
	int marker = Sys_MemLowMark(&tmp_mem);

	model_trimesh_part_t *trimesh = (model_trimesh_part_t *)Sys_MemAlloc(&tmp_mem, sizeof(model_trimesh_part_t), "iqmtrimesh");

	trimesh->verts = (model_vertex_t *)Sys_MemAlloc(&tmp_mem, sizeof(model_vertex_t) * model->hdr->num_vertexes, "iqmtrimesh");
	trimesh->vert_stride = sizeof(model_vertex_t);
	trimesh->vert_count = model->hdr->num_vertexes;

    for(int i = 0; i < (int)model->hdr->num_vertexes; i++)
    {
        if(model->inposition)
			Math_Vector3Copy(model->inposition + i * 3, trimesh->verts[i].origin);
        if(model->innormal)
			Math_Vector3Copy(model->innormal + i * 3, trimesh->verts[i].normal);
        if(model->intangent)
			Math_Vector4Copy(model->intangent + i * 4, trimesh->verts[i].tangent);
        if(model->intexcoord)
			Math_Vector2Copy(model->intexcoord + i * 2, trimesh->verts[i].texcoord0);
        if(model->inblendindex)
			Math_Vector4Copy(model->inblendindex + i * 4, trimesh->verts[i].bones);
        if(model->inblendweight)
		{
			vec_t *vboweightptr = (vec_t *)trimesh->verts[i].weights;
			vboweightptr[0] = ((vec_t)(model->inblendweight + i * 4)[0]) / 255.f;
			vboweightptr[1] = ((vec_t)(model->inblendweight + i * 4)[1]) / 255.f;
			vboweightptr[2] = ((vec_t)(model->inblendweight + i * 4)[2]) / 255.f;
			vboweightptr[3] = ((vec_t)(model->inblendweight + i * 4)[3]) / 255.f;
		}
    }

	trimesh->indexes = (int *)Sys_MemAlloc(&tmp_mem, sizeof(int) * model->hdr->num_triangles * 3, "iqmtrimesh");
	trimesh->index_count = model->hdr->num_triangles * 3;
	trimesh->index_stride = sizeof(int) * 3;

	for (unsigned int i = 0; i < model->hdr->num_triangles; i++)
	{
		trimesh->indexes[i * 3] = model->tris[i].vertex[0];
		trimesh->indexes[i * 3 + 1] = model->tris[i].vertex[1];
		trimesh->indexes[i * 3 + 2] = model->tris[i].vertex[2];
	}

	model->vbo_id = Sys_UploadVBO(-1, trimesh, false, false);

	loadexts(); /* TODO: shouldn't be there */

    if(hasUBO)
    {
		/* using SHADER_FIXED_LIGHT_SKINNING but this data should be equal across all SKINNING shaders TODO: will it be? */
        GLuint blockidx = glGetUniformBlockIndex(shader_program_ids[SHADER_FIXED_LIGHT_SKINNING], "u_animdata"), bonematsidx;
        const GLchar *bonematsname = "u_bonemats";
        glGetUniformIndices(shader_program_ids[SHADER_FIXED_LIGHT_SKINNING], 1, &bonematsname, &bonematsidx);
        glGetActiveUniformBlockiv(shader_program_ids[SHADER_FIXED_LIGHT_SKINNING], blockidx, GL_UNIFORM_BLOCK_DATA_SIZE, &model->ubosize);
        glGetActiveUniformsiv(shader_program_ids[SHADER_FIXED_LIGHT_SKINNING], 1, &bonematsidx, GL_UNIFORM_OFFSET, &model->bonematsoffset);
        glUniformBlockBinding(shader_program_ids[SHADER_FIXED_LIGHT_SKINNING], blockidx, 0);
        if(!model->ubo) glGenBuffers(1, &model->ubo);
    }

	Sys_MemFreeToLowMark(&tmp_mem, marker);
#endif
#endif /* DEDICATED_SERVER */
}

/*
===================
Sys_ModelAnimationInfoIQMDo

Should only be called by Sys_ModelAnimationInfoIQM.
===================
*/
EXTERNC void Sys_ModelAnimationInfoIQMDo(m_model_iqm_t *model, const unsigned int animation, unsigned int *start_frame, unsigned int *num_frames, int *loop, vec_t *frames_per_second, int *multiple_slots, int *vertex_animation)
{
	unsigned int found_anim;

	if (animation < 0 || animation >= NUM_ANIMATIONS)
		Sys_Error("Sys_ModelAnimationInfoIQMDo: animation %d out of range in model %s\n", animation, model->name);

	found_anim = model->animation_cache[animation];

	if (found_anim == NUM_ANIMATIONS)
		Sys_Error("Sys_ModelAnimationInfoIQMDo: animation %d not found in model %s\n", animation, model->name);

	iqmanim &a = model->anims[found_anim];
	if (start_frame)
		*start_frame = a.first_frame;
	if (num_frames)
		*num_frames = a.num_frames;
	if (loop)
		*loop = (a.flags & IQM_LOOP) ? true : false;
	if (frames_per_second)
		*frames_per_second = a.framerate;
	if (multiple_slots)
		*multiple_slots = true;
	if (vertex_animation)
		*vertex_animation = false;
}

/*
===================
Sys_ModelAnimationExistsIQMDo

Should only be called by Sys_ModelAnimationExistsIQM.
===================
*/
EXTERNC int Sys_ModelAnimationExistsIQMDo(m_model_iqm_t *model, const unsigned int animation)
{
	unsigned int found_anim;

	if (animation < 0 || animation >= NUM_ANIMATIONS)
		Sys_Error("Sys_ModelAnimationExistsIQMDo: animation %d out of range in model %s\n", animation, model->name);

	found_anim = model->animation_cache[animation];

	if (found_anim == NUM_ANIMATIONS)
		return false;

	return true;
}

/*
===================
Sys_ModelAABBIQMDo

Should only be called by Sys_ModelAABBIQM.
TODO: use vec_t frame to calculate interpolated AABB after doing the animation or use the min and max in each axis from floor and ceil frame
===================
*/
EXTERNC void Sys_ModelAABBIQMDo(m_model_iqm_t *model, const vec_t frame, vec3_t mins, vec3_t maxs)
{
	Math_Vector3Copy(model->bounds[(int)floor(frame)].bbmin, mins);
	Math_Vector3Copy(model->bounds[(int)floor(frame)].bbmax, maxs);
}

/*
===================
Sys_ModelGetTagTransformIQMDo

Should only be called by Sys_ModelGetTagTransformIQM.
===================
*/
EXTERNC void Sys_ModelGetTagTransformIQMDo(m_model_iqm_t *model, const unsigned int tag_idx, const int local_coords, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, const int ent)
{
	/* TODO: tags in default poses for when there are no animations? */
	if (!model->numframes)
		Sys_Error("Sys_ModelGetTagTransformIQMDo: %s has no animations, tags not supported yet\n", model->name);
	if (!model->lastmodelmatrix)
		Sys_Error("Sys_ModelGetTagTransformIQMDo: model not animated and positioned at least once!\n");
	for (int i = 0; i < ANIMATION_MAX_BLENDED_FRAMES; i++)
		if (model->lastframes[ent][i] == -1)
			Sys_Error("Sys_ModelGetTagTransformIQMDo: %s wasn't animated yet\n", model->name);

	if (tag_idx < 0 || tag_idx >= NUM_MODEL_TAGS)
		Sys_Error("Sys_ModelGetTagTransformIQMDo: tag %d out of range for model %s\n", tag_idx, model->name);
	if (!model->modeltags_enabled[tag_idx])
		Sys_Error("Sys_ModelGetTagTransformIQMDo: model %s doesn't have the tag %d enabled\n", model->name, tag_idx);

	Matrix3x4 jointtransformed;

	if (local_coords)
	{
		jointtransformed = model->outframe[ent][model->modeltags_joint[tag_idx]];
	}
	else
	{
		Matrix3x4 realentmatrix(Vec4(model->lastmodelmatrix[ent][0], model->lastmodelmatrix[ent][4], model->lastmodelmatrix[ent][8], model->lastmodelmatrix[ent][12]), Vec4(model->lastmodelmatrix[ent][1], model->lastmodelmatrix[ent][5], model->lastmodelmatrix[ent][9], model->lastmodelmatrix[ent][13]), Vec4(model->lastmodelmatrix[ent][2], model->lastmodelmatrix[ent][6], model->lastmodelmatrix[ent][10], model->lastmodelmatrix[ent][14]));
		jointtransformed = realentmatrix * (model->outframe[ent][model->modeltags_joint[tag_idx]]);
	}

	if (origin)
	{
		Vec3 origin_transformed = jointtransformed.transform(model->modeltags_base[tag_idx]);
		Math_Vector3Copy(origin_transformed, origin);
	}

	/* TODO: for these three: store a rotation matrix in the tag file to be transformed */
	if (forward)
		Sys_Error("Sys_ModelGetTagTransformIQMDo: forward unimplemented\n");

	if (right)
		Sys_Error("Sys_ModelGetTagTransformIQMDo: right unimplemented\n");

	if (up)
		Sys_Error("Sys_ModelGetTagTransformIQMDo: up unimplemented\n");
}

/*
===================
Sys_ModelAnimateIQMDo

Should only be called by Sys_ModelAnimateIQM and Sys_VideoDraw3DModelIQMDo.
===================
*/
EXTERNC void Sys_ModelAnimateIQMDo(m_model_iqm_t *model, const int ent, vec3_t origin, vec3_t angles, vec_t *frames, const int anim_pitch)
{
	int i, reanimate;
	int just_created = false;

	/* if the first one wasn't allocated, none were */
	if (model->lastmodelmatrix[ent] == NULL)
	{
		/* TODO: this is slow */
		model->lastmodelmatrix[ent] = (vec_t *)Sys_MemAlloc(&mdl_mem, sizeof(vec_t) * 16, "modeldata");
		model->modelmatrix_origin[ent] = (vec_t *)Sys_MemAlloc(&mdl_mem, sizeof(vec_t) * 3, "modeldata");
		model->modelmatrix_angles[ent] = (vec_t *)Sys_MemAlloc(&mdl_mem, sizeof(vec_t) * 3, "modeldata");
		just_created = true;
	}

	if (just_created || !Math_Vector3Compare(origin, model->modelmatrix_origin[ent]) || !Math_Vector3Compare(angles, model->modelmatrix_angles[ent]))
	{
		Math_Vector3Copy(origin, model->modelmatrix_origin[ent]);
		Math_Vector3Copy(angles, model->modelmatrix_angles[ent]);

		/* rotate model torso only if we have pitch and a Torso joint */
		vec3_t newangles;
		Math_Vector3Copy(angles, newangles);
		if (model->torso_joint != -1 && anim_pitch)
			newangles[ANGLES_PITCH] = 0; /* do not rotate the whole model TODO: what if we WANT to have the feet in an angle? also, kinematic_angles[ANGLES_PITCH] should be used here instead of ZERO, because the physics object may be created with non-zero pitch when doing kinematic angles */

		Math_MatrixModel4x4(model->lastmodelmatrix[ent], origin, newangles, NULL);
	}

	/* TODO: allow animation decoupled from rendering for the server to make tracelines */
	/* TODO: cache this per-entity, because we may draw multiple passess: THIS IS IMPORTANT */
	reanimate = false;
	if (model->numframes)
	{
		for (i = 0; i < ANIMATION_MAX_BLENDED_FRAMES; i++)
		{
			if (just_created || model->lastframes[ent][i] != frames[i])
			{
				model->lastframes[ent][i] = frames[i];
				reanimate = true;
				/* TODO: do not reanimate if no joints will be affected by the changed frame */
				/* TODO: use mod to catch overflows in frame number here? */
			}
		}
	}
	if (reanimate)
	{
		/* if the first one wasn't allocated, none were */
		if (model->outframe[ent] == NULL)
		{
			/* TODO: this is slow */
#ifndef IQM_GPU_ANIMATION
			model->outposition[ent] = (float *)Sys_MemAlloc(&mdl_mem, sizeof(float) * 3 * model->numverts, "modeldata");
			model->outnormal[ent] = (float *)Sys_MemAlloc(&mdl_mem, sizeof(float) * 3 * model->numverts, "modeldata");
			model->outtangent[ent] = (float *)Sys_MemAlloc(&mdl_mem, sizeof(float) * 3 * model->numverts, "modeldata");
			model->outbitangent[ent] = (float *)Sys_MemAlloc(&mdl_mem, sizeof(float) * 3 * model->numverts, "modeldata");
#endif
			model->outframe[ent] = (Matrix3x4 *)Sys_MemAlloc(&mdl_mem, sizeof(Matrix3x4) * model->hdr->num_joints, "modeldata");
		}
		animateiqm_generatematrices(model, frames, ent, anim_pitch);
		animateiqm_generateattributes(model, ent);
		/* TODO only store what we will need? also no need to store if doing stacked rendering (but would be needed for getting the transform) */
	}
}

/*
===================
Sys_VideoDraw3DModelIQMDo

Should only be called by Sys_VideoDraw3DModelIQM.
===================
*/
EXTERNC int Sys_VideoDraw3DModelIQMDo(m_model_iqm_t *model, vec_t *frames, const int ent, vec3_t origin, vec3_t angles, const int anim_pitch, unsigned int desired_shader, const vec3_t ambient, const vec3_t directional, const vec3_t direction)
{
#ifndef DEDICATED_SERVER
	/* animate if necessary */
	Sys_ModelAnimateIQMDo(model, ent, origin, angles, frames, anim_pitch);

	if (!model->lastmodelmatrix)
		Sys_Error("Sys_VideoDraw3DModelIQMDo: model not animated and positioned at least once!\n");

	Sys_VideoTransformFor3DModel(model->lastmodelmatrix[ent]);

	if (desired_shader == SHADER_LIGHTMAPPING)
	{
		/* set lights before transforming the matrix TODO: even the view matrix? seems ok right now, maybe because of glMatrixMode()? */
		const GLfloat ambientcol[4] = { ambient[0], ambient[1], ambient[2], 1 },
					diffusecol[4] = { directional[0], directional[1], directional[2], 1 },
					lightdir[4] = { direction[0], direction[1], direction[2], 0 };
#ifdef IQM_GPU_ANIMATION
		if (model->numframes)
			Sys_VideoBindShaderProgram(SHADER_FIXED_LIGHT_SKINNING, lightdir, diffusecol, ambientcol);
		else
#endif
			Sys_VideoBindShaderProgram(SHADER_FIXED_LIGHT, lightdir, diffusecol, ambientcol);
	}
	else
	{
#ifdef IQM_GPU_ANIMATION
		if (model->numframes)
		{
			int skinning_shader;
			switch (desired_shader)
			{
				case SHADER_LIGHTING_NO_SHADOWS:
					skinning_shader = SHADER_LIGHTING_NO_SHADOWS_SKINNING;
					break;
				case SHADER_DEPTHSTORE:
					skinning_shader = SHADER_DEPTHSTORE_SKINNING;
					break;
				case SHADER_SHADOWMAPPING:
					skinning_shader = SHADER_SHADOWMAPPING_SKINNING;
					break;
				default:
					Sys_Error("Sys_VideoDraw3DModelIQMDo: can't find IQM shader for shader %d\n", desired_shader);
					skinning_shader = desired_shader; /* shut up the compiler */
			}
			Sys_VideoBindShaderProgram(skinning_shader, NULL, NULL, NULL);
		}
		else
#endif
			Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL);
	}
#endif /* DEDICATED_SERVER */
	return renderiqm(model, ent);
}
