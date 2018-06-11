#version 330 core
#ifdef GL_ARB_uniform_buffer_object
#extension GL_ARB_uniform_buffer_object : enable
#endif

attribute vec3 in_position;
attribute vec4 in_color; /* unused */
attribute vec2 in_texcoord0;
attribute vec2 in_texcoord1; /* unused */
attribute vec3 in_normal;
attribute vec4 in_tangent;
attribute vec4 in_weights;
attribute vec4 in_bones;

varying vec2 ex_texcoord0;
varying vec4 ex_lightresult;

#ifdef GL_ARB_uniform_buffer_object
layout(std140) uniform u_animdata
{
	uniform mat3x4 u_bonemats[40]; /* TODO: pass SHADER_MAX_BONES as a define to the shader */
};
#else
uniform mat3x4 u_bonemats[40]; /* TODO: pass SHADER_MAX_BONES as a define to the shader */
#endif

uniform mat4 u_view;
uniform mat3 u_mv_normals;
uniform mat4 u_mvp;
uniform vec4 u_color;
uniform vec4 u_light0position;
uniform vec4 u_light0diffuse;
uniform vec4 u_light0ambient;

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
	/* vec3 mtangent = in_tangent.xyz * madjtrans; /* tangent not used, just here as an example TODO */
	/* vec3 mbitangent = cross(mnormal, mtangent) * in_tangent.w; /* bitangent not used, just here as an example TODO */

	/* TODO: SMOOTH SHADING */
	ex_lightresult = u_color * clamp((clamp(dot(normalize(u_mv_normals * mnormal), (u_view * normalize(u_light0position)).xyz), 0.0, 1.0) * u_light0diffuse + u_light0ambient), 0.0, 1.0);
}
