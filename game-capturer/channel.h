#pragma once
#include <cstdint>
#include "window.h"

struct FrameRegion
{
    uint32_t token;
    uint64_t address;
};

struct Channel
{
    FrameRegion frame_region;
    Window window;
};