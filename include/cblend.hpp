#pragma once

#include <cblend_reflection.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <deque>
#include <ranges>
#include <string>
#include <unordered_map>

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
    constexpr explicit BlockCode(u32 val) : value(val) {}
    constexpr explicit BlockCode(const ArrayValue& val)
    {
        value = std::bit_cast<u32>(val);

        // Apparently blender isn't writing block codes as a char sequence...
        if constexpr (std::endian::native == std::endian::big)
        {
            u32 swapped = (value >> 24) & 0xFF;
            swapped |= ((value >> 16) & 0xFF) << 8;
            swapped |= ((value >> 8) & 0xFF) << 16;
            swapped |= (value & 0xFF) << 24;
            value = swapped;
        }
    }

    inline bool operator==(const BlockCode& rhs) const = default;
    inline auto operator<=>(const BlockCode& rhs) const = default;

    u32 value = {};
};

static constexpr BlockCode BLOCK_CODE_DATA({ 'D', 'A', 'T', 'A' });// Arbitrary data
static constexpr BlockCode BLOCK_CODE_GLOB({ 'G', 'L', 'O', 'B' }); // Global struct
static constexpr BlockCode BLOCK_CODE_DNA1({ 'D', 'N', 'A', '1' }); // SDNA data
static constexpr BlockCode BLOCK_CODE_TEST({ 'T', 'E', 'S', 'T' }); // Thumbnail previews
static constexpr BlockCode BLOCK_CODE_REND({ 'R', 'E', 'N', 'D' }); // Scene and frame info
static constexpr BlockCode BLOCK_CODE_USER({ 'U', 'S', 'E', 'R' }); // User preferences
static constexpr BlockCode BLOCK_CODE_ENDB({ 'E', 'N', 'D', 'B' }); // End of file
static constexpr BlockCode BLOCK_CODE_AC({ 'A', 'C' }); // Action channel
static constexpr BlockCode BLOCK_CODE_AR({ 'A', 'R' }); // Armature
static constexpr BlockCode BLOCK_CODE_BR({ 'B', 'R' }); // Brush
static constexpr BlockCode BLOCK_CODE_CA({ 'C', 'A' }); // Camera
static constexpr BlockCode BLOCK_CODE_CF({ 'C', 'F' }); // Cache file
static constexpr BlockCode BLOCK_CODE_Co({ 'C', 'O' }); // Constraint
static constexpr BlockCode BLOCK_CODE_CU({ 'C', 'U' }); // Curve
static constexpr BlockCode BLOCK_CODE_CV({ 'C', 'V' }); // Curves
static constexpr BlockCode BLOCK_CODE_FS({ 'F', 'S' }); // Fluid sim
static constexpr BlockCode BLOCK_CODE_GD({ 'G', 'D' }); // Grease pencil
static constexpr BlockCode BLOCK_CODE_GR({ 'G', 'R' }); // Collection
static constexpr BlockCode BLOCK_CODE_ID({ 'I', 'D' }); // Placeholder
static constexpr BlockCode BLOCK_CODE_IM({ 'I', 'M' }); // Image
static constexpr BlockCode BLOCK_CODE_IP({ 'I', 'P' }); // Ipo
static constexpr BlockCode BLOCK_CODE_KE({ 'K', 'E' }); // Shape key
static constexpr BlockCode BLOCK_CODE_LA({ 'L', 'A' }); // Light
static constexpr BlockCode BLOCK_CODE_LI({ 'L', 'I' }); // Library
static constexpr BlockCode BLOCK_CODE_LP({ 'L', 'P' }); // Light probe
static constexpr BlockCode BLOCK_CODE_LS({ 'L', 'S' }); // Line style
static constexpr BlockCode BLOCK_CODE_LT({ 'L', 'T' }); // Lattice
static constexpr BlockCode BLOCK_CODE_MA({ 'M', 'A' }); // Material
static constexpr BlockCode BLOCK_CODE_MB({ 'M', 'B' }); // Meta ball
static constexpr BlockCode BLOCK_CODE_MC({ 'M', 'C' }); // Movie clip
static constexpr BlockCode BLOCK_CODE_ME({ 'M', 'E' }); // Mesh
static constexpr BlockCode BLOCK_CODE_MS({ 'M', 'S' }); // Mask
static constexpr BlockCode BLOCK_CODE_NL({ 'N', 'L' }); // Outline
static constexpr BlockCode BLOCK_CODE_NT({ 'N', 'T' }); // Node tree
static constexpr BlockCode BLOCK_CODE_OB({ 'O', 'B' }); // Object
static constexpr BlockCode BLOCK_CODE_PA({ 'P', 'A' }); // Particle settings
static constexpr BlockCode BLOCK_CODE_PC({ 'P', 'C' }); // Paint curve
static constexpr BlockCode BLOCK_CODE_PL({ 'P', 'L' }); // Palette
static constexpr BlockCode BLOCK_CODE_PT({ 'P', 'T' }); // Point cloud
static constexpr BlockCode BLOCK_CODE_SC({ 'S', 'C' }); // Scene
static constexpr BlockCode BLOCK_CODE_SI({ 'S', 'I' }); // Simulation
static constexpr BlockCode BLOCK_CODE_SK({ 'S', 'K' }); // Speaker
static constexpr BlockCode BLOCK_CODE_SN({ 'S', 'N' }); // Deprecated
static constexpr BlockCode BLOCK_CODE_SO({ 'S', 'O' }); // Sound
static constexpr BlockCode BLOCK_CODE_SQ({ 'S', 'Q' }); // Fake data
static constexpr BlockCode BLOCK_CODE_SR({ 'S', 'R' }); // Screen
static constexpr BlockCode BLOCK_CODE_TE({ 'T', 'E' }); // Texture
static constexpr BlockCode BLOCK_CODE_TX({ 'T', 'X' }); // Text
static constexpr BlockCode BLOCK_CODE_VF({ 'V', 'F' }); // Vector font
static constexpr BlockCode BLOCK_CODE_VO({ 'V', 'O' }); // Volume
static constexpr BlockCode BLOCK_CODE_WM({ 'W', 'M' }); // Window manager
static constexpr BlockCode BLOCK_CODE_WO({ 'W', 'O' }); // World
static constexpr BlockCode BLOCK_CODE_WS({ 'W', 'S' }); // Workspace

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

using TypeList = std::deque<TypeContainer>;
using TypeMap = std::unordered_map<std::string_view, usize>;
using StructMap = std::unordered_map<usize, usize>;

struct TypeDatabase
{
    TypeList type_list;
    TypeMap type_map;
    StructMap struct_map;
};

class Blend final
{
public:
    static Result<const Blend, BlendParseError> Open(std::string_view path);

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

    Blend(File& file, TypeDatabase& type_database);
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
