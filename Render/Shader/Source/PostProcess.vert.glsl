#version 450

// Full-screen triangle vertex shader for post-processing passes.
// Draws a single triangle covering the entire viewport; no vertex buffer
// needed — positions and UVs are generated from gl_VertexIndex.
layout(location = 0) out vec2 vUv;

void main() {
    // Full-screen triangle: (-1,-1), (3,-1), (-1,3)
    // The bit pattern produces (-1,-1), (3,-1), (-1,3).  The factor of
    // two is required; without it the triangle only covers the upper-left
    // quarter and leaves a diagonal half-screen unrendered.
    vec2 pos = vec2(
        float((gl_VertexIndex << 1) & 2),
        float(gl_VertexIndex & 2)
    ) * 2.0 - 1.0;
    // SDL_GPU presents render-target textures with the opposite vertical
    // sampling orientation from the top-left 2D coordinate system used by
    // Renderer2D. Flip only the sampled source UV; keep clip-space positions
    // unchanged so the composite still covers the complete target.
    vUv = vec2(pos.x * 0.5 + 0.5, 0.5 - pos.y * 0.5);
    gl_Position = vec4(pos, 0.0, 1.0);
}
