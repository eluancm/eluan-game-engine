#version 330 core
#ifdef GL_ARB_uniform_buffer_object
#extension GL_ARB_uniform_buffer_object : enable
#endif

attribute vec3 in_position;
attribute vec4 in_color; /* unused */
attribute vec2 in_texcoord0; /* unused */
attribute vec2 in_texcoord1; /* unused */
attribute vec3 in_normal; /* unused */
attribute vec4 in_tangent; /* unused */
attribute vec4 in_weights;
attribute vec4 in_bones;

#ifdef GL_ARB_uniform_buffer_object
layout(std140) uniform u_animdata
{
	uniform mat3x4 u_bonemats[40]; /* TODO: pass SHADER_MAX_BONES as a define to the shader */
};
#else
uniform mat3x4 u_bonemats[40]; /* TODO: pass SHADER_MAX_BONES as a define to the shader */
#endif

uniform mat4 u_mvp;

void main()
{
	/* this is why we only have at most 4 bones influencing a vertex, to fit */
	mat3x4 m = u_bonemats[int(in_bones.x)] * in_weights.x;
	m += u_bonemats[int(in_bones.y)] * in_weights.y;
	m += u_bonemats[int(in_bones.z)] * in_weights.z;
	m += u_bonemats[int(in_bones.w)] * in_weights.w;
	vec4 position_4_untransform = vec4(in_position, 1.0);
	vec4 position_4 = vec4(position_4_untransform * m, 1.0);

    gl_Position = u_mvp * position_4;
}
