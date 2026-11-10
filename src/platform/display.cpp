#include "platform/display.hpp"

#include <fbink.h>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace oxide {
namespace {
constexpr int rendererStartupMilliseconds = 30000;
constexpr short firstRow = 5;

class Framebuffer {
public:
    Framebuffer() : fd_(fbink_open()) {
        if (fd_ < 0) {
            throw std::runtime_error("Cannot open framebuffer");
        }
    }

    ~Framebuffer() {
        fbink_close(fd_);
    }

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    int get() const {
        return fd_;
    }

private:
    int fd_;
};

void render(FileDescriptor& ready) {
    Framebuffer framebuffer;
    FBInkConfig config = {};
    config.row = firstRow;
    config.is_rpadded = true;
    config.is_quiet = true;
    if (fbink_init(framebuffer.get(), &config) < 0 ||
        fbink_cls(framebuffer.get(), &config, nullptr, false) < 0) {
        throw std::runtime_error("Cannot initialize or clear framebuffer");
    }
    FBInkState state = {};
    fbink_get_state(&config, &state);
    if (state.max_rows == 0 || write(ready.get(), "1", 1) != 1) {
        throw std::runtime_error("Cannot signal renderer readiness");
    }
    ready.close();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!std::cin.eof()) {
            line += '\n';
        }
        const int rows = fbink_print(framebuffer.get(), line.c_str(), &config);
        if (rows < 0) {
            log("Cannot render script output: " + std::to_string(rows));
            continue;
        }
        config.row = static_cast<short>((config.row + rows) % state.max_rows);
    }
    if (std::cin.bad()) {
        throw std::runtime_error("Cannot read renderer input");
    }
}
}

Child startRenderer(Pipe& screen) {
    Pipe ready;
    const pid_t pid = fork();
    if (pid == 0) {
        ready.readEnd.close();
        screen.writeEnd.close();
        if (dup2(screen.readEnd.get(), STDIN_FILENO) < 0) {
            _exit(execFailure);
        }
        screen.readEnd.close();
        try {
            render(ready.writeEnd);
            _exit(0);
        } catch (const std::exception& error) {
            log(error.what());
            _exit(1);
        }
    }
    Child renderer(pid);
    ready.writeEnd.close();
    screen.readEnd.close();
    struct pollfd descriptor = {ready.readEnd.get(), POLLIN, 0};
    char initialized = 0;
    if (poll(&descriptor, 1, rendererStartupMilliseconds) != 1 ||
        read(ready.readEnd.get(), &initialized, 1) != 1 || initialized != '1') {
        throw std::runtime_error("Framebuffer renderer did not become ready");
    }
    setNonblocking(screen.writeEnd.get());
    return renderer;
}
}
