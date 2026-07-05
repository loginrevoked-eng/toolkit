import ctypes

class TaskbarController:
    def __init__(self):
        self._taskbar_hwnd = None
        self._start_hwnd = None

    def hide_taskbar(self):
        SW_HIDE = 0
        user32 = ctypes.windll.user32

        self._taskbar_hwnd = user32.FindWindowW("Shell_TrayWnd", None)
        self._start_hwnd = user32.FindWindowW("Button", "Start")  # always None on Win10/11

        if not self._taskbar_hwnd:
            print("Could not find taskbar window.")
            return False

        user32.ShowWindow(self._taskbar_hwnd, SW_HIDE)
        if self._start_hwnd:
            user32.ShowWindow(self._start_hwnd, SW_HIDE)

        return True

    def show_taskbar(self):
        SW_SHOW = 1
        user32 = ctypes.windll.user32

        hwnd = self._taskbar_hwnd or user32.FindWindowW("Shell_TrayWnd", None)
        if not hwnd:
            print("Could not find taskbar window.")
            return False

        user32.ShowWindow(hwnd, SW_SHOW)
        if self._start_hwnd:
            user32.ShowWindow(self._start_hwnd, SW_SHOW)

        return True
