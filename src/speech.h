#ifndef PACOMMON_SPEECH_H
#define PACOMMON_SPEECH_H

#include <stdbool.h>
#include <stddef.h>

/* Speaks what a tool already shows on the screen, for people who cannot read it. The tool says
 * what to speak: never a password, nor what is typed. Compiled with -DSPEECH_SPEECHD (what
 * pac.mk's PAC_SPEECH_CFLAGS gives for SPEECH=speechd) it talks to speech-dispatcher through
 * libspeechd; without it all of this does nothing and no speech library is needed. */

/* The locale the speech should be in: LANGUAGE if it is not NULL or empty, else the first of the
 * environment's LANGUAGE, LC_ALL, LC_MESSAGES and LANG that is set; "" if none is. */
const char *speech_locale(const char *language);

/* The language code for the speech server from a locale or a LANGUAGE-style list ("pt_BR.UTF-8",
 * "pt_BR:pt"): its first name, up to the region, as "pt-BR". Empty if there is none usable ("C",
 * "POSIX", NULL). At most size - 1 characters. */
void speech_language_code(const char *locale, char *out, size_t size);

/* Connects to the speech server as the tool called tool ("paclock"), in the language
 * speech_locale(language) gives. Returns false, and speaks nothing afterwards, if it cannot (and
 * reports it once on stderr, translated if the tool names its i18n.h as config_file.c does). Call it before anything that must not wait for the
 * server, such as locking the screen. */
bool speech_open(const char *tool, const char *language);
/* Says text now, cutting off whatever is still being said. Nothing if it is empty or speech is
 * not open. */
void speech_say(const char *text);
/* Waits until what is being said has ended, but at most timeout_ms milliseconds. Returns at once
 * if nothing is being said or speech is not open. Call it before speech_close, which would cut
 * the speech off. */
void speech_finish(int timeout_ms);
/* Closes the connection. Safe to call when it is not open. */
void speech_close(void);

#endif
