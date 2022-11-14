#pragma once

#include <cblend_format.hpp>
#include <cblend_query.hpp>
#include <cblend_reflection.hpp>
#include <cblend_stream.hpp>
#include <range/v3/view/filter.hpp>

#include <deque>
#include <functional>
#include <tuple>
#include <unordered_map>

namespace cblend
{
struct TypeDatabase
{
    using TypeList = std::deque<TypeContainer>;
    using TypeMap = std::unordered_map<std::string_view, usize>;
    using StructMap = std::unordered_map<usize, usize>;

    TypeList type_list;
    TypeMap type_map;
    StructMap struct_map;
    StructMap index_map;
};

struct MemoryRange
{
    u64 head = 0;
    u64 tail = 0;
    MemorySpan span;
};

class MemoryTable final
{
public:
    MemoryTable() = default;
    explicit MemoryTable(std::vector<MemoryRange>& ranges);

    [[nodiscard]] MemorySpan GetMemory(u64 address, usize size) const;
    template<class T>
    Option<T> GetMemory(u64 address) const;

private:
    std::vector<MemoryRange> m_Ranges;
};

enum class ReflectionError : u8
{
    InvalidSdnaStruct,
    InvalidSdnaField,
    InvalidSdnaFieldName,
};

using BlendError = std::variant<FileStreamError, FormatError, ReflectionError>;

class BlendType;
using QueryValueResult = std::tuple<BlendType, MemorySpan>;

enum class QueryValueError : u8
{
    InvalidQuery,
    InvalidType,
    InvalidValue,
    FieldNotFound,
    IndexOutOfBounds,
    IndexedInvalidType,
};

class BlendFieldInfo;

class BlendType
{
public:
    BlendType(const MemoryTable& memory_table, const Type& type);

    bool operator==(const BlendType& other) const;

    [[nodiscard]] bool IsArray() const;
    [[nodiscard]] bool IsPointer() const;
    [[nodiscard]] bool IsPrimitive() const;
    [[nodiscard]] bool IsStruct() const;

    [[nodiscard]] usize GetSize() const;

    [[nodiscard]] bool HasElementType() const;
    [[nodiscard]] Option<BlendType> GetElementType() const;

    [[nodiscard]] usize GetArrayRank() const;

    [[nodiscard]] Option<BlendFieldInfo> GetField(std::string_view field_name) const;
    [[nodiscard]] std::vector<BlendFieldInfo> GetFields() const;

    [[nodiscard]] Result<QueryValueResult, QueryValueError> QueryValue(MemorySpan data, const Query& query) const;
    template<QueryString Input>
    [[nodiscard]] Result<QueryValueResult, QueryValueError> QueryValue(MemorySpan data) const;

    template<class T>
    [[nodiscard]] Result<T, QueryValueError> QueryValue(MemorySpan data, const Query& query) const;
    template<class T, QueryString Input>
    [[nodiscard]] Result<T, QueryValueError> QueryValue(MemorySpan data) const;

    template<class T>
    [[nodiscard]] Result<T, QueryValueError> QueryValue(const Block& block, const Query& query) const;
    template<class T, QueryString Input>
    [[nodiscard]] Result<T, QueryValueError> QueryValue(const Block& block) const;

    [[nodiscard]] Result<void, QueryValueError>
    QueryEachValue(MemorySpan data, const Query& query, const std::function<void(const BlendType&, MemorySpan)>& callback) const;
    template<QueryString Input>
    [[nodiscard]] Result<void, QueryValueError>
    QueryEachValue(MemorySpan data, const std::function<void(const BlendType&, MemorySpan)>& callback) const;

    template<class T>
    [[nodiscard]] Result<void, QueryValueError>
    QueryEachValue(MemorySpan data, const Query& query, const std::function<void(T)>& callback) const;
    template<class T, QueryString Input>
    [[nodiscard]] Result<void, QueryValueError> QueryEachValue(MemorySpan data, const std::function<void(T)>& callback) const;

private:
    const MemoryTable& m_MemoryTable;
    const Type& m_Type;
    const Type* m_ElementType = nullptr;
    usize m_ArrayRank = 0U;
    std::vector<const AggregateType::Field*> m_Fields;
    std::unordered_map<std::string_view, const AggregateType::Field*> m_FieldsByName;
};

class BlendFieldInfo
{
public:
    BlendFieldInfo(const MemoryTable& memory_table, const AggregateType::Field& field, const BlendType& declaring_type);

    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] const BlendType& GetDeclaringType() const;
    [[nodiscard]] const BlendType& GetFieldType() const;

    [[nodiscard]] MemorySpan GetData(MemorySpan span) const;
    [[nodiscard]] MemorySpan GetData(const Block& block) const;

    [[nodiscard]] MemorySpan GetPointerData(MemorySpan span) const;
    [[nodiscard]] MemorySpan GetPointerData(const Block& block) const;

    template<class T>
    [[nodiscard]] Option<T> GetValue(MemorySpan span) const;
    template<class T>
    [[nodiscard]] Option<T> GetValue(const Block& block) const;

    template<class T>
    [[nodiscard]] Option<T*> GetPointer(MemorySpan span) const;
    template<class T>
    [[nodiscard]] Option<T*> GetPointer(const Block& block) const;

    template<class T>
    [[nodiscard]] Option<T> GetPointerValue(MemorySpan span) const;
    template<class T>
    [[nodiscard]] Option<T> GetPointerValue(const Block& block) const;

private:
    const MemoryTable& m_MemoryTable;
    usize m_Offset;
    std::string_view m_Name;
    const BlendType& m_DeclaringType;
    BlendType m_FieldType;
    usize m_Size;
};

class Blend final
{
public:
    Blend(const Blend&) = delete;
    Blend(Blend&&) = default;
    Blend& operator=(const Blend&) = delete;
    Blend& operator=(Blend&&) = default;
    ~Blend() = default;

    static Result<Blend, BlendError> Open(std::string_view path);
    static Result<Blend, BlendError> Read(MemorySpan buffer);

    [[nodiscard]] Endian GetEndian() const;
    [[nodiscard]] Pointer GetPointer() const;

    [[nodiscard]] usize GetBlockCount() const;
    [[nodiscard]] usize GetBlockCount(const BlockCode& code) const;

    [[nodiscard]] auto GetBlocks(const BlockCode& code) const;
    [[nodiscard]] Option<const Block&> GetBlock(const BlockCode& code) const;

    [[nodiscard]] auto GetBlocks(const BlendType& type) const;
    [[nodiscard]] Option<const Block&> GetBlock(const BlendType& type) const;

    [[nodiscard]] Option<BlendType> GetType(std::string_view name) const;
    [[nodiscard]] Option<BlendType> GetBlockType(const Block& block) const;

private:
    File m_File = {};
    TypeDatabase m_TypeDatabase = {};
    MemoryTable m_MemoryTable = {};

    Blend(File& file, TypeDatabase& type_database, MemoryTable& memory_table);
};

template<class T>
inline Option<T> MemoryTable::GetMemory(u64 address) const
{
    if (auto result = GetMemory(address, sizeof(T)); !result.empty())
    {
        std::array<u8, sizeof(T)> value = {};
        std::copy(result.begin(), result.end(), value.data());
        return std::bit_cast<T>(value);
    }

    return NULL_OPTION;
}

template<QueryString Input>
Result<QueryValueResult, QueryValueError> BlendType::QueryValue(MemorySpan data) const
{
    const auto query = Query::Create<Input>();
    if (!query)
    {
        return MakeError(QueryValueError::InvalidQuery);
    }

    return QueryValue(data, *query);
}

template<class T>
inline Result<T, QueryValueError> BlendType::QueryValue(MemorySpan data, const Query& query) const
{
    const auto result = QueryValue(data, query);
    if (!result)
    {
        return MakeError(result.error());
    }

    [[maybe_unused]] const auto& [result_type, result_data] = *result;
    if constexpr (std::is_same_v<T, MemorySpan>)
    {
        return result_data;
    }
    else if constexpr (std::is_pointer_v<T>)
    {
        if (!result_data.empty() && result_data.size() != sizeof(std::remove_pointer_t<T>))
        {
            return MakeError(QueryValueError::InvalidType);
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<T>(result_data.data());
    }
    else
    {
        if (result_data.data() == nullptr)
        {
            return MakeError(QueryValueError::InvalidValue);
        }

        if (result_data.size() != sizeof(T))
        {
            return MakeError(QueryValueError::InvalidType);
        }

        std::array<u8, sizeof(T)> value = {};
        std::copy(result_data.begin(), result_data.end(), value.data());
        return std::bit_cast<T>(value);
    }
}

template<class T, QueryString Input>
inline Result<T, QueryValueError> BlendType::QueryValue(MemorySpan data) const
{
    const auto query = Query::Create<Input>();
    if (!query)
    {
        return MakeError(QueryValueError::InvalidQuery);
    }
    return QueryValue<T>(data, *query);
}

template<class T>
inline Result<T, QueryValueError> BlendType::QueryValue(const Block& block, const Query& query) const
{
    return QueryValue<T>(block.body, query);
}

template<class T, QueryString Input>
inline Result<T, QueryValueError> BlendType::QueryValue(const Block& block) const
{
    return QueryValue<T, Input>(block.body);
}

template<QueryString Input>
inline Result<void, QueryValueError>
BlendType::QueryEachValue(MemorySpan data, const std::function<void(const BlendType&, MemorySpan)>& callback) const
{
    const auto query = Query::Create<Input>();
    if (!query)
    {
        return MakeError(QueryValueError::InvalidQuery);
    }
    return QueryEachValue(data, *query, callback);
}

template<class T>
inline Result<void, QueryValueError>
BlendType::QueryEachValue(MemorySpan data, const Query& query, const std::function<void(T)>& callback) const
{
    while (!data.empty() && data.data() != nullptr)
    {
        auto next_value = QueryValue<"next[0]">(data);
        if (!next_value)
        {
            return MakeError(next_value.error());
        }

        const auto& [next_type, next_data] = *next_value;
        if (next_data.data() == nullptr)
        {
            return {};
        }

        const auto query_value = next_type.QueryValue<T>(next_data, query);
        if (!query_value)
        {
            return MakeError(query_value.error());
        }

        callback(*query_value);
        data = next_data;
    }
    return {};
}

template<class T, QueryString Input>
inline Result<void, QueryValueError> BlendType::QueryEachValue(MemorySpan data, const std::function<void(T)>& callback) const
{
    const auto query = Query::Create<Input>();
    if (!query)
    {
        return MakeError(QueryValueError::InvalidQuery);
    }
    return QueryEachValue<T>(data, *query, callback);
}

template<class T>
inline Option<T> BlendFieldInfo::GetValue(MemorySpan span) const
{
    if (m_Size != sizeof(T))
    {
        return NULL_OPTION;
    }

    if (auto value = GetData(span); value.size() == m_Size)
    {
        std::array<u8, sizeof(T)> result = {};
        std::copy(value.begin(), value.end(), result.data());
        return std::bit_cast<T>(result);
    }

    return NULL_OPTION;
}

template<class T>
inline Option<T> BlendFieldInfo::GetValue(const Block& block) const
{
    return GetValue<T>(block.body);
}

template<class T>
inline Option<T*> BlendFieldInfo::GetPointer(MemorySpan span) const
{
    const MemorySpan data = GetPointerData(span);

    if (data.size() == sizeof(T))
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<T*>(data.data());
    }

    return NULL_OPTION;
}

template<class T>
inline Option<T*> BlendFieldInfo::GetPointer(const Block& block) const
{
    return GetPointer<T>(block.body);
}

template<class T>
inline Option<T> BlendFieldInfo::GetPointerValue(MemorySpan span) const
{
    if (auto value = GetPointer<T>(span))
    {
        return **value;
    }

    return NULL_OPTION;
}

template<class T>
inline Option<T> BlendFieldInfo::GetPointerValue(const Block& block) const
{
    return GetPointerValue<T>(block.body);
}

static constexpr auto BlockFilter(const BlockCode& code)
{
    return [&code](const Block& block) -> bool
    {
        return block.header.code == code;
    };
}

inline auto Blend::GetBlocks(const BlockCode& code) const
{
    return m_File.blocks | ranges::views::filter(BlockFilter(code));
}

static constexpr auto TypeFilter(const Blend& blend, const BlendType& type)
{
    return [&blend, &type](const Block& block) -> bool
    {
        if (const auto block_type = blend.GetBlockType(block); block_type != NULL_OPTION)
        {
            return *block_type == type;
        }
        return false;
    };
}

inline auto Blend::GetBlocks(const BlendType& type) const
{
    return m_File.blocks | ranges::views::filter(TypeFilter(*this, type));
}
} // namespace cblend
