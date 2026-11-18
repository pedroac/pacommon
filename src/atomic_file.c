#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "atomic_file.h"
#include "tunables.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

bool atomic_write(const char *path, const void *data, size_t len) {
    char temp[PAC_FILE_PATH_MAX + 16];
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    size_t folder = slash ? (size_t)(slash - path) : 0;
    if (!*name || strlen(path) >= PAC_FILE_PATH_MAX) { errno = EINVAL; return false; }
    /* ".name.pid" next to the file; "." is the folder of a path with no slash. */
    if (snprintf(temp, sizeof temp, "%.*s%s.%s.%ld", (int)folder, path, slash ? "/" : "", name,
                 (long)getpid()) >= (int)sizeof temp) { errno = ENAMETOOLONG; return false; }

    unlink(temp);   /* a left-over of a crash; O_EXCL below refuses to follow anything else */
    int fd = open(temp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    bool ok = true;
    for (size_t done = 0; ok && done < len;) {
        ssize_t w = write(fd, (const char *)data + done, len - done);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) ok = false;
        else done += (size_t)w;
    }
    ok = ok && fsync(fd) == 0;
    int saved = errno;
    ok = close(fd) == 0 && ok;
    if (ok && rename(temp, path) < 0) { saved = errno; ok = false; }
    if (!ok) { unlink(temp); errno = saved; }
    return ok;
}
