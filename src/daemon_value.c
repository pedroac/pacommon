#include "daemon_value.h"
#include <stdio.h>
#include <string.h>

char *daemon_split_attrs(char *value) {
    char *tab = strchr(value, '\t');
    if (!tab) return NULL;
    *tab = '\0';
    return tab + 1;
}

bool daemon_attr(const char *attrs, const char *name, char *out, size_t n) {
    size_t nl = strlen(name);
    if (n) out[0] = '\0';
    for (const char *p = attrs; p && *p;) {
        size_t l = strcspn(p, "\t");
        if (l > nl + 1 && !strncmp(p, name, nl) && p[nl] == '=') {
            if (n) snprintf(out, n, "%.*s", (int)(l - nl - 1), p + nl + 1);
            return true;
        }
        p += l + (p[l] == '\t');
    }
    return false;
}

bool daemon_has_attr(const char *attrs, const char *name, const char *word) {
    char found[32];
    return daemon_attr(attrs, name, found, sizeof found) && !strcmp(found, word);
}

bool daemon_device_key(const char *key, const char *const *namespaces, size_t count) {
    for (size_t i = 0; i < count; i++) {
        size_t n = strlen(namespaces[i]);
        if (!strncmp(key, namespaces[i], n) && key[n] == '.' && key[n + 1] && !strchr(key + n + 1, '.')) return true;
    }
    return false;
}
