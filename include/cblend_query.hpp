#include <cblend_types.hpp>

#include <algorithm>
#include <array>
#include <string_view>
#include <variant>
#include <vector>

namespace cblend
{
namespace algorithms
{
    template<class ContainerT, class ValueT>
    constexpr bool Contains(ContainerT&& container, ValueT&& value)
    {
        const auto begin_it = std::begin(container);
        const auto end_it = std::end(container);
        return std::find(begin_it, end_it, std::forward<ValueT>(value)) != end_it;
    }

    constexpr std::string_view Trim(std::string_view string)
    {
        constexpr auto IS_WHITESPACE = [](char character)
        {
            constexpr std::array<char, 6> WHITESPACE{
                ' ', '\f', '\n', '\r', '\t', '\v',
            };
            return Contains(WHITESPACE, character);
        };

        if (string.empty())
        {
            return {};
        }

        // NOLINTNEXTLINE(readability-qualified-auto)
        auto left = string.begin();
        while (IS_WHITESPACE(*left))
        {
            left++;
            if (left == string.end())
            {
                return {};
            }
        }

        // NOLINTNEXTLINE(readability-qualified-auto)
        auto right = string.end() - 1;
        while (right > left && IS_WHITESPACE(*right))
        {
            --right;
        }

        // NOLINTNEXTLINE(*-pointer-arithmetic)
        return std::string_view{ &left[0], static_cast<usize>(std::distance(left, right)) + 1 };
    }
} // namespace algorithms

template<usize N>
struct TokenizeDelimiter
{
    // NOLINTBEGIN(*-avoid-c-arrays, *-explicit-conversions)
    char string[N]{};
    usize size{ N - 1 };

    constexpr TokenizeDelimiter(const char (&tokens)[N]) { std::copy(std::begin(tokens), std::end(tokens), std::begin(string)); }

    constexpr TokenizeDelimiter(char token)
    {
        string[0] = token;
        string[1] = '\0';
    }
    // NOLINTEND(*-avoid-c-arrays, *-explicit-conversions)
};

template<usize N = 2>
TokenizeDelimiter(char) -> TokenizeDelimiter<2>;

enum class TokenizeBehavior : u8
{
    None,
    TrimWhitespace = 1U << 0U,
    SkipEmpty = 1U << 1U,
    AnyOfDelimiter = 1U << 2U,
};

constexpr TokenizeBehavior operator|(TokenizeBehavior rhs, TokenizeBehavior lhs)
{
    return TokenizeBehavior(u8(lhs) | u8(rhs));
}

constexpr bool operator&(TokenizeBehavior rhs, TokenizeBehavior lhs)
{
    return (u8(lhs) & u8(rhs)) != 0;
}

template<TokenizeDelimiter Delimiter, TokenizeBehavior Behavior = TokenizeBehavior::None, usize MaxTokens = std::string_view::npos>
class Tokenize
{
public:
    constexpr Tokenize() = default;
    constexpr Tokenize(const Tokenize&) = default;
    constexpr Tokenize(Tokenize&&) noexcept = default;
    constexpr Tokenize& operator=(const Tokenize&) = default;
    constexpr Tokenize& operator=(Tokenize&&) noexcept = default;
    constexpr ~Tokenize() = default;

    explicit constexpr Tokenize(std::nullptr_t) {}
    explicit constexpr Tokenize(const char* source) : m_Source{ source } { GetNext(); }
    explicit constexpr Tokenize(std::string_view source) : m_Source{ source } { GetNext(); }

    constexpr bool operator==(const Tokenize& rhs) const = default;
    constexpr bool operator!=(const Tokenize& rhs) const = default;

    constexpr auto begin() { return *this; }
    constexpr auto end() { return Tokenize{ m_Source, m_Source.size() }; }
    constexpr auto begin() const { return *this; }
    constexpr auto end() const { return Tokenize{ m_Source, m_Source.size() }; }
    constexpr auto cbegin() const { return *this; }
    constexpr auto cend() const { return Tokenize{ m_Source, m_Source.size() }; }

    constexpr auto operator*() const
    {
        const usize pos{ m_Position == 0 || (Behavior & TokenizeBehavior::AnyOfDelimiter) ? m_Position : m_Position + Delimiter.size - 1 };
        const std::string_view res{ m_Source.substr(pos, m_Next - pos) };
        if constexpr (Behavior & TokenizeBehavior::TrimWhitespace)
        {
            return algorithms::Trim(res);
        }
        else
        {
            return res;
        }
    }

    constexpr Tokenize operator++()
    {
        if (!Advance())
        {
            *this = end();
        }
        return *this;
    }

    // NOLINTNEXTLINE(cert-dcl21-cpp)
    constexpr Tokenize operator++(int)
    {
        auto copy = *this;
        ++(*this);
        return copy;
    }

private:
    std::string_view m_Source;
    usize m_Position{ 0 };
    usize m_Next{ 0 };
    usize m_NumTokens{ 0 };

    constexpr Tokenize(std::string_view source, usize size) : m_Source{ source }, m_Position{ size }, m_Next{ size } {}

    constexpr void GetNext()
    {
        if constexpr (MaxTokens != std::string_view::npos)
        {
            if (m_NumTokens == MaxTokens)
            {
                m_Next = m_Source.size();
                return;
            }
        }

        m_Next = m_Position;
        if constexpr (Behavior & TokenizeBehavior::AnyOfDelimiter)
        {
            while (m_Next < m_Source.size() && !algorithms::Contains(Delimiter.string, m_Source[m_Next]))
            {
                ++m_Next;
            }
        }
        else
        {
            while (m_Next < m_Source.size() && !m_Source.substr(m_Next).starts_with(Delimiter.string))
            {
                ++m_Next;
            }
        }

        if constexpr (Behavior & TokenizeBehavior::SkipEmpty)
        {
            while (m_Next == m_Position && m_Position < m_Source.size())
            {
                m_Position = m_Next + 1;
                GetNext();
            }
            if (m_Position != m_Next)
            {
                ++m_NumTokens;
            }
        }
        else
        {
            ++m_NumTokens;
        }
    }

    constexpr bool Advance()
    {
        m_Position = m_Next + 1;
        GetNext();
        return m_Position < m_Source.size();
    }
};

template<usize N>
struct QueryString
{
    // NOLINTBEGIN(*-avoid-c-arrays, *-explicit-conversions)
    char string[N]{};
    usize size{ N };

    constexpr QueryString(const char (&input)[N]) { std::copy(std::begin(input), std::end(input), std::begin(string)); }
    // NOLINTEND(*-avoid-c-arrays, *-explicit-conversions)
};

template<class T>
constexpr bool ALWAYS_FALSE = false;

template<class T>
consteval auto ToType([[maybe_unused]] std::string_view type_as_string)
{
    static_assert(ALWAYS_FALSE<T>, "ToType was called with an unsupported type");
}

template<>
consteval auto ToType<std::string_view>(std::string_view type_as_string)
{
    return type_as_string;
}

template<class T, QueryString Input, class Tokenizer = Tokenize<' '>>
consteval auto ToTypes()
{
    // Not random access iterators, so we need to get size like this
    constexpr auto NUM_TOKENS = []()
    {
        auto tokenizer = Tokenizer(std::string_view{ Input.string, Input.size });
        usize count{};
        for ([[maybe_unused]] auto token : tokenizer)
        {
            ++count;
        }
        return count;
    }();

    std::array<T, NUM_TOKENS> out_array{};
    auto tokenizer = Tokenizer(std::string_view{ Input.string, Input.size });
    for (usize index = 0; index < NUM_TOKENS; ++index)
    {
        out_array[index] = ToType<T>(*tokenizer);
        ++tokenizer;
    }
    return out_array;
}

using QueryToken = std::variant<std::string_view, usize>;
using QueryTokens = std::vector<QueryToken>;

enum class QueryError
{
    InvalidQueryString
};

class Query final
{
public:
    [[nodiscard]] static Result<Query, QueryError> Create(std::span<const QueryToken> tokens);
    [[nodiscard]] static Result<Query, QueryError> Create(std::span<const std::string_view> tokens);
    template<QueryString Input>
    [[nodiscard]] static Result<Query, QueryError> Create()
    {
        using Tokenizer = Tokenize<"[].", TokenizeBehavior::AnyOfDelimiter | TokenizeBehavior::SkipEmpty>;
        const auto tokens = ToTypes<std::string_view, Input, Tokenizer>();
        return Create(std::span{ tokens.data(), tokens.size() });
    }

    [[nodiscard]] usize GetTokenCount() const;
    [[nodiscard]] Option<QueryToken> GetToken(usize token_index) const;

    constexpr auto begin() noexcept { return m_Tokens.begin(); }
    constexpr auto end() noexcept { return m_Tokens.end(); }
    [[nodiscard]] constexpr auto begin() const noexcept { return m_Tokens.begin(); }
    [[nodiscard]] constexpr auto end() const noexcept { return m_Tokens.end(); }
    [[nodiscard]] constexpr auto cbegin() const noexcept { return m_Tokens.cbegin(); }
    [[nodiscard]] constexpr auto cend() const noexcept { return m_Tokens.cend(); }

private:
    QueryTokens m_Tokens;
};
} // namespace cblend
