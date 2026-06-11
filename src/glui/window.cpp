#include "window.h"

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace glui {

Window::Window(int width, int height, const std::string& title) {
    if (!glfwInit())
        throw std::runtime_error("glfwInit() failed");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow() failed");
    }

    glfwMakeContextCurrent(m_window);
    // 0: vsync может никогда не сработать для невидимого/неуправляемого
    // окна (headless/Xvfb), что блокирует glfwSwapBuffers() навсегда.
    glfwSwapInterval(0);

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        throw std::runtime_error("gladLoadGL() failed");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetCursorPosCallback(m_window, &Window::cursorPosCallback);
    glfwSetMouseButtonCallback(m_window, &Window::mouseButtonCallback);
}

Window::~Window() {
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::pollEvents() {
    m_input.mousePressed  = false;
    m_input.mouseReleased = false;
    glfwPollEvents();
}

void Window::swapBuffers() {
    glfwSwapBuffers(m_window);
}

void Window::framebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(m_window, &width, &height);
}

void Window::windowSize(int& width, int& height) const {
    glfwGetWindowSize(m_window, &width, &height);
}

void Window::cursorPosCallback(GLFWwindow* w, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    self->m_input.mouseX = x;
    self->m_input.mouseY = y;
}

void Window::mouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (action == GLFW_PRESS) {
        self->m_input.mouseDown    = true;
        self->m_input.mousePressed = true;
    } else if (action == GLFW_RELEASE) {
        self->m_input.mouseDown     = false;
        self->m_input.mouseReleased = true;
    }
}

} // namespace glui
