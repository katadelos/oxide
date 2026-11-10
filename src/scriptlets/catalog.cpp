#include "scriptlets/catalog.hpp"

#include "paths.hpp"
#include "platform/database.hpp"
#include "platform/files.hpp"
#include "platform/firmware.hpp"
#include "platform/json.hpp"
#include "scriptlets/metadata.hpp"
#include "scriptlets/thumbnails.hpp"

#include <cctype>
#include <cerrno>
#include <stdexcept>
#include <sys/stat.h>

namespace oxide {
bool catalog::validUuid(const std::string& uuid) {
    if (uuid.size() != uuidLength) {
        return false;
    }
    const std::string pattern = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";
    for (size_t i = 0; i < uuid.size(); ++i) {
        if (pattern[i] == '-') {
            if (uuid[i] != '-') {
                return false;
            }
        } else if (std::isxdigit(static_cast<unsigned char>(uuid[i])) == 0) {
            return false;
        }
    }
    return true;
}

namespace {
struct Entry {
    std::string uuid;
    std::string path;
    std::string image;
};

Entry findEntry(Database& database, const char* query, const std::string& value) {
    Statement row(database, query);
    row.bind(1, value);
    if (!row.next()) {
        return {};
    }
    Entry entry{row.text(0), row.text(1), row.textOrEmpty(2)};
    if (!catalog::validUuid(entry.uuid)) {
        throw std::runtime_error("Invalid catalogue UUID");
    }
    if (row.next()) {
        throw std::runtime_error("Ambiguous catalogue entry");
    }
    return entry;
}

Entry findByLocation(Database& database, const std::string& path) {
    return findEntry(
        database, "SELECT p_uuid,p_location,p_thumbnail FROM Entries WHERE p_location=?", path);
}

Entry findByUuid(Database& database, const std::string& uuid) {
    return findEntry(
        database, "SELECT p_uuid,p_location,p_thumbnail FROM Entries WHERE p_uuid=?", uuid);
}

std::string metadataRequest(
    const Metadata& meta,
    const std::string& path,
    const std::string& uuid,
    const std::string& image,
    const struct stat& status,
    bool update) {
    Json request(ensure(cJSON_CreateObject()), cJSON_Delete);
    ensure(cJSON_AddStringToObject(request.get(), "type", "ChangeRequest"));
    auto* commands = ensure(cJSON_AddArrayToObject(request.get(), "commands"));
    auto* command = appendObject(commands);
    auto* record = ensure(cJSON_AddObjectToObject(command, update ? "update" : "insert"));
    ensure(cJSON_AddStringToObject(record, "uuid", uuid.c_str()));
    ensure(cJSON_AddStringToObject(record, "location", path.c_str()));
    ensure(cJSON_AddStringToObject(record, "type", "Entry:Item"));
    ensure(cJSON_AddStringToObject(record, "mimeType", mime));
    ensure(cJSON_AddStringToObject(record, "cdeKey", ("oxide-" + uuid).c_str()));
    ensure(cJSON_AddStringToObject(record, "cdeType", "PDOC"));
    ensure(cJSON_AddStringToObject(record, "thumbnail", image.c_str()));
    ensure(
        cJSON_AddNumberToObject(record, "modificationTime", static_cast<double>(status.st_mtime)));
    ensure(cJSON_AddNumberToObject(record, "diskUsage", static_cast<double>(status.st_size)));
    ensure(cJSON_AddNumberToObject(record, "contentSize", static_cast<double>(status.st_size)));
    ensure(cJSON_AddBoolToObject(record, "isVisibleInHome", 1));
    ensure(cJSON_AddBoolToObject(record, "isArchived", 0));
    auto* titles = ensure(cJSON_AddArrayToObject(record, "titles"));
    auto* title = appendObject(titles);
    ensure(cJSON_AddStringToObject(title, "display", meta.name.c_str()));
    auto* credits = ensure(cJSON_AddArrayToObject(record, "credits"));
    auto* credit = appendObject(credits);
    ensure(cJSON_AddStringToObject(credit, "kind", "Author"));
    auto* author = ensure(cJSON_AddObjectToObject(credit, "name"));
    ensure(cJSON_AddStringToObject(author, "display", meta.author.c_str()));
    auto* displays = ensure(cJSON_AddArrayToObject(record, "displayObjects"));
    for (const char* field : {"titles", "credits"}) {
        auto* display = appendObject(displays);
        ensure(cJSON_AddStringToObject(display, "ref", field));
    }
    return serialize(request.get());
}

}

void catalog::check() {
    Database database("/var/local/cc.db");
    Statement columns(database, "SELECT p_uuid,p_location,p_thumbnail FROM Entries LIMIT 0");
    columns.next();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void catalog::index(const std::string& input, const std::string& requestedUuid) {
    const auto path = requireScriptFile(input);
    if (!requestedUuid.empty() && !validUuid(requestedUuid)) {
        throw std::runtime_error("Invalid event UUID");
    }
    const auto meta = metadata(path);
    struct stat status;
    if (stat(path.c_str(), &status) != 0) {
        throw std::runtime_error("Cannot stat script");
    }
    Database database("/var/local/cc.db");
    Entry entry = findByLocation(database, path);
    if (!requestedUuid.empty() && !entry.uuid.empty() && requestedUuid != entry.uuid) {
        throw std::runtime_error("UUID does not match script");
    }
    if (entry.uuid.empty() && !requestedUuid.empty()) {
        entry = findByUuid(database, requestedUuid);
        if (!entry.uuid.empty() && entry.path != path) {
            requireScriptLocation(entry.path);
            struct stat previous;
            if ((stat(entry.path.c_str(), &previous) == 0) || errno != ENOENT) {
                throw std::runtime_error("UUID belongs to another file");
            }
        }
    }
    const bool update = !entry.uuid.empty();
    std::string uuid = entry.uuid.empty() ? requestedUuid : entry.uuid;
    if (uuid.empty()) {
        char generated[uuidLength + 1] = {};
        scanner_gen_uuid(generated, sizeof generated);
        generated[uuidLength] = '\0';
        uuid = generated;
        if (!validUuid(uuid)) {
            throw std::runtime_error("Scanner returned invalid UUID");
        }
    }
    std::string image = entry.image;
    try {
        image = thumbnails::create(meta, path, uuid);
    } catch (const std::exception& error) {
        log(std::string("Icon: ") + error.what());
    }
    try {
        const auto payload = metadataRequest(meta, path, uuid, image, status, update);
        if (scanner_post_string(payload.c_str()) != 0) {
            throw std::runtime_error("Catalogue rejected script metadata");
        }
    } catch (...) {
        if (image != entry.image) {
            thumbnails::remove(image);
        }
        throw;
    }
    if (image != entry.image) {
        thumbnails::remove(entry.image);
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void catalog::remove(const std::string& input, const std::string& requestedUuid) {
    const auto path = requireScriptLocation(input);
    if (!requestedUuid.empty() && !validUuid(requestedUuid)) {
        throw std::runtime_error("Invalid delete UUID");
    }
    struct stat status;
    if ((lstat(path.c_str(), &status) == 0) || errno != ENOENT) {
        throw std::runtime_error("Refusing to delete a present or inaccessible script");
    }
    Database database("/var/local/cc.db");
    Entry entry = findByLocation(database, path);
    if (entry.uuid.empty()) {
        return;
    }
    if (!requestedUuid.empty() && requestedUuid != entry.uuid) {
        throw std::runtime_error("Delete UUID mismatch");
    }
    if (scanner_delete_ccat_entry(entry.uuid.c_str()) < 0) {
        throw std::runtime_error("Catalogue rejected removal");
    }
    thumbnails::remove(entry.image);
}

}
