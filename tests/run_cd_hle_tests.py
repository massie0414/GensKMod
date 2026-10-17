"""Build Win32 tests with VS C++ tools. Optional game images are supplied by the user."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path)
    parser.add_argument("--sol-feace", type=Path, help="SOL-FEACE CUE for opening and gameplay tests")
    parser.add_argument("--tenka-fubu", type=Path, help="TENKA FUBU CUE for boot and gameplay tests")
    parser.add_argument("--quiz-scramble", type=Path, help="QUIZ SCRAMBLE CUE for boot and input tests")
    parser.add_argument("--skip-build", action="store_true", help="reuse the Release emulator objects")
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    image = args.image.resolve() if args.image else None
    sol_feace = args.sol_feace.resolve() if args.sol_feace else None
    tenka_fubu = args.tenka_fubu.resolve() if args.tenka_fubu else None
    quiz_scramble = args.quiz_scramble.resolve() if args.quiz_scramble else None
    if quiz_scramble and not quiz_scramble.is_file():
        parser.error("QUIZ SCRAMBLE image does not exist")
    if tenka_fubu and not tenka_fubu.is_file():
        parser.error("TENKA FUBU image does not exist")
    if image and not image.is_file():
        parser.error("image does not exist")
    if sol_feace and not sol_feace.is_file():
        parser.error("SOL-FEACE image does not exist")
    env = {key.upper(): value for key, value in os.environ.items()}
    vswhere = Path(env["PROGRAMFILES(X86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = Path(subprocess.check_output([
        str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"
    ], env=env, text=True).strip())
    out = root / "src/Gens/bin/cd-hle-tests"
    out.mkdir(parents=True, exist_ok=True)
    if not args.skip_build:
        subprocess.run([str(vs / "MSBuild/Current/Bin/MSBuild.exe"),
                        str(root / "src/gensKMod.sln"), "/m:1", "/nr:false", "/p:Configuration=Release",
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

    compile_test("cd_cue_test", ["tests/cd_cue_test.c", "src/Gens/cd_cue.c"])
    if image or sol_feace or tenka_fubu or quiz_scramble:
        objects = sorted((root / "src/Gens/bin/obj/Release").rglob("*.obj"))
        if not objects:
            parser.error("Release objects are missing; omit --skip-build")
        libraries = [
            "/LIBPATH:" + str(root / "src/dx70_min/lib"),
            str(root / "src/Gens/libs/zlib.lib"), str(root / "src/Gens/libs/htmlhelp.lib"),
            "wsock32.lib", "comctl32.lib", "ddraw.lib", "dsound.lib", "dinput.lib",
            "dxguid.lib", "winmm.lib", "vfw32.lib", "user32.lib", "gdi32.lib",
            "shell32.lib", "advapi32.lib", "comdlg32.lib"
        ]
        compile_test("cd_hle_contract_test", ["tests/cd_hle_contract_test.cpp"], objects, libraries)
        if image:
            compile_test("cd_hle_boot_test", ["tests/cd_hle_boot_test.cpp"], objects, libraries)
        if sol_feace:
            compile_test("cd_sol_feace_test", ["tests/cd_sol_feace_test.cpp"], objects, libraries)
        if quiz_scramble:
            compile_test("cd_quiz_scramble_test", ["tests/cd_quiz_scramble_test.cpp"], objects, libraries)
        if tenka_fubu:
            compile_test("cd_tenka_fubu_test", ["tests/cd_tenka_fubu_test.cpp"], objects, libraries)
    if args.build_only:
        print("Test executables:", out)
        return
    run = Path(tempfile.mkdtemp(prefix="run-", dir=out))
    subprocess.run([str(out / "cd_cue_test.exe")], cwd=run, env=env, check=True)
    if image or sol_feace or tenka_fubu or quiz_scramble:
        subprocess.run([str(out / "cd_hle_contract_test.exe")], cwd=run, env=env, check=True)
    if quiz_scramble:
        for timing, rate in (("normal", 22050), ("accurate", 44100)):
            for mode in ("play", "opening"):
                folder = run / ("quiz-scramble-" + timing + "-" + mode)
                folder.mkdir()
                subprocess.run([str(out / "cd_quiz_scramble_test.exe"), str(quiz_scramble), mode, timing, str(rate)],
                               cwd=folder, env=env, check=True)
    if tenka_fubu:
        for timing, rate in (("normal", 22050), ("accurate", 44100)):
            for mode in ("opening", "play"):
                folder = run / ("tenka-fubu-" + timing + "-" + mode)
                folder.mkdir()
                subprocess.run([str(out / "cd_tenka_fubu_test.exe"), str(tenka_fubu), mode, timing, str(rate)],
                               cwd=folder, env=env, check=True)
    if sol_feace:
        for timing, rate in (("normal", 22050), ("accurate", 44100)):
            for mode in ("opening", "play"):
                folder = run / ("sol-feace-" + timing + "-" + mode)
                folder.mkdir()
                subprocess.run([str(out / "cd_sol_feace_test.exe"), str(sol_feace), mode, timing, str(rate)],
                               cwd=folder, env=env, check=True)
    if image:
        for timing in ("normal", "accurate"):
            folder = run / timing
            folder.mkdir()
            for mode in ("opening", "new", "continue"):
                subprocess.run([str(out / "cd_hle_boot_test.exe"), str(image), mode, timing],
                               cwd=folder, env=env, check=True)
            for rate in (11025, 22050):
                audio_folder = folder / ("opening-" + str(rate))
                audio_folder.mkdir()
                subprocess.run([str(out / "cd_hle_boot_test.exe"), str(image), "opening", timing, str(rate)],
                               cwd=audio_folder, env=env, check=True)
    print("PASS. Screenshots, traces and isolated test saves:", run)


if __name__ == "__main__":
    main()
