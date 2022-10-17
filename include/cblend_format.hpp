#pragma once

#include <cblend_types.hpp>

#include <array>
#include <bit>
#include <string_view>
#include <vector>

namespace cblend
{
class Stream;

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

class BlockCode
{
public:
    static constexpr usize ARRAY_VALUE_LENGTH = 4U;
    using ArrayValue = std::array<u8, ARRAY_VALUE_LENGTH>;

    constexpr BlockCode() = default;
    constexpr explicit BlockCode(u32 value) : m_Value(value) {}
    constexpr explicit BlockCode(const ArrayValue& value) : m_Value(std::bit_cast<u32>(value))
    {
        // Apparently blender isn't writing block codes as a char sequence...
        if constexpr (std::endian::native == std::endian::big)
        {
            u32 swapped = (m_Value >> 24) & 0xFF;
            swapped |= ((m_Value >> 16) & 0xFF) << 8;
            swapped |= ((m_Value >> 8) & 0xFF) << 16;
            swapped |= (m_Value & 0xFF) << 24;
            m_Value = swapped;
        }
    }

    inline bool operator==(const BlockCode& rhs) const = default;
    inline auto operator<=>(const BlockCode& rhs) const = default;

private:
    u32 m_Value = {};
};

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

struct SdnaField
{
    u16 type_index = 0U;
    u16 name_index = 0U;
};

struct SdnaStruct
{
    u16 type_index = 0U;
    std::vector<SdnaField> fields = {};
};

struct Sdna
{
    std::vector<std::string_view> field_names = {};
    std::vector<std::string_view> type_names = {};
    std::vector<u16> type_lengths = {};
    std::vector<SdnaStruct> structs = {};
};

enum class FormatError
{
    InvalidFileHeader,
    InvalidBlockHeader,
    UnexpectedEndOfFile,
    FileNotExhausted,
    SdnaNotFound,
    InvalidSdnaHeader,
    UnexpectedEndOfSdna,
    SdnaNotExhausted,
};

[[nodiscard]] Result<Header, FormatError> ReadHeader(Stream& stream);
[[nodiscard]] Result<File, FormatError> ReadFile(Stream& stream, const Header& header);
[[nodiscard]] Result<Sdna, FormatError> ReadSdna(const File& file);

static constexpr BlockCode BLOCK_CODE_DATA({ 'D', 'A', 'T', 'A' }); // Arbitrary data
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
static constexpr BlockCode BLOCK_CODE_CO({ 'C', 'O' }); // Constraint
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

constexpr BlockCode BLOCK_CODE_SDNA({ 'S', 'D', 'N', 'A' });
constexpr BlockCode BLOCK_CODE_NAME({ 'N', 'A', 'M', 'E' });
constexpr BlockCode BLOCK_CODE_TYPE({ 'T', 'Y', 'P', 'E' });
constexpr BlockCode BLOCK_CODE_TLEN({ 'T', 'L', 'E', 'N' });
constexpr BlockCode BLOCK_CODE_STRC({ 'S', 'T', 'R', 'C' });
} // namespace cblend
