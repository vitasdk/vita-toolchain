#!/usr/bin/env python3
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time
import unittest


def write_executable(path, content):
    path.write_text(content, encoding="utf-8")
    path.chmod(0o755)


class CMakeStubTests(unittest.TestCase):
    vita_cmake = None
    libs_gen = None

    def create_project(self, root, mode):
        source = root / "source project"
        sdk = root / "fake sdk"
        tools = sdk / "test tools"
        share = sdk / "share"
        source.mkdir(parents=True)
        tools.mkdir(parents=True)
        share.mkdir(parents=True)

        exporter = tools / "fake-export.py"
        assembler = tools / "fake-assembler.py"
        archiver = tools / "fake-archiver.py"
        ranlib = tools / "fake-ranlib.py"
        libs_gen = tools / "vita-libs-gen"
        shutil.copy2(self.libs_gen, libs_gen)

        write_executable(
            exporter,
            '''#!/usr/bin/env python3
import pathlib
import sys

kind, source, config, output = sys.argv[1:5]
revision = "2" if "revision: 2" in pathlib.Path(config).read_text() else "1"
if kind == "kernel":
    libraries = f"""
      KernelLibrary:
        kernel: true
        nid: 0x20000001
        functions:
          kernelFunction{revision}: 0x30000001
"""
else:
    libraries = f"""
      UserLibrary:
        kernel: false
        nid: 0x20000002
        functions:
          userFunction{revision}: 0x30000002
"""
pathlib.Path(output).write_text(f"""version: 2
firmware: 3.60
modules:
  SampleModule:
    nid: 0x10000001
    libraries:
{libraries}""")
''',
        )
        write_executable(
            assembler,
            """#!/usr/bin/env python3
import pathlib
import sys

args = sys.argv[1:]
if "--version" in args:
    print("fake GNU assembler 1.0")
    raise SystemExit(0)
if "-o" in args:
    output = pathlib.Path(args[args.index("-o") + 1])
    source = next((pathlib.Path(arg) for arg in reversed(args) if arg.endswith((".S", ".s"))), None)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(b"FAKEOBJ\\n" + (source.read_bytes() if source else b""))
""",
        )
        write_executable(
            archiver,
            """#!/usr/bin/env python3
import pathlib
import sys

args = sys.argv[1:]
archive = pathlib.Path(args[1])
members = [pathlib.Path(arg) for arg in args[2:]]
archive.parent.mkdir(parents=True, exist_ok=True)
archive.write_bytes(
    b"!<arch>\\n" + b"".join(member.name.encode() + b"\\n" + member.read_bytes() for member in members)
)
""",
        )
        write_executable(ranlib, "#!/usr/bin/env python3\n")

        (share / "vita.toolchain.cmake").write_text(
            f"""
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_ASM_COMPILER [==[{assembler}]==] CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER_ID GNU CACHE STRING "" FORCE)
set(CMAKE_ASM_COMPILER_WORKS TRUE CACHE BOOL "" FORCE)
set(CMAKE_ASM_COMPILER_FORCED TRUE CACHE BOOL "" FORCE)
set(CMAKE_AR [==[{archiver}]==] CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB [==[{ranlib}]==] CACHE FILEPATH "" FORCE)
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
""",
            encoding="utf-8",
        )

        config_source = source / "SampleModule.yml"
        config_source.write_text("SampleModule:\n  revision: 1\n", encoding="utf-8")
        (source / "SampleModule.elf").write_bytes(b"ELF")
        (source / "CMakeLists.txt").write_text(
            f"""
cmake_minimum_required(VERSION 3.16)
project(cmake_stubs_test NONE)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(VITA_ELF_EXPORT "${{Python3_EXECUTABLE}}" [==[{exporter}]==])
set(VITA_LIBS_GEN [==[{libs_gen}]==])
include([==[{self.vita_cmake}]==])

set(module_file "${{CMAKE_CURRENT_SOURCE_DIR}}/SampleModule.elf")
if(MODE STREQUAL "genexports")
  set(config_file "${{CMAKE_CURRENT_BINARY_DIR}}/SampleModule.yml")
  add_custom_command(
    OUTPUT "${{config_file}}"
    COMMAND "${{CMAKE_COMMAND}}" -E copy_if_different
      "${{CMAKE_CURRENT_SOURCE_DIR}}/SampleModule.yml"
      "${{config_file}}"
    DEPENDS "${{CMAKE_CURRENT_SOURCE_DIR}}/SampleModule.yml"
    VERBATIM
  )
  set(stub_flags GEN_EXPORTS)
elseif(MODE STREQUAL "kernel")
  set(config_file "${{CMAKE_CURRENT_SOURCE_DIR}}/SampleModule.yml")
  set(stub_flags KERNEL)
else()
  set(config_file "${{CMAKE_CURRENT_SOURCE_DIR}}/SampleModule.yml")
  set(stub_flags)
endif()

vita_create_stubs(stubs "${{module_file}}" "${{config_file}}" ${{stub_flags}})
""",
            encoding="utf-8",
        )
        return source, sdk, config_source

    def run_case(self, generator, mode):
        with tempfile.TemporaryDirectory(prefix="vita cmake stubs ") as tmp:
            root = pathlib.Path(tmp)
            source, sdk, config_source = self.create_project(root, mode)
            build = root / "build output"
            env = os.environ.copy()
            env["VITASDK"] = str(sdk)

            configured = subprocess.run(
                [
                    "cmake",
                    "-S",
                    str(source),
                    "-B",
                    str(build),
                    "-G",
                    generator,
                    f"-DMODE={mode}",
                ],
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertEqual(configured.returncode, 0, configured.stderr)

            built = subprocess.run(
                ["cmake", "--build", str(build), "--target", "stubs"],
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)

            strong = build / "libSampleModule_stub.a"
            weak = build / "libSampleModule_stub_weak.a"
            self.assertTrue(strong.is_file(), strong)
            self.assertTrue(weak.is_file(), weak)
            tracked_archives = [strong, weak]
            if mode == "kernel":
                kernel_archives = list(build.rglob("libKernelLibrary_stub.a"))
                self.assertTrue(kernel_archives, "kernel library archive was not generated")
                tracked_archives.append(kernel_archives[0])

            before = tuple(path.stat().st_mtime_ns for path in tracked_archives)
            time.sleep(1.1)
            rebuilt = subprocess.run(
                ["cmake", "--build", str(build), "--target", "stubs"],
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertEqual(rebuilt.returncode, 0, rebuilt.stdout + rebuilt.stderr)
            self.assertEqual(
                before,
                tuple(path.stat().st_mtime_ns for path in tracked_archives),
                "a no-op parent rebuild rebuilt the nested stub archives",
            )

            config_source.write_text("SampleModule:\n  revision: 2\n", encoding="utf-8")
            time.sleep(1.1)
            changed = subprocess.run(
                ["cmake", "--build", str(build), "--target", "stubs"],
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertEqual(changed.returncode, 0, changed.stdout + changed.stderr)
            self.assertNotEqual(
                before,
                tuple(path.stat().st_mtime_ns for path in tracked_archives),
            )

            cleaned = subprocess.run(
                ["cmake", "--build", str(build), "--target", "clean"],
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertEqual(cleaned.returncode, 0, cleaned.stdout + cleaned.stderr)
            self.assertFalse(strong.exists())
            self.assertFalse(weak.exists())

            final = subprocess.run(
                ["cmake", "--build", str(build), "--target", "stubs"],
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertEqual(final.returncode, 0, final.stdout + final.stderr)
            self.assertTrue(strong.is_file())
            self.assertTrue(weak.is_file())

    def test_ninja_ordinary_yaml(self):
        self.run_case("Ninja", "ordinary")

    def test_unix_makefiles_kernel_yaml(self):
        self.run_case("Unix Makefiles", "kernel")

    def test_ninja_build_time_gen_exports_yaml(self):
        self.run_case("Ninja", "genexports")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: test_cmake_stubs.py <vita.cmake> <vita-libs-gen>")
    CMakeStubTests.vita_cmake = pathlib.Path(sys.argv[1]).resolve()
    CMakeStubTests.libs_gen = pathlib.Path(sys.argv[2]).resolve()
    unittest.main(argv=[sys.argv[0]])
