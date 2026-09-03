#version 450

layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 textureCoordinate;
layout(location = 0) out vec4 outColor;

// SDL_GPU binds fragment samplers at set 2, binding 0 (see Sprite.frag.glsl).
layout(set = 2, binding = 0) uniform sampler2D spriteTexture;

void main() {
    outColor = vertexColor * texture(spriteTexture, textureCoordinate);
}
