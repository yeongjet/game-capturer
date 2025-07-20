#pragma once
#include <cstdint>

struct Window
{
    uint8_t id;
    char window_title[35];
    uint16_t width;
    uint16_t height;
};
