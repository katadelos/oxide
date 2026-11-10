#include <sqlite3.h>

int sqlite3_open_v2(
    const char* /*path*/, sqlite3** /*database*/, int /*flags*/, const char* /*vfs*/) {
    __builtin_trap();
}

int sqlite3_close(sqlite3* /*database*/) {
    __builtin_trap();
}

int sqlite3_busy_timeout(sqlite3* /*database*/, int /*milliseconds*/) {
    __builtin_trap();
}

int sqlite3_exec(
    sqlite3* /*database*/,
    const char* /*sql*/,
    int (* /*callback*/)(void*, int, char**, char**),
    void* /*context*/,
    char** /*error*/) {
    __builtin_trap();
}

const char* sqlite3_errmsg(sqlite3* /*database*/) {
    __builtin_trap();
}

int sqlite3_prepare_v2(
    sqlite3* /*database*/,
    const char* /*sql*/,
    int /*length*/,
    sqlite3_stmt** /*statement*/,
    const char** /*tail*/) {
    __builtin_trap();
}

int sqlite3_finalize(sqlite3_stmt* /*statement*/) {
    __builtin_trap();
}

int sqlite3_step(sqlite3_stmt* /*statement*/) {
    __builtin_trap();
}

int sqlite3_bind_text(
    sqlite3_stmt* /*statement*/,
    int /*index*/,
    const char* /*text*/,
    int /*length*/,
    void (* /*destroy*/)(void*)) {
    __builtin_trap();
}

const unsigned char* sqlite3_column_text(sqlite3_stmt* /*statement*/, int /*column*/) {
    __builtin_trap();
}

int sqlite3_column_type(sqlite3_stmt* /*statement*/, int /*column*/) {
    __builtin_trap();
}
