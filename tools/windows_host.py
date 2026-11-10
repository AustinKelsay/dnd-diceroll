"""Win64 OS primitives. Never use TerminateProcess/taskkill for Unreal lifecycle."""
from contextlib import contextmanager
import ctypes
import json
import os
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET


BELOW_NORMAL = 0x00004000


def powershell(script):
    # Scripts are fixed strings, with numeric PIDs/ports only, not shell/user input.
    return subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
                          check=True, capture_output=True, text=True, timeout=30,
                          creationflags=BELOW_NORMAL).stdout.strip()


def json_rows(script):
    raw = powershell(script)
    data = json.loads(raw) if raw else []
    return [data] if isinstance(data, dict) else data


def editor_processes(project):
    from workspace import project_argument
    rows = json_rows("Get-CimInstance Win32_Process -Filter \"Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'\" "
                     "-ErrorAction Stop | Select-Object ProcessId,ExecutablePath,CommandLine | ConvertTo-Json -Compress")
    found = []
    for row in rows:
        command, executable = row.get("CommandLine"), row.get("ExecutablePath")
        if not command or not executable:
            raise RuntimeError("Cannot inspect an Unreal process command/executable; ownership is unknown")
        if project_argument(command, project, windows=True):
            found.append({"pid": int(row["ProcessId"]), "command": command, "executable": executable})
    return found


def listener_pid(port):
    port = int(port)
    if not 1 <= port <= 65535:
        raise ValueError("Invalid MCP port")
    # Query all listeners so an inaccessible/broken query fails instead of looking unoccupied.
    rows = json_rows("Get-NetTCPConnection -State Listen -ErrorAction Stop | "
                     "Select-Object LocalAddress,LocalPort,OwningProcess | ConvertTo-Json -Compress")
    pids = {int(row["OwningProcess"]) for row in rows if int(row["LocalPort"]) == port}
    if len(pids) > 1:
        raise RuntimeError("Multiple listener PIDs; editor ownership is ambiguous")
    return next(iter(pids), None)


def normal_quit(pid):
    pid = int(pid)
    if pid <= 0:
        raise ValueError("Invalid process PID")
    result = powershell(f"(Get-Process -Id {pid} -ErrorAction Stop).CloseMainWindow()")
    if result.lower() != "true":
        raise RuntimeError(f"PID {pid} declined a normal window-close request; left running")


def pid_alive(pid):
    from ctypes import wintypes
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x1000, False, int(pid))  # QUERY_LIMITED_INFORMATION only
    if not handle:
        if ctypes.get_last_error() == 87:  # ERROR_INVALID_PARAMETER: PID no longer exists
            return False
        raise RuntimeError(f"Cannot query PID {pid}; process existence is unknown")
    try:
        code = wintypes.DWORD()
        if not kernel.GetExitCodeProcess(handle, ctypes.byref(code)):
            raise RuntimeError(f"Cannot query exit status of PID {pid}")
        return code.value == 259  # STILL_ACTIVE
    finally:
        kernel.CloseHandle(handle)


@contextmanager
def operation_lock(path):
    import msvcrt
    with Path(path).open("a+b") as lock:
        if not lock.seek(0, 2):
            lock.write(b"0")
            lock.flush()
        lock.seek(0)
        try:
            msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as exc:
            raise RuntimeError("Another helper operation owns this workspace") from exc
        try:
            yield
        finally:
            lock.seek(0)
            msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)


def batch_command(args):
    """Explicit cmd quoting for .bat files, refusing cmd expansion/metacharacters."""
    args = [str(arg) for arg in args]
    if not args or Path(args[0]).suffix.lower() not in (".bat", ".cmd"):
        return args
    if any(any(char in arg for char in '\r\n"%!&|<>^') for arg in args):
        raise ValueError("Batch arguments contain unsupported cmd metacharacters")
    command = " ".join('"' + arg + '"' for arg in args)
    return '"' + os.environ.get("COMSPEC", "cmd.exe") + '" /d /s /c "' + command + '"'


def dx12_report(path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["dxdiag.exe", "/whql:off", "/x", str(path)], check=True, timeout=90,
                   creationflags=BELOW_NORMAL)
    return parse_dxdiag(path.read_text(encoding="utf-8-sig"))


def parse_dxdiag(text):
    root = ET.fromstring(text)
    directx = root.findtext("SystemInformation/DirectXVersion", "")
    displays = []
    for device in root.findall("DisplayDevices/DisplayDevice"):
        levels = device.findtext("FeatureLevels", "")
        if "12_" in levels or "12." in levels:
            displays.append({"name": device.findtext("CardName", ""), "feature_levels": levels,
                             "driver": device.findtext("DriverVersion", "")})
    if "12" not in directx or not displays:
        raise RuntimeError("DxDiag does not report DirectX 12 with a feature-level-12 adapter")
    return {"directx": directx, "adapters": displays,
            "note": "SM6/Nanite still requires a rendered runtime log, not this capability probe"}
