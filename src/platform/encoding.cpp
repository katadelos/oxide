#include "platform/encoding.hpp"

#include <glib.h>
#include <memory>
#include <stdexcept>

namespace oxide {
std::string unescapeUri(const std::string& encoded) {
    std::unique_ptr<char, decltype(&g_free)> decoded(
        g_uri_unescape_string(encoded.c_str(), nullptr), g_free);
    if (!decoded) {
        throw std::runtime_error("Invalid URL escape");
    }
    return decoded.get();
}
}
