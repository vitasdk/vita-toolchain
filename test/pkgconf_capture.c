#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static void print_environment(const wchar_t *name) {
    const wchar_t *value = _wgetenv(name);
    wprintf(L"ENV:%ls=%ls\n", name, value ? value : L"<unset>");
}

int wmain(int argc, wchar_t **argv) {
    wprintf(L"PROGRAM=%ls\n", argv[0]);
    for (int index = 1; index < argc; index++) {
        wprintf(L"ARG:%d=%ls\n", index - 1, argv[index]);
    }

    print_environment(L"PKG_CONFIG_DIR");
    print_environment(L"PKG_CONFIG_PATH");
    print_environment(L"PKG_CONFIG_SYSROOT_DIR");
    print_environment(L"PKG_CONFIG_LIBDIR");

    const wchar_t *exit_text = _wgetenv(L"PKGCONF_CAPTURE_EXIT");
    if (!exit_text || !*exit_text) {
        return 0;
    }

    wchar_t *end = NULL;
    errno = 0;
    long exit_code = wcstol(exit_text, &end, 10);
    if (errno || end == exit_text || *end || exit_code < 0 || exit_code > 255) {
        return 126;
    }
    return (int)exit_code;
}
