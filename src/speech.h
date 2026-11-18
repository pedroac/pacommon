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
/* How much a message matters, among everything that speaks on the system. speech-dispatcher keeps
 * this order between all its clients, other tools and screen readers too (its "message priority
 * model"): a more important message cuts off or goes ahead of a lesser one, whichever process says it.
 * SPEECH_TEXT: what the user is looking at, like the focus; it cuts off the one before it, and waits
 *   for an important or a message one that is being said.
 * SPEECH_MESSAGE: what a program tells; it never cuts off another message or important one, it
 *   waits its turn, and it cuts off text and notifications.
 * SPEECH_IMPORTANT: an error; never cut off by anything, said at once (cutting off what is being
 *   said), in order with other important ones. Keep it short and rare.
 * SPEECH_NOTIFICATION: a bonus, dropped if anything else is being said or waits. */
typedef enum { SPEECH_TEXT, SPEECH_MESSAGE, SPEECH_IMPORTANT, SPEECH_NOTIFICATION } SpeechPriority;
/* Says text with that priority and leaves the arbitration to the server: unlike speech_say it does
 * not cancel this tool's own earlier speech. Nothing if text is empty or speech is not open. */
void speech_say_as(SpeechPriority priority, const char *text);
/* Stops what is being said and forgets what speech_announce planned and had not said, without
 * saying anything. Nothing if speech is not open. */
void speech_cancel(void);
/* Whether something said is still being spoken. For a tool that wants to say more without cutting
 * it off (speech_say does). False if nothing is being said or speech is not open. */
bool speech_busy(void);
/* Waits until what was said last has been spoken, but at most timeout_ms milliseconds (the server
 * may not report the end). For a tool that is about to exit: closing may cut the speech short.
 * Returns at once if nothing is being said or speech is not open. */
void speech_finish(unsigned timeout_ms);
/* ---- announcing the focus ---- */

/* What to say when the focus moves, for a tool whose focus changes fast (typing a search, holding an
 * arrow key). Not every step is spoken: only where it stops. */
typedef struct {
    const char *intro;      /* said first, every time it is set (a greeting); NULL or "" for none */
    const char *lead;       /* what the focus is in (the number of results, the section): said only when it changed */
    const char *name;       /* what has the focus: the important part */
    const char *desc;       /* what it is: said later, if the focus stays and the name has been spoken, as SPEECH_NOTIFICATION */
    SpeechPriority priority;    /* of intro, lead and name: SPEECH_TEXT (0) if not set */
    unsigned settle_ms;     /* how long things must be still before the name is said */
    unsigned desc_ms;       /* how long after the name the description is said */
} SpeechAnnounce;
/* Plans what to say, in place of what was planned and not yet said. Said by speech_tick when
 * settle_ms have passed without another call: intro, lead (if it is not the lead said last), name;
 * then desc after desc_ms, waiting for the name to end instead of cutting it off. The same thing
 * twice is said once. Nothing happens if speech is not open. Parts are cut at PAC_SPEECH_PART_MAX. */
void speech_announce(const SpeechAnnounce *a);
/* Milliseconds until speech_tick has something to say, 0 if it is due, -1 if nothing is planned:
 * the timeout for the tool's wait for events. */
int speech_next_ms(void);
/* Says what is due. Call it after each wait. */
void speech_tick(void);
/* For tests: where speech_tick says things and what time it is (NULL for the real ones). */
void speech_test_hooks(void (*say)(SpeechPriority priority, const char *text), long long (*now_ms)(void));

/* Closes the connection. Safe to call when it is not open. */
void speech_close(void);

#endif
