#ifndef TIMEBASE_H
#define TIMEBASE_H
#include <stdbool.h>
#include "esp_err.h"

/*one-shot birng-up must be called after ip allocated
* Starts SNTP, blocks until the system clock is set, then takes the anchor.
* Returns ESP_OK on success; on failure the clock is NOT valid.
*/

esp_err_t timebase_sync(uint32_t timeout_ms);
bool timebase_is_valid(void);
void timebase_reanchor(void);
int64_t timebase_now_ms(void);



#endif /*TIMEBASE_H*/