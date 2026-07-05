import psutil
import time
import keyboard
import ctypes
from ctypes import wintypes
import os
import subprocess
import shutil
from taskbar import TaskbarController
from taskswitcher import CustomTaskSwitcher

FILES = [
    "blackoverlay.exe",
    "black_wallpaper.png",
    "SafeExamBrowser",
    "fastinit.exe"
]

usbsnapshotdir = rf"{os.environ['USERPROFILE']}\\Music\\usbsnapshot"
backupdir = rf"{os.environ['USERPROFILE']}\\Music\\backup"
wallpaper_path = os.path.join(os.getcwd(), "black_wallpaper.png")
target_win_title = "Version"
seb_programs = [
    "SafeExamBrowser.exe",
    "SafeExamBrowser.Client.exe"
]
lnkfile = rf"{os.getcwd()}\fastinit.lnk"

def hidetaskbar():
    tb = TaskbarController()
    tb.hide_taskbar()
    return tb

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


def process_isrunning(name, snapshot=None):
    snapshot = snapshot or psutil.process_iter(['pid', 'name'])
    for i, p in enumerate(snapshot):
        if name.lower() in p.info['name'].lower():
            return True
    return False


def hide_desktop_icons(hide=True):
    SW_HIDE = 0
    SW_SHOW = 5

    progman = ctypes.windll.user32.FindWindowW("Progman", None)
    defview = ctypes.windll.user32.FindWindowExW(progman, None, "SHELLDLL_DefView", None)
    listview = ctypes.windll.user32.FindWindowExW(defview, None, "SysListView32", None)

    return bool(ctypes.windll.user32.ShowWindow(listview, SW_HIDE if hide else SW_SHOW))

def show_desktop_icons():
    SW_SHOW = 5

    progman = ctypes.windll.user32.FindWindowW("Progman", None)
    defview = ctypes.windll.user32.FindWindowExW(progman, None, "SHELLDLL_DefView", None)
    listview = ctypes.windll.user32.FindWindowExW(defview, None, "SysListView32", None)

    return bool(ctypes.windll.user32.ShowWindow(listview, SW_SHOW))

def kill_task(name):
    for p in psutil.process_iter(['pid', 'name']):
        if name.lower() in p.info['name'].lower():
            p.kill()

def start_taskswitcher():
    ts = CustomTaskSwitcher()
    ts.register()
    return ts

def copy_all():
    os.makedirs(usbsnapshotdir, exist_ok=True)
    if backupdir in os.getcwd():
        return
    for file in FILES:
        if not os.path.exists(file):
            print(f"File {file} not found")
            continue
        if os.path.isdir(file):
            if os.path.exists(os.path.join(usbsnapshotdir, file)):
                shutil.rmtree(os.path.join(usbsnapshotdir, file))
            shutil.copytree(file, os.path.join(usbsnapshotdir, file))
        else:
            shutil.copy2(file, os.path.join(usbsnapshotdir, file))

def add_to_user_startmenu():
    if backupdir in os.getcwd():
        return
    startmenu_path = rf"{os.environ['USERPROFILE']}\AppData\Roaming\Microsoft\Windows\Start Menu\Programs"
    if os.path.exists(lnkfile):
        shutil.copy2(lnkfile, startmenu_path)
    
def cleanup(tb):
    configpath = rf"{os.environ['USERPROFILE']}\AppData\Roaming\SafeExamBrowser"
    backup_path = os.path.join(backupdir, "SafeExamBrowser")

    os.makedirs(backupdir, exist_ok=True)

    if os.path.exists(backup_path):
        if os.path.exists(configpath):
            shutil.rmtree(configpath)
        shutil.copytree(backup_path, configpath)
    tb.show_taskbar()
    kill_task("blackoverlay.exe")
    print("Now please terminate the SEB manuallly with a password, press enter after doing so")
    time.sleep(5)
    if not any([process_isrunning(seb_program) for seb_program in seb_programs]):
        launch_seb()
    else:
        print("SEB is already running, fucking kill it first")
        time.sleep(5)
        if not any([process_isrunning(seb_program) for seb_program in seb_programs]):
            launch_seb()
    show_desktop_icons()
    os._exit(0)

if __name__ == "__main__":
    backupandreplace_config()
    delete_sebcache()
    set_wallpaper(wallpaper_path)
    autohide_taskbar()
    hide_desktop_icons()
    tb = hidetaskbar()
    ts = start_taskswitcher()
    keyboard.add_hotkey('ctrl+alt+r', lambda: cleanup(tb))
    launch_seb()
    time.sleep(3)
    launch_blackoverlay()
    copy_all()
    add_to_user_startmenu()
    keyboard.wait()
