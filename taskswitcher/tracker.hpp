#ifndef TRACKER_HPP
#define TRACKER_HPP

#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

struct WindowInfo {
    HWND hwnd;
    std::wstring title;
    DWORD processId;
    std::string processName;
};

class ForegroundWindowTracker {
private:
    std::unordered_map<HWND, WindowInfo> windows;
    std::vector<HWND> mruList;
    std::vector<WindowInfo> mruWindows;
    std::vector<std::string> mruStringList;
    std::vector<HWND> lastPrintedMRU;

    std::mutex windowsMutex;
    std::mutex mruMutex;

    HHOOK keyboardHook = nullptr;
    HWINEVENTHOOK eventHook = nullptr;

    bool ctrlPressed = false;
    bool altPressed = false;

    std::function<void(std::vector<std::string>&)> onHotkey;

    static ForegroundWindowTracker* s_instance;

    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    BOOL handleEnumWindow(HWND hwnd);

    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hWinEventHook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD dwEventThread,
        DWORD dwmsEventTime
    );
    void handleForegroundChange(DWORD event, HWND hwnd);

    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    LRESULT handleKeyboardEvent(int nCode, WPARAM wParam, LPARAM lParam);

    std::string formatMruEntryFromWindowInfo(WindowInfo wInfo);
    void populateMruList();

public:
    ForegroundWindowTracker() = default;
    ~ForegroundWindowTracker();

    void SetHotkeyCallback(std::function<void(std::vector<std::string>&)> callback);
    bool RegisterHooks();
    void refreshMru();
    void Init();
    void eventLoop();
    void Unhook();
};

#endif // TRACKER_HPP