#version 330 core
// Astigmatismo: el ojo enfoca distinto según el meridiano, así que cada punto de luz
// se convierte en una línea. Se simula con un desenfoque gaussiano 1D a lo largo de un eje.
// Además, la aberración cromática hace que el azul se desenfoque más que el rojo.
// (El desenfoque perpendicular, más leve, lo aplica blur.frag en un segundo pase.)
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec2  uTexel;
uniform float uAngle;
uniform float uSigma;
uniform float uChroma;

void main()
{
    vec2 dir = vec2(cos(uAngle), sin(uAngle)) * uTexel;

    vec3 sigma  = uSigma * vec3(1.0 - 0.30 * uChroma, 1.0, 1.0 + 0.40 * uChroma);
    vec3 inv2s2 = 0.5 / (sigma * sigma + 1e-4);
    int  radius = int(min(ceil(sigma.b * 3.0), 48.0));

    vec3 acc  = vec3(0.0);
    vec3 wsum = vec3(0.0);
    for (int i = -radius; i <= radius; ++i) {
        vec3 w = exp(-float(i * i) * inv2s2);                // un peso por canal
        acc  += texture(uTex, vUV + dir * float(i)).rgb * w;
        wsum += w;
    }
    FragColor = vec4(acc / wsum, 1.0);
}
