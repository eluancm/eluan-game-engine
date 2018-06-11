#version 330 core

varying vec4 ex_color;
varying vec2 ex_texcoord0;

uniform sampler2D u_texture0;
uniform vec4 u_color;

void main()
{
	/* modulate */
    gl_FragColor = texture2D(u_texture0, ex_texcoord0) * ex_color * u_color;
}
