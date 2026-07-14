#include "taskbar_controller.hpp"

#include <iostream>

namespace {
constexpr int kSwHide = 0;
constexpr int kSwShow = 1;
}

bool TaskbarController::hide_taskbar() {
    taskbar_hwnd_ = ::FindWindowW(L"Shell_TrayWnd", nullptr);
    // This is historically the Start button class, but it is not stable on
    // newer Windows releases. Keep it as an optional best-effort lookup.
    start_hwnd_ = ::FindWindowW(L"Button", L"Start");

    if (!taskbar_hwnd_) {
        std::wcerr << L"Could not find taskbar window.\n";
        return false;
    }

    ::ShowWindow(taskbar_hwnd_, kSwHide);
    if (start_hwnd_) {
        ::ShowWindow(start_hwnd_, kSwHide);
    }

    return true;
}

bool TaskbarController::show_taskbar() {
    HWND hwnd = taskbar_hwnd_ ? taskbar_hwnd_ : ::FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!hwnd) {
        std::wcerr << L"Could not find taskbar window.\n";
        return false;
    }

    ::ShowWindow(hwnd, kSwShow);
    if (start_hwnd_) {
        ::ShowWindow(start_hwnd_, kSwShow);
    }

    return true;
}
