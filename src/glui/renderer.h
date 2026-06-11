#pragma once
#include <string>

struct NVGcontext;
struct NVGcolor;

namespace glui {

// Обёртка над NanoVG GL3-контекстом. Создаётся один раз после инициализации
// OpenGL (glad), удаляется перед уничтожением окна.
class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Загружает шрифт из файла, возвращает true при успехе.
    bool loadFont(const std::string& name, const std::string& path);

    // fbWidth/fbHeight — размер framebuffer'а в пикселях,
    // winWidth/winHeight — логический размер окна (для pixelRatio на HiDPI).
    void beginFrame(int winWidth, int winHeight, float pixelRatio);
    void endFrame();

    [[nodiscard]] NVGcontext* ctx() const { return m_vg; }

private:
    NVGcontext* m_vg = nullptr;
};

// Парсит "#rrggbb" или "#rrggbbaa" в NVGcolor. На ошибку формата
// возвращает непрозрачный чёрный.
[[nodiscard]] NVGcolor colorFromHex(const std::string& hex);

} // namespace glui
