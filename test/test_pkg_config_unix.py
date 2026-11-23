#!/usr/bin/env python3
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile


def write_backend(path: Path, name: str) -> None:
    path.write_text(
        "#!/usr/bin/env python3\n"
        "import json, os, sys\n"
        f"payload = {{'backend': {name!r}, 'argv': sys.argv[1:], "
        "'env': {key: os.environ.get(key) for key in "
        "['PKG_CONFIG_DIR', 'PKG_CONFIG_PATH', 'PKG_CONFIG_SYSROOT_DIR', "
        "'PKG_CONFIG_PRELOADED_FILES', 'PKG_CONFIG_MSVC_SYNTAX', "
        "'PKG_CONFIG_SYSTEM_INCLUDE_PATH', 'PKG_CONFIG_SYSTEM_LIBRARY_PATH', "
        "'PKG_CONFIG_LIBDIR']}}\n"
        "print(json.dumps(payload))\n"
        "sys.exit(23 if '--fail' in sys.argv else 0)\n",
        encoding="utf-8",
    )
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


def invoke(wrapper: Path, env: dict[str, str], *arguments: str) -> tuple[subprocess.CompletedProcess[str], dict[str, object]]:
    result = subprocess.run(
        [str(wrapper), *arguments],
        env=env,
        text=True,
        capture_output=True,
        check=False,
    )
    payload: dict[str, object] = {}
    if result.stdout.strip():
        payload = json.loads(result.stdout)
    return result, payload


def test_real_pkgconf(source_wrapper: Path) -> None:
    pkgconf = shutil.which("pkgconf")
    assert pkgconf is not None, "pkgconf is required for the Unix frontend integration test"

    with tempfile.TemporaryDirectory(prefix="vita sdk real pkgconf ") as temporary:
        root = Path(temporary)
        bin_dir = root / "bin"
        lib_pc = root / "arm-vita-eabi" / "lib" / "pkgconfig"
        share_pc = root / "arm-vita-eabi" / "share" / "pkgconfig"
        for directory in (bin_dir, lib_pc, share_pc):
            directory.mkdir(parents=True, exist_ok=True)

        wrapper = bin_dir / "arm-vita-eabi-pkg-config"
        shutil.copy2(source_wrapper, wrapper)
        wrapper.chmod(wrapper.stat().st_mode | stat.S_IXUSR)
        (bin_dir / "pkgconf").symlink_to(pkgconf)

        (lib_pc / "vitasdk-wrapper-probe.pc").write_text(
            "prefix=${VITASDK}/arm-vita-eabi\n"
            "exec_prefix=${prefix}\n"
            "libdir=${exec_prefix}/lib\n"
            "includedir=${prefix}/include\n"
            "Name: vitasdk-wrapper-probe\n"
            "Description: VitaSDK pkg-config frontend integration test\n"
            "Version: 1.0\n"
            "Libs: -L${libdir} -lprobe\n"
            "Cflags: -I${includedir}/probe\n",
            encoding="utf-8",
        )

        env = os.environ.copy()
        env.update(
            {
                "VITASDK": str(root),
                "PKG_CONFIG_DIR": "polluted-dir",
                "PKG_CONFIG_PATH": "polluted-path",
                "PKG_CONFIG_SYSROOT_DIR": "polluted-sysroot",
                "PKG_CONFIG_PRELOADED_FILES": "polluted-preloaded",
                "PKG_CONFIG_MSVC_SYNTAX": "1",
                "PKG_CONFIG_SYSTEM_INCLUDE_PATH": "polluted-system-include",
                "PKG_CONFIG_SYSTEM_LIBRARY_PATH": "polluted-system-library",
                "PKG_CONFIG_LIBDIR": "polluted-libdir",
            }
        )

        cflags = subprocess.run(
            [str(wrapper), "--cflags", "vitasdk-wrapper-probe"],
            env=env,
            text=True,
            capture_output=True,
            check=False,
        )
        assert cflags.returncode == 0, cflags.stderr
        assert cflags.stdout.strip().startswith("-I"), cflags.stdout
        assert "/arm-vita-eabi/include/probe" in cflags.stdout, cflags.stdout

        libs = subprocess.run(
            [str(wrapper), "--libs", "vitasdk-wrapper-probe"],
            env=env,
            text=True,
            capture_output=True,
            check=False,
        )
        assert libs.returncode == 0, libs.stderr
        assert libs.stdout.strip().startswith("-L"), libs.stdout
        assert "-lprobe" in libs.stdout, libs.stdout
        assert "/libpath:" not in libs.stdout, libs.stdout
        assert "probe.lib" not in libs.stdout, libs.stdout


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_pkg_config_unix.py <wrapper>")

    source_wrapper = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="vita sdk pkgconfig ") as temporary:
        root = Path(temporary)
        bin_dir = root / "bin"
        host_bin = root / "host-bin"
        lib_pc = root / "arm-vita-eabi" / "lib" / "pkgconfig"
        share_pc = root / "arm-vita-eabi" / "share" / "pkgconfig"
        for directory in (bin_dir, host_bin, lib_pc, share_pc):
            directory.mkdir(parents=True, exist_ok=True)

        wrapper = bin_dir / "arm-vita-eabi-pkg-config"
        shutil.copy2(source_wrapper, wrapper)
        wrapper.chmod(wrapper.stat().st_mode | stat.S_IXUSR)
        write_backend(bin_dir / "pkgconf", "bundled")
        write_backend(host_bin / "pkgconf", "host-pkgconf")
        write_backend(host_bin / "pkg-config", "host-pkg-config")

        env = os.environ.copy()
        env.update(
            {
                "VITASDK": str(root),
                "PATH": f"{host_bin}{os.pathsep}{env['PATH']}",
                "PKG_CONFIG_DIR": "polluted-dir",
                "PKG_CONFIG_PATH": "polluted-path",
                "PKG_CONFIG_SYSROOT_DIR": "polluted-sysroot",
                "PKG_CONFIG_PRELOADED_FILES": "polluted-preloaded",
                "PKG_CONFIG_MSVC_SYNTAX": "polluted-msvc",
                "PKG_CONFIG_SYSTEM_INCLUDE_PATH": "polluted-system-include",
                "PKG_CONFIG_SYSTEM_LIBRARY_PATH": "polluted-system-library",
                "PKG_CONFIG_LIBDIR": "polluted-libdir",
            }
        )

        result, payload = invoke(wrapper, env, "--libs", "sample", "--define-variable=note=a b")
        assert result.returncode == 0, result.stderr
        assert payload["backend"] == "bundled"
        assert payload["argv"] == [
            f"--define-variable=VITASDK={root}",
            "--define-prefix",
            "--static",
            "--libs",
            "sample",
            "--define-variable=note=a b",
        ]
        assert payload["env"] == {
            "PKG_CONFIG_DIR": None,
            "PKG_CONFIG_PATH": None,
            "PKG_CONFIG_SYSROOT_DIR": None,
            "PKG_CONFIG_PRELOADED_FILES": None,
            "PKG_CONFIG_MSVC_SYNTAX": None,
            "PKG_CONFIG_SYSTEM_INCLUDE_PATH": None,
            "PKG_CONFIG_SYSTEM_LIBRARY_PATH": None,
            "PKG_CONFIG_LIBDIR": f"{lib_pc}:{share_pc}",
        }

        result, payload = invoke(wrapper, env, "--version")
        assert result.returncode == 0, result.stderr
        assert payload["backend"] == "bundled"
        assert payload["argv"] == ["--version"]

        result, _ = invoke(wrapper, env, "--fail")
        assert result.returncode == 23

        (bin_dir / "pkgconf").unlink()
        result, payload = invoke(wrapper, env, "--modversion", "sample")
        assert result.returncode == 0, result.stderr
        assert payload["backend"] == "host-pkgconf"
        assert payload["argv"][:3] == [
            f"--define-variable=VITASDK={root}",
            "--define-prefix",
            "--static",
        ]

        isolated_bin = root / "isolated-bin"
        isolated_bin.mkdir()
        bash = shutil.which("bash")
        assert bash is not None
        (isolated_bin / "bash").symlink_to(bash)
        result, _ = invoke(wrapper, {**env, "PATH": str(isolated_bin)}, "--libs", "sample")
        assert result.returncode == 127
        assert result.stderr.strip() == "arm-vita-eabi-pkg-config: pkg-config is not installed. Aborting."

    test_real_pkgconf(source_wrapper)
    print("test_pkg_config_unix: ALL TESTS PASSED")


if __name__ == "__main__":
    main()
