// Phase 0: OpenGL UI PoC — изолированный таргет naleystogramm-glui.
// Окно (GLFW) → OpenGL 3.3 core (glad) → 2D-рендер (NanoVG) → тема (Qt-free
// порт ThemePalette) → core_bridge (NetworkManager без Qt). Не трогает
// существующий Qt-таргет naleystogramm.

#include <glad/gl.h>

#include "window.h"
#include "renderer.h"
#include "theme.h"
#include "core_bridge.h"
#include "widgets/button.h"

#include "nanovg.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

namespace {

// Дамп текущего framebuffer'а в PPM — для headless-проверки PoC
// (переменные окружения GLUI_SCREENSHOT / GLUI_AUTOCLOSE_FRAMES).
void dumpFramebufferPPM(const std::string& path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);
    // OpenGL читает снизу вверх — переворачиваем по строкам
    for (int y = height - 1; y >= 0; --y)
        std::fwrite(pixels.data() + static_cast<size_t>(y) * width * 3, 1,
                     static_cast<size_t>(width) * 3, f);
    std::fclose(f);
}

} // namespace

int main() {
    using namespace glui;

    Window window(1280, 720, "Naleystogramm GLUI PoC");
    Renderer renderer;

    if (!renderer.loadFont("sans", GLUI_FONT_PATH)) {
        std::fprintf(stderr, "Не удалось загрузить шрифт: %s\n", GLUI_FONT_PATH);
        return 1;
    }

    Theme currentTheme = Theme::Dark;
    ThemePalette palette = paletteFor(currentTheme);

    Button themeButton(40.0f, 100.0f, 220.0f, 44.0f, "Переключить тему");

    CoreBridge core;
    std::deque<std::string> logLines;
    constexpr size_t kMaxVisibleLogLines = 12;

    themeButton.onClick = [&]() {
        currentTheme = (currentTheme == Theme::Dark) ? Theme::Light : Theme::Dark;
        palette = paletteFor(currentTheme);
    };

    const char* screenshotPath = std::getenv("GLUI_SCREENSHOT");
    const char* autoCloseEnv   = std::getenv("GLUI_AUTOCLOSE_FRAMES");
    const int   autoCloseFrames = autoCloseEnv ? std::atoi(autoCloseEnv) : 0;
    int frameCount = 0;

    while (!window.shouldClose()) {
        window.pollEvents();
        themeButton.handleInput(window.input());

        for (const auto& line : core.drainLogs()) {
            logLines.push_back(line);
            while (logLines.size() > kMaxVisibleLogLines)
                logLines.pop_front();
        }

        int fbWidth, fbHeight, winWidth, winHeight;
        window.framebufferSize(fbWidth, fbHeight);
        window.windowSize(winWidth, winHeight);
        float pixelRatio = winWidth > 0
            ? static_cast<float>(fbWidth) / static_cast<float>(winWidth)
            : 1.0f;

        NVGcolor bg = colorFromHex(palette.bg);
        glClearColor(bg.r, bg.g, bg.b, 1.0f);

        renderer.beginFrame(winWidth, winHeight, pixelRatio);
        NVGcontext* vg = renderer.ctx();

        nvgFontFace(vg, "sans");
        nvgFontSize(vg, 28.0f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, colorFromHex(palette.textPrimary));
        nvgText(vg, 40.0f, 40.0f, "Налейстограм — OpenGL UI PoC", nullptr);

        themeButton.draw(vg, palette);

        nvgFontSize(vg, 14.0f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, colorFromHex(palette.textMuted));
        float logY = 200.0f;
        for (const auto& line : logLines) {
            nvgText(vg, 40.0f, logY, line.c_str(), nullptr);
            logY += 20.0f;
        }

        renderer.endFrame();

        ++frameCount;
        if (screenshotPath && frameCount == autoCloseFrames)
            dumpFramebufferPPM(screenshotPath, fbWidth, fbHeight);

        window.swapBuffers();

        if (autoCloseFrames > 0 && frameCount >= autoCloseFrames)
            break;
    }

    return 0;
}
