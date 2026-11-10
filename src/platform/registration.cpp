#include "platform/registration.hpp"

#include "paths.hpp"
#include "platform/database.hpp"
#include "platform/files.hpp"

#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace oxide {
namespace {
const char* const shellAssociations = "contentId IN ('GL:*.sh','GL:*.SH','MT:text/x-shellscript')";

std::string launcherCommand() {
    return std::string("/var/local/su -s ") + installedBinary + " root _launch";
}

registration::State readState(Database& database) {
    Statement rows(
        database,
        "SELECT handlerId,interface,contentId,defaultAssoc FROM associations WHERE " +
            std::string(shellAssociations));
    unsigned count = 0;
    while (rows.next()) {
        const auto handler = rows.text(0);
        const auto interface = rows.text(1);
        const auto content = rows.text(2);
        const bool extractor = handler == "org.katadelos.oxide.extractor" &&
                               interface == "extractor" &&
                               (content == "GL:*.sh" || content == "GL:*.SH");
        const bool launcher = handler == launcherService && interface == "application" &&
                              content == "MT:text/x-shellscript";
        if ((!extractor && !launcher) || rows.text(3) != "true") {
            return registration::State::Conflict;
        }
        ++count;
    }
    if (count == 0) {
        return registration::State::Inactive;
    }
    constexpr unsigned expectedAssociations = 3;
    if (count != expectedAssociations) {
        return registration::State::Conflict;
    }
    Statement command(
        database,
        "SELECT value FROM properties WHERE handlerId='org.katadelos.oxide.launcher' "
        "AND name='command'");
    if (!command.next() || command.text(0) != launcherCommand()) {
        return registration::State::Conflict;
    }
    return registration::State::Owned;
}

void clear(Database& database) {
    database.exec("DELETE FROM associations WHERE handlerId IN "
                  "('org.katadelos.oxide.extractor','org.katadelos.oxide.launcher');"
                  "DELETE FROM properties WHERE handlerId IN "
                  "('org.katadelos.oxide.extractor','org.katadelos.oxide.launcher');"
                  "DELETE FROM handlerIds WHERE handlerId IN "
                  "('org.katadelos.oxide.extractor','org.katadelos.oxide.launcher');");
    Statement remaining(
        database,
        "SELECT 1 FROM associations WHERE " + std::string(shellAssociations) + " LIMIT 1");
    if (!remaining.next()) {
        database.exec(
            "DELETE FROM mimetypes WHERE ext='sh' AND mimetype='MT:text/x-shellscript';"
            "DELETE FROM extenstions WHERE ext='sh' AND mimetype='MT:text/x-shellscript';");
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void property(Database& database, const char* handler, const char* name, const std::string& value) {
    Statement insert(database, "INSERT INTO properties(handlerId,name,value) VALUES(?,?,?)");
    insert.bind(1, handler);
    insert.bind(2, name);
    insert.bind(3, value);
    insert.next();
}

}

void registration::check() {
    struct stat helper;
    if (stat("/var/local/su", &helper) != 0 || !S_ISREG(helper.st_mode) || helper.st_uid != 0 ||
        (helper.st_mode & S_ISUID) == 0 || access("/var/local/su", X_OK) != 0) {
        throw std::runtime_error("/var/local/su must be installed setuid root");
    }
    if (!exists("/usr/lib/ccat/libEBridge.so")) {
        throw std::runtime_error("Stock EBridge extractor is missing");
    }
    Database database("/var/local/appreg.db");
    Statement columns(
        database, "SELECT handlerId,interface,contentId,defaultAssoc FROM associations LIMIT 0");
    columns.next();
}

void registration::install() {
    Database database("/var/local/appreg.db", Database::Access::ReadWrite);
    Transaction transaction(database);
    if (readState(database) == State::Conflict) {
        throw std::runtime_error(
            "Conflicting shellscript registration; remove it before starting oxide");
    }
    clear(database);
    database.exec("INSERT OR IGNORE INTO interfaces VALUES('application');"
                  "INSERT OR IGNORE INTO interfaces VALUES('extractor');"
                  "INSERT INTO handlerIds VALUES('org.katadelos.oxide.extractor');"
                  "INSERT INTO handlerIds VALUES('org.katadelos.oxide.launcher');");
    property(database, "org.katadelos.oxide.extractor", "lib", "/usr/lib/ccat/libEBridge.so");
    property(database, "org.katadelos.oxide.extractor", "entry", "load_bridge");
    property(database, "org.katadelos.oxide.extractor", "lipcId", scannerService);
    property(database, launcherService, "command", launcherCommand());
    property(database, launcherService, "lipcId", launcherService);
    property(database, launcherService, "extend-start", "Y");
    property(database, launcherService, "unloadPolicy", "unloadOnPause");
    property(database, launcherService, "searchbar-mode", "transient");
    property(database, launcherService, "supportedOrientation", "U");
    property(database, launcherService, "default-chrome-style", "NH");
    for (const char* name : {"maxLoadTime", "maxGoTime", "maxPauseTime", "maxUnloadTime"}) {
        property(database, launcherService, name, "60");
    }
    database.exec(
        "INSERT INTO mimetypes VALUES('sh','MT:text/x-shellscript'); INSERT INTO extenstions "
        "VALUES('sh','MT:text/x-shellscript');"
        "INSERT INTO associations "
        "VALUES('org.katadelos.oxide.extractor','extractor','GL:*.sh','true');"
        "INSERT INTO associations "
        "VALUES('org.katadelos.oxide.extractor','extractor','GL:*.SH','true');"
        "INSERT INTO associations "
        "VALUES('org.katadelos.oxide.launcher','application','MT:text/x-shellscript','true');");
    transaction.commit();
}

bool registration::uninstall() {
    Database database("/var/local/appreg.db", Database::Access::ReadWrite);
    Transaction transaction(database);
    bool registered;
    {
        Statement owned(
            database,
            "SELECT 1 FROM handlerIds WHERE handlerId IN "
            "('org.katadelos.oxide.extractor','org.katadelos.oxide.launcher') LIMIT 1");
        registered = owned.next();
    }
    if (!registered) {
        return false;
    }
    clear(database);
    transaction.commit();
    return true;
}

registration::State registration::state() {
    Database database("/var/local/appreg.db");
    return readState(database);
}
}
