#version 330 core

layout(location = 0) in ivec4 in_block;

out ivec4 v_block;

void main() {
    v_block = in_block;
}
