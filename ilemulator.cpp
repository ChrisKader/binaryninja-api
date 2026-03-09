// Copyright (c) 2015-2026 Vector 35 Inc
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "binaryninjaapi.h"

using namespace BinaryNinja;


LLILEmulator::LLILEmulator(Ref<BinaryView> view)
{
	m_object = BNCreateLLILEmulatorForView(view->GetObject());
}


LLILEmulator::LLILEmulator(Ref<LowLevelILFunction> il, Ref<BinaryView> view)
{
	m_object = BNCreateLLILEmulator(il->GetObject(), view->GetObject());
}


LLILEmulator::LLILEmulator(BNLLILEmulator* emu)
{
	m_object = emu;
}


bool LLILEmulator::SetEntryPoint(uint64_t addr)
{
	return BNLLILEmulatorSetEntryPoint(m_object, addr);
}


void LLILEmulator::SetEntryPoint(Ref<LowLevelILFunction> il, size_t instrIndex)
{
	BNLLILEmulatorSetEntryPointForIL(m_object, il->GetObject(), instrIndex);
}


// ─── Execution ───────────────────────────────────────────────────────────────

BNILEmulatorStopReason LLILEmulator::Run()
{
	return BNILEmulatorRun(BNLLILEmulatorGetBase(m_object));
}


BNILEmulatorStopReason LLILEmulator::Step()
{
	return BNILEmulatorStep(BNLLILEmulatorGetBase(m_object));
}


BNILEmulatorStopReason LLILEmulator::StepN(size_t n)
{
	return BNILEmulatorStepN(BNLLILEmulatorGetBase(m_object), n);
}


// ─── State ───────────────────────────────────────────────────────────────────

size_t LLILEmulator::GetInstructionIndex() const
{
	return BNILEmulatorGetInstructionIndex(BNLLILEmulatorGetBase(m_object));
}


void LLILEmulator::SetInstructionIndex(size_t index)
{
	BNILEmulatorSetInstructionIndex(BNLLILEmulatorGetBase(m_object), index);
}


uint64_t LLILEmulator::GetCurrentAddress() const
{
	return BNILEmulatorGetCurrentAddress(BNLLILEmulatorGetBase(m_object));
}


BNILEmulatorStopReason LLILEmulator::GetStopReason() const
{
	return BNILEmulatorGetStopReason(BNLLILEmulatorGetBase(m_object));
}


std::string LLILEmulator::GetStopMessage() const
{
	char* msg = BNILEmulatorGetStopMessage(BNLLILEmulatorGetBase(m_object));
	std::string result(msg);
	BNFreeString(msg);
	return result;
}


// ─── Memory ──────────────────────────────────────────────────────────────────

size_t LLILEmulator::ReadMemory(void* dest, uint64_t addr, size_t len) const
{
	return BNILEmulatorReadMemory(BNLLILEmulatorGetBase(m_object), dest, addr, len);
}


size_t LLILEmulator::WriteMemory(uint64_t addr, const void* src, size_t len)
{
	return BNILEmulatorWriteMemory(BNLLILEmulatorGetBase(m_object), addr, src, len);
}


void LLILEmulator::MapMemory(uint64_t addr, const void* data, size_t len)
{
	BNILEmulatorMapMemory(BNLLILEmulatorGetBase(m_object), addr, data, len);
}


void LLILEmulator::MapMemory(uint64_t addr, size_t len)
{
	BNILEmulatorMapMemoryZero(BNLLILEmulatorGetBase(m_object), addr, len);
}


// ─── Breakpoints ─────────────────────────────────────────────────────────────

void LLILEmulator::AddBreakpoint(uint64_t addr)
{
	BNILEmulatorAddBreakpoint(BNLLILEmulatorGetBase(m_object), addr);
}


void LLILEmulator::RemoveBreakpoint(uint64_t addr)
{
	BNILEmulatorRemoveBreakpoint(BNLLILEmulatorGetBase(m_object), addr);
}


void LLILEmulator::ClearBreakpoints()
{
	BNILEmulatorClearBreakpoints(BNLLILEmulatorGetBase(m_object));
}


// ─── Limits ──────────────────────────────────────────────────────────────────

void LLILEmulator::SetMaxInstructions(size_t max)
{
	BNILEmulatorSetMaxInstructions(BNLLILEmulatorGetBase(m_object), max);
}


size_t LLILEmulator::GetInstructionsExecuted() const
{
	return BNILEmulatorGetInstructionsExecuted(BNLLILEmulatorGetBase(m_object));
}


// ─── Hook bridge callbacks ───────────────────────────────────────────────────

bool LLILEmulator::CallHookCallback(void* ctxt, BNILEmulator*, uint64_t target)
{
	LLILEmulator* self = (LLILEmulator*)ctxt;
	return self->m_callHook(self, target);
}


bool LLILEmulator::SyscallHookCallback(void* ctxt, BNILEmulator*)
{
	LLILEmulator* self = (LLILEmulator*)ctxt;
	return self->m_syscallHook(self);
}


bool LLILEmulator::MemoryReadHookCallback(
	void* ctxt, BNILEmulator*, uint64_t addr, size_t size, uint64_t* value)
{
	LLILEmulator* self = (LLILEmulator*)ctxt;
	return self->m_memoryReadHook(self, addr, size, *value);
}


bool LLILEmulator::MemoryWriteHookCallback(
	void* ctxt, BNILEmulator*, uint64_t addr, size_t size, uint64_t value)
{
	LLILEmulator* self = (LLILEmulator*)ctxt;
	return self->m_memoryWriteHook(self, addr, size, value);
}


bool LLILEmulator::PreInstructionHookCallback(void* ctxt, BNILEmulator*, size_t instrIndex)
{
	LLILEmulator* self = (LLILEmulator*)ctxt;
	return self->m_preInstructionHook(self, instrIndex);
}


bool LLILEmulator::IntrinsicHookCallback(void* ctxt, BNLLILEmulator*,
	uint32_t intrinsic, const uint64_t* params, size_t paramCount,
	uint64_t* outValues, uint32_t* outRegs, size_t* outCount)
{
	LLILEmulator* self = (LLILEmulator*)ctxt;
	std::vector<uint64_t> paramVec(params, params + paramCount);
	std::vector<std::pair<uint32_t, uint64_t>> outputs;

	bool result = self->m_intrinsicHook(self, intrinsic, paramVec, outputs);

	size_t count = outputs.size();
	if (outCount)
		*outCount = count;
	for (size_t i = 0; i < count; i++)
	{
		if (outRegs)
			outRegs[i] = outputs[i].first;
		if (outValues)
			outValues[i] = outputs[i].second;
	}
	return result;
}


// ─── Hook setters ────────────────────────────────────────────────────────────

void LLILEmulator::SetCallHook(const std::function<bool(LLILEmulator*, uint64_t)>& hook)
{
	m_callHook = hook;
	BNILEmulatorSetCallHook(BNLLILEmulatorGetBase(m_object),
		hook ? (void*)this : nullptr,
		hook ? CallHookCallback : nullptr);
}


void LLILEmulator::SetSyscallHook(const std::function<bool(LLILEmulator*)>& hook)
{
	m_syscallHook = hook;
	BNILEmulatorSetSyscallHook(BNLLILEmulatorGetBase(m_object),
		hook ? (void*)this : nullptr,
		hook ? SyscallHookCallback : nullptr);
}


void LLILEmulator::SetMemoryReadHook(
	const std::function<bool(LLILEmulator*, uint64_t, size_t, uint64_t&)>& hook)
{
	m_memoryReadHook = hook;
	BNILEmulatorSetMemoryReadHook(BNLLILEmulatorGetBase(m_object),
		hook ? (void*)this : nullptr,
		hook ? MemoryReadHookCallback : nullptr);
}


void LLILEmulator::SetMemoryWriteHook(
	const std::function<bool(LLILEmulator*, uint64_t, size_t, uint64_t)>& hook)
{
	m_memoryWriteHook = hook;
	BNILEmulatorSetMemoryWriteHook(BNLLILEmulatorGetBase(m_object),
		hook ? (void*)this : nullptr,
		hook ? MemoryWriteHookCallback : nullptr);
}


void LLILEmulator::SetPreInstructionHook(const std::function<bool(LLILEmulator*, size_t)>& hook)
{
	m_preInstructionHook = hook;
	BNILEmulatorSetPreInstructionHook(BNLLILEmulatorGetBase(m_object),
		hook ? (void*)this : nullptr,
		hook ? PreInstructionHookCallback : nullptr);
}


void LLILEmulator::SetIntrinsicHook(const std::function<bool(LLILEmulator*, uint32_t,
	const std::vector<uint64_t>&, std::vector<std::pair<uint32_t, uint64_t>>&)>& hook)
{
	m_intrinsicHook = hook;
	BNLLILEmulatorSetIntrinsicHook(m_object,
		hook ? (void*)this : nullptr,
		hook ? IntrinsicHookCallback : nullptr);
}


// ─── Register / flag / temp access ──────────────────────────────────────────

uint64_t LLILEmulator::GetRegister(uint32_t reg) const
{
	return BNLLILEmulatorGetRegister(m_object, reg);
}


void LLILEmulator::SetRegister(uint32_t reg, uint64_t value)
{
	BNLLILEmulatorSetRegister(m_object, reg, value);
}


uint64_t LLILEmulator::GetTempRegister(uint32_t index) const
{
	return BNLLILEmulatorGetTempRegister(m_object, index);
}


void LLILEmulator::SetTempRegister(uint32_t index, uint64_t value)
{
	BNLLILEmulatorSetTempRegister(m_object, index, value);
}


std::unordered_map<uint32_t, uint64_t> LLILEmulator::GetAllTempRegisters() const
{
	// Query count first, then fetch
	size_t count = BNLLILEmulatorGetAllTempRegisters(m_object, nullptr, nullptr, 0);
	std::unordered_map<uint32_t, uint64_t> result;
	if (count == 0)
		return result;
	std::vector<uint32_t> indices(count);
	std::vector<uint64_t> values(count);
	count = BNLLILEmulatorGetAllTempRegisters(m_object, indices.data(), values.data(), count);
	for (size_t i = 0; i < count; i++)
		result[indices[i]] = values[i];
	return result;
}


uint8_t LLILEmulator::GetFlag(uint32_t flag) const
{
	return BNLLILEmulatorGetFlag(m_object, flag);
}


void LLILEmulator::SetFlag(uint32_t flag, uint8_t value)
{
	BNLLILEmulatorSetFlag(m_object, flag, value);
}


// ─── Cross-function state ────────────────────────────────────────────────────

size_t LLILEmulator::GetCallStackDepth() const
{
	return BNLLILEmulatorGetCallStackDepth(m_object);
}


// ─── Reset ───────────────────────────────────────────────────────────────────

void LLILEmulator::Reset()
{
	BNILEmulatorReset(BNLLILEmulatorGetBase(m_object));
}
