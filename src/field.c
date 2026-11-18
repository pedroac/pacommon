#define _DEFAULT_SOURCE
#include "field.h"
#include "utf8.h"
#include <string.h>

void field_clear(Field *f) {
    explicit_bzero(f, sizeof(*f));
}

/* Length of the well-formed UTF-8 sequence at s (at most max bytes), or 0 if there is none. */
static size_t utf8_len(const char *s, size_t max) {
    const unsigned char *u = (const unsigned char *)s;
    if (max == 0) return 0;

    size_t n = 0;
    unsigned char lo = 0x80, hi = 0xBF;
    if (u[0] < 0x80) return 1;
    else if (u[0] >= 0xC2 && u[0] <= 0xDF) n = 2;
    else if (u[0] == 0xE0) { n = 3; lo = 0xA0; }
    else if (u[0] >= 0xE1 && u[0] <= 0xEC) n = 3;
    else if (u[0] == 0xED) { n = 3; hi = 0x9F; }
    else if (u[0] >= 0xEE && u[0] <= 0xEF) n = 3;
    else if (u[0] == 0xF0) { n = 4; lo = 0x90; }
    else if (u[0] >= 0xF1 && u[0] <= 0xF3) n = 4;
    else if (u[0] == 0xF4) { n = 4; hi = 0x8F; }
    else return 0;

    if (max < n) return 0;
    if (u[1] < lo || u[1] > hi) return 0;
    for (size_t i = 2; i < n; i++)
        if (u[i] < 0x80 || u[i] > 0xBF) return 0;
    return n;
}

/* The invariants every function keeps and relies on. A field that breaks them (it was written to
 * directly, or is uninitialized memory) is wiped rather than trusted. */
static bool sane(const Field *f) {
    return f->len < FIELD_MAX && f->count <= f->len && f->text[f->len] == '\0';
}

bool field_append_key(Field *f, const char *utf8, uint32_t key, uint32_t mods) {
    if (!utf8) return false;
    if (!sane(f)) field_clear(f);
    size_t n = strlen(utf8);
    if (n >= FIELD_MAX - f->len) return false;   /* written so that it cannot overflow */

    /* A key types one character; anything longer is kept as plain characters. */
    bool single = n > 0 && utf8_len(utf8, n) == n;
    for (size_t i = 0; i < n;) {
        size_t c = utf8_len(utf8 + i, n - i);
        if (c == 0) c = 1;   /* not UTF-8: keep the byte as it is */
        f->typed[f->count++] = (Typed){ single ? key : 0, single ? mods : 0, (unsigned char)c };
        i += c;
    }
    memcpy(f->text + f->len, utf8, n + 1);
    f->len += n;
    return true;
}

bool field_append(Field *f, const char *utf8) {
    return field_append_key(f, utf8, 0, 0);
}

void field_backspace(Field *f) {
    if (!sane(f)) field_clear(f);
    if (f->count == 0) return;
    f->len -= f->typed[--f->count].len;
    explicit_bzero(f->text + f->len, FIELD_MAX - f->len);
    explicit_bzero(&f->typed[f->count], sizeof(Typed));
}

void field_clear_preedit(Field *f) {
    explicit_bzero(f->preedit, sizeof(f->preedit));
    f->preedit_len = 0;
    f->preedit_begin = f->preedit_end = -1;
}

const char *field_preedit(const Field *f) {
    return f->preedit_len < FIELD_PREEDIT_MAX && f->preedit[f->preedit_len] == '\0' ? f->preedit : "";
}

/* Whether byte offset `at` of s (n bytes) is the start of a character, or the end. */
static bool on_boundary(const char *s, size_t n, size_t at) {
    return at == n || (at < n && ((unsigned char)s[at] & 0xC0) != 0x80);
}

bool field_set_preedit(Field *f, const char *utf8, int begin, int end) {
    field_clear_preedit(f);
    if (!utf8 || !utf8[0]) return true;
    size_t n = strlen(utf8);
    if (n > FIELD_PREEDIT_MAX - 1) {                    /* cut at a character, the last that fits */
        n = FIELD_PREEDIT_MAX - 1;
        while (n > 0 && !on_boundary(utf8, strlen(utf8), n)) n--;
    }
    if (n == 0 || !utf8_plain(utf8, n)) return n == 0;
    memcpy(f->preedit, utf8, n);
    f->preedit_len = n;
    bool ok = begin >= 0 && end >= begin && (size_t)end <= n && on_boundary(f->preedit, n, (size_t)begin) &&
              on_boundary(f->preedit, n, (size_t)end);
    f->preedit_begin = ok ? begin : -1;
    f->preedit_end = ok ? end : -1;
    return true;
}

size_t field_delete_before(Field *f, size_t bytes) {
    if (!sane(f)) field_clear(f);
    size_t removed = 0;
    while (f->count > 0 && removed + f->typed[f->count - 1].len <= bytes) {
        removed += f->typed[f->count - 1].len;
        field_backspace(f);
    }
    return removed;
}

size_t field_chars(const Field *f) {
    return f->count;
}

bool field_retype(Field *f, FieldTranslate translate, void *data) {
    if (!translate) return false;
    if (!sane(f)) {
        field_clear(f);
        return false;
    }
    char text[FIELD_MAX] = "";
    Typed typed[FIELD_MAX];
    size_t len = 0, count = 0, old = 0;

    for (size_t i = 0; i < f->count; i++) {
        char now[16] = "";
        const char *piece = f->text + old;
        size_t n = f->typed[i].len;
        if (n == 0 || n > f->len - old) {   /* the lengths do not add up to the text */
            explicit_bzero(text, sizeof(text));
            explicit_bzero(typed, sizeof(typed));
            field_clear(f);
            return false;
        }
        if (f->typed[i].key && translate(f->typed[i].key, f->typed[i].mods, now, sizeof(now), data)) {
            now[sizeof(now) - 1] = '\0';   /* a callback that does not end its text */
            size_t m = strlen(now);
            if (m > 0 && (unsigned char)now[0] >= 0x20 && (unsigned char)now[0] != 0x7f &&
                utf8_len(now, m) == m) {
                piece = now;
                n = m;
            }
        }
        if (n >= FIELD_MAX - len) {   /* does not fit: keep the field as it is */
            explicit_bzero(text, sizeof(text));
            explicit_bzero(typed, sizeof(typed));
            return false;
        }
        memcpy(text + len, piece, n);
        typed[count] = f->typed[i];
        typed[count].len = n;
        len += n;
        count++;
        old += f->typed[i].len;
    }
    bool changed = len != f->len || memcmp(text, f->text, len) != 0;
    explicit_bzero(f->text, FIELD_MAX);
    memcpy(f->text, text, len);
    memcpy(f->typed, typed, count * sizeof(Typed));
    f->len = len;
    f->count = count;
    explicit_bzero(text, sizeof(text));
    explicit_bzero(typed, sizeof(typed));
    return changed;
}

/* X11 keysyms (xkbcommon-keysyms.h, fixed by the protocol), written out so that this file needs no library. */
#define KEYSYM_BACKSPACE 0xff08
#define KEYSYM_U_LOWER 0x75
#define KEYSYM_U_UPPER 0x55

FieldKey field_edit_key(Field *f, const FieldKeyEvent *e, size_t max) {
    if (!sane(f)) field_clear(f);
    size_t before = f->len;
    bool ctrl = e->held & FIELD_CTRL;

    if (e->keysym == KEYSYM_BACKSPACE || (ctrl && (e->keysym == KEYSYM_U_LOWER || e->keysym == KEYSYM_U_UPPER))) {
        if (e->keysym != KEYSYM_BACKSPACE || ctrl) field_clear(f);
        else field_backspace(f);
        return f->len != before ? FIELD_KEY_EDITED : FIELD_KEY_IGNORED;
    }

    if (e->held & (FIELD_CTRL | FIELD_ALT | FIELD_LOGO)) return FIELD_KEY_IGNORED;
    if (!e->text || (unsigned char)e->text[0] < 0x20 || (unsigned char)e->text[0] == 0x7f) return FIELD_KEY_IGNORED;
    if (max == 0 || max > FIELD_MAX - 1) max = FIELD_MAX - 1;
    if (f->len > max || strlen(e->text) > max - f->len) return FIELD_KEY_FULL;
    return field_append_key(f, e->text, e->key, e->mods) ? FIELD_KEY_EDITED : FIELD_KEY_FULL;
}
