#!/usr/bin/env python3
import sys
import os
import zipfile
import subprocess
import tempfile

def main():
    if len(sys.argv) < 2:
        print("Usage: test_pack_vpk.py <path-to-vita-pack-vpk>")
        sys.exit(1)
        
    pack_vpk = sys.argv[1]
    
    with tempfile.TemporaryDirectory() as tmpdir:
        # Create dummy sfo, eboot, and assets
        sfo_path = os.path.join(tmpdir, "param.sfo")
        eboot_path = os.path.join(tmpdir, "eboot.bin")
        asset_path = os.path.join(tmpdir, "icon0.png")
        extra_dir = os.path.join(tmpdir, "assets")
        os.makedirs(extra_dir, exist_ok=True)
        extra_file = os.path.join(extra_dir, "config.txt")
        
        with open(sfo_path, "wb") as f:
            f.write(b"MOCK_SFO_DATA")
        with open(eboot_path, "wb") as f:
            f.write(b"MOCK_EBOOT_DATA")
        with open(asset_path, "wb") as f:
            f.write(b"MOCK_ICON_PNG")
        with open(extra_file, "wb") as f:
            f.write(b"CONFIG_DATA")
            
        vpk_path = os.path.join(tmpdir, "test.vpk")
        
        # Test 1: Valid VPK generation
        cmd = [
            pack_vpk,
            "-s", sfo_path,
            "-b", eboot_path,
            "-a", f"{asset_path}=sce_sys/icon0.png",
            "-a", f"{extra_dir}=assets",
            vpk_path
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print("Failed to run vita-pack-vpk:", res.stderr)
            sys.exit(1)
            
        assert os.path.exists(vpk_path), "VPK file was not created"
        
        with zipfile.ZipFile(vpk_path, "r") as z:
            names = z.namelist()
            assert "sce_sys/param.sfo" in names, "Missing param.sfo in VPK"
            assert "eboot.bin" in names, "Missing eboot.bin in VPK"
            assert "sce_sys/icon0.png" in names, "Missing icon0.png in VPK"
            assert "assets/config.txt" in names, "Missing directory entry in VPK"
            assert z.read("eboot.bin") == b"MOCK_EBOOT_DATA"
            assert z.read("sce_sys/param.sfo") == b"MOCK_SFO_DATA"
            assert z.read("assets/config.txt") == b"CONFIG_DATA"
            
        # Test 2: Error reporting on missing -a file (Issue #287)
        nonexistent = os.path.join(tmpdir, "does_not_exist.bin")
        vpk_fail = os.path.join(tmpdir, "fail.vpk")
        cmd_fail = [
            pack_vpk,
            "-s", sfo_path,
            "-b", eboot_path,
            "-a", f"{nonexistent}=does_not_exist.bin",
            vpk_fail
        ]
        res_fail = subprocess.run(cmd_fail, capture_output=True, text=True)
        assert res_fail.returncode != 0, "vita-pack-vpk should fail on missing file"
        assert "Error: cannot stat" in res_fail.stderr or "Error: failed to add" in res_fail.stderr, \
            f"Expected error message on stderr, got:\n{res_fail.stderr}"
            
    print("test_pack_vpk: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
