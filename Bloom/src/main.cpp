#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "PageTexture.hpp"
#include "RenderTarget.hpp"
#include "Shader.hpp"

#ifndef DEFAULT_SHADER_DIR
#define DEFAULT_SHADER_DIR "shaders"
#endif

// parámetros
struct VisionParams {
    bool  astigmatism = true;
    bool  cataract    = true;
    bool  bloom       = true;
    bool  lamp        = true;
    bool  showOriginal = false;      // visión normal

    float astigAngleDeg = 90.f;     // eje en el que se alarga el desenfoque
    float astigSigma    = 3.0f;     // px (a resolución completa)
    float astigChroma   = 0.6f;
    float cataractSeverity = 0.5f;
    float bloomIntensity   = 0.6f;
    float bloomThreshold   = 1.0f;
    float exposure         = 1.0f;
};

struct View {
    float zoom = 1.f;
    float cx = 0.f, cy = 0.f;
};


class Renderer {
public:
    static constexpr int LEVELS = 5;

    Renderer(const std::string& dir, const std::string& image, float scale)
        : page_(dir + "/fullscreen.vert", dir + "/page.frag"),
          astig_(dir + "/fullscreen.vert", dir + "/astigmatism.frag"),
          blur_(dir + "/fullscreen.vert", dir + "/blur.frag"),
          bright_(dir + "/fullscreen.vert", dir + "/bright.frag"),
          comp_(dir + "/fullscreen.vert", dir + "/composite.frag"),
          scale_(scale)
    {
        glGenVertexArrays(1, &vao_);            // VAO vacío: el vértice sale de gl_VertexID
        pageTex_ = createPageTexture(image, pageW_, pageH_);
    }

    ~Renderer()
    {
        glDeleteTextures(1, &pageTex_);
        glDeleteVertexArrays(1, &vao_);
    }

    int pageW() const { return pageW_; }
    int pageH() const { return pageH_; }

    void render(const VisionParams& p, const View& v, int fbW, int fbH)
    {
        ensureTargets(fbW, fbH);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        const float s = float(scene_.w) / float(fbW);   // escala real del buffer interno

        // 1) Escena HDR
        scene_.bind();
        page_.use();
        bindTex(0, pageTex_);
        page_.setInt("uPage", 0);
        page_.setVec2("uResolution", float(scene_.w), float(scene_.h));
        page_.setVec2("uPageSize", float(pageW_), float(pageH_));
        page_.setVec2("uCenter", v.cx, v.cy);
        page_.setFloat("uZoom", v.zoom * s);
        page_.setFloat("uLamp", p.lamp ? 15.f : 0.f);
        draw();
        GLuint cur = scene_.tex;

        const bool effects = !p.showOriginal;
        const float cat = (effects && p.cataract) ? p.cataractSeverity : 0.f;

        // 2) Astigmatismo
        if (effects && p.astigmatism && p.astigSigma > 0.05f) {
            const float sigma = p.astigSigma * s;
            const float ang   = p.astigAngleDeg * 3.14159265f / 180.f;

            RenderTarget& t = freeTarget(cur);
            t.bind();
            astig_.use();
            bindTex(0, cur);
            astig_.setInt("uTex", 0);
            astig_.setVec2("uTexel", 1.f / t.w, 1.f / t.h);
            astig_.setFloat("uAngle", ang);
            astig_.setFloat("uSigma", sigma);
            astig_.setFloat("uChroma", p.astigChroma);
            draw();
            cur = t.tex;

            // desenfoque leve en el eje perpendicular
            RenderTarget& t2 = freeTarget(cur);
            blurPass(cur, t2, -std::sin(ang), std::cos(ang), 0.25f * sigma);
            cur = t2.tex;
        }

        // 3) Catarata
        if (cat > 0.f) {
            const float sigma = (0.5f + 3.0f * cat) * s;
            RenderTarget& a = freeTarget(cur);
            blurPass(cur, a, 1.f, 0.f, sigma);
            RenderTarget& b = freeTarget(a.tex);
            blurPass(a.tex, b, 0.f, 1.f, sigma);
            cur = b.tex;
        }

        // 4) Bloom
        const bool bloomOn = effects && p.bloom;
        if (bloomOn || cat > 0.f) {
            // Con catarata el umbral baja hacia 0: toda la imagen dispersa luz (velo).
            float thr = bloomOn ? p.bloomThreshold : 0.f;
            thr *= (1.f - cat);

            bloomRes_[0].bind();
            bright_.use();
            bindTex(0, cur);
            bright_.setInt("uTex", 0);
            bright_.setFloat("uThreshold", thr);
            bright_.setFloat("uKnee", 0.5f);
            draw();

            for (int i = 0; i < LEVELS; ++i) {
                GLuint in = (i == 0) ? bloomRes_[0].tex : bloomRes_[i - 1].tex;
                blurPass(in, bloomTmp_[i], 1.f, 0.f, 2.0f);
                blurPass(bloomTmp_[i].tex, bloomRes_[i], 0.f, 1.f, 2.0f);
            }
        }

        // 5) Composición final
        float w[LEVELS] = {0.30f, 0.25f, 0.20f, 0.15f, 0.10f};
        float sum = 0.f;
        for (int i = 0; i < LEVELS; ++i) {
            w[i] *= 1.f + 2.0f * cat * float(i) / float(LEVELS - 1);
            sum += w[i];
        }
        for (float& x : w) x /= sum;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbW, fbH);
        comp_.use();
        bindTex(0, cur);
        comp_.setInt("uScene", 0);
        for (int i = 0; i < LEVELS; ++i) {
            bindTex(1 + i, bloomRes_[i].tex);
            comp_.setInt(("uBloom" + std::to_string(i)).c_str(), 1 + i);
        }
        comp_.setFloats("uBloomWeights", w, LEVELS);
        comp_.setFloat("uBloomIntensity", bloomOn ? p.bloomIntensity : 0.f);
        comp_.setFloat("uCataract", cat);
        comp_.setFloat("uExposure", p.exposure);
        comp_.setFloat("uAspect", float(fbW) / float(fbH));
        draw();
    }

private:
    void draw() { glBindVertexArray(vao_); glDrawArrays(GL_TRIANGLES, 0, 3); }

    static void bindTex(int unit, GLuint tex)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, tex);
    }

    // Devuelve un buffer interno distinto del que se está leyendo.
    RenderTarget& freeTarget(GLuint curTex) { return curTex == ping_.tex ? pong_ : ping_; }

    void blurPass(GLuint src, RenderTarget& dst, float dx, float dy, float sigma)
    {
        dst.bind();
        blur_.use();
        bindTex(0, src);
        blur_.setInt("uTex", 0);
        blur_.setVec2("uStep", dx / dst.w, dy / dst.h);
        blur_.setFloat("uSigma", sigma);
        draw();
    }

    void ensureTargets(int fbW, int fbH)
    {
        int iw = std::max(16, int(fbW * scale_));
        int ih = std::max(16, int(fbH * scale_));
        if (iw == scene_.w && ih == scene_.h) return;

        scene_.create(iw, ih);
        ping_.create(iw, ih);
        pong_.create(iw, ih);
        for (int i = 0; i < LEVELS; ++i) {
            iw = std::max(1, iw / 2);
            ih = std::max(1, ih / 2);
            bloomRes_[i].create(iw, ih);
            bloomTmp_[i].create(iw, ih);
        }
    }

    Shader page_, astig_, blur_, bright_, comp_;
    float  scale_;
    GLuint vao_ = 0, pageTex_ = 0;
    int    pageW_ = 0, pageH_ = 0;
    RenderTarget scene_, ping_, pong_;
    std::array<RenderTarget, LEVELS> bloomRes_, bloomTmp_;
};

// Entrada
struct App {
    VisionParams p;
    View  v;
    bool  dirty = true;
    bool  shot  = false;
    bool  dragging = false;
    double lastX = 0, lastY = 0;
    int   fbW = 0, fbH = 0, winW = 1, winH = 1;
    float pageW = 1, pageH = 1;
    float lastMs = 0;
};

static void resetView(App& a)
{
    a.v.zoom = 0.8f * float(a.fbW) / a.pageW;
    a.v.cx   = a.pageW * 0.5f;
    a.v.cy   = (float(a.fbH) * 0.5f - 20.f) / a.v.zoom;
}

static void onKey(GLFWwindow* w, int key, int, int action, int)
{
    if (action == GLFW_RELEASE) return;
    App& a = *static_cast<App*>(glfwGetWindowUserPointer(w));
    VisionParams& p = a.p;
    const bool press = (action == GLFW_PRESS);
    auto clampf = [](float& x, float lo, float hi) { x = std::min(hi, std::max(lo, x)); };

    switch (key) {
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, GLFW_TRUE); return;
    case GLFW_KEY_1: if (press) p.astigmatism  = !p.astigmatism;  break;
    case GLFW_KEY_2: if (press) p.cataract     = !p.cataract;     break;
    case GLFW_KEY_3: if (press) p.bloom        = !p.bloom;        break;
    case GLFW_KEY_L: if (press) p.lamp         = !p.lamp;         break;
    case GLFW_KEY_O: if (press) p.showOriginal = !p.showOriginal; break;
    case GLFW_KEY_LEFT:  p.astigAngleDeg -= 5.f; break;
    case GLFW_KEY_RIGHT: p.astigAngleDeg += 5.f; break;
    case GLFW_KEY_UP:    p.astigSigma += 0.5f;   break;
    case GLFW_KEY_DOWN:  p.astigSigma -= 0.5f;   break;
    case GLFW_KEY_Z: p.cataractSeverity -= 0.05f; break;
    case GLFW_KEY_X: p.cataractSeverity += 0.05f; break;
    case GLFW_KEY_V: p.bloomIntensity   -= 0.05f; break;
    case GLFW_KEY_B: p.bloomIntensity   += 0.05f; break;
    case GLFW_KEY_N: p.bloomThreshold   -= 0.05f; break;
    case GLFW_KEY_M: p.bloomThreshold   += 0.05f; break;
    case GLFW_KEY_D: p.exposure -= 0.05f; break;
    case GLFW_KEY_E: p.exposure += 0.05f; break;
    case GLFW_KEY_P: if (press) a.shot = true; break;
    case GLFW_KEY_R:
        if (press) { p = VisionParams(); resetView(a); }
        break;
    default: return;
    }
    p.astigAngleDeg = std::fmod(p.astigAngleDeg + 180.f, 180.f);
    clampf(p.astigSigma, 0.f, 12.f);
    clampf(p.cataractSeverity, 0.f, 1.f);
    clampf(p.bloomIntensity, 0.f, 3.f);
    clampf(p.bloomThreshold, 0.f, 4.f);
    clampf(p.exposure, 0.2f, 4.f);
    a.dirty = true;
}

static void onScroll(GLFWwindow* w, double, double dy)
{
    App& a = *static_cast<App*>(glfwGetWindowUserPointer(w));
    a.v.zoom = std::min(6.f, std::max(0.15f, a.v.zoom * std::pow(1.1f, float(dy))));
    a.dirty = true;
}

static void onMouseButton(GLFWwindow* w, int button, int action, int)
{
    App& a = *static_cast<App*>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        a.dragging = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &a.lastX, &a.lastY);
    }
}

static void onCursor(GLFWwindow* w, double x, double y)
{
    App& a = *static_cast<App*>(glfwGetWindowUserPointer(w));
    if (a.dragging) {
        float k = float(a.fbW) / float(std::max(1, a.winW));   // HiDPI
        a.v.cx -= float(x - a.lastX) * k / a.v.zoom;
        a.v.cy -= float(y - a.lastY) * k / a.v.zoom;
        a.dirty = true;
    }
    a.lastX = x; a.lastY = y;
}

static void onResize(GLFWwindow* w, int, int)
{
    App& a = *static_cast<App*>(glfwGetWindowUserPointer(w));
    glfwGetWindowSize(w, &a.winW, &a.winH);
    a.dirty = true;
}

static void onRefresh(GLFWwindow* w)
{
    static_cast<App*>(glfwGetWindowUserPointer(w))->dirty = true;
}

static void saveScreenshot(int w, int h)
{
    static int n = 0;
    std::vector<unsigned char> buf(size_t(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, buf.data());

    char name[64];
    std::snprintf(name, sizeof(name), "captura_%03d.ppm", n++);
    FILE* f = std::fopen(name, "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y) std::fwrite(&buf[size_t(y) * w * 3], 1, size_t(w) * 3, f);
    std::fclose(f);
    std::cout << "Captura guardada: " << name << "\n";
}

static void printHelp()
{
    std::cout <<
        "\n=== BloomVision - controles ===\n"
        " 1 / 2 / 3   astigmatismo / catarata / bloom  (on/off)\n"
        " O           ver original (vision normal)      L  lampara on/off\n"
        " Izq / Der   eje del astigmatismo (-/+ 5 grados)\n"
        " Arr / Abj   fuerza del astigmatismo (sigma +/-)\n"
        " Z / X       severidad de la catarata -/+\n"
        " V / B       intensidad del bloom -/+      N / M  umbral del bloom -/+\n"
        " D / E       exposicion -/+\n"
        " Rueda       zoom        Arrastrar (clic izq.)  mover la pagina\n"
        " P           captura (PPM)      R  reiniciar      ESC  salir\n\n";
}

// ------------------------------------------------------------------ main
int main(int argc, char** argv)
{
    std::string image, shaderDir = DEFAULT_SHADER_DIR;
    float scale = 1.0f;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--scale"   && i + 1 < argc) scale = std::max(0.2f, std::min(1.f, float(std::atof(argv[++i]))));
        else if (a == "--shaders" && i + 1 < argc) shaderDir = argv[++i];
        else if (a == "-h" || a == "--help") {
            std::cout << "Uso: " << argv[0] << " [pagina.png] [--scale 0.2..1] [--shaders ruta]\n";
            return 0;
        } else image = a;
    }

    if (!glfwInit()) { std::cerr << "No se pudo iniciar GLFW\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(1000, 700, "BloomVision", nullptr, nullptr);
    if (!win) { std::cerr << "No se pudo crear la ventana OpenGL 3.3\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "No se pudo iniciar GLEW\n"; return 1; }
    std::cout << "GL_RENDERER: " << glGetString(GL_RENDERER) << "\n"
              << "GL_VERSION : " << glGetString(GL_VERSION) << "\n";
    printHelp();

    int rc = 0;
    try {
        auto renderer = std::make_unique<Renderer>(shaderDir, image, scale);

        App app;
        app.pageW = float(renderer->pageW());
        app.pageH = float(renderer->pageH());
        glfwGetFramebufferSize(win, &app.fbW, &app.fbH);
        glfwGetWindowSize(win, &app.winW, &app.winH);
        resetView(app);

        glfwSetWindowUserPointer(win, &app);
        glfwSetKeyCallback(win, onKey);
        glfwSetScrollCallback(win, onScroll);
        glfwSetMouseButtonCallback(win, onMouseButton);
        glfwSetCursorPosCallback(win, onCursor);
        glfwSetWindowSizeCallback(win, onResize);
        glfwSetFramebufferSizeCallback(win, onResize);
        glfwSetWindowRefreshCallback(win, onRefresh);

        while (!glfwWindowShouldClose(win)) {
            if (app.dirty) {
                glfwGetFramebufferSize(win, &app.fbW, &app.fbH);
                if (app.fbW > 0 && app.fbH > 0) {
                    double t0 = glfwGetTime();
                    renderer->render(app.p, app.v, app.fbW, app.fbH);
                    if (app.shot) { saveScreenshot(app.fbW, app.fbH); app.shot = false; }
                    glfwSwapBuffers(win);
                    glFinish();
                    app.lastMs = float((glfwGetTime() - t0) * 1000.0);

                    char title[256];
                    const VisionParams& p = app.p;
                    std::snprintf(title, sizeof(title),
                        "BloomVision | Astig[%s] eje %.0f° s=%.1f | Catarata[%s] %.2f | "
                        "Bloom[%s] I=%.2f T=%.2f | Exp %.2f | %s%.0f ms",
                        p.astigmatism ? "ON" : "off", p.astigAngleDeg, p.astigSigma,
                        p.cataract ? "ON" : "off", p.cataractSeverity,
                        p.bloom ? "ON" : "off", p.bloomIntensity, p.bloomThreshold, p.exposure,
                        p.showOriginal ? "[ORIGINAL] " : "", app.lastMs);
                    glfwSetWindowTitle(win, title);
                }
                app.dirty = false;
            }
            glfwWaitEvents();
        }
        renderer.reset();
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n";
        rc = 1;
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return rc;
}
