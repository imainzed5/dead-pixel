#version 330 core

in vec2 vUv;
in vec4 vColor;
in vec2 vScreenPos;

uniform sampler2D uTexture;

// Lighting uniforms
uniform float uAmbientLight;                  // 0.0 = pitch black, 1.0 = full daylight
uniform vec3 uAmbientColor;                   // tint color for ambient light
uniform int uLightCount;                      // number of active point lights (max 16)
uniform vec2 uLightPositions[16];             // screen-space positions
uniform vec3 uLightColors[16];                // RGB color of each light
uniform float uLightRadii[16];               // falloff radius in screen pixels

out vec4 FragColor;

void main()
{
    vec4 texColor = texture(uTexture, vUv) * vColor;

    // Compute lighting contribution
    vec3 lighting = uAmbientColor * uAmbientLight;

    for (int i = 0; i < uLightCount; ++i)
    {
        float dist = distance(vScreenPos, uLightPositions[i]);
        float attenuation = clamp(1.0 - dist / uLightRadii[i], 0.0, 1.0);
        attenuation *= attenuation; // quadratic falloff for softer edges
        lighting += uLightColors[i] * attenuation;
    }

    lighting = clamp(lighting, 0.0, 1.0);
    FragColor = vec4(texColor.rgb * lighting, texColor.a);
}
