#ifndef FIELD_H
#define FIELD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "tunables.h"


/* One character of a field, and the key that typed it: pressing key (an evdev code) with
 * the modifiers mods gives it in the layout it was typed in, and another character in
 * another layout. key is 0 for a character that came from nowhere in particular (a compose
 * sequence, or text loaded from disk), which stays as it is. */
typedef struct {
    uint32_t key, mods;
    unsigned char len;     /* bytes in the text */
} Typed;

/* A line of UTF-8 text that is only edited at its end. */
typedef struct {
    char text[FIELD_MAX];
    size_t len;
    Typed typed[FIELD_MAX];
    size_t count;          /* characters */
} Field;

/* Wipes the memory too: a field may have held a password. */
void field_clear(Field *f);
/* Returns false, leaving the field unchanged, when the text does not fit. */
bool field_append(Field *f, const char *utf8);
/* The same for text a key typed, so that it can be typed again with another layout. */
bool field_append_key(Field *f, const char *utf8, uint32_t key, uint32_t mods);
void field_backspace(Field *f);
/* Number of characters (code points), not bytes. */
size_t field_chars(const Field *f);

/* What a key types now: writes it to out and returns false if it types nothing. */
typedef bool (*FieldTranslate)(uint32_t key, uint32_t mods, char *out, size_t size, void *data);
/* Types the keys of the field again, with whatever translate says they type now. Returns
 * whether that changed the text. */
bool field_retype(Field *f, FieldTranslate translate, void *data);

#endif
