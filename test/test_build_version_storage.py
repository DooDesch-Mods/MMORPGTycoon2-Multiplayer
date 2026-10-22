"""Verify version-only rebuilds using the production config source and CMake properties."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BuildVersionStorageTests(unittest.TestCase):
    def test_version_is_not_a_target_wide_definition(self):
        root = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        plugin = (ROOT / "cmake/framework_plugin.cmake").read_text(encoding="utf-8")
        self.assertLess(root.index("set_property(SOURCE src/core/config.cpp"), root.index("if(MT2MP_FRAMEWORK_PLUGIN)"))
        for text in [root[root.index("target_compile_definitions(MT2_Multiplayer"):], plugin]:
            self.assertNotIn('MT2MP_BUILD_VERSION="${MT2MP_BUILD_VERSION}"', text)
        self.assertNotIn("MT2MP_BUILD_VERSION", (ROOT / "include/mt2mp/core/config.h").read_text(encoding="utf-8"))

    @unittest.skipUnless(os.name == "nt" and shutil.which("cl") and shutil.which("cmake"),
                         "The warm-build proof requires an MSVC developer shell")
    def test_actual_config_links_one_version_and_only_config_rebuilds(self):
        text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        start = text.index("set_property(SOURCE src/core/config.cpp")
        end = text.index("SKIP_PRECOMPILE_HEADERS TRUE)", start) + len("SKIP_PRECOMPILE_HEADERS TRUE)")
        properties = text[start:end].replace("src/core/config.cpp", '"' + (ROOT / "src/core/config.cpp").as_posix() + '"')
        with tempfile.TemporaryDirectory(prefix="mt2-version-storage-") as directory:
            fixture = Path(directory)
            build = fixture / "build"
            (fixture / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(VersionFixture LANGUAGES CXX)\n'
                'set(MT2MP_BUILD_VERSION "" CACHE STRING "Version")\n' + properties + '\n'
                'add_executable(version_fixture main.cpp consumer.cpp "' + (ROOT / "src/core/config.cpp").as_posix() + '")\n'
                'target_include_directories(version_fixture PRIVATE "' + (ROOT / "include").as_posix() + '")\n'
                'target_compile_features(version_fixture PRIVATE cxx_std_17)\n'
                'target_precompile_headers(version_fixture PRIVATE <string>)\n', encoding="utf-8")
            (fixture / "consumer.cpp").write_text('#include "mt2mp/core/config.h"\nconst char* consumer(){return mt2mp::MOD_VERSION;}\n', encoding="utf-8")
            (fixture / "main.cpp").write_text(
                '#include "mt2mp/core/config.h"\n#include <cstring>\n#include <type_traits>\n'
                'static_assert(std::is_same_v<decltype(mt2mp::MOD_VERSION),const char* const>);\n'
                'const char* consumer();\nint main(int argc,char** argv){return argc!=2||consumer()!=mt2mp::MOD_VERSION||std::strcmp(mt2mp::MOD_VERSION,argv[1]);}\n', encoding="utf-8")

            def run(*arguments):
                result = subprocess.run(arguments, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result.stdout + result.stderr

            def configure(version):
                run("cmake", "-S", str(fixture), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64", "-DMT2MP_BUILD_VERSION=" + version)

            configure("0.1.0-beta.21")
            run("cmake", "--build", str(build), "--config", "Release")
            executable = build / "Release/version_fixture.exe"
            run(str(executable), "0.1.0-beta.21")
            before = {str(path.relative_to(build)): path.stat().st_mtime_ns for path in build.rglob("*.obj")}
            self.assertTrue(any(Path(name).name == "config.obj" for name in before), before)
            self.assertTrue(any(Path(name).name == "consumer.obj" for name in before), before)
            executable_before = executable.stat().st_mtime_ns
            configure("0.1.0-beta.22")
            run("cmake", "--build", str(build), "--config", "Release")
            run(str(executable), "0.1.0-beta.22")
            changed = [name for name, stamp in before.items() if (build / name).stat().st_mtime_ns != stamp]
            self.assertEqual(len(changed), 1, changed)
            self.assertEqual(Path(changed[0]).name, "config.obj")
            self.assertNotEqual(executable.stat().st_mtime_ns, executable_before)


if __name__ == "__main__":
    unittest.main()
