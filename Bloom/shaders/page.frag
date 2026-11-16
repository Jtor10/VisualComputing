#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uPage;
uniform vec2  uResolution;
uniform vec2  uPageSize;
uniform vec2  uCenter;
uniform float uZoom;
uniform float uLamp;

float hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float vnoise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

void main()
{

    vec2 d     = vUV * uResolution - 0.5 * uResolution;
    vec2 texel = uCenter + vec2(d.x, -d.y) / uZoom;
    vec2 uv    = texel / uPageSize;

    vec3 ink = pow(texture(uPage, uv).rgb, vec3(2.2));

    vec2  q       = abs(uv - 0.5) - 0.5;
    float outside = max(q.x, q.y);

    // Lámpara arriba a la derecha
    float aspect = uResolution.x / uResolution.y;
    vec2  dl     = (vUV - vec2(0.90, 0.88)) * vec2(aspect, 1.0);
    float dist   = length(dl);
    float irr    = 1.35 + ((uLamp > 0.0) ? 0.5 * exp(-dist * dist * 2.0) : 0.0);

    vec3 col;
    if (outside < 0.0) {
        vec3 paper = ink * vec3(1.0, 0.97, 0.90);            // papel ligeramente cálido
        float grain = 0.5 * vnoise(texel * 0.9) + 0.5 * vnoise(texel * 0.23);
        paper *= 0.96 + 0.06 * grain;                        // fibra del papel
        paper *= mix(0.60, 1.0, smoothstep(0.0, 0.10, uv.x)); // sombra
        col = paper * irr;
    } else {
        float shadow = smoothstep(0.0, 0.05, outside);
        col = vec3(0.020, 0.017, 0.014) * (0.4 + 0.6 * shadow);
    }

    col += uLamp * vec3(1.0, 0.93, 0.80) * (1.0 - smoothstep(0.028, 0.035, dist));

    FragColor = vec4(col, 1.0);
}
