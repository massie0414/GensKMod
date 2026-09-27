"""Build and run SH2 CPU usage and debug window tests (no game ROM required)."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true", help="reuse the Release emulator objects")
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    env = {key.upper(): value for key, value in os.environ.items()}
    vswhere = Path(env["PROGRAMFILES(X86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = Path(subprocess.check_output([
        str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"
    ], env=env, text=True).strip())
    out = root / "src/Gens/bin/32x-boot-tests"
    out.mkdir(parents=True, exist_ok=True)
    if not args.skip_build:
        subprocess.run([str(vs / "MSBuild/Current/Bin/MSBuild.exe"),
                        str(root / "src/gensKMod.sln"), "/m", "/p:Configuration=Release",
                        "/p:Platform=Win32", "/v:minimal"], cwd=root, env=env, check=True)
    vcvars = vs / "VC/Auxiliary/Build/vcvars32.bat"

    def compile_test(name, sources, objects=(), libraries=()):
        objdir = out / (name + "-obj")
        objdir.mkdir(exist_ok=True)
        command = ["cl", "/nologo", "/MT", "/EHsc", "/D_CRT_SECURE_NO_WARNINGS",
                   "/I" + str(root / "src/dx70_min/include"),
                   "/Fo" + str(objdir) + "\\", "/Fe" + str(out / (name + ".exe"))]
        command += [str(root / path) for path in sources]
        command += [str(path) for path in objects]
        command += ["/link", "/SUBSYSTEM:CONSOLE", "/SAFESEH:NO", "/NODEFAULTLIB:libc.lib"]
        command += list(libraries)
        script = 'call "' + str(vcvars) + '" >nul && ' + subprocess.list2cmdline(command)
        subprocess.run(script, shell=True, cwd=root, env=env, check=True)

    objects = sorted((root / "src/Gens/bin/obj/Release").rglob("*.obj"))
    if not objects:
        parser.error("Release objects are missing; omit --skip-build")
    compile_test("sh2_usage_test", ["tests/sh2_usage_test.cpp"], objects, [
        "/LIBPATH:" + str(root / "src/dx70_min/lib"),
        str(root / "src/Gens/libs/zlib.lib"), str(root / "src/Gens/libs/htmlhelp.lib"),
        "wsock32.lib", "comctl32.lib", "ddraw.lib", "dsound.lib", "dinput.lib",
        "dxguid.lib", "winmm.lib", "vfw32.lib", "user32.lib", "gdi32.lib",
        "shell32.lib", "advapi32.lib", "comdlg32.lib"
    ])
    compile_test("sh2_window_test", ["tests/sh2_window_test.cpp"], objects, [
        str(root / "src/Gens/bin/obj/Release/Gens.res"),
        "/LIBPATH:" + str(root / "src/dx70_min/lib"),
        str(root / "src/Gens/libs/zlib.lib"), str(root / "src/Gens/libs/htmlhelp.lib"),
        "wsock32.lib", "comctl32.lib", "ddraw.lib", "dsound.lib", "dinput.lib",
        "dxguid.lib", "winmm.lib", "vfw32.lib", "user32.lib", "gdi32.lib",
        "shell32.lib", "advapi32.lib", "comdlg32.lib"
    ])
    if args.build_only:
        print("Test executables:", out)
        return
    subprocess.run([str(out / "sh2_usage_test.exe")], check=True, timeout=15)
    subprocess.run([str(out / "sh2_window_test.exe")], check=True, timeout=15)
    print("PASS: SH2 usage and window tests")



if __name__ == "__main__":
    main()
