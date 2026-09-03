#version 450

// Batched 2D vertex pipeline used by Renderer2D. Vertex layout matches
// atom::render::Vertex2D: vertex color (straight alpha) at offset 0,
// position (pixels, top-left origin, y-down) at offset 16, texture
// coordinate at offset 24. Color is declared first so its FLOAT4 size
// lands on a 16-byte boundary, which D3D12 input layouts require.
layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inPosition;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 textureCoordinate;

// SDL_GPU binds vertex uniforms at set 1, binding 0 (see MeshUnlit.vert.glsl).
layout(set = 1, binding = 0, std140) uniform CameraData {
    mat4 viewProjection;
};

void main() {
    vertexColor = inColor;
    textureCoordinate = inTexCoord;
    gl_Position = viewProjection * vec4(inPosition, 0.0, 1.0);
}
