#!/usr/bin/env python3
import sys
import os
import re
import subprocess
import tempfile

def configure_and_get_command(vita_cmake, fixture_dir, self_flag, tmpdir):
    build_dir = os.path.join(tmpdir, self_flag or "default")
    res = subprocess.run([
        "cmake", "-S", fixture_dir, "-B", build_dir,
        f"-DVITA_CMAKE={vita_cmake}", f"-DSELF_FLAG={self_flag}", "-G", "Unix Makefiles"
    ], capture_output=True, text=True)
    if res.returncode != 0:
        print(f"cmake configure failed (SELF_FLAG={self_flag}):", res.stderr)
        sys.exit(1)

    build_make = os.path.join(build_dir, "CMakeFiles", "mytarget.self-self.dir", "build.make")
    with open(build_make) as f:
        content = f.read()

    m = re.search(r'^\techo (.*) \S+/mytarget\.velf \S+/mytarget\.self\.out$', content, re.MULTILINE)
    assert m, f"Could not find the vita-make-fself command in {build_make}"
    return m.group(1).split()

def main():
    if len(sys.argv) < 2:
        print("Usage: test_cmake_secretsafe.py <path-to-vita.cmake>")
        sys.exit(1)

    vita_cmake = sys.argv[1]
    fixture_dir = os.path.join(os.path.dirname(__file__), "fixtures", "cmake_secretsafe")

    with tempfile.TemporaryDirectory() as tmpdir:
        # Default: safe (-s), no secret-safe.
        default_flags = configure_and_get_command(vita_cmake, fixture_dir, "", tmpdir)
        assert "-s" in default_flags, f"Expected -s by default, got {default_flags}"
        assert "-ss" not in default_flags, f"Did not expect -ss by default, got {default_flags}"

        # UNSAFE: neither -s nor -ss.
        unsafe_flags = configure_and_get_command(vita_cmake, fixture_dir, "UNSAFE", tmpdir)
        assert "-s" not in unsafe_flags, f"UNSAFE should drop -s, got {unsafe_flags}"
        assert "-ss" not in unsafe_flags, f"UNSAFE should not add -ss, got {unsafe_flags}"

        # SECRETSAFE: -ss, and NOT the plain -s (#111).
        secretsafe_flags = configure_and_get_command(vita_cmake, fixture_dir, "SECRETSAFE", tmpdir)
        assert "-ss" in secretsafe_flags, f"Expected -ss with SECRETSAFE, got {secretsafe_flags}"
        assert "-s" not in secretsafe_flags, f"SECRETSAFE should not also pass plain -s, got {secretsafe_flags}"

    print("test_cmake_secretsafe: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
