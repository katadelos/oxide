#pragma once

namespace oxide {
namespace registration {
enum class State : unsigned char {
    Inactive,
    Owned,
    Conflict
};
void check();
State state();
void install();
bool uninstall();
}
}
