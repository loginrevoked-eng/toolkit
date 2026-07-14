#include "tracker.hpp"
#include "ui.hpp"
#include "logging.hpp"
#include <atomic>
#include <chrono>
#include <thread>


int main() {
    Logging::SetLogLevel(LogLevel_Type::DEBUG);
    Logging::info("Task Switcher starting");
    ForegroundWindowTracker tracker;
    
    // Initialize UI
    SDL_SelectorUI::ListUI listUI;
    listUI.windowWidth = 800;
    listUI.windowHeight = 600;
        
    SDL_SelectorUI::SwitcherUI ui("Task Switcher", 800, 600, listUI);
    std::atomic_bool uiRunning{false};
    
    // Set up hotkey callback to start the UI loop
    tracker.SetHotkeyCallback([&ui, &uiRunning, &tracker](std::vector<WindowInfo> windows) {
        Logging::info("=== HOTKEY TRIGGERED ===");
        Logging::info("MRU List size: {}", windows.size());
        for (size_t i = 0; i < windows.size(); i++) {
            Logging::slopyap("  [{}]: {}", i, Logging::format("{}: {}", windows[i].processName, wstring_to_string(windows[i].title)));
        }
        bool expected = false;
        if (!uiRunning.compare_exchange_strong(expected, true)) {
            Logging::debug("Task Switcher UI is already running");
            return;
        }

        std::thread([&ui, items = std::move(windows), &uiRunning, &tracker]() mutable {
            Logging::info("Opening selector with {} entries", items.size());
            int chosenIndex = ui.StartLoop(&items);
            if (chosenIndex >= 0 && chosenIndex < static_cast<int>(items.size())) {
                Logging::info("Selected index {} -> activating window", chosenIndex);
                tracker.ActivateWindow(items[chosenIndex].hwnd);
            }
            uiRunning.store(false);
        }).detach();
    });
    
    // Register hooks on main thread
    if (!tracker.RegisterHooks()) {
        Logging::error("Failed to install hooks");
        return 1;
    }
    
    // Keep main thread alive with message loop and periodic refresh
    Logging::info("Task Switcher running. Press Ctrl+Alt+S to show window list.");
    
    // Run message pump on main thread (required for keyboard hook to work)
    MSG msg;
    while (true) {
        // Process messages with timeout
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        // Sleep to avoid busy-wait
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
