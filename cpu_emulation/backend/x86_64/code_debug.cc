/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// x86_64 machine IR insns debugging.

#include <cinttypes>
#include <string>

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/logging.h"
#include "berberis/base/stringprintf.h"

// TODO(b/179708579): share this code with 32-bit backend.

using std::string;

namespace berberis {

const char* GetMachineHardRegDebugName(MachineReg r) {
  static const char* kHardRegs[] = {
      "?",    "r8",     "r9",   "r10",   "r11",   "rsi",   "rdi",   "rax",   "rbx",
      "rcx",  "rdx",    "rbp",  "rsp",   "r12",   "r13",   "r14",   "r15",   "?",
      "?",    "eflags", "xmm0", "xmm1",  "xmm2",  "xmm3",  "xmm4",  "xmm5",  "xmm6",
      "xmm7", "xmm8",   "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
  };
  CHECK_LT(static_cast<unsigned>(r.reg()), std::size(kHardRegs));
  return kHardRegs[r.reg()];
}

namespace x86_64 {

string Enter::GetDebugString() const {
  string out(StringPrintf("ENTER "));
  for (int i = 0; i < NumRegOperands(); ++i) {
    if (i != 0) {
      out += ", ";
    }
    out += GetRegOperandDebugString(this, i);
  }
  return out;
}

std::string GetDebugString(const MachineInsnX86_64& insn,
                           const char* mnemo,
                           size_t x86_operands_count,
                           const X86Operand x86_operands[]) {
  std::string result{};
  size_t reg_idx = 0, mem_idx = 0;
  for (size_t position = 0; position < x86_operands_count; position++) {
    if (result.empty()) {
      result = mnemo;
      result += " ";
    } else if (position > 0 && x86_operands[position - 1] == X86Operand::kComment &&
               x86_operands[position] == X86Operand::kImmediate) {
      if (insn.disp2() || insn.disp3()) {
        result += " @";
      }
    } else {
      result += ", ";
    }
    switch (x86_operands[position]) {
      case X86Operand::kRegisterOperand:
        result += GetRegOperandDebugString(&insn, reg_idx++);
        break;
      case X86Operand::kImplicitRegisterOperand:
        result += StringPrintf("(%s)", GetRegOperandDebugString(&insn, reg_idx++).c_str());
        break;
      case X86Operand::kSSAUseDefRegisterOperand:
        result += GetRegOperandDebugString(&insn, reg_idx++);
        result += "/";
        result += GetRegOperandDebugString(&insn, reg_idx++);
        break;
      case X86Operand::kImplicitSSAUseDefRegisterOperand:
        result += StringPrintf("(%s", GetRegOperandDebugString(&insn, reg_idx++).c_str());
        result += "/";
        result += StringPrintf("%s)", GetRegOperandDebugString(&insn, reg_idx++).c_str());
        break;
      case X86Operand::kMemoryOperand: {
        auto [has_base, has_index] = OpcodeHasMemoryBaseIndex(insn.opcode(), mem_idx++);
        int32_t scale;
        if (has_index) {
          scale = 1 << (mem_idx == 1 ? insn.scale() : mem_idx == 2 ? insn.scale2() : insn.scale3());
        }
        int32_t disp = mem_idx == 1 ? insn.disp() : mem_idx == 2 ? insn.disp2() : insn.disp3();
        if (has_base) {
          if (has_index) {
            result += StringPrintf("[%s + %s * %d + 0x%x]",
                                   GetRegOperandDebugString(&insn, reg_idx).c_str(),
                                   GetRegOperandDebugString(&insn, reg_idx + 1).c_str(),
                                   scale,
                                   disp);
            reg_idx += 2;
          } else {
            result += StringPrintf(
                "[%s + 0x%x]", GetRegOperandDebugString(&insn, reg_idx++).c_str(), disp);
          }
        } else if (has_index) {
          result += StringPrintf(
              "[%s * %d + 0x%x]", GetRegOperandDebugString(&insn, reg_idx++).c_str(), scale, disp);
        } else {
          result += StringPrintf("[0x%x]", disp);
        }
        break;
      }
      case X86Operand::kCondition:
        result += GetCondName(insn.cond());
        break;
      case X86Operand::kImmediate:
        result += StringPrintf("0x%" PRIx64, insn.imm());
        break;
      case X86Operand::kComment:
        if (insn.disp2() || insn.disp3()) {
          result += bit_cast<char*>(uint64_t(insn.disp3()) << 32 | insn.disp2());
        }
        break;
    }
  }
  if (insn.recovery_pc() && !IsConfigFlagSet(kDeterministicTracing)) {
    result += StringPrintf(" <0x%" PRIxPTR ">", insn.recovery_pc());
  }
  return result;
}

}  // namespace x86_64

}  // namespace berberis
