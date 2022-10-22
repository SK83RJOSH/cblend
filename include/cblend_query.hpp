#pragma once

#include <cblend_types.hpp>

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cblend
{
using QueryToken = std::variant<std::string, usize>;
using QueryTokens = std::vector<QueryToken>;

class QueryBuilder final
{
public:
    static Option<QueryTokens> New(std::string_view query_string) { return Tokenize(query_string); }

private:
    [[nodiscard]] static Option<std::vector<QueryToken>> Tokenize(std::string_view query_string);
};
} // namespace cblend
