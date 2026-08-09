#!/usr/bin/env python3
import sys
import os
import subprocess
import tempfile

YAML_WITH_FIRMWARE = """
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

YAML_WITHOUT_FIRMWARE = """
version: 2
modules:
  SceTestModule:
    nid: 0x12345678
    libraries:
      SceTestModule:
        nid: 0x12345678
        functions:
          sceTestFunction: 0xDEADBEEF
"""

def main():
    if len(sys.argv) < 2:
        print("Usage: test_libs_gen_2.py <path-to-vita-libs-gen-2>")
        sys.exit(1)
        
    libs_gen = sys.argv[1]
    
    with tempfile.TemporaryDirectory() as tmpdir:
        # Test 1: YAML with firmware
        yml1_path = os.path.join(tmpdir, "fw.yml")
        with open(yml1_path, "w") as f:
            f.write(YAML_WITH_FIRMWARE)
            
        out1_dir = os.path.join(tmpdir, "out1")
        os.makedirs(out1_dir, exist_ok=True)
        
        res1 = subprocess.run([libs_gen, f"-yml={yml1_path}", f"-output={out1_dir}"], capture_output=True, text=True)
        if res1.returncode != 0:
            print("Failed libs-gen-2 with firmware:", res1.stderr)
            sys.exit(1)
            
        # Verify generated Makefile or assembly stubs
        files1 = os.listdir(out1_dir)
        assert len(files1) > 0, "No output files generated from YAML with firmware"
        
        # Test 2: YAML without firmware (Issue #244 regression test)
        yml2_path = os.path.join(tmpdir, "nofw.yml")
        with open(yml2_path, "w") as f:
            f.write(YAML_WITHOUT_FIRMWARE)
            
        out2_dir = os.path.join(tmpdir, "out2")
        os.makedirs(out2_dir, exist_ok=True)
        
        res2 = subprocess.run([libs_gen, f"-yml={yml2_path}", f"-output={out2_dir}"], capture_output=True, text=True)
        if res2.returncode != 0:
            print("Failed libs-gen-2 without firmware (Issue #244):", res2.stderr)
            sys.exit(1)
            
        files2 = os.listdir(out2_dir)
        assert len(files2) > 0, "No output files generated from YAML without firmware"
        
    print("test_libs_gen_2: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
