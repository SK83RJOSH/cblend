#include <cblend_query.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <ranges>

using namespace cblend;

[[nodiscard]] Option<std::string> AsNamed(std::string_view input)
{
    // Smallest possible named is .a
    if (input.size() < 2)
    {
        return NULL_OPTION;
    }

    const std::string_view name = std::string_view(input.data() + 1, input.size() - 1);

    // Must begin with A-Z or _
    if (std::isalpha(name[0]) == 0 && name[0] != '_')
    {
        return NULL_OPTION;
    }

    // All characters must be A-Z, 0-9, or _
    const auto valid_character = [](const char chr)
    {
        return std::isalpha(chr) != 0 || std::isdigit(chr) != 0 || chr == '_';
    };

    if (!std::ranges::all_of(name, valid_character))
    {
        return NULL_OPTION;
    }

    return std::string(name);
}

[[nodiscard]] Option<usize> AsIndex(std::string_view input)
{
    // Smallest possible index is [1]
    if (input.size() < 3)
    {
        return NULL_OPTION;
    }

    // Substract the added length of '[' and ']'
    if (input.size() - 2 > std::numeric_limits<usize>::max_digits10)
    {
        return NULL_OPTION;
    }

    usize result(0);
    usize multiplier(1);
    for (const char character : std::string_view{ input.data() + 1, input.size() - 2 })
    {
        static constexpr usize BASE(10);
        if (std::isdigit(character) == 0)
        {
            return NULL_OPTION;
        }
        result += usize(character - '0') * multiplier;
        multiplier *= BASE;
    }
    return result;
};

[[nodiscard]] std::string_view::iterator FindNextToken(std::string_view::iterator head, std::string_view::iterator tail)
{
    while (head != tail && *head != '.' && *head != '[')
    {
        head++;
    }

    return head;
}

Option<std::vector<QueryToken>> QueryBuilder::Tokenize(std::string_view query_string)
{
    std::vector<QueryToken> tokens;

    if (!query_string.empty())
    {
        auto token_head = query_string.begin();
        auto token_tail = FindNextToken(token_head, query_string.end());

        do {
            std::string_view value(token_head, token_tail);

            if (auto token = AsNamed(value))
            {
                tokens.emplace_back(*token);
            }
            else if (auto token = AsIndex(value))
            {
                tokens.emplace_back(*token);
            }
            else
            {
                return NULL_OPTION;
            }

            token_head = token_tail;
            token_tail = FindNextToken(token_head, query_string.end());
        } while (token_tail != query_string.end());
    }

    if (tokens.empty())
    {
        return NULL_OPTION;
    }

    return tokens;
};
