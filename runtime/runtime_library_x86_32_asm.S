/*
 * Copyright (C) 2016 The Android Open Source Project
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

// "Calling conventions" among generated code and trampolines
// ==========================================================
//
// Introduction
// ------------
//
// To ensure the high performance of our generated code, we employ a couple of
// techniques:
//
// - We allow generated regions to jump among them without transferring control
//   back to Berberis runtime.
//
// - We use custom "calling conventions" that are different from the standard
//   x86 calling conventions, with some items passed in registers.
//
// Entry and exits
// ---------------
//
// Upon entry into generated code and trampoline adapters, we must have:
//
// - %ebp pointing to CPUState,
//
// - every field in CPUState up to date, except insn_addr, and
//
// - %eax containing up-to-date value for potentially stale CPUState::insn_addr.
//
// Since we jump among generated code and trampolines, each region must adhere
// to the "calling conventions" above as it exits.
//
// Each region is allowed to use the stack pointed to by %esp.  However, it must
// restore %esp before exiting.
//
// All other registers (%ebx, %ecx, %edx, %edi, %esi, and SIMD registers) are
// "caller saved".  That is, regions are allowed to use them without restoring
// their original values.
//
// Berberis runtime -> generated code
// ---------------------------------
//
// If we are transferring control to generated code and trampolines from Berberis
// runtime, such as ExecuteGuest, then we must do so via
// berberis_RunGeneratedCode, which is responsible for setting up
// registers for the "calling conventions".
//
// Generated code -> Berberis runtime
// ---------------------------------
//
// When we exiting generate code, we must do so via END_GENERATED_CODE macro
// defined in this file.  The macro ensures that CPUState is fully up to date,
// including insn_addr, before transferring control back to Berberis runtime.
//
// CALL_INTERP
// ---------------------------------
//
// If a region needs to perform a CALL_INTERP, it must ensure that every field
// in CPUState is up to date, including insn_addr.  CALL_INTERP follows the
// regular x86 ABI.  As such, it does not expect or return insn_addr via %eax.


    .globl berberis_RunGeneratedCode
    .balign 16

berberis_RunGeneratedCode:

    // On x86-32 Linux, stack should be aligned on 16 at every call insn.
    // That means stack is 12 mod 16 on function entry.
    // See https://01.org/sites/default/files/file_attach/intel386-psabi-1.0.pdf
    //
    // Stack:
    //  0:               <- stack after prologue, aligned for next call
    //  4:
    //  8:
    // 12: saved esi     <- stack after prologue
    // 16: saved edi
    // 20: saved ebx
    // 24: saved ebp
    // 28: return addr
    // 32: arg0 - state  <- stack at call insn - aligned on 16
    // 36: arg1 - code

    // Prologue
    pushl %ebp
    // movl %esp, %ebp  // kOmitFramePointer
    pushl %ebx
    pushl %edi
    pushl %esi

    // Align stack for next call
    subl $12, %esp  // kStackAlignAtCall, kFrameSizeAtTranslatedCode

    // Set state pointer
    movl 32(%esp), %ebp  // kStateRegister, kOmitFramePointer

    // Set insn_addr.
    movl 0x48(%ebp), %eax

    // Set kInsideGeneratedCode residence.
    movb $1, 0x169(%ebp)

    // Jump to entry
    jmp *36(%esp)


// Perform all the steps needed to exit generated code except return, which is
// up to the users of this macro.  The users of this macro may choose to perform
// a sibling call as necessary.
.macro END_GENERATED_CODE
    // Sync insn_addr.
    movl %eax, 0x48(%ebp)
    // Set kOutsideGeneratedCode residence.
    movb $0, 0x169(%ebp)

    // Restore stack
    addl $12, %esp  // kFrameSizeAtTranslatedCode

    // Epilogue
    popl %esi
    popl %edi
    popl %ebx
    popl %ebp
.endm


    .globl berberis_entry_ExitGeneratedCode
    .balign 16

berberis_entry_ExitGeneratedCode:

    END_GENERATED_CODE
    ret


    .globl berberis_entry_Stop
    .balign 16

berberis_entry_Stop:

    END_GENERATED_CODE
    ret


    .extern berberis_HandleNoExec
    .globl berberis_entry_NoExec
    .balign 16

berberis_entry_NoExec:

    END_GENERATED_CODE

    // void berberis_HandleNoExec(ThreadState*);
    // Perform a sibling call to berberis_HandleNoExec.  We can safely do
    // so because 0x4(%esp) still contains "state", which is passed from the
    // caller of berberis_RunGeneratedCode.  We could call the function
    // here instead of jumping to it, but it would be more work to do so because
    // we would have to align the stack, set up the argument, issue the "ret"
    // instruction after the call.
    // TODO(kazu): Remove state from HandleNoExec parameters.  Get it from the
    // guest thread instead.
    jmp berberis_HandleNoExec


    .globl berberis_entry_Interpret
    .balign 16

berberis_entry_Interpret:

    // Not called for arm, but the function needs to be defined to link with
    // berberis runtime_library.h, which creates a constant pointer to this
    // function. Emit ud2 to force a crash so we can detect if this has
    // accidentally been called.
    ud2


    .extern berberis_HandleUnpredictable
    .globl berberis_entry_Unpredictable
    .balign 16

berberis_entry_Unpredictable:

    END_GENERATED_CODE

    // void berberis_HandleUnpredictable(ThreadState*);
    // See the comment above about the sibling call.
    jmp berberis_HandleUnpredictable


    .extern berberis_HandleNotTranslated
    .globl berberis_entry_NotTranslated
    .balign 16

berberis_entry_NotTranslated:

    END_GENERATED_CODE

    // void berberis_HandleNotTranslated(ThreadState*);
    // See the comment above about the sibling call.
    jmp berberis_HandleNotTranslated


    .globl berberis_entry_Translating
    .balign 16

berberis_entry_Translating:

    // TODO(levarum): Run interpreter while translation is in progress.
    END_GENERATED_CODE
    ret


    .globl berberis_entry_Invalidating
    .balign 16

berberis_entry_Invalidating:

    // TODO(levarum): maybe call sched_yield() here.
    END_GENERATED_CODE
    ret


    .globl berberis_entry_Wrapping
    .balign 16

berberis_entry_Wrapping:

    // TODO(levarum): maybe call sched_yield() here.
    END_GENERATED_CODE
    ret
