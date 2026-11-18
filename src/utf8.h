#ifndef UTF8_H
#define UTF8_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Decodes one strictly valid UTF-8 sequence (no overlongs, surrogates or code points
 * above U+10FFFF). Returns its length in bytes and stores the code point, or returns 0
 * if s does not start with a valid sequence. s must be NUL-terminated. */
int utf8_decode(const char *s, uint32_t *cp);

/* Whether the n bytes of s are valid UTF-8 (as utf8_decode takes it) and have no control character (below U+0020, or
 * DEL): text that is safe to put in a one line field. Used for what comes from outside, such as an input method. */
bool utf8_plain(const char *s, size_t n);

#endif
