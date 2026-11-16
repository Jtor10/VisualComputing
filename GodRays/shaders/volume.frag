in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uDepth;
uniform mat4  uInvVP;
uniform vec3  uCamPos;
uniform vec3  uBoxMin;
uniform vec3  uBoxMax;
uniform float uFogH;
uniform float uFogDensity;
uniform float uNoise;
uniform float uTime;
uniform int   uSteps;
uniform int   uRays;
uniform float uG;
uniform vec3  uLightColor;
uniform float uRayIntensity;
uniform vec3  uFogAmbient;
uniform float uExposure;

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}
float vnoise(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash13(i),                 hash13(i + vec3(1,0,0)), f.x),
                   mix(hash13(i + vec3(0,1,0)),   hash13(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash13(i + vec3(0,0,1)),   hash13(i + vec3(1,0,1)), f.x),
                   mix(hash13(i + vec3(0,1,1)),   hash13(i + vec3(1,1,1)), f.x), f.y), f.z);
}
float fbm(vec3 p) {
    float a = 0.5, s = 0.0;
    for (int i = 0; i < 3; i++) { s += a * vnoise(p); p *= 2.03; a *= 0.5; }
    return s;
}

float fogDensity(vec3 p) {
    float h = 1.0 - smoothstep(uFogH - 0.6, uFogH, p.y);
    float n = fbm(p * 0.45 + vec3(uTime * 0.03, 0.0, uTime * 0.02));
    float k = mix(1.0, clamp(n * 2.0, 0.0, 1.6), uNoise);
    return uFogDensity * h * k;
}

float phaseHG(float c) {
    float g = uG;
    return (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * c, 1.5);
}

vec2 rayBox(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax) {
    vec3 inv = 1.0 / rd;
    vec3 t0 = (bmin - ro) * inv, t1 = (bmax - ro) * inv;
    vec3 tmin = min(t0, t1), tmax = max(t0, t1);
    return vec2(max(max(tmin.x, tmin.y), tmin.z), min(min(tmax.x, tmax.y), tmax.z));
}

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec3  scene = texture(uScene, vUV).rgb;
    float depth = texture(uDepth, vUV).r;

    vec4 w = uInvVP * vec4(vUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec3 surf = w.xyz / w.w;
    vec3 ro = uCamPos;
    vec3 rd = surf - ro;
    float maxT = length(rd);
    rd /= maxT;

    vec3  scatter = vec3(0.0);
    float trans   = 1.0;

    vec2  tb = rayBox(ro, rd, uBoxMin, vec3(uBoxMax.x, uFogH, uBoxMax.z));
    float t0 = max(tb.x, 0.0);
    float t1 = min(tb.y, maxT);

    if (t1 > t0) {
        float stepLen = (t1 - t0) / float(uSteps);
        float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
        float t = t0 + stepLen * jitter;

        for (int i = 0; i < uSteps; i++) {
            vec3  p    = ro + rd * t;
            float dens = fogDensity(p);
            if (dens > 1e-4) {
                vec3 inL = uFogAmbient;
                if (uRays != 0) {
                    float vis = spotFactor(p);
                    if (vis > 0.0) vis *= shadowLookup(p, 0.08, 0);
                    if (vis > 0.0) {
                        vec3 Ld = normalize(p - uLightPos);
                        inL += uLightColor * uRayIntensity * vis * phaseHG(dot(Ld, -rd));
                    }
                }
                float stepT = exp(-dens * stepLen);
                scatter += trans * inL * (1.0 - stepT);
                trans   *= stepT;
            }
            t += stepLen;
        }
    }

    vec3 col = scene * trans + scatter;
    col = aces(col * uExposure);
    col = pow(col, vec3(1.0 / 2.2));
    FragColor = vec4(col, 1.0);
}
