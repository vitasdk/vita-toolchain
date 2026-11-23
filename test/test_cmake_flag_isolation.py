import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path


BASE_FLAGS = {
    "VITA_ELF_CREATE_FLAGS": "--elf-base",
    "VITA_MAKE_FSELF_FLAGS": "--fself-base",
    "VITA_ELF_EXPORT_FLAGS": "--export-base",
    "VITA_LIBS_GEN_FLAGS": "--libs-base",
    "VITA_MKSFOEX_FLAGS": "--sfo-base",
    "VITA_PACK_VPK_FLAGS": "--pack-base",
}


def read_build_make(build_dir, target):
    path = build_dir / "CMakeFiles" / f"{target}.dir" / "build.make"
    assert path.is_file(), f"Missing generated build file: {path}"
    return path.read_text()


def echo_commands(content):
    return [
        shlex.split(line.strip())
        for line in content.splitlines()
        if line.startswith("\techo ")
    ]


def find_command(commands, *, contains=None, endswith=None):
    contains = contains or []
    matches = [
        command
        for command in commands
        if all(token in command for token in contains)
        and (endswith is None or command[-1].endswith(endswith))
    ]
    assert len(matches) == 1, (
        f"Expected one command containing {contains} and ending with {endswith}, "
        f"got {matches}"
    )
    return matches[0]


def command_flags(command, trailing_arguments):
    return command[1:-trailing_arguments]


def read_public_flags(build_dir):
    values = {}
    for line in (build_dir / "public-flags.txt").read_text().splitlines():
        name, value = line.split("=", 1)
        values[name] = value
    return values


def main():
    if len(sys.argv) != 2:
        print("Usage: test_cmake_flag_isolation.py <path-to-vita.cmake>")
        sys.exit(1)

    vita_cmake = os.path.abspath(sys.argv[1])
    fixture_dir = Path(__file__).parent / "fixtures" / "cmake_flag_isolation"

    with tempfile.TemporaryDirectory() as tmpdir:
        build_dir = Path(tmpdir)
        result = subprocess.run(
            [
                "cmake",
                "-S",
                str(fixture_dir),
                "-B",
                str(build_dir),
                f"-DVITA_CMAKE={vita_cmake}",
                "-G",
                "Unix Makefiles",
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print("cmake configure failed:", result.stderr)
            sys.exit(1)

        first_elf = find_command(
            echo_commands(read_build_make(build_dir, "self_a-velf")),
            endswith="self_a.velf",
        )
        second_elf = find_command(
            echo_commands(read_build_make(build_dir, "self_b-velf")),
            endswith="self_b.velf",
        )
        assert command_flags(first_elf, 2) == ["--elf-base", "-s"], first_elf
        assert command_flags(second_elf, 2) == ["--elf-base"], second_elf

        first_self = find_command(
            echo_commands(read_build_make(build_dir, "first.self-self")),
            endswith="first.self.out",
        )
        second_self = find_command(
            echo_commands(read_build_make(build_dir, "second.self-self")),
            endswith="second.self.out",
        )
        assert command_flags(first_self, 2) == [
            "--fself-base",
            "-c",
            "-m",
            "42",
            "-s",
        ], first_self
        assert command_flags(second_self, 2) == ["--fself-base"], second_self

        first_stubs = echo_commands(read_build_make(build_dir, "stubs_a"))
        second_stubs = echo_commands(read_build_make(build_dir, "stubs_b"))
        first_export = find_command(first_stubs, contains=["user", "--export-base"])
        second_export = find_command(second_stubs, contains=["kernel", "--export-base"])
        first_libs = find_command(first_stubs, contains=["--libs-base"])
        second_libs = find_command(second_stubs, contains=["--libs-base"])
        assert first_export[1:3] == ["user", "--export-base"], first_export
        assert second_export[1:3] == ["kernel", "--export-base"], second_export
        assert first_libs[1] == "--libs-base", first_libs
        assert second_libs[1] == "--libs-base", second_libs

        first_vpk_make = read_build_make(build_dir, "first.vpk-vpk")
        second_vpk_make = read_build_make(build_dir, "second.vpk-vpk")
        first_vpk_commands = echo_commands(first_vpk_make)
        second_vpk_commands = echo_commands(second_vpk_make)

        first_sfo = find_command(first_vpk_commands, endswith="first.vpk_param.sfo")
        second_sfo = find_command(second_vpk_commands, endswith="second.vpk_param.sfo")
        assert command_flags(first_sfo, 2) == [
            "--sfo-base",
            "-s",
            "APP_VER=01.23",
            "-s",
            "TITLE_ID=FIRST0001",
        ], first_sfo
        assert command_flags(second_sfo, 2) == [
            "--sfo-base",
            "-s",
            "TITLE_ID=SECOND002",
        ], second_sfo

        first_pack = find_command(first_vpk_commands, endswith="first.vpk.out")
        second_pack = find_command(second_vpk_commands, endswith="second.vpk.out")
        assert first_pack[1] == "--pack-base", first_pack
        assert "-a" in first_pack, first_pack
        assert any("resource.txt=data/resource.txt" in token for token in first_pack), first_pack
        assert second_pack[1] == "--pack-base", second_pack
        assert "-a" not in second_pack, second_pack
        assert "resource.txt" not in second_vpk_make, second_vpk_make
        assert "FIRST0001" not in second_vpk_make, second_vpk_make
        assert "APP_VER=01.23" not in second_vpk_make, second_vpk_make

        assert read_public_flags(build_dir) == BASE_FLAGS

    print("test_cmake_flag_isolation: ALL TESTS PASSED")


if __name__ == "__main__":
    main()
