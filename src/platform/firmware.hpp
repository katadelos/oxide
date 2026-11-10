#pragma once
#include <cstddef>

extern "C" {
// output needs at least 37 bytes, including the terminating NUL.
void scanner_gen_uuid(char* output, std::size_t capacity);

// Both return zero on success, negative on failure.
int scanner_post_string(const char* request);
int scanner_delete_ccat_entry(const char* uuid);

// Consumes input; returns malloc-owned input_path + ".jpg", or NULL on failure.
char* extractor_resize_thumbnail(const char* input_path, int width, int height);
}
