#pragma once
#include "../theme.h"
#include "../window.h"
#include <string>
#include <functional>

struct NVGcontext;

namespace glui {

// Минимальная скруглённая кнопка: прямоугольник + центрированный текст,
// hover/pressed подсветка accent-цветом палитры, клик через onClick.
class Button {
public:
    Button(float x, float y, float w, float h, std::string label);

    // Обновляет hover/pressed по InputState и вызывает onClick при клике
    // (mouseReleased внутри границ кнопки, было нажато тоже внутри).
    void handleInput(const InputState& input);

    void draw(NVGcontext* vg, const ThemePalette& palette) const;

    std::function<void()> onClick;

private:
    [[nodiscard]] bool contains(double px, double py) const;

    float m_x, m_y, m_w, m_h;
    std::string m_label;

    bool m_hover   = false;
    bool m_pressed = false;
};

} // namespace glui
