/* Tests of textdraw.c: loading fonts, the font of each script, measuring, drawing, fitting. They draw to
 * an image and look at the pixels. They use the fonts that are installed (Noto, DejaVu) and say which
 * they skipped. Built twice by the Makefile, like text_test.c: with HarfBuzz and FriBidi, and with
 * -DNO_I18N. */
#define _GNU_SOURCE
#include "textdraw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures, skipped;

/* LeakSanitizer (make sanitize): Cairo keeps the fontconfig pattern of a toy font face for the whole run.
 * It is the only thing ignored; a face of ours that is not freed is still reported. Not called otherwise. */
const char *__lsan_default_suppressions(void) { return "leak:FcPatternDuplicate\n"; }

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

static const char *const latin_names[] = { "NotoSans-Regular.ttf", "DejaVuSans.ttf", NULL };

/* How many pixels of the image are not empty, and the leftmost and rightmost columns that are. */
static long inked(cairo_surface_t *s, int *left, int *right) {
    cairo_surface_flush(s);
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s), stride = cairo_image_surface_get_stride(s);
    const unsigned char *data = cairo_image_surface_get_data(s);
    long n = 0;
    *left = w;
    *right = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (data[y * stride + x * 4 + 3]) {
                n++;
                if (x < *left) *left = x;
                if (x > *right) *right = x;
            }
    return n;
}

typedef struct { cairo_surface_t *s; cairo_t *cr; } Canvas;

static Canvas canvas(cairo_font_face_t *face, double size) {
    Canvas c;
    c.s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 600, 120);
    c.cr = cairo_create(c.s);
    cairo_set_font_face(c.cr, face);
    cairo_set_font_size(c.cr, size);
    cairo_set_source_rgb(c.cr, 1, 1, 1);
    return c;
}

static void done(Canvas c) {
    cairo_destroy(c.cr);
    cairo_surface_destroy(c.s);
}

static void test_measure_and_draw(cairo_font_face_t *latin) {
    Canvas c = canvas(latin, 40);
    double hello = pac_font_width(c.cr, "Hello"), two = pac_font_width(c.cr, "Hello World");
    CHECK(hello > 40 && two > hello + 40);
    CHECK(pac_font_width(c.cr, "") == 0);
    cairo_move_to(c.cr, 10, 80);
    double w = pac_font_text(c.cr, "Hello", true);
    CHECK(w == hello);

    double x, y;
    cairo_get_current_point(c.cr, &x, &y);
    CHECK(x > 10 + hello - 0.01 && x < 10 + hello + 0.01 && y == 80);   /* the point is at the end of the text */

    int left, right;
    CHECK(inked(c.s, &left, &right) > 100);
    CHECK(left >= 8 && right <= (int)(10 + hello + 2));                  /* what is drawn is where it was asked */

    /* Measuring draws nothing, and a second text begins where the first ended. */
    Canvas m = canvas(latin, 40);
    cairo_move_to(m.cr, 10, 80);
    pac_font_width(m.cr, "Hello");
    CHECK(inked(m.s, &left, &right) == 0);
    done(m);

    /* A bigger size is wider, in proportion. */
    cairo_set_font_size(c.cr, 80);
    CHECK(pac_font_width(c.cr, "Hello") > hello * 1.9 && pac_font_width(c.cr, "Hello") < hello * 2.1);
    done(c);
}

static void test_paths(cairo_font_face_t *latin) {
    /* The same text as outlines: drawn too, and the same size. */
    Canvas c = canvas(latin, 40);
    double plain = pac_font_width(c.cr, "Hello");
    pac_font_set_paths(true);
    cairo_move_to(c.cr, 10, 80);
    pac_font_show(c.cr, "Hello");
    int left, right;
    CHECK(inked(c.s, &left, &right) > 100);
    CHECK(pac_font_width(c.cr, "Hello") == plain);
    pac_font_set_paths(false);
    done(c);
}

static void test_toy(void) {
    /* A face that is not from pac_font_load is Cairo's own: measured and drawn by Cairo. */
    cairo_font_face_t *toy = cairo_toy_font_face_create("sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    Canvas c = canvas(toy, 40);
    CHECK(pac_font_width(c.cr, "Hello") > 40);
    cairo_move_to(c.cr, 10, 80);
    pac_font_show(c.cr, "Hello");
    int left, right;
    CHECK(inked(c.s, &left, &right) > 50);
    done(c);
    cairo_font_face_destroy(toy);
}

static void test_scripts(cairo_font_face_t *latin) {
    cairo_font_face_t *arabic = pac_font_lazy("NotoSansArabic-Regular.ttf", 0);
    if (!arabic) {
        skipped++;
        fprintf(stderr, "skipped: no Noto Sans Arabic\n");
        return;
    }
    /* The main font has no Arabic letters: they come from the font of their script, found by itself. */
    Canvas c = canvas(latin, 40);
    const char *text = "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7";   /* marhaba */
    double w = pac_font_width(c.cr, text);
    CHECK(w > 40);
    cairo_move_to(c.cr, 10, 80);
    pac_font_show(c.cr, text);
    int left, right;
    CHECK(inked(c.s, &left, &right) > 100);
    CHECK(left >= 8 && right <= (int)(10 + w + 2));

    /* Latin and Arabic on one line: the sum, within what shaping does at the join. */
    double mixed = pac_font_width(c.cr, "ab \xD9\x85\xD8\xB1\xD8\xAD");
    CHECK(mixed > pac_font_width(c.cr, "ab") && mixed > pac_font_width(c.cr, "\xD9\x85\xD8\xB1\xD8\xAD"));
    done(c);
}

static void test_lazy(void) {
    CHECK(pac_font_lazy("NoSuchFont-Regular.ttf", 0) == NULL);
    CHECK(pac_font_lazy("NoSuchFont-Regular.ttf", 0) == NULL);   /* a miss is remembered, and is still a miss */
    cairo_font_face_t *a = pac_font_lazy("NotoSans-Regular.ttf", 0);
    if (a) {
        CHECK(pac_font_lazy("NotoSans-Regular.ttf", 0) == a);    /* the same face the second time */
        CHECK(pac_font_ft(a) != NULL);
    }
    CHECK(pac_font_ft(NULL) == NULL);
    cairo_font_face_t *toy = cairo_toy_font_face_create("sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    CHECK(pac_font_ft(toy) == NULL);
    cairo_font_face_destroy(toy);
    CHECK(pac_font_load("/nonexistent/font.ttf", NULL, 0) == NULL);
    CHECK(pac_font_load(NULL, NULL, 0) == NULL);
}

static void test_fit_and_cut(cairo_font_face_t *latin) {
    Canvas c = canvas(latin, 30);
    const char *long_text = "A long line of text that cannot fit in a narrow place";
    double full = pac_font_width(c.cr, long_text), max_w = full / 3;
    char out[256];

    pac_font_fit(c.cr, "short", max_w, false, out, sizeof out);
    CHECK(strcmp(out, "short") == 0);                              /* what fits is left as it is */
    pac_font_fit(c.cr, long_text, max_w, false, out, sizeof out);
    CHECK(strcmp(out + strlen(out) - 3, "\xE2\x80\xA6") == 0);      /* cut at the end, with an ellipsis */
    CHECK(pac_font_width(c.cr, out) <= max_w && strlen(out) > 5);
    pac_font_fit(c.cr, long_text, max_w, true, out, sizeof out);
    CHECK(strncmp(out, "\xE2\x80\xA6", 3) == 0 && pac_font_width(c.cr, out) <= max_w);   /* or at the start */
    CHECK(strcmp(out + strlen(out) - 5, "place") == 0);            /* keeping the end, where the cursor is */
    pac_font_fit(c.cr, long_text, max_w, false, out, 12);          /* a small buffer: cut, never overflowed */
    CHECK(strlen(out) < 12);

    CHECK(pac_font_cut(c.cr, long_text, full * 2) == strlen(long_text));     /* all of it fits */
    size_t cut = pac_font_cut(c.cr, long_text, max_w);
    CHECK(cut > 3 && cut < strlen(long_text));
    CHECK(pac_font_width(c.cr, "A") > 0 && pac_font_cut(c.cr, long_text, 0.001) >= 1);   /* at least one letter */
    done(c);
}

static void test_caret(cairo_font_face_t *latin) {
    Canvas c = canvas(latin, 40);
    int dir = 0;
    double full = pac_font_width(c.cr, "Hello");
    CHECK(pac_font_caret(c.cr, "Hello", 0, &dir) == 0 && dir == 1);             /* the start, text going right */
    CHECK(fabs(pac_font_caret(c.cr, "Hello", 5, &dir) - full) < 0.01 && dir == 1);   /* the end is the width */
    double mid = pac_font_caret(c.cr, "Hello", 2, NULL);                       /* a NULL for the direction is fine */
    CHECK(mid > 10 && mid < full);
    cairo_set_font_size(c.cr, 80);                                             /* in pixels: it follows the size */
    CHECK(pac_font_caret(c.cr, "Hello", 2, NULL) > mid * 1.9);
    cairo_set_font_size(c.cr, 40);

    /* The span between two offsets: its two ends, in order, in pixels. */
    double lo, hi;
    pac_font_span(c.cr, 10, "Hello", 1, 3, &lo, &hi);
    CHECK(fabs(lo - (10 + pac_font_caret(c.cr, "Hello", 1, NULL))) < 0.01 && fabs(hi - (10 + pac_font_caret(c.cr, "Hello", 3, NULL))) < 0.01 && lo < hi);
    double lo2, hi2;
    pac_font_span(c.cr, 10, "Hello", 3, 1, &lo2, &hi2);
    CHECK(lo2 == lo && hi2 == hi);                                              /* in order, whichever way it is asked */
    pac_font_span(c.cr, 10, "Hello", 2, 2, &lo, &hi);
    CHECK(lo == hi);

    /* What is composed is underlined, and the part the input method is on more; a cursor is where it says. */
    {
        Canvas k = canvas(latin, 40);
        cairo_move_to(k.cr, 10, 80);
        pac_font_show(k.cr, "Hello");
        int l0, r0;
        long before = inked(k.s, &l0, &r0);
        double cursor = pac_font_preedit(k.cr, 10, 90, "Hello", 3, 1, 2, 2);          /* "llo" is composed, the cursor on "l" */
        int l1, r1;
        long after = inked(k.s, &l1, &r1);
        CHECK(after > before + 20);                                                   /* the lines were drawn */
        double expect = 10 + pac_font_caret(k.cr, "Hello", 3, NULL);                  /* 2 of "Hello", plus the 1 of "llo" */
        CHECK(fabs(cursor - expect) < 0.01);
        CHECK(isnan(pac_font_preedit(k.cr, 10, 100, "Hello", 3, -1, -1, 2)));          /* none to show */
        CHECK(isnan(pac_font_preedit(k.cr, 10, 100, "Hello", 0, 0, 0, 2)));            /* nothing composed: nothing marked */
        double wide = pac_font_preedit(k.cr, 10, 100, "Hello", 3, 0, 99, 2);           /* an end past the text is the end */
        CHECK(!isnan(wide) && fabs(wide - (10 + pac_font_caret(k.cr, "Hello", 2, NULL))) < 0.01);
        CHECK(!isnan(pac_font_preedit(k.cr, 10, 100, "Hello", 99, 0, 0, 2)));          /* more than there is: all of it */
        done(k);
    }

    /* The bar goes `gap` from the end, on the side the text goes on, and is `width` wide there. */
    CHECK(fabs(pac_font_caret_x(c.cr, 10, "Hello", 2, 3) - (10 + full + 2)) < 0.01);
    CHECK(fabs(pac_font_caret_x(c.cr, 10, "", 2, 3) - 12) < 0.01);                /* nothing typed: just after the start */

    /* Nothing to put it next to: the start, and the direction of the language. */
    CHECK(pac_font_caret(c.cr, "", 0, &dir) == 0 && dir == 1);
#ifndef NO_I18N                                                              /* the plain flavour has no right-to-left */
    pac_font_set_language("ar");
    CHECK(pac_font_caret(c.cr, "", 0, &dir) == 0 && dir == -1);
    pac_font_set_language("");
#endif

    /* A Cairo font is measured by Cairo. */
    cairo_font_face_t *toy = cairo_toy_font_face_create("sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_face(c.cr, toy);
    double toy_full = pac_font_width(c.cr, "Hello");
    CHECK(fabs(pac_font_caret(c.cr, "Hello", 5, &dir) - toy_full) < 0.01 && dir == 1);
    CHECK(pac_font_caret(c.cr, "Hello", 0, NULL) == 0);
    cairo_set_font_face(c.cr, latin);
    cairo_font_face_destroy(toy);

    /* Arabic, from the font of its script: it goes to the left, so its end is at the left end. */
#ifdef NO_I18N
    (void)0;
#else
    if (pac_font_lazy("NotoSansArabic-Regular.ttf", 0)) {
        const char *text = "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7";
        pac_font_set_language("ar");
        double w = pac_font_width(c.cr, text);
        CHECK(fabs(pac_font_caret(c.cr, text, 0, &dir) - w) < 0.01 && dir == -1);
        CHECK(fabs(pac_font_caret(c.cr, text, 10, &dir)) < 0.01 && dir == -1);
        CHECK(fabs(pac_font_caret_x(c.cr, 100, text, 2, 3) - (100 - 2 - 3)) < 0.01);   /* at the left of the text, its bar left of that */
        pac_font_set_language("");
    } else {
        skipped++;
        fprintf(stderr, "skipped: no Noto Sans Arabic (caret)\n");
    }
#endif
    done(c);
}

static void test_language(cairo_font_face_t *latin) {
    /* The language only changes how things are drawn where it matters; it never makes a text vanish. */
    Canvas c = canvas(latin, 40);
    double plain = pac_font_width(c.cr, "Hello");
    pac_font_set_language("ar");
    CHECK(pac_font_width(c.cr, "Hello") > 0);
    pac_font_set_language("zh_TW.UTF-8");
    CHECK(pac_font_width(c.cr, "Hello") == plain);
    char big[200];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    pac_font_set_language(big);                                     /* too long: cut, not overflowed */
    CHECK(pac_font_width(c.cr, "Hello") == plain);
    pac_font_set_language(NULL);
    pac_font_set_language("");
    CHECK(pac_font_width(c.cr, "Hello") == plain);
    done(c);
}

int main(void) {
    if (!pac_font_init() || !pac_font_init()) { fprintf(stderr, "FreeType did not start\n"); return 2; }   /* twice: the same */

    test_toy();
    test_lazy();

    cairo_font_face_t *latin = pac_font_load(NULL, latin_names, 0);
    cairo_font_face_t *again = pac_font_load(NULL, latin_names, 0);
    CHECK(again != latin || !latin);                                 /* each call makes its own face: the caller owns it */
    if (!latin) {
        fprintf(stderr, "skipped: no Latin font (Noto Sans or DejaVu Sans): the drawing tests did not run\n");
        skipped++;
    } else {
        test_measure_and_draw(latin);
        test_paths(latin);
        test_scripts(latin);
        test_fit_and_cut(latin);
        test_caret(latin);
        test_language(latin);
    }
    cairo_font_face_t *mine[] = { latin, again };               /* both are the caller's, and finish destroys them */
    pac_font_finish(mine, 2);
    printf("%d checks, %d failures, %d skipped\n", checks, failures, skipped);
    return failures ? 1 : 0;
}
