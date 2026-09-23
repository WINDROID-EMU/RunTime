/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay & Rien Gupta, 2026 - Adapted for ReXGlue runtime (ARM64 Android)
 */

#include <rex/exception_handler.h>

#include <signal.h>
#include <ucontext.h>

#include <cstdint>
#include <cstring>

#include <rex/assert.h>
#include <rex/logging.h>
#include <rex/math.h>
#include <rex/platform.h>

namespace rex::arch {

bool signal_handlers_installed_ = false;
struct sigaction original_sigill_handler_;
struct sigaction original_sigsegv_handler_;

constexpr size_t kMaxHandlerCount = 8;
std::pair<ExceptionHandler::Handler, void*> handlers_[kMaxHandlerCount];

static void ExceptionHandlerCallback(int signal_number, siginfo_t* signal_info,
                                     void* signal_context) {
  mcontext_t& mcontext = reinterpret_cast<ucontext_t*>(signal_context)->uc_mcontext;

  HostThreadContext thread_context;

  // Linux/Android ARM64: mcontext_t is struct sigcontext
  std::memcpy(thread_context.x, mcontext.regs, sizeof(thread_context.x));
  thread_context.sp = mcontext.sp;
  thread_context.pc = mcontext.pc;
  thread_context.pstate = mcontext.pstate;

  struct fpsimd_context* mcontext_fpsimd = nullptr;
  struct esr_context* mcontext_esr = nullptr;
  for (struct _aarch64_ctx* mcontext_extension =
           reinterpret_cast<struct _aarch64_ctx*>(mcontext.__reserved);
       mcontext_extension->magic;
       mcontext_extension = reinterpret_cast<struct _aarch64_ctx*>(
           reinterpret_cast<uint8_t*>(mcontext_extension) + mcontext_extension->size)) {
    switch (mcontext_extension->magic) {
      case FPSIMD_MAGIC:
        mcontext_fpsimd = reinterpret_cast<struct fpsimd_context*>(mcontext_extension);
        break;
      case ESR_MAGIC:
        mcontext_esr = reinterpret_cast<struct esr_context*>(mcontext_extension);
        break;
      default:
        break;
    }
  }

  if (mcontext_fpsimd) {
    thread_context.fpsr = mcontext_fpsimd->fpsr;
    thread_context.fpcr = mcontext_fpsimd->fpcr;
    std::memcpy(thread_context.v, mcontext_fpsimd->vregs, sizeof(thread_context.v));
  }

  Exception ex;
  switch (signal_number) {
    case SIGILL:
      ex.InitializeIllegalInstruction(&thread_context);
      break;
    case SIGSEGV: {
      Exception::AccessViolationOperation access_violation_operation;
      if (mcontext_esr && ((mcontext_esr->esr >> 26) & 0b111110) == 0b100100) {
        access_violation_operation = (mcontext_esr->esr & (UINT64_C(1) << 6))
                                         ? Exception::AccessViolationOperation::kWrite
                                         : Exception::AccessViolationOperation::kRead;
      } else {
        bool instruction_is_store;
        if (IsArm64LoadPrefetchStore(*reinterpret_cast<const uint32_t*>(mcontext.pc),
                                     instruction_is_store)) {
          access_violation_operation = instruction_is_store
                                           ? Exception::AccessViolationOperation::kWrite
                                           : Exception::AccessViolationOperation::kRead;
        } else {
          access_violation_operation = Exception::AccessViolationOperation::kUnknown;
        }
      }
      ex.InitializeAccessViolation(&thread_context,
                                   reinterpret_cast<uint64_t>(signal_info->si_addr),
                                   access_violation_operation);
    } break;
    default:
      assert_unhandled_case(signal_number);
      return;
  }

  for (size_t i = 0; i < rex::countof(handlers_) && handlers_[i].first; ++i) {
    if (handlers_[i].first(&ex, handlers_[i].second)) {
      // Exception handled.
      uint32_t modified_register_index;
      uint32_t modified_x_registers_remaining = ex.modified_x_registers();
      while (rex::bit_scan_forward(modified_x_registers_remaining, &modified_register_index)) {
        modified_x_registers_remaining &= ~(UINT32_C(1) << modified_register_index);
        mcontext.regs[modified_register_index] = thread_context.x[modified_register_index];
      }
      mcontext.sp = thread_context.sp;
      mcontext.pc = thread_context.pc;
      mcontext.pstate = thread_context.pstate;
      if (mcontext_fpsimd) {
        mcontext_fpsimd->fpsr = thread_context.fpsr;
        mcontext_fpsimd->fpcr = thread_context.fpcr;
        uint32_t modified_v_registers_remaining = ex.modified_v_registers();
        while (rex::bit_scan_forward(modified_v_registers_remaining, &modified_register_index)) {
          modified_v_registers_remaining &= ~(UINT32_C(1) << modified_register_index);
          std::memcpy(&mcontext_fpsimd->vregs[modified_register_index],
                      &thread_context.v[modified_register_index], sizeof(vec128_t));
          mcontext.regs[modified_register_index] = thread_context.x[modified_register_index];
        }
      }
      return;
    }
  }
}

void ExceptionHandler::Install(Handler fn, void* data) {
  if (!signal_handlers_installed_) {
    struct sigaction signal_handler;

    std::memset(&signal_handler, 0, sizeof(signal_handler));
    signal_handler.sa_sigaction = ExceptionHandlerCallback;
    signal_handler.sa_flags = SA_SIGINFO;

    if (sigaction(SIGILL, &signal_handler, &original_sigill_handler_) != 0) {
      assert_always("Failed to install new SIGILL handler");
    }
    if (sigaction(SIGSEGV, &signal_handler, &original_sigsegv_handler_) != 0) {
      assert_always("Failed to install new SIGSEGV handler");
    }
    signal_handlers_installed_ = true;
  }

  for (size_t i = 0; i < rex::countof(handlers_); ++i) {
    if (!handlers_[i].first) {
      handlers_[i].first = fn;
      handlers_[i].second = data;
      return;
    }
  }
  assert_always("Too many exception handlers installed");
}

void ExceptionHandler::Uninstall(Handler fn, void* data) {
  for (size_t i = 0; i < rex::countof(handlers_); ++i) {
    if (handlers_[i].first == fn && handlers_[i].second == data) {
      for (; i < rex::countof(handlers_) - 1; ++i) {
        handlers_[i] = handlers_[i + 1];
      }
      handlers_[i].first = nullptr;
      handlers_[i].second = nullptr;
      break;
    }
  }

  bool has_any = false;
  for (size_t i = 0; i < rex::countof(handlers_); ++i) {
    if (handlers_[i].first) {
      has_any = true;
      break;
    }
  }
  if (!has_any) {
    if (signal_handlers_installed_) {
      if (sigaction(SIGILL, &original_sigill_handler_, NULL) != 0) {
        assert_always("Failed to restore original SIGILL handler");
      }
      if (sigaction(SIGSEGV, &original_sigsegv_handler_, NULL) != 0) {
        assert_always("Failed to restore original SIGSEGV handler");
      }
      signal_handlers_installed_ = false;
    }
  }
}

}  // namespace rex::arch
