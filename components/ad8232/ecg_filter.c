/**
 * @file    ecg_filter.c
 * @brief   Implementation of the three-stage ECG pre-processing filter chain.
 *
 * Each stage is a second-order IIR section (biquad) implemented in the
 * Direct Form II Transposed structure. DF2T is chosen because it needs only
 * two state variables per section and has good numerical behaviour with
 * single-precision float, which suits a real-time sample-by-sample path on
 * the ESP32 (the FPU handles one float multiply per cycle).
 *
 * Cost per sample: 3 sections x 5 multiplies = 15 float multiplies. This is
 * negligible against the 360 Hz sample period (~2.78 ms), so the filter adds
 * no meaningful latency to the acquisition task.
 *
 * Design method: the coefficients follow the standard RBJ "Audio EQ Cookbook"
 * bilinear-transform formulas. Butterworth response (Q = 1/sqrt(2)) is used for
 * the high-pass and low-pass so the pass-band is maximally flat.
 */
#include "ecg_filter.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief One second-order IIR section (biquad).
 *
 * Transfer function (a0 normalised to 1):
 *     H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
 *
 * z1, z2 are the two delay-line state variables carried between samples.
 */
typedef struct {
    float b0, b1, b2;   /* feed-forward (numerator) coefficients   */
    float a1, a2;       /* feed-back   (denominator) coefficients  */
    float z1, z2;       /* delay-element state (per-section memory) */
} biquad_t;

static biquad_t s_hp;      /* stage 1: high-pass  0.5 Hz - baseline wander */
static biquad_t s_notch;   /* stage 2: notch      50  Hz - mains rejection */
static biquad_t s_lp;      /* stage 3: low-pass   40  Hz - EMG / HF noise  */

/**
 * @brief Process a single sample through one biquad (Direct Form II Transposed).
 *
 * The two state updates below implement the DF2T signal flow: the output is
 * formed first, then the delay elements are updated using both the current
 * input and the just-computed output.
 */
static float biquad_process(biquad_t *bq, float x)
{
    float y = bq->b0 * x + bq->z1;                 /* current output           */
    bq->z1  = bq->b1 * x - bq->a1 * y + bq->z2;    /* update first delay unit  */
    bq->z2  = bq->b2 * x - bq->a2 * y;             /* update second delay unit */
    return y;
}

/**
 * @brief Design a second-order Butterworth HIGH-PASS section.
 *
 * Removes baseline wander (slow drift from respiration and electrode
 * polarisation). Cut-off fc is set to 0.5 Hz, the standard diagnostic ECG
 * lower edge - low enough to preserve the ST segment and T wave, high enough
 * to reject breathing drift (~0.2-0.3 Hz).
 *
 * @param bq  Section to populate.
 * @param fs  Sampling rate (Hz).
 * @param fc  -3 dB cut-off frequency (Hz).
 */
static void design_highpass(biquad_t *bq, float fs, float fc)
{
    float w0    = 2.0f * (float)M_PI * fc / fs;    /* normalised angular freq */
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float q     = 0.7071f;                          /* Butterworth: Q = 1/sqrt(2) */
    float alpha = sinw0 / (2.0f * q);

    float a0 =  1.0f + alpha;                       /* normalisation term */
    bq->b0 =  (1.0f + cosw0) / 2.0f / a0;
    bq->b1 = -(1.0f + cosw0)        / a0;
    bq->b2 =  (1.0f + cosw0) / 2.0f / a0;
    bq->a1 = -2.0f * cosw0          / a0;
    bq->a2 =  (1.0f - alpha)        / a0;
    bq->z1 = bq->z2 = 0.0f;                          /* clear state */
}

/**
 * @brief Design a second-order NOTCH (band-stop) section.
 *
 * Places a deep null at f0 (50 Hz UK mains) while leaving neighbouring
 * frequencies almost untouched. The width of the null is set by Q: a higher Q
 * gives a narrower notch that removes less of the surrounding ECG energy, but
 * is less forgiving if the mains frequency drifts (real grids wander a few
 * tenths of a Hz). Q = 30 is a narrow starting point; retune if residual
 * 50 Hz remains.
 *
 * @param bq  Section to populate.
 * @param fs  Sampling rate (Hz).
 * @param f0  Centre (notch) frequency (Hz).
 * @param q   Quality factor (higher = narrower notch).
 */
static void design_notch(biquad_t *bq, float fs, float f0, float q)
{
    float w0    = 2.0f * (float)M_PI * f0 / fs;
    float cosw0 = cosf(w0);
    float alpha = sinf(w0) / (2.0f * q);

    float a0 =  1.0f + alpha;
    bq->b0 =  1.0f          / a0;
    bq->b1 = -2.0f * cosw0  / a0;
    bq->b2 =  1.0f          / a0;
    bq->a1 = -2.0f * cosw0  / a0;
    bq->a2 = (1.0f - alpha) / a0;
    bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Design a second-order Butterworth LOW-PASS section.
 *
 * Removes high-frequency muscle (EMG) noise and any residual switching noise
 * above the ECG band. Cut-off fc is set to 40 Hz, the standard diagnostic ECG
 * upper edge - the QRS complex energy sits below this, so the waveform shape
 * is preserved while the jitter above 40 Hz is attenuated.
 *
 * @param bq  Section to populate.
 * @param fs  Sampling rate (Hz).
 * @param fc  -3 dB cut-off frequency (Hz).
 */
static void design_lowpass(biquad_t *bq, float fs, float fc)
{
    float w0    = 2.0f * (float)M_PI * fc / fs;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float q     = 0.7071f;                          /* Butterworth */
    float alpha = sinw0 / (2.0f * q);

    float a0 =  1.0f + alpha;
    bq->b0 =  (1.0f - cosw0) / 2.0f / a0;
    bq->b1 =  (1.0f - cosw0)        / a0;
    bq->b2 =  (1.0f - cosw0) / 2.0f / a0;
    bq->a1 = -2.0f * cosw0          / a0;
    bq->a2 =  (1.0f - alpha)        / a0;
    bq->z1 = bq->z2 = 0.0f;
}

/**
 * @brief Design all three sections for the given sampling rate.
 *
 * Coefficients depend on fs, so this MUST be re-called after any sampling-rate
 * change. Cut-off / notch frequencies are the diagnostic ECG standard values.
 */
void ecg_filter_init(float fs)
{
    design_highpass(&s_hp,    fs, 0.5f);            /* 0.5 Hz baseline removal */
    design_notch   (&s_notch, fs, 50.0f, 30.0f);   /* 50 Hz mains, Q = 30     */
    design_lowpass (&s_lp,    fs, 40.0f);           /* 40 Hz EMG removal       */
}

/**
 * @brief Zero the delay elements of every stage.
 *
 * IIR sections carry history in z1/z2. On a fresh acquisition that history is
 * stale and would inject a startup transient, so clear it before restarting.
 * Expect a short settling period (tens of samples) after a reset before the
 * output is fully valid.
 */
void ecg_filter_reset(void)
{
    s_hp.z1    = s_hp.z2    = 0.0f;
    s_notch.z1 = s_notch.z2 = 0.0f;
    s_lp.z1    = s_lp.z2    = 0.0f;
}

/**
 * @brief Run one raw ADC sample through the full cascade.
 *
 * Order matters: remove the large DC bias and slow drift FIRST (high-pass),
 * so the later stages operate on a signal already centred near zero. Then
 * reject mains, then trim high-frequency noise.
 */
float ecg_filter_apply(int32_t raw)
{
    float x = (float)raw;
    x = biquad_process(&s_hp,    x);   /* 1. strip baseline wander + DC offset */
    x = biquad_process(&s_notch, x);   /* 2. reject 50 Hz mains interference   */
    x = biquad_process(&s_lp,    x);   /* 3. attenuate high-frequency EMG noise */
    return x;
}