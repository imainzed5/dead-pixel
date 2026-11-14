#version 330 core

// The mood pass (phase 12): the world as drawn, with the character's state
// laid over it. Every effect is 0 when neutral; see world/MoodRules.h.

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uScene;
uniform vec2 uSize;
uniform float uTime;
uniform float uSaturation;
uniform float uVignette;
uniform float uBlur;
uniform float uBlink;
uniform float uColdEdge;
uniform float uWarmCast;
uniform float uShimmer;
uniform float uDoubleImage;
uniform float uHaze;
uniform float uZoom;
uniform float uDimEdges;
uniform float uShakePx;
uniform float uRedPulse;

vec3 scene(vec2 uv)
{
    return texture(uScene, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
}

void main()
{
    vec2 px = 1.0 / uSize;
    // Held: the view pulls in. Panic: a pixel's jolt on the beat.
    vec2 uv = vec2(0.5) + (vUv - vec2(0.5)) * (1.0 - uZoom);
    uv += vec2(uShakePx, -uShakePx) * px;

    // How far out from the middle (0 centre, 1 at the edge midpoints).
    vec2 c = vUv - vec2(0.5);
    float r = length(c) * 2.0;
    float edge = smoothstep(0.45, 1.05, r);

    // Heat: the air wavers, most at the top of the view (the far side).
    if (uShimmer > 0.0)
    {
        float band = smoothstep(0.35, 1.0, vUv.y);
        uv.x += sin(vUv.y * 90.0 + uTime * 2.2) * 1.5 * px.x * uShimmer * band;
    }

    vec3 col = scene(uv);

    // Tired: the edges go soft.
    if (uBlur > 0.0)
    {
        float k = 2.0 + 1.5 * uBlur;
        vec3 sum = col;
        sum += scene(uv + vec2(k, 0.0) * px);
        sum += scene(uv - vec2(k, 0.0) * px);
        sum += scene(uv + vec2(0.0, k) * px);
        sum += scene(uv - vec2(0.0, k) * px);
        sum += scene(uv + vec2(k, k) * px * 0.7);
        sum += scene(uv - vec2(k, k) * px * 0.7);
        sum += scene(uv + vec2(k, -k) * px * 0.7);
        sum += scene(uv - vec2(k, -k) * px * 0.7);
        col = mix(col, sum / 9.0, clamp(uBlur * edge * 1.4, 0.0, 1.0));
    }

    // Fever: the world doubles at the edges.
    if (uDoubleImage > 0.0)
    {
        vec3 ghost = scene(uv + vec2(4.0, 1.0) * px * (0.5 + 0.5 * sin(uTime * 1.3)));
        col = mix(col, ghost, 0.45 * uDoubleImage * smoothstep(0.25, 0.9, r));
    }

    // Depression: the colour drains.
    float grey = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(grey), col, uSaturation);

    // Heat and cold.
    col = mix(col, col * vec3(1.08, 0.98, 0.84) + vec3(0.03, 0.01, 0.0), uWarmCast * 0.6);
    col = mix(col, col * vec3(0.72, 0.84, 1.12) + vec3(0.0, 0.03, 0.08), uColdEdge * edge);

    // Smoke: a grey haze over everything.
    col = mix(col, vec3(0.46, 0.45, 0.44), uHaze * 0.45);

    // Panic closes the view; listening dims its rim.
    if (uVignette > 0.0)
    {
        float inner = 1.0 - 2.0 * uVignette;
        col *= 1.0 - 0.92 * smoothstep(inner, inner + 0.45, r);
    }
    col *= 1.0 - uDimEdges * smoothstep(0.35, 1.0, r);

    // Close to dying: a red edge in time with the heart.
    col = mix(col, vec3(0.55, 0.02, 0.02), clamp(uRedPulse * 0.55 * smoothstep(0.5, 1.1, r), 0.0, 1.0));

    // A blink.
    col *= 1.0 - uBlink;

    FragColor = vec4(col, 1.0);
}
