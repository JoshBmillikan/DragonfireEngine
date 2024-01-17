//
// Created by josh on 1/13/24.
//

#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

struct PHYSFS_File;

namespace dragonfire {

class File {
    PHYSFS_File* fp = nullptr;

public:
    enum class Mode { READ, WRITE, APPEND };
    explicit File(const char* path, Mode mode = Mode::READ);

    explicit File(PHYSFS_File* fp) : fp(fp) {}

    [[nodiscard]] size_t length() const;
    size_t read(void* buffer, size_t bufSize) const;

    template<typename T = std::byte>
    size_t read(std::span<T> span) const
    {
        const size_t size = sizeof(T) * span.size();
        return read(span.data(), size);
    }

    [[nodiscard]] std::vector<std::byte> read() const;
    [[nodiscard]] std::string readString() const;

    size_t write(const void* buffer, size_t len);

    template<typename T = std::byte>
    size_t write(const std::span<T> span)
    {
        const size_t size = sizeof(T) * span;
        return write(span.data(), size);
    }

    size_t write(std::string_view txt); // NOLINT(*-use-nodiscard)

    void close();

    ~File() noexcept;

    /***
     * @brief Initialize PhysFs
     * @param argc program argument count
     * @param argv array of command line arguments
     */
    static void init(int argc, char** argv);
    static void init(const char* argv0);
    static void mount(const char* dir, const char* mountPoint, bool append = false);
    static void mount(const std::string& dir, const std::string& mountPoint, bool append = false);
    File(const File& other) = delete;

    File(File&& other) noexcept : fp(other.fp) {}

    File& operator=(const File& other) = delete;

    File& operator=(File&& other) noexcept
    {
        if (this == &other)
            return *this;
        fp = other.fp;
        return *this;
    }
};

struct PhysFsError final : std::runtime_error {
    PhysFsError();
    explicit PhysFsError(int errorCode);
};

}// namespace raven