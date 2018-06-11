#version 330 core

varying vec3 ex_texcoord0;

uniform samplerCube u_texture2;

void main()
{
	gl_FragColor = texture(u_texture2, ex_texcoord0);
	// old GL, just like the shadows code:
	// gl_FragColor = textureCube(cube_texture, texcoords);
}
