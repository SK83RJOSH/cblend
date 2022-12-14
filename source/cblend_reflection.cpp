#include <cblend_reflection.hpp>

using namespace cblend;

bool Type::IsAggregateType() const
{
    return GetCanonicalType() == CanonicalType::Aggregate;
}

bool Type::IsArrayType() const
{
    return GetCanonicalType() == CanonicalType::Array;
}

bool Type::IsFunctionType() const
{
    return GetCanonicalType() == CanonicalType::Function;
}

bool Type::IsFundamentalType() const
{
    return GetCanonicalType() == CanonicalType::Fundamental;
}

bool Type::IsPointerType() const
{
    return GetCanonicalType() == CanonicalType::Pointer;
}

TypeContainer::TypeContainer(std::unique_ptr<Type> contained) : m_Pointer(contained.get()), m_Contained(std::move(contained)) {}

const Type* const* TypeContainer::operator&() const
{
    return &m_Pointer;
}

TypeContainer::operator const Type&() const
{
    return *m_Pointer;
}

AggregateType::AggregateType(usize size, std::string_view name, std::vector<AggregateType::Field>& fields)
    : m_Size(size)
    , m_Name(name)
    , m_Fields(fields)
{
}

std::string_view AggregateType::GetName() const
{
    return m_Name;
}

usize AggregateType::GetSize() const
{
    return m_Size;
}

std::span<const AggregateType::Field> AggregateType::GetFields() const
{
    return m_Fields;
}

Option<usize> AggregateType::GetFieldOffset(usize field_index) const
{
    if (field_index >= m_Fields.size())
    {
        return NULL_OPTION;
    }
    return m_Fields[field_index].offset;
}

Option<const Type&> AggregateType::GetFieldType(usize field_index) const
{
    if (field_index >= m_Fields.size())
    {
        return NULL_OPTION;
    }
    return **m_Fields[field_index].type;
}

CanonicalType AggregateType::GetCanonicalType() const
{
    return CanonicalType::Aggregate;
}

ArrayType::ArrayType(usize element_count, const Type* const* element_type) : m_ElementCount(element_count), m_ElementType(element_type) {}

usize ArrayType::GetElementCount() const
{
    return m_ElementCount;
}

const Type& ArrayType::GetElementType() const
{
    return **m_ElementType;
}

usize ArrayType::GetSize() const
{
    return GetElementType().GetSize() * m_ElementCount;
}

CanonicalType ArrayType::GetCanonicalType() const
{
    return CanonicalType::Array;
}

FunctionType::FunctionType(std::string_view name) : m_Name(name) {}

std::string_view FunctionType::GetName() const
{
    return m_Name;
}

usize FunctionType::GetSize() const
{
    return 0;
}

CanonicalType FunctionType::GetCanonicalType() const
{
    return CanonicalType::Function;
}

FundamentalType::FundamentalType(std::string_view name, usize size) : m_Name(name), m_Size(size) {}

std::string_view FundamentalType::GetName() const
{
    return m_Name;
}

usize FundamentalType::GetSize() const
{
    return m_Size;
}

CanonicalType FundamentalType::GetCanonicalType() const
{
    return CanonicalType::Fundamental;
}

PointerType::PointerType(const Type* const* pointee_type, usize size) : m_PointeeType(pointee_type), m_Size(size) {}

const Type& PointerType::GetPointeeType() const
{
    return **m_PointeeType;
}

usize PointerType::GetSize() const
{
    return m_Size;
}

CanonicalType PointerType::GetCanonicalType() const
{
    return CanonicalType::Pointer;
}
