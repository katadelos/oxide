#include "platform/database.hpp"

#include "platform/files.hpp"

#include <climits>
#include <stdexcept>

namespace oxide {
namespace {
constexpr int busyTimeoutMilliseconds = 3000;
}

Database::Database(const char* path, Access access) {
    const int flags = access == Access::ReadWrite ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
    try {
        requireSuccess(sqlite3_open_v2(path, &handle_, flags, nullptr), "Cannot open database");
        requireSuccess(
            sqlite3_busy_timeout(handle_, busyTimeoutMilliseconds), "Cannot set database timeout");
    } catch (...) {
        if (handle_ != nullptr) {
            sqlite3_close(handle_);
        }
        throw;
    }
}

Database::~Database() {
    sqlite3_close(handle_);
}

void Database::requireSuccess(int result, const char* operation) const {
    if (result != SQLITE_OK) {
        const char* detail = (handle_ != nullptr) ? sqlite3_errmsg(handle_) : "No database handle";
        throw std::runtime_error(std::string(operation) + ": " + detail);
    }
}

void Database::exec(const std::string& sql) {
    requireSuccess(
        sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, nullptr), "Cannot execute SQL");
}

Statement::Statement(Database& database, const std::string& sql) : database_(database) {
    database_.requireSuccess(
        sqlite3_prepare_v2(database_.handle_, sql.c_str(), -1, &handle_, nullptr),
        "Cannot prepare query");
}

Statement::~Statement() {
    sqlite3_finalize(handle_);
}

void Statement::bind(int parameter, const std::string& value) {
    if (value.size() > static_cast<size_t>(INT_MAX)) {
        throw std::length_error("SQL binding is too large");
    }
    database_.requireSuccess(
        sqlite3_bind_text(
            handle_, parameter, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT),
        "Cannot bind query parameter");
}

bool Statement::next() {
    const int result = sqlite3_step(handle_);
    if (result == SQLITE_ROW) {
        return true;
    }
    if (result == SQLITE_DONE) {
        return false;
    }
    database_.requireSuccess(result, "Cannot read query");
    return false;
}

std::string Statement::text(int column) const {
    if (sqlite3_column_type(handle_, column) == SQLITE_NULL) {
        throw std::runtime_error("Unexpected NULL database value");
    }
    const auto* value = sqlite3_column_text(handle_, column);
    if (value == nullptr) {
        throw std::bad_alloc();
    }
    return reinterpret_cast<const char*>(value);
}

std::string Statement::textOrEmpty(int column) const {
    if (sqlite3_column_type(handle_, column) == SQLITE_NULL) {
        return "";
    }
    return text(column);
}

Transaction::Transaction(Database& database) : database_(database) {
    database_.exec("BEGIN IMMEDIATE");
}

Transaction::~Transaction() {
    if (!committed_) {
        try {
            database_.exec("ROLLBACK");
        } catch (const std::exception& error) {
            log(error.what());
        }
    }
}

void Transaction::commit() {
    database_.exec("COMMIT");
    committed_ = true;
}
}
