#ifndef TEST_TOUCHGFX_CLICKEVENT_HPP
#define TEST_TOUCHGFX_CLICKEVENT_HPP
#include <stdint.h>
namespace touchgfx {
class ClickEvent {
public:
    enum ClickEventType { PRESSED, RELEASED };
    ClickEventType getType() const { return RELEASED; }
    int16_t getX() const { return 0; }
    int16_t getY() const { return 0; }
};
}
#endif
