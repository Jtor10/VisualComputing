#include "PageTexture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

namespace {

struct Canvas {
    int w, h;
    std::vector<unsigned char> px;
    Canvas(int W, int H) : w(W), h(H), px(size_t(W) * H * 3, 250) {}

    void rect(int x, int y, int rw, int rh, unsigned char v)
    {
        for (int j = std::max(0, y); j < std::min(h, y + rh); ++j)
            for (int i = std::max(0, x); i < std::min(w, x + rw); ++i) {
                unsigned char* p = &px[(size_t(j) * w + i) * 3];
                p[0] = p[1] = p[2] = v;
            }
    }
};

void letter(Canvas& c, int x, int y, int type, int s)
{
    const int xh = 16 * s, t = 2 * s;
    const unsigned char ink = 28;
    switch (type) {
    case 0: c.rect(x + 4 * s, y, t, xh, ink); break;                                    // i
    case 1: c.rect(x + 2 * s, y, t, xh, ink); c.rect(x + 2 * s, y, 8 * s, t, ink);
            c.rect(x + 8 * s, y, t, xh, ink); break;                                    // n
    case 2: c.rect(x + 1 * s, y, t, xh, ink); c.rect(x + 8 * s, y, t, xh, ink);
            c.rect(x + 1 * s, y, 9 * s, t, ink); c.rect(x + 1 * s, y + xh - t, 9 * s, t, ink); break; // o
    case 3: c.rect(x + 4 * s, y - 8 * s, t, xh + 8 * s, ink);
            c.rect(x + 4 * s, y, 6 * s, t, ink); break;                                 // h/d
    case 4: c.rect(x + 1 * s, y, t, xh, ink); c.rect(x + 1 * s, y, 9 * s, t, ink);
            c.rect(x + 1 * s, y + xh / 2, 9 * s, t, ink);
            c.rect(x + 1 * s, y + xh - t, 9 * s, t, ink); break;                       // e
    case 5: c.rect(x + 5 * s, y - 4 * s, t, xh + 4 * s, ink);
            c.rect(x + 1 * s, y + 2 * s, 9 * s, t, ink); break;                         // t
    default:c.rect(x + 2 * s, y, t, xh + 8 * s, ink); c.rect(x + 2 * s, y, 8 * s, t, ink);
            c.rect(x + 8 * s, y, t, xh / 2 + 2 * s, ink);
            c.rect(x + 2 * s, y + xh / 2, 8 * s, t, ink); break;                        // p
    }
}

std::vector<unsigned char> greekedPage(int W, int H)
{
    Canvas c(W, H);
    std::mt19937 rng(1234);
    auto rnd = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };

    // Título
    {
        const int s = 2, n = 10;
        int x = (W - n * 12 * s) / 2;
        for (int k = 0; k < n; ++k) letter(c, x + k * 12 * s, 150, rnd(0, 6), s);
    }

    // Cuerpo
    const int marginX = 140, lineH = 36;
    bool newPara = true;
    for (int y = 260; y < H - 200; y += lineH) {
        int  x       = marginX + (newPara ? 36 : 0);
        bool endPara = rnd(0, 8) == 0;
        int  endX    = endPara ? marginX + rnd(250, 700) : W - marginX;
        for (;;) {
            int len = rnd(2, 9);
            if (x + len * 12 > endX) break;
            for (int k = 0; k < len; ++k) letter(c, x + k * 12, y, rnd(0, 6), 1);
            x += len * 12 + 14;
        }
        newPara = endPara;
        if (endPara) y += 18;
    }
    return c.px;
}

}

GLuint createPageTexture(const std::string& path, int& w, int& h)
{
    unsigned char* data = nullptr;
    std::vector<unsigned char> generated;

    if (!path.empty()) {
        int n = 0;
        data = stbi_load(path.c_str(), &w, &h, &n, 3);
        if (!data)
            std::cerr << "[aviso] No pude cargar '" << path << "' (" << stbi_failure_reason()
                      << "). Uso una pagina procedural.\n";
    }
    if (!data) {
        w = 1240; h = 1754;                      // A4 a 150 dpi
        generated = greekedPage(w, h);
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // La primera fila (arriba) queda en v = 0
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 data ? data : generated.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (data) stbi_image_free(data);
    return tex;
}
