#version 450

layout(location = 0) out vec2 textureCoordinate;

const vec2 positions[6] = vec2[](
    vec2(-1.0,  1.0), vec2( 1.0,  1.0), vec2( 1.0, -1.0),
    vec2(-1.0,  1.0), vec2( 1.0, -1.0), vec2(-1.0, -1.0));
const vec2 coordinates[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    textureCoordinate = coordinates[gl_VertexIndex];
}
