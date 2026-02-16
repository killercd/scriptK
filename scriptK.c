/*
 * scriptK - Scrittura rapida e simulazione copia/incolla dove la clipboard è disabilitata.
 * C puro, solo API native Windows.
 * Compilazione (MSVC): cl scriptK.c user32.lib
 * Compilazione (MinGW): x86_64-w64-mingw32-gcc scriptK.c -o scriptK.exe -luser32 -mwindows -municode
 */

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

#define IDC_TEXTAREA     101
#define IDC_SECONDS      102
#define IDC_BUTTON_START 103
#define IDC_SELECTALL    104
#define IDC_COPY         105

#define TYPING_DELAY_MS  10

typedef struct {
    WCHAR *text;
    int   seconds;
} ThreadParams;

static HWND g_hwndText;
static HWND g_hwndSeconds;

static void SendKeyChar(WCHAR ch)
{
    INPUT inputs[2] = { 0 };

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = 0;
    inputs[0].ki.wScan = ch;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
    inputs[0].ki.time = 0;
    inputs[0].ki.dwExtraInfo = 0;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0;
    inputs[1].ki.wScan = ch;
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    inputs[1].ki.time = 0;
    inputs[1].ki.dwExtraInfo = 0;

    SendInput(2, inputs, sizeof(INPUT));
}

static void SendEnter(void)
{
    INPUT inputs[2] = { 0 };

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_RETURN;
    inputs[0].ki.wScan = 0;
    inputs[0].ki.dwFlags = 0;
    inputs[0].ki.time = 0;
    inputs[0].ki.dwExtraInfo = 0;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_RETURN;
    inputs[1].ki.wScan = 0;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[1].ki.time = 0;
    inputs[1].ki.dwExtraInfo = 0;

    SendInput(2, inputs, sizeof(INPUT));
}

static DWORD WINAPI TypingThread(LPVOID param)
{
    ThreadParams *p = (ThreadParams *)param;
    WCHAR *text = p->text;
    int seconds = p->seconds;

    if (seconds > 0)
        Sleep(seconds * 1000);

    for (; *text; text++) {
        if (*text == L'\n') {
            SendEnter();
        } else if (*text == L'\r') {
            if (*(text + 1) == L'\n')
                text++;
            SendEnter();
        } else {
            SendKeyChar(*text);
        }
        Sleep(TYPING_DELAY_MS);
    }

    HeapFree(GetProcessHeap(), 0, p->text);
    HeapFree(GetProcessHeap(), 0, p);
    return 0;
}

static void OnStart(void)
{
    int len, seconds_val;
    WCHAR *buf;
    WCHAR sec_buf[32];
    ThreadParams *params;
    HANDLE thread;

    len = GetWindowTextLengthW(g_hwndText);
    if (len <= 0) {
        MessageBoxW(NULL, L"Data error", L"Input Error", MB_OK | MB_ICONWARNING);
        return;
    }

    buf = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (size_t)(len + 1) * sizeof(WCHAR));
    if (!buf) {
        MessageBoxW(NULL, L"Memory error", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    GetWindowTextW(g_hwndText, buf, len + 1);

    /* Trim trailing CR/LF from multiline edit */
    while (len > 0 && (buf[len - 1] == L'\r' || buf[len - 1] == L'\n'))
        buf[--len] = L'\0';
    if (len <= 0) {
        HeapFree(GetProcessHeap(), 0, buf);
        MessageBoxW(NULL, L"Data error", L"Input Error", MB_OK | MB_ICONWARNING);
        return;
    }

    GetWindowTextW(g_hwndSeconds, sec_buf, (int)(sizeof(sec_buf) / sizeof(WCHAR)));
    seconds_val = (int)wcstol(sec_buf, NULL, 10);
    if (seconds_val < 0) {
        HeapFree(GetProcessHeap(), 0, buf);
        MessageBoxW(NULL, L"Wrong waiting number", L"Input Error", MB_OK | MB_ICONERROR);
        return;
    }

    params = (ThreadParams *)HeapAlloc(GetProcessHeap(), 0, sizeof(ThreadParams));
    if (!params) {
        HeapFree(GetProcessHeap(), 0, buf);
        MessageBoxW(NULL, L"Memory error", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    params->text = buf;
    params->seconds = seconds_val;

    {
        WCHAR msg[64];
        wsprintfW(msg, L"Waiting %d seconds...", seconds_val);
        MessageBoxW(NULL, msg, L"Info", MB_OK | MB_ICONINFORMATION);
    }

    thread = CreateThread(NULL, 0, TypingThread, params, 0, NULL);
    if (thread)
        CloseHandle(thread);
    else {
        HeapFree(GetProcessHeap(), 0, buf);
        HeapFree(GetProcessHeap(), 0, params);
        MessageBoxW(NULL, L"Could not start typing thread", L"Error", MB_OK | MB_ICONERROR);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        int padx = 10, pady = 5;
        int y = 10;

        CreateWindowW(L"Static", L"Text data:", WS_CHILD | WS_VISIBLE,
                      padx, y, 200, 18, hwnd, NULL, NULL, NULL);
        y += 18 + pady;

        g_hwndText = CreateWindowW(L"Edit", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL
            | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            padx, y, 480, 120, hwnd, (HMENU)(INT_PTR)IDC_TEXTAREA, NULL, NULL);
        y += 120 + pady;

        CreateWindowW(L"Static", L"Waiting seconds:", WS_CHILD | WS_VISIBLE,
                      padx, y, 200, 18, hwnd, NULL, NULL, NULL);
        y += 18 + pady;

        g_hwndSeconds = CreateWindowW(L"Edit", L"0",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            padx, y, 120, 22, hwnd, (HMENU)(INT_PTR)IDC_SECONDS, NULL, NULL);
        y += 22 + 10;

        CreateWindowW(L"Button", L"Start", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      padx, y, 80, 28, hwnd, (HMENU)(INT_PTR)IDC_BUTTON_START, NULL, NULL);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_SELECTALL) {
            HWND focus = GetFocus();
            if (focus == g_hwndText || focus == g_hwndSeconds)
                SendMessageW(focus, EM_SETSEL, 0, -1);
            return 0;
        }
        if (LOWORD(wParam) == IDC_COPY) {
            HWND focus = GetFocus();
            if (focus == g_hwndText || focus == g_hwndSeconds)
                SendMessageW(focus, WM_COPY, 0, 0);
            return 0;
        }
        if (LOWORD(wParam) == IDC_BUTTON_START && HIWORD(wParam) == BN_CLICKED) {
            OnStart();
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    WNDCLASSEXW wc = { 0 };
    HWND hwnd;
    MSG msg;
    ACCEL accels[] = {
        { FCONTROL, 'A', IDC_SELECTALL },
        { FCONTROL, 'C', IDC_COPY }
    };
    HACCEL hAccel;

    (void)hPrevInstance;
    (void)pCmdLine;

    hAccel = CreateAcceleratorTableW(accels, 2);

    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ScriptKWindow";

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"RegisterClass failed", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    hwnd = CreateWindowExW(0, L"ScriptKWindow", L"Flipper script",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 540, 390,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"CreateWindow failed", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAcceleratorW(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (hAccel)
        DestroyAcceleratorTable(hAccel);
    return (int)msg.wParam;
}
