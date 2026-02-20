// ================================================================
//  fir_filter_tb.cpp
//  C-Simulation Testbench for fir_filter
//  Target Tool : Vitis HLS 2024.2
//
//  Three independent tests:
//    Test 1 – Impulse response   → output must equal h[0..10]
//    Test 2 – Step response      → must converge to 0.5 * sum(h)
//    Test 3 – Frequency response → passband pass, stopband attenuation
//
//  Return value:
//    0           = all tests passed  (Vitis HLS marks C-Sim as PASS)
//    non-zero    = number of failures (Vitis HLS marks C-Sim as FAIL)
//
//  NOTE: The HLS function fir_filter() uses a 'static' shift
//  register internally.  Between tests we call flush_hls() which
//  feeds N_TAPS zero samples to drain and reset the filter state.
//  Both the HLS DUT and the floating-point reference are always
//  flushed together to stay synchronised.
// ================================================================

#include <cstdio>
#include <cstring>
#include <cmath>
#include "fir_filter.h"

// ── Compile-time constant ─────────────────────────────────────────
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Test parameters ───────────────────────────────────────────────
static const double TOLERANCE   = 0.002;   // max |HLS - REF| allowed
static const int    WARMUP_LEN  = N_TAPS;  // cycles to flush transient
static const int    MEASURE_LEN = 64;      // cycles for power measurement

// ── Floating-point reference coefficients ─────────────────────────
// Must match the fixed-point values in fir_filter.cpp exactly.
static const double REF_H[N_TAPS] = {
    -0.013707,
    -0.012433,
     0.036916,
     0.116248,
     0.192526,
     0.224898,
     0.192526,
     0.116248,
     0.036916,
    -0.012433,
    -0.013707
};

// ── Reference filter state ────────────────────────────────────────
// Separate from the HLS DUT.  Reset with ref_reset().
static double ref_sr[N_TAPS];

static void ref_reset(void)
{
    memset(ref_sr, 0, sizeof(ref_sr));
}

static double ref_filter(double x)
{
    int i, k;
    double y = 0.0;
    for (i = N_TAPS - 1; i > 0; --i)
        ref_sr[i] = ref_sr[i - 1];
    ref_sr[0] = x;
    for (k = 0; k < N_TAPS; ++k)
        y += REF_H[k] * ref_sr[k];
    return y;
}

// ── Flush both filters to a clean zero state ──────────────────────
// Feeds N_TAPS zero samples through both the HLS DUT and the
// reference model so they both start each test from the same
// all-zero delay-line state.
static void flush_both(void)
{
    int i;
    data_t zero_in  = data_t(0);
    data_t dummy;
    ref_reset();
    for (i = 0; i < N_TAPS; ++i)
        fir_filter(zero_in, dummy);
}

// ── Check one sample: print result, return 1 if fail ──────────────
static int check_sample(int n, double hls_val, double ref_val,
                         double tol, int always_print)
{
    double err = fabs(hls_val - ref_val);
    int    ok  = (err <= tol) ? 1 : 0;
    if (always_print || !ok)
        printf("  n=%-3d  HLS=%+9.6f  REF=%+9.6f  err=%9.6f  %s\n",
               n, hls_val, ref_val, err, ok ? "PASS" : "*** FAIL ***");
    return ok ? 0 : 1;
}

// =================================================================
//  TEST 1 — Impulse Response
// =================================================================
static int test_impulse(void)
{
    int n, fail = 0;
    int n_total = N_TAPS + 4;   // 11 taps + 4 trailing zeros

    printf("=================================================================\n");
    printf("  TEST 1 : Impulse Response\n");
    printf("  Feed x[0]=1, x[n]=0 for n>0.\n");
    printf("  Expected y[n] = h[n]  (the filter coefficient sequence).\n");
    printf("=================================================================\n");

    flush_both();

    for (n = 0; n < n_total; ++n)
    {
        double  x_d   = (n == 0) ? 1.0 : 0.0;
        data_t  x_fix = data_t(x_d);
        data_t  y_fix;

        fir_filter(x_fix, y_fix);

        double y_hls = (double)y_fix;
        double y_ref = ref_filter(x_d);

        fail += check_sample(n, y_hls, y_ref, TOLERANCE, 1);
    }

    printf("  Result : %s  (%d failure(s))\n\n",
           fail == 0 ? "PASSED" : "FAILED", fail);
    return fail;
}

// =================================================================
//  TEST 2 — Step Response
// =================================================================
static int test_step(void)
{
    int n, fail = 0;
    int n_total  = N_TAPS + 10;
    double step_val = 0.5;

    // Compute expected steady-state value: step * sum(h[k])
    double coeff_sum = 0.0;
    int k;
    for (k = 0; k < N_TAPS; ++k) coeff_sum += REF_H[k];
    double steady = step_val * coeff_sum;

    printf("=================================================================\n");
    printf("  TEST 2 : Step Response  (x = %.1f constant)\n", step_val);
    printf("  DC steady-state = %.1f * sum(h) = %.1f * %.6f = %.6f\n",
           step_val, step_val, coeff_sum, steady);
    printf("=================================================================\n");

    flush_both();

    for (n = 0; n < n_total; ++n)
    {
        data_t  x_fix = data_t(step_val);
        data_t  y_fix;

        fir_filter(x_fix, y_fix);

        double y_hls = (double)y_fix;
        double y_ref = ref_filter(step_val);

        // During transient compare to reference model;
        // once delay line is full compare to theoretical steady state.
        double ref_val  = (n >= N_TAPS - 1) ? steady : y_ref;
        double tol_this = (n >= N_TAPS - 1) ? TOLERANCE : TOLERANCE * 4.0;
        int    always   = (n >= N_TAPS - 2) ? 1 : 0;

        fail += check_sample(n, y_hls, ref_val, tol_this, always);
    }

    printf("  Result : %s  (%d failure(s))\n\n",
           fail == 0 ? "PASSED" : "FAILED", fail);
    return fail;
}

// =================================================================
//  TEST 3 — Frequency Response
//  Measures power gain for two test frequencies:
//    (a) Passband : f = 0.05 * Fs  →  gain must be > -3 dB
//    (b) Stopband : f = 0.40 * Fs  →  gain must be < -20 dB
// =================================================================
static double measure_gain_dB(double freq_norm)
{
    int n;
    double p_in  = 0.0;
    double p_out = 0.0;
    data_t x_fix, y_fix;

    // Warm up: flush transient (WARMUP_LEN cycles, output discarded)
    for (n = 0; n < WARMUP_LEN; ++n)
    {
        double xd = 0.5 * sin(2.0 * M_PI * freq_norm * (double)n);
        x_fix = data_t(xd);
        fir_filter(x_fix, y_fix);
    }

    // Measure power ratio over MEASURE_LEN cycles
    for (n = WARMUP_LEN; n < WARMUP_LEN + MEASURE_LEN; ++n)
    {
        double xd = 0.5 * sin(2.0 * M_PI * freq_norm * (double)n);
        x_fix = data_t(xd);
        fir_filter(x_fix, y_fix);
        p_in  += xd          * xd;
        p_out += (double)y_fix * (double)y_fix;
    }

    if (p_in < 1e-12) return -999.0;   // guard against divide-by-zero
    return 10.0 * log10(p_out / p_in);
}

static int test_frequency(void)
{
    int fail = 0;

    printf("=================================================================\n");
    printf("  TEST 3 : Frequency Response\n");
    printf("=================================================================\n");

    // Passband test: f = 0.05 * Fs
    flush_both();
    double gain_pass = measure_gain_dB(0.05);
    int ok_pass = (gain_pass >= -3.0) ? 1 : 0;
    if (!ok_pass) fail++;
    printf("  Passband  f=0.05*Fs : gain = %7.3f dB  [need >= -3.0 dB]  %s\n",
           gain_pass, ok_pass ? "PASS" : "*** FAIL ***");

    // Stopband test: f = 0.40 * Fs
    flush_both();
    double gain_stop = measure_gain_dB(0.40);
    int ok_stop = (gain_stop <= -20.0) ? 1 : 0;
    if (!ok_stop) fail++;
    printf("  Stopband  f=0.40*Fs : gain = %7.3f dB  [need <= -20.0 dB] %s\n",
           gain_stop, ok_stop ? "PASS" : "*** FAIL ***");

    printf("  Result : %s  (%d failure(s))\n\n",
           fail == 0 ? "PASSED" : "FAILED", fail);
    return fail;
}

// =================================================================
//  main
// =================================================================
int main(void)
{
    int total_fail = 0;

    printf("\n");
    printf("#################################################################\n");
    printf("#  fir_filter C-Simulation Testbench                           #\n");
    printf("#################################################################\n\n");

    total_fail += test_impulse();
    total_fail += test_step();
    total_fail += test_frequency();

    printf("#################################################################\n");
    if (total_fail == 0)
        printf("#  ALL TESTS PASSED                                             #\n");
    else
        printf("#  TOTAL FAILURES : %-3d                                         #\n",
               total_fail);
    printf("#################################################################\n\n");

    // Vitis HLS C-Sim: return 0 = PASS, non-zero = FAIL
    return total_fail;
}
