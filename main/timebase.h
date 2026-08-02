#ifndef TIMEBASE_H
#define TIMEBASE_H
#include <stdbool.h>
#include "esp_err.h"

/* one-shot bring-up must be called after ip allocated
* Starts SNTP, blocks until the system clock is set, then takes the anchor.
* Returns ESP_OK on success; on failure the clock is NOT valid.
*/
esp_err_t timebase_sync(uint32_t timeout_ms);
bool timebase_is_valid(void);
void timebase_reanchor(void);
int64_t timebase_now_ms(void);

/* Convert an arbitrary esp_timer_get_time() microsecond value (e.g. one
 * captured earlier by a sampler) into epoch milliseconds, using the same
 * anchor as timebase_now_ms(). Caller should check timebase_is_valid()
 * first; if the timebase was never synced, the result is meaningless. */
int64_t timebase_us_to_epoch_ms(int64_t t_us);

#endif /*TIMEBASE_H*/