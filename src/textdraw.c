#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "textdraw.h"
#include "fontfind.h"
#include <cairo/cairo-ft.h>
#include <ft2build.h>
#include <stdint.h>
#include FT_FREETYPE_H
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FT_Library ft;
static bool text_as_paths;
static const cairo_user_data_key_t ft_face_key;

static void done_face(void *face) {
    FT_Done_Face(face);
}

FT_Face pac_font_ft(cairo_font_face_t *face) {
    return face ? cairo_font_face_get_user_data(face, &ft_face_key) : NULL;
}

bool pac_font_init(void) {
    return ft || FT_Init_FreeType(&ft) == 0;
}

cairo_font_face_t *pac_font_load(const char *path, const char *const *names, int index) {
    char found[256];
    if (!path || access(path, R_OK) != 0) {
        FontDirs dirs;
        font_dirs_init(&dirs);
        path = names && font_find(dirs.list, names, found, sizeof(found)) ? found : NULL;
    }
    if (!path) return NULL;

    FT_Face face;
    if (FT_New_Face(ft, path, index, &face) != 0) return NULL;
    cairo_font_face_t *cface = cairo_ft_font_face_create_for_ft_face(face, 0);
    cairo_font_face_set_user_data(cface, &ft_face_key, face, done_face);
    return cface;
}

/* The fonts a tool loads cover Latin, Cyrillic and Greek. Letters of other scripts (typed with
 * another keyboard layout, in a PAM prompt, in the name of an app) come from the font of their script,
 * which is only looked up and loaded when such a letter first has to be drawn. */
typedef struct {
    uint32_t lo, hi;
    const char *file;
    int index;           /* the face in a .ttc: Noto CJK is JP, KR, SC, TC, HK; or CJK_BY_LANGUAGE */
} ScriptFont;

/* The ideographs are drawn differently in Japanese, Korean and the two Chinese: the face is the language's. */
#define CJK_BY_LANGUAGE (-1)

static const ScriptFont script_fonts[] = {
    { 0x0530, 0x058F, "NotoSansArmenian-Regular.ttf", 0 },
    { 0x0590, 0x05FF, "NotoSansHebrew-Regular.ttf", 0 },
    { 0xFB1D, 0xFB4F, "NotoSansHebrew-Regular.ttf", 0 },
    { 0x0600, 0x06FF, "NotoSansArabic-Regular.ttf", 0 },
    { 0x0750, 0x077F, "NotoSansArabic-Regular.ttf", 0 },
    { 0x08A0, 0x08FF, "NotoSansArabic-Regular.ttf", 0 },
    { 0xFB50, 0xFDFF, "NotoSansArabic-Regular.ttf", 0 },
    { 0xFE70, 0xFEFF, "NotoSansArabic-Regular.ttf", 0 },
    { 0x0700, 0x074F, "NotoSansSyriac-Regular.ttf", 0 },
    { 0x0900, 0x097F, "NotoSansDevanagari-Regular.ttf", 0 },
    { 0x0980, 0x09FF, "NotoSansBengali-Regular.ttf", 0 },
    { 0x0A00, 0x0A7F, "NotoSansGurmukhi-Regular.ttf", 0 },
    { 0x0A80, 0x0AFF, "NotoSansGujarati-Regular.ttf", 0 },
    { 0x0B00, 0x0B7F, "NotoSansOriya-Regular.ttf", 0 },
    { 0x0B80, 0x0BFF, "NotoSansTamil-Regular.ttf", 0 },
    { 0x0C00, 0x0C7F, "NotoSansTelugu-Regular.ttf", 0 },
    { 0x0C80, 0x0CFF, "NotoSansKannada-Regular.ttf", 0 },
    { 0x0D00, 0x0D7F, "NotoSansMalayalam-Regular.ttf", 0 },
    { 0x0D80, 0x0DFF, "NotoSansSinhala-Regular.ttf", 0 },
    { 0x0E00, 0x0E7F, "NotoSansThai-Regular.ttf", 0 },
    { 0x0E80, 0x0EFF, "NotoSansLao-Regular.ttf", 0 },
    { 0x0F00, 0x0FFF, "NotoSansTibetan-Regular.ttf", 0 },
    { 0x1000, 0x109F, "NotoSansMyanmar-Regular.ttf", 0 },
    { 0x10A0, 0x10FF, "NotoSansGeorgian-Regular.ttf", 0 },
    { 0x2D00, 0x2D2F, "NotoSansGeorgian-Regular.ttf", 0 },
    { 0x1200, 0x139F, "NotoSansEthiopic-Regular.ttf", 0 },
    { 0x1780, 0x17FF, "NotoSansKhmer-Regular.ttf", 0 },
    { 0x1100, 0x11FF, "NotoSansCJK-Regular.ttc", 1 },   /* Hangul */
    { 0x3130, 0x318F, "NotoSansCJK-Regular.ttc", 1 },
    { 0xAC00, 0xD7AF, "NotoSansCJK-Regular.ttc", 1 },
    { 0x2E80, 0x9FFF, "NotoSansCJK-Regular.ttc", CJK_BY_LANGUAGE },   /* kana and ideographs */
    { 0xFF00, 0xFFEF, "NotoSansCJK-Regular.ttc", CJK_BY_LANGUAGE },
    /* Anything else: whatever covers the most. */
    { 0, 0x10FFFF, "DejaVuSans.ttf", 0 },
    { 0, 0x10FFFF, "NotoSansSymbols-Regular.ttf", 0 },
    { 0, 0x10FFFF, "NotoSansSymbols2-Regular.ttf", 0 },
};

/* One slot per entry of the table: it cannot hold more distinct files than that. */
#define MAX_LOADED (sizeof(script_fonts) / sizeof(script_fonts[0]))

typedef struct {
    const char *file;
    int index;
    cairo_font_face_t *face;   /* NULL: looked for and not found */
    FT_Face ft;
} Loaded;

static Loaded loaded[MAX_LOADED];
static int loaded_count;

/* Loads a font file on first use, and remembers a miss too: searching the font directories
 * is not free. */
static Loaded *font_lazy(const char *file, int index) {
    for (int i = 0; i < loaded_count; i++)
        if (strcmp(loaded[i].file, file) == 0 && loaded[i].index == index)
            return loaded[i].face ? &loaded[i] : NULL;
    if ((size_t)loaded_count == MAX_LOADED) return NULL;

    Loaded *l = &loaded[loaded_count++];
    const char *names[] = { file, NULL };
    l->file = file;
    l->index = index;
    l->face = pac_font_load(NULL, names, index);
    l->ft = pac_font_ft(l->face);
    return l->face ? l : NULL;
}

cairo_font_face_t *pac_font_lazy(const char *file, int index) {
    Loaded *l = font_lazy(file, index);
    return l ? l->face : NULL;
}

void pac_font_finish(cairo_font_face_t *const *faces, size_t count) {
    pac_text_finish();   /* the shaping holds the faces: before they are freed */
    for (size_t i = 0; i < count; i++)
        if (faces[i]) cairo_font_face_destroy(faces[i]);
    for (int i = 0; i < loaded_count; i++)
        if (loaded[i].face) cairo_font_face_destroy(loaded[i].face);
    loaded_count = 0;
    FT_Done_FreeType(ft);
    ft = NULL;
}

/* The language the texts are in, for the shaping and for the direction of a line with no letter that
 * says it: "ar", "zh-TW". The part before ':' '.' or '@' of what is set; '_' is '-'. */
static char text_language[32];

void pac_font_set_language(const char *language) {
    size_t n = 0;
    for (; language && language[n] && !strchr(":.@", language[n]) && n < sizeof(text_language) - 1; n++)
        text_language[n] = language[n] == '_' ? '-' : language[n];
    text_language[n] = '\0';
}

void pac_font_set_paths(bool on) {
    text_as_paths = on;
}

/* Which face of Noto CJK draws the ideographs: they are drawn differently in Japanese, Korean and
 * the two Chinese. The faces of the collection: JP, KR, SC, TC, HK. */
static int cjk_face_index(void) {
    if (strncmp(text_language, "ko", 2) == 0) return 1;
    if (strncmp(text_language, "zh", 2) == 0) {
        if (strstr(text_language, "TW") || strstr(text_language, "Hant")) return 3;
        if (strstr(text_language, "HK") || strstr(text_language, "MO")) return 4;
        return 2;
    }
    return 0;
}

/* The face to draw a code point with: the one being drawn with (data) if it has the letter, else the
 * one of its script (textlayout.h). NULL if nobody has it: it is drawn as that font's "missing" box. */
static FT_Face pick_face(uint32_t cp, void *data) {
    FT_Face primary = pac_font_ft(data);
    if (primary && FT_Get_Char_Index(primary, cp)) return primary;

    for (size_t i = 0; i < sizeof(script_fonts) / sizeof(script_fonts[0]); i++) {
        const ScriptFont *s = &script_fonts[i];
        if (cp < s->lo || cp > s->hi) continue;
        int index = s->index == CJK_BY_LANGUAGE ? cjk_face_index() : s->index;
        Loaded *l = font_lazy(s->file, index);
        if (l && FT_Get_Char_Index(l->ft, cp)) return l->ft;
    }
    return NULL;
}

/* The cairo face of an FT face that textlayout picked. */
static cairo_font_face_t *cairo_face_of(FT_Face face, cairo_font_face_t *primary) {
    if (pac_font_ft(primary) == face) return primary;
    for (int i = 0; i < loaded_count; i++)
        if (loaded[i].ft == face && loaded[i].face) return loaded[i].face;
    return primary;
}

static PacTextOptions text_options(cairo_font_face_t *primary) {
    /* The texts are in the language of the interface: its direction, whatever letter a message starts with. */
    return (PacTextOptions){ pick_face, primary, pac_font_ft(primary), text_language[0] ? text_language : NULL,
                             pac_text_language_rtl(text_language) ? PAC_TEXT_RTL : PAC_TEXT_LTR };
}

/* Cairo keeps a glyph cache for show_glyphs (about 260 kB of memory the first time); the outline of
 * a glyph, filled, does not use it. */
static void draw_glyphs(cairo_t *cr, const cairo_glyph_t *glyphs, int count) {
    if (!text_as_paths) {
        cairo_show_glyphs(cr, glyphs, count);
        return;
    }
    cairo_glyph_path(cr, glyphs, count);
    cairo_fill(cr);
}

/* Lays out s (shaped, and in the order of its scripts: textlayout.h), each letter in the font that
 * has it, from the current point; draws it if asked. Returns the width. */
double pac_font_text(cairo_t *cr, const char *s, bool draw) {
    cairo_font_face_t *primary = cairo_font_face_reference(cairo_get_font_face(cr));
    double width = 0;

    if (!pac_font_ft(primary)) {   /* Cairo's own font: let it do the work */
        cairo_text_extents_t e;
        cairo_text_extents(cr, s, &e);
        if (draw && text_as_paths) {
            double x, y;
            cairo_get_current_point(cr, &x, &y);
            cairo_text_path(cr, s);
            cairo_fill(cr);
            cairo_move_to(cr, x + e.x_advance, y);
        } else if (draw) {
            cairo_show_text(cr, s);
        }
        width = e.x_advance;
    } else {
        static PacTextLine line;
        PacTextOptions options = text_options(primary);
        if (pac_text_layout(&options, s, &line)) {
            cairo_matrix_t matrix;
            cairo_get_font_matrix(cr, &matrix);
            double size = matrix.xx;   /* the font size: the layout is in em */

            if (draw && line.count) {
                double x0, y0;
                cairo_get_current_point(cr, &x0, &y0);
                cairo_glyph_t glyphs[PAC_TEXT_GLYPHS_MAX];
                for (size_t start = 0; start < line.count;) {
                    size_t end = start;
                    FT_Face face = line.glyphs[start].face;
                    for (; end < line.count && line.glyphs[end].face == face; end++)
                        glyphs[end - start] = (cairo_glyph_t){ line.glyphs[end].glyph, x0 + line.glyphs[end].x * size,
                                                               y0 + line.glyphs[end].y * size };
                    cairo_set_font_face(cr, cairo_face_of(face, primary));
                    draw_glyphs(cr, glyphs, (int)(end - start));
                    start = end;
                }
                cairo_set_font_face(cr, primary);
                cairo_move_to(cr, x0 + line.width * size, y0);
            }
            width = line.width * size;
        }
    }
    cairo_font_face_destroy(primary);
    return width;
}

double pac_font_width(cairo_t *cr, const char *s) {
    return pac_font_text(cr, s, false);
}

void pac_font_show(cairo_t *cr, const char *s) {
    pac_font_text(cr, s, true);
}

void pac_font_fit(cairo_t *cr, const char *text, double max_w, bool keep_end, char *out, size_t size) {
    snprintf(out, size, "%s", text);
    if (pac_font_width(cr, out) <= max_w) return;

    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", text);
    for (;;) {
        int written;
        if (keep_end) {
            size_t skip = pac_text_next_cluster(tmp, 0);
            memmove(tmp, tmp + skip, strlen(tmp + skip) + 1);
            written = snprintf(out, size, "…%s", tmp);
        } else {
            tmp[pac_text_prev_cluster(tmp, strlen(tmp))] = '\0';
            written = snprintf(out, size, "%s…", tmp);
        }
        /* What does not fit in `out` is cut: the width of what is left is what is measured. */
        if (written < 0 || !tmp[0] || pac_font_width(cr, out) <= max_w) return;
    }
}

double pac_font_caret(cairo_t *cr, const char *utf8, size_t at, int *dir) {
    cairo_font_face_t *primary = cairo_get_font_face(cr);
    double x = 0;
    int d = 0;
    if (pac_font_ft(primary)) {
        PacTextOptions options = text_options(primary);
        cairo_matrix_t matrix;
        cairo_get_font_matrix(cr, &matrix);
        d = pac_text_caret(&options, utf8, at, &x);
        x *= matrix.xx;
    } else if (utf8 && utf8[0]) {   /* Cairo's own font: the width of what is before it */
        char before[512];
        size_t n = at < sizeof(before) - 1 ? at : sizeof(before) - 1;
        for (size_t i = 0; i < n && utf8[i]; i++) before[i] = utf8[i];
        before[n < strlen(utf8) ? n : strlen(utf8)] = '\0';
        cairo_text_extents_t e;
        cairo_text_extents(cr, before, &e);
        x = e.x_advance;
        d = 1;
    }
    if (dir) *dir = d ? d : (pac_text_language_rtl(text_language) ? -1 : 1);
    return x;
}

double pac_font_preedit(cairo_t *cr, double x0, double baseline, const char *shown, size_t composing, int begin, int end,
                        double thick) {
    size_t total = strlen(shown), from = total >= composing ? total - composing : 0;
    double left, right;
    if (composing == 0) return NAN;
    pac_font_span(cr, x0, shown, from, total, &left, &right);
    cairo_rectangle(cr, left, baseline, right - left, thick);
    cairo_fill(cr);
    if (begin < 0) return NAN;
    if ((size_t)end > composing) end = (int)composing;
    if (end > begin) {
        pac_font_span(cr, x0, shown, from + (size_t)begin, from + (size_t)end, &left, &right);
        cairo_rectangle(cr, left, baseline, right - left, thick * 2);
        cairo_fill(cr);
    }
    return x0 + pac_font_caret(cr, shown, from + (size_t)(begin > (int)composing ? (int)composing : begin), NULL);
}

void pac_font_span(cairo_t *cr, double x0, const char *utf8, size_t from, size_t to, double *left, double *right) {
    double a = x0 + pac_font_caret(cr, utf8, from, NULL), b = x0 + pac_font_caret(cr, utf8, to, NULL);
    *left = a < b ? a : b;
    *right = a < b ? b : a;
}

double pac_font_caret_x(cairo_t *cr, double x0, const char *utf8, double gap, double width) {
    int dir;
    double at = x0 + pac_font_caret(cr, utf8, strlen(utf8), &dir);
    return dir > 0 ? at + gap : at - gap - width;
}

size_t pac_font_cut(cairo_t *cr, const char *utf8, double max_w) {
    PacTextOptions options = text_options(cairo_get_font_face(cr));
    cairo_matrix_t matrix;
    cairo_get_font_matrix(cr, &matrix);
    return pac_text_fit(&options, utf8, max_w / matrix.xx, NULL);
}
