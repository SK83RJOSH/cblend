#include <cblend_reflection.hpp>

using namespace cblend;

Type::Type(CanonicalType type) : m_CanonicalType(type) {}

CanonicalType Type::GetCanonicalType() const
{
    return m_CanonicalType;
}

bool Type::IsAggregateType() const
{
    return m_CanonicalType == CanonicalType::Aggregate;
}

bool Type::IsArrayType() const
{
    return m_CanonicalType == CanonicalType::Pointer;
}

bool Type::IsFunctionType() const
{
    return m_CanonicalType == CanonicalType::Function;
}

bool Type::IsPointerType() const
{
    return m_CanonicalType == CanonicalType::Pointer;
}

AggregateType::AggregateType(usize size, std::string_view name, std::vector<const Type*>&& fields)
    : Type(CanonicalType::Aggregate)
    , m_Size(size)
    , m_Name(name)
    , m_Fields(fields)
{
}

usize AggregateType::GetSize() const
{
    return m_Size;
}

std::string_view AggregateType::GetName() const
{
    return m_Name;
}

std::span<const Type* const> AggregateType::GetFields() const
{
    return m_Fields;
}

const Type* AggregateType::GetField(usize field_index) const
{
    if (field_index >= m_Fields.size())
    {
        return nullptr;
    }
    return m_Fields[field_index];
}

ArrayType::ArrayType(usize size, const Type* element_type) : Type(CanonicalType::Array), m_Size(size), m_ElementType(element_type) {}

usize ArrayType::GetSize() const
{
    return m_Size;
}

const Type* ArrayType::GetElementType() const
{
    return m_ElementType;
}

FunctionType::FunctionType(usize size, std::string_view name) : Type(CanonicalType::Function), m_Size(size), m_Name(name) {}

std::string_view FunctionType::GetName() const
{
    return m_Name;
}

FundamentalType::FundamentalType(usize size, std::string_view name) : Type(CanonicalType::Fundamental), m_Size(size), m_Name(name) {}

usize FundamentalType::GetSize() const
{
    return m_Size;
}

std::string_view FundamentalType::GetName() const
{
    return m_Name;
}

PointerType::PointerType(usize size, const Type* pointee_type) : Type(CanonicalType::Pointer), m_Size(size), m_PointeeType(pointee_type) {}

usize PointerType::GetSize() const
{
    return m_Size;
}

const Type* PointerType::GetPointeeType() const
{
    return m_PointeeType;
}
