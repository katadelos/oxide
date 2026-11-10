cmake_minimum_required(VERSION 3.21)
project(oxide VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(su src/su.c)
target_compile_options(su PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)
add_executable(oxide
    src/runtime/execution.cpp
    src/scriptlets/metadata.cpp
    src/platform/files.cpp
    src/platform/process.cpp
    src/platform/display.cpp
)
include(cmake/FBInk.cmake)
if(OXIDE_ARCH STREQUAL "kindle4")
    target_sources(oxide PRIVATE src/kindle4/main.cpp)
    include(cmake/booklet.cmake)
else()
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/cjson/cJSON.c")
        message(FATAL_ERROR "Initialize dependencies with ./scripts/checkout-deps")
    endif()
    add_library(cjson STATIC third_party/cjson/cJSON.c)
    target_include_directories(cjson SYSTEM PUBLIC third_party/cjson)
    set_target_properties(cjson PROPERTIES C_VISIBILITY_PRESET hidden)
    target_sources(oxide PRIVATE
        src/main.cpp
        src/control.cpp
        src/runtime/daemon.cpp
        src/runtime/launcher.cpp
        src/scriptlets/catalog.cpp
        src/scriptlets/thumbnails.cpp
        src/platform/encoding.cpp
        src/platform/database.cpp
        src/platform/registration.cpp
        src/platform/lipc.cpp
    )
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    pkg_check_modules(SQLITE REQUIRED sqlite3)
    target_link_libraries(oxide PRIVATE cjson)
    include(cmake/KindleLibraries.cmake)
    include(cmake/openlipc.cmake)
endif()
target_include_directories(oxide PRIVATE src)
target_compile_definitions(oxide PRIVATE _FILE_OFFSET_BITS=64)
target_compile_options(oxide PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type -fvisibility=hidden)
find_package(Threads REQUIRED)
target_link_libraries(oxide PRIVATE Threads::Threads)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(oxide PRIVATE -static-libstdc++ -static-libgcc -Wl,--exclude-libs,ALL)
endif()
install(TARGETS oxide RUNTIME DESTINATION /var/local/oxide)
install(TARGETS su RUNTIME DESTINATION /var/local
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE SETUID)

include(cmake/Checks.cmake)
oxide_add_checks()
include(cmake/package.cmake)
