#include "logging.hpp"
#include "tracker.hpp"
#include <algorithm>
#include <thread>

ForegroundWindowTracker* ForegroundWindowTracker::s_instance = nullptr;

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
    std::wstring exePath;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess) {
        wchar_t imagePath[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, imagePath, &size)) {
            exePath = imagePath;
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
    windows[hwnd] = WindowInfo{ hwnd, title, processId, exePath, processName };
    Logging::slopyap("{}:{}", processName, wstring_to_string(title));
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
    std::wstring exePath;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) {
        Logging::debug("Failed to open process {} error:{}", processId, GetLastError());
    } else {
        wchar_t buffer[MAX_PATH];
        DWORD size = sizeof(buffer) / sizeof(buffer[0]);
        if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
            exePath = buffer;
            wchar_t* exeName = PathFindFileNameW(buffer);
            processName = exeName ? exeName : buffer;
        } else {
            Logging::debug("Failed to get process name for process {} error:{}", processId, GetLastError());
        }
        CloseHandle(hProcess);
    }

    Logging::slopyap("Foreground changed to:\n  hwnd={}\n  title={}\n  processId={}\n  processName={}",
        (void*)hwnd, wstring_to_string(title), processId, wstring_to_string(processName));

    std::scoped_lock lock(mruMutex, windowsMutex);
    windows[hwnd] = WindowInfo{ hwnd, title, processId, exePath, wstring_to_string(processName) };
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
    Logging::slopyap("{}", fmt);
    return fmt;
}

void ForegroundWindowTracker::populateMruList(){
    Logging::slopyap("Populating MRU list");
    std::scoped_lock lock(mruMutex, windowsMutex);
    Logging::slopyap("mruList size: {}, windows map size: {}", mruList.size(), windows.size());
    mruStringList.clear();
    mruWindows.clear();
    if (!mruList.empty()) {
        for (auto &hwnd : mruList) {
            auto found = windows.find(hwnd);
            if(found != windows.end()) {
                std::string entry = formatMruEntryFromWindowInfo(found->second);
                mruStringList.push_back(entry);
                mruWindows.push_back(found->second);
                Logging::slopyap("Added: {}", entry);
            } else {
                Logging::slopyap("HWND {} not found in windows map", (void*)hwnd);
            }
        }
    }

    if (mruStringList.empty() && !windows.empty()) {
        std::vector<WindowInfo> fallbackWindows;
        fallbackWindows.reserve(windows.size());
        for (const auto& [hwnd, info] : windows) {
            (void)hwnd;
            fallbackWindows.push_back(info);
        }

        std::sort(fallbackWindows.begin(), fallbackWindows.end(),
            [](const WindowInfo& a, const WindowInfo& b) {
                if (a.processName != b.processName) {
                    return a.processName < b.processName;
                }
                return a.title < b.title;
            });

        for (const auto& info : fallbackWindows) {
            mruStringList.push_back(formatMruEntryFromWindowInfo(info));
            mruWindows.push_back(info);
        }

        Logging::slopyap("mruList was empty; populated {} entries from current windows", mruStringList.size());
    }
    Logging::slopyap("Final mruStringList size: {}", mruStringList.size());
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
                    Logging::debug("Refreshing MRU on demand");
                    refreshMru();
                    populateMruList();
                    onHotkey(mruWindows);
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

void ForegroundWindowTracker::SetHotkeyCallback(std::function<void(std::vector<WindowInfo>)> callback) {
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
        s_instance = nullptr;
        return false;
    }

    keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, NULL, 0);
    if (!keyboardHook) {
        Logging::error("SetWindowsHookEx failed for keyboard hook");
        UnhookWinEvent(eventHook);
        eventHook = NULL;
        s_instance = nullptr;
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

std::vector<WindowInfo> ForegroundWindowTracker::GetMruWindowsSnapshot() {
    std::scoped_lock lock(mruMutex, windowsMutex);
    return mruWindows;
}

bool ForegroundWindowTracker::ActivateWindow(HWND hwnd) {
    if (!IsWindow(hwnd)) {
        Logging::error("ActivateWindow called with invalid hwnd {}", (void*)hwnd);
        return false;
    }

    Logging::info("Activating window hwnd={}", (void*)hwnd);

    DWORD targetPid = 0;
    DWORD targetThreadId = GetWindowThreadProcessId(hwnd, &targetPid);
    HWND foregroundWindow = GetForegroundWindow();
    DWORD foregroundThreadId = foregroundWindow ? GetWindowThreadProcessId(foregroundWindow, nullptr) : 0;
    DWORD currentThreadId = GetCurrentThreadId();

    bool attachedTarget = false;
    bool attachedForeground = false;

    if (targetThreadId != 0 && targetThreadId != currentThreadId) {
        if (AttachThreadInput(currentThreadId, targetThreadId, TRUE)) {
            attachedTarget = true;
        }
    }

    if (foregroundThreadId != 0 && foregroundThreadId != currentThreadId && foregroundThreadId != targetThreadId) {
        if (AttachThreadInput(currentThreadId, foregroundThreadId, TRUE)) {
            attachedForeground = true;
        }
    }

    if (IsIconic(hwnd)) {
        ShowWindowAsync(hwnd, SW_RESTORE);
    } else {
        ShowWindowAsync(hwnd, SW_SHOW);
    }

    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    if (attachedForeground) {
        AttachThreadInput(currentThreadId, foregroundThreadId, FALSE);
    }
    if (attachedTarget) {
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);
    }

    return true;
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
