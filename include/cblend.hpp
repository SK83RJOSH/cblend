#pragma once

#include <cblend_format.hpp>
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
    Option<T> GetMemory(u64 address) const
    {
        if (auto result = GetMemory(address, sizeof(T)); !result.empty())
        {
            std::array<u8, sizeof(T)> value;
            std::copy(result.begin(), result.end(), value.data());
            return std::bit_cast<T>(value);
        }
        return NULL_OPTION;
    }

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
    BlendType(const Type& type);

    inline bool operator==(const BlendType& rhs) const = default;

    [[nodiscard]] bool HasElementType() const;
    [[nodiscard]] Option<BlendType> GetElementType() const;

    [[nodiscard]] Option<BlendFieldInfo> GetField(std::string_view field_name) const;
    [[nodiscard]] std::vector<BlendFieldInfo> GetFields() const;

private:
    const Type* m_ElementType = nullptr;
    std::vector<const AggregateType::Field*> m_Fields;
    std::unordered_map<std::string_view, const AggregateType::Field*> m_FieldsByName;
};

class BlendFieldInfo
{
public:
    BlendFieldInfo(const AggregateType::Field& field, const BlendType& declaring_type);

    inline bool operator==(const BlendFieldInfo& rhs) const = default;

    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] const BlendType& GetDeclaringType() const;
    [[nodiscard]] const BlendType& GetFieldType() const;

    [[nodiscard]] std::span<const u8> GetData(std::span<const u8> span) const;
    [[nodiscard]] std::span<const u8> GetData(const Block& block) const { return GetData(block.body); }

    template<class T>
    [[nodiscard]] Option<T> GetValue(std::span<const u8> span) const;
    template<class T>
    [[nodiscard]] Option<T> GetValue(const Block& block) const;

private:
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
inline [[nodiscard]] Option<T> BlendFieldInfo::GetValue(std::span<const u8> span) const
{
    if (m_Size != sizeof(T))
    {
        return NULL_OPTION;
    }

    if (auto value = GetData(span); value.size() == m_Size)
    {
        std::array<u8, sizeof(T)> result;
        std::copy(value.begin(), value.end(), result.data());
        return std::bit_cast<T>(result);
    }

    return NULL_OPTION;
}

template<class T>
inline [[nodiscard]] Option<T> BlendFieldInfo::GetValue(const Block& block) const
{
    return GetValue<T>(block.body);
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
