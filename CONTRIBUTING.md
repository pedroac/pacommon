# Contributing

Thank you for looking. The pac tools are small, open and meant to be changed. Contributions,
forks and alternatives are all welcome.

## Contributing here

- **Small changes are best.** One fix or one feature per change, with a test, and the docs and
  README kept true. [PRINCIPLES.md](PRINCIPLES.md) says what the tools must be; if a change
  breaks a principle, say why in `docs/decisions.md`.
- **Build and test first.** `make test` and `make sanitize` pass with no warnings. Say on which
  distribution and compositor you tried it; *untested* is better said than hidden.
- **Reports are contributions.** A bug with the steps to repeat it, a keyboard layout that is
  wrong, a translation that reads badly, a program or compositor it does not work with.
- **Translations** go through gettext (`po/`). A machine translation is marked `fuzzy` until a
  native speaker has read it.
- **Be kind.** Disagree about the code, not about people.

## Using AI, or not

AI tools are used in this project, openly: Claude generates code, Dropstone reviews it, Gemini
and ChatGPT investigate, and a person reads, understands and tests every change
([principles 45 to 48](PRINCIPLES.md)). You may use AI in a contribution if the same is true of
yours, and you say so in the change. [AGENTS.md](AGENTS.md) tells an AI agent how to work here.

You do not have to. If you would rather not use or depend on AI-written code, that is a fair
choice, and the apps are still yours to learn from: the code is small and readable, the reasons
are in `docs/decisions.md`, the libraries and protocols are in `docs/apis.md`, and some tools
come with a behavior-level specification (pacland's `feature-spec.md`) written so that it can be
implemented from the document alone. Write your own from scratch, if you like, and the licence
says you can.

## Forks and alternatives

The licence is MIT: use, change, fork and redistribute, for any purpose. A fork that fits other
needs (another distribution, another compositor, a different look, a different set of features)
is not a competitor but another answer to the same need, and a good one is worth a link. If you
publish one, tell us and we will point to it. If you take something back from a fork, say where
it came from.

## What the project promises

Nothing is sent anywhere. Nothing is hidden. What is stored is minimal, local and listed.
What has been tested and what has not is written down. These are
[principles 44 and 56](PRINCIPLES.md), and they are the ones that do not bend.
