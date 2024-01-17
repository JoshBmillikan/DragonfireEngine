//
// Created by josh on 1/13/24.
//

#include "file.h"
#include <cassert>
#include <physfs.h>
#include <spdlog/spdlog.h>

namespace dragonfire {
File::File(const char* path, const Mode mode)
{
    switch (mode) {
        case Mode::READ: fp = PHYSFS_openRead(path); break;
        case Mode::WRITE: fp = PHYSFS_openWrite(path); break;
        case Mode::APPEND: fp = PHYSFS_openAppend(path); break;
    }
    if (fp == nullptr)
        throw PhysFsError();
}

size_t File::length() const
{
    const int64_t len = PHYSFS_fileLength(fp);
    if (len < 0)
        throw PhysFsError();
    return len;
}

size_t File::read(void* buffer, const size_t bufSize) const
{
    const int64_t read = PHYSFS_readBytes(fp, buffer, bufSize);
    if (read < 0)
        throw PhysFsError();
    return read;
}

std::vector<std::byte> File::read() const
{
    std::vector<std::byte> out;
    out.resize(length());
    const size_t read = File::read(std::span(out));
    assert(read == out.size());
    return out;
}

std::string File::readString() const
{
    std::string str;
    const auto len = length();
    str.resize(len + 1);
    read(str.data(), len);
    return str;
}

// ReSharper disable once CppMemberFunctionMayBeConst
size_t File::write(const void* buffer, const size_t len)
{
    const int64_t written = PHYSFS_writeBytes(fp, buffer, len);
    if (written < 0)
        throw PhysFsError();
    return written;
}

size_t File::write(const std::string_view txt)
{
    return write(txt.data(), txt.size() * sizeof(char));
}

void File::close()
{
    if (fp) {
        if (PHYSFS_close(fp) == 0)
            throw PhysFsError();
        fp = nullptr;
    }
}

File::~File() noexcept
{
    if (fp) {
        if (PHYSFS_close(fp) == 0)
            SPDLOG_ERROR("Failed to close file: {}", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }
}

void File::init(const int argc, char** argv)
{
    init(argc > 0 ? *argv : nullptr);
}

void File::init(const char* argv0)
{
    int err = PHYSFS_init(argv0);
    if (err)
        PHYSFS_setSaneConfig("org", APP_NAME, "zip", false, false);
#ifdef DATA_DIRECTORY
    if (err)
        err = PHYSFS_mount(DATA_DIRECTORY, nullptr, 0);
#endif

    if (err == 0)
        throw PhysFsError();
}

void File::mount(const char* dir, const char* mountPoint, const bool append)
{
    if (PHYSFS_mount(dir, mountPoint, append) == 0)
        throw PhysFsError();
}

void File::mount(const std::string& dir, const std::string& mountPoint, const bool append)
{
    mount(dir.c_str(), mountPoint.c_str(), append);
}

PhysFsError::PhysFsError() : PhysFsError(PHYSFS_getLastErrorCode()) {}

PhysFsError::PhysFsError(int errorCode)
    : runtime_error(PHYSFS_getErrorByCode(static_cast<PHYSFS_ErrorCode>(errorCode)))
{
}
}// namespace raven