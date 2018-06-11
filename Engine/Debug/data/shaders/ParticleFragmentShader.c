#version 330 core

varying vec4 ex_color; /* 0-255, is an GL_UNSIGNED_BYTE */

uniform vec4 u_color;

void main()
{
	/* modulate */
	gl_FragColor = (ex_color / 255.0) * u_color;
}