#include <glib.h>

void g_free(gpointer /*memory*/) {
    __builtin_trap();
}

gchar* g_uri_unescape_string(const gchar* /*escaped*/, const gchar* /*illegal*/) {
    __builtin_trap();
}
