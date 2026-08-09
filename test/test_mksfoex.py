#!/usr/bin/env python3
import sys
import os
import struct
import subprocess
import tempfile

def read_sfo(data):
    if len(data) < 20:
        raise ValueError("SFO file too small")
    magic, version, keyofs, valofs, count = struct.unpack('<IIIII', data[:20])
    if magic != 0x46535000: # '\0PSF'
        raise ValueError(f"Invalid magic: {hex(magic)}")
    
    entries = []
    pos = 20
    for _ in range(count):
        nameofs, alignment, type_id, valsize, totalsize, dataofs = struct.unpack('<HBBIII', data[pos:pos+16])
        pos += 16
        # Read name
        name_end = data.find(b'\0', keyofs + nameofs)
        name = data[keyofs + nameofs:name_end].decode('utf-8')
        # Read value
        val_raw = data[valofs + dataofs:valofs + dataofs + valsize]
        if type_id == 2: # PSF_TYPE_STR (utf-8 string)
            val = val_raw.rstrip(b'\0').decode('utf-8')
        elif type_id == 4: # PSF_TYPE_VAL (uint32)
            val = struct.unpack('<I', val_raw[:4])[0]
        else:
            val = val_raw
        entries.append((name, val))
    return entries

def main():
    if len(sys.argv) < 2:
        print("Usage: test_mksfoex.py <path-to-vita-mksfoex>")
        sys.exit(1)
    
    mksfoex = sys.argv[1]
    
    with tempfile.TemporaryDirectory() as tmpdir:
        # Test 1: Default SFO creation
        sfo1 = os.path.join(tmpdir, "default.sfo")
        res = subprocess.run([mksfoex, "-s", "TITLE_ID=ABCD12345", "TestTitle", sfo1], capture_output=True, text=True)
        if res.returncode != 0:
            print("Failed to run vita-mksfoex:", res.stderr)
            sys.exit(1)
        
        with open(sfo1, "rb") as f:
            entries1 = read_sfo(f.read())
        
        keys1 = [e[0] for e in entries1]
        assert "TITLE" in keys1, "Missing TITLE in default sfo"
        assert "STITLE" in keys1, "Missing STITLE in default sfo"
        assert "TITLE_ID" in keys1, "Missing TITLE_ID in default sfo"
        
        # Verify keys are sorted
        assert keys1 == sorted(keys1), f"Keys are not sorted: {keys1}"
        
        # Verify title values
        dict1 = dict(entries1)
        assert dict1["TITLE"] == "TestTitle", f"Expected TITLE 'TestTitle', got '{dict1['TITLE']}'"
        assert dict1["STITLE"] == "TestTitle", f"Expected STITLE 'TestTitle', got '{dict1['STITLE']}'"
        assert dict1["TITLE_ID"] == "ABCD12345", f"Expected TITLE_ID 'ABCD12345', got '{dict1['TITLE_ID']}'"
        
        # Test 2: Separate TITLE and STITLE options (PR #284)
        sfo2 = os.path.join(tmpdir, "custom.sfo")
        res = subprocess.run([
            mksfoex,
            "-s", "TITLE=Long Title",
            "-s", "STITLE=Short",
            "-s", "TITLE_ID=TEST00001",
            "IgnoredDefaultTitle",
            sfo2
        ], capture_output=True, text=True)
        if res.returncode != 0:
            print("Failed custom sfo:", res.stderr)
            sys.exit(1)
            
        with open(sfo2, "rb") as f:
            dict2 = dict(read_sfo(f.read()))
        assert dict2["TITLE"] == "Long Title", f"Expected 'Long Title', got '{dict2['TITLE']}'"
        assert dict2["STITLE"] == "Short", f"Expected 'Short', got '{dict2['STITLE']}'"
        
        # Test 3: Empty flag (-e)
        sfo3 = os.path.join(tmpdir, "empty.sfo")
        res = subprocess.run([
            mksfoex,
            "-e",
            "-s", "CUSTOM_KEY=CustomVal",
            "-d", "CUSTOM_INT=42",
            sfo3
        ], capture_output=True, text=True)
        if res.returncode != 0:
            print("Failed empty sfo:", res.stderr)
            sys.exit(1)
            
        with open(sfo3, "rb") as f:
            entries3 = read_sfo(f.read())
        dict3 = dict(entries3)
        assert len(entries3) == 2, f"Expected 2 entries with -e, got {len(entries3)}"
        assert dict3["CUSTOM_KEY"] == "CustomVal"
        assert dict3["CUSTOM_INT"] == 42
        
    print("test_mksfoex: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
