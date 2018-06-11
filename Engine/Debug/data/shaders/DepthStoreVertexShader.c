#version 330 core

attribute vec3 in_position;
attribute vec4 in_color; /* unused */
attribute vec2 in_texcoord0; /* unused */
attribute vec2 in_texcoord1; /* unused */
attribute vec3 in_normal; /* unused */
attribute vec4 in_tangent; /* unused */
attribute vec4 in_weights; /* unused */
attribute vec4 in_bones; /* unused */

uniform mat4 u_mvp;

void main()
{
	vec4 position_4 = vec4(in_position, 1.0);

    gl_Position = u_mvp * position_4;
}