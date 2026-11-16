#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static const int   SHADOW_RES   = 2048;
static const float ROOM_IN      = 5.7f;
static const float ROOM_OUT     = 6.3f;
static const float ROOM_H       = 5.0f;
static const float LIGHT_DIST   = 24.0f;
static const float LIGHT_NEAR   = 2.0f;
static const float LIGHT_FAR    = 60.0f;
static const float LIGHT_FOV    = 60.0f;
static const float SPOT_OUTER   = 28.0f;
static const float SPOT_INNER   = 22.0f;

struct State {
    glm::vec3 camPos{-4.8f, 1.7f, -1.0f};
    float yaw = 5.0f, pitch = 6.0f;
    float lightAz = 19.0f, lightEl = 30.0f;
    float fogH = ROOM_H * 0.5f;
    float fogDensity = 0.12f;
    bool  rays = true;
    bool  noise = true;
} S;

static void setPreset(int n) {
    if (n == 1) { S.camPos = {-4.8f, 1.7f, -1.0f}; S.yaw =   5.0f; S.pitch = 6.0f; }
    if (n == 2) { S.camPos = { 0.0f, 1.7f,  5.0f}; S.yaw = -90.0f; S.pitch = 4.0f; }
    if (n == 3) { S.camPos = {-4.8f, 1.7f,  4.8f}; S.yaw = -35.0f; S.pitch = 4.0f; }
}

// --- Lectura de archivos -----------------------------------------------------
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { fprintf(stderr, "No se pudo abrir: %s\n", path.c_str()); exit(1); }
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

// --- Utilidades OpenGL -------------------------------------------------------
static GLuint compileShader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "Error compilando shader:\n%s\n", log);
        exit(1);
    }
    return s;
}

static GLuint makeProgram(const std::string& vs, const std::string& fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        fprintf(stderr, "Error enlazando programa:\n%s\n", log);
        exit(1);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

static void u1f(GLuint p, const char* n, float v)            { glUniform1f(glGetUniformLocation(p, n), v); }
static void u1i(GLuint p, const char* n, int v)              { glUniform1i(glGetUniformLocation(p, n), v); }
static void u3f(GLuint p, const char* n, const glm::vec3& v) { glUniform3fv(glGetUniformLocation(p, n), 1, glm::value_ptr(v)); }
static void um4(GLuint p, const char* n, const glm::mat4& m) { glUniformMatrix4fv(glGetUniformLocation(p, n), 1, GL_FALSE, glm::value_ptr(m)); }

// --- Geometría ---------------------------------------------------------------
struct Vertex { glm::vec3 p, n, c; };
static std::vector<Vertex> gVerts;

static void addBox(glm::vec3 mn, glm::vec3 mx, glm::vec3 col) {
    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n) {
        if (glm::dot(glm::cross(b - a, c - a), n) < 0.0f) std::swap(b, d);
        gVerts.push_back({a, n, col}); gVerts.push_back({b, n, col}); gVerts.push_back({c, n, col});
        gVerts.push_back({a, n, col}); gVerts.push_back({c, n, col}); gVerts.push_back({d, n, col});
    };
    quad({mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},{mx.x,mn.y,mx.z}, { 1, 0, 0});
    quad({mn.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},{mn.x,mn.y,mx.z}, {-1, 0, 0});
    quad({mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},{mx.x,mx.y,mn.z}, { 0, 1, 0});
    quad({mn.x,mn.y,mn.z},{mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mn.y,mn.z}, { 0,-1, 0});
    quad({mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z}, { 0, 0, 1});
    quad({mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z}, { 0, 0,-1});
}

static void buildScene() {
    const glm::vec3 wall(0.78f, 0.74f, 0.68f);
    const glm::vec3 roof(0.50f, 0.50f, 0.52f);
    const glm::vec3 pillar(0.62f, 0.60f, 0.57f);
    const glm::vec3 crate(0.50f, 0.33f, 0.20f);
    const float I = ROOM_IN, O = ROOM_OUT, H = ROOM_H;

    addBox({-O, -0.4f, -O}, {O, 0.0f, O}, glm::vec3(0.3f));
    addBox({-O, H, -O}, {O, H + 0.4f, O}, roof);
    addBox({-O, 0, -O}, {O, H, -I}, wall);
    addBox({-O, 0,  I}, {O, H,  O}, wall);
    addBox({-O, 0, -I}, {-I, H,  I}, wall);

    const float sill = 1.6f, head = 4.0f;
    addBox({I, 0, -I}, {O, sill, I}, wall);
    addBox({I, head, -I}, {O, H, I}, wall);
    addBox({I, sill, -I},    {O, head, -3.9f}, wall);
    addBox({I, sill, -2.5f}, {O, head, -0.7f}, wall);
    addBox({I, sill,  0.7f}, {O, head,  2.5f}, wall);
    addBox({I, sill,  3.9f}, {O, head,  I},    wall);

    addBox({1.2f, 0, -2.1f}, {1.8f, H, -1.5f}, pillar);
    addBox({-1.6f, 0, 1.4f}, {-1.0f, H, 2.0f}, pillar);
    addBox({-3.2f, 0, -3.6f}, {-2.0f, 1.1f, -2.4f}, crate);
}

// --- Framebuffers ------------------------------------------------------------
struct SceneFBO { GLuint fbo = 0, color = 0, depth = 0; int w = 0, h = 0; };

static void createSceneFBO(SceneFBO& f, int w, int h) {
    if (f.fbo) {
        glDeleteFramebuffers(1, &f.fbo);
        glDeleteTextures(1, &f.color);
        glDeleteTextures(1, &f.depth);
    }
    f.w = w; f.h = h;
    glGenTextures(1, &f.color);
    glBindTexture(GL_TEXTURE_2D, f.color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &f.depth);
    glBindTexture(GL_TEXTURE_2D, f.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &f.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, f.color, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, f.depth, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO de escena incompleto\n");
        exit(1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void createShadowFBO(GLuint& fbo, GLuint& tex) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_RES, SHADOW_RES, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO de sombra incompleto\n");
        exit(1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// --- Entrada -----------------------------------------------------------------
static double gLastX = 0, gLastY = 0;
static bool   gFirstMouse = true;

static void mouseCb(GLFWwindow*, double x, double y) {
    if (gFirstMouse) { gLastX = x; gLastY = y; gFirstMouse = false; }
    S.yaw   += float(x - gLastX) * 0.1f;
    S.pitch -= float(y - gLastY) * 0.1f;
    S.pitch  = glm::clamp(S.pitch, -89.0f, 89.0f);
    gLastX = x; gLastY = y;
}

static void keyCb(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, 1); break;
        case GLFW_KEY_R: S.rays  = !S.rays;  break;
        case GLFW_KEY_N: S.noise = !S.noise; break;
        case GLFW_KEY_F: S.fogH = std::max(0.5f, S.fogH - 0.25f); break;
        case GLFW_KEY_G: S.fogH = std::min(ROOM_H - 0.2f, S.fogH + 0.25f); break;
        case GLFW_KEY_C: S.fogDensity = std::max(0.01f, S.fogDensity * 0.8f); break;
        case GLFW_KEY_V: S.fogDensity = std::min(1.0f,  S.fogDensity * 1.25f); break;
        case GLFW_KEY_1: setPreset(1); break;
        case GLFW_KEY_2: setPreset(2); break;
        case GLFW_KEY_3: setPreset(3); break;
    }
}

static glm::vec3 camDir() {
    float y = glm::radians(S.yaw), p = glm::radians(S.pitch);
    return glm::normalize(glm::vec3(cosf(p) * cosf(y), sinf(p), cosf(p) * sinf(y)));
}

static void processInput(GLFWwindow* w, float dt) {
    glm::vec3 fwd = camDir();
    glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
    float speed = (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 6.0f : 2.5f) * dt;
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) S.camPos += fwd * speed;
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) S.camPos -= fwd * speed;
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) S.camPos += right * speed;
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) S.camPos -= right * speed;
    if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS)        S.camPos.y += speed;
    if (glfwGetKey(w, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) S.camPos.y -= speed;
    S.camPos.x = glm::clamp(S.camPos.x, -ROOM_IN + 0.3f, ROOM_IN - 0.3f);
    S.camPos.z = glm::clamp(S.camPos.z, -ROOM_IN + 0.3f, ROOM_IN - 0.3f);
    S.camPos.y = glm::clamp(S.camPos.y, 0.3f, ROOM_H - 0.3f);

    if (glfwGetKey(w, GLFW_KEY_LEFT)  == GLFW_PRESS) S.lightAz -= 30.0f * dt;
    if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) S.lightAz += 30.0f * dt;
    if (glfwGetKey(w, GLFW_KEY_UP)    == GLFW_PRESS) S.lightEl += 20.0f * dt;
    if (glfwGetKey(w, GLFW_KEY_DOWN)  == GLFW_PRESS) S.lightEl -= 20.0f * dt;
    S.lightAz = glm::clamp(S.lightAz, -40.0f, 40.0f);
    S.lightEl = glm::clamp(S.lightEl, 12.0f, 60.0f);
}

static void saveShot(const char* path, int w, int h) {
    std::vector<unsigned char> px(size_t(w) * h * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    FILE* f = fopen(path, "wb");
    if (!f) { perror("saveShot"); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y) fwrite(&px[size_t(y) * w * 3], 1, size_t(w) * 3, f);
    fclose(f);
    printf("Captura guardada en %s\n", path);
}

// --- main --------------------------------------------------------------------
int main(int argc, char** argv) {
    std::string shotPath;
    int view = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) shotPath = argv[++i];
        else if (!strcmp(argv[i], "--view") && i + 1 < argc) view = atoi(argv[++i]);
    }
    if (view) setPreset(view);

    if (!glfwInit()) { fprintf(stderr, "No se pudo iniciar GLFW\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(1280, 720, "God rays volumetricos", nullptr, nullptr);
    if (!win) { fprintf(stderr, "No se pudo crear la ventana\n"); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(shotPath.empty() ? 1 : 0);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { fprintf(stderr, "No se pudo iniciar GLEW\n"); return 1; }
    glGetError();

    if (shotPath.empty()) {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPosCallback(win, mouseCb);
    }
    glfwSetKeyCallback(win, keyCb);

    // --- Carga de shaders desde disco ---
    const std::string ver = "#version 330 core\n";
    const std::string common = readFile("shaders/common.glsl");

    GLuint progShadow = makeProgram(readFile("shaders/shadow.vert"), readFile("shaders/shadow.frag"));
    GLuint progScene  = makeProgram(readFile("shaders/scene.vert"),
                                    ver + common + readFile("shaders/scene.frag"));
    GLuint progVolume = makeProgram(readFile("shaders/fullscreen.vert"),
                                    ver + common + readFile("shaders/volume.frag"));

    // --- Geometría ---
    buildScene();
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, gVerts.size() * sizeof(Vertex), gVerts.data(), GL_STATIC_DRAW);
    for (int i = 0; i < 3; i++) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3) * i));
    }
    GLuint emptyVao;
    glGenVertexArrays(1, &emptyVao);
    const GLsizei vertCount = GLsizei(gVerts.size());

    // --- Framebuffers ---
    GLuint shadowFBO, shadowTex;
    createShadowFBO(shadowFBO, shadowTex);
    SceneFBO scene;

    double lastT = glfwGetTime(), titleT = lastT;
    int frames = 0, shotFrames = 0;

    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = float(now - lastT);
        lastT = now;
        glfwPollEvents();
        if (shotPath.empty()) processInput(win, dt);

        int W, H;
        glfwGetFramebufferSize(win, &W, &H);
        if (W == 0 || H == 0) continue;
        if (W != scene.w || H != scene.h) createSceneFBO(scene, W, H);

        const glm::vec3 target(0.0f, 0.5f, 0.0f);
        float az = glm::radians(S.lightAz), el = glm::radians(S.lightEl);
        glm::vec3 lightPos = target + LIGHT_DIST * glm::vec3(cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az));
        glm::vec3 lightDir = glm::normalize(target - lightPos);
        glm::mat4 lightVP = glm::perspective(glm::radians(LIGHT_FOV), 1.0f, LIGHT_NEAR, LIGHT_FAR) *
                            glm::lookAt(lightPos, target, glm::vec3(0, 1, 0));
        glm::vec3 lightColor = glm::vec3(1.0f, 0.86f, 0.60f) * 4.0f;

        auto setLightUniforms = [&](GLuint p) {
            um4(p, "uLightVP", lightVP);
            u3f(p, "uLightPos", lightPos);
            u3f(p, "uLightDir", lightDir);
            u1f(p, "uCosInner", cosf(glm::radians(SPOT_INNER)));
            u1f(p, "uCosOuter", cosf(glm::radians(SPOT_OUTER)));
            u1f(p, "uLNear", LIGHT_NEAR);
            u1f(p, "uLFar", LIGHT_FAR);
            u1f(p, "uTexel", 1.0f / SHADOW_RES);
            u1i(p, "uShadow", 0);
        };

        // 1) SHADOW PASS
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glViewport(0, 0, SHADOW_RES, SHADOW_RES);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(progShadow);
        um4(progShadow, "uLightVP", lightVP);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, vertCount);

        // 2) SCENE PASS
        glBindFramebuffer(GL_FRAMEBUFFER, scene.fbo);
        glViewport(0, 0, W, H);
        glCullFace(GL_BACK);
        glClearColor(1.1f, 1.35f, 1.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(70.0f), float(W) / float(H), 0.1f, 100.0f);
        glm::vec3 fwd = camDir();
        glm::mat4 viewM = glm::lookAt(S.camPos, S.camPos + fwd, glm::vec3(0, 1, 0));
        glm::mat4 VP = proj * viewM;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, shadowTex);
        glUseProgram(progScene);
        um4(progScene, "uVP", VP);
        u3f(progScene, "uLightColor", lightColor);
        setLightUniforms(progScene);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, vertCount);

        // 3) VOLUMETRIC / COMPOSITE PASS
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, W, H);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, shadowTex);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, scene.color);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, scene.depth);

        glUseProgram(progVolume);
        setLightUniforms(progVolume);
        u1i(progVolume, "uScene", 1);
        u1i(progVolume, "uDepth", 2);
        um4(progVolume, "uInvVP", glm::inverse(VP));
        u3f(progVolume, "uCamPos", S.camPos);
        u3f(progVolume, "uBoxMin", glm::vec3(-ROOM_IN, 0.0f, -ROOM_IN));
        u3f(progVolume, "uBoxMax", glm::vec3(ROOM_IN, ROOM_H, ROOM_IN));
        u1f(progVolume, "uFogH", S.fogH);
        u1f(progVolume, "uFogDensity", S.fogDensity);
        u1f(progVolume, "uNoise", S.noise ? 1.0f : 0.0f);
        u1f(progVolume, "uTime", float(now));
        u1i(progVolume, "uSteps", 72);
        u1i(progVolume, "uRays", S.rays ? 1 : 0);
        u1f(progVolume, "uG", 0.55f);
        u3f(progVolume, "uLightColor", lightColor);
        u1f(progVolume, "uRayIntensity", 1.5f);
        u3f(progVolume, "uFogAmbient", glm::vec3(0.05f, 0.058f, 0.075f));
        u1f(progVolume, "uExposure", 1.0f);
        glBindVertexArray(emptyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glEnable(GL_DEPTH_TEST);

        if (!shotPath.empty() && ++shotFrames == 3) {
            saveShot(shotPath.c_str(), W, H);
            break;
        }

        glfwSwapBuffers(win);

        frames++;
        if (now - titleT > 0.5) {
            char t[256];
            snprintf(t, sizeof(t), "God rays | %.0f FPS | rayos:%s ruido:%s | niebla h=%.2f dens=%.3f | luz az=%.0f el=%.0f",
                     frames / (now - titleT), S.rays ? "ON" : "OFF", S.noise ? "ON" : "OFF",
                     S.fogH, S.fogDensity, S.lightAz, S.lightEl);
            glfwSetWindowTitle(win, t);
            titleT = now;
            frames = 0;
        }
    }

    glfwTerminate();
    return 0;
}
