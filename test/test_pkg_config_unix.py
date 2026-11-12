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
            "PKG_CONFIG_DIR": "",
            "PKG_CONFIG_PATH": "",
            "PKG_CONFIG_SYSROOT_DIR": "",
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

    print("test_pkg_config_unix: ALL TESTS PASSED")


if __name__ == "__main__":
    main()
