#version 330 core

varying vec2 ex_texcoord0;
varying vec4 ex_lightresult;

uniform sampler2D u_texture0;

void main()
{
	gl_FragColor = ex_lightresult * texture2D(u_texture0, ex_texcoord0);	
}