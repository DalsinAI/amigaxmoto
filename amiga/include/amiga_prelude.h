/*
 * Included before every C++ file of the AmigaOS 3 build (-include).
 * - The toolchain's <pthread.h>, which libstdc++ pulls in, turns read(),
 *   write(), open() ... into cancellation-point macros when <unistd.h> came
 *   first, and then <istream>'s own read() member does not compile. Taken
 *   first, here, it defines none of them.
 * - glibc's headers bring <time.h> in with <string> and friends; libnix's
 *   don't, and several of X-Moto's files call time() and localtime()
 *   without including it.
 * The AmiCradle Team. GPL 2 or later, like X-Moto.
 */
#ifndef AMIGA_PRELUDE_H
#define AMIGA_PRELUDE_H
#include <pthread.h>
#include <time.h>
#endif
