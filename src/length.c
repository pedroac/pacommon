#include "length.h"
#include "tunables.h"
#include <math.h>
#include <string.h>

#define MM_PER_INCH 25.4
#define POINTS_PER_INCH 72.0

double pac_px_per_mm_known(int physical_mm, int mode_px, double scale) {
    if (physical_mm <= 0 || mode_px <= 0 || !(scale >= 1)) return 0;
    double dpi = (double)mode_px / physical_mm * MM_PER_INCH;
    if (dpi < PAC_DPI_MIN || dpi > PAC_DPI_MAX) return 0;
    return (double)mode_px / physical_mm / scale;
}

double pac_px_per_mm(int physical_mm, int mode_px, double scale) {
    double known = pac_px_per_mm_known(physical_mm, mode_px, scale);
    return known > 0 ? known : PAC_DPI_DEFAULT / MM_PER_INCH / (scale >= 1 ? scale : 1);
}

double pac_length_px(PacLength length, double px_per_mm, double whole) {
    switch (length.unit) {
    case PAC_LENGTH_PX: return length.value;
    case PAC_LENGTH_MM: return length.value * px_per_mm;
    case PAC_LENGTH_IN: return length.value * px_per_mm * MM_PER_INCH;
    case PAC_LENGTH_PT: return length.value * px_per_mm * MM_PER_INCH / POINTS_PER_INCH;
    case PAC_LENGTH_PERCENT: return length.value / 100 * whole;
    }
    return length.value;
}

double pac_length_clamp(double px, const PacLength *min, const PacLength *max, double px_per_mm,
                        double whole) {
    if (max) {
        double top = pac_length_px(*max, px_per_mm, whole);
        double floor_px = PAC_TEXT_MM_FLOOR * px_per_mm;
        if (top < floor_px) top = floor_px;               /* a maximum no one could read is a mistake */
        if (px > top) px = top;
    }
    if (min) {
        double bottom = pac_length_px(*min, px_per_mm, whole);
        double ceiling_px = PAC_TEXT_MM_CEILING * px_per_mm;
        if (bottom > ceiling_px) bottom = ceiling_px;     /* so is a minimum that is a billboard */
        if (px < bottom) px = bottom;
    }
    return px;
}
