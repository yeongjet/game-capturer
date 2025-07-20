#pragma once
#include "window.h"

// Cannot exceed MaxCallerData = 56
struct Channel
{
    uint32_t remote_token;
    uint64_t remote_address;
    Window window;
};
