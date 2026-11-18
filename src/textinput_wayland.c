#include "textinput_wayland.h"
#include "text-input-unstable-v3-client-protocol.h"
#include <stdlib.h>
#include <string.h>

struct PacTextInput {
    struct zwp_text_input_v3 *input;
    PacTextInputSink sink;
    void *data;
    PacTextInputPending pending;
    bool focused;                   /* the surface has the text-input focus: between enter and leave */
    PacTextInputField want, sent;   /* what the tool wants, and what the compositor has (sent.enabled: committed) */
};

/* Sends what the compositor does not have yet. Every enable resets what was set, so it carries the whole state. */
static void flush(PacTextInput *ti) {
    if (!ti->focused) return;
    const PacTextInputField *want = &ti->want;
    bool rect = want->w > 0 && want->h > 0;
    if (!want->enabled) {
        if (ti->sent.enabled) {
            zwp_text_input_v3_disable(ti->input);
            zwp_text_input_v3_commit(ti->input);
            ti->sent.enabled = false;
        }
        return;
    }
    bool new_field = !ti->sent.enabled || want->purpose != ti->sent.purpose;
    bool moved = rect && (want->x != ti->sent.x || want->y != ti->sent.y || want->w != ti->sent.w || want->h != ti->sent.h);
    if (!new_field && !moved) return;
    if (new_field) {
        zwp_text_input_v3_enable(ti->input);
        zwp_text_input_v3_set_content_type(ti->input, ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE, want->purpose);
    }
    if (rect) zwp_text_input_v3_set_cursor_rectangle(ti->input, want->x, want->y, want->w, want->h);
    zwp_text_input_v3_commit(ti->input);
    ti->sent = *want;
    ti->sent.enabled = true;
}

static void on_enter(void *data, struct zwp_text_input_v3 *input, struct wl_surface *surface) {
    (void)input; (void)surface;
    PacTextInput *ti = data;
    ti->focused = true;
    ti->sent.enabled = false;       /* the compositor starts again with nothing enabled */
    flush(ti);
}

static void on_leave(void *data, struct zwp_text_input_v3 *input, struct wl_surface *surface) {
    (void)input; (void)surface;
    PacTextInput *ti = data;
    ti->focused = false;
    ti->sent.enabled = false;
    pac_textinput_pending_reset(&ti->pending);
    if (ti->sink.preedit) ti->sink.preedit(ti->data, "", -1, -1);    /* a preedit does not outlive the focus */
}

static void on_preedit(void *data, struct zwp_text_input_v3 *input, const char *text, int32_t begin, int32_t end) {
    (void)input;
    pac_textinput_pending_preedit(&((PacTextInput *)data)->pending, text, begin, end);
}

static void on_commit(void *data, struct zwp_text_input_v3 *input, const char *text) {
    (void)input;
    pac_textinput_pending_commit(&((PacTextInput *)data)->pending, text);
}

static void on_delete(void *data, struct zwp_text_input_v3 *input, uint32_t before, uint32_t after) {
    (void)input;
    pac_textinput_pending_delete(&((PacTextInput *)data)->pending, before, after);
}

static void on_done(void *data, struct zwp_text_input_v3 *input, uint32_t serial) {
    (void)input; (void)serial;     /* applied whatever the serial is (a state the compositor has not seen yet is still applied) */
    PacTextInput *ti = data;
    pac_textinput_apply(&ti->pending, &ti->sink, ti->data);
}

static const struct zwp_text_input_v3_listener listener = {
    .enter = on_enter, .leave = on_leave, .preedit_string = on_preedit, .commit_string = on_commit,
    .delete_surrounding_text = on_delete, .done = on_done,
};

PacTextInput *pac_textinput_create(struct zwp_text_input_manager_v3 *manager, struct wl_seat *seat,
                                   const PacTextInputSink *sink, void *data) {
    if (!manager || !seat || !sink) return NULL;
    PacTextInput *ti = calloc(1, sizeof(*ti));   /* once, at start: a few hundred bytes that live as long as the tool */
    if (!ti) return NULL;
    ti->input = zwp_text_input_manager_v3_get_text_input(manager, seat);
    if (!ti->input) {
        free(ti);
        return NULL;
    }
    ti->sink = *sink;
    ti->data = data;
    pac_textinput_pending_reset(&ti->pending);
    zwp_text_input_v3_add_listener(ti->input, &listener, ti);
    return ti;
}

void pac_textinput_destroy(PacTextInput *ti) {
    if (!ti) return;
    zwp_text_input_v3_destroy(ti->input);
    free(ti);
}

void pac_textinput_set_field(PacTextInput *ti, const PacTextInputField *field) {
    if (!ti || !field) return;
    ti->want = *field;
    flush(ti);
}
