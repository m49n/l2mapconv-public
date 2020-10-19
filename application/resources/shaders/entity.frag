#version 330 core

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_position;

out vec4 out_color;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform vec3 u_camera;

void main() {
    vec3 normal = v_normal;
    vec3 light_position = u_camera;
    vec3 light_direction = normalize(light_position - v_position);

    // Two side light model.
    if (dot(normal, light_direction) < 0.0f) {
        normal = -normal;
    }

    float light_intensity = max(0.0f, dot(normal, light_direction));
    vec3 diffuse = u_color * light_intensity;
    vec3 ambient = u_color * 0.25f;

    out_color = vec4(u_color * (ambient + diffuse), 1.0f);
}
