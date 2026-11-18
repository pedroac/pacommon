#ifndef TEXTINPUT_H
#define TEXTINPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "tunables.h"

/* What an input method (the program that lets a person type Chinese, Japanese, Korean, ... with a keyboard of
 * letters) does to the field that has the focus, with no Wayland in it: the text-input-v3 protocol sends a
 * few events and then `done`, and the events are to be applied together, in an order the protocol fixes. This is
 * that: it keeps the pending state, checks what comes from outside (a compositor, a program somewhere else) and
 * applies it to a sink, which is the tool's field. textinput_wayland.[ch] is the part that talks to the
 * compositor.
 *
 * What is refused, on purpose: a string that is not valid UTF-8, has a control character (a newline in a one line
 * field) or is longer than PAC_TEXTINPUT_TEXT_MAX is taken as empty, whole. */

typedef struct {
    /* The preedit is what is being composed: show it, underlined, after the text; "" removes it. `begin` and `end`
     * are the cursor in it, in bytes, or -1 and -1 for none shown. */
    void (*preedit)(void *data, const char *utf8, int begin, int end);
    /* Text to insert at the end of the field. */
    void (*commit)(void *data, const char *utf8);
    /* Delete about this many bytes before the cursor (the end of the field), whole characters. What is after the
     * cursor is not asked for: there is nothing there. */
    void (*delete_before)(void *data, size_t bytes);
} PacTextInputSink;

typedef struct {
    char preedit[PAC_TEXTINPUT_TEXT_MAX], commit[PAC_TEXTINPUT_TEXT_MAX];
    int begin, end;                  /* the cursor in the preedit, or -1, -1 */
    bool has_preedit;                /* a preedit_string event came: even an empty one removes the old */
    uint32_t delete_before, delete_after;
} PacTextInputPending;

void pac_textinput_pending_reset(PacTextInputPending *p);
/* The events, each of which only changes the pending state. */
void pac_textinput_pending_preedit(PacTextInputPending *p, const char *utf8, int begin, int end);
void pac_textinput_pending_commit(PacTextInputPending *p, const char *utf8);
void pac_textinput_pending_delete(PacTextInputPending *p, uint32_t before, uint32_t after);
/* `done`: applies the pending state in the order of the protocol (the old preedit is removed, the surrounding text
 * deleted, the commit inserted, then the new preedit shown) and resets it. Does nothing when nothing is pending. */
void pac_textinput_apply(PacTextInputPending *p, const PacTextInputSink *sink, void *data);

#endif
