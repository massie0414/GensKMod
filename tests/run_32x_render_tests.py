"""Build and test 32X H32/H40 composition and output; optionally run a supplied ISO."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--image", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    env = {key.upper(): value for key, value in os.environ.items()}
    vswhere = Path(env["PROGRAMFILES(X86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = Path(subprocess.check_output([
        str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"
    ], env=env, text=True).strip())
    out = root / "src/Gens/bin/32x-render-tests"
    out.mkdir(parents=True, exist_ok=True)
    if not args.skip_build:
        subprocess.run([str(vs / "MSBuild/Current/Bin/MSBuild.exe"),
                        str(root / "src/gensKMod.sln"), "/m:1", "/nr:false", "/p:Configuration=Release",
                        "/p:Platform=Win32", "/v:minimal"], cwd=root, env=env, check=True)
    objects = sorted((root / "src/Gens/bin/obj/Release").rglob("*.obj"))
    if not objects:
        parser.error("Release objects are missing; omit --skip-build")
    command = ["cl", "/nologo", "/MT", "/EHsc", "/D_CRT_SECURE_NO_WARNINGS",
               "/I" + str(root / "src/dx70_min/include"),
               "/Fo" + str(out) + "\\", "/Fe" + str(out / "render_test.exe"),
               str(root / "tests/32x_render_test.cpp")]
    command += [str(path) for path in objects]
    command += [str(root / "src/Gens/bin/obj/Release/Gens.res"),
                "/link", "/SUBSYSTEM:CONSOLE", "/SAFESEH:NO", "/NODEFAULTLIB:libc.lib",
                "/LIBPATH:" + str(root / "src/dx70_min/lib"),
                str(root / "src/Gens/libs/zlib.lib"), str(root / "src/Gens/libs/htmlhelp.lib"),
                "wsock32.lib", "comctl32.lib", "ddraw.lib", "dsound.lib", "dinput.lib",
                "dxguid.lib", "winmm.lib", "vfw32.lib", "user32.lib", "gdi32.lib",
                "shell32.lib", "advapi32.lib", "comdlg32.lib"]
    script = 'call "' + str(vs / "VC/Auxiliary/Build/vcvars32.bat") + '" >nul && '
    subprocess.run(script + subprocess.list2cmdline(command), shell=True, cwd=root, env=env, check=True)
    command = [str(out / "render_test.exe")]
    if args.image:
        command += [str(args.image.resolve())]
    subprocess.run(command, cwd=out, env=env, check=True, timeout=120)
    print("PASS. Test output:", out)


if __name__ == "__main__":
    main()
