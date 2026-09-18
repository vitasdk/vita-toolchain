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


def make_segment_end_reloc_fixture(source_path, output_path):
    """Patch sample.elf so one ABS32 relocation targets a symbol at PT_LOAD end."""
    with open(source_path, 'rb') as f:
        data = bytearray(f.read())

    assert data[:4] == b'\x7fELF', "Invalid ELF fixture"
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', data, 0)
    e_phoff, e_shoff = header[5], header[6]
    e_phentsize, e_phnum = header[9], header[10]
    e_shentsize, e_shnum, e_shstrndx = header[11], header[12], header[13]

    program_headers = []
    load_segments = []
    for i in range(e_phnum):
        ph = struct.unpack_from('<IIIIIIII', data, e_phoff + i * e_phentsize)
        p_type, p_offset, p_vaddr, _, p_filesz, p_memsz, _, _ = ph
        program_headers.append({
            'type': p_type,
            'offset': p_offset,
            'vaddr': p_vaddr,
            'filesz': p_filesz,
            'memsz': p_memsz,
        })
        if p_type == 1:  # PT_LOAD
            load_segments.append(program_headers[-1])

    sections = []
    for i in range(e_shnum):
        sh = struct.unpack_from('<IIIIIIIIII', data, e_shoff + i * e_shentsize)
        sections.append({
            'name_off': sh[0],
            'type': sh[1],
            'flags': sh[2],
            'addr': sh[3],
            'offset': sh[4],
            'size': sh[5],
            'link': sh[6],
            'info': sh[7],
            'entsize': sh[9],
        })

    shstr = sections[e_shstrndx]
    shstr_data = data[shstr['offset']:shstr['offset'] + shstr['size']]

    def section_name(section):
        start = section['name_off']
        end = shstr_data.find(b'\0', start)
        return shstr_data[start:end].decode('latin1')

    bss_ndx = next(i for i, section in enumerate(sections) if section_name(section) == '.bss')
    bss = sections[bss_ndx]
    bss_end = bss['addr'] + bss['size']

    symseg = None
    for i, segment in enumerate(load_segments):
        segment_end = segment['vaddr'] + segment['memsz']
        if bss['addr'] >= segment['vaddr'] and bss_end == segment_end:
            symseg = i
            symoff = bss_end - segment['vaddr']
            break
    assert symseg is not None, ".bss must end exactly at a PT_LOAD boundary for this regression"

    symtab_ndx = next(i for i, section in enumerate(sections) if section['type'] == 2)  # SHT_SYMTAB
    symtab = sections[symtab_ndx]
    strtab = sections[symtab['link']]
    strtab_data = data[strtab['offset']:strtab['offset'] + strtab['size']]
    sym_entsize = symtab['entsize'] or 16

    end_sym_ndx = None
    for i in range(symtab['size'] // sym_entsize):
        sym_off = symtab['offset'] + i * sym_entsize
        st_name = struct.unpack_from('<I', data, sym_off)[0]
        name_end = strtab_data.find(b'\0', st_name)
        name = strtab_data[st_name:name_end].decode('latin1')
        if name == '__bss_end__':
            end_sym_ndx = i
            struct.pack_into('<I', data, sym_off + 4, bss_end)
            struct.pack_into('<H', data, sym_off + 14, bss_ndx)
            break
    assert end_sym_ndx is not None, "sample.elf is missing __bss_end__"

    rel = next(section for section in sections if section['type'] == 9 and section['size'] >= 8)  # SHT_REL
    target = sections[rel['info']]
    assert target['size'] >= 4 and (target['flags'] & 0x2), "REL target must be allocatable"
    target_vaddr = target['addr']

    datseg = None
    target_file_offset = None
    for i, segment in enumerate(load_segments):
        if (target_vaddr >= segment['vaddr'] and
                target_vaddr + 4 <= segment['vaddr'] + segment['filesz']):
            datseg = i
            datoff = target_vaddr - segment['vaddr']
            target_file_offset = segment['offset'] + datoff
            break
    assert datseg is not None, "REL target is not file-backed by a PT_LOAD segment"

    # Make the first relocation an ABS32 reference with addend zero. Before the
    # fix, vita-elf-create cannot assign __bss_end__ to a segment because its
    # value is exactly p_vaddr + p_memsz, so this relocation is silently lost.
    struct.pack_into('<I', data, target_file_offset, bss_end)
    struct.pack_into('<II', data, rel['offset'], target_vaddr, (end_sym_ndx << 8) | 2)

    with open(output_path, 'wb') as f:
        f.write(data)

    return datseg, datoff, symseg, symoff

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
        # Use a deterministic input basename so the default module name is
        # identical on hosts with '/' and '\\' path separators.
        res1 = subprocess.run([elf_create, "sample.elf", velf1],
                              cwd=fixtures_dir, capture_output=True, text=True)
        if res1.returncode != 0:
            print("Failed vita-elf-create on sample.elf:", res1.stderr)
            sys.exit(1)
            
        secs1 = inspect_velf_sections(velf1)
        assert ".sceModuleInfo.rodata" in secs1, "Missing .sceModuleInfo.rodata in generated VELF"
        assert ".sceLib.ent" in secs1, "Missing .sceLib.ent in generated VELF"
        assert ".sceLib.stubs" in secs1, "Missing .sceLib.stubs in generated VELF"
        assert ".sceFNID.rodata" in secs1, "Missing .sceFNID.rodata in generated VELF"
        assert ".sceVNID.rodata" in secs1, "Missing .sceVNID.rodata in generated VELF"

        # The export/import table headers start with a 1-byte struct size
        # followed by a reserved zero byte (Issue #114) — not a 16-bit size.
        # For the values emitted here the bytes are the same either way, so
        # this pins the on-disk format for both interpretations.
        ent_data = secs1[".sceLib.ent"]["data"]
        assert ent_data[0] == 0x20 and ent_data[1] == 0x00, \
            f"Regression (#114): sce_module_exports must start 0x20,0x00, got {ent_data[0]:#x},{ent_data[1]:#x}"
        stub_data = secs1[".sceLib.stubs"]["data"]
        assert stub_data[0] in (0x24, 0x34) and stub_data[1] == 0x00, \
            f"Regression (#114): sce_module_imports must start 0x24/0x34,0x00, got {stub_data[0]:#x},{stub_data[1]:#x}"
        
        # Test 2: Unwind and Exception tables (.ARM.exidx and .ARM.extab - PR #281)
        velf2 = os.path.join(tmpdir, "sample_exidx.velf")
        res2 = subprocess.run([elf_create, "-n", "sample_exidx.elf", velf2],
                              cwd=fixtures_dir, capture_output=True, text=True)
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

        # Golden-master layout regression check (#47). Keep the comparison
        # byte-exact; the deterministic basenames above make it host-independent.
        golden1 = os.path.join(fixtures_dir, "sample.velf")
        compare_to_golden(velf1, golden1, "sample.elf")

        golden2 = os.path.join(fixtures_dir, "sample_exidx.velf")
        compare_to_golden(velf2, golden2, "sample_exidx.elf")

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

        # Test 4: module attributes from the export yml reach the velf module
        # info (Issue #200). attributes lives at offset 0 of sce_module_info,
        # right before version (0x0101).
        attr_yml = os.path.join(tmpdir, "attr.yml")
        with open(attr_yml, "w") as f:
            f.write(
                "SampleAttr:\n"
                "  attributes: 0x1234\n"
                "  version:\n"
                "    major: 1\n"
                "    minor: 1\n"
                "  nid: 0xDEADBEEF\n"
            )
        velf4 = os.path.join(tmpdir, "sample_attr.velf")
        res4 = subprocess.run([elf_create, "-e", attr_yml, sample_elf, velf4], capture_output=True, text=True)
        if res4.returncode != 0:
            print("Failed vita-elf-create with export yml:", res4.stderr)
            sys.exit(1)

        secs4 = inspect_velf_sections(velf4)
        mod_info4 = secs4[".sceModuleInfo.rodata"]["data"]
        attributes, version = struct.unpack_from('<HH', mod_info4, 0)
        assert version == 0x0101, f"Expected module version 0x0101, got {hex(version)}"
        assert attributes == 0x1234, f"Regression (#200): expected module attributes 0x1234, got {hex(attributes)}"

        # Test 5: A symbol can legally denote one-past-the-end of its section.
        # If that section also ends at a PT_LOAD boundary, vita-elf-create must
        # still emit the relocation using an offset equal to p_memsz.
        segment_end_elf = os.path.join(tmpdir, "sample_segment_end.elf")
        expected_datseg, expected_datoff, expected_symseg, expected_symoff = \
            make_segment_end_reloc_fixture(sample_elf, segment_end_elf)
        velf4 = os.path.join(tmpdir, "sample_segment_end.velf")
        res4 = subprocess.run([elf_create, segment_end_elf, velf4], capture_output=True, text=True)
        if res4.returncode != 0:
            print("Failed vita-elf-create on segment-end relocation fixture:", res4.stderr)
            sys.exit(1)

        secs4 = inspect_velf_sections(velf4)
        assert ".sce.rel" in secs4, "Missing .sce.rel in segment-end regression VELF"
        rel_data = secs4[".sce.rel"]["data"]
        assert len(rel_data) % 12 == 0, "Unexpected .sce.rel entry size"

        found_segment_end_reloc = False
        for off in range(0, len(rel_data), 12):
            word1, word2, word3 = struct.unpack_from('<III', rel_data, off)
            symseg = (word1 >> 4) & 0xF
            code = (word1 >> 8) & 0xFF
            datseg = (word1 >> 16) & 0xF
            if (code == 2 and datseg == expected_datseg and word3 == expected_datoff and
                    symseg == expected_symseg and word2 == expected_symoff):
                found_segment_end_reloc = True
                break

        assert found_segment_end_reloc, \
            "Regression: relocation against a symbol at the exact end of a PT_LOAD segment was dropped"

    print("test_elf_create: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
