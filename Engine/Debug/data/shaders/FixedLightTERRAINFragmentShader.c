#version 330 core

varying vec2 ex_texcoord0;
varying vec2 ex_texcoord1;
varying float ex_worldpos_unscaled_y;
varying vec3 ex_textureblendingproperties;
varying vec4 ex_lightresult;

uniform sampler2D u_texture0; /* low elevation */
uniform sampler2D u_texture1; /* mid elevation */
uniform sampler2D u_texture3; /* high elevation */
uniform sampler2D u_texture4; /* path */
uniform sampler2D u_texture5; /* path mask (for the entire heightmap) */

void main()
{
	float fScale = ex_worldpos_unscaled_y;
	float fRange1 = ex_textureblendingproperties.x;
	float fRange2 = ex_textureblendingproperties.x + ex_textureblendingproperties.z;
	float fRange3 = ex_textureblendingproperties.y;
	float fRange4 = ex_textureblendingproperties.y + ex_textureblendingproperties.z;
	vec4 tex_color;

	/* TODO: conditions are SLOW? */

	if(fScale <= fRange1)
		tex_color = texture2D(u_texture0, ex_texcoord0);
	else if(fScale <= fRange2)
	{
		fScale -= fRange1;
		fScale /= (fRange2 - fRange1);

		float fScale2 = fScale;
		fScale = 1.0 - fScale;

		tex_color = texture2D(u_texture0, ex_texcoord0) * fScale;
		tex_color += texture2D(u_texture1, ex_texcoord0) * fScale2;
	}
	else if(fScale <= fRange3)
		tex_color = texture2D(u_texture1, ex_texcoord0);
	else if(fScale <= fRange4)
	{
		fScale -= fRange3;
		fScale /= (fRange4 - fRange3);

		float fScale2 = fScale;
		fScale = 1.0 - fScale;

		tex_color = texture2D(u_texture1, ex_texcoord0) * fScale;
		tex_color += texture2D(u_texture3, ex_texcoord0) * fScale2;
	}
	else
		tex_color = texture2D(u_texture3, ex_texcoord0);

	vec2 vPathCoord = vec2(ex_texcoord0.x / ex_texcoord1.x, ex_texcoord0.y / ex_texcoord1.y);
	vec4 vPathIntensity = texture2D(u_texture5, vPathCoord); /* black color means there is a path */
	fScale = vPathIntensity.x; /* TODO: in what format are we uploading the texture do GPU memory? */

	vec4 vPathColor = texture2D(u_texture4, ex_texcoord0); 
	gl_FragColor = (fScale * tex_color + (1.0 - fScale) * vPathColor) * ex_lightresult;
}