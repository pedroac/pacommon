#include "textlayout.h"
#include <string.h>

#ifndef NO_I18N
#include <fribidi.h>
#include <hb-ft.h>
#include <hb.h>
#endif

/* ---- UTF-8 ---- */

/* The character at s (not the end): its length in bytes, at least 1, and the code point in *cp.
 * A broken sequence, an overlong one, a surrogate or a value over U+10FFFF is U+FFFD, and eats
 * the bytes that were read (never the end of the string). */
static size_t decode(const char *s, uint32_t *cp) {
    const unsigned char *p = (const unsigned char *)s;
    uint32_t c = p[0];
    int extra = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
    if (extra < 0) {
        *cp = 0xFFFD;
        return 1;
    }
    if (extra) c &= 0x3F >> extra;
    int i = 1;
    for (; i <= extra && (p[i] & 0xC0) == 0x80; i++) c = c << 6 | (p[i] & 0x3F);
    if (i <= extra || (extra == 1 && c < 0x80) || (extra == 2 && c < 0x800) || (extra == 3 && c < 0x10000) ||
        c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) {
        *cp = 0xFFFD;
        return (size_t)i;
    }
    *cp = c;
    return (size_t)i;
}

/* ---- where a line may break ---- */

static bool is_space(uint32_t c) {
    return c == ' ' || c == '\t' || c == 0x3000 || c == 0x200B || (c >= 0x2000 && c <= 0x2006) ||
           (c >= 0x2008 && c <= 0x200A) || c == 0x1680;
}

/* Kana, ideographs and Hangul syllables: a line may end between two of them. */
static bool is_ideographic(uint32_t c) {
    return (c >= 0x2E80 && c <= 0x9FFF) || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) ||
           (c >= 0xFF00 && c <= 0xFFEF) || (c >= 0x20000 && c <= 0x3FFFF);
}

/* Closing marks, and the small and prolonged kana: they do not start a line. */
static bool no_line_start(uint32_t c) {
    static const uint32_t set[] = { 0x3001, 0x3002, 0x3009, 0x300B, 0x300D, 0x300F, 0x3011, 0x3015, 0x3017, 0x3019,
        0x301C, 0x30FC, 0x3005, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083, 0x3085, 0x3087, 0x308E,
        0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30C3, 0x30E3, 0x30E5, 0x30E7, 0x30EE, 0xFF01, 0xFF09, 0xFF0C,
        0xFF0E, 0xFF1A, 0xFF1B, 0xFF1F, 0xFF3D, 0xFF5D, 0xFF60 };
    for (size_t i = 0; i < sizeof set / sizeof set[0]; i++)
        if (set[i] == c) return true;
    return false;
}

/* Opening marks: they do not end a line. */
static bool no_line_end(uint32_t c) {
    static const uint32_t set[] = { 0x3008, 0x300A, 0x300C, 0x300E, 0x3010, 0x3014, 0x3016, 0x3018, 0x301D,
        0xFF08, 0xFF3B, 0xFF5B, 0xFF5F };
    for (size_t i = 0; i < sizeof set / sizeof set[0]; i++)
        if (set[i] == c) return true;
    return false;
}

bool pac_text_can_break(uint32_t prev, uint32_t next) {
    if (is_space(next)) return false;                     /* the space stays with the word before it */
    if (is_space(prev)) return true;
    if ((prev == '-' || prev == 0x2010 || prev == 0x2013) && next > ' ') return true;
    if (is_ideographic(prev) && is_ideographic(next)) return !no_line_start(next) && !no_line_end(prev);
    return false;   /* Thai, Lao, Khmer and Myanmar have no spaces, and no dictionary here: see pac_text_fit */
}

/* ---- clusters ---- */

enum { H_NONE, H_L, H_V, H_T, H_LV, H_LVT };

static int hangul_class(uint32_t c) {
    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0xA960 && c <= 0xA97C)) return H_L;
    if ((c >= 0x1160 && c <= 0x11A7) || (c >= 0xD7B0 && c <= 0xD7C6)) return H_V;
    if ((c >= 0x11A8 && c <= 0x11FF) || (c >= 0xD7CB && c <= 0xD7FB)) return H_T;
    if (c >= 0xAC00 && c <= 0xD7A3) return (c - 0xAC00) % 28 == 0 ? H_LV : H_LVT;
    return H_NONE;
}

static bool hangul_joins(int have, int next) {
    switch (have) {
    case H_L: return next == H_L || next == H_V || next == H_LV || next == H_LVT;
    case H_V: case H_LV: return next == H_V || next == H_T;
    case H_T: case H_LVT: return next == H_T;
    default: return false;
    }
}

static bool is_regional(uint32_t c) { return c >= 0x1F1E6 && c <= 0x1F1FF; }

static bool is_pictograph(uint32_t c) {
    return (c >= 0x2600 && c <= 0x27BF) || (c >= 0x1F000 && c <= 0x1FAFF) || c == 0x2764 || c == 0x2B50;
}

/* A mark, a joiner or a selector: it belongs to the letter before it. */
static bool is_extend(uint32_t c) {
    if (c == 0x200C || c == 0x200D || (c >= 0x1F3FB && c <= 0x1F3FF) || (c >= 0xE0020 && c <= 0xE007F) ||
        c == 0x0E33 || c == 0x0EB3)   /* Thai and Lao sara am: a letter that is written after its consonant */
        return true;
#ifndef NO_I18N
    switch (hb_unicode_general_category(hb_unicode_funcs_get_default(), c)) {
    case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
    case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
    case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
        return true;
    default:
        return false;
    }
#else
    return false;
#endif
}

size_t pac_text_next_cluster(const char *s, size_t pos) {
    if (!s[pos]) return pos;
    uint32_t cp;
    size_t end = pos + decode(s + pos, &cp);
    if (cp == '\r' && s[end] == '\n') return end + 1;

    int hangul = hangul_class(cp);
    bool pair = is_regional(cp), joined = false;
    while (s[end]) {
        uint32_t next;
        size_t n = decode(s + end, &next);
        bool join = is_extend(next);
        if (!join && joined && is_pictograph(next)) join = true;        /* an emoji after a joiner */
        if (!join && pair && is_regional(next)) { join = true; pair = false; }
        if (!join && hangul && hangul_joins(hangul, hangul_class(next))) {
            join = true;
            hangul = hangul_class(next);
        }
        if (!join) break;
        joined = next == 0x200D;
        end += n;
    }
    return end;
}

size_t pac_text_prev_cluster(const char *s, size_t pos) {
    size_t at = 0, before = 0;
    while (at < pos && s[at]) {
        before = at;
        at = pac_text_next_cluster(s, at);
    }
    return before;
}

/* ---- the same for every build: how much of a paragraph fits ---- */

/* The width of the start of utf8 that ends at cuts[k], without the spaces at its end. */
static double prefix_width(const PacTextOptions *o, const char *utf8, const size_t *cuts, size_t k) {
    static PacTextLine line;
    while (k > 0) {   /* a cluster that is one space is not part of the width */
        uint32_t c;
        decode(utf8 + cuts[k - 1], &c);
        if (cuts[k] - cuts[k - 1] != 1 || !is_space(c)) break;
        k--;
    }
    size_t end = cuts[k];
    char part[PAC_TEXT_CHARS_MAX * 4 + 1];
    if (end == 0 || end >= sizeof part) return 0;
    memcpy(part, utf8, end);
    part[end] = '\0';
    return pac_text_layout(o, part, &line) ? line.width : 0;
}

size_t pac_text_fit(const PacTextOptions *o, const char *utf8, double max_em, double *width_em) {
    size_t cuts[PAC_TEXT_CHARS_MAX + 1], count = 0, len = strlen(utf8);
    cuts[0] = 0;
    while (cuts[count] < len && count < PAC_TEXT_CHARS_MAX && cuts[count] < PAC_TEXT_CHARS_MAX * 4)
        cuts[count + 1] = pac_text_next_cluster(utf8, cuts[count]), count++;
    if (width_em) *width_em = 0;
    if (count == 0) return 0;

    /* The most clusters that fit: widths only grow as clusters are added, so bisect. */
    size_t fits = 0, high = count;
    while (fits < high) {
        size_t mid = (fits + high + 1) / 2;
        if (prefix_width(o, utf8, cuts, mid) <= max_em) fits = mid;
        else high = mid - 1;
    }
    if (fits == 0) {                                           /* not even one: it goes alone, however wide */
        if (width_em) *width_em = prefix_width(o, utf8, cuts, 1);
        return cuts[1];
    }
    /* All of it (what is measured is all of it, or the start of what is too long to hold), or the last place
     * where the line may end, or else after the last cluster that fits. */
    size_t cut = cuts[fits];
    if (fits < count) {
        for (size_t k = fits; k >= 1; k--) {
            uint32_t prev = 0, next;
            for (size_t at = cuts[k - 1]; at < cuts[k];) at += decode(utf8 + at, &prev);
            decode(utf8 + cuts[k], &next);
            if (pac_text_can_break(prev, next)) {
                fits = k;
                cut = cuts[k];
                break;
            }
        }
    }
    if (width_em) *width_em = prefix_width(o, utf8, cuts, fits);
    return cut;
}

#ifndef NO_I18N

/* ---- shaping and the order of a line (HarfBuzz and FriBidi) ---- */

typedef struct {
    FT_Face face;
    hb_font_t *font;
    int upem;
} Shaper;

static Shaper shapers[PAC_TEXT_FONTS_MAX];
static int shaper_count;
static hb_buffer_t *buffer;

static Shaper *shaper_for(FT_Face face) {
    for (int i = 0; i < shaper_count; i++)
        if (shapers[i].face == face) return &shapers[i];
    if (shaper_count == PAC_TEXT_FONTS_MAX || face->units_per_EM <= 0) return NULL;
    hb_face_t *hb_face = hb_ft_face_create_referenced(face);
    if (!hb_face) return NULL;
    hb_font_t *font = hb_font_create(hb_face);
    hb_face_destroy(hb_face);
    if (!font) return NULL;
    /* One unit of the font is one unit of the scale, so the advances come back in font units. */
    hb_font_set_scale(font, face->units_per_EM, face->units_per_EM);
    Shaper *s = &shapers[shaper_count++];
    s->face = face;
    s->font = font;
    s->upem = face->units_per_EM;
    return s;
}

void pac_text_finish(void) {
    for (int i = 0; i < shaper_count; i++) hb_font_destroy(shapers[i].font);
    shaper_count = 0;
    if (buffer) hb_buffer_destroy(buffer);
    buffer = NULL;
}

/* Languages written from right to left. */
bool pac_text_language_rtl(const char *lang) {
    static const char *const rtl[] = { "ar", "he", "iw", "fa", "ur", "ps", "sd", "ug", "yi", "dv" };
    if (!lang) return false;
    for (size_t i = 0; i < sizeof rtl / sizeof rtl[0]; i++) {
        size_t n = strlen(rtl[i]);
        if (strncmp(lang, rtl[i], n) == 0 && (lang[n] == '\0' || lang[n] == '_' || lang[n] == '-' || lang[n] == '.'))
            return true;
    }
    return false;
}

/* Characters that take the face of the one before them: marks, joiners, selectors. */
static bool follows_letter(uint32_t c) {
    return is_extend(c) || c == 0x200B || c == 0x2060 || (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xE0100 && c <= 0xE01EF) ||
           c == 0x034F || (c >= 0x200E && c <= 0x200F);
}

typedef struct {
    int start, end, level;
    FT_Face face;
    hb_script_t script;
} Run;

/* Rule L2 of the Unicode bidirectional algorithm, on whole runs. */
static void reorder(Run *runs, int count) {
    int highest = 0, lowest_odd = 255;
    for (int i = 0; i < count; i++) {
        if (runs[i].level > highest) highest = runs[i].level;
        if ((runs[i].level & 1) && runs[i].level < lowest_odd) lowest_odd = runs[i].level;
    }
    for (int level = highest; level >= lowest_odd && level >= 1; level--) {
        for (int i = 0; i < count;) {
            if (runs[i].level < level) {
                i++;
                continue;
            }
            int j = i;
            while (j < count && runs[j].level >= level) j++;
            for (int a = i, b = j - 1; a < b; a++, b--) {
                Run tmp = runs[a];
                runs[a] = runs[b];
                runs[b] = tmp;
            }
            i = j;
        }
    }
}

bool pac_text_layout(const PacTextOptions *o, const char *utf8, PacTextLine *out) {
    out->count = 0;
    out->chars = 0;
    out->width = 0;
    out->rtl = false;
    if (!o || !o->fallback || !utf8 || !utf8[0]) return false;

    uint32_t cps[PAC_TEXT_CHARS_MAX];
    FT_Face faces[PAC_TEXT_CHARS_MAX];
    hb_script_t scripts[PAC_TEXT_CHARS_MAX];
    int n = 0;
    const char *rest = utf8;
    while (*rest && n < PAC_TEXT_CHARS_MAX) {
        uint32_t cp;
        rest += decode(rest, &cp);
        cps[n++] = cp;
    }
    /* More than it holds: cut at a cluster, never in the middle of one. */
    if (*rest && n > 1) {
        while (n > 1 && follows_letter(cps[n - 1])) n--;
        if (n > 1) n--;   /* the letter whose marks have no room */
    }
    out->chars = (size_t)n;

    for (int i = 0; i < n; i++) {
        FT_Face face = NULL;
        if (i > 0 && follows_letter(cps[i]) &&
            (FT_Get_Char_Index(faces[i - 1], cps[i]) != 0 || cps[i] == 0x200C || cps[i] == 0x200D))
            face = faces[i - 1];
        else if (o->pick)
            face = o->pick(cps[i], o->data);
        faces[i] = face ? face : o->fallback;
    }

    /* The script of each character; the common and the inherited take the one before (or after, at
     * the start), so that a space or a mark does not split a run. */
    hb_script_t last = HB_SCRIPT_INVALID;
    for (int i = 0; i < n; i++) {
        hb_script_t s = hb_unicode_script(hb_unicode_funcs_get_default(), cps[i]);
        if (s == HB_SCRIPT_COMMON || s == HB_SCRIPT_INHERITED || s == HB_SCRIPT_UNKNOWN) s = last == HB_SCRIPT_INVALID ? HB_SCRIPT_COMMON : last;
        else last = s;
        scripts[i] = s;
    }
    int lead = 0;   /* a line that starts with common characters takes the script of the first that has one */
    while (lead < n && scripts[lead] == HB_SCRIPT_COMMON) lead++;
    for (int i = 0; i < lead && lead < n; i++) scripts[i] = scripts[lead];

    /* The order: the levels of the bidirectional algorithm, for the base direction asked for, or
     * the line's own (its first strong letter), or the language's if it has none. */
    FriBidiCharType types[PAC_TEXT_CHARS_MAX];
    FriBidiBracketType brackets[PAC_TEXT_CHARS_MAX];
    FriBidiLevel levels[PAC_TEXT_CHARS_MAX];
    fribidi_get_bidi_types((const FriBidiChar *)cps, n, types);
    fribidi_get_bracket_types((const FriBidiChar *)cps, n, types, brackets);
    FriBidiParType base = o->direction == PAC_TEXT_RTL ? FRIBIDI_PAR_RTL : o->direction == PAC_TEXT_LTR ? FRIBIDI_PAR_LTR
                        : fribidi_get_par_direction(types, n);
    if (base == FRIBIDI_PAR_ON) base = pac_text_language_rtl(o->language) ? FRIBIDI_PAR_RTL : FRIBIDI_PAR_LTR;
    if (fribidi_get_par_embedding_levels_ex(types, brackets, n, &base, levels) == 0) {
        memset(levels, 0, (size_t)n);
        base = FRIBIDI_PAR_LTR;
    }
    out->rtl = FRIBIDI_IS_RTL(base) != 0;

    Run runs[PAC_TEXT_CHARS_MAX];
    int run_count = 0;
    for (int i = 0; i < n;) {
        int j = i + 1;
        while (j < n && levels[j] == levels[i] && faces[j] == faces[i] && scripts[j] == scripts[i]) j++;
        runs[run_count++] = (Run){ i, j, levels[i], faces[i], scripts[i] };
        i = j;
    }
    reorder(runs, run_count);

    if (!buffer) buffer = hb_buffer_create();
    hb_language_t language = o->language && o->language[0] ? hb_language_from_string(o->language, -1) : HB_LANGUAGE_INVALID;
    double pen = 0;

    for (int r = 0; r < run_count; r++) {
        Shaper *shaper = shaper_for(runs[r].face);
        if (!shaper || !buffer) continue;
        hb_buffer_clear_contents(buffer);
        hb_buffer_add_utf32(buffer, cps + runs[r].start, runs[r].end - runs[r].start, 0, runs[r].end - runs[r].start);
        hb_buffer_set_direction(buffer, (runs[r].level & 1) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer, runs[r].script);
        if (language != HB_LANGUAGE_INVALID) hb_buffer_set_language(buffer, language);
        hb_shape(shaper->font, buffer, NULL, 0);

        unsigned int count;
        hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, &count);
        hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buffer, &count);
        for (unsigned int g = 0; g < count; g++) {
            if (out->count < PAC_TEXT_GLYPHS_MAX) {
                PacTextGlyph *glyph = &out->glyphs[out->count++];
                glyph->face = runs[r].face;
                glyph->glyph = info[g].codepoint;
                glyph->x = pen + pos[g].x_offset / (double)shaper->upem;
                glyph->y = -pos[g].y_offset / (double)shaper->upem;
                glyph->pen = pen;
                glyph->advance = pos[g].x_advance / (double)shaper->upem;
                glyph->cluster = (uint16_t)(runs[r].start + (int)info[g].cluster);
                glyph->level = (uint8_t)runs[r].level;
            }
            pen += pos[g].x_advance / (double)shaper->upem;
        }
    }
    out->width = pen;
    return true;
}

#else /* NO_I18N */

/* ---- no shaping: one glyph for each character, from left to right ---- */

void pac_text_finish(void) {}

bool pac_text_language_rtl(const char *lang) {
    (void)lang;
    return false;
}

bool pac_text_layout(const PacTextOptions *o, const char *utf8, PacTextLine *out) {
    out->count = 0;
    out->chars = 0;
    out->width = 0;
    out->rtl = false;
    if (!o || !o->fallback || !utf8 || !utf8[0]) return false;

    double pen = 0;
    int n = 0;
    for (const char *p = utf8; *p && n < PAC_TEXT_CHARS_MAX; n++) {
        uint32_t cp;
        p += decode(p, &cp);
        FT_Face face = o->pick ? o->pick(cp, o->data) : NULL;
        if (!face) face = o->fallback;
        FT_UInt glyph = FT_Get_Char_Index(face, cp);
        double advance = 0;
        if (face->units_per_EM > 0 && FT_Load_Glyph(face, glyph, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING) == 0)
            advance = (double)face->glyph->metrics.horiAdvance / face->units_per_EM;
        if (out->count < PAC_TEXT_GLYPHS_MAX) {
            out->glyphs[out->count++] = (PacTextGlyph){ face, glyph, pen, 0, pen, advance, (uint16_t)n, 0 };
        }
        pen += advance;
    }
    out->chars = (size_t)n;
    out->width = pen;
    return true;
}

#endif /* NO_I18N */

/* ---- the caret ---- */

int pac_text_caret(const PacTextOptions *o, const char *utf8, size_t at, double *x_em) {
    static PacTextLine line;
    *x_em = 0;
    if (!utf8 || !pac_text_layout(o, utf8, &line) || line.count == 0) return 0;

    /* The character the caret sits next to: the one before it, or the first at the start of the text. */
    size_t chars = 0, target = 0;
    bool leading = true;
    for (const char *p = utf8; *p && chars < line.chars; chars++) {
        size_t here = (size_t)(p - utf8);
        if (here >= at) break;
        uint32_t cp;
        p += decode(p, &cp);
        target = chars;
        leading = false;
    }
    if (leading) target = 0;

    /* The cluster that has it: the last that starts at or before it (the characters of a run are in order). */
    uint16_t start = 0;
    for (size_t g = 0; g < line.count; g++)
        if (line.glyphs[g].cluster <= target && line.glyphs[g].cluster >= start) start = line.glyphs[g].cluster;
    double left = line.width, right = 0;
    bool rtl = false;
    for (size_t g = 0; g < line.count; g++) {
        const PacTextGlyph *glyph = &line.glyphs[g];
        if (glyph->cluster != start) continue;
        if (glyph->pen < left) left = glyph->pen;
        if (glyph->pen + glyph->advance > right) right = glyph->pen + glyph->advance;
        rtl = glyph->level & 1;
    }
    if (left > right) return 0;                         /* no glyph for it (there were too many) */
    /* After a character: where it ends as the text goes, its right side in a left-to-right run and its left side in a
     * right-to-left one. At the start of the text: where the first begins, the other side. */
    *x_em = leading ? (rtl ? right : left) : (rtl ? left : right);
    return rtl ? -1 : 1;
}
