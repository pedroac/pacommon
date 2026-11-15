# pacommon

Code shared by the pac tools, and their common [principles](PRINCIPLES.md).

Not a library to install: each tool's `Makefile` compiles the files it needs from `../pacommon`
 (change `PACOMMON` to put it elsewhere), so every tool stays one `make`.

| File | What | Used by |
| --- | --- | --- |
| `src/config_file.[ch]` | The settings file: where `<tool>.conf` is looked for and how it is read | all |
| `src/fontfind.[ch]` | Finds a font file by name in the system's font directories | pacgreet, paclock, pacpeek |
| `src/setting.[ch]` | A store for the settings read from the file | pacpeek, pacreact |
| `pac.conf` | The settings every tool shares, as an example: read before `<tool>.conf` | all |
| `pac.mk` | Install rules for the tools' Makefiles: keep what is there, `REPLACE=1` to replace | all |
| `src/tunables.h` | Every size, limit and default, changeable with `-DNAME=value` | all |
| `src/compiler.h` | `PAC_LIKELY`, `PAC_UNLIKELY`, `PAC_COLD` for GCC and Clang, for code a profile shows is hot | any |
| `src/field.[ch]` | A line of text edited at its end, remembering which key typed each character | pacgreet, paclock |
| `src/speech.[ch]` | Speaks what a tool shows, through speech-dispatcher (`SPEECH=speechd`), or nothing (`SPEECH=none`) | paclock |
| `src/palette.[ch]` | The colors every tool shares (normal and high contrast), with the checks of principle 33 | pacgreet, paclock |

## Build and test

```
make              # compile with the hardening flags the tools use
make test         # unit tests, no display needed
make sanitize     # the same under AddressSanitizer and UBSan
tests/distros.sh  # the tests on several distributions (needs podman or docker)
```

Linux only, developed on Arch Linux: [what has been tested](docs/platform.md).

## More

- [Using it in a tool](docs/using.md)
- [Configuration: the file, shared keys, build options](docs/configuration.md)
- [Accessibility checklist](docs/accessibility.md)
- [Developing: rules, AI-assisted changes](docs/development.md)
- [Decisions](docs/decisions.md)
- [AGENTS.md](AGENTS.md) for AI agents, and [CONTRIBUTING.md](CONTRIBUTING.md) for everyone
- [APIs: documentation and tested versions of every library](docs/apis.md)

## Forks and alternatives

MIT licensed: use, change and fork it. Forks and alternatives for other needs, or written from
scratch, are as welcome as contributions, and [CONTRIBUTING.md](CONTRIBUTING.md) says how. If you
would rather not rely on AI-written code, the code and its documentation are still a reference.

## AI Assistance Disclosure

AI is used on purpose, in three roles:
- **Claude** generates code (the shared code, and later the Makefile, tests and documentation);
- **Dropstone** reviews and improves it;
- **Gemini**  and **ChatGPT** investigate.

The code is meant to stay simple and readable. A human, the author, reviews all of it and tests
every feature in different environments, to ensure performance, security, and minimal overhead
 ([principles 45 to 48](PRINCIPLES.md)).

## License

MIT, see [LICENSE](LICENSE).
