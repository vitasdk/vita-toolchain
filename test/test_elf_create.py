#!/usr/bin/env python3
import sys
import os
import struct
import subprocess
import tempfile

def inspect_velf_sections(velf_path):
    with open(velf_path, 'rb') as f:
        data = f.read()
    
    assert data[:4] == b'\x7fELF', "Invalid ELF magic in VELF"
    e_type, e_machine, e_version, e_entry, e_phoff, e_shoff, e_flags, e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx = struct.unpack('<HHIIIIIHHHHHH', data[16:52])
    
    strtab_hdr_pos = e_shoff + e_shstrndx * e_shentsize
    sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = struct.unpack('<IIIIII', data[strtab_hdr_pos:strtab_hdr_pos+24])
    shstrtab = data[sh_offset:sh_offset+sh_size]
    
    sections = {}
    for i in range(e_shnum):
        pos = e_shoff + i * e_shentsize
        sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = struct.unpack('<IIIIII', data[pos:pos+24])
        name_end = shstrtab.find(b'\0', sh_name)
        sec_name = shstrtab[sh_name:name_end].decode('latin1')
        sec_data = data[sh_offset:sh_offset+sh_size] if sh_type != 8 else b'' # 8 is SHT_NOBITS
        sections[sec_name] = {
            'addr': sh_addr,
            'size': sh_size,
            'offset': sh_offset,
            'type': sh_type,
            'data': sec_data
        }
    return sections

def compare_to_golden(generated_path, golden_path, label):
    with open(generated_path, 'rb') as f:
        gen_data = f.read()
    with open(golden_path, 'rb') as f:
        golden_data = f.read()

    if gen_data == golden_data:
        return

    gen_secs = inspect_velf_sections(generated_path)
    golden_secs = inspect_velf_sections(golden_path)
    gen_names = list(gen_secs.keys())
    golden_names = list(golden_secs.keys())

    lines = [f"{label}: generated VELF does not match golden fixture {golden_path}"]
    if gen_names != golden_names:
        lines.append(f"  section order differs:\n    golden:    {golden_names}\n    generated: {gen_names}")
    else:
        for name in golden_names:
            g, n = golden_secs[name], gen_secs[name]
            if g['offset'] != n['offset'] or g['size'] != n['size']:
                lines.append(
                    f"  {name}: golden offset=0x{g['offset']:x} size=0x{g['size']:x}"
                    f"  generated offset=0x{n['offset']:x} size=0x{n['size']:x}"
                )
    if len(gen_data) != len(golden_data):
        lines.append(f"  file size differs: golden={len(golden_data)} generated={len(gen_data)}")

    diff_offset = next((i for i in range(min(len(gen_data), len(golden_data))) if gen_data[i] != golden_data[i]), None)
    if diff_offset is not None:
        containing = next(
            (name for name, s in golden_secs.items() if s['offset'] <= diff_offset < s['offset'] + s['size']),
            "(no known section / in ELF/program headers)"
        )
        lines.append(f"  first differing byte at file offset 0x{diff_offset:x}, inside section {containing}")

    lines.append("  If this is an intentional layout change, regenerate with: test/regen_golden.sh <path-to-vita-elf-create>")
    raise AssertionError("\n".join(lines))

def main():
    if len(sys.argv) < 2:
        print("Usage: test_elf_create.py <path-to-vita-elf-create>")
        sys.exit(1)
        
    elf_create = sys.argv[1]
    fixtures_dir = os.path.join(os.path.dirname(__file__), "fixtures")
    sample_elf = os.path.join(fixtures_dir, "sample.elf")
    sample_exidx_elf = os.path.join(fixtures_dir, "sample_exidx.elf")
    
    with tempfile.TemporaryDirectory() as tmpdir:
        # Test 1: Standard sample.elf conversion
        velf1 = os.path.join(tmpdir, "sample.velf")
        res1 = subprocess.run([elf_create, sample_elf, velf1], capture_output=True, text=True)
        if res1.returncode != 0:
            print("Failed vita-elf-create on sample.elf:", res1.stderr)
            sys.exit(1)
            
        secs1 = inspect_velf_sections(velf1)
        assert ".sceModuleInfo.rodata" in secs1, "Missing .sceModuleInfo.rodata in generated VELF"
        assert ".sceLib.ent" in secs1, "Missing .sceLib.ent in generated VELF"
        assert ".sceLib.stubs" in secs1, "Missing .sceLib.stubs in generated VELF"
        assert ".sceFNID.rodata" in secs1, "Missing .sceFNID.rodata in generated VELF"
        assert ".sceVNID.rodata" in secs1, "Missing .sceVNID.rodata in generated VELF"
        
        # Test 2: Unwind and Exception tables (.ARM.exidx and .ARM.extab - PR #281)
        velf2 = os.path.join(tmpdir, "sample_exidx.velf")
        res2 = subprocess.run([elf_create, "-n", sample_exidx_elf, velf2], capture_output=True, text=True)
        if res2.returncode != 0:
            print("Failed vita-elf-create on sample_exidx.elf:", res2.stderr)
            sys.exit(1)
            
        secs2 = inspect_velf_sections(velf2)
        assert ".sceModuleInfo.rodata" in secs2, "Missing .sceModuleInfo.rodata"
        mod_info_data = secs2[".sceModuleInfo.rodata"]["data"]
        exidx_top, exidx_end, extab_top, extab_end = struct.unpack('<IIII', mod_info_data[0x4C:0x5C])
        
        # In sample_exidx.elf, extab is at offset 0x8 (size 0xC) and exidx is at offset 0x14 (size 0x10)
        assert extab_top == 0x8, f"Expected extab_top 0x8, got {hex(extab_top)}"
        assert extab_end == 0x14, f"Expected extab_end 0x14, got {hex(extab_end)}"
        assert exidx_top == 0x14, f"Expected exidx_top 0x14, got {hex(exidx_top)}"
        assert exidx_end == 0x24, f"Expected exidx_end 0x24, got {hex(exidx_end)}"

        # Test 3: Golden-master layout regression check (#47).
        # Byte-exact comparison against checked-in reference VELFs, so a layout/alignment
        # regression is caught even when the section presence/value checks above still pass.
        golden1 = os.path.join(fixtures_dir, "sample.velf")
        compare_to_golden(velf1, golden1, "sample.elf")

        golden2 = os.path.join(fixtures_dir, "sample_exidx.velf")
        compare_to_golden(velf2, golden2, "sample_exidx.elf")

    print("test_elf_create: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
