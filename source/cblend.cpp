#include <cblend.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/sort.hpp>

#include <cctype>
#include <charconv>

using namespace cblend;

MemoryTable::MemoryTable(std::vector<MemoryRange>& ranges) : m_Ranges(std::move(ranges)) {}

std::span<const u8> MemoryTable::GetMemory(u64 address, usize size) const
{
    const u64 head = address;
    const u64 tail = address + size;
    auto range_contains = [head, tail](const MemoryRange& range)
    {
        return range.head <= head && range.tail >= tail;
    };

    // TODO: Figure out how to use partition_point here...
    if (auto result = ranges::find_if(m_Ranges, range_contains); result != m_Ranges.end())
    {
        return std::span{ result->span.data() + (head - result->head), size };
    }

    return {};
}

BlendType::BlendType(const MemoryTable& memory_table, const Type& type) : m_MemoryTable(memory_table), m_Type(type)
{
    if (IsPointer())
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        m_ElementType = &reinterpret_cast<const PointerType&>(type).GetPointeeType();
    }
    else if (IsArray())
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto& array = reinterpret_cast<const ArrayType&>(type);
        m_ElementType = &array.GetElementType();
        m_ArrayRank = array.GetElementCount();
    }
    else if (IsStruct())
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto fields = reinterpret_cast<const AggregateType&>(type).GetFields();
        m_Fields.reserve(fields.size());
        m_FieldsByName.reserve(fields.size());

        for (const auto& field : fields)
        {
            m_Fields.emplace_back(&field);
            m_FieldsByName.emplace(field.name, &field);
        }
    }
}

[[nodiscard]] bool BlendType::IsArray() const
{
    return m_Type.IsArrayType();
}

[[nodiscard]] bool BlendType::IsPointer() const
{
    return m_Type.IsPointerType();
}

[[nodiscard]] bool BlendType::IsPrimitive() const
{
    return m_Type.IsFundamentalType();
}

[[nodiscard]] bool BlendType::IsStruct() const
{
    return m_Type.IsAggregateType();
}

[[nodiscard]] usize BlendType::GetSize() const
{
    return m_Type.GetSize();
}

[[nodiscard]] bool BlendType::HasElementType() const
{
    return m_ElementType != nullptr;
}

[[nodiscard]] Option<BlendType> BlendType::GetElementType() const
{
    if (m_ElementType != nullptr)
    {
        const Type& type = *m_ElementType;
        return BlendType(m_MemoryTable, type);
    }
    return NULL_OPTION;
}

[[nodiscard]] usize BlendType::GetArrayRank() const
{
    return m_ArrayRank;
}

[[nodiscard]] Option<BlendFieldInfo> BlendType::GetField(std::string_view field_name) const
{
    if (const auto& field = m_FieldsByName.find(field_name); field != m_FieldsByName.end())
    {
        return BlendFieldInfo(m_MemoryTable, *field->second, *this);
    }
    return NULL_OPTION;
}

[[nodiscard]] std::vector<BlendFieldInfo> BlendType::GetFields() const
{
    std::vector<BlendFieldInfo> results;
    results.reserve(m_Fields.size());
    for (const auto& field : m_Fields)
    {
        results.emplace_back(m_MemoryTable, *field, *this);
    }
    return results;
}

BlendFieldInfo::BlendFieldInfo(const MemoryTable& memory_table, const AggregateType::Field& field, const BlendType& declaring_type)
    : m_MemoryTable(memory_table)
    , m_Offset(field.offset)
    , m_Name(field.name)
    , m_DeclaringType(declaring_type)
    , m_FieldType(m_MemoryTable, **field.type)
    , m_Size((*field.type)->GetSize())
{
}

[[nodiscard]] std::string_view BlendFieldInfo::GetName() const
{
    return m_Name;
}

[[nodiscard]] const BlendType& BlendFieldInfo::GetDeclaringType() const
{
    return m_DeclaringType;
}

[[nodiscard]] const BlendType& BlendFieldInfo::GetFieldType() const
{
    return m_FieldType;
}

[[nodiscard]] std::span<const u8> BlendFieldInfo::GetData(std::span<const u8> span) const
{
    if (m_Offset + m_Size > span.size())
    {
        return {};
    }

    return std::span{ span.data() + m_Offset, m_Size };
}

[[nodiscard]] std::span<const u8> BlendFieldInfo::GetData(const Block& block) const
{
    return GetData(block.body);
}

std::span<const u8> BlendFieldInfo::GetPointerData(std::span<const u8> span) const
{
    if (!m_FieldType.IsPointer())
    {
        return {};
    }

    if (auto value = GetData(span); value.size() == m_Size)
    {
        if (m_Size == sizeof(u64))
        {
            std::array<u8, sizeof(u64)> result = {};
            std::copy(value.begin(), value.end(), result.data());
            return m_MemoryTable.GetMemory(std::bit_cast<u64>(result), m_FieldType.GetElementType()->GetSize());
        }

        if (m_Size == sizeof(u32))
        {
            std::array<u8, sizeof(u32)> result = {};
            std::copy(value.begin(), value.end(), result.data());
            return m_MemoryTable.GetMemory(std::bit_cast<u32>(result), m_FieldType.GetElementType()->GetSize());
        }
    }

    return {};
}

std::span<const u8> BlendFieldInfo::GetPointerData(const Block& block) const
{
    return GetPointerData(block.body);
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
    return ranges::all_of(name, [](const char chr) { return std::isalpha(chr) != 0 || std::isdigit(chr) != 0 || chr == '_'; });
}

Result<Option<AggregateType::Field>, ReflectionError>
ProcessFunctionPointerField(usize field_offset, std::string_view field_name, usize pointer_size, TypeDatabase::TypeList& types)
{
    if (!field_name.starts_with('('))
    {
        return NULL_OPTION;
    }

    static constexpr usize MIN_FUNCTION_POINTER_LENGTH = 5;

    if (field_name.size() <= MIN_FUNCTION_POINTER_LENGTH)
    {
        return MakeError(ReflectionError::InvalidSdnaFieldName);
    }

    const std::string_view name(field_name.begin() + 2, field_name.end() - 3);

    if (!IsValidName(name))
    {
        return MakeError(ReflectionError::InvalidSdnaFieldName);
    }

    types.push_back(MakeContainer<FunctionType>(name));
    types.push_back(MakeContainer<PointerType>(&types.back(), pointer_size));

    return AggregateType::Field{ .offset = field_offset, .name = name, .type = &types.back() };
}

usize CountPointers(std::string_view field_name)
{
    // Scan until we reach the beginning of the field name
    usize index = 0;
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

const Type* const* AddPointers(usize pointer_count, const Type* const* type, usize pointer_size, TypeDatabase::TypeList& types)
{
    while (pointer_count > 0)
    {
        types.push_back(MakeContainer<PointerType>(type, pointer_size));
        type = &types.back();
        --pointer_count;
    }
    return type;
}

Result<Option<AggregateType::Field>, ReflectionError> ProcessField(
    usize field_offset,
    std::string_view field_name,
    usize pointer_size,
    const Type* const* field_type,
    TypeDatabase::TypeList& types
)
{
    const usize pointer_count = CountPointers(field_name);
    const usize name_length = CalculateNameLength(field_name, pointer_count);
    const std::string_view name = std::string_view(&field_name[pointer_count], name_length);
    const Type* const* type = field_type;

    if (!IsValidName(name))
    {
        return MakeError(ReflectionError::InvalidSdnaFieldName);
    }

    usize name_end = pointer_count + name_length;

    // Handle arrays such as: field_name[0][1]
    while (name_end + 1 < field_name.size())
    {
        auto array_begin = name_end + 1;

        // First character must be '['
        if (field_name[name_end] != '[')
        {
            return MakeError(ReflectionError::InvalidSdnaFieldName);
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
                    return MakeError(ReflectionError::InvalidSdnaFieldName);
                }
            }
            // We encountered a non-digit
            else if (std::isdigit(field_name[name_end]) == 0)
            {
                return MakeError(ReflectionError::InvalidSdnaFieldName);
            }
        }

        auto array_end = name_end;

        // Last character must be ']'
        if (field_name[name_end] != ']')
        {
            return MakeError(ReflectionError::InvalidSdnaFieldName);
        }

        usize count = 0;
        auto result = std::from_chars(field_name.data() + array_begin, field_name.data() + array_end, count);

        if (result.ec != std::errc())
        {
            return MakeError(ReflectionError::InvalidSdnaFieldName);
        }

        types.push_back(MakeContainer<ArrayType>(count, type));
        type = &types.back();
    }

    return AggregateType::Field{ .offset = field_offset, .name = name, .type = AddPointers(pointer_count, type, pointer_size, types) };
}

Result<TypeDatabase, ReflectionError> CreateTypeDatabase(const File& file, const Sdna& sdna)
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
            return MakeError(ReflectionError::InvalidSdnaStruct);
        }

        type_database.struct_map.emplace(struct_index++, type_index);

        const usize field_count = fields.size();
        usize field_offset = 0;
        std::vector<AggregateType::Field> aggregate_fields;
        aggregate_fields.reserve(field_count);

        for (const auto& [field_type_index, field_name_index] : fields)
        {
            if (field_name_index >= field_name_count || field_type_index >= type_count)
            {
                return MakeError(ReflectionError::InvalidSdnaField);
            }

            const auto& field_name = sdna.field_names[field_name_index];

            // Handle function pointers such as: (*field_name)()
            if (const auto field = ProcessFunctionPointerField(field_offset, field_name, pointer_size, type_database.type_list);
                *field != NULL_OPTION)
            {
                field_offset += (*(*field)->type)->GetSize();
                aggregate_fields.push_back(**field);
                continue;
            }
            // NOLINTNEXTLINE(readability-else-after-return)
            else if (!field.has_value())
            {
                return MakeError(field.error());
            }

            const auto& field_type = type_database.type_list[field_type_index];

            // Handle generic fields such as: **field_name[1][2]
            if (const auto field = ProcessField(field_offset, field_name, pointer_size, &field_type, type_database.type_list);
                *field != NULL_OPTION)
            {
                field_offset += (*(*field)->type)->GetSize();
                aggregate_fields.push_back(**field);
                continue;
            }
            // NOLINTNEXTLINE(readability-else-after-return)
            else if (!field.has_value())
            {
                return MakeError(field.error());
            }

            return MakeError(ReflectionError::InvalidSdnaField);
        }

        const auto size = sdna.type_lengths[type_index];
        const auto& name = sdna.type_names[type_index];
        type_database.type_list[type_index] = MakeContainer<AggregateType>(size, name, aggregate_fields);
    }

    return type_database;
}

MemoryTable CreateMemoryTable(const File& file)
{
    std::vector<MemoryRange> ranges;
    ranges.reserve(file.blocks.size());

    for (const auto& [header, body] : file.blocks)
    {
        if (header.length == 0)
        {
            continue;
        }
        ranges.emplace_back(MemoryRange{ header.address, header.address + header.length, std::span{ body } });
    }

    ranges::sort(ranges, [](const MemoryRange& first, const MemoryRange& second) { return first.head < second.head; });

    return MemoryTable(ranges);
}

Result<const Blend, BlendError> Blend::Open(std::string_view path)
{
    auto stream = FileStream::Create(path);

    if (!stream)
    {
        return MakeError(BlendError(stream.error()));
    }

    const auto header = ReadHeader(*stream);

    if (!header)
    {
        return MakeError(BlendError(header.error()));
    }

    if (header->endian == Endian::Little)
    {
        stream->SetEndian(std::endian::little);
    }
    else
    {
        stream->SetEndian(std::endian::big);
    }

    auto file = ReadFile(*stream, *header);

    if (!file)
    {
        return MakeError(BlendError(file.error()));
    }

    if (!stream->IsAtEnd())
    {
        return MakeError(BlendError(FormatError::FileNotExhausted));
    }

    auto sdna = ReadSdna(*file);

    if (!sdna)
    {
        return MakeError(BlendError(sdna.error()));
    }

    auto type_database = CreateTypeDatabase(*file, *sdna);

    if (!type_database)
    {
        return MakeError(BlendError(type_database.error()));
    }

    auto memory_table = CreateMemoryTable(*file);

    return Blend(*file, *type_database, memory_table);
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
    return ranges::count_if(m_File.blocks, BlockFilter(code));
}

[[nodiscard]] Option<const Block&> Blend::GetBlock(const BlockCode& code) const
{
    if (auto blocks = GetBlocks(code); blocks.begin() != blocks.end())
    {
        return blocks.front();
    }
    return NULL_OPTION;
}

Option<BlendType> cblend::Blend::GetBlockType(const Block& block) const
{
    if (const auto& type_index = m_TypeDatabase.struct_map.find(block.header.struct_index);
        type_index != m_TypeDatabase.struct_map.end() && type_index->second < m_TypeDatabase.type_list.size() && type_index->second > 0)
    {
        const Type& result(m_TypeDatabase.type_list[type_index->second]);
        return BlendType(m_MemoryTable, result);
    }
    return NULL_OPTION;
}

Blend::Blend(File& file, TypeDatabase& type_database, MemoryTable& memory_table)
    : m_File(std::move(file))
    , m_TypeDatabase(std::move(type_database))
    , m_MemoryTable(memory_table)
{
}
