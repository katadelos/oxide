#include "platform/lipc.hpp"

namespace oxide {
LipcError::LipcError(const std::string& operation, LIPCcode code)
    : std::runtime_error(operation + " (LIPC " + std::to_string(code) + ")"), code_(code) {}

LIPCcode LipcError::code() const {
    return code_;
}

Lipc::Lipc(const char* name) {
    LIPCcode error = LIPC_OK;
    if (name != nullptr) {
        handle_ = LipcOpenEx(name, &error);
    } else {
        handle_ = LipcOpenNoName();
    }
    if ((handle_ == nullptr) || error != LIPC_OK) {
        if (handle_ != nullptr) {
            LipcClose(handle_);
        }
        if (error == LIPC_OK) {
            error = LIPC_ERROR_INVALID_HANDLE;
        }
        throw LipcError("Cannot open LIPC service", error);
    }
}

Lipc::~Lipc() {
    LipcClose(handle_);
}

void Lipc::requireSuccess(LIPCcode result, const std::string& operation) {
    if (result != LIPC_OK) {
        throw LipcError(operation, result);
    }
}

void Lipc::stringProperty(
    const char* name, LipcPropCallback get, LipcPropCallback set, void* context) {
    requireSuccess(
        LipcRegisterStringProperty(handle_, name, get, set, context),
        std::string("Cannot register ") + name);
}

void Lipc::intProperty(
    const char* name, LipcPropCallback get, LipcPropCallback set, void* context) {
    requireSuccess(
        LipcRegisterIntProperty(handle_, name, get, set, context),
        std::string("Cannot register ") + name);
}

void Lipc::set(const char* service, const char* property, const std::string& value) {
    requireSuccess(
        LipcSetStringProperty(handle_, service, property, value.c_str()),
        std::string("Cannot set ") + service + "." + property);
}

void Lipc::set(const char* service, const char* property, int value) {
    requireSuccess(
        LipcSetIntProperty(handle_, service, property, value),
        std::string("Cannot set ") + service + "." + property);
}

int Lipc::get(const char* service, const char* property) {
    int value = 0;
    requireSuccess(
        LipcGetIntProperty(handle_, service, property, &value),
        std::string("Cannot read ") + service + "." + property);
    return value;
}
}
