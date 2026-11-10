#include "control.hpp"
#include "platform/files.hpp"
#include "runtime/runtime.hpp"

#include <cstdio>
#include <stdexcept>
#include <syslog.h>

namespace {
int usage(const std::string& action) {
    puts("Usage: oxide start [--no-rescan]|status|rescan|uninstall");
    return action == "help" || action == "--help" ? 0 : 2;
}

int commandAction(const std::string& action) {
    if (action == "start") {
        oxide::start();
        return 0;
    }
    if (action == "uninstall") {
        oxide::uninstall();
        return 0;
    }
    if (action == "status") {
        return oxide::status();
    }
    if (action == "rescan") {
        oxide::rescan();
        return 0;
    }
    if (action == "_serve") {
        return oxide::serve();
    }
    if (action == "_launch") {
        return oxide::launcher();
    }
    return usage(action);
}
}

int main(int argc, char** argv) {
    openlog("oxide", LOG_PID, LOG_USER);
    try {
        const std::string action = argc > 1 ? argv[1] : "help";
        if (action != "help" && action != "--help") {
            oxide::requireInstalled();
        }
        if (argc == 2) {
            return commandAction(action);
        }
        if (argc == 3 && action == "start" && std::string(argv[2]) == "--no-rescan") {
            oxide::start(false);
            return 0;
        }
        if (argc == 3 && action == "_execute") {
            return oxide::execute(argv[2]);
        }
        return usage(action);
    } catch (const std::exception& error) {
        oxide::log(error.what());
        return 1;
    }
}
