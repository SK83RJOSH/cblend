#pragma once

#include <cblend_types.hpp>

#include <span>
#include <string_view>
#include <vector>

namespace cblend
{
enum class CanonicalType
{
    Aggregate,
    Array,
    Function,
    Fundamental,
    Pointer,
};

class Type
{
public:
    explicit Type(CanonicalType canonical_type);

    Type(const Type&) = delete;
    Type(Type&&) = delete;
    Type& operator=(const Type&) = delete;
    Type& operator=(Type&&) = delete;
    ~Type() = default;

    [[nodiscard]] CanonicalType GetCanonicalType() const;
    [[nodiscard]] bool IsAggregateType() const;
    [[nodiscard]] bool IsArrayType() const;
    [[nodiscard]] bool IsFunctionType() const;
    [[nodiscard]] bool IsFundamentalType() const;
    [[nodiscard]] bool IsPointerType() const;

private:
    const CanonicalType m_CanonicalType;
};

class AggregateType : public Type
{
public:
    AggregateType(usize size, std::string_view name, std::vector<const Type*>&& fields);

    [[nodiscard]] usize GetSize() const;
    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] std::span<const Type* const> GetFields() const;
    [[nodiscard]] const Type* GetField(usize field_index) const;

private:
    const usize m_Size;
    const std::string_view m_Name;
    const std::vector<const Type*> m_Fields;
};

class ArrayType : public Type
{
public:
    ArrayType(usize size, const Type* element_type);

    [[nodiscard]] usize GetSize() const;
    [[nodiscard]] const Type* GetElementType() const;

private:
    const usize m_Size;
    const Type* const m_ElementType;
};

class FunctionType : public Type
{
public:
    FunctionType(usize size, std::string_view name);

    [[nodiscard]] usize GetSize() const;
    [[nodiscard]] std::string_view GetName() const;

private:
    const usize m_Size;
    const std::string_view m_Name;
};

class FundamentalType : public Type
{
public:
    FundamentalType(usize size, std::string_view name);

    [[nodiscard]] usize GetSize() const;
    [[nodiscard]] std::string_view GetName() const;

private:
    const usize m_Size;
    const std::string_view m_Name;
};

class PointerType : public Type
{
public:
    PointerType(const usize size, const Type* pointee_type);

    [[nodiscard]] usize GetSize() const;
    [[nodiscard]] const Type* GetPointeeType() const;

private:
    const usize m_Size;
    const Type* const m_PointeeType;
};
} // namespace cblend
