#version 330 core

uniform float uExposure;
uniform float uOperator;
uniform float uContrast;
uniform float uTemperature;
uniform int   uMode;         // 0 escena, 1 plot
uniform int   uUseTexture;   // 0 sintética, 1 imagen
uniform sampler2D uScene;

in  vec2 vTexCoord;
out vec4 outColor;

// ── Escena HDR sintética ────────────────────────────────────────────────
vec3 hdrScene(vec2 p) {
    vec3 sky = mix(vec3(0.10, 0.16, 0.30),
                   vec3(0.50, 0.66, 0.92),
                   clamp(p.y, 0.0, 1.0));

    vec2  sun  = vec2(0.32, 0.70);
    float ds   = distance(p, sun);
    float core = smoothstep(0.11, 0.0, ds);
    float glow = exp(-ds * 7.0);
    vec3  sunC = (core * 40.0 + glow * 6.0) * vec3(1.00, 0.95, 0.80);

    float g1 = smoothstep(0.035, 0.0, distance(p, vec2(0.64, 0.42)));
    float g2 = smoothstep(0.025, 0.0, distance(p, vec2(0.78, 0.30)));
    vec3  glints = (g1 * 16.0 + g2 * 12.0) * vec3(1.0, 0.9, 0.7);

    float ridge = step(p.y, 0.20 + 0.05 * sin(p.x * 13.0));
    vec3  fg    = vec3(0.03, 0.04, 0.045);

    return mix(sky + sunC + glints, fg, ridge);
}

// ── Operadores ──────────────────────────────────────────────────────────
vec3 reinhard(vec3 c) { return c / (1.0 + c); }

vec3 aces(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 applyOperator(vec3 L) {
    if      (uOperator < 0.5) return clamp(L, 0.0, 1.0);
    else if (uOperator < 1.5) return reinhard(L);
    else                      return aces(L);
}

// ── Plot de curvas ──────────────────────────────────────────────────────
vec3 plotCurves(vec2 p) {
    vec3 col = vec3(0.08);
    if (abs(p.y - 0.05) < 0.0015) col = vec3(0.4);
    if (abs(p.x - 0.05) < 0.0015) col = vec3(0.4);

    float Lx = p.x * 8.0;
    float yClamp    = clamp(Lx, 0.0, 1.0);
    float yReinhard = Lx / (1.0 + Lx);
    float yAces     = aces(vec3(Lx)).r;

    float w = 0.008;
    if (abs(p.y - yClamp)    < w) col = vec3(1.0, 0.2, 0.2);
    if (abs(p.y - yReinhard) < w) col = vec3(0.2, 1.0, 0.3);
    if (abs(p.y - yAces)     < w) col = vec3(0.3, 0.5, 1.0);

    float marker = exp2(uExposure);
    if (abs(Lx - marker) < 0.02) col = mix(col, vec3(1.0), 0.5);

    return col;
}

void main() {
    if (uMode == 1) {
        outColor = vec4(plotCurves(vTexCoord), 1.0);
        return;
    }

    // ── SELECTOR: sintética o imagen ────────────────────────────────────
    vec3 L;
    if (uUseTexture == 1) {
        L = texture(uScene, vTexCoord).rgb;
        // Ajuste opcional: si es una foto LDR, multiplicar para simular HDR.
        // Descomenta si cargas .jpg/.png y quieres ver high range:
        L *= 10.0;
    } else {
        L = hdrScene(vTexCoord);
    }

    L *= exp2(uExposure);
    vec3 m = applyOperator(L);

    m = clamp((m - 0.5) * uContrast + 0.5, 0.0, 1.0);
    m *= vec3(1.0 + uTemperature, 1.0, 1.0 - uTemperature);
    m = pow(clamp(m, 0.0, 1.0), vec3(1.0 / 2.2));

    outColor = vec4(m, 1.0);
}
