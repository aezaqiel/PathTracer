#pragma once

#include <cstdint>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

using usize = size_t;

static_assert(sizeof(u8)  == 1, "Expected u8 to be 1 byte");
static_assert(sizeof(u16) == 2, "Expected u16 to be 2 byte");
static_assert(sizeof(u32) == 4, "Expected u32 to be 4 byte");
static_assert(sizeof(u64) == 8, "Expected u64 to be 8 byte");

static_assert(sizeof(i8)  == 1, "Expected i8 to be 1 byte");
static_assert(sizeof(i16) == 2, "Expected i16 to be 2 byte");
static_assert(sizeof(i32) == 4, "Expected i32 to be 4 byte");
static_assert(sizeof(i64) == 8, "Expected i64 to be 8 byte");

static_assert(sizeof(f32) == 4, "Expected f32 to be 4 byte");
static_assert(sizeof(f64) == 8, "Expected f64 to be 8 byte");

static_assert(sizeof(usize) == 8, "Expected usize to be 8 byte");
