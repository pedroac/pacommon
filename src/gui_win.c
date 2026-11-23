#define _GNU_SOURCE
#include "gui_win.h"
#include "tunables.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "cursor-shape-v1-client-protocol.h"
#include "tablet-unstable-v2-client-protocol.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include "length.h"
#include "utf8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

#define REPEAT_NONE (-1LL)

#define MAX_OUTPUTS 8

typedef struct {
    struct wl_output *output;
    int physical_mm, mode_px, mode_h, scale;
    char name[40];
} Output;

struct Win {
    Output outputs[MAX_OUTPUTS];
    int output_count;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct zwlr_layer_shell_v1 *layer_shell;
    uint32_t layer_shell_version;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    struct xdg_wm_base *wm_base;   /* a normal window (toplevel) instead of the layer surface */
    struct xdg_surface *xsurface;
    struct xdg_toplevel *toplevel;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct zwp_tablet_manager_v2 *tablet_manager;
    struct zwp_tablet_seat_v2 *tablet_seat;
    char tablet_name[96];
    int pad_buttons;           /* of the first pad, 0 if none */
    struct wp_cursor_shape_manager_v1 *cursor_manager;
    struct wp_cursor_shape_device_v1 *cursor_device;
    uint32_t pointer_serial;           /* of the last enter: what a cursor shape is set with */
    bool pointer_in;
    WinCursor cursor;
    struct wl_data_device_manager *data_manager;
    struct wl_data_device *data_device;
    struct wl_data_offer *selection;   /* the clipboard */
    const char *selection_mime;        /* the kind of text it can give, NULL if none */

    struct xkb_context *xkb;
    struct xkb_keymap *keymap;
    struct xkb_state *xkb_state;
    struct xkb_compose_state *compose;   /* dead keys and Compose: ´ then a gives á */

    WinHandlers handlers;
    void *data;
    Anchor anchor;
    int margin;
    int width, height;     /* logical */
    int scale;
    bool configured, running;
    double px, py;         /* the pointer */
    uint32_t last_key, last_mods;   /* the key that was sent last, and the modifiers then */

    int repeat_rate, repeat_delay;
    uint32_t repeat_key;
    long long repeat_due;  /* ms, REPEAT_NONE for none */
    long long repeat_every;
};

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

/* ---- surface ---- */

static void surface_enter(void *d, struct wl_surface *s, struct wl_output *o) { (void)d; (void)s; (void)o; }
static void surface_leave(void *d, struct wl_surface *s, struct wl_output *o) { (void)d; (void)s; (void)o; }
static void surface_buffer_transform(void *d, struct wl_surface *s, uint32_t t) { (void)d; (void)s; (void)t; }

static void surface_buffer_scale(void *data, struct wl_surface *s, int32_t factor) {
    Win *w = data;
    (void)s;
    if (factor < 1) factor = 1;
    if (factor > MAX_SCALE) factor = MAX_SCALE;
    if (factor == w->scale) return;
    w->scale = factor;
    if (w->configured) w->handlers.redraw(w->data);
}

static const struct wl_surface_listener surface_listener = {
    .enter = surface_enter,
    .leave = surface_leave,
    .preferred_buffer_scale = surface_buffer_scale,
    .preferred_buffer_transform = surface_buffer_transform,
};

static void layer_configure(void *data, struct zwlr_layer_surface_v1 *l, uint32_t serial, uint32_t width,
                            uint32_t height) {
    Win *w = data;
    if (width) w->width = (int)width;
    if (height) w->height = (int)height;
    zwlr_layer_surface_v1_ack_configure(l, serial);
    w->configured = true;
    w->handlers.redraw(w->data);
}

static void layer_closed(void *data, struct zwlr_layer_surface_v1 *l) {
    Win *w = data;
    (void)l;
    w->running = false;
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
    .closed = layer_closed,
};

/* ---- a normal window ---- */

static void wm_ping(void *d, struct xdg_wm_base *b, uint32_t serial) { (void)d; xdg_wm_base_pong(b, serial); }
static const struct xdg_wm_base_listener wm_listener = { .ping = wm_ping };

static void xsurface_configure(void *data, struct xdg_surface *x, uint32_t serial) {
    Win *w = data;
    xdg_surface_ack_configure(x, serial);
    w->configured = true;
    w->handlers.redraw(w->data);
}
static const struct xdg_surface_listener xsurface_listener = { .configure = xsurface_configure };

static void toplevel_configure(void *data, struct xdg_toplevel *t, int32_t width, int32_t height, struct wl_array *states) {
    Win *w = data;
    (void)t; (void)states;
    if (width > 0) w->width = width;     /* 0: the window's own size */
    if (height > 0) w->height = height;
}
static void toplevel_close(void *data, struct xdg_toplevel *t) { Win *w = data; (void)t; w->running = false; }
static void toplevel_bounds(void *d, struct xdg_toplevel *t, int32_t w, int32_t h) { (void)d; (void)t; (void)w; (void)h; }
static void toplevel_caps(void *d, struct xdg_toplevel *t, struct wl_array *c) { (void)d; (void)t; (void)c; }
static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
    .configure_bounds = toplevel_bounds,
    .wm_capabilities = toplevel_caps,
};

/* ---- keyboard ---- */

static void keyboard_keymap(void *data, struct wl_keyboard *k, uint32_t format, int fd, uint32_t size) {
    Win *w = data;
    (void)k;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || size == 0 || size > (1u << 22)) {
        close(fd);
        return;
    }
    char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) return;
    struct xkb_keymap *keymap = xkb_keymap_new_from_buffer(w->xkb, map, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1,
                                                          XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, size);
    if (!keymap) return;
    struct xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        xkb_keymap_unref(keymap);
        return;
    }
    if (w->xkb_state) xkb_state_unref(w->xkb_state);
    if (w->keymap) xkb_keymap_unref(w->keymap);
    w->keymap = keymap;
    w->xkb_state = state;
    if (w->handlers.redraw) w->handlers.redraw(w->data);   /* the layout in use may have changed */
}

static void keyboard_enter(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys) {
    (void)d; (void)k; (void)serial; (void)s; (void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s) {
    Win *w = data;
    (void)k; (void)serial; (void)s;
    w->repeat_due = REPEAT_NONE;
}

static unsigned held_mods(const Win *w) {
    unsigned held = 0;
    if (xkb_state_mod_name_is_active(w->xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0) held |= FIELD_CTRL;
    if (xkb_state_mod_name_is_active(w->xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) held |= FIELD_ALT;
    if (xkb_state_mod_name_is_active(w->xkb_state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0) held |= FIELD_LOGO;
    return held;
}

static void send_key(Win *w, uint32_t key) {
    char text[16] = "";
    xkb_keycode_t code = key + 8;   /* evdev to XKB */
    w->last_key = key;
    w->last_mods = xkb_state_serialize_mods(w->xkb_state, XKB_STATE_MODS_EFFECTIVE);
    xkb_keysym_t sym = xkb_state_key_get_one_sym(w->xkb_state, code);
    int n = xkb_state_key_get_utf8(w->xkb_state, code, text, sizeof(text));
    if (n < 0 || (size_t)n >= sizeof(text)) text[0] = '\0';
    if (w->compose && xkb_compose_state_feed(w->compose, sym) == XKB_COMPOSE_FEED_ACCEPTED) {
        switch (xkb_compose_state_get_status(w->compose)) {
        case XKB_COMPOSE_COMPOSING:
            return;   /* waiting for the letter after the accent */
        case XKB_COMPOSE_COMPOSED:
            n = xkb_compose_state_get_utf8(w->compose, text, sizeof(text));
            if (n < 0 || (size_t)n >= sizeof(text)) text[0] = '\0';
            sym = xkb_compose_state_get_one_sym(w->compose);
            xkb_compose_state_reset(w->compose);
            break;
        case XKB_COMPOSE_CANCELLED:
            xkb_compose_state_reset(w->compose);
            return;
        default:
            break;
        }
    }
    w->handlers.key(w->data, sym, text, held_mods(w));
}

static void keyboard_key(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state) {
    Win *w = data;
    (void)k; (void)serial; (void)time;
    if (!w->xkb_state) return;
    if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (key == w->repeat_key) w->repeat_due = REPEAT_NONE;
        return;
    }
    send_key(w, key);
    if (w->repeat_rate > 0 && xkb_keymap_key_repeats(w->keymap, key + 8)) {
        w->repeat_key = key;
        w->repeat_due = now_ms() + w->repeat_delay;
        w->repeat_every = 1000 / w->repeat_rate;
    } else {
        w->repeat_due = REPEAT_NONE;
    }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t depressed,
                               uint32_t latched, uint32_t locked, uint32_t group) {
    Win *w = data;
    (void)k; (void)serial;
    if (w->xkb_state) xkb_state_update_mask(w->xkb_state, depressed, latched, locked, 0, 0, group);
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *k, int32_t rate, int32_t delay) {
    Win *w = data;
    (void)k;
    w->repeat_rate = rate;
    w->repeat_delay = delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

void win_last_key(const Win *w, uint32_t *key, uint32_t *mods) {
    *key = w->last_key;
    *mods = w->last_mods;
}

bool win_key_text(Win *w, uint32_t key, uint32_t mods, char *out, size_t size) {
    out[0] = '\0';
    if (!w->keymap) return false;
    struct xkb_state *state = xkb_state_new(w->keymap);
    if (!state) return false;
    xkb_state_update_mask(state, mods, 0, 0, 0, 0, 0);
    int n = xkb_state_key_get_utf8(state, key + 8, out, size);
    xkb_state_unref(state);
    if (n <= 0 || (size_t)n >= size) { out[0] = '\0'; return false; }
    return true;
}

const char *win_layout_name(const Win *w) {
    static char name[96];
    name[0] = '\0';
    if (!w->keymap || !w->xkb_state) return name;
    xkb_layout_index_t index = xkb_state_serialize_layout(w->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
    const char *n = index < xkb_keymap_num_layouts(w->keymap) ? xkb_keymap_layout_get_name(w->keymap, index) : NULL;
    if (n) snprintf(name, sizeof(name), "%s", n);
    return name;
}

int win_repeat_timeout(const Win *w) {
    if (w->repeat_due == REPEAT_NONE) return -1;
    long long left = w->repeat_due - now_ms();
    return left > 0 ? (int)left : 0;
}

void win_repeat(Win *w) {
    if (w->repeat_due == REPEAT_NONE || now_ms() < w->repeat_due || !w->xkb_state) return;
    send_key(w, w->repeat_key);
    w->repeat_due += w->repeat_every;
}

/* ---- pointer ---- */

/* The compositor draws the cursor: only its name is said. Without the protocol the cursor stays as it is. */
static void apply_cursor(Win *w) {
    if (!w->cursor_device || !w->pointer_in) return;
    static const enum wp_cursor_shape_device_v1_shape shapes[] = {
        [WIN_CURSOR_DEFAULT] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT,
        [WIN_CURSOR_POINTER] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER,
        [WIN_CURSOR_TEXT] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT,
        [WIN_CURSOR_GRAB] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB,
        [WIN_CURSOR_GRABBING] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING,
    };
    wp_cursor_shape_device_v1_set_shape(w->cursor_device, w->pointer_serial, shapes[w->cursor]);
}

void win_set_cursor(Win *w, WinCursor cursor) {
    if (cursor == w->cursor) return;
    w->cursor = cursor;
    apply_cursor(w);
}

static void pointer_enter(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x,
                          wl_fixed_t y) {
    Win *w = data;
    (void)p; (void)serial; (void)s;
    w->px = wl_fixed_to_double(x);
    w->py = wl_fixed_to_double(y);
    w->pointer_serial = serial;
    w->pointer_in = true;
    apply_cursor(w);
    w->handlers.pointer(w->data, w->px, w->py, 0);
}

static void pointer_leave(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s) {
    Win *w = data;
    (void)p; (void)serial; (void)s;
    w->pointer_in = false;
}

static void pointer_motion(void *data, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    Win *w = data;
    (void)p; (void)time;
    w->px = wl_fixed_to_double(x);
    w->py = wl_fixed_to_double(y);
    w->handlers.pointer(w->data, w->px, w->py, 0);
}

static void pointer_button(void *data, struct wl_pointer *p, uint32_t serial, uint32_t time, uint32_t button,
                           uint32_t state) {
    Win *w = data;
    (void)p; (void)serial; (void)time;
    if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
        if (button == BTN_LEFT) w->handlers.pointer(w->data, w->px, w->py, 3);
        return;
    }
    w->handlers.pointer(w->data, w->px, w->py, button == BTN_LEFT ? 1 : 2);
}

static void pointer_axis(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis, wl_fixed_t value) {
    Win *w = data;
    (void)p; (void)time;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) w->handlers.scroll(w->data, wl_fixed_to_double(value));
}

static void pointer_frame(void *d, struct wl_pointer *p) { (void)d; (void)p; }
static void pointer_axis_source(void *d, struct wl_pointer *p, uint32_t s) { (void)d; (void)p; (void)s; }
static void pointer_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a) { (void)d; (void)p; (void)t; (void)a; }
static void pointer_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v) { (void)d; (void)p; (void)a; (void)v; }
static void pointer_axis_value120(void *d, struct wl_pointer *p, uint32_t a, int32_t v) { (void)d; (void)p; (void)a; (void)v; }
static void pointer_axis_direction(void *d, struct wl_pointer *p, uint32_t a, uint32_t v) { (void)d; (void)p; (void)a; (void)v; }

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
    .axis_value120 = pointer_axis_value120,
    .axis_relative_direction = pointer_axis_direction,
};

/* ---- the clipboard ---- */

/* The kinds of text, the best first. A new offer is described by its `offer` events, before it is the selection. */
static const char *const text_mimes[] = { "text/plain;charset=utf-8", "text/plain", "UTF8_STRING", NULL };

typedef struct {
    int best;    /* index in text_mimes of the best kind seen, or -1 */
} OfferInfo;

static void offer_mime(void *data, struct wl_data_offer *offer, const char *mime) {
    (void)offer;
    OfferInfo *info = data;
    for (int i = 0; text_mimes[i]; i++)
        if (strcmp(mime, text_mimes[i]) == 0 && (info->best < 0 || i < info->best)) info->best = i;
}

static void offer_source_actions(void *d, struct wl_data_offer *o, uint32_t a) { (void)d; (void)o; (void)a; }
static void offer_action(void *d, struct wl_data_offer *o, uint32_t a) { (void)d; (void)o; (void)a; }

static const struct wl_data_offer_listener offer_listener = {
    .offer = offer_mime, .source_actions = offer_source_actions, .action = offer_action,
};

static void device_data_offer(void *data, struct wl_data_device *dev, struct wl_data_offer *offer) {
    (void)data; (void)dev;
    OfferInfo *info = malloc(sizeof(*info));
    if (!info) return;
    info->best = -1;
    wl_data_offer_add_listener(offer, &offer_listener, info);
}

static void device_selection(void *data, struct wl_data_device *dev, struct wl_data_offer *offer) {
    Win *w = data;
    (void)dev;
    if (w->selection) {
        free(wl_data_offer_get_user_data(w->selection));
        wl_data_offer_destroy(w->selection);
    }
    w->selection = offer;
    w->selection_mime = NULL;
    if (offer) {
        OfferInfo *info = wl_data_offer_get_user_data(offer);
        if (info && info->best >= 0) w->selection_mime = text_mimes[info->best];
    }
}

/* Drag and drop is not used, but its offers have to be answered for. */
static void device_enter(void *d, struct wl_data_device *dev, uint32_t serial, struct wl_surface *s, wl_fixed_t x,
                         wl_fixed_t y, struct wl_data_offer *o) {
    (void)d; (void)dev; (void)serial; (void)s; (void)x; (void)y;
    if (o) {
        free(wl_data_offer_get_user_data(o));
        wl_data_offer_destroy(o);
    }
}
static void device_leave(void *d, struct wl_data_device *dev) { (void)d; (void)dev; }
static void device_motion(void *d, struct wl_data_device *dev, uint32_t t, wl_fixed_t x, wl_fixed_t y) { (void)d; (void)dev; (void)t; (void)x; (void)y; }
static void device_drop(void *d, struct wl_data_device *dev) { (void)d; (void)dev; }

static const struct wl_data_device_listener device_listener = {
    .data_offer = device_data_offer, .enter = device_enter, .leave = device_leave, .motion = device_motion,
    .drop = device_drop, .selection = device_selection,
};

bool win_paste(Win *w, char *out, size_t size) {
    if (!w->selection || !w->selection_mime || size < 2) return false;
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) < 0) return false;
    wl_data_offer_receive(w->selection, w->selection_mime, fds[1]);
    close(fds[1]);
    wl_display_flush(w->display);

    char raw[512];
    size_t len = 0;
    bool ok = true;
    for (;;) {
        struct pollfd p = { .fd = fds[0], .events = POLLIN };
        int r = poll(&p, 1, 500);
        if (r <= 0) { ok = false; break; }   /* the owner did not answer */
        ssize_t n = read(fds[0], raw + len, sizeof(raw) - 1 - len);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        len += (size_t)n;
        if (len == sizeof(raw) - 1) { ok = false; break; }   /* more than any password */
    }
    close(fds[0]);
    raw[len] = '\0';
    if (ok) {
        char *start = raw, *end = raw + len;
        char *nl = memchr(raw, '\n', len);
        if (nl) end = nl;
        while (start < end && (*start == ' ' || *start == '\t' || *start == '\r')) start++;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) end--;
        size_t n = (size_t)(end - start);
        ok = n > 0 && n < size && utf8_plain(start, n);
        if (ok) {
            memcpy(out, start, n);
            out[n] = '\0';
        }
    }
    explicit_bzero(raw, sizeof(raw));
    return ok;
}

/* ---- the pen: the tablet protocol, for a window that shows what the pen does ---- */

typedef struct {
    Win *win;
    WinTablet pen;
} ToolState;

/* Events that are not used. */
static void tool_u2(void *d, struct zwp_tablet_tool_v2 *t, uint32_t a, uint32_t b) { (void)d; (void)t; (void)a; (void)b; }
static void tool_u1(void *d, struct zwp_tablet_tool_v2 *t, uint32_t a) { (void)d; (void)t; (void)a; }
static void tool_i1(void *d, struct zwp_tablet_tool_v2 *t, int32_t a) { (void)d; (void)t; (void)a; }
static void tool_f1(void *d, struct zwp_tablet_tool_v2 *t, wl_fixed_t a) { (void)d; (void)t; (void)a; }
static void tool_fi(void *d, struct zwp_tablet_tool_v2 *t, wl_fixed_t a, int32_t b) { (void)d; (void)t; (void)a; (void)b; }
static void tool_u3(void *d, struct zwp_tablet_tool_v2 *t, uint32_t a, uint32_t b, uint32_t c) { (void)d; (void)t; (void)a; (void)b; (void)c; }
static void tool_done(void *d, struct zwp_tablet_tool_v2 *t) { (void)d; (void)t; }
static void tab_uu(void *d, struct zwp_tablet_v2 *t, uint32_t a, uint32_t b) { (void)d; (void)t; (void)a; (void)b; }
static void tab_s(void *d, struct zwp_tablet_v2 *t, const char *a) { (void)d; (void)t; (void)a; }
static void tab_done(void *d, struct zwp_tablet_v2 *t) { (void)d; (void)t; }
static void tool_type(void *d, struct zwp_tablet_tool_v2 *t, uint32_t type) {
    (void)t;
    ((ToolState *)d)->pen.eraser = type == ZWP_TABLET_TOOL_V2_TYPE_ERASER;
}
static void tool_proximity_in(void *d, struct zwp_tablet_tool_v2 *t, uint32_t serial, struct zwp_tablet_v2 *tab, struct wl_surface *s) {
    (void)t; (void)serial; (void)tab;
    ToolState *st = d;
    st->pen.near = s == st->win->surface;
}
static void tool_proximity_out(void *d, struct zwp_tablet_tool_v2 *t) {
    (void)t;
    ToolState *st = d;
    st->pen.near = false;
    st->pen.down = false;
    st->pen.pressure = 0;
    if (st->win->handlers.tablet) st->win->handlers.tablet(st->win->data, &st->pen);
}
static void tool_down(void *d, struct zwp_tablet_tool_v2 *t, uint32_t serial) { (void)t; (void)serial; ((ToolState *)d)->pen.down = true; }
static void tool_up(void *d, struct zwp_tablet_tool_v2 *t) { (void)t; ((ToolState *)d)->pen.down = false; ((ToolState *)d)->pen.pressure = 0; }
static void tool_motion(void *d, struct zwp_tablet_tool_v2 *t, wl_fixed_t x, wl_fixed_t y) {
    (void)t;
    ((ToolState *)d)->pen.x = wl_fixed_to_double(x);
    ((ToolState *)d)->pen.y = wl_fixed_to_double(y);
}
static void tool_pressure(void *d, struct zwp_tablet_tool_v2 *t, uint32_t p) { (void)t; ((ToolState *)d)->pen.pressure = p / 65535.0; }
static void tool_tilt(void *d, struct zwp_tablet_tool_v2 *t, wl_fixed_t x, wl_fixed_t y) {
    (void)t;
    ((ToolState *)d)->pen.tilt_x = wl_fixed_to_double(x);
    ((ToolState *)d)->pen.tilt_y = wl_fixed_to_double(y);
}
static void tool_frame(void *d, struct zwp_tablet_tool_v2 *t, uint32_t time) {
    (void)t; (void)time;
    ToolState *st = d;
    if (st->pen.near && st->win->handlers.tablet) st->win->handlers.tablet(st->win->data, &st->pen);
}
static void tool_removed(void *d, struct zwp_tablet_tool_v2 *t) {
    free(d);
    zwp_tablet_tool_v2_destroy(t);
}

static const struct zwp_tablet_tool_v2_listener tool_listener = {
    .type = tool_type,
    .hardware_serial = tool_u2,
    .hardware_id_wacom = tool_u2,
    .capability = tool_u1,
    .done = tool_done,
    .removed = tool_removed,
    .proximity_in = tool_proximity_in,
    .proximity_out = tool_proximity_out,
    .down = tool_down,
    .up = tool_up,
    .motion = tool_motion,
    .pressure = tool_pressure,
    .distance = tool_u1,
    .tilt = tool_tilt,
    .rotation = tool_f1,
    .slider = tool_i1,
    .wheel = tool_fi,
    .button = tool_u3,
    .frame = tool_frame,
};

static void tablet_name_event(void *d, struct zwp_tablet_v2 *t, const char *name) {
    (void)t;
    Win *w = d;
    if (!w->tablet_name[0]) snprintf(w->tablet_name, sizeof(w->tablet_name), "%s", name);
}
static void tablet_removed(void *d, struct zwp_tablet_v2 *t) { (void)d; zwp_tablet_v2_destroy(t); }
static const struct zwp_tablet_v2_listener tablet_listener = {
    .name = tablet_name_event,
    .id = tab_uu,
    .path = tab_s,
    .done = tab_done,
    .removed = tablet_removed,
};

static void seat_tablet_added(void *d, struct zwp_tablet_seat_v2 *s, struct zwp_tablet_v2 *t) {
    (void)s;
    zwp_tablet_v2_add_listener(t, &tablet_listener, d);
}
static void seat_tool_added(void *d, struct zwp_tablet_seat_v2 *s, struct zwp_tablet_tool_v2 *t) {
    (void)s;
    ToolState *st = calloc(1, sizeof(*st));
    if (!st) {
        zwp_tablet_tool_v2_destroy(t);
        return;
    }
    st->win = d;
    zwp_tablet_tool_v2_add_listener(t, &tool_listener, st);
}
/* The pad: only how many buttons it has is wanted (to offer them); its groups, rings and strips are not listened to. */
static void pad_group(void *d, struct zwp_tablet_pad_v2 *p, struct zwp_tablet_pad_group_v2 *g) { (void)d; (void)p; (void)g; }
static void pad_path(void *d, struct zwp_tablet_pad_v2 *p, const char *s) { (void)d; (void)p; (void)s; }
static void pad_buttons(void *d, struct zwp_tablet_pad_v2 *p, uint32_t n) {
    (void)p;
    Win *w = d;
    if (w->pad_buttons == 0) w->pad_buttons = (int)n;
}
static void pad_done(void *d, struct zwp_tablet_pad_v2 *p) { (void)d; (void)p; }
static void pad_button(void *d, struct zwp_tablet_pad_v2 *p, uint32_t t, uint32_t b, uint32_t s) { (void)d; (void)p; (void)t; (void)b; (void)s; }
static void pad_enter(void *d, struct zwp_tablet_pad_v2 *p, uint32_t serial, struct zwp_tablet_v2 *t, struct wl_surface *s) { (void)d; (void)p; (void)serial; (void)t; (void)s; }
static void pad_leave(void *d, struct zwp_tablet_pad_v2 *p, uint32_t serial, struct wl_surface *s) { (void)d; (void)p; (void)serial; (void)s; }
static void pad_removed(void *d, struct zwp_tablet_pad_v2 *p) { (void)d; zwp_tablet_pad_v2_destroy(p); }
static const struct zwp_tablet_pad_v2_listener pad_listener = {
    .group = pad_group, .path = pad_path, .buttons = pad_buttons, .done = pad_done, .button = pad_button,
    .enter = pad_enter, .leave = pad_leave, .removed = pad_removed,
};
static void seat_pad_added(void *d, struct zwp_tablet_seat_v2 *s, struct zwp_tablet_pad_v2 *p) {
    (void)s;
    zwp_tablet_pad_v2_add_listener(p, &pad_listener, d);
}
static const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
    .tablet_added = seat_tablet_added, .tool_added = seat_tool_added, .pad_added = seat_pad_added,
};

const char *win_tablet_name(const Win *w) { return w->tablet_name; }
int win_pad_buttons(const Win *w) { return w->pad_buttons; }
int win_output_count(const Win *w) { return w->output_count; }
const char *win_output_name(const Win *w, int i) { return i >= 0 && i < w->output_count ? w->outputs[i].name : ""; }

/* ---- seat and registry ---- */

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    Win *w = data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !w->keyboard) {
        w->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(w->keyboard, &keyboard_listener, w);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && w->keyboard) {
        wl_keyboard_release(w->keyboard);
        w->keyboard = NULL;
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !w->pointer) {
        w->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(w->pointer, &pointer_listener, w);
        if (w->cursor_manager) w->cursor_device = wp_cursor_shape_manager_v1_get_pointer(w->cursor_manager, w->pointer);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && w->pointer) {
        if (w->cursor_device) wp_cursor_shape_device_v1_destroy(w->cursor_device);
        w->cursor_device = NULL;
        wl_pointer_release(w->pointer);
        w->pointer = NULL;
    }
}

static void seat_name(void *d, struct wl_seat *s, const char *n) { (void)d; (void)s; (void)n; }

static const struct wl_seat_listener seat_listener = { .capabilities = seat_capabilities, .name = seat_name };

/* ---- outputs: only their size is wanted, to make the text a real size ---- */

static void output_geometry(void *data, struct wl_output *o, int32_t x, int32_t y, int32_t pw, int32_t ph,
                            int32_t subpixel, const char *make, const char *model, int32_t transform) {
    (void)o; (void)x; (void)y; (void)ph; (void)subpixel; (void)make; (void)model; (void)transform;
    ((Output *)data)->physical_mm = pw;
}
static void output_mode(void *data, struct wl_output *o, uint32_t flags, int32_t w, int32_t h, int32_t refresh) {
    (void)o; (void)refresh;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        ((Output *)data)->mode_px = w;
        ((Output *)data)->mode_h = h;
    }
}
static void output_done(void *d, struct wl_output *o) { (void)d; (void)o; }
static void output_name(void *data, struct wl_output *o, const char *name) {
    (void)o;
    snprintf(((Output *)data)->name, sizeof(((Output *)data)->name), "%s", name);
}
static void output_description(void *d, struct wl_output *o, const char *t) { (void)d; (void)o; (void)t; }
static void output_scale(void *data, struct wl_output *o, int32_t factor) {
    (void)o;
    ((Output *)data)->scale = factor < 1 ? 1 : factor;
}
static const struct wl_output_listener output_listener = {
    .geometry = output_geometry, .mode = output_mode, .done = output_done, .scale = output_scale,
    .name = output_name, .description = output_description,
};

/* The output the window will most likely be on: the only one, else the first that says how large it is. */
static const Output *likely_output(const Win *w) {
    if (w->output_count == 1) return &w->outputs[0];
    for (int i = 0; i < w->output_count; i++)
        if (w->outputs[i].physical_mm > 0 && w->outputs[i].mode_px > 0) return &w->outputs[i];
    return w->output_count ? &w->outputs[0] : NULL;
}

static void registry_global(void *data, struct wl_registry *r, uint32_t name, const char *interface,
                            uint32_t version) {
    Win *w = data;
#define BIND(iface, ver) wl_registry_bind(r, name, &iface##_interface, version < (ver) ? version : (ver))
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        w->compositor = BIND(wl_compositor, 6);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        w->shm = BIND(wl_shm, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0 && !w->seat) {
        w->seat = BIND(wl_seat, 5);
        wl_seat_add_listener(w->seat, &seat_listener, w);
    } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
        w->data_manager = BIND(wl_data_device_manager, 3);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        w->layer_shell = BIND(zwlr_layer_shell_v1, 4);
        w->layer_shell_version = version < 4 ? version : 4;
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        w->wm_base = BIND(xdg_wm_base, 2);
        xdg_wm_base_add_listener(w->wm_base, &wm_listener, w);
    } else if (strcmp(interface, wl_output_interface.name) == 0 && w->output_count < MAX_OUTPUTS) {
        Output *o = &w->outputs[w->output_count++];
        o->scale = 1;
        o->output = BIND(wl_output, 4);
        wl_output_add_listener(o->output, &output_listener, o);
    } else if (strcmp(interface, zwp_tablet_manager_v2_interface.name) == 0) {
        w->tablet_manager = BIND(zwp_tablet_manager_v2, 1);
    } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
        w->cursor_manager = BIND(wp_cursor_shape_manager_v1, 1);
    }
#undef BIND
}

static void registry_remove(void *d, struct wl_registry *r, uint32_t n) { (void)d; (void)r; (void)n; }

static const struct wl_registry_listener registry_listener = { .global = registry_global, .global_remove = registry_remove };

/* ---- create and destroy ---- */

static uint32_t anchor_bits(Anchor a) {
    switch (a) {
    case ANCHOR_TOP_RIGHT: return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    case ANCHOR_TOP_LEFT: return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
    case ANCHOR_BOTTOM_RIGHT: return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    case ANCHOR_BOTTOM_LEFT: return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
    default: return 0;
    }
}

Win *win_create(int width, int height, Anchor anchor, int margin, bool toplevel, const WinHandlers *handlers, void *data) {
    Win *w = calloc(1, sizeof(*w));
    if (!w) return NULL;
    w->handlers = *handlers;
    w->data = data;
    w->anchor = anchor;
    w->margin = margin;
    w->width = width;
    w->height = height;
    w->scale = 1;
    w->repeat_due = REPEAT_NONE;
    w->running = true;

    w->display = wl_display_connect(NULL);
    if (!w->display) {
        free(w);
        return NULL;
    }
    w->xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (w->xkb) {
        const char *locale = getenv("LC_ALL");
        if (!locale || !*locale) locale = getenv("LC_CTYPE");
        if (!locale || !*locale) locale = getenv("LANG");
        if (!locale || !*locale) locale = "C";
        struct xkb_compose_table *table =
            xkb_compose_table_new_from_locale(w->xkb, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (table) {
            w->compose = xkb_compose_state_new(table, XKB_COMPOSE_STATE_NO_FLAGS);
            xkb_compose_table_unref(table);
        }
    }
    w->registry = wl_display_get_registry(w->display);
    wl_registry_add_listener(w->registry, &registry_listener, w);
    wl_display_roundtrip(w->display);
    wl_display_roundtrip(w->display);   /* the seat's capabilities */
    if (!w->compositor || !w->shm || !(toplevel ? w->wm_base != NULL : w->layer_shell != NULL) || !w->xkb) {
        win_destroy(w);
        return NULL;
    }

    double known = 0, guess = PAC_DPI_DEFAULT / 25.4, shortest = 0;
    const Output *out = likely_output(w);
    if (out) {
        known = pac_px_per_mm_known(out->physical_mm, out->mode_px, out->scale);
        guess = pac_px_per_mm(out->physical_mm, out->mode_px, out->scale);
        shortest = (out->mode_px < out->mode_h ? out->mode_px : out->mode_h) / (double)out->scale;
    }
    double zoom = w->handlers.scale ? w->handlers.scale(w->data, known, guess, shortest) : 1;
    w->width = width = (int)(width * zoom + 0.5);
    w->height = height = (int)(height * zoom + 0.5);   /* 0 stays 0: the compositor gives the screen's height */

    /* Asked only by a window that wants the pen: the tablets that are there are announced at once. */
    if (w->handlers.tablet && w->tablet_manager && w->seat) {
        w->tablet_seat = zwp_tablet_manager_v2_get_tablet_seat(w->tablet_manager, w->seat);
        zwp_tablet_seat_v2_add_listener(w->tablet_seat, &tablet_seat_listener, w);
        wl_display_roundtrip(w->display);
    }

    if (w->data_manager && w->seat) {   /* without it there is no paste, and everything else works */
        w->data_device = wl_data_device_manager_get_data_device(w->data_manager, w->seat);
        wl_data_device_add_listener(w->data_device, &device_listener, w);
    }

    w->surface = wl_compositor_create_surface(w->compositor);
    wl_surface_add_listener(w->surface, &surface_listener, w);
    if (toplevel) {
        if (height == 0) w->height = height = width * 3 / 4;
        w->xsurface = xdg_wm_base_get_xdg_surface(w->wm_base, w->surface);
        xdg_surface_add_listener(w->xsurface, &xsurface_listener, w);
        w->toplevel = xdg_surface_get_toplevel(w->xsurface);
        xdg_toplevel_add_listener(w->toplevel, &toplevel_listener, w);
        xdg_toplevel_set_title(w->toplevel, app_name);
        xdg_toplevel_set_app_id(w->toplevel, app_name);
        xdg_toplevel_set_min_size(w->toplevel, width, height);
    } else {
        w->layer = zwlr_layer_shell_v1_get_layer_surface(w->layer_shell, w->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
                                                         app_name);
        zwlr_layer_surface_v1_add_listener(w->layer, &layer_listener, w);
        zwlr_layer_surface_v1_set_size(w->layer, (uint32_t)width, (uint32_t)height);
        zwlr_layer_surface_v1_set_anchor(
            w->layer, anchor_bits(anchor) | (height ? 0 : ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM));
        zwlr_layer_surface_v1_set_margin(w->layer, margin, margin, margin, margin);
        zwlr_layer_surface_v1_set_keyboard_interactivity(
            w->layer, w->layer_shell_version >= 4 ? ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND
                                                  : ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    }
    wl_surface_commit(w->surface);
    return w;
}

void win_destroy(Win *w) {
    if (!w) return;
    if (w->toplevel) xdg_toplevel_destroy(w->toplevel);
    if (w->xsurface) xdg_surface_destroy(w->xsurface);
    if (w->layer) zwlr_layer_surface_v1_destroy(w->layer);
    if (w->surface) wl_surface_destroy(w->surface);
    if (w->keyboard) wl_keyboard_release(w->keyboard);
    if (w->tablet_seat) zwp_tablet_seat_v2_destroy(w->tablet_seat);
    if (w->tablet_manager) zwp_tablet_manager_v2_destroy(w->tablet_manager);
    for (int i = 0; i < w->output_count; i++)
        if (w->outputs[i].output) wl_output_destroy(w->outputs[i].output);
    if (w->cursor_device) wp_cursor_shape_device_v1_destroy(w->cursor_device);
    if (w->cursor_manager) wp_cursor_shape_manager_v1_destroy(w->cursor_manager);
    if (w->pointer) wl_pointer_release(w->pointer);
    if (w->selection) {
        free(wl_data_offer_get_user_data(w->selection));
        wl_data_offer_destroy(w->selection);
    }
    if (w->data_device) wl_data_device_release(w->data_device);
    if (w->data_manager) wl_data_device_manager_destroy(w->data_manager);
    if (w->xkb_state) xkb_state_unref(w->xkb_state);
    if (w->keymap) xkb_keymap_unref(w->keymap);
    if (w->compose) xkb_compose_state_unref(w->compose);
    if (w->xkb) xkb_context_unref(w->xkb);
    if (w->layer_shell) zwlr_layer_shell_v1_destroy(w->layer_shell);
    if (w->wm_base) xdg_wm_base_destroy(w->wm_base);
    if (w->seat) wl_seat_destroy(w->seat);
    if (w->shm) wl_shm_destroy(w->shm);
    if (w->compositor) wl_compositor_destroy(w->compositor);
    if (w->registry) wl_registry_destroy(w->registry);
    if (w->display) {
        wl_display_flush(w->display);
        wl_display_disconnect(w->display);
    }
    free(w);
}

/* ---- the loop ---- */

int win_fd(const Win *w) { return wl_display_get_fd(w->display); }

bool win_prepare(Win *w) {
    while (wl_display_prepare_read(w->display) != 0)
        if (wl_display_dispatch_pending(w->display) < 0) return false;
    if (wl_display_flush(w->display) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(w->display);
        return false;
    }
    return true;
}

bool win_read(Win *w, bool readable) {
    if (readable) {
        if (wl_display_read_events(w->display) < 0) return false;
    } else {
        wl_display_cancel_read(w->display);
    }
    return wl_display_dispatch_pending(w->display) >= 0;
}

bool win_running(const Win *w) { return w->running; }
void win_close(Win *w) { w->running = false; }
bool win_ready(const Win *w) { return w->configured; }
bool win_toplevel(const Win *w) { return w->toplevel != NULL; }
int win_width(const Win *w) { return w->width; }
int win_height(const Win *w) { return w->height; }

/* ---- drawing ---- */

typedef struct {
    struct wl_buffer *buffer;
    void *data;
    size_t size;
} Shot;

static void buffer_release(void *data, struct wl_buffer *buffer) {
    Shot *s = data;
    wl_buffer_destroy(buffer);
    munmap(s->data, s->size);
    free(s);
}

static const struct wl_buffer_listener buffer_listener = { .release = buffer_release };

/* The buffer and its context go together: win_end finds the buffer from the context. */
static const cairo_user_data_key_t shot_key;

cairo_t *win_begin(Win *w) {
    int pw = w->width * w->scale, ph = w->height * w->scale;
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, pw);
    size_t size = (size_t)stride * (size_t)ph;
    int fd = memfd_create(app_name, MFD_CLOEXEC);
    if (fd < 0) return NULL;
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return NULL;
    }
    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(w->shm, fd, (int32_t)size);
    close(fd);
    Shot *shot = calloc(1, sizeof(*shot));
    if (!shot) {
        wl_shm_pool_destroy(pool);
        munmap(mem, size);
        return NULL;
    }
    shot->data = mem;
    shot->size = size;
    shot->buffer = wl_shm_pool_create_buffer(pool, 0, pw, ph, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    wl_buffer_add_listener(shot->buffer, &buffer_listener, shot);

    cairo_surface_t *surface = cairo_image_surface_create_for_data(mem, CAIRO_FORMAT_ARGB32, pw, ph, stride);
    cairo_t *cr = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_set_user_data(cr, &shot_key, shot, NULL);
    cairo_scale(cr, w->scale, w->scale);
    return cr;
}

void win_end(Win *w, cairo_t *cr) {
    Shot *shot = cairo_get_user_data(cr, &shot_key);
    cairo_surface_flush(cairo_get_target(cr));
    cairo_destroy(cr);
    wl_surface_set_buffer_scale(w->surface, w->scale);
    wl_surface_attach(w->surface, shot->buffer, 0, 0);
    wl_surface_damage_buffer(w->surface, 0, 0, w->width * w->scale, w->height * w->scale);
    wl_surface_commit(w->surface);
}
