#pragma once
#include <cstddef>
#include <string>

namespace oxide {
namespace catalog {
constexpr size_t uuidLength = 36;
bool validUuid(const std::string& uuid);
void check();
void index(const std::string& input, const std::string& requestedUuid);
void remove(const std::string& input, const std::string& requestedUuid);
}
}
