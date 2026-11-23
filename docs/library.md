# pacommon as a library: static, shared, or neither

Decision of 2026-11-23. Short version: **keep compiling the files into each tool (`PACOMMON_LINK=source`,
the default); static and shared libraries exist as a choice, for the part of pacommon that does not
depend on how a tool is configured.** A shared library for all of pacommon is not a good idea.

## The three ways

`PACOMMON_LINK` is read by `pac.mk`, so a tool's `make` takes it:

```
make                          # source: the files a tool lists are compiled into it (default)
make PACOMMON_LINK=static     # links pacommon/build/libpacommon.a
make PACOMMON_LINK=shared     # links libpacommon.so (rpath to pacommon/build; `make install-lib` to install it)
make -C pacommon lib          # builds both libraries; `make -C pacommon install-lib` installs them and the headers
```

`libpacommon` has only: `atomic_file config_file daemon_value fontfind length loop palette paths setting
utf8 cmdjob`. Only the names the headers declare are exported (a version script). A tool opts in by
splitting its list: what is in the library goes in `PAC_LIB_SRCS`, the rest in `COMMON_SRCS`
(see pacreactd's Makefile, the example, and the comment in `pac.mk`).

## Why the default stays source

Measured on 2026-11-23 on pacreactd, the tool that uses the least of pacommon:

| | Binary | Start (`--version`, mean of 300) |
| --- | --- | --- |
| source | 147,312 bytes | 1.89 ms |
| static | 151,688 bytes (+3%) | not measured (same as source: no dynamic loading) |
| shared | 143,216 bytes, plus `libpacommon.so` | 2.47 ms (+0.6 ms, +30%) |

1. **The code is not the same in every tool.** The tools build it with their own flags and choices:
   `-DPACOMMON_I18N='"i18n.h"'` (each tool's own gettext domain and catalogs: the messages of
   `config_file.c`, `speech.c` and `layout_text.c` are translated by the tool), `-DNO_I18N` (`I18N=0`
   removes HarfBuzz and FriBidi), `SPEECH=none`, `app_name` (a symbol the tool defines, used by `gui_win.c`)
   and tunables such as `-DICON_CACHE=64` or `-DFIELD_MAX=...`, some of which are the size of a structure
   the tool and the code share. A prebuilt library has one answer for each; a tool that wants another
   one cannot use it, and a mismatch is a silent ABI break, not a link error. Of the 27 source files only the
   11 above are free of it, and they are the small ones.
2. **What sharing would save is small.** Counting what the tools compile from pacommon (`size`), it is
   about 0.3 MB across all fourteen tools, and 1 to 25% of each binary. Pages of a shared library are
   shared in memory only between tools that run at the same time (bar, notifications, compositor,
   wallpaper), and those are the ones that use the least of it. The big consumers (text, GUI) are the
   files that cannot be in the library (point 1).
3. **Security and startup.** `pacgreet`, `pacauth` and `paclock` run before or instead of a login. A
   library adds a dynamic dependency (a file that has to be the right one, found by the loader),
   relocations and ~0.6 ms to a start that is already fast. Static PIE with full RELRO and no
   dependencies beyond the system's is simpler to audit and to trust (principles 5 and 39).
4. **Versioning.** A shared library's ABI is a promise to every tool (principle 4 asks that a
   library's ABI be chosen and documented). Each tool has pacommon as a git submodule, at the
   commit it was tested with; with source, a change to pacommon cannot break a tool that has not
   taken it.
5. **Static is correct but pointless.** The linker already keeps only what a tool calls (`-ffunction-sections`
   with `--gc-sections`), so `libpacommon.a` saves no bytes; it is built with the library's flags, not the
   tool's (`-O2`, not `-Os`), so the binary is slightly larger. Its only use is a distribution that wants
   to build pacommon once and many tools against it.

## When each is right

- **source**: any tool, always, and the only choice for the ones that use text, GUI, speech, layout or
  translated messages from pacommon (all of them except pacreactd, and any tool that is content with the 11 files).
- **static**: a packager who wants to build pacommon once and several tools against it.
- **shared**: a distribution that ships many pac daemons and wants one copy on disk and in memory, and
  accepts that those tools take the defaults and English-only messages from `config_file.c`. The
  version of the library must then match the headers the tools were built with (`libpacommon.so.0`;
  increment the soname when a header changes).

## What would change this decision

A real, measured cost of duplicated code (not today's 0.3 MB); more tools that need only the
configuration-free files; or a C ABI promise the author is ready to keep. None holds now.

## Related: how a tool finds pacommon

Every Makefile now has the same lines: `PACOMMON ?=` the `./pacommon` submodule if there is one, else a
`pacommon` next to the tool, else an error that says what to do. `PACOMMON=path make` overrides both.
