#ifndef PACOMMON_SETTING_H
#define PACOMMON_SETTING_H

#include <stdbool.h>
#include "tunables.h"

/* The string settings of a tool whose code reads them where it needs them, as getenv() would be
 * read. They come from the settings file (see config_file.h) and from nowhere else: not from the
 * environment. Keys are lower case, as in the file: "font_size". They are kept in one fixed block
 * (PAC_SETTING_POOL, tunables.h), so nothing is allocated. */

/* The value of a setting, or NULL if it is not set (or key is NULL). The pointer is valid until
 * the next change to any setting: copy the value if it is needed after that. */
const char *pac_setting(const char *key);

/* Sets a setting, replacing an earlier value; NULL unsets it. False, leaving everything as it was,
 * if the key is empty or longer than PAC_SETTING_KEY_MAX, the value is longer than
 * PAC_SETTING_VALUE_MAX, or the block is full. */
bool pac_setting_set(const char *key, const char *value);

/* Unsets every setting. */
void pac_setting_clear(void);

/* A PacConfigHandler that stores every key it is given: for the tool whose handler accepts any
 * of the settings its code reads. */
bool pac_setting_store(const char *key, const char *value, void *data);

#endif
