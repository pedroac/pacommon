#include "setting.h"
#include <string.h>

/* All the settings live in one block, as consecutive "key\0value\0" records, with no allocation:
 * nothing to fail, free or leak, and the memory is known when building (PAC_SETTING_POOL). A
 * lookup walks the records; a tool has a few dozen settings, so that costs nothing. */
static char pool[PAC_SETTING_POOL];
static size_t used;

/* The record for key, or NULL. */
static char *find(const char *key) {
    for (char *r = pool; r < pool + used;) {
        if (strcmp(r, key) == 0) return r;
        r += strlen(r) + 1;        /* the key */
        r += strlen(r) + 1;        /* the value */
    }
    return NULL;
}

static size_t record_size(const char *r) {
    size_t key = strlen(r) + 1;
    return key + strlen(r + key) + 1;
}

static void remove_record(char *r) {
    size_t n = record_size(r);
    memmove(r, r + n, (size_t)(pool + used - (r + n)));
    used -= n;
    memset(pool + used, 0, n);
}

void pac_setting_clear(void) {
    memset(pool, 0, sizeof pool);
    used = 0;
}

const char *pac_setting(const char *key) {
    if (!key) return NULL;
    char *r = find(key);
    return r ? r + strlen(r) + 1 : NULL;
}

bool pac_setting_set(const char *key, const char *value) {
    size_t klen = key ? strlen(key) : 0;
    if (klen == 0 || klen > PAC_SETTING_KEY_MAX) return false;
    size_t vlen = value ? strlen(value) : 0;
    if (vlen > PAC_SETTING_VALUE_MAX) return false;

    char *old = find(key);
    if (!value) {
        if (old) remove_record(old);
        return true;
    }
    /* Checked before anything changes, so that a refused value leaves the old one. */
    size_t room = sizeof pool - used + (old ? record_size(old) : 0);
    if (klen + vlen + 2 > room) return false;

    if (old) remove_record(old);
    memcpy(pool + used, key, klen + 1);
    memcpy(pool + used + klen + 1, value, vlen + 1);
    used += klen + vlen + 2;
    return true;
}

bool pac_setting_store(const char *key, const char *value, void *data) {
    (void)data;
    return pac_setting_set(key, value);
}
