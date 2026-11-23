#ifndef PACOMMON_TEXTDRAW_H
#define PACOMMON_TEXTDRAW_H

#include "fontfind.h"
#include "textlayout.h"
#include <cairo/cairo.h>
#include <stdbool.h>
#include <stddef.h>

/* Draws and measures text with Cairo, right in every script: textlayout.h works out which glyphs and
 * where, and this finds the font of each letter (the main font if it has it, else the one of its
 * script, loaded the first time such a letter is drawn) and draws them. What pacgreet and paclock each
 * had, so that every tool that draws text does it the same way.
 *
 * A tool loads its own fonts with pac_font_load, sets one on the Cairo context as usual
 * (cairo_set_font_face and cairo_set_font_size), and then measures and draws with the functions below:
 * they use the font and the size that are set. A face that did not come from here (Cairo's own, from
 * cairo_toy_font_face_create) is drawn by Cairo, without shaping.
 *
 * Built with I18N (HarfBuzz and FriBidi, see textlayout.h) or with -DNO_I18N. Nothing here is thread
 * safe: the state is static, as textlayout's is. */

/* Starts FreeType. False if it cannot. Call first; calling it again does nothing. */
bool pac_font_init(void);

/* Ends everything: the shaping, then `faces` (the count that the tool made or got from pac_font_load,
 * each destroyed once; NULL entries are skipped), then the fonts loaded for other scripts, then
 * FreeType, in the order that is safe. The tool must not use any face afterwards, and is about to exit:
 * Cairo keeps a cache of the fonts it drew with, which still refers to the faces, so cairo_debug_reset_static_data
 * must not be called, before or after this, in a program that draws with them. */
void pac_font_finish(cairo_font_face_t *const *faces, size_t count);

/* The face of the font file at `path` (the face `index` of a collection), or, if there is no such
 * file or path is NULL, of the first of `names` (file names, NULL at the end) found in the system's
 * font directories. NULL if there is none. The caller owns it: pass it to pac_font_finish. */
cairo_font_face_t *pac_font_load(const char *path, const char *const *names, int index);

/* PAC_FONTS_REGULAR, PAC_FONTS_BOLD and PAC_FONTS_LIGHT, the names for pac_font_load, are in fontfind.h. */

/* The face of a font file looked for by its name when first asked for, and remembered, a miss too
 * (searching the font directories is not free). NULL if there is none. Owned here: not to be destroyed. */
cairo_font_face_t *pac_font_lazy(const char *file, int index);

/* The FreeType face behind a face from pac_font_load or pac_font_lazy; NULL for any other. */
FT_Face pac_font_ft(cairo_font_face_t *face);

/* The language the texts are in ("ar", "zh-TW", "pt_BR.UTF-8"), for the shaping, for the direction of
 * a line that has no letter that says it, and for which of the Chinese, Japanese and Korean faces draws
 * the ideographs. Empty for none. */
void pac_font_set_language(const char *language);

/* Draw glyphs as paths, not with Cairo's glyph cache (about 260 kB of memory the first time). */
void pac_font_set_paths(bool on);

/* Lays out one line of utf8 from the current point and draws it if `draw`; returns its width. The
 * current point moves to the end of what was drawn. */
double pac_font_text(cairo_t *cr, const char *utf8, bool draw);
double pac_font_width(cairo_t *cr, const char *utf8);
void pac_font_show(cairo_t *cr, const char *utf8);

/* Where a caret goes at byte offset `at` of a line of utf8 drawn with pac_font_text, in pixels from where the
 * line starts (the current point when it was drawn), for the font and the size that are set. Placed by the
 * layout itself, so it is right in a line that mixes directions (see pac_text_caret). *dir, if not NULL, gets
 * the direction the text goes from the caret, +1 to the right and -1 to the left; for an empty text, the
 * direction of the language (pac_font_set_language). */
double pac_font_caret(cairo_t *cr, const char *utf8, size_t at, int *dir);

/* Marks what an input method is composing in a line that was drawn from x0 with its baseline at `baseline`: the last
 * `composing` bytes of `shown`, underlined with a line `thick` thick in the current color, and with a line twice as
 * thick under the part that the input method is on, from byte `begin` to `end` of it (equal, or -1: none). Returns the
 * x of the cursor in it, at `begin`, or NaN when the input method asks for none to be shown (begin is -1). */
double pac_font_preedit(cairo_t *cr, double x0, double baseline, const char *shown, size_t composing, int begin, int end,
                        double thick);

/* The two ends of the part of a line between byte offsets `from` and `to`, for the line drawn from x0: for underlining
 * what an input method is composing. It is one piece, between where the caret is at each of the two offsets, even
 * where the directions of the text mix. */
void pac_font_span(cairo_t *cr, double x0, const char *utf8, size_t from, size_t to, double *left, double *right);

/* The x of the left edge of a caret bar `width` wide at the end of utf8, which was drawn starting at x0: `gap` away
 * from the end of the text, on the side the text goes on (the right of left-to-right letters, the left of
 * right-to-left ones: pac_font_caret). For the text that is being typed. */
double pac_font_caret_x(cairo_t *cr, double x0, const char *utf8, double gap, double width);

/* Copies text to out (of size), shortened with an ellipsis until it fits max_w: at the end for a label,
 * at the start (keep_end) for what is being typed, where the cursor is. Cut between clusters, never
 * between a letter and its marks. */
void pac_font_fit(cairo_t *cr, const char *text, double max_w, bool keep_end, char *out, size_t size);

/* How many bytes of utf8 fit in max_w, cut where the script allows a line to end: for wrapping a
 * line onto a second one. At least one letter. */
size_t pac_font_cut(cairo_t *cr, const char *utf8, double max_w);

#endif
