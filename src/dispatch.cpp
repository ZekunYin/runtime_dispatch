#define HWY_AVX2 2
#define HWY_SSE4 4
#define HWY_AVX512 16

#include <xmmintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif

#include <iostream>
#include <stdint.h>
#include <atomic>

#include "compiler_specific.h"
#include "dispatch.h"

bool IsBitSet(const uint32_t reg, const int index) {
  return (reg & (1U << index)) != 0;
}


// Calls CPUID instruction with eax=level and ecx=count and returns the result
// in abcd array where abcd = {eax, ebx, ecx, edx} (hence the name abcd).
void Cpuid(const uint32_t level, const uint32_t count,
           uint32_t* HWY_RESTRICT abcd) {
#ifdef _MSC_VER
  int regs[4];
  __cpuidex(regs, level, count);
  for (int i = 0; i < 4; ++i) {
    abcd[i] = regs[i];
  }
#else
  uint32_t a, b, c, d;
  __cpuid_count(level, count, a, b, c, d);
  abcd[0] = a;
  abcd[1] = b;
  abcd[2] = c;
  abcd[3] = d;
#endif
}

// Returns the lower 32 bits of extended control register 0.
// Requires CPU support for "OSXSAVE" (see below).
uint32_t ReadXCR0() {
#ifdef _MSC_VER
  return static_cast<uint32_t>(_xgetbv(0));
#else
  uint32_t xcr0, xcr0_high;
  const uint32_t index = 0;
  asm volatile(".byte 0x0F, 0x01, 0xD0"
               : "=a"(xcr0), "=d"(xcr0_high)
               : "c"(index));
  return xcr0;
#endif
}

// Not function-local => no compiler-generated locking.
std::atomic<int> supported_{-1};  // Not yet initialized

// Bits indicating which instruction set extensions are supported.
constexpr uint32_t kSSE = 1 << 0;
constexpr uint32_t kSSE2 = 1 << 1;
constexpr uint32_t kSSE3 = 1 << 2;
constexpr uint32_t kSSSE3 = 1 << 3;
constexpr uint32_t kSSE41 = 1 << 4;
constexpr uint32_t kSSE42 = 1 << 5;
constexpr uint32_t kGroupSSE4 = kSSE | kSSE2 | kSSE3 | kSSSE3 | kSSE41 | kSSE42;

constexpr uint32_t kAVX = 1u << 6;
constexpr uint32_t kAVX2 = 1u << 7;
constexpr uint32_t kFMA = 1u << 8;
constexpr uint32_t kLZCNT = 1u << 9;
constexpr uint32_t kBMI = 1u << 10;
constexpr uint32_t kBMI2 = 1u << 11;

// We normally assume BMI/BMI2 are available if AVX2 is. This allows us to
// use BZHI and (compiler-generated) MULX. However, VirtualBox lacks BMI/BMI2
// [https://www.virtualbox.org/ticket/15471]. Thus we provide the option of
// avoiding using and requiring BMI so AVX2 can still be used.
#ifdef HWY_DISABLE_BMI2
constexpr uint32_t kGroupAVX2 = kAVX | kAVX2 | kFMA | kLZCNT;
#else
constexpr uint32_t kGroupAVX2 = kAVX | kAVX2 | kFMA | kLZCNT | kBMI | kBMI2;
#endif

constexpr uint32_t kAVX512F = 1u << 12;
constexpr uint32_t kAVX512VL = 1u << 13;
constexpr uint32_t kAVX512DQ = 1u << 14;
constexpr uint32_t kAVX512BW = 1u << 15;
constexpr uint32_t kGroupAVX512 = kAVX512F | kAVX512VL | kAVX512DQ | kAVX512BW;

int GetBitField()
{
  int bits_ = supported_.load(std::memory_order_acquire);
  if (bits_ != -1) {
    return bits_;
  }

  bits_ = 0;

  uint32_t flags = 0;
  uint32_t abcd[4];

  Cpuid(0, 0, abcd);
  const uint32_t max_level = abcd[0];

  // Standard feature flags
  Cpuid(1, 0, abcd);
  flags |= IsBitSet(abcd[3], 25) ? kSSE : 0;
  flags |= IsBitSet(abcd[3], 26) ? kSSE2 : 0;
  flags |= IsBitSet(abcd[2], 0) ? kSSE3 : 0;
  flags |= IsBitSet(abcd[2], 9) ? kSSSE3 : 0;
  flags |= IsBitSet(abcd[2], 19) ? kSSE41 : 0;
  flags |= IsBitSet(abcd[2], 20) ? kSSE42 : 0;
  flags |= IsBitSet(abcd[2], 12) ? kFMA : 0;
  flags |= IsBitSet(abcd[2], 28) ? kAVX : 0;
  const bool has_osxsave = IsBitSet(abcd[2], 27);

  // Extended feature flags
  Cpuid(0x80000001U, 0, abcd);
  flags |= IsBitSet(abcd[2], 5) ? kLZCNT : 0;

  // Extended features
  if (max_level >= 7) {
    Cpuid(7, 0, abcd);
    flags |= IsBitSet(abcd[1], 3) ? kBMI : 0;
    flags |= IsBitSet(abcd[1], 5) ? kAVX2 : 0;
    flags |= IsBitSet(abcd[1], 8) ? kBMI2 : 0;

    flags |= IsBitSet(abcd[1], 16) ? kAVX512F : 0;
    flags |= IsBitSet(abcd[1], 17) ? kAVX512DQ : 0;
    flags |= IsBitSet(abcd[1], 30) ? kAVX512BW : 0;
    flags |= IsBitSet(abcd[1], 31) ? kAVX512VL : 0;
  }

  // Verify OS support for XSAVE, without which XMM/YMM registers are not
  // preserved across context switches and are not safe to use.
  if (has_osxsave) {
    const uint32_t xcr0 = ReadXCR0();
    // XMM
    if (!IsBitSet(xcr0, 1)) {
      flags = 0;
    }
    // YMM
    if (!IsBitSet(xcr0, 2)) {
      flags &= ~kGroupAVX2;
    }
    // ZMM + opmask
    if ((xcr0 & 0x70) != 0x70) {
      flags &= ~kGroupAVX512;
    }
  }

  // Set target bit(s) if all their group's flags are all set.
  if ((flags & kGroupAVX512) == kGroupAVX512) {
    bits_ |= HWY_AVX512;
  }
  if ((flags & kGroupAVX2) == kGroupAVX2) {
    bits_ |= HWY_AVX2;
  }
  if ((flags & kGroupSSE4) == kGroupSSE4) {
    bits_ |= HWY_SSE4;
  }

  supported_.store(bits_, std::memory_order_release);

  return bits_;
}


