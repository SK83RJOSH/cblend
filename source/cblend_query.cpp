#include <cblend_query.hpp>
#include <range/v3/algorithm/all_of.hpp>

#include <cctype>
#include <limits>

using namespace cblend;

[[nodiscard]] Option<std::string_view> AsName(std::string_view input)
{
    // Must begin with A-Z or _
    if (input.empty() || std::isalpha(input[0]) == 0 && input[0] != '_')
    {
        return NULL_OPTION;
    }

    // All characters must be A-Z, 0-9, or _
    const auto valid_character = [](const char chr)
    {
        return std::isalpha(chr) != 0 || std::isdigit(chr) != 0 || chr == '_';
    };

    if (!ranges::all_of(input, valid_character))
    {
        return NULL_OPTION;
    }

    return input;
}

[[nodiscard]] Option<usize> AsIndex(std::string_view input)
{
    constexpr auto MAX_DIGITS = std::numeric_limits<usize>::digits10;

    if (input.empty() || input.size() > MAX_DIGITS)
    {
        return NULL_OPTION;
    }

    usize result(0);
    usize multiplier(1);
    for (const char character : input)
    {
        constexpr usize BASE(10);
        if (std::isdigit(character) == 0)
        {
            return NULL_OPTION;
        }
        result += usize(character - '0') * multiplier;
        multiplier *= BASE;
    }
    return result;
};

Result<Query, QueryError> Query::Create(std::span<const QueryToken> tokens)
{
    Query result = {};
    result.m_Tokens.reserve(tokens.size());

    for (const auto& token : tokens)
    {
        result.m_Tokens.push_back(token);
    }

    if (result.m_Tokens.empty())
    {
        return MakeError(QueryError::InvalidQueryString);
    }

    return result;
}

Result<Query, QueryError> Query::Create(std::span<const std::string_view> tokens)
{
    Query result = {};
    result.m_Tokens.reserve(tokens.size());

    for (const auto& token : tokens)
    {
        if (auto name_token = AsName(token))
        {
            result.m_Tokens.push_back(*name_token);
        }
        else if (auto index_token = AsIndex(token))
        {
            result.m_Tokens.push_back(*index_token);
        }
    }

    if (result.m_Tokens.empty() || result.m_Tokens.size() != tokens.size())
    {
        return MakeError(QueryError::InvalidQueryString);
    }

    return result;
}

usize Query::GetTokenCount() const
{
    return m_Tokens.size();
}

Option<QueryToken> Query::GetToken(usize token_index) const
{
    if (token_index >= m_Tokens.size())
    {
        return NULL_OPTION;
    }
    return m_Tokens[token_index];
}
