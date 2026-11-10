#pragma once
#include <string>
#include <sys/types.h>
#include <vector>

namespace oxide {
constexpr int execFailure = 127;

// Destruction kills and reaps; Group also kills descendants after the child exits.
class Child {
public:
    enum class Scope : unsigned char {
        Process,
        Group
    };
    explicit Child(pid_t pid = 0, Scope scope = Scope::Process);
    ~Child();
    Child(Child&& other) noexcept;
    Child& operator=(Child&& other) noexcept;
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
    bool running() const;
    bool poll();
    int result() const;
    void signal(int number) const;
    pid_t release();

private:
    void cleanup();
    void finish(int status);
    pid_t pid_;
    Scope scope_;
    int result_ = 0;
};

int command(const std::vector<std::string>& arguments);
void installStopSignals();
bool stopRequested();
}
