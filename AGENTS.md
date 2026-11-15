# AGENTS.md

For AI agents (and people) working in `pacommon` or in any pac tool. Short on purpose: the reasons
are in [PRINCIPLES.md](PRINCIPLES.md) and [docs/decisions.md](docs/decisions.md).

## What this is

`pacommon` is the code shared by the pac tools (pacbar, pacgreet, paclock, pacland, pacpeek,
pacreact, ...), and their common principles. It is not installed: each tool compiles the files it
uses from `../pacommon`, statically, into its own binary. Linux only, C, GCC or Clang.

```
make              # compile with the hardening flags and warnings the tools use
make test         # unit tests, no display needed
make sanitize     # the same under AddressSanitizer and UBSan (also finds leaks)
tests/distros.sh  # the tests in containers of other distributions (podman or docker)
```

A change is not done until `make test` and `make sanitize` pass **with no warnings**, and the same
holds for every tool that uses the file you changed (`make -C ../pacreact test`, and so on).

## Read before you write

1. [PRINCIPLES.md](PRINCIPLES.md): what the tools must be. Each principle says how it is checked.
2. [docs/apis.md](docs/apis.md): **before writing any call to a library or protocol**, read the
   documentation it points to and the headers of the *installed* version. Do not write an API from
   memory. wlroots has no stable API. Update the file when an API or a version changes.
3. [docs/decisions.md](docs/decisions.md): before changing something that was decided, read why. If
   you change it, add an entry. If a rule must be broken, say so there, with the reason.
4. [docs/development.md](docs/development.md): guards, performance hints, how to add a file.

## How to work

- **Simple and readable first.** No cleverness, no abstraction for one use, no option nobody asked
  for. Match the code around yours: naming, comment density, idiom. Smaller is better.
- **Do what was asked, no more.** A fix does not refactor its neighbours. If you see another
  problem, say so; do not fix it unasked.
- **Data first.** Decide what the data is, how big it can be and how it is laid out before writing
  the functions; arrays of plain structs read in order, not linked structures (principle 7).
- **Fixed sizes before the heap**, and one allocation when the heap is needed. Every limit is a
  named constant in `tunables.h` (principles 6 and 12).
- **Guard everything that comes from outside:** files, arguments, the environment, other programs'
  output. Check sizes by subtraction, check pointers, refuse what does not fit, never trust a
  length (principle 41). Compile without warnings; do not switch warnings off, fix the cause.
- **A program before a library, a library before writing it** when the work is occasional and the
  output small (principle 3). A new dependency needs the comparison of principle 4 first.
- **No telemetry, no network calls, no hidden behavior.** Never store a secret. Say what a tool
  stores (principles 31 and 44).
- **Every change has a test, and every bug a test that fails without the fix.** Feed parsers
  hostile input. Do not weaken or delete a test to make it pass.
- **Keep the docs true.** When a change touches an option, a command, a file or a dependency,
  update the README, the example `.conf`, `docs/` and `decisions.md` in the same change. The README
  stays lean: link to `docs/` instead of growing.
- **Do not commit, push, tag or publish unless asked.** Do not touch files outside the project.
- **Ask when the decision is the author's:** a new dependency, a user-visible default, a behavior
  that breaks a principle, anything that sends data anywhere.

## Be honest

Say what you ran and what you did not. If a test fails, show it. If you could not test on another
distribution or compositor, say that: *untested* is not *supported* (principle 14). Do not claim a
review, a measurement or a test that did not happen. Say when something is a guess.

## Reviewing (what an AI review is asked)

A review is read-only. Report only what you are confident is a problem, ranked, each with the file
and line, the input or state that makes it fail, and a suggested fix. The four questions:

1. **Simplicity:** can a person read and explain this? Is anything bigger than the job, clever,
   speculative or duplicated? Does it read like the code around it?
2. **Quality:** are names, errors and failure paths handled the way the rest of the code does it?
   Is there a test for each path, including the failing ones? Does it build without warnings?
3. **Security:** is every input from outside bounded and checked (principle 41)? Overflows (length
   arithmetic that can wrap), unterminated strings, NUL bytes, symbolic links, files that are not
   regular files, shell use, secrets left in memory or in files, privileges kept longer than needed.
4. **Bugs:** off-by-one, leaks, use after free, uninitialized data, wrong error handling, races,
   behavior that differs between glibc and musl, GCC and Clang.

A human reads the change as well. An AI review adds to that and never replaces it (principle 47).

## Layout

```
src/              the source, all of it in this folder:
  config_file.[ch]  the settings file and pac.conf      setting.[ch]    the settings store
  field.[ch]        a line of text (passwords)          fontfind.[ch]   finds a font file
  palette.[ch]      the shared colors (normal, high)    compiler.h      GCC/Clang hints
  tunables.h        sizes, limits, defaults
pac.mk            install rules for the tools' Makefiles   pac.conf     the shared settings, example
tests/test.c      unit tests                            tests/distros.sh  tests on other distributions
docs/             using, configuration, accessibility, platform, apis, decisions, development
```

## In a tool

Each pac tool has its own `AGENTS.md` that points here. Everything above applies, plus what is in
that tool's README and `docs/`.
