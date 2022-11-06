#pragma once

#include <cblend_types.hpp>
#include <range/v3/algorithm/reverse.hpp>

#include <algorithm>
#include <bit>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace cblend
{
class Stream
{
public:
    enum class StreamPosition : u8
    {
        Begin,
        End
    };
    using SeekValue = std::variant<StreamPosition, usize, ssize>;

    Stream() = default;
    Stream(const Stream&) = delete;
    Stream(Stream&&) = default;
    Stream& operator=(const Stream&) = delete;
    Stream& operator=(Stream&&) = default;
    virtual ~Stream() = default;

    [[nodiscard]] bool IsAtEnd() const;
    [[nodiscard]] bool CanRead(usize size) const;

    [[nodiscard]] usize GetSize() const;
    [[nodiscard]] usize GetPosition() const;

    [[nodiscard]] std::endian GetEndian() const;
    void SetEndian(std::endian endian);

    [[nodiscard]] bool Seek(SeekValue value);
    [[nodiscard]] bool Skip(usize number);
    [[nodiscard]] bool Align(usize alignment);

    [[nodiscard]] bool Read(std::span<std::byte> value);
    [[nodiscard]] bool Read(std::string& value);

    template<class T>
    [[nodiscard]] bool Read(T& value);

    template<class T>
    [[nodiscard]] Option<T> Read();

    template<class T, class... Args>
    bool Read(SeekValue position, Args&&... args);

    template<class T>
    [[nodiscard]] Option<T> Read(SeekValue position);

protected:
    usize m_Size = 0;
    usize m_Position = 0;
    std::endian m_Endian = std::endian::native;

    virtual bool Read(std::byte* value, usize length) = 0;
    virtual bool SeekPosition(StreamPosition position) = 0;
    virtual bool SeekAbsolute(usize position) = 0;
    virtual bool SeekRelative(ssize position) = 0;
};

enum class FileStreamError : u8
{
    FileNotFound,
    DirectorySpecified,
    AccessDenied,
};

class FileStream final : public Stream
{
public:
    explicit FileStream(std::ifstream& stream);
    FileStream(const FileStream&) = delete;
    FileStream(FileStream&&) = default;
    FileStream& operator=(const FileStream&) = delete;
    FileStream& operator=(FileStream&&) = default;
    ~FileStream() final = default;

    static Result<FileStream, FileStreamError> Create(std::string_view path);

    using Stream::Read;

private:
    std::ifstream m_Stream = {};

    bool Read(std::byte* value, usize length) final;
    bool SeekPosition(StreamPosition position) final;
    bool SeekAbsolute(usize position) final;
    bool SeekRelative(ssize position) final;
};

class MemoryStream final : public Stream
{
public:
    explicit MemoryStream(std::span<const u8> span);
    MemoryStream(const MemoryStream&) = delete;
    MemoryStream(MemoryStream&&) = default;
    MemoryStream& operator=(const MemoryStream&) = delete;
    MemoryStream& operator=(MemoryStream&&) = default;
    ~MemoryStream() final = default;

    using Stream::Read;
    [[nodiscard]] bool Read(std::string_view& value);

private:
    std::span<const u8> m_Span;

    bool Read(std::byte* value, usize length) final;
    bool SeekPosition(StreamPosition position) final;
    bool SeekAbsolute(usize position) final;
    bool SeekRelative(ssize position) final;
};

template<std::integral T>
constexpr static T ByteSwap(T& value) noexcept
{
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding");
    auto swapped = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    ranges::reverse(swapped);
    return std::bit_cast<T>(swapped);
}

template<class T>
inline bool Stream::Read(T& value)
{
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding");
    const bool result = Read(std::as_writable_bytes(std::span{ std::addressof(value), 1 }));
    if constexpr (std::is_integral_v<T> && sizeof(T) > 1)
    {
        if (result && m_Endian != std::endian::native)
        {
            value = ByteSwap(value);
        }
    }
    return result;
}

template<class T>
inline Option<T> Stream::Read()
{
    T value = {};
    if (Read(value))
    {
        return value;
    }
    return NULL_OPTION;
}

template<class T, class... Args>
inline bool Stream::Read(SeekValue position, Args&&... args)
{
    const usize current_position = m_Position;
    if (!Seek(position))
    {
        return false;
    }
    const bool result = Read(std::forward<Args>(args)...);
    if (!Seek(current_position))
    {
        return false;
    }
    return result;
}

template<class T>
inline Option<T> Stream::Read(SeekValue position)
{
    T value = {};
    if (Read(position, value))
    {
        return value;
    }
    return NULL_OPTION;
}
} // namespace cblend
