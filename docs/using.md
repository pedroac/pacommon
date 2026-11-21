# Using pacommon in a tool

pacommon is source, not a library. A tool's `Makefile` compiles the files it calls and links the
objects into its own binary, so the linker keeps only what is used.

```make
PACOMMON ?= ../pacommon
CFLAGS   += -I$(PACOMMON)/src
COMMON_SRCS = $(PACOMMON)/src/config_file.c $(PACOMMON)/src/fontfind.c
COMMON_OBJS = $(COMMON_SRCS:$(PACOMMON)/src/%.c=build/pacommon_%.o)

build/pacommon_%.o: $(PACOMMON)/src/%.c
	$(CC) $(ALL_CFLAGS) -DPACOMMON_I18N='"i18n.h"' -c $< -o $@
```

`-DPACOMMON_I18N='"i18n.h"'` is only for a tool that translates its messages: `config_file.c`
then includes the tool's `i18n.h` for `_()`. Without it messages stay in English. `config_file.h`
also honours `-DSYSCONFDIR='"/etc"'` for a distribution that keeps system settings elsewhere.

`tunables.h` holds every size, limit and default, which can be changed with `-DNAME=value` when
building ([development.md](development.md)). `compiler.h` is a header only: three macros for compiler hints, to be used where a profile says so
([development.md](development.md#performance-hints)).

## pac.mk: installing

A tool's `Makefile` includes `$(PACOMMON)/pac.mk` and installs what it needs to work out of the box
with `$(call pac_install_keep,SOURCE,DESTINATION,MODE)`, and `$(pac_install_shared_conf)` for
`pac.conf`. A file that is already there is kept, with a warning if it differs, and
`make install REPLACE=1` replaces it and saves the old one as `NAME.bak.DATE`. `make uninstall`
removes a file only if it is still the one that was installed. See [development.md](development.md).

## config_file: the settings file

```c
static bool handler(const char *key, const char *value, void *data) {
    Options *o = data;
    if (strcmp(key, "text_scale") == 0) return parse_scale(value, &o->text_scale);
    return false;                      /* unknown key: reported and skipped */
}

Options o = defaults();                /* every setting has a default */
pac_config_load("pacpeek", handler, &o);
/* ...then command-line options, which override the files */
```

- The handler returns `false` for an unknown key or a bad value. `config_file` prints
  `prog: path:line: ignoring "key = value"` on stderr and goes on. It never aborts.
- Lines are `key = value`. `#` starts a comment anywhere on a line, so a value cannot contain
  `#`. Only the first `=` splits, so `url = a=b` works. Spaces around the key and value are
  dropped. A line over 1023 bytes, or with a NUL byte in it, is skipped whole.
- The file must be a regular file of at most 64 KiB (`PAC_CONFIG_MAX_SIZE`). A pipe, a device, a
  folder or a larger file is reported and ignored, and the tool starts anyway. Text from the file
  is shown with control characters replaced by `?` when it is reported.
- `pac_config_load_file` reads one file and returns how many lines were skipped, or `-1` if it is
  missing or cannot be used. A missing file is normal and silent; the other reasons are reported.
- `pac_config_path` gives the path of one location, for a tool that reads the files itself (for
  example to watch them or to check who owns them).

A tool that calls `pac_config_load` gets `pac.conf` first (see
[configuration.md](configuration.md)). A tool that reads its files itself calls
`pac_config_load_shared(prog, handler, data)` before its own: the handler is given only the shared
keys with sane values, and its refusal of one is silent.

The files are read in this order, each overriding the earlier. The handler is called for every
line of every file, so the last call for a key wins.

| # | Path |
| --- | --- |
| 0 | `/etc/pac/<tool>.conf` |
| 1 | `$XDG_CONFIG_HOME/pac/<tool>.conf`, or `~/.config/pac/<tool>.conf` |
| 2 | `./<tool>.conf` |

## setting: settings read where they are needed

For a tool whose code would otherwise call `getenv()` in many places. The values come from the
settings file and never from the environment.

```c
pac_config_load("pacreact", pac_setting_store, NULL);   /* keep every key */
const char *font = pac_setting("font");                 /* NULL if not set */
```

`pac_setting_set(key, NULL)` unsets a key, and `pac_setting_clear()` frees everything. The pointer
from `pac_setting` is valid until the next change to any setting. The store is meant for a few dozen settings. The store is one block of `PAC_SETTING_POOL` bytes (4,096 by
default, no allocation): a key of at most 63 bytes, a value of at most 1023, and what fits in the
block. Anything beyond that is refused (`false`) and what is stored stays as it was.

## fontfind: finding a font file

```c
FontDirs dirs;
font_dirs_init(&dirs);                       /* XDG_DATA_HOME, ~/.fonts, XDG_DATA_DIRS */
const char *names[] = { "DejaVuSans.ttf", "LiberationSans-Regular.ttf", NULL };
char path[256];
if (!font_find(dirs.list, names, path, sizeof path)) { /* say so once, use the fallback */ }
```

The earliest name in `names` wins wherever it is found, and the search stops as soon as the first
name is found. Symbolic links are followed. Unreadable files are ignored. Only absolute directories are used: a relative `$XDG_DATA_HOME` or `$XDG_DATA_DIRS` entry is
ignored. `font_find` keeps its
state in statics, so it is **not thread-safe**.

## field: a line of text for a password or a prompt

`Field` holds up to `FIELD_MAX - 1` bytes of UTF-8 that are only edited at the end.

- `field_append_key` remembers the key (an evdev code) and modifiers that typed each character, so
  `field_retype` can type the same keys again after a keyboard layout change. Text that did not
  come from one key (`field_append`, a compose sequence) is left alone.
- `field_edit_key` is the one set of editing rules for every field of every tool: Backspace deletes
  a character, Ctrl+Backspace and Ctrl+U clear the field, and text is appended unless Ctrl, Alt or
  Logo is held or it starts with a control character. The tool handles its own keys (Enter, Tab,
  arrows, Escape) first, and calls this with the rest. It returns `FIELD_KEY_EDITED` only when the
  text changed (Backspace in an empty field is `FIELD_KEY_IGNORED`, so a caller does not redraw or
  reset anything for it), and `FIELD_KEY_FULL` when text does not fit, leaving the field as it was.
  `max` is a limit of the tool's own, in bytes (`0` for the size of the field). The keysyms are
  written as numbers in `field.c`, so it needs no xkbcommon.
- Malformed UTF-8 is kept byte by byte. It is never read past its end.
- `field_backspace` and `field_clear` use `explicit_bzero`, so a password does not stay in memory.
  The caller must call `field_clear` when it is done with a field and must not copy it around.
- Appends fail, leaving the field unchanged, when the text does not fit.
- A `Field` is checked on use: if its length, count or end no longer agree (it was written to
  directly, or never initialized) it is wiped rather than trusted, and `field_retype` returns
  `false`. A translate callback that does not end its text is not read past its buffer.

## utf8: reading one character

`utf8_decode(s, &cp)` returns the length in bytes of the sequence at the start of `s` and its code
point, or 0 if it is not valid: a stray continuation byte, an overlong form, a surrogate, a value
above U+10FFFF, or a sequence cut short by the end of the string. `s` must end with a NUL; nothing
is read past it. The caller decides what to do with a 0 (skip one byte, stop, replace it).

## language: choosing the language of the texts

`pac_set_language("pt_BR:pt")` sets `LANGUAGE` and the message locale so that gettext uses that
language even in a greeter or a daemon that starts with no locale. An empty string leaves the
environment's choice; a name with anything but letters, digits and `_-.@:`, or longer than
`PAC_LANGUAGE_MAX`, returns `false` and changes nothing. It needs no gettext, so a tool built with
`I18N=0` simply does not call it. It changes the environment: call it once, at start.

## atomic_file: writing a file whole or not at all

`atomic_write(path, data, len)` writes to a new private file (`.name.pid`, mode 0600, `O_EXCL`,
`O_NOFOLLOW`) in the same folder, calls `fsync`, and renames it over `path`. On any failure it
returns `false` with `errno` set, removes its temporary file and leaves `path` as it was.

- The folder must exist: the caller makes it, and chooses its mode.
- A symbolic link at `path` is replaced, not followed; a link at the temporary name is not followed.
- The data is in memory, so a caller that has a lot to write builds it first. A path of
  `PAC_FILE_PATH_MAX` bytes or more is refused with `EINVAL`.

## layout: keyboard layouts and the picker

For a tool that lets a person choose the keyboard layout (pacgreet, paclock). `layouts_load` gives
the layouts to offer: the comma-separated specs of a setting (`pt, us(intl)`, a space before the
variant is allowed), described with the names xkeyboard-config has, or every layout it has when
there is no setting. `layout_parse_spec` and `layout_spec` read and write one spec. Specs and lists
that do not fit their fields (`Layout` has 24 bytes for the code and for the variant, 96 for the
name) are refused or cut, never overflowed.

- `LayoutPicker` holds the layouts, the one in use, and the search typed to find one (a `Field`).
  Number 0 is always the compositor's own layout, which has no name to search. Typing never changes
  the layout in use, because the next key would be read with it: a search only proposes one
  (`pick`), and `picker_settle` returns the one to switch to. `picker_step` goes round the layouts,
  past one marked `unavailable`.
- `pac_layout_name` is a layout's name in the person's language, from xkeyboard-config's own
  catalog; the name itself where there is none, and always with `-DNO_I18N`. A tool names it
  `i18n_layout_name` in its `i18n.h`.
- It is compiled by the tools that list it (paclock does not with `LAYOUTS=0`).
- `MAX_LAYOUTS` and `LAYOUT_LIST_MAX` (the largest rules list that is read) are in `tunables.h`.
  `LayoutPicker` is `MAX_LAYOUTS + 1` layouts, so build every file of a tool with the same value.
- `layout_detect.[ch]` is separate, for the tool that needs it (a greeter: the compositor does not
  know the system's layout). `layout_detect(etc, ...)` reads Xorg configuration, then
  `default/keyboard`, then the `KEYMAP` of `vconsole.conf`, under the folder `etc`.

## textdraw: drawing text right in every script

For a tool that draws with Cairo. `textlayout` works out which glyphs and where; `textdraw` finds the
font of each letter and draws them, the same way in every tool.

1. `pac_font_init()`, then a face for the main font with `pac_font_load(path, names, index)` (the file at
   `path`, else the first of `names` found in the font directories; the caller owns it), set on the
   Cairo context as usual (`cairo_set_font_face`, `cairo_set_font_size`). `PAC_FONTS_REGULAR` and
   `PAC_FONTS_BOLD` are the names the tools prefer: Atkinson Hyperlegible first, made for readability,
   then Noto Sans, DejaVu Sans and Liberation Sans. **Install `ttf-atkinson-hyperlegible`** (Arch; other
   distributions call it `fonts-atkinson-hyperlegible` or `atkinson-hyperlegible-fonts`) for it; without it
   the next one is used.
2. `pac_font_set_language("ar")` for the shaping, the direction of a line with no letter that says it,
   and which of the Chinese, Japanese and Korean faces draws the ideographs.
3. `pac_font_width`, `pac_font_show` (or `pac_font_text(cr, s, draw)`) measure and draw one line from the
   current point with the font and size that are set. A letter the main font does not have comes from the
   font of its script (Noto Sans Arabic, Devanagari, Thai, CJK, ...), looked up and loaded the first time
   it is needed (`pac_font_lazy` is the same lookup, for a tool that wants a font of its own by file name).
   A face that is not from `pac_font_load` (Cairo's own) is drawn by Cairo, without shaping.
4. `pac_font_fit` shortens a line with an ellipsis (at the end, or at the start for what is being typed),
   cutting between clusters; `pac_font_cut` says how many bytes fit, for wrapping onto a second line.
5. `pac_font_caret(cr, text, at, &dir)` says where a caret goes at byte offset `at` of a line drawn with
   `pac_font_text`, in pixels from where the line started, and which way the text goes from there; and
   `pac_font_caret_x(cr, x0, text, gap, width)` is the left edge of a caret bar at the *end* of text drawn from
   `x0`, which is what a field that is typed into wants. The place comes from the layout, so it is right in a line
   that mixes directions: after Latin letters it is at their right, after Arabic or Hebrew ones at their left, and
   in "web" followed by Arabic in a left-to-right line it is in the middle, where the Arabic run begins. An
   empty text has no place to be next to: it is the start, with the direction of the language.
5. `pac_font_finish(faces, n)` ends it all, in the order that is safe: the shaping, the faces the tool
   made, the script fonts, FreeType. The tool is about to exit. Never call `cairo_debug_reset_static_data`
   in a program that draws with these: Cairo's cache still refers to the faces.

Where a script's font is not installed its letters are drawn as the missing-glyph box; the fonts are
looked for by their Noto names in the font directories (`fontfind`). Right-to-left text is put in order
and each line is left aligned inside the box the tool gives it: turning a whole layout over for a right-to-left
language (tiles, icons, scrollbar, arrows) is the tool's to do, with `pac_text_language_rtl`; pacmenu does it
(`ui_set_direction`), and its entry in `pacmenu/docs/decisions.md` says how.
Not thread safe. Built with HarfBuzz and FriBidi, or with `-DNO_I18N` for English only; needs Cairo.

## layout_text: the words of the layout picker

`layout_text_hint()` ("Type to search"), `layout_text_nothing()` ("No keyboard matches") and
`layout_text_name(out, size, name)` ("Portuguese keyboard") are what a tool shows and says about the
picker, so that they are the same in every tool, on the screen and aloud, and translated once. They are
in a file of their own so that a tool built without `layout.c` can still use them. Name the tool's
`i18n.h` with `-DPACOMMON_I18N='"i18n.h"'` for the texts to be translated, and add the file to the
`xgettext` of the tool's `pot` target. When to say them is the tool's: what a tool says when the
focus enters the picker is its own to decide, but the words are these.

## speech: speaking what the screen shows

```make
include $(PACOMMON)/pac.mk                      # SPEECH=speechd (default) or none
PKGS += $(PAC_SPEECH_PKGS)                      # speech-dispatcher, or nothing
ALL_CFLAGS += $(PAC_SPEECH_CFLAGS)              # -DSPEECH_SPEECHD, or nothing; for speech.c too
COMMON_SRCS += $(PACOMMON)/src/speech.c
```

```c
#include "speech.h"
bool speaking = speech_open("paclock", settings.language);   // before anything that must not wait
speech_say("Authentication failed");                         // cuts off what is still being said
speech_cancel();                                             // stop speaking, say nothing
speech_finish(2000);                                         // before exiting: let it end, at most 2 s
speech_close();
```

For a focus that moves fast (typing a search, holding an arrow key) use `speech_announce` instead of
`speech_say` on every change: it says only where the focus stops, the lead (results, section) only
when it changed, and the description later, without cutting the name off.

```c
speech_announce(&(SpeechAnnounce){ .lead = "12 results.", .name = "Firefox", .desc = "Web browser",
                                   .settle_ms = 150, .desc_ms = 1000 });
int wait = speech_next_ms();                    // -1: nothing planned; use it in the event wait's timeout
/* ... wait for events, at most `wait` ... */  speech_tick();
```

Several tools (and a screen reader) speak through the same server, which keeps one order between
all of them. `speech_say_as(priority, text)` picks where a message stands: `SPEECH_IMPORTANT` (an
error; never cut off, said at once), `SPEECH_MESSAGE` (waits for its turn, cuts off text and
notifications), `SPEECH_TEXT` (what is being looked at; cuts off the text before it, waits for
messages) and `SPEECH_NOTIFICATION` (dropped if anything else is being said). `speech_say` is
`SPEECH_MESSAGE` that also cancels this tool's own earlier speech; `speech_announce` says the name
as `SPEECH_TEXT` (or `.priority`) and the description as `SPEECH_NOTIFICATION`.

The tool decides what is spoken and when; this only talks to the server. Speak only what is also on
the screen, and never a password or what is typed. `speech_open` returns false (and says so once on
stderr, translated like `config_file.c`'s messages) if the server cannot be reached: the tool goes
on in silence. Without `-DSPEECH_SPEECHD` every function does nothing and no library is needed.
`speech_locale` and `speech_language_code` give the language for the server from the tool's
`language` setting or the environment. Reading the `speech` setting is the tool's job (it is in the
shared `pac.conf` keys). `SPEECH=atspi` is not implemented: `pac.mk` refuses it.

## palette: the colors every tool shares

```c
#include "palette.h"
const PacPalette *p = high_contrast ? &PAC_PALETTE_HIGH : &PAC_PALETTE_NORMAL;
```

`PAC_PALETTE_NORMAL` and `PAC_PALETTE_HIGH` are the two looks of principle 33 (a calm palette, and
black and white with nothing dimmed). A tool uses them instead of its own colors so that every
tool matches. `pac_contrast` is the WCAG ratio; `pac_palette_contrast_ok` and
`pac_palette_meanings_apart` are the checks the tests run.

## length: sizes with a unit, and from the screen to pixels

```c
#include "length.h"
PacLength min;
if (!pac_length_parse("3mm", &min)) { /* "3" has no unit: refused */ }
double ppm = pac_px_per_mm(physical_mm, mode_width_px, scale);   /* wl_output geometry, mode */
double px = pac_length_clamp(size_px, &min, &max, ppm, screen_short_side);
```

`pac_length_parse` is in the header, so checking a setting needs no other file (`config_file.c` does it for
`text_size_min` and `text_size_max`); `pac_px_per_mm` and `pac_length_clamp` are in `length.c`, which a tool that
converts adds to its `COMMON_SRCS`.

Units are `px` (logical pixels), `mm`, `in`, `pt` (1/72 inch) and `%` (of `whole`, which the tool picks).
`pac_px_per_mm` takes the physical width in millimetres (`wl_output.geometry`), the pixel width of the
current mode (`wl_output.mode`, flagged current) and the output's scale, and returns logical pixels
per millimetre. A size that is 0 or makes under 50 or over 600 dots per inch (the `PAC_DPI_*`
limits) is not believed: it gives 96 dpi. `pac_px_per_mm_known` gives 0 there instead, for a size that
is worse on a guess than on the tool's own rule. The tool listens to `wl_output`; this code does not.

## textlayout: text drawn right in every script

```c
#include "textlayout.h"
PacTextOptions o = { pick, NULL, main_face, "ar", PAC_TEXT_RTL };   /* pick: the face of a code point */
static PacTextLine line;
if (pac_text_layout(&o, utf8, &line)) {
    for (size_t i = 0; i < line.count; i++)   /* in visual order, from left to right; sizes are in em */
        draw(line.glyphs[i].face, line.glyphs[i].glyph, x + line.glyphs[i].x * size, y + line.glyphs[i].y * size);
    double width = line.width * size;
}
size_t cut = pac_text_fit(&o, paragraph, max_width / size, NULL);   /* where a line may end */
/* ... and where a caret goes, for a line of text that is being typed (in em from the left end of the line): */
double x; int dir = pac_text_caret(&o, text, strlen(text), &x);   /* dir is +1 to the right, -1 to the left, 0 for none */
```

The tool chooses the fonts (`pick`: the main font if it has the letter, else one of its script, or NULL) and draws
the glyphs; this module does the rest: **shaping** (HarfBuzz: Arabic letters that join, the vowel signs and conjuncts
of Indic scripts, Thai marks), the **order** of right-to-left and mixed text (FriBidi, with the brackets), **lines**
that break after a space or a hyphen and between ideographs (a full stop does not start a line), and **cuts** that do not
tear a letter from its marks (`pac_text_next_cluster`, `pac_text_prev_cluster`: Thai and Indic marks, Hangul syllables,
emoji sequences, flags). `pac_text_language_rtl` says whether a language is written from right to left: a text in
Arabic is a right-to-left paragraph even when it starts with "Caps Lock", so a tool gives `PAC_TEXT_RTL` or
`PAC_TEXT_LTR` for its own texts, and `PAC_TEXT_AUTO` (the first strong letter) for what it does not know.

A tool adds `harfbuzz fribidi` to its `PKGS` when built with I18N, and `textlayout.c` to its `COMMON_SRCS`. Built
with `-DNO_I18N` the same functions need neither library: one glyph for each letter, from left to right, and cuts
between letters (no marks joined). Nothing is thread safe, and a line is at most `PAC_TEXT_CHARS_MAX` letters.
`pac_text_finish` before the faces are freed. **Not done:** Thai, Lao, Khmer and Myanmar have no break opportunity
between words (that needs a dictionary), so a line of them breaks after the last letter that fits; the cluster
of an Indic conjunct (consonant, virama, consonant) is two clusters; a line wraps as its own paragraph (bidi runs do
not carry over to the next line). Tested with Noto Sans fonts where installed (`tests/text_test.c`); the font tests
say what they skipped.
