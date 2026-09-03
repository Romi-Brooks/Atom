#version 450

// Cyberpunk-style chromatic aberration post-processing shader.
// Shifts RGB channels apart horizontally, with stronger separation toward
// the screen edges (vignette falloff). Add scanlines and subtle noise for
// the full CRT / cyberpunk look.
//
// SDL_GPU fragment uniform buffers use set 3, binding 0 on all backends.
// Expected uniforms (push-constant/UBO at set 3, binding 0):
//   float uAmount;      // 0.0 = off, ~0.003 = subtle, 0.01 = strong
//   float uScanline;    // 0.0 = off, 0.08 = typical CRT scanlines
//   float uNoise;       // 0.0 = off, 0.05 = subtle grain
//   float uTime;        // seconds, for animated noise

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D uScene;

layout(set = 3, binding = 0, std140) uniform PostParams {
    float uAmount;
    float uScanline;
    float uNoise;
    float uTime;
    vec4 uRegion; // normalized top-left x/y/width/height
    vec4 uMask;   // corner radius px, feather px, unused, unused
};

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec2 uv = vUv;

    // Edge falloff: more aberration toward the corners.
    vec2 radial = uv - 0.5;
    float dist = length(radial);
    float falloff = smoothstep(0.0, 0.7, dist);
    float shift = uAmount * falloff;
    // Rotate the aberration axis once per second for the familiar animated
    // RGB split effect while keeping the animation deterministic.
    float angle = floor(uTime) * 1.57079632679;
    vec2 axis = vec2(cos(angle), sin(angle));
    vec2 dir = axis * shift;

    // Sample RGB channels at slightly offset UVs.
    float r = texture(uScene, uv + dir).r;
    float g = texture(uScene, uv).g;
    float b = texture(uScene, uv - dir).b;
    vec3 color = vec3(r, g, b);
    float alpha = texture(uScene, uv).a;

    // Scanlines (darken every other row).
    if (uScanline > 0.0) {
        float scan = sin(uv.y * 1200.0 + uTime * 10.0) * 0.5 + 0.5;
        color *= 1.0 - uScanline * scan;
    }

    // Film grain / noise.
    if (uNoise > 0.0) {
        float n = hash(uv * vec2(1920.0, 1080.0) + uTime * 60.0);
        color += (n - 0.5) * uNoise;
    }

    const vec2 maskUv = vUv;
    const vec2 pixelSize = vec2(textureSize(uScene, 0));
    const vec2 p = maskUv * pixelSize - (uRegion.xy + 0.5 * uRegion.zw) * pixelSize;
    const vec2 halfExtent = 0.5 * uRegion.zw * pixelSize;
    const float corner = min(uMask.x, min(halfExtent.x, halfExtent.y));
    const vec2 q = abs(p) - (halfExtent - vec2(corner));
    const float sdf = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - corner;
    const float edge = max(uMask.y, 0.5);
    const float mask = 1.0 - smoothstep(-edge, edge, sdf);
    outColor = vec4(color, alpha * mask);
}
