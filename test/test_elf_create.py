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

        # Test 3: MOVW/MOVT relocations against an imported stub symbol survive
        # into the SCE relocation table (Issue #225). The fixture was built with
        # vitasdk from an inline-asm sample loading the address of
        # scePowerIsPowerOnline via movw/movt instead of calling it directly,
        # matching the original report. See fixtures/sample_movwmovt.c.
        sample_movwmovt_elf = os.path.join(fixtures_dir, "sample_movwmovt.elf")
        velf3 = os.path.join(tmpdir, "sample_movwmovt.velf")
        res3 = subprocess.run([elf_create, sample_movwmovt_elf, velf3], capture_output=True, text=True)
        if res3.returncode != 0:
            print("Failed vita-elf-create on sample_movwmovt.elf:", res3.stderr)
            sys.exit(1)

        secs3 = inspect_velf_sections(velf3)
        assert ".sce.rel" in secs3, "Missing .sce.rel in generated VELF"
        rel_data = secs3[".sce.rel"]["data"]
        assert len(rel_data) % 12 == 0, "Unexpected .sce.rel entry size"

        # R_ARM_THM_MOVW_ABS_NC (47) / R_ARM_THM_MOVT_ABS (48) at the exact
        # offset/addend of the scePowerIsPowerOnline reference, extracted via
        # `arm-vita-eabi-readelf -r sample_movwmovt.elf` against the source
        # (offsets are segment-relative, so 0x810001b8/0x810001bc minus the
        # 0x81000000 segment base).
        R_ARM_THM_MOVW_ABS_NC = 47
        R_ARM_THM_MOVT_ABS = 48
        EXPECTED_MOVW_OFFSET = 0x1b8
        EXPECTED_MOVT_OFFSET = 0x1bc
        EXPECTED_SYM_ADDEND = 0x3330

        found_movw = found_movt = False
        for off in range(0, len(rel_data), 12):
            word1, word2, word3 = struct.unpack_from('<III', rel_data, off)
            code = (word1 >> 8) & 0xFF
            addend = word2
            r_offset = word3
            if code == R_ARM_THM_MOVW_ABS_NC and r_offset == EXPECTED_MOVW_OFFSET and addend == EXPECTED_SYM_ADDEND:
                found_movw = True
            if code == R_ARM_THM_MOVT_ABS and r_offset == EXPECTED_MOVT_OFFSET and addend == EXPECTED_SYM_ADDEND:
                found_movt = True

        assert found_movw, "Regression (#225): MOVW relocation against scePowerIsPowerOnline missing from .sce.rel"
        assert found_movt, "Regression (#225): MOVT relocation against scePowerIsPowerOnline missing from .sce.rel"

        # Test 4: PIC binaries (Issue #274). R_ARM_BASE_PREL (25) used to be
        # rejected with "Invalid relocation type 25"; it is now encoded as
        # R_ARM_REL32 against _GLOBAL_OFFSET_TABLE_, R_ARM_GOT_BREL is a
        # link-time constant, and the GOT slots (which a static -Wl,-q link
        # leaves without relocations) get synthesized ABS32 entries.
        # Expected offsets extracted from the fixture with
        # `arm-vita-eabi-readelf -r/-S sample_pic.elf`: the BASE_PREL literal
        # at 0x810001c4 (text segment + 0x1c4); .got at 0x8101000c with its
        # one used slot at +0xc (data segment + 0x18) holding the address of
        # shared_state (0x81010024, data segment + 0x24).
        sample_pic_elf = os.path.join(fixtures_dir, "sample_pic.elf")
        velf4 = os.path.join(tmpdir, "sample_pic.velf")
        res4 = subprocess.run([elf_create, sample_pic_elf, velf4], capture_output=True, text=True)
        if res4.returncode != 0:
            print("Regression (#274): vita-elf-create failed on a PIC binary:", res4.stderr)
            sys.exit(1)

        secs4 = inspect_velf_sections(velf4)
        rel4 = secs4[".sce.rel"]["data"]
        R_ARM_ABS32 = 2
        R_ARM_REL32 = 3
        got_base_rel32 = got_slot_abs32 = False
        for off in range(0, len(rel4), 12):
            w1, w2, w3 = struct.unpack_from('<III', rel4, off)
            code = (w1 >> 8) & 0xFF
            if code == R_ARM_REL32 and w3 == 0x1c4:
                got_base_rel32 = True
            if code == R_ARM_ABS32 and w3 == 0x18 and w2 == 0x24:
                got_slot_abs32 = True
        assert got_base_rel32, "Regression (#274): missing REL32 entry for the R_ARM_BASE_PREL literal"
        assert got_slot_abs32, "Regression (#274): missing synthesized ABS32 entry for the GOT slot"

    print("test_elf_create: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
