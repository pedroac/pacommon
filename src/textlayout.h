#ifndef PACOMMON_TEXTLAYOUT_H
#define PACOMMON_TEXTLAYOUT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "tunables.h"

/* Text that is drawn right in every script: shaping (Arabic letters that join, the vowel signs of
 * Indic scripts and Thai), right-to-left and mixed text in the right order, lines that break where
 * the script allows it, and cuts that do not tear a letter from its marks. The tool draws the glyphs;
 * this works out which, and where.
 *
 * Built with I18N (the default of the tools): HarfBuzz and FriBidi do the work (pkg-config harfbuzz
 * fribidi; the tool adds them to its PKGS). Built with -DNO_I18N: the same functions, with none of
 * that, for a tool that is English only: one glyph for each character, from left to right, and cuts
 * between characters. A tool calls the same functions either way.
 *
 * Nothing here is thread safe (the shaping buffer and the font cache are static), and every length
 * is bounded by PAC_TEXT_CHARS_MAX characters: what is longer is cut at a cluster. */

typedef enum {
    PAC_TEXT_AUTO,     /* from the first strong letter; else from `language` (use for text of unknown language) */
    PAC_TEXT_LTR,
    PAC_TEXT_RTL,
} PacTextDirection;

typedef struct {
    /* The face to draw a character with: the main font if it has the letter, else the one of its
     * script. NULL means no font has it, and `fallback` shows its missing-glyph box. Marks and zero
     * width joiners are drawn with the face of the letter before them if it has them. */
    FT_Face (*pick)(uint32_t cp, void *data);
    void *data;
    FT_Face fallback;            /* not NULL */
    const char *language;        /* "ar", "th", "ja": for the shaping and the default direction; or NULL */
    PacTextDirection direction;
} PacTextOptions;

typedef struct {
    FT_Face face;
    uint32_t glyph;              /* the glyph of that face */
    double x, y;                 /* where to draw it, in em: x from the left end of the line, y from the
                                  * baseline (down is positive, as in Cairo) */
} PacTextGlyph;

typedef struct {
    PacTextGlyph glyphs[PAC_TEXT_GLYPHS_MAX];   /* in visual order, from left to right */
    size_t count;
    double width;                /* in em */
    bool rtl;                    /* the direction of the paragraph that was used */
} PacTextLine;

/* Whether `language` ("ar", "he", "fa_IR", "ar-EG.UTF-8") is written from right to left. A text in that
 * language is a right-to-left paragraph even when it starts with a Latin word ("Caps Lock" in an Arabic
 * message): a tool passes PAC_TEXT_RTL (or LTR) for its own texts, and PAC_TEXT_AUTO for what it does not
 * know the language of (a name typed by someone). Always false in a NO_I18N build. */
bool pac_text_language_rtl(const char *language);

/* Lays out one line (no line breaks in it). Sizes are in em: multiply by the font size. False if
 * `fallback` is NULL or the text is empty (count and width are then 0). */
bool pac_text_layout(const PacTextOptions *o, const char *utf8, PacTextLine *out);

/* How many bytes of utf8 (one paragraph) fit in max_em: the longest start of it that does, cut where
 * the script allows a line to end (after a space or a hyphen, between ideographs), else after the last
 * letter that fits; at least one letter, even if it is wider. What is left starts after any spaces:
 * the caller skips them. width_em (may be NULL) gets the width of what fits, without its end spaces. */
size_t pac_text_fit(const PacTextOptions *o, const char *utf8, double max_em, double *width_em);

/* Where the cluster (a letter and its marks, an emoji sequence, a Hangul syllable, a pair of regional
 * indicators) that starts at pos ends, and where the one before pos starts. pos is a byte offset of
 * utf8, a boundary or inside a cluster. */
size_t pac_text_next_cluster(const char *utf8, size_t pos);
size_t pac_text_prev_cluster(const char *utf8, size_t pos);

/* Whether a line may end between the characters prev and next. */
bool pac_text_can_break(uint32_t prev, uint32_t next);

/* Releases what the shaping keeps (the fonts it made). Call before the FT_Faces are freed. */
void pac_text_finish(void);

#endif
