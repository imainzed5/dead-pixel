#version 330 core

in vec2 vUv;
in vec4 vColor;
in vec2 vScreenPos;

uniform sampler2D uTexture;

// Lighting uniforms
uniform float uAmbientLight;                  // 0.0 = pitch black, 1.0 = full daylight
uniform vec3 uAmbientColor;                   // tint color for ambient light
uniform float uSaturation;                    // 1.0 = as drawn; the tone preset drains or boosts colour
uniform int uLightCount;                      // number of active point lights (max 16)
uniform vec2 uLightPositions[16];             // screen-space positions
uniform vec3 uLightColors[16];                // RGB color (and strength) of each light
uniform float uLightRadii[16];                // falloff radius in screen pixels, measured on the ground

// One cone light (the player's flashlight).
uniform int uSpotEnabled;
uniform vec2 uSpotPosition;                   // screen-space origin
uniform vec2 uSpotDirection;                  // unit direction in ground space (screen y doubled)
uniform float uSpotCosHalfAngle;
uniform float uSpotRange;                     // screen pixels, measured on the ground
uniform vec3 uSpotColor;

out vec4 FragColor;

// Isometric ground is squashed 2:1 on screen; undo that so light pools are
// round on the ground instead of on the screen.
vec2 groundDelta(vec2 from, vec2 to)
{
    vec2 d = to - from;
    d.y *= 2.0;
    return d;
}

void main()
{
    vec4 texColor = texture(uTexture, vUv) * vColor;

    vec3 lighting = uAmbientColor * uAmbientLight;

    for (int i = 0; i < uLightCount; ++i)
    {
        float dist = length(groundDelta(uLightPositions[i], vScreenPos));
        float attenuation = clamp(1.0 - dist / uLightRadii[i], 0.0, 1.0);
        attenuation *= attenuation; // quadratic falloff for softer edges
        lighting += uLightColors[i] * attenuation;
    }

    if (uSpotEnabled != 0)
    {
        vec2 d = groundDelta(uSpotPosition, vScreenPos);
        float dist = length(d);
        if (dist > 0.001)
        {
            float cosAngle = dot(d / dist, uSpotDirection);
            float cone = smoothstep(uSpotCosHalfAngle, uSpotCosHalfAngle + 0.06, cosAngle);
            float falloff = clamp(1.0 - dist / uSpotRange, 0.0, 1.0);
            // Bright core near the lens, long soft throw.
            lighting += uSpotColor * cone * (falloff * 0.75 + falloff * falloff * 0.5);
        }
    }

    lighting = clamp(lighting, 0.0, 1.0);
    vec3 color = texColor.rgb * lighting;
    float grey = dot(color, vec3(0.299, 0.587, 0.114));
    color = clamp(mix(vec3(grey), color, uSaturation), 0.0, 1.0);
    FragColor = vec4(color, texColor.a);
}
