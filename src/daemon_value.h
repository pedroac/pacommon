#ifndef PACOMMON_DAEMON_VALUE_H
#define PACOMMON_DAEMON_VALUE_H

#include <stdbool.h>
#include <stddef.h>

/* What a client of pacreactd (pacbar, pacreact) reads out of a line of its protocol: "key value<TAB>name=word<TAB>name=word". The
 * grammar is written once in pacreactd's docs/protocol.md; this is the part every client needs, so that a change to it is one change. */

/* The attributes of a value are what follows its first TAB, and are never part of the value: a name that says "level=critical" is only a
 * name. Cuts VALUE off before them and returns them (NULL if there are none). */
char *daemon_split_attrs(char *value);

/* Copies the word of the attribute NAME ("kind=storage" has "storage") to out; false, and out empty, if there is none. */
bool daemon_attr(const char *attrs, const char *name, char *out, size_t n);

/* Whether the attributes have name=word. */
bool daemon_has_attr(const char *attrs, const char *name, const char *word);

/* Whether key is a device of one of these namespaces: "usb.u7" is, "usb.u7.size" and "usb.size" are not (a device is NS.ID, and what it
 * has is under it). */
bool daemon_device_key(const char *key, const char *const *namespaces, size_t count);

#endif
