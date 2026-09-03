#version 450

// Glitch transition shader for show/hide / next-track / prev-track effects.
// Combines horizontal block displacement, RGB channel splitting, and
// scanline jitter. Driven by a single progress value (0..1) that the
// application animates over ~200-400 ms.
//
// SDL_GPU fragment uniform buffers use set 3, binding 0 on all backends.
// Expected uniforms (push-constant/UBO at set 3, binding 0):
//   float uProgress;    // 0.0 = no glitch, 1.0 = full glitch (mid-transition)
//   float uIntensity;   // global strength multiplier, ~1.0
//   float uTime;        // seconds, for random block animation
//   float uDirection;   // +1 = next (shift right), -1 = prev (shift left)

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D uScene;

layout(set = 3, binding = 0, std140) uniform GlitchParams {
    float uProgress;
    float uIntensity;
    float uTime;
    float uDirection;
    vec4 uRegion; // normalized top-left x/y/width/height
    vec4 uMask;   // corner radius px, feather px, unused, unused
};

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    vec2 uv = vUv;
    float p = uProgress * uIntensity;

    // --- Horizontal block displacement ---
    // Slice the screen into ~20 horizontal bands; each band randomly shifts
    // left or right when glitch is active.
    float bandCount = 20.0;
    float band = floor(uv.y * bandCount);
    float bandNoise = hash(vec2(band, floor(uTime * 30.0)));
    float displacement = (bandNoise - 0.5) * 2.0 * p * 0.08 * uDirection;
    // Only displace a random subset of bands for a broken-up look.
    if (hash(vec2(band, 1.7)) > 0.5) {
        uv.x += displacement;
    }

    // --- RGB channel split ---
    float split = p * 0.015;
    vec2 splitDir = normalize(vec2(1.0, 0.3));
    float r = texture(uScene, uv + splitDir * split).r;
    float g = texture(uScene, uv).g;
    float b = texture(uScene, uv - splitDir * split).b;
    vec3 color = vec3(r, g, b);
    float sourceAlpha = texture(uScene, uv).a;

    // --- Scanline jitter ---
    // Thin horizontal lines that jump vertically.
    float lineNoise = noise(vec2(uv.y * 80.0, uTime * 20.0));
    if (lineNoise > 0.85) {
        color += vec3(0.1, 0.15, 0.2) * p;
    }

    // --- Brightness flash at peak glitch ---
    float flash = smoothstep(0.3, 0.6, p) * (1.0 - smoothstep(0.6, 0.9, p));
    color += flash * 0.15;

    // --- Alpha fade for show/hide ---
    // Fade out as progress approaches 1 (for hide), or fade in from 1 (for show).
    // The application controls direction; here we just expose a soft edge.
    float alpha = 1.0 - smoothstep(0.7, 1.0, p) * 0.8;

    const vec2 maskUv = vUv;
    const vec2 pixelSize = vec2(textureSize(uScene, 0));
    const vec2 pxy = maskUv * pixelSize - (uRegion.xy + 0.5 * uRegion.zw) * pixelSize;
    const vec2 halfExtent = 0.5 * uRegion.zw * pixelSize;
    const float corner = min(uMask.x, min(halfExtent.x, halfExtent.y));
    const vec2 q = abs(pxy) - (halfExtent - vec2(corner));
    const float sdf = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - corner;
    const float edge = max(uMask.y, 0.5);
    const float mask = 1.0 - smoothstep(-edge, edge, sdf);
    outColor = vec4(color, sourceAlpha * alpha * mask);
}
