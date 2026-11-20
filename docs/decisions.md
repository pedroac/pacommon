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

- **Speech shared between processes is left to the server.** Tools that speak at once could have
  coordinated through shared state, but speech-dispatcher already orders every client's messages
  by priority (its message priority model), screen readers included, and shared state would go
  stale when a process dies and would not cover them. So pacommon only passes a priority
  (`speech_say_as`, `SpeechAnnounce.priority`). Tried with two processes on the author's server
  (speech-dispatcher, 2026-11): a text message waits for a message being said; important ones
  queue in order; an important one cuts off text at once; a notification is dropped while
  something is said. A cue for a speech that was cut off ("...uh")
  is not pacommon's job: only the server knows that a message was cut off by a higher priority,
  and a tool that said it afterwards would be a new speech cutting in on the one that interrupted.
  It would belong in speech-dispatcher.
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

## 2026-11-18: `utf8`, `language` and `atomic_file` move here from the tools

- **Decided:** three small files that the tools had each written for themselves are added here:
  `utf8.[ch]` (`utf8_decode`, byte for byte the file pacpeek and pacreact both have),
  `language.[ch]` (`pac_set_language`, the `i18n_set_language` of pacgreet, paclock and pacpeek,
  with its limit now `PAC_LANGUAGE_MAX`), and `atomic_file.[ch]` (`atomic_write`, the temporary
  file, `fsync` and `rename` that pacgreet's `store_save` and pacmenu's `usage_save` each write;
  pacreact has similar code that was not read for this).
- **Why:** the same code in several tools drifts: the copies of `i18n_set_language` already differ
  in a comment, and the writers differ in what they check (pacgreet's `store_save` does not call
  `fsync`, pacmenu's does). One tested copy is smaller in every binary and easier to review.
- **Switched over (2026-11-18):** `utf8`: pacpeek, pacreact. `language`: pacgreet and paclock (through
  `i18n.h`, which names `i18n_set_language` as `pac_set_language`), pacpeek, pacreact.
  `atomic_write`: pacgreet, pacmenu. Not switched: pacreact's `state.c`, `alerts.c` and `timer.c`,
  which write a file their own way and were not read for this.
- **Not moved:** `i18n.[ch]` stays in each tool, because it holds the tool's gettext domain and its
  `_()` (`PACOMMON_I18N` picks it up); only the language function, which needs neither, is shared.
  `atomic_write` does not make folders and takes its data from memory, to keep it to one job; a
  tool that has more to say than fits in memory, or that needs a folder, does that part itself.
- **Names:** `pac_set_language` and `atomic_write` do not repeat a tool's `i18n_set_language` or
  `pf_write`, so a tool can switch over one call at a time.
- **When:** 2026-11-18

## 2026-11-18: pacommon is the common source of truth, and one set of editing keys

- **Decided:** `pacommon` is where a rule that two tools share is written once, so that the tools
  behave the same; it is not code that every tool must carry. A tool that does not need a file does
  not compile it, and a feature a build can turn off has a switch (`NO_I18N` for text layout). This
  replaces the reading of the rule in `docs/development.md` as "nothing that needs a library".
  What stays in a tool is what only that tool does. No Wayland code has been written here; whether
  to is answered in the 2026-11-18 entry on keyboard layouts (it is not ruled out), and the
  measurement behind it is in the next point.
- **Measured:** the Wayland and xkb code of `pacgreet`, `paclock`, `pacpeek`, `pacmenu`, `pacreact`
  and `pacbar` (comparing function bodies): about 520 lines are identical or near-identical, all of
  them between `pacgreet`, `paclock` and `pacpeek`; the other three share none. The comparison
  missed the input-field code, which is inline in larger functions, so it is a lower bound. It is
  a rough match on normalized lines, not a proof.
- **Also decided:** `field_edit_key` (`field.c`) is the one set of editing keys. It replaces the
  copies in `pacgreet` (user, password, layout filter), `paclock` (password, layout filter) and
  `pacmenu` (search), which had drifted: `pacmenu`'s search did not clear on Ctrl+U, and `paclock`'s
  layout filter cleared on neither Ctrl+Backspace nor Ctrl+U.
- **What changed for a person:** Ctrl+U clears `pacmenu`'s search; Ctrl+Backspace and Ctrl+U clear
  `paclock`'s layout filter. Backspace, Ctrl+Backspace or Ctrl+U on an *empty* field no longer
  counts as an edit: in `pacgreet` and `paclock` it no longer clears an error message, and in
  `pacmenu` it no longer resets the selection (it would have). Text a tool limits to less than
  `FIELD_MAX` (`pacmenu`'s `QUERY_MAX`) is passed as `max`.
- **Why `held` and not xkbcommon's types:** the function takes the keysym as a number and the held
  modifiers as `FIELD_CTRL`, `FIELD_ALT` and `FIELD_LOGO`, so `field.c` stays libc only.
- **When:** 2026-11-18

## 2026-11-18: keyboard layouts are here, and why the pac tools are what they are

- **Decided:** the keyboard layout code that pacgreet and paclock each had is `layout.[ch]` and
  `layout_detect.[ch]` here: the list, `layout_parse_spec`, `layouts_load`, `LayoutPicker` and its
  functions, `pac_layout_name`, and (for pacgreet) detecting the system's layout. `MAX_LAYOUTS` and
  the list limit (now `LAYOUT_LIST_MAX`) moved to `tunables.h`.
- **Why:** paclock's `layout.c` was a trimmed copy of pacgreet's: 11 of its 12 functions were
  identical, and the twelfth, `layout_parse_spec`, had drifted. paclock's handles more (it reads
  `us (intl)` and keeps the name properly), pacgreet's does not. pacommon has paclock's version.
- **What changed for a person:** pacgreet now reads a spec with a space before the variant
  (`us (intl)`), as paclock already did, in `layouts` and `default_layout`.
- **The session selector stays in pacgreet:** only it has sessions, and what it shares with the
  layout row is drawing (`draw_choice`, in each tool's `render.c`), which needs Cairo and is not here.
- **Tests:** the picker, list, spec and detection tests moved here from the two tools, with more
  (lists and specs that do not fit, a picker with nothing to step to, files that are not `.conf`).
- **What the tools are (the author's words, as reasons for the rules here):** the pac tools are
  not the apps a person uses a computer for (a browser, a drawing program); they are the tools
  around those apps: logging in, a window manager, a way to find and start an app, a bar,
  notifications. Some are always visible, are the start and the end of a session, or are always
  watching. They would not be useful on Windows or macOS, where people are not expected to pick
  such tools. That is why they are tied to Wayland (X11 being obsolete, and most apps on Linux
  using Wayland), and why they have to be lean and fast, and **consistent with each other**, at
  least those of one desktop. A regular app can use something else and be portable; these need not.
- **So:** pacommon is where what the desktop tools do the same way is written once. Wayland-specific
  code here is not ruled out (the tools depend on Wayland anyway); it has not been written because
  the measured overlap was small (previous entry), not because of a rule.
- **When:** 2026-11-18

## 2026-11-18: accessibility here helps other tools, with care, and is always optional

- **Decided (the author's direction):** pacommon should help the tools be inclusive, which is why it
  has accessibility code (`speech`, `palette`, `field`, the layout picker). It is not to be done
  blindly: attention to detail, so that what a person is told and how it is said is convenient and
  easy to understand, the same in every tool. And it must be easy to turn off: in the build, with a
  compiler directive (`SPEECH=none`, to save size), and in the configuration (`speech = no`), so
  that each person chooses, since for most people speech is an annoyance.
- **Already so:** principles 25 and 34 to 36 say it. `SPEECH=none` links no speech library, and
  `speech = no` loads no backend (`docs/accessibility.md`, checklist 7 and 8).
- **When:** 2026-11-18

## 2026-11-18: an accessibility audit, and what is left open

- **Read, not run:** nothing was used with a screen reader, a color-blindness simulation or a real display; the findings are from the code, the docs and what could be built and measured. Checked by running: `SPEECH=none` links no libspeechd in pacmenu, pacgreet and paclock; pacreact's colors against pacommon's contrast rules.
- **Fixed:** `text_scale` is read by `pac_text_scale_parse` in pacmenu, pacpeek and pacreact (pacpeek accepted `nan`; `strtod` is locale dependent); `test_text_scale` here covers the parser; pacreact's colors have a test. pacland already refuses non-finite values and keeps its own table-driven parser.
- **Done:** pacmenu is translated (its own entry), except for drawing scripts that need shaping.
- **Open, for the author:** (1) speech is on by default in pacmenu, pacgreet and paclock, and the default build links libspeechd, while for most people speech is an annoyance: keep it, turn it off, or turn it on only when a screen reader runs; (2) how pacmenu gets `textlayout` (new dependencies) and whether the script font code that pacgreet and paclock each have (about 250 lines; pacreact and pacpeek have their own) becomes a shared file first; (3) more catalogs for pacmenu; (4) the spoken text of the layout picker is written twice (pacgreet's and paclock's `announce.c`); (5) **pacbar** has no keyboard path, no speech and no `text_scale` (its sizes follow `height`), and its contrast depends on the desktop in normal mode: the author postponed the decision.
- **When:** 2026-11-18

## 2026-11-18: text drawing is here (`textdraw`), and pacgreet, paclock and pacmenu use it

- **Decided:** the font and glyph drawing that pacgreet and paclock each had is `textdraw.[ch]`: loading a font file, the font of each script (the table of Noto fonts, the Chinese, Japanese and Korean faces by language), finding the font of a letter, drawing what `textlayout` shaped, measuring, fitting a line with an ellipsis, and cutting one to wrap it. Measured before moving it: the whole block (about 170 lines of code, plus `fit`, `text_width` and `show_text`) was identical in the two tools, line for line. They use it now, and pacmenu is the first new user (its own entry).
- **What differs from what the tools had:** the module no longer keeps "the font being measured" as a variable of its own: the font is the one set on the Cairo context. `pac_font_finish` takes the faces the tool made and ends everything in the safe order; `pac_font_init` may be called again. Nothing else changed in how a line is laid out or drawn.
- **Checked:** the pacgreet and paclock binaries are byte-for-byte the same size as before (default and `I18N=0`). Their own tests do not draw (`render.c` is not in them), so the shared code has tests here (`tests/textdraw_test.c`, 38 checks in both flavours, under the sanitizers): the pixels of what is drawn, Arabic taken from its own font, `pac_font_lazy` remembering a miss, fitting and cutting, a Cairo font. Neither tool was run on a compositor after the change.
- **Not moved:** pacreact's `text.c` and pacpeek's `render.c` do the same job in their own way (pacreact with its own `pick`); they were not read for this.
- **Found while testing:** Cairo's scaled font cache holds the faces, so `cairo_debug_reset_static_data` after `FT_Done_FreeType` crashes in `FT_Done_Face`, and before it asserts in Cairo. Tools exit instead; the tests do not reset the cache and suppress only the fontconfig pattern that a toy font face leaves (LeakSanitizer). Written in `docs/apis.md` and `using.md`.
- **When:** 2026-11-18

## 2026-11-18: the layout picker's words are written once (`layout_text`)

- **Decided:** "Type to search", "No keyboard matches" and the "%s keyboard" label are `layout_text.[ch]`. pacgreet and paclock had each written them in `announce.c` (spoken), `render.c` (drawn) and `main.c` (the label).
- **Why:** they had drifted: paclock said "Type to change it" when the focus entered the picker, while its own screen showed "Type to search". Everything spoken is also shown (principle 36), so the wording the screen already had is the one kept, and typing does search: it never changes the layout until it is settled. A person using a screen reader heard different guidance at the login screen and at the lock screen.
- **What is not shared:** when each tool says it. pacgreet says the hint on every visit to the picker with an empty search; paclock says it when the focus enters it. That is each tool's flow, not wording.
- **Where the file is:** apart from `layout.c`, which `LAYOUTS=0` leaves out of paclock, because the words are drawn and said either way.
- **When:** 2026-11-18

## 2026-11-18: speech stays on by default

- **Decided (the author):** `speech` defaults to `yes` in pacmenu, pacgreet, paclock and pacreact, and the default build is `SPEECH=speechd`. Nothing changed in the code: that is how they were.
- **Why:** without speech-dispatcher speech cannot work, so it is as if it were off, apart from a few resources; and if it were off by default, a blind person could be stuck, needing someone to turn it on, most of all at the login screen, where there is nothing to configure first. Most people may be surprised that a tool speaks, but they can turn it off (`speech = no`, once in `pac.conf` for every tool, or `SPEECH=none` when building). The surprise is the smaller cost.
- **Known cost:** on a machine without speech-dispatcher each start says once that it cannot reach the speech server (on stderr, translated).
- **An idea, not built:** at the start of a session, ask the person whether they want speech. It would need somewhere to remember the answer for the greeter (which runs before any settings of the person's) and a way to be reached without seeing the screen; not designed.
- **Closes:** the first open point of the 2026-11-18 accessibility audit above.
- **When:** 2026-11-18

## 2026-11-18: pacmenu has all sixteen catalogs

- **Done:** the third open point of the accessibility audit: pacmenu has the same sixteen catalogs as pacgreet and paclock (details in `pacmenu/docs/decisions.md`). Machine-written, not reviewed by a native speaker; looked at on screen in every language, tried by no person.
- **Found on the way:** in 15 of the 16 languages both pacgreet's and paclock's catalogs leave pacommon's shared config messages (`not a regular file`, `too large`, ...) untranslated; only Portuguese has them. They were translated in pacmenu's catalogs, not in the siblings'.
- **Still open:** (the layout of pacmenu is mirrored for right-to-left languages since the next entry), and the pacbar keyboard decision, postponed.
- **When:** 2026-11-18

## 2026-11-18: pacmenu mirrors its layout for right-to-left languages

- **Done:** the "layout is not mirrored" point left open above: pacmenu turns the whole menu over for a right-to-left language (`pacmenu/docs/decisions.md`, last entry). `textdraw` still draws each line in order and left aligned in its box; the mirroring is the tool's, using `pac_text_language_rtl`.
- **Not shared:** it was written in pacmenu only. pacgreet and paclock are centered cards (a field, a line), so there is little to turn over, but their right-to-left behavior (where an arrow button sits, where a typed line starts) has not been looked at here.
- **When:** 2026-11-18

## 2026-11-18: the caret is placed by the layout (`pac_text_caret`), and the three tools use it

- **Decided:** where a caret goes is worked out from the shaped line, not from the first letter of the text: `PacTextGlyph` now says which character it starts (`cluster`), the direction of its run (`level`) and the space it takes (`pen`, `advance`), and `pac_text_caret` puts the caret at the end of the character before it, which is the right side of a left-to-right run and the left side of a right-to-left one (the start of the text: the near side of the first). `textdraw` has `pac_font_caret` (pixels) and `pac_font_caret_x` (the bar of a field that is typed into). pacmenu's search box, pacgreet's username, password and layout search, and paclock's password and layout search use it.
- **Why:** the first version in pacmenu looked at the first letter with a direction (`starts_rtl`), which is right for text of one direction and wrong for a line that has both: "web" followed by Arabic in a left-to-right menu ends where the Arabic run begins, in the middle of what is drawn, which no first-letter rule can say. That function is gone.
- **One caret:** a person who types sees one caret; at a place where two runs meet it is the one of the character before it, as in most editors. A second caret for the other direction, and moving the caret by keys through mixed text, are not done (nothing here moves a caret: fields are edited at their end).
- **Checked:** `tests/text_test.c` (Latin; Hebrew, Arabic; a left-to-right line that ends in Hebrew and a right-to-left line that ends in Latin, both against the widths of their runs) and `tests/textdraw_test.c`, in both flavours and under the sanitizers; swapping the two sides fails 16 checks. In the plain flavour (`NO_I18N`) everything goes left to right and the caret is at the end of the last letter.
- **Not checked:** the dots of a password do not say which way it was typed, so their caret follows the language of the menu (the left end in a right-to-left one); nothing about a password is passed to the drawing, on purpose.
- **When:** 2026-11-18

## 2026-11-18: keyboard completeness is for the desktop, and pacbar is an exception to principle 35

- **Decided (the author):** pacbar does not take keyboard focus; everything it does can be done with the keyboard elsewhere in the desktop (the compositor's keys for workspaces and windows, the menu, the volume keys, a tray app that is planned). The reasoning, the table of each action and its key, and what is open are in `pacbar/docs/decisions.md`.
- **For the principles:** principle 35 is written for each tool; this is an exception to it, allowed by principle 54 and written down. It may be worth saying in the principle itself that a tool that is pointer-first by design must give the keyboard path of each action in its `decisions.md`; the text of the principles is the author's, and was not changed.
- **Open:** the tray (no keyboard path until its app exists), and the launcher binding the author describes (releasing super), which is not a default of pacland.
- **Closes:** the pacbar keyboard decision that was postponed.
- **When:** 2026-11-18

## 2026-11-18: the accessibility gaps are registered; the speech engine is a requirement

- **Done:** the gaps found in the audit are in a table in `docs/accessibility.md` ("Known gaps"), with their status. Decided by the author: item 8 (the tray, clock and network and volume apps) is **delayed**; **pacbar does not need speech**.
- **The speech engine:** the docs now say that speech needs speech-dispatcher *and* a speech engine with a voice (espeak-ng, Piper, Festival...) and a sound server, in `docs/platform.md`, `docs/accessibility.md`, `pac.conf` and the READMEs of the tools that speak (pacmenu, pacgreet, paclock, pacreact). Checked on the author's machine: speech-dispatcher 0.12.1 lists espeak-ng as an optional dependency (installed), and its default module there is Piper. Not done: a check at run time that an engine exists (the server accepts text either way), and a sound cue when speech cannot be reached, which would be a new default for people without speech-dispatcher.
- **When:** 2026-11-18

## 2026-11-18: input methods (text-input-v3), client side

- **Done:** `pacommon` has `textinput.[ch]` (the events of an input method, applied in the order the protocol says, with what comes from outside checked: UTF-8, no control characters, a size), `textinput_wayland.[ch]` (the protocol; built only when `wayland-protocols` has the XML), `Field` can hold a preedit, and `textdraw` has `pac_font_preedit` and `pac_font_span` (underline, cursor). pacmenu (search), pacgreet (username, layout search) and paclock (layout search) use them. They are opt-in per file like the rest.
- **Decided (by me, as the default of the work; say if not):** the surrounding text is **never sent** (the username or search may be private, and the tools do not need it), and **no input method is enabled on a password field** (the field is not told about, and `purpose` is `normal` or `name` only). Version 1 of the protocol is bound.
- **Not done / open:** pacland has no `text-input` and `input-method` relay, so on it nothing happens: the fields work as before. paclock places the input method's window by the first card when there are several screens. Not tried with a real input method (fcitx5, ibus) or on a lock surface (`ext-session-lock`): only tests of the logic and of the drawing. The question that remains is whether a compositor sends text input to a lock surface.
- **Tests:** hostile input to the events, the order, resets, sizes (pacommon); the drawing and the cursor rectangle (pacmenu, pacgreet, paclock); `utf8_plain` shared by `Field` and `textinput`. Sanitizers clean.
- **When:** 2026-11-18

## 2026-11-18: `reduce_motion`, a shared setting

- **Done:** `reduce_motion = yes|no` (default `no`) is a shared key of `pac.conf`, checked in `config_file.c` like `speech`. pacreact reads it and turns its fade off, whatever `fade_ms` says (`fade_ms = 0` was the only way before). It is an accessibility setting: `resources` and `privacy` never change it (rule 1 of the levels).
- **Why shared and not per tool:** a person who needs less motion should say it once. Any tool that gains motion reads the same key.
- **Not known:** which tools animate beyond pacreact. A text search found none in pacmenu, pacgreet, paclock, pacpeek and pacbar, and pacland was not read. Not tried by a person who needs it.
- **Still open (register, item 2):** sticky, slow and bounce keys, and a magnifier, which are the compositor's and not started.
- **When:** 2026-11-18

## 2026-11-18: the compositor gets keyboard help and a magnifier

- **Done, in pacland:** `sticky_keys`, `slow_keys`, `bounce_keys` (all off) and the `magnify in|out|off` actions with `magnifier_step` and `magnifier_max`. They are pacland's own settings, not shared keys of `pac.conf`: only a compositor can do them (they need to see every key and draw the whole screen), and the other tools get their effect without a setting. The reasons, the limits and what was tested are in `../pacland/docs/decisions.md`.
- **Register:** item 2 now lists what is left (a screen reader, an on-screen keyboard); item 5 says pacland has no animation. Nothing was used by a person who needs it, and not on a real GPU, screen or keyboard.
- **Asked of the author:** default keys for the magnifier (none are bound), and whether sticky keys should have a way to be switched on from the keyboard.
- **When:** 2026-11-18

## 2026-11-20: Wayland first, X11 programs as a build option

- **Changed:** principle 16 said "Wayland only. No X11". It now says Wayland first: the tools stay Wayland programs with no X11 backend, but a compositor (pacland) may run X11-only programs through Xwayland, chosen at build time (`XWAYLAND=1` default, `XWAYLAND=0` leaves it out, as principle 10 asks of optional features).
- **Why:** the author's aim is Wayland only, for fewer resources; but some people need X11-only programs, and a desktop where they do not start is frustrating. The default favors working programs; the option favors the lean build. Xwayland itself starts only when an X11 program connects, so a session without one pays nothing at run time.
- **Not changed:** the other tools (bars, greeters, lockers, ...) remain Wayland clients only; this is about compositors. Whoever adds X11 elsewhere must say why here.
- **Details and what was tried:** pacland's `docs/decisions.md`, same date.
- **When:** 2026-11-20

## 2026-11-20: what a client of pacreactd reads out of a line

- **Done:** `daemon_value.[ch]`: cut the attributes off a value (what follows its first TAB), read `name=word` out of them, and tell a device key (`NS.ID`, with nothing under it) from a property. pacbar and pacreact used byte-identical copies of the first two and each their own test of a device key.
- **Why shared:** the line's grammar is written once (pacreactd's `docs/protocol.md`); a change to it (a quoting rule, a new kind of key) should be one change, not one per client. The list of namespaces stays each tool's own: they show different things.
- **Cost:** two tools compile one small file; any other tool that talks to pacreactd gets the same reading for free.
- **When:** 2026-11-20
