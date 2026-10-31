// Used by run_controller_input_tests.py; production functions are inserted below.
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "G_Input.h"
#include "resource.h"

#define MAX_JOYS 8
#define KEYDOWN(key) (Keys[key] & 0x80)
struct FakeDevice {
    bool keyboard;
    HRESULT pollResult, stateResult, acquireResult;
    unsigned polls, reads, acquires, releases;
    BYTE keys[256];
    DIJOYSTATE state;
    HRESULT Poll() { ++polls; return pollResult; }
    HRESULT GetDeviceState(DWORD n, void *p) {
        ++reads;
        if (FAILED(stateResult)) return stateResult;
        memcpy(p, keyboard ? (void *)keys : (void *)&state, n);
        return stateResult;
    }
    HRESULT Acquire() {
        ++acquires;
        if (SUCCEEDED(acquireResult)) pollResult = stateResult = DI_OK;
        return acquireResult;
    }
    void Unacquire() {}
    void Release() { assert(++releases == 1); }
} keyboard, joystick, directInput;
FakeDevice *lpDI, *lpDIDKeyboard, *lpDIDMouse, *Joy_ID[MAX_JOYS];
DIJOYSTATE Joy_State[MAX_JOYS];
bool Joy_Valid[MAX_JOYS], Keyboard_Valid;
unsigned char Keys[256];
K_Def Keys_Def[8];
int Nb_Joys;

HINSTANCE ghInstance;
HWND HWnd, testForeground;
int Full_Screen, Controller_1_Type, Controller_1B_Type, Controller_1C_Type, Controller_1D_Type;
int Controller_2_Type, Controller_2B_Type, Controller_2C_Type, Controller_2D_Type;
char Manual_Path[256], Str_Tmp[1024];
unsigned serviced;
bool failTimer;
void Make_IO_Table() {}
int Detect_Format(char *) { return -1; }
#define WORD_L(...) ((void)0)

void neutral() {
    memset(keyboard.keys, 0, sizeof(keyboard.keys));
    memset(&joystick.state, 0, sizeof(joystick.state));
    for (int i = 0; i < 4; ++i) joystick.state.rgdwPOV[i] = 0xffffffff;
}
int Init_Input(HINSTANCE, HWND dialog) {
    memset(&keyboard, 0, sizeof(keyboard));
    memset(&joystick, 0, sizeof(joystick));
    memset(&directInput, 0, sizeof(directInput));
    keyboard.keyboard = true;
    lpDI = &directInput;
    lpDIDKeyboard = &keyboard;
    Joy_ID[0] = &joystick;
    Nb_Joys = 1;
    neutral();
    testForeground = dialog;
    return 1;
}
HWND TestForeground() { return testForeground; }
BOOL HiddenPosition(HWND h, HWND after, int x, int y, int cx, int cy, UINT flags) {
    return SetWindowPos(h, after, x, y, cx, cy, (flags & ~SWP_SHOWWINDOW) | SWP_NOACTIVATE);
}
BOOL ModelessEnd(HWND h, INT_PTR) { return DestroyWindow(h); }
UINT_PTR TestTimer(HWND h, UINT_PTR id, UINT ms, TIMERPROC proc) {
    return failTimer ? 0 : SetTimer(h, id, ms, proc);
}
#define GetForegroundWindow TestForeground
#define SetWindowPos HiddenPosition
#define EndDialog ModelessEnd
#define SetTimer TestTimer

// PRODUCTION_FUNCTIONS

INT_PTR CALLBACK DialogProc(HWND dialog, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_APP) { ++serviced; return TRUE; }
    return ControllerProc(dialog, msg, wp, lp);
}
void pump() {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
void tick(HWND dialog) { SendMessage(dialog, WM_TIMER, KEY_SETTING_TIMER, 0); }
void click(HWND dialog, int id) {
    SendMessage(dialog, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(dialog, id));
}
HWND openDialog() {
    HWND dialog = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_CONTROLLER), NULL, DialogProc);
    assert(dialog);
    return dialog;
}
void start(HWND dialog) {
    click(dialog, IDC_BUTTON_SETKEYSP1);
    assert(KeySetting.dialog == dialog);
    neutral(); tick(dialog);
    assert(!KeySetting.waitRelease);
}
void closeDialog(HWND dialog) {
    SendMessage(dialog, WM_CLOSE, 0, 0);
    assert(!IsWindow(dialog) && !KeySetting.dialog);
    End_Input(); // Repeat teardown must not release stale device pointers.
    assert(keyboard.releases == 1 && joystick.releases == 1 && directInput.releases == 1);
}
bool textIs(HWND dialog, int id, const char *expected) {
    char actual[256];
    GetDlgItemText(dialog, id, actual, sizeof(actual));
    return !strcmp(actual, expected);
}

int main() {
    ghInstance = GetModuleHandle(NULL);
    memset(Keys_Def, 0x5a, sizeof(Keys_Def));
    K_Def original[8]; memcpy(original, Keys_Def, sizeof(original));
    HWND dialog = openDialog();
    start(dialog);
    // A real timer and queued UI work continue to run while there is no input.
    unsigned initialReads = keyboard.reads;
    PostMessage(dialog, WM_APP, 0, 0);
    DWORD until = GetTickCount() + 180;
    while ((LONG)(GetTickCount() - until) < 0) { pump(); Sleep(2); }
    assert(serviced == 1 && keyboard.reads > initialReads && keyboard.reads < initialReads + 20);
    assert(KeySetting.step == 0);
    assert(textIs(dialog, IDOK, "Cancel"));
    assert(!IsWindowEnabled(GetDlgItem(dialog, IDC_BUTTON_SETKEYSP2)));
    joystick.state.rgdwPOV[0] = 0; tick(dialog);
    assert(KeySetting.step == 1);
    for (int i = 0; i < 20; ++i) tick(dialog);
    assert(KeySetting.step == 1); // Held UP cannot silently become DOWN.
    assert(!memcmp(Keys_Def, original, sizeof(original)));
    neutral(); tick(dialog);
    joystick.state.rgdwPOV[0] = 13500; tick(dialog);
    assert(KeySetting.step == 2 && KeySetting.pending.Down == 0x1083);
    assert(Check_Key_Pressed(0x1083) && Check_Key_Pressed(0x1082));
    click(dialog, IDOK); // The Cancel button discards partial bindings.
    assert(!KeySetting.dialog && !memcmp(Keys_Def, original, sizeof(original)));
    assert(textIs(dialog, IDOK, "OK") && IsWindowEnabled(GetDlgItem(dialog, IDC_BUTTON_SETKEYSP2)));
    initialReads = keyboard.reads; tick(dialog);
    assert(keyboard.reads == initialReads); // Already queued timer after cancellation.
    puts("PASS: timer pacing, responsive message queue, held controls, diagonal DOWN, atomic cancellation");

    start(dialog);
    joystick.state.rgbButtons[0] = 0x80; tick(dialog);
    joystick.pollResult = DIERR_INPUTLOST;
    joystick.acquireResult = DIERR_OTHERAPPHASPRIO;
    unsigned retries = joystick.acquires;
    for (int i = 0; i < 5; ++i) tick(dialog);
    assert(joystick.acquires == retries + 5 && !Joy_Valid[0]);
    assert(!Check_Key_Pressed(0x1010) && Joy_State[0].rgdwPOV[0] == 0xffffffff);
    assert(KeySetting.waitRelease && KeySetting.step == 1);
    assert(textIs(dialog, IDC_STATIC_TEXT2, "Input unavailable. Reconnect or cancel."));
    joystick.acquireResult = DI_OK; tick(dialog);
    assert(Joy_Valid[0] && KeySetting.waitRelease && KeySetting.step == 1);
    neutral(); tick(dialog);
    assert(!KeySetting.waitRelease);
    // A failed GetDeviceState (even with successful Poll) is also handled.
    joystick.stateResult = E_FAIL; tick(dialog);
    assert(!Joy_Valid[0] && Pressed_Key(0) == 0);
    assert(textIs(dialog, IDC_STATIC_TEXT2, "Input unavailable. Reconnect or cancel."));
    joystick.stateResult = DI_OK;
    testForeground = NULL;
    joystick.state.rgbButtons[1] = 0x80; tick(dialog);
    assert(KeySetting.step == 1);
    testForeground = dialog; tick(dialog);
    assert(KeySetting.step == 1 && KeySetting.waitRelease);
    neutral(); tick(dialog);
    joystick.state.rgbButtons[1] = 0x80; tick(dialog);
    assert(KeySetting.step == 2);
    // Close remains available even if input cannot be reacquired.
    joystick.pollResult = DIERR_INPUTLOST;
    joystick.acquireResult = DIERR_OTHERAPPHASPRIO;
    PostMessage(dialog, WM_CLOSE, 0, 0); pump();
    assert(!IsWindow(dialog) && !KeySetting.dialog && !memcmp(Keys_Def, original, sizeof(original)));
    End_Input();
    puts("PASS: lost input, bounded retries, stale-state clearing, recovery, focus loss, close during failure");

    dialog = openDialog();
    keyboard.keys[DIK_A] = 0x80; Update_Input();
    assert(Check_Key_Pressed(DIK_A));
    keyboard.stateResult = DIERR_NOTACQUIRED; keyboard.acquireResult = DIERR_OTHERAPPHASPRIO;
    Update_Input(); assert(!Check_Key_Pressed(DIK_A) && !Keyboard_Valid);
    keyboard.acquireResult = DI_OK; Update_Input(); assert(Check_Key_Pressed(DIK_A));
    neutral();
    start(dialog);
    HWND capture = GetDlgItem(dialog, IDC_STATIC_TEXT1);
    assert(SendMessage(capture, WM_GETDLGCODE, 0, 0) & DLGC_WANTALLKEYS);
    const WPARAM dialogKeys[] = {VK_RETURN, VK_SPACE, VK_TAB, VK_UP};
    for (int i = 0; i < 4; ++i) {
        MSG msg = {}; msg.hwnd = capture; msg.message = WM_KEYDOWN; msg.wParam = dialogKeys[i];
        IsDialogMessage(dialog, &msg);
        assert(KeySetting.dialog == dialog && KeySetting.step == 0);
    }
    SendMessage(dialog, WM_COMMAND, IDOK, 0); // Default-button notification is not a mouse Cancel.
    assert(KeySetting.dialog == dialog);
    SendMessage(capture, WM_KEYUP, VK_ESCAPE, 0);
    assert(!KeySetting.dialog && !memcmp(Keys_Def, original, sizeof(original)));
    assert(!(SendMessage(capture, WM_GETDLGCODE, 0, 0) & DLGC_WANTALLKEYS));
    failTimer = true;
    click(dialog, IDC_BUTTON_SETKEYSP1);
    assert(!KeySetting.dialog && textIs(dialog, IDOK, "OK"));
    failTimer = false;
    puts("PASS: keyboard recovery, Enter/Space/Tab/arrows, Escape, timer failure cleanup");

    const unsigned bindings[] = {DIK_UP, DIK_DOWN, DIK_LEFT, DIK_RIGHT, DIK_RETURN,
        DIK_A, DIK_S, DIK_D, DIK_RSHIFT, DIK_Z, DIK_X, DIK_C};
    const int players[] = {0, 2, 3, 4, 1, 5, 6, 7};
    for (int slot = 0; slot < 8; ++slot) {
        int six = slot & 1, player = players[slot], count = six ? 12 : 8;
        SendDlgItemMessage(dialog, IDC_COMBO_PADP1 + slot, CB_SETCURSEL, six, 0);
        click(dialog, IDC_BUTTON_SETKEYSP1 + slot);
        assert(KeySetting.player == player && KeySetting.count == count);
        neutral(); tick(dialog);
        K_Def before = Keys_Def[player];
        for (int i = 0; i < count; ++i) {
            keyboard.keys[bindings[i]] = 0x80; tick(dialog);
            assert(!memcmp(&Keys_Def[player], &before, sizeof(before)));
            neutral(); tick(dialog);
        }
        assert(!KeySetting.dialog);
        K_Def &got = Keys_Def[player];
        assert(got.Up == DIK_UP && got.Down == DIK_DOWN && got.Left == DIK_LEFT && got.Right == DIK_RIGHT);
        assert(got.Start == DIK_RETURN && got.A == DIK_A && got.B == DIK_S && got.C == DIK_D);
        assert(got.Mode == (six ? DIK_RSHIFT : before.Mode) && got.X == (six ? DIK_Z : before.X));
        assert(got.Y == (six ? DIK_X : before.Y) && got.Z == (six ? DIK_C : before.Z));
    }
    // Complete both pad types entirely with POV directions and gamepad buttons.
    const DWORD padDirections[] = {31500, 13500, 22500, 4500};
    for (int six = 0; six < 2; ++six) {
        SendDlgItemMessage(dialog, IDC_COMBO_PADP1, CB_SETCURSEL, six, 0);
        start(dialog);
        K_Def before = Keys_Def[0];
        for (int i = 0; i < (six ? 12 : 8); ++i) {
            if (i < 4) joystick.state.rgdwPOV[0] = padDirections[i];
            else joystick.state.rgbButtons[i - 4] = 0x80;
            tick(dialog);
            assert(!memcmp(&Keys_Def[0], &before, sizeof(before)));
            neutral(); tick(dialog);
        }
        assert(!KeySetting.dialog);
        K_Def &got = Keys_Def[0];
        assert(got.Up == 0x1081 && got.Down == 0x1083 && got.Left == 0x1084 && got.Right == 0x1082);
        assert(got.Start == 0x1010 && got.A == 0x1011 && got.B == 0x1012 && got.C == 0x1013);
        assert(got.Mode == (six ? 0x1014 : before.Mode) && got.X == (six ? 0x1015 : before.X));
        assert(got.Y == (six ? 0x1016 : before.Y) && got.Z == (six ? 0x1017 : before.Z));
    }
    neutral(); joystick.state.lY = 1000; Update_Input();
    assert(Pressed_Key(0) == 0x1002 && Check_Key_Pressed(0x1002));
    for (DWORD angle = 0; angle < 36000; angle += 4500) {
        neutral(); joystick.state.rgdwPOV[0] = angle; Update_Input();
        unsigned directions = 0;
        for (unsigned d = 1; d <= 4; ++d) directions += !!Check_Key_Pressed(0x1080 + d);
        assert(directions == (angle % 9000 ? 2 : 1));
    }
    neutral(); Update_Input(); assert(!Pressed_Key(0));
    joystick.state.rgdwPOV[0] = 0xffff; Update_Input(); assert(!Pressed_Key(0));
    assert(!Check_Key_Pressed(0x1f81) && !Check_Key_Pressed(0x107f));
    closeDialog(dialog);
    puts("PASS: all eight players, three/six-button keyboard and gamepad completion, axes, gameplay diagonals, neutral POV, teardown");
}
