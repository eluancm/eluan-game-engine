#version 330 core

//#extension GL_EXT_gpu_shader4 : require

attribute vec3 in_position;
attribute vec4 in_color; /* unused */
attribute vec2 in_texcoord0;
attribute vec2 in_texcoord1; /* unused */
attribute vec3 in_normal;
attribute vec4 in_tangent; /* unused */
attribute vec4 in_weights; /* unused */
attribute vec4 in_bones; /* unused */

varying vec2 ex_texcoord0;
varying vec4 position_cs;
varying vec3 normal_cs;

uniform mat4 u_mv;
uniform mat3 u_mv_normals;
uniform mat4 u_mvp;

void main()
{
	vec4 position_4 = vec4(in_position, 1.0);

    gl_Position = u_mvp * position_4;

	ex_texcoord0 = in_texcoord0;

	position_cs = u_mv * position_4;
	normal_cs = normalize(u_mv_normals * in_normal);
}
