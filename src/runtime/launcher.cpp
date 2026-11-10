#include "paths.hpp"
#include "platform/encoding.hpp"
#include "platform/files.hpp"
#include "platform/lipc.hpp"
#include "platform/process.hpp"
#include "runtime/runtime.hpp"

#include <chrono>
#include <mutex>
#include <signal.h>
#include <thread>
#include <unistd.h>

namespace oxide {
namespace {
constexpr int launchTimeoutSeconds = 60;
constexpr int unloadTimeoutSeconds = 10;

struct AppRequest {
    std::string token;
    std::string argument;
};

AppRequest parseRequest(const char* value) {
    if (value == nullptr) {
        throw std::runtime_error("Missing application message");
    }
    const std::string message(value);
    const auto colon = message.find(':');
    const auto token = message.substr(0, colon);
    const bool decimalToken =
        !token.empty() && token.find_first_not_of("0123456789") == std::string::npos;
    if (colon == std::string::npos || !decimalToken) {
        throw std::runtime_error("Invalid application token");
    }
    return {token, message.substr(colon + 1)};
}

std::string launchPath(const std::string& argument) {
    const std::string prefix = std::string("app://") + launcherService;
    if (argument.compare(0, prefix.size(), prefix) != 0) {
        throw std::runtime_error("Invalid application launch URL");
    }
    const auto encoded = argument.substr(prefix.size());
    const auto withoutQuery = encoded.substr(0, encoded.find('?'));
    return unescapeUri(withoutQuery);
}

class App {
public:
    explicit App(Lipc& connection) : client(connection) {}

    void go(const std::string& argument) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (phase_ != Phase::WaitingForGo) {
            throw std::runtime_error("Application already launched or unloaded");
        }
        const auto path = launchPath(argument);
        const char* program = installedBinary;
        const char* filename = path.c_str();
        const pid_t pid = fork();
        if (pid == 0) {
            if (setpgid(0, 0) < 0) {
                _exit(execFailure);
            }
            // LIPC is threaded: exec before calling inherited libraries.
            execl(program, program, "_execute", filename, static_cast<char*>(nullptr));
            _exit(execFailure);
        }
        child_ = Child(pid, Child::Scope::Group);
        setpgid(pid, pid);
        phase_ = Phase::Running;
    }

    void unload() {
        std::lock_guard<std::mutex> guard(mutex_);
        if (phase_ == Phase::Running) {
            child_.signal(SIGTERM);
            phase_ = Phase::Cancelling;
            deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        } else if (phase_ != Phase::Cancelling) {
            phase_ = Phase::Finished;
        }
    }

    bool poll() {
        bool stopApp = false;
        bool active;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            const auto now = std::chrono::steady_clock::now();
            if (phase_ == Phase::Cancelling && now >= deadline_) {
                child_.signal(SIGKILL);
            }
            if ((phase_ == Phase::Running || phase_ == Phase::Cancelling) && child_.poll()) {
                log("Script exited " + std::to_string(child_.result()));
                if (phase_ == Phase::Cancelling) {
                    phase_ = Phase::Finished;
                } else {
                    phase_ = Phase::WaitingForUnload;
                    deadline_ = now + std::chrono::seconds(unloadTimeoutSeconds);
                    stopApp = true;
                }
            }
            if ((phase_ == Phase::WaitingForGo || phase_ == Phase::WaitingForUnload) &&
                now >= deadline_) {
                phase_ = Phase::Finished;
            }
            active = phase_ != Phase::Finished;
        }
        // stop can re-enter unload; release the mutex first.
        if (stopApp) {
            client.set("com.lab126.appmgrd", "stop", launcherService);
        }
        return active;
    }

    Lipc& client;

private:
    enum class Phase : unsigned char {
        WaitingForGo,
        Running,
        Cancelling,
        WaitingForUnload,
        Finished
    };
    std::mutex mutex_;
    Child child_;
    Phase phase_ = Phase::WaitingForGo;
    std::chrono::steady_clock::time_point deadline_ =
        std::chrono::steady_clock::now() + std::chrono::seconds(launchTimeoutSeconds);
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
LIPCcode appEvent(LIPC* /*unused*/, const char* property, void* value, void* context) {
    try {
        if (property == nullptr) {
            return LIPC_ERROR_INVALID_ARG;
        }
        const auto request = parseRequest(static_cast<char*>(value));
        const std::string name(property);
        auto& app = *static_cast<App*>(context);
        int result = 0;
        try {
            if (name == "go") {
                app.go(request.argument);
            } else if (name == "unload") {
                app.unload();
            }
        } catch (const std::exception& error) {
            log(error.what());
            result = 1;
        }
        const auto reply = request.token + ':' + std::to_string(result) + ':';
        app.client.set("com.lab126.appmgrd", (name + "result").c_str(), reply);
        return LIPC_OK;
    } catch (const std::exception& error) {
        log(error.what());
        return LIPC_ERROR_UNKNOWN;
    }
}
}

int launcher() {
    requireRoot();
    FileLock singleton(std::string(runDir) + "/launcher.lock");
    if (!singleton.acquired()) {
        throw std::runtime_error("Launcher is already running");
    }
    installStopSignals();
    Lipc client;
    App app(client);
    Lipc service(launcherService);
    for (const char* name : {"load", "go", "pause", "unload"}) {
        service.stringProperty(name, nullptr, appEvent, &app);
    }
    client.set("com.lab126.appmgrd", "runresult", std::string("0:") + launcherService);
    while (!stopRequested() && app.poll()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(runtimePollMilliseconds));
    }
    app.unload();
    while (app.poll()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(runtimePollMilliseconds));
    }
    return 0;
}
}
