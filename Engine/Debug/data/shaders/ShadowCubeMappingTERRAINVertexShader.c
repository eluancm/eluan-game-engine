#version 330 core

//#extension GL_EXT_gpu_shader4 : require

attribute vec3 in_position;
attribute vec4 in_color; /* unused */
attribute vec2 in_texcoord0;
attribute vec2 in_texcoord1; /* texture scaling used, to undo in_texcoord0 scaling */
attribute vec3 in_normal;
attribute vec4 in_tangent; /* unused */
attribute vec4 in_weights; /* first component as Y scale (TODO: send as 1/Y instead?), second and third components as texture 0->1 and 1->3 transition start, fourth component as transition window size */
attribute vec4 in_bones; /* unused */

varying vec2 ex_texcoord0;
varying vec2 ex_texcoord1;
varying float ex_worldpos_unscaled_y;
varying vec3 ex_textureblendingproperties;
varying vec4 position_cs;
varying vec3 normal_cs;

uniform mat4 u_model;
uniform mat4 u_mv;
uniform mat3 u_mv_normals;
uniform mat4 u_mvp;

void main()
{
	vec4 position_4 = vec4(in_position, 1.0);

    gl_Position = u_mvp * position_4;

	ex_texcoord0 = in_texcoord0;
	ex_texcoord1 = in_texcoord1;
	ex_worldpos_unscaled_y = (u_model * vec4(in_position, 1.0)).y / in_weights.x;
	ex_textureblendingproperties = in_weights.yzw;

	position_cs = u_mv * position_4;
	normal_cs = normalize(u_mv_normals * in_normal);
}
