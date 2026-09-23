"""Build and exercise CD32X boot, PWM DMA, input, playback and reset."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, help="user-supplied MODPlayer32X ISO")
    parser.add_argument("--skip-build", action="store_true", help="reuse the Release emulator objects")
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    image = args.image.resolve() if args.image else None
    if not image and not args.build_only:
        parser.error("--image is required to run the game tests")
    if image and not image.is_file():
        parser.error("image does not exist")
    env = {key.upper(): value for key, value in os.environ.items()}
    vswhere = Path(env["PROGRAMFILES(X86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = Path(subprocess.check_output([
        str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"
    ], env=env, text=True).strip())
    out = root / "src/Gens/bin/cd32x-tests"
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

    if image:
        objects = sorted((root / "src/Gens/bin/obj/Release").rglob("*.obj"))
        if not objects:
            parser.error("Release objects are missing; omit --skip-build")
        compile_test("cd32x_boot_test", ["tests/cd32x_boot_test.cpp"], objects, [
            "/LIBPATH:" + str(root / "src/dx70_min/lib"),
            str(root / "src/Gens/libs/zlib.lib"), str(root / "src/Gens/libs/htmlhelp.lib"),
            "wsock32.lib", "comctl32.lib", "ddraw.lib", "dsound.lib", "dinput.lib",
            "dxguid.lib", "winmm.lib", "vfw32.lib", "user32.lib", "gdi32.lib",
            "shell32.lib", "advapi32.lib", "comdlg32.lib"
        ])
    if args.build_only:
        print("Test executables:", out)
        return
    run = Path(tempfile.mkdtemp(prefix="run-", dir=out))
    if image:
        for mode in ("normal", "fast", "accurate"):
            folder = run / mode
            folder.mkdir()
            subprocess.run([str(out / "cd32x_boot_test.exe"), str(image), mode],
                           cwd=folder, env=env, check=True, timeout=120)
        for rate in (11025,22050):
            folder=run / ("audio-"+str(rate))
            folder.mkdir()
            subprocess.run([str(out / "cd32x_boot_test.exe"), str(image), "normal", str(rate)],
                           cwd=folder, env=env, check=True, timeout=120)
    print("PASS. Screenshots and isolated test saves:", run)


if __name__ == "__main__":
    main()
