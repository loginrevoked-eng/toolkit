#include "logging.hpp"
#include "tracker.hpp"

ForegroundWindowTracker* ForegroundWindowTracker::s_instance = nullptr;

void ForegroundWindowTracker::Panic() {
    Logging::error("Panic mode activated!");
    std::exit(1);
}

BOOL CALLBACK ForegroundWindowTracker::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* self = reinterpret_cast<ForegroundWindowTracker*>(lParam);
    return self->handleEnumWindow(hwnd);
}

BOOL ForegroundWindowTracker::handleEnumWindow(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    int length = GetWindowTextLengthW(hwnd);
    if (length == 0) return TRUE;

    std::wstring title(length + 1, L'\0');
    GetWindowTextW(hwnd, &title[0], length + 1);
    title.resize(length);

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    std::string processName;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess) {
        wchar_t imagePath[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, imagePath, &size)) {
            wchar_t* exeName = PathFindFileNameW(imagePath);
            int reqSize = WideCharToMultiByte(CP_UTF8, 0, exeName, -1, NULL, 0, NULL, NULL);
            if (reqSize > 0) {
                processName.resize(reqSize - 1);
                WideCharToMultiByte(CP_UTF8, 0, exeName, -1, &processName[0], reqSize, NULL, NULL);
            }
        }
        CloseHandle(hProcess);
    }

    std::lock_guard<std::mutex> lock(windowsMutex);
    windows[hwnd] = WindowInfo{ hwnd, title, processId, processName };
    Logging::info("{}:{}", processName, wstring_to_string(title));
    return TRUE;
}

void CALLBACK ForegroundWindowTracker::WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime)
{
    if (s_instance) {
        s_instance->handleForegroundChange(event, hwnd);
    }
}

void ForegroundWindowTracker::handleForegroundChange(DWORD event, HWND hwnd) {
    if (event != EVENT_SYSTEM_FOREGROUND || hwnd == NULL) {
        return;
    }

    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) {
        return;
    }
    std::wstring title(len + 1, L'\0');
    GetWindowTextW(hwnd, &title[0], len + 1);
    title.resize(len);

    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    std::wstring processName = L"unknown";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) {
        Logging::debug("Failed to open process {} error:{}", processId, GetLastError());
    } else {
        wchar_t buffer[MAX_PATH];
        DWORD size = sizeof(buffer) / sizeof(buffer[0]);
        if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
            processName = std::wstring(buffer);
        } else {
            Logging::debug("Failed to get process name for process {} error:{}", processId, GetLastError());
        }
        CloseHandle(hProcess);
    }

    Logging::info("Foreground changed to:\n  hwnd={}\n  title={}\n  processId={}\n  processName={}",
        (void*)hwnd, wstring_to_string(title), processId, wstring_to_string(processName));

    {
        std::lock_guard<std::mutex> windowsLock(windowsMutex);
        windows[hwnd] = WindowInfo{ hwnd, title, processId, wstring_to_string(processName) };
    }

    std::lock_guard<std::mutex> lock(mruMutex);
    // move-to-front semantics
    auto it = std::find(mruList.begin(), mruList.end(), hwnd);
    if (it != mruList.end()) {
        mruList.erase(it);
    }
    mruList.insert(mruList.begin(), hwnd);
}

LRESULT CALLBACK ForegroundWindowTracker::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (s_instance) {
        return s_instance->handleKeyboardEvent(nCode, wParam, lParam);
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

std::string ForegroundWindowTracker::formatMruEntryFromWindowInfo(WindowInfo wInfo){
    std::string fmt = Logging::format(
        "{}: {}",
        wInfo.processName,
        wstring_to_string(wInfo.title)
    );
    Logging::info("{}", fmt);
    return fmt;
}

void ForegroundWindowTracker::populateMruList(){
    Logging::info("Populating MRU list");
    std::lock_guard<std::mutex> lockMru(mruMutex);
    std::lock_guard<std::mutex> lockWindows(windowsMutex);
    Logging::info("mruList size: {}, windows map size: {}", mruList.size(), windows.size());
    mruStringList.clear();
    for (auto &hwnd : mruList) {
        auto found = windows.find(hwnd);
        if(found != windows.end()) {
            std::string entry = formatMruEntryFromWindowInfo(found->second);
            mruStringList.push_back(entry);
            Logging::info("Added: {}", entry);
        } else {
            Logging::info("HWND {} not found in windows map", (void*)hwnd);
        }
    }
    Logging::info("Final mruStringList size: {}", mruStringList.size());
}

LRESULT ForegroundWindowTracker::handleKeyboardEvent(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* kbs = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (kbs->vkCode == VK_LCONTROL || kbs->vkCode == VK_RCONTROL) {
                ctrlPressed = true;
            }
            if (kbs->vkCode == VK_LMENU || kbs->vkCode == VK_RMENU) {
                altPressed = true;
            }
            if (ctrlPressed && altPressed && kbs->vkCode == 'S') {
                Logging::info("Hotkey Ctrl+Alt+S pressed - showing MRU list");
                if (onHotkey) {
                    populateMruList();
                    onHotkey(mruStringList);
                }
            }
        }

        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            if (kbs->vkCode == VK_LCONTROL || kbs->vkCode == VK_RCONTROL) {
                ctrlPressed = false;
            }
            if (kbs->vkCode == VK_LMENU || kbs->vkCode == VK_RMENU) {
                altPressed = false;
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

ForegroundWindowTracker::~ForegroundWindowTracker() {
    Unhook();
}

void ForegroundWindowTracker::SetHotkeyCallback(std::function<void(std::vector<std::string>&)> callback) {
    onHotkey = callback;
}

bool ForegroundWindowTracker::RegisterHooks() {
    s_instance = this;

    eventHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        NULL,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT
    );
    if (!eventHook) {
        Logging::error("SetWinEventHook failed");
        return false;
    }

    keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, NULL, 0);
    if (!keyboardHook) {
        Logging::error("SetWindowsHookEx failed for keyboard hook");
        UnhookWinEvent(eventHook);
        eventHook = NULL;
        return false;
    }

    Logging::info("Hooks installed successfully. Press Ctrl+Alt+S to show MRU list.");
    return true;
}

void ForegroundWindowTracker::refreshMru() {
    {
        std::lock_guard<std::mutex> lock(windowsMutex);
        windows.clear();
    }
    // handleEnumWindow() takes windowsMutex itself per-entry, so the lock
    // above must not still be held while EnumWindows is running.
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));
}

void ForegroundWindowTracker::Init(){
    refreshMru();
    RegisterHooks();
    std::thread eventLoopThread(&ForegroundWindowTracker::eventLoop, this);
    eventLoopThread.detach();
}

void ForegroundWindowTracker::eventLoop() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void ForegroundWindowTracker::Unhook() {
    if (eventHook) {
        UnhookWinEvent(eventHook);
        Logging::debug("Unhooked event hook");
        eventHook = NULL;
    }
    if (keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        Logging::debug("Unhooked keyboard hook");
        keyboardHook = NULL;
    }
    if (s_instance == this) {
        s_instance = nullptr;
        Logging::debug("Reset s_instance");
    }
    Logging::debug("All hooks have been uninstalled");
}