#pragma once
#include <cJSON.h>
#include <memory>
#include <string>

namespace oxide {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

inline cJSON* ensure(cJSON* value) {
    if (value == nullptr) {
        throw std::bad_alloc();
    }
    return value;
}

inline cJSON* appendObject(cJSON* array) {
    Json object(ensure(cJSON_CreateObject()), cJSON_Delete);
    if (cJSON_AddItemToArray(array, object.get()) == 0) {
        throw std::bad_alloc();
    }
    return object.release();
}

inline std::string serialize(const cJSON* value) {
    std::unique_ptr<char, decltype(&cJSON_free)> text(cJSON_PrintUnformatted(value), cJSON_free);
    if (!text) {
        throw std::bad_alloc();
    }
    return text.get();
}

}
