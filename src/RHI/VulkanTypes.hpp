#pragma once

#include <volk.h>

#include "Types.hpp"

#ifndef NDEBUG
    #define VK_CHECK(expr) do { VkResult result_ = (expr); if (result_ != VK_SUCCESS) { std::println(std::cerr, "VK_CHECK Failed: {}", #expr); std::abort(); } } while (false)
#else
    #define VK_CHECK(expr) (expr)
#endif
