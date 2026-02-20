// ================================================================
//  fir_filter.cpp
//  11-Tap Hamming-Windowed FIR Low-Pass Filter
//  Target Tool : Vitis HLS 2024.2
//
//  KEY FIX IN THIS VERSION:
//  The COEFFS array and its ARRAY_PARTITION pragma have been moved
//  INSIDE the function body.  In Vitis HLS 2024.2, ALL #pragma HLS
//  directives must appear inside function scope — placing them at
//  file scope causes:
//    "pragma can only be declared within the function scope"
//
//  Filter Design
//  -------------
//  Type   : Linear Phase FIR Type-I (symmetric, odd tap count)
//  Window : Hamming
//  Taps   : 11
//  Cutoff : Fc = 0.1 x Fs  (normalised digital frequency)
//
//  Coefficient table  [symmetric: h[k] == h[10-k]]
//  h[0]  = h[10] = -0.013707
//  h[1]  = h[9]  = -0.012433
//  h[2]  = h[8]  =  0.036916
//  h[3]  = h[7]  =  0.116248
//  h[4]  = h[6]  =  0.192526
//  h[5]           =  0.224898  (centre tap, peak)
//  Sum             =  0.880737  (DC gain)
// ================================================================

#include "fir_filter.h"

// ================================================================
//  fir_filter  -  Top-Level HLS Function
//
//  x_in  : one Q1.15 input sample per call
//  y_out : one Q1.15 filtered output sample per call
//
//  Pipeline : II=1  (one new sample accepted every clock cycle)
//  Resources: 11 DSP48E1, ~200 FFs, ~100 LUTs  (Zynq-7020 est.)
// ================================================================
void fir_filter(data_t x_in, data_t &y_out)
{
    // ── (1) Top-function declaration ─────────────────────────────
    //  MUST be the very first pragma inside the function.
    //  Vitis HLS 2024.2 requires this even when set_top and
    //  open_solution -top are present in the TCL script.
    //  name= must EXACTLY match the C function name above.
#pragma HLS TOP name=fir_filter

    // ── (2) Block-level interface ─────────────────────────────────
    //  ap_ctrl_none removes ap_start/ap_done/ap_idle/ap_ready.
    //  The core acts as a pure datapath with no handshake.
#pragma HLS INTERFACE ap_ctrl_none port=return

    // ── (3) Data port interfaces ──────────────────────────────────
    //  ap_none = bare wire connections (no AXI, no valid/ready).
#pragma HLS INTERFACE ap_none port=x_in
#pragma HLS INTERFACE ap_none port=y_out

    // ── (4) Coefficient ROM ───────────────────────────────────────
    //  Declared INSIDE the function so the ARRAY_PARTITION pragma
    //  below is legal (all #pragma HLS must be in function scope).
    //
    //  'static const' tells HLS this is a read-only ROM.
    //  No RAM ports are generated; values are baked into the logic.
    //  coef_t() constructor forces Q1.17 rounding of double literals.
    static const coef_t COEFFS[N_TAPS] = {
        coef_t(-0.013707),   // h[0]
        coef_t(-0.012433),   // h[1]
        coef_t( 0.036916),   // h[2]
        coef_t( 0.116248),   // h[3]
        coef_t( 0.192526),   // h[4]
        coef_t( 0.224898),   // h[5]  centre tap
        coef_t( 0.192526),   // h[6]
        coef_t( 0.116248),   // h[7]
        coef_t( 0.036916),   // h[8]
        coef_t(-0.012433),   // h[9]
        coef_t(-0.013707)    // h[10]
    };
    //  Expose every coefficient as an independent signal so the MAC
    //  loop can read all 11 in the same clock cycle after unrolling.
#pragma HLS ARRAY_PARTITION variable=COEFFS complete dim=1

    // ── (5) Shift register (delay line) ──────────────────────────
    //  'static' retains value between calls (= D flip-flop chain).
    //  Initialised to all-zeros at device power-up.
    static data_t sr[N_TAPS];
    //  Each element in its own register for parallel access.
#pragma HLS ARRAY_PARTITION variable=sr complete dim=1

    // ── (6) Pipeline directive ────────────────────────────────────
    //  Place AFTER all partition pragmas.
    //  II=1 means one new input sample is accepted every clock cycle.
#pragma HLS PIPELINE II=1

    // ── Step A : Shift the delay line ────────────────────────────
    //  Move every stored sample one position to the right.
    //  UNROLL makes all 10 copies happen simultaneously.
    SHIFT_LOOP:
    for (int i = N_TAPS - 1; i > 0; --i) {
#pragma HLS UNROLL
        sr[i] = sr[i - 1];
    }
    sr[0] = x_in;   // insert newest sample at position 0

    // ── Step B : Multiply-Accumulate (MAC) ───────────────────────
    //  y[n] = sum_{k=0}^{N-1}  h[k] * x[n-k]
    //
    //  acc_t is Q4.32 (36-bit) — wide enough to hold the sum of
    //  11 products without overflow (max sum ~2.47, fits in 4 bits).
    acc_t acc = acc_t(0);

    MAC_LOOP:
    for (int k = 0; k < N_TAPS; ++k) {
#pragma HLS UNROLL
        acc += acc_t(COEFFS[k] * sr[k]);
    }

    // ── Step C : Write output ─────────────────────────────────────
    //  Cast back to Q1.15.  AP_SAT saturates, AP_RND rounds.
    y_out = data_t(acc);
}
