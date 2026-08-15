#!/usr/bin/env python3
import sys
import os
import subprocess
import tempfile

# Original repro from #95: a root-level "version" key used to fall into the
# "unknow tag" warning path in read_vita_imports() instead of being accepted
# as a no-op, on the way to a reported crash.
YAML_WITH_VERSION = """
version: 2
firmware: 3.60
modules:
  SceLibKernel:
    nid: 0xCA94D18E
    libraries:
      SceLibKernel:
        nid: 0xCA94D18E
        functions:
          sceKernelGetThreadId: 0x25A118A4
        variables:
          SceKernelStackGuard: 0x3E5A5A5A
"""

def main():
    if len(sys.argv) < 2:
        print("Usage: test_libs_gen.py <path-to-vita-libs-gen>")
        sys.exit(1)

    libs_gen = sys.argv[1]

    with tempfile.TemporaryDirectory() as tmpdir:
        yml_path = os.path.join(tmpdir, "version.yml")
        with open(yml_path, "w") as f:
            f.write(YAML_WITH_VERSION)

        out_dir = os.path.join(tmpdir, "out")
        os.makedirs(out_dir, exist_ok=True)

        res = subprocess.run([libs_gen, yml_path, out_dir], capture_output=True, text=True)
        if res.returncode != 0:
            print("Failed vita-libs-gen on a yml with a root-level 'version' key:", res.stderr)
            sys.exit(1)

        assert "unknow tag 'version'" not in res.stderr, \
            f"Regression: root-level 'version' key hit the unknown-tag warning path again (#95): {res.stderr!r}"

        files = os.listdir(out_dir)
        assert len(files) > 0, "No output files generated from a yml with a 'version' key"

    print("test_libs_gen: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
