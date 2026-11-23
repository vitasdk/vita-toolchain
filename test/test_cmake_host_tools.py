#!/usr/bin/env python3
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest


class CMakeHostToolTests(unittest.TestCase):
    vita_cmake = None
    toolchain = None
    fixture = pathlib.Path(__file__).parent / "fixtures" / "cmake_host_tools"

    def test_toolchain_resolves_psp2rela_with_host_suffix(self):
        with tempfile.TemporaryDirectory(prefix="vita sdk path ") as tmp:
            sdk = pathlib.Path(tmp) / "fake sdk"
            sdk.mkdir()
            expected = sdk / "bin" / "psp2rela.exe"
            script = pathlib.Path(tmp) / "check-toolchain.cmake"
            script.write_text(
                f"""
set(CMAKE_HOST_WIN32 TRUE)
set(ENV{{VITASDK}} [==[{sdk}]==])
include([==[{self.toolchain}]==])
if(NOT VITA_PSP2RELA STREQUAL [==[{expected}]==])
  message(FATAL_ERROR "unexpected VITA_PSP2RELA='${{VITA_PSP2RELA}}'")
endif()
get_property(_type CACHE VITA_PSP2RELA PROPERTY TYPE)
if(NOT _type STREQUAL "PATH")
  message(FATAL_ERROR "VITA_PSP2RELA cache type is '${{_type}}', expected PATH")
endif()
""",
                encoding="utf-8",
            )
            result = subprocess.run(
                ["cmake", "-P", str(script)], capture_output=True, text=True
            )
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_rel_optimize_uses_resolved_psp2rela_path(self):
        with tempfile.TemporaryDirectory(prefix="vita host tools ") as tmp:
            build = pathlib.Path(tmp) / "build"
            psp2rela = pathlib.Path(tmp) / "sdk tools" / "psp2rela.exe"
            result = subprocess.run(
                [
                    "cmake",
                    "-S",
                    str(self.fixture),
                    "-B",
                    str(build),
                    "-G",
                    "Unix Makefiles",
                    f"-DVITA_CMAKE={self.vita_cmake}",
                    f"-DVITA_PSP2RELA={psp2rela}",
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

            commands = "\n".join(
                path.read_text(encoding="utf-8")
                for path in build.rglob("build.make")
                if "psp2rela" in path.read_text(encoding="utf-8")
            ).replace("\\ ", " ")
            self.assertIn(str(psp2rela), commands)
            self.assertNotRegex(commands, r"(?m)^\s*psp2rela(?:\s|$)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(
            "Usage: test_cmake_host_tools.py <vita.cmake> <vita.toolchain.cmake>"
        )
    CMakeHostToolTests.vita_cmake = pathlib.Path(sys.argv[1]).resolve()
    CMakeHostToolTests.toolchain = pathlib.Path(sys.argv[2]).resolve()
    unittest.main(argv=[sys.argv[0]])
