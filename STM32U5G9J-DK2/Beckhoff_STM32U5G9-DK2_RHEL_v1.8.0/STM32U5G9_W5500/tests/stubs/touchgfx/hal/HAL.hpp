#ifndef TEST_TOUCHGFX_HAL_HPP
#define TEST_TOUCHGFX_HAL_HPP
#include <touchgfx/widgets/Widget.hpp>
namespace touchgfx {
class LCD {
public:
    void fillRect(const Rect&, colortype, uint8_t) {}
};
class HAL {
public:
    static LCD& lcd() { static LCD lcdInstance; return lcdInstance; }
};
}
#endif
