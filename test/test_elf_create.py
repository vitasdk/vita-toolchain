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


VITASDK_NOTE = struct.pack('<III8sI', 8, 4, 1, b'VitaSDK\0', 1)


def add_vitasdk_note(source_path, output_path, note=VITASDK_NOTE, section_type=7, flags=0):
    with open(source_path, 'rb') as f:
        data = bytearray(f.read())
    shoff = struct.unpack_from('<I', data, 32)[0]
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', data, 46)
    assert shentsize == 40
    headers = bytearray(data[shoff:shoff + shnum * shentsize])
    str_offset, str_size = struct.unpack_from('<II', headers, shstrndx * shentsize + 16)
    strings = data[str_offset:str_offset + str_size] + b'.note.vitasdk\0'
    new_str_offset = len(data)
    data.extend(strings)
    struct.pack_into('<II', headers, shstrndx * shentsize + 16, new_str_offset, len(strings))
    data.extend(b'\0' * (-len(data) % 4))
    note_offset = len(data)
    data.extend(note)
    data.extend(b'\0' * (-len(data) % 4))
    struct.pack_into('<I', data, 32, len(data))
    struct.pack_into('<H', data, 48, shnum + 1)
    data.extend(headers)
    data.extend(struct.pack('<10I', str_size, section_type, flags, 0, note_offset,
                            len(note), 0, 0, 4, 0))
    with open(output_path, 'wb') as f:
        f.write(data)


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


def assert_empty_imports(velf):
    sections = inspect_velf_sections(velf)
    module_info = sections['.sceModuleInfo.rodata']['data']
    export_top, export_end, import_top, import_end = struct.unpack_from('<IIII', module_info, 0x24)
    assert export_end > export_top, "Import-free module lost its exports"
    assert import_top == import_end, "Import-free module has a nonempty import table"
    for section in ('.sceLib.stubs', '.sceFNID.rodata', '.sceVNID.rodata',
                    '.sceImport.rodata', '.sceFStub.rodata', '.sceVStub.rodata'):
        assert sections.get(section, {}).get('size', 0) == 0, f"Unexpected import data in {section}"
    return sections


def test_empty_imports(elf_create, sample_elf, tmpdir):
    marked_elf = os.path.join(tmpdir, 'marked_exidx.elf')
    add_vitasdk_note(sample_elf, marked_elf)
    sample_elf = marked_elf
    assert not any(name.startswith('.vitalink.') for name in inspect_velf_sections(sample_elf)), \
        "Empty-import regression fixture must not contain import stubs"

    config = os.path.join(tmpdir, 'no_imports.yml')
    with open(config, 'w') as f:
        f.write('''no_imports:
  process_image: false
  imagemodule: false
  main:
    start: _start
  libraries:
    no_imports:
      functions:
        - __gxx_personality_v0
''')

    modes = [
        ('default', []),
        ('config', ['-e', config]),
        ('entrypoint', ['-m', '_start,,']),
        ('generated', ['-g', os.path.join(tmpdir, 'generated.yml')]),
    ]
    for name, flags in modes:
        velf = os.path.join(tmpdir, f'no_imports_{name}.velf')
        result = subprocess.run([elf_create, *flags, sample_elf, velf], capture_output=True, text=True)
        assert result.returncode == 0, f"Import-free conversion ({name}) failed: {result.stderr}"

        assert_empty_imports(velf)

        legacy_velf = os.path.join(tmpdir, f'no_imports_{name}_legacy.velf')
        result = subprocess.run([elf_create, '-n', *flags, sample_elf, legacy_velf],
                                capture_output=True, text=True)
        assert result.returncode == 0, f"Legacy -n conversion ({name}) failed: {result.stderr}"
        with open(velf, 'rb') as f, open(legacy_velf, 'rb') as legacy:
            assert f.read() == legacy.read(), f"Legacy -n changed the output ({name})"

    with open(sample_elf, 'rb') as f:
        non_arm = bytearray(f.read())
    struct.pack_into('<H', non_arm, 18, 3)  # EM_386
    non_arm_elf = os.path.join(tmpdir, 'non_arm.elf')
    with open(non_arm_elf, 'wb') as f:
        f.write(non_arm)
    result = subprocess.run([elf_create, non_arm_elf, os.path.join(tmpdir, 'non_arm.velf')],
                            capture_output=True, text=True)
    assert result.returncode != 0 and 'not an ARM binary' in result.stderr, \
        "Allowing empty imports must not bypass ARM validation"


def test_import_free_plugin(elf_create, fixtures_dir, tmpdir):
    sample_elf = os.path.join(tmpdir, 'marked_plugin.elf')
    add_vitasdk_note(os.path.join(fixtures_dir, 'sample_no_imports.elf'), sample_elf)
    config = os.path.join(fixtures_dir, 'sample_no_imports.yml')
    source_sections = inspect_velf_sections(sample_elf)
    assert not any(name.startswith('.vitalink.') for name in source_sections), \
        "Plugin fixture must not contain import stubs"
    assert not any(section['type'] in (4, 9) for section in source_sections.values()), \
        "Plugin fixture must not contain REL or RELA sections"

    outputs = []
    for name, flags in (('default', []), ('legacy', ['-n'])):
        velf = os.path.join(tmpdir, f'plugin_{name}.velf')
        result = subprocess.run([elf_create, *flags, '-e', config, sample_elf, velf],
                                capture_output=True, text=True)
        assert result.returncode == 0, f"Zero-relocation plugin ({name}) failed: {result.stderr}"
        assert 'No relocation sections' in result.stderr and '-Wl,-q' in result.stderr, \
            "Keep the diagnostic for builds that omitted relocation information"

        sections = assert_empty_imports(velf)
        assert sections['.text']['data'] == source_sections['.text']['data'], "Plugin code changed"
        assert sections['.sce.rel']['size'] > 0, "Missing generated export relocations"
        exports = sections['.sceLib.ent']['data']
        assert len(exports) == 0x40, "Missing plugin export library"
        assert struct.unpack_from('<H', exports, 6)[0] == 3, "Missing module entrypoint exports"
        assert struct.unpack_from('<H', exports, 0x26)[0] == 1, "Missing simple_greeting export"
        start, stop = struct.unpack_from('<II', sections['.sceModuleInfo.rodata']['data'], 0x44)
        assert start != 0xffffffff and stop != 0xffffffff, "Missing module start/stop offsets"
        with open(velf, 'rb') as f:
            outputs.append(f.read())
    assert outputs[0] == outputs[1], "Legacy -n changed the plugin output"

    with open(sample_elf, 'rb') as f:
        no_symbols = bytearray(f.read())
    shoff = struct.unpack_from('<I', no_symbols, 32)[0]
    shentsize, shnum = struct.unpack_from('<HH', no_symbols, 46)
    for i in range(shnum):
        type_offset = shoff + i * shentsize + 4
        if struct.unpack_from('<I', no_symbols, type_offset)[0] == 2:
            struct.pack_into('<I', no_symbols, type_offset, 1)
    no_symbols_elf = os.path.join(tmpdir, 'no_symbols.elf')
    with open(no_symbols_elf, 'wb') as f:
        f.write(no_symbols)
    result = subprocess.run([elf_create, '-e', config, no_symbols_elf,
                             os.path.join(tmpdir, 'no_symbols.velf')], capture_output=True, text=True)
    assert result.returncode != 0 and 'No symbol table' in result.stderr, \
        "Allowing zero relocations must not bypass symbol-table validation"


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
        res2 = subprocess.run([elf_create, '-n', sample_exidx_elf, velf2], capture_output=True, text=True)
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


        # Test 4: A symbol can legally denote one-past-the-end of its section.
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

        test_empty_imports(elf_create, sample_exidx_elf, tmpdir)
        test_import_free_plugin(elf_create, fixtures_dir, tmpdir)

    print("test_elf_create: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
