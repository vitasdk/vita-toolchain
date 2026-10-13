#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct {
    wchar_t *data;
    size_t length;
    size_t capacity;
} WideBuffer;

static int buffer_reserve(WideBuffer *buffer, size_t extra) {
    if (extra > SIZE_MAX - buffer->length - 1) {
        return 0;
    }
    size_t required = buffer->length + extra + 1;
    if (required <= buffer->capacity) {
        return 1;
    }

    size_t capacity = buffer->capacity ? buffer->capacity : 128;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }

    if (capacity > SIZE_MAX / sizeof(*buffer->data)) {
        return 0;
    }
    wchar_t *data = realloc(buffer->data, capacity * sizeof(*data));
    if (!data) {
        return 0;
    }
    buffer->data = data;
    buffer->capacity = capacity;
    return 1;
}

static int buffer_append_character(WideBuffer *buffer, wchar_t value) {
    if (!buffer_reserve(buffer, 1)) {
        return 0;
    }
    buffer->data[buffer->length++] = value;
    buffer->data[buffer->length] = L'\0';
    return 1;
}

static int buffer_append_repeated(WideBuffer *buffer, wchar_t value, size_t count) {
    if (!buffer_reserve(buffer, count)) {
        return 0;
    }
    for (size_t index = 0; index < count; index++) {
        buffer->data[buffer->length++] = value;
    }
    buffer->data[buffer->length] = L'\0';
    return 1;
}

static int buffer_append_argument(WideBuffer *buffer, const wchar_t *argument) {
    if (buffer->length && !buffer_append_character(buffer, L' ')) {
        return 0;
    }

    int requires_quotes = !*argument || wcspbrk(argument, L" \t\n\v\"") != NULL;
    if (!requires_quotes) {
        size_t length = wcslen(argument);
        if (!buffer_reserve(buffer, length)) {
            return 0;
        }
        memcpy(buffer->data + buffer->length, argument, (length + 1) * sizeof(*argument));
        buffer->length += length;
        return 1;
    }

    if (!buffer_append_character(buffer, L'\"')) {
        return 0;
    }

    size_t backslashes = 0;
    for (const wchar_t *cursor = argument; *cursor; cursor++) {
        if (*cursor == L'\\') {
            backslashes++;
            continue;
        }
        if (*cursor == L'\"') {
            if (!buffer_append_repeated(buffer, L'\\', backslashes * 2 + 1) ||
                !buffer_append_character(buffer, L'\"')) {
                return 0;
            }
        } else {
            if (!buffer_append_repeated(buffer, L'\\', backslashes) ||
                !buffer_append_character(buffer, *cursor)) {
                return 0;
            }
        }
        backslashes = 0;
    }

    return buffer_append_repeated(buffer, L'\\', backslashes * 2) &&
           buffer_append_character(buffer, L'\"');
}

static wchar_t *module_path(void) {
    DWORD capacity = 512;
    for (;;) {
        wchar_t *path = calloc(capacity, sizeof(*path));
        if (!path) {
            return NULL;
        }

        DWORD length = GetModuleFileNameW(NULL, path, capacity);
        if (!length) {
            free(path);
            return NULL;
        }
        if (length < capacity) {
            return path;
        }

        free(path);
        if (capacity >= 32768) {
            return NULL;
        }
        capacity *= 2;
    }
}

static wchar_t *last_separator(wchar_t *path) {
    wchar_t *backslash = wcsrchr(path, L'\\');
    wchar_t *slash = wcsrchr(path, L'/');
    if (!backslash) {
        return slash;
    }
    if (!slash) {
        return backslash;
    }
    return backslash > slash ? backslash : slash;
}

static wchar_t *parent_directory(const wchar_t *path) {
    wchar_t *parent = _wcsdup(path);
    if (!parent) {
        return NULL;
    }

    wchar_t *separator = last_separator(parent);
    if (!separator || separator == parent) {
        free(parent);
        return NULL;
    }
    *separator = L'\0';
    return parent;
}

static wchar_t *join_path(const wchar_t *directory, const wchar_t *name) {
    size_t directory_length = wcslen(directory);
    size_t name_length = wcslen(name);
    int needs_separator = directory_length && directory[directory_length - 1] != L'\\' &&
                          directory[directory_length - 1] != L'/';
    wchar_t *path = calloc(directory_length + needs_separator + name_length + 1, sizeof(*path));
    if (!path) {
        return NULL;
    }

    wcscpy(path, directory);
    if (needs_separator) {
        wcscat(path, L"\\");
    }
    wcscat(path, name);
    return path;
}

static int regular_file_exists(const wchar_t *path) {
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static wchar_t *search_path(const wchar_t *name) {
    DWORD capacity = 512;
    for (;;) {
        wchar_t *path = calloc(capacity, sizeof(*path));
        if (!path) {
            return NULL;
        }
        DWORD length = SearchPathW(NULL, name, NULL, capacity, path, NULL);
        if (!length) {
            free(path);
            return NULL;
        }
        if (length < capacity) {
            return path;
        }
        free(path);
        capacity = length + 1;
    }
}

static wchar_t *make_libdir(const wchar_t *root) {
    static const wchar_t first[] = L"/arm-vita-eabi/lib/pkgconfig";
    static const wchar_t second[] = L"/arm-vita-eabi/share/pkgconfig";
    size_t length = wcslen(root) * 2 + wcslen(first) + wcslen(second) + 2;
    wchar_t *value = calloc(length, sizeof(*value));
    if (!value) {
        return NULL;
    }
    swprintf(value, length, L"%ls%ls;%ls%ls", root, first, root, second);
    return value;
}

static wchar_t *make_define_argument(const wchar_t *root) {
    static const wchar_t prefix[] = L"--define-variable=VITASDK=";
    size_t length = wcslen(prefix) + wcslen(root) + 1;
    wchar_t *argument = calloc(length, sizeof(*argument));
    if (!argument) {
        return NULL;
    }
    swprintf(argument, length, L"%ls%ls", prefix, root);
    return argument;
}

static wchar_t *pkgconf_path(const wchar_t *path) {
    wchar_t *normalized = _wcsdup(path);
    if (!normalized) {
        return NULL;
    }
    for (wchar_t *cursor = normalized; *cursor; cursor++) {
        if (*cursor == L'\\') {
            *cursor = L'/';
        }
    }
    return normalized;
}

static int run_backend(const wchar_t *backend, int argc, wchar_t **argv,
                       const wchar_t *define_argument, DWORD *exit_code) {
    WideBuffer command_line = {0};
    int version_only = argc > 1 && wcscmp(argv[1], L"--version") == 0;

    if (!buffer_append_argument(&command_line, backend)) {
        free(command_line.data);
        return 0;
    }
    if (version_only) {
        if (!buffer_append_argument(&command_line, L"--version")) {
            free(command_line.data);
            return 0;
        }
    } else {
        if (!buffer_append_argument(&command_line, define_argument) ||
            !buffer_append_argument(&command_line, L"--define-prefix") ||
            !buffer_append_argument(&command_line, L"--static")) {
            free(command_line.data);
            return 0;
        }
        for (int index = 1; index < argc; index++) {
            if (!buffer_append_argument(&command_line, argv[index])) {
                free(command_line.data);
                return 0;
            }
        }
    }

    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    startup.cb = sizeof(startup);
    if (!CreateProcessW(backend, command_line.data, NULL, NULL, TRUE, 0, NULL, NULL,
                        &startup, &process)) {
        free(command_line.data);
        return 0;
    }
    free(command_line.data);

    DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    int success = wait_result == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return success;
}

int wmain(int argc, wchar_t **argv) {
    wchar_t *executable = module_path();
    wchar_t *bin_directory = executable ? parent_directory(executable) : NULL;
    wchar_t *root = bin_directory ? parent_directory(bin_directory) : NULL;
    wchar_t *normalized_root = root ? pkgconf_path(root) : NULL;
    wchar_t *bundled_backend = bin_directory ? join_path(bin_directory, L"pkgconf.exe") : NULL;
    wchar_t *libdir = normalized_root ? make_libdir(normalized_root) : NULL;
    wchar_t *define_argument = normalized_root ? make_define_argument(normalized_root) : NULL;

    if (!executable || !bin_directory || !root || !normalized_root || !bundled_backend || !libdir ||
        !define_argument) {
        fwprintf(stderr, L"arm-vita-eabi-pkg-config: unable to determine the VitaSDK installation root\n");
        free(define_argument);
        free(libdir);
        free(bundled_backend);
        free(normalized_root);
        free(root);
        free(bin_directory);
        free(executable);
        return 127;
    }

    if (!SetEnvironmentVariableW(L"VITASDK", normalized_root) ||
        !SetEnvironmentVariableW(L"PKG_CONFIG_DIR", NULL) ||
        !SetEnvironmentVariableW(L"PKG_CONFIG_PATH", NULL) ||
        !SetEnvironmentVariableW(L"PKG_CONFIG_SYSROOT_DIR", NULL) ||
        !SetEnvironmentVariableW(L"PKG_CONFIG_LIBDIR", libdir)) {
        fwprintf(stderr, L"arm-vita-eabi-pkg-config: unable to configure the pkgconf environment (%lu)\n",
                 GetLastError());
        free(define_argument);
        free(libdir);
        free(bundled_backend);
        free(normalized_root);
        free(root);
        free(bin_directory);
        free(executable);
        return 127;
    }

    wchar_t *backend = NULL;
    if (regular_file_exists(bundled_backend)) {
        backend = _wcsdup(bundled_backend);
    } else {
        backend = search_path(L"pkgconf.exe");
        if (!backend) {
            backend = search_path(L"pkg-config.exe");
        }
    }

    DWORD exit_code = 127;
    if (!backend) {
        fwprintf(stderr, L"arm-vita-eabi-pkg-config: pkgconf.exe is not installed\n");
    } else if (!run_backend(backend, argc, argv, define_argument, &exit_code)) {
        fwprintf(stderr, L"arm-vita-eabi-pkg-config: unable to execute pkgconf backend '%ls' (%lu)\n",
                 backend, GetLastError());
        exit_code = 127;
    }

    free(backend);
    free(define_argument);
    free(libdir);
    free(bundled_backend);
    free(normalized_root);
    free(root);
    free(bin_directory);
    free(executable);
    return (int)exit_code;
}
