#pragma once

#include <cblend_format.hpp>
#include <cblend_query.hpp>
#include <cblend_reflection.hpp>
#include <cblend_stream.hpp>

#include <deque>
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
};

struct MemoryRange
{
    u64 head = 0;
    u64 tail = 0;
    std::span<const u8> span;
};

class MemoryTable final
{
public:
    MemoryTable() = default;
    explicit MemoryTable(std::vector<MemoryRange>& ranges);

    std::span<const u8> GetMemory(u64 address, usize size) const;
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

class BlendFieldInfo;

class BlendType
{
public:
    BlendType(const MemoryTable& memory_table, const Type& type);

    inline bool operator==(const BlendType& rhs) const = default;

    [[nodiscard]] bool IsArray() const;
    [[nodiscard]] bool IsPointer() const;
    [[nodiscard]] bool IsPrimitive() const;
    [[nodiscard]] bool IsStruct() const;

    [[nodiscard]] usize GetSize() const;

    [[nodiscard]] bool HasElementType() const;
    [[nodiscard]] Option<BlendType> GetElementType() const;

    [[nodiscard]] Option<BlendFieldInfo> GetField(std::string_view field_name) const;
    [[nodiscard]] std::vector<BlendFieldInfo> GetFields() const;

    template<class T>
    [[nodiscard]] Option<T> QueryValue(const Block& block, const Query& query) const;
    template<class T, QueryString Input>
    [[nodiscard]] Option<T> QueryValue(const Block& block) const;

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

    inline bool operator==(const BlendFieldInfo& rhs) const = default;

    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] const BlendType& GetDeclaringType() const;
    [[nodiscard]] const BlendType& GetFieldType() const;

    [[nodiscard]] std::span<const u8> GetData(std::span<const u8> span) const;
    [[nodiscard]] std::span<const u8> GetData(const Block& block) const;

    [[nodiscard]] std::span<const u8> GetPointerData(std::span<const u8> span) const;
    [[nodiscard]] std::span<const u8> GetPointerData(const Block& block) const;

    template<class T>
    [[nodiscard]] Option<T> GetValue(std::span<const u8> span) const;
    template<class T>
    [[nodiscard]] Option<T> GetValue(const Block& block) const;

    template<class T>
    [[nodiscard]] Option<T*> GetPointer(std::span<const u8> span) const;
    template<class T>
    [[nodiscard]] Option<T*> GetPointer(const Block& block) const;

    template<class T>
    [[nodiscard]] Option<T> GetPointerValue(std::span<const u8> span) const;
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
    static Result<const Blend, BlendError> Open(std::string_view path);

    [[nodiscard]] Endian GetEndian() const;
    [[nodiscard]] Pointer GetPointer() const;

    [[nodiscard]] usize GetBlockCount() const;
    [[nodiscard]] usize GetBlockCount(const BlockCode& code) const;

    [[nodiscard]] auto GetBlocks(const BlockCode& code) const;
    [[nodiscard]] Option<const Block&> GetBlock(const BlockCode& code) const;

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

template<class T>
inline Option<T> BlendType::QueryValue(const Block& block, const Query& query) const
{
    std::span<const u8> struct_data = block.body;
    std::unique_ptr<BlendFieldInfo> field_info = nullptr;
    std::span<const u8> field_data = block.body;
    std::unique_ptr<BlendType> type = std::make_unique<BlendType>(*this);

    for (const auto& token : query)
    {
        if (const auto* index = std::get_if<usize>(&token); index != nullptr && field_info != nullptr)
        {
            const auto element_type = type->GetElementType();
            if (!element_type)
            {
                // INDEXED_NON_ARRAY_OR_NON_POINTER
                return NULL_OPTION;
            }
            if (type->IsPointer())
            {
                type = std::make_unique<BlendType>(*element_type);
                field_data = field_info->GetPointerData(struct_data);
                if (field_data.data() != nullptr)
                {
                    field_data = std::span{ field_data.data() + (*index * field_data.size()), field_data.size() };
                }
            }
            else
            {
                type = std::make_unique<BlendType>(*element_type);
                return NULL_OPTION;
            }
        }
        else if (const auto* field_name = std::get_if<std::string_view>(&token); field_name != nullptr && type != nullptr)
        {
            if (type->IsStruct())
            {
                const auto info = type->GetField(*field_name);
                if (!info)
                {
                    // FIELD_NOT_FOUND
                    return NULL_OPTION;
                }
                struct_data = field_data;
                field_info = std::make_unique<BlendFieldInfo>(*info);
                field_data = field_info->GetData(field_data);
                type = std::make_unique<BlendType>(field_info->GetFieldType());
            }
            else
            {
                // INDEXED_NON_STRUCT
                return NULL_OPTION;
            }
        }
        else
        {
            // QUERY_INVALID
            return NULL_OPTION;
        }
    }

    if constexpr (std::is_pointer_v<T>)
    {
        if (field_data.data() == nullptr || (field_data.size() != 0 && field_data.size() != sizeof(std::remove_pointer_t<T>)))
        {
            // INVALID_TYPE
            return NULL_OPTION;
        }
        return reinterpret_cast<T>(field_data.data());
    }
    else
    {
        if (field_data.size() != sizeof(T))
        {
            // INVALID_TYPE
            return NULL_OPTION;
        }

        std::array<u8, sizeof(T)> result = {};
        std::copy(field_data.begin(), field_data.end(), result.data());
        return std::bit_cast<T>(result);
    }
}

template<class T, QueryString Input>
inline Option<T> BlendType::QueryValue(const Block& block) const
{
    const auto query = Query::Create<Input>();
    if (!query)
    {
        return NULL_OPTION;
    }
    return QueryValue<T>(block, *query);
}

template<class T>
inline Option<T> BlendFieldInfo::GetValue(std::span<const u8> span) const
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
inline Option<T*> BlendFieldInfo::GetPointer(std::span<const u8> span) const
{
    std::span<const u8> data = GetPointerData(span);

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
inline Option<T> BlendFieldInfo::GetPointerValue(std::span<const u8> span) const
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
    return m_File.blocks | std::views::filter(BlockFilter(code));
}
} // namespace cblend
