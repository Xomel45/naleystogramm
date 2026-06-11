#include "button.h"
#include "../renderer.h"

#include "nanovg.h"

namespace glui {

Button::Button(float x, float y, float w, float h, std::string label)
    : m_x(x), m_y(y), m_w(w), m_h(h), m_label(std::move(label)) {}

bool Button::contains(double px, double py) const {
    return px >= m_x && px <= m_x + m_w && py >= m_y && py <= m_y + m_h;
}

void Button::handleInput(const InputState& input) {
    m_hover = contains(input.mouseX, input.mouseY);

    if (m_hover && input.mousePressed)
        m_pressed = true;

    if (input.mouseReleased) {
        if (m_pressed && m_hover && onClick)
            onClick();
        m_pressed = false;
    }

    if (!input.mouseDown)
        m_pressed = false;
}

void Button::draw(NVGcontext* vg, const ThemePalette& palette) const {
    NVGcolor fill = colorFromHex(
        m_pressed ? palette.accentPressed
                  : (m_hover ? palette.accentHover : palette.accent));

    nvgBeginPath(vg);
    nvgRoundedRect(vg, m_x, m_y, m_w, m_h, 8.0f);
    nvgFillColor(vg, fill);
    nvgFill(vg);

    nvgFontFace(vg, "sans");
    nvgFontSize(vg, 18.0f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, colorFromHex(palette.textOnAccent));
    nvgText(vg, m_x + m_w / 2.0f, m_y + m_h / 2.0f, m_label.c_str(), nullptr);
}

} // namespace glui
