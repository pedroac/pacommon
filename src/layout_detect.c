#define _GNU_SOURCE
#include "layout_detect.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

/* The first entry of "us,pt", or all of "us". */
static void first_of_list(const char *list, char *out, size_t size) {
    snprintf(out, size, "%.*s", (int)(strcspn(list, ",") < size - 1 ? strcspn(list, ",") : size - 1), list);
}

static bool set_detected(Layout *out, const char *layout, const char *variant) {
    memset(out, 0, sizeof(*out));
    first_of_list(layout, out->layout, sizeof(out->layout));
    first_of_list(variant, out->variant, sizeof(out->variant));
    snprintf(out->name, sizeof(out->name), "%s", out->layout);
    return out->layout[0] != '\0';
}

/* The value in quotes after the quoted key, as in:  Option "XkbLayout" "pt" */
static bool quoted_value(const char *line, const char *key, char *out, size_t size) {
    char quoted[48];
    snprintf(quoted, sizeof(quoted), "\"%s\"", key);
    const char *p = strstr(line, quoted);
    if (!p) return false;
    p += strlen(quoted);
    p = strchr(p, '"');
    if (!p) return false;
    const char *end = strchr(++p, '"');
    if (!end) return false;
    snprintf(out, size, "%.*s", (int)(end - p < (long)size - 1 ? end - p : (long)size - 1), p);
    return true;
}

bool layout_from_xorg(const char *text, Layout *out) {
    char layout[64] = "", variant[64] = "";
    while (*text) {
        size_t n = strcspn(text, "\n");
        char line[256];
        snprintf(line, sizeof(line), "%.*s", (int)(n < sizeof(line) - 1 ? n : sizeof(line) - 1), text);
        text += n + (text[n] ? 1 : 0);
        const char *hash = strchr(line, '#');
        if (hash) line[hash - line] = '\0';
        quoted_value(line, "XkbLayout", layout, sizeof(layout));
        quoted_value(line, "XkbVariant", variant, sizeof(variant));
    }
    return set_detected(out, layout, variant);
}

/* XKBLAYOUT=pt, XKBLAYOUT="pt": the value, without quotes. */
static bool shell_value(const char *line, const char *key, char *out, size_t size) {
    size_t len = strlen(key);
    while (isspace((unsigned char)*line)) line++;
    if (strncmp(line, key, len) != 0 || line[len] != '=') return false;
    line += len + 1;
    char quote = *line == '"' || *line == '\'' ? *line++ : '\0';
    size_t n = quote ? strcspn(line, (char[]){ quote, '\0' }) : strcspn(line, " \t\r#");
    snprintf(out, size, "%.*s", (int)(n < size - 1 ? n : size - 1), line);
    return true;
}

bool layout_from_shell_vars(const char *text, Layout *out) {
    char layout[64] = "", variant[64] = "";
    while (*text) {
        size_t n = strcspn(text, "\n");
        char line[256];
        snprintf(line, sizeof(line), "%.*s", (int)(n < sizeof(line) - 1 ? n : sizeof(line) - 1), text);
        text += n + (text[n] ? 1 : 0);
        shell_value(line, "XKBLAYOUT", layout, sizeof(layout));
        shell_value(line, "XKBVARIANT", variant, sizeof(variant));
    }
    return set_detected(out, layout, variant);
}

bool layout_from_console_keymap(const char *keymap, const Layout *known, int known_count, Layout *out) {
    /* The console names that are not the X11 name of the layout. */
    static const struct { const char *keymap, *layout, *variant; } renamed[] = {
        { "uk", "gb", "" }, { "dvorak", "us", "dvorak" }, { "colemak", "us", "colemak" },
    };
    for (size_t i = 0; i < sizeof(renamed) / sizeof(renamed[0]); i++)
        if (strcmp(keymap, renamed[i].keymap) == 0) return set_detected(out, renamed[i].layout, renamed[i].variant);

    /* "pt-latin9" is the Portuguese layout, in a Latin-9 encoding. */
    char layout[24];
    snprintf(layout, sizeof(layout), "%.*s", (int)strcspn(keymap, "-_.") < 23 ? (int)strcspn(keymap, "-_.") : 23, keymap);
    for (int i = 0; i < known_count; i++)
        if (strcmp(known[i].layout, layout) == 0) return set_detected(out, layout, "");
    return false;
}

/* A small system file (a few lines); size is the most any of them can be, and the rest is cut. */
static bool slurp(const char *path, char *text, size_t size) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    size_t got = fread(text, 1, size - 1, f);
    fclose(f);
    text[got] = '\0';
    return true;
}

bool layout_detect(const char *etc, const Layout *known, int known_count, Layout *out) {
    char path[512], text[8192];
    bool found = false;

    snprintf(path, sizeof(path), "%s/X11/xorg.conf.d", etc);
    DIR *dir = opendir(path);
    for (struct dirent *e; dir && !found && (e = readdir(dir));) {
        size_t len = strlen(e->d_name);
        if (len < 6 || strcmp(e->d_name + len - 5, ".conf") != 0) continue;
        char file[800];
        snprintf(file, sizeof(file), "%s/%s", path, e->d_name);
        found = slurp(file, text, sizeof(text)) && layout_from_xorg(text, out);
    }
    if (dir) closedir(dir);

    if (!found) {
        snprintf(path, sizeof(path), "%s/default/keyboard", etc);
        found = slurp(path, text, sizeof(text)) && layout_from_shell_vars(text, out);
    }
    if (!found) {
        snprintf(path, sizeof(path), "%s/vconsole.conf", etc);
        if (slurp(path, text, sizeof(text))) {
            char keymap[64] = "";
            found = layout_from_shell_vars(text, out);
            for (const char *p = text; !found && *p;) {
                size_t n = strcspn(p, "\n");
                char line[128];
                snprintf(line, sizeof(line), "%.*s", (int)(n < sizeof(line) - 1 ? n : sizeof(line) - 1), p);
                p += n + (p[n] ? 1 : 0);
                if (shell_value(line, "KEYMAP", keymap, sizeof(keymap)))
                    found = layout_from_console_keymap(keymap, known, known_count, out);
            }
        }
    }
    return found;
}
