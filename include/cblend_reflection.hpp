#pragma once

#include <cblend_types.hpp>

#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace cblend
{
enum class CanonicalType : u8
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
    Type() = default;
    Type(const Type&) = delete;
    Type(Type&&) = delete;
    Type& operator=(const Type&) = delete;
    Type& operator=(Type&&) = delete;
    virtual ~Type() = default;

    [[nodiscard]] virtual usize GetSize() const = 0;
    [[nodiscard]] bool IsAggregateType() const;
    [[nodiscard]] bool IsArrayType() const;
    [[nodiscard]] bool IsFunctionType() const;
    [[nodiscard]] bool IsFundamentalType() const;
    [[nodiscard]] bool IsPointerType() const;

protected:
    [[nodiscard]] virtual CanonicalType GetCanonicalType() const = 0;
};

class TypeContainer final
{
public:
    TypeContainer() = default;
    explicit TypeContainer(std::unique_ptr<Type> contained);
    TypeContainer(const TypeContainer&) = delete;
    TypeContainer(TypeContainer&& other) noexcept = default;
    TypeContainer& operator=(const TypeContainer&) = delete;
    TypeContainer& operator=(TypeContainer&& other) noexcept = default;
    ~TypeContainer() = default;

    const Type* const* operator&() const;
    explicit operator const Type&() const;

private:
    const Type* m_Pointer = nullptr;
    std::unique_ptr<const Type> m_Contained = nullptr;
};

template<class T, class... Args>
requires std::is_base_of_v<Type, T>
[[nodiscard]] inline TypeContainer MakeContainer(Args&&... args)
{
    return TypeContainer(std::make_unique<T>(std::forward<Args>(args)...));
}

class AggregateType final : public Type
{
public:
    struct Field
    {
        const usize offset = 0;
        const std::string_view name = {};
        const Type* const* type = nullptr;
    };

    AggregateType(usize size, std::string_view name, std::vector<AggregateType::Field>& fields);
    AggregateType(const AggregateType&) = delete;
    AggregateType(AggregateType&& other) noexcept = delete;
    AggregateType& operator=(const AggregateType&) = delete;
    AggregateType& operator=(AggregateType&& other) noexcept = delete;
    ~AggregateType() final = default;

    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] std::span<const Field> GetFields() const;
    [[nodiscard]] Option<usize> GetFieldOffset(usize field_index) const;
    [[nodiscard]] Option<const Type&> GetFieldType(usize field_index) const;
    [[nodiscard]] usize GetSize() const final;

private:
    const usize m_Size;
    const std::string_view m_Name;
    const std::vector<Field> m_Fields;

    [[nodiscard]] CanonicalType GetCanonicalType() const final;
};

class ArrayType final : public Type
{
public:
    ArrayType(usize element_count, const Type* const* element_type);
    ArrayType(const ArrayType&) = delete;
    ArrayType(ArrayType&& other) noexcept = delete;
    ArrayType& operator=(const ArrayType&) = delete;
    ArrayType& operator=(ArrayType&& other) noexcept = delete;
    ~ArrayType() final = default;

    [[nodiscard]] usize GetElementCount() const;
    [[nodiscard]] const Type& GetElementType() const;
    [[nodiscard]] usize GetSize() const final;

private:
    const usize m_ElementCount;
    const Type* const* m_ElementType;

    [[nodiscard]] CanonicalType GetCanonicalType() const final;
};

class FunctionType final : public Type
{
public:
    explicit FunctionType(std::string_view name);
    FunctionType(const FunctionType&) = delete;
    FunctionType(FunctionType&& other) noexcept = delete;
    FunctionType& operator=(const FunctionType&) = delete;
    FunctionType& operator=(FunctionType&& other) noexcept = delete;
    ~FunctionType() final = default;

    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] usize GetSize() const final;

private:
    const std::string_view m_Name;

    [[nodiscard]] CanonicalType GetCanonicalType() const final;
};

class FundamentalType final : public Type
{
public:
    FundamentalType(std::string_view name, usize size);
    FundamentalType(const FundamentalType&) = delete;
    FundamentalType(FundamentalType&& other) noexcept = delete;
    FundamentalType& operator=(const FundamentalType&) = delete;
    FundamentalType& operator=(FundamentalType&& other) noexcept = delete;
    ~FundamentalType() final = default;

    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] usize GetSize() const final;

private:
    const std::string_view m_Name;
    const usize m_Size;

    [[nodiscard]] CanonicalType GetCanonicalType() const final;
};

class PointerType final : public Type
{
public:
    PointerType(const Type* const* pointee_type, usize size);
    PointerType(const PointerType&) = delete;
    PointerType(PointerType&& other) noexcept = delete;
    PointerType& operator=(const PointerType&) = delete;
    PointerType& operator=(PointerType&& other) noexcept = delete;
    ~PointerType() final = default;

    [[nodiscard]] const Type& GetPointeeType() const;
    [[nodiscard]] usize GetSize() const final;

private:
    const Type* const* m_PointeeType;
    const usize m_Size;

    [[nodiscard]] CanonicalType GetCanonicalType() const final;
};
} // namespace cblend
