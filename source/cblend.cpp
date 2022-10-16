#include <cblend.hpp>
#include <cblend_stream.hpp>

#include <cctype>
#include <charconv>
#include <deque>
#include <filesystem>
#include <memory>

using namespace cblend;

template<class T>
concept PtrType = std::is_same_v<T, u32> || std::is_same_v<T, u64>;

[[nodiscard]] Result<FileStream, BlendParseError> CreateStream(std::string_view path)
{
    namespace fs = std::filesystem;

    auto file_path = fs::path(path);

    if (!fs::exists(file_path))
    {
        return MakeError(BlendParseError::FileNotFound);
    }

    if (!fs::is_regular_file(file_path))
    {
        return MakeError(BlendParseError::DirectorySpecified);
    }

    std::ifstream stream(file_path.c_str(), std::ifstream::binary);

    if (stream.bad())
    {
        return MakeError(BlendParseError::AccessDenied);
    }

    return FileStream(stream);
}

[[nodiscard]] Result<Header, BlendParseError> ReadHeader(Stream& stream)
{
    Header header;

    if (!stream.Read(header.magic))
    {
        return MakeError(BlendParseError::UnexpectedEndOfFile);
    }

    if (header.magic != HEADER_MAGIC)
    {
        return MakeError(BlendParseError::InvalidFileHeader);
    }

    if (!stream.Read(header.pointer))
    {
        return MakeError(BlendParseError::UnexpectedEndOfFile);
    }

    if (header.pointer != Pointer::U32 && header.pointer != Pointer::U64)
    {
        return MakeError(BlendParseError::InvalidFileHeader);
    }

    if (!stream.Read(header.endian))
    {
        return MakeError(BlendParseError::UnexpectedEndOfFile);
    }

    if (header.endian != Endian::Big && header.endian != Endian::Little)
    {
        return MakeError(BlendParseError::InvalidFileHeader);
    }

    if (!stream.Read(header.version))
    {
        return MakeError(BlendParseError::UnexpectedEndOfFile);
    }

    return header;
}

template<PtrType Ptr>
[[nodiscard]] Result<BlockHeader, BlendParseError> ReadBlockHeader(Stream& stream)
{
    BlockHeader header = {};

    if (!stream.Read(header.code))
    {
        return MakeError(BlendParseError::UnexpectedEndOfFile);
    }

    if (!stream.Read(header.length))
    {
        return MakeError(BlendParseError::UnexpectedEndOfFile);
    }

    Ptr address = 0U;

    if (!stream.Read(address))
    {
        return MakeError(BlendParseError::UnexpectedEndOfFile);
    }

    header.address = u64(address);

    if (!stream.Read(header.struct_index))
    {
        return MakeError(BlendParseError::UnexpectedEndOfFile);
    }

    if (!stream.Read(header.count))
    {
        return MakeError(BlendParseError::UnexpectedEndOfFile);
    }

    return header;
}

template<PtrType Ptr>
[[nodiscard]] Result<File, BlendParseError> ReadFile(Stream& stream, const Header& header)
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
                return MakeError(BlendParseError::UnexpectedEndOfFile);
            }
        }

        blocks.push_back(block);
    }

    return File{
        header,
        blocks,
    };
}

[[nodiscard]] Result<std::vector<std::string_view>, BlendParseError> ReadSdnaStrings(MemoryStream& stream, const BlockCode& code)
{
    BlockCode magic;

    if (!stream.Read(magic))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
    }

    if (magic != code)
    {
        return MakeError(BlendParseError::InvalidSdnaHeader);
    }

    u32 count = 0U;

    if (!stream.Read(count))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
    }

    std::vector<std::string_view> strings(count, "");

    for (u32 name_index = 0; name_index < count; ++name_index)
    {
        if (!stream.Read(strings[name_index]))
        {
            return MakeError(BlendParseError::UnexpectedEndOfSdna);
        }
    }

    if (!stream.Align(4))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
    }

    return strings;
}

[[nodiscard]] Result<std::vector<u16>, BlendParseError> ReadSdnaLengths(Stream& stream, const usize count)
{
    BlockCode magic;

    if (!stream.Read(magic))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
    }

    if (magic != SDNA_TLEN_MAGIC)
    {
        return MakeError(BlendParseError::InvalidSdnaHeader);
    }

    auto type_lengths = std::vector<u16>(count, 0U);

    for (usize type_index = 0; type_index < count; ++type_index)
    {
        if (!stream.Read(type_lengths[type_index]))
        {
            return MakeError(BlendParseError::UnexpectedEndOfSdna);
        }
    }

    if (!stream.Align(4))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
    }

    return type_lengths;
}

[[nodiscard]] Result<SdnaStruct, BlendParseError> ReadSdnaStruct(Stream& stream)
{
    SdnaStruct result;

    if (!stream.Read(result.type_index))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
    }

    u16 field_count = 0U;

    if (!stream.Read(field_count))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
    }

    result.fields.resize(field_count);

    for (u32 field_index = 0; field_index < field_count; ++field_index)
    {
        auto& field = result.fields[field_index];

        if (!stream.Read(field.type_index))
        {
            return MakeError(BlendParseError::UnexpectedEndOfSdna);
        }

        if (!stream.Read(field.name_index))
        {
            return MakeError(BlendParseError::UnexpectedEndOfSdna);
        }
    }

    return result;
}

[[nodiscard]] Result<std::vector<SdnaStruct>, BlendParseError> ReadSdnaStructs(Stream& stream)
{
    BlockCode magic;

    if (!stream.Read(magic))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
    }

    if (magic != SDNA_STRC_MAGIC)
    {
        return MakeError(BlendParseError::InvalidSdnaHeader);
    }

    u32 struct_count = 0U;

    if (!stream.Read(struct_count))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
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

Result<Sdna, BlendParseError> ReadSdna(const File& file)
{
    auto blocks = file.blocks | std::views::filter(BlockFilter(BLOCK_CODE_DNA1));

    if (blocks.end() == blocks.begin())
    {
        return MakeError(BlendParseError::SdnaNotFound);
    }

    auto stream = MemoryStream(blocks.begin()->body);

    BlockCode magic;

    if (!stream.Read(magic))
    {
        return MakeError(BlendParseError::UnexpectedEndOfSdna);
    }

    if (magic != SDNA_MAGIC)
    {
        return MakeError(BlendParseError::InvalidSdnaHeader);
    }

    auto field_names = ReadSdnaStrings(stream, SDNA_NAME_MAGIC);

    if (!field_names)
    {
        return MakeError(field_names.error());
    }

    auto type_names = ReadSdnaStrings(stream, SDNA_TYPE_MAGIC);

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
        return MakeError(BlendParseError::SdnaNotExhausted);
    }

    return Sdna{
        .field_names = std::move(*field_names),
        .type_names = std::move(*type_names),
        .type_lengths = std::move(*type_lengths),
        .structs = std::move(*structs),
    };
}

bool IsValidName(std::string_view name)
{
    // Must not be empty
    if (name.empty())
    {
        return false;
    }

    // Must begin with A-Z or _
    if (std::isalpha(name[0]) == 0 && name[0] != '_')
    {
        return false;
    }

    // All characters must be A-Z, 0-9, or _
    return std::ranges::all_of(name, [](const char chr) { return std::isalpha(chr) != 0 || std::isdigit(chr) != 0 || chr == '_'; });
}

Result<const Type* const*, BlendParseError> ProcessFunctionPointer(std::string_view field_name, usize pointer_size, TypeList& types)
{
    if (!field_name.starts_with('('))
    {
        return nullptr;
    }

    static constexpr usize MIN_FUNCTION_POINTER_LENGTH = 5;

    if (field_name.size() <= MIN_FUNCTION_POINTER_LENGTH)
    {
        return MakeError(BlendParseError::InvalidSdnaFieldName);
    }

    std::string_view name(field_name.begin() + 2, field_name.end() - 3);

    if (!IsValidName(name))
    {
        return MakeError(BlendParseError::InvalidSdnaFieldName);
    }

    types.push_back(MakeContainer<FunctionType>(name));
    types.push_back(MakeContainer<PointerType>(&types.back(), pointer_size));
    return &types.back();
}

usize CountPointers(std::string_view field_name)
{
    // Scan until we reach the beginning of the field name
    ssize index = 0;
    while (index + 1 < field_name.size() && field_name[index] == '*')
    {
        ++index;
    }
    return index;
}

usize CalculateNameLength(std::string_view field_name, usize pointer_count)
{
    // Scan until we reach the end or an array
    usize index = pointer_count;
    while (index < field_name.size() && field_name[index] != '[')
    {
        ++index;
    }
    return index - pointer_count;
}

const Type* const* AddPointers(usize pointer_count, const Type* const* type, usize pointer_size, TypeList& types)
{
    while (pointer_count > 0)
    {
        types.push_back(MakeContainer<PointerType>(type, pointer_size));
        type = &types.back();
        --pointer_count;
    }
    return type;
}

Result<const Type* const*, BlendParseError>
ProcessField(std::string_view field_name, usize pointer_size, const Type* const* field_type, TypeList& types)
{
    const usize pointer_count = CountPointers(field_name);
    const usize name_length = CalculateNameLength(field_name, pointer_count);
    const std::string_view name = std::string_view(&field_name[pointer_count], name_length);
    const Type* const* type = field_type;

    if (!IsValidName(name))
    {
        return MakeError(BlendParseError::InvalidSdnaFieldName);
    }

    usize name_end = pointer_count + name_length;

    // Handle arrays such as: field_name[0][1]
    while (name_end + 1 < field_name.size())
    {
        auto array_begin = name_end + 1;

        // First character must be '['
        if (field_name[name_end] != '[')
        {
            return MakeError(BlendParseError::InvalidSdnaFieldName);
        }

        while (name_end + 1 < field_name.size())
        {
            ++name_end;
            // Handle the ending of an array
            if (field_name[name_end] == ']')
            {
                if (name_end + 1 >= field_name.size())
                {
                    continue;
                }

                // Next character must be '['
                if (field_name[++name_end] != '[')
                {
                    return MakeError(BlendParseError::InvalidSdnaFieldName);
                }
            }
            // We encountered a non-digit
            else if (std::isdigit(field_name[name_end]) == 0)
            {
                return MakeError(BlendParseError::InvalidSdnaFieldName);
            }
        }

        auto array_end = name_end;

        // Last character must be ']'
        if (field_name[name_end] != ']')
        {
            return MakeError(BlendParseError::InvalidSdnaFieldName);
        }

        usize count = 0;
        auto result = std::from_chars(field_name.data() + array_begin, field_name.data() + array_end, count);

        if (result.ec != std::errc())
        {
            return MakeError(BlendParseError::InvalidSdnaFieldName);
        }

        types.push_back(MakeContainer<ArrayType>(count, type));
        type = &types.back();
    }

    return AddPointers(pointer_count, type, pointer_size, types);
}

Result<TypeDatabase, BlendParseError> CreateTypeDatabase(const File& file, const Sdna& sdna)
{
    const usize type_count = sdna.type_lengths.size();
    const usize struct_count = sdna.structs.size();
    const usize pointer_size = file.header.pointer == Pointer::U32 ? sizeof(u32) : sizeof(u64);
    const usize field_name_count = sdna.field_names.size();

    // First pass, assumes all types are fundamental
    TypeDatabase type_database;
    type_database.type_list.resize(type_count);
    type_database.type_map.reserve(type_count);

    for (usize type_index = 0; type_index < type_count; ++type_index)
    {
        const auto size = sdna.type_lengths[type_index];
        const auto& name = sdna.type_names[type_index];
        type_database.type_list[type_index] = MakeContainer<FundamentalType>(name, size);
        type_database.type_map.emplace(name, type_index);
    }

    // Second pass, construct all aggregrate types
    usize struct_index = 0;
    type_database.struct_map.reserve(struct_count);

    for (const auto& [type_index, fields] : sdna.structs)
    {
        if (type_index >= type_count)
        {
            return MakeError(BlendParseError::InvalidSdnaStruct);
        }

        type_database.struct_map.emplace(++struct_index, type_index);

        const usize field_count = fields.size();
        std::vector<const Type* const*> aggregate_fields;
        aggregate_fields.reserve(field_count);

        for (const auto& [field_type_index, field_name_index] : fields)
        {
            if (field_name_index >= field_name_count || field_type_index >= type_count)
            {
                return MakeError(BlendParseError::InvalidSdnaField);
            }

            const auto& field_name = sdna.field_names[field_name_index];

            // Handle function pointers such as: (*field_name)()
            if (const auto type = ProcessFunctionPointer(field_name, pointer_size, type_database.type_list); *type != nullptr)
            {
                aggregate_fields.push_back(*type);
                continue;
            }
            // NOLINTNEXTLINE(readability-else-after-return)
            else if (!type.has_value())
            {
                return MakeError(type.error());
            }

            const auto& field_type = type_database.type_list[field_type_index];

            // Handle generic fields such as: **field_name[1][2]
            if (const auto type = ProcessField(field_name, pointer_size, &field_type, type_database.type_list); *type != nullptr)
            {
                aggregate_fields.push_back(*type);
                continue;
            }
            // NOLINTNEXTLINE(readability-else-after-return)
            else if (!type.has_value())
            {
                return MakeError(type.error());
            }

            return MakeError(BlendParseError::InvalidSdnaField);
        }

        const auto size = sdna.type_lengths[type_index];
        const auto& name = sdna.type_names[type_index];
        type_database.type_list[type_index] = MakeContainer<AggregateType>(size, name, aggregate_fields);
    }

    return type_database;
}

Result<const Blend, BlendParseError> Blend::Open(std::string_view path)
{
    auto stream = CreateStream(path);

    if (!stream)
    {
        return MakeError(stream.error());
    }

    const auto header = ReadHeader(*stream);

    if (!header)
    {
        return MakeError(header.error());
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
        return MakeError(file.error());
    }

    if (!stream->IsAtEnd())
    {
        return MakeError(BlendParseError::FileNotExhausted);
    }

    auto sdna = ReadSdna(*file);

    if (!sdna)
    {
        return MakeError(sdna.error());
    }

    auto type_database = CreateTypeDatabase(*file, *sdna);

    if (!type_database)
    {
        return MakeError(type_database.error());
    }

    return Blend(*file, *type_database);
}

[[nodiscard]] Endian Blend::GetEndian() const
{
    return m_File.header.endian;
}

[[nodiscard]] Pointer Blend::GetPointer() const
{
    return m_File.header.pointer;
}

[[nodiscard]] usize Blend::GetBlockCount() const
{
    return m_File.blocks.size();
}

[[nodiscard]] usize Blend::GetBlockCount(const BlockCode& code) const
{
    return std::ranges::count_if(m_File.blocks, BlockFilter(code));
}

[[nodiscard]] Option<const Block&> Blend::GetBlock(const BlockCode& code) const
{
    if (auto blocks = GetBlocks(code); blocks.begin() != blocks.end())
    {
        return blocks.front();
    }
    return NULL_OPTION;
}

Option<const Type&> cblend::Blend::GetBlockType(const Block& block) const
{
    if (const auto& type_index = m_TypeDatabase.struct_map.find(block.header.struct_index);
        type_index != m_TypeDatabase.struct_map.end() && type_index->second < m_TypeDatabase.type_list.size() && type_index->second > 0)
    {
        const Type& result = m_TypeDatabase.type_list[type_index->second];
        return result;
    }
    return NULL_OPTION;
}

Blend::Blend(File& file, TypeDatabase& type_database) : m_File(std::move(file)), m_TypeDatabase(std::move(type_database)) {}
