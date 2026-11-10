#pragma once
#include <openlipc.h>
#include <stdexcept>
#include <string>

namespace oxide {

class LipcError : public std::runtime_error {
public:
    LipcError(const std::string& operation, LIPCcode code);
    LIPCcode code() const;

private:
    LIPCcode code_;
};

// Callback contexts must outlive this connection; callbacks run on LIPC's thread.
class Lipc {
public:
    explicit Lipc(const char* name = nullptr);
    ~Lipc();
    Lipc(const Lipc&) = delete;
    Lipc& operator=(const Lipc&) = delete;

    void
    stringProperty(const char* name, LipcPropCallback get, LipcPropCallback set, void* context);
    void intProperty(const char* name, LipcPropCallback get, LipcPropCallback set, void* context);
    void set(const char* service, const char* property, const std::string& value);
    void set(const char* service, const char* property, int value);
    int get(const char* service, const char* property);

private:
    static void requireSuccess(LIPCcode result, const std::string& operation);
    LIPC* handle_ = nullptr;
};

}
