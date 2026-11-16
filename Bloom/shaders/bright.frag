#version 330 core
// Pase de brillos: deja pasar solo lo que supera el umbral
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform float uThreshold;
uniform float uKnee;

void main()
{
    vec3  c    = texture(uTex, vUV).rgb;
    float br   = max(c.r, max(c.g, c.b));
    float knee = uThreshold * uKnee + 1e-5;

    float soft = clamp(br - uThreshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);

    float contrib = max(soft, br - uThreshold) / max(br, 1e-5);
    FragColor = vec4(c * contrib, 1.0);
}
