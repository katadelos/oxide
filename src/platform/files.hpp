#pragma once
#include <cstddef>
#include <string>
#include <sys/types.h>

namespace oxide {
constexpr unsigned privateFileMode = 0600;
constexpr unsigned privateDirectoryMode = 0700;
constexpr unsigned sharedFileMode = 0644;
constexpr unsigned sharedDirectoryMode = 0755;
constexpr size_t oneMiB = size_t{1024} * 1024;

class FileDescriptor {
public:
    explicit FileDescriptor(int descriptor = -1);
    ~FileDescriptor();
    FileDescriptor(FileDescriptor&& other) noexcept;
    FileDescriptor& operator=(FileDescriptor&& other) noexcept;
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    int get() const;
    int release();
    void close();

private:
    int fd_;
};

struct Pipe {
    Pipe();
    FileDescriptor readEnd;
    FileDescriptor writeEnd;
};

class FileLock {
public:
    explicit FileLock(const std::string& path);
    bool acquired() const;

private:
    FileDescriptor file_;
    bool acquired_ = false;
};

FileDescriptor openFile(const std::string& path, int flags, unsigned mode = privateFileMode);
void setNonblocking(int descriptor);
bool exists(const std::string& path);
void ensureDirectory(const std::string& path, unsigned mode = privateDirectoryMode);
std::string readFile(const std::string& path, size_t limit = oneMiB);
std::string executablePath();
void requireRoot();
void requireInstalled();
void log(const std::string& message);

}
