"""Check the production emulation scheduler with a deterministic clock and frame callbacks."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'src/Gens/bin/speed-tests'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/Gens/G_ddraw.cpp').read_text()
function = source.split('int Update_Emulation(HWND hWnd)', 1)[1].split('int Update_Emulation_One', 1)[0]
harness = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
using HWND = void*;
unsigned int clock_ms, Last_Time, New_Time, Used_Time;
double Emulation_Speed;
int CPU_Mode, Frame_Skip, Frame_Number, Sound_Enable, Sleep_Time;
int WP, RP, Sound_Segs = 8, Seg_L[882], Seg_R[882];
int rendered, skipped, controls, flips;
unsigned int GetTickCount() { return clock_ms; }
void Sleep(int) {}
int Get_Current_Seg() { return 0; }
void Write_Sound_Buffer(void*) {}
void Update_Controllers() { ++controls; }
void Flip_GFX(HWND) { ++flips; }
int frame() { ++rendered; Seg_L[0] += 100; Seg_R[0] += 100; return 1; }
int fast() { ++skipped; Seg_L[0] += 100; Seg_R[0] += 100; return 1; }
int (*Update_Frame)() = frame;
int (*Update_Frame_Fast)() = fast;
int Update_Emulation(HWND hWnd)
'''
harness += function
harness += r'''
int main() {
 for (int pal = 0; pal < 2; ++pal)
 for (double speed : {0.125, 0.25, 0.5, 2.0, 4.0, 8.0})
 for (int sound = 0; sound < 2; ++sound)
 for (int skip = -1; skip <= 8; ++skip) {
  CPU_Mode = pal; Emulation_Speed = speed; Sound_Enable = sound; Frame_Skip = skip;
  Last_Time = clock_ms = Used_Time = 0;
  rendered = skipped = controls = flips = 0;
  for (clock_ms = 1; clock_ms <= 4000; ++clock_ms) Update_Emulation(nullptr);
  assert(rendered + skipped == (pal ? 50 : 60) * speed * 4);
  assert(controls == rendered + skipped);
  assert(flips == rendered && rendered == (pal ? 50 : 60) * 4 * (speed < 1 ? speed : 1));
  assert(Seg_L[0] == 0 && Seg_R[0] == 0);
 }
 // Long stalls have bounded catch-up work.
 CPU_Mode = 0; Emulation_Speed = 4; Last_Time = Used_Time = 0;
 rendered = skipped = 0; clock_ms = 10000; Update_Emulation(nullptr);
 assert(rendered + skipped == 24);
 // DWORD tick counter wrap preserves elapsed time.
 Last_Time = 0xfffffff0u; clock_ms = 4; Used_Time = 0;
 rendered = skipped = 0; Update_Emulation(nullptr);
 assert(rendered + skipped == 4);
 // Maximum speed also bounds catch-up work after a long stall.
 Emulation_Speed = 8; Last_Time = Used_Time = 0;
 rendered = skipped = 0; clock_ms = 10000; Update_Emulation(nullptr);
 assert(rendered + skipped == 48);
 // Normal speed remains paced with fixed frame skip and audio enabled.
 for (int pal = 0; pal < 2; ++pal)
 for (int sound = 0; sound < 2; ++sound)
 for (int skip = 0; skip <= 8; ++skip) {
  CPU_Mode = pal; Emulation_Speed = 1; Sound_Enable = sound;
  Frame_Skip = skip; Frame_Number = 0;
  Last_Time = clock_ms = Used_Time = 0;
  rendered = skipped = controls = flips = 0;
  for (clock_ms = 1; clock_ms <= 4000; ++clock_ms) Update_Emulation(nullptr);
  int total = (pal ? 50 : 60) * 4;
  assert(rendered + skipped == total);
  assert(rendered == total / (skip + 1));
  assert(controls == total && flips == rendered);
 }
 puts("PASS: speed/PAL/audio/frame-skip combinations, stalls, clock wrap, paced normal speed");
}
'''
cpp = out / 'speed_scheduler_test.cpp'
cpp.write_text(harness)
env = {k.upper(): v for k, v in os.environ.items()}
vswhere = Path(env['PROGRAMFILES(X86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = Path(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip())
command = ['cl', '/nologo', '/EHsc', '/MT', str(cpp), '/Fo' + str(out / 'test.obj'), '/Fe' + str(out / 'test.exe')]
subprocess.run('call "' + str(vs / 'VC/Auxiliary/Build/vcvars32.bat') + '" >nul && ' + subprocess.list2cmdline(command), shell=True, cwd=root, env=env, check=True)
subprocess.run([str(out / 'test.exe')], check=True)
