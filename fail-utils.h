#ifndef FAIL_UTILS_H
#define FAIL_UTILS_H

#ifndef __MINGW32__
#include <err.h>
#else
#include <stdio.h>
#include <string.h>
#define warnx(fmt, args...) fprintf(stderr, __FILE__ ": " fmt, ##args)
#define warn(fmt, args...) warnx(fmt ": %s", ##args, strerror(errno))
#define errx(retval, args...) do {warnx(args); exit(retval);} while (0)
#define err(retval, args...) do {warn(args); exit(retval);} while (0)
#endif

#define FAIL_EX(label, function, fmt...) do { \
	function(fmt); \
	goto label; \
} while (0)
#define FAIL(fmt...) FAIL_EX(failure, warn, fmt)
#define FAILX(fmt...) FAIL_EX(failure, warnx, fmt)
#define FAILE(fmt, args...) FAIL_EX(failure, warnx, fmt ": %s", ##args, elf_errmsg(-1))
#define ASSERT(condition) do { \
	if (!(condition)) FAILX("Assertion failed: (" #condition ")"); \
} while (0)

#define SYS_ASSERT(cmd) if ((cmd) < 0) FAIL(#cmd " failed")
#define ELF_ASSERT(cmd) if (!(cmd)) FAILE(#cmd " failed")

#endif
