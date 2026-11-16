#ifndef PACOMMON_CONFIG_FILE_H
#define PACOMMON_CONFIG_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include "tunables.h"


/* The settings file of every pac tool: "key = value" lines, '#' starts a comment, blank lines are
 * ignored. `prog` is the tool's name: it names the file (<prog>.conf) and starts every message.
 *
 * The file is looked for in three places, each overriding the earlier:
 *   SYSCONFDIR/pac/<prog>.conf                      (/etc/pac)
 *   $XDG_CONFIG_HOME/pac/<prog>.conf, or ~/.config/pac/<prog>.conf
 *   <prog>.conf in the current folder */
#define PAC_CONFIG_LOCATIONS 3

/* What a settings file may be (the limits are in tunables.h). Anything else is reported and skipped (the tool still starts):
 * a file that is not a regular file (a pipe would block the tool, a device never ends), a file
 * over PAC_CONFIG_MAX_SIZE bytes, a line over PAC_CONFIG_MAX_LINE bytes, or a line with a NUL. */

/* Called for each setting; returns false for an unknown key or a bad value, which is reported
 * and skipped. */
typedef bool (*PacConfigHandler)(const char *key, const char *value, void *data);

/* One file. Returns how many lines were skipped (bad, unknown, too long or with a NUL), or -1 if
 * the file is missing or cannot be used (not a regular file, too large, not readable). A file that
 * does not exist is silent; the other reasons are reported. */
int pac_config_load_file(const char *prog, const char *path, PacConfigHandler handler, void *data);

/* A yes/no setting: yes, true, on or 1; no, false, off or 0, in any case. `out` (may be NULL) gets the value;
 * false for anything else, and `out` is then left alone. The shared keys are checked with it, and a tool reads its
 * own yes/no settings with it, so that a value the shared check accepts is one the tool accepts too. */
bool pac_config_bool(const char *value, bool *out);

/* The settings that every pac tool has in common (docs/configuration.md) can be set once, in
 * pac.conf, which is looked for in the same three places as <prog>.conf. */
#define PAC_SHARED_CONFIG "pac"

/* True for the keys that pac.conf may hold: contrast, text_scale, font, language, icons, theme,
 * speech and braille_table. */
bool pac_config_shared_key(const char *key);

/* The shared files, from the lowest priority to the highest. A key that is not a shared one, or
 * has a value that cannot be right (text_scale = big), is reported and skipped. A valid one goes to
 * the handler, whose refusal is silent: a tool does not have every shared setting, and `speech` in
 * pac.conf must not be a warning in a tool that cannot speak. */
void pac_config_load_shared(const char *prog, PacConfigHandler handler, void *data);

/* Every file there is, from the lowest priority to the highest: the shared files first, then the
 * tool's own, so that a setting in <prog>.conf, anywhere, beats the same one in pac.conf. */
void pac_config_load(const char *prog, PacConfigHandler handler, void *data);

/* The path of location `which` (0 .. PAC_CONFIG_LOCATIONS - 1, lowest priority first), for a
 * tool that reads its files itself. `sysconfdir` NULL means SYSCONFDIR. False if there is no
 * such location (no $HOME), the path does not fit, or `prog` is empty or has a '/' in it (it
 * names a file, and must not be able to name another folder). */
bool pac_config_path(const char *sysconfdir, const char *prog, int which, char *path, size_t size);

#endif
