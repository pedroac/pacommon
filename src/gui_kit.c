#define _GNU_SOURCE
#include "gui_kit.h"
#include "gui_img.h"
#include "fontfind.h"
#include "textdraw.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static long long kit_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static PacColor mix(PacColor a, PacColor b, double t) {
    return (PacColor){ a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

bool kit_init(Kit *k, const UiOptions *o) {
    if (!pac_font_init()) return false;
    k->opt = *o;
    k->text_scale = o->text_scale > 0 ? o->text_scale : 1;
    k->ts = k->text_scale * UI_ZOOM;
    k->high = o->high_contrast;
    k->dirty = true;
    pac_font_set_language(o->language ? o->language : "");
    k->regular = pac_font_load(o->font, PAC_FONTS_REGULAR, 0);
    k->bold = pac_font_load(NULL, PAC_FONTS_BOLD, 0);
    if (!k->regular) return false;
    if (o->speech) k->speaking = speech_open(app_name, o->language);
    const PacPalette *p = k->high ? &PAC_PALETTE_HIGH : &PAC_PALETTE_NORMAL;
    k->c.text = k->high ? p->text : (PacColor){ 1, 1, 1 };
    /* Secondary text is lifted from the shared palette: readability comes first. */
    k->c.dim = k->high ? p->dim : (PacColor){ 0.78, 0.79, 0.83 };
    k->c.bg = k->high ? p->background : (PacColor){ 0.045, 0.045, 0.06 };
    k->c.rule = k->high ? p->rule : (PacColor){ 0.52, 0.53, 0.58 };
    k->c.card = mix(p->background, p->text, k->high ? 0 : 0.10);
    k->c.hover = mix(p->background, p->text, k->high ? 0.18 : 0.16);
    k->c.warn = p->warn;
    k->c.error = p->error;
    k->c.accent = k->high ? (PacColor){ 0.45, 0.75, 1.0 } : (PacColor){ 0.55, 0.74, 1.0 };
    k->c.ok = k->high ? (PacColor){ 0.40, 1.0, 0.50 } : (PacColor){ 0.55, 0.88, 0.62 };
    return true;
}

void kit_destroy(Kit *k) {
    cairo_font_face_t *faces[2] = { k->regular, k->bold };
    speech_close();
    pac_font_finish(faces, 2);
}

/* The smallest text is UI_TEXT_MM millimetres (times text_scale) where the screen says how large it is, held
 * between text_size_min and text_size_max; everything else follows in proportion. The window never grows past
 * the screen, whatever the sizes say. */
double kit_scale_for(Kit *k, double design_h, double known, double guess, double screen_short) {
    double scale = k->text_scale * UI_ZOOM;
    if (known > 0 || k->opt.has_text_size_min || k->opt.has_text_size_max) {
        double smallest = known > 0 ? UI_TEXT_MM * known * k->text_scale : SMALLEST_TEXT_PX * scale;
        double px = pac_length_clamp(smallest, k->opt.has_text_size_min ? &k->opt.text_size_min : NULL,
                                     k->opt.has_text_size_max ? &k->opt.text_size_max : NULL, guess,
                                     screen_short > 0 ? screen_short : design_h * scale);
        scale = px / SMALLEST_TEXT_PX;
    }
    if (screen_short > 0 && design_h * scale > screen_short) scale = screen_short / design_h;
    k->ts = scale;
    return scale;
}

/* ---- speech ---- */

/* Speaks what the screen shows, never what is typed. */
void kit_speak(const Kit *k, SpeechPriority priority, const char *text) {
    if (k->speaking) speech_say_as(priority, text);
}

void kit_message_shown(Kit *k, char *msg, int *level, bool ok) {
    k->msg = msg;
    k->msg_level = level;
    k->ok_due = ok ? kit_now_ms() + OK_MESSAGE_MS : 0;
}

int kit_speech_timeout(const Kit *k) {
    int wait = k->speaking ? speech_next_ms() : -1;
    if (k->ok_due) {
        long long left = k->ok_due - kit_now_ms();
        int ms = left < 0 ? 0 : left > 100000 ? 100000 : (int)left;
        if (wait < 0 || ms < wait) wait = ms;
    }
    return wait;
}

void kit_speech_tick(Kit *k) {
    if (k->speaking) speech_tick();
    if (k->ok_due && kit_now_ms() >= k->ok_due) {   /* a good message is not kept: it was read */
        k->ok_due = 0;
        if (k->msg) k->msg[0] = '\0';
        if (k->msg_level) *k->msg_level = 0;
        k->dirty = true;
    }
}

/* ---- hits and focus ---- */

int kit_focus_position(const Kit *k) {
    for (int i = 0; i < k->nhits; i++)
        if (k->hits[i].focusable && k->hits[i].kind == k->focus_kind && k->hits[i].index == k->focus_index) return i;
    return -1;
}

void kit_scroll_clamp(KitScroll *s) {
    double max = s->content_h > s->view_h ? s->content_h - s->view_h : 0;
    if (s->scroll > max) s->scroll = max;
    if (s->scroll < 0) s->scroll = 0;
}

static void scroll_bar(Kit *k, KitScroll *v, cairo_t *cr, double right, double top) {
    v->sb_on = v->content_h > v->view_h && v->view_h > 0;
    if (!v->sb_on) {
        v->sb_drag = false;
        return;
    }
    double s = k->ts;
    v->sb_th = v->view_h * v->view_h / v->content_h;
    v->sb_ty = top + v->view_h * v->scroll / v->content_h;
    v->sb_top = top;
    v->sb_x = right - 18 * s;   /* the part that takes the pointer is wider than what is drawn */
    kit_color(cr, k->c.rule, 1);
    kit_round_rect(cr, right - 6 * s, v->sb_ty, 3 * s, v->sb_th, 1.5 * s);
    cairo_fill(cr);
}

void kit_scroll_end(Kit *k, KitScroll *v, cairo_t *cr, double right, double top, double content_h) {
    v->content_h = content_h;
    double max = content_h > v->view_h ? content_h - v->view_h : 0;
    if (v->scroll > max || v->scroll < 0) {
        v->scroll = v->scroll > max ? max : 0;
        k->dirty = true;
    }
    scroll_bar(k, v, cr, right, top);
}

bool kit_scroll_pointer(Kit *k, KitScroll *v, double x, double y, int button) {
    if (!v->sb_on) return false;
    double room = v->view_h - v->sb_th;
    if (button == 3) {
        bool was = v->sb_drag;
        v->sb_drag = false;
        return was;
    }
    if (button == 0 && v->sb_drag) {
        if (room > 0) v->scroll = (y - v->sb_grab - v->sb_top) / room * (v->content_h - v->view_h);
        kit_scroll_clamp(v);
        k->dirty = true;
        return true;
    }
    if (button == 1 && x >= v->sb_x && y >= v->sb_top && y <= v->sb_top + v->view_h) {
        v->sb_drag = true;
        if (y >= v->sb_ty && y <= v->sb_ty + v->sb_th) v->sb_grab = y - v->sb_ty;
        else v->sb_grab = v->sb_th / 2;   /* clicked on the track: the thumb comes to the pointer */
        if (room > 0) v->scroll = (y - v->sb_grab - v->sb_top) / room * (v->content_h - v->view_h);
        kit_scroll_clamp(v);
        k->dirty = true;
        return true;
    }
    return false;
}

void kit_clamp_scroll(Kit *k) { kit_scroll_clamp(&k->sc); }

void kit_draw_scrollbar(Kit *k, cairo_t *cr, double W, double top) { scroll_bar(k, &k->sc, cr, W, top); }

void kit_end_scroll(Kit *k, cairo_t *cr, double W, double top, double content_h) { kit_scroll_end(k, &k->sc, cr, W, top, content_h); }

bool kit_scrollbar_pointer(Kit *k, double x, double y, int button) { return kit_scroll_pointer(k, &k->sc, x, y, button); }

void kit_move_focus(Kit *k, int step) {
    int n = k->nhits, cur = kit_focus_position(k);
    if (n == 0) return;
    int i = cur < 0 ? (step > 0 ? -1 : n) : cur;
    for (int tries = 0; tries < n; tries++) {
        i += step;
        if (i < 0 || i >= n) break;
        if (k->hits[i].focusable) {
            k->focus_kind = k->hits[i].kind;
            k->focus_index = k->hits[i].index;
            return;
        }
    }
}

int kit_hit_at(const Kit *k, double x, double y) {
    for (int i = k->nhits - 1; i >= 0; i--) {
        const Hit *h = &k->hits[i];
        if (x >= h->x && x < h->x + h->w && y >= h->y && y < h->y + h->h) return i;
    }
    return -1;
}

/* ---- drawing ---- */

void kit_color(cairo_t *cr, PacColor c, double alpha) { cairo_set_source_rgba(cr, c.r, c.g, c.b, alpha); }

void kit_round_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void kit_set_font(Kit *k, cairo_t *cr, bool bold, double px) {
    cairo_set_font_face(cr, bold && k->bold ? k->bold : k->regular);
    cairo_set_font_size(cr, px * k->ts);
}

/* Draws text with its middle at cy, fitted to max_w; returns its width. */
double kit_label(Kit *k, cairo_t *cr, double x, double cy, double max_w, const char *s, bool bold, double px,
                    PacColor c) {
    char fit[256];
    kit_set_font(k, cr, bold, px);
    pac_font_fit(cr, s, max_w, false, fit, sizeof(fit));
    kit_color(cr, c, 1);
    cairo_move_to(cr, x, cy + px * k->ts * 0.36);
    double w = pac_font_text(cr, fit, true);
    cairo_new_path(cr);   /* no current point is left for the next shape to be joined to */
    return w;
}

double kit_label_width(Kit *k, cairo_t *cr, const char *s, bool bold, double px) {
    kit_set_font(k, cr, bold, px);
    return pac_font_width(cr, s);
}

void kit_add_hit(Kit *k, double x, double y, double w, double h, int kind, int index, bool focusable,
                    double clip0, double clip1) {
    if (k->nhits >= MAX_HITS) return;
    if (y < clip0) { h -= clip0 - y; y = clip0; }
    if (y + h > clip1) h = clip1 - y;
    if (h <= 0) return;
    k->hits[k->nhits++] = (Hit){ x, y, w, h, kind, index, focusable };
}

bool kit_is_focus(const Kit *k, int kind, int index) {
    return k->kbd && k->focus_kind == kind && k->focus_index == index;
}

bool kit_is_hover(const Kit *k, int kind, int index) {
    return k->hover_kind == kind && k->hover_index == index;
}

void kit_focus_ring(Kit *k, cairo_t *cr, double x, double y, double w, double h, double r) {
    kit_color(cr, k->c.accent, 1);
    cairo_set_line_width(cr, 2 * k->ts);
    kit_round_rect(cr, x + k->ts, y + k->ts, w - 2 * k->ts, h - 2 * k->ts, r);
    cairo_stroke(cr);
}

/* A button: the primary one is filled with the accent color. */
void kit_button(Kit *k, cairo_t *cr, double x, double y, double w, double h, const char *text, bool primary,
                   bool enabled, int kind, int index, bool focusable, double clip0, double clip1) {
    bool hover = enabled && kit_is_hover(k, kind, index);
    if (primary) kit_color(cr, k->c.accent, enabled ? (hover ? 0.85 : 1) : 0.35);
    else kit_color(cr, hover ? k->c.hover : k->c.card, 1);
    kit_round_rect(cr, x, y, w, h, 8 * k->ts);
    cairo_fill(cr);
    if (!primary) {
        kit_color(cr, k->c.rule, 1);
        cairo_set_line_width(cr, k->ts);
        kit_round_rect(cr, x + 0.5, y + 0.5, w - 1, h - 1, 8 * k->ts);
        cairo_stroke(cr);
    }
    double tw = kit_label_width(k, cr, text, true, 15);
    if (tw > w - 12 * k->ts) tw = w - 12 * k->ts;
    kit_label(k, cr, x + (w - tw) / 2, y + h / 2, w - 12 * k->ts, text, true, 15,
          primary ? k->c.bg : (enabled ? k->c.text : k->c.dim));
    if (enabled) kit_add_hit(k, x, y, w, h, kind, index, focusable, clip0, clip1);
    if (enabled && kit_is_focus(k, kind, index)) kit_focus_ring(k, cr, x - 2 * k->ts, y - 2 * k->ts, w + 4 * k->ts, h + 4 * k->ts, 9 * k->ts);
}

void kit_toggle_switch(Kit *k, cairo_t *cr, double x, double cy, bool on) {
    double w = 40 * k->ts, h = 22 * k->ts;
    kit_color(cr, on ? k->c.accent : k->c.rule, 1);
    kit_round_rect(cr, x, cy - h / 2, w, h, h / 2);
    cairo_fill(cr);
    kit_color(cr, on ? k->c.bg : k->c.text, 1);
    cairo_arc(cr, on ? x + w - h / 2 : x + h / 2, cy, h / 2 - 3 * k->ts, 0, 2 * M_PI);
    cairo_fill(cr);
}


double kit_pad_for(const Kit *k) {
    double s = k->ts, W = win_width(k->win), pad = 20 * s;
    if (W - 2 * pad > UI_MAX_W * s) pad = (W - UI_MAX_W * s) / 2;
    return pad;
}

double kit_pad(Kit *k) {
    k->content_pad = kit_pad_for(k);
    return k->content_pad;
}

double kit_pad_wide(Kit *k, int *columns) {
    double s = k->ts, W = win_width(k->win), pad = 20 * s, cap = UI_MAX_W * s;
    *columns = 1;
    if (W >= UI_TWO_COLUMNS_MIN * s) {
        *columns = 2;
        cap = UI_MAX_W_WIDE * s;
    }
    if (W - 2 * pad > cap) pad = (W - cap) / 2;
    k->content_pad = pad;
    return pad;
}

void kit_rule(Kit *k, cairo_t *cr, double x, double y, double w) {
    kit_color(cr, k->c.rule, 0.6);
    cairo_rectangle(cr, x, y, w, k->ts >= 1.6 ? 2 : 1);
    cairo_fill(cr);
}

/* The window's body and border, its title and the button that closes it. Returns where the content starts. */
double kit_frame(Kit *k, cairo_t *cr, const char *title, int close_kind) {
    double s = k->ts, W = win_width(k->win), H = win_height(k->win), pad = k->content_pad > 0 ? k->content_pad : 20 * s;
    bool normal = win_toplevel(k->win);   /* a normal window: square, opaque, and closed by the compositor's own means */
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    if (normal) {
        kit_color(cr, k->c.bg, 1);
        cairo_rectangle(cr, 0, 0, W, H);
        cairo_fill(cr);
    } else {
        kit_color(cr, k->c.bg, 0.98);
        kit_round_rect(cr, 0, 0, W, H, 14 * s);
        cairo_fill(cr);
        kit_color(cr, k->c.rule, 1);
        cairo_set_line_width(cr, s);
        kit_round_rect(cr, 0.5, 0.5, W - 1, H - 1, 14 * s);
        cairo_stroke(cr);
    }

    double y = 20 * s, r = 18 * s;
    kit_label(k, cr, pad, y + r, W - 2 * pad - (normal ? 0 : 2 * r + 8 * s), title, true, 20, k->c.text);
    if (normal) return y + 2 * r + 10 * s;
    double cx = W - pad - r, cy = y + r;
    if (kit_is_hover(k, close_kind, 0)) {
        kit_color(cr, k->c.hover, 1);
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    kit_color(cr, k->c.text, 1);
    cairo_set_line_width(cr, 2.2 * s);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, cx - 6 * s, cy - 6 * s); cairo_line_to(cr, cx + 6 * s, cy + 6 * s);
    cairo_move_to(cr, cx + 6 * s, cy - 6 * s); cairo_line_to(cr, cx - 6 * s, cy + 6 * s);
    cairo_stroke(cr);
    kit_add_hit(k, cx - r, cy - r, 2 * r, 2 * r, close_kind, 0, false, 0, H);
    return y + 2 * r + 10 * s;
}

/* ---- pictures ---- */

/* A picture, made at most side across (square, from the middle) so that drawing it is cheap; NULL if it is not one that can be read. */
cairo_surface_t *kit_load_square(const char *path, int side) {
    cairo_surface_t *src = img_load(path, side);   /* PNG, JPEG or WebP */
    if (!src) return NULL;
    int w = cairo_image_surface_get_width(src), h = cairo_image_surface_get_height(src);
    if (w < 1 || h < 1) { cairo_surface_destroy(src); return NULL; }
    cairo_surface_t *dst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, side, side);
    cairo_t *cr = cairo_create(dst);
    double m = w < h ? w : h, sc = side / m;
    cairo_scale(cr, sc, sc);
    cairo_set_source_surface(cr, src, -(w - m) / 2, -(h - m) / 2);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(src);
    return dst;
}

/* A picture in a circle: the surface, or a person's shape where there is none. */
void kit_round_picture(Kit *k, cairo_t *cr, cairo_surface_t *img, double cx, double cy, double r) {
    cairo_save(cr);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_clip(cr);
    if (img) {
        double w = cairo_image_surface_get_width(img);
        cairo_translate(cr, cx - r, cy - r);
        cairo_scale(cr, 2 * r / w, 2 * r / w);
        cairo_set_source_surface(cr, img, 0, 0);
        cairo_paint(cr);
    } else {
        kit_color(cr, k->c.hover, 1);
        cairo_paint(cr);
        kit_color(cr, k->c.dim, 1);
        cairo_arc(cr, cx, cy - r * 0.22, r * 0.3, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, cx, cy + r * 0.85, r * 0.55, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_restore(cr);
}

void kit_sparkline(Kit *k, cairo_t *cr, double x, double y, double w, double h, const double *v, int n, int capacity,
                   double min_span, double min_rel, bool zero_based, PacColor c) {
    if (n < 2 || capacity < 2) return;
    double lo = v[0], hi = v[0];
    for (int i = 1; i < n; i++) {
        if (v[i] < lo) lo = v[i];
        if (v[i] > hi) hi = v[i];
    }
    double need = fmax(min_span, min_rel * fabs(hi)), span = hi - lo;
    if (zero_based) {
        lo = 0;
        span = fmax(hi, need);
    } else if (span < need) {
        lo = (hi + lo) / 2 - need / 2;
        span = need;
    }
    double s = k->ts, px = 0, py = 0;
    cairo_save(cr);
    cairo_set_line_width(cr, 2 * s);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    kit_color(cr, c, 1);
    for (int i = 0; i < n; i++) {
        px = x + w - w * (n - 1 - i) / (capacity - 1);
        py = y + h - h * (v[i] - lo) / span;
        if (i == 0) cairo_move_to(cr, px, py);
        else cairo_line_to(cr, px, py);
    }
    cairo_stroke(cr);
    cairo_arc(cr, px, py, 3 * s, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
}

void kit_gauge(Kit *k, cairo_t *cr, double x, double y, double w, double h, double fraction, double tick, PacColor fill) {
    kit_color(cr, k->c.rule, 1);
    kit_round_rect(cr, x, y, w, h, h / 2);
    cairo_fill(cr);
    if (fraction > 0) {
        kit_color(cr, fill, 1);
        kit_round_rect(cr, x, y, fmax(h, w * fmin(1, fraction)), h, h / 2);
        cairo_fill(cr);
    }
    if (tick > 0 && tick < 1) {
        kit_color(cr, k->c.text, 1);
        cairo_rectangle(cr, x + w * tick - 1 * k->ts, y - 2 * k->ts, 2 * k->ts, h + 4 * k->ts);
        cairo_fill(cr);
    }
}
