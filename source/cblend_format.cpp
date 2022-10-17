#include <cblend_format.hpp>
#include <cblend_stream.hpp>

using namespace cblend;

template<class T>
concept PtrType = std::is_same_v<T, u32> || std::is_same_v<T, u64>;

[[nodiscard]] Result<Header, FormatError> cblend::ReadHeader(Stream& stream)
{
    Header header;

    if (!stream.Read(header.magic))
    {
        return MakeError(FormatError::UnexpectedEndOfFile);
    }

    if (header.magic != HEADER_MAGIC)
    {
        return MakeError(FormatError::InvalidFileHeader);
    }

    if (!stream.Read(header.pointer))
    {
        return MakeError(FormatError::UnexpectedEndOfFile);
    }

    if (header.pointer != Pointer::U32 && header.pointer != Pointer::U64)
    {
        return MakeError(FormatError::InvalidFileHeader);
    }

    if (!stream.Read(header.endian))
    {
        return MakeError(FormatError::UnexpectedEndOfFile);
    }

    if (header.endian != Endian::Big && header.endian != Endian::Little)
    {
        return MakeError(FormatError::InvalidFileHeader);
    }

    if (!stream.Read(header.version))
    {
        return MakeError(FormatError::UnexpectedEndOfFile);
    }

    return header;
}

template<PtrType Ptr>
[[nodiscard]] Result<BlockHeader, FormatError> ReadBlockHeader(Stream& stream)
{
    BlockHeader header = {};

    if (!stream.Read(header.code))
    {
        return MakeError(FormatError::UnexpectedEndOfFile);
    }

    if (!stream.Read(header.length))
    {
        return MakeError(FormatError::UnexpectedEndOfFile);
    }

    Ptr address = 0U;

    if (!stream.Read(address))
    {
        return MakeError(FormatError::UnexpectedEndOfFile);
    }

    header.address = u64(address);

    if (!stream.Read(header.struct_index))
    {
        return MakeError(FormatError::UnexpectedEndOfFile);
    }

    if (!stream.Read(header.count))
    {
        return MakeError(FormatError::UnexpectedEndOfFile);
    }

    return header;
}

template<PtrType Ptr>
[[nodiscard]] Result<File, FormatError> ReadFile(Stream& stream, const Header& header)
{
    Block block;
    std::vector<Block> blocks;

    while (block.header.code != BLOCK_CODE_ENDB)
    {
        auto block_header = ReadBlockHeader<Ptr>(stream);

        if (!block_header)
        {
            return MakeError(block_header.error());
        }

        block.header = *block_header;

        if (block.header.length != 0)
        {
            block.body.resize(block.header.length, 0U);

            if (!stream.Read(std::as_writable_bytes(std::span{ block.body })))
            {
                return MakeError(FormatError::UnexpectedEndOfFile);
            }
        }

        blocks.push_back(block);
    }

    return File{
        header,
        blocks,
    };
}

[[nodiscard]] Result<File, FormatError> cblend::ReadFile(Stream& stream, const Header& header)
{
    if (header.pointer == Pointer::U32)
    {
        return ::ReadFile<u32>(stream, header);
    }

    return ::ReadFile<u64>(stream, header);
}

[[nodiscard]] Result<std::vector<std::string_view>, FormatError> ReadSdnaStrings(MemoryStream& stream, const BlockCode& code)
{
    BlockCode block_code;

    if (!stream.Read(block_code))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    if (block_code != code)
    {
        return MakeError(FormatError::InvalidSdnaHeader);
    }

    u32 count = 0U;

    if (!stream.Read(count))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    std::vector<std::string_view> strings(count, "");

    for (u32 name_index = 0; name_index < count; ++name_index)
    {
        if (!stream.Read(strings[name_index]))
        {
            return MakeError(FormatError::UnexpectedEndOfSdna);
        }
    }

    if (!stream.Align(4))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    return strings;
}

[[nodiscard]] Result<std::vector<u16>, FormatError> ReadSdnaLengths(Stream& stream, const usize count)
{
    BlockCode block_code;

    if (!stream.Read(block_code))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    if (block_code != BLOCK_CODE_TLEN)
    {
        return MakeError(FormatError::InvalidSdnaHeader);
    }

    auto type_lengths = std::vector<u16>(count, 0U);

    for (usize type_index = 0; type_index < count; ++type_index)
    {
        if (!stream.Read(type_lengths[type_index]))
        {
            return MakeError(FormatError::UnexpectedEndOfSdna);
        }
    }

    if (!stream.Align(4))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    return type_lengths;
}

[[nodiscard]] Result<SdnaStruct, FormatError> ReadSdnaStruct(Stream& stream)
{
    SdnaStruct result;

    if (!stream.Read(result.type_index))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    u16 field_count = 0U;

    if (!stream.Read(field_count))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    result.fields.resize(field_count);

    for (u32 field_index = 0; field_index < field_count; ++field_index)
    {
        auto& field = result.fields[field_index];

        if (!stream.Read(field.type_index))
        {
            return MakeError(FormatError::UnexpectedEndOfSdna);
        }

        if (!stream.Read(field.name_index))
        {
            return MakeError(FormatError::UnexpectedEndOfSdna);
        }
    }

    return result;
}

[[nodiscard]] Result<std::vector<SdnaStruct>, FormatError> ReadSdnaStructs(Stream& stream)
{
    BlockCode block_code;

    if (!stream.Read(block_code))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    if (block_code != BLOCK_CODE_STRC)
    {
        return MakeError(FormatError::InvalidSdnaHeader);
    }

    u32 struct_count = 0U;

    if (!stream.Read(struct_count))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    std::vector<SdnaStruct> result(struct_count, SdnaStruct{});

    for (u32 struct_index = 0; struct_index < struct_count; ++struct_index)
    {
        auto current = ReadSdnaStruct(stream);

        if (!current)
        {
            return MakeError(current.error());
        }

        result[struct_index] = std::move(*current);
    }

    return result;
}

Result<Sdna, FormatError> cblend::ReadSdna(const File& file)
{
    const auto block = std::ranges::find_if(file.blocks, [](const auto& block) { return block.header.code == BLOCK_CODE_DNA1; });

    if (block == file.blocks.end())
    {
        return MakeError(FormatError::SdnaNotFound);
    }

    auto stream = MemoryStream(block->body);

    BlockCode block_code;

    if (!stream.Read(block_code))
    {
        return MakeError(FormatError::UnexpectedEndOfSdna);
    }

    if (block_code != BLOCK_CODE_SDNA)
    {
        return MakeError(FormatError::InvalidSdnaHeader);
    }

    auto field_names = ReadSdnaStrings(stream, BLOCK_CODE_NAME);

    if (!field_names)
    {
        return MakeError(field_names.error());
    }

    auto type_names = ReadSdnaStrings(stream, BLOCK_CODE_TYPE);

    if (!type_names)
    {
        return MakeError(type_names.error());
    }

    auto type_lengths = ReadSdnaLengths(stream, type_names->size());

    if (!type_lengths)
    {
        return MakeError(type_lengths.error());
    }

    auto structs = ReadSdnaStructs(stream);

    if (!structs)
    {
        return MakeError(structs.error());
    }

    if (!stream.IsAtEnd())
    {
        return MakeError(FormatError::SdnaNotExhausted);
    }

    return Sdna{
        .field_names = std::move(*field_names),
        .type_names = std::move(*type_names),
        .type_lengths = std::move(*type_lengths),
        .structs = std::move(*structs),
    };
}
