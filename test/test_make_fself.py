#!/usr/bin/env python3
import sys
import os
import struct
import subprocess
import tempfile

def main():
    if len(sys.argv) < 3:
        print("Usage: test_make_fself.py <path-to-vita-make-fself> <path-to-psp2rela>")
        sys.exit(1)
        
    make_fself = sys.argv[1]
    psp2rela = sys.argv[2]
    fixtures_dir = os.path.join(os.path.dirname(__file__), "fixtures")
    sample_velf = os.path.join(fixtures_dir, "sample.velf")
    
    with tempfile.TemporaryDirectory() as tmpdir:
        # Test 1: Generate uncompressed FSELF
        fself_path = os.path.join(tmpdir, "eboot.bin")
        res1 = subprocess.run([make_fself, sample_velf, fself_path], capture_output=True, text=True)
        if res1.returncode != 0:
            print("Failed vita-make-fself:", res1.stderr)
            sys.exit(1)
            
        with open(fself_path, "rb") as f:
            fself_data = f.read()
            
        assert len(fself_data) > 0x80, "FSELF file too small"
        magic, version = struct.unpack('<II', fself_data[:8])
        assert magic == 0x00454353, f"Expected SCE\\0 magic 0x00454353, got {hex(magic)}"
        
        # Test 2: Generate compressed FSELF (-c)
        fself_c_path = os.path.join(tmpdir, "eboot_c.bin")
        res2 = subprocess.run([make_fself, "-c", sample_velf, fself_c_path], capture_output=True, text=True)
        if res2.returncode != 0:
            print("Failed vita-make-fself with compression:", res2.stderr)
            sys.exit(1)
            
        with open(fself_c_path, "rb") as f:
            fself_c_data = f.read()
        assert fself_c_data[:4] == b'SCE\0'
        
        # Test 3: Relocation converter with psp2rela
        fself_rela_out = os.path.join(tmpdir, "eboot_rela.bin")
        res3 = subprocess.run([psp2rela, f"-src={fself_path}", f"-dst={fself_rela_out}"], capture_output=True, text=True)
        if res3.returncode != 0:
            print("Failed psp2rela on generated fself:", res3.stderr)
            sys.exit(1)
        assert os.path.exists(fself_rela_out), "psp2rela output was not created"
            
    print("test_make_fself: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
