#include "tracker.cpp"
#include "ui.cpp"
#include "logging.hpp"


int main() {
    Logging::SetLogLevel(Logging::LogLevel_Type::DEBUG);
    ForegroundWindowTracker tracker;
    
    // Initialize UI
    SDL_SelectorUI::ListUI listUI;
    listUI.windowWidth = 800;
    listUI.windowHeight = 600;
        
    SDL_SelectorUI::SwitcherUI ui("Task Switcher", 800, 600, listUI);
    
    // Set up hotkey callback to start the UI loop
    tracker.SetHotkeyCallback([&ui](std::vector<std::string>& mruList) {
        Logging::info("=== HOTKEY TRIGGERED ===");
        Logging::info("MRU List size: {}", mruList.size());
        for (size_t i = 0; i < mruList.size(); i++) {
            Logging::info("  [{}]: {}", i, mruList[i]);
        }
        ui.StartLoop(&mruList);
    });
    
    // Refresh window list and register hooks on main thread
    tracker.refreshMru();
    tracker.RegisterHooks();
    
    // Keep main thread alive with message loop and periodic refresh
    Logging::info("Task Switcher running. Press Ctrl+Alt+S to show window list.");
    
    // Run message pump on main thread (required for keyboard hook to work)
    MSG msg;
    DWORD lastRefresh = GetTickCount();
    while (true) {
        // Process messages with timeout
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        
        // Refresh MRU list every second
        DWORD now = GetTickCount();
        if (now - lastRefresh > 1000) {
            tracker.refreshMru();
            lastRefresh = now;
        }
        
        // Sleep to avoid busy-wait
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
