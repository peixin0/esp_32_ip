/**
 * @file    ecg_filter.h
 * @brief   Real-time ECG pre-processing filter chain for the AD8232 front end.
 *
 * Three cascaded second-order IIR (biquad) sections clean the raw ADC stream:
 *
 *     raw ADC --> [ high-pass 0.5 Hz ] --> [ notch 50 Hz ] --> [ low-pass 40 Hz ] --> filtered
 *                  removes baseline        removes mains        removes high-freq
 *                  wander (respiration,    interference          EMG / muscle noise
 *                  electrode drift)        (UK grid = 50 Hz)
 *
 * The 0.5-40 Hz overall pass-band matches the diagnostic ECG bandwidth defined
 * by the AAMI/IEC standards, so the chain preserves clinically relevant content
 * (P, QRS, T) while rejecting the dominant artefact sources seen on this rig.
 *
 * IMPORTANT: every coefficient is derived from the sampling rate fs. The system
 * supports two rates (360 Hz and 180 Hz), so the coefficients are computed at
 * run time in ecg_filter_init() rather than hard-coded. If the sampling rate is
 * ever changed, ecg_filter_init() MUST be called again with the new rate,
 * otherwise the notch and cut-off frequencies land in the wrong place.
 */
#ifndef ECG_FILTER_H_
#define ECG_FILTER_H_

#include <stdint.h>

/**
 * @brief Design all three biquad sections for the given sampling rate.
 *        Call once before sampling starts, and again whenever fs changes.
 * @param fs  Current ECG sampling rate in Hz (e.g. 360.0f or 180.0f).
 */
void ecg_filter_init(float fs);

/**
 * @brief Push one raw ADC sample through the full filter chain.
 * @param raw  Raw 12-bit ADC count from the AD8232 (0..4095).
 * @return     Filtered sample. Note: after the high-pass stage removes the DC
 *             bias (~2048 counts), the output is centred near zero and swings
 *             both positive and negative. Add an offset before display if a
 *             positive-only trace is required.
 */
float ecg_filter_apply(int32_t raw);

/**
 * @brief Clear the internal filter state (delay elements) of all stages.
 *        Call when restarting acquisition (e.g. after lead-off, power cycle,
 *        or a sampling-rate switch) so stale history does not corrupt the
 *        first samples of the new session.
 */
void ecg_filter_reset(void);

#endif /* ECG_FILTER_H_ */