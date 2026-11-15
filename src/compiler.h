#ifndef PACOMMON_COMPILER_H
#define PACOMMON_COMPILER_H

/* Hints for GCC and Clang, the only compilers the pac tools are built with. They change how code
 * is laid out, not what it does, so a wrong hint costs speed and never correctness. Use them only
 * where a profile shows they help, and say in a comment what was measured (docs/development.md). */

/* The condition is almost always true, or almost always false: a hot loop's common case, or an
 * error path that should stay out of the way. */
#define PAC_LIKELY(x)   __builtin_expect(!!(x), 1)
#define PAC_UNLIKELY(x) __builtin_expect(!!(x), 0)

/* A function that is called rarely, such as one that reports an error: it is placed away from the
 * hot code and the paths that call it are treated as unlikely. */
#define PAC_COLD __attribute__((cold))

#endif
