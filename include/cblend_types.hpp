#pragma once

#include <tl/expected.hpp>
#include <tl/optional.hpp>

#include <cstddef>
#include <cstdint>

namespace cblend
{
template<class T, class E>
using result = tl::expected<T, E>;

template<class E>
using error = tl::unexpected<E>;

template<class E>
// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr error<typename std::decay<E>::type> make_error(E&& err)
{
    return tl::make_unexpected(err);
}

template<class T>
using option = tl::optional<T>;

using nullopt_t = tl::nullopt_t;
// NOLINTNEXTLINE(readability-identifier-naming)
static constexpr auto nullopt = tl::nullopt;

template<class T>
// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr option<T> make_option(T&& val)
{
    return tl::make_optional(val);
}

template<class T, class... Args>
// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr option<T> make_option(Args&&... args)
{
    return tl::make_optional(args...);
}

template<class T, class U, class... Args>
// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr option<T> make_option(std::initializer_list<U> init, Args&&... args)
{
    return tl::make_optional(init, args...);
}

using s8 = std::int8_t;
using u8 = std::uint8_t;

using s16 = std::int16_t;
using u16 = std::uint16_t;

using s32 = std::int32_t;
using u32 = std::uint32_t;

using s64 = std::int64_t;
using u64 = std::uint64_t;

using ssize = std::ptrdiff_t;
using usize = std::size_t;

using f32 = float;
using f64 = double;
} // namespace cblend
