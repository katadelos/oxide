#include "platform/files.hpp"

#include "paths.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <limits.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <syslog.h>
#include <system_error>
#include <unistd.h>

namespace oxide {
namespace {
constexpr size_t readBufferSize = 4096;

std::system_error fileError(const std::string& operation) {
    return std::system_error(errno, std::generic_category(), operation);
}
}

FileDescriptor::FileDescriptor(int descriptor) : fd_(descriptor) {}

FileDescriptor::~FileDescriptor() {
    close();
}

FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.release()) {}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.release();
    }
    return *this;
}

int FileDescriptor::get() const {
    return fd_;
}

int FileDescriptor::release() {
    const int descriptor = fd_;
    fd_ = -1;
    return descriptor;
}

void FileDescriptor::close() {
    if (fd_ >= 0) {
        ::close(release());
    }
}

FileDescriptor openFile(const std::string& path, int flags, unsigned mode) {
    const int descriptor = open(path.c_str(), flags | O_CLOEXEC, mode);
    if (descriptor < 0) {
        throw fileError("Cannot open " + path);
    }
    return FileDescriptor(descriptor);
}

Pipe::Pipe() {
    int descriptors[2];
    if (pipe(descriptors) < 0) {
        throw fileError("Cannot create pipe");
    }
    readEnd = FileDescriptor(descriptors[0]);
    writeEnd = FileDescriptor(descriptors[1]);
    if (fcntl(readEnd.get(), F_SETFD, FD_CLOEXEC) < 0 ||
        fcntl(writeEnd.get(), F_SETFD, FD_CLOEXEC) < 0) {
        throw fileError("Cannot set pipe close-on-exec");
    }
}

FileLock::FileLock(const std::string& path) : file_(openFile(path, O_RDWR | O_CREAT)) {
    while (flock(file_.get(), LOCK_EX | LOCK_NB) < 0) {
        if (errno == EINTR) {
            continue;
        }
        if (errno == EWOULDBLOCK) {
            return;
        }
        throw fileError("Cannot lock " + path);
    }
    acquired_ = true;
}

bool FileLock::acquired() const {
    return acquired_;
}

void setNonblocking(int descriptor) {
    const int flags = fcntl(descriptor, F_GETFL);
    if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw fileError("Cannot make pipe nonblocking");
    }
}

bool exists(const std::string& path) {
    if (access(path.c_str(), F_OK) == 0) {
        return true;
    }
    if (errno == ENOENT || errno == ENOTDIR) {
        return false;
    }
    throw fileError("Cannot inspect " + path);
}

void ensureDirectory(const std::string& path, unsigned mode) {
    if (mkdir(path.c_str(), mode) < 0 && errno != EEXIST) {
        throw fileError("Cannot create " + path);
    }
    struct stat status;
    if (lstat(path.c_str(), &status) < 0) {
        throw fileError("Cannot inspect " + path);
    }
    if (!S_ISDIR(status.st_mode)) {
        throw std::runtime_error("Not a directory: " + path);
    }
}

std::string readFile(const std::string& path, size_t limit) {
    auto file = openFile(path, O_RDONLY | O_NOFOLLOW);
    std::string contents;
    char buffer[readBufferSize];
    for (;;) {
        const ssize_t count = read(file.get(), buffer, sizeof buffer);
        if (count == 0) {
            return contents;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw fileError("Cannot read " + path);
        }
        if (static_cast<size_t>(count) > limit - contents.size()) {
            throw std::runtime_error("File exceeds size limit: " + path);
        }
        contents.append(buffer, count);
    }
}

std::string executablePath() {
    char path[PATH_MAX];
    const ssize_t count = readlink("/proc/self/exe", path, sizeof path);
    if (count < 0) {
        throw fileError("Cannot resolve executable");
    }
    if (count == sizeof path) {
        throw std::runtime_error("Executable path is too long");
    }
    return std::string(path, count);
}

void requireRoot() {
    if (geteuid() != 0) {
        throw std::runtime_error("Run this command as root");
    }
}

void requireInstalled() {
    if (executablePath() != installedBinary) {
        throw std::runtime_error(std::string("Run the installed binary at ") + installedBinary);
    }
}

void log(const std::string& message) {
    syslog(LOG_INFO, "%s", message.c_str());
    fprintf(stderr, "oxide: %s\n", message.c_str());
}
}
