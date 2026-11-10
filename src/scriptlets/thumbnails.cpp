#include "scriptlets/thumbnails.hpp"

#include "platform/files.hpp"
#include "platform/firmware.hpp"
#include "scriptlets/catalog.hpp"
#include "scriptlets/metadata.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace oxide {
namespace {
constexpr int thumbnailWidth = 221;
constexpr int thumbnailHeight = 335;
constexpr size_t iconSizeLimit = 4U * oneMiB;
constexpr size_t thumbnailRandomLength = 6;
const std::string jpegExtension = ".jpg";

}

std::string thumbnails::create(
    const Metadata& meta,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const std::string& path,
    const std::string& uuid) {
    if (meta.icon.empty()) {
        return "";
    }
    const auto image =
        meta.icon[0] == '/' ? meta.icon : path.substr(0, path.find_last_of('/') + 1) + meta.icon;
    const auto bytes = readFile(image, iconSizeLimit);
    if (bytes.empty()) {
        throw std::runtime_error("Empty icon");
    }
    ensureDirectory("/mnt/us/system/thumbnails", sharedDirectoryMode);
    std::string temp = "/mnt/us/system/thumbnails/oxide-" + uuid + "-XXXXXX";
    // NOLINTNEXTLINE(readability-container-data-pointer)
    FileDescriptor file(mkstemp(&temp[0]));
    if (file.get() < 0) {
        throw std::runtime_error("Cannot create icon file");
    }
    const auto output = temp + ".jpg";
    try {
        size_t written = 0;
        while (written < bytes.size()) {
            const ssize_t count = write(file.get(), bytes.data() + written, bytes.size() - written);
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                throw std::runtime_error("Cannot write icon file");
            }
            written += count;
        }
        if (::close(file.release()) < 0) {
            throw std::runtime_error("Cannot close icon file");
        }
        std::unique_ptr<char, decltype(&free)> resized(
            extractor_resize_thumbnail(temp.c_str(), thumbnailWidth, thumbnailHeight), free);
        if (!resized || output != resized.get()) {
            throw std::runtime_error("Firmware could not resize icon");
        }
        struct stat status;
        if (stat(output.c_str(), &status) != 0) {
            throw std::runtime_error("Firmware did not create a thumbnail");
        }
        if (!S_ISREG(status.st_mode) || status.st_size == 0) {
            throw std::runtime_error("Firmware created an invalid thumbnail");
        }
        if (chmod(output.c_str(), sharedFileMode) != 0) {
            throw std::runtime_error("Cannot set thumbnail permissions");
        }
        unlink(temp.c_str());
        return output;
    } catch (...) {
        unlink(temp.c_str());
        unlink(output.c_str());
        throw;
    }
}

void thumbnails::remove(const std::string& path) {
    const std::string prefix = "/mnt/us/system/thumbnails/oxide-";
    const bool ownedPrefix = path.compare(0, prefix.size(), prefix) == 0;
    const size_t randomStart = catalog::uuidLength + 1;
    const size_t extensionStart = randomStart + thumbnailRandomLength;
    const bool expectedLength =
        path.size() == prefix.size() + extensionStart + jpegExtension.size();
    if (!ownedPrefix || !expectedLength) {
        return;
    }
    const auto filename = path.substr(prefix.size());
    if (!catalog::validUuid(filename.substr(0, catalog::uuidLength))) {
        return;
    }
    if (filename[catalog::uuidLength] != '-' || filename.substr(extensionStart) != jpegExtension) {
        return;
    }
    for (size_t i = randomStart; i < extensionStart; ++i) {
        if (std::isalnum(static_cast<unsigned char>(filename[i])) == 0) {
            return;
        }
    }
    unlink(path.c_str());
}

}
