#pragma once

#if defined(__aarch64__) || defined(_M_ARM64)
    #include <arm_neon.h>
    #define EQ1_DOUBLE2_NEON 1
#elif defined(__x86_64__) || defined(_M_X64)
    #include <emmintrin.h>
    #define EQ1_DOUBLE2_SSE2 1
#endif

namespace eq1
{

// Two doubles side by side, one per lane, for a Stereo Band's Left and Right through one loop: a NEON
// float64x2_t on arm64, an SSE2 __m128d on x64 (every x64 CPU has SSE2: no AVX, FMA intrinsics, -march or
// runtime dispatch, ADR 0008), and ScalarDouble2, two plain doubles, where neither is available.
//
// The arithmetic on a vector Double2 is the compiler's own operators (*, + and -), not intrinsics, so
// that a kernel written once as a template over its sample type (biquadStep in Biquad.h) is the same
// expression for a double and for a Double2, and the compiler treats both alike. That is what makes
// each lane round exactly as the scalar path does: clang's default (-ffp-contract=on) fuses a * b + c
// within one expression into a fused multiply-add on arm64, decided per expression in its front end,
// for a double and for a vector the same way (llvm.fmuladd on both); x64 has no fused multiply-add
// without AVX2, so neither path fuses there, and MSVC's /fp:precise fuses nothing. Arithmetic behind a
// function call (an intrinsic, an overloaded operator on a wrapper struct) would not be fused, and
// would round differently from the scalar path on arm64. The fallback has no operators for that
// reason: biquadStep runs the scalar kernel on each of its lanes. So an arm64 build and an x64 build may
// differ in the last bits, which ADR 0008 allows: its exact rule holds between builds on one platform.
//
// make2 (lane0, lane1) builds one, fill2 (value) puts the value in both lanes, lane0 and lane1 read them.

struct ScalarDouble2
{
    double lane0, lane1;
};

template <typename Lanes>
Lanes make2 (double lane0, double lane1);
template <typename Lanes>
Lanes fill2 (double value);

template <>
inline ScalarDouble2 make2<ScalarDouble2> (double lane0, double lane1) { return { lane0, lane1 }; }
template <>
inline ScalarDouble2 fill2<ScalarDouble2> (double value) { return { value, value }; }
inline double lane0 (ScalarDouble2 v) { return v.lane0; }
inline double lane1 (ScalarDouble2 v) { return v.lane1; }

#if EQ1_DOUBLE2_NEON

using Double2 = float64x2_t;

template <>
inline Double2 make2<Double2> (double lane0, double lane1) { return vcombine_f64 (vdup_n_f64 (lane0), vdup_n_f64 (lane1)); }
template <>
inline Double2 fill2<Double2> (double value) { return vdupq_n_f64 (value); }
inline double lane0 (Double2 v) { return vgetq_lane_f64 (v, 0); }
inline double lane1 (Double2 v) { return vgetq_lane_f64 (v, 1); }

#elif EQ1_DOUBLE2_SSE2

using Double2 = __m128d;

template <>
inline Double2 make2<Double2> (double lane0, double lane1) { return _mm_set_pd (lane1, lane0); }
template <>
inline Double2 fill2<Double2> (double value) { return _mm_set1_pd (value); }
inline double lane0 (Double2 v) { return _mm_cvtsd_f64 (v); }
inline double lane1 (Double2 v) { return _mm_cvtsd_f64 (_mm_unpackhi_pd (v, v)); }

    #if defined(_MSC_VER) && ! defined(__clang__)
// MSVC's __m128d is a union without operators; clang's and GCC's is a vector type with them built in.
inline __m128d operator* (__m128d a, __m128d b) { return _mm_mul_pd (a, b); }
inline __m128d operator+ (__m128d a, __m128d b) { return _mm_add_pd (a, b); }
inline __m128d operator- (__m128d a, __m128d b) { return _mm_sub_pd (a, b); }
    #endif

#else

using Double2 = ScalarDouble2;

#endif

} // namespace eq1
