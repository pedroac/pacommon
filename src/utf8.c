#include "utf8.h"
#include <string.h>

int utf8_decode(const char *s, uint32_t *cp) {
    const unsigned char *p = (const unsigned char *)s;
    int len;
    uint32_t c, min;

    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    else if ((p[0] & 0xE0) == 0xC0) { len = 2; c = p[0] & 0x1F; min = 0x80; }
    else if ((p[0] & 0xF0) == 0xE0) { len = 3; c = p[0] & 0x0F; min = 0x800; }
    else if ((p[0] & 0xF8) == 0xF0) { len = 4; c = p[0] & 0x07; min = 0x10000; }
    else return 0;

    for (int i = 1; i < len; i++) {
        if ((p[i] & 0xC0) != 0x80) return 0;
        c = (c << 6) | (p[i] & 0x3F);
    }
    if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0;
    *cp = c;
    return len;
}

bool utf8_plain(const char *s, size_t n) {
    char copy[8];
    for (size_t i = 0; i < n;) {
        /* utf8_decode reads up to a NUL: look at a short copy that ends, so that it never reads past n. */
        size_t take = n - i < 4 ? n - i : 4;
        memcpy(copy, s + i, take);
        copy[take] = '\0';
        uint32_t cp;
        int c = utf8_decode(copy, &cp);
        if (c <= 0 || (size_t)c > take || cp < 0x20 || cp == 0x7f) return false;
        i += (size_t)c;
    }
    return true;
}
