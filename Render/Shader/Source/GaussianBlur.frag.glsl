#version 450

// Single-pass, screen-space 2D Gaussian approximation used for the MusicCard
// backdrop. The source is the already-rendered wallpaper, while the resolve
// pass is scissored and rounded to the popup rectangle. This keeps the
// debugger and the rest of the wallpaper untouched.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D uScene;

// SDL_GPU fragment uniform buffers use set 3, binding 0.
// x: blur radius in source texels, y: rounded-corner radius in pixels,
// z: edge feather in pixels.
layout(set = 3, binding = 0, std140) uniform BlurParams {
    float uRadius;
    float uCornerRadius;
    float uFeather;
    float uUnused;
    vec4 uRegion; // normalized x, y, width, height in top-left coordinates
};

void main() {
    // sigma ~= 2.0, support = 4. The two-dimensional kernel is the outer
    // product of this normalized one-dimensional kernel (81 samples).
    const float weights[5] = float[](0.2041636887, 0.1801738229, 0.1238315360, 0.0662822453, 0.0276305506);
    const vec2 texel = 1.0 / vec2(textureSize(uScene, 0));
    const float radius = max(uRadius, 0.0);

    vec4 result = vec4(0.0);
    for (int y = -4; y <= 4; ++y) {
        const int ay = y < 0 ? -y : y;
        for (int x = -4; x <= 4; ++x) {
            const int ax = x < 0 ? -x : x;
            const vec2 offset = vec2(float(x), float(y)) * radius * texel;
            result += texture(uScene, vUv + offset) * weights[ax] * weights[ay];
        }
    }

    // vUv already follows the top-left framebuffer coordinate system used by
    // the fullscreen triangle. Only texture sampling needs the backend's
    // orientation handling; the mask must not be flipped a second time.
    const vec2 maskUv = vUv;
    const vec2 pixelSize = vec2(textureSize(uScene, 0));
    const vec2 p = maskUv * pixelSize - (uRegion.xy + 0.5 * uRegion.zw) * pixelSize;
    const vec2 halfExtent = 0.5 * uRegion.zw * pixelSize;
    const float corner = min(uCornerRadius, min(halfExtent.x, halfExtent.y));
    const vec2 q = abs(p) - (halfExtent - vec2(corner));
    const float signedDistance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - corner;
    const float feather = max(uFeather, 0.5);
    const float mask = 1.0 - smoothstep(-feather, feather, signedDistance);
    outColor = vec4(result.rgb, result.a * mask);
}
