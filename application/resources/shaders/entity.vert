#version 330 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in mat4 in_model;

out vec3 v_normal;
out vec2 v_uv;
out vec3 v_position;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

void main() {
    mat4 model = u_model * in_model;
    gl_Position = u_projection * u_view * model * vec4(in_position, 1.0f);

    mat3 normal_matrix = transpose(inverse(mat3(model)));
    v_normal = normalize(normal_matrix * in_normal);

    v_uv = in_uv;
    v_position = vec3(model * vec4(in_position, 1.0f));
}
