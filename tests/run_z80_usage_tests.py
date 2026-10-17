"""Build and run Z80 CPU usage and panel tests (no game ROM required)."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom")
    parser.add_argument("--reference", action="store_true", help="compare against native Z80 execution without wait classification")
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
    out = root / "src/Gens/bin/z80-usage-tests"
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
                   "/I" + str(root / "src/dx70_min/include"), "/I" + str(root / "src/Gens"),
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
    name = "z80_rom_usage_test" if args.rom else "z80_usage_test"
    sources = ["tests/" + name + ".cpp"]
    if args.reference:
        reference = out / "Cpu_Z80_reference.c"
        cpu_source = (root / "src/Gens/Cpu_Z80.c").read_text()
        cpu_source = cpu_source[:cpu_source.index("unsigned FASTCALL Z80_Exec_Profiled(")] + "unsigned FASTCALL Z80_Exec_Profiled(Z80_CONTEXT *cpu, int target) { return z80_Exec(cpu,target); }\n"
        reference.write_text(cpu_source)
        sources.append(str(reference))
        objects = [obj for obj in objects if obj.name.lower() != "cpu_z80.obj"]
        name += "_reference"
    compile_test(name, sources, objects, [
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
    subprocess.run([str(out / (name + ".exe"))] + ([str(Path(args.rom).resolve())] if args.rom else []) + (["reference"] if args.reference else []), cwd=out if args.rom else root, check=True, timeout=120)



if __name__ == "__main__":
    main()
