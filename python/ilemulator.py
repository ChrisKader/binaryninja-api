# Copyright (c) 2015-2026 Vector 35 Inc
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

import ctypes
from typing import Callable, Dict, List, Optional, Tuple

from . import _binaryninjacore as core
from .enums import ILEmulatorStopReason
from . import binaryview
from . import lowlevelil


class LLILEmulator:
    """Concrete emulator for Low Level IL.

    Executes LLIL instructions with full register, flag, and memory state.
    Supports cross-function emulation and user-defined hooks for calls,
    syscalls, memory access, and intrinsics.

    Example usage::

        >>> emu = LLILEmulator(bv)
        >>> emu.set_entry_point(here)
        >>> emu.set_register("rsp", 0x7fff0000)
        >>> emu.set_call_hook(lambda emu, target: True)  # skip all calls
        >>> emu.set_max_instructions(1000)
        >>> reason = emu.run()
        >>> print(f"Stopped: {reason.name}, rax = {hex(emu.get_register('rax'))}")
    """

    def __init__(
        self,
        view: 'binaryview.BinaryView',
        il: Optional['lowlevelil.LowLevelILFunction'] = None,
        handle: Optional[core.BNLLILEmulator] = None,
    ):
        if handle is not None:
            LLILHandle = ctypes.POINTER(core.BNLLILEmulator)
            self.handle = ctypes.cast(handle, LLILHandle)
        elif il is not None:
            self.handle = core.BNCreateLLILEmulator(il.handle, view.handle)
            assert self.handle is not None, "Failed to create LLILEmulator"
        else:
            self.handle = core.BNCreateLLILEmulatorForView(view.handle)
            assert self.handle is not None, "Failed to create LLILEmulator"

        self._arch = il.arch if il is not None else view.arch
        self._view = view

        # Store callback wrappers to prevent garbage collection
        self._call_hook_cb = None
        self._syscall_hook_cb = None
        self._memory_read_hook_cb = None
        self._memory_write_hook_cb = None
        self._pre_instruction_hook_cb = None
        self._intrinsic_hook_cb = None

        # Store user callbacks
        self._call_hook = None
        self._syscall_hook = None
        self._memory_read_hook = None
        self._memory_write_hook = None
        self._pre_instruction_hook = None
        self._intrinsic_hook = None

    def __del__(self):
        if core is not None and hasattr(self, 'handle') and self.handle is not None:
            core.BNFreeLLILEmulator(self.handle)

    def _get_base(self):
        return core.BNLLILEmulatorGetBase(self.handle)

    # ── Execution ─────────────────────────────────────────────────────────

    def run(self) -> ILEmulatorStopReason:
        """Run until a stop condition is hit."""
        return ILEmulatorStopReason(core.BNILEmulatorRun(self._get_base()))

    def step(self) -> ILEmulatorStopReason:
        """Execute a single instruction."""
        return ILEmulatorStopReason(core.BNILEmulatorStep(self._get_base()))

    def step_n(self, n: int) -> ILEmulatorStopReason:
        """Execute up to *n* instructions."""
        return ILEmulatorStopReason(core.BNILEmulatorStepN(self._get_base(), n))

    # ── State ─────────────────────────────────────────────────────────────

    @property
    def instruction_index(self) -> int:
        return core.BNILEmulatorGetInstructionIndex(self._get_base())

    @instruction_index.setter
    def instruction_index(self, index: int):
        core.BNILEmulatorSetInstructionIndex(self._get_base(), index)

    @property
    def current_address(self) -> int:
        return core.BNILEmulatorGetCurrentAddress(self._get_base())

    @property
    def stop_reason(self) -> ILEmulatorStopReason:
        return ILEmulatorStopReason(core.BNILEmulatorGetStopReason(self._get_base()))

    @property
    def stop_message(self) -> str:
        return core.BNILEmulatorGetStopMessage(self._get_base())

    @property
    def instructions_executed(self) -> int:
        return core.BNILEmulatorGetInstructionsExecuted(self._get_base())

    @property
    def call_stack_depth(self) -> int:
        return core.BNLLILEmulatorGetCallStackDepth(self.handle)

    # ── Entry point ───────────────────────────────────────────────────────

    def set_entry_point(self, addr_or_il, instr_index: Optional[int] = None) -> bool:
        """Set the emulation entry point.

        Two forms:

        - ``set_entry_point(0x401000)`` — resolve address to a function and
          start at its first LLIL instruction.  Returns ``False`` if the
          address does not belong to an analyzed function.

        - ``set_entry_point(func.llil, 5)`` — start at LLIL instruction
          index 5 of the given LLIL function.  Always succeeds.
        """
        if instr_index is not None:
            # (LowLevelILFunction, index) form
            il = addr_or_il
            core.BNLLILEmulatorSetEntryPointForIL(self.handle, il.handle, instr_index)
            self._arch = il.arch
            return True
        else:
            # address form
            result = core.BNLLILEmulatorSetEntryPoint(self.handle, addr_or_il)
            return result

    # ── Memory ────────────────────────────────────────────────────────────

    def read_memory(self, addr: int, length: int) -> bytes:
        """Read *length* bytes from emulator memory at *addr*."""
        buf = (ctypes.c_ubyte * length)()
        n = core.BNILEmulatorReadMemory(self._get_base(), buf, addr, length)
        return bytes(buf[:n])

    def write_memory(self, addr: int, data: bytes) -> int:
        """Write *data* to emulator memory at *addr*.  Returns bytes written."""
        buf = (ctypes.c_ubyte * len(data))(*data)
        return core.BNILEmulatorWriteMemory(self._get_base(), addr, buf, len(data))

    # ── Breakpoints ───────────────────────────────────────────────────────

    def add_breakpoint(self, addr: int):
        core.BNILEmulatorAddBreakpoint(self._get_base(), addr)

    def remove_breakpoint(self, addr: int):
        core.BNILEmulatorRemoveBreakpoint(self._get_base(), addr)

    def clear_breakpoints(self):
        core.BNILEmulatorClearBreakpoints(self._get_base())

    # ── Limits ────────────────────────────────────────────────────────────

    def set_max_instructions(self, max_count: int):
        core.BNILEmulatorSetMaxInstructions(self._get_base(), max_count)

    # ── Register / flag access ────────────────────────────────────────────

    def _resolve_reg(self, reg) -> int:
        """Accept register name (str) or numeric register ID (int)."""
        if isinstance(reg, str):
            return self._arch.regs[reg].index
        return reg

    def get_register(self, reg) -> int:
        """Get register value.  *reg* can be a name (``'rax'``) or numeric ID."""
        return core.BNLLILEmulatorGetRegister(self.handle, self._resolve_reg(reg))

    def set_register(self, reg, value: int):
        """Set register value.  *reg* can be a name (``'rax'``) or numeric ID."""
        core.BNLLILEmulatorSetRegister(self.handle, self._resolve_reg(reg), value)

    def get_temp_register(self, index: int) -> int:
        return core.BNLLILEmulatorGetTempRegister(self.handle, index)

    def set_temp_register(self, index: int, value: int):
        core.BNLLILEmulatorSetTempRegister(self.handle, index, value)

    def get_flag(self, flag) -> int:
        """Get flag value.  *flag* can be a name (``'z'``) or numeric ID."""
        if isinstance(flag, str):
            flag = self._arch._flags[flag]
        return core.BNLLILEmulatorGetFlag(self.handle, flag)

    def set_flag(self, flag, value: int):
        """Set flag value.  *flag* can be a name (``'z'``) or numeric ID."""
        if isinstance(flag, str):
            flag = self._arch._flags[flag]
        core.BNLLILEmulatorSetFlag(self.handle, flag, value)

    @property
    def regs(self) -> Dict[str, int]:
        """Snapshot of all named registers as ``{name: value}``."""
        result = {}
        for name in self._arch.regs:
            result[name] = self.get_register(name)
        return result

    # ── Hooks ─────────────────────────────────────────────────────────────

    def set_call_hook(self, callback: Optional[Callable[['LLILEmulator', int], bool]]):
        """Set a hook called on CALL instructions.

        The callback receives ``(emulator, target_address)`` and should return
        ``True`` to indicate the call was handled (emulator advances past the
        call) or ``False`` to let the emulator try cross-function emulation.
        Pass ``None`` to remove the hook.
        """
        self._call_hook = callback
        if callback is None:
            core.BNILEmulatorSetCallHook(self._get_base(), None, None)
            self._call_hook_cb = None
            return

        @ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p,
            ctypes.POINTER(core.BNILEmulator), ctypes.c_ulonglong)
        def _cb(ctxt, emu, target):
            try:
                return self._call_hook(self, target)
            except:
                return False

        self._call_hook_cb = _cb
        core.BNILEmulatorSetCallHook(self._get_base(), None, _cb)

    def set_syscall_hook(self, callback: Optional[Callable[['LLILEmulator'], bool]]):
        """Set a hook called on SYSCALL instructions.

        Return ``True`` if handled, ``False`` to stop.
        """
        self._syscall_hook = callback
        if callback is None:
            core.BNILEmulatorSetSyscallHook(self._get_base(), None, None)
            self._syscall_hook_cb = None
            return

        @ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p,
            ctypes.POINTER(core.BNILEmulator))
        def _cb(ctxt, emu):
            try:
                return self._syscall_hook(self)
            except:
                return False

        self._syscall_hook_cb = _cb
        core.BNILEmulatorSetSyscallHook(self._get_base(), None, _cb)

    def set_memory_read_hook(
        self,
        callback: Optional[Callable[['LLILEmulator', int, int], Optional[int]]],
    ):
        """Set a hook called on every memory read.

        The callback receives ``(emulator, address, size)`` and should return
        the value to use, or ``None`` to fall through to normal memory.
        """
        self._memory_read_hook = callback
        if callback is None:
            core.BNILEmulatorSetMemoryReadHook(self._get_base(), None, None)
            self._memory_read_hook_cb = None
            return

        @ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p,
            ctypes.POINTER(core.BNILEmulator),
            ctypes.c_ulonglong, ctypes.c_ulonglong,
            ctypes.POINTER(ctypes.c_ulonglong))
        def _cb(ctxt, emu, addr, size, out_value):
            try:
                result = self._memory_read_hook(self, addr, size)
                if result is not None:
                    out_value[0] = result
                    return True
                return False
            except:
                return False

        self._memory_read_hook_cb = _cb
        core.BNILEmulatorSetMemoryReadHook(self._get_base(), None, _cb)

    def set_memory_write_hook(
        self,
        callback: Optional[Callable[['LLILEmulator', int, int, int], bool]],
    ):
        """Set a hook called on every memory write.

        The callback receives ``(emulator, address, size, value)`` and should
        return ``True`` if handled, ``False`` to let the write proceed normally.
        """
        self._memory_write_hook = callback
        if callback is None:
            core.BNILEmulatorSetMemoryWriteHook(self._get_base(), None, None)
            self._memory_write_hook_cb = None
            return

        @ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p,
            ctypes.POINTER(core.BNILEmulator),
            ctypes.c_ulonglong, ctypes.c_ulonglong, ctypes.c_ulonglong)
        def _cb(ctxt, emu, addr, size, value):
            try:
                return self._memory_write_hook(self, addr, size, value)
            except:
                return False

        self._memory_write_hook_cb = _cb
        core.BNILEmulatorSetMemoryWriteHook(self._get_base(), None, _cb)

    def set_pre_instruction_hook(
        self,
        callback: Optional[Callable[['LLILEmulator', int], bool]],
    ):
        """Set a hook called before each instruction executes.

        The callback receives ``(emulator, instruction_index)`` and should
        return ``True`` to continue or ``False`` to stop.
        """
        self._pre_instruction_hook = callback
        if callback is None:
            core.BNILEmulatorSetPreInstructionHook(self._get_base(), None, None)
            self._pre_instruction_hook_cb = None
            return

        @ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p,
            ctypes.POINTER(core.BNILEmulator), ctypes.c_ulonglong)
        def _cb(ctxt, emu, instr_index):
            try:
                return self._pre_instruction_hook(self, instr_index)
            except:
                return False

        self._pre_instruction_hook_cb = _cb
        core.BNILEmulatorSetPreInstructionHook(self._get_base(), None, _cb)

    def set_intrinsic_hook(
        self,
        callback: Optional[Callable[
            ['LLILEmulator', int, List[int]],
            Optional[List[Tuple[int, int]]],
        ]],
    ):
        """Set a hook called on INTRINSIC instructions.

        The callback receives ``(emulator, intrinsic_id, params)`` and should
        return a list of ``(register_id, value)`` pairs if handled, or ``None``
        to stop with Unimplemented.
        """
        self._intrinsic_hook = callback
        if callback is None:
            core.BNLLILEmulatorSetIntrinsicHook(self.handle, None, None)
            self._intrinsic_hook_cb = None
            return

        @ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p,
            ctypes.POINTER(core.BNLLILEmulator), ctypes.c_uint,
            ctypes.POINTER(ctypes.c_ulonglong), ctypes.c_ulonglong,
            ctypes.POINTER(ctypes.c_ulonglong), ctypes.POINTER(ctypes.c_uint),
            ctypes.POINTER(ctypes.c_ulonglong))
        def _cb(ctxt, emu, intrinsic, params, param_count, out_values, out_regs, out_count):
            try:
                param_list = [params[i] for i in range(param_count)]
                result = self._intrinsic_hook(self, intrinsic, param_list)
                if result is not None:
                    out_count[0] = len(result)
                    for i, (reg, val) in enumerate(result):
                        out_regs[i] = reg
                        out_values[i] = val
                    return True
                return False
            except:
                return False

        self._intrinsic_hook_cb = _cb
        core.BNLLILEmulatorSetIntrinsicHook(self.handle, None, _cb)

    # ── Reset ─────────────────────────────────────────────────────────────

    def reset(self):
        """Reset all emulator state (registers, memory, flags, call stack)."""
        core.BNILEmulatorReset(self._get_base())
