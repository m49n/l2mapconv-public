#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in ivec4 v_block[];

out vec2 v_uv;

uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_model;

void main() {
    vec3 quad[4] = vec3[](
        vec3(-1.0f, 1.0f, 0.0f),
        vec3(-1.0f, -1.0f, 0.0f),
        vec3(1.0f, 1.0f, 0.0f),
        vec3(1.0f, -1.0f, 0.0f)
    );

    vec2 uvs[4] = vec2[](
        vec2(0.0f, 0.25f),
        vec2(0.0f, 0.0f),
        vec2(0.25f, 0.25f),
        vec2(0.25f, 0.0f)
    );

    float x = v_block[0].x;
    float y = v_block[0].y;
    float z = v_block[0].z;
    int type = v_block[0].w & 0xff;
    int nswe = v_block[0].w >> 16;

    float scale = type == 0 ? 63.0f : 7.0f;
    vec3 center = vec3(x * 16.0f + scale, y * 16.0f + scale, z - 16.0f);

    if ((nswe & 0x8) != 0) {
        uvs[0].x += 0.5f;
        uvs[1].x += 0.5f;
        uvs[2].x += 0.5f;
        uvs[3].x += 0.5f;
    }

    if ((nswe & 0x4) != 0) {
        uvs[0].x += 0.25f;
        uvs[1].x += 0.25f;
        uvs[2].x += 0.25f;
        uvs[3].x += 0.25f;
    }

    if ((nswe & 0x2) != 0) {
        uvs[0].y += 0.5f;
        uvs[1].y += 0.5f;
        uvs[2].y += 0.5f;
        uvs[3].y += 0.5f;
    }

    if ((nswe & 0x1) != 0) {
        uvs[0].y += 0.25f;
        uvs[1].y += 0.25f;
        uvs[2].y += 0.25f;
        uvs[3].y += 0.25f;
    }

    for (int i = 0; i < 4; i++) {
        vec3 position = center + quad[i] * scale;

        v_uv = type == 0 ? vec2(0.0f, 0.0f) : uvs[i];
        gl_Position = u_projection * u_view * u_model * vec4(position, 1.0f);
        EmitVertex();
    }

    EndPrimitive();
}
