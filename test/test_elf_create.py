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

# ---- Input-ELF introspection for the native TLS tests (rule 4) ----
# These parse the *source* .elf fixtures directly (program headers, symbols,
# .rel.* sections) so the expected module_info values are computed from the
# fixture itself instead of being hardcoded.

SHT_SYMTAB = 2
PT_LOAD = 1
PT_TLS = 7
R_ARM_ABS32 = 2
R_ARM_TLS_LE32 = 108

def parse_ehdr(data):
    (e_type, e_machine, e_version, e_entry, e_phoff, e_shoff, e_flags,
     e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx) = struct.unpack('<HHIIIIIHHHHHH', data[16:52])
    return dict(e_type=e_type, e_entry=e_entry, e_phoff=e_phoff, e_shoff=e_shoff,
                e_phentsize=e_phentsize, e_phnum=e_phnum, e_shentsize=e_shentsize,
                e_shnum=e_shnum, e_shstrndx=e_shstrndx)

def parse_shdrs_full(data, ehdr):
    shdrs = []
    for i in range(ehdr['e_shnum']):
        pos = ehdr['e_shoff'] + i * ehdr['e_shentsize']
        (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size,
         sh_link, sh_info, sh_addralign, sh_entsize) = struct.unpack_from('<10I', data, pos)
        shdrs.append(dict(name_off=sh_name, type=sh_type, flags=sh_flags, addr=sh_addr,
                           offset=sh_offset, size=sh_size, link=sh_link, info=sh_info,
                           addralign=sh_addralign, entsize=sh_entsize))
    shstrtab_hdr = shdrs[ehdr['e_shstrndx']]
    shstrtab = data[shstrtab_hdr['offset']:shstrtab_hdr['offset'] + shstrtab_hdr['size']]
    for s in shdrs:
        end = shstrtab.find(b'\0', s['name_off'])
        s['name'] = shstrtab[s['name_off']:end].decode('latin1')
    return shdrs

def parse_phdrs(data, ehdr):
    phdrs = []
    for i in range(ehdr['e_phnum']):
        off = ehdr['e_phoff'] + i * ehdr['e_phentsize']
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from('<8I', data, off)
        phdrs.append(dict(type=p_type, offset=p_offset, vaddr=p_vaddr, paddr=p_paddr,
                           filesz=p_filesz, memsz=p_memsz, flags=p_flags, align=p_align, _off=off))
    return phdrs

def parse_symbols(data, shdrs):
    symtab = next(s for s in shdrs if s['type'] == SHT_SYMTAB)
    strtab = shdrs[symtab['link']]
    strdata = data[strtab['offset']:strtab['offset'] + strtab['size']]
    by_name, by_index = {}, {}
    for i in range(symtab['size'] // symtab['entsize']):
        off = symtab['offset'] + i * symtab['entsize']
        st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from('<IIIBBH', data, off)
        end = strdata.find(b'\0', st_name)
        name = strdata[st_name:end].decode('latin1')
        entry = dict(name=name, value=st_value, shndx=st_shndx)
        by_index[i] = entry
        if name:
            by_name[name] = entry
    return by_name, by_index

def parse_rel_entries(data, sec):
    return [struct.unpack_from('<II', data, sec['offset'] + i * sec['entsize'])
            for i in range(sec['size'] // sec['entsize'])]

def read_source_elf(path):
    with open(path, 'rb') as f:
        data = f.read()
    ehdr = parse_ehdr(data)
    shdrs = parse_shdrs_full(data, ehdr)
    phdrs = parse_phdrs(data, ehdr)
    syms, syms_by_idx = parse_symbols(data, shdrs)
    return dict(data=data, ehdr=ehdr, shdrs=shdrs, phdrs=phdrs, syms=syms, syms_by_idx=syms_by_idx)

def segment_containing(phdrs, vaddr):
    for p in phdrs:
        if p['type'] == PT_LOAD and p['vaddr'] <= vaddr < p['vaddr'] + p['memsz']:
            return p
    return None

def load_index(phdrs, seg):
    return [p for p in phdrs if p['type'] == PT_LOAD].index(seg)

def module_info_tls_fields(velf_path):
    mod = inspect_velf_sections(velf_path)['.sceModuleInfo.rodata']['data']
    # sce_module_info_raw layout (see src/sce-elf-defs.h): tls_start/tls_filesz/tls_memsz
    # sit right after module_nid, at fixed offsets 0x38/0x3C/0x40 (struct is 0x5C bytes).
    return struct.unpack_from('<III', mod, 0x38)

def sce_rel_entries(velf_path):
    rel = inspect_velf_sections(velf_path)['.sce.rel']['data']
    out = []
    for off in range(0, len(rel), 12):
        word1, word2, word3 = struct.unpack_from('<III', rel, off)
        out.append(dict(code=(word1 >> 8) & 0xFF, symseg=(word1 >> 4) & 0xF, datseg=(word1 >> 16) & 0xF,
                         addend=word2, offset=word3))
    return out

def patch_pt_tls(src_path, mutator):
    """Copy src_path, apply mutator(data, pt_tls_phdr_offset) to its program headers, return the patched bytes."""
    with open(src_path, 'rb') as f:
        data = bytearray(f.read())
    ehdr = parse_ehdr(data)
    tls_off = None
    for i in range(ehdr['e_phnum']):
        off = ehdr['e_phoff'] + i * ehdr['e_phentsize']
        p_type, = struct.unpack_from('<I', data, off)
        if p_type == PT_TLS:
            tls_off = off
            break
    assert tls_off is not None, "fixture has no PT_TLS segment to patch"
    mutator(data, ehdr, tls_off)
    return bytes(data)

def run_convert(elf_create, elf_bytes, tmpdir, name, extra_args=()):
    elf_path = os.path.join(tmpdir, name + ".elf")
    velf_path = os.path.join(tmpdir, name + ".velf")
    with open(elf_path, 'wb') as f:
        f.write(elf_bytes)
    res = subprocess.run([elf_create, *extra_args, elf_path, velf_path], capture_output=True, text=True)
    return res, velf_path

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

        # Test 4: native TLS in a process image, .tdata + .tbss from two translation
        # units (rule 4a). .tdata carries its own R_ARM_ABS32 relocation (tls_ptr
        # holds &global_var), which must survive into .sce.rel unmangled. Expected
        # tls_start/tls_filesz/tls_memsz are computed from the fixture's own PT_TLS
        # program header, not hardcoded. See fixtures/sample_tls_two_tu_{a,b}.c.
        sample_tls_two_tu_elf = os.path.join(fixtures_dir, "sample_tls_two_tu.elf")
        src = read_source_elf(sample_tls_two_tu_elf)
        tls_phdr = next(p for p in src['phdrs'] if p['type'] == PT_TLS)
        entry_seg = segment_containing(src['phdrs'], src['ehdr']['e_entry'])
        assert entry_seg is not None, "module_start's segment not found"

        expected_tls_start = tls_phdr['vaddr'] - entry_seg['vaddr']

        velf4 = os.path.join(tmpdir, "sample_tls_two_tu.velf")
        res4 = subprocess.run([elf_create, "-n", sample_tls_two_tu_elf, velf4], capture_output=True, text=True)
        if res4.returncode != 0:
            print("Failed vita-elf-create on sample_tls_two_tu.elf:", res4.stderr)
            sys.exit(1)

        tls_start, tls_filesz, tls_memsz = module_info_tls_fields(velf4)
        assert tls_start == expected_tls_start, f"tls_start {hex(tls_start)} != expected {hex(expected_tls_start)}"
        assert tls_filesz == tls_phdr['filesz'], f"tls_filesz {tls_filesz} != expected {tls_phdr['filesz']}"
        assert tls_memsz == tls_phdr['memsz'], f"tls_memsz {tls_memsz} != expected {tls_phdr['memsz']}"

        tdata_rel_sec = next(s for s in src['shdrs'] if s['name'] == '.rel.tdata')
        found_abs32 = False
        for r_offset, r_info in parse_rel_entries(src['data'], tdata_rel_sec):
            r_sym, r_type = r_info >> 8, r_info & 0xff
            if r_type != R_ARM_ABS32:
                continue
            sym = src['syms_by_idx'][r_sym]
            sym_seg = segment_containing(src['phdrs'], sym['value'])
            expected = dict(code=R_ARM_ABS32,
                             datseg=load_index(src['phdrs'], entry_seg),
                             symseg=load_index(src['phdrs'], sym_seg),
                             offset=r_offset - entry_seg['vaddr'],
                             addend=sym['value'] - sym_seg['vaddr'])
            if expected in sce_rel_entries(velf4):
                found_abs32 = True
        assert found_abs32, "R_ARM_ABS32 relocation inside .tdata missing/incorrect in output .sce.rel"

        # Test 5: .tbss-only TLS template - p_filesz == 0, p_memsz > 0 (rule 4b).
        # Must still convert and produce a correct (non-zero) tls_start; only
        # p_memsz == 0 means "no TLS present". See fixtures/sample_tls_tbss.c.
        sample_tls_tbss_elf = os.path.join(fixtures_dir, "sample_tls_tbss.elf")
        src5 = read_source_elf(sample_tls_tbss_elf)
        tls_phdr5 = next(p for p in src5['phdrs'] if p['type'] == PT_TLS)
        assert tls_phdr5['filesz'] == 0 and tls_phdr5['memsz'] > 0, "fixture is not .tbss-only"
        entry_seg5 = segment_containing(src5['phdrs'], src5['ehdr']['e_entry'])
        expected_tls_start5 = tls_phdr5['vaddr'] - entry_seg5['vaddr']

        velf5 = os.path.join(tmpdir, "sample_tls_tbss.velf")
        res5 = subprocess.run([elf_create, "-n", sample_tls_tbss_elf, velf5], capture_output=True, text=True)
        if res5.returncode != 0:
            print("Failed vita-elf-create on sample_tls_tbss.elf:", res5.stderr)
            sys.exit(1)

        tls_start5, tls_filesz5, tls_memsz5 = module_info_tls_fields(velf5)
        assert tls_start5 == expected_tls_start5, f"tls_start {hex(tls_start5)} != expected {hex(expected_tls_start5)}"
        assert tls_filesz5 == 0, f"expected tls_filesz 0, got {tls_filesz5}"
        assert tls_memsz5 == tls_phdr5['memsz'], f"tls_memsz {tls_memsz5} != expected {tls_phdr5['memsz']}"

        # Test 6: the rule 4a fixture converted as a module (process_image: false)
        # must be rejected (rule 1). See fixtures/sample_tls_module.yml.
        sample_tls_module_yml = os.path.join(fixtures_dir, "sample_tls_module.yml")
        res6 = subprocess.run([elf_create, "-n", "-e", sample_tls_module_yml, sample_tls_two_tu_elf,
                                os.path.join(tmpdir, "sample_tls_module.velf")], capture_output=True, text=True)
        assert res6.returncode != 0, "Expected conversion to fail for a non-process-image TLS module"
        assert "process image" in res6.stderr and "not one" in res6.stderr, \
            f"Expected a 'process image ... not one' error, got: {res6.stderr}"

        # Test 7: an unsupported TLS relocation model (R_ARM_TLS_IE32, built with
        # -fPIC -ftls-model=initial-exec) must be rejected by name (rule 2). See
        # fixtures/sample_tls_ie32_{def,use}.c.
        sample_tls_ie32_elf = os.path.join(fixtures_dir, "sample_tls_ie32.elf")
        res7 = subprocess.run([elf_create, "-n", sample_tls_ie32_elf,
                                os.path.join(tmpdir, "sample_tls_ie32.velf")], capture_output=True, text=True)
        assert res7.returncode != 0, "Expected conversion to fail for an unsupported TLS relocation"
        assert "R_ARM_TLS_IE32" in res7.stderr, f"Expected the error to name R_ARM_TLS_IE32, got: {res7.stderr}"

        # Test 8: malformed PT_TLS program headers, produced by patching a copy of
        # sample_tls_two_tu.elf's program header table in Python (rule 4e). Each
        # mutation must be independently rejected.
        def mutate_filesz_gt_memsz(data, ehdr, off):
            p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from('<8I', data, off)
            struct.pack_into('<8I', data, off, p_type, p_offset, p_vaddr, p_paddr, p_memsz + 4, p_memsz, p_flags, p_align)

        def mutate_bad_align(data, ehdr, off):
            p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from('<8I', data, off)
            struct.pack_into('<8I', data, off, p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, 3)

        def mutate_duplicate_pt_tls(data, ehdr, off):
            entsize = ehdr['e_phentsize']
            tls_bytes = bytes(data[off:off + entsize])
            for i in range(ehdr['e_phnum']):
                o = ehdr['e_phoff'] + i * entsize
                p_type, = struct.unpack_from('<I', data, o)
                if p_type == PT_LOAD and o != off:
                    data[o:o + entsize] = tls_bytes
                    return
            assert False, "no second PT_LOAD phdr slot to duplicate into"

        for mutator, label, expect_in_stderr in (
            (mutate_filesz_gt_memsz, "p_filesz > p_memsz", "exceeds"),
            (mutate_bad_align, "p_align = 3 (not a power of two)", "power of two"),
            (mutate_duplicate_pt_tls, "duplicate PT_TLS segment", "more than one"),
        ):
            patched = patch_pt_tls(sample_tls_two_tu_elf, mutator)
            res8, _ = run_convert(elf_create, patched, tmpdir, "sample_tls_malformed", extra_args=("-n",))
            assert res8.returncode != 0, f"Expected conversion to fail for malformed PT_TLS: {label}"
            assert expect_in_stderr in res8.stderr, \
                f"Expected '{expect_in_stderr}' in error for malformed PT_TLS ({label}), got: {res8.stderr}"

        # Test 9: byte-identical output regression (rule 3). Converting a fixture
        # with no PT_TLS at all must produce output identical, byte for byte, to
        # vita-elf-create before this whole native-TLS-validation change (commit
        # 156f66b). fixtures/sample_156f66b.velf is that commit's own output for
        # fixtures/sample.elf, kept as a golden reference.
        golden_velf = os.path.join(fixtures_dir, "sample_156f66b.velf")
        with open(golden_velf, 'rb') as f:
            golden_bytes = f.read()
        with open(velf1, 'rb') as f:
            current_bytes = f.read()
        assert current_bytes == golden_bytes, \
            "Regression: converting a non-TLS ELF no longer produces byte-identical output to 156f66b"

    print("test_elf_create: ALL TESTS PASSED")

if __name__ == "__main__":
    main()
