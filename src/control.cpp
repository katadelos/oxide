#include "control.hpp"

#include "paths.hpp"
#include "platform/files.hpp"
#include "platform/lipc.hpp"
#include "platform/process.hpp"
#include "platform/registration.hpp"
#include "runtime/runtime.hpp"
#include "scriptlets/catalog.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <strings.h>
#include <sys/stat.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

namespace oxide {
namespace {
constexpr unsigned shutdownPollAttempts = 50;
constexpr int scannerStartupSeconds = 10;
const char* const stockScanner = "com.lab126.scanner";
const char* const scannerRestartFile = "/var/run/oxide/scanner-restart-pending";
enum class ScannerState : unsigned char {
    Current,
    RestartPending
};

ScannerState scannerState() {
    return exists(scannerRestartFile) ? ScannerState::RestartPending : ScannerState::Current;
}

void requireScannerRestart() {
    ensureDirectory(runDir, sharedDirectoryMode);
    // Persist restart intent before registration changes, so failures remain retryable.
    auto pending = openFile(scannerRestartFile, O_WRONLY | O_CREAT | O_NOFOLLOW);
}

void waitForScanner(Lipc& lipc) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(scannerStartupSeconds);
    for (;;) {
        try {
            lipc.get(stockScanner, "fullScanStatus");
            return;
        } catch (const LipcError& error) {
            const bool starting = error.code() == LIPC_ERROR_NO_SUCH_SOURCE ||
                                  error.code() == LIPC_ERROR_NO_SUCH_PROPERTY;
            if (!starting || std::chrono::steady_clock::now() >= deadline) {
                throw;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(runtimePollMilliseconds));
    }
}

void restartScanner() {
    command({"/sbin/stop", "scanner"});
    if (command({"/sbin/start", "scanner"}) != 0) {
        throw std::runtime_error("Cannot start stock scanner");
    }
    Lipc lipc;
    waitForScanner(lipc);
    if (unlink(scannerRestartFile) < 0) {
        throw std::system_error(errno, std::generic_category(), "Cannot clear scanner restart");
    }
}

void removeRegistration() {
    requireScannerRestart();
    registration::uninstall();
    restartScanner();
}

void waitForExit(const std::string& name) {
    const auto path = std::string(runDir) + "/" + name + ".lock";
    if (!exists(path)) {
        return;
    }
    for (unsigned attempt = 0; attempt < shutdownPollAttempts; ++attempt) {
        FileLock process(path);
        if (process.acquired()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(runtimePollMilliseconds));
    }
    throw std::runtime_error(name + " did not stop");
}

void stopLauncher() {
    const auto path = std::string(runDir) + "/launcher.lock";
    if (!exists(path)) {
        return;
    }
    {
        FileLock launcher(path);
        if (launcher.acquired()) {
            return;
        }
    }
    Lipc lipc;
    lipc.set("com.lab126.appmgrd", "stop", launcherService);
    waitForExit("launcher");
}

void stopDaemon() {
    if (daemonReady()) {
        Lipc lipc;
        lipc.set(scannerService, "shutdown", 1);
    }
    waitForExit("daemon");
}

void recoverActivation(bool startedDaemon) {
    // Keep the bridge alive until the scanner releases its registration.
    try {
        removeRegistration();
        if (startedDaemon) {
            stopDaemon();
        }
    } catch (const std::exception& error) {
        log(std::string("Activation recovery: ") + error.what());
    }
}

void walk(const std::string& path, std::vector<std::string>& scripts) {
    std::unique_ptr<DIR, int (*)(DIR*)> directory(opendir(path.c_str()), closedir);
    if (!directory) {
        throw std::runtime_error("Cannot scan " + path);
    }
    for (;;) {
        errno = 0;
        const auto* entry = readdir(directory.get());
        if (entry == nullptr) {
            if (errno != 0) {
                throw std::runtime_error("Cannot read directory " + path);
            }
            break;
        }
        const std::string name = entry->d_name;
        if (name.empty() || name[0] == '.') {
            continue;
        }
        std::string file = path;
        file += '/';
        file += name;
        struct stat status;
        if (lstat(file.c_str(), &status) != 0) {
            throw std::runtime_error("Cannot stat " + file);
        }
        if (S_ISDIR(status.st_mode)) {
            const bool sidecar = name.size() >= 4 && name.substr(name.size() - 4) == ".sdr";
            if (!sidecar) {
                walk(file, scripts);
            }
        } else if (S_ISREG(status.st_mode)) {
            const bool shellScript =
                name.size() > 3 && (strcasecmp(name.c_str() + name.size() - 3, ".sh") == 0);
            if (shellScript) {
                scripts.push_back(file);
            }
        }
    }
}
}

void start(bool refresh) {
    requireRoot();
    FileLock lifecycle("/var/run/oxide-control.lock");
    if (!lifecycle.acquired()) {
        throw std::runtime_error("Another controller is active");
    }
    registration::check();
    catalog::check();
    ensureDirectory(stateDir);
    const auto registrationState = registration::state();
    if (registrationState == registration::State::Conflict) {
        throw std::runtime_error(
            "Conflicting shellscript registration; remove it before starting oxide");
    }
    const bool alreadyRegistered = registrationState == registration::State::Owned;
    const bool startedDaemon = !daemonReady();
    if (alreadyRegistered && !startedDaemon && scannerState() == ScannerState::Current) {
        puts("oxide is already active");
        return;
    }
    requireScannerRestart();
    if (startedDaemon) {
        startDaemon();
    }
    try {
        if (!alreadyRegistered) {
            registration::install();
        }
        restartScanner();
    } catch (...) {
        if (!alreadyRegistered) {
            recoverActivation(startedDaemon);
        }
        throw;
    }
    puts("oxide is active");
    if (!refresh) {
        return;
    }
    try {
        rescan();
    } catch (const std::exception& error) {
        throw std::runtime_error(
            std::string("oxide is active, but refresh failed: ") + error.what());
    }
}

void uninstall() {
    requireRoot();
    FileLock lifecycle("/var/run/oxide-control.lock");
    if (!lifecycle.acquired()) {
        throw std::runtime_error("Another controller is active");
    }
    stopLauncher();
    removeRegistration();
    stopDaemon();
    puts("oxide is uninstalled; scripts retained");
}

void rescan() {
    requireRoot();
    if (!daemonReady() || registration::state() != registration::State::Owned ||
        scannerState() == ScannerState::RestartPending) {
        throw std::runtime_error("oxide is not active; check oxide status");
    }
    Lipc lipc;
    if (lipc.get("com.lab126.volumd", "userstoreIsAvailable") != 1) {
        throw std::runtime_error("Userstore is unavailable");
    }
    waitForScanner(lipc);
    std::vector<std::string> paths;
    walk("/mnt/us/documents", paths);
    unsigned failures = 0;
    // Refresh unchanged icons per file; the full scan below recovers missed deletions.
    for (const auto& path : paths) {
        try {
            lipc.set(stockScanner, "reScanFile", path);
        } catch (const std::exception& error) {
            log(path + ": " + error.what());
            ++failures;
        }
    }
    lipc.set(stockScanner, "doFullScan", 1);
    if (failures != 0) {
        throw std::runtime_error(std::to_string(failures) + " refresh requests failed");
    }
}

int status() {
    const bool live = daemonReady();
    const auto registrationState = registration::state();
    const auto scanner = scannerState();
    printf("daemon: %s\n", live ? "ready" : "stopped");
    printf(
        "scanner configuration: %s\n",
        scanner == ScannerState::Current ? "current" : "restart pending");
    switch (registrationState) {
    case registration::State::Inactive:
        puts("registration: inactive");
        break;
    case registration::State::Owned:
        puts("registration: installed");
        break;
    case registration::State::Conflict:
        puts("registration: conflicting shellscript handler or launcher command");
        break;
    }
    return live && registrationState == registration::State::Owned &&
                   scanner == ScannerState::Current
               ? 0
               : 1;
}
}
