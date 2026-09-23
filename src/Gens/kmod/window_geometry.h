#ifndef KMOD_WINDOW_GEOMETRY_H
#define KMOD_WINDOW_GEOMETRY_H

#include <windows.h>
#include <stdio.h>

static void DebugWindow_SaveGeometry(HWND hwnd, const char *key, const char *config_file)
{
	WINDOWPLACEMENT placement = { sizeof(placement) };
	MONITORINFO monitor = { sizeof(monitor) };
	RECT rect;
	char value[128];
	if (!GetWindowPlacement(hwnd, &placement)) return;
	rect = placement.rcNormalPosition;
	/* WINDOWPLACEMENT uses workspace coordinates for these non-tool windows. */
	if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
		OffsetRect(&rect, monitor.rcWork.left - monitor.rcMonitor.left,
			monitor.rcWork.top - monitor.rcMonitor.top);
	wsprintf(value, "%ld,%ld,%ld,%ld", rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top);
	WritePrivateProfileString("DebugWindows", key, value, config_file);
}

static void DebugWindow_RestoreGeometry(HWND hwnd, const char *key, const char *config_file)
{
	char value[128], extra;
	long x, y, width, height;
	RECT rect;
	MONITORINFO monitor = { sizeof(monitor) };
	GetPrivateProfileString("DebugWindows", key, "", value, sizeof(value), config_file);
	if (sscanf(value, "%ld,%ld,%ld,%ld%c", &x, &y, &width, &height, &extra) != 4)
		return;
	/* Reject corrupt settings before doing coordinate arithmetic. */
	if (x < -1000000 || x > 1000000 || y < -1000000 || y > 1000000 ||
		width <= 0 || width > 1000000 || height <= 0 || height > 1000000) return;
	SetRect(&rect, x, y, x + width, y + height);
	if (!GetMonitorInfo(MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST), &monitor)) return;
	width = min(max(width, GetSystemMetrics(SM_CXMINTRACK)), monitor.rcWork.right - monitor.rcWork.left);
	height = min(max(height, GetSystemMetrics(SM_CYMINTRACK)), monitor.rcWork.bottom - monitor.rcWork.top);
	x = max(monitor.rcWork.left, min(x, monitor.rcWork.right - width));
	y = max(monitor.rcWork.top, min(y, monitor.rcWork.bottom - height));
	SetWindowPos(hwnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

#endif
