#!/usr/bin/env python3
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def parse_capture(output: str) -> tuple[Path, list[str], dict[str, str]]:
    program: Path | None = None
    arguments: dict[int, str] = {}
    environment: dict[str, str] = {}
    for line in output.splitlines():
        if line.startswith("PROGRAM="):
            program = Path(line.removeprefix("PROGRAM="))
        elif line.startswith("ARG:"):
            index_text, value = line.removeprefix("ARG:").split("=", 1)
            arguments[int(index_text)] = value
        elif line.startswith("ENV:"):
            name, value = line.removeprefix("ENV:").split("=", 1)
            environment[name] = value
    if program is None:
        raise AssertionError(f"capture backend did not report its program path:\n{output}")
    return program, [arguments[index] for index in sorted(arguments)], environment


def normalized(path: str | Path) -> str:
    return os.path.normcase(os.path.normpath(str(path)))


def run(wrapper: Path, env: dict[str, str], *arguments: str) -> tuple[subprocess.CompletedProcess[str], Path, list[str], dict[str, str]]:
    result = subprocess.run(
        [str(wrapper), *arguments],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    program, captured_arguments, captured_environment = parse_capture(result.stdout)
    return result, program, captured_arguments, captured_environment


def main() -> None:
    if os.name != "nt":
        raise SystemExit("test_pkg_config_windows.py must run on Windows")
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_pkg_config_windows.py <wrapper.exe> <capture.exe>")

    source_wrapper = Path(sys.argv[1]).resolve()
    source_capture = Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="vita sdk pkgconfig ") as temporary:
        root = Path(temporary)
        bin_dir = root / "bin"
        lib_pc = root / "arm-vita-eabi" / "lib" / "pkgconfig"
        share_pc = root / "arm-vita-eabi" / "share" / "pkgconfig"
        for directory in (bin_dir, lib_pc, share_pc):
            directory.mkdir(parents=True, exist_ok=True)

        wrapper = bin_dir / "arm-vita-eabi-pkg-config.exe"
        backend = bin_dir / "pkgconf.exe"
        shutil.copy2(source_wrapper, wrapper)
        shutil.copy2(source_capture, backend)

        env = os.environ.copy()
        env.update(
            {
                "VITASDK": str(root / "wrong sdk"),
                "PKG_CONFIG_DIR": "polluted-dir",
                "PKG_CONFIG_PATH": "polluted-path",
                "PKG_CONFIG_SYSROOT_DIR": "polluted-sysroot",
                "PKG_CONFIG_LIBDIR": "polluted-libdir",
            }
        )

        result, program, arguments, captured_environment = run(
            wrapper,
            env,
            "--libs",
            "sample",
            "--define-variable=note=a b",
            '--define-variable=quote=a"b',
            "--define-variable=trail=C:\\path with space\\",
            "",
        )
        assert result.returncode == 0, result.stderr
        assert normalized(program) == normalized(backend)
        assert len(arguments) == 9
        assert arguments[0].startswith("--define-variable=VITASDK=")
        defined_root = arguments[0].split("=", 2)[2]
        assert "\\" not in defined_root
        assert normalized(defined_root) == normalized(root)
        assert arguments[1:] == [
            "--define-prefix",
            "--static",
            "--libs",
            "sample",
            "--define-variable=note=a b",
            '--define-variable=quote=a"b',
            "--define-variable=trail=C:\\path with space\\",
            "",
        ]
        assert captured_environment["PKG_CONFIG_DIR"] in ("", "<unset>")
        assert captured_environment["PKG_CONFIG_PATH"] in ("", "<unset>")
        assert captured_environment["PKG_CONFIG_SYSROOT_DIR"] in ("", "<unset>")
        libdirs = captured_environment["PKG_CONFIG_LIBDIR"].split(";")
        assert all("\\" not in path for path in libdirs)
        assert [normalized(path) for path in libdirs] == [normalized(lib_pc), normalized(share_pc)]

        result, program, arguments, _ = run(wrapper, env, "--version")
        assert result.returncode == 0, result.stderr
        assert normalized(program) == normalized(backend)
        assert arguments == ["--version"]

        failing_env = env.copy()
        failing_env["PKGCONF_CAPTURE_EXIT"] = "23"
        result, _, _, _ = run(wrapper, failing_env, "--modversion", "sample")
        assert result.returncode == 23

        host_bin = root / "host-bin"
        host_bin.mkdir()
        host_backend = host_bin / "pkgconf.exe"
        shutil.copy2(source_capture, host_backend)
        backend.unlink()
        fallback_env = env.copy()
        fallback_env["PATH"] = f"{host_bin}{os.pathsep}{env['PATH']}"
        result, program, arguments, _ = run(wrapper, fallback_env, "--modversion", "sample")
        assert result.returncode == 0, result.stderr
        assert normalized(program) == normalized(host_backend)
        assert arguments[-2:] == ["--modversion", "sample"]

        host_backend.unlink()
        missing = subprocess.run(
            [str(wrapper), "--version"],
            env={**env, "PATH": str(host_bin)},
            capture_output=True,
            text=True,
            check=False,
        )
        assert missing.returncode != 0
        assert "pkgconf" in missing.stderr.lower()

    print("test_pkg_config_windows: ALL TESTS PASSED")


if __name__ == "__main__":
    main()
