#pragma once
#include <sqlite3.h>
#include <string>

namespace oxide {
class Statement;

class Database {
public:
    enum class Access : unsigned char {
        ReadOnly,
        ReadWrite
    };
    explicit Database(const char* path, Access access = Access::ReadOnly);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    void exec(const std::string& sql);

private:
    friend class Statement;
    void requireSuccess(int result, const char* operation) const;
    sqlite3* handle_ = nullptr;
};

// The database must outlive its statements.
class Statement {
public:
    Statement(Database& database, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    void bind(int parameter, const std::string& value);
    bool next();
    std::string text(int column) const;
    std::string textOrEmpty(int column) const;

private:
    Database& database_;
    sqlite3_stmt* handle_ = nullptr;
};

class Transaction {
public:
    explicit Transaction(Database& database);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    void commit();

private:
    Database& database_;
    bool committed_ = false;
};
}
