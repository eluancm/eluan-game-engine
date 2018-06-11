#version 330 core

varying vec2 ex_texcoord0;
varying vec2 ex_texcoord1;
varying float ex_worldpos_unscaled_y;
varying vec3 ex_textureblendingproperties;
varying vec4 position_cs;
varying vec3 normal_cs;

uniform vec4 u_color;
uniform vec3 u_light_positions[8]; /* TODO: pass SHADER_MAX_LIGHTS as a define to the shader */
uniform float u_light_intensities[8]; /* TODO: pass SHADER_MAX_BONES as a define to the shader */
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
	gl_FragColor = vec4(0);
	vec4 texcolor;

	/* TODO: conditions are SLOW? */

	if(fScale <= fRange1)
		texcolor = texture2D(u_texture0, ex_texcoord0);
	else if(fScale <= fRange2)
	{
		fScale -= fRange1;
		fScale /= (fRange2 - fRange1);

		float fScale2 = fScale;
		fScale = 1.0 - fScale;

		texcolor = texture2D(u_texture0, ex_texcoord0) * fScale;
		texcolor += texture2D(u_texture1, ex_texcoord0) * fScale2;
	}
	else if(fScale <= fRange3)
		texcolor = texture2D(u_texture1, ex_texcoord0);
	else if(fScale <= fRange4)
	{
		fScale -= fRange3;
		fScale /= (fRange4 - fRange3);

		float fScale2 = fScale;
		fScale = 1.0 - fScale;

		texcolor = texture2D(u_texture1, ex_texcoord0) * fScale;
		texcolor += texture2D(u_texture3, ex_texcoord0) * fScale2;
	}
	else
		texcolor = texture2D(u_texture3, ex_texcoord0);

	vec2 vPathCoord = vec2(ex_texcoord0.x / ex_texcoord1.x, ex_texcoord0.y / ex_texcoord1.y);
	vec4 vPathIntensity = texture2D(u_texture5, vPathCoord); /* black color means there is a path */
	fScale = vPathIntensity.x; /* TODO: in what format are we uploading the texture do GPU memory? */

	vec4 vPathColor = texture2D(u_texture4, ex_texcoord0); 

	texcolor = u_color * (fScale * texcolor + (1.0 - fScale) * vPathColor);

	vec3 lvector0 = u_light_positions[0] - position_cs.xyz;
	float ldistance0 = length(lvector0);
	float lintensity0 = max(dot(normal_cs, normalize(lvector0)), 0.0) * u_light_intensities[0];
	lintensity0 /= ldistance0 * ldistance0;
	lintensity0 /= lintensity0 + 0.5;
	vec3 diffuse0 = vec3(lintensity0);
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse0, 1) * texcolor;

	vec3 lvector1 = u_light_positions[1] - position_cs.xyz;
	float ldistance1 = length(lvector1);
	float lintensity1 = max(dot(normal_cs, normalize(lvector1)), 0.0) * u_light_intensities[1];
	lintensity1 /= ldistance1 * ldistance1;
	lintensity1 /= lintensity1 + 0.5;
	vec3 diffuse1 = vec3(lintensity1);
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse1, 1) * texcolor;

	vec3 lvector2 = u_light_positions[2] - position_cs.xyz;
	float ldistance2 = length(lvector2);
	float lintensity2 = max(dot(normal_cs, normalize(lvector2)), 0.0) * u_light_intensities[2];
	lintensity2 /= ldistance2 * ldistance2;
	lintensity2 /= lintensity2 + 0.5;
	vec3 diffuse2 = vec3(lintensity2);
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse2, 1) * texcolor;

	vec3 lvector3 = u_light_positions[3] - position_cs.xyz;
	float ldistance3 = length(lvector3);
	float lintensity3 = max(dot(normal_cs, normalize(lvector3)), 0.0) * u_light_intensities[3];
	lintensity3 /= ldistance3 * ldistance3;
	lintensity3 /= lintensity3 + 0.5;
	vec3 diffuse3 = vec3(lintensity3);
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse3, 1) * texcolor;

	vec3 lvector4 = u_light_positions[4] - position_cs.xyz;
	float ldistance4 = length(lvector4);
	float lintensity4 = max(dot(normal_cs, normalize(lvector4)), 0.0) * u_light_intensities[4];
	lintensity4 /= ldistance4 * ldistance4;
	lintensity4 /= lintensity4 + 0.5;
	vec3 diffuse4 = vec3(lintensity4);
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse4, 1) * texcolor;

	vec3 lvector5 = u_light_positions[5] - position_cs.xyz;
	float ldistance5 = length(lvector5);
	float lintensity5 = max(dot(normal_cs, normalize(lvector5)), 0.0) * u_light_intensities[5];
	lintensity5 /= ldistance5 * ldistance5;
	lintensity5 /= lintensity5 + 0.5;
	vec3 diffuse5 = vec3(lintensity5);
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse5, 1) * texcolor;

	vec3 lvector6 = u_light_positions[6] - position_cs.xyz;
	float ldistance6 = length(lvector6);
	float lintensity6 = max(dot(normal_cs, normalize(lvector6)), 0.0) * u_light_intensities[6];
	lintensity6 /= ldistance6 * ldistance6;
	lintensity6 /= lintensity6 + 0.5;
	vec3 diffuse6 = vec3(lintensity6);
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse6, 1) * texcolor;

	vec3 lvector7 = u_light_positions[7] - position_cs.xyz;
	float ldistance7 = length(lvector7);
	float lintensity7 = max(dot(normal_cs, normalize(lvector7)), 0.0) * u_light_intensities[7];
	lintensity7 /= ldistance7 * ldistance7;
	lintensity7 /= lintensity7 + 0.5;
	vec3 diffuse7 = vec3(lintensity7);
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse7, 1) * texcolor;
}