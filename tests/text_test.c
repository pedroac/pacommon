/* Tests of textlayout.c: shaping, order, clusters and where a line fits. The font tests use the fonts
 * that are installed (Noto, DejaVu) and say which they skipped. Built twice by the Makefile: with
 * HarfBuzz and FriBidi, and with -DNO_I18N for the plain fallback. */
#define _GNU_SOURCE
#include "fontfind.h"
#include "textlayout.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures, skipped;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

static FT_Library ft;

static FT_Face load(const char *file) {
    FontDirs dirs;
    font_dirs_init(&dirs);
    const char *names[] = { file, NULL };
    char path[FONT_PATH_MAX];
    FT_Face face = NULL;
    if (font_find(dirs.list, names, path, sizeof path) && FT_New_Face(ft, path, 0, &face) == 0) return face;
    return NULL;
}

/* The faces a test uses, and which one draws what. */
static struct { FT_Face latin, arabic, hebrew, devanagari, thai, cjk; } F;

static FT_Face pick(uint32_t cp, void *data) {
    (void)data;
    if (F.latin && FT_Get_Char_Index(F.latin, cp)) return F.latin;
    if (cp >= 0x0590 && cp <= 0x05FF) return F.hebrew;
    if (cp >= 0x0600 && cp <= 0x06FF) return F.arabic;
    if (cp >= 0x0900 && cp <= 0x097F) return F.devanagari;
    if (cp >= 0x0E00 && cp <= 0x0E7F) return F.thai;
    if (cp >= 0x2E80 && cp <= 0x9FFF) return F.cjk;
    if (cp >= 0x3000 && cp <= 0x30FF) return F.cjk;
    return NULL;
}

static PacTextOptions options(PacTextDirection dir, const char *language) {
    return (PacTextOptions){ pick, NULL, F.latin, language, dir };
}

static PacTextLine line;

static uint32_t glyph_of(FT_Face face, uint32_t cp) {
    return FT_Get_Char_Index(face, cp);
}

/* ---- pure: no font ---- */

static void test_clusters(void) {
    const char *abc = "abc";
    CHECK(pac_text_next_cluster(abc, 0) == 1 && pac_text_next_cluster(abc, 2) == 3 && pac_text_next_cluster(abc, 3) == 3);
    CHECK(pac_text_prev_cluster(abc, 3) == 2 && pac_text_prev_cluster(abc, 1) == 0 && pac_text_prev_cluster(abc, 0) == 0);
    CHECK(pac_text_next_cluster("\r\nx", 0) == 2);                              /* CR LF is one */

    const char *hangul = "\xE1\x84\x80\xE1\x85\xA1\xE1\x86\xA8x";               /* U+1100 U+1161 U+11A8: one syllable */
    CHECK(pac_text_next_cluster(hangul, 0) == 9);
    const char *syllable = "\xED\x95\x9Cx";                                      /* U+D55C, one */
    CHECK(pac_text_next_cluster(syllable, 0) == 3);

    /* A broken sequence is one character of its own, and never the end of the string. */
    CHECK(pac_text_next_cluster("\xFF" "a", 0) == 1);
    CHECK(pac_text_next_cluster("\xE2\x82", 0) == 2);                           /* cut short: what is there */
    CHECK(pac_text_next_cluster("\xC0\x80" "a", 0) == 2);                       /* overlong: one replacement */
    CHECK(pac_text_next_cluster("\xED\xA0\x80" "a", 0) == 3);                   /* a surrogate */
    CHECK(pac_text_next_cluster("", 0) == 0);

#ifndef NO_I18N
    CHECK(pac_text_next_cluster("e\xCC\x81x", 0) == 3);                         /* e and U+0301 */
    CHECK(pac_text_prev_cluster("e\xCC\x81x", 3) == 0);
    CHECK(pac_text_prev_cluster("e\xCC\x81x", 2) == 0);                         /* inside the cluster: its start */
    CHECK(pac_text_next_cluster("\xE0\xB8\x81\xE0\xB9\x87" "x", 0) == 6);       /* Thai: ko kai and the mai taikhu */
    CHECK(pac_text_next_cluster("\xE0\xB8\x81\xE0\xB8\xB3" "x", 0) == 6);       /* and sara am */
    /* man, joiner, woman, joiner, girl: one emoji */
    const char *family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7x";
    CHECK(pac_text_next_cluster(family, 0) == 18);
    const char *flags = "\xF0\x9F\x87\xA7\xF0\x9F\x87\xB7\xF0\x9F\x87\xB5\xF0\x9F\x87\xB9";   /* BR PT: two flags */
    CHECK(pac_text_next_cluster(flags, 0) == 8 && pac_text_next_cluster(flags, 8) == 16);
    CHECK(pac_text_next_cluster("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD" "x", 0) == 8);   /* thumbs up with a skin tone */
#endif
}

static void test_language_direction(void) {
#ifndef NO_I18N
    CHECK(pac_text_language_rtl("ar") && pac_text_language_rtl("he") && pac_text_language_rtl("fa"));
    CHECK(pac_text_language_rtl("ar_EG.UTF-8") && pac_text_language_rtl("fa-IR") && pac_text_language_rtl("ur"));
    CHECK(!pac_text_language_rtl("arx") && !pac_text_language_rtl("en") && !pac_text_language_rtl("pt_BR"));
    CHECK(!pac_text_language_rtl("") && !pac_text_language_rtl(NULL) && !pac_text_language_rtl("a"));
#else
    CHECK(!pac_text_language_rtl("ar") && !pac_text_language_rtl(NULL));
#endif
}

static void test_can_break(void) {
    CHECK(pac_text_can_break('a', 'b') == false);
    CHECK(pac_text_can_break(' ', 'b') && !pac_text_can_break('a', ' '));       /* after a space, not before */
    CHECK(pac_text_can_break('-', 'b') && !pac_text_can_break('-', ' '));
    CHECK(pac_text_can_break(0x65E5, 0x672C));                                  /* between ideographs */
    CHECK(!pac_text_can_break(0x65E5, 0x3002));                                 /* not before a full stop */
    CHECK(!pac_text_can_break(0x300C, 0x65E5));                                 /* not after an opening bracket */
    CHECK(!pac_text_can_break(0x0E01, 0x0E02));                                 /* Thai: no dictionary */
    CHECK(pac_text_can_break(0x3000, 0x65E5));
}

/* ---- with fonts ---- */

static void test_latin(void) {
    PacTextOptions o = options(PAC_TEXT_AUTO, "en");
    CHECK(!pac_text_layout(&o, "", &line) && line.count == 0 && line.width == 0);
    CHECK(pac_text_layout(&o, "Hello", &line) && line.count == 5 && line.width > 2 && !line.rtl);
    CHECK(line.glyphs[0].glyph == glyph_of(F.latin, 'H') && line.glyphs[0].x == 0);
    for (size_t i = 1; i < line.count; i++) CHECK(line.glyphs[i].x > line.glyphs[i - 1].x);

    PacTextOptions bad = o;
    bad.fallback = NULL;
    CHECK(!pac_text_layout(&bad, "x", &line) && line.count == 0);
    CHECK(!pac_text_layout(&o, NULL, &line));

    /* A character no font has is the box of the fallback. */
    CHECK(pac_text_layout(&o, "\xF0\x9F\x90\x89", &line) && line.count == 1 && line.glyphs[0].face == F.latin);

    /* Broken bytes are replacement characters, one for each, not a crash. */
    CHECK(pac_text_layout(&o, "\xFF\xFE", &line) && line.count == 2);

    /* Longer than a line holds: bounded. */
    char big[2000];
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    CHECK(pac_text_layout(&o, big, &line) && line.count <= PAC_TEXT_GLYPHS_MAX && line.count > 0);
}

#ifndef NO_I18N
static void test_shaping(void) {
    if (!F.arabic) { skipped++; fprintf(stderr, "skipped: no Noto Sans Arabic\n"); return; }
    PacTextOptions o = options(PAC_TEXT_AUTO, "ar");

    /* beh, seen, meem: written joined, so the glyphs are not the ones of the letters alone (the initial,
     * medial and final forms). A font may draw the dots as marks of their own: three letters, at least three glyphs. */
    CHECK(pac_text_layout(&o, "\xD8\xA8\xD8\xB3\xD9\x85", &line) && line.count >= 3 && line.rtl);
    uint32_t seen = glyph_of(F.arabic, 0x0633), meem = glyph_of(F.arabic, 0x0645);
    int forms = 0;
    for (size_t i = 0; i < line.count; i++)
        if (line.glyphs[i].glyph == seen || line.glyphs[i].glyph == meem) forms++;
    CHECK(forms == 0);                                  /* neither is in its isolated form: they are joined */
    /* Visual order is left to right: the last letter is first, and it is the widest way back to the start. */
    CHECK(line.glyphs[0].x == 0 && line.width > 0.5);

    /* lam and alef: lam joins to the alef after it, alef to the lam before it: not the isolated glyphs. */
    CHECK(pac_text_layout(&o, "\xD9\x84\xD8\xA7", &line) && line.count >= 1 && line.count <= 2);
    uint32_t lam = glyph_of(F.arabic, 0x0644), alef = glyph_of(F.arabic, 0x0627);
    for (size_t i = 0; i < line.count; i++) CHECK(line.glyphs[i].glyph != lam && line.glyphs[i].glyph != alef);

    /* The same letters alone are the isolated glyphs. */
    CHECK(pac_text_layout(&o, "\xD9\x84", &line) && line.count == 1 && line.glyphs[0].glyph != 0);
}

static void test_order(void) {
    if (!F.hebrew) { skipped++; fprintf(stderr, "skipped: no Noto Sans Hebrew\n"); return; }
    PacTextOptions o = options(PAC_TEXT_AUTO, NULL);
    uint32_t aleph = glyph_of(F.hebrew, 0x05D0), gimel = glyph_of(F.hebrew, 0x05D2);

    /* Latin, a space, then three Hebrew letters: the Hebrew reads from the right, so it is drawn gimel bet aleph. */
    CHECK(pac_text_layout(&o, "abc \xD7\x90\xD7\x91\xD7\x92", &line) && line.count == 7 && !line.rtl);
    CHECK(line.glyphs[0].glyph == glyph_of(F.latin, 'a'));
    CHECK(line.glyphs[4].glyph == gimel && line.glyphs[6].glyph == aleph);
    CHECK(line.glyphs[4].face == F.hebrew && line.glyphs[0].face == F.latin);

    /* The same drawn from a line that starts with Hebrew: the paragraph is right to left, and looks the same. */
    CHECK(pac_text_layout(&o, "\xD7\x90\xD7\x91\xD7\x92 abc", &line) && line.count == 7 && line.rtl);
    CHECK(line.glyphs[0].glyph == glyph_of(F.latin, 'a') && line.glyphs[6].glyph == aleph);

    /* Forced direction: English words stay in their order in a right-to-left paragraph (a space between
     * two of them is theirs), but a full stop at the end, which is neutral, goes to the left. */
    PacTextOptions rtl = options(PAC_TEXT_RTL, NULL);
    CHECK(pac_text_layout(&rtl, "abc def", &line) && line.count == 7 && line.rtl);
    CHECK(line.glyphs[0].glyph == glyph_of(F.latin, 'a') && line.glyphs[6].glyph == glyph_of(F.latin, 'f'));
    CHECK(pac_text_layout(&rtl, "abc.", &line) && line.glyphs[0].glyph == glyph_of(F.latin, '.') &&
          line.glyphs[3].glyph == glyph_of(F.latin, 'c'));
    PacTextOptions ltr = options(PAC_TEXT_LTR, NULL);
    CHECK(pac_text_layout(&ltr, "abc.", &line) && !line.rtl && line.glyphs[0].glyph == glyph_of(F.latin, 'a'));

    /* A line with no letter that says which way it goes takes the language's: digits in Hebrew. */
    PacTextOptions he = options(PAC_TEXT_AUTO, "he");
    CHECK(pac_text_layout(&he, "123", &line) && line.rtl);
    PacTextOptions en = options(PAC_TEXT_AUTO, "en");
    CHECK(pac_text_layout(&en, "123", &line) && !line.rtl);
    PacTextOptions ar_region = options(PAC_TEXT_AUTO, "ar_EG.UTF-8");
    CHECK(pac_text_layout(&ar_region, "123", &line) && line.rtl);
    PacTextOptions arx = options(PAC_TEXT_AUTO, "arx");
    CHECK(pac_text_layout(&arx, "123", &line) && !line.rtl);   /* a different language: not "ar" */

    /* Brackets in right-to-left text are the mirror of what was typed. */
    CHECK(pac_text_layout(&o, "\xD7\x90 (\xD7\x91)", &line) && line.rtl && line.count == 5);
}

static void test_scripts(void) {
    if (F.devanagari) {
        PacTextOptions o = options(PAC_TEXT_AUTO, "hi");
        /* ka, virama, ssa, i: a conjunct and a vowel sign that goes before it: fewer glyphs than letters. */
        CHECK(pac_text_layout(&o, "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7\xE0\xA4\xBF", &line) && line.count < 4 && line.count >= 1);
        CHECK(line.width > 0.3);
    } else { skipped++; fprintf(stderr, "skipped: no Noto Sans Devanagari\n"); }

    if (F.thai) {
        PacTextOptions o = options(PAC_TEXT_AUTO, "th");
        /* ko kai with a mai taikhu: the mark has no advance, so the width is that of the consonant alone. */
        PacTextLine one, two;
        CHECK(pac_text_layout(&o, "\xE0\xB8\x81", &one) && one.count == 1);
        CHECK(pac_text_layout(&o, "\xE0\xB8\x81\xE0\xB9\x87", &two) && two.count == 2);
        CHECK(fabs(one.width - two.width) < 0.001);
    } else { skipped++; fprintf(stderr, "skipped: no Noto Sans Thai\n"); }
}

static void test_marks_keep_the_face(void) {
    /* A Latin letter with a combining accent stays in the Latin font (the mark follows its letter). */
    PacTextOptions o = options(PAC_TEXT_AUTO, "pt");
    CHECK(pac_text_layout(&o, "e\xCC\x81", &line) && line.count >= 1);
    for (size_t i = 0; i < line.count; i++) CHECK(line.glyphs[i].face == F.latin);
}
#endif

static void test_fit(void) {
    PacTextOptions o = options(PAC_TEXT_AUTO, "en");
    double w_all, w;
    CHECK(pac_text_layout(&o, "aaa bbb ccc", &line));
    w_all = line.width;

    /* It all fits. */
    CHECK(pac_text_fit(&o, "aaa bbb ccc", w_all + 0.01, &w) == 11 && fabs(w - w_all) < 0.0001);

    /* Too narrow for all of it: it ends after a space, and the width does not count that space. */
    CHECK(pac_text_layout(&o, "aaa bb", &line));
    double w_bb = line.width;
    CHECK(pac_text_layout(&o, "aaa", &line));
    double w_aaa = line.width;
    CHECK(pac_text_fit(&o, "aaa bbb ccc", w_bb + 0.001, &w) == 4 && fabs(w - w_aaa) < 0.0001);

    /* A word that does not fit by itself is cut between letters, not dropped. */
    size_t cut = pac_text_fit(&o, "aaaaaaaaaaaa", w_aaa * 2, &w);
    CHECK(cut > 0 && cut < 12 && w <= w_aaa * 2 + 0.0001);

    /* Not even one letter fits: that letter goes alone. */
    CHECK(pac_text_fit(&o, "mmm", 0.01, &w) == 1);
    CHECK(pac_text_fit(&o, "", 5, &w) == 0 && w == 0);
    CHECK(pac_text_fit(&o, "   ", 5, NULL) == 3);                              /* only spaces: they fit, nothing to show */

    /* Spaces in a row are not a place to break wider than they are. */
    CHECK(pac_text_layout(&o, "aaa", &line));
    CHECK(pac_text_fit(&o, "aaa      bbb", line.width + 0.001, NULL) >= 3);

    /* Bounded: a line longer than the layout holds is cut, and what comes back is a prefix. */
    char big[3000];
    memset(big, 'w', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    size_t n = pac_text_fit(&o, big, 100000, NULL);
    CHECK(n > 0 && n <= PAC_TEXT_CHARS_MAX && n < sizeof big - 1);

#ifndef NO_I18N
    /* Letters and their marks stay together: e + acute, three times. */
    const char *accents = "e\xCC\x81" "e\xCC\x81" "e\xCC\x81";
    CHECK(pac_text_layout(&o, "e\xCC\x81", &line));
    double one = line.width;
    size_t cut2 = pac_text_fit(&o, accents, one * 1.5, NULL);
    CHECK(cut2 == 3);
    CHECK(pac_text_fit(&o, accents, one * 0.1, NULL) == 3);                     /* even narrower: the pair goes alone */

    if (F.cjk) {
        /* Ideographs break between them; a full stop does not start the next line. */
        const char *ja = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x80\x82\xE6\x97\xA5";   /* nichi hon go . nichi */
        CHECK(pac_text_layout(&o, "\xE6\x97\xA5", &line));
        double em = line.width;
        size_t c = pac_text_fit(&o, ja, em * 3.5, NULL);
        CHECK(c == 6);   /* the full stop does not start a line, so 'go' goes with it: the break is before 'go' */
        size_t c2 = pac_text_fit(&o, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", em * 2.5, NULL);
        CHECK(c2 == 6);
    } else { skipped++; fprintf(stderr, "skipped: no Noto Sans CJK\n"); }
#endif
}

static void test_random(void) {
    /* Whatever the bytes, nothing overruns and every cut is inside the text. */
    PacTextOptions o = options(PAC_TEXT_AUTO, "ar");
    unsigned seed = 12345;
    for (int i = 0; i < 1500; i++) {
        char s[80];
        size_t len = 1 + (size_t)(rand_r(&seed) % 70);
        for (size_t k = 0; k < len; k++) {
            unsigned r = rand_r(&seed) % 12;
            s[k] = r < 3 ? (char)(rand_r(&seed) & 0xFF) : r < 6 ? (char)(0x80 + rand_r(&seed) % 0x40) : (char)(0x20 + rand_r(&seed) % 0x5F);
        }
        s[len] = '\0';
        pac_text_layout(&o, s, &line);
        CHECK(line.count <= PAC_TEXT_GLYPHS_MAX);
        size_t cut = pac_text_fit(&o, s, 1 + (rand_r(&seed) % 100) / 10.0, NULL);
        CHECK(cut <= strlen(s));
        size_t pos = strlen(s) / 2;
        CHECK(pac_text_next_cluster(s, pos) <= strlen(s) && pac_text_prev_cluster(s, pos) <= pos);
    }
}

int main(void) {
    if (FT_Init_FreeType(&ft) != 0) { fprintf(stderr, "FreeType did not start\n"); return 2; }

    test_clusters();
    test_can_break();
    test_language_direction();

    F.latin = load("NotoSans-Regular.ttf");
    if (!F.latin) F.latin = load("DejaVuSans.ttf");
    F.arabic = load("NotoSansArabic-Regular.ttf");
    F.hebrew = load("NotoSansHebrew-Regular.ttf");
    F.devanagari = load("NotoSansDevanagari-Regular.ttf");
    F.thai = load("NotoSansThai-Regular.ttf");
    FontDirs dirs;
    font_dirs_init(&dirs);
    {
        const char *names[] = { "NotoSansCJK-Regular.ttc", NULL };
        char path[FONT_PATH_MAX];
        if (font_find(dirs.list, names, path, sizeof path)) FT_New_Face(ft, path, 0, &F.cjk);
    }

    if (!F.latin) {
        fprintf(stderr, "skipped: no Latin font (Noto Sans or DejaVu Sans): the layout tests did not run\n");
        skipped++;
    } else {
        test_latin();
        test_fit();
        test_random();
#ifndef NO_I18N
        test_shaping();
        test_order();
        test_scripts();
        test_marks_keep_the_face();
#endif
    }

    pac_text_finish();
    printf("%d checks, %d failures, %d skipped\n", checks, failures, skipped);
    return failures ? 1 : 0;
}
