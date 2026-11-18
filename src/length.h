#ifndef PACOMMON_LENGTH_H
#define PACOMMON_LENGTH_H

#include "tunables.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* A size written with a unit, as a setting can hold it: "12px", "3mm", "0.2in", "10pt", "5%". */
typedef enum { PAC_LENGTH_PX, PAC_LENGTH_MM, PAC_LENGTH_IN, PAC_LENGTH_PT, PAC_LENGTH_PERCENT } PacLengthUnit;

typedef struct {
    double value;
    PacLengthUnit unit;
} PacLength;

/* Reads the number at the start of text: digits, with at most one '.', no sign and no exponent.
 * Returns the bytes it took, or 0 if there is no digit. It does not use strtod, which follows
 * LC_NUMERIC: in a locale that writes "1,5", strtod("1.5") stops at the dot and the setting is
 * refused. */
static inline size_t pac_number_parse(const char *text, double *out) {
    size_t i = 0;
    double value = 0;
    bool any = false;
    for (; isdigit((unsigned char)text[i]); i++) {
        value = value * 10 + (text[i] - '0');
        any = true;
    }
    if (text[i] == '.') {
        double place = 0.1;
        for (i++; isdigit((unsigned char)text[i]); i++) {
            value += (text[i] - '0') * place;
            place *= 0.1;
            any = true;
        }
    }
    if (!any) return 0;
    *out = value;
    return i;
}

/* Reads "<number><unit>" (a space between is fine, the unit is not case sensitive). The unit is
 * required, and the value must be above 0 and at most PAC_LENGTH_MAX. False for anything else
 * (a sign, no unit, an unknown unit, an exponent, hex, text after the unit); `out` is then left
 * alone. In the header, so that a tool that only checks a setting (config_file.c does) needs no
 * other file. */
static inline bool pac_length_parse(const char *text, PacLength *out) {
    static const struct { const char *name; PacLengthUnit unit; } units[] = {
        { "px", PAC_LENGTH_PX }, { "mm", PAC_LENGTH_MM }, { "in", PAC_LENGTH_IN },
        { "pt", PAC_LENGTH_PT }, { "%", PAC_LENGTH_PERCENT },
    };
    double value;
    if (!text) return false;
    size_t n = pac_number_parse(text, &value);
    if (n == 0 || !(value > 0 && value <= PAC_LENGTH_MAX)) return false;   /* also false for infinity */
    const char *end = text + n;
    while (*end == ' ') end++;
    for (size_t i = 0; i < sizeof units / sizeof units[0]; i++)
        if (strcasecmp(end, units[i].name) == 0) {
            out->value = value;
            out->unit = units[i].unit;
            return true;
        }
    return false;
}

/* Reads a text_scale (docs/configuration.md): a plain number from PAC_TEXT_SCALE_MIN to PAC_TEXT_SCALE_MAX.
 * False for anything else (empty, text after the number, out of range); `out` is then left alone. In the
 * header for the same reason as pac_length_parse: the shared-key check and a tool's own setting use one rule. */
static inline bool pac_text_scale_parse(const char *text, double *out) {
    if (!text) return false;
    double scale;
    size_t n = pac_number_parse(text, &scale);
    if (n == 0 || text[n]) return false;   /* nothing after the number */
    if (!(scale >= PAC_TEXT_SCALE_MIN && scale <= PAC_TEXT_SCALE_MAX)) return false;
    *out = scale;
    return true;
}

/* Logical pixels per millimetre of an output, from what its wl_output says: the physical width in
 * millimetres (`physical_mm`), the width of its current mode in pixels (`mode_px`) and the
 * buffer pixels per logical pixel (`scale`). Both widths are along the same side, so a rotated
 * output needs no special case. A size of 0 (virtual machines, some projectors and TVs), or one
 * that makes less than PAC_DPI_MIN or more than PAC_DPI_MAX dots per inch, is not believed: the
 * result is then PAC_DPI_DEFAULT dots per inch. */
double pac_px_per_mm(int physical_mm, int mode_px, double scale);

/* The same, but 0 where pac_px_per_mm would have guessed: for a size that is wrong on a guess (a
 * base text size on a 4K virtual screen taken as 96 dpi) and better left to the tool's own rule. */
double pac_px_per_mm_known(int physical_mm, int mode_px, double scale);

/* The length in logical pixels. `px_per_mm` is from pac_px_per_mm; `whole` is what 100% is, in
 * logical pixels (a tool says what: the screen's shorter side, for one). */
double pac_length_px(PacLength length, double px_per_mm, double whole);

/* `px` held between the two lengths, which are in the same terms as for pac_length_px. If the
 * minimum comes out above the maximum, the minimum wins: text is cut off less than it is
 * unreadable. A NULL length is no limit. A maximum under PAC_TEXT_MM_FLOOR, or a minimum over
 * PAC_TEXT_MM_CEILING, is raised or lowered to it (a typo, not a wish). */
double pac_length_clamp(double px, const PacLength *min, const PacLength *max, double px_per_mm,
                        double whole);

#endif
