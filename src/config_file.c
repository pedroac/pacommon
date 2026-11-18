#include "config_file.h"
#include "length.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

/* A tool that translates its messages names its i18n.h here (-DPACOMMON_I18N='"i18n.h"'). */
#ifdef PACOMMON_I18N
#include PACOMMON_I18N
#else
#define _(s) (s)
#endif

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

/* What is shown of text from a file: no control characters (an escape sequence in it could
 * otherwise rewrite the terminal it is reported on), and at most size - 1 bytes. */
static const char *shown(const char *s, char *out, size_t size) {
    size_t i = 0;
    for (; s[i] && i < size - 1; i++)
        out[i] = (unsigned char)s[i] < 0x20 || s[i] == 0x7f ? '?' : s[i];
    out[i] = '\0';
    if (s[i] && size > 4) memcpy(out + size - 4, "...", 4);
    return out;
}

enum { LINE_OK, LINE_LONG, LINE_NUL, LINE_END };

/* Reads one line into line, without its newline. A line that does not fit, or that has a NUL in it
 * (which would end it early for every string function), is read to its end all the same, so that
 * what follows is not taken for part of it. `bytes` counts everything read. */
static int read_line(FILE *f, char *line, size_t size, size_t *bytes) {
    size_t len = 0;
    int state = LINE_OK, c;
    while ((c = getc(f)) != EOF) {
        (*bytes)++;
        if (c == '\n') break;
        if (c == '\0') state = LINE_NUL;
        else if (len < size - 1) line[len++] = (char)c;
        else if (state == LINE_OK) state = LINE_LONG;
    }
    if (c == EOF && len == 0 && state == LINE_OK) return LINE_END;
    line[len] = '\0';
    return state;
}

int pac_config_load_file(const char *prog, const char *path, PacConfigHandler handler, void *data) {
    if (!prog || !path || !handler) return -1;
    char name[80];

    /* O_NONBLOCK so that opening a pipe cannot block; the type is checked before anything is read. */
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) {
        if (errno != ENOENT && errno != ENOTDIR)
            fprintf(stderr, _("%s: %s: cannot be read: %s\n"), prog, shown(path, name, sizeof name), strerror(errno));
        return -1;
    }
    struct stat st;
    const char *problem = NULL;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) problem = _("not a regular file");
    else if (st.st_size > PAC_CONFIG_MAX_SIZE) problem = _("too large");
    FILE *f = problem ? NULL : fdopen(fd, "r");
    if (!f) {
        if (!problem) problem = _("cannot be read");
        fprintf(stderr, _("%s: %s: %s, ignored\n"), prog, shown(path, name, sizeof name), problem);
        close(fd);
        return -1;
    }

    int skipped = 0;
    size_t bytes = 0;
    char line[PAC_CONFIG_MAX_LINE + 1];
    for (int n = 1; bytes <= PAC_CONFIG_MAX_SIZE; n++) {
        int state = read_line(f, line, sizeof line, &bytes);
        if (state == LINE_END) break;
        if (state != LINE_OK) {
            fprintf(stderr, state == LINE_LONG ? _("%s: %s:%d: line too long, ignored\n")
                                               : _("%s: %s:%d: line has a NUL byte, ignored\n"),
                    prog, shown(path, name, sizeof name), n);
            skipped++;
            continue;
        }
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        char *text = trim(line);
        if (!*text) continue;

        char *eq = strchr(text, '=');
        if (!eq) {
            fprintf(stderr, _("%s: %s:%d: expected \"key = value\"\n"), prog, shown(path, name, sizeof name), n);
            skipped++;
            continue;
        }
        *eq = '\0';
        char *key = trim(text), *value = trim(eq + 1);
        if (!handler(key, value, data)) {
            char k[40], v[80];
            fprintf(stderr, _("%s: %s:%d: ignoring \"%s = %s\"\n"), prog, shown(path, name, sizeof name), n,
                    shown(key, k, sizeof k), shown(value, v, sizeof v));
            skipped++;
        }
    }
    if (bytes > PAC_CONFIG_MAX_SIZE)   /* it grew while it was being read */
        fprintf(stderr, _("%s: %s: too large, the rest is ignored\n"), prog, shown(path, name, sizeof name));
    fclose(f);
    return skipped;
}

bool pac_config_path(const char *sysconfdir, const char *prog, int which, char *path, size_t size) {
    if (!prog || !*prog || strchr(prog, '/') || !path || size == 0) return false;
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    int len;
    if (which == 0) len = snprintf(path, size, "%s/pac/%s.conf", sysconfdir ? sysconfdir : SYSCONFDIR, prog);
    else if (which == 1 && xdg && *xdg) len = snprintf(path, size, "%s/pac/%s.conf", xdg, prog);
    else if (which == 1 && home && *home) len = snprintf(path, size, "%s/.config/pac/%s.conf", home, prog);
    else if (which == 2) len = snprintf(path, size, "%s.conf", prog);
    else return false;
    return len > 0 && (size_t)len < size;
}

static const char *const shared_keys[] = {
    "contrast", "text_scale", "text_size_min", "text_size_max", "font", "language", "icons", "theme", "speech",
    "resources", "privacy", "braille_table", "reduce_motion",
};

bool pac_config_shared_key(const char *key) {
    if (!key) return false;
    for (size_t i = 0; i < sizeof shared_keys / sizeof shared_keys[0]; i++)
        if (strcmp(key, shared_keys[i]) == 0) return true;
    return false;
}

bool pac_config_bool(const char *value, bool *out) {
    static const struct { const char *word; bool value; } words[] = {
        { "yes", true }, { "true", true }, { "on", true }, { "1", true },
        { "no", false }, { "false", false }, { "off", false }, { "0", false },
    };
    for (size_t i = 0; i < sizeof words / sizeof words[0]; i++)
        if (strcasecmp(value, words[i].word) == 0) {
            if (out) *out = words[i].value;
            return true;
        }
    return false;
}

/* The shared keys have the same meaning in every tool, so what is a nonsense value is known here. */
static bool shared_value_ok(const char *key, const char *value) {
    /* contrast is normal or high: the value every tool takes (docs/configuration.md). */
    if (strcmp(key, "contrast") == 0)
        return strcmp(value, "normal") == 0 || strcmp(value, "high") == 0;
    if (strcmp(key, "text_size_min") == 0 || strcmp(key, "text_size_max") == 0) {
        PacLength length;
        return pac_length_parse(value, &length);
    }
    /* The levels, in order (resources has three, privacy four): each tool says what they do. */
    if (strcmp(key, "resources") == 0)
        return strcmp(value, "normal") == 0 || strcmp(value, "low") == 0 || strcmp(value, "minimal") == 0;
    if (strcmp(key, "privacy") == 0)
        return strcmp(value, "normal") == 0 || strcmp(value, "medium") == 0 || strcmp(value, "high") == 0 ||
               strcmp(value, "paranoid") == 0;
    if (strcmp(key, "speech") == 0 || strcmp(key, "reduce_motion") == 0) return pac_config_bool(value, NULL);
    if (strcmp(key, "text_scale") == 0) {
        double scale;
        return pac_text_scale_parse(value, &scale);
    }
    return *value != '\0';   /* a font, language, icon set, theme or braille table has a name */
}

struct shared {
    PacConfigHandler handler;
    void *data;
};

static bool shared_handler(const char *key, const char *value, void *data) {
    const struct shared *s = data;
    if (!pac_config_shared_key(key) || !shared_value_ok(key, value)) return false;
    s->handler(key, value, s->data);   /* a tool without this setting refuses it: not an error */
    return true;
}

void pac_config_load_shared_file(const char *prog, const char *path, PacConfigHandler handler, void *data) {
    if (!handler) return;
    struct shared shared = { handler, data };
    pac_config_load_file(prog, path, shared_handler, &shared);
}

void pac_config_load_shared_locations(const char *prog, int count, PacConfigHandler handler, void *data) {
    char path[PAC_CONFIG_PATH_MAX];
    for (int which = 0; which < count && which < PAC_CONFIG_LOCATIONS; which++)
        if (pac_config_path(NULL, PAC_SHARED_CONFIG, which, path, sizeof path))
            pac_config_load_shared_file(prog, path, handler, data);
}

void pac_config_load_shared(const char *prog, PacConfigHandler handler, void *data) {
    pac_config_load_shared_locations(prog, PAC_CONFIG_LOCATIONS, handler, data);
}

void pac_config_load(const char *prog, PacConfigHandler handler, void *data) {
    pac_config_load_shared(prog, handler, data);
    char path[PAC_CONFIG_PATH_MAX];
    for (int which = 0; which < PAC_CONFIG_LOCATIONS; which++)
        if (pac_config_path(NULL, prog, which, path, sizeof path)) pac_config_load_file(prog, path, handler, data);
}
