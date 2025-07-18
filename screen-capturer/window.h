#pragma once
#include <cstdint>

struct Window
{
    uint8_t id;
    char* title;
    uint16_t width;
	uint16_t height;
    size_t pixel_count;
};
