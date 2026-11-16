uniform sampler2D uShadow;
uniform mat4  uLightVP;
uniform vec3  uLightPos;
uniform vec3  uLightDir;
uniform float uCosInner;
uniform float uCosOuter;
uniform float uLNear;
uniform float uLFar;
uniform float uTexel;

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return 2.0 * uLNear * uLFar / (uLFar + uLNear - z * (uLFar - uLNear));
}

float spotFactor(vec3 p) {
    vec3 L = normalize(p - uLightPos);
    return smoothstep(uCosOuter, uCosInner, dot(L, uLightDir));
}

float shadowLookup(vec3 p, float bias, int r) {
    vec4 lc = uLightVP * vec4(p, 1.0);
    if (lc.w <= 0.0) return 0.0;
    vec3 ndc = lc.xyz / lc.w;
    if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0 || ndc.z > 1.0) return 0.0;
    vec2  uv  = ndc.xy * 0.5 + 0.5;
    float cur = linearizeDepth(ndc.z * 0.5 + 0.5);
    float sum = 0.0, n = 0.0;
    for (int x = -r; x <= r; x++)
        for (int y = -r; y <= r; y++) {
            float st = linearizeDepth(texture(uShadow, uv + vec2(x, y) * uTexel).r);
            sum += (cur - bias <= st) ? 1.0 : 0.0;
            n += 1.0;
        }
    return sum / n;
}
