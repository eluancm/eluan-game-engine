#version 330 core
#ifdef GL_ARB_uniform_buffer_object
#extension GL_ARB_uniform_buffer_object : enable
#endif

attribute vec3 in_position;
attribute vec4 in_color; /* unused */
attribute vec2 in_texcoord0;
attribute vec2 in_texcoord1; /* unused */
attribute vec3 in_normal;
attribute vec4 in_tangent; /* unused */
attribute vec4 in_weights;
attribute vec4 in_bones;

varying vec2 ex_texcoord0;
varying vec4 position_cs;
varying vec3 normal_cs;

#ifdef GL_ARB_uniform_buffer_object
layout(std140) uniform u_animdata
{
	uniform mat3x4 u_bonemats[40]; /* TODO: pass SHADER_MAX_BONES as a define to the shader */
};
#else
uniform mat3x4 u_bonemats[40]; /* TODO: pass SHADER_MAX_BONES as a define to the shader */
#endif

uniform mat4 u_mv;
uniform mat3 u_mv_normals;
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

	ex_texcoord0 = in_texcoord0;

	mat3 madjtrans = mat3(cross(m[1].xyz, m[2].xyz), cross(m[2].xyz, m[0].xyz), cross(m[0].xyz, m[1].xyz));
	vec3 mnormal = in_normal * madjtrans;

	position_cs = u_mv * position_4;
	normal_cs = normalize(u_mv_normals * mnormal);
}
