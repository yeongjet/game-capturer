#pragma once
#include <cstdint>
#include "window.h"

struct Channel
{
    uint32_t remote_token;
    uint64_t remote_address;
    Window window;
};