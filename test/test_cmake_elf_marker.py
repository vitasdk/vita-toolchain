#!/usr/bin/env python3
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

from test_elf_create import VITASDK_NOTE


def main():
    toolchain = Path(sys.argv[1]).resolve()
    compiler = Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        scripts = root / 'toolchain with spaces'
        sdk = root / 'sdk with spaces'
        source = root / 'source'
        for path in (scripts, sdk, source):
            path.mkdir()
        shutil.copy2(toolchain, scripts / toolchain.name)
        shutil.copy2(toolchain.with_name('vita-elf-note.ld'), scripts / 'vita-elf-note.ld')
        (source / 'main.c').write_text('int main(void) { return 0; }\n')
        (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.16)
project(marker_probe LANGUAGES C)
add_executable(probe main.c)
target_link_options(probe PRIVATE -nostdlib -Wl,-e,main)
if(NOT CMAKE_EXE_LINKER_FLAGS MATCHES "--gc-sections")
  message(FATAL_ERROR "User linker flags were lost")
endif()
''')
        build = root / 'build'
        env = dict(os.environ, VITASDK=str(sdk))
        command = ['cmake', '-S', str(source), '-B', str(build),
                   '-DCMAKE_TOOLCHAIN_FILE=' + str(scripts / toolchain.name),
                   '-DCMAKE_C_COMPILER=' + str(compiler),
                   '-DCMAKE_EXE_LINKER_FLAGS=-Wl,--gc-sections']
        for _ in range(2):
            result = subprocess.run(command, env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True)
            assert result.returncode == 0, result.stdout
        subprocess.run(['cmake', '--build', str(build)], env=env, check=True)
        data = (build / 'probe').read_bytes()
        assert data[:4] == b'\x7fELF'
        is_64 = data[4] == 2
        header = struct.unpack_from('<16sHHIQQQIHHHHHH' if is_64 else '<16sHHIIIIIHHHHHH', data)
        sections = [struct.unpack_from('<IIQQQQIIQQ' if is_64 else '<10I', data,
                                       header[6] + i * header[11]) for i in range(header[12])]
        strings = sections[header[13]]
        names = data[strings[4]:strings[4] + strings[5]]
        notes = [s for s in sections if names[s[0]:names.index(0, s[0])] == b'.note.vitasdk']
        assert len(notes) == 1, 'CMake must emit one marker even after reconfiguration'
        note = notes[0]
        assert note[1] == 7 and not (note[2] & 2) and note[8] == 4
        assert data[note[4]:note[4] + note[5]] == VITASDK_NOTE

        layout = source / 'custom.ld'
        layout.write_text('''SECTIONS {
  .text 0x10000 : { *(.text*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) }
  /DISCARD/ : { *(.comment) }
}
''')
        custom_flags = f'-Wl,--gc-sections -no-pie -Wl,-T,"{layout}"'
        result = subprocess.run([*command, '-DCMAKE_EXE_LINKER_FLAGS=' + custom_flags],
                                env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        assert result.returncode == 0, result.stdout
        subprocess.run(['cmake', '--build', str(build)], env=env, check=True)
        assert VITASDK_NOTE in (build / 'probe').read_bytes()
    print('test_cmake_elf_marker: ALL TESTS PASSED')


if __name__ == '__main__':
    main()
