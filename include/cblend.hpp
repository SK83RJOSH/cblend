#pragma once

#include <cblend_reflection.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <ranges>

namespace cblend
{
enum class Endian : u8
{
    Big = 0x56,
    Little = 0x76,
};

enum class Pointer : u8
{
    U32 = 0x5f,
    U64 = 0x2d,
};

static constexpr usize HEADER_MAGIC_LENGTH = 7U;
using HeaderMagic = std::array<u8, HEADER_MAGIC_LENGTH>;
static constexpr HeaderMagic HEADER_MAGIC = { 'B', 'L', 'E', 'N', 'D', 'E', 'R' };

static constexpr usize HEADER_VERSION_LENGTH = 3U;
using HeaderVersion = std::array<u8, HEADER_VERSION_LENGTH>;
static constexpr HeaderVersion HEADER_VERSION = { '1', '0', '0' };

struct Header
{
    HeaderMagic magic = HEADER_MAGIC;
    Pointer pointer = Pointer::U64;
    Endian endian = Endian::Little;
    HeaderVersion version = HEADER_VERSION;
};

struct BlockCode
{
    static constexpr usize ARRAY_VALUE_LENGTH = 4U;
    using ArrayValue = std::array<u8, ARRAY_VALUE_LENGTH>;

    constexpr BlockCode() = default;
    constexpr explicit BlockCode(uint32_t val) : value(val) {}
    constexpr explicit BlockCode(const ArrayValue& val) : value(std::bit_cast<uint32_t>(val)) {}

    inline bool operator==(const BlockCode& rhs) const = default;
    inline auto operator<=>(const BlockCode& rhs) const = default;

    uint32_t value = {};
};

static constexpr BlockCode BLOCK_CODE_DNA1({ 'D', 'N', 'A', '1' });
static constexpr BlockCode BLOCK_CODE_ENDB({ 'E', 'N', 'D', 'B' });

struct BlockHeader
{
    BlockCode code = {};
    u32 length = 0U;
    u64 address = 0U;
    u32 struct_index = 0U;
    u32 count = 0U;
};

struct Block
{
    BlockHeader header = {};
    std::vector<u8> body = {};
};

struct File
{
    Header header = {};
    std::vector<Block> blocks = {};
};

constexpr BlockCode SDNA_MAGIC({ 'S', 'D', 'N', 'A' });
constexpr BlockCode SDNA_NAME_MAGIC({ 'N', 'A', 'M', 'E' });
constexpr BlockCode SDNA_TYPE_MAGIC({ 'T', 'Y', 'P', 'E' });
constexpr BlockCode SDNA_TLEN_MAGIC({ 'T', 'L', 'E', 'N' });
constexpr BlockCode SDNA_STRC_MAGIC({ 'S', 'T', 'R', 'C' });

struct SdnaField
{
    u16 type_index;
    u16 name_index;
};

struct SdnaStruct
{
    u16 type_index;
    std::vector<SdnaField> fields;
};

struct Sdna
{
    std::vector<std::string_view> field_names;
    std::vector<std::string_view> type_names;
    std::vector<u16> type_lengths;
    std::vector<SdnaStruct> structs;
};

enum class BlendParseError
{
    Unknown,
    FileNotFound,
    DirectorySpecified,
    AccessDenied,
    InvalidFileHeader,
    InvalidBlockHeader,
    UnexpectedEndOfFile,
    FileNotExhausted,
    SdnaNotFound,
    InvalidSdnaHeader,
    UnexpectedEndOfSdna,
    SdnaNotExhausted,
    InvalidSdnaStruct,
    InvalidSdnaField,
    InvalidSdnaFieldName,
};

class Blend final
{
public:
    static Result<Blend, BlendParseError> Open(std::string_view path);

    [[nodiscard]] Endian GetEndian() const;
    [[nodiscard]] Pointer GetPointer() const;

    [[nodiscard]] usize GetBlockCount() const;
    [[nodiscard]] usize GetBlockCount(const BlockCode& code) const;

    [[nodiscard]] auto GetBlocks(const BlockCode& code) const;
    [[nodiscard]] Option<const Block&> GetBlock(const BlockCode& code) const;

private:
    const File m_File = {};
    const Sdna m_Sdna = {};

    Blend(File& file, Sdna& sdna);
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
