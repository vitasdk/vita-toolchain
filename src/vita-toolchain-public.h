#ifndef VITA_TOOLCHAIN_PUBLIC
#if (defined(_WIN32) || defined(__CYGWIN__)) && defined(VITA_TOOLCHAIN_SHARED)
#define VITA_TOOLCHAIN_PUBLIC __declspec(dllimport)
#else
#define VITA_TOOLCHAIN_PUBLIC
#endif
#endif
