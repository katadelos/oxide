# Import stubs link against firmware paths and are never installed.
function(oxide_import_library target source filename stock_path)
    add_library(${target} SHARED "${PROJECT_SOURCE_DIR}/cmake/imports/${source}.cpp")
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${filename}"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/imports"
        NO_SONAME TRUE)
    target_link_options(${target} PRIVATE -nostdlib "-Wl,-soname,${stock_path}")
endfunction()

oxide_import_library(kindle_scanner scanner scanner /usr/lib/libscanner.so)
oxide_import_library(kindle_extractor extractor extractor_util /usr/lib/libextractor_util.so)
oxide_import_library(kindle_glib glib glib-2.0 /usr/lib/libglib-2.0.so.0)
oxide_import_library(kindle_sqlite sqlite sqlite3 /usr/lib/libsqlite3.so.0)
target_include_directories(kindle_glib SYSTEM PUBLIC ${GLIB_INCLUDE_DIRS})
target_include_directories(kindle_sqlite SYSTEM PUBLIC ${SQLITE_INCLUDE_DIRS})

target_link_libraries(oxide PRIVATE kindle_scanner kindle_extractor kindle_glib kindle_sqlite)
# Eager binding covers missing library dependencies in PW1 firmware.
target_link_options(oxide PRIVATE -Wl,-z,now)
set_target_properties(oxide PROPERTIES SKIP_BUILD_RPATH TRUE)
