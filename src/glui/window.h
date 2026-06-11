#pragma once
#include <string>

struct GLFWwindow;

namespace glui {

// Состояние ввода за текущий кадр. pressed/released — однокадровые события
// (true только в том кадре, где произошло нажатие/отпускание).
struct InputState {
    double mouseX        = 0.0;
    double mouseY        = 0.0;
    bool   mouseDown     = false;
    bool   mousePressed  = false;
    bool   mouseReleased = false;
};

// Тонкая обёртка над GLFW: окно + OpenGL 3.3 core контекст + glad-загрузка.
class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] bool shouldClose() const;

    // Сбрасывает однокадровые флаги ввода и опрашивает события GLFW.
    void pollEvents();
    void swapBuffers();

    // Текущий размер framebuffer'а (для glViewport, в пикселях).
    void framebufferSize(int& width, int& height) const;

    // Текущий логический размер окна (для nvgBeginFrame, в "экранных" единицах).
    void windowSize(int& width, int& height) const;

    [[nodiscard]] const InputState& input() const { return m_input; }
    [[nodiscard]] GLFWwindow* handle() const { return m_window; }

private:
    GLFWwindow* m_window = nullptr;
    InputState  m_input;

    static void cursorPosCallback(GLFWwindow* w, double x, double y);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
};

} // namespace glui
