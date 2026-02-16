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
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define IDC_TEXTAREA     101
#define IDC_SECONDS      102
#define IDC_BUTTON_START 103
#define IDC_SELECTALL    104
#define IDC_COPY         105

#define TYPING_DELAY_MS  10
#define WM_APP_RUN_DEBUG (WM_APP + 0)

#define DEBUG_OUTPUT_FILE L"output.txt"
#define DEBUG_BUFFER_INIT 4096

typedef struct {
    WCHAR *text;
    int   seconds;
    int   debug_mode;
    WCHAR *input_path;
    HWND  hwnd_main;     /* when debug_mode: close window when done */
} ThreadParams;

static HWND g_hwndText;
static HWND g_hwndSeconds;

/* Debug mode: set when --debug <file> is passed */
static int g_debug_mode;
static WCHAR *g_debug_file_content;   /* file content to put in text area, heap-allocated */
static WCHAR *g_debug_input_path;     /* path for comparison / display, heap-allocated */

/* Allocate and normalize line endings: \r\n and \r -> \n. Caller must HeapFree result. */
static WCHAR *NormalizeLineEndings(const WCHAR *src)
{
    size_t n = 0, i, j;
    const WCHAR *p;
    WCHAR *out;
    for (p = src; *p; p++) {
        if (*p == L'\r' && *(p + 1) == L'\n') { n++; p++; }
        else if (*p == L'\r' || *p == L'\n') n++;
        n++;
    }
    out = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (n + 1) * sizeof(WCHAR));
    if (!out) return NULL;
    for (i = 0, j = 0; src[i]; ) {
        if (src[i] == L'\r' && src[i + 1] == L'\n') { out[j++] = L'\n'; i += 2; }
        else if (src[i] == L'\r' || src[i] == L'\n') { out[j++] = L'\n'; i++; }
        else out[j++] = src[i++];
    }
    out[j] = L'\0';
    return out;
}

/* Read file to heap-allocated WCHAR*. Handles UTF-16 LE BOM and UTF-8. Returns NULL on error. */
static WCHAR *ReadFileToWide(const WCHAR *path)
{
    HANDLE h;
    DWORD size, read;
    char *raw = NULL;
    WCHAR *out = NULL;
    int utf8 = 0;
    int wide_size;

    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    size = GetFileSize(h, NULL);
    if (size == (DWORD)-1 || size == 0) { CloseHandle(h); return NULL; }
    raw = (char *)HeapAlloc(GetProcessHeap(), 0, size);
    if (!raw) { CloseHandle(h); return NULL; }
    if (!ReadFile(h, raw, size, &read, NULL) || read != size) {
        HeapFree(GetProcessHeap(), 0, raw);
        CloseHandle(h);
        return NULL;
    }
    CloseHandle(h);

    if (size >= 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE) {
        /* UTF-16 LE BOM */
        wide_size = (int)((size - 2) / sizeof(WCHAR));
        out = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (wide_size + 1) * sizeof(WCHAR));
        if (out) {
            memcpy(out, raw + 2, (size_t)wide_size * sizeof(WCHAR));
            out[wide_size] = L'\0';
        }
    } else {
        /* Assume UTF-8 (skip UTF-8 BOM if present) */
        utf8 = 1;
        if (size >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
            raw += 3;
            size -= 3;
        }
        wide_size = MultiByteToWideChar(CP_UTF8, 0, raw, (int)size, NULL, 0);
        if (wide_size <= 0) { HeapFree(GetProcessHeap(), 0, raw); return NULL; }
        out = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (wide_size + 1) * sizeof(WCHAR));
        if (out) {
            MultiByteToWideChar(CP_UTF8, 0, raw, (int)size, out, wide_size);
            out[wide_size] = L'\0';
        }
    }
    HeapFree(GetProcessHeap(), 0, raw);
    return out;
}

/* Write wide string to file as UTF-8. */
static int WriteWideToFileUtf8(const WCHAR *path, const WCHAR *text)
{
    HANDLE h;
    int len = 0;
    int n;
    char *utf8 = NULL;
    DWORD written;

    while (text[len]) len++;
    n = WideCharToMultiByte(CP_UTF8, 0, text, len, NULL, 0, NULL, NULL);
    if (n <= 0) return 0;
    utf8 = (char *)HeapAlloc(GetProcessHeap(), 0, (size_t)n);
    if (!utf8) return 0;
    WideCharToMultiByte(CP_UTF8, 0, text, len, utf8, n, NULL, NULL);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { HeapFree(GetProcessHeap(), 0, utf8); return 0; }
    WriteFile(h, utf8, (DWORD)n, &written, NULL);
    CloseHandle(h);
    HeapFree(GetProcessHeap(), 0, utf8);
    return (written == (DWORD)n);
}

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
    WCHAR *buffer = NULL;
    size_t buf_cap = 0, buf_len = 0;

    if (seconds > 0)
        Sleep(seconds * 1000);

    if (p->debug_mode) {
        buf_cap = DEBUG_BUFFER_INIT;
        buffer = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, buf_cap * sizeof(WCHAR));
        if (!buffer) goto cleanup;
    }

    for (; *text; text++) {
        if (*text == L'\n') {
            if (p->debug_mode) {
                if (buf_len >= buf_cap) {
                    buf_cap *= 2;
                    buffer = (WCHAR *)HeapReAlloc(GetProcessHeap(), 0, buffer, buf_cap * sizeof(WCHAR));
                    if (!buffer) goto cleanup;
                }
                buffer[buf_len++] = L'\n';
            } else {
                SendEnter();
            }
        } else if (*text == L'\r') {
            if (*(text + 1) == L'\n')
                text++;
            if (p->debug_mode) {
                if (buf_len >= buf_cap) {
                    buf_cap *= 2;
                    buffer = (WCHAR *)HeapReAlloc(GetProcessHeap(), 0, buffer, buf_cap * sizeof(WCHAR));
                    if (!buffer) goto cleanup;
                }
                buffer[buf_len++] = L'\n';
            } else {
                SendEnter();
            }
        } else {
            if (p->debug_mode) {
                if (buf_len >= buf_cap) {
                    buf_cap *= 2;
                    buffer = (WCHAR *)HeapReAlloc(GetProcessHeap(), 0, buffer, buf_cap * sizeof(WCHAR));
                    if (!buffer) goto cleanup;
                }
                buffer[buf_len++] = *text;
            } else {
                SendKeyChar(*text);
            }
        }
        if (!p->debug_mode)
            Sleep(TYPING_DELAY_MS);
    }

    if (p->debug_mode && buffer) {
        buffer[buf_len] = L'\0';
        WriteWideToFileUtf8(DEBUG_OUTPUT_FILE, buffer);
        {
            WCHAR *expected = NormalizeLineEndings(p->text);
            int ok = 0;
            if (expected) {
                size_t ex_len = 0;
                while (expected[ex_len]) ex_len++;
                if (ex_len == buf_len && memcmp(expected, buffer, buf_len * sizeof(WCHAR)) == 0)
                    ok = 1;
                HeapFree(GetProcessHeap(), 0, expected);
            }
            if (ok) {
                printf("DEBUG ok\n");
                WriteWideToFileUtf8(L"debug_result.txt", L"DEBUG ok");
            } else {
                printf("DEBUG ko\n");
                WriteWideToFileUtf8(L"debug_result.txt", L"DEBUG ko");
            }
            fflush(stdout);
        }
        if (p->hwnd_main)
            PostMessageW(p->hwnd_main, WM_CLOSE, 0, 0);
    }

cleanup:
    if (buffer) HeapFree(GetProcessHeap(), 0, buffer);
    HeapFree(GetProcessHeap(), 0, p->text);
    if (p->input_path) HeapFree(GetProcessHeap(), 0, p->input_path);
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

    /* Trim trailing CR/LF from multiline edit (skip in debug so output matches input file) */
    if (!g_debug_mode) {
        while (len > 0 && (buf[len - 1] == L'\r' || buf[len - 1] == L'\n'))
            buf[--len] = L'\0';
        if (len <= 0) {
            HeapFree(GetProcessHeap(), 0, buf);
            MessageBoxW(NULL, L"Data error", L"Input Error", MB_OK | MB_ICONWARNING);
            return;
        }
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
    params->debug_mode = g_debug_mode;
    params->input_path = NULL;
    params->hwnd_main = NULL;
    if (g_debug_mode) {
        params->hwnd_main = GetParent(g_hwndText);  /* main window is parent of the edit */
        if (g_debug_input_path) {
            size_t plen = wcslen(g_debug_input_path) + 1;
            params->input_path = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, plen * sizeof(WCHAR));
            if (params->input_path)
                wcscpy(params->input_path, g_debug_input_path);
        }
    }

    if (!g_debug_mode) {
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
        if (g_debug_mode && g_debug_file_content) {
            SetWindowTextW(g_hwndText, g_debug_file_content);
            SetWindowTextW(g_hwndSeconds, L"1");
            PostMessageW(hwnd, WM_APP_RUN_DEBUG, 0, 0);
        }
        return 0;
    }
    case WM_APP_RUN_DEBUG:
        OnStart();
        return 0;
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

/* Parse command line for --debug <path>. Returns 1 if debug mode, 0 otherwise. Sets globals. */
static int ParseDebugArgs(void)
{
    PWSTR cmd = GetCommandLineW();
    PWSTR p, start;
    size_t len;

    if (!cmd) return 0;
    p = cmd;
    while (*p == L' ' || *p == L'\t') p++;
    if (*p == L'"') { p++; while (*p && *p != L'"') p++; if (*p) p++; }
    else while (*p && *p != L' ' && *p != L'\t') p++;
    while (*p == L' ' || *p == L'\t') p++;
    if (p[0] != L'-' || p[1] != L'-' || p[2] != L'd' || p[3] != L'e' || p[4] != L'b' || p[5] != L'u' || p[6] != L'g' || (p[7] != L'\0' && p[7] != L' ' && p[7] != L'\t'))
        return 0;
    p += 7;
    while (*p == L' ' || *p == L'\t') p++;
    if (!*p) return 0;
    start = p;
    if (*p == L'"') {
        start = ++p;
        while (*p && *p != L'"') p++;
    } else {
        while (*p && *p != L' ' && *p != L'\t') p++;
    }
    len = (size_t)(p - start);
    if (len == 0) return 0;
    g_debug_input_path = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
    if (!g_debug_input_path) return 0;
    memcpy(g_debug_input_path, start, len * sizeof(WCHAR));
    g_debug_input_path[len] = L'\0';
    g_debug_file_content = ReadFileToWide(g_debug_input_path);
    if (!g_debug_file_content) {
        HeapFree(GetProcessHeap(), 0, g_debug_input_path);
        g_debug_input_path = NULL;
        return 0;
    }
    g_debug_mode = 1;
    return 1;
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

    if (ParseDebugArgs()) {
        /* Use parent console if run from terminal, else open new console */
        if (!AttachConsole(ATTACH_PARENT_PROCESS))
            AllocConsole();
        (void)freopen("CONOUT$", "w", stdout);
    }

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
