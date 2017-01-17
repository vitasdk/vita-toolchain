#ifndef VITA_TOOLCHAIN_PUBLIC
#ifndef VITA_TOOLCHAIN_SHARED
#define VITA_TOOLCHAIN_PUBLIC
#elif (defined(_WIN32) || defined(__CYGWIN__))
#define VITA_TOOLCHAIN_PUBLIC __declspec(dllexport)
#else
#define VITA_TOOLCHAIN_PUBLIC __attribute__((visibility("default")))
#endif
#endif
