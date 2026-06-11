#include "renderer.h"

#include <glad/gl.h>
#include "nanovg.h"
#include "nanovg_gl.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace glui {

Renderer::Renderer() {
    m_vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!m_vg)
        throw std::runtime_error("nvgCreateGL3() failed");
}

Renderer::~Renderer() {
    if (m_vg) nvgDeleteGL3(m_vg);
}

bool Renderer::loadFont(const std::string& name, const std::string& path) {
    int handle = nvgCreateFont(m_vg, name.c_str(), path.c_str());
    return handle != -1;
}

void Renderer::beginFrame(int winWidth, int winHeight, float pixelRatio) {
    glViewport(0, 0,
        static_cast<int>(winWidth * pixelRatio),
        static_cast<int>(winHeight * pixelRatio));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(m_vg, static_cast<float>(winWidth), static_cast<float>(winHeight), pixelRatio);
}

void Renderer::endFrame() {
    nvgEndFrame(m_vg);
}

NVGcolor colorFromHex(const std::string& hex) {
    if (hex.size() < 7 || hex[0] != '#')
        return nvgRGBA(0, 0, 0, 255);

    auto hexByte = [](const std::string& s, size_t pos) -> unsigned char {
        return static_cast<unsigned char>(std::strtol(s.substr(pos, 2).c_str(), nullptr, 16));
    };

    unsigned char r = hexByte(hex, 1);
    unsigned char g = hexByte(hex, 3);
    unsigned char b = hexByte(hex, 5);
    unsigned char a = (hex.size() >= 9) ? hexByte(hex, 7) : 255;
    return nvgRGBA(r, g, b, a);
}

} // namespace glui
