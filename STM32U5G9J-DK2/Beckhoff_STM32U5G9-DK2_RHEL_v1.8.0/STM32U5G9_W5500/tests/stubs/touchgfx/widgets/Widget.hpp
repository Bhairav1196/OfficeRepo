#ifndef TEST_TOUCHGFX_WIDGET_HPP
#define TEST_TOUCHGFX_WIDGET_HPP
#include <stdint.h>
namespace touchgfx {
using colortype = uint32_t;
struct Rect {
    int16_t x; int16_t y; int16_t width; int16_t height;
    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(int16_t x_, int16_t y_, int16_t w_, int16_t h_) : x(x_), y(y_), width(w_), height(h_) {}
};
class ClickEvent;
class Widget {
public:
    virtual ~Widget() {}
    virtual void draw(const Rect&) const {}
    virtual Rect getSolidRect() const { return Rect(); }
    virtual void handleClickEvent(const ClickEvent&) {}
    void setPosition(int16_t, int16_t, int16_t, int16_t) {}
    void setTouchable(bool) {}
    void invalidate() {}
protected:
    void translateRectToAbsolute(Rect&) const {}
};
}
#endif
