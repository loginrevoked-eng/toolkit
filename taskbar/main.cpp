#include "taskbar_controller.hpp"

#include <iostream>
#include <string_view>

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 2) {
        std::wcerr << L"Usage: taskbar.exe hide|show\n";
        return 1;
    }

    TaskbarController controller;
    std::wstring_view command{argv[1]};

    if (command == L"hide") {
        return controller.hide_taskbar() ? 0 : 2;
    }

    if (command == L"show") {
        return controller.show_taskbar() ? 0 : 2;
    }

    std::wcerr << L"Unknown command: " << command << L"\n";
    std::wcerr << L"Usage: taskbar.exe hide|show\n";
    return 1;
}
