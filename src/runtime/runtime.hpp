#pragma once
#include <string>

namespace oxide {
constexpr int runtimePollMilliseconds = 100;

void startDaemon();
bool daemonReady();
int serve();
int launcher();
int execute(const std::string& input);
}
