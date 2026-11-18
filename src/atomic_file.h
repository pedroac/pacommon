#ifndef ATOMIC_FILE_H
#define ATOMIC_FILE_H

#include <stdbool.h>
#include <stddef.h>

/* Writes len bytes to path so that a crash or a full disk never leaves half a file: the data goes
 * to a new private file (mode 0600) in the same folder, is flushed to disk, and is then renamed
 * over path. Returns false, with errno set and path untouched, if anything fails, if the folder
 * does not exist (it is not made) or if the path is longer than PAC_FILE_PATH_MAX. The temporary
 * file is made with O_EXCL and O_NOFOLLOW, so a link someone left in the folder is not followed.
 * If path is a symbolic link, the link is replaced, not what it points to. */
bool atomic_write(const char *path, const void *data, size_t len);

#endif
