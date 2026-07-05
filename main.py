import ctypes
from ctypes import wintypes
import os
import subprocess
import shutil



backupdir = rf"{os.environ['USERPROFILE']}\\Music\\backup"
wallpaper_path = os.path.join(os.getcwd(), "black_wallpaper.png")
target_win_title = "Version"


def launch_blackoverlay():
    subprocess.Popen(["blackoverlay.exe", "--target-window-title", target_win_title])

def launch_seb():
    x86 = rf"{os.environ['PROGRAMFILES(x86)']}\\SafeExamBrowser\\Application\\SafeExamBrowser.exe"
    x64 = rf"{os.environ['PROGRAMFILES']}\\SafeExamBrowser\\Application\\SafeExamBrowser.exe"

    if os.path.exists(x86):
        subprocess.Popen([x86])
    elif os.path.exists(x64):
        subprocess.Popen([x64])
    else:
        print("SafeExamBrowser not found")


def backupandreplace_config():
    configpath = rf"{os.environ['USERPROFILE']}\AppData\Roaming\SafeExamBrowser"
    backup_path = os.path.join(backupdir, "SafeExamBrowser")

    os.makedirs(backupdir, exist_ok=True)

    if os.path.exists(configpath):
        if os.path.exists(backup_path):
            shutil.rmtree(backup_path)
        shutil.move(configpath, backup_path)

    shutil.copytree("SafeExamBrowser", configpath)

def delete_sebcache():
    sebcachepath = rf"{os.environ['USERPROFILE']}\\AppData\\Local\\SafeExamBrowser"
    if os.path.exists(sebcachepath):
        shutil.rmtree(sebcachepath)

def set_wallpaper(image_path):
    if not os.path.exists(image_path):
        print("iamge doesnt exist or not found")
    return bool(ctypes.windll.user32.SystemParametersInfoW(20, 0, image_path, 3))



def autohide_taskbar(enable=True):
    ABM_SETSTATE = 0xB
    ABS_AUTOHIDE = 0x1

    class APPBARDATA(ctypes.Structure):
        _fields_ = [
            ("cbSize", wintypes.DWORD),
            ("hWnd", wintypes.HWND),
            ("uCallbackMessage", wintypes.UINT),
            ("uEdge", wintypes.UINT),
            ("rc", wintypes.RECT),
            ("lParam", wintypes.LPARAM),
        ]

    hwnd = ctypes.windll.user32.FindWindowW("Shell_TrayWnd", None)
    abd = APPBARDATA()
    abd.cbSize = ctypes.sizeof(APPBARDATA)
    abd.hWnd = hwnd
    abd.lParam = ABS_AUTOHIDE if enable else 0
    return bool(ctypes.windll.shell32.SHAppBarMessage(ABM_SETSTATE, ctypes.byref(abd)))



def hide_desktop_icons(hide=True):
    SW_HIDE = 0
    SW_SHOW = 5

    progman = ctypes.windll.user32.FindWindowW("Progman", None)
    defview = ctypes.windll.user32.FindWindowExW(progman, None, "SHELLDLL_DefView", None)
    listview = ctypes.windll.user32.FindWindowExW(defview, None, "SysListView32", None)

    return bool(ctypes.windll.user32.ShowWindow(listview, SW_HIDE if hide else SW_SHOW))




if __name__ == "__main__":
    backupandreplace_config()
    delete_sebcache()
    set_wallpaper(wallpaper_path)
    autohide_taskbar()
    hide_desktop_icons()
    launch_seb()
    launch_blackoverlay()