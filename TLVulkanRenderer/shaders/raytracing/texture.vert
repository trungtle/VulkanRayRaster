#version 450
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shading_language_420pack : enable

layout(binding = 0) uniform mat4 u_mvp;

layout(location = 0) in vec2 v_position;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec2 f_uv;

void main()
{
    f_uv = texcoord;
    gl_Position = u_mvp * vec4(v_position, 0.0, 1.0);
}