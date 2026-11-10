#include <openlipc.h>

LIPC* LipcOpenNoName() {
    __builtin_trap();
}

LIPC* LipcOpenEx(const char* /*service*/, LIPCcode* /*code*/) {
    __builtin_trap();
}

void LipcClose(LIPC* /*lipc*/) {
    __builtin_trap();
}

LIPCcode LipcRegisterStringProperty(
    LIPC* /*lipc*/,
    const char* /*property*/,
    LipcPropCallback /*get*/,
    LipcPropCallback /*set*/,
    void* /*data*/) {
    __builtin_trap();
}

LIPCcode LipcRegisterIntProperty(
    LIPC* /*lipc*/,
    const char* /*property*/,
    LipcPropCallback /*get*/,
    LipcPropCallback /*set*/,
    void* /*data*/) {
    __builtin_trap();
}

LIPCcode LipcSetStringProperty(
    LIPC* /*lipc*/, const char* /*service*/, const char* /*property*/, const char* /*value*/) {
    __builtin_trap();
}

LIPCcode LipcSetIntProperty(
    LIPC* /*lipc*/, const char* /*service*/, const char* /*property*/, int /*value*/) {
    __builtin_trap();
}

LIPCcode LipcGetIntProperty(
    LIPC* /*lipc*/, const char* /*service*/, const char* /*property*/, int* /*value*/) {
    __builtin_trap();
}
