#version 330 core
// Desenfoque gaussiano separable
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec2  uStep;
uniform float uSigma;

void main()
{
    float s      = max(uSigma, 0.001);
    int   radius = int(min(ceil(s * 3.0), 32.0));
    float inv2s2 = 0.5 / (s * s);

    vec3  sum  = texture(uTex, vUV).rgb;
    float wsum = 1.0;
    for (int i = 1; i <= radius; ++i) {
        float w = exp(-float(i * i) * inv2s2);
        vec2  o = uStep * float(i);
        sum  += w * (texture(uTex, vUV + o).rgb + texture(uTex, vUV - o).rgb);
        wsum += 2.0 * w;
    }
    FragColor = vec4(sum / wsum, 1.0);
}
