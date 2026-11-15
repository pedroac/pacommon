#define _DEFAULT_SOURCE
#include "field.h"
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
