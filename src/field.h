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
    char preedit[FIELD_PREEDIT_MAX];   /* what an input method is composing: shown after the text, not part of it */
    size_t preedit_len;
    int preedit_begin, preedit_end;    /* the cursor in it, in bytes; -1 for both: none shown (only meant while there is a preedit) */
} Field;

/* Wipes the memory too: a field may have held a password. */
void field_clear(Field *f);
/* Returns false, leaving the field unchanged, when the text does not fit. */
bool field_append(Field *f, const char *utf8);
/* The same for text a key typed, so that it can be typed again with another layout. */
bool field_append_key(Field *f, const char *utf8, uint32_t key, uint32_t mods);
void field_backspace(Field *f);
/* What an input method is composing (a preedit): shown with the text, underlined, and not part of it until it is
 * committed with field_append. `begin` and `end` are the cursor in it, in bytes (equal: a bar; different: a
 * highlighted part), or -1 and -1 for none shown. Returns false, leaving no preedit, if utf8 is not valid or has a
 * control character; one that is longer than FIELD_PREEDIT_MAX - 1 bytes is cut at a character; an empty one clears.
 * A cursor that is out of the text, backwards or inside a character is none. */
bool field_set_preedit(Field *f, const char *utf8, int begin, int end);
void field_clear_preedit(Field *f);
/* The preedit, "" if there is none. */
const char *field_preedit(const Field *f);
/* Takes at most `bytes` bytes off the end of the text, whole characters only. Returns how many bytes it took. */
size_t field_delete_before(Field *f, size_t bytes);
/* Number of characters (code points), not bytes. */
size_t field_chars(const Field *f);

/* What a key types now: writes it to out and returns false if it types nothing. */
typedef bool (*FieldTranslate)(uint32_t key, uint32_t mods, char *out, size_t size, void *data);
/* Types the keys of the field again, with whatever translate says they type now. Returns
 * whether that changed the text. */
bool field_retype(Field *f, FieldTranslate translate, void *data);

/* ---- editing keys ---- */

/* Modifiers that change what a key means to a field (a set of these is FieldKeyEvent.held). */
enum { FIELD_CTRL = 1, FIELD_ALT = 2, FIELD_LOGO = 4 };

/* One key press, as a tool's keyboard code has it. keysym is an X11 keysym (xkbcommon's), text
 * what the key types with compose and dead keys applied (UTF-8, may be NULL), and key and mods the
 * evdev code and xkb modifier mask that field_retype wants (0 if the field is never retyped). */
typedef struct {
    uint32_t keysym;
    const char *text;
    uint32_t key, mods;
    unsigned held;
} FieldKeyEvent;

typedef enum {
    FIELD_KEY_IGNORED,   /* not an editing key, or it changed nothing (Backspace in an empty field) */
    FIELD_KEY_EDITED,    /* the text changed */
    FIELD_KEY_FULL       /* it is text and it does not fit: the field is as it was */
} FieldKey;

/* The one set of editing rules, so that every field of every tool behaves the same: Backspace
 * deletes a character, Ctrl+Backspace and Ctrl+U clear the field, and text is appended unless
 * Ctrl, Alt or Logo is held or it starts with a control character (below U+0020, or DEL). The tool
 * handles its own keys (Enter, Tab, arrows, Escape) before calling this. max is the most bytes the
 * text may hold, FIELD_MAX - 1 at most; 0 means that. */
FieldKey field_edit_key(Field *f, const FieldKeyEvent *e, size_t max);

#endif
