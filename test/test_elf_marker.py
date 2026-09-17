#!/usr/bin/env python3
import os
import struct
import subprocess
import sys
import tempfile

from test_elf_create import VITASDK_NOTE, add_vitasdk_note, assert_empty_imports


def main():
    elf_create = sys.argv[1]
    fixtures = os.path.join(os.path.dirname(__file__), 'fixtures')
    sample = os.path.join(fixtures, 'sample_no_imports.elf')
    config = os.path.join(fixtures, 'sample_no_imports.yml')
    with tempfile.TemporaryDirectory() as tmpdir:
        output = os.path.join(tmpdir, 'output.velf')

        def convert(source, flags=()):
            return subprocess.run([elf_create, *flags, source, output],
                                  capture_output=True, text=True)

        modes = [[], ['-e', config], ['-m', 'module_start,module_stop,module_exit'],
                 ['-g', os.path.join(tmpdir, 'generated.yml')]]
        for flags in modes:
            result = convert(sample, flags)
            assert result.returncode != 0 and 'No VitaSDK ELF marker' in result.stderr, \
                f"Unmarked ARM ELF was not rejected ({flags}): {result.stderr}"
            result = convert(sample, ['-n', *flags])
            assert result.returncode == 0, f"Legacy override failed: {result.stderr}"

        marked = os.path.join(tmpdir, 'marked.elf')
        add_vitasdk_note(sample, marked)
        for flags in modes:
            result = convert(marked, flags)
            assert result.returncode == 0, f"Marked import-free ELF failed: {result.stderr}"
            assert_empty_imports(output)

        duplicate = os.path.join(tmpdir, 'duplicate.elf')
        add_vitasdk_note(marked, duplicate)
        assert convert(duplicate, ['-e', config]).returncode == 0
        add_vitasdk_note(sample, duplicate, VITASDK_NOTE * 2)
        assert convert(duplicate, ['-e', config]).returncode == 0, \
            'Coalesced linker and compatibility-script notes must be accepted'

        mutations = [
            ('empty', b'', 7, 0),
            ('truncated', VITASDK_NOTE[:-1], 7, 0),
            ('trailing-data', VITASDK_NOTE + b'\0', 7, 0),
            ('owner', VITASDK_NOTE[:12] + b'NotVita\0' + VITASDK_NOTE[20:], 7, 0),
            ('type', VITASDK_NOTE[:8] + struct.pack('<I', 2) + VITASDK_NOTE[12:], 7, 0),
            ('version', VITASDK_NOTE[:20] + struct.pack('<I', 2), 7, 0),
            ('second-version', VITASDK_NOTE + VITASDK_NOTE[:20] + struct.pack('<I', 2), 7, 0),
            ('name-size', struct.pack('<I', 7) + VITASDK_NOTE[4:], 7, 0),
            ('desc-size', VITASDK_NOTE[:4] + struct.pack('<I', 8) + VITASDK_NOTE[8:], 7, 0),
            ('not-a-note', VITASDK_NOTE, 1, 0),
            ('allocated', VITASDK_NOTE, 7, 2),
        ]
        for name, note, section_type, flags in mutations:
            bad = os.path.join(tmpdir, name + '.elf')
            for source in (sample, marked, os.path.join(fixtures, 'sample.elf')):
                add_vitasdk_note(source, bad, note, section_type, flags)
                for options in ([], ['-n']):
                    result = convert(bad, options)
                    assert result.returncode != 0 and 'Invalid .note.vitasdk' in result.stderr, \
                        f"Invalid marker accepted ({name}, {options}): {result.stderr}"

        with open(marked, 'rb') as f:
            original = f.read()
        for name, offset, value, diagnostic in [
                ('non-arm', 18, 3, 'not an ARM binary'),
                ('relocatable', 16, 1, 'not an ET_EXEC binary'),
                ('already-velf', 16, 0xfe04, 'not an ET_EXEC binary')]:
            bad = os.path.join(tmpdir, name + '.elf')
            data = bytearray(original)
            struct.pack_into('<H', data, offset, value)
            with open(bad, 'wb') as f:
                f.write(data)
            for options in ([], ['-n']):
                result = convert(bad, options)
                assert result.returncode != 0 and diagnostic in result.stderr, \
                    f"Marker bypassed ELF validation ({name}): {result.stderr}"

    print('test_elf_marker: ALL TESTS PASSED')


if __name__ == '__main__':
    main()
