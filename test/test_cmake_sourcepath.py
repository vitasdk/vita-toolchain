#!/usr/bin/env python3
import sys
import os
import re
import subprocess
import tempfile

def main():
    if len(sys.argv) < 2:
        print("Usage: test_cmake_sourcepath.py <path-to-vita.cmake>")
        sys.exit(1)

    vita_cmake = sys.argv[1]
    fixture_dir = os.path.join(os.path.dirname(__file__), "fixtures", "cmake_sourcepath")

    with tempfile.TemporaryDirectory() as tmpdir:
        res = subprocess.run([
            "cmake", "-S", fixture_dir, "-B", tmpdir,
            f"-DVITA_CMAKE={vita_cmake}", "-G", "Unix Makefiles"
        ], capture_output=True, text=True)
        if res.returncode != 0:
            print("cmake configure failed:", res.stderr)
            sys.exit(1)

        # mytarget has OUTPUT_NAME "othername" — vita_create_self must build the
        # velf from the actual linked file (othername), not from a path guessed
        # off the CMake target name (mytarget). Regression test for #280.
        build_make = os.path.join(tmpdir, "CMakeFiles", "mytarget-velf.dir", "build.make")
        with open(build_make) as f:
            content = f.read()

        m = re.search(r'^\techo (\S+) \S+/mytarget\.velf$', content, re.MULTILINE)
        assert m, f"Could not find the vita-elf-create command in {build_make}"
        sourcepath = m.group(1)
        assert sourcepath.endswith("/othername"), \
            f"Expected the OUTPUT_NAME 'othername' path, got '{sourcepath}' (still using the CMake target name?)"

    print("test_cmake_sourcepath: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
