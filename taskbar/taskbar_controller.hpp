#pragma once

#include <windows.h>

class TaskbarController {
public:
    TaskbarController() = default;

    bool hide_taskbar();
    bool show_taskbar();

private:
    HWND taskbar_hwnd_ = nullptr;
    HWND start_hwnd_ = nullptr;
};
