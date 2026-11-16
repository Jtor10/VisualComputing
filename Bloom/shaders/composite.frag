#version 330 core
// Pase final: combina la imagen óptica con astigmatismo, con el bloom,
// aplica los efectos propios de la catarata, tone mapping y gamma.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom0;   // 1/2 de resolución
uniform sampler2D uBloom1;   // 1/4
uniform sampler2D uBloom2;   // 1/8
uniform sampler2D uBloom3;   // 1/16
uniform sampler2D uBloom4;   // 1/32 (halo más ancho)

uniform float uBloomIntensity;
uniform float uBloomWeights[5];
uniform float uCataract;     // severidad (0 = sin catarata)
uniform float uExposure;
uniform float uAspect;

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
    return mix(mix(hash(i),               hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float fbm(vec2 p)
{
    float a = 0.5, s = 0.0;
    for (int i = 0; i < 4; ++i) { s += a * vnoise(p); p = p * 2.03 + 17.1; a *= 0.5; }
    return s;
}

vec3 aces(vec3 x)   // Narkowicz
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 sharp = texture(uScene, vUV).rgb;
    vec3 b0 = texture(uBloom0, vUV).rgb;
    vec3 b1 = texture(uBloom1, vUV).rgb;
    vec3 b2 = texture(uBloom2, vUV).rgb;
    vec3 b3 = texture(uBloom3, vUV).rgb;
    vec3 b4 = texture(uBloom4, vUV).rgb;

    // Bloom
    vec3 glow = (b0 * uBloomWeights[0] + b1 * uBloomWeights[1] + b2 * uBloomWeights[2]
               + b3 * uBloomWeights[3] + b4 * uBloomWeights[4]) * uBloomIntensity;
    vec3 col = sharp + glow;

    // Catarata
        if (uCataract > 0.0) {
        // Luz dispersada a ángulos grandes
        vec3 haze = (b2 + b3 + b4) / 3.0;

        // Opacidades del cristalino
        vec2  p   = (vUV - 0.5) * vec2(uAspect, 1.0);
        float r   = length(p);
        vec2  dir = (r > 1e-4) ? p / r : vec2(1.0, 0.0);

        float spokes   = fbm(dir * 3.5 + r * 0.7);                       // rayos radiales
        float cortical = smoothstep(0.22, 0.85, r) * smoothstep(0.40, 0.75, spokes);
        float psc      = exp(-r * r / 0.05) * (0.55 + 0.45 * fbm(p * 7.0)); // mancha central
        float opacity  = clamp((0.9 * cortical + 0.6 * psc) * uCataract * 1.3, 0.0, 1.0);

        col = mix(col, haze + 0.15 * sharp, 0.45 * uCataract);           // baja el contraste
        col = mix(col, haze * vec3(0.85, 0.78, 0.65), opacity * 0.75);   // zonas opacas
        col *= mix(vec3(1.0), vec3(1.0, 0.84, 0.55), uCataract);         // amarilleo
        float l = dot(col, vec3(0.2126, 0.7152, 0.0722));
        col = mix(col, vec3(l), 0.25 * uCataract);                       // pérdida de saturación
    }

    // Salida
    col *= uExposure;
    col  = aces(col);

    float vig = 1.0 - smoothstep(0.45, 1.25, length((vUV - 0.5) * vec2(uAspect, 1.0) * 1.2));
    col *= mix(0.65, 1.0, vig);

    col = pow(col, vec3(1.0 / 2.2));
    col += (hash(gl_FragCoord.xy) - 0.5) / 255.0;
    FragColor = vec4(col, 1.0);
}
