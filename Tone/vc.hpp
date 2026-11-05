#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace vc {

struct Mat4 {
    std::array<float, 16> m{};
};

inline Mat4 identity() {
    Mat4 r;
    r.m = {1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1};
    return r;
}

// Multiplicación: r = a · b (aplicar b primero, luego a)
inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int rw = 0; rw < 4; ++rw) {
            float s = 0;
            for (int k = 0; k < 4; ++k)
                s += a.m[k*4 + rw] * b.m[c*4 + k];
            r.m[c*4 + rw] = s;
        }
    return r;
}

// Perspective (columna-major)
inline Mat4 perspective(float fovy, float aspect, float near, float far) {
    Mat4 r{};
    float f = 1.0f / std::tan(fovy / 2.0f);
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (far + near) / (near - far);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * far * near) / (near - far);
    return r;
}

inline Mat4 translation(float x, float y, float z) {
    Mat4 r = identity();
    r.m[12] = x; r.m[13] = y; r.m[14] = z;
    return r;
}

inline Mat4 rotationY(float rad) {
    Mat4 r = identity();
    float c = std::cos(rad), s = std::sin(rad);
    r.m[0] =  c; r.m[2] = -s;
    r.m[8] =  s; r.m[10] = c;
    return r;
}

// ── Vec3 helpers ────────────────────────────────────────────────────────
struct Vec3 { float x, y, z; };

inline Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y*b.z - a.z*b.y,
             a.z*b.x - a.x*b.z,
             a.x*b.y - a.y*b.x };
}
inline float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 normalize(const Vec3& v) {
    float len = std::sqrt(dot(v, v));
    if (len == 0) return {0,0,0};
    return { v.x/len, v.y/len, v.z/len };
}

// ── lookAt: view matrix ─────────────────────────────────────────────────
inline Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 f = normalize(sub(target, eye));       // forward
    Vec3 s = normalize(cross(f, up));           // side
    Vec3 u = cross(s, f);                       // up

    Mat4 r{};
    r.m[0]  =  s.x;  r.m[4]  =  s.y;  r.m[8]  =  s.z;  r.m[12] = -dot(s, eye);
    r.m[1]  =  u.x;  r.m[5]  =  u.y;  r.m[9]  =  u.z;  r.m[13] = -dot(u, eye);
    r.m[2]  = -f.x;  r.m[6]  = -f.y;  r.m[10] = -f.z;  r.m[14] =  dot(f, eye);
    r.m[3]  =  0;    r.m[7]  =  0;    r.m[11] =  0;    r.m[15] =  1;
    return r;
}

// ── readFile ─────────────────────────────────────────────────────────────
inline std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "No se pudo abrir: " << path << "\n"; std::exit(1); }
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

// ── Shader helpers ───────────────────────────────────────────────────────
inline GLuint compileShader(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; GLsizei len;
        glGetShaderInfoLog(s, sizeof(log), &len, log);
        std::cerr << "Error compilando shader:\n" << log << "\n";
        std::exit(1);
    }
    return s;
}

struct ProgramInfo {
    GLuint program = 0;
    // localizaciones de atributos/uniforms
    GLint aPosition = -1, aVertexColor = -1;
    GLint uProjectionMatrix = -1, uModelViewMatrix = -1;
};

inline ProgramInfo createProgramInfo(const std::string& vertSrc,
                                     const std::string& fragSrc) {
    ProgramInfo info;
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    info.program = glCreateProgram();
    glAttachShader(info.program, vs);
    glAttachShader(info.program, fs);
    glLinkProgram(info.program);
    GLint ok = 0;
    glGetProgramiv(info.program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]; GLsizei len;
        glGetProgramInfoLog(info.program, sizeof(log), &len, log);
        std::cerr << "Error linkeando programa:\n" << log << "\n";
        std::exit(1);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    info.aPosition         = glGetAttribLocation (info.program, "aPosition");
    info.aVertexColor      = glGetAttribLocation (info.program, "aVertexColor");
    info.uProjectionMatrix = glGetUniformLocation(info.program, "uProjectionMatrix");
    info.uModelViewMatrix  = glGetUniformLocation(info.program, "uModelViewMatrix");
    return info;
}

// ── BufferInfo ───────────────────────────────────────────────────────────
struct Attribute {
    std::string name;
    int         size;         // componentes por vértice (3, 4, ...)
    std::vector<float> data;
};

struct BufferInfo {
    GLuint vao = 0, vboPos = 0, vboCol = 0;
    GLsizei count = 0;
    ProgramInfo* prog = nullptr;
};

inline BufferInfo createBufferInfo(ProgramInfo& prog,
                                   std::initializer_list<Attribute> attrs) {
    BufferInfo bi;
    bi.prog = &prog;
    glGenVertexArrays(1, &bi.vao);
    glBindVertexArray(bi.vao);

    GLuint vbos[2] = {0, 0};
    int idx = 0;
    for (const auto& a : attrs) {
        GLint loc = glGetAttribLocation(prog.program, a.name.c_str());
        if (loc < 0) { std::cerr << "Atributo no encontrado: " << a.name << "\n"; std::exit(1); }
        glGenBuffers(1, &vbos[idx]);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[idx]);
        glBufferData(GL_ARRAY_BUFFER,
                     a.data.size() * sizeof(float),
                     a.data.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, a.size, GL_FLOAT, GL_FALSE,
                              a.size * sizeof(float), nullptr);
        ++idx;
    }
    bi.vboPos = vbos[0];
    bi.vboCol = vbos[1];
    bi.count  = (GLsizei)(attrs.begin()->data.size() / attrs.begin()->size);
    glBindVertexArray(0);
    return bi;
}

inline void setUniform(const ProgramInfo& prog, const char* name, const Mat4& mat) {
    GLint loc = glGetUniformLocation(prog.program, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, mat.m.data());
}

inline void drawBufferInfo(const BufferInfo& bi) {
    glBindVertexArray(bi.vao);
    glDrawArrays(GL_TRIANGLES, 0, bi.count);
    glBindVertexArray(0);
}

}
