#version 330 core

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_position;

out vec4 out_color;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform vec3 u_camera;

void main() {
    vec3 texture = vec3(texture2D(u_texture, v_uv));
    vec3 color = texture + u_color;

    vec3 normal = v_normal;
    vec3 light_position = u_camera;
    vec3 light_direction = normalize(light_position - v_position);

    if (dot(normal, light_direction) < 0.0f) {
        normal = -normal;
        color = color * 0.5f;
    }

    float light_intensity = max(0.0f, dot(normal, light_direction));
    vec3 diffuse = color * light_intensity;
    vec3 ambient = texture * 1.5f + u_color * 0.25f;

    out_color = vec4(color * (ambient + diffuse), 1.0f);
}
