#include <cblend_stream.hpp>

#include <cstring>
#include <filesystem>

using namespace cblend;

bool Stream::IsAtEnd() const
{
    return m_Position >= m_Size;
}

bool Stream::CanRead(usize size) const
{
    return !IsAtEnd() && m_Size - m_Position >= size;
}

usize Stream::GetSize() const
{
    return m_Size;
}

usize Stream::GetPosition() const
{
    return m_Position;
}

std::endian Stream::GetEndian() const
{
    return m_Endian;
}

void Stream::SetEndian(std::endian endian)
{
    m_Endian = endian;
}

bool Stream::Seek(SeekValue value)
{
    return std::visit(
        [this](auto& position)
        {
            using T = std::decay_t<decltype(position)>;

            if constexpr (std::is_same_v<T, StreamPosition>)
            {
                return SeekPosition(position);
            }
            else if constexpr (std::is_same_v<T, usize>)
            {
                return SeekAbsolute(position);
            }
            else if constexpr (std::is_same_v<T, ssize>)
            {
                return SeekRelative(position);
            }
        },
        value
    );
}

bool Stream::Skip(usize number)
{
    return Seek(ssize(number));
}

bool Stream::Align(usize alignment)
{
    const usize quotient = m_Position / alignment;
    const auto remainder = static_cast<usize>((m_Position % alignment) != 0);
    const usize position = (quotient + remainder) * alignment;

    if (position >= m_Size)
    {
        return false;
    }

    m_Position = position;
    return true;
}

bool Stream::Read(std::span<std::byte> value)
{
    if (!CanRead(value.size()))
    {
        return false;
    }
    return Read(value.data(), ssize(value.size()));
}

bool Stream::Read(std::string& value)
{
    std::string result = {};

    while (true)
    {
        char character = 0;
        if (!Read(character))
        {
            return false;
        }
        if (character == 0)
        {
            break;
        }
        result.append(1, character);
    }

    value = std::move(result);
    return true;
}

FileStream::FileStream(std::ifstream& stream) : m_Stream(std::move(stream))
{
    SeekPosition(StreamPosition::End);
    m_Size = m_Stream.tellg();
    SeekPosition(StreamPosition::Begin);
}

bool FileStream::Read(std::byte* value, usize length)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    m_Stream.read(reinterpret_cast<char*>(value), ssize(length));
    m_Position = m_Stream.tellg();
    return !m_Stream.bad();
}

bool FileStream::SeekPosition(StreamPosition position)
{
    if (position == StreamPosition::Begin)
    {
        m_Stream.seekg(0, std::ios::beg);
    }
    else if (position == StreamPosition::End)
    {
        m_Stream.seekg(0, std::ios::end);
    }
    m_Position = m_Stream.tellg();
    return !m_Stream.bad();
}

bool FileStream::SeekAbsolute(usize position)
{
    m_Stream.seekg(ssize(position));
    m_Position = m_Stream.tellg();
    return !m_Stream.bad();
}

bool FileStream::SeekRelative(ssize position)
{
    m_Stream.seekg(position, std::ifstream::cur);
    m_Position = m_Stream.tellg();
    return !m_Stream.bad();
}

[[nodiscard]] Result<FileStream, FileStreamError> FileStream::Create(std::string_view path)
{
    namespace fs = std::filesystem;

    auto file_path = fs::path(path);

    if (!fs::exists(file_path))
    {
        return MakeError(FileStreamError::FileNotFound);
    }

    if (!fs::is_regular_file(file_path))
    {
        return MakeError(FileStreamError::DirectorySpecified);
    }

    std::ifstream stream(file_path.c_str(), std::ifstream::binary);

    if (stream.bad())
    {
        return MakeError(FileStreamError::AccessDenied);
    }

    return FileStream(stream);
}

MemoryStream::MemoryStream(std::span<const u8> span) : m_Span(span)
{
    m_Position = 0;
    m_Size = m_Span.size();
}

bool MemoryStream::Read(std::string_view& value)
{
    usize str_len = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const char* str = reinterpret_cast<const char*>(m_Span.data() + m_Position);

    while (true)
    {
        char character = 0;
        if (!Read(character))
        {
            return false;
        }
        if (character == 0)
        {
            break;
        }
        ++str_len;
    }

    value = std::string_view(str, str_len);
    return true;
}

bool MemoryStream::Read(std::byte* value, usize length)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (std::memcpy(reinterpret_cast<void*>(value), reinterpret_cast<const void*>(m_Span.data() + m_Position), length) == nullptr)
    {
        return false;
    }
    m_Position += length;
    return true;
}

bool MemoryStream::SeekPosition(StreamPosition position)
{
    if (position == StreamPosition::Begin)
    {
        m_Position = 0;
    }
    else if (position == StreamPosition::End)
    {
        m_Position = m_Span.size();
    }
    return true;
}

bool MemoryStream::SeekAbsolute(usize position)
{
    if (position >= m_Size)
    {
        return false;
    }
    m_Position = position;
    return true;
}

bool MemoryStream::SeekRelative(ssize position)
{
    if (m_Position + position >= m_Size)
    {
        return false;
    }
    m_Position = m_Position + position;
    return true;
}
