#version 330 core

attribute vec3 in_position;
attribute vec4 in_color; /* unused */
attribute vec2 in_texcoord0; /* unused */
attribute vec2 in_texcoord1; /* unused */
attribute vec3 in_normal; /* unused */
attribute vec4 in_tangent; /* unused */
attribute vec4 in_weights; /* unused */
attribute vec4 in_bones; /* unused */

varying vec3 ex_texcoord0;

uniform mat4 u_view;
uniform mat4 u_projection;

void main()
{
    gl_Position = (u_projection * u_view * vec4(in_position, 1.0)).xyww; /* always at max depth */
	ex_texcoord0 = in_position;
}
