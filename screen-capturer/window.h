#pragma once
#include <cstdint>
#include <string>

struct Window
{
    uint8_t id;
    char title[256];
    uint16_t width;
    uint16_t height;
    size_t pixel_count;
};
