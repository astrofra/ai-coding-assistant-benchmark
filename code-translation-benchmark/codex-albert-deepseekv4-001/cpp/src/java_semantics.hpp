// Host language bridge: defined Java int/long arithmetic, shifts and narrowing.
#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace java {
static_assert(sizeof(int) == 4, "Java int requires 32 bits");
inline int signed32(std::uint32_t n) {
    return n <= 0x7fffffffu ? int(n) : -1 - int(~n);
}
inline std::int64_t signed64(std::uint64_t n) {
    return n <= 0x7fffffffffffffffULL ? std::int64_t(n) : -1 - std::int64_t(~n);
}
inline int add(int a, int b) { return signed32(std::uint32_t(a) + std::uint32_t(b)); }
inline int sub(int a, int b) { return signed32(std::uint32_t(a) - std::uint32_t(b)); }
inline int mul(int a, int b) { return signed32(std::uint32_t(a) * std::uint32_t(b)); }
inline int shl(int a, unsigned n) { return signed32(std::uint32_t(a) << (n & 31)); }
inline int shr(int a, unsigned n) {
    n &= 31;
    if (!n) return a;
    return signed32((std::uint32_t(a) >> n) | (a < 0 ? (~std::uint32_t(0) << (32-n)) : 0));
}
inline std::int64_t add64(std::int64_t a, std::int64_t b) { return signed64(std::uint64_t(a) + std::uint64_t(b)); }
inline std::int64_t sub64(std::int64_t a, std::int64_t b) { return signed64(std::uint64_t(a) - std::uint64_t(b)); }
inline std::int64_t mul64(std::int64_t a, std::int64_t b) { return signed64(std::uint64_t(a) * std::uint64_t(b)); }
inline std::int64_t div64(std::int64_t a, std::int64_t b) {
    if (!b) throw std::runtime_error("Java long division by zero");
    if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return a;
    return a/b;
}
inline int narrow(std::int64_t a) { return signed32(std::uint32_t(a)); }
inline int toInt(float a) {
    if (std::isnan(a)) return 0;
    if (a >= 2147483648.0f) return 2147483647;
    if (a <= -2147483648.0f) return (-2147483647 - 1);
    return int(a);
}
}
