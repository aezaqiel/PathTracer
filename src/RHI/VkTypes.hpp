#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#ifndef NDEBUG
    #include "Logger.hpp"
    #define VK_CHECK(expr) do { VkResult result_ = (expr); if (result_ != VK_SUCCESS) { LOG_FATAL("VK_CHECK Failed: {}", #expr); std::abort(); } } while (false)
#else
    #define VK_CHECK(expr) (expr)
#endif
