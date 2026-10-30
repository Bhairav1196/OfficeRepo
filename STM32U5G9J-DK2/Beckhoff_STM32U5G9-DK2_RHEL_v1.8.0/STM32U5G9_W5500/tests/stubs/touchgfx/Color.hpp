#ifndef TEST_TOUCHGFX_COLOR_HPP
#define TEST_TOUCHGFX_COLOR_HPP
#include <stdint.h>
#include <touchgfx/widgets/Widget.hpp>
namespace touchgfx {
class Color {
public:
    static colortype getColorFromRGB(uint8_t r, uint8_t g, uint8_t b)
    { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b; }
};
}
#endif
