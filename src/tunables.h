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
_Static_assert(PAC_SETTING_KEY_MAX >= 1 && PAC_SETTING_VALUE_MAX >= 0 &&
               PAC_SETTING_POOL >= PAC_SETTING_KEY_MAX + PAC_SETTING_VALUE_MAX + 2,
               "PAC_SETTING_POOL must hold at least one setting of the largest size");
_Static_assert(PAC_SPEECH_PART_MAX >= 32, "PAC_SPEECH_PART_MAX is too small");
_Static_assert(FIELD_MAX >= 16 && FIELD_MAX <= 65536, "FIELD_MAX is out of range");
_Static_assert(FONT_MAX_DIRS >= 1 && FONT_DIR_MAX >= 32 && FONT_PATH_MAX >= 32 && FONT_WALK_FDS >= 1,
               "the font search limits are too small");

#endif
