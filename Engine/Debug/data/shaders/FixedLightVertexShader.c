#version 330 core

attribute vec3 in_position;
attribute vec4 in_color; /* unused */
attribute vec2 in_texcoord0;
attribute vec2 in_texcoord1; /* unused */
attribute vec3 in_normal;
attribute vec4 in_tangent;
attribute vec4 in_weights; /* unused */
attribute vec4 in_bones; /* unused */

varying vec2 ex_texcoord0;
varying vec4 ex_lightresult;

uniform mat4 u_view;
uniform mat3 u_mv_normals;
uniform mat4 u_mvp;
uniform vec4 u_color;
uniform vec4 u_light0position;
uniform vec4 u_light0diffuse;
uniform vec4 u_light0ambient;

void main()
{
	vec4 position_4 = vec4(in_position, 1.0);

    gl_Position = u_mvp * position_4;

	ex_texcoord0 = in_texcoord0;

	/* TODO: SMOOTH SHADING */
	ex_lightresult = u_color * clamp((clamp(dot(normalize(u_mv_normals * in_normal), (u_view * normalize(u_light0position)).xyz), 0.0, 1.0) * u_light0diffuse + u_light0ambient), 0.0, 1.0);
}
