"""Exercise production controller assignment with fake devices and a real hidden Win32 dialog."""
import os
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    out = root / "src/Gens/bin/controller-input-tests"
    out.mkdir(parents=True, exist_ok=True)
    source = (root / "src/Gens/G_dinput.cpp").read_text(encoding="utf-8")
    main_source = (root / "src/Gens/G_main.cpp").read_bytes().decode("latin-1")
    # Compile the actual functions, replacing only device/host dependencies.
    sections = [
        source[source.index("static void Clear_Joy_State("):source.index("BOOL CALLBACK InitJoystick(")],
        source[source.index("void Update_Input()"):source.index("void Update_Controllers()")],
        source[source.index("// Key assignment runs"):source.index("void Scan_Player_Net(")],
    ]
    start = main_source.index("LRESULT CALLBACK ControllerProc(",
                              main_source.index("// Controller configuration dialog box procedure"))
    end = main_source.index("\n}", start) + 2
    sections.append(main_source[start:end])
    harness = (root / "tests/controller_input_test.cpp").read_text()
    generated = out / "controller_input_test.cpp"
    generated.write_text(harness.replace("// PRODUCTION_FUNCTIONS", "\n".join(sections)))
    env = {k.upper(): v for k, v in os.environ.items()}
    vswhere = Path(env["PROGRAMFILES(X86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = Path(subprocess.check_output([
        str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"
    ], text=True, env=env).strip())
    vcvars = vs / "VC/Auxiliary/Build/vcvars32.bat"

    def build(command, cwd):
        subprocess.run('call "' + str(vcvars) + '" >nul && ' + subprocess.list2cmdline(command),
                       shell=True, cwd=cwd, env=env, check=True)

    resource = out / "Gens.res"
    build(["rc", "/nologo", "/fo", str(resource), "gens.rc"], root / "src/Gens")
    exe = out / "controller_input_test.exe"
    build(["cl", "/nologo", "/O2", "/MT", "/EHsc", "/D_CRT_SECURE_NO_WARNINGS",
           "/I" + str(root / "src/Gens"), "/I" + str(root / "src/dx70_min/include"),
           str(generated), "/Fo" + str(out) + "\\", "/Fe" + str(exe),
           "/link", str(resource), "user32.lib", "gdi32.lib"], out)
    result = subprocess.run([str(exe)], cwd=out, capture_output=True, text=True, timeout=15)
    print(result.stdout, end="")
    print(result.stderr, end="")
    result.check_returncode()


if __name__ == "__main__":
    main()
