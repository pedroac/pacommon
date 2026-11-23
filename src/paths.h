#ifndef PACOMMON_PATHS_H
#define PACOMMON_PATHS_H

#include <stdbool.h>
#include <stddef.h>

/* Where things are, from the variables that locate files (principle 19). Only absolute paths are
 * used: a relative $XDG_DATA_HOME is ignored. */
#define PATHS_MAX 16
#define PATHS_LEN 256

typedef struct {
    char dir[PATHS_MAX][PATHS_LEN];
    int count;
} PathList;

/* $XDG_DATA_HOME (or ~/.local/share) first, then $XDG_DATA_DIRS (or /usr/local/share:/usr/share). */
void paths_data(PathList *list);

/* The folder of the person's own state: $XDG_STATE_HOME or ~/.local/state. False if there is none. */
bool paths_state(char *out, size_t size);

/* The person's folders from user-dirs.dirs: key is "DOWNLOAD", "DOCUMENTS"... Returns false if
 * the folder is not known or does not exist. */
bool paths_user_dir(const char *key, char *out, size_t size);

/* $XDG_CONFIG_HOME or ~/.config, the person's settings. False if there is none. */
bool paths_config(char *out, size_t size);

/* The home folder from $HOME, if it is an absolute path. */
bool paths_home(char *out, size_t size);

#endif
