/*
	This code was written by me, Eluan Costa Miranda, unless otherwise noted.
	Use or distribution of this code must have explict authorization by me.
	This code is copyright 2011-2014 Eluan Costa Miranda <eluancm@gmail.com>
	No warranties.
*/

#include "engine.h"

#ifndef DEDICATED_SERVER

#include <SDL.h>
#include <GL/glew.h>
#include <FTGL/ftgl.h>

#define HARDWARE_OCCLUSION_QUERIES_IGNORE_VOXELS
#define HARDWARE_OCCLUSION_QUERIES_TEST_ONLY_AABB_FOR_DYNAMIC_MODELS

/*
============================================================================

Video-wide stuff

TODO: entity, voxel chunck, particle distance culling
TODO: view frustum culling
TODO: portal culling (oclussion query) for no bsp vis precalculation
TODO: voxel lighting and lighting caching (with the entities lights)
TODO: sun as a light
TODO: sun glare, lens flares, bumpmaps, light bloom
TODO: motion blur for high speeds
TODO: depth of field
TODO: render front to back, always with occlusion queries and after finishing, occlusion query the light frusta (including the 6 cubemap frusta) to see if the light view's need rendering! BAM!
TODO: for redundant state checking, NEVER set it immediately, only before the render, allowing states to go from X->Y->X->Render without having to set states twice!
TODO: if a chunk of terrain, bsp leaf or entire bsp model is culled, everything inside should be culled too!
TODO: make terrain/map/voxels/etc always be rendered first? for better occlusion queries (entity allocation may interfere when using multi-entity worlds)
TODO: adjust fov/etc of lights to focus only on the part of the view frustum visible to the light, for better shadows
TODO: nearplane bem perto pro sol (e pra diminuir jaggedness nas sombras) com distância (PlayerPos DOT LightDir) � () SomeDistance (maybe this value negated)
TODO: allocate EVERYTHING in the same vbo (with offsets for the base vertex and index) and when the vbo is full, alocate another and so on. (Go back to previous vbo if a new model fits where a bigger didn't fit - do a clever algorithm for best fit in fewer vbos)

============================================================================
*/

/* last shader bound */
static GLuint bound_shader;

static char *shader_vertex_names[SHADER_NUM] =
{
	"DepthStoreVertexShader",
	"DepthStoreSKINNINGVertexShader",
	"DepthStoreVertexShader",
	"ShadowCubeMappingVertexShader",
	"ShadowCubeMappingSKINNINGVertexShader",
	"ShadowCubeMappingTERRAINVertexShader",

	"LightingNoShadowsVertexShader",
	"LightingNoShadowsSKINNINGVertexShader",
	"LightingNoShadowsTERRAINVertexShader",

	"FixedLightVertexShader",
	"FixedLightSKINNINGVertexShader",
	"FixedLightTERRAINVertexShader",
	"LightmappingVertexShader",
	"ParticleVertexShader",
	"SkyboxVertexShader",

	"2DVertexShader",
	"2DVertexShader"
};

static char *shader_fragment_names[SHADER_NUM] =
{
	"",
	"",
	"",
	"ShadowCubeMappingFragmentShader",
	"ShadowCubeMappingFragmentShader",
	"ShadowCubeMappingTERRAINFragmentShader",

	"LightingNoShadowsFragmentShader",
	"LightingNoShadowsFragmentShader",
	"LightingNoShadowsTERRAINFragmentShader",

	"FixedLightFragmentShader",
	"FixedLightFragmentShader",
	"FixedLightTERRAINFragmentShader",
	"LightmappingFragmentShader",
	"ParticleFragmentShader",
	"SkyboxFragmentShader",

	"2DFragmentShader",
	"2DTextFragmentShader"
};

static char *shader_names[SHADER_NUM] =
{
	"DepthStore",
	"DepthStoreSKINNING",
	"DepthStoreTERRAIN",
	"ShadowCubeMapping",
	"ShadowCubeMappingSKINNING",
	"ShadowCubeMappingTERRAIN",

	"LightingNoShadows",
	"LightingNoShadowsSKINNING",
	"LightingNoShadowsTERRAIN",

	"FixedLight",
	"FixedLightSKINNING",
	"FixedLightTERRAIN",
	"Lightmapping",
	"Particle",
	"Skybox",

	"2D",
	"2DText"
};

/*
============================================================================

Font management

TODO: multiple fonts, alignment (center, justify, right)

============================================================================
*/

static FTGLTextureFont *font = NULL;
static cvar_t *sys_fontextraverticalspacing = NULL;

/*
===================
Sys_FontGetStringWidth
===================
*/
vec_t Sys_FontGetStringWidth(const char *text, vec_t scalex)
{
	if (!font)
		return 0;

	return (vec_t)font->Advance(text) * scalex;
}

/*
===================
Sys_FontGetStringHeight
===================
*/
vec_t Sys_FontGetStringHeight(const char *text, vec_t scaley)
{
	if (!font)
		return 0;

	/* TODO: is this right? consider what characters are in the text? */
	return font->FaceSize() * scaley + (vec_t)sys_fontextraverticalspacing->doublevalue;
}

/*
===================
Sys_FontDraw
===================
*/
void Sys_FontDraw(const char *text)
{
	if (!font)
		return;

	font->Render(text);
}

/*
===================
Sys_FontInit
===================
*/
void Sys_FontInit(void)
{
	unsigned char *fontdata;
	int fontdatasize;

	if (font)
		return;

	fontdatasize = Host_FSLoadBinaryFile("fonts/DejaVuSansMono.ttf", &std_mem, "font", &fontdata, false);

	if (fontdatasize == -1)
		Sys_Error("Sys_FontInit: Couldn't load fonts/DejaVuSansMono.ttf!\n");

	/* Create a pixmap font from a TrueType file */
	font = new FTGLTextureFont(fontdata, fontdatasize);

	/* If something went wrong, bail out. */
	if(font->Error())
		Sys_Error("Sys_FontInit: couldn't process font \"fonts/DejaVuSansMono.ttf\"");

	font->Depth(3.);
	font->Outset(-.5, 1.5);

	/* Set the face size and the character map. If something went wrong, bail out. */
	font->FaceSize(72);
	if(!font->CharMap(ft_encoding_unicode)) /* makes utf-8 work */
		Sys_Error("Sys_FontInit: couldn't load the character map \"ft_encoding_unicode\" for font \"fonts/DejaVuSansMono.ttf\"");

	/* Create a string containing all characters between 128 and 255 and preload the Latin-1 chars without rendering them */
	/* char buf[129]; TODO: common ocidental characters in utf-8
	int i;
	for(i = 128; i < 256; i++)
	{
	    buf[i - 128] = (unsigned char)i;
	}
	buf[i - 128] = 0;
	font->Advance(buf); */
	/* TODO: is this right? */
	char buf[256];
	int i;
	for(i = 1; i < 256; i++)
	{
	    buf[i - 1] = (unsigned char)i;
	}
	buf[255] = 0;
	font->Advance(buf);

	sys_fontextraverticalspacing = Host_CMDAddCvar("sys_fontextraverticalspacing", "2", 0);
}

/*
===================
Sys_FontShutdown
===================
*/
void Sys_FontShutdown(void)
{
	/* Destroy the font object. */
	if (font)
		delete font;
	font = NULL;
}

/*
============================================================================

Texture loading and uploading to video memory (if applicable)

TODO: texture streaming
TODO: out of memory errors at ~1gb in a 2gb card are because we are a 32-bit executable?

============================================================================
*/

static GLint max_texture_size = 0;

static cvar_t *r_linearfiltering = NULL; /* linear filtering for textures */

static int bound_texture0 = -1;
static int bound_texture1 = -1;
static int bound_texture2 = -1;
static int bound_texture3 = -1;
static int bound_texture4 = -1;
static int bound_texture5 = -1;
static int bound_texture6 = -1;
static int bound_texture7 = -1;
static int bound_texture8 = -1;
static int bound_texture9 = -1;
static int bound_texture10 = -1;
static int bound_texture11 = -1;
static int bound_texture12 = -1;

typedef struct texture_sys_s {
	GLuint id;
	int cl_id;
	int texture_has_alpha; /* true if a texture has alpha TODO: treat alpha test (only two levels, 0 and 255) and alpha blending separately, alpha test should be faster */
} texture_sys_t;

/* indexed by cl_id, this is never cleared/cleaned and is only set when loading a texture with a given cl_id */
static texture_sys_t texture_sys[MAX_TEXTURES];

/* white null opaque and transparent textures for when we don't want to create a shader permutation */
static texture_sys_t *texture_nullopaque = NULL;
static texture_sys_t *texture_nulltransparent = NULL;

#endif /* DEDICATED_SERVER */

typedef struct _TargaHeader {
    unsigned char   id_length, colormap_type, image_type;
    unsigned short  colormap_index, colormap_length;
    unsigned char   colormap_size;
    unsigned short  x_origin, y_origin, width, height;
    unsigned char   pixel_size, attributes;
} TargaHeader;

/*
===================
Sys_ProcessTextureTGA

Parse on-disk data for TGA files. This code is based on code that's not mine, but, since it's generic enough, I don't think
any copyright applies (let's hope this is true.)

TODO: clean this mess, endianness
===================
*/
unsigned char *Sys_ProcessTextureTGA(unsigned char *data, int size, int *width, int *height)
{
	unsigned int numPixels;
	unsigned char *pixbuf;
	int row, column;
	unsigned char *end;
	TargaHeader targa_header;
	unsigned char *targa_rgba;
	unsigned char tmp;

	if (size < 18)
	{
		Sys_Printf("LoadTGA: header too short\n");
		return NULL;
	}

	end = data + size;

	targa_header.id_length = data[0];
	targa_header.colormap_type = data[1];
	targa_header.image_type = data[2];

	memcpy(&targa_header.colormap_index, &data[3], 2);
	memcpy(&targa_header.colormap_length, &data[5], 2);
	targa_header.colormap_size = data[7];
	memcpy(&targa_header.x_origin, &data[8], 2);
	memcpy(&targa_header.y_origin, &data[10], 2);
	memcpy(&targa_header.width, &data[12], 2);
	memcpy(&targa_header.height, &data[14], 2);
	targa_header.pixel_size = data[16];
	targa_header.attributes = data[17];

	data += 18;

	if (targa_header.image_type!=2
		&& targa_header.image_type!=10
		&& targa_header.image_type != 3 )
	{
		Sys_Printf("LoadTGA: Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n");
		return NULL;
	}

	if ( targa_header.colormap_type != 0 )
	{
		Sys_Printf("LoadTGA: colormaps not supported\n");
		return NULL;
	}

	if ( ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) && targa_header.image_type != 3 )
	{
		Sys_Printf("LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
		return NULL;
	}

	*width = targa_header.width;
	*height = targa_header.height;
	numPixels = (*width) * (*height) * 4;

	if(!*width || !*height || numPixels > 0x7FFFFFFF || numPixels / (*width) / 4 != *height)
	{
		Sys_Printf("LoadTGA: invalid image size\n");
		return NULL;
	}

	targa_rgba = (unsigned char *)Sys_MemAlloc(&tmp_mem, numPixels, "texturedata2");

	if (targa_header.id_length != 0)
	{
		if (data + targa_header.id_length > end)
		{
			Sys_Printf("LoadTGA: header too short\n");
			return NULL;
		}

		data += targa_header.id_length;  // skip TARGA image comment
	}

	if ( targa_header.image_type==2 || targa_header.image_type == 3 )
	{
		if(data + (*width)*(*height)*targa_header.pixel_size/8 > end)
		{
			Sys_Printf("LoadTGA: file truncated\n");
			return NULL;
		}

		// Uncompressed RGB or gray scale image
		for(row=(*height)-1; row>=0; row--)
		{
			pixbuf = targa_rgba + row*(*width)*4;
			for(column=0; column<(*width); column++)
			{
				unsigned char red,green,blue,alphabyte;
				switch (targa_header.pixel_size)
				{

				case 8:
					blue = *data++;
					green = blue;
					red = blue;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;

				case 24:
					blue = *data++;
					green = *data++;
					red = *data++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *data++;
					green = *data++;
					red = *data++;
					alphabyte = *data++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				default:
					Sys_Printf("LoadTGA: illegal pixel_size '%d'\n", targa_header.pixel_size);
					return NULL;
				}
			}
		}
	}
	else if (targa_header.image_type==10) {   // Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;

		red = 0;
		green = 0;
		blue = 0;
		alphabyte = 0xff;

		for(row=(*height)-1; row>=0; row--) {
			pixbuf = targa_rgba + row*(*width)*4;
			for(column=0; column<(*width); ) {
				if(data + 1 > end)
				{
					Sys_Printf("LoadTGA: file truncated\n");
					return NULL;
				}
				packetHeader= *data++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        // run-length packet
					if(data + targa_header.pixel_size/8 > end)
					{
						Sys_Printf("LoadTGA: file truncated\n");
						return NULL;
					}
					switch (targa_header.pixel_size) {
						case 24:
								blue = *data++;
								green = *data++;
								red = *data++;
								alphabyte = 255;
								break;
						case 32:
								blue = *data++;
								green = *data++;
								red = *data++;
								alphabyte = *data++;
								break;
						default:
							Sys_Printf("LoadTGA: illegal pixel_size '%d'\n", targa_header.pixel_size);
							return NULL;
					}

					for(j=0;j<packetSize;j++) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==(*width)) { // run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*(*width)*4;
						}
					}
				}
				else {                            // non run-length packet

					if(data + targa_header.pixel_size/8*packetSize > end)
					{
						Sys_Printf("LoadTGA: file truncated\n");
						return NULL;
					}
					for(j=0;j<packetSize;j++) {
						switch (targa_header.pixel_size) {
							case 24:
									blue = *data++;
									green = *data++;
									red = *data++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = *data++;
									green = *data++;
									red = *data++;
									alphabyte = *data++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
							default:
								Sys_Printf("LoadTGA: illegal pixel_size '%d'\n", targa_header.pixel_size);
								return NULL;
						}
						column++;
						if (column==(*width)) { // pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*(*width)*4;
						}
					}
				}
			}
			breakOut:;
		}
	}

	/* top-down image, revert */
	if (targa_header.attributes & 0x20) {
		/* TODO: optimize this (or just use SDL_image) */
		int i, j;
		unsigned char *pixels;

		pixels = (unsigned char *)Sys_MemAlloc(&tmp_mem, numPixels, "texturedata3");

		for (j = 0; j < (*height); j++)
		{
			for (i = 0; i < (*width); i++)
			{
				pixels[(j * (*width) + i) * 4 + 0] = targa_rgba[((*height - j - 1) * (*width) + i) * 4 + 0];
				pixels[(j * (*width) + i) * 4 + 1] = targa_rgba[((*height - j - 1) * (*width) + i) * 4 + 1];
				pixels[(j * (*width) + i) * 4 + 2] = targa_rgba[((*height - j - 1) * (*width) + i) * 4 + 2];
				pixels[(j * (*width) + i) * 4 + 3] = targa_rgba[((*height - j - 1) * (*width) + i) * 4 + 3];
			}
		}

		targa_rgba = pixels;
	}

	/* swap from rgba to bgra */
	for (pixbuf = targa_rgba; pixbuf < targa_rgba + (*width) * (*height) * 4; pixbuf += 4)
	{
		tmp = pixbuf[0];
		pixbuf[0] = pixbuf[2];
		pixbuf[2] = tmp;
	}

	return targa_rgba;
}

/*
===================
Sys_GenerateMipMaps

Generates mipmaps in GL_BGRA and GL_UNSIGNED_INT_8_8_8_8_REV data.
The input data must have enough space to hold the number of levels required.
level == 0 means original data
level == 1 means width/2 and height/2
etc...
TODO: other, better, filtering methods! This is just for testing
TODO: for alpha textures, do not mix, separate and use the pixels type that
appear the most (for ties, alternate selecting alpha or non alpha depending
on the miplevel). Use the discarded pixels somehow on the neighboring pixels'
filtering.
===================
*/
void Sys_GenerateMipMaps(const unsigned char *name, unsigned char *data, const int width, const int height, const int level_count)
{
	unsigned char *data_previous_ptr;
	int width_previous;
	int height_previous;
	unsigned char *data_start_ptr;
	int level_start;
	int width_start;
	int height_start;

	data_previous_ptr = data;
	width_previous = width;
	height_previous = height;
	data_start_ptr = data + width * height * 4;
	level_start = 1;
	width_start = width / 2;
	height_start = height / 2;

	/* TODO CONSOLEDEBUG Sys_Printf("Generating %d mimaps for %s (%dx%d)\n", level_count, name, width, height); */

	while (level_start <= level_count)
	{
		int i, j, k;
		for (i = 0; i < height_start; i++)
		{
			for (j = 0; j < width_start; j++)
			{
				for (k = 0; k < 4; k++)
				{
					data_start_ptr[(i * width_start + j) * 4 + k] = (data_previous_ptr[(i * 2 * width_previous + j * 2) * 4 + k] +
																	data_previous_ptr[((i * 2 + 1) * width_previous + j * 2 + 1) * 4 + k] +
																	data_previous_ptr[(i * 2 * width_previous + j * 2) * 4 + k] +
																	data_previous_ptr[((i * 2 + 1) * width_previous + j * 2 + 1) * 4 + k]) / 4;
				}
			}
		}

		data_previous_ptr += width_previous * height_previous * 4;
		width_previous /= 2;
		height_previous /= 2;
		data_start_ptr += width_start * height_start * 4;
		level_start++;
		width_start /= 2;
		height_start /= 2;
	}
}

/*
===================
Sys_LoadTextureData

Returns GL_BGRA and GL_UNSIGNED_INT_8_8_8_8_REV data.
===================
*/
void Sys_LoadTextureData(const char *name, int *outwidth, int *outheight, unsigned char **outdata, mempool_t *mempool)
{
	int size;
	unsigned char *data;
	unsigned char *pixels;
	char path[MAX_PATH];
	int width, height, i;

	Sys_Snprintf(path, MAX_PATH, "textures/%s.tga", name);

	for (i = 0; i < MAX_PATH; i++)
		if (path[i] == ':')
			path[i] = '_'; /* TODO FIXME: hack for some materials */

	/* TODO: endianness for binary formats */
	if ((size = Host_FSLoadBinaryFile(path, mempool, "texturedata", &data, false)) == -1)
		Host_Error("Sys_LoadTextureData: file not found: \"%s\"\n", path);
	if ((pixels = Sys_ProcessTextureTGA(data, size, &width, &height)) == NULL)
		Sys_Error("Sys_LoadTextureData: error processing \"%s\", file probably corrupted.\n", path);

	*outwidth = width;
	*outheight = height;
	*outdata = pixels;
}

#ifndef DEDICATED_SERVER

/*
===================
Sys_LoadTexture

Should only be called by CL_LoadTexture
Helper functions should return GL_BGRA and GL_UNSIGNED_INT_8_8_8_8_REV data.
indata, inwidth and inheight are optional, for loading generated textures only.
If data_has_mipmaps is true, then mipmaps will be loaded using Host_FSLoadBinaryFile OR sequentially after indata, depending if indata is set or not.
mipmapuntilwidth and mipmapuntilheight are use to define the size smallest mipmaps (useful for a texture atlas, set to a size where the smallest individual texture will be 1x1 (yes, if the contained textures have different sizes only the smallest will go to 1x1))
mipmapuntilwidth and mipmapuntilheight can be approximates, the next bigger mipmap size will be used.
cl_id must be -1 or a textures[] (in cl_video.c) index

NOTE: it's useful to map to the CENTER of the corner pixels if using texture atlas, to avoid filtering with borders! TODO: check this in the code
TODO FIXME: some implementations (GLES mostly) NEED power of two textures
TODO: do not make mipmaps for 2d data (use mipmapuntilwidth and mipmapuntilheight as -1? or 0? or INT_MAX?)
===================
*/
void Sys_LoadTexture(const char *name, int cl_id, unsigned int *id, int *outwidth, int *outheight, unsigned char *indata, int inwidth, int inheight, int data_has_mipmaps, int mipmapuntilwidth, int mipmapuntilheight)
{
	int marker;
	unsigned char *pixels;
	char newname[MAX_PATH];
	int miplevel = 0;
	int miplevelwidth;
	int miplevelheight;
	int has_alpha = false;

	if (mipmapuntilwidth < 1 || mipmapuntilheight < 1)
		Sys_Error("Sys_LoadTexture: error processing \"%s\", minimum mipmap size should be 1x1, called with %dx%d.\n", mipmapuntilwidth, mipmapuntilheight);

	marker = Sys_MemLowMark(&tmp_mem);

	bound_texture0 = -1; /* clear the cache */
	glActiveTexture(GL_TEXTURE0); /* TODO: needed? */
	/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_2D); */

	glGenTextures(1, id);
	glBindTexture(GL_TEXTURE_2D, *id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	if (r_linearfiltering->doublevalue)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	}

	while (1)
	{
		/* subsequent passes, halve the coordinates */
		if (miplevel > 0)
		{
			/* TODO: does this work correctly? a 1x1 texture would also generate a 1x1 second level mipmap? */
			miplevelwidth >>= 1;
			miplevelheight >>= 1;

			if (miplevelwidth < mipmapuntilwidth && miplevelheight < mipmapuntilheight)
				break;

			if (miplevelwidth < mipmapuntilwidth)
				miplevelwidth = mipmapuntilwidth;
			if (miplevelheight < mipmapuntilheight)
				miplevelheight = mipmapuntilheight;
		}

		if (!indata)
		{
			int newwidth, newheight;
			if (data_has_mipmaps)
				Sys_Snprintf(newname, MAX_PATH, "%s_miplevel%d", name, miplevel);
			else
				Sys_Snprintf(newname, MAX_PATH, "%s", name);

			Sys_LoadTextureData(newname, &newwidth, &newheight, &pixels, &tmp_mem);

			if (miplevel == 0)
			{
				/* the first one, let's store initial data */
				*outwidth = newwidth;
				*outheight = newheight;
				miplevelwidth = newwidth;
				miplevelheight = newheight;
			}
			else if (newwidth != miplevelwidth || newheight != miplevelheight)
			{
				/* loaded data has wrong size for this miplevel! */
				Sys_Error("Sys_LoadTexture: error processing \"%s\", miplevel %d mismatch: %dx%d should be %dx%d.\n", newname, miplevel, newwidth, newheight, miplevelwidth, miplevelheight);
			}
		}
		else
		{
			/* TODO: test */
			pixels = indata;

			if (miplevel == 0)
			{
				/* the first one, let's store initial data */
				*outwidth = inwidth;
				*outheight = inheight;
				miplevelwidth = inwidth;
				miplevelheight = inheight;
			}

			indata += miplevelwidth * miplevelheight * 4; /* four component data - FIXME */
		}

		/* do this here because we now know the size of the image for sure */
		if (miplevel == 0)
		{
			int calcmip = 0;
			int calcwidth = *outwidth;
			int calcheight = *outheight;

			/* TODO: cvar for this! */
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			while (1)
			{
				/* TODO: does this work correctly? a 1x1 texture would also generate a 1x1 second level mipmap? */
				calcwidth >>= 1;
				calcheight >>= 1;

				if (calcwidth < mipmapuntilwidth && calcheight < mipmapuntilheight)
					break;

				if (calcwidth < mipmapuntilwidth)
					calcwidth = mipmapuntilwidth;
				if (calcheight < mipmapuntilheight)
					calcheight = mipmapuntilheight;

				calcmip++;
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, calcmip); /* default is 1000, according to gl 4.5 spec */

			/* count alpha pixels only on first miplevel */
			if (cl_id != -1) /* we have a slot to store this kind of data */
			{
				int i;
				unsigned char *alphabytes = pixels + 3;
				for (i = 0; i < miplevelwidth * miplevelheight; i++)
				{
					if (*alphabytes < 255)
					{
						has_alpha = true;
						break;
					}
					alphabytes += 4;
				}
			}

			if (has_alpha)
				texture_sys[cl_id].texture_has_alpha = true;
			else
				texture_sys[cl_id].texture_has_alpha = false;
			texture_sys[cl_id].id = *id;
			texture_sys[cl_id].cl_id = cl_id;
		}

		if (!miplevelwidth || !miplevelheight)
			Sys_Error("Sys_LoadTexture: error processing \"%s\", invalid dimension: %dx%d for miplevel %d.\n", name, miplevelwidth, miplevelheight, miplevel);
		if (miplevelwidth > max_texture_size)
			Sys_Error("Sys_LoadTexture: error processing \"%s\", width %d > %d for miplevel %d.\n", name, miplevelwidth, max_texture_size, miplevel);
		if (miplevelheight > max_texture_size)
			Sys_Error("Sys_LoadTexture: error processing \"%s\", height %d > %d for miplevel %d.\n", name, miplevelheight, max_texture_size, miplevel);

		/* TODO CONSOLEDEBUG Sys_Printf("Sys_LoadTexture: %s miplevel %d, %dx%d%s\n", indata ? name : newname, miplevel, miplevelwidth, miplevelheight, has_alpha ? " (cl texture marked as having alpha)" : ""); */
		glTexImage2D(GL_TEXTURE_2D, miplevel, GL_RGBA8, miplevelwidth, miplevelheight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);

		if (!data_has_mipmaps)
		{
			glGenerateMipmap(GL_TEXTURE_2D); /* TODO? which GL versions? */
			/* TODO: mipmap generation for framebuffer textures after rendering to them, when needed (for in-game stuff like monitors) (bind colorbuffer to GL_TEXTURE_2D and call glGenerateMipMap) */
			break;
		}

		miplevel++;
	}

	Sys_MemFreeToLowMark(&tmp_mem, marker);
}

/*
===================
Sys_UnloadTexture

Should only be called by CL_CleanTextures
===================
*/
void Sys_UnloadTexture(unsigned int *id)
{
	glDeleteTextures(1, id);
}

/*
===================
Sys_BindTexture
===================
*/
void Sys_BindTexture(unsigned int id, int slot)
{
	if (slot == GL_TEXTURE0)
	{
		if (bound_texture0 == id)
			return;
		if (bound_shader == SHADER_DEPTHSTORE || bound_shader == SHADER_DEPTHSTORE_SKINNING || bound_shader == SHADER_DEPTHSTORE_TERRAIN)
			return;

		bound_texture0 = id;

		if (bound_texture0 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE0);
			glDisable(GL_TEXTURE_2D); */
			return;
		}

		glActiveTexture(GL_TEXTURE0);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_2D); */
		glBindTexture(GL_TEXTURE_2D, bound_texture0);
	}
	else if (slot == GL_TEXTURE1)
	{
		if (bound_texture1 == id)
			return;
		if (bound_shader == SHADER_DEPTHSTORE || bound_shader == SHADER_DEPTHSTORE_SKINNING || bound_shader == SHADER_DEPTHSTORE_TERRAIN)
			return;

		bound_texture1 = id;

		if (bound_texture1 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE1);
			glDisable(GL_TEXTURE_2D); */
			return;
		}

		glActiveTexture(GL_TEXTURE1);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_2D); */
		glBindTexture(GL_TEXTURE_2D, bound_texture1);
	}
	else if (slot == GL_TEXTURE2) /* CUBEMAP */
	{
		if (bound_texture2 == id)
			return;

		bound_texture2 = id;

		if (bound_texture2 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE2);
			glDisable(GL_TEXTURE_CUBE_MAP); */
			return;
		}

		glActiveTexture(GL_TEXTURE2);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_CUBE_MAP); */
		glBindTexture(GL_TEXTURE_CUBE_MAP, bound_texture2);
	}
	else if (slot == GL_TEXTURE3)
	{
		if (bound_texture3 == id)
			return;
		if (bound_shader == SHADER_DEPTHSTORE || bound_shader == SHADER_DEPTHSTORE_SKINNING || bound_shader == SHADER_DEPTHSTORE_TERRAIN)
			return;

		bound_texture3 = id;

		if (bound_texture3 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE3);
			glDisable(GL_TEXTURE_2D); */
			return;
		}

		glActiveTexture(GL_TEXTURE3);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_2D); */
		glBindTexture(GL_TEXTURE_2D, bound_texture3);
	}
	else if (slot == GL_TEXTURE4)
	{
		if (bound_texture4 == id)
			return;
		if (bound_shader == SHADER_DEPTHSTORE || bound_shader == SHADER_DEPTHSTORE_SKINNING || bound_shader == SHADER_DEPTHSTORE_TERRAIN)
			return;

		bound_texture4 = id;

		if (bound_texture4 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE4);
			glDisable(GL_TEXTURE_2D); */
			return;
		}

		glActiveTexture(GL_TEXTURE4);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_2D); */
		glBindTexture(GL_TEXTURE_2D, bound_texture4);
	}
	else if (slot == GL_TEXTURE5)
	{
		if (bound_texture5 == id)
			return;
		if (bound_shader == SHADER_DEPTHSTORE || bound_shader == SHADER_DEPTHSTORE_SKINNING || bound_shader == SHADER_DEPTHSTORE_TERRAIN)
			return;

		bound_texture5 = id;

		if (bound_texture5 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE5);
			glDisable(GL_TEXTURE_2D); */
			return;
		}

		glActiveTexture(GL_TEXTURE5);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_2D); */
		glBindTexture(GL_TEXTURE_2D, bound_texture5);
	}
	else if (slot == GL_TEXTURE6) /* CUBEMAP */
	{
		if (bound_texture6 == id)
			return;

		bound_texture6 = id;

		if (bound_texture6 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE6);
			glDisable(GL_TEXTURE_CUBE_MAP); */
			return;
		}

		glActiveTexture(GL_TEXTURE6);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_CUBE_MAP); */
		glBindTexture(GL_TEXTURE_CUBE_MAP, bound_texture6);
	}
	else if (slot == GL_TEXTURE7) /* CUBEMAP */
	{
		if (bound_texture7 == id)
			return;

		bound_texture7 = id;

		if (bound_texture7 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE7);
			glDisable(GL_TEXTURE_CUBE_MAP); */
			return;
		}

		glActiveTexture(GL_TEXTURE7);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_CUBE_MAP); */
		glBindTexture(GL_TEXTURE_CUBE_MAP, bound_texture7);
	}
	else if (slot == GL_TEXTURE8) /* CUBEMAP */
	{
		if (bound_texture8 == id)
			return;

		bound_texture8 = id;

		if (bound_texture8 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE8);
			glDisable(GL_TEXTURE_CUBE_MAP); */
			return;
		}

		glActiveTexture(GL_TEXTURE8);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_CUBE_MAP); */
		glBindTexture(GL_TEXTURE_CUBE_MAP, bound_texture8);
	}
	else if (slot == GL_TEXTURE9) /* CUBEMAP */
	{
		if (bound_texture9 == id)
			return;

		bound_texture9 = id;

		if (bound_texture9 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE9);
			glDisable(GL_TEXTURE_CUBE_MAP); */
			return;
		}

		glActiveTexture(GL_TEXTURE9);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_CUBE_MAP); */
		glBindTexture(GL_TEXTURE_CUBE_MAP, bound_texture9);
	}
	else if (slot == GL_TEXTURE10) /* CUBEMAP */
	{
		if (bound_texture10 == id)
			return;

		bound_texture10 = id;

		if (bound_texture10 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE10);
			glDisable(GL_TEXTURE_CUBE_MAP); */
			return;
		}

		glActiveTexture(GL_TEXTURE10);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_CUBE_MAP); */
		glBindTexture(GL_TEXTURE_CUBE_MAP, bound_texture10);
	}
	else if (slot == GL_TEXTURE11) /* CUBEMAP */
	{
		if (bound_texture11 == id)
			return;

		bound_texture11 = id;

		if (bound_texture11 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE11);
			glDisable(GL_TEXTURE_CUBE_MAP); */
			return;
		}

		glActiveTexture(GL_TEXTURE11);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_CUBE_MAP); */
		glBindTexture(GL_TEXTURE_CUBE_MAP, bound_texture11);
	}
	else if (slot == GL_TEXTURE12) /* CUBEMAP */
	{
		if (bound_texture12 == id)
			return;

		bound_texture12 = id;

		if (bound_texture12 == -1)
		{
			/* THIS IS FIXED FUNCTION glActiveTexture(GL_TEXTURE12);
			glDisable(GL_TEXTURE_CUBE_MAP); */
			return;
		}

		glActiveTexture(GL_TEXTURE12);
		/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_CUBE_MAP); */
		glBindTexture(GL_TEXTURE_CUBE_MAP, bound_texture12);
	}
}

/*
============================================================================

Framebuffer Objects Management

---

Shadowing using shadow mapping and lighting
BEWARE of znear and zfar when using this technique

TODO:
-diferentiate between directional and omnidirectional lights
-perspective and ortographic projections too?
-how to take into account transparency? (make it shadow "just a little")
-do a depth pass of the entire scene first, to avoid sampling shadow maps
more than once?
-colored lighting (use opengl lights or pass the color to the fragment shader?)
-self-shadowing (a GL_FRONT face won't cast shadow in another GL_FRONT face?)
-remove _EXT, ARB, etc
-when doing omnidirectional shadow cube mapping, cull if the light frustum
(whose range depends on light intensity) doesn't intersect the camera frustum,
this is important! can it be done with a depth-first pass to help about fully
occluded lights frustums?
-do something special about the entities attached to lights? they will mess
with the znear if they have a model (and the light may start inside them)
-penumbra acoording to distance from shadow caster to shadow destination
-for transparent entities, fetch color from the light point of view at the
same position as depth!! there you have the color of the shadow :D

============================================================================
*/

/* framebuffer id for rendering from the light's point of view */
GLuint sm_fbo_id[SHADER_MAX_LIGHTS];
/* z-values will be rendered to this texture when using the fbo_id framebuffer */
GLuint sm_depth_texturecube_id[SHADER_MAX_LIGHTS];

/* framebuffer id for rendering from the camera's point of view */
GLuint cam_fbo_id = 0;
GLuint cam_depth_renderbuffer_id = 0;
GLuint cam_color_texture_id = 0;

/* TODO: configurable */
/* width and height must be equal for cubemaps, doh! */
const int shadowmap_width = 1024;
const int shadowmap_height = 1024;

/*
===================
Sys_FBOGenerate

Should only be called by Sys_InitVideo and Sys_VideoChangeMode
TODO: check sm_fbo_id framebuffer completeness/status
TODO: changing resolutions LOSE the shadowmapping framebuffer
===================
*/
void Sys_FBOGenerate(vec_t width, vec_t height)
{
	const GLenum draw_buffers[1] = {GL_COLOR_ATTACHMENT0};
	GLenum error;
	int i;
	int light;

	bound_texture0 = -1; /* clear the cache */
	glActiveTexture(GL_TEXTURE0); /* TODO: needed? */
	/* THIS IS FIXED FUNCTION glEnable(GL_TEXTURE_2D); */

	for (light = 0; light < SHADER_MAX_LIGHTS; light++)
	{
		/* depth cube map */
		glGenTextures(1, &sm_depth_texturecube_id[light]);
		glBindTexture(GL_TEXTURE_CUBE_MAP, sm_depth_texturecube_id[light]);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		/* THIS IS FIXED FUNCTION glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE); */
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		for (i = 0; i < 6; ++i) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, shadowmap_width, shadowmap_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

		/* shadow framebuffer */
		glGenFramebuffers(1, &sm_fbo_id[light]);
		glBindFramebuffer(GL_FRAMEBUFFER, sm_fbo_id[light]);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		/* WIN32 and MAC OS X BUG: even with glDrawBuffer(GL_NONE), a color buffer is necessary! Appears to be fixed... */
	}

	/* camera point of view texture and renderbuffer */
	glGenRenderbuffers(1, &cam_depth_renderbuffer_id);
	glBindRenderbuffer(GL_RENDERBUFFER, cam_depth_renderbuffer_id);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, (int)width, (int)height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glGenTextures(1, &cam_color_texture_id);
	glBindTexture(GL_TEXTURE_2D, cam_color_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)width, (int)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	/* camera framebuffer */
	glGenFramebuffers(1, &cam_fbo_id);
	glBindFramebuffer(GL_FRAMEBUFFER, cam_fbo_id);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cam_depth_renderbuffer_id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cam_color_texture_id, 0);
	glDrawBuffers(1, draw_buffers);
	error = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (error != GL_FRAMEBUFFER_COMPLETE)
		Sys_Error("Sys_ShadowFBOGenerate: camera framebuffer not complete: %s", gluErrorString(error));
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	Sys_Printf("Shadowmapping framebuffer(s): %dx%d\n", shadowmap_width, shadowmap_height);
	Sys_Printf("Camera framebuffer: %dx%d\n", (int)width, (int)height);
}

/*
===================
Sys_FBOCleanup

Should only be called by Sys_ShutdownVideo and Sys_VideoChangeMode
===================
*/
void Sys_FBOCleanup(void)
{
	int light;

	for (light = 0; light < SHADER_MAX_LIGHTS; light++)
	{
		if (sm_fbo_id[light])
		{
			glDeleteFramebuffers(1, &sm_fbo_id[light]);
			sm_fbo_id[light] = 0;
		}
		if (sm_depth_texturecube_id[light])
		{
			glDeleteTextures(1, &sm_depth_texturecube_id[light]);
			sm_depth_texturecube_id[light] = 0;
		}
	}

	Sys_Printf("Shadowmapping framebuffer(s) destroyed.\n");

	if (cam_fbo_id)
	{
		glDeleteFramebuffers(1, &cam_fbo_id);
		cam_fbo_id = 0;
	}
	if (cam_depth_renderbuffer_id)
	{
		glDeleteRenderbuffers(1, &cam_depth_renderbuffer_id);
		cam_depth_renderbuffer_id = 0;
	}
	if (cam_color_texture_id)
	{
		glDeleteTextures(1, &cam_color_texture_id);
		cam_color_texture_id = 0;
	}

	Sys_Printf("Camera framebuffer destroyed.\n");
}

/*
============================================================================

Shader loading

Shaders should be loaded only ONCE per GL context, they should be deleted
by the driver when the context ends.

TODO: is it deleted? buggy drivers, you know.
TODO: uniform color is 0-1 and vertex color is 0-255?

============================================================================
*/

/* TODO: pass these as parameters to allow multithreaded calculations? */
/* TODO: be ware of PADDING when using these globals with SIMD, etc! We do some memory copies here that may break */

static vec_t modelviewmatrix[16];
static vec_t modelviewmatrix_normals[9];
static vec_t modelviewprojectionmatrix[16];
static vec_t modelmatrix[16];
static vec_t viewmatrix[16];
static vec_t projectionmatrix[16];

#define SHADOW_FOV_Y	90.f
#define SHADOW_ASPECT	1.f
#define SHADOW_NEAR		0.1
	/* TODO: calculate zfar for light_range with respect to the light intensity (ZFAR and intensity 1:1 is wrong) because it may get us illumination without shadows if the value is too low */
#define SHADOW_FAR		100

static vec_t camera_view_matrix_inv[16];

static vec_t light_position_cameraspace[SHADER_MAX_LIGHTS][3];
static vec_t light_view_matrix[SHADER_MAX_LIGHTS][16];
static vec_t light_face_matrix[6][16];
static vec_t light_projection_matrix[SHADER_MAX_LIGHTS][16];
static vec_t light_intensity[SHADER_MAX_LIGHTS];

const static vec3_t light_faces_forward[6] =
{
	 1.0, 0.0,  0.0, /* +X */
	-1.0, 0.0,  0.0, /* -X */
	 0.0, 1.0,  0.0, /* +Y */
	 0.0,-1.0,  0.0, /* -Y */
	 0.0, 0.0,  1.0, /* +Z */
	 0.0, 0.0, -1.0, /* -Z */
};

/* cubemaps have weird Y direction */
const static vec3_t light_faces_up[6] =
{
	0.0, -1.0,  0.0,
	0.0, -1.0,  0.0,
	0.0,  0.0,  1.0,
	0.0,  0.0, -1.0,
	0.0, -1.0,  0.0,
	0.0, -1.0,  0.0,
};

/*
===================
Sys_ShadowCreateLightMatrices

Creates the static light matrices
===================
*/
void Sys_ShadowCreateLightMatrices(void)
{
	/* create light face matrices */
	Math_MatrixLookAt4x4(light_face_matrix[0], 0.0, 0.0, 0.0, light_faces_forward[0][0], light_faces_forward[0][1], light_faces_forward[0][2], light_faces_up[0][0], light_faces_up[0][1], light_faces_up[0][2]);
	Math_MatrixLookAt4x4(light_face_matrix[1], 0.0, 0.0, 0.0, light_faces_forward[1][0], light_faces_forward[1][1], light_faces_forward[1][2], light_faces_up[1][0], light_faces_up[1][1], light_faces_up[1][2]);
	Math_MatrixLookAt4x4(light_face_matrix[2], 0.0, 0.0, 0.0, light_faces_forward[2][0], light_faces_forward[2][1], light_faces_forward[2][2], light_faces_up[2][0], light_faces_up[2][1], light_faces_up[2][2]);
	Math_MatrixLookAt4x4(light_face_matrix[3], 0.0, 0.0, 0.0, light_faces_forward[3][0], light_faces_forward[3][1], light_faces_forward[3][2], light_faces_up[3][0], light_faces_up[3][1], light_faces_up[3][2]);
	Math_MatrixLookAt4x4(light_face_matrix[4], 0.0, 0.0, 0.0, light_faces_forward[4][0], light_faces_forward[4][1], light_faces_forward[4][2], light_faces_up[4][0], light_faces_up[4][1], light_faces_up[4][2]);
	Math_MatrixLookAt4x4(light_face_matrix[5], 0.0, 0.0, 0.0, light_faces_forward[5][0], light_faces_forward[5][1], light_faces_forward[5][2], light_faces_up[5][0], light_faces_up[5][1], light_faces_up[5][2]);
}

/*
===================
Sys_ShadowUpdateLightMatrices

Updates the dynamic light matrices
===================
*/
void Sys_ShadowUpdateLightMatrices(vec3_t cameraorigin, vec3_t cameraangles, vec_t *lightoriginworldspace, vec_t *light_range, int light_position_cameraspace_only)
{
	int light;

	if (!light_position_cameraspace_only)
	{
		for (light = 0; light < SHADER_MAX_LIGHTS; light++)
		{
			/* create light projection matrix */
			double xmin, xmax, ymin, ymax;

			if (!light_range[light])
				break;

			/* TODO: see if the zfar is right */
			Math_PerspectiveToFrustum(SHADOW_FOV_Y, SHADOW_ASPECT, SHADOW_NEAR, SHADOW_FAR, &xmin, &xmax, &ymin, &ymax);
			Math_MatrixPerspectiveFrustum4x4(light_projection_matrix[light], (vec_t)xmin, (vec_t)xmax, (vec_t)ymin, (vec_t)ymax, (vec_t)SHADOW_NEAR, (vec_t)SHADOW_FAR);
		}
	}

	if (!light_position_cameraspace_only)
	{
		/* create camera inverse matrix */
		camera_view_matrix_inv[ 0] = viewmatrix[ 0];
		camera_view_matrix_inv[ 1] = viewmatrix[ 4];
		camera_view_matrix_inv[ 2] = viewmatrix[ 8];
		camera_view_matrix_inv[ 4] = viewmatrix[ 1];
		camera_view_matrix_inv[ 5] = viewmatrix[ 5];
		camera_view_matrix_inv[ 6] = viewmatrix[ 9];
		camera_view_matrix_inv[ 8] = viewmatrix[ 2];
		camera_view_matrix_inv[ 9] = viewmatrix[ 6];
		camera_view_matrix_inv[10] = viewmatrix[10];
		camera_view_matrix_inv[12] = cameraorigin[0];
		camera_view_matrix_inv[13] = cameraorigin[1];
		camera_view_matrix_inv[14] = cameraorigin[2];
		camera_view_matrix_inv[ 3] = 0.0f;
		camera_view_matrix_inv[ 7] = 0.0f;
		camera_view_matrix_inv[11] = 0.0f;
		camera_view_matrix_inv[15] = 1.0f;

		for (light = 0; light < SHADER_MAX_LIGHTS; light++)
		{
			if (!light_range[light])
				break;
			/* create the light view matrix (construct this as you would a camera matrix) */
			Math_MatrixIdentity4x4(light_view_matrix[light]);
			Math_MatrixTranslate4x4(light_view_matrix[light], -lightoriginworldspace[light * 3], -lightoriginworldspace[light * 3 + 1], -lightoriginworldspace[light * 3 + 2]);
		}
	}

	for (light = 0; light < SHADER_MAX_LIGHTS; light++)
	{
		if (!light_range[light])
				break;
		/* transform world space light position to camera space */
		light_position_cameraspace[light][0] =
			viewmatrix[ 0] * lightoriginworldspace[light * 3] +
			viewmatrix[ 4] * lightoriginworldspace[light * 3 + 1] +
			viewmatrix[ 8] * lightoriginworldspace[light * 3 + 2] +
			viewmatrix[12];
		light_position_cameraspace[light][1] =
			viewmatrix[ 1] * lightoriginworldspace[light * 3] +
			viewmatrix[ 5] * lightoriginworldspace[light * 3 + 1] +
			viewmatrix[ 9] * lightoriginworldspace[light * 3 + 2] +
			viewmatrix[13];
		light_position_cameraspace[light][2] =
			viewmatrix[ 2] * lightoriginworldspace[light * 3] +
			viewmatrix[ 6] * lightoriginworldspace[light * 3 + 1] +
			viewmatrix[10] * lightoriginworldspace[light * 3 + 2] +
			viewmatrix[14];
	}
}

static char *shader_uniform_names[SHADER_NUM_UNIFORMS] =
{
	/* general */
	"u_projection",
	"u_view",
	"u_model",
	"u_mv",
	"u_mv_normals",
	"u_mvp",
	"u_color",
	"u_texture0",
	"u_texture1",
	"u_texture3",
	"u_texture4",
	"u_texture5",
	/* shadowmapping */
	"u_texture2",
	"u_texture6",
	"u_texture7",
	"u_texture8",
	"u_texture9",
	"u_texture10",
	"u_texture11",
	"u_texture12",
	"u_camera_view_matrix_inv",
	"u_light_view_matrices",
	"u_light_projection_matrices",
	/* dynamic global lighting */
	"u_light_positions",
	"u_light_intensities",
	/* fixed function light emulation TODO: complete, mainly regarding w of lightpos, unify with global lighting */
	"u_light0position",
	"u_light0diffuse",
	"u_light0ambient",
	/* skeletal */
	"u_bonemats"
};

static GLuint shader_uniform_locations[SHADER_NUM][SHADER_NUM_UNIFORMS];
GLuint shader_program_ids[SHADER_NUM]; /* TODO: make this a static var after iqm has been fixed */

enum {
    SHADER_ATTRIB_VERTEX = 0,
    SHADER_ATTRIB_COLOR = 1,
	SHADER_ATTRIB_TEXCOORD0,
	SHADER_ATTRIB_TEXCOORD1,
	SHADER_ATTRIB_NORMAL,
	SHADER_ATTRIB_TANGENT,
	SHADER_ATTRIB_WEIGHTS,
	SHADER_ATTRIB_BONES,
    SHADER_NUM_ATTRIBUTES
};

static char *shader_attrib_names[SHADER_NUM_ATTRIBUTES] =
{
	"in_position",
	"in_color",
	"in_texcoord0",
	"in_texcoord1",
	"in_normal",
	"in_tangent",
	"in_weights",
	"in_bones"
};

/*
===================
Sys_VideoLoadShaderObject
===================
*/
GLuint Sys_VideoLoadShaderObject(char *name, unsigned int type)
{
	GLuint handle;
	const GLchar *files[1];
	GLint result;
	GLint error_log_length;
	char *error_log_text;
	GLsizei actual_error_log_length;
	char path[MAX_PATH];
	unsigned char *buffer;
	int size;
	int marker;

	marker = Sys_MemLowMark(&tmp_mem);

	Sys_Snprintf(path, MAX_PATH, "shaders/%s.c", name);
	if ((size = Host_FSLoadBinaryFile(path, &tmp_mem, "shaderobject", &buffer, false)) == -1)
		Sys_Error("Sys_VideoLoadShader: couldn't load %s\n", path);

	handle = glCreateShader(type);
	if (!handle)
		Sys_Error("Sys_VideoLoadShader: Shader object failed for %s", path);

	files[0] = (const GLchar *)buffer;
	/* parameters: handle of our shader object, number of strings, strings, size of each string (if NULL for all or zero for one, reads until \0 is found) */
	glShaderSource(handle, 1, files, NULL);
	glCompileShader(handle);
	glGetShaderiv(handle, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &error_log_length);
		error_log_text = (char *)Sys_MemAlloc(&tmp_mem, sizeof(char) * error_log_length, "shaderobjecterror");
		glGetShaderInfoLog(handle, error_log_length, &actual_error_log_length, error_log_text);
		Sys_Error("Sys_VideoLoadShader: Shader '%s' failed compilation:\n-----------------------------\n%s\n-----------------------------\n", path, error_log_text);
	}

	Sys_MemFreeToLowMark(&tmp_mem, marker);

	Sys_Printf("Loaded shader object %s\n", path);

	return handle;
}

/*
===================
Sys_VideoLoadShaderPrograms

TODO: detach and delete shader objects from program after linking? there is word of problems with buggy opengl es drivers
TODO: check for errors here (glGetProgramiv and glGetProgramInfoLog)
TODO: document which shaders use which attributes and uniforms
===================
*/
void Sys_VideoLoadShaderPrograms(void)
{
	GLuint vertex_shader_handle;
	GLuint fragment_shader_handle;
	GLint result;
	GLint error_log_length;
	char *error_log_text;
	GLsizei actual_error_log_length;
	int marker;
	int i, j;

	marker = Sys_MemLowMark(&tmp_mem);

	for (i = 0; i < SHADER_NUM; i++)
	{
		if (shader_vertex_names[i][0])
			vertex_shader_handle = Sys_VideoLoadShaderObject(shader_vertex_names[i], GL_VERTEX_SHADER);

		if (shader_fragment_names[i][0])
			fragment_shader_handle = Sys_VideoLoadShaderObject(shader_fragment_names[i], GL_FRAGMENT_SHADER);

		shader_program_ids[i] = glCreateProgram();
		if (shader_vertex_names[i][0])
			glAttachShader(shader_program_ids[i], vertex_shader_handle);
		if (shader_fragment_names[i][0])
			glAttachShader(shader_program_ids[i], fragment_shader_handle);

		/* TODO: only set what the shader uses? */
		for (j = 0; j < SHADER_NUM_ATTRIBUTES; j++)
			glBindAttribLocation(shader_program_ids[i], j, shader_attrib_names[j]);

		glLinkProgram(shader_program_ids[i]);

		glGetProgramiv(shader_program_ids[i], GL_LINK_STATUS, &result);
		if (!result)
		{
			glGetProgramiv(shader_program_ids[i], GL_INFO_LOG_LENGTH, &error_log_length);
			error_log_text = (char *)Sys_MemAlloc(&tmp_mem, sizeof(char) * error_log_length, "shaderprogramerror");
			glGetProgramInfoLog(shader_program_ids[i], error_log_length, &actual_error_log_length, error_log_text);
			Sys_Error("Sys_VideoLoadShadowMappingShaderPrograms: Shader '%s' failed to link:\n-----------------------------\n%s\n-----------------------------\n", shader_names[i], error_log_text);
		}

		for (j = 0; j < SHADER_NUM_UNIFORMS; j++)
		{
			shader_uniform_locations[i][j] = glGetUniformLocation(shader_program_ids[i], shader_uniform_names[j]);
		}
	}

	bound_shader = -1;

	Sys_MemFreeToLowMark(&tmp_mem, marker);
}

/*
===================
Sys_VideoUpdateUniform

"count" doesn't apply to all uniforms

TODO: are uniforms cached by opengl when changing programs?
===================
*/
typedef struct uniform_cache_s {
	vec_t shader_uniform_projectionmatrix[16];
	vec_t shader_uniform_viewmatrix[16];
	vec_t shader_uniform_modelmatrix[16];
	vec_t shader_uniform_mv[16];
	vec_t shader_uniform_mv_normals[9];
	vec_t shader_uniform_mvp[16];
	vec4_t shader_uniform_color;
	unsigned int shader_uniform_texture0;
	unsigned int shader_uniform_texture1;
	unsigned int shader_uniform_texture3;
	unsigned int shader_uniform_texture4;
	unsigned int shader_uniform_texture5;
	unsigned int shader_uniform_texture2;
	unsigned int shader_uniform_texture6;
	unsigned int shader_uniform_texture7;
	unsigned int shader_uniform_texture8;
	unsigned int shader_uniform_texture9;
	unsigned int shader_uniform_texture10;
	unsigned int shader_uniform_texture11;
	unsigned int shader_uniform_texture12;
	vec_t shader_uniform_camera_view_matrix_inv[16];
	vec_t shader_uniform_light_view_matrices[SHADER_MAX_LIGHTS][16];
	vec_t shader_uniform_light_projection_matrices[SHADER_MAX_LIGHTS][16];
	vec3_t shader_uniform_light_positions[SHADER_MAX_LIGHTS];
	vec_t shader_uniform_light_intensities[SHADER_MAX_LIGHTS];
	vec4_t shader_uniform_light0_position;
	vec4_t shader_uniform_light0_diffuse;
	vec4_t shader_uniform_light0_ambient;
	vec_t shader_uniform_bonemats[SHADER_MAX_BONES][12];
} uniform_cache_t;
static uniform_cache_t uniform_cache[SHADER_NUM];
void Sys_VideoUpdateUniform(const unsigned int uniform_id, const void *data, const int count)
{
	const vec_t *v_vec = (const vec_t *)data;
	const unsigned int *v_uint = (const unsigned int *)data;
	int i;

	switch (uniform_id)
	{
		case SHADER_UNIFORM_PROJECTIONMATRIX:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_PROJECTIONMATRIX] != -1)
			{
				if (Math_MatrixIsEqual4x4(v_vec, uniform_cache[bound_shader].shader_uniform_projectionmatrix))
					break;
				Math_MatrixCopy4x4(uniform_cache[bound_shader].shader_uniform_projectionmatrix, v_vec);
				glUniformMatrix4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_PROJECTIONMATRIX], 1, false, v_vec);
			}
			break;
		case SHADER_UNIFORM_VIEWMATRIX:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_VIEWMATRIX] != -1)
			{
				if (Math_MatrixIsEqual4x4(v_vec, uniform_cache[bound_shader].shader_uniform_viewmatrix))
					break;
				Math_MatrixCopy4x4(uniform_cache[bound_shader].shader_uniform_viewmatrix, v_vec);
				glUniformMatrix4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_VIEWMATRIX], 1, false, v_vec);
			}
			break;
		case SHADER_UNIFORM_MODELMATRIX:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_MODELMATRIX] != -1)
			{
				if (Math_MatrixIsEqual4x4(v_vec, uniform_cache[bound_shader].shader_uniform_modelmatrix))
					break;
				Math_MatrixCopy4x4(uniform_cache[bound_shader].shader_uniform_modelmatrix, v_vec);
				glUniformMatrix4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_MODELMATRIX], 1, false, v_vec);
			}
			break;
		case SHADER_UNIFORM_MV:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_MV] != -1)
			{
				if (Math_MatrixIsEqual4x4(v_vec, uniform_cache[bound_shader].shader_uniform_mv))
					break;
				Math_MatrixCopy4x4(uniform_cache[bound_shader].shader_uniform_mv, v_vec);
				glUniformMatrix4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_MV], 1, false, v_vec);
			}
			break;
		case SHADER_UNIFORM_MV_NORMALS:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_MV_NORMALS] != -1)
			{
				if (Math_MatrixIsEqual3x3(v_vec, uniform_cache[bound_shader].shader_uniform_mv_normals))
					break;
				Math_MatrixCopy3x3(uniform_cache[bound_shader].shader_uniform_mv_normals, v_vec);
				glUniformMatrix3fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_MV_NORMALS], 1, false, v_vec); /* TODO: use transposition here in the parameter instead of doing it manually? */
			}
			break;
		case SHADER_UNIFORM_MVP:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_MVP] != -1)
			{
				if (Math_MatrixIsEqual4x4(v_vec, uniform_cache[bound_shader].shader_uniform_mvp))
					break;
				Math_MatrixCopy4x4(uniform_cache[bound_shader].shader_uniform_mvp, v_vec);
				glUniformMatrix4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_MVP], 1, false, v_vec);
			}
			break;
		case SHADER_UNIFORM_COLOR:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_COLOR] != -1)
			{
				if (Math_Vector4Compare(v_vec, uniform_cache[bound_shader].shader_uniform_color))
					break;
				Math_Vector4Copy(v_vec, uniform_cache[bound_shader].shader_uniform_color);
				glUniform4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_COLOR], 1, v_vec);
			}
			break;
		case SHADER_UNIFORM_TEXTURE0:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE0] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture0)
					break;
				uniform_cache[bound_shader].shader_uniform_texture0 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE0], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE1:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE1] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture1)
					break;
				uniform_cache[bound_shader].shader_uniform_texture1 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE1], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE3:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE3] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture3)
					break;
				uniform_cache[bound_shader].shader_uniform_texture3 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE3], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE4:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE4] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture4)
					break;
				uniform_cache[bound_shader].shader_uniform_texture4 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE4], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE5:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE5] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture5)
					break;
				uniform_cache[bound_shader].shader_uniform_texture5 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE5], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE2:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE2] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture2)
					break;
				uniform_cache[bound_shader].shader_uniform_texture2 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE2], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE6:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE6] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture6)
					break;
				uniform_cache[bound_shader].shader_uniform_texture6 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE6], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE7:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE7] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture7)
					break;
				uniform_cache[bound_shader].shader_uniform_texture7 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE7], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE8:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE8] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture8)
					break;
				uniform_cache[bound_shader].shader_uniform_texture8 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE8], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE9:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE9] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture9)
					break;
				uniform_cache[bound_shader].shader_uniform_texture9 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE9], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE10:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE10] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture10)
					break;
				uniform_cache[bound_shader].shader_uniform_texture10 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE10], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE11:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE11] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture11)
					break;
				uniform_cache[bound_shader].shader_uniform_texture11 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE11], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_TEXTURE12:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE12] != -1)
			{
				if (v_uint[0] == uniform_cache[bound_shader].shader_uniform_texture12)
					break;
				uniform_cache[bound_shader].shader_uniform_texture12 = v_uint[0];
				glUniform1i(shader_uniform_locations[bound_shader][SHADER_UNIFORM_TEXTURE12], v_uint[0]);
			}
			break;
		case SHADER_UNIFORM_CAMERA_VIEW_MATRIX_INV:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_CAMERA_VIEW_MATRIX_INV] != -1)
			{
				if (Math_MatrixIsEqual4x4(v_vec, uniform_cache[bound_shader].shader_uniform_camera_view_matrix_inv))
					break;
				Math_MatrixCopy4x4(uniform_cache[bound_shader].shader_uniform_camera_view_matrix_inv, v_vec);
				glUniformMatrix4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_CAMERA_VIEW_MATRIX_INV], 1, false, v_vec);
			}
			break;
		case SHADER_UNIFORM_LIGHT_VIEW_MATRICES:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT_VIEW_MATRICES] != -1)
			{
				for (i = 0; i < count; i++)
					if (!Math_MatrixIsEqual4x4(&v_vec[16 * i], uniform_cache[bound_shader].shader_uniform_light_view_matrices[i]))
						break;
				if (i == count)
					break;
				memcpy(uniform_cache[bound_shader].shader_uniform_light_view_matrices, v_vec, sizeof(vec_t) * count * 16);
				glUniformMatrix4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT_VIEW_MATRICES], count, false, v_vec);
			}
			break;
		case SHADER_UNIFORM_LIGHT_PROJECTION_MATRICES:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT_PROJECTION_MATRICES] != -1)
			{
				for (i = 0; i < count; i++)
					if (!Math_MatrixIsEqual4x4(&v_vec[16 * i], uniform_cache[bound_shader].shader_uniform_light_projection_matrices[i]))
						break;
				if (i == count)
					break;
				memcpy(uniform_cache[bound_shader].shader_uniform_light_projection_matrices, v_vec, sizeof(vec_t) * count * 16);
				glUniformMatrix4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT_PROJECTION_MATRICES], count, false, v_vec);
			}
			break;
		case SHADER_UNIFORM_LIGHT_POSITIONS:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT_POSITIONS] != -1)
			{
				for (i = 0; i < count; i++)
					if (!Math_Vector3Compare(&v_vec[3 * i], uniform_cache[bound_shader].shader_uniform_light_positions[i]))
						break;
				if (i == count)
					break;
				memcpy(uniform_cache[bound_shader].shader_uniform_light_positions, v_vec, sizeof(vec_t) * count * 3);
				glUniform3fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT_POSITIONS], count, v_vec);
			}
			break;
		case SHADER_UNIFORM_LIGHT_INTENSITIES:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT_INTENSITIES] != -1)
			{
				for (i = 0; i < count; i++)
					if (v_vec[i] != uniform_cache[bound_shader].shader_uniform_light_intensities[i])
						break;
				if (i == count)
					break;
				memcpy(uniform_cache[bound_shader].shader_uniform_light_intensities, v_vec, sizeof(vec_t) * count);
				glUniform1fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT_INTENSITIES], count, v_vec);
			}
			break;
		case SHADER_UNIFORM_LIGHT0_POSITION:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT0_POSITION] != -1)
			{
				if (Math_Vector4Compare(v_vec, uniform_cache[bound_shader].shader_uniform_light0_position))
					break;
				Math_Vector4Copy(v_vec, uniform_cache[bound_shader].shader_uniform_light0_position);
				glUniform4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT0_POSITION], 1, v_vec);
			}
			break;
		case SHADER_UNIFORM_LIGHT0_DIFFUSE:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT0_DIFFUSE] != -1)
			{
				if (Math_Vector4Compare(v_vec, uniform_cache[bound_shader].shader_uniform_light0_diffuse))
					break;
				Math_Vector4Copy(v_vec, uniform_cache[bound_shader].shader_uniform_light0_diffuse);
				glUniform4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT0_DIFFUSE], 1, v_vec);
			}
			break;
		case SHADER_UNIFORM_LIGHT0_AMBIENT:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT0_AMBIENT] != -1)
			{
				if (Math_Vector4Compare(v_vec, uniform_cache[bound_shader].shader_uniform_light0_ambient))
					break;
				Math_Vector4Copy(v_vec, uniform_cache[bound_shader].shader_uniform_light0_ambient);
				glUniform4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_LIGHT0_AMBIENT], 1, v_vec);
			}
			break;
		case SHADER_UNIFORM_BONEMATS:
			if (shader_uniform_locations[bound_shader][SHADER_UNIFORM_BONEMATS] != -1)
			{
				for (i = 0; i < count; i++)
					if (!Math_Vector4Compare(&v_vec[12 * i], uniform_cache[bound_shader].shader_uniform_bonemats[i]))
						break;
				if (i == count)
					break;
				memcpy(uniform_cache[bound_shader].shader_uniform_bonemats, v_vec, sizeof(vec_t) * count * 12);
				glUniformMatrix3x4fv(shader_uniform_locations[bound_shader][SHADER_UNIFORM_BONEMATS], count, false, v_vec);
			}
			break;
		default:
			Sys_Error("Sys_VideoUpdateUniform: unknown uniform: %u\n", uniform_id);
	}
}

void Sys_VideoInitUniforms(void)
{
	int i;

	memset(&uniform_cache, 0, sizeof(uniform_cache));

	for (i = 0; i < SHADER_NUM; i++)
	{
		/* TODO: is this needed? */
		uniform_cache[i].shader_uniform_texture0 = -1;
		uniform_cache[i].shader_uniform_texture1 = -1;
		uniform_cache[i].shader_uniform_texture2 = -1;
		uniform_cache[i].shader_uniform_texture3 = -1;
		uniform_cache[i].shader_uniform_texture4 = -1;
		uniform_cache[i].shader_uniform_texture5 = -1;
		uniform_cache[i].shader_uniform_texture6 = -1;
		uniform_cache[i].shader_uniform_texture7 = -1;
		uniform_cache[i].shader_uniform_texture8 = -1;
		uniform_cache[i].shader_uniform_texture9 = -1;
		uniform_cache[i].shader_uniform_texture10 = -1;
		uniform_cache[i].shader_uniform_texture11 = -1;
		uniform_cache[i].shader_uniform_texture12 = -1;
	}
}

/*
===================
Sys_VideoBindShaderProgram

This function will upload uniforms, but if any uniforms are
updated after calling this function (think matrices), they
should be re-uploaded manually

TODO: pass all uniforms as parameters?
===================
*/
void Sys_VideoBindShaderProgram(unsigned int shader_id, const vec4_t light0_position, const vec4_t light0_diffuse, const vec4_t light0_ambient)
{
	if (shader_id != bound_shader)
	{
		bound_shader = shader_id;

		if (shader_id == -1) /* TODO: remove this when we get to go full-shaders */
		{
			glUseProgram(0);
			return;
		}

		glUseProgram(shader_program_ids[shader_id]);
	}

	Sys_VideoUpdateUniform(SHADER_UNIFORM_PROJECTIONMATRIX, projectionmatrix, 1);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_VIEWMATRIX, viewmatrix, 1);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_MODELMATRIX, modelmatrix, 1);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_MV, modelviewmatrix, 1);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_MV_NORMALS, modelviewmatrix_normals, 1);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_MVP, modelviewprojectionmatrix, 1);
	{ const vec4_t v_color = {1, 1, 1, 1}; Sys_VideoUpdateUniform(SHADER_UNIFORM_COLOR, v_color, 1); } /* TODO */
	{ const unsigned int value = 0; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE0, &value, 1); }
	{ const unsigned int value = 1; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE1, &value, 1); }
	{ const unsigned int value = 3; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE3, &value, 1); }
	{ const unsigned int value = 4; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE4, &value, 1); }
	{ const unsigned int value = 5; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE5, &value, 1); }
	{ const unsigned int value = 2; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE2, &value, 1); }
	{ const unsigned int value = 6; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE6, &value, 1); }
	{ const unsigned int value = 7; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE7, &value, 1); }
	{ const unsigned int value = 8; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE8, &value, 1); }
	{ const unsigned int value = 9; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE9, &value, 1); }
	{ const unsigned int value = 10; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE10, &value, 1); }
	{ const unsigned int value = 11; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE11, &value, 1); }
	{ const unsigned int value = 12; Sys_VideoUpdateUniform(SHADER_UNIFORM_TEXTURE12, &value, 1); }
	Sys_VideoUpdateUniform(SHADER_UNIFORM_CAMERA_VIEW_MATRIX_INV, camera_view_matrix_inv, 1);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_LIGHT_VIEW_MATRICES, light_view_matrix, SHADER_MAX_LIGHTS);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_LIGHT_PROJECTION_MATRICES, light_projection_matrix, SHADER_MAX_LIGHTS);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_LIGHT_POSITIONS, light_position_cameraspace, SHADER_MAX_LIGHTS);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_LIGHT_INTENSITIES, light_intensity, SHADER_MAX_LIGHTS);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_LIGHT0_POSITION, light0_position, 1);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_LIGHT0_DIFFUSE, light0_diffuse, 1);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_LIGHT0_AMBIENT, light0_ambient, 1);
	/* Sys_VideoUpdateUniform(SHADER_UNIFORM_BONEMATS, bonearray, bonecount); TODO */
}

/*
===================
Sys_VideoTransformFor3DModel

Transform the modelview matrix to the entity's position and rotation
Call this before setting the shader and the uniform
TODO: get rid of redundant transforms (being set when setting a shader, if even if caching at least a comparison is made)
===================
*/
void Sys_VideoTransformFor3DModel(vec_t *ent_modelmatrix)
{
	if (ent_modelmatrix)
		Math_MatrixCopy4x4(modelmatrix, ent_modelmatrix);
	else
		Math_MatrixIdentity4x4(modelmatrix);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_MODELMATRIX, modelmatrix, 1);

	Math_MatrixMultiply4x4(modelviewmatrix, viewmatrix, modelmatrix);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_MV, modelviewmatrix, 1);
	{
		vec_t tmp[16];
		Math_MatrixInverse4x4(tmp, modelviewmatrix);
		Math_MatrixTranspose4x4(tmp, tmp);
		Math_Matrix3x3From4x4Top(modelviewmatrix_normals, tmp);
		Sys_VideoUpdateUniform(SHADER_UNIFORM_MV_NORMALS, modelviewmatrix_normals, 1);
	}
	Math_MatrixMultiply4x4(modelviewprojectionmatrix, projectionmatrix, modelviewmatrix);
	Sys_VideoUpdateUniform(SHADER_UNIFORM_MVP, modelviewprojectionmatrix, 1);
}

/*
============================================================================

Vertex buffer objects management with integrated element index buffer

a VBO is in the same format as model_vertex_t
Do not simply draw active VBOs, draw from their parent structures, verifying
if they (the parent structures) are active.

TODO: see if we do some optimizations to use unsigned shorts as indexes when
possible (reduce number of triangles indices)

============================================================================
*/

#define MAX_VBOS			16384

typedef struct vbo_s {
	int				active;
	int				is_triangle_strip;
	int				is_dynamic;

	unsigned int	vertex_vbo;
	int				vertex_stride;
	unsigned int	vertex_count;

	unsigned int	indices_vbo;
	int				indices_stride;
	unsigned int	indices_count;

	/* "Why should both functions error? Because you didn't use a Vertex Array
	 * Object. glEnableVertexAttribArray sets state in the current VAO. There is
	 * no current VAO, so... error. Same goes for glVertexAttribPointer. It's
	 * even in the list of errors for both on those pages.
	 * You don't need a VAO in a compatibility context, but you do in a core
	 * context. Which you asked for. So... you need one."
	 */
	GLuint			vao;
} vbo_t;

vbo_t vbos[MAX_VBOS + 1]; /* one more for the streaming buffer: "Note: the core profile of OpenGL 3.1+ removed the ability to use client memory; there, you must use buffer objects" */
#define STREAMING_VBO MAX_VBOS /* do not change this, code depends on this value */
static int bound_vbo = -1;

/*
===================
Sys_BindVBO

TODO: select which attributes to enable
===================
*/
void Sys_BindVBO(int id)
{
	if (bound_vbo == id)
		return;

	bound_vbo = id;
	if (id == -1)
	{
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); /* TODO: not needed */
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); /* TODO: already part of the vao state? */
		return;
	}

	if (id < 0 || id > MAX_VBOS)
		Sys_Error("Sys_BindVBO: invalid id %d\n", id);

	if (!vbos[id].active)
		return; /* ignore silently if trying to drawn an empty VBO */

	glBindVertexArray(vbos[id].vao);
	/* enable VBO to turn gl*Pointer pointer arguments into offsets to the current VBO */
	glBindBuffer(GL_ARRAY_BUFFER, vbos[id].vertex_vbo); /* TODO: not needed */
	/* bind indices TODO: use GL_UNSIGNED_SHORT or GL_UNSIGNED_BYTE when applicable */
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos[id].indices_vbo); /* TODO: already part of the vao state? */
}

/*
===================
Sys_UpdateVBO

"is_triangle_strip" indicates use of GL_TRIANGLE_STRIP instead of GL_TRIANGLES

TODO: use glMapBuffer? sync problems if using?
TODO: when converting from/to USHORTS/UINTS, take care about the ALLOCATION step
TODO: auto-generate triangle strips and fans for everything, never use GL_TRIANGLES
TODO: I am making a mess with the counts/strides, perhaps sometimes allocating too much memory
===================
*/
void Sys_UpdateVBO(int id, model_trimesh_part_t *trimesh, int is_triangle_strip)
{
	if (id < 0 || id >= MAX_VBOS)
		Sys_Error("Sys_UpdateVBO: invalid id %d\n", id);

    if (!vbos[id].active)
        Sys_Error("Sys_UpdateVBO: vbo %d not active\n", id);

	if (!vbos[id].is_dynamic)
		Sys_Error("Sys_UpdateVBO: vbo %d not dynamic\n", id);

	if (trimesh->vert_count != vbos[id].vertex_count || trimesh->index_count != vbos[id].indices_count)
		Sys_Error("Sys_UpdateVBO: trimesh->vert_count != vbos[id].vertex_count || trimesh->index_count != vbos[id].indices_count for id %d\n", id);

    Sys_BindVBO(id);

    /* use glBufferSubData to avoid discard and reallocation */
	glBufferSubData(GL_ARRAY_BUFFER, 0, trimesh->vert_stride * trimesh->vert_count, trimesh->verts);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, trimesh->index_stride * trimesh->index_count / 3, trimesh->indexes);

	vbos[id].is_triangle_strip = is_triangle_strip;
}

/*

"id" must be set if replacing an already existing VBO, otherwise set to "-1"
Returns the VBO id on success and -1 if no VBO was created
"is_triangle_strip" indicates use of GL_TRIANGLE_STRIP instead of GL_TRIANGLES
"will_be_update" must be set if intending to update the VBO in the future with Sys_UpdateVBO()

TODO: use glMapBuffer? sync problems if using?
TODO: or maybe glBufferSubData when only updating values, to avoid reallocation
TODO: when converting from/to USHORTS/UINTS, take care about the ALLOCATION step
TODO: auto-generate triangle strips and fans for everything, never use GL_TRIANGLES
TODO: I am making a mess with the counts/strides, perhaps sometimes allocating too much memory
===================
*/
int Sys_UploadVBO(int id, model_trimesh_part_t *trimesh, int is_triangle_strip, int will_be_updated)
{
	const model_vertex_t *v = NULL; /* for pointers */
	int wasactive;
	int i;

	if (id < -1 || id >= MAX_VBOS)
		Sys_Error("Sys_UploadVBO: invalid id %d\n", id);

	if (id == -1)
	{
		for (i = 0; i < MAX_VBOS; i++)
		{
			if (!vbos[i].active)
			{
				id = i;
				break;
			}
		}

		if (i == MAX_VBOS)
			Sys_Error("Sys_UploadVBO: too many vbos (more than %d)\n", MAX_VBOS);
	}
	wasactive = vbos[id].active;

	if (!trimesh->vert_count || !trimesh->index_count)
	{
		Sys_DeleteVBO(id); /* TODO: just buffer null data? */
		return -1; /* nothing to do */
	}

	if (!wasactive)
	{
		glGenBuffers(1, &vbos[id].vertex_vbo);
		glGenBuffers(1, &vbos[id].indices_vbo);
		glGenVertexArrays(1, &vbos[id].vao);
	}

	glBindVertexArray(vbos[id].vao); /* TODO: is this needed here? */

	if (!vbos[id].vertex_vbo || !vbos[id].indices_vbo)
		Sys_Error("Sys_UploadVBO: Error generating VBOs for object %d\n", id);

	glBindBuffer(GL_ARRAY_BUFFER, vbos[id].vertex_vbo);
	if (wasactive) /* TODO: is this optimization still relevant? */
		glBufferData(GL_ARRAY_BUFFER, 0, NULL, will_be_updated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	glBufferData(GL_ARRAY_BUFFER, trimesh->vert_stride * trimesh->vert_count, trimesh->verts, will_be_updated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	vbos[id].vertex_count = trimesh->vert_count;
	vbos[id].vertex_stride = trimesh->vert_stride;

	/* vertices */
	glVertexAttribPointer(SHADER_ATTRIB_VERTEX, 3, GL_FLOAT, 0, vbos[id].vertex_stride, &v->origin);
	glEnableVertexAttribArray(SHADER_ATTRIB_VERTEX);

	/* texture0 */
	glVertexAttribPointer(SHADER_ATTRIB_TEXCOORD0, 2, GL_FLOAT, 0, vbos[id].vertex_stride, &v->texcoord0);
	glEnableVertexAttribArray(SHADER_ATTRIB_TEXCOORD0);

	/* texture1 */
	glVertexAttribPointer(SHADER_ATTRIB_TEXCOORD1, 2, GL_FLOAT, 0, vbos[id].vertex_stride, &v->texcoord1);
	glEnableVertexAttribArray(SHADER_ATTRIB_TEXCOORD1);

	/* normals */
	glVertexAttribPointer(SHADER_ATTRIB_NORMAL, 3, GL_FLOAT, 0, vbos[id].vertex_stride, &v->normal);
	glEnableVertexAttribArray(SHADER_ATTRIB_NORMAL);

	/* colors */
	glVertexAttribPointer(SHADER_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, 0, vbos[id].vertex_stride, &v->color);
	glEnableVertexAttribArray(SHADER_ATTRIB_COLOR);

	/* tangent */
	glVertexAttribPointer(SHADER_ATTRIB_TANGENT, 4, GL_FLOAT, 0, vbos[id].vertex_stride, &v->tangent);
	glEnableVertexAttribArray(SHADER_ATTRIB_TANGENT);

	/* weight */
	glVertexAttribPointer(SHADER_ATTRIB_WEIGHTS, 4, GL_FLOAT, 0, vbos[id].vertex_stride, &v->weights);
	glEnableVertexAttribArray(SHADER_ATTRIB_WEIGHTS);

	/* bones */
	glVertexAttribPointer(SHADER_ATTRIB_BONES, 4, GL_UNSIGNED_BYTE, 0, vbos[id].vertex_stride, &v->bones);
	glEnableVertexAttribArray(SHADER_ATTRIB_BONES);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos[id].indices_vbo);
	if (wasactive) /* TODO: is this optimization still relevant? */
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, NULL, will_be_updated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, trimesh->index_stride * trimesh->index_count / 3, trimesh->indexes, will_be_updated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	vbos[id].indices_count = trimesh->index_count;
	vbos[id].indices_stride = trimesh->index_stride;

	vbos[id].active = true;
	vbos[id].is_triangle_strip = is_triangle_strip;
	vbos[id].is_dynamic = will_be_updated;

	bound_vbo = -1;
	glBindVertexArray(0); /* TODO: is this needed here? */
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return id;
}

/*
===================
Sys_BindStreamingVBO

TODO: use glMapBuffer?
TODO: see when it's necessary to bind the array and index buffers (array only when modifying? index only to associate to the vao?)
TODO: currently recreating the buffer for every draw, this is slow!
===================
*/
void Sys_BindStreamingVBO(int use_index_buffer)
{
	int id = STREAMING_VBO;

	glBindVertexArray(vbos[id].vao); /* TODO: is this needed here? */
	glBindBuffer(GL_ARRAY_BUFFER, vbos[id].vertex_vbo);
	glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STREAM_DRAW); /* TODO: is this optimization still relevant? */

	if (use_index_buffer)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos[id].indices_vbo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, NULL, GL_STREAM_DRAW); /* TODO: is this optimization still relevant? */
	}
	else
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	bound_vbo = id;
}

/*
===================
Sys_DeleteVBO
===================
*/
void Sys_DeleteVBO(int id)
{
	if (id < 0 || id > MAX_VBOS) /* > is for for STREAMING_VBO */
		Sys_Error("Sys_DeleteVBO: invalid id %d\n", id);

	if (vbos[id].active) /* delete previous object with this id */
	{
		vbos[id].active = false;
		vbos[id].is_triangle_strip = false;
		vbos[id].is_dynamic = false;

		glDeleteBuffers(1, &vbos[id].vertex_vbo);
		vbos[id].vertex_vbo = 0;
		vbos[id].vertex_stride = 0;
		vbos[id].vertex_count = 0;

		glDeleteBuffers(1, &vbos[id].indices_vbo);
		vbos[id].indices_vbo = 0;
		vbos[id].indices_stride = 0;
		vbos[id].indices_count = 0;

		glDeleteVertexArrays(1, &vbos[id].vao);
		vbos[id].vao = 0;
	}
}

/*
===================
Sys_VBOsInit
===================
*/
void Sys_VBOsInit(void)
{
	int i;

	for (i = 0; i <= MAX_VBOS; i++) /* <= is for for STREAMING_VBO */
	{
		vbos[i].active = false;
		vbos[i].is_triangle_strip = false;
		vbos[i].is_dynamic = false;

		vbos[i].vertex_vbo = 0;
		vbos[i].vertex_stride = 0;
		vbos[i].vertex_count = 0;

		vbos[i].indices_vbo = 0;
		vbos[i].indices_stride = 0;
		vbos[i].indices_count = 0;

		vbos[i].vao = 0;
	}

	vbos[STREAMING_VBO].active = true;
	vbos[STREAMING_VBO].is_dynamic = true;
	glGenBuffers(1, &vbos[STREAMING_VBO].vertex_vbo);
	glGenBuffers(1, &vbos[STREAMING_VBO].indices_vbo);
	glGenVertexArrays(1, &vbos[STREAMING_VBO].vao);
}

/*
===================
Sys_CleanVBOs
===================
*/
void Sys_CleanVBOs(int shutdown)
{
	int i;

	for (i = 0; i < MAX_VBOS; i++)
		Sys_DeleteVBO(i);

	if (shutdown)
		Sys_DeleteVBO(STREAMING_VBO);
}

/*
============================================================================

Frustum culling

TODO: do it faster (see quake?), cache results between passes/frames/etc.
Quake probably isn't the fastest.
TODO: proper integration with the rest of the code (use of host_math, fov
calculations, bsp tracing, etc
TODO: see if false positives happen with VERY BIG objects, probably solved by testing an sphere that encloses the maximum radius of the aabb (which also brings more false positives but is probably better)
TODO: search for aabb/mins/maxs everywhere in the code and do ORIENTE BOXES right!
============================================================================
*/

#if 0 /* radar method */

static enum {
	FRUSTUM_CULL_OUTSIDE = 0,
	FRUSTUM_CULL_INTERSECT = 1,
	FRUSTUM_CULL_INSIDE
};

typedef struct frustum_culling_data_s {
	vec3_t cc; /* camera position */
	vec3_t X,Y,Z; /* the camera referential */
	vec_t nearD, farD, width, height;
	vec_t ratio, tang;

	/* required to test spheres */
	vec_t sphereFactorX, sphereFactorY;
} frustum_culling_data_t;

static frustum_culling_data_t frustum;

/*
===================
Sys_Video3DFrustumCullingSetProjectionData

Should be called every time the projection matrix changes
===================
*/
#define HALF_ANG2RAD (3.14159265358979323846 / 360.0)
void Sys_Video3DFrustumCullingSetProjectionData(vec_t fovy, vec_t ratio, vec_t nearD, vec_t farD)
{
	/* half of the the horizontal field of view */
	vec_t fovx;

	frustum.ratio = ratio;
	frustum.nearD = nearD;
	frustum.farD = farD;

	/* compute width and height of the near section */
	fovy *= (vec_t)HALF_ANG2RAD;
	frustum.tang = tanf(fovy);

	frustum.height = nearD * frustum.tang;
	frustum.width = frustum.height * ratio;

	frustum.sphereFactorY = 1.0f / cosf(fovy);

	fovx = atanf(frustum.tang * ratio);
	frustum.sphereFactorX = 1.0f / cosf(fovx);
}

/*
===================
Sys_Video3DFrustumCullingSetViewData

Should be called every time the view matrix changes
===================
*/
void Sys_Video3DFrustumCullingSetViewData(vec3_t eye, vec3_t center, vec3_t up)
{
	Math_Vector3Copy(eye, frustum.cc);

	/*
		compute the Z axis of the camera referential
		this axis points in the same direction from
		the looking direction
	*/
	Math_Vector3ScaleAdd(eye, -1, center, frustum.Z);
	Math_Vector3Normalize(frustum.Z);

	/* X axis of camera with given "up" vector and Z axis */
	Math_CrossProduct3(frustum.Z, up, frustum.X);
	Math_Vector3Normalize(frustum.X);

	/* the real "up" vector is the cross product of X and Z TODO: why not just copy? */
	Math_CrossProduct3(frustum.X, frustum.Z, frustum.Y);
	Math_Vector3Normalize(frustum.Y) /* TODO: will it be normalized already? */
}

/*
===================
Sys_Video3DFrustumCullingTestPoint

Edge counts as inside
TODO: epsilon
TODO: test
===================
*/
int Sys_Video3DFrustumCullingTestPoint(vec3_t point)
{
	vec3_t v;
	vec3_t minus_z;
	vec_t pcz, pcx, pcy, aux;

	Math_Vector3Scale(frustum.Z, minus_z, -1);

	/* compute vector from camera position to p */
	Math_Vector3ScaleAdd(frustum.cc, -1, point, v);

	/* TODO: is this testing order the most likely to give early culls? */

 	/* compute and test the X coordinate */
 	pcx = Math_DotProduct3(v, frustum.X);
 	aux = aux * frustum.ratio;
 	if (pcx > aux || pcx < -aux)
		return FRUSTUM_CULL_OUTSIDE;

	/* compute and test the Z coordinate */
	pcz = Math_DotProduct3(v, minus_z);
	if (pcz > frustum.farD || pcz < frustum.nearD)
 		return FRUSTUM_CULL_OUTSIDE;

 	/* compute and test the Y coordinate */
	pcy = Math_DotProduct3(v, frustum.Y);
 	aux = pcz * frustum.tang;
 	if (pcy > aux || pcy < -aux)
 		return FRUSTUM_CULL_OUTSIDE;

	return FRUSTUM_CULL_INSIDE;
}

/*
===================
Sys_Video3DFrustumCullingTestSphere

Edge counts as inside
TODO: epsilon
TODO: test
===================
*/
int Sys_Video3DFrustumCullingTestSphere(vec3_t point, vec_t radius)
{
	vec_t d;
	vec_t az,ax,ay;
	int result = FRUSTUM_CULL_INSIDE;
	vec3_t v;
	vec3_t minus_z;

	Math_Vector3Scale(frustum.Z, minus_z, -1);

	Math_Vector3ScaleAdd(frustum.cc, -1, point, v);

	/* TODO: is this testing order the most likely to give early culls? */

	az = Math_DotProduct3(v, minus_z);
	if (az > frustum.farD + radius || az < frustum.nearD - radius)
 		return FRUSTUM_CULL_OUTSIDE;

 	if (az > frustum.farD - radius || az < frustum.nearD + radius)
 		result = FRUSTUM_CULL_INTERSECT;

 	ay = Math_DotProduct3(v, frustum.Y);
 	d = frustum.sphereFactorY * radius;
 	az *= frustum.tang;
 	if (ay > az + d || ay < -az - d)
 		return FRUSTUM_CULL_OUTSIDE;

 	if (ay > az - d || ay < -az + d)
 		result = FRUSTUM_CULL_INTERSECT;

 	ax = Math_DotProduct3(v, frustum.X);
 	az *= frustum.ratio;
 	d = frustum.sphereFactorX * radius;
 	if (ax > az + d || ax < -az - d)
 		return FRUSTUM_CULL_OUTSIDE;

 	if (ax > az - d || ax < -az + d)
		result = FRUSTUM_CULL_INTERSECT;

	return result;
}
#endif /* radar method */

enum {
	FRUSTUM_CULL_OUTSIDE = 0,
	FRUSTUM_CULL_INTERSECT = 1,
	FRUSTUM_CULL_INSIDE
};

enum FRUSTUM_PLANES {
	/* optimize culling heuristically by defining left-right first */
	FRUSTUM_LEFT = 0,
	FRUSTUM_RIGHT = 1,
	FRUSTUM_TOP,
	FRUSTUM_BOTTOM,
	FRUSTUM_NEARPLANE,
	FRUSTUM_FARPLANE,
	FRUSTUM_MAX_PLANES
};

enum FRUSTUM_VERTICES {
	FRUSTUM_VERTICE_FAR_BOTTOM_LEFT = 0,
	FRUSTUM_VERTICE_FAR_TOP_LEFT = 1,
	FRUSTUM_VERTICE_FAR_TOP_RIGHT,
	FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT,
	FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT,
	FRUSTUM_VERTICE_NEAR_TOP_LEFT,
	FRUSTUM_VERTICE_NEAR_TOP_RIGHT,
	FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT,
	FRUSTUM_VERTICE_MAX
};

typedef struct frustum_culling_data_s {
	vec4_t planes[FRUSTUM_MAX_PLANES]; /* [0][1][2] normal [3] dist */
	vec_t nearD, farD, ratio, fovy, tang;
	vec_t nw, nh, fw, fh;

	vec3_t origin, target_center, up;
} frustum_culling_data_t;

static frustum_culling_data_t primary_render_frustum; /* to store the camera frustum of the current frame being rendered globally */
static frustum_culling_data_t *current_frustum = NULL; /* pointer to the current testing frustum */

#define FRUSTUM_CULLING_LIGHT_KDOP_MAX_PLANES		13

typedef struct frustum_culling_light_kdop_s {
	vec4_t planes[FRUSTUM_CULLING_LIGHT_KDOP_MAX_PLANES];
	int num_planes;
} frustum_culling_light_kdop_t;

static frustum_culling_light_kdop_t *current_light_kdop = NULL; /* pointer to the current view->light testing kdop */

/*
===================
Sys_Video3DFrustumCullingSetProjectionData

Should be called every time the projection matrix changes
===================
*/
#define HALF_ANG2RAD (3.14159265358979323846 / 360.0)
void Sys_Video3DFrustumCullingSetProjectionData(frustum_culling_data_t *dest_frustum, vec_t fovy, vec_t ratio, vec_t nearD, vec_t farD)
{
	dest_frustum->ratio = ratio;
	dest_frustum->fovy = fovy;
	dest_frustum->nearD = nearD;
	dest_frustum->farD = farD;

	/* compute width and height of the near and far plane sections */
	fovy *= (vec_t)HALF_ANG2RAD;
	dest_frustum->tang = tanf(fovy);

	dest_frustum->nh = dest_frustum->nearD * dest_frustum->tang;
	dest_frustum->nw = dest_frustum->nh * dest_frustum->ratio;
	dest_frustum->fh = dest_frustum->farD  * dest_frustum->tang;
	dest_frustum->fw = dest_frustum->fh * dest_frustum->ratio;
}

/*
===================
Sys_Video3DFrustumCullingSetViewData

Should be called every time the view matrix changes
===================
*/
void Sys_Video3DFrustumCullingSetViewData(frustum_culling_data_t *dest_frustum, const vec3_t eye, const vec3_t center, const vec3_t up)
{
	vec3_t nc, fc, X, Y, Z, minus_z;
	vec3_t aux, normal;

	/*
		compute the Z axis of the camera
		this axis points in the opposite direction from
		the looking direction
	*/
	Math_Vector3ScaleAdd(center, -1, eye, Z);
	Math_Vector3Normalize(Z);
	Math_Vector3Scale(Z, minus_z, -1);

	/* X axis of camera with given "up" vector and Z axis */
	Math_CrossProduct3(up, Z, X);
	Math_Vector3Normalize(X);

	/* the real "up" vector is the cross product of Z and X TODO: why not just copy? */
	Math_CrossProduct3(Z, X, Y);
	Math_Vector3Normalize(Y) /* TODO: will it be normalized already? */

	/* compute the centers of the near and far planes */
	Math_Vector3ScaleAdd(Z, -dest_frustum->nearD, eye, nc); /* nc = p - Z * dest->nearD; */
	Math_Vector3ScaleAdd(Z, -dest_frustum->farD, eye, fc); /* fc = p - Z * dest->farD; */

	Math_PlaneNormalAndPointToPlaneEquation(minus_z, nc, dest_frustum->planes[FRUSTUM_NEARPLANE]);
	Math_PlaneNormalAndPointToPlaneEquation(Z, fc, dest_frustum->planes[FRUSTUM_FARPLANE]);

	Math_Vector3ScaleAdd(Y, dest_frustum->nh, nc, aux);;
	Math_Vector3ScaleAdd(eye, -1, aux, aux);
	Math_Vector3Normalize(aux);
	Math_CrossProduct3(aux, X, normal);
	Math_Vector3Normalize(normal);
	Math_Vector3ScaleAdd(Y, dest_frustum->nh, nc, aux);
	Math_PlaneNormalAndPointToPlaneEquation(normal, aux, dest_frustum->planes[FRUSTUM_TOP]);

	Math_Vector3ScaleAdd(Y, -dest_frustum->nh, nc, aux);;
	Math_Vector3ScaleAdd(eye, -1, aux, aux);
	Math_Vector3Normalize(aux);
	Math_CrossProduct3(X, aux, normal);
	Math_Vector3Normalize(normal);
	Math_Vector3ScaleAdd(Y, -dest_frustum->nh, nc, aux);
	Math_PlaneNormalAndPointToPlaneEquation(normal, aux, dest_frustum->planes[FRUSTUM_BOTTOM]);

	Math_Vector3ScaleAdd(X, -dest_frustum->nw, nc, aux);;
	Math_Vector3ScaleAdd(eye, -1, aux, aux);
	Math_Vector3Normalize(aux);
	Math_CrossProduct3(aux, Y, normal);
	Math_Vector3Normalize(normal);
	Math_Vector3ScaleAdd(X, -dest_frustum->nw, nc, aux);
	Math_PlaneNormalAndPointToPlaneEquation(normal, aux, dest_frustum->planes[FRUSTUM_LEFT]);

	Math_Vector3ScaleAdd(X, dest_frustum->nw, nc, aux);;
	Math_Vector3ScaleAdd(eye, -1, aux, aux);
	Math_Vector3Normalize(aux);
	Math_CrossProduct3(Y, aux, normal);
	Math_Vector3Normalize(normal);
	Math_Vector3ScaleAdd(X, dest_frustum->nw, nc, aux);
	Math_PlaneNormalAndPointToPlaneEquation(normal, aux, dest_frustum->planes[FRUSTUM_RIGHT]);

	/*
		more legible:

		aux = (nc + Y*nh) - p;
		aux.normalize();
		normal = aux * X;
		pl[TOP].setNormalAndPoint(normal,nc+Y*nh);

		aux = (nc - Y*nh) - p;
		aux.normalize();
		normal = X * aux;
		pl[BOTTOM].setNormalAndPoint(normal,nc-Y*nh);

		aux = (nc - X*nw) - p;
		aux.normalize();
		normal = aux * Y;
		pl[LEFT].setNormalAndPoint(normal,nc-X*nw);

		aux = (nc + X*nw) - p;
		aux.normalize();
		normal = Y * aux;
		pl[RIGHT].setNormalAndPoint(normal,nc+X*nw);
	*/

	Math_Vector3Copy(eye, dest_frustum->origin);
	Math_Vector3Copy(center, dest_frustum->target_center);
	Math_Vector3Copy(up, dest_frustum->up);
}

/*
===================
Sys_Video3DFrustumCullingTestPoint

Edge counts as inside
TODO: epsilon
TODO: test
===================
*/
int Sys_Video3DFrustumCullingTestPoint(const vec3_t point)
{
	int i;

	/* TODO: is this testing order the most likely to give early culls? */
	for(i = 0; i < FRUSTUM_MAX_PLANES; i++)
		if (Math_PlaneDistanceToPoint(current_frustum->planes[i], point) < 0)
			return FRUSTUM_CULL_OUTSIDE;

	if (current_light_kdop)
	{
		/* TODO: is this testing order the most likely to give early culls? */
		for(i = 0; i < current_light_kdop->num_planes; i++)
			if (Math_PlaneDistanceToPoint(current_light_kdop->planes[i], point) < 0)
				return FRUSTUM_CULL_OUTSIDE;
	}

	return FRUSTUM_CULL_INSIDE;
}

/*
===================
Sys_Video3DFrustumCullingTestSphere

Edge counts as inside
TODO: epsilon
TODO: test
TODO: false positives? "The sphere test contains false positives when the sphere intersects 2 planes in a corner and don't intersect the frustum."
===================
*/
int Sys_Video3DFrustumCullingTestSphere(const vec3_t point, const vec_t radius)
{
	int i;
	vec_t distance;
	int result = FRUSTUM_CULL_INSIDE;

	/* TODO: is this testing order the most likely to give early culls? */
	for(i = 0; i < FRUSTUM_MAX_PLANES; i++)
	{
		distance = Math_PlaneDistanceToPoint(current_frustum->planes[i], point);
		if (distance < -radius)
			return FRUSTUM_CULL_OUTSIDE;
		else if (distance < radius)
			result = FRUSTUM_CULL_INTERSECT;
	}

	if (current_light_kdop)
	{
		/* TODO: is this testing order the most likely to give early culls? */
		for(i = 0; i < current_light_kdop->num_planes; i++)
		{
			distance = Math_PlaneDistanceToPoint(current_light_kdop->planes[i], point);
			if (distance < -radius)
				return FRUSTUM_CULL_OUTSIDE;
			else if (distance < radius)
				result = FRUSTUM_CULL_INTERSECT;
		}
	}

	return result;
}

/*
===================
Sys_Video3DFrustumCullingTestAABB

Edge counts as inside
TODO: epsilon
TODO: test
===================
*/
int Sys_Video3DFrustumCullingTestAABB(const vec3_t point, const vec3_t angles, const vec3_t aabbmins, const vec3_t aabbmaxs)
{
	int i;
	int result = FRUSTUM_CULL_INSIDE;
	vec3_t mins, maxs;

	/* TODO FIXME: currently we just expand the AABB for oriented boxes and ignore the angles, this will cull less stuff! */
	if (!Math_Vector3IsZero(angles))
	{
		vec_t abs_max;
		abs_max = (vec_t)fabs(aabbmins[0]);
		abs_max = Math_Max((vec_t)fabs(aabbmins[1]), abs_max);
		abs_max = Math_Max((vec_t)fabs(aabbmins[2]), abs_max);
		abs_max = Math_Max((vec_t)fabs(aabbmaxs[0]), abs_max);
		abs_max = Math_Max((vec_t)fabs(aabbmaxs[1]), abs_max);
		abs_max = Math_Max((vec_t)fabs(aabbmaxs[2]), abs_max);
		Math_Vector3Set(mins, -abs_max, -abs_max, -abs_max);
		Math_Vector3Set(maxs, abs_max, abs_max, abs_max);
	}
	else
	{
		Math_Vector3Copy(aabbmins, mins);
		Math_Vector3Copy(aabbmaxs, maxs);
	}

	for(i = 0; i < FRUSTUM_MAX_PLANES; i++)
	{
		vec3_t p, n;

		Math_Vector3Add(point, mins, p);
		if (current_frustum->planes[i][0] >= 0)
			p[0] = point[0] + maxs[0];
		if (current_frustum->planes[i][1] >= 0)
			p[1] = point[1] + maxs[1];
		if (current_frustum->planes[i][2] >= 0)
			p[2] = point[2] + maxs[2];

		Math_Vector3Add(point, maxs, n);
		if (current_frustum->planes[i][0] >= 0)
			n[0] = point[0] + mins[0];
		if (current_frustum->planes[i][1] >= 0)
			n[1] = point[1] + mins[1];
		if (current_frustum->planes[i][2] >= 0)
			n[2] = point[2] + mins[2];

		/* is the positive vertex outside? */
		if (Math_PlaneDistanceToPoint(current_frustum->planes[i], p) < 0)
			return FRUSTUM_CULL_OUTSIDE;
		/* is the negative vertex outside? */
		else if (Math_PlaneDistanceToPoint(current_frustum->planes[i], n) < 0)
			result = FRUSTUM_CULL_INTERSECT;
	}

	/* TODO FIXME: try do see if the p-vertex/n-vertex method works with our k-DOP? */
	if (current_light_kdop)
	{
		/* TODO: is this testing order the most likely to give early culls? */
		for(i = 0; i < current_light_kdop->num_planes; i++)
		{
			vec3_t maxs, mins, center, pextents, planenormalabsolute;
			vec_t fR, fS;

			/* center of the AABB */
			Math_Vector3Add(point, aabbmins, mins);
			Math_Vector3Add(point, aabbmaxs, maxs);
			Math_Vector3Add(maxs, mins, center);
			Math_Vector3Scale(center, center, 0.5f);
			/* positive extents */
			Math_Vector3ScaleAdd(center, -1, maxs, pextents);

			/* compute the projected interval radius of _aAabb onto L(t) = _aAabb.c + t * _pPlane.n */
			planenormalabsolute[0] = fabsf(current_light_kdop->planes[i][0]);
			planenormalabsolute[1] = fabsf(current_light_kdop->planes[i][1]);
			planenormalabsolute[2] = fabsf(current_light_kdop->planes[i][2]);
			fR = Math_DotProduct3(pextents, planenormalabsolute);

			/* distance from box center to plane */
			fS = Math_PlaneDistanceToPoint(current_light_kdop->planes[i], center);

			/* if less than R, overlap */
			if (fabsf(fS) <= fR)
				result = FRUSTUM_CULL_INTERSECT;
			/* otherwise it is in front or back of the plane */
			else if (fS <= fR)
				return FRUSTUM_CULL_OUTSIDE;
		}
	}

	return result;
}

/*
===================
Sys_Video3DFrustumCorners

Returns the eight vertices defining the input frustum.
Be sure to have space for eight vertices in out_vertices
TODO: test
===================
*/
void Sys_Video3DFrustumCorners(const frustum_culling_data_t *in_frustum, vec_t *out_vertices)
{
	Math_PlaneIntersectionPoint(in_frustum->planes[FRUSTUM_FARPLANE], in_frustum->planes[FRUSTUM_BOTTOM], in_frustum->planes[FRUSTUM_LEFT], &out_vertices[3 * FRUSTUM_VERTICE_FAR_BOTTOM_LEFT]);
	Math_PlaneIntersectionPoint(in_frustum->planes[FRUSTUM_FARPLANE], in_frustum->planes[FRUSTUM_TOP], in_frustum->planes[FRUSTUM_LEFT], &out_vertices[3 * FRUSTUM_VERTICE_FAR_TOP_LEFT]);
	Math_PlaneIntersectionPoint(in_frustum->planes[FRUSTUM_FARPLANE], in_frustum->planes[FRUSTUM_TOP], in_frustum->planes[FRUSTUM_RIGHT], &out_vertices[3 * FRUSTUM_VERTICE_FAR_TOP_RIGHT]);
	Math_PlaneIntersectionPoint(in_frustum->planes[FRUSTUM_FARPLANE], in_frustum->planes[FRUSTUM_BOTTOM], in_frustum->planes[FRUSTUM_RIGHT], &out_vertices[3 * FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT]);

	/* again with the near plane */
	Math_PlaneIntersectionPoint(in_frustum->planes[FRUSTUM_NEARPLANE], in_frustum->planes[FRUSTUM_BOTTOM], in_frustum->planes[FRUSTUM_LEFT], &out_vertices[3 * FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT]);
	Math_PlaneIntersectionPoint(in_frustum->planes[FRUSTUM_NEARPLANE], in_frustum->planes[FRUSTUM_TOP], in_frustum->planes[FRUSTUM_LEFT], &out_vertices[3 * FRUSTUM_VERTICE_NEAR_TOP_LEFT]);
	Math_PlaneIntersectionPoint(in_frustum->planes[FRUSTUM_NEARPLANE], in_frustum->planes[FRUSTUM_TOP], in_frustum->planes[FRUSTUM_RIGHT], &out_vertices[3 * FRUSTUM_VERTICE_NEAR_TOP_RIGHT]);
	Math_PlaneIntersectionPoint(in_frustum->planes[FRUSTUM_NEARPLANE], in_frustum->planes[FRUSTUM_BOTTOM], in_frustum->planes[FRUSTUM_RIGHT], &out_vertices[3 * FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT]);
}

/*
===================
Sys_Video3DFrustumGetNeighbors

Given a plane index (FRUSTUM_*), the 4 neighbors of that plane are returned.
Be sure to have space for 4 plane indices in out_planes.
TODO: test
===================
*/
void Sys_Video3DFrustumGetNeighbors(const int plane_index, int *out_planes)
{
	int i;
	static const int fpTable[FRUSTUM_MAX_PLANES][4] =
	{
		{	/* FRUSTUM_LEFT */
			FRUSTUM_TOP,
			FRUSTUM_BOTTOM,
			FRUSTUM_NEARPLANE,
			FRUSTUM_FARPLANE
		},
		{	/* FRUSTUM_RIGHT */
			FRUSTUM_TOP,
			FRUSTUM_BOTTOM,
			FRUSTUM_NEARPLANE,
			FRUSTUM_FARPLANE
		},
		{	/* FRUSTUM_TOP */
			FRUSTUM_LEFT,
			FRUSTUM_RIGHT,
			FRUSTUM_NEARPLANE,
			FRUSTUM_FARPLANE
		},
		{	/* FRUSTUM_BOTTOM */
			FRUSTUM_LEFT,
			FRUSTUM_RIGHT,
			FRUSTUM_NEARPLANE,
			FRUSTUM_FARPLANE
		},
		{	/* FRUSTUM_NEARPLANE */
			FRUSTUM_LEFT,
			FRUSTUM_RIGHT,
			FRUSTUM_TOP,
			FRUSTUM_BOTTOM
		},
		{	/* FRUSTUM_FARPLANE */
			FRUSTUM_LEFT,
			FRUSTUM_RIGHT,
			FRUSTUM_TOP,
			FRUSTUM_BOTTOM
		},
	};

	for (i = 4; i--;)
		out_planes[i] = fpTable[plane_index][i];
}

/*
===================
Sys_Video3DFrustumGetCornersOfPlanes

Given two plane indices (FRUSTUM_*), returns the two points shared by those planes.
The points are always returned in counter-clockwise order, assuming the first input
plane is facing towards the viewer.
Be sure to have space for 2 vertex indices in out_vertex_indices.
TODO: test
TODO: winding order - right-handedness?
===================
*/
void Sys_Video3DFrustumGetCornersOfPlanes(const int plane_index0, const int plane_index1, int *out_vertex_indices)
{
	static const int fpTable[FRUSTUM_MAX_PLANES][FRUSTUM_MAX_PLANES][2] =
	{
		{	/* FRUSTUM_LEFT */
			{	/* FRUSTUM_LEFT */
				FRUSTUM_VERTICE_FAR_BOTTOM_LEFT, FRUSTUM_VERTICE_FAR_BOTTOM_LEFT,		/* invalid combination */
			},
			{	/* FRUSTUM_RIGHT */
				FRUSTUM_VERTICE_FAR_BOTTOM_LEFT, FRUSTUM_VERTICE_FAR_BOTTOM_LEFT,		/* invalid combination */
			},
			{	/* FRUSTUM_TOP */
				FRUSTUM_VERTICE_NEAR_TOP_LEFT, FRUSTUM_VERTICE_FAR_TOP_LEFT,
			},
			{	/* FRUSTUM_BOTTOM */
				FRUSTUM_VERTICE_FAR_BOTTOM_LEFT, FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT,
			},
			{	/* FRUSTUM_NEARPLANE */
				FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT, FRUSTUM_VERTICE_NEAR_TOP_LEFT,
			},
			{	/* FRUSTUM_FARPLANE */
				FRUSTUM_VERTICE_FAR_TOP_LEFT, FRUSTUM_VERTICE_FAR_BOTTOM_LEFT,
			},
		},
		{	/* FRUSTUM_RIGHT */
			{	/* FRUSTUM_LEFT */
				FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT, FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT,	/* invalid combination */
			},
			{	/* FRUSTUM_RIGHT */
				FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT, FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT,	/* invalid combination */
			},
			{	/* FRUSTUM_TOP */
				FRUSTUM_VERTICE_FAR_TOP_RIGHT, FRUSTUM_VERTICE_NEAR_TOP_RIGHT,
			},
			{	/* FRUSTUM_BOTTOM */
				FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT, FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT,
			},
			{	/* FRUSTUM_NEARPLANE */
				FRUSTUM_VERTICE_NEAR_TOP_RIGHT, FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT,
			},
			{	/* FRUSTUM_FARPLANE */
				FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT, FRUSTUM_VERTICE_FAR_TOP_RIGHT,
			},
		},
		{	/* FRUSTUM_TOP */
			{	/* FRUSTUM_LEFT */
				FRUSTUM_VERTICE_FAR_TOP_LEFT, FRUSTUM_VERTICE_NEAR_TOP_LEFT,
			},
			{	/* FRUSTUM_RIGHT */
				FRUSTUM_VERTICE_NEAR_TOP_RIGHT, FRUSTUM_VERTICE_FAR_TOP_RIGHT,
			},
			{	/* FRUSTUM_TOP */
				FRUSTUM_VERTICE_NEAR_TOP_LEFT, FRUSTUM_VERTICE_FAR_TOP_LEFT,		/* invalid combination */
			},
			{	/* FRUSTUM_BOTTOM */
				FRUSTUM_VERTICE_FAR_BOTTOM_LEFT, FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT,	/* invalid combination */
			},
			{	/* FRUSTUM_NEARPLANE */
				FRUSTUM_VERTICE_NEAR_TOP_LEFT, FRUSTUM_VERTICE_NEAR_TOP_RIGHT,
			},
			{	/* FRUSTUM_FARPLANE */
				FRUSTUM_VERTICE_FAR_TOP_RIGHT, FRUSTUM_VERTICE_FAR_TOP_LEFT,
			},
		},
		{	/* FRUSTUM_BOTTOM */
			{	/* FRUSTUM_LEFT */
				FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT, FRUSTUM_VERTICE_FAR_BOTTOM_LEFT,
			},
			{	/* FRUSTUM_RIGHT */
				FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT, FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT,
			},
			{	/* FRUSTUM_TOP */
				FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT, FRUSTUM_VERTICE_FAR_BOTTOM_LEFT,	/* invalid combination */
			},
			{	/* FRUSTUM_BOTTOM */
				FRUSTUM_VERTICE_FAR_BOTTOM_LEFT, FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT,	/* invalid combination */
			},
			{	/* FRUSTUM_NEARPLANE */
				FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT, FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT,
			},
			{	/* FRUSTUM_FARPLANE */
				FRUSTUM_VERTICE_FAR_BOTTOM_LEFT, FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT,
			},
		},
		{	/* FRUSTUM_NEARPLANE */
			{	/* FRUSTUM_LEFT */
				FRUSTUM_VERTICE_NEAR_TOP_LEFT, FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT,
			},
			{	/* FRUSTUM_RIGHT */
				FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT, FRUSTUM_VERTICE_NEAR_TOP_RIGHT,
			},
			{	/* FRUSTUM_TOP */
				FRUSTUM_VERTICE_NEAR_TOP_RIGHT, FRUSTUM_VERTICE_NEAR_TOP_LEFT,
			},
			{	/* FRUSTUM_BOTTOM */
				FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT, FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT,
			},
			{	/* FRUSTUM_NEARPLANE */
				FRUSTUM_VERTICE_NEAR_TOP_LEFT, FRUSTUM_VERTICE_NEAR_TOP_RIGHT,		/* invalid combination */
			},
			{	/* FRUSTUM_FARPLANE */
				FRUSTUM_VERTICE_FAR_TOP_RIGHT, FRUSTUM_VERTICE_FAR_TOP_LEFT,		/* invalid combination */
			},
		},
		{	/* FRUSTUM_FARPLANE */
			{	/* FRUSTUM_LEFT */
				FRUSTUM_VERTICE_FAR_BOTTOM_LEFT, FRUSTUM_VERTICE_FAR_TOP_LEFT,
			},
			{	/* FRUSTUM_RIGHT */
				FRUSTUM_VERTICE_FAR_TOP_RIGHT, FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT,
			},
			{	/* FRUSTUM_TOP */
				FRUSTUM_VERTICE_FAR_TOP_LEFT, FRUSTUM_VERTICE_FAR_TOP_RIGHT,
			},
			{	/* FRUSTUM_BOTTOM */
				FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT, FRUSTUM_VERTICE_FAR_BOTTOM_LEFT,
			},
			{	/* FRUSTUM_NEARPLANE */
				FRUSTUM_VERTICE_FAR_TOP_LEFT, FRUSTUM_VERTICE_FAR_TOP_RIGHT,		/* invalid combination */
			},
			{	/* FRUSTUM_FARPLANE */
				FRUSTUM_VERTICE_FAR_TOP_RIGHT, FRUSTUM_VERTICE_FAR_TOP_LEFT,		/* invalid combination */
			},
		},
	};
	out_vertex_indices[0] = fpTable[plane_index0][plane_index1][0];
	out_vertex_indices[1] = fpTable[plane_index0][plane_index1][1];
}

/*
===================
Sys_Video3DFrustumLightKDopAddPlane

Add an plane to a k-DOP
TODO: test
===================
*/
void Sys_Video3DFrustumLightKDopAddPlane(frustum_culling_light_kdop_t *kdop, const vec4_t plane)
{
	if (kdop->num_planes == FRUSTUM_CULLING_LIGHT_KDOP_MAX_PLANES)
		Sys_Error("Sys_Video3DFrustumLightKDopAddPlane: too many planes\n");

	Math_Vector4Copy(plane, kdop->planes[kdop->num_planes]);
	kdop->num_planes++;
}

/*
===================
Sys_Video3DFrustumMakeLightFrustumAndSecondaryKDop

Creates the light frustum for occlusion tests.
Also creates a k-DOP for a given light based on the corner points of a camera view frustum
that encompasses all the space that can have objects casting shadows on the view
frustum.
TODO: test
TODO: winding order - right-handedness?
TODO: separate this function into two
===================
*/
void Sys_Video3DFrustumMakeLightFrustumAndSecondaryKDop(const frustum_culling_data_t *camera_frustum, frustum_culling_light_kdop_t *light_kdop_dest, const vec3_t light_origin, const frustum_culling_data_t *light_frustum, model_vertex_t *destverts, unsigned int *destindices)
{
	int i, j;
	vec_t dotproducts[FRUSTUM_MAX_PLANES];
	vec3_t corners[8];

	/* create frustum for rendering */

	Sys_Video3DFrustumCorners(light_frustum, corners[0]);

	for (i = 0; i < FRUSTUM_VERTICE_MAX; i++)
	{
		Math_Vector3Copy(corners[i], destverts[i].origin);
		/* debug stuff */
		destverts[i].color[0] = 32 * i;
		destverts[i].color[1] = 32 * i;
		destverts[i].color[2] = 32 * i;
		destverts[i].color[3] = 127;
		/* TODO: need to do normals? */
		destverts[i].normal[0] = -1;
		destverts[i].normal[1] = 0;
		destverts[i].normal[2] = 0;
	}

	/* left */
	destindices[0] = FRUSTUM_VERTICE_FAR_TOP_LEFT; destindices[1] = FRUSTUM_VERTICE_FAR_BOTTOM_LEFT; destindices[2] = FRUSTUM_VERTICE_NEAR_TOP_LEFT;
	destindices[3] = FRUSTUM_VERTICE_NEAR_TOP_LEFT; destindices[4] = FRUSTUM_VERTICE_FAR_BOTTOM_LEFT; destindices[5] = FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT;

	/* bottom */
	destindices[6] = FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT; destindices[7] = FRUSTUM_VERTICE_FAR_BOTTOM_LEFT; destindices[8] = FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT;
	destindices[9] = FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT; destindices[10] = FRUSTUM_VERTICE_FAR_BOTTOM_LEFT; destindices[11] = FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT;

	/* far */
	destindices[12] = FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT; destindices[13] = FRUSTUM_VERTICE_FAR_BOTTOM_LEFT; destindices[14] = FRUSTUM_VERTICE_FAR_TOP_LEFT;
	destindices[15] = FRUSTUM_VERTICE_FAR_TOP_LEFT; destindices[16] = FRUSTUM_VERTICE_FAR_TOP_RIGHT; destindices[17] = FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT;

	/* right */
	destindices[18] = FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT; destindices[19] = FRUSTUM_VERTICE_FAR_TOP_RIGHT; destindices[20] = FRUSTUM_VERTICE_NEAR_TOP_RIGHT;
	destindices[21] = FRUSTUM_VERTICE_NEAR_TOP_RIGHT; destindices[22] = FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT; destindices[23] = FRUSTUM_VERTICE_FAR_BOTTOM_RIGHT;

	/* top */
	destindices[24] = FRUSTUM_VERTICE_FAR_TOP_RIGHT; destindices[25] = FRUSTUM_VERTICE_FAR_TOP_LEFT; destindices[26] = FRUSTUM_VERTICE_NEAR_TOP_LEFT;
	destindices[27] = FRUSTUM_VERTICE_NEAR_TOP_LEFT; destindices[28] = FRUSTUM_VERTICE_NEAR_TOP_RIGHT; destindices[29] = FRUSTUM_VERTICE_FAR_TOP_RIGHT;

	/* near */
	destindices[30] = FRUSTUM_VERTICE_NEAR_TOP_RIGHT; destindices[31] = FRUSTUM_VERTICE_NEAR_TOP_LEFT; destindices[32] = FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT;
	destindices[33] = FRUSTUM_VERTICE_NEAR_BOTTOM_LEFT; destindices[34] = FRUSTUM_VERTICE_NEAR_BOTTOM_RIGHT; destindices[35] = FRUSTUM_VERTICE_NEAR_TOP_RIGHT;

	/* create secondary k-DOP */

	light_kdop_dest->num_planes = 0;

	Sys_Video3DFrustumCorners(camera_frustum, corners[0]);

	/* add planes that are facing towards us */
	for (i = FRUSTUM_MAX_PLANES; i--;)
	{
		vec_t fDir = Math_PlaneDistanceToPoint(camera_frustum->planes[i], light_origin);
		if (fDir >= 0)
			Sys_Video3DFrustumLightKDopAddPlane(light_kdop_dest, camera_frustum->planes[i]);
		dotproducts[i] = fDir;
	}
	/* we have added the back sides of the planes, now find the edges */
	/* for each plane */
	for (i = FRUSTUM_MAX_PLANES; i--;)
	{
		int fpNeighbors[4];

		/* if this plane is facing away from us, move on */
		if (dotproducts[i] < 0)
			continue;
		/* For each neighbor of this plane */
		Sys_Video3DFrustumGetNeighbors(i, fpNeighbors);
		for (j = 4; j--;)
		{
			vec_t fNeighborDir = Math_PlaneDistanceToPoint(camera_frustum->planes[fpNeighbors[j]], light_origin);
			/* If this plane is facing away from us, the edge between plane i and plane j marks the edge of a plane we need to add */
			if (fNeighborDir < 0)
			{
				vec3_t newplanevertices[3];
				vec4_t newplane;
				int fpPoints[2];
				Sys_Video3DFrustumGetCornersOfPlanes(i, fpNeighbors[j], fpPoints);
				Math_Vector3Copy(corners[fpPoints[0]], newplanevertices[0]);
				Math_Vector3Copy(corners[fpPoints[1]], newplanevertices[1]);
				Math_Vector3Copy(light_origin, newplanevertices[2]); /* TODO FIXME: THIS DOES NOT WORK RIGHT!!! set to the closest of the four near vertices? have a near plane! etc this will cull thin objects like doors that are too close to the light source */
				Math_PlaneFromThreePoints(newplanevertices[0], newplanevertices[1], newplanevertices[2], newplane);
				Sys_Video3DFrustumLightKDopAddPlane(light_kdop_dest, newplane);
			}
		}
	}
}

/*
============================================================================

Caching

TODO: finnish it (entity matrices, etc...)
TODO: have each mirror/water/reflection/etc have it's own cache for everything related to the CAMERA?
TODO: CLEAR CACHES WHEN A NEW SERVER IS STARTED!!!

============================================================================
*/

typedef struct light_cache_s {
	frustum_culling_data_t frustum;
	model_vertex_t frustumverts[FRUSTUM_VERTICE_MAX];
	unsigned int frustumindices[36]; /* TODO: use triangle strips/fans */

	frustum_culling_light_kdop_t lightkdop;

	vec_t camera_view_nearD, camera_view_farD, camera_view_ratio, camera_view_fovy;
	vec3_t camera_view_origin, camera_view_target_center, camera_view_up;
} light_cache_t;

static light_cache_t light_cache[MAX_EDICTS][6]; /* if point light, we have a cubemap */

/*
===================
Sys_VideoCacheCreateLight

Creates or updates a light cache, if necessary
TODO: test
TODO: according to kdop development, see what else will trigger updates in it
===================
*/
void Sys_VideoCacheCreateLight(const entindex_t ent, const int cubemap_side, const vec3_t origin, const vec3_t target_center, const vec3_t up, const vec_t fovy, const vec_t ratio, const vec_t near, const vec_t far, const frustum_culling_data_t *camera_for_kdop)
{
	int update_projection = false;
	int update_view = false;
	int update_kdop = false;

	if (fovy != light_cache[ent][cubemap_side].frustum.fovy)
	{
		update_projection = true;
		update_view = true;
		update_kdop = true;
	}
	else if (ratio != light_cache[ent][cubemap_side].frustum.ratio)
	{
		update_projection = true;
		update_view = true;
		update_kdop = true;
	}
	else if (near != light_cache[ent][cubemap_side].frustum.nearD)
	{
		update_projection = true;
		update_view = true;
		update_kdop = true;
	}
	else if (far != light_cache[ent][cubemap_side].frustum.farD)
	{
		update_projection = true;
		update_view = true;
		update_kdop = true;
	}
	else if (!Math_Vector3Compare(origin, light_cache[ent][cubemap_side].frustum.origin))
	{
		update_view = true;
		update_kdop = true;
	}
	else if (!Math_Vector3Compare(target_center, light_cache[ent][cubemap_side].frustum.target_center))
	{
		update_view = true;
		update_kdop = true;
	}
	else if (!Math_Vector3Compare(up, light_cache[ent][cubemap_side].frustum.up))
	{
		update_view = true;
		update_kdop = true;
	}

	if (camera_for_kdop->fovy != light_cache[ent][cubemap_side].camera_view_fovy)
	{
		update_kdop = true;
		light_cache[ent][cubemap_side].camera_view_fovy = camera_for_kdop->fovy;
	}
	if (camera_for_kdop->ratio != light_cache[ent][cubemap_side].camera_view_ratio)
	{
		update_kdop = true;
		light_cache[ent][cubemap_side].camera_view_ratio = camera_for_kdop->ratio;
	}
	if (camera_for_kdop->nearD != light_cache[ent][cubemap_side].camera_view_nearD)
	{
		update_kdop = true;
		light_cache[ent][cubemap_side].camera_view_nearD = camera_for_kdop->nearD;

	}
	if (camera_for_kdop->farD != light_cache[ent][cubemap_side].camera_view_farD)
	{
		update_kdop = true;
		light_cache[ent][cubemap_side].camera_view_farD = camera_for_kdop->farD;
	}

	if (!Math_Vector3Compare(camera_for_kdop->origin, light_cache[ent][cubemap_side].camera_view_origin))
	{
		update_kdop = true;
		Math_Vector3Copy(camera_for_kdop->origin, light_cache[ent][cubemap_side].camera_view_origin);
	}
	if (!Math_Vector3Compare(camera_for_kdop->target_center, light_cache[ent][cubemap_side].camera_view_target_center))
	{
		update_kdop = true;
		Math_Vector3Copy(camera_for_kdop->target_center, light_cache[ent][cubemap_side].camera_view_target_center);
	}
	if (!Math_Vector3Compare(camera_for_kdop->up, light_cache[ent][cubemap_side].camera_view_up))
	{
		update_kdop = true;
		Math_Vector3Copy(camera_for_kdop->up, light_cache[ent][cubemap_side].camera_view_up);
	}

	if (update_projection)
		Sys_Video3DFrustumCullingSetProjectionData(&light_cache[ent][cubemap_side].frustum, fovy, ratio, near, far);
	if (update_view)
		Sys_Video3DFrustumCullingSetViewData(&light_cache[ent][cubemap_side].frustum, origin, target_center, up);
	if (update_kdop)
		Sys_Video3DFrustumMakeLightFrustumAndSecondaryKDop(camera_for_kdop, &light_cache[ent][cubemap_side].lightkdop, origin, &light_cache[ent][cubemap_side].frustum, light_cache[ent][cubemap_side].frustumverts, light_cache[ent][cubemap_side].frustumindices);
}

/*
===================
Sys_VideoCacheRenderLightFrustum

Renders the light frustum, for occlusion queries
TODO: test
TODO: according to kdop development, see what else will trigger updates in it
===================
*/
void Sys_VideoCacheRenderLightFrustum(const entindex_t ent, const int cubemap_side, const vec3_t origin, const vec3_t angles)
{
	Sys_VideoTransformFor3DModel(NULL);

	Sys_VideoDraw3DTriangles(light_cache[ent][cubemap_side].frustumverts, FRUSTUM_VERTICE_MAX, true, true, false, false, true, light_cache[ent][cubemap_side].frustumindices, 36, -1, -1, -1, -1, -1, 0);
}

enum ENTITY_OCCLUSION_STATE {
	ENTITY_OCCLUSION_STATE_HIDDEN = 0,
	ENTITY_OCCLUSION_STATE_VISIBLE = 1,
	ENTITY_OCCLUSION_STATE_WAITING,
	ENTITY_OCCLUSION_STATE_WAITING_JUSTSTARTED
};

typedef struct scene_occlusion_s {
	GLuint query_id[MAX_EDICTS];
	int occluded[MAX_EDICTS];
	int occlusion_state[MAX_EDICTS];

	GLuint voxel_query_id[VOXEL_MAX_CHUNKS];
	int voxel_occluded[VOXEL_MAX_CHUNKS];
	int voxel_occlusion_state[VOXEL_MAX_CHUNKS];
} scene_occlusion_t;

scene_occlusion_t render_occlusion_data;

typedef struct light_scene_occlusion_s {
	GLuint query_light_frustum_id[6]; /* for the light, six in case this is a point light using a cubemap */
	int light_frustum_occluded[6];
	int light_frustum_occlusion_state[6];
} light_scene_occlusion_t;

light_scene_occlusion_t light_occlusion_data[MAX_EDICTS];

static int entity_occlusion_cache_inited = false;

scene_occlusion_t *current_occlusion_data = NULL;

static int draw_occluded = true; /* this both forces drawing of an occluded entity AND enables occlusion query for the entity */

/*
===================
Sys_Video3DCacheOcclusionTestStart

Returns true if entity occluded, results may not get ready instantly and the state may only change on subsequent calls.
Call it AFTER setting the modelmatrix (so no need to pass angles and origin) and before rendering a model

TODO: draw front-to-back for better culling! z-sort entities when doing a pass with this testing (not texture or model sort)!
TODO:
rendering the models will cause lots of redundant processing to be done (skeletal animations, etc)
rendering the AABBs won't work with depth writes (they are too big and will occlude other visible stuff)
solution: render static stuff (world, doors, etc) as models with depth write and render dynamic stuff (players, enemies, items, etc) as AABBs without depth write?
TODO: do not even start (check if anything_drawn can be decided fast beforehand?)
===================
*/
int Sys_Video3DCacheOcclusionTestStart(int obj_index, int obj_is_voxel)
{
	GLuint *query_id;
	int *occluded, *occlusion_state;

	if (!current_occlusion_data)
		return false; /* always visible in this case */

	if (obj_is_voxel)
	{
		query_id = &current_occlusion_data->voxel_query_id[obj_index];
		occluded = &current_occlusion_data->voxel_occluded[obj_index];
		occlusion_state = &current_occlusion_data->voxel_occlusion_state[obj_index];
	}
	else
	{
		query_id = &current_occlusion_data->query_id[obj_index];
		occluded = &current_occlusion_data->occluded[obj_index];
		occlusion_state = &current_occlusion_data->occlusion_state[obj_index];
	}

	if (*occlusion_state == ENTITY_OCCLUSION_STATE_WAITING)
	{
		GLuint passed = INT_MAX;
		GLuint available = 0;

		glGetQueryObjectuiv(*query_id, GL_QUERY_RESULT_AVAILABLE, &available);

		if(available)
		{
			passed = 0;
			glGetQueryObjectuiv(*query_id, GL_QUERY_RESULT, &passed);
			*occlusion_state = (passed) ? ENTITY_OCCLUSION_STATE_VISIBLE : ENTITY_OCCLUSION_STATE_HIDDEN;
			*occluded = (passed) ? false : true;
		}
	}

	if (draw_occluded)
		if (*occlusion_state != ENTITY_OCCLUSION_STATE_WAITING)
		{
			*occlusion_state = ENTITY_OCCLUSION_STATE_WAITING_JUSTSTARTED;
			glBeginQuery(GL_ANY_SAMPLES_PASSED, *query_id);
		}

	return *occluded;
}

/*
===================
Sys_Video3DCacheOcclusionTestEnd

Call it AFTER rendering a model
===================
*/
void Sys_Video3DCacheOcclusionTestEnd(int obj_index, int obj_is_voxel, int anything_drawn)
{
	int *occluded, *occlusion_state;

	if (!current_occlusion_data)
		return;

	if (obj_is_voxel)
	{
		occluded = &current_occlusion_data->voxel_occluded[obj_index];
		occlusion_state = &current_occlusion_data->voxel_occlusion_state[obj_index];
	}
	else
	{
		occluded = &current_occlusion_data->occluded[obj_index];
		occlusion_state = &current_occlusion_data->occlusion_state[obj_index];
	}

	if (draw_occluded)
	{
		if (*occlusion_state == ENTITY_OCCLUSION_STATE_WAITING_JUSTSTARTED)
		{
			if (anything_drawn)
			{
				*occlusion_state = ENTITY_OCCLUSION_STATE_WAITING;
			}
			else
			{
				/* to ignore this query (since nothing was drawn, there is no problem if drawing isn't finished when beginning the next query */
				*occlusion_state =  *occluded ? ENTITY_OCCLUSION_STATE_HIDDEN : ENTITY_OCCLUSION_STATE_VISIBLE;
			}
			glEndQuery(GL_ANY_SAMPLES_PASSED);
		}
	}
}

/*
===================
Sys_Video3DCacheOcclusionResetData
===================
*/
void Sys_Video3DCacheOcclusionResetData(scene_occlusion_t *data, int create)
{
	entindex_t ent;
	int chunk;

	if (create)
		glGenQueries(MAX_EDICTS, &data->query_id[0]);
	else
		glDeleteQueries(MAX_EDICTS, &data->query_id[0]);
	for (ent = 0; ent < MAX_EDICTS; ent++)
	{
		data->occluded[ent] = false;
		data->occlusion_state[ent] = ENTITY_OCCLUSION_STATE_VISIBLE;
	}

	if (create)
		glGenQueries(VOXEL_MAX_CHUNKS, &data->voxel_query_id[0]);
	else
		glDeleteQueries(VOXEL_MAX_CHUNKS, &data->voxel_query_id[0]);
	for (chunk = 0; chunk < VOXEL_MAX_CHUNKS; chunk++)
	{
		data->voxel_occluded[chunk] = false;
		data->voxel_occlusion_state[chunk] = ENTITY_OCCLUSION_STATE_VISIBLE;
	}
}

/*
===================
Sys_Video3DCacheOcclusionInit

TODO: HUNDREDS of MB of OCCLUSION DATA!
===================
*/
void Sys_Video3DCacheOcclusionInit(void)
{
	entindex_t ent;
	int i;

	if (entity_occlusion_cache_inited)
		return;

	/* default to NOT occlued, to avoid lights shining through opaque stuff */
	/* TODO: reset everything to NOT occlued when spawning a new camera or a new light (one that wasn't present last frame won't work because stuff may be deleted and another light occupy the same slot in the same frame - use some sort of entity id/uuid?) */
	Sys_Video3DCacheOcclusionResetData(&render_occlusion_data, true);
	for (ent = 0; ent < MAX_EDICTS; ent++)
	{
		glGenQueries(6, &light_occlusion_data[ent].query_light_frustum_id[0]);
		for (i = 0; i < 6; i++)
		{
			light_occlusion_data[ent].light_frustum_occluded[i] = false;
			light_occlusion_data[ent].light_frustum_occlusion_state[i] = ENTITY_OCCLUSION_STATE_VISIBLE;
		}
	}

	entity_occlusion_cache_inited = true;
}

/*
===================
Sys_Video3DCacheOcclusionShutdown
===================
*/
void Sys_Video3DCacheOcclusionShutdown(void)
{
	entindex_t ent;
	int i;

	if (!entity_occlusion_cache_inited)
		return;

	Sys_Video3DCacheOcclusionResetData(&render_occlusion_data, false);
	for (ent = 0; ent < MAX_EDICTS; ent++)
	{
		glDeleteQueries(6, &light_occlusion_data[ent].query_light_frustum_id[0]);
		for (i = 0; i < 6; i++)
		{
			light_occlusion_data[ent].light_frustum_occluded[i] = false;
			light_occlusion_data[ent].light_frustum_occlusion_state[i] = ENTITY_OCCLUSION_STATE_VISIBLE;
		}
	}

	entity_occlusion_cache_inited = false;
}

/*
============================================================================

Video refresh related system functions

In the interest of speed, drawing functions make no bounds checking.
TODO: extend this lack of checking to where it's redundant

============================================================================
*/

static SDL_Window *window = NULL;
static int video_width = 0, video_height = 0;
static int vsync_enabled = 0;

EXTERNC cvar_t *vid_windowedwidth = NULL;
EXTERNC cvar_t *vid_windowedheight = NULL;
EXTERNC cvar_t *vid_fullscreen = NULL;
cvar_t *vid_vsync = NULL;
cvar_t *r_fov = NULL; /* horizontal fov in a 4:3 display, will be properly scaled for widescreen displays */
cvar_t *r_znear = NULL; /* nearclip plane */
cvar_t *r_zfar = NULL; /* farclip plane */

EXTERNC cvar_t *r_lodbias = NULL;
EXTERNC cvar_t *r_lodhysteresis = NULL;

EXTERNC cvar_t *r_lockpvs = NULL;

EXTERNC int video_3d_alpha_pass = false; /* when true, the 3d renderer is rendering stuff with textures that have alpha values (only texture0 is checked! TODO check more? know which ones will contribute to the visual part? there may be more diffuse or the diffuse may not be texture0) */

static GLenum depthfunc_cache;
static GLboolean depthmask_cache;

/*
===================
Sys_VideoSetDepthTest

OpenGL doesn't write to the depth buffer if GL_DEPTH_TEST is disabled, so we
use GL_ALWAYS as a means of still writing to it when rendering ordered polygons.
===================
*/
void Sys_VideoSetDepthTest(int state, int force)
{
	if ((state && depthfunc_cache == GL_ALWAYS) || force)
	{
		glDepthFunc(GL_LEQUAL); /* GL_LEQUAL for doing multiple passes, GL_LESS would reject the new passes */
		depthfunc_cache = GL_LEQUAL;
	}
	else if ((!state && depthfunc_cache == GL_LEQUAL) || force)
	{
		glDepthFunc(GL_ALWAYS);
		depthfunc_cache = GL_ALWAYS;
	}
}

/*
===================
Sys_VideoSetDepthMask

Sets writing to the depth buffer
===================
*/
void Sys_VideoSetDepthMask(int state, int force)
{
	if ((state && depthmask_cache == GL_FALSE) || force)
	{
		glDepthMask(GL_TRUE);
		depthmask_cache = GL_TRUE;
	}
	else if ((!state && depthmask_cache == GL_TRUE) || force)
	{
		glDepthMask(GL_FALSE);
		depthmask_cache = GL_FALSE;
	}
}

/*
===================
Sys_VideoCalvFov

For calculating X fov, pass parameters as normal
For calculating Y fov, swap width and height
===================
*/
double Sys_VideoCalcFov (double fov, double width, double height)
{
	double a, y;

	/* some sane values */
	Math_Bound(10.0, fov, 170.0);

	y = height / tan (fov / 360 * M_PI);
	a = atan (width / y);
	a = a * 360 / M_PI;

	return a;
}

/*
===================
Sys_VideoSet3D

Vertical fov is always the same, horizontal fov stretchs or shrinks to adapt to different aspect ratios
without stretching the screen.
===================
*/
void Sys_VideoSet3D(const vec3_t origin, const vec3_t angles, const vec_t zfar, const int set_camera_frustum)
{
	/* adjust horizontal fov while having the vertical fov fixed*/
	double real_fovx, real_fovy;
	double xmin, xmax, ymin, ymax, znear = r_znear->doublevalue, aspect; /* TODO: configure znear, merge with SHADOW_ZNEAR? */

	real_fovx = r_fov->doublevalue;
	real_fovy = Sys_VideoCalcFov(real_fovx, BASE_HEIGHT, BASE_WIDTH);
	real_fovx = Sys_VideoCalcFov(real_fovy, video_width, video_height);
	aspect = (double)video_width / (double)video_height;

	/* projection matrix */
	Math_PerspectiveToFrustum(real_fovy, aspect, znear, zfar, &xmin, &xmax, &ymin, &ymax);
	Math_MatrixPerspectiveFrustum4x4(projectionmatrix, (vec_t)xmin, (vec_t)xmax, (vec_t)ymin, (vec_t)ymax, (vec_t)znear, zfar);

	if (set_camera_frustum) /* TODO: this is just temporary, we need to break Sys_VideoSet3D up */
	{
		vec3_t forward, right, up, center;
		Math_AnglesToVec(angles, forward, right, up);
		Math_Vector3Add(origin, forward, center);

		current_frustum = &primary_render_frustum;
		current_light_kdop = NULL;
		Sys_Video3DFrustumCullingSetProjectionData(&primary_render_frustum, (vec_t)real_fovy, (vec_t)aspect, (vec_t)znear, zfar);
		Sys_Video3DFrustumCullingSetViewData(&primary_render_frustum, origin, center, up);
	}
	current_occlusion_data = &render_occlusion_data;

	/* view matrix */
	Math_MatrixIdentity4x4(viewmatrix);

	Math_MatrixRotateZ4x4(viewmatrix, -angles[ANGLES_ROLL]);
	Math_MatrixRotateX4x4(viewmatrix, -angles[ANGLES_PITCH]);
	Math_MatrixRotateY4x4(viewmatrix, -angles[ANGLES_YAW]);
	Math_MatrixTranslate4x4(viewmatrix, -origin[0], -origin[1], -origin[2]);

	/* model matrix */
	Math_MatrixIdentity4x4(modelmatrix);

	/* modelview, modelview_normals and modelviewprojection will be updated only inside Sys_VideoTransformFor3DModel, so make sure to have viewmatrix and projectionmatrix settled when calling it. */
	Math_MatrixMultiply4x4(modelviewmatrix, viewmatrix, modelmatrix);
	{
		vec_t tmp[16];
		Math_MatrixInverse4x4(tmp, modelviewmatrix);
		Math_MatrixTranspose4x4(tmp, tmp);
		Math_Matrix3x3From4x4Top(modelviewmatrix_normals, tmp);
	}
	Math_MatrixMultiply4x4(modelviewprojectionmatrix, projectionmatrix, modelviewmatrix);

	/* misc */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW); /* TODO: CAN WE CHANGE THIS TO GL_CW TO TEMPORARILTY TO DEAL EASILY WITH INVERTED VERTEX ORDER IN SOME MODELS? ALSO, SE IF THERE IS NO PIPELINE FLUSH */
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable(GL_DEPTH_TEST);
	Sys_VideoSetDepthTest(true, true); /* by default, we use the depth buffer in 3d mode */
	Sys_VideoSetDepthMask(true, true);
	glClearColor(0, 0, 0, 0);
}

static GLdouble unproj_projectionmatrix[16];
static GLdouble unproj_viewmatrix[16];
static GLint unproj_viewport[4];
static int unproj_valid = false;

/*
===================
Sys_Video3DPointAndDirectionFromUnitXYSetMatrixes

Unprojects a screen coordinate in the [0, 1) range.
Returns true if successful
TODO: is it [0, 1]?
TODO: server should create the matrices and calculate this itself instead of accepting the input from the player. CHEAT RISK! And there are also aspect ratio issues, etc for the server to deal... Damn!
===================
*/
void Sys_Video3DPointAndDirectionFromUnitXYSetMatrixes(const int valid, const vec_t *in_projectionmatrix, const vec_t *in_viewmatrix, const int *in_viewport)
{
	int i;

	if (!valid)
	{
		unproj_valid = false;
		return;
	}

	unproj_valid = true;
	for (i = 0; i < 16; i++)
	{
		unproj_projectionmatrix[i] = in_projectionmatrix[i];
		unproj_viewmatrix[i] = in_viewmatrix[i];
	}
	for (i = 0; i < 4; i++)
	{
		unproj_viewport[i] = in_viewport[i];
	}
}

/*
===================
Sys_Video3DPointAndDirectionFromUnitXY

Unprojects a screen coordinate in the [0, 1) range.
Returns true if successful
TODO: is it [0, 1]?
TODO: server should create the matrices and calculate this itself instead of accepting the input from the player. CHEAT RISK! And there are also aspect ratio issues, etc for the server to deal... Damn!
===================
*/
int Sys_Video3DPointAndDirectionFromUnitXY(const vec_t unit_x, const vec_t unit_y, vec3_t point, vec3_t direction)
{
	GLdouble vecnear[3];
	GLdouble vecfar[3];
	int i;
	GLdouble dir[3];
	vec_t screen_x, screen_y;

	if (!unproj_valid)
		return false;

	screen_x = unit_x * video_width;
	screen_y = video_height - unit_y * video_height; /* convert up-down to down-up */

	if (gluUnProject(screen_x, screen_y, -1, unproj_viewmatrix, unproj_projectionmatrix, unproj_viewport, &vecnear[0], &vecnear[1], &vecnear[2]) != GL_TRUE)
		return false;
	if (gluUnProject(screen_x, screen_y, 1, unproj_viewmatrix, unproj_projectionmatrix, unproj_viewport, &vecfar[0], &vecfar[1], &vecfar[2]) != GL_TRUE)
		return false;

	Math_Vector3ScaleAdd(vecnear, -1, vecfar, dir);
	for (i = 0; i < 3; i++) /* TODO: because of double vs float */
	{
		direction[i] = (vec_t)dir[i];
		point[i] = (vec_t)vecnear[i];
	}

	return true;
}

/*
===================
Sys_VideoSet2D
===================
*/
void Sys_VideoSet2D(int use_base_size)
{
	glViewport(0, 0, video_width, video_height);

	if (use_base_size)
	{
		/* adjust horizontal resolution */
		double ratio = (double)video_height / BASE_HEIGHT;
		double newwidth = (double)video_width / ratio;
		double diff = (double)newwidth - BASE_WIDTH;
		/* simulate the base size */
		Math_MatrixOrtho4x4(projectionmatrix, 0.f - ((vec_t)diff / 2.f), (vec_t)BASE_WIDTH + ((vec_t)diff /2.f), (vec_t)BASE_HEIGHT, 0, -99999, 99999);
	}
	else
	{
		/* use the full range */
		Math_MatrixOrtho4x4(projectionmatrix, 0, (vec_t)video_width, (vec_t)video_height, 0, -99999, 99999);
	}

	Math_MatrixIdentity4x4(viewmatrix);
	Math_MatrixIdentity4x4(modelmatrix);

	/* modelview, modelview_normals and modelviewprojection will be updated only inside Sys_VideoTransformFor3DModel, so make sure to have viewmatrix and projectionmatrix settled when calling it. */
	Math_MatrixMultiply4x4(modelviewmatrix, viewmatrix, modelmatrix);
	{
		vec_t tmp[16];
		Math_MatrixInverse4x4(tmp, modelviewmatrix);
		Math_MatrixTranspose4x4(tmp, tmp);
		Math_Matrix3x3From4x4Top(modelviewmatrix_normals, tmp);
	}
	Math_MatrixMultiply4x4(modelviewprojectionmatrix, projectionmatrix, modelviewmatrix);

	/* misc */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	Sys_VideoSetDepthTest(false, true); /* by default, we don't use the depth buffer in 2d mode */
	Sys_VideoSetDepthMask(true, true);
	glActiveTexture(GL_TEXTURE0); /* TODO: needed? */
}

/*
===================
Sys_Video2DAbsoluteFromUnit*

Gets an absolute 2d position from a unitary position on the screen.
May return negative values for horizontal, but all values are guaranteed to be on the screen's 2D viewable area
Same input may return different results for different aspect ratios
===================
*/
void Sys_Video2DAbsoluteFromUnitX(vec_t unit_x, vec_t *abs_x)
{
	double ratio;
	double newwidth;
	double diff;

	if (!video_width || !video_height)
		Sys_Error("Sys_Video2DAbsoluteFromUnitX: Video mode note set!\n");

	/* TODO: precalc this, it's used elsewhere also */
	ratio = (double)video_height / BASE_HEIGHT;
	newwidth = (double)video_width / ratio;
	diff = (double)newwidth - BASE_WIDTH;

	*abs_x = (vec_t)(unit_x * newwidth - (diff / 2.));
}
void Sys_Video2DAbsoluteFromUnitY(vec_t unit_y, vec_t *abs_y)
{
	*abs_y = unit_y * BASE_HEIGHT;
}


/*
===================
Sys_VideoDraw2DPic

Should only be called after Sys_VideoSet2D
===================
*/
void Sys_VideoDraw2DPic(unsigned int *id, int width, int height, int x, int y)
{
	int i;
	GLfloat quaddata[36];
	GLfloat *quadverts, *quadcolors, *quadtexcoords;
	quadverts = quaddata; /* size 12 */
	quadcolors = quadverts + 12; /* size 16 */
	quadtexcoords = quadcolors + 16; /* size 8 */

	quadverts[0] = (vec_t)x; quadverts[1] = (vec_t)(y + height); quadverts[2] = 0;
	quadverts[3] = (vec_t)(x + width); quadverts[4] = (vec_t)(y + height); quadverts[5] = 0;
	quadverts[6] = (vec_t)x; quadverts[7] = (vec_t)y; quadverts[8] = 0;
	quadverts[9] = (vec_t)(x + width); quadverts[10] = (vec_t)y; quadverts[11] = 0;
	for (i = 0; i < 16; i++)
		quadcolors[i] = 1.0;
	quadtexcoords[0] = 0; quadtexcoords[1] = 1;
	quadtexcoords[2] = 1; quadtexcoords[3] = 1;
	quadtexcoords[4] = 0; quadtexcoords[5] = 0;
	quadtexcoords[6] = 1; quadtexcoords[7] = 0;

	Sys_VideoBindShaderProgram(SHADER_2D, NULL, NULL, NULL);

	Sys_BindStreamingVBO(false);

	Sys_BindTexture(*id, GL_TEXTURE0);
	Sys_BindTexture(-1, GL_TEXTURE1); /* TODO: needed? */
	Sys_BindTexture(-1, GL_TEXTURE3); /* TODO: needed? */
	Sys_BindTexture(-1, GL_TEXTURE4); /* TODO: needed? */
	Sys_BindTexture(-1, GL_TEXTURE5); /* TODO: needed? */

	Sys_VideoTransformFor3DModel(NULL);

	/* TODO colors? */
	glBufferData(GL_ARRAY_BUFFER, sizeof(quaddata), quaddata, GL_STREAM_DRAW);
	glVertexAttribPointer(SHADER_ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, (void *)((quadverts - quaddata) * sizeof(GLfloat)));
	glEnableVertexAttribArray(SHADER_ATTRIB_VERTEX);
	glVertexAttribPointer(SHADER_ATTRIB_COLOR, 4, GL_FLOAT, 0, 0, (void *)((quadcolors - quaddata) * sizeof(GLfloat)));
	glEnableVertexAttribArray(SHADER_ATTRIB_COLOR);
	glVertexAttribPointer(SHADER_ATTRIB_TEXCOORD0, 2, GL_FLOAT, 0, 0, (void *)((quadtexcoords - quaddata) * sizeof(GLfloat)));
	glEnableVertexAttribArray(SHADER_ATTRIB_TEXCOORD0);
	glDisableVertexAttribArray(SHADER_ATTRIB_TEXCOORD1);
	glDisableVertexAttribArray(SHADER_ATTRIB_NORMAL);
	glDisableVertexAttribArray(SHADER_ATTRIB_TANGENT);
	glDisableVertexAttribArray(SHADER_ATTRIB_WEIGHTS);
	glDisableVertexAttribArray(SHADER_ATTRIB_BONES);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/*
===================
Sys_VideoDrawText

Should only be called after Sys_VideoSet2D
RGBA should be 0-1
===================
*/
void Sys_VideoDraw2DText(const char *text, vec_t x, vec_t y, vec_t scalex, vec_t scaley, vec_t r, vec_t g, vec_t b, vec_t a)
{
	vec_t transform[16];

	Sys_VideoBindShaderProgram(SHADER_2DTEXT, NULL, NULL, NULL);

	Sys_BindStreamingVBO(false);

	/* do this in reverse order, leave GL_TEXTURE0 active for the font rendering library */
	Sys_BindTexture(-1, GL_TEXTURE1); /* TODO: needed? */
	Sys_BindTexture(-1, GL_TEXTURE3); /* TODO: needed? */
	Sys_BindTexture(-1, GL_TEXTURE4); /* TODO: needed? */
	Sys_BindTexture(-1, GL_TEXTURE5); /* TODO: needed? */
	Sys_BindTexture(texture_nullopaque->id, GL_TEXTURE0); /* TODO: just select, do not bind */

	{ const vec4_t v_color = {r, g, b, a}; Sys_VideoUpdateUniform(SHADER_UNIFORM_COLOR, v_color, 1); }

	/* coordinate system is from bottom-left, put it on upper-left */
	y += Sys_FontGetStringHeight(text, scaley);

	/* scale affects position, se adjust it */
	x /= scalex;
	y /= scaley;

	/* coordinate system is from bottom-left, put it on upper-left */
	Math_MatrixIdentity4x4(transform);
	Math_MatrixScale4x4(transform, scalex, -scaley, 1);
	Math_MatrixTranslate4x4(transform, x, -y, 0);
	Sys_VideoTransformFor3DModel(transform);

	/* TODO glPushAttrib(GL_ALL_ATTRIB_BITS); */
	Sys_FontDraw(text);
	/* TODO glPopAttrib(); */

	{ const vec4_t v_color = {1, 1, 1, 1}; Sys_VideoUpdateUniform(SHADER_UNIFORM_COLOR, v_color, 1); } /* TODO */

	bound_texture0 = -2; /* TODO: cache the font texture too */
}

/*
===================
Sys_VideoDraw2DFill

Should only be called after Sys_VideoSet2D
RGBA should be 0-1
===================
*/
void Sys_VideoDraw2DFill(int width, int height, int x, int y, vec_t r, vec_t g, vec_t b, vec_t a)
{
	/* TODO better to change shaders or use nullopaque? */
	int i;
	GLfloat quaddata[36];
	GLfloat *quadverts, *quadcolors, *quadtexcoords;
	quadverts = quaddata; /* size 12 */
	quadcolors = quadverts + 12; /* size 16 */
	quadtexcoords = quadcolors + 16; /* size 8 */

	quadverts[0] = (vec_t)x; quadverts[1] = (vec_t)(y + height); quadverts[2] = 0;
	quadverts[3] = (vec_t)(x + width); quadverts[4] = (vec_t)(y + height); quadverts[5] = 0;
	quadverts[6] = (vec_t)x; quadverts[7] = (vec_t)y; quadverts[8] = 0;
	quadverts[9] = (vec_t)(x + width); quadverts[10] = (vec_t)y; quadverts[11] = 0;

	for (i = 0; i < 16; i += 4)
	{
		quadcolors[i] = r;
		quadcolors[i + 1] = g;
		quadcolors[i + 2] = b;
		quadcolors[i + 3] = a;
	}
	quadtexcoords[0] = 0; quadtexcoords[1] = 1;
	quadtexcoords[2] = 1; quadtexcoords[3] = 1;
	quadtexcoords[4] = 0; quadtexcoords[5] = 0;
	quadtexcoords[6] = 1; quadtexcoords[7] = 0;

	Sys_VideoBindShaderProgram(SHADER_2D, NULL, NULL, NULL);

	Sys_BindStreamingVBO(false);

	Sys_BindTexture(texture_nullopaque->id, GL_TEXTURE0);
	Sys_BindTexture(-1, GL_TEXTURE1); /* TODO: needed? */
	Sys_BindTexture(-1, GL_TEXTURE3); /* TODO: needed? */
	Sys_BindTexture(-1, GL_TEXTURE4); /* TODO: needed? */
	Sys_BindTexture(-1, GL_TEXTURE5); /* TODO: needed? */

	Sys_VideoTransformFor3DModel(NULL);

	glBufferData(GL_ARRAY_BUFFER, sizeof(quaddata), quaddata, GL_STREAM_DRAW);
	glVertexAttribPointer(SHADER_ATTRIB_VERTEX, 3, GL_FLOAT, 0, 0, (void *)((quadverts - quaddata) * sizeof(GLfloat)));
	glEnableVertexAttribArray(SHADER_ATTRIB_VERTEX);
	glVertexAttribPointer(SHADER_ATTRIB_COLOR, 4, GL_FLOAT, 0, 0, (void *)((quadcolors - quaddata) * sizeof(GLfloat)));
	glEnableVertexAttribArray(SHADER_ATTRIB_COLOR);
	glVertexAttribPointer(SHADER_ATTRIB_TEXCOORD0, 2, GL_FLOAT, 0, 0, (void *)((quadtexcoords - quaddata) * sizeof(GLfloat)));
	glEnableVertexAttribArray(SHADER_ATTRIB_TEXCOORD0);
	glDisableVertexAttribArray(SHADER_ATTRIB_TEXCOORD1);
	glDisableVertexAttribArray(SHADER_ATTRIB_NORMAL);
	glDisableVertexAttribArray(SHADER_ATTRIB_TANGENT);
	glDisableVertexAttribArray(SHADER_ATTRIB_WEIGHTS);
	glDisableVertexAttribArray(SHADER_ATTRIB_BONES);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/*
===================
Sys_VideoDraw3DTrianglesOrVBOCheckPass

Should not be called directly by client code.
Returns true if a given triangle data or vbo data would be drawn in this pass, in this frame
TODO: other conditions
===================
*/
int Sys_VideoDraw3DTrianglesOrVBOCheckPass(int texture_cl_id0, int texture_cl_id1, int texture_cl_id3, int texture_cl_id4, int texture_cl_id5)
{
	if (texture_cl_id0 == -1 && video_3d_alpha_pass) /* if no texture, draw in the opaque pass FIXME */
		return false;
	if (texture_cl_id0 != -1)
	{
		if (texture_sys[texture_cl_id0].texture_has_alpha && !video_3d_alpha_pass)
			return false;
		if (!texture_sys[texture_cl_id0].texture_has_alpha && video_3d_alpha_pass)
			return false;
	}

	return true;
}

/*
===================
Sys_VideoDraw3DTriangles

Should not be called directly.
Returns true if a rendering call took place.
origins, colors, texcoords0, texcoords1, normals are in the formats defined in model_vertex_t
TODO: shader programs, pass all vertex atributes to shaders.
TODO: in the future, error if no normal is passed
TODO: indices as GL_UNSIGNED_INT (GLES2 platforms mostly won't work with this, use GL_UNSIGNED_SHORT for them), check indice limit when loading models
===================
*/
int Sys_VideoDraw3DTriangles(const model_vertex_t *verts, const int vert_count, const int use_origins, const int use_colors, const int use_texcoords0, const int use_texcoords1, const int use_normals, const unsigned int *indices, const unsigned int indices_count, int texture_cl_id0, int texture_cl_id1, int texture_cl_id3, int texture_cl_id4, int texture_cl_id5, int is_triangle_strip_or_fan)
{
	static const int common_stride = sizeof(model_vertex_t);
	const model_vertex_t *v = NULL; /* for pointers */

	if (!Sys_VideoDraw3DTrianglesOrVBOCheckPass(texture_cl_id0, texture_cl_id1, texture_cl_id3, texture_cl_id4, texture_cl_id5))
		return false;

	Sys_BindStreamingVBO(true);

	/* TODO: buffering unused vertex components (weight, etc), buffering everything everytime */
	glBufferData(GL_ARRAY_BUFFER, sizeof(model_vertex_t) * vert_count, verts, GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices_count, indices, GL_STREAM_DRAW); /* TODO: use GL_UNSIGNED_SHORT or GL_UNSIGNED_BYTE when applicable */

	/* vertices */
	if (use_origins)
	{
		glVertexAttribPointer(SHADER_ATTRIB_VERTEX, 3, GL_FLOAT, 0, common_stride, &v->origin);
		glEnableVertexAttribArray(SHADER_ATTRIB_VERTEX);
	}
	else
		glDisableVertexAttribArray(SHADER_ATTRIB_VERTEX);

	/* colors */
	if (use_colors)
	{
		glVertexAttribPointer(SHADER_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, 0, common_stride, &v->color);
		glEnableVertexAttribArray(SHADER_ATTRIB_COLOR);
	}
	else
		glDisableVertexAttribArray(SHADER_ATTRIB_COLOR);

	/* textures */
	Sys_BindTexture(texture_cl_id0 != -1 ? texture_sys[texture_cl_id0].id : texture_nullopaque->id, GL_TEXTURE0);
	if (use_texcoords0)
	{
		glVertexAttribPointer(SHADER_ATTRIB_TEXCOORD0, 2, GL_FLOAT, 0, common_stride, &v->texcoord0);
		glEnableVertexAttribArray(SHADER_ATTRIB_TEXCOORD0);
	}
	else
		glDisableVertexAttribArray(SHADER_ATTRIB_TEXCOORD0);

	Sys_BindTexture(texture_cl_id1 != -1 ? texture_sys[texture_cl_id1].id : texture_nullopaque->id, GL_TEXTURE1);
	if (use_texcoords1)
	{
		glVertexAttribPointer(SHADER_ATTRIB_TEXCOORD1, 2, GL_FLOAT, 0, common_stride, &v->texcoord1);
		glEnableVertexAttribArray(SHADER_ATTRIB_TEXCOORD1);
	}
	else
		glDisableVertexAttribArray(SHADER_ATTRIB_TEXCOORD1);

	Sys_BindTexture(texture_cl_id3 != -1 ? texture_sys[texture_cl_id3].id : texture_nullopaque->id, GL_TEXTURE3);
	Sys_BindTexture(texture_cl_id4 != -1 ? texture_sys[texture_cl_id4].id : texture_nullopaque->id, GL_TEXTURE4);
	Sys_BindTexture(texture_cl_id5 != -1 ? texture_sys[texture_cl_id5].id : texture_nullopaque->id, GL_TEXTURE5);

	/* normals */
	if (use_normals)
	{
		glVertexAttribPointer(SHADER_ATTRIB_NORMAL, 3, GL_FLOAT, 0, common_stride, &v->normal);
		glEnableVertexAttribArray(SHADER_ATTRIB_NORMAL);
	}
	else
		glDisableVertexAttribArray(SHADER_ATTRIB_NORMAL);

	glDisableVertexAttribArray(SHADER_ATTRIB_TANGENT);
	glDisableVertexAttribArray(SHADER_ATTRIB_WEIGHTS);
	glDisableVertexAttribArray(SHADER_ATTRIB_BONES);

	/* draw them TODO: use GL_UNSIGNED_SHORT or GL_UNSIGNED_BYTE when applicable */
	switch (is_triangle_strip_or_fan)
	{
		case 0:
			glDrawElements(GL_TRIANGLES, indices_count, GL_UNSIGNED_INT, 0); /* TODO: always use glDrawRangeElements? */
			break;
		case 1:
			glDrawElements(GL_TRIANGLE_STRIP, indices_count, GL_UNSIGNED_INT, 0); /* TODO: always use glDrawRangeElements? */
			break;
		case 2:
			glDrawElements(GL_TRIANGLE_FAN, indices_count, GL_UNSIGNED_INT, 0); /* TODO: always use glDrawRangeElements? */
			break;
		default:
			Sys_Error("Sys_VideoDraw3DTriangles: is_triangle_strip_or_fan == %d\n", is_triangle_strip_or_fan);
	}

	return true;
}

/*
===================
Sys_VideoDrawVBO

Should not be called directly.
Returns true if a rendering call took place.
WARNING: keep offsets in sync with model_vertex_t

TODO: shader programs, pass all vertex atributes to shaders.
TODO: do not bind texture in this function, so that the entire world can be drawn with the same texture atlas
(or modify Sys_VideoBindTexture to not bind again when it's the same - do gl drivers do this?
TODO: in the future, error if no normal is passed
TODO: indices as GL_UNSIGNED_INT version (GLES2 platforms mostly won't work with this, use GL_UNSIGNED_SHORT for them), beware of negative parameters, check indice limit when loading models
===================
*/
int Sys_VideoDrawVBO(const int id, int texture_cl_id0, int texture_cl_id1, int texture_cl_id3, int texture_cl_id4, int texture_cl_id5, int vertstartinclusive, int vertendinclusive, int idxcount, int idxstart)
{
	int type;

	if (!Sys_VideoDraw3DTrianglesOrVBOCheckPass(texture_cl_id0, texture_cl_id1, texture_cl_id3, texture_cl_id4, texture_cl_id5))
		return false;

	Sys_BindVBO(id);
	Sys_BindTexture(texture_cl_id0 != -1 ? texture_sys[texture_cl_id0].id : texture_nullopaque->id, GL_TEXTURE0);
	Sys_BindTexture(texture_cl_id1 != -1 ? texture_sys[texture_cl_id1].id : texture_nullopaque->id, GL_TEXTURE1);
	Sys_BindTexture(texture_cl_id3 != -1 ? texture_sys[texture_cl_id3].id : texture_nullopaque->id, GL_TEXTURE3);
	Sys_BindTexture(texture_cl_id4 != -1 ? texture_sys[texture_cl_id4].id : texture_nullopaque->id, GL_TEXTURE4);
	Sys_BindTexture(texture_cl_id5 != -1 ? texture_sys[texture_cl_id5].id : texture_nullopaque->id, GL_TEXTURE5);

	if (vbos[id].is_triangle_strip)
		type = GL_TRIANGLE_STRIP;
	else
		type = GL_TRIANGLES;

	if (vertstartinclusive == -1 || vertendinclusive == -1)
	{
		if (idxcount == -1 || idxstart == -1)
			glDrawElements(type, vbos[id].indices_count, GL_UNSIGNED_INT, NULL); /* TODO: always use glDrawRangeElements? */
		else
			glDrawElements(type, idxcount, GL_UNSIGNED_INT, (void *)(idxstart * sizeof (unsigned int))); /* TODO: always use glDrawRangeElements? */
	}
	else
        /* TODO: see limits for range, use glGet() */
		glDrawRangeElements(type, vertstartinclusive, vertendinclusive, idxcount, GL_UNSIGNED_INT, (void *)(idxstart * sizeof (unsigned int))); /* TODO: not index limits, but value limits? */

	return true;
}

/*
===================
Sys_StartVideoFrame
===================
*/
void Sys_StartVideoFrame(void)
{
	if ((int)vid_vsync->doublevalue != vsync_enabled)
	{
		vsync_enabled = (int)vid_vsync->doublevalue;
		SDL_GL_SetSwapInterval(vsync_enabled ? 1 : 0); /* this function may be called at will on an already created context? */
	}
	Sys_Video3DPointAndDirectionFromUnitXYSetMatrixes(false, NULL, NULL, NULL); /* clear the unprojection of leftovers */

	/* Clear the color and depth buffers. */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	/* TODO: this is here only because now we can be sure that the cl subsystem has initialized */
	/* TODO: Sys Calling CL */
	if (!texture_nullopaque)
		texture_nullopaque = &texture_sys[CL_LoadTexture("nullopaque", true, NULL, 0, 0, false, 1, 1)->cl_id];
	if (!texture_nulltransparent)
		texture_nulltransparent = &texture_sys[CL_LoadTexture("null", true, NULL, 0, 0, false, 1, 1)->cl_id];
}

/*
===================
Sys_EndVideoFrame
===================
*/
void Sys_EndVideoFrame(void)
{
	/*
		Swap the buffers. This this tells the driver to render the next frame from the contents of
		the back-buffer, and to set all rendering operations to occur on what was the front-buffer.

		Double buffering prevents nasty visual tearing from the application drawing on areas of the
		screen that are being updated at the same time.
	*/
	SDL_GL_SwapWindow(window);
}

/*
===================
Sys_GetWidthHeight

This should be used primarily for scaling reasons.
Should ONLY be used by sys_*.c!
===================
*/
void Sys_GetWidthHeight(int *width, int *height)
{
	if (!video_width || !video_height)
		Sys_Error("Sys_GetWidthHeight: video not initialized!\n");

	*width = video_width;
	*height = video_height;
}

/*
===================
Sys_VideoValidateMode

TODO: validate according to system capabilities too
===================
*/
int Sys_VideoValidateMode(int width, int height)
{
	if (width < BASE_WIDTH || height < BASE_HEIGHT)
		return false;

	return true;
}

/*
===================
Sys_VideoChangeMode
===================
*/
void Sys_VideoChangeMode(int width, int height, int fullscreen)
{
	Sys_Printf("Changing resolution...\n");

	if (!window)
		Sys_Error("Sys_VideoChangeMode: Video not initialized\n");

	if (fullscreen)
	{
		width = 0;
		height = 0;
	}
	else if (width == -1 || height == -1)
	{
		width = BASE_WIDTH;
		height = BASE_HEIGHT;
	}

	Sys_FBOCleanup();

	SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	SDL_SetWindowSize(window, width, height);
	SDL_GetWindowSize(window, &video_width, &video_height);
	if (!Sys_VideoValidateMode(video_width, video_height))
		Sys_Error("Sys_VideoChangeMode: Invalid video mode %d x %d\n", video_width, video_height); /* TODO FIXME: reset to default resolution */

	Sys_FBOGenerate((vec_t)video_width, (vec_t)video_height);

	Host_CMDForceCvarSetValue(vid_windowedwidth, video_width, true);
	Host_CMDForceCvarSetValue(vid_windowedheight, video_height, true);

	Sys_Printf("Video mode changed to: %dx%d (%s)\n", video_width, video_height, fullscreen ? "fullscreen" : "windowed");
	Sys_ExclusiveInput(true); /* ignore initial mouse position when starting directly into a game (with +map start, for example) - if the mouse is inside the window but not centered, stray mouse move events would happen */
}

/*
===================
Sys_CMD_VidSetWindowed
===================
*/
EXTERNC int ignore_window_resize_event = false; /* TODO FIXME: hack for ignoring the resizing event of switch from fullscreen to windowed */
void Sys_CMD_VidSetWindowed(void)
{
	if (host_cmd_argc != 3 && host_cmd_argc != 1)
	{
		Sys_Printf("usage:\n\"vid_setwindowed width height\" (set a new windowed resolution)\n\"vid_setwindowed\" (goes to the last saved one)\n");
	}
	else
	{
		if (!vid_fullscreen->doublevalue && !strcmp(host_cmd_argv[1], vid_windowedwidth->charvalue) && !strcmp(host_cmd_argv[2], vid_windowedheight->charvalue))
		{
			Sys_Printf("Already at the requested video mode\n");
			return;
		}
		if (vid_fullscreen->doublevalue)
			ignore_window_resize_event = true;
		Host_CMDForceCvarSetValue(vid_fullscreen, 0, true);
		if (host_cmd_argc == 3)
		{
			Sys_VideoChangeMode(atoi(host_cmd_argv[1]), atoi(host_cmd_argv[2]), (int)vid_fullscreen->doublevalue);
		}
		else
		{
			Sys_VideoChangeMode((int)vid_windowedwidth->doublevalue, (int)vid_windowedheight->doublevalue, (int)vid_fullscreen->doublevalue);
		}
	}
}

/*
===================
Sys_CMD_VidSetFullscreen
===================
*/
void Sys_CMD_VidSetFullscreen(void)
{
	if (host_cmd_argc != 1)
	{
		Sys_Printf("\"vid_setfullscreen\" takes no arguments\n");
	}
	else
	{
		if (vid_fullscreen->doublevalue)
		{
			Sys_Printf("Already at the requested video mode\n");
			return;
		}

		Host_CMDForceCvarSetValue(vid_fullscreen, 1, true);
		Sys_VideoChangeMode((int)vid_windowedwidth->doublevalue, (int)vid_windowedheight->doublevalue, (int)vid_fullscreen->doublevalue);
	}
}

/*
===================
Sys_CMD_Screenshot
===================
*/
void Sys_CMD_Screenshot(void)
{
	if (host_cmd_argc > 2)
	{
		Sys_Printf("Usage: \"screenshot\" [optional name without extension]\n");
	}
	else
	{
		int lowmark;
		unsigned char *buffer;
		char name[MAX_PATH];

		if (host_cmd_argc == 2)
		{
			Sys_Snprintf(name, sizeof(name), "%s.tga", host_cmd_argv[1]);
		}
		else
		{
			int i;
			for (i = 0; i <= 9999; i++)
			{
				Sys_Snprintf(name, sizeof(name), "screenshot%04d.tga", i);
				if (Host_FSFileExists(name) == -1)
					break;
			}
			if (i == 10000)
			{
				Sys_Printf("Can't take screenshot, folder full. Move or delete some older screenshots to create space.\n");
				return;
			}
		}

		lowmark = Sys_MemLowMark(&tmp_mem);
		buffer = (unsigned char *)Sys_MemAlloc(&tmp_mem, video_width * video_height * 3 + sizeof(TargaHeader), "screenshot");

		/* created an uncompressed tga file by hand */
		buffer[2] = 2;
		buffer[12] = video_width & 255;
		buffer[13] = video_width >> 8;
		buffer[14] = video_height & 255;
		buffer[15] = video_height >> 8;
		buffer[16] = 24;
		glReadPixels(0, 0, video_width, video_height, GL_RGB, GL_UNSIGNED_BYTE, buffer + sizeof(TargaHeader));

		/* SWAP TODO: not needed on windows??? WHY THE NEED TO SWAP BLUE AND GREEN ON LINUX? */
		unsigned char *ptr = buffer + sizeof(TargaHeader);
		for (int i = 0; i < video_width * video_height; i++)
		{
			unsigned char tmp = ptr[i * 3 + 1];
			ptr[i * 3 + 1] = ptr[i * 3 + 2];
			ptr[i * 3 + 2] = tmp;
		}

		if (Host_FSWriteBinaryFile(name, buffer, video_width * video_height * 3 + sizeof(TargaHeader)) != video_width * video_height * 3 + sizeof(TargaHeader))
			Sys_Printf("Couldn't write %s\n", name);
		else
			Sys_Printf("Wrote %s\n", name);

		Sys_MemFreeToLowMark(&tmp_mem, lowmark);
	}
}

void myErrorCallback(GLenum _source, GLenum _type, GLuint _id, GLenum _severity, GLsizei _length, const char *_message, const void *_userParam)
{
    /* TODO: decide what errors cause a Sys_Error() or Host_Error() (being aware that a Host_Error() may leave the display in an unconsistent state) */
    printf("OpenGL Message: %s\n", _message);
}

/*
===================
Sys_InitVideo

A full screen display will always ignore wanted_width and wanted_height and
use the current system resolution, to prevent unwanted artifacts in LCD displays.

TODO: Call SDL_GetDisplayMode() in a loop, SDL_GetNumDisplayModes() times to list all resolutions in the menu for selecting resolution
TODO: context/window bpp?
TODO: support fullscreen changing resolution?
TODO: output device selection? or leave that to the operating system?
TODO: IMPORTANT: if gl version TOO OLD, ERROR! Reason: gl function calls will cause segfaults
===================
*/
void Sys_InitVideo(void)
{
	/* start at the default resolution, windowed */
	int wanted_width = -1;
	int wanted_height = -1;
	int wanted_fullscreen = false;

	/* Dimensions of our window. */
	int width = 0;
	int height = 0;
	/* Flags we will pass into SDL_SetVideoMode. */
	int flags = 0;
	const char *version;
	GLenum err; /* for glew */

	if (SDL_WasInit(SDL_INIT_VIDEO))
		Sys_Error("Sys_InitVideo: already initialized!\n");

	SDL_InitSubSystem(SDL_INIT_VIDEO);

	/* use current system values */
	if (!wanted_width || !wanted_height || wanted_fullscreen)
	{
		width = 0;
		height = 0;
	}
	else if (wanted_width == -1 || wanted_height == -1)
	{
		width = BASE_WIDTH;
		height = BASE_HEIGHT;
	}
	else
	{
		width = wanted_width;
		height = wanted_height;
	}

	/*
		Now, we want to setup our requested window attributes for our OpenGL window.
		We want 8 bits of red, green and blue. We also want at least a 24-bit depth buffer.

		The last thing we do is request a double buffered window. '1' turns on double
		buffering, '0' turns it off.
	*/
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); /* TODO: fails with gl < 3.x??? */
	/* SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY); */

	/* TODO: drawing to framebuffers DOES NOT use AA */
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); /* TODO */
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);	/* TODO */

	if (wanted_fullscreen) /* We want to request that SDL provide us with an OpenGL window, in a fullscreen video mode (without changing the resolution) */
		flags = SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP;
	else
		flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	/* Set the video mode TODO: set title from a game config */
	window = SDL_CreateWindow("Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
	if (!window)
	{
		/*
			This could happen for a variety of reasons, including DISPLAY not being set,
			the specified resolution not being available, etc.
		*/
		Sys_Error("Video mode set failed: %s\n", SDL_GetError());
	}
	if (!SDL_GL_CreateContext(window))
		Sys_Error("GL Context creation failed: %s\n", SDL_GetError());

	/* TODO ICON SDL_SetWindowIcon(window, icon); */
	SDL_ShowCursor(false);

	version = (const char *)glGetString(GL_VERSION);
	Sys_Printf("GL version: %s\n", version);

	/* set up parameters */
	SDL_GetWindowSize(window, &video_width, &video_height);
	if (!Sys_VideoValidateMode(video_width, video_height))
		Sys_Error("Invalid video mode %d x %d\n", video_width, video_height);

	/* TODO: refresh rate? set according to the max fps we want for our game? */
	Sys_Printf("Sys_InitVideo: %dx%d (%s) OK.\n", video_width, video_height, wanted_fullscreen ? "fullscreen" : "windowed");

	vid_vsync = Host_CMDAddCvar("vid_vsync", "1", CVAR_ARCHIVE);
	r_fov = Host_CMDAddCvar("r_fov", "90", CVAR_ARCHIVE);
	r_znear = Host_CMDAddCvar("r_znear", "0.05", CVAR_ARCHIVE);
	r_zfar = Host_CMDAddCvar("r_zfar", "4096", CVAR_ARCHIVE);
	r_lodbias = Host_CMDAddCvar("r_lodbias", "1.2", CVAR_ARCHIVE); /* TODO: good default for this */
	r_lodhysteresis = Host_CMDAddCvar("r_lodhysteresis", "10", CVAR_ARCHIVE); /* TODO: good default for this */
	r_lockpvs = Host_CMDAddCvar("r_lockpvs", "0", 0);
	r_linearfiltering = Host_CMDAddCvar("r_linearfiltering", "1", CVAR_ARCHIVE);

	/* TODO: triple buffering */
	vsync_enabled = (int)vid_vsync->doublevalue;
	SDL_GL_SetSwapInterval(vsync_enabled ? 1 : 0); /* this function may be called at will on an already created context? */
	/*
	 * "GLEW's problem is that it calls glGetString(GL_EXTENSIONS) which causes
	 * GL_INVALID_ENUM on GL 3.2 core context as soon as glewInit() is called. It
	 * also doesn't fetch the function pointers. The solution is for GLEW to use
	 * glGetStringi instead. The current version of GLEW is 1.7.0 but they still
	 * haven't corrected it. The only fix is to use glewExperimental for now."
	 */
	glewExperimental = true;

	err = glewInit();
	if (err != GLEW_OK)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		Sys_Error("GLEW Error: %s\n", glewGetErrorString(err));
	}

	Sys_Printf("Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

    /* TODO: First check for GL_ARB_debug_output, then... */
    glDebugMessageCallbackARB((GLDEBUGPROCARB)myErrorCallback, NULL);
    glEnable(GL_DEBUG_OUTPUT);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

#ifdef USE_PRIMITIVE_RESTART_INDEX
	/* TODO: best place to set this? */
	glEnable(GL_PRIMITIVE_RESTART);
	/* TODO: needs gl 3.1 */
	glPrimitiveRestartIndex(SYS_VIDEO_PRIMITIVE_RESTART_INDEX);
#endif
	/* Create framebuffer for shadowing */
	Sys_FBOGenerate((vec_t)video_width, (vec_t)video_height);

	/* Load shader programs and etc, only once per context */
	Sys_VideoLoadShaderPrograms();
	Sys_ShadowCreateLightMatrices();
	Sys_FontInit();

	/* these cvars should be changed by cmds, because a resolution change is needed */
	vid_windowedwidth = Host_CMDAddCvar("_vid_windowedwidth", "-1", CVAR_READONLY | CVAR_ARCHIVE);
	vid_windowedheight = Host_CMDAddCvar("_vid_windowedheight", "-1", CVAR_READONLY | CVAR_ARCHIVE);
	vid_fullscreen = Host_CMDAddCvar("_vid_fullscreen", "1", CVAR_READONLY | CVAR_ARCHIVE);
	Host_CMDAdd("vid_setwindowed", Sys_CMD_VidSetWindowed);
	Host_CMDAdd("vid_setfullscreen", Sys_CMD_VidSetFullscreen);
	Host_CMDAdd("screenshot", Sys_CMD_Screenshot);
	/* restart with saved values, which were already loaded */
	Sys_VideoChangeMode((int)vid_windowedwidth->doublevalue, (int)vid_windowedheight->doublevalue, (int)vid_fullscreen->doublevalue);

	bound_vbo = -1;
	bound_texture0 = -1;
	bound_texture1 = -1;
	bound_texture2 = -1;
	bound_texture3 = -1;
	bound_texture4 = -1;
	bound_texture5 = -1;
	bound_texture6 = -1;
	bound_texture7 = -1;
	bound_texture8 = -1;
	bound_texture9 = -1;
	bound_texture10 = -1;
	bound_texture11 = -1;
	bound_texture12 = -1;
	Sys_VideoInitUniforms();
	Sys_Video3DCacheOcclusionInit();
}

/*
===================
Sys_ShutdownVideo

TODO: use AMD CodeXL to detect leaks
===================
*/
void Sys_ShutdownVideo(void)
{
	Sys_Video3DCacheOcclusionShutdown();
	Sys_FontShutdown();
	Sys_FBOCleanup();
	SDL_GL_DeleteContext(window);
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	window = NULL;
	video_width = 0;
	video_height = 0;
}

/*
============================================================================

Skyboxes

============================================================================
*/

int skybox_exists = false;
int skybox_myvbo = 0;
GLuint skybox_texture = 0;

int skybox_indices[] = {
	/* front face */
	0, 1, 2,
	0, 2, 3,
	/* back face */
	4, 5, 6,
	4, 6, 7,
	/* right face */
	1, 4, 7,
	1, 7, 2,
	/* left face */
	5, 0, 3,
	5, 3, 6,
	/* top face */
	3, 2, 7,
	3, 7, 6,
	/* bottom face */
	5, 4, 1,
	5, 1, 0
};

vec_t skybox_vertices[] = {
	-10.0f, -10.0f,  10.0f,
	 10.0f, -10.0f,  10.0f,
	 10.0f,  10.0f,  10.0f,
	-10.0f,  10.0f,  10.0f,
	 10.0f, -10.0f, -10.0f,
	-10.0f, -10.0f, -10.0f,
	-10.0f,  10.0f, -10.0f,
	 10.0f,  10.0f, -10.0f
};

model_vertex_t skybox_vertices_full[8];

/*
===================
Sys_SkyboxLoad
===================
*/
void Sys_SkyboxLoad(unsigned char *name)
{
	if (skybox_exists)
		return;
	int marker = Sys_MemLowMark(&tmp_mem);

	model_trimesh_part_t skyboxtrimesh;
	skyboxtrimesh.indexes = skybox_indices;
	skyboxtrimesh.index_count = 36;
	skyboxtrimesh.index_stride = sizeof(int) * 3;
	skyboxtrimesh.verts = skybox_vertices_full;
	skyboxtrimesh.vert_count = 8;
	skyboxtrimesh.vert_stride = sizeof(model_vertex_t);

	for (int i = 0; i < 8; i++)
	{
		skybox_vertices_full[i].origin[0] = skybox_vertices[i * 3];
		skybox_vertices_full[i].origin[1] = skybox_vertices[i * 3 + 1];
		skybox_vertices_full[i].origin[2] = skybox_vertices[i * 3 + 2];
	}

	skybox_myvbo = Sys_UploadVBO(-1, &skyboxtrimesh, false, false);

	/* TODO: see if the sides are correct */
	const char *side_suffix[] = {"_ft", "_bk", "_up", "_dn", "_lf", "_rt"};
	const GLuint side_defs[] = {	GL_TEXTURE_CUBE_MAP_POSITIVE_Z, /* invert z */
								GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
								GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
								GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
								GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
								GL_TEXTURE_CUBE_MAP_POSITIVE_X};

	/* generate a cube-map texture to hold all the sides */
	glActiveTexture(GL_TEXTURE2);
	glGenTextures(1, &skybox_texture);

	/* load each image and copy into a side of the cube-map texture */
	for (int i = 0; i < 6; i++)
	{
		char side_full_name[MAX_TEXTURE_NAME];
		Sys_Snprintf(side_full_name, MAX_TEXTURE_NAME, "skybox/%s%s", name, side_suffix[i]);

		glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture);

		int x, y;
		unsigned char *image_data;
		Sys_LoadTextureData(side_full_name, &x, &y, &image_data, &tmp_mem);

		if ((x & (x - 1)) != 0 || (y & (y - 1)) != 0)
			Sys_Printf("WARNING: image %s is not power-of-2 dimensions\n", side_full_name);

		/* copy image data into 'target' side of cube map */
		glTexImage2D(side_defs[i], 0, GL_RGBA8, x, y, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, image_data);
		Sys_MemFreeToLowMark(&tmp_mem, marker);
	}

	/* format cube map texture */
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	skybox_exists = true;
}

/*
===================
Sys_SkyboxUnload
===================
*/
void Sys_SkyboxUnload(void)
{
	if(!skybox_exists)
		return;

	if (skybox_texture)
	{
		glDeleteTextures(1, &skybox_texture);
		skybox_texture = 0;
	}

	skybox_exists = false;
}

/*
===================
Sys_DrawSkybox
===================
*/
void Sys_DrawSkybox(unsigned int desired_shader)
{
	if (!skybox_exists)
		return;

	Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL);
	Sys_BindTexture(skybox_texture, GL_TEXTURE2);
	/* the coordinates are for an aabb, but we are inside it! so don't cull */
	glDisable(GL_CULL_FACE);
	Sys_VideoDrawVBO(skybox_myvbo, -1, -1, -1, -1, -1, -1, -1, -1, -1);
	glEnable(GL_CULL_FACE);
}

/*
============================================================================

3D Video Frame Rendering

============================================================================
*/

/*
===================
Sys_DrawVoxels
===================
*/
/* FIXME: not being reentrant is the lesser problem of these globals */
int sortedvoxels[VOXEL_MAX_CHUNKS];
static vec_t voxeldists[VOXEL_MAX_CHUNKS]; /* TODO: cache distances */
int voxel_zcmp(const void * a, const void * b)
{
	if (voxeldists[*(int *)a] < voxeldists[*(int *)b])
		return -1;
	if (voxeldists[*(int *)a] > voxeldists[*(int *)b])
		return 1;
	return 0;
}
int voxel_zcmpinv(const void * a, const void * b)
{
	if (voxeldists[*(int *)a] > voxeldists[*(int *)b])
		return -1;
	if (voxeldists[*(int *)a] < voxeldists[*(int *)b])
		return 1;
	return 0;
}
void Sys_DrawVoxels(vec3_t cameraorigin, model_voxel_t *voxel, unsigned int desired_shader)
{
	/*
		TODO: move to own file?
		TODO: voxels with alpha channel should be z-sorted
		TODO: drawing voxels and transparency... integrate with CL_DrawEntities/do triangle batching and sorting?
		TODO: lots of this is system (implementation) specific data, we should not have them in host_voxels.c
		TODO: VERY SLOW (optimise trimeshes, render distance, etc)
	*/
	int i;
	int voxellist_size = 0;

#ifdef HARDWARE_OCCLUSION_QUERIES_IGNORE_VOXELS
	if (desired_shader == SHADER_DEPTHSTORE && current_occlusion_data) /* if we are testing for occlusion, ignore voxels (they are too much and the drawing is trivial) */
		return;
#endif

	Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL); /* TODO: make possible to use fixed lights? */

	for (i = 0; i < voxel->num_chunks; i++)
		if (voxel->chunklist[i]->vbo_id != -1)
		{
			vec3_t center;
			Math_Vector3ScaleAdd(voxel->chunklist[i]->mins, -1, voxel->chunklist[i]->maxs, center);
			Math_Vector3ScaleAdd(cameraorigin, -1, center, center);
			/* voxeldists[i] = (vec_t)Math_Vector3Length(center); */
			voxeldists[i] = (vec_t)Math_Vector3LengthSquared(center); /* no need to sqrt, we will only compare between themselves */

			sortedvoxels[voxellist_size++] = i;
		}

	/* sort TODO: test */
	if (video_3d_alpha_pass)
		qsort(sortedvoxels, voxellist_size, sizeof(sortedvoxels[0]), voxel_zcmpinv);
	else
		qsort(sortedvoxels, voxellist_size, sizeof(sortedvoxels[0]), voxel_zcmp);

	for (i = 0; i < voxellist_size; i++)
	{
		if (voxel->chunklist[sortedvoxels[i]]->vbo_id != -1)
		{
			int occluded, draw_calls_issued = false;
			if (Sys_Video3DFrustumCullingTestAABB(null_vec3, null_vec3, voxel->chunklist[sortedvoxels[i]]->mins, voxel->chunklist[sortedvoxels[i]]->maxs) == FRUSTUM_CULL_OUTSIDE)
				continue;
			occluded = Sys_Video3DCacheOcclusionTestStart(sortedvoxels[i], true);
			if (!occluded || (occluded && draw_occluded))
				if (Sys_VideoDrawVBO(voxel->chunklist[sortedvoxels[i]]->vbo_id, ((texture_t *)voxel->texture)->cl_id, -1, -1, -1, -1, -1, -1, -1, -1))
					draw_calls_issued = true;
			Sys_Video3DCacheOcclusionTestEnd(sortedvoxels[i], true, draw_calls_issued);
		}
	}
}

/*
===================
Sys_DrawEntities

TODO: sort by batched triangles when rendering, not here (will also be useful for z-sorting triangles)
===================
*/
static const unsigned int aabbtris[36] = { /* TODO: use triangle strips/fans */
	/* front face */
	0, 1, 2,
	0, 2, 3,
	/* back face */
	4, 5, 6,
	4, 6, 7,
	/* right face */
	1, 4, 7,
	1, 7, 2,
	/* left face */
	5, 0, 3,
	5, 3, 6,
	/* top face */
	3, 2, 7,
	3, 7, 6,
	/* bottom face */
	5, 4, 1,
	5, 1, 0
};
/* FIXME: not being reentrant is the lesser problem of these globals */
static snapshot_edict_t *entitylist;
static vec_t dists[MAX_EDICTS]; /* TODO: cache distances */
#define DYNAMIC_FAR_CONSTANT (video_3d_alpha_pass ? 0 : (vec_t)r_zfar->doublevalue) /* constant to send dynamic models farther, to let static ones draw first */
int modelindexcmp(const void * a, const void * b)
{
	/* TODO: do submodel sorting! will they be in their right positions as using the basemodels vbo? */
	return (entitylist[*(int *)a].model - entitylist[*(int *)b].model);
}
int zcmp(const void * a, const void * b)
{
	/* TODO: do submodel sorting! will they be in their right positions as using the basemodels vbo? */
	if (dists[*(int *)a] < dists[*(int *)b])
		return -1;
	if (dists[*(int *)a] > dists[*(int *)b])
		return 1;
	return 0;
}
int zcmpinv(const void * a, const void * b)
{
	/* TODO: do submodel sorting! will they be in their right positions as using the basemodels vbo? */
	if (dists[*(int *)a] > dists[*(int *)b])
		return -1;
	if (dists[*(int *)a] < dists[*(int *)b])
		return 1;
	return 0;
}
void Sys_DrawEntities(vec3_t cameraorigin, vec3_t cameraangles, snapshot_edict_t *entities, int not_from_viewent, unsigned int desired_shader)
{
	/*
		TODO: move to own file?
		TODO: triangles in models with alpha channel on the skin should be z-sorted with the entire world
		TODO: linked list to avoid useless searches through the list
	*/
	entindex_t i;
	model_t *mdl;
	snapshot_edict_t *ent;

	/* these are for sorting */
	int numvisibleents = 0;
	int visibleentsstack[MAX_EDICTS];
	entitylist = entities;

	numvisibleents = 0;

	for (i = 0; i < MAX_EDICTS; i++)
		if (entities[i].model && entities[i].active) /* TODO: create a debug cvar to control drawing of the null model for debugging purposes (because of this and of draworder/z-buffer issues with all-alpha mull models, null models SHOULD be visible! And do not send the entities with null model from the server if this debug cvar is off (because of this, both client and server will need to have the cvar set) */
		{

			/* some sorts need the distance (like when desired_shader == SHADER_DEPTHSTORE or video_3d_alpha_pass == true */
			/* TODO: check if it's really better to sort by z in this case (vbo changes may be costly) */
			/* TODO: some of them may be in absolute coordinates. Use maxs - mins instead of origin then  */
			/* TODO: if an indoor big map with BSP, ignore sorting and draw it first/last (depending on occasion)? I did this DYNAMIC_FAR_CONSTANT because of this, but it's a hack */
			vec3_t dist;
			Math_Vector3ScaleAdd(cameraorigin, -1, entities[i].origin, dist);
			/* dists[i] = (vec_t)Math_Vector3Length(dist) + (Sys_ModelIsStatic(cls.precached_models[entities[i].model]->data) ? 0 : DYNAMIC_FAR_CONSTANT); /* TODO: wtf, referencing cls */
			/* no need to sqrt, we will only compare between themselves */
			dists[i] = (vec_t)Math_Vector3LengthSquared(dist) + (Sys_ModelIsStatic(cls.precached_models[entities[i].model]->data) ? 0 : DYNAMIC_FAR_CONSTANT * DYNAMIC_FAR_CONSTANT); /* TODO: wtf, referencing cls */
			/* DYNAMIC_FAR_CONSTANT probably not used right, should just do two passes */

			visibleentsstack[numvisibleents++] = i;
		}

		/* TODO CONSOLEDEBUG Sys_Printf("received %d entities\n", numvisibleents); */

	/* sort TODO: test */
	if (desired_shader == SHADER_DEPTHSTORE)
		qsort(visibleentsstack, numvisibleents, sizeof(visibleentsstack[0]), zcmp);
	else if (video_3d_alpha_pass)
		qsort(visibleentsstack, numvisibleents, sizeof(visibleentsstack[0]), zcmpinv);
	else
		qsort(visibleentsstack, numvisibleents, sizeof(visibleentsstack[0]), modelindexcmp);

	for (i = 0; i < numvisibleents; i++)
	{
		/* won't draw our own model, will draw the model of the viewent as a viewmodel, except when rendering from other points of view TODO: make both one in the future */
		/* TODO: solution for viewmodel's poking through walls! Making them small will make shadows not cast well on them, messing with the depth buffer and depth range may also mess with shadow casting on them... */
		/* TODO: self model can shadow self viewmodel */
		/* TODO: do not draw if shadow's model? project shadow's model? fullbright if shadow's model? */
		if (visibleentsstack[i] == cls.prediction_snapshot.my_ent && !not_from_viewent) /* TODO: wtf, referencing cls */
			continue;
		if (visibleentsstack[i] == cls.prediction_snapshot.cameraent && not_from_viewent) /* TODO: wtf, referencing cls */
			continue;

		/* TODO: stupid system I created: if cls.lastmessage is zero (rollover, starting point, etc) EVERY BLANK ENTITY WILL BE RENDERED!!! */
		/* TODO: no need to check lastreceived here since it was checked before addint to bisibleentsstack */
		if (entities[visibleentsstack[i]].active) /* only draw if it was included in the last packet */
		{
			ent = &entities[visibleentsstack[i]];
			mdl = cls.precached_models[ent->model]; /* TODO: wtf, referencing cls */

			if (!mdl || !mdl->active)
				Sys_Error("Trying to draw an inactive model for entity %d\n", i);

			/* TODO: determine visible surfaces, frustum culling */
			/* TODO: sort by shader, then vbo, texture */
			/* TODO: do not include occluded stuff in the qsort */
			vec3_t mins, maxs;
			int occluded, draw_calls_issued = false;

			/* TODO: testing only frame 0! store maximum AABB of all frames to avoid doing too many tests??? (warning: sometimes base frames have COMPLETELY different poses / also, calculating the biggest aabb of all the animation slot frames/interpolated frames may not be enough with additive animation and blended animations (AABB will be too big/too small, respectively)) */
			Sys_ModelAABB(mdl->data, ent->frame[ANIMATION_SLOT_ALLJOINTS], mins, maxs);
			if (Sys_Video3DFrustumCullingTestAABB(ent->origin, ent->angles, mins, maxs) == FRUSTUM_CULL_OUTSIDE)
				continue;

			occluded = Sys_Video3DCacheOcclusionTestStart(visibleentsstack[i], false);
			if (!occluded || (occluded && draw_occluded))
			{
#ifdef HARDWARE_OCCLUSION_QUERIES_TEST_ONLY_AABB_FOR_DYNAMIC_MODELS
				if (!Sys_ModelIsStatic(mdl->data) && desired_shader == SHADER_DEPTHSTORE) /* TODO: other cases where we want to decide if we draw the full model or the aabb */
				{
					vec_t ent_modelmatrix[16];
					int depth = depthmask_cache;
					model_vertex_t verts[8];

					verts[0].origin[0] = mins[0]; verts[0].origin[1] = mins[1]; verts[0].origin[2] = maxs[2];
					verts[1].origin[0] = maxs[0]; verts[1].origin[1] = mins[1]; verts[1].origin[2] = maxs[2];
					verts[2].origin[0] = maxs[0]; verts[2].origin[1] = maxs[1]; verts[2].origin[2] = maxs[2];
					verts[3].origin[0] = mins[0]; verts[3].origin[1] = maxs[1]; verts[3].origin[2] = maxs[2];
					verts[4].origin[0] = maxs[0]; verts[4].origin[1] = mins[1]; verts[4].origin[2] = mins[2];
					verts[5].origin[0] = mins[0]; verts[5].origin[1] = mins[1]; verts[5].origin[2] = mins[2];
					verts[6].origin[0] = mins[0]; verts[6].origin[1] = maxs[1]; verts[6].origin[2] = mins[2];
					verts[7].origin[0] = maxs[0]; verts[7].origin[1] = maxs[1]; verts[7].origin[2] = mins[2];

					if (depth)
						Sys_VideoSetDepthMask(false, false);

					Math_MatrixModel4x4(ent_modelmatrix, ent->origin, ent->angles, NULL);
					Sys_VideoTransformFor3DModel(ent_modelmatrix);
					Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL); /* TODO: make possible to use fixed lights? */
					/* TODO: only disable cull if the view is inside the aabb (or, in that case, just make it as if the entity passed the occlusion query */
					glDisable(GL_CULL_FACE);
					if (Sys_VideoDraw3DTriangles(verts, 8, true, false, false, false, false, aabbtris, 36, -1, -1, -1, -1, -1, 0))
						draw_calls_issued = true;
					glEnable(GL_CULL_FACE);

					if (depth)
						Sys_VideoSetDepthMask(true, false);
				}
				else
				{
#endif
					draw_calls_issued = Sys_VideoDraw3DModel(mdl->data, cameraorigin, cameraangles, ent->origin, ent->angles, ent->anim_pitch, ent->frame, visibleentsstack[i], desired_shader);
#ifdef HARDWARE_OCCLUSION_QUERIES_TEST_ONLY_AABB_FOR_DYNAMIC_MODELS
				}
#endif
			}

			Sys_Video3DCacheOcclusionTestEnd(visibleentsstack[i], false, draw_calls_issued);
		}
	}
}

/*
===================
Sys_DrawParticles

TODO: optimize this
TODO: what about orientation? and shadow casting? (needs to orient to the light, remember that we only have front faces)
TODO: culling (frustum and occlusion query)
===================
*/
#define PARTICLE_HALF_SIZE		0.015625f
void Sys_DrawParticles(vec3_t forward, vec3_t right, vec3_t up, particle_t *particles, unsigned int desired_shader)
{
	particle_t *cur;
	model_vertex_t verts[4];
	unsigned int indices[4];

	Sys_VideoBindShaderProgram(desired_shader, NULL, NULL, NULL); /* TODO: make possible to use fixed lights? */

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 3;
	indices[3] = 2;

	while (particles->next)
	{
		cur = particles->next;

		verts[0].origin[0] = cur->org[0] - right[0] * PARTICLE_HALF_SIZE * cur->scale - up[0] * PARTICLE_HALF_SIZE * cur->scale;
		verts[0].origin[1] = cur->org[1] - right[1] * PARTICLE_HALF_SIZE * cur->scale - up[1] * PARTICLE_HALF_SIZE * cur->scale;
		verts[0].origin[2] = cur->org[2] - right[2] * PARTICLE_HALF_SIZE * cur->scale - up[2] * PARTICLE_HALF_SIZE * cur->scale;

		verts[1].origin[0] = cur->org[0] + right[0] * PARTICLE_HALF_SIZE * cur->scale - up[0] * PARTICLE_HALF_SIZE * cur->scale;
		verts[1].origin[1] = cur->org[1] + right[1] * PARTICLE_HALF_SIZE * cur->scale - up[1] * PARTICLE_HALF_SIZE * cur->scale;
		verts[1].origin[2] = cur->org[2] + right[2] * PARTICLE_HALF_SIZE * cur->scale - up[2] * PARTICLE_HALF_SIZE * cur->scale;

		verts[2].origin[0] = cur->org[0] + right[0] * PARTICLE_HALF_SIZE * cur->scale + up[0] * PARTICLE_HALF_SIZE * cur->scale;
		verts[2].origin[1] = cur->org[1] + right[1] * PARTICLE_HALF_SIZE * cur->scale + up[1] * PARTICLE_HALF_SIZE * cur->scale;
		verts[2].origin[2] = cur->org[2] + right[2] * PARTICLE_HALF_SIZE * cur->scale + up[2] * PARTICLE_HALF_SIZE * cur->scale;

		verts[3].origin[0] = cur->org[0] - right[0] * PARTICLE_HALF_SIZE * cur->scale + up[0] * PARTICLE_HALF_SIZE * cur->scale;
		verts[3].origin[1] = cur->org[1] - right[1] * PARTICLE_HALF_SIZE * cur->scale + up[1] * PARTICLE_HALF_SIZE * cur->scale;
		verts[3].origin[2] = cur->org[2] - right[2] * PARTICLE_HALF_SIZE * cur->scale + up[2] * PARTICLE_HALF_SIZE * cur->scale;

		memcpy(verts[0].color, cur->color, sizeof(unsigned char) * 4);
		memcpy(verts[1].color, cur->color, sizeof(unsigned char) * 4);
		memcpy(verts[2].color, cur->color, sizeof(unsigned char) * 4);
		memcpy(verts[3].color, cur->color, sizeof(unsigned char) * 4);

		verts[0].normal[0] = -forward[0];
		verts[0].normal[1] = -forward[1];
		verts[0].normal[2] = -forward[2];

		verts[1].normal[0] = -forward[0];
		verts[1].normal[1] = -forward[1];
		verts[1].normal[2] = -forward[2];

		verts[2].normal[0] = -forward[0];
		verts[2].normal[1] = -forward[1];
		verts[2].normal[2] = -forward[2];

		verts[3].normal[0] = -forward[0];
		verts[3].normal[1] = -forward[1];
		verts[3].normal[2] = -forward[2];

		Sys_VideoDraw3DTriangles(verts, 4, true, true, false, false, true, indices, 4, texture_nulltransparent->cl_id, -1, -1, -1, -1, 1);

		particles = particles->next;
	}
}

/*
===================
Sys_DrawObjects

This function draws everything in the 3d world
"not_from_viewent" will make this client's model be drawn instead of the viewent's model

TODO: drawing all opaque with z-buffer, them drawing all transparent with z-test no-write is enough? NO, need to sort
TODO: that is faster? for (object) for (light) OR for (light) for (object)?
TODO: z-sort for more efficient occlusion queries? draw scenery first (bsp models should be already OK because of pvs, but I do a SORT on the faces per texture, so probably won't be anymore)
TODO: lot's of calculations are done TWICE for different value of video_3d_alpha_pass (and for EACH PASS!) inside drawing functions (matrices, sorting, etc) if the entity won't be drawn in this pass or if it was culled by frustum or occlusion!
===================
*/
void Sys_DrawObjects(vec3_t cameraorigin, vec3_t cameraangles, snapshot_edict_t *entities, particle_t *particles, model_voxel_t *voxel, int not_from_viewent, unsigned int desired_shader)
{
	/* TODO: this takes alpha texture in account, but still we need to take into account alpha in the color uniform/vertex colors and z-sorting. Or do depth peeling or something like that for intersecting geometry. */
	for (video_3d_alpha_pass = 0; video_3d_alpha_pass < 2; video_3d_alpha_pass++)
	{
#if 0 /* disabled this code to allow occlusion queries in transparent textures, even if not writing to the depth buffer */
		if (desired_shader == SHADER_DEPTHSTORE && video_3d_alpha_pass) /* TODO: currently transparent stuff do not write to the depth buffer */
			break;
#endif

		if (desired_shader == SHADER_DEPTHSTORE)
			glColorMask(0, 0, 0, 0);
		if (video_3d_alpha_pass)
			Sys_VideoSetDepthMask(false, false);

		if (voxel)
		{
			Sys_VideoTransformFor3DModel(NULL);
			Sys_DrawVoxels(cameraorigin, voxel, desired_shader);
		}
		if (entities)
		{
			Sys_DrawEntities(cameraorigin, cameraangles, entities, not_from_viewent, desired_shader);
			Sys_VideoTransformFor3DModel(NULL);
		}
		if (particles && desired_shader == SHADER_LIGHTMAPPING) /* particles won't be affected by any dynamic lights TODO: make them be, also, depending on the particle type, would ever drawing the AABB first be needed? */
		{
			vec3_t up;
			vec3_t right;
			vec3_t forward;

			Math_AnglesToVec(cameraangles, forward, right, up);
			Sys_DrawParticles(forward, right, up, particles, SHADER_PARTICLE);
		}

		if (video_3d_alpha_pass)
			Sys_VideoSetDepthMask(true, false); /* reset it */
		if (desired_shader == SHADER_DEPTHSTORE)
			glColorMask(1, 1, 1, 1); /* reset it */
	}
	video_3d_alpha_pass = false;
}

/*
===================
Sys_Draw3DLightShadowPass

Rendering pass for one light source, casting shadows
TODO: do not render when the entity/model/mesh/etc origin + mins/max is outside of the light radius
TODO: draw a zfar plane first, to optimze early-z and to avoid objects outside of the light's radius being lit.
TODO: make the entity emiting the light glow/fullbright/etc
===================
*/
void Sys_Draw3DLightShadowPass(vec3_t cameraorigin, vec3_t cameraangles, void *entities_ptr, void *particles_ptr, void *voxel_ptr, vec_t *light_pos, entindex_t *light_ent, int *light_visiblesides)
{
	int i, light;

	/*
		-------------------
		LIGHT VIEW RENDERING
		-------------------
	*/
	/* TODO: calculate zfar for light_range with respect to the light intensity (ZFAR and intensity 1:1 is wrong) because it may get us illumination without shadows if the value is too low */
	/* TODO: see if the zfar is right */
	Sys_VideoSet3D(cameraorigin, cameraangles, (vec_t)r_zfar->doublevalue, false); /* TODO: doing unnecessary operations here? we need the viewmatrix for the next one */
	Sys_ShadowUpdateLightMatrices(cameraorigin, cameraangles, light_pos, light_intensity, false);
	/* in case we render the shadowmap to a different resolution, the viewport must be modified accordingly. */
	glViewport(0, 0, shadowmap_width, shadowmap_height);
	/* to avoid self-shadowing */
	glPolygonOffset(1.1f, 40);
	glEnable(GL_POLYGON_OFFSET_FILL);

	for (light = 0; light < SHADER_MAX_LIGHTS; light++)
	{
		/* first step: render from the light POV to a FBO, store depth in a cubemap */
		glBindFramebuffer(GL_FRAMEBUFFER, sm_fbo_id[light]); /* rendering offscreen */

		if (light_intensity[light])
			Math_MatrixCopy4x4(projectionmatrix, light_projection_matrix[light]);
		for (i = 0; i < 6; ++i)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, sm_depth_texturecube_id[light], 0);
			glClear(GL_DEPTH_BUFFER_BIT);

			if (!(light_visiblesides[light] & (1 << i))) /* side not visible */
				continue;

			Math_MatrixMultiply4x4(viewmatrix, light_face_matrix[i], light_view_matrix[light]);

			Sys_VideoUpdateUniform(SHADER_UNIFORM_VIEWMATRIX, viewmatrix, 1);

			current_frustum = &light_cache[light_ent[light]][i].frustum;
			current_light_kdop = &light_cache[light_ent[light]][i].lightkdop;
			current_occlusion_data = NULL; /* no need to start occlusion queries since we won't draw anything else after the depthstore */

			/* TODO: only draw shadow casters, what about transparent entities, etc */
			/* TODO FIXME: sending cameraangles to omnidirectional lights? (for sprite models and particles... so that they keep looking at the camera for shadows - not enought because they only have front faces) */
			/* TODO FIXME: light point of view may cull some faces from a bsp model via visdata, making the shadows look wrong, we need PORTAL VIS for this to work? rendering without vis is too costly */
			/* TODO: one of the sides of the cubemap may cull one of the faces blocking it and bleed through it! (easy to see: Y- just above a ceiling - it cuts the ceiling) */
			/* TODO: ignore drawing the model (from the light point of view only) from the entities that emit light (which comes from their origin)? */
			Sys_DrawObjects(&light_pos[light * 3], cameraangles, (snapshot_edict_t *)entities_ptr, (particle_t *)particles_ptr, (model_voxel_t *)voxel_ptr, true, SHADER_DEPTHSTORE);
		}
	}

	/*
		-------------------
		CAMERA VIEW RENDERING
		-------------------
	*/

	/* Now rendering from the camera point of view, using the FBO to generate shadows */
	glBindFramebuffer(GL_FRAMEBUFFER, cam_fbo_id);
	glViewport(0, 0, video_width, video_height);

	Sys_VideoSet3D(cameraorigin, cameraangles, (vec_t)r_zfar->doublevalue, false);
	current_frustum = &primary_render_frustum;
	current_light_kdop = NULL;
	current_occlusion_data = &render_occlusion_data;
	glPolygonOffset(0.0, 0.0);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Sys_BindTexture(sm_depth_texturecube_id[0], GL_TEXTURE2);
	Sys_BindTexture(sm_depth_texturecube_id[1], GL_TEXTURE6);
	Sys_BindTexture(sm_depth_texturecube_id[2], GL_TEXTURE7);
	Sys_BindTexture(sm_depth_texturecube_id[3], GL_TEXTURE8);
	Sys_BindTexture(sm_depth_texturecube_id[4], GL_TEXTURE9);
	Sys_BindTexture(sm_depth_texturecube_id[5], GL_TEXTURE10);
	Sys_BindTexture(sm_depth_texturecube_id[6], GL_TEXTURE11);
	Sys_BindTexture(sm_depth_texturecube_id[7], GL_TEXTURE12);
	Sys_DrawObjects(cameraorigin, cameraangles, (snapshot_edict_t *)entities_ptr, (particle_t *)particles_ptr, (model_voxel_t *)voxel_ptr, (cls.prediction_snapshot.cameraent == cls.prediction_snapshot.viewent) ? false : true, SHADER_SHADOWMAPPING); /* TODO: wtf, referencing cls */
}

/*
===================
Sys_Draw3DLightNoShadowPass

Rendering pass for one light source, without casting shadows.
Because of this, the intensity for these type of lights should be kept small - normals are the
only thing blocking them.
TODO: do not render when the entity/model/mesh/etc origin + mins/max is outside of the light radius
TODO: make the entity emiting the light glow/fullbright/etc
===================
*/
void Sys_Draw3DLightNoShadowPass(vec3_t cameraorigin, vec3_t cameraangles, void *entities_ptr, void *particles_ptr, void *voxel_ptr, vec_t *light_pos)
{
	/*
		-------------------
		CAMERA VIEW RENDERING
		-------------------
	*/

	/* Now rendering from the camera point of view, using the FBO */
	glBindFramebuffer(GL_FRAMEBUFFER, cam_fbo_id);
	glViewport(0, 0, video_width, video_height);

	Sys_VideoSet3D(cameraorigin, cameraangles, (vec_t)r_zfar->doublevalue, false);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Sys_ShadowUpdateLightMatrices(cameraorigin, cameraangles, light_pos, light_intensity, true);
	Sys_DrawObjects(cameraorigin, cameraangles, (snapshot_edict_t *)entities_ptr, (particle_t *)particles_ptr, (model_voxel_t *)voxel_ptr, (cls.prediction_snapshot.cameraent == cls.prediction_snapshot.viewent) ? false : true, SHADER_LIGHTING_NO_SHADOWS); /* TODO: wtf, referencing cls */
}

/*
===================
Sys_Draw3DStaticLightmapPass

Rendering pass for static lightmaps

TODO: for entities, use lightmaps from the world entity (entity[0], model[1]) lightgrid or project down lightmap
TODO: voxel static lighting
TODO: problem: too static, moving entities like doors do not affect the lightmap - use dynamic lights with shadows if you want these effects and leave these areas unlit by static lightmaps
===================
*/
void Sys_Draw3DStaticLightmapPass(vec3_t cameraorigin, vec3_t cameraangles, void *entities_ptr, void *particles_ptr, void *voxel_ptr)
{
	Sys_DrawObjects(cameraorigin, cameraangles, (snapshot_edict_t *)entities_ptr, (particle_t *)particles_ptr, (model_voxel_t *)voxel_ptr, (cls.prediction_snapshot.cameraent == cls.prediction_snapshot.viewent) ? false : true, SHADER_LIGHTMAPPING); /* TODO: wtf, referencing cls */
}

/*
===================
Sys_Draw3DBegin

Sets up basic stuff and do a depth pass
===================
*/
void Sys_Draw3DBegin(vec3_t cameraorigin, vec3_t cameraangles, void *entities_ptr, void *particles_ptr, void *voxel_ptr)
{
	GLint viewport[4] = {0, 0, video_width, video_height};
	/*
		-------------------
		CAMERA VIEW RENDERING
		-------------------
	*/
	/* Now rendering from the camera point of view, using the FBO */
	glBindFramebuffer(GL_FRAMEBUFFER, cam_fbo_id);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Sys_VideoSet3D(cameraorigin, cameraangles, (vec_t)r_zfar->doublevalue, true);
	/* create the unprojection matrixes from the main view */
	Sys_Video3DPointAndDirectionFromUnitXYSetMatrixes(true, projectionmatrix, viewmatrix, viewport);

	/* first do a depth-only render, to make the next passes faster TODO: save the depth from this pass and use for all camera drawings? */
	Sys_DrawObjects(cameraorigin, cameraangles, (snapshot_edict_t *)entities_ptr, (particle_t *)particles_ptr, (model_voxel_t *)voxel_ptr, (cls.prediction_snapshot.cameraent == cls.prediction_snapshot.viewent) ? false : true, SHADER_DEPTHSTORE); /* TODO: wtf, referencing cls */

	/* special view matrix without translation for the skybox - draw after the depth pass for early Z */
	Sys_VideoSet3D(null_vec3, cameraangles, (vec_t)r_zfar->doublevalue, false);
	Sys_DrawSkybox(SHADER_SKYBOX);
}

/*
===================
Sys_Draw3DTestLights

Tests lights for visibility of their lighted area
TODO: the frusta that we render will pass through walls and set the lights as visible
===================
*/
void Sys_Draw3DTestLights(vec_t *point_light_pos, vec_t *point_light_intensities, entindex_t *point_light_ent, int num_lights)
{
	int light, i;
	vec3_t center;

	for (light = 0; light < num_lights; light++)
	{
		if (!point_light_intensities[light])
			continue;

		for (i = 0; i < 6; i++)
		{
			Math_Vector3Add(&point_light_pos[light * 3], light_faces_forward[i], center);
			Sys_VideoCacheCreateLight(point_light_ent[light], i, &point_light_pos[light * 3], center, light_faces_up[i], SHADOW_FOV_Y, SHADOW_ASPECT, (vec_t)SHADOW_NEAR, point_light_intensities[light] * point_light_intensities[light], &primary_render_frustum);
			/* TODO: if cache updated, retest anyway? If doing it, be careful that if the light is always moving and testing was very slow, it will NEVER get culled and will always retest */
			if (light_occlusion_data[point_light_ent[light]].light_frustum_occlusion_state[i] != ENTITY_OCCLUSION_STATE_WAITING)
			{
				light_occlusion_data[point_light_ent[light]].light_frustum_occlusion_state[i] = ENTITY_OCCLUSION_STATE_WAITING;
				glBeginQuery(GL_ANY_SAMPLES_PASSED, light_occlusion_data[point_light_ent[light]].query_light_frustum_id[i]);
				Sys_VideoCacheRenderLightFrustum(point_light_ent[light], i, NULL, NULL);
				glEndQuery(GL_ANY_SAMPLES_PASSED);
			}
		}
	}
}

/*
===================
Sys_Draw3DFrame

This is the refresh for the main camera view
TODO: fix light bleeding for real? still happens?
TODO: make it configurable which passes to do (and have "backup" lights in case one disables the static lightmaps)
TODO: combine more than one point-light-no-shadows per pass? how does GL_LIGHTs work?
TODO: alpha blending NEEDS z-sorting or other technique like depth peeling
TODO: adjust lights field of view to enclose the view frustum and have better coverage to reduce jagged edges in shadows
TODO: one of the cubemap shadows not working with voxels?
TODO: WARN MAPPERS/NOTE INTO MAPPING DOCUMENT: while the light frusta draw through walls for being too big and having a quadratic zfar (can these be fixed?), DO NOT put very big intensities in lights, just ENOUGH!
TODO: with a software depth buffer:
depth pass

se aabb frustrum check falhar, retorna NAODESENHA
se no frustum, desenha aabb SEM DEPTH WRITE
se desenhar aabb falhar, retorna NAODESENHA
desenha normal, COM DEPTH WRITE

normal pass: desenha se não NAODESENHA
===================
*/
#define MAX_CONCURRENT_LIGHTS	(SHADER_MAX_LIGHTS * 8) /* TODO: cvar for this */
/* TODO: not thread safe */
vec_t point_light_pos_noshadow[MAX_CONCURRENT_LIGHTS * 3];
vec_t point_light_intensities_noshadow[MAX_CONCURRENT_LIGHTS];
entindex_t point_light_ent_noshadow[MAX_CONCURRENT_LIGHTS];
vec_t point_light_pos_shadow[MAX_CONCURRENT_LIGHTS * 3];
vec_t point_light_intensities_shadow[MAX_CONCURRENT_LIGHTS];
entindex_t point_light_ent_shadow[MAX_CONCURRENT_LIGHTS];
void Sys_Draw3DFrame(vec3_t cameraorigin, vec3_t cameraangles, void *entities_ptr, void *particles_ptr, void *voxel_ptr)
{
	int num_lights_noshadow = 0;
	int num_lights_shadow = 0;
	int i, lightpass, curlight, cubemapside, sidesvisible;
	frustum_culling_data_t *frustumsave;
	vec_t *curlight_noshadow = point_light_pos_noshadow;
	vec_t *curlight_shadow = point_light_pos_shadow;
	entindex_t ent;
	int visents = 0;

	for (ent = 0; ent < MAX_EDICTS; ent++)
	{
		snapshot_edict_t *curent = &((snapshot_edict_t *)entities_ptr)[ent];

		if (curent->active)
			visents++;
		/* TODO: if too many lights, give priority to the closest and/or brightest ones. Maybe set light uniforms on the closest/brighest PER SURFACE! */
		if (num_lights_noshadow == MAX_CONCURRENT_LIGHTS)
		{
			Sys_Printf("Sys_Draw3DFrame: too many lights with no shadow (more than %d)\n", num_lights_noshadow);
			break;
		}
		if (num_lights_shadow == MAX_CONCURRENT_LIGHTS)
		{
			Sys_Printf("Sys_Draw3DFrame: too many lights with shadow (more than %d)\n", num_lights_shadow);
			break;
		}
		if (curent->active && curent->light_intensity) /* only select if it was included in the last packet and is non-zero */
		{
			if (curent->light_intensity > 0)
			{
				Math_Vector3Copy(curent->origin, curlight_shadow);
				point_light_intensities_shadow[num_lights_shadow] = curent->light_intensity;
				point_light_ent_shadow[num_lights_shadow] = ent;
				num_lights_shadow++;
				curlight_shadow += 3;
			}
			else
			{
				Math_Vector3Copy(curent->origin, curlight_noshadow);
				point_light_intensities_noshadow[num_lights_noshadow] = -(curent->light_intensity); /* negative intensities mean no shadows */
				point_light_ent_noshadow[num_lights_noshadow] = ent;
				num_lights_noshadow++;
				curlight_noshadow += 3;
			}
		}
	}
	/* TODO CONSOLEDEBUG Sys_Printf("%d visible entities\n", visents); */

	/* set up and do a depth pass */
	draw_occluded = true;
	Sys_Draw3DBegin(cameraorigin, cameraangles, entities_ptr, particles_ptr, voxel_ptr);
	draw_occluded = false;

	Sys_VideoSet3D(cameraorigin, cameraangles, (vec_t)r_zfar->doublevalue, false);
	/* test lights for visibility */
	if (num_lights_noshadow || num_lights_shadow)
	{
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		Sys_VideoSetDepthMask(false, false);
		Sys_VideoBindShaderProgram(SHADER_DEPTHSTORE, NULL, NULL, NULL);
		Sys_Draw3DTestLights(point_light_pos_noshadow, point_light_intensities_noshadow, point_light_ent_noshadow, num_lights_noshadow);
		Sys_Draw3DTestLights(point_light_pos_shadow, point_light_intensities_shadow, point_light_ent_shadow, num_lights_shadow);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		Sys_VideoSetDepthMask(true, false);
	}
	/* draw static lights */
	Sys_Draw3DStaticLightmapPass(cameraorigin, cameraangles, entities_ptr, particles_ptr, voxel_ptr);

	/* now will blend the pass into the screen FIXME: will blend with anything that is already on the screen */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	Sys_VideoSet2D(false);
	Sys_VideoBindShaderProgram(SHADER_2D, NULL, NULL, NULL);
	glBlendFunc(GL_ONE, GL_ONE); /* TODO: is this the best blend mode for this? */
	Sys_VideoDraw2DPic(&cam_color_texture_id, video_width, -video_height, 0, video_height); /* reverse y-coordinates */

	/* draw dynamic light + shadows TODO: too many lights in a huge voxel world causes MASSIVE slowdowns */
	curlight = 0;
	frustumsave = current_frustum;
	for (lightpass = 0; lightpass <= num_lights_noshadow / SHADER_MAX_LIGHTS; lightpass++)
	{
		entindex_t light_ent[SHADER_MAX_LIGHTS];
		vec3_t light_pos[SHADER_MAX_LIGHTS];
		int light_visiblesides[SHADER_MAX_LIGHTS]; /* bit map */
		for (i = 0; i < SHADER_MAX_LIGHTS; i++)
		{
			light_visiblesides[i] = 0; /* done iteratively */
			if (curlight == num_lights_noshadow)
			{
				light_intensity[i] = 0;
				light_ent[i] = 0;
				Math_ClearVector3(light_pos[i]);
				continue;
			}
			sidesvisible = 0;
			for (cubemapside = 0; cubemapside < 6; cubemapside++)
			{
				light_scene_occlusion_t *curlight_odata = &light_occlusion_data[point_light_ent_noshadow[curlight]];
				frustum_culling_data_t *curlight_frustum = &light_cache[point_light_ent_noshadow[curlight]][cubemapside].frustum;
				if (curlight_odata->light_frustum_occlusion_state[cubemapside] == ENTITY_OCCLUSION_STATE_WAITING)
				{
					GLuint passed = INT_MAX;
					GLuint available = 0;

					glGetQueryObjectuiv(curlight_odata->query_light_frustum_id[cubemapside], GL_QUERY_RESULT_AVAILABLE, &available);

					if(available)
					{
						passed = 0;
						glGetQueryObjectuiv(curlight_odata->query_light_frustum_id[cubemapside], GL_QUERY_RESULT, &passed);
						curlight_odata->light_frustum_occlusion_state[cubemapside] = (passed) ? ENTITY_OCCLUSION_STATE_VISIBLE : ENTITY_OCCLUSION_STATE_HIDDEN;
						curlight_odata->light_frustum_occluded[cubemapside] = (passed) ? false : true;
					}
				}

				/* if view is inside the light, assume visible TODO: see if this radius is enough! probably will only reach the CENTER of the screen, but the four corners will be farther always than this radius. Also the same visibility problem as when rendering: light may be blocked and we still set it as visible because the frustum spans across walls. */
				current_frustum = curlight_frustum;
				if (Sys_Video3DFrustumCullingTestSphere(cameraorigin, (vec_t)r_znear->doublevalue) != FRUSTUM_CULL_OUTSIDE)
				{
					curlight_odata->light_frustum_occlusion_state[cubemapside] = ENTITY_OCCLUSION_STATE_VISIBLE; /* stop waiting too */
					curlight_odata->light_frustum_occluded[cubemapside] = false;
				}

				if (!curlight_odata->light_frustum_occluded[cubemapside])
				{
					sidesvisible++;
					light_visiblesides[i] += 1 << cubemapside;
				}
			}
			/* TODO CONSOLEDEBUG Sys_Printf("light noshadows %d, %d sides visible (intensity %f)\n", point_light_ent_noshadow[curlight], sidesvisible, point_light_intensities_noshadow[curlight]); */
			if (!sidesvisible)
			{
				curlight++;
				i--;
				continue; /* occluded */
			}

			light_intensity[i] = point_light_intensities_noshadow[curlight];
			light_ent[i] = point_light_ent_noshadow[curlight];
			Math_Vector3Copy(&point_light_pos_noshadow[curlight * 3], light_pos[i]);
			curlight++;
		}
		if (!light_intensity[0]) /* nothing in this pack */
			break;
		current_frustum = frustumsave;
		Sys_Draw3DLightNoShadowPass(cameraorigin, cameraangles, entities_ptr, particles_ptr, voxel_ptr, light_pos[0]);

		/* now will blend the pass into the screen FIXME: will blend with anything that is already on the screen */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		Sys_VideoSet2D(false);
		Sys_VideoBindShaderProgram(SHADER_2D, NULL, NULL, NULL);
		glBlendFunc(GL_ONE, GL_ONE); /* TODO: is this the best blend mode for this? */
		Sys_VideoDraw2DPic(&cam_color_texture_id, video_width, -video_height, 0, video_height); /* reverse y-coordinates */
		if (!light_intensity[i - 1]) /* all empty after this pack */
			break;
	}
	curlight = 0;
	frustumsave = current_frustum;
	for (lightpass = 0; lightpass <= num_lights_shadow / SHADER_MAX_LIGHTS; lightpass++)
	{
		entindex_t light_ent[SHADER_MAX_LIGHTS];
		vec3_t light_pos[SHADER_MAX_LIGHTS];
		int light_visiblesides[SHADER_MAX_LIGHTS]; /* bit map */
		for (i = 0; i < SHADER_MAX_LIGHTS; i++)
		{
			light_visiblesides[i] = 0; /* done iteratively */
			if (curlight == num_lights_shadow)
			{
				light_intensity[i] = 0;
				light_ent[i] = 0;
				Math_ClearVector3(light_pos[i]);
				continue;
			}
			sidesvisible = 0;
			for (cubemapside = 0; cubemapside < 6; cubemapside++)
			{
				light_scene_occlusion_t *curlight_odata = &light_occlusion_data[point_light_ent_shadow[curlight]];
				frustum_culling_data_t *curlight_frustum = &light_cache[point_light_ent_shadow[curlight]][cubemapside].frustum;
				if (curlight_odata->light_frustum_occlusion_state[cubemapside] == ENTITY_OCCLUSION_STATE_WAITING)
				{
					GLuint passed = INT_MAX;
					GLuint available = 0;

					glGetQueryObjectuiv(curlight_odata->query_light_frustum_id[cubemapside], GL_QUERY_RESULT_AVAILABLE, &available);

					if(available)
					{
						passed = 0;
						glGetQueryObjectuiv(curlight_odata->query_light_frustum_id[cubemapside], GL_QUERY_RESULT, &passed);
						curlight_odata->light_frustum_occlusion_state[cubemapside] = (passed) ? ENTITY_OCCLUSION_STATE_VISIBLE : ENTITY_OCCLUSION_STATE_HIDDEN;
						curlight_odata->light_frustum_occluded[cubemapside] = (passed) ? false : true;
					}
				}

				/* if view is inside the light, assume visible TODO: see if this radius is enough! probably will only reach the CENTER of the screen, but the four corners will be farther always than this radius. Also the same visibility problem as when rendering: light may be blocked and we still set it as visible because the frustum spans across walls. */
				current_frustum = curlight_frustum;
				if (Sys_Video3DFrustumCullingTestSphere(cameraorigin, (vec_t)r_znear->doublevalue) != FRUSTUM_CULL_OUTSIDE)
				{
					curlight_odata->light_frustum_occlusion_state[cubemapside] = ENTITY_OCCLUSION_STATE_VISIBLE; /* stop waiting too */
					curlight_odata->light_frustum_occluded[cubemapside] = false;
				}

				if (!curlight_odata->light_frustum_occluded[cubemapside])
				{
					sidesvisible++;
					light_visiblesides[i] += 1 << cubemapside;
				}
			}
			/* TODO CONSOLEDEBUG Sys_Printf("light shadows %d, %d sides visible (intensity %f)\n", point_light_ent_shadow[curlight], sidesvisible, point_light_intensities_shadow[curlight]); */
			if (!sidesvisible)
			{
				curlight++;
				i--;
				continue; /* occluded */
			}

			light_intensity[i] = point_light_intensities_shadow[curlight];
			light_ent[i] = point_light_ent_shadow[curlight];
			Math_Vector3Copy(&point_light_pos_shadow[curlight * 3], light_pos[i]);
			curlight++;
		}
		if (!light_intensity[0]) /* nothing in this pack */
			break;
		current_frustum = frustumsave;
			Sys_Draw3DLightShadowPass(cameraorigin, cameraangles, entities_ptr, particles_ptr, voxel_ptr, light_pos[0], light_ent, light_visiblesides);

		/* now will blend the pass into the screen FIXME: will blend with anything that is already on the screen */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		Sys_VideoSet2D(false);
		Sys_VideoBindShaderProgram(SHADER_2D, NULL, NULL, NULL);
		glBlendFunc(GL_ONE, GL_ONE); /* TODO: is this the best blend mode for this? */
		Sys_VideoDraw2DPic(&cam_color_texture_id, video_width, -video_height, 0, video_height); /* reverse y-coordinates */
		if (!light_intensity[i - 1]) /* all empty after this pack */
			break;
	}

	if (svs.active) /* TODO: wtf, referencing cls or svs */
	{
#if 0
		/* adjust horizontal fov while having the vertical fov fixed*/
		double real_fovx, real_fovy;
		double xmin, xmax, ymin, ymax, znear = 0.1, aspect;

		real_fovx = r_fov->doublevalue;
		real_fovy = Sys_VideoCalcFov(real_fovx, BASE_HEIGHT, BASE_WIDTH);
		real_fovx = Sys_VideoCalcFov(real_fovy, video_width, video_height);

		/* these matrix operations are not GL3+ compatible */

		/* projection matrix */
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		aspect = (double)video_width / (double)video_height;

		ymax = znear * tan(real_fovy * M_PI / 360.0);
		ymin = -ymax;

		xmin = ymin * aspect;
		xmax = ymax * aspect;

		glFrustum(xmin, xmax, ymin, ymax, znear, r_zfar->doublevalue);

		/* view matrix */
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glRotatef (-cameraangles[ANGLES_ROLL],  0, 0, 1);
		glRotatef (-cameraangles[ANGLES_PITCH],  1, 0, 0);
		glRotatef (-cameraangles[ANGLES_YAW],  0, 1, 0);
		glTranslatef (-cameraorigin[0], -cameraorigin[1], -cameraorigin[2]);

		Sys_VideoBindShaderProgram(-1, NULL, NULL, NULL);

		Sys_BindTexture(-1, GL_TEXTURE0);
		Sys_BindTexture(-1, GL_TEXTURE1);
		Sys_BindTexture(-1, GL_TEXTURE3);
		Sys_BindTexture(-1, GL_TEXTURE4);
		Sys_BindTexture(-1, GL_TEXTURE5);

		Sys_VideoTransformFor3DModel(NULL);

		Sys_BindVBO(-1);
		/* call physics code to draw debug stuff to the screen */
		/* TODO: make it work with core GL 3.1+ */
		Sys_PhysicsDebugDraw(svs.physworld); /* TODO: option to render the prediction world or the local server world TODO: wtf, referencing cls or svs */
#endif
	}
}

#endif /* DEDICATED_SERVER */
