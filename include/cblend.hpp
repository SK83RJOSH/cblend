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
        std::array<u8, sizeof(T)> value;
        if (auto result = GetMemory(address, value.size()); !result.empty())
        {
            std::copy(result.begin(), result.end(), value.data());
            return std::bit_cast<T>(value);
        }
        return NULL_OPTION;
    }

private:
    std::vector<MemoryRange> m_Ranges;
};

enum class ReflectionError
{
    InvalidSdnaStruct,
    InvalidSdnaField,
    InvalidSdnaFieldName,
};

using BlendError = std::variant<FileStreamError, FormatError, ReflectionError>;

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

    [[nodiscard]] Option<const Type&> GetBlockType(const Block& block) const;

private:
    File m_File = {};
    TypeDatabase m_TypeDatabase = {};
    MemoryTable m_MemoryTable = {};

    Blend(File& file, TypeDatabase& type_database, MemoryTable& memory_table);
};

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
