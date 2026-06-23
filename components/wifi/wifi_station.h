#ifndef WIFI_STATION_H
#define WIFI_STATION_H


#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Bring up the TCP/IP stack, start WiFi in station mode and begin associating
 * with the AP configured in menuconfig. Returns once the driver has started;
 * association completes asynchronously in the background. */
esp_err_t wifi_station_init(void);
/* Block up to timeout_ms waiting for an IP address. Returns true if connected,
 * false on timeout. Other work (sensor sampling) need not call this at all. */
bool wifi_station_wait_connected(uint32_t timeout_ms);
/* Non-blocking link-state query for the network/publish task to poll. */
bool wifi_station_is_connected(void);


#ifdef __cplusplus
}
#endif
#endif //WIFI_STATION_H
