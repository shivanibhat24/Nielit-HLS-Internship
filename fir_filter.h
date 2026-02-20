// ================================================================
//  fir_filter.h
//  11-Tap Hamming-Windowed FIR Low-Pass Filter
//  Target Tool : Vitis HLS 2024.2
//  Target Part : xc7z020clg484-1  (Zynq-7020)
//  Clock       : 100 MHz  (10 ns period)
// ================================================================

#ifndef FIR_FILTER_H
#define FIR_FILTER_H

// ap_fixed.h is the ONLY HLS header needed for this design.
// Do NOT include hls_stream.h — it is unused and can confuse
// the 2024.2 code analyser.
#include <ap_fixed.h>

// ── Filter size ──────────────────────────────────────────────────
//  Keep as a plain enum (not #define, not static const) so it is
//  visible as a compile-time constant in both .cpp files without
//  any linkage issues.
enum { N_TAPS = 11 };

// ── Fixed-point type definitions ─────────────────────────────────
//
//  ap_fixed<W, I, Q, O>
//    W : total word width in bits
//    I : number of integer bits  (includes the sign bit)
//    Q : quantisation rounding   AP_RND  = round to nearest
//    O : overflow handling       AP_SAT  = saturate (clamp)
//
//  data_t  – Q1.15  16-bit signed  range [-1, +1)
//  coef_t  – Q1.17  18-bit signed  range [-1, +1)  (extra precision)
//  acc_t   – Q4.32  36-bit signed  range [-8, +8)
//            4 integer bits are enough guard bits for the sum of
//            11 products, each bounded by |coef| * |data| < 1.

typedef ap_fixed<16, 1, AP_RND, AP_SAT>  data_t;
typedef ap_fixed<18, 1, AP_RND, AP_SAT>  coef_t;
typedef ap_fixed<36, 4, AP_RND, AP_SAT>  acc_t;

// ── Top-level function prototype ─────────────────────────────────
void fir_filter(data_t x_in, data_t &y_out);

#endif  // FIR_FILTER_H
