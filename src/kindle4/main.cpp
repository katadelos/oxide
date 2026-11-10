#include "paths.hpp"
#include "platform/files.hpp"
#include "platform/process.hpp"
#include "runtime/runtime.hpp"
#include "scriptlets/metadata.hpp"

#include <chrono>
#include <cstdio>
#include <signal.h>
#include <stdexcept>
#include <sys/prctl.h>
#include <syslog.h>
#include <thread>
#include <unistd.h>

namespace {
constexpr int signalExitOffset = 128;

int launch(const std::string& path) {
    oxide::requireScriptFile(path);
    oxide::ensureDirectory(oxide::stateDir);
    oxide::ensureDirectory(oxide::runDir);
    oxide::FileLock lock(std::string(oxide::runDir) + "/launcher.lock");
    if (!lock.acquired()) {
        throw std::runtime_error("Launcher is already running");
    }
    oxide::installStopSignals();
    const pid_t parent = getppid();
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) < 0 || getppid() != parent) {
        throw std::runtime_error("Cannot watch the booklet process");
    }
    const pid_t pid = fork();
    if (pid == 0) {
        if (setpgid(0, 0) < 0) {
            _exit(oxide::execFailure);
        }
        execl(
            oxide::installedBinary,
            oxide::installedBinary,
            "_execute",
            path.c_str(),
            static_cast<char*>(nullptr));
        _exit(oxide::execFailure);
    }
    oxide::Child child(pid, oxide::Child::Scope::Group);
    setpgid(pid, pid);
    while (!child.poll() && !oxide::stopRequested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(oxide::runtimePollMilliseconds));
    }
    if (oxide::stopRequested()) {
        child.signal(SIGTERM);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!child.poll() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(oxide::runtimePollMilliseconds));
        }
        return signalExitOffset + SIGTERM;
    }
    oxide::log("Script exited " + std::to_string(child.result()));
    return child.result();
}
}

int main(int argc, char** argv) {
    openlog("oxide", LOG_PID, LOG_USER);
    try {
        oxide::requireRoot();
        if (argc == 3) {
            oxide::requireInstalled();
            const std::string action = argv[1];
            if (action == "_metadata") {
                const auto header = oxide::metadata(oxide::requireScriptFile(argv[2]));
                printf("%s\n%s\n", header.name.c_str(), header.author.c_str());
                return 0;
            }
            if (action == "_launch") {
                return launch(argv[2]);
            }
            if (action == "_execute") {
                return oxide::execute(argv[2]);
            }
        }
        puts("Kindle 4: use install.sh to install or uninstall the Home booklet.");
        return argc == 1 || (argc == 2 && std::string(argv[1]) == "--help") ? 0 : 2;
    } catch (const std::exception& error) {
        oxide::log(error.what());
        return 1;
    }
}
