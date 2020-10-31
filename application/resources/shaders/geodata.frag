#version 330 core

in vec2 v_uv;

out vec4 out_color;

uniform sampler2D u_texture;
uniform vec3 u_color;

void main() {
    out_color = texture2D(u_texture, v_uv) * vec4(1.0f, 1.0f, 1.0f, 0.9f);
}
