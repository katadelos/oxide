#pragma once
#include <stdexcept>
#include <string>

namespace oxide {
struct Metadata {
    std::string name;
    std::string author;
    std::string icon;
    bool fbink = true;
};

Metadata metadata(const std::string& path);
std::string requireScriptLocation(const std::string& path);
std::string requireScriptFile(const std::string& path);
}
