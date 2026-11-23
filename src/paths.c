#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *absolute_env(const char *name) {
    const char *v = getenv(name);
    return v && v[0] == '/' ? v : NULL;
}

bool paths_home(char *out, size_t size) {
    const char *home = absolute_env("HOME");
    return home && snprintf(out, size, "%s", home) < (int)size;
}

static void add(PathList *list, const char *dir, size_t len) {
    if (len == 0 || dir[0] != '/' || len >= PATHS_LEN || list->count >= PATHS_MAX) return;
    while (len > 1 && dir[len - 1] == '/') len--;
    memcpy(list->dir[list->count], dir, len);
    list->dir[list->count][len] = '\0';
    list->count++;
}

void paths_data(PathList *list) {
    list->count = 0;
    const char *data_home = absolute_env("XDG_DATA_HOME");
    char home[PATHS_LEN], fallback[PATHS_LEN];
    if (data_home) {
        add(list, data_home, strlen(data_home));
    } else if (paths_home(home, sizeof home) && snprintf(fallback, sizeof fallback, "%s/.local/share", home) < (int)sizeof fallback) {
        add(list, fallback, strlen(fallback));
    }
    const char *dirs = getenv("XDG_DATA_DIRS");
    if (!dirs || !dirs[0]) dirs = "/usr/local/share:/usr/share";
    for (const char *p = dirs; *p;) {
        size_t len = strcspn(p, ":");
        add(list, p, len);
        p += len + (p[len] ? 1 : 0);
    }
}

bool paths_config(char *out, size_t size) {
    const char *config = absolute_env("XDG_CONFIG_HOME");
    if (config) return snprintf(out, size, "%s", config) < (int)size;
    char home[PATHS_LEN];
    return paths_home(home, sizeof home) && snprintf(out, size, "%s/.config", home) < (int)size;
}

bool paths_state(char *out, size_t size) {
    const char *state = absolute_env("XDG_STATE_HOME");
    if (state) return snprintf(out, size, "%s", state) < (int)size;
    char home[PATHS_LEN];
    return paths_home(home, sizeof home) && snprintf(out, size, "%s/.local/state", home) < (int)size;
}

static bool is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* "$HOME/Downloads" or "/data/Downloads", as user-dirs.dirs writes them, inside quotes. */
static bool expand(const char *value, const char *home, char *out, size_t size) {
    if (strncmp(value, "$HOME", 5) == 0 && (value[5] == '/' || value[5] == '\0'))
        return snprintf(out, size, "%s%s", home, value + 5) < (int)size;
    return value[0] == '/' && snprintf(out, size, "%s", value) < (int)size;
}

bool paths_user_dir(const char *key, char *out, size_t size) {
    char home[PATHS_LEN], config[PATHS_LEN], file[PATHS_LEN + 32];
    if (!paths_home(home, sizeof home) || !paths_config(config, sizeof config)) return false;
    snprintf(file, sizeof file, "%s/user-dirs.dirs", config);

    char want[48];
    snprintf(want, sizeof want, "XDG_%s_DIR=", key);
    bool found = false;
    FILE *f = fopen(file, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof line, f)) {
            if (strncmp(line, want, strlen(want)) != 0) continue;
            char *v = line + strlen(want);
            if (*v == '"') v++;
            v[strcspn(v, "\"\n")] = '\0';
            found = expand(v, home, out, size);
        }
        fclose(f);
    }
    if (!found) {   /* no file, or no such line: the folders' English names, if they are there */
        static const struct { const char *key, *name; } plain[] = {
            { "DESKTOP", "Desktop" }, { "DOWNLOAD", "Downloads" }, { "DOCUMENTS", "Documents" },
            { "MUSIC", "Music" }, { "PICTURES", "Pictures" }, { "VIDEOS", "Videos" },
        };
        for (size_t i = 0; i < sizeof plain / sizeof *plain; i++)
            if (strcmp(plain[i].key, key) == 0 && snprintf(out, size, "%s/%s", home, plain[i].name) < (int)size) found = true;
    }
    for (size_t n = strlen(out); n > 1 && out[n - 1] == '/'; ) out[--n] = '\0';    /* "$HOME/" is $HOME */
    return found && is_dir(out) && strcmp(out, home) != 0;   /* user-dirs.dirs says $HOME for "none" */
}
