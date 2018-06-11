#version 330 core

attribute vec3 in_position;
attribute vec4 in_color; /* unused */
attribute vec2 in_texcoord0;
attribute vec2 in_texcoord1;
attribute vec3 in_normal; /* unused */
attribute vec4 in_tangent; /* unused */
attribute vec4 in_weights; /* unused */
attribute vec4 in_bones; /* unused */

varying vec2 ex_texcoord0;
varying vec2 ex_texcoord1;

uniform mat4 u_mvp;

void main()
{
    gl_Position = u_mvp * vec4(in_position, 1.0);
	ex_texcoord0 = in_texcoord0;
	ex_texcoord1 = in_texcoord1;
}