#include "platform/firmware.hpp"

void scanner_gen_uuid(char* /*output*/, std::size_t /*capacity*/) {
    __builtin_trap();
}

int scanner_post_string(const char* /*request*/) {
    __builtin_trap();
}

int scanner_delete_ccat_entry(const char* /*uuid*/) {
    __builtin_trap();
}
