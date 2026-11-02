"""Read-only LLDB assertion capture for an explicitly owned Mac Development game.

Import in LLDB, then run `scene-culling-arm --help`. This does not launch, resume,
quit, or evaluate expressions in the target. Native input/lifecycle is separate.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shlex
import time

_CAPTURE = None
# These offsets were verified against this exact monolithic executable's
# disassembly. They MUST NOT be applied to an editor or another game build.
_LAYOUT_UUID = "B44B6629-0127-3BA8-BB1A-06C2A988B1AE"


def verify_identity(actual: str, expected: str, expected_sha256: str) -> Path:
    executable = Path(actual).resolve(strict=True)
    if executable != Path(expected).resolve(strict=True):
        raise ValueError("Selected process executable differs from the explicit owned target")
    if executable.name != "DnDDiceRoll" or executable.parent.name != "MacOS":
        raise ValueError("Capture supports the packaged game executable, not an editor")
    with executable.open("rb") as stream:
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
        actual_sha256 = digest.hexdigest()
    if len(expected_sha256) != 64 or actual_sha256 != expected_sha256:
        raise ValueError("Selected executable does not match the expected SHA-256")
    return executable


def is_scene_culling_assertion(expression: str, stack: list[str]) -> bool:
    return expression == "UploadDataProxy == nullptr" and any(
        "FSceneCullingBuilder::" in frame or "FSceneCulling::FinalizeUpdateAndClear" in frame
        for frame in stack
    )


class Capture:
    def __init__(self, target, output: Path):
        self.target = target
        self.process = target.GetProcess()
        self.output = output
        self.breakpoints = []

    def read(self, address: int, size: int) -> bytes:
        import lldb
        error = lldb.SBError()
        result = self.process.ReadMemory(address, size, error)
        if not error.Success():
            raise ValueError(error.GetCString())
        return result

    def integer(self, address: int, size: int = 8) -> int:
        return int.from_bytes(self.read(address, size), "little")

    def string(self, address: int) -> str:
        import lldb
        error = lldb.SBError()
        result = self.process.ReadCStringFromMemory(address, 4096, error)
        return result if error.Success() else "<unreadable>"

    def symbol(self, address: int) -> str:
        import lldb
        context = self.target.ResolveSymbolContextForAddress(
            self.target.ResolveLoadAddress(address), lldb.eSymbolContextSymbol
        )
        return context.GetSymbol().GetName() or "<unknown>"

    def renderer(self, builder: int) -> dict:
        """No layout inference: only read fields for the verified binary UUID."""
        if self.target.GetModuleAtIndex(0).GetUUIDString() != _LAYOUT_UUID:
            return {"builder": hex(builder), "layout": "unavailable for this UUID"}
        scene = self.integer(builder + 0x70)
        proxy = self.integer(builder + 0x108)
        result = {
            "builder": hex(builder), "scene": hex(scene),
            "active_builder": hex(self.integer(scene + 0x2F8)),
            "post_task": hex(self.integer(scene + 0x570)),
            "num_dirty_blocks": self.integer(builder + 0x98, 4),
            "proxies": [hex(self.integer(builder + offset)) for offset in (0x108, 0x138, 0x168, 0x198)],
            "block_proxy_first_symbol": self.symbol(self.integer(proxy)) if proxy else None,
            "layout_uuid": _LAYOUT_UUID,
        }
        return result

    def assertion(self, frame):
        import lldb
        if (self.output / "assertion.json").exists():
            print("Assertion already captured; preserving the first failure.", flush=True)
            return True
        thread = frame.GetThread()
        registers = {name: frame.FindRegister(name).GetValueAsUnsigned() for name in
                     ["pc", "sp", "fp", "lr"] + ["x" + str(i) for i in range(29)]}
        stack = [str(f) for f in thread]
        payload = {
            "time": time.time(), "pid": self.process.GetProcessID(),
            "thread_id": thread.GetThreadID(), "thread_name": thread.GetName(),
            "expression": self.string(registers["x0"]),
            "file": self.string(registers["x1"]), "line": registers["x2"],
            "registers": registers, "stack": stack,
            "module_uuid": self.target.GetModuleAtIndex(0).GetUUIDString(),
        }
        payload["scene_culling_signature"] = is_scene_culling_assertion(payload["expression"], stack)
        # Preserve argument registers before any subsequent collection can fail.
        (self.output / "assertion.json").write_text(json.dumps(payload, indent=2))
        output = []
        interpreter = self.target.GetDebugger().GetCommandInterpreter()
        for f in list(thread)[:9]:
            context = lldb.SBExecutionContext(f)
            for command in ["register read", "frame variable --show-types --depth 2",
                            f"disassemble --start-address {max(f.GetPC() - 64, 0)} --count 35"]:
                result = lldb.SBCommandReturnObject()
                interpreter.HandleCommand(command, context, result)
                output.append(str(f) + "\n" + command + "\n" + result.GetOutput() + result.GetError())
        result = lldb.SBCommandReturnObject()
        interpreter.HandleCommand("thread backtrace all", lldb.SBExecutionContext(frame), result)
        output.append(result.GetOutput() + result.GetError())
        (self.output / "assertion-context.txt").write_text("\n".join(output))
        # x20 is the builder only at this specific verified return address.
        caller = thread.GetFrameAtIndex(1)
        symbol = caller.GetSymbol()
        offset = caller.GetPC() - symbol.GetStartAddress().GetLoadAddress(self.target)
        if (payload["module_uuid"] == _LAYOUT_UUID and payload["line"] == 317
                and "ProcessPostSceneUpdate" in str(caller) and offset == 20176):
            builder = caller.FindRegister("x20").GetValueAsUnsigned()
            try:
                state = self.renderer(builder)
                (self.output / "renderer-state.json").write_text(json.dumps(state, indent=2))
                for name, address, size in [("builder", builder, 0x1E0),
                                             ("scene-culling", int(state["scene"], 16), 0x600),
                                             ("block-proxy", int(state["proxies"][0], 16), 0x40)]:
                    if address:
                        (self.output / (name + ".bin")).write_bytes(self.read(address, size))
            except ValueError as error:
                (self.output / "renderer-read-error.txt").write_text(str(error))
        (self.output / "capture-complete.json").write_text(json.dumps({"time": time.time()}))
        print("Scene assertion captured. Target remains stopped; inspect before resuming.", flush=True)
        return True


def on_assert(frame, location, internal_dict):
    try:
        return _CAPTURE.assertion(frame)
    except Exception as error:
        (_CAPTURE.output / "capture-error.txt").write_text(repr(error))
        return True  # A capture failure must never auto-continue a real assertion.


def arm(debugger, command, result, internal_dict):
    import lldb
    global _CAPTURE
    parser = argparse.ArgumentParser(prog="scene-culling-arm")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--expected-executable", required=True)
    parser.add_argument("--sha256", required=True)
    try:
        args = parser.parse_args(shlex.split(command))
        if _CAPTURE is not None:
            raise ValueError("Already armed in this debugger; use a fresh debugger/run directory")
        target = debugger.GetSelectedTarget()
        process = target.GetProcess()
        if not process.IsValid() or process.GetState() != lldb.eStateStopped:
            raise ValueError("Attach to the explicit owned process and stop it before arming")
        executable = verify_identity(str(target.GetExecutable().fullpath), args.expected_executable, args.sha256)
        if any((args.output / name).exists() for name in ("armed.json", "assertion.json", "renderer-events.jsonl")):
            raise ValueError("Capture evidence already exists; choose a fresh output directory")
        args.output.mkdir(parents=True, exist_ok=True)
        capture = Capture(target, args.output)
        bp = target.BreakpointCreateByRegex(r"^FDebug::CheckVerifyFailedImpl2\(")
        capture.breakpoints.append(bp.GetID())
        if bp.GetNumResolvedLocations() == 0:
            target.BreakpointDelete(bp.GetID())
            raise ValueError("No assertion entry breakpoint resolved; capture is not armed")
        bp.SetScriptCallbackFunction(__name__ + ".on_assert")
        _CAPTURE = capture
        receipt = {"pid": process.GetProcessID(), "executable": str(executable),
                   "sha256": args.sha256, "module_uuid": target.GetModuleAtIndex(0).GetUUIDString(),
                   "assertion_locations": bp.GetNumResolvedLocations(),
                   "breakpoint_ids": capture.breakpoints, "time": time.time()}
        (args.output / "armed.json").write_text(json.dumps(receipt, indent=2))
        result.PutCString(json.dumps(receipt))
    except (Exception, SystemExit) as error:
        # An arming error must leave no partial breakpoints behind.
        if "capture" in locals() and _CAPTURE is None:
            for breakpoint_id in capture.breakpoints:
                target.BreakpointDelete(breakpoint_id)
        result.SetError(str(error))


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand("command script add -f " + __name__ + ".arm scene-culling-arm")
