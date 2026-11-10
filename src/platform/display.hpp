#pragma once
#include "platform/files.hpp"
#include "platform/process.hpp"

namespace oxide {
// Returns after the initial screen clear.
Child startRenderer(Pipe& screen);
}
