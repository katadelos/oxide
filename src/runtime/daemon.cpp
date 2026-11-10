#include "paths.hpp"
#include "platform/files.hpp"
#include "platform/lipc.hpp"
#include "platform/process.hpp"
#include "runtime/runtime.hpp"
#include "scriptlets/catalog.hpp"

#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <mutex>
#include <pthread.h>
#include <signal.h>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace oxide {
namespace {
constexpr int startupTimeoutSeconds = 10;

struct ScannerEvent {
    std::string uuid;
    std::string path;
};

ScannerEvent scannerEvent(const char* value) {
    if (value == nullptr) {
        throw std::runtime_error("Empty scanner event");
    }
    const std::string message(value);
    const auto first = message.find(':');
    if (first == std::string::npos) {
        throw std::runtime_error("Expected glob:uuid:path");
    }
    const auto second = message.find(':', first + 1);
    if (second == std::string::npos) {
        throw std::runtime_error("Expected glob:uuid:path");
    }
    const auto glob = message.substr(0, first);
    if (glob != "*.sh" && glob != "*.SH") {
        throw std::runtime_error("Unexpected scanner glob");
    }
    return {message.substr(first + 1, second - first - 1), message.substr(second + 1)};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
LIPCcode change(LIPC* /*unused*/, const char* property, void* value, void* context) {
    try {
        std::lock_guard<std::mutex> guard(*static_cast<std::mutex*>(context));
        if (property == nullptr) {
            return LIPC_ERROR_INVALID_ARG;
        }
        const auto event = scannerEvent(static_cast<char*>(value));
        if (std::string(property) == "delete") {
            catalog::remove(event.path, event.uuid);
        } else {
            catalog::index(event.path, event.uuid);
        }
        return LIPC_OK;
    } catch (const std::exception& error) {
        log(error.what());
        // Error 8 asks EBridge to retry as delete/add. Ordinary failures must not.
        return LIPC_ERROR_UNKNOWN;
    }
}

LIPCcode ready(LIPC* /*unused*/, const char* /*unused*/, void* value, void* /*unused*/) {
    if (value == nullptr) {
        return LIPC_ERROR_INVALID_ARG;
    }
    *static_cast<int*>(value) = 1;
    return LIPC_OK;
}

LIPCcode shutdown(LIPC* /*unused*/, const char* /*unused*/, void* /*unused*/, void* /*unused*/) {
    return kill(getpid(), SIGTERM) == 0 ? LIPC_OK : LIPC_ERROR_UNKNOWN;
}

void runtimeDirectories() {
    ensureDirectory(runDir, sharedDirectoryMode);
    ensureDirectory(stateDir);
}

}

bool daemonReady() {
    Lipc lipc;
    try {
        return lipc.get(scannerService, "ready") == 1;
    } catch (const LipcError& error) {
        if (error.code() == LIPC_ERROR_NO_SUCH_SOURCE ||
            error.code() == LIPC_ERROR_NO_SUCH_PROPERTY) {
            return false;
        }
        throw;
    }
}

int serve() {
    requireRoot();
    runtimeDirectories();
    FileLock singleton(std::string(runDir) + "/daemon.lock");
    if (!singleton.acquired()) {
        throw std::runtime_error("Daemon is already running");
    }
    // Block before LIPC creates its threads.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGTERM);
    sigaddset(&signals, SIGINT);
    const int blocked = pthread_sigmask(SIG_BLOCK, &signals, nullptr);
    if (blocked != 0) {
        throw std::system_error(blocked, std::generic_category(), "Cannot block shutdown signals");
    }
    signal(SIGPIPE, SIG_IGN);
    std::mutex catalogMutex;
    Lipc lipc(scannerService);
    for (const char* name : {"add", "update", "delete"}) {
        lipc.stringProperty(name, nullptr, change, &catalogMutex);
    }
    lipc.intProperty("shutdown", nullptr, shutdown, nullptr);
    lipc.intProperty("ready", ready, nullptr, nullptr);
    int received = 0;
    const int waited = sigwait(&signals, &received);
    if (waited != 0) {
        throw std::system_error(waited, std::generic_category(), "Cannot wait for shutdown");
    }
    return 0;
}

void startDaemon() {
    requireRoot();
    runtimeDirectories();
    const pid_t pid = fork();
    if (pid == 0) {
        if (setsid() < 0) {
            _exit(1);
        }
        const int null = open("/dev/null", O_RDONLY | O_CLOEXEC);
        const int logFile = open(
            "/var/run/oxide/daemon.log",
            O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
            privateFileMode);
        if (null < 0 || logFile < 0) {
            _exit(1);
        }
        if (dup2(null, 0) < 0 || dup2(logFile, 1) < 0 || dup2(logFile, 2) < 0) {
            _exit(1);
        }
        execl(installedBinary, installedBinary, "_serve", static_cast<char*>(nullptr));
        _exit(execFailure);
    }
    Child daemon(pid);
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(startupTimeoutSeconds);
    while (!daemon.poll() && std::chrono::steady_clock::now() < deadline) {
        if (daemonReady()) {
            daemon.release();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(runtimePollMilliseconds));
    }
    throw std::runtime_error("Daemon did not become ready; see /var/run/oxide/daemon.log");
}
}
