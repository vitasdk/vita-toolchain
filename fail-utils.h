#ifndef FAIL_UTILS_H
#define FAIL_UTILS_H

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
