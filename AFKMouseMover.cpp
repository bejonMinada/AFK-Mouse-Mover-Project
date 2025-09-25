#include <windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include "Resource.h"
#include <shellapi.h> // Include for Shell_NotifyIcon

#pragma comment(lib, "Comctl32.lib")

// --- Control IDs ---
#define ID_INPUT 101
#define ID_START 102
#define ID_STOP  103
#define ID_STATUS 104

// --- Constants for the Tray Icon ---
#define WM_TRAYICON (WM_APP + 1)
#define IDM_RESTORE 110
#define IDM_EXIT_TRAY 111

// --- Constants ---
constexpr int kMinAFKSeconds = 5;
constexpr int kLoopSleepMs = 200;
constexpr int kMouseMovePixels = 10;

// --- Global Variables ---
HWND hInput, hStartBtn, hStopBtn, hStatus;
std::atomic<bool> isRunning = false;
DWORD afkTimeoutMs = 0;
NOTIFYICONDATA nid = {}; // For the tray icon

// --- Function Declarations ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void StartMonitoring(HWND hwnd);
void StopMonitoring();
void MonitorAFK();
void MoveMouseInSquare();
void UpdateStatus(const std::wstring& text);
DWORD GetLastInputTick();
void AddTrayIcon(HWND hwnd);
void RemoveTrayIcon(HWND hwnd);

// --- Tray Icon Helper Functions ---
void AddTrayIcon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1; // Unique ID for this icon
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON; // Our custom message
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_AFKMOUSEMOVER));
    wcscpy_s(nid.szTip, L"AFK Mouse Mover"); // Tooltip text
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hwnd) {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

// --- Other Function Implementations ---

DWORD GetLastInputTick() {
    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    return GetLastInputInfo(&lii) ? lii.dwTime : GetTickCount();
}

void MoveMouseInSquare() {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    int dx[] = { kMouseMovePixels, 0, -kMouseMovePixels, 0 };
    int dy[] = { 0, kMouseMovePixels, 0, -kMouseMovePixels };
    for (int i = 0; i < 4; ++i) {
        input.mi.dx = dx[i];
        input.mi.dy = dy[i];
        SendInput(1, &input, sizeof(INPUT));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void UpdateStatus(const std::wstring& text) {
    SetWindowTextW(hStatus, text.c_str());
}

void MonitorAFK() {
    DWORD lastInputTick = GetLastInputTick();
    while (isRunning) {
        DWORD currentInputTick = GetLastInputTick();
        if (currentInputTick != lastInputTick) {
            lastInputTick = currentInputTick;
            UpdateStatus(L"Status: Active");
        }
        else {
            DWORD idleTime = GetTickCount() - currentInputTick;
            if (idleTime >= afkTimeoutMs) {
                UpdateStatus(L"Status: AFK");
                MoveMouseInSquare();
                lastInputTick = GetLastInputTick();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kLoopSleepMs));
    }
    UpdateStatus(L"Status: Stopped");
}

void StartMonitoring(HWND hwnd) {
    int textLength = GetWindowTextLengthW(hInput);
    if (textLength == 0) {
        MessageBoxW(hwnd, L"Please enter a timeout value.", L"Input Required", MB_ICONWARNING | MB_OK);
        return;
    }

    // Corrected line: std::wstring
    std::wstring buffer(textLength + 1, L'\0');
    GetWindowTextW(hInput, &buffer[0], textLength + 1);
    buffer.resize(textLength);

    int seconds = _wtoi(buffer.c_str());
    if (seconds < kMinAFKSeconds) {
        std::wstring errorMsg = L"AFK timeout must be at least " + std::to_wstring(kMinAFKSeconds) + L" seconds.";
        MessageBoxW(hwnd, errorMsg.c_str(), L"Invalid Input", MB_ICONWARNING | MB_OK);
        return;
    }
    afkTimeoutMs = seconds * 1000;
    isRunning = true;
    EnableWindow(hInput, FALSE);
    EnableWindow(hStartBtn, FALSE);
    EnableWindow(hStopBtn, TRUE);
    UpdateStatus(L"Status: Monitoring...");
    std::thread(MonitorAFK).detach();
}

void StopMonitoring() {
    if (isRunning) {
        isRunning = false;
        EnableWindow(hInput, TRUE);
        EnableWindow(hStartBtn, TRUE);
        EnableWindow(hStopBtn, FALSE);
        UpdateStatus(L"Status: Stopped");
    }
}

// --- Main Window Procedure ---

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TRAYICON:
        switch (lParam) {
        case WM_LBUTTONDBLCLK:
            ShowWindow(hwnd, SW_RESTORE);
            break;
        case WM_RBUTTONUP:
        {
            POINT curPoint;
            GetCursorPos(&curPoint);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, IDM_RESTORE, L"Restore");
            AppendMenu(hMenu, MF_STRING, IDM_EXIT_TRAY, L"Exit");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, curPoint.x, curPoint.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;
        }
        break;

    case WM_SYSCOMMAND:
        if (wParam == SC_MINIMIZE) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_CREATE:
    {
        CreateWindowW(L"STATIC", L"AFK Timeout (sec):", WS_VISIBLE | WS_CHILD,
            20, 20, 120, 20, hwnd, NULL, NULL, NULL);
        hInput = CreateWindowW(L"EDIT", L"30", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP,
            150, 20, 100, 20, hwnd, (HMENU)ID_INPUT, NULL, NULL);
        hStartBtn = CreateWindowW(L"BUTTON", L"Start", WS_VISIBLE | WS_CHILD | WS_TABSTOP,
            270, 20, 80, 25, hwnd, (HMENU)ID_START, NULL, NULL);
        hStopBtn = CreateWindowW(L"BUTTON", L"Stop", WS_VISIBLE | WS_CHILD | WS_DISABLED | WS_TABSTOP,
            270, 55, 80, 25, hwnd, (HMENU)ID_STOP, NULL, NULL);
        hStatus = CreateWindowW(L"STATIC", L"Status: Idle", WS_VISIBLE | WS_CHILD,
            20, 60, 230, 20, hwnd, (HMENU)ID_STATUS, NULL, NULL);
        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_AFKMOUSEMOVER));
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

        AddTrayIcon(hwnd);
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_START:
            StartMonitoring(hwnd);
            break;
        case ID_STOP:
            StopMonitoring();
            break;
        case IDM_RESTORE:
            ShowWindow(hwnd, SW_RESTORE);
            break;
        case IDM_EXIT_TRAY:
            DestroyWindow(hwnd);
            break;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            if (IsWindowEnabled(hStartBtn)) StartMonitoring(hwnd);
        }
        else if (wParam == VK_ESCAPE) {
            if (IsWindowEnabled(hStopBtn)) StopMonitoring();
        }
        break;

    case WM_DESTROY:
        RemoveTrayIcon(hwnd);
        isRunning = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"AFKMouseMoverWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AFKMOUSEMOVER));

    if (!RegisterClassW(&wc)) {
        return 0;
    }

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"AFK Mouse Mover",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 160,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}