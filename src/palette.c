#include "palette.h"
#include <math.h>
#include <stdio.h>

/* The look every pac tool shares. The normal palette is calm and dim; the high one is black and
 * white, with nothing dimmed. The contrast of each color is checked by pacommon's tests. */
const PacPalette PAC_PALETTE_NORMAL = {
    .text = { 0.95, 0.95, 0.95 },
    .label = { 0.49, 0.50, 0.54 },
    .label_focus = { 0.52, 0.53, 0.57 },
    .dim = { 0.55, 0.56, 0.60 },
    .faint = { 0.50, 0.51, 0.55 },
    .rule = { 0.38, 0.39, 0.43 },
    .warn = { 0.85, 0.74, 0.42 },
    .error = { 0.87, 0.47, 0.44 },
    .background = { 0.075, 0.075, 0.090 },
    .busy_alpha = 0.5,
};

const PacPalette PAC_PALETTE_HIGH = {
    .text = { 1, 1, 1 },
    .label = { 0.68, 0.68, 0.68 },
    .label_focus = { 0.85, 0.85, 0.85 },
    .dim = { 0.85, 0.85, 0.85 },
    .faint = { 0.75, 0.75, 0.75 },
    .rule = { 0.62, 0.62, 0.62 },
    .warn = { 1.0, 0.88, 0.30 },
    .error = { 1.0, 0.45, 0.40 },
    .background = { 0, 0, 0 },
    .busy_alpha = 1.0,
};

static double linear(double c) {
    return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

static double luminance(PacColor c) {
    return 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b);
}

double pac_contrast(PacColor a, PacColor b) {
    double la = luminance(a), lb = luminance(b);
    return la > lb ? (la + 0.05) / (lb + 0.05) : (lb + 0.05) / (la + 0.05);
}

static bool check(const char *name, PacColor c, PacColor bg, double min, char *why, size_t size) {
    double ratio = pac_contrast(c, bg);
    if (ratio >= min) return true;
    snprintf(why, size, "%s is %.2f:1 against the background, below %.1f:1", name, ratio, min);
    return false;
}

bool pac_palette_contrast_ok(const PacPalette *p, char *why, size_t size) {
    static const struct { const char *name; size_t off; double min; } text[] = {
        { "text", offsetof(PacPalette, text), 4.5 },
        { "label", offsetof(PacPalette, label), 4.5 },
        { "label_focus", offsetof(PacPalette, label_focus), 4.5 },
        { "dim", offsetof(PacPalette, dim), 4.5 },
        { "faint", offsetof(PacPalette, faint), 4.5 },
        { "warn", offsetof(PacPalette, warn), 4.5 },
        { "error", offsetof(PacPalette, error), 4.5 },
        { "rule", offsetof(PacPalette, rule), 3.0 },
    };
    for (size_t i = 0; i < sizeof text / sizeof *text; i++) {
        const PacColor *c = (const PacColor *)((const char *)p + text[i].off);
        if (!check(text[i].name, *c, p->background, text[i].min, why, size)) return false;
    }
    return true;
}

/* Viénot, Brettel and Mollon (1999): sRGB linear -> LMS, one cone replaced, and back. The
 * matrices are the ones for the Hunt-Pointer-Estevez space with sRGB primaries. */
static PacColor simulate(PacColor c, int kind) {
    double r = linear(c.r), g = linear(c.g), b = linear(c.b);
    double L = 0.4002 * r + 0.7076 * g - 0.0808 * b;
    double M = -0.2263 * r + 1.1653 * g + 0.0457 * b;
    double S = 0.9182 * b;
    if (kind == 0) L = 2.02344 * M - 2.52581 * S;
    else if (kind == 1) M = 0.494207 * L + 1.24827 * S;
    else S = -0.395913 * L + 0.801109 * M;
    double lr = 1.8601 * L - 1.1295 * M + 0.2199 * S;
    double lg = 0.3612 * L + 0.6388 * M;
    double lb = 1.0891 * S;
    return (PacColor){ lr < 0 ? 0 : lr > 1 ? 1 : lr,
                       lg < 0 ? 0 : lg > 1 ? 1 : lg,
                       lb < 0 ? 0 : lb > 1 ? 1 : lb };
}

bool pac_palette_meanings_apart(const PacPalette *p, char *why, size_t size) {
    static const char *const names[] = { "protanopia", "deuteranopia", "tritanopia" };
    for (int k = 0; k < 3; k++) {
        PacColor a = simulate(p->warn, k), b = simulate(p->error, k);
        double dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
        double distance = sqrt(dr * dr + dg * dg + db * db);
        if (distance < 0.1) {
            snprintf(why, size, "warn and error are %.2f apart with %s, below 0.1", distance, names[k]);
            return false;
        }
    }
    return true;
}
