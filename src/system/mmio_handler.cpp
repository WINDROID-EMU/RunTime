/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <algorithm>
#include <cstring>
#include <utility>

#include <rex/assert.h>
#include <rex/exception_handler.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/platform.h>
#include <rex/system/mmio_handler.h>
#include <rex/types.h>

using namespace rex::arch;

namespace rex::runtime {

MMIOHandler* MMIOHandler::global_handler_ = nullptr;

MMIOHandler* MMIOHandler::global_handler() {
  return global_handler_;
}

std::unique_ptr<MMIOHandler> MMIOHandler::Install(uint8_t* virtual_membase,
                                                  uint8_t* physical_membase, uint8_t* membase_end,
                                                  HostToGuestVirtual host_to_guest_virtual,
                                                  const void* host_to_guest_virtual_context,
                                                  AccessViolationCallback access_violation_callback,
                                                  void* access_violation_callback_context) {
  // There can be only one handler at a time.
  assert_null(global_handler_);
  if (global_handler_) {
    return nullptr;
  }

  auto handler = std::unique_ptr<MMIOHandler>(new MMIOHandler(
      virtual_membase, physical_membase, membase_end, host_to_guest_virtual,
      host_to_guest_virtual_context, access_violation_callback, access_violation_callback_context));

  // Install exception handler for memory coherence (SharedMemory write tracking).
  // Note: MMIO operations are handled at the recompiler level via REX_MM_LOAD/STORE
  // macros that call CheckLoad/CheckStore directly.
  arch::ExceptionHandler::Install(ExceptionCallbackThunk, handler.get());

  global_handler_ = handler.get();
  return handler;
}

MMIOHandler::MMIOHandler(uint8_t* virtual_membase, uint8_t* physical_membase, uint8_t* membase_end,
                         HostToGuestVirtual host_to_guest_virtual,
                         const void* host_to_guest_virtual_context,
                         AccessViolationCallback access_violation_callback,
                         void* access_violation_callback_context)
    : virtual_membase_(virtual_membase),
      physical_membase_(physical_membase),
      memory_end_(membase_end),
      host_to_guest_virtual_(host_to_guest_virtual),
      host_to_guest_virtual_context_(host_to_guest_virtual_context),
      access_violation_callback_(access_violation_callback),
      access_violation_callback_context_(access_violation_callback_context) {}

MMIOHandler::~MMIOHandler() {
  arch::ExceptionHandler::Uninstall(ExceptionCallbackThunk, this);

  if (global_handler_ == this) {
    global_handler_ = nullptr;
  } else {
    REXLOG_ERROR("~MMIOHandler: global_handler_ does not match this instance");
  }
}

bool MMIOHandler::RegisterRange(uint32_t virtual_address, uint32_t mask, uint32_t size,
                                void* context, MMIOReadCallback read_callback,
                                MMIOWriteCallback write_callback) {
  mapped_ranges_.push_back({
      virtual_address,
      mask,
      size,
      context,
      read_callback,
      write_callback,
  });
  return true;
}

MMIORange* MMIOHandler::LookupRange(uint32_t virtual_address) {
  for (auto& range : mapped_ranges_) {
    if ((virtual_address & range.mask) == range.address) {
      return &range;
    }
  }
  return nullptr;
}

bool MMIOHandler::CheckLoad(uint32_t virtual_address, uint32_t* out_value) {
  for (const auto& range : mapped_ranges_) {
    if ((virtual_address & range.mask) == range.address) {
      *out_value =
          static_cast<uint32_t>(range.read(nullptr, range.callback_context, virtual_address));
      return true;
    }
  }
  return false;
}

bool MMIOHandler::CheckStore(uint32_t virtual_address, uint32_t value) {
  for (const auto& range : mapped_ranges_) {
    if ((virtual_address & range.mask) == range.address) {
      range.write(nullptr, range.callback_context, virtual_address, value);
      return true;
    }
  }
  return false;
}

bool MMIOHandler::TryDecodeLoadStore(const uint8_t* p, DecodedLoadStore& decoded_out) {
  std::memset(&decoded_out, 0, sizeof(decoded_out));
  decoded_out.length = sizeof(uint32_t);
  uint32_t instruction = *reinterpret_cast<const uint32_t*>(p);

  // Literal loading (PC-relative) is not handled.

  if ((instruction & kArm64LoadStoreAnyFMask) != kArm64LoadStoreAnyFixed) {
    // Not a load or a store instruction.
    return false;
  }

  if ((instruction & kArm64LoadStorePairAnyFMask) == kArm64LoadStorePairAnyFixed) {
    // Handling MMIO only for single 32-bit values, not for pairs.
    return false;
  }

  uint8_t value_reg_base;
  switch (Arm64LoadStoreOp(instruction & kArm64LoadStoreMask)) {
    case Arm64LoadStoreOp::kSTR_w:
      decoded_out.is_load = false;
      value_reg_base = DecodedLoadStore::kArm64ValueRegX0;
      break;
    case Arm64LoadStoreOp::kLDR_w:
      decoded_out.is_load = true;
      value_reg_base = DecodedLoadStore::kArm64ValueRegX0;
      break;
    case Arm64LoadStoreOp::kSTR_s:
      decoded_out.is_load = false;
      value_reg_base = DecodedLoadStore::kArm64ValueRegV0;
      break;
    case Arm64LoadStoreOp::kLDR_s:
      decoded_out.is_load = true;
      value_reg_base = DecodedLoadStore::kArm64ValueRegV0;
      break;
    default:
      return false;
  }

  // `Rt` field (load / store register).
  decoded_out.value_reg = value_reg_base + (instruction & 31);
  if (decoded_out.is_load && decoded_out.value_reg == DecodedLoadStore::kArm64ValueRegZero) {
    // Zero constant rather than a register read.
    decoded_out.is_constant = true;
    decoded_out.constant = 0;
  }

  decoded_out.mem_has_base = true;
  // The base is Xn (for 0...30) or SP (for 31).
  // `Rn` field (first source register).
  decoded_out.mem_base_reg = (instruction >> 5) & 31;

  bool is_unsigned_offset =
      (instruction & kArm64LoadStoreUnsignedOffsetFMask) == kArm64LoadStoreUnsignedOffsetFixed;
  if (is_unsigned_offset) {
    // LDR|STR Wt|St, [Xn|SP{, #pimm}]
    uint32_t unsigned_offset = (instruction >> 10) & 4095;
    decoded_out.mem_displacement = ptrdiff_t(sizeof(uint32_t) * unsigned_offset);
  } else {
    Arm64LoadStoreOffsetFixed offset =
        Arm64LoadStoreOffsetFixed(instruction & kArm64LoadStoreOffsetFMask);
    int32_t signed_offset = int32_t(instruction << (32 - (9 + 12))) >> (32 - 9);
    switch (offset) {
      case Arm64LoadStoreOffsetFixed::kUnscaledOffset: {
        decoded_out.mem_displacement = signed_offset;
      } break;
      case Arm64LoadStoreOffsetFixed::kPostIndex: {
        decoded_out.mem_base_writeback = true;
        decoded_out.mem_base_writeback_offset = signed_offset;
      } break;
      case Arm64LoadStoreOffsetFixed::kPreIndex: {
        decoded_out.mem_base_writeback = true;
        decoded_out.mem_base_writeback_offset = signed_offset;
        decoded_out.mem_displacement = signed_offset;
      } break;
      case Arm64LoadStoreOffsetFixed::kRegisterOffset: {
        decoded_out.mem_index_reg = (instruction >> 16) & 31;
        if (decoded_out.mem_index_reg != DecodedLoadStore::kArm64RegZero) {
          decoded_out.mem_has_index = true;
          uint32_t extend_mode = (instruction >> 13) & 0b111;
          if (!(extend_mode & 0b010)) {
            return false;
          }
          decoded_out.mem_index_size = (extend_mode & 0b001) ? sizeof(uint64_t) : sizeof(uint32_t);
          decoded_out.mem_index_sign_extend = (extend_mode & 0b100) != 0;
          decoded_out.mem_scale = (instruction & (UINT32_C(1) << 12)) ? sizeof(uint32_t) : 1;
        }
      } break;
      default:
        return false;
    }
  }

  return true;
}

bool MMIOHandler::ExceptionCallbackThunk(arch::Exception* ex, void* data) {
  return reinterpret_cast<MMIOHandler*>(data)->ExceptionCallback(ex);
}

bool MMIOHandler::ExceptionCallback(arch::Exception* ex) {
  if (ex->code() != arch::Exception::Code::kAccessViolation) {
    return false;
  }
  arch::Exception::AccessViolationOperation operation = ex->access_violation_operation();
  if (operation != arch::Exception::AccessViolationOperation::kRead &&
      operation != arch::Exception::AccessViolationOperation::kWrite) {
    // Data Execution Prevention or something else uninteresting.
    return false;
  }
  bool is_write = operation == arch::Exception::AccessViolationOperation::kWrite;
  if (ex->fault_address() < uint64_t(virtual_membase_) ||
      ex->fault_address() > uint64_t(memory_end_)) {
    // Quick kill anything outside our mapping.
    return false;
  }
  void* fault_host_address = reinterpret_cast<void*>(ex->fault_address());

  // Access violations are pretty rare, so we can do a linear search here.
  // Only check if in the virtual range, as we only support virtual ranges.
  const MMIORange* range = nullptr;
  uint32_t fault_guest_virtual_address = 0;
  if (ex->fault_address() < uint64_t(physical_membase_)) {
    fault_guest_virtual_address =
        host_to_guest_virtual_(host_to_guest_virtual_context_, fault_host_address);
    for (const auto& test_range : mapped_ranges_) {
      if ((fault_guest_virtual_address & test_range.mask) == test_range.address) {
        // Address is within the range of this mapping.
        range = &test_range;
        break;
      }
    }
  }
  if (!range) {
    // Recheck if the pages are still protected (race condition - another thread
    // clears the watch we just hit).
    // Do this under the lock so we don't introduce another race condition.
    auto lock = global_critical_region_.Acquire();
    memory::PageAccess cur_access;
    size_t page_length = memory::page_size();
    memory::QueryProtect(fault_host_address, page_length, cur_access);
    if (cur_access != memory::PageAccess::kNoAccess &&
        (!is_write || cur_access != memory::PageAccess::kReadOnly)) {
      // Another thread has cleared this watch. Abort.
      return true;
    }
    // The address is not found within any range, so either a write watch or an
    // actual access violation.
    if (access_violation_callback_) {
      return access_violation_callback_(std::move(lock), access_violation_callback_context_,
                                        fault_host_address, is_write);
    }
    return false;
  }

  auto rip = ex->pc();
  auto p = reinterpret_cast<const uint8_t*>(rip);
  DecodedLoadStore decoded_load_store;
  if (!TryDecodeLoadStore(p, decoded_load_store)) {
    REXLOG_ERROR("Unable to decode MMIO load or store instruction at {:p}",
                 static_cast<const void*>(p));
    assert_always("Unknown MMIO instruction type");
    return false;
  }

  arch::HostThreadContext& thread_context = *ex->thread_context();

#if REX_ARCH_ARM64
  // Preserve the base address with the pre- or the post-index offset to write
  // it after writing the result (since the base address register and the
  // register to load to may be the same, in which case it should receive the
  // original base address with the offset).
  uintptr_t mem_base_writeback_address = 0;
  if (decoded_load_store.mem_has_base && decoded_load_store.mem_base_writeback) {
    if (decoded_load_store.mem_base_reg == DecodedLoadStore::kArm64MemBaseRegSp) {
      mem_base_writeback_address = thread_context.sp;
    } else {
      assert_true(decoded_load_store.mem_base_reg <= 30);
      mem_base_writeback_address = thread_context.x[decoded_load_store.mem_base_reg];
    }
    mem_base_writeback_address += decoded_load_store.mem_base_writeback_offset;
  }
#endif  // REX_ARCH_ARM64

  uint8_t value_reg = decoded_load_store.value_reg;
  if (decoded_load_store.is_load) {
    // Load of a memory value - read from range, swap, and store in the
    // register.
    uint32_t value = range->read(nullptr, range->callback_context, fault_guest_virtual_address);
    if (!decoded_load_store.byte_swap) {
      // We swap only if it's not a movbe, as otherwise we are swapping twice.
      value = rex::byte_swap(value);
    }
    if (value_reg >= DecodedLoadStore::kArm64ValueRegX0 &&
        value_reg <= (DecodedLoadStore::kArm64ValueRegX0 + 30)) {
      ex->ModifyXRegister(value_reg - DecodedLoadStore::kArm64ValueRegX0) = value;
    } else if (value_reg >= DecodedLoadStore::kArm64ValueRegV0 &&
               value_reg <= (DecodedLoadStore::kArm64ValueRegV0 + 31)) {
      ex->ModifyVRegister(value_reg - DecodedLoadStore::kArm64ValueRegV0).u32[0] = value;
    } else {
      assert_true(value_reg == DecodedLoadStore::kArm64ValueRegZero);
      // Register write is ignored for X31.
    }
  } else {
    // Store of a register value - read register, swap, write to range.
    uint32_t value;
    if (decoded_load_store.is_constant) {
      value = uint32_t(decoded_load_store.constant);
    } else {
      if (value_reg >= DecodedLoadStore::kArm64ValueRegX0 &&
          value_reg <= (DecodedLoadStore::kArm64ValueRegX0 + 30)) {
        value = uint32_t(thread_context.x[value_reg - DecodedLoadStore::kArm64ValueRegX0]);
      } else if (value_reg >= DecodedLoadStore::kArm64ValueRegV0 &&
                 value_reg <= (DecodedLoadStore::kArm64ValueRegV0 + 31)) {
        value = thread_context.v[value_reg - DecodedLoadStore::kArm64ValueRegV0].u32[0];
      } else {
        assert_true(value_reg == DecodedLoadStore::kArm64ValueRegZero);
        value = 0;
      }
      if (!decoded_load_store.byte_swap) {
        // We swap only if it's not a movbe, as otherwise we are swapping twice.
        value = rex::byte_swap(value);
      }
    }
    range->write(nullptr, range->callback_context, fault_guest_virtual_address, value);
  }

  // Write the base address with the pre- or the post-index offset, overwriting
  // the register to load to if it's the same.
  if (decoded_load_store.mem_has_base && decoded_load_store.mem_base_writeback) {
    if (decoded_load_store.mem_base_reg == DecodedLoadStore::kArm64MemBaseRegSp) {
      thread_context.sp = mem_base_writeback_address;
    } else {
      assert_true(decoded_load_store.mem_base_reg <= 30);
      ex->ModifyXRegister(decoded_load_store.mem_base_reg) = mem_base_writeback_address;
    }
  }

  // Advance RIP to the next instruction so that we resume properly.
  ex->set_resume_pc(rip + decoded_load_store.length);

  return true;
}

}  // namespace rex::runtime
