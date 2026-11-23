#ifndef PACOMMON_FCFONT_H
#define PACOMMON_FCFONT_H

#include <stdbool.h>
#include <stddef.h>

/* Which fonts a tool draws with (through fontconfig; fontfind.c is the one without it). It prefers Nerd Fonts: the text in one, and the symbols
 * (search, lock, the kinds of apps) from one, so that they all have the same style. */

/* True if fontconfig has a font of exactly this family. */
bool font_installed(const char *family);

/* The font file that fontconfig would use for a family (and its bold), with the face's number in it
 * for a collection: what the text drawing needs, which does not take a family name. False if
 * fontconfig has nothing for it. */
bool font_file(const char *family, bool bold, char *path, size_t size, int *index);

/* The family for the text: `wanted` if it is a name (and installed), else, for "auto", the first
 * installed Nerd Font of a short list, else "Sans". */
void fonts_pick_text(const char *wanted, char *out, size_t size);

/* The family for the symbols: the text font if it is a Nerd Font, else the first installed Nerd
 * Font of a short list. Empty if there is none: the tool then draws what it can without symbols. */
void fonts_pick_symbols(const char *text, char *out, size_t size);

#endif
