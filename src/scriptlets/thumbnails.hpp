#pragma once
#include <string>

namespace oxide {
struct Metadata;

namespace thumbnails {
std::string create(const Metadata& metadata, const std::string& path, const std::string& uuid);
void remove(const std::string& path);
}
}
