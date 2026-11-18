#ifndef PACOMMON_TUNABLES_H
#define PACOMMON_TUNABLES_H

/* Everything in the shared code that a person may want to change: sizes, limits and defaults.
 * Each one can be set when building, without editing a file:
 *
 *     make CFLAGS="-O2 -DFIELD_MAX=1024 -DPAC_SETTING_POOL=8192"
 *
 * Build every file of a tool with the same values: some of them are the size of a structure that
 * is shared between files (FIELD_MAX is the size of a Field). A value that would make the code
 * unsafe or useless is refused by the checks at the end of this file, at build time.
 *
 * Features that are switched on or off when building (SPEECH, I18N, ...) belong to each tool's own
 * tunables.h, with its sizes. pacommon has one: -DPACOMMON_I18N='"i18n.h"' (config_file.c). */

/* ---- config_file ---- */

/* Where the system-wide settings are. A distribution that keeps them elsewhere sets this. */
#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif
/* The largest settings file that is read, in bytes. A real one is a few hundred. */
#ifndef PAC_CONFIG_MAX_SIZE
#define PAC_CONFIG_MAX_SIZE 65536
#endif
/* The longest line of a settings file, in bytes. A longer one is skipped whole. */
#ifndef PAC_CONFIG_MAX_LINE
#define PAC_CONFIG_MAX_LINE 1023
#endif
/* The longest path of a settings file, in bytes, including the end. */
#ifndef PAC_CONFIG_PATH_MAX
#define PAC_CONFIG_PATH_MAX 1024
#endif

/* The range of the shared text_scale setting (0.5 to 2.5, as docs/configuration.md says). */
#ifndef PAC_TEXT_SCALE_MIN
#define PAC_TEXT_SCALE_MIN 0.5
#endif
#ifndef PAC_TEXT_SCALE_MAX
#define PAC_TEXT_SCALE_MAX 2.5
#endif

/* A length with a unit (length.c): the largest value one may have, and the dots per inch an
 * output may report before it is not believed (96 is then assumed). */
#ifndef PAC_LENGTH_MAX
#define PAC_LENGTH_MAX 100000.0
#endif
/* The text size limits a person sets (text_size_min, text_size_max) are held between these, in
 * millimetres: a maximum under the floor, or a minimum over the ceiling, is taken as a mistake and
 * not as a wish (docs/configuration.md). */
#ifndef PAC_TEXT_MM_FLOOR
#define PAC_TEXT_MM_FLOOR 2.0
#endif
#ifndef PAC_TEXT_MM_CEILING
#define PAC_TEXT_MM_CEILING 40.0
#endif
#ifndef PAC_DPI_MIN
#define PAC_DPI_MIN 50.0
#endif
#ifndef PAC_DPI_MAX
#define PAC_DPI_MAX 600.0
#endif
#ifndef PAC_DPI_DEFAULT
#define PAC_DPI_DEFAULT 96.0
#endif

/* textlayout.c: the most characters of one line it lays out, the most glyphs it gives back (a few
 * characters can become more than one glyph), and the fonts it keeps shaping data for. */
#ifndef PAC_TEXT_CHARS_MAX
#define PAC_TEXT_CHARS_MAX 256
#endif
#ifndef PAC_TEXT_GLYPHS_MAX
#define PAC_TEXT_GLYPHS_MAX 320
#endif
#ifndef PAC_TEXT_FONTS_MAX
#define PAC_TEXT_FONTS_MAX 16
#endif

/* ---- setting ---- */

/* Settings are kept in one block of this many bytes, with no allocation: each takes its key, its
 * value and two ends. The biggest tool so far (66 options, every one set) needs about 3,000. The
 * block is in static memory that is not touched, so it is not resident, until it is used. */
#ifndef PAC_SETTING_POOL
#define PAC_SETTING_POOL 4096
#endif
#ifndef PAC_SETTING_KEY_MAX      /* bytes in a key */
#define PAC_SETTING_KEY_MAX 63
#endif
#ifndef PAC_SETTING_VALUE_MAX    /* bytes in a value */
#define PAC_SETTING_VALUE_MAX 1023
#endif

/* ---- field ---- */

/* Bytes in a Field, ending included, and so at most FIELD_MAX - 1 characters of a password. */
#ifndef FIELD_MAX
#define FIELD_MAX 512
#endif

/* ---- fontfind ---- */

#ifndef FONT_MAX_DIRS            /* directories searched */
#define FONT_MAX_DIRS 16
#endif
#ifndef FONT_DIR_MAX             /* bytes in the path of one of them */
#define FONT_DIR_MAX 256
#endif
#ifndef FONT_PATH_MAX            /* bytes in the path of the font that is found */
#define FONT_PATH_MAX 256
#endif
#ifndef FONT_WALK_FDS            /* file descriptors the folder walk may hold open */
#define FONT_WALK_FDS 16
#endif

/* ---- speech ---- */

/* The longest part (lead, name or description) of speech_announce, in bytes, including the end. */
#ifndef PAC_SPEECH_PART_MAX
#define PAC_SPEECH_PART_MAX 256
#endif

/* ---- checks: a build with values that cannot work stops here ---- */

_Static_assert(PAC_CONFIG_MAX_LINE >= 63 && PAC_CONFIG_MAX_LINE < PAC_CONFIG_MAX_SIZE,
               "PAC_CONFIG_MAX_LINE must hold a setting and be smaller than a file");
_Static_assert(PAC_CONFIG_PATH_MAX >= 64, "PAC_CONFIG_PATH_MAX is too small for a path");
_Static_assert(PAC_TEXT_SCALE_MIN > 0 && PAC_TEXT_SCALE_MAX > PAC_TEXT_SCALE_MIN,
               "the text scale range is wrong");
_Static_assert(PAC_LENGTH_MAX > 0 && PAC_DPI_MIN > 0 && PAC_DPI_MAX > PAC_DPI_MIN &&
               PAC_DPI_DEFAULT >= PAC_DPI_MIN && PAC_DPI_DEFAULT <= PAC_DPI_MAX,
               "the length limits are wrong");
_Static_assert(PAC_TEXT_MM_FLOOR > 0 && PAC_TEXT_MM_CEILING > PAC_TEXT_MM_FLOOR,
               "the text size sanity range is wrong");
_Static_assert(PAC_TEXT_CHARS_MAX >= 16 && PAC_TEXT_GLYPHS_MAX >= PAC_TEXT_CHARS_MAX && PAC_TEXT_FONTS_MAX >= 2,
               "the text layout limits are too small");
_Static_assert(PAC_SETTING_KEY_MAX >= 1 && PAC_SETTING_VALUE_MAX >= 0 &&
               PAC_SETTING_POOL >= PAC_SETTING_KEY_MAX + PAC_SETTING_VALUE_MAX + 2,
               "PAC_SETTING_POOL must hold at least one setting of the largest size");
_Static_assert(PAC_SPEECH_PART_MAX >= 32, "PAC_SPEECH_PART_MAX is too small");
_Static_assert(FIELD_MAX >= 16 && FIELD_MAX <= 65536, "FIELD_MAX is out of range");
_Static_assert(FONT_MAX_DIRS >= 1 && FONT_DIR_MAX >= 32 && FONT_PATH_MAX >= 32 && FONT_WALK_FDS >= 1,
               "the font search limits are too small");

#endif
