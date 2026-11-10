#include "platform/process.hpp"

#include <cerrno>
#include <chrono>
#include <signal.h>
#include <stdexcept>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace oxide {
namespace {
constexpr int signalExitOffset = 128;
constexpr int commandPollMilliseconds = 50;
constexpr unsigned commandTimeoutSeconds = 30;
volatile sig_atomic_t signalReceived = 0;

void signalStop(int /*unused*/) {
    signalReceived = 1;
}
}

void installStopSignals() {
    struct sigaction action = {};
    action.sa_handler = signalStop;
    sigemptyset(&action.sa_mask);
    if ((sigaction(SIGTERM, &action, nullptr) != 0) || (sigaction(SIGINT, &action, nullptr) != 0)) {
        throw std::system_error(errno, std::generic_category(), "Cannot install signal handlers");
    }
    signal(SIGPIPE, SIG_IGN);
}

bool stopRequested() {
    return signalReceived != 0;
}

Child::Child(pid_t pid, Scope scope) : pid_(pid), scope_(scope) {
    if (pid < 0) {
        throw std::system_error(errno, std::generic_category(), "Cannot fork child");
    }
}

Child::~Child() {
    cleanup();
}

Child::Child(Child&& other) noexcept
    : pid_(other.release()), scope_(other.scope_), result_(other.result_) {}

Child& Child::operator=(Child&& other) noexcept {
    if (this != &other) {
        cleanup();
        pid_ = other.release();
        scope_ = other.scope_;
        result_ = other.result_;
    }
    return *this;
}

void Child::signal(int number) const {
    if (pid_ > 0) {
        const pid_t target = scope_ == Scope::Group ? -pid_ : pid_;
        kill(target, number);
    }
}

void Child::cleanup() {
    if (pid_ > 0) {
        signal(SIGKILL);
        while (waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
        }
        pid_ = 0;
    }
}

bool Child::running() const {
    return pid_ > 0;
}

pid_t Child::release() {
    const pid_t pid = pid_;
    pid_ = 0;
    return pid;
}

void Child::finish(int status) {
    if (scope_ == Scope::Group) {
        signal(SIGKILL);
    }
    if (WIFEXITED(status)) {
        result_ = WEXITSTATUS(status);
    } else {
        result_ = signalExitOffset + WTERMSIG(status);
    }
    pid_ = 0;
}

bool Child::poll() {
    if (!running()) {
        return true;
    }
    int status = 0;
    const pid_t reaped = waitpid(pid_, &status, WNOHANG);
    if (reaped == pid_) {
        finish(status);
        return true;
    }
    if (reaped < 0 && errno != EINTR) {
        throw std::system_error(errno, std::generic_category(), "Cannot reap child");
    }
    return false;
}

int Child::result() const {
    return result_;
}

int command(const std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    const pid_t pid = fork();
    if (pid == 0) {
        if (setpgid(0, 0) < 0) {
            _exit(execFailure);
        }
        execvp(argv[0], argv.data());
        _exit(execFailure);
    }
    Child child(pid);
    setpgid(pid, pid);
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(commandTimeoutSeconds);
    while (!child.poll()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            kill(-pid, SIGKILL);
            throw std::runtime_error("Command timed out: " + arguments[0]);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(commandPollMilliseconds));
    }
    return child.result();
}
}
