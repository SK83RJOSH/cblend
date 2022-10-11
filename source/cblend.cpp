#include <cblend.hpp>
#include <cblend_stream.hpp>

#include <filesystem>

using namespace cblend;

template<class T>
concept PtrType = std::is_same_v<T, u32> || std::is_same_v<T, u64>;

[[nodiscard]] result<FileStream, BlendParseError> CreateStream(std::string_view path)
{
    namespace fs = std::filesystem;

    auto file_path = fs::path(path);

    if (!fs::exists(file_path))
    {
        return make_error(BlendParseError::FileNotFound);
    }

    if (!fs::is_regular_file(file_path))
    {
        return make_error(BlendParseError::DirectorySpecified);
    }

    std::ifstream stream(file_path.c_str(), std::ifstream::binary);

    if (stream.bad())
    {
        return make_error(BlendParseError::AccessDenied);
    }

    return FileStream(stream);
}

[[nodiscard]] result<Header, BlendParseError> ReadHeader(Stream& stream)
{
    Header header;

    if (!stream.Read(header.magic))
    {
        return make_error(BlendParseError::UnexpectedEndOfFile);
    }

    if (header.magic != HEADER_MAGIC)
    {
        return make_error(BlendParseError::InvalidFileHeader);
    }

    if (!stream.Read(header.pointer))
    {
        return make_error(BlendParseError::UnexpectedEndOfFile);
    }

    if (header.pointer != Pointer::U32 && header.pointer != Pointer::U64)
    {
        return make_error(BlendParseError::InvalidFileHeader);
    }

    if (!stream.Read(header.endian))
    {
        return make_error(BlendParseError::UnexpectedEndOfFile);
    }

    if (header.endian != Endian::Big && header.endian != Endian::Little)
    {
        return make_error(BlendParseError::InvalidFileHeader);
    }

    if (!stream.Read(header.version))
    {
        return make_error(BlendParseError::UnexpectedEndOfFile);
    }

    return header;
}

template<PtrType Ptr>
[[nodiscard]] result<BlockHeader, BlendParseError> ReadBlockHeader(Stream& stream)
{
    BlockHeader header = {};

    if (!stream.Read(header.code))
    {
        return make_error(BlendParseError::UnexpectedEndOfFile);
    }

    if (!stream.Read(header.length))
    {
        return make_error(BlendParseError::UnexpectedEndOfFile);
    }

    Ptr address = 0U;

    if (!stream.Read(address))
    {
        return make_error(BlendParseError::UnexpectedEndOfFile);
    }

    header.address = u64(address);

    if (!stream.Read(header.sdna_index))
    {
        return make_error(BlendParseError::UnexpectedEndOfFile);
    }

    if (!stream.Read(header.count))
    {
        return make_error(BlendParseError::UnexpectedEndOfFile);
    }

    return header;
}

template<PtrType Ptr>
[[nodiscard]] result<File, BlendParseError> ReadFile(Stream& stream, const Header& header)
{
    Block block;
    std::vector<Block> blocks;

    while (block.header.code != BLOCK_CODE_ENDB)
    {
        auto block_header = ReadBlockHeader<Ptr>(stream);

        if (!block_header)
        {
            return make_error(block_header.error());
        }

        block.header = *block_header;

        if (block.header.length != 0)
        {
            block.body.resize(block.header.length, 0U);

            if (!stream.Read(std::as_writable_bytes(std::span{ block.body })))
            {
                return make_error(BlendParseError::UnexpectedEndOfFile);
            }
        }

        blocks.push_back(block);
    }

    return File{
        header,
        blocks,
    };
}

[[nodiscard]] result<std::vector<std::string_view>, BlendParseError> ReadSdnaStrings(MemoryStream& stream, const BlockCode& code)
{
    BlockCode magic;

    if (!stream.Read(magic))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    if (magic != code)
    {
        return make_error(BlendParseError::InvalidSdnaHeader);
    }

    u32 count = 0U;

    if (!stream.Read(count))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    std::vector<std::string_view> strings(count, "");

    for (u32 name_index = 0; name_index < count; ++name_index)
    {
        if (!stream.Read(strings[name_index]))
        {
            return make_error(BlendParseError::UnexpectedEndOfSdna);
        }
    }

    if (!stream.Align(4))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    return strings;
}

[[nodiscard]] result<std::vector<u16>, BlendParseError> ReadSdnaLengths(Stream& stream, const usize count)
{
    BlockCode magic;

    if (!stream.Read(magic))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    if (magic != SDNA_TLEN_MAGIC)
    {
        return make_error(BlendParseError::InvalidSdnaHeader);
    }

    auto type_lengths = std::vector<u16>(count, 0U);

    for (usize type_index = 0; type_index < count; ++type_index)
    {
        if (!stream.Read(type_lengths[type_index]))
        {
            return make_error(BlendParseError::UnexpectedEndOfSdna);
        }
    }

    if (!stream.Align(4))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    return type_lengths;
}

[[nodiscard]] result<SdnaStruct, BlendParseError> ReadSdnaStruct(Stream& stream)
{
    SdnaStruct result;

    if (!stream.Read(result.type_index))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    u16 field_count = 0U;

    if (!stream.Read(field_count))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    result.fields.resize(field_count);

    for (u32 field_index = 0; field_index < field_count; ++field_index)
    {
        auto& field = result.fields[field_index];

        if (!stream.Read(field.type_index))
        {
            return make_error(BlendParseError::UnexpectedEndOfSdna);
        }

        if (!stream.Read(field.name_index))
        {
            return make_error(BlendParseError::UnexpectedEndOfSdna);
        }
    }

    return result;
}

[[nodiscard]] result<std::vector<SdnaStruct>, BlendParseError> ReadSdnaStructs(Stream& stream)
{
    BlockCode magic;

    if (!stream.Read(magic))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    if (magic != SDNA_STRC_MAGIC)
    {
        return make_error(BlendParseError::InvalidSdnaHeader);
    }

    u32 struct_count = 0U;

    if (!stream.Read(struct_count))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    std::vector<SdnaStruct> result(struct_count, SdnaStruct{});

    for (u32 struct_index = 0; struct_index < struct_count; ++struct_index)
    {
        auto current = ReadSdnaStruct(stream);

        if (!current)
        {
            return make_error(current.error());
        }

        result[struct_index] = std::move(*current);
    }

    return result;
}

result<Sdna, BlendParseError> ReadSdna(const File& file)
{
    auto blocks = file.blocks | std::views::filter(BlockFilter(BLOCK_CODE_DNA1));

    if (blocks.end() == blocks.begin())
    {
        return make_error(BlendParseError::SdnaNotFound);
    }

    auto stream = MemoryStream(blocks.begin()->body);

    BlockCode magic;

    if (!stream.Read(magic))
    {
        return make_error(BlendParseError::UnexpectedEndOfSdna);
    }

    if (magic != SDNA_MAGIC)
    {
        return make_error(BlendParseError::InvalidSdnaHeader);
    }

    auto field_names = ReadSdnaStrings(stream, SDNA_NAME_MAGIC);

    if (!field_names)
    {
        return make_error(field_names.error());
    }

    auto type_names = ReadSdnaStrings(stream, SDNA_TYPE_MAGIC);

    if (!type_names)
    {
        return make_error(type_names.error());
    }

    auto type_lengths = ReadSdnaLengths(stream, type_names->size());

    if (!type_lengths)
    {
        return make_error(type_lengths.error());
    }

    auto structs = ReadSdnaStructs(stream);

    if (!structs)
    {
        return make_error(structs.error());
    }

    if (!stream.IsAtEnd())
    {
        return make_error(BlendParseError::SdnaNotExhausted);
    }

    return Sdna{
        .field_names = std::move(*field_names),
        .type_names = std::move(*type_names),
        .type_lengths = std::move(*type_lengths),
        .structs = std::move(*structs),
    };
}

result<Blend, BlendParseError> Blend::Open(std::string_view path)
{
    auto stream = CreateStream(path);

    if (!stream)
    {
        return make_error(stream.error());
    }

    const auto header = ReadHeader(*stream);

    if (!header)
    {
        return make_error(header.error());
    }

    if (header->endian == Endian::Little)
    {
        stream->SetEndian(std::endian::little);
    }
    else
    {
        stream->SetEndian(std::endian::big);
    }

    auto file = [&]
    {
        if (header->pointer == Pointer::U32)
        {
            return ReadFile<u32>(*stream, *header);
        }

        return ReadFile<u64>(*stream, *header);
    }();

    if (!file)
    {
        return make_error(file.error());
    }

    if (!stream->IsAtEnd())
    {
        return make_error(BlendParseError::FileNotExhausted);
    }

    auto sdna = ReadSdna(*file);

    if (!sdna)
    {
        return make_error(sdna.error());
    }

    return Blend(*file, *sdna);
}

cblend::Blend::Blend(File& file, Sdna& sdna) : m_File(std::move(file)), m_Sdna(std::move(sdna)) {}
