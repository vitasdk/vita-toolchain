#include <windows.h>

#include <stdio.h>
#include <wchar.h>

static BOOL WINAPI fail_get_exit_code(HANDLE process, LPDWORD exit_code);
static BOOL WINAPI close_handle_and_clobber_error(HANDLE handle);

#define GetExitCodeProcess fail_get_exit_code
#define CloseHandle close_handle_and_clobber_error
#define wmain pkgconfig_frontend_wmain
#include "../pkgconfig/arm-vita-eabi-pkg-config-windows.c"
#undef wmain
#undef CloseHandle
#undef GetExitCodeProcess

static BOOL WINAPI fail_get_exit_code(HANDLE process, LPDWORD exit_code) {
    (void)process;
    (void)exit_code;
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

static BOOL WINAPI close_handle_and_clobber_error(HANDLE handle) {
    BOOL result = CloseHandle(handle);
    SetLastError(ERROR_INVALID_HANDLE);
    return result;
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 2) {
        fwprintf(stderr, L"usage: pkgconfig-error-preservation <backend.exe>\n");
        return 2;
    }

    wchar_t *backend_argv[] = {L"arm-vita-eabi-pkg-config.exe"};
    DWORD exit_code = 0;
    SetLastError(ERROR_SUCCESS);
    if (run_backend(argv[1], 1, backend_argv, L"--define-variable=VITASDK=C:/sdk", &exit_code)) {
        fwprintf(stderr, L"run_backend unexpectedly succeeded\n");
        return 1;
    }

    DWORD error = GetLastError();
    if (error != ERROR_ACCESS_DENIED) {
        fwprintf(stderr, L"run_backend reported %lu instead of %lu\n", error,
                 (DWORD)ERROR_ACCESS_DENIED);
        return 1;
    }
    return 0;
}
