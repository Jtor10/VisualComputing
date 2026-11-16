in vec3 vPos;
in vec3 vNormal;
in vec3 vColor;
out vec4 FragColor;
uniform vec3 uLightColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 albedo = vColor;
    if (N.y > 0.9 && abs(vPos.y) < 0.001) {
        float chk = mod(floor(vPos.x * 0.75) + floor(vPos.z * 0.75), 2.0);
        albedo = mix(vec3(0.34, 0.30, 0.26), vec3(0.20, 0.18, 0.16), chk);
    }
    vec3  L     = normalize(uLightPos - vPos);
    float NdotL = max(dot(N, L), 0.0);
    float bias  = 0.03 + 0.06 * (1.0 - NdotL);
    float sh    = shadowLookup(vPos + N * 0.02, bias, 1) * spotFactor(vPos);

    vec3 ambient = mix(vec3(0.045, 0.050, 0.060), vec3(0.085, 0.090, 0.105), N.y * 0.5 + 0.5);
    vec3 col = albedo * (ambient + uLightColor * NdotL * sh);
    FragColor = vec4(col, 1.0);
}
