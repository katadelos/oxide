#include "platform/display.hpp"
#include "platform/files.hpp"
#include "platform/process.hpp"
#include "runtime/runtime.hpp"
#include "scriptlets/metadata.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace oxide {
namespace {
constexpr size_t outputBufferSize = 4096;
constexpr unsigned outputDrainBatches = 32;
constexpr int rendererPollMilliseconds = 50;
constexpr int outputDrainSeconds = 1;
enum class ExecutionPhase : unsigned char {
    Running,
    Draining
};
enum class OutputState : unsigned char {
    More,
    Empty,
    Closed
};

Child startScript(const std::string& path, Pipe& output) {
    const pid_t pid = fork();
    if (pid == 0) {
        const int null = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (null < 0 || dup2(null, STDIN_FILENO) < 0 ||
            dup2(output.writeEnd.get(), STDOUT_FILENO) < 0 ||
            dup2(output.writeEnd.get(), STDERR_FILENO) < 0) {
            _exit(execFailure);
        }
        execl("/bin/sh", "sh", "-l", path.c_str(), static_cast<char*>(nullptr));
        _exit(execFailure);
    }
    Child script(pid);
    output.writeEnd.close();
    setNonblocking(output.readEnd.get());
    return script;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
OutputState drainOutput(int output, int screen, int logFile, size_t& logged) {
    constexpr size_t logLimit = oneMiB;
    char buffer[outputBufferSize];
    for (unsigned batch = 0; batch < outputDrainBatches; ++batch) {
        const ssize_t count = read(output, buffer, sizeof buffer);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count == 0) {
            return OutputState::Closed;
        }
        if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return OutputState::Empty;
            }
            throw std::system_error(errno, std::generic_category(), "Cannot read script output");
        }
        if (logFile >= 0 && logged < logLimit) {
            const size_t available = std::min(static_cast<size_t>(count), logLimit - logged);
            const ssize_t written = write(logFile, buffer, available);
            if (written > 0) {
                logged += written;
            }
        }
        // Display writes are best effort.
        if (screen >= 0) {
            (void)write(screen, buffer, count);
        }
    }
    return OutputState::More;
}
}

int execute(const std::string& input) {
    requireRoot();
    const auto path = requireScriptFile(input);
    const auto header = metadata(path);
    signal(SIGPIPE, SIG_IGN);

    FileDescriptor screenOutput;
    Child renderer;
    if (header.fbink) {
        Pipe screen;
        renderer = startRenderer(screen);
        screenOutput = std::move(screen.writeEnd);
    }
    Pipe output;
    Child script = startScript(path, output);
    FileDescriptor logFile(open(
        "/var/local/oxide/last-run.log",
        O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC,
        privateFileMode));
    if (logFile.get() < 0) {
        log("Cannot open last-run.log");
    }
    size_t logged = 0;
    auto phase = ExecutionPhase::Running;
    auto outputState = OutputState::Empty;
    std::chrono::steady_clock::time_point drainDeadline;
    for (;;) {
        if (phase == ExecutionPhase::Running && script.poll()) {
            phase = ExecutionPhase::Draining;
            drainDeadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(outputDrainSeconds);
        }
        if (outputState != OutputState::Closed) {
            outputState =
                drainOutput(output.readEnd.get(), screenOutput.get(), logFile.get(), logged);
            if (outputState == OutputState::Closed) {
                output.readEnd.close();
            }
        }
        // Descendants may retain stdout; drain buffered output without waiting for EOF.
        if (phase == ExecutionPhase::Draining) {
            if (outputState != OutputState::More ||
                std::chrono::steady_clock::now() >= drainDeadline) {
                break;
            }
        } else if (outputState != OutputState::More) {
            struct pollfd descriptor = {output.readEnd.get(), POLLIN, 0};
            poll(&descriptor, 1, runtimePollMilliseconds);
        }
    }

    output.readEnd.close();
    screenOutput.close();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!renderer.poll() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(rendererPollMilliseconds));
    }
    return script.result();
}
}
