#define _GNU_SOURCE
#include "fontfind.h"
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Only absolute paths: a relative one would search whatever folder the tool was started in. */
static void add_dir(FontDirs *dirs, int *count, const char *base, const char *suffix) {
    if (*count >= FONT_MAX_DIRS || base[0] != '/') return;
    int n = snprintf(dirs->storage[*count], sizeof(dirs->storage[0]), "%s%s", base, suffix);
    if (n < 0 || n >= (int)sizeof(dirs->storage[0])) return;
    dirs->list[*count] = dirs->storage[*count];
    (*count)++;
}

void font_dirs_init(FontDirs *dirs) {
    memset(dirs, 0, sizeof(*dirs));
    int count = 0;

    const char *home = getenv("HOME");
    const char *data_home = getenv("XDG_DATA_HOME");
    if (data_home && *data_home == '/') add_dir(dirs, &count, data_home, "/fonts");
    else if (home) add_dir(dirs, &count, home, "/.local/share/fonts");
    if (home) add_dir(dirs, &count, home, "/.fonts");

    const char *data_dirs = getenv("XDG_DATA_DIRS");
    if (!data_dirs || !*data_dirs) data_dirs = "/usr/local/share:/usr/share";
    while (*data_dirs) {
        size_t len = strcspn(data_dirs, ":");
        if (len > 0 && len < FONT_DIR_MAX) {
            char base[FONT_DIR_MAX];
            memcpy(base, data_dirs, len);
            base[len] = '\0';
            add_dir(dirs, &count, base, "/fonts");
        }
        data_dirs += len + (data_dirs[len] ? 1 : 0);
    }
}

/* nftw() takes no user data, so the search state is static. */
static const char *const *search_names;
static int best_rank;
static char best_path[FONT_PATH_MAX];

static int visit(const char *path, const struct stat *sb, int type, struct FTW *ftw) {
    (void)sb;
    if (type != FTW_F) return 0;

    const char *base = path + ftw->base;
    for (int rank = 0; search_names[rank] && rank < best_rank; rank++) {
        if (strcmp(base, search_names[rank]) != 0 || access(path, R_OK) != 0) continue;
        if (snprintf(best_path, sizeof(best_path), "%s", path) >= (int)sizeof(best_path)) continue;
        best_rank = rank;
        return rank == 0;   /* nothing can beat the first choice: stop searching */
    }
    return 0;
}

bool font_find(const char *const *dirs, const char *const *names, char *path, size_t size) {
    if (!dirs || !names || !path || size == 0) return false;
    int count = 0;
    while (names[count]) count++;

    search_names = names;
    best_rank = count;
    best_path[0] = '\0';
    /* Follow symlinks: font directories are often links (package stores), and nftw() skips
     * directories it has already visited, so a loop cannot recurse forever. */
    for (int i = 0; dirs[i] && best_rank > 0; i++) nftw(dirs[i], visit, FONT_WALK_FDS, 0);
    search_names = NULL;   /* do not keep a pointer to the caller's array */

    if (best_rank == count) return false;
    int n = snprintf(path, size, "%s", best_path);
    return n > 0 && n < (int)size;
}
