#include "textinput.h"
#include "utf8.h"
#include <string.h>

void pac_textinput_pending_reset(PacTextInputPending *p) {
    memset(p, 0, sizeof(*p));
    p->begin = p->end = -1;
}

/* Valid UTF-8, no control character, and it fits: copied to out. Otherwise out is empty and false is returned. */
static bool take(const char *in, char *out) {
    out[0] = '\0';
    if (!in) return true;
    size_t n = strlen(in);
    if (n >= PAC_TEXTINPUT_TEXT_MAX || !utf8_plain(in, n)) return false;
    memcpy(out, in, n + 1);
    return true;
}

void pac_textinput_pending_preedit(PacTextInputPending *p, const char *utf8, int begin, int end) {
    p->has_preedit = true;
    p->begin = p->end = -1;
    if (!take(utf8, p->preedit)) return;
    size_t n = strlen(p->preedit);
    /* The cursor must be in the text, in order, and between characters: else none is shown. */
    if (begin >= 0 && end >= begin && (size_t)end <= n &&
        (begin == (int)n || ((unsigned char)p->preedit[begin] & 0xC0) != 0x80) &&
        (end == (int)n || ((unsigned char)p->preedit[end] & 0xC0) != 0x80)) {
        p->begin = begin;
        p->end = end;
    }
}

void pac_textinput_pending_commit(PacTextInputPending *p, const char *utf8) {
    take(utf8, p->commit);
}

void pac_textinput_pending_delete(PacTextInputPending *p, uint32_t before, uint32_t after) {
    p->delete_before = before;
    p->delete_after = after;
}

void pac_textinput_apply(PacTextInputPending *p, const PacTextInputSink *sink, void *data) {
    PacTextInputPending now = *p;
    pac_textinput_pending_reset(p);
    if (!sink) return;
    /* 1. Replace the existing preedit: it goes, whatever comes next. */
    if (now.has_preedit || now.commit[0] || now.delete_before || now.delete_after) {
        if (sink->preedit) sink->preedit(data, "", -1, -1);
    }
    /* 2. Delete the surrounding text; 3. insert the commit with the cursor at its end. */
    if (now.delete_before && sink->delete_before) sink->delete_before(data, now.delete_before);
    if (now.commit[0] && sink->commit) sink->commit(data, now.commit);
    /* 5. and 6. The new preedit, with the cursor in it. */
    if (now.has_preedit && now.preedit[0] && sink->preedit) sink->preedit(data, now.preedit, now.begin, now.end);
}
