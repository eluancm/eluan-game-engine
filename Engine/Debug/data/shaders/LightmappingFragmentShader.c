#version 330 core

varying vec2 ex_texcoord0;
varying vec2 ex_texcoord1;

uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform vec4 u_color;

void main()
{
	/* modulate */
	gl_FragColor = texture2D(u_texture0, ex_texcoord0) * texture2D(u_texture1, ex_texcoord1) * u_color;
}