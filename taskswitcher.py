import keyboard
import ctypes
from ctypes import wintypes
import threading
import time
import tkinter as tk

import psutil
import win32api
import win32con
import win32gui
import win32process


class CustomTaskSwitcher:
    EVENT_SYSTEM_FOREGROUND = 0x0003
    WINEVENT_OUTOFCONTEXT = 0x0000
    EXCLUDED_PROCESSES = {
        "SystemSettings.exe",
        "ShellExperienceHost.exe",
        "StartMenuExperienceHost.exe",
    }
    WinEventProcType = ctypes.WINFUNCTYPE(
        None,
        wintypes.HANDLE,
        wintypes.DWORD,
        wintypes.HWND,
        wintypes.LONG,
        wintypes.LONG,
        wintypes.DWORD,
        wintypes.DWORD,
    )

    def __init__(self, config: Config | None = None, logger: Logger | None = None):
        self.config = config
        self.logger = logger
        self.open_windows = []
        self.mru = []
        self.mru_lock = threading.Lock()
        self._callback = None
        self._focus_hook_thread = None
        self._start_focus_hook()

    # ---------- focus hook ----------

    def _start_focus_hook(self):
        if self._focus_hook_thread and self._focus_hook_thread.is_alive():
            return
        self._callback = self.WinEventProcType(self._on_foreground_event)
        self._focus_hook_thread = threading.Thread(target=self._hook_loop, daemon=True)
        self._focus_hook_thread.start()

    def _on_foreground_event(self, hWinEventHook, event, hwnd, idObject, idChild, dwEventThread, dwmsEventTime):
        if hwnd:
            self._touch_mru(hwnd)

    def _touch_mru(self, hwnd):
        """Move hwnd to the front of the MRU list."""
        with self.mru_lock:
            if hwnd in self.mru:
                self.mru.remove(hwnd)
            self.mru.insert(0, hwnd)

    def _hook_loop(self):
        hook = ctypes.windll.user32.SetWinEventHook(
            self.EVENT_SYSTEM_FOREGROUND,
            self.EVENT_SYSTEM_FOREGROUND,
            0,
            self._callback,
            0,
            0,
            self.WINEVENT_OUTOFCONTEXT,
        )
        msg = wintypes.MSG()
        try:
            while ctypes.windll.user32.GetMessageW(ctypes.byref(msg), 0, 0, 0) != 0:
                ctypes.windll.user32.TranslateMessage(ctypes.byref(msg))
                ctypes.windll.user32.DispatchMessageW(ctypes.byref(msg))
        finally:
            if hook:
                ctypes.windll.user32.UnhookWinEvent(hook)

    # ---------- window enumeration ----------

    def _is_switchable_window(self, hwnd):
        if not hwnd or not win32gui.IsWindow(hwnd) or not win32gui.IsWindowVisible(hwnd):
            return False
        if not win32gui.GetWindowText(hwnd).strip():
            return False
        try:
            rect = win32gui.GetWindowRect(hwnd)
        except win32gui.error:
            return False
        return (rect[2] - rect[0]) > 0 and (rect[3] - rect[1]) > 0

    def get_open_windows(self):
        self.open_windows = []

        def enum_handler(hwnd, _):
            if not self._is_switchable_window(hwnd):
                return
            _, pid = win32process.GetWindowThreadProcessId(hwnd)
            try:
                proc_name = psutil.Process(pid).name()
            except psutil.NoSuchProcess:
                proc_name = "unknown"
            if proc_name in self.EXCLUDED_PROCESSES:
                return
            self.open_windows.append({
                "hwnd": hwnd,
                "title": win32gui.GetWindowText(hwnd),
                "proc": proc_name,
            })

        win32gui.EnumWindows(enum_handler, None)

        with self.mru_lock:
            mru_index = {hwnd: i for i, hwnd in enumerate(self.mru)}

        self.open_windows.sort(key=lambda w: mru_index.get(w["hwnd"], len(mru_index)))
        return self.open_windows

    # ---------- switching ----------

    @staticmethod
    def force_foreground(hwnd):
        """SetForegroundWindow is blocked by Windows unless the calling process
        just received input. Instead of faking a keystroke (which flashes the
        Alt menu bar system-wide), we borrow the current foreground thread's
        input state via AttachThreadInput, which satisfies the same check
        without dispatching any real key events."""
        if not hwnd or not win32gui.IsWindow(hwnd):
            return False

        switch_to_this_window = getattr(ctypes.windll.user32, "SwitchToThisWindow", None)

        for attempt in range(3):
            try:
                if win32gui.IsIconic(hwnd):
                    win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
                else:
                    win32gui.ShowWindow(hwnd, win32con.SW_SHOW)

                fg_hwnd = win32gui.GetForegroundWindow()
                fg_thread, _ = win32process.GetWindowThreadProcessId(fg_hwnd)
                target_thread, _ = win32process.GetWindowThreadProcessId(hwnd)
                current_thread = win32api.GetCurrentThreadId()

                attached_fg = attached_target = False
                try:
                    if fg_thread and fg_thread != current_thread:
                        attached_fg = bool(ctypes.windll.user32.AttachThreadInput(current_thread, fg_thread, True))
                    if target_thread and target_thread != current_thread:
                        attached_target = bool(ctypes.windll.user32.AttachThreadInput(current_thread, target_thread, True))

                    win32gui.BringWindowToTop(hwnd)
                    win32gui.SetForegroundWindow(hwnd)
                    if switch_to_this_window:
                        switch_to_this_window(hwnd, True)
                finally:
                    if attached_fg:
                        ctypes.windll.user32.AttachThreadInput(current_thread, fg_thread, False)
                    if attached_target:
                        ctypes.windll.user32.AttachThreadInput(current_thread, target_thread, False)
            except Exception:
                pass

            if win32gui.GetForegroundWindow() == hwnd:
                return True
            time.sleep(0.02)

        return False

    def switch_to_previous(self):
        """Switch focus to the second-most-recently-used window."""
        try:
            print("switch_to_previous: hotkey fired")
            windows = self.get_open_windows()
            print(f"switch_to_previous: visible windows = {len(windows)}")
            if not windows:
                print("switch_to_previous: no visible windows")
                return

            current = win32gui.GetForegroundWindow()
            valid_hwnds = {w["hwnd"] for w in windows}
            print(f"switch_to_previous: current hwnd = {current}")

            with self.mru_lock:
                candidates = [hwnd for hwnd in self.mru if hwnd in valid_hwnds and hwnd != current]
            print(f"switch_to_previous: mru candidates = {candidates}")

            # Fall back to the next window in enumeration order if MRU has nothing usable.
            target = candidates[0] if candidates else next(
                (w["hwnd"] for w in windows if w["hwnd"] != current), windows[0]["hwnd"]
            )
            print(f"switch_to_previous: target hwnd = {target}")
            switched = self.force_foreground(target)
            print(f"switch_to_previous: switched = {switched}, foreground now = {win32gui.GetForegroundWindow()}")
        except Exception as e:
            print(f"switch_to_previous: failed = {e}")
            if self.logger:
                self.logger.error("Failed to switch to previous window: {}", e)

    def show_switcher(self):
        """Show a simple Tk list of open windows and switch on selection."""
        try:
            windows = self.get_open_windows()
            if not windows:
                if self.logger:
                    self.logger.info("No visible windows found.")
                return

            root = tk.Tk()
            root.title("Switcher")
            root.attributes("-topmost", True)
            root.geometry("400x300+600+300")

            listbox = tk.Listbox(root, font=("Segoe UI", 12))
            listbox.pack(fill="both", expand=True)
            for w in windows:
                listbox.insert("end", f"{w['proc']}: {w['title']}")
            listbox.selection_set(0)

            def activate(event=None):
                selection = listbox.curselection()
                if not selection:
                    return
                hwnd = windows[selection[0]]["hwnd"]
                root.destroy()
                self.force_foreground(hwnd)

            listbox.bind("<Return>", activate)
            listbox.bind("<ButtonRelease-1>", activate)
            root.bind("<Escape>", lambda e: root.destroy())

            root.lift()
            root.update_idletasks()
            root.after(10, lambda: (root.focus_force(), listbox.focus_set()))

            root.mainloop()
            self.open_windows.clear()
        except Exception as e:
            if self.logger:
                self.logger.error("Failed to show switcher: {}", e)

    def register(self, switcher_hotkey: str = "ctrl+alt+s", switch_previous_hotkey: str = "ctrl+alt+p"):
        keyboard.add_hotkey(switcher_hotkey, self.show_switcher)
        keyboard.add_hotkey(switch_previous_hotkey, self.switch_to_previous)
        print(f"registered hotkeys: {switcher_hotkey}, {switch_previous_hotkey}")

    def listen(self):
        keyboard.wait()


if __name__ == "__main__":
    switcher = CustomTaskSwitcher()
    switcher.register()
    switcher.listen()
