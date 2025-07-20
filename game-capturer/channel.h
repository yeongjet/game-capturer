#pragma once
#include <cstdint>
#include <string>

struct Channel
{
    uint32_t remote_token;
    uint64_t remote_address;
    uint8_t window_id;
    std::string window_title;
    uint16_t window_width;
    uint16_t window_height;
};