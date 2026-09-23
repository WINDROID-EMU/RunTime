/**
 * @file        rex/core/fiber_posix.cpp
 * @brief       ARM64 Android backend for rex::thread::Fiber
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <rex/thread/fiber.h>

#include <cassert>
#include <cstdint>

#if defined(__aarch64__) || defined(_M_ARM64)
extern "C" void rex_fiber_switch_asm(rex::thread::Aarch64FiberContext* from,
                                     const rex::thread::Aarch64FiberContext* to);

__asm__(
    ".text\n"
    ".global rex_fiber_switch_asm\n"
    ".type rex_fiber_switch_asm, %function\n"
    "rex_fiber_switch_asm:\n"
    "  stp x19, x20, [x0, #0]\n"
    "  stp x21, x22, [x0, #16]\n"
    "  stp x23, x24, [x0, #32]\n"
    "  stp x25, x26, [x0, #48]\n"
    "  stp x27, x28, [x0, #64]\n"
    "  stp x29, x30, [x0, #80]\n"
    "  mov x2, sp\n"
    "  str x2, [x0, #96]\n"
    "  stp d8, d9,   [x0, #104]\n"
    "  stp d10, d11, [x0, #120]\n"
    "  stp d12, d13, [x0, #136]\n"
    "  stp d14, d15, [x0, #152]\n"
    "\n"
    "  ldp x19, x20, [x1, #0]\n"
    "  ldp x21, x22, [x1, #16]\n"
    "  ldp x23, x24, [x1, #32]\n"
    "  ldp x25, x26, [x1, #48]\n"
    "  ldp x27, x28, [x1, #64]\n"
    "  ldp x29, x30, [x1, #80]\n"
    "  ldr x2, [x1, #96]\n"
    "  mov sp, x2\n"
    "  ldp d8, d9,   [x1, #104]\n"
    "  ldp d10, d11, [x1, #120]\n"
    "  ldp d12, d13, [x1, #136]\n"
    "  ldp d14, d15, [x1, #152]\n"
    "  ret\n"
);
#endif

namespace rex::thread {

thread_local Fiber* Fiber::tls_current_ = nullptr;

Fiber* Fiber::ConvertCurrentThread() {
  auto* f = new Fiber();
  f->is_thread_fiber_ = true;
  tls_current_ = f;
  return f;
}

Fiber* Fiber::Create(size_t stack_size, void (*entry)(void*), void* arg) {
  auto* f = new Fiber();
  f->entry_ = entry;
  f->arg_ = arg;
  f->stack_.resize(stack_size);

  // Stack grows downwards, align SP to 16 bytes
  uintptr_t sp = (reinterpret_cast<uintptr_t>(f->stack_.data() + f->stack_.size())) & ~0xFULL;
  f->context_.sp = static_cast<uint64_t>(sp);
  f->context_.regs[10] = 0;  // fp (x29)
  f->context_.regs[11] = reinterpret_cast<uint64_t>(&Fiber::Trampoline);  // lr (x30)

  return f;
}

/*static*/ void Fiber::Trampoline() {
  Fiber* f = tls_current_;
  if (f && f->entry_) {
    f->entry_(f->arg_);
  }
}

void Fiber::SwitchTo(Fiber* target) {
  Fiber* from = tls_current_;
  tls_current_ = target;
#if defined(__aarch64__) || defined(_M_ARM64)
  rex_fiber_switch_asm(&from->context_, &target->context_);
#endif
}

void Fiber::Destroy() {
  if (is_thread_fiber_) {
    tls_current_ = nullptr;
  } else {
    assert(this != tls_current_ && "Destroy called on the currently running fiber");
  }
  delete this;
}

}  // namespace rex::thread
