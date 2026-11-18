#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "layout.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef NO_I18N
#include <libintl.h>
#endif

#ifdef NO_I18N
const char *pac_layout_name(const char *name) { return name; }
#else
const char *pac_layout_name(const char *name) {
    static bool bound;
    static const char *const domains[] = { "xkeyboard-config-2", "xkeyboard-config" };
    if (!bound) {
        for (int i = 0; i < 2; i++) bind_textdomain_codeset(domains[i], "UTF-8");
        bound = true;
    }
    for (int i = 0; i < 2; i++) {
        const char *text = dgettext(domains[i], name);
        if (text != name) return text;
    }
    return name;
}
#endif

static const char *const LIST_PATHS[] = {
    "/usr/share/X11/xkb/rules/evdev.lst",
    "/usr/share/xkeyboard-config-2/rules/evdev.lst",
    NULL,
};

int layouts_parse_list(const char *text, Layout *out, int max) {
    int count = 0;
    bool in_section = false;

    while (*text && count < max) {
        char line[256];
        size_t n = strcspn(text, "\n");
        snprintf(line, sizeof(line), "%.*s", (int)(n < sizeof(line) - 1 ? n : sizeof(line) - 1), text);
        text += n + (text[n] ? 1 : 0);

        if (line[0] == '!') {
            in_section = strncmp(line, "! layout", 8) == 0 && !isalnum((unsigned char)line[8]);
            continue;
        }
        if (!in_section) continue;

        char *code = line;
        while (isspace((unsigned char)*code)) code++;
        if (!*code) continue;
        char *end = code;
        while (*end && !isspace((unsigned char)*end)) end++;
        char *name = end;
        while (isspace((unsigned char)*name)) name++;
        *end = '\0';

        /* "custom" stands for a layout the user writes: there is none to type with. */
        if (strcmp(code, "custom") == 0) continue;

        Layout *l = &out[count++];
        memset(l, 0, sizeof(*l));
        snprintf(l->layout, sizeof(l->layout), "%.*s", (int)sizeof(l->layout) - 1, code);
        snprintf(l->name, sizeof(l->name), "%.*s", (int)sizeof(l->name) - 1, *name ? pac_layout_name(name) : code);
    }
    return count;
}

bool layout_parse_spec(const char *spec, Layout *out) {
    memset(out, 0, sizeof(*out));
    while (isspace((unsigned char)*spec)) spec++;
    size_t len = strcspn(spec, "( \t");
    if (len == 0 || len >= sizeof(out->layout)) return false;
    snprintf(out->layout, sizeof(out->layout), "%.*s", (int)len, spec);

    const char *rest = spec + len;
    while (*rest == ' ' || *rest == '\t') rest++;      /* "us (intl)" is "us(intl)" */
    const char *end = spec + len;                       /* where what names the layout ends */
    if (*rest == '(') {
        const char *close = strchr(rest, ')');
        if (!close || close - rest - 1 <= 0 || (size_t)(close - rest - 1) >= sizeof(out->variant))
            return false;
        snprintf(out->variant, sizeof(out->variant), "%.*s", (int)(close - rest - 1), rest + 1);
        end = close + 1;
    }
    int shown = (int)(end - spec);
    if (shown > (int)sizeof(out->name) - 1) shown = (int)sizeof(out->name) - 1;
    snprintf(out->name, sizeof(out->name), "%.*s", shown, spec);
    return true;
}

void layout_spec(const Layout *layout, char *out, int size) {
    if (layout->variant[0]) snprintf(out, size, "%s(%s)", layout->layout, layout->variant);
    else snprintf(out, size, "%s", layout->layout);
}

static char *read_list(void) {
    for (int i = 0; LIST_PATHS[i]; i++) {
        int fd = open(LIST_PATHS[i], O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        struct stat st;
        if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
            st.st_size > LAYOUT_LIST_MAX) {
            close(fd);
            continue;
        }
        /* The rules list is read whole, once, at start-up: its size is known only now (LAYOUT_LIST_MAX bounds
         * it), and a static block of that size would stay resident for the whole run. The caller frees it. */
        char *text = malloc((size_t)st.st_size + 1);
        if (!text) { close(fd); return NULL; }
        size_t got = 0;
        while (got < (size_t)st.st_size) {
            ssize_t n = read(fd, text + got, (size_t)st.st_size - got);
            if (n <= 0) break;
            got += (size_t)n;
        }
        close(fd);
        text[got] = '\0';
        return text;
    }
    return NULL;
}

int layouts_load(Layout *out, int max, const char *configured) {
    char *list = read_list();
    int count = 0;

    if (!configured || !*configured) {
        if (list) count = layouts_parse_list(list, out, max);
        free(list);
        return count;
    }

    /* Described like the list describes them, with the variant after the name. */
    Layout known[MAX_LAYOUTS];
    int known_count = list ? layouts_parse_list(list, known, MAX_LAYOUTS) : 0;
    free(list);

    char copy[512];
    snprintf(copy, sizeof(copy), "%s", configured);
    for (char *save, *tok = strtok_r(copy, ",", &save); tok && count < max;
         tok = strtok_r(NULL, ",", &save)) {
        Layout l;
        if (!layout_parse_spec(tok, &l)) continue;
        for (int i = 0; i < known_count; i++) {
            if (strcmp(known[i].layout, l.layout) != 0) continue;
            if (l.variant[0]) snprintf(l.name, sizeof(l.name), "%s (%s)", known[i].name, l.variant);
            else snprintf(l.name, sizeof(l.name), "%s", known[i].name);
            break;
        }
        out[count++] = l;
    }
    return count;
}

/* ---- picker ---- */

bool picker_matches(const LayoutPicker *p, int i) {
    const Layout *l = &p->items[i];
    if (i > 0 && l->unavailable) return false;
    if (!p->filter.len) return true;
    /* The compositor's own layout (0) has no name to match. */
    return i > 0 && (strcasestr(l->name, p->filter.text) || strcasestr(l->layout, p->filter.text));
}

int picker_match_count(const LayoutPicker *p) {
    int n = 0;
    for (int i = 1; i < p->count; i++) n += picker_matches(p, i);
    return n;
}

int picker_neighbour(const LayoutPicker *p, int from, int dir) {
    for (int i = 1; i < p->count; i++) {
        int next = (from + dir * i + p->count * i) % p->count;
        if (picker_matches(p, next)) return next;
    }
    return -1;
}

int picker_step(LayoutPicker *p, int dir) {
    if (p->filter.len) {
        int next = p->pick ? picker_neighbour(p, p->pick, dir) : -1;
        if (next > 0) p->pick = next;
        return -1;
    }
    return picker_neighbour(p, p->current, dir);
}

void picker_filter_changed(LayoutPicker *p) {
    int found = picker_neighbour(p, 0, 1);
    p->pick = found > 0 ? found : 0;
}

int picker_settle(LayoutPicker *p) {
    int apply = p->filter.len && p->pick ? p->pick : -1;
    picker_cancel(p);
    return apply;
}

void picker_cancel(LayoutPicker *p) {
    field_clear(&p->filter);
    p->pick = 0;
}
