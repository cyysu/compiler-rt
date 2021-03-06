// Test the behavior of malloc/calloc/realloc/new when the allocation size is
// more than ASan allocator's max allowed one.
// By default (allocator_may_return_null=0) the process should crash.
// With allocator_may_return_null=1 the allocator should return 0, except the
// operator new(), which should crash anyway (operator new(std::nothrow) should
// return nullptr, indeed).
//
// RUN: %clangxx_asan -O0 %s -o %t
// RUN: not %run %t malloc 2>&1 | FileCheck %s --check-prefix=CHECK-mCRASH
// RUN: %env_asan_opts=allocator_may_return_null=0 not %run %t malloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-mCRASH
// RUN: %env_asan_opts=allocator_may_return_null=1     %run %t malloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-mNULL
// RUN: %env_asan_opts=allocator_may_return_null=0 not %run %t calloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-cCRASH
// RUN: %env_asan_opts=allocator_may_return_null=1     %run %t calloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-cNULL
// RUN: %env_asan_opts=allocator_may_return_null=0 not %run %t calloc-overflow 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-coCRASH
// RUN: %env_asan_opts=allocator_may_return_null=1     %run %t calloc-overflow 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-coNULL
// RUN: %env_asan_opts=allocator_may_return_null=0 not %run %t realloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-rCRASH
// RUN: %env_asan_opts=allocator_may_return_null=1     %run %t realloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-rNULL
// RUN: %env_asan_opts=allocator_may_return_null=0 not %run %t realloc-after-malloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-mrCRASH
// RUN: %env_asan_opts=allocator_may_return_null=1     %run %t realloc-after-malloc 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-mrNULL
// RUN: %env_asan_opts=allocator_may_return_null=0 not %run %t new 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-nCRASH
// RUN: %env_asan_opts=allocator_may_return_null=1 not %run %t new 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-nCRASH-OOM
// RUN: %env_asan_opts=allocator_may_return_null=0 not %run %t new-nothrow 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-nnCRASH
// RUN: %env_asan_opts=allocator_may_return_null=1     %run %t new-nothrow 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-nnNULL

// UNSUPPORTED: win32

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits>
#include <new>

int main(int argc, char **argv) {
  // Disable stderr buffering. Needed on Windows.
  setvbuf(stderr, NULL, _IONBF, 0);

  assert(argc == 2);
  const char *action = argv[1];
  fprintf(stderr, "%s:\n", action);

  static const size_t kMaxAllowedMallocSizePlusOne =
#if __LP64__ || defined(_WIN64)
      (1ULL << 40) + 1;
#else
      (3UL << 30) + 1;
#endif

  void *x = 0;
  if (!strcmp(action, "malloc")) {
    x = malloc(kMaxAllowedMallocSizePlusOne);
  } else if (!strcmp(action, "calloc")) {
    x = calloc((kMaxAllowedMallocSizePlusOne / 4) + 1, 4);
  } else if (!strcmp(action, "calloc-overflow")) {
    volatile size_t kMaxSizeT = std::numeric_limits<size_t>::max();
    size_t kArraySize = 4096;
    volatile size_t kArraySize2 = kMaxSizeT / kArraySize + 10;
    x = calloc(kArraySize, kArraySize2);
  } else if (!strcmp(action, "realloc")) {
    x = realloc(0, kMaxAllowedMallocSizePlusOne);
  } else if (!strcmp(action, "realloc-after-malloc")) {
    char *t = (char*)malloc(100);
    *t = 42;
    x = realloc(t, kMaxAllowedMallocSizePlusOne);
    assert(*t == 42);
    free(t);
  } else if (!strcmp(action, "new")) {
    x = operator new(kMaxAllowedMallocSizePlusOne);
  } else if (!strcmp(action, "new-nothrow")) {
    x = operator new(kMaxAllowedMallocSizePlusOne, std::nothrow);
  } else {
    assert(0);
  }

  fprintf(stderr, "errno: %d\n", errno);

  // The NULL pointer is printed differently on different systems, while (long)0
  // is always the same.
  fprintf(stderr, "x: %lx\n", (long)x);
  free(x);

  return x != 0;
}

// CHECK-mCRASH: malloc:
// CHECK-mCRASH: SUMMARY: AddressSanitizer: allocation-size-too-big
// CHECK-cCRASH: calloc:
// CHECK-cCRASH: SUMMARY: AddressSanitizer: allocation-size-too-big
// CHECK-coCRASH: calloc-overflow:
// CHECK-coCRASH: SUMMARY: AddressSanitizer: calloc-overflow
// CHECK-rCRASH: realloc:
// CHECK-rCRASH: SUMMARY: AddressSanitizer: allocation-size-too-big
// CHECK-mrCRASH: realloc-after-malloc:
// CHECK-mrCRASH: SUMMARY: AddressSanitizer: allocation-size-too-big
// CHECK-nCRASH: new:
// CHECK-nCRASH: SUMMARY: AddressSanitizer: allocation-size-too-big
// CHECK-nCRASH-OOM: new:
// CHECK-nCRASH-OOM: SUMMARY: AddressSanitizer: out-of-memory
// CHECK-nnCRASH: new-nothrow:
// CHECK-nnCRASH: SUMMARY: AddressSanitizer: allocation-size-too-big

// CHECK-mNULL: malloc:
// CHECK-mNULL: errno: 12
// CHECK-mNULL: x: 0
// CHECK-cNULL: calloc:
// CHECK-cNULL: errno: 12
// CHECK-cNULL: x: 0
// CHECK-coNULL: calloc-overflow:
// CHECK-coNULL: errno: 12
// CHECK-coNULL: x: 0
// CHECK-rNULL: realloc:
// CHECK-rNULL: errno: 12
// CHECK-rNULL: x: 0
// CHECK-mrNULL: realloc-after-malloc:
// CHECK-mrNULL: errno: 12
// CHECK-mrNULL: x: 0
// CHECK-nnNULL: new-nothrow:
// CHECK-nnNULL: x: 0
