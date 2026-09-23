#include <rex/platform.h>
#include <rex/thread/atomic.h>

static_assert(REX_PLATFORM_LINUX, "This file is POSIX-only");

namespace rex::thread {

int32_t atomic_inc(volatile int32_t* value) {
  return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST);
}
int32_t atomic_dec(volatile int32_t* value) {
  return __atomic_sub_fetch(value, 1, __ATOMIC_SEQ_CST);
}

int32_t atomic_exchange(int32_t new_value, volatile int32_t* value) {
  return __atomic_exchange_n(value, new_value, __ATOMIC_SEQ_CST);
}
int64_t atomic_exchange(int64_t new_value, volatile int64_t* value) {
  return __atomic_exchange_n(value, new_value, __ATOMIC_SEQ_CST);
}

int32_t atomic_exchange_add(int32_t amount, volatile int32_t* value) {
  return __atomic_fetch_add(value, amount, __ATOMIC_SEQ_CST);
}
int64_t atomic_exchange_add(int64_t amount, volatile int64_t* value) {
  return __atomic_fetch_add(value, amount, __ATOMIC_SEQ_CST);
}

bool atomic_cas(int32_t old_value, int32_t new_value, volatile int32_t* value) {
  return __atomic_compare_exchange_n(value, &old_value, new_value, false, __ATOMIC_SEQ_CST,
                                      __ATOMIC_SEQ_CST);
}
bool atomic_cas(int64_t old_value, int64_t new_value, volatile int64_t* value) {
  return __atomic_compare_exchange_n(value, &old_value, new_value, false, __ATOMIC_SEQ_CST,
                                      __ATOMIC_SEQ_CST);
}

void atomic_store_release(int32_t new_value, volatile int32_t* value) {
  __atomic_store_n(value, new_value, __ATOMIC_RELEASE);
}

}  // namespace rex::thread
