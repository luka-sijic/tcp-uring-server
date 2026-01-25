#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

// SIMD backends
#if defined(__AVX2__)
#include <immintrin.h>
#define SBBF_HAS_AVX2 1
#elif defined(__SSE2__)
#include <emmintrin.h>
#define SBBF_HAS_SSE2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#define SBBF_HAS_NEON 1
#endif

/*
How it works?
1. We pick a block using a hash function
2. We pick bits inside the block

Split-Block

[..........................ONE 256-BIT BLOCK]...........................]
|
| Lane 0 | Lane 1 | Lane 2 | Lane 3 | Lane 4 | Lane 5 | Lane 6 | Lane 7 |
| [32bit] | [32bit] | [32bit] | [32bit] | [32bit] | [32bit] | [32bit] | [32bit]
|

[ BLOCK X ]
 Lane 0: [00000001000000000000000000000000] <- Hash0 picks bit 7
 Lane 1: [00000000000000000010000000000000]  <-- Hash1 picks bit 18
 Lane 2: [00000000000001000000000000000000]  <-- Hash2 picks bit 13
 Lane 3: [10000000000000000000000000000000]  <-- Hash3 picks bit 0
 Lane 4: [00000000000000000000000000000100]  <-- Hash4 picks bit 29
 Lane 5: [00000000001000000000000000000000]  <-- Hash5 picks bit 10
 Lane 6: [00000000000000000000010000000000]  <-- Hash6 picks bit 21
 Lane 7: [00000100000000000000000000000000]  <-- Hash7 picks bit 5

Filter hashes "test" and "test2" into the same block
filter - 1 0 1 0 1 0 1 0 1
test -   1 0 0 0 0 0 1 0 1
AND      1 0 0 0 0 0 1 0 1

OR
AND
XOR
*/

class SBBF {
  static constexpr size_t LANES = 8;
  static constexpr size_t BITS_PER_BLOCK = LANES * 32; // 256
  // 4x uint64_t = 256 bits; AVX2-friendly on x86, still fine on ARM.
  struct alignas(32) Block {
    uint32_t w[LANES];
  };
  static constexpr uint32_t SALT[LANES] = {
      0x47b6137bu, 0x44974d91u, 0x8824ad5bu, 0xa2b7289du,
      0x705495c7u, 0x2df1424bu, 0x9efc4947u, 0x5c6bfb31u};

public:
  explicit SBBF(size_t n, double p) {
    if (n == 0)
      n = 1;
    if (!(p > 0.0 && p < 1.0))
      p = 0.01;

    const double ln2 = std::log(2.0);

    const double m_bits_f =
        (-static_cast<double>(n) * std::log(p)) / (ln2 * ln2);
    size_t m_bits = static_cast<size_t>(ceil(m_bits_f));
    if (m_bits == 0)
      m_bits = 64;

    size_t blocks = (m_bits + (BITS_PER_BLOCK - 1)) / BITS_PER_BLOCK;
    if (blocks == 0)
      blocks = 1;
    m_ = round_up_pow2(blocks);

    // Allocate filter
    filter_.assign(m_, Block{});
  }

  static inline bool simd_block_contains(const Block &a,
                                         const Block &b) noexcept {
#if defined(__AVX2__)
    __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a.w));
    __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(b.w));
    __m256i vd = _mm256_xor_si256(_mm256_and_si256(va, vb), vb);
    return _mm256_testz_si256(vd, vd) != 0;

#elif defined(__SSE2__)
    const __m128i zero = _mm_setzero_si128();

    // 8x u32 = 2 halves of 128 bits each: [0..3], [4..7]
    __m128i a0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&a.w[0]));
    __m128i b0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&b.w[0]));
    __m128i d0 = _mm_xor_si128(_mm_and_si128(a0, b0), b0);

    __m128i a1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&a.w[4]));
    __m128i b1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&b.w[4]));
    __m128i d1 = _mm_xor_si128(_mm_and_si128(a1, b1), b1);

    __m128i d = _mm_or_si128(d0, d1);
    __m128i eq = _mm_cmpeq_epi8(d, zero);
    return _mm_movemask_epi8(eq) == 0xFFFF;

#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
    // 8x u32 -> can treat as 4x u64 loads, or 2x u128 loads; simplest: 2x u128
    uint32x4_t a0 = vld1q_u32(&a.w[0]);
    uint32x4_t b0 = vld1q_u32(&b.w[0]);
    uint32x4_t d0 = veorq_u32(vandq_u32(a0, b0), b0);

    uint32x4_t a1 = vld1q_u32(&a.w[4]);
    uint32x4_t b1 = vld1q_u32(&b.w[4]);
    uint32x4_t d1 = veorq_u32(vandq_u32(a1, b1), b1);

    uint32x4_t o = vorrq_u32(d0, d1);

    uint32_t x = vgetq_lane_u32(o, 0) | vgetq_lane_u32(o, 1) |
                 vgetq_lane_u32(o, 2) | vgetq_lane_u32(o, 3);
    return x == 0;

#else
    for (size_t i = 0; i < LANES; ++i) {
      if ((a.w[i] & b.w[i]) != b.w[i])
        return false;
    }
    return true;
#endif
  }

  static inline void simd_block_or(Block &a, const Block &b) noexcept {
#if defined(__AVX2__)
    __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a.w));
    __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(b.w));
    va = _mm256_or_si256(va, vb);
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(a.w), va);

#elif defined(__SSE2__)
    __m128i a0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&a.w[0]));
    __m128i b0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&b.w[0]));
    __m128i a1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&a.w[4]));
    __m128i b1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&b.w[4]));

    a0 = _mm_or_si128(a0, b0);
    a1 = _mm_or_si128(a1, b1);

    _mm_storeu_si128(reinterpret_cast<__m128i *>(&a.w[0]), a0);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(&a.w[4]), a1);

#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
    uint32x4_t a0 = vld1q_u32(&a.w[0]);
    uint32x4_t b0 = vld1q_u32(&b.w[0]);
    uint32x4_t a1 = vld1q_u32(&a.w[4]);
    uint32x4_t b1 = vld1q_u32(&b.w[4]);

    a0 = vorrq_u32(a0, b0);
    a1 = vorrq_u32(a1, b1);

    vst1q_u32(&a.w[0], a0);
    vst1q_u32(&a.w[4], a1);

#else
    for (size_t i = 0; i < LANES; ++i)
      a.w[i] |= b.w[i];
#endif
  }

  static uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4eb59ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  static inline uint64_t hash64(std::string_view s) noexcept {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
      h ^= static_cast<uint64_t>(c);
      h *= 1099511628211ULL;
    }
    return h;
  }

  bool insert(const std::string_view &key) noexcept {
    uint64_t h = hash64(key);
    uint64_t h1 = mix64(h);
    uint64_t h2 = mix64(h1) | 1ULL;

    const size_t idx = static_cast<size_t>(h1) & (m_ - 1);

    Block mask = make_mask_block(h1, h2);
    simd_block_or(filter_[idx], mask);
    return true;
  }

  bool possiblyContains(const std::string_view &key) const noexcept {
    uint64_t h = hash64(key);
    uint64_t h1 = mix64(h);
    uint64_t h2 = mix64(h1) | 1ULL;

    const size_t idx = static_cast<size_t>(h1) & (m_ - 1);

    Block mask = make_mask_block(h1, h2);
    return simd_block_contains(filter_[idx], mask);
  }

private:
  Block make_mask_block(uint64_t h1, uint64_t h2) const noexcept {
    uint32_t x = static_cast<uint32_t>(h2);

    Block m{};
    for (size_t i = 0; i < LANES; ++i) {
      uint32_t bit = (x * SALT[i]) >> 27;
      m.w[i] = (1u << bit);
    }
    return m;
  }

  static size_t round_up_pow2(size_t x) noexcept {
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    if constexpr (sizeof(size_t) == 8)
      x |= x >> 32;
    return x + 1;
  }
  size_t m_;
  std::vector<Block> filter_;
};

/*
int main() {
  auto sbf = SBBF(10000, .001);
  sbf.insert("hello");
  sbf.insert("username");
  sbf.insert("application");
  std::cout << sbf.possiblyContains("hello") << std::endl;
  std::cout << sbf.possiblyContains("app") << std::endl;

  double golden_ratio = (1.0 + std::sqrt(5.0)) / 2.0;
  // long double ans = (1 << 64) / golden_ratio;
  return 0;
}
*/
