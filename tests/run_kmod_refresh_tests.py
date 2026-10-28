"""Check production debug refresh pacing with a deterministic clock, plus the 32X panel."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / "src/Gens/bin/kmod-refresh-tests"
out.mkdir(parents=True, exist_ok=True)
source = (root / "src/Gens/kmod.c").read_text()
refresh = source[source.index("static DWORD last_panel_update;"):source.index("void kmod_close()")]
panels = [
    "watchers", "message", "planes", "m68kdebug", "z80debug", "vdpdebug", "vdpreg",
    "sprites", "ym2612", "psg", "s68kdebug", "cdcdebug", "cdgfx", "cdprg", "cdreg",
    "mSH2", "sSH2", "vdp32x", "s32xreg",
]
harness = r'''
#include <cassert>
#include <cstdio>
typedef unsigned long DWORD;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
DWORD clock_ms;
DWORD GetTickCount() { return clock_ms; }
int SegaCD_Started, _32X_Started, CD_32X_Active, AutoShot_KMod, AutoPause_KMod, Paused;
unsigned recordings, shots, panel_calls[19];
void GMVUpdateRecord_KMod() { ++recordings; }
void Take_Shot() { ++shots; }
'''
for i, name in enumerate(panels):
    harness += f"void {name}_update() {{ ++panel_calls[{i}]; }}\n"
harness += refresh
harness += r'''
int main() {
    SegaCD_Started = CD_32X_Active = 1;
    AutoShot_KMod = 1;
    // Busy host loop and emulation waits must not multiply the UI work.
    for (clock_ms = 0; clock_ms < 1000; ++clock_ms)
        for (int i = 0; i < 10; ++i) Update_KMod_Periodic();
    for (int i = 0; i < 19; ++i) assert(panel_calls[i] == 10);
    assert(recordings == 10000 && shots == 10000);
    AutoShot_KMod = 0;
    // Opening a panel and frame stepping refresh immediately, even at the same tick.
    clock_ms = 999;
    Update_KMod(); Update_KMod();
    for (int i = 0; i < 19; ++i) assert(panel_calls[i] == 12);
    AutoPause_KMod = 1;
    Update_KMod_Periodic();
    assert(Paused && !AutoPause_KMod && panel_calls[0] == 12);
    // A long stall causes one refresh, with no catch-up burst.
    clock_ms = 100000;
    Update_KMod_Periodic(); Update_KMod_Periodic();
    assert(panel_calls[0] == 13);
    // DWORD rollover must not suppress updates or create a busy refresh loop.
    clock_ms = 0xfffffff0;
    Update_KMod();
    clock_ms = 0x53; Update_KMod_Periodic(); assert(panel_calls[0] == 14);
    clock_ms = 0x54; Update_KMod_Periodic(); assert(panel_calls[0] == 15);
    // System-specific panels retain their activation rules.
    SegaCD_Started = CD_32X_Active = 0; _32X_Started = 1;
    Update_KMod();
    assert(panel_calls[0] == 16 && panel_calls[10] == 15 && panel_calls[15] == 16);
    _32X_Started = 0; Update_KMod();
    assert(panel_calls[0] == 17 && panel_calls[10] == 15 && panel_calls[15] == 16);
    puts("PASS: bounded panel refresh, immediate manual refresh, unchanged action hooks, stalls, tick wrap, system gating");
}
'''
cpp = out / "kmod_refresh_test.cpp"
cpp.write_text(harness)
env = {k.upper(): v for k, v in os.environ.items()}
vswhere = Path(env["PROGRAMFILES(X86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
vs = Path(subprocess.check_output([
    str(vswhere), "-latest", "-products", "*", "-requires",
    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"
], text=True, env=env).strip())

def compile_and_run(name, sources, extra=()):
    command = ["cl", "/nologo", "/O2", "/MT", "/EHsc", "/D_CRT_SECURE_NO_WARNINGS",
               "/Fo" + str(out) + "\\", "/Fe" + str(out / (name + ".exe"))]
    command += [str(p) for p in sources] + list(extra)
    subprocess.run('call "' + str(vs / "VC/Auxiliary/Build/vcvars32.bat") + '" >nul && ' +
                   subprocess.list2cmdline(command), shell=True, cwd=root, env=env, check=True)
    subprocess.run([str(out / (name + ".exe"))], cwd=out, check=True, timeout=30)

compile_and_run("kmod_refresh_test", [cpp])
compile_and_run("vdp32x_window_test", [root / "tests/vdp32x_window_test.c"], [
    "/link", str(root / "src/Gens/bin/obj/Release/Gens.res"),
    "user32.lib", "gdi32.lib", "comdlg32.lib", "comctl32.lib"
])
compile_and_run("vdp_window_test", [root / "tests/vdp_window_test.c"], [
    "/link", str(root / "src/Gens/bin/obj/Release/Gens.res"),
    "user32.lib", "gdi32.lib", "comdlg32.lib"
])
