#pragma once

#include <tl/expected.hpp>
#include <tl/optional.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace cblend
{
template<class T, class E>
using Result = tl::expected<T, E>;

template<class E>
using Error = tl::unexpected<E>;

template<class E>
inline constexpr Error<typename std::decay<E>::type> MakeError(E&& err)
{
    return tl::make_unexpected(err);
}

template<class T>
using Option = tl::optional<T>;

using NullOption = tl::nullopt_t;
static constexpr auto NULL_OPTION = tl::nullopt;

template<class T>
inline constexpr Option<T> MakeOption(T&& val)
{
    return tl::make_optional(val);
}

template<class T, class... Args>
inline constexpr Option<T> MakeOption(Args&&... args)
{
    return tl::make_optional(args...);
}

template<class T, class U, class... Args>
inline constexpr Option<T> MakeOption(std::initializer_list<U> init, Args&&... args)
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

using MemorySpan = std::span<const u8>;
} // namespace cblend
