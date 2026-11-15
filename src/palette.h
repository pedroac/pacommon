#ifndef PACOMMON_PALETTE_H
#define PACOMMON_PALETTE_H

#include <stdbool.h>
#include <stddef.h>

/* A color, 0..1 per channel, sRGB. */
typedef struct { double r, g, b; } PacColor;

/* The colors of a pac tool: the look every tool shares (principle 33). */
typedef struct {
    PacColor text, label, label_focus, dim, faint, rule, warn, error, background;
    double busy_alpha;     /* how much is left of everything while the tool works */
} PacPalette;

extern const PacPalette PAC_PALETTE_NORMAL;
extern const PacPalette PAC_PALETTE_HIGH;

/* WCAG 2 contrast ratio, 1 to 21. */
double pac_contrast(PacColor a, PacColor b);

/* True if every text color is at least 4.5:1 against the background and the rule (graphics) at
 * least 3:1. `why` gets the first failure, for the test's message. */
bool pac_palette_contrast_ok(const PacPalette *p, char *why, size_t size);

/* True if warn and error stay apart (distance above 0.1 in linear RGB) as seen with protanopia,
 * deuteranopia and tritanopia. `why` gets the first failure. */
bool pac_palette_meanings_apart(const PacPalette *p, char *why, size_t size);

#endif
