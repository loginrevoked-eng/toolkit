import ctypes
from ctypes import wintypes
from dataclasses import dataclass
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from typing import Dict, List
import winreg

from cryptography.fernet import Fernet
import keyboard
import psutil
import requests
import win32com.client

from taskbar import TaskbarController
from taskswitcher import CustomTaskSwitcher


@dataclass
class Process:
    pid: int
    name: str
    fullpath: str


@dataclass
class FastInitConfig:
    backup_dir: str
    install_dir: str
    wallpaper_filename: str
    shortcut_filename: str
    seb_programs: List[str]
    seb_main_exe_paths: List[str]
    seb_cache_dir: str
    seb_config_dir: str
    seb_config_dir_sub: str
    overlay_target_win_title: str
    black_overlay_exe_name: str
    fastinit_exe_name: str
    old_wallpaper_path: str
    migrate_only_if_from_usb: bool = True
    create_shelllink_oninstall: bool = True

    def from_dict(self, _dict: Dict):
        for key, value in _dict.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def from_json(self, json_path: str):
        try:
            with open(json_path, "r") as f:
                _dict = json.load(f)
            self.from_dict(_dict)
            return self
        except FileNotFoundError:
            print(f"Config file not found: {json_path}")
            return self
        except Exception:
            raise

    def from_restapi(self, api_url: str, headers=None):
        response = None
        if headers:
            try:
                response = requests.get(api_url, headers=headers)
            except Exception as e:
                print(f"Failed to fetch config from {api_url}: {e}")
                return self
        else:
            try:
                response = requests.get(api_url)
            except Exception as e:
                print(f"Failed to fetch config from {api_url}: {e}")
                return self
        if response and response.status_code == 200:
            _dict = response.json()
            self.from_dict(_dict)
        return self

    def from_encrypted_json(self, json_path: str, key: bytes):
        with open(json_path, "rb") as f:
            encrypted_data = f.read()

        decrypted_data = Fernet(key).decrypt(encrypted_data)
        _dict = json.loads(decrypted_data)
        self.from_dict(_dict)
        return self


class WinSysSettings:
    def __init__(self):
        self.taskbar_controller = TaskbarController()

    def save_current_wallpaper(self, dst: str):
        key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Control Panel\Desktop")
        wallpaper_path, _ = winreg.QueryValueEx(key, "WallPaper")
        winreg.CloseKey(key)
        if os.path.exists(dst):
            os.remove(dst)
        shutil.copy2(wallpaper_path, dst)

    def restore_wallpaper(self, old_wallpaper_path: str):
        if os.path.exists(old_wallpaper_path):
            self.set_wallpaper(old_wallpaper_path)
            os.remove(old_wallpaper_path)

    def set_wallpaper(self, wallpaper_path: str):
        if not os.path.exists(wallpaper_path):
            print("image doesnt exist or not found")
        return bool(
            ctypes.windll.user32.SystemParametersInfoW(20, 0, wallpaper_path, 3)
        )

    def hide_taskbar(self):
        SW_HIDE = 0

        progman = ctypes.windll.user32.FindWindowW("Progman", None)
        defview = ctypes.windll.user32.FindWindowExW(
            progman, None, "SHELLDLL_DefView", None
        )
        listview = ctypes.windll.user32.FindWindowExW(
            defview, None, "SysListView32", None
        )

        return bool(ctypes.windll.user32.ShowWindow(listview, SW_HIDE))

    def show_taskbar(self):
        SW_SHOW = 5

        progman = ctypes.windll.user32.FindWindowW("Progman", None)
        defview = ctypes.windll.user32.FindWindowExW(
            progman, None, "SHELLDLL_DefView", None
        )
        listview = ctypes.windll.user32.FindWindowExW(
            defview, None, "SysListView32", None
        )

        return bool(ctypes.windll.user32.ShowWindow(listview, SW_SHOW))

    def hide_desktop_icons(self):
        self.taskbar_controller.hide_taskbar()

    def show_desktop_icons(self):
        self.taskbar_controller.show_taskbar()


class FileManager:
    def __init__(self, config: FastInitConfig):
        self.config = config

    def backup_SebConfig(self):
        dst = os.path.expandvars(
            str(Path(self.config.backup_dir) / self.config.seb_config_dir_sub)
        )
        src = os.path.expandvars(self.config.seb_config_dir)
        os.makedirs(self.config.backup_dir, exist_ok=True)

        if os.path.exists(src):
            if os.path.exists(dst):
                shutil.rmtree(dst)
            shutil.move(src, dst)
        return dst

    def replace_SebConfig(self):
        dst = os.path.expandvars(self.config.seb_config_dir)
        src = os.path.expandvars(
            str(Path(self.config.install_dir) / self.config.seb_config_dir_sub)
        )
        if os.path.exists(src):
            if os.path.exists(dst):
                shutil.rmtree(dst)
            shutil.move(src, dst)
        return dst

    def delete_SebCache(self):
        cache_path = os.path.expandvars(self.config.seb_cache_dir)
        if os.path.exists(cache_path):
            shutil.rmtree(cache_path)

    def is_Installed(self):
        return os.getcwd() == os.path.expandvars(self.config.install_dir)

    def shouldMigrate(self):
        return not self.is_Installed() and (
            os.getcwd().count("\\") == 1
            if self.config.migrate_only_if_from_usb
            else True
        )

    def migrate_Project(self):
        if self.shouldMigrate():
            shutil.copytree(
                os.getcwd(), os.path.expandvars(self.config.install_dir)
            )

    def restore_SebConfig(self):
        dst = os.path.expandvars(self.config.seb_config_dir)
        src = os.path.expandvars(
            str(Path(self.config.backup_dir) / self.config.seb_config_dir_sub)
        )
        if os.path.exists(src):
            if os.path.exists(dst):
                shutil.rmtree(dst)
            shutil.move(src, dst)
        return dst

    def resolve_SebPath(self):
        for path in self.config.seb_main_exe_paths:
            if os.path.exists(os.path.expandvars(path)):
                return os.path.expandvars(path)
        return None

    def addShellLinkToUserStartMenu(self):
        if not self.is_Installed():
            shutil.copy2(
                str(
                    Path(self.config.install_dir)
                    / self.config.shortcut_filename
                ),
                os.path.join(
                    os.environ["APPDATA"],
                    "Microsoft",
                    "Windows",
                    "Start Menu",
                    "Programs",
                    self.config.shortcut_filename,
                ),
            )
        return True

    def make_UserStartMenuShellLink(self):
        if not self.is_Installed():
            shell = win32com.client.Dispatch("WScript.Shell")
            shortcut = shell.CreateShortcut(
                os.path.join(
                    os.environ["APPDATA"],
                    "Microsoft",
                    "Windows",
                    "Start Menu",
                    "Programs",
                    self.config.shortcut_filename,
                )
            )
            shortcut.Targetpath = os.path.join(
                self.config.install_dir, self.config.fastinit_exe_name
            )
            shortcut.save()
        return True


class ProcessManager:
    def __init__(self):
        pass

    def procDictFromNames(
        self, procNames: List[str], procs: Dict[str, Process] = None
    ) -> Dict[str, Process]:
        if procs is None:
            procs = {}
        for proc in psutil.process_iter(["pid", "name", "exe"]):
            try:
                pname = proc.info["name"]
                if pname in procNames and pname not in procs:
                    procs[pname] = Process(
                        pid=proc.info["pid"],
                        name=pname,
                        fullpath=proc.info["exe"] or "",
                    )
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        return procs

    def kill_Process(self, pid: int = None, pname: str = None) -> bool:
        killed = False
        if pid is not None:
            try:
                p = psutil.Process(pid)
                p.kill()
                killed = True
            except psutil.NoSuchProcess:
                pass
        elif pname is not None:
            for proc in psutil.process_iter(["pid", "name"]):
                if proc.info["name"] == pname:
                    try:
                        proc.kill()
                        killed = True
                    except psutil.NoSuchProcess:
                        pass
        return killed

    def start_Process(self, ppath: str) -> bool:
        try:
            subprocess.Popen(ppath)
            return True
        except Exception:
            return False

    def process_Exists(self, pname: str) -> bool:
        for proc in psutil.process_iter(["name"]):
            if proc.info["name"] == pname:
                return True
        return False

    def pid_FromName(self, pname: str) -> int:
        for proc in psutil.process_iter(["pid", "name"]):
            if proc.info["name"] == pname:
                return proc.info["pid"]
        return -1

    def name_FromPid(self, pid: int) -> str:
        try:
            return psutil.Process(pid).name()
        except psutil.NoSuchProcess:
            return None


def cleanUp(
    processes: Dict[str, Process],
    config: FastInitConfig,
    pm: ProcessManager,
    ws: WinSysSettings,
    fm: FileManager,
):
    for pname, p in processes.items():
        try:
            pm.kill_Process(pid=p.pid)
        except Exception:
            print(
                f"Please kill {pname} manually since it failed to be killed programmatically"
            )
            while pm.process_Exists(pname):
                time.sleep(0.5)

    seb_path = fm.resolve_SebPath()
    if seb_path:
        pm.start_Process(seb_path)

    ws.show_taskbar()
    ws.restore_wallpaper(config.old_wallpaper_path)
    fm.restore_SebConfig()
    ws.show_desktop_icons()


def main():
    config = (
        FastInitConfig(
            backup_dir=os.path.expandvars(r"%USERPROFILE%\Music\backup"),
            install_dir=os.path.expandvars(r"%USERPROFILE%\Music\usbsnapshot"),
            wallpaper_filename="black_wallpaper.png",
            shortcut_filename="fastinit.lnk",
            seb_programs=["SafeExamBrowser.exe", "SafeExamBrowser.Client.exe"],
            seb_main_exe_paths=[
                r"%PROGRAMFILES(x86)%\SafeExamBrowser\Application\SafeExamBrowser.exe",
                r"%PROGRAMFILES%\SafeExamBrowser\Application\SafeExamBrowser.exe",
            ],
            seb_cache_dir=r"%USERPROFILE%\AppData\Local\SafeExamBrowser",
            seb_config_dir=r"%USERPROFILE%\AppData\Roaming\SafeExamBrowser",
            seb_config_dir_sub="SafeExamBrowser",
            overlay_target_win_title="Version",
            black_overlay_exe_name="blackoverlay.exe",
            fastinit_exe_name="fastinit.exe",
            old_wallpaper_path=os.path.join(
                os.path.expandvars(r"%USERPROFILE%\Music\backup"),
                "old_wallpaper.png",
            ),
        )
        .from_json("config.json")
        .from_restapi("http://localhost:8000/config")
    )

    pm = ProcessManager()
    ws = WinSysSettings()
    fm = FileManager(config)

    active_processes: Dict[str, Process] = {}

    ws.save_current_wallpaper(config.old_wallpaper_path)
    fm.backup_SebConfig()
    fm.replace_SebConfig()
    fm.delete_SebCache()

    ws.set_wallpaper(os.path.join(os.getcwd(), config.wallpaper_filename))
    ws.hide_taskbar()
    ws.hide_desktop_icons()

    ts = CustomTaskSwitcher()
    ts.register(
        switcher_hotkey="ctrl+alt+s",
        switch_previous_hotkey="ctrl+alt+p",
    )

    keyboard.add_hotkey(
        "ctrl+alt+r",
        lambda: cleanUp(active_processes, config, pm, ws, fm),
    )
    seb_path = fm.resolve_SebPath()
    if seb_path and pm.start_Process(seb_path):
        pid = pm.pid_FromName("SafeExamBrowser.exe")
        active_processes["SafeExamBrowser"] = Process(
            pid, "SafeExamBrowser.exe", seb_path
        )

    time.sleep(3)

    overlay_path = os.path.join(os.getcwd(), config.black_overlay_exe_name)
    if pm.start_Process(
        f"{overlay_path} --target-window-title {config.overlay_target_win_title}"
    ):
        pid = pm.pid_FromName(config.black_overlay_exe_name)
        active_processes["blackoverlay"] = Process(
            pid, config.black_overlay_exe_name, overlay_path
        )

    pm.procDictFromNames(config.seb_programs, active_processes)

    fm.migrate_Project()
    if config.create_shelllink_oninstall:
        fm.make_UserStartMenuShellLink()
    else:
        fm.addShellLinkToUserStartMenu()

    keyboard.wait()


if __name__ == "__main__":
    main()