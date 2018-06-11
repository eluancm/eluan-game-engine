#version 330 core

//#extension GL_EXT_gpu_shader4 : require

varying vec2 ex_texcoord0;
varying vec2 ex_texcoord1;
varying float ex_worldpos_unscaled_y;
varying vec3 ex_textureblendingproperties;
varying vec4 position_cs;
varying vec3 normal_cs;

uniform vec4 u_color;
uniform mat4x4 u_camera_view_matrix_inv;
uniform mat4x4 u_light_view_matrices[8]; /* TODO: pass SHADER_MAX_LIGHTS as a define to the shader */
uniform mat4x4 u_light_projection_matrices[8]; /* TODO: pass SHADER_MAX_LIGHTS as a define to the shader */
uniform vec3 u_light_positions[8]; /* TODO: pass SHADER_MAX_LIGHTS as a define to the shader */
uniform float u_light_intensities[8]; /* TODO: pass SHADER_MAX_LIGHTS as a define to the shader */
uniform sampler2D u_texture0; /* low elevation */
/* TODO: pass SHADER_MAX_LIGHTS as a define to the shader */
uniform samplerCubeShadow u_texture2; /* shadow cubemap */
uniform sampler2D u_texture1; /* mid elevation */
uniform sampler2D u_texture3; /* high elevation */
uniform sampler2D u_texture4; /* path */
uniform sampler2D u_texture5; /* path mask (for the entire heightmap) */
uniform samplerCubeShadow u_texture6;
uniform samplerCubeShadow u_texture7;
uniform samplerCubeShadow u_texture8;
uniform samplerCubeShadow u_texture9;
uniform samplerCubeShadow u_texture10;
uniform samplerCubeShadow u_texture11;
uniform samplerCubeShadow u_texture12;

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

	vec4 position_ls0 = u_light_view_matrices[0] * u_camera_view_matrix_inv * position_cs; /* TODO: optimize the last two multiplications */
	// shadow map test
	vec4 abs_position0 = abs(position_ls0);
	float fs_z0 = -max(abs_position0.x, max(abs_position0.y, abs_position0.z));
	vec4 clip0 = u_light_projection_matrices[0] * vec4(0.0, 0.0, fs_z0, 1.0);
	float depth0 = (clip0.z / clip0.w) * 0.5 + 0.5;
	vec3 result0 = vec3(texture(u_texture2, vec4(position_ls0.xyz, depth0)));

	/* TODO: interpolate shadowcube results using some nice technique to avoid pixelized shadows / lots more of optimization to enhance shadows, read papers */
	vec3 lvector0 = u_light_positions[0] - position_cs.xyz;
	float ldistance0 = length(lvector0);
	float lintensity0 = max(dot(normal_cs, normalize(lvector0)), 0.0) * u_light_intensities[0];
	lintensity0 /= ldistance0 * ldistance0;
	lintensity0 /= lintensity0 + 0.5;
	vec3 diffuse0 = lintensity0 * result0.zyx;
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse0, 1) * texcolor;

	vec4 position_ls1 = u_light_view_matrices[1] * u_camera_view_matrix_inv * position_cs; /* TODO: optimize the last two multiplications */
	// shadow map test
	vec4 abs_position1 = abs(position_ls1);
	float fs_z1 = -max(abs_position1.x, max(abs_position1.y, abs_position1.z));
	vec4 clip1 = u_light_projection_matrices[1] * vec4(0.0, 0.0, fs_z1, 1.0);
	float depth1 = (clip1.z / clip1.w) * 0.5 + 0.5;
	vec3 result1 = vec3(texture(u_texture6, vec4(position_ls1.xyz, depth1)));

	/* TODO: interpolate shadowcube results using some nice technique to avoid pixelized shadows / lots more of optimization to enhance shadows, read papers */
	vec3 lvector1 = u_light_positions[1] - position_cs.xyz;
	float ldistance1 = length(lvector1);
	float lintensity1 = max(dot(normal_cs, normalize(lvector1)), 0.0) * u_light_intensities[1];
	lintensity1 /= ldistance1 * ldistance1;
	lintensity1 /= lintensity1 + 0.5;
	vec3 diffuse1 = lintensity1 * result1.zyx;
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse1, 1) * texcolor;

	vec4 position_ls2 = u_light_view_matrices[2] * u_camera_view_matrix_inv * position_cs; /* TODO: optimize the last two multiplications */
	// shadow map test
	vec4 abs_position2 = abs(position_ls2);
	float fs_z2 = -max(abs_position2.x, max(abs_position2.y, abs_position2.z));
	vec4 clip2 = u_light_projection_matrices[2] * vec4(0.0, 0.0, fs_z2, 1.0);
	float depth2 = (clip2.z / clip2.w) * 0.5 + 0.5;
	vec3 result2 = vec3(texture(u_texture7, vec4(position_ls2.xyz, depth2)));

	/* TODO: interpolate shadowcube results using some nice technique to avoid pixelized shadows / lots more of optimization to enhance shadows, read papers */
	vec3 lvector2 = u_light_positions[2] - position_cs.xyz;
	float ldistance2 = length(lvector2);
	float lintensity2 = max(dot(normal_cs, normalize(lvector2)), 0.0) * u_light_intensities[2];
	lintensity2 /= ldistance2 * ldistance2;
	lintensity2 /= lintensity2 + 0.5;
	vec3 diffuse2 = lintensity2 * result2.zyx;
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse2, 1) * texcolor;

	vec4 position_ls3 = u_light_view_matrices[3] * u_camera_view_matrix_inv * position_cs; /* TODO: optimize the last two multiplications */
	// shadow map test
	vec4 abs_position3 = abs(position_ls3);
	float fs_z3 = -max(abs_position3.x, max(abs_position3.y, abs_position3.z));
	vec4 clip3 = u_light_projection_matrices[3] * vec4(0.0, 0.0, fs_z3, 1.0);
	float depth3 = (clip3.z / clip3.w) * 0.5 + 0.5;
	vec3 result3 = vec3(texture(u_texture8, vec4(position_ls3.xyz, depth3)));

	/* TODO: interpolate shadowcube results using some nice technique to avoid pixelized shadows / lots more of optimization to enhance shadows, read papers */
	vec3 lvector3 = u_light_positions[3] - position_cs.xyz;
	float ldistance3 = length(lvector3);
	float lintensity3 = max(dot(normal_cs, normalize(lvector3)), 0.0) * u_light_intensities[3];
	lintensity3 /= ldistance3 * ldistance3;
	lintensity3 /= lintensity3 + 0.5;
	vec3 diffuse3 = lintensity3 * result3.zyx;
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse3, 1) * texcolor;

	vec4 position_ls4 = u_light_view_matrices[4] * u_camera_view_matrix_inv * position_cs; /* TODO: optimize the last two multiplications */
	// shadow map test
	vec4 abs_position4 = abs(position_ls4);
	float fs_z4 = -max(abs_position4.x, max(abs_position4.y, abs_position4.z));
	vec4 clip4 = u_light_projection_matrices[4] * vec4(0.0, 0.0, fs_z4, 1.0);
	float depth4 = (clip4.z / clip4.w) * 0.5 + 0.5;
	vec3 result4 = vec3(texture(u_texture9, vec4(position_ls4.xyz, depth4)));

	/* TODO: interpolate shadowcube results using some nice technique to avoid pixelized shadows / lots more of optimization to enhance shadows, read papers */
	vec3 lvector4 = u_light_positions[4] - position_cs.xyz;
	float ldistance4 = length(lvector4);
	float lintensity4 = max(dot(normal_cs, normalize(lvector4)), 0.0) * u_light_intensities[4];
	lintensity4 /= ldistance4 * ldistance4;
	lintensity4 /= lintensity4 + 0.5;
	vec3 diffuse4 = lintensity4 * result4.zyx;
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse4, 1) * texcolor;

	vec4 position_ls5 = u_light_view_matrices[5] * u_camera_view_matrix_inv * position_cs; /* TODO: optimize the last two multiplications */
	// shadow map test
	vec4 abs_position5 = abs(position_ls5);
	float fs_z5 = -max(abs_position5.x, max(abs_position5.y, abs_position5.z));
	vec4 clip5 = u_light_projection_matrices[5] * vec4(0.0, 0.0, fs_z5, 1.0);
	float depth5 = (clip5.z / clip5.w) * 0.5 + 0.5;
	vec3 result5 = vec3(texture(u_texture10, vec4(position_ls5.xyz, depth5)));

	/* TODO: interpolate shadowcube results using some nice technique to avoid pixelized shadows / lots more of optimization to enhance shadows, read papers */
	vec3 lvector5 = u_light_positions[5] - position_cs.xyz;
	float ldistance5 = length(lvector5);
	float lintensity5 = max(dot(normal_cs, normalize(lvector5)), 0.0) * u_light_intensities[5];
	lintensity5 /= ldistance5 * ldistance5;
	lintensity5 /= lintensity5 + 0.5;
	vec3 diffuse5 = lintensity5 * result5.zyx;
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse5, 1) * texcolor;

	vec4 position_ls6 = u_light_view_matrices[6] * u_camera_view_matrix_inv * position_cs; /* TODO: optimize the last two multiplications */
	// shadow map test
	vec4 abs_position6 = abs(position_ls6);
	float fs_z6 = -max(abs_position6.x, max(abs_position6.y, abs_position6.z));
	vec4 clip6 = u_light_projection_matrices[6] * vec4(0.0, 0.0, fs_z6, 1.0);
	float depth6 = (clip6.z / clip6.w) * 0.5 + 0.5;
	vec3 result6 = vec3(texture(u_texture11, vec4(position_ls6.xyz, depth6)));

	/* TODO: interpolate shadowcube results using some nice technique to avoid pixelized shadows / lots more of optimization to enhance shadows, read papers */
	vec3 lvector6 = u_light_positions[6] - position_cs.xyz;
	float ldistance6 = length(lvector6);
	float lintensity6 = max(dot(normal_cs, normalize(lvector6)), 0.0) * u_light_intensities[6];
	lintensity6 /= ldistance6 * ldistance6;
	lintensity6 /= lintensity6 + 0.5;
	vec3 diffuse6 = lintensity6 * result6.zyx;
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse6, 1) * texcolor;

	vec4 position_ls7 = u_light_view_matrices[7] * u_camera_view_matrix_inv * position_cs; /* TODO: optimize the last two multiplications */
	// shadow map test
	vec4 abs_position7 = abs(position_ls7);
	float fs_z7 = -max(abs_position7.x, max(abs_position7.y, abs_position7.z));
	vec4 clip7 = u_light_projection_matrices[7] * vec4(0.0, 0.0, fs_z7, 1.0);
	float depth7 = (clip7.z / clip7.w) * 0.5 + 0.5;
	vec3 result7 = vec3(texture(u_texture12, vec4(position_ls7.xyz, depth7)));

	/* TODO: interpolate shadowcube results using some nice technique to avoid pixelized shadows / lots more of optimization to enhance shadows, read papers */
	vec3 lvector7 = u_light_positions[7] - position_cs.xyz;
	float ldistance7 = length(lvector7);
	float lintensity7 = max(dot(normal_cs, normalize(lvector7)), 0.0) * u_light_intensities[7];
	lintensity7 /= ldistance7 * ldistance7;
	lintensity7 /= lintensity7 + 0.5;
	vec3 diffuse7 = lintensity7 * result7.zyx;
	/* TODO: do ambient light, ambient occlusion, not using ifs
	if (diffuse.r < 0.4)
		diffuse.r = 0.4;
	if (diffuse.g < 0.4)
		diffuse.g = 0.4;
	if (diffuse.b < 0.4)
		diffuse.b = 0.4;*/
	gl_FragColor += vec4(diffuse7, 1) * texcolor;
}
