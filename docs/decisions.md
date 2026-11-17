# Decisions

Implementation decisions that the code does not explain by itself. Newest first. Format: what was
decided, why, and what else was considered.

The entries marked "recorded from the code" were written down on 2026-11-15 from the code, its
comments and `PRINCIPLES.md`. The author should confirm that the reasons are the real ones.

## 2026-11-16: text layout in pacommon (`textlayout.[ch]`)

- **Decided:** one module that lays out a line of text and says where a paragraph may break: shaping with HarfBuzz, the
  order of right-to-left and mixed text with FriBidi, cluster-aware cuts, and line breaks after spaces and hyphens and between
  ideographs. It takes FreeType faces and gives back glyphs and positions in em, so a tool draws them with its own library
  (paclock: Cairo). The choice of fonts stays with the tool (a callback), as the table of script fonts is the tool's.
  With `-DNO_I18N` (the tool's `I18N=0`) the same functions need neither library and draw left to right. The `pacommon`
  Makefile builds and tests both flavours where HarfBuzz and FriBidi are installed (`tests/text_test.c`: 4,589 checks with
  them, 4,543 plain, under the sanitizers too, with a fuzz of random bytes).
- **Why:** the principles already expected HarfBuzz and FriBidi in translated builds (principle 2, `apis.md`), and pacpeek and
  pacreact had their own; Arabic, Hebrew, Indic and Thai text cannot be drawn without them, and a language the person reads
  is more than the usual few. In `pacommon` every tool can have it.
- **Considered:** Pango (rejected: heavy, and it draws itself); libunibreak or ICU for the line breaks (rejected: a new
  dependency for what spaces, hyphens and ideographs cover; Thai and the like fall back to a cut after the last letter that
  fits); moving pacpeek's `shape.c` (it does one font and no fallback; paclock needs a font for each script and marks that
  keep the face of their letter).
- **A language's direction beats the first letter:** a message in Arabic, Hebrew or Persian that starts with a Latin word
  ("Caps Lock") was laid out left to right, with its colon on the wrong side; the tool passes the direction of its language
  (`pac_text_language_rtl`). `PAC_TEXT_AUTO` stays for text of unknown language.
- **Cost:** about 8 kB of binary in paclock, and the two libraries when built with I18N (HarfBuzz is already mapped by FreeType).
- **When:** 2026-11-16

## 2026-11-16: comparisons are for reference

- **Decided:** a comparison with another tool (binary size, memory, CPU) is written as a reference and not as a goal, and principle 57 now says so. A saving is taken only where it costs a person nothing they need, and its cost is written next to it.
- **Why:** the author: apps for people should be simple, but not extremely minimalist, so that they become inconvenient for the sake of machine performance.

## 2026-11-16: privacy prevails over resources

- **Decided:** when `privacy` and `resources` disagree, privacy wins: a tool does what the privacy level promises even if that takes more memory, another process or more time than `resources` allows (`docs/configuration.md` rule 5, principle 58). In paclock, `privacy = medium` runs `busctl` and `pactl` at `resources = minimal` too.
- **Why:** the author: a person who sets a higher privacy level does it because it matters, and it could be a company policy. A level that quietly gives way to a memory saving would break that promise without saying so.
- **Considered:** making `resources = minimal` skip the privacy work that costs memory (rejected, for that reason); a warning when the two disagree (not done: the combination is allowed and the tool's README says what it costs).

## 2026-11-16: shared text size limits, and the `resources` and `privacy` levels

- **Decided:** `text_size_min`, `text_size_max`, `resources` (`normal`, `low`, `minimal`) and
  `privacy` (`normal`, `medium`, `high`, `paranoid`: four, as the author listed what each does) are shared keys in `pac.conf`, validated in
  `config_file.c`; `low_memory` stays as the old name of `resources = low`. The rules for what
  wins (accessibility settings are never changed by a level, the limits beat base sizes, absurd
  limits are pulled to 2 and 40 mm, a card may stay on the screen) are in
  `docs/configuration.md` and principle 58. Each tool decides what its levels do.
- **Why:** someone who needs larger text, speech or high contrast should say it once for every
  tool, and a tool that saves memory or hides information must not undo it. Levels instead of a
  yes/no so that one setting can mean "a little less" and "as little as possible".
- **Considered:** one number per level (rejected: three words are readable in a file); letting
  `minimal` override the size limits (rejected: accessibility before memory); a floor and ceiling
  on the final size, not only on the limits (rejected: it would also override a tool's own base
  size on a small screen, where the fit rule is the right limit).
- **Cost, and a mistake caught:** `config_file.c` checks the sizes with `pac_length_parse`. It first lived in `length.c`,
  and pacgreet, pacpeek and pacreact (which compile `config_file.c` and not `length.c`) failed to link against it. It is now
  `static inline` in `length.h`, so no existing tool needs a new file; `length.c` is only for the conversion.

## 2026-11-16: lengths with a unit (`length.[ch]`)

- **Decided:** one shared parser and converter for sizes written as `12px`, `3mm`, `0.2in`, `10pt` or `5%`,
  and for turning a `wl_output`'s physical size into pixels. The unit is required. A physical size
  that is 0 or makes under 50 or over 600 dpi is replaced by 96 dpi.
- **Why:** a text size limit has to mean the same size on every screen, which only a physical unit
  does, and more than one tool will want it. Virtual machines, projectors and some TVs report 0 or
  made-up millimetres, so the number cannot be trusted blindly. A bare number was refused because
  it cannot say which unit it is.
- **Considered:** `pt` is there because it is the unit of fonts. Exponents and hexadecimal are
  refused so `1e1mm` and `0x10px` are not read. The conversion does not listen to `wl_output`
  itself: it takes the numbers, so it needs no display and is tested without one.

## Open

- **Speech through a program.** By principle 3, speech could be `spd-say` (or `speechd`'s own
  client) run as a command instead of linking libspeechd: no library, and the same
  `speech = no` and `SPEECH=none`. It costs a process per message and gives no way to interrupt
  or to know when speech ends, which a library does. `SPEECH=speechd` (the library, accepted
  above) stays the default until someone tries the command and finds it good enough. `atspi`
  stays a library: there is no command for it.
- **Automatic README checks.** Today an AI checks the README against the code and docs as a step
  of each change (`development.md`). Enforcing it with a hook or CI is not set up.

## 2026-11-15, eighth round: the tools checked against the principles

- **`contrast` is `normal` or `high` in `pac.conf` too** (principles 19 and 21). Every tool and
  every tool's example already used `normal`/`high`, but `pacommon`'s check for the shared file
  accepted only `yes`/`no`, so `contrast = high` in `pac.conf` was reported and skipped. The check,
  `pac.conf`, `docs/configuration.md` and `docs/accessibility.md` now say `normal`/`high`; `yes` and
  `no` are refused because no tool accepts them. Considered: changing the five tools to `yes`/`no`,
  which is more work and would break every existing file. `pac.conf` also offered `icons = Adwaita`,
  which no tool accepts; it now says `auto`, the value every tool takes.
- **pacland reads `pac.conf`** (principle 19). `config_load` calls `pac_config_load_shared` before
  its own files, with an adapter to `apply_setting`; a shared key pacland does not have (`theme`,
  `speech`, `low_memory`, `braille_table`) is ignored silently. `--config FILE` still reads only
  that file.
- **The example `.conf` files are commented out at their defaults** (principle 23). pacgreet,
  paclock and pacreact had their default lines active, so the installed file was not "the example
  with every line commented out" (`docs/development.md`). They are now commented, and the values
  are the defaults the code already had.
- **`make` builds translations** (principles 38 and 49). pacgreet's `all` built only the binary;
  the catalogs were built by `make test` and `make install`. It now builds `$(MOS)` too, like the
  other tools.
- **A missing `pacommon` says so** (principle 49). Each tool's Makefile checks for
  `$(PACOMMON)/pac.mk` before including it and stops with a message that says to clone it next to
  the tool or set `PACOMMON=`.
- **pacland's link line adds `--as-needed` itself** (principle 5), so a user's `LDFLAGS` cannot
  drop it.
- **pacland refuses a configuration file with a NUL byte** (principle 41). Its own parser reads the
  file into one block and every string function stops at the first NUL, so the rest was silently
  ignored; the file is now refused whole, like `pacommon`'s parser, with a warning. A test writes
  one and checks that the old configuration is kept.
- **`pac.conf` errors go through gettext in every tool** (principle 38). pacgreet, paclock, pacreact
  and pacland now compile `config_file.c` with `-DPACOMMON_I18N='"i18n.h"'`, and `make pot` extracts
  its strings too; pacpeek already did both. The catalogs have the new entries empty until a
  translator fills them, so they read in English until then.
- **pacpeek checks `late` where it is read** (principle 20). `system_late_window_valid` shares the
  parser with `system_late_window`, so a bad window is reported with its file and line and skipped,
  instead of a message without a location at start-up.
- **pacreact checks numeric settings where they are read** (principle 20). A table in
  `settings_file.c` holds the range of each integer setting and of `scale`, so a bad value is
  reported with the file and line and skipped; the same place checks the fixed-value settings
  (`anchor`, `osd`, `icons`, `contrast`, `print`, `jacks`, `sound`, `click`), `probe_host` and the
  `usb` kind list. `int_value()` still clamps a value set another way.
- **`icons = theme` works in pacgreet** (principle 21). pacgreet has no icon theme, so `theme` means
  words only, the same as `0`; one `pac.conf` now works in every tool.
- **pacbar's example `.conf` is commented out at its defaults** (principle 23). Its `height` was
  active at 42 while the code's default is 32, so installing it changed the bar; it is now
  `#height = 32`, with the other four default lines commented too.
- **`low_memory`, `speech` and `braille_table` are documented as ignored** by a tool that does not
  have the feature (principles 21, 25 and 57). The build options are what remove the optional
  features today; no tool has a runtime `low_memory` trade-off yet.

## 2026-11-15, seventh round: convenience before smallness

- **Convenience is the default when it conflicts with smallness** (principle 57, in "Small and
  fast"). A smaller buffer, a shorter list, a missing feature or a faster path is not worth making
  the tool harder to use or making the person do work the tool could do. `low_memory = yes`, or a
  build option that does the same, turns the convenience off for someone who wants the smallest
  memory or the most speed.
- **Convenience is not an excuse for incorrectness.** A missing check, a skipped event or an error
  that is not reported stays a bug.
- **The summary now says "1 to 9 and 57"** for the "Small and fast" group. This is the first use of
  the stable-number rule: the new principle takes the next free number and sits in its section.

## 2026-11-15, sixth round: the principles for a library

- **"One job" is now "one coherent job", for a tool or a library** (principle 1). A GUI library's
  job is broad by nature; the rule is coherence, not a single feature.
- **Static linking is a rule about `pacommon` only** (principle 5). It no longer reads as a general
  rule for every library; a library that other programs link may be shared. The symbol table is no
  longer required to be stripped.
- **A GUI library may use the configuration for user preferences** (principle 19): speech, text
  scale, palette, icon sources. The application owns the file and passes the values.
- **A GUI library may provide the accessibility layer** (principles 36 and 37): accessible roles,
  names and states, speech and braille. The tool configures it.
- **Theming stays a simple `.conf` file** (principles 17 and 22): a list of values, not a style
  sheet or a CSS-like language.
- **A fixed block is the preferred allocation** (principle 6), and the heap is used when it is
  necessary or better; "allocate once" is the default, not a law.
- **A library's public API and ABI are a dependency for its users** (principles 4 and 53), so they
  are chosen and documented like one.

## 2026-11-15, fifth round: the principles checked against the code

- **Principle 3 no longer says a helper gets "only file paths".** pacreact runs `rsvg-convert` with
  fixed options (`-a -w N -h N -f png`) and the path last (`src/sicon.c`), so the rule now says
  fixed arguments of its own and any path from outside last or after `--`.
- **Principle 19 lists the variables the tools really read:** the storage variables
  (`XDG_CACHE_HOME`, `XDG_STATE_HOME`), the session and service ones (`DISPLAY`,
  `XKB_DEFAULT_LAYOUT`, `GREETD_SOCK`, `SWAYSOCK`, `PULSE_SERVER`, `CUPS_SERVER`) and `PATH` for a
  helper. Before, it named only the config, data and runtime ones, which principle 31 already
  contradicted.
- **Principle 41's overflow example now checks `len <= MAX` first.** `MAX - len` wraps when `len` is
  larger, so the old example could pass a length that does not fit.
- **Principle 2 no longer offers SDL3 for windowing.** Windowing stays on the Wayland protocols
  (principle 16); SDL3 is for audio and input where it is smaller.
- **Principles 41 and 44 have a check.** They were the security rule and the one that never bends,
  and neither said how it was checked. The sentence at the top now says that where a principle does
  not say, the review in `AGENTS.md` is the check.
- **The summary's ranges are a snapshot.** The numbers are stable and new principles are placed by
  subject, so a section can later hold non-consecutive numbers.
- **`docs/apis.md` gains PAM and libdbus**, which paclock and pacgreet link and the file did not
  list (principle 53).

## 2026-11-15, fourth round: setting the principles (accepted by the author)

Done before the first push, because other repositories will clone this one and refer to the
principles by number.

- **The numbers are stable from now on** (stated at the top of `PRINCIPLES.md`). A new principle
  takes the next free number and sits in its section; a dropped one leaves its number unused. This
  was the last renumbering: it had been done several times while the file grew, and every
  cross-reference had to be rewritten each time. Found while doing it: a reference written as
  "principles 6 and 11" is not caught by a "6 to 11" pattern, so such a list must be checked by hand.
- **A summary at the top** of `PRINCIPLES.md`: seven lines, one per group, with the numbers.
- **Data first, read in order** (principle 7): a program is designed from its data (what, how much,
  laid out how), and the code is the transformations of it: arrays and rings read in order, not
  linked lists and trees of separate allocations; hot data apart from cold. For data that is large
  or walked often, and measured (principle 9); for a dozen items clarity wins. Examples already in the code: pacreact's history ring and
  `pacommon`'s settings block (which was a linked list).
- **Show only what is needed** (28) and **Confirm what is not obvious** (29): the interface is in
  layers, and an action whose effect cannot be seen (pacland's configuration reload) is confirmed
  in one short, translated, speakable line, including when it failed.
- **Private files** (42): files a tool creates in the person's directory are 0600, folders 0700,
  set explicitly, never through a symbolic link, written to a temporary file and renamed.
- **Plain build** (49) now says that `pacommon` is cloned next to a tool.

Open, found against the new principles and not changed:

- No audit of the tools for linked lists and pointer chasing in hot loops (principle 7).

## 2026-11-15, third round (accepted by the author)

- **`make install` works out of the box and never overwrites silently** (principle 49). The tools
  each had their own way (pacland and paclock kept existing files, the others did not install
  configuration at all). `pac.mk` now gives all of them one: install what does not exist, leave an
  identical file alone, keep a different one with a warning, and replace it only with
  `REPLACE=1`, saving the old one as `NAME.bak.DATE` (a timestamp, so that a second replace does
  not overwrite the first backup). Programs, man pages and translations are always replaced:
  they are not the person's. Assets the person may have edited (configuration, PAM file, session
  file, service files) use the keep rule. Not installed automatically: greetd's `config.toml`,
  because a wrong login configuration can lock a person out, so it stays an example. Considered:
  always overwriting, which loses edits; never installing configuration, which means the tool
  does not start configured.
- **`pac.conf`: shared settings, read first** (principle 19). The keys are the ones that mean the
  same everywhere (`contrast`, `text_scale`, `font`, `language`, `icons`, `theme`, `speech`,
  `low_memory`, `braille_table`). All the `pac.conf` files are read before any `<tool>.conf`, so
  a tool's own file beats it. Considered: reading `pac.conf` and `<tool>.conf` per location, so
  that a user's `pac.conf` would beat the system's `<tool>.conf`; rejected as harder to explain.
  A tool that does not have a shared setting ignores it silently, or `speech` in `pac.conf` would
  warn in every tool that cannot speak. The cost: a typo in a value that only a tool can judge is
  silent in `pac.conf`; the value ranges that are the same everywhere (`text_scale` 0.5 to 2.5,
  `contrast` normal/high, yes/no) are checked in `pacommon`. Wired in: pacbar, pacgreet, paclock,
  pacpeek (through `pac_config_load`) and pacreact, and pacland through an adapter to its own
  parser (`config_parse_text`).
- **`pacommon` is linked statically, from source, always.** It already was; this records why a
  `libpacommon.a` was not made: `config_file.c` includes each tool's own `i18n.h`, so one prebuilt
  archive could not serve a translated and an untranslated tool.
- **Build flags** (principle 5): the tools were mostly there (`-Os`, `-ffunction-sections`,
  `--gc-sections`, `-s`). pacpeek was not: it used `-O2` with no section flags and no stripping;
  now it matches, and went from 111,792 to 76,240 bytes (-32%). The only CMake project, pacgui,
  had no build type (so no optimization) and no section flags; it is now `MinSizeRel` with
  `--gc-sections`, stripped (68,104 bytes for its demo). The other tools use Makefiles, not CMake.
- **Warnings fixed, not suppressed.** pacgreet, paclock and pacreact had `-Wno-format-truncation`.
  It is removed and what it hid is fixed: cut text now says it is cut (`memcpy` with a bound, or
  the return value of `snprintf` used). `-Wshadow -Wvla -Wformat-security` were added to every
  Makefile; the six tools build with no warnings. Real bugs found by this, in pacreact's client:
  `n += snprintf(msg + n, sizeof msg - n, ...)` let `n` pass the end of the buffer when one
  argument was longer than 1023 bytes, so `sizeof msg - n` wrapped to a huge size (a write past
  the buffer); fixed with a clamp. And two variable-length arrays sized by `argc`, now a fixed array
  of `CLIENT_ARGV_MAX` with a check. `-Wformat=2` was not adopted for the tools: its
  `-Wformat-nonliteral` fires on every gettext message.
- **`AGENTS.md`** in `pacommon` and in each tool: how an AI agent works here, and the four
  questions of an AI review (simplicity, quality, security, bugs).
- **`CONTRIBUTING.md` and "Open and transparent"** (principle 56): contributions, forks and
  alternatives are welcome; a person who prefers not to use AI can still use the apps as a
  reference and write their own. pacland's `feature-spec.md` is the model for a specification that
  lets someone implement a tool from scratch.

## 2026-11-15, second round (accepted by the author)

- **Libraries that earn their place; compare before choosing** (principles 2 and 4). The old rule
  was "no toolkit". The new one allows any library that is lean and fits, even an unusual one,
  and asks for a comparison on **RAM** (resident size of the idle tool), **CPU** (time, wakeups)
  and **UX**, against the alternatives, which include writing it ourselves and running a program
  (principle 3). The comparison is a table here. Template:

  | Option | RAM (idle, resident) | CPU (work, wakeups) | UX | Verdict |
  | --- | --- | --- | --- | --- |
  | (the candidate) | measured, or "not measured" | | | |
  | (the same thing written by hand) | | | | |
  | (a program that is run) | | | | |

  Candidates so far, none of them used and none compared yet:
  - **Clay** (flexbox-style layout, a single header, installed here as 0.14). It does not allocate:
    the caller gives it one arena, whose size is asked of `Clay_MinMemorySize()`, which is the
    shape principle 6 wants. Measured with a small program on this machine: at its default limits
    the arena is **5.85 MB**; with `Clay_SetMaxElementCount(256)` it is **0.19 MB** (1024 elements:
    0.74 MB). Resident memory grew by 140 to 232 kB after `Clay_Initialize` in each case (one run
    each, so rough: most of the arena is not touched until used). So a tool that uses it sets the
    element count to what its screen needs, and writes the number in its `tunables.h`.
  - **SDL3** (windowing, input and audio; installed here as 3.4.18, a 3.3 MB `libSDL3.so`). Worth
    comparing for a tool that needs more than a layer-shell surface, and attractive because other
    programs, games above all, load it too. Idle resident size and wakeups are not measured.
- **Settings are one fixed block, not allocations** (principle 6). Measured with 32 and 66
  settings of typical size: before, 96 and 198 allocations, using 3,072 and 6,336 bytes of heap;
  now 0 allocations and 0 heap, in a 4,096-byte static block (`PAC_SETTING_POOL`). The largest
  example `.conf` of any tool (pacland, 66 options, every one set) needs about 2,800 bytes, so
  4 KiB holds it. Untouched static memory is not resident, so a tool with 10 settings pays one
  page. The cost: the pointer from `pac_setting` is valid only until the next change to *any*
  setting (before, until that key changed). pacreact and pacpeek only set while reading the file,
  at start-up, and read afterwards, so they are not affected.
- **`tunables.h`** (principle 12): every size, limit and default in `pacommon` is in one header
  with `#ifndef` around each, so `make CFLAGS="-O2 -DFIELD_MAX=1024"` changes it. `_Static_assert`s
  refuse values that cannot work. The size of a `Field` depends on `FIELD_MAX`, so every file of a
  tool must be built with the same value, which the Makefile does by using one `CFLAGS`.
- **Themes are `.conf` files** (principle 22). Not written yet: `pacommon` will get a loader that
  looks in the three folders of the principle and reads the file with `config_file`. A theme name
  is a file name and must not contain a `/`.
- **`docs/apis.md`** is the one file that names the documentation, the tested version and the
  place that sets the version of every API (principle 53), with wlroots first because it has no
  stable API. pacland already sets its version in one place (`WLROOTS` in its `Makefile`).
- **Remembered choices** (principle 31). pacgreet keeps the last user, session and layout in
  `$XDG_CACHE_HOME/pacgreet/state` (else `~/.cache/pacgreet/state`), mode 0600, opened with
  `O_NOFOLLOW`. The XDG specification has `$XDG_STATE_HOME` for "data that should persist but is
  not important enough for configuration"; cache is meant for what may be deleted. Losing this file
  costs only a few extra steps, so cache is acceptable, and a cleaner that empties it is harmless.
  Open: whether to move to `$XDG_STATE_HOME` so that it lasts.
- **Rules serve people** (principle 54). The principles are defaults. A deliberate exception is
  written here with its reason; the one that is not traded away is respecting the person
  (principle 44).

## 2026-11-15 (accepted by the author)

- **Guards at every entry point that takes outside input** (principle 41). Found by reading the
  code and trying hostile input; both were real bugs, fixed with tests:
  - A NUL byte in a settings line made the code think the line was too long and skip the *next*
    line too (`strchr` stops at the NUL, so the end of the line was not seen).
  - A pipe named `<tool>.conf` in the current folder made the tool hang at start-up, and a
    device such as `/dev/zero` would be read forever.

  Decisions: the file reader is `getc`-based so that it sees every byte; a settings file is a
  regular file of at most 64 KiB (a real one is a few hundred bytes), opened non-blocking and
  checked with `fstat` before it is read; reported text has control characters replaced by `?`;
  `pac_config_path` refuses a tool name with a `/` in it, since it names a file; the setting store
  has limits (63-byte keys, 1023-byte values, and later a fixed block) so that a hostile file
  cannot make it grow, which it could before (up to the size of the file, in 1023-byte values); `Field` checks
  its own invariants and wipes itself if they are broken; relative `XDG_*` directories are
  ignored, as the XDG specification says they must be. Considered: making a settings file that
  is too large be read up to the limit instead of refused; rejected because a cut file may end in
  the middle of a line and change what a setting means. The limits are named constants in the
  headers (`PAC_CONFIG_MAX_SIZE` can be overridden at build time).

- **Stack and static storage before the heap** (principle 6). Model, read in pacreact
  (`src/history.c`, `docs/design.md`): the notification history is a static array of 64 `Entry`
  structs with fixed text fields (app 64, summary 256, body 512 bytes, about 60 KB in all). No
  `malloc`. A new notification takes the slot after the newest, so the oldest is overwritten and
  memory never grows. A replaced card updates its open entry in place. Text longer than a field
  is cut by `snprintf`, and `history_clear` wipes the whole array with `explicit_bzero`. Output is
  built in fixed buffers on the stack (`line[512]`, and `flatten` into buffers sized `max + 4`
  for the "…"). Static rather than stack because the history must outlive every call, and 60 KB
  is too much for a stack. The limits are in the code and in `docs/usage.md`, so a user knows how
  much is kept. Where the heap is used today:
  - `pacommon/setting.c`: was a linked list of `strdup`ed keys and values; now one static block
    (see the second round below), so it no longer allocates.
  - `pacpeek/src/query.c`: `make_env` allocates a copy of the environment, whose size is not
    known (it is freed right after the programs are started).
  - `pacpeek/src/display.c` (`calloc` of the display) and `main.c` (`strdup` of the monitor
    name): not looked at yet.

- **Delegate to a command-line program rather than link a library or write the feature**
  (principle 3). Seen in pacreact, checked in its source and docs: SVG icons are drawn by running
  `rsvg-convert` (`src/sicon.c`, `posix_spawn`, no shell, absolute path only, 1.5 s deadline, 256 KiB
  output cap, PNG output decoded by the ordinary PNG loader); librsvg was left out because it
  pulled in GLib, libxml2, ICU, glycin and libstdc++ and more than doubled the resident size of
  the daemon for a few small icons, each drawn once and cached. Sounds are played by starting
  `pw-play` or `paplay` (`src/sound.c`, `fork` and `execlp`, the player named by `player`), so no
  audio library is linked. Cost: a process per use (about 10 to 15 ms for an icon), and a missing
  program means a missing feature, which is said once. pacpeek does the same with `wpctl`,
  `pacreact count`, `makoctl` and `dunstctl`: it used to run them from a script through `/bin/sh`,
  and since 2026-11-15 starts each directly (`src/query.c`, `posix_spawnp`, fixed arguments, no
  shell), side by side, with one epoll descriptor for the caller to poll. The notification
  programs are still tried in order, the next one when a program is missing or exits with an
  error. Its arguments were fixed, so the shell was never an injection risk there; it was
  dropped for the extra `sh` process and so that the rule has no exceptions.
- **`SPEECH=speechd` is the default build; `BRAILLE=0`.** Why: principle 34 says accessible by
  default, so a person who builds from source and needs speech has it, while principle 10 keeps
  the choice explicit: `SPEECH=none` compiles it out and removes the dependency. libspeechd is
  the smaller library, and `speech = no` in the configuration costs nothing at runtime (nothing
  is loaded or connected). Braille is only for tools that print, so it is off unless asked for.
  Considered: `SPEECH=none` by default, which leaves a blind person who builds from source with
  no speech; and `atspi` by default, which is more code and memory (see `accessibility.md`).
- **Key names `speech`, `low_memory` and `braille_table`** are final, with the values in
  `docs/configuration.md`.
- **The color palette lives in `pacommon`** (a `palette.[ch]`, with the contrast and color-blindness
  test of principle 33) so that a tool can match the others. pacgreet and paclock use it; pacpeek,
  pacreact and pacland still define their own colors. **A common `speak(text)` interface will
  too**, so that a tool does not depend on the backend; it is not written yet.

## 2026-11-15 (recorded from the code)

- **Shared code is compiled into each tool, not a shared library.** Each tool's `Makefile` builds
  the files it calls from `../pacommon`. Why: the linker keeps only what is used, there is no
  library to install or version, and every tool stays one `make`. Considered: a `libpacommon.so`;
  rejected for the extra package and the memory of a mapped library for a few functions.
- **Settings come from files only, not environment variables.** Why: one place to look, and the
  same behavior whether a tool is started from a shell, a compositor's config or a service.
  `setting.[ch]` exists so that code that used `getenv()` for its options can read them where it
  needs them. Considered: environment variables as overrides; rejected because they are invisible
  and are inherited by child processes.
- **`key = value` lines with `#` comments, not INI sections, TOML or JSON.** Why: it can be
  parsed in about forty lines, written by hand, and has no cases to get wrong. The cost: a value
  cannot contain `#`. Considered: a real format; rejected as more code than the settings need.
- **A line over 1023 bytes is skipped whole.** Why: reading its tail as a new line would turn
  part of a long value into a setting. Considered: truncating; rejected for the same reason.
- **`field.c` uses `explicit_bzero` and has no fallback.** Why: a password must be wiped, and
  `memset` may be removed by the compiler. Cost: it needs glibc 2.25 or musl 1.1.20.
- **`Field` is a fixed array of `FIELD_MAX` bytes.** Why: no allocation to fail and no copy of a
  password left on the heap; a password longer than 511 bytes is refused. Considered: a growing
  buffer; rejected.
- **`font_find` uses `nftw` and static state.** Why: `nftw` follows links and does not loop; it
  takes no user data, so the state is static, which makes it not thread-safe. No tool searches
  from two threads.
- **`setting.c` is a linear scan over packed records.** Why: a tool has a few dozen settings, so
  lookup cost does not matter and a hash table would be more code. It was a linked list of
  allocations until the second round below.
- **The Makefile adds hardening flags to `CFLAGS` instead of relying on them.** Why: a user who
  overrides `CFLAGS` for optimization must not lose them silently. CET is probed with the
  compiler because `uname -m` describes the build host, not the target.

## 2026-11-16: one yes/no parser (`pac_config_bool`)

- **Decided:** `pac_config_bool` in `config_file.c` reads yes, true, on, 1, no, false, off and 0 in any case; the shared-key check uses it (it was a validator only) and so does a tool's own yes/no settings (paclock's `parse_bool` is gone).
- **Why:** the shared check was case-insensitive and paclock's own parser was not, so `speech = Yes` in `pac.conf` passed the check and was refused by the tool. One function, one rule.
- **When:** 2026-11-16

## 2026-11-16: `low_memory` removed

- **The `low_memory` key is gone** from `pac.conf`, `config_file.c`, the tests and the docs: it was
  only the older name of `resources = low`. A `low_memory` line is now an unknown key, as in any
  other file. The entries above that mention it are history.

## 2026-11-17: the shared files, one at a time or without the current folder

- **Decided:** `pac_config_load_shared` is split: `pac_config_load_shared_file(prog, path, ...)` reads one `pac.conf` with the shared-key checks, and `pac_config_load_shared_locations(prog, count, ...)` reads the first `count` of the usual places; `pac_config_load_shared` is all of them. `PAC_CONFIG_LOCATIONS_NO_CWD` (2) leaves out the current folder.
- **Why:** pacgreet runs as the greeter user before anybody has logged in and runs commands from its own settings, so it must not read files from its working folder. Its `pac.conf` was the one place left that did. The other tools' behavior is unchanged.
- **When:** 2026-11-17
