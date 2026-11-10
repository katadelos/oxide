#include "scriptlets/metadata.hpp"

#include "paths.hpp"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <limits.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

namespace oxide {
namespace {
constexpr unsigned headerLineLimit = 128;
constexpr size_t headerByteLimit = size_t{512} * 1024;

std::string trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

void readHeaderField(const std::string& line, Metadata& result) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        return;
    }
    const auto field = line.substr(0, colon);
    const auto value = trim(line.substr(colon + 1));
    if (value.empty()) {
        return;
    }
    if (field == "# Name") {
        result.name = value;
    } else if (field == "# Author") {
        result.author = value;
    } else if (field == "# Icon") {
        result.icon = value;
    }
}
}

std::string requireScriptLocation(const std::string& path) {
    const std::string root = documents;
    const bool inDocuments = path.compare(0, root.size(), root) == 0;
    const bool hasFilename = path.size() >= root.size() + 4;
    if (!inDocuments || !hasFilename) {
        throw std::runtime_error("Expected a .sh file under /mnt/us/documents");
    }
    const bool shellExtension = strcasecmp(path.c_str() + path.size() - 3, ".sh") == 0;
    const bool containsNul = path.find('\0') != std::string::npos;
    if (!shellExtension || containsNul) {
        throw std::runtime_error("Invalid script filename: " + path);
    }
    size_t start = root.size();
    while (start < path.size()) {
        size_t end = path.find('/', start);
        std::string part = path.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") {
            throw std::runtime_error("Noncanonical script path");
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return path;
}

std::string requireScriptFile(const std::string& path) {
    requireScriptLocation(path);
    char resolved[PATH_MAX];
    if (realpath(path.c_str(), resolved) == nullptr) {
        throw std::runtime_error("Cannot resolve script: " + path);
    }
    if (path != resolved) {
        throw std::runtime_error("Script path traverses a symlink: " + path);
    }
    struct stat status;
    if (lstat(path.c_str(), &status) != 0 || !S_ISREG(status.st_mode)) {
        throw std::runtime_error("Script must be a regular file: " + path);
    }
    return path;
}

Metadata metadata(const std::string& path) {
    Metadata result;
    result.name = path.substr(path.find_last_of('/') + 1);
    result.author = "Unknown";
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Cannot read script metadata");
    }
    std::string line;
    size_t bytes = 0;
    for (unsigned lines = 0; lines < headerLineLimit; ++lines) {
        line.clear();
        char character;
        while (stream.get(character) && character != '\n') {
            if (++bytes > headerByteLimit) {
                throw std::runtime_error("Script header exceeds 512 KiB");
            }
            line += character;
        }
        if (line.empty() && !stream) {
            break;
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (line[0] != '#') {
            break;
        }
        if (line == "# DontUseFBInk") {
            result.fbink = false;
            continue;
        }
        readHeaderField(line, result);
    }
    return result;
}
}
