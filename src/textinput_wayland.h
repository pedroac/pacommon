#ifndef TEXTINPUT_WAYLAND_H
#define TEXTINPUT_WAYLAND_H

#include "textinput.h"
#include <stdbool.h>
#include <stdint.h>

/* The part of the input method support that talks to the compositor: the text-input-v3 protocol
 * (wayland-protocols, unstable/text-input; textinput.h is what it does to a field). The tool binds
 * zwp_text_input_manager_v3 from the registry, **at version 1** (every compositor that has the protocol has that
 * one), and generates the protocol's client header and code with wayland-scanner like its other protocols; this
 * file includes the header "text-input-unstable-v3-client-protocol.h" from the tool's build folder.
 *
 * What it asks of the input method, and what it does not: it says where the cursor is (so that the window of
 * candidates is next to it) and what kind of field it is. It never sends the text of the field (the surrounding
 * text), so the input method does not see what was typed before; a field that is a password is never enabled, so
 * no input method sees it at all. */

struct zwp_text_input_manager_v3;
struct wl_seat;

typedef struct PacTextInput PacTextInput;

/* purpose, from the protocol: 0 normal, 7 name, 8 password (do not enable a password: see above), 9 pin. */
enum { PAC_TEXTINPUT_NORMAL = 0, PAC_TEXTINPUT_NAME = 7 };

typedef struct {
    bool enabled;                   /* a field that takes text has the focus (and it is not a password) */
    uint32_t purpose;
    int x, y, w, h;                 /* the cursor, in the surface's coordinates; w and h of 0: not known */
} PacTextInputField;

/* NULL if there is no manager (the compositor has no input methods): the tool goes on without them. The sink is
 * what the input method's events do. */
PacTextInput *pac_textinput_create(struct zwp_text_input_manager_v3 *manager, struct wl_seat *seat,
                                   const PacTextInputSink *sink, void *data);
void pac_textinput_destroy(PacTextInput *ti);

/* What the tool wants now: call it whenever the field that has the focus, or where its cursor is, changes (it is
 * cheap to call for a frame in which nothing did). What is not yet possible, because the surface has not got the
 * keyboard focus, is sent when it has. */
void pac_textinput_set_field(PacTextInput *ti, const PacTextInputField *field);

#endif
