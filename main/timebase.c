#include "timebase.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_err.h"
#include "esp_timer.h"
#include <stdbool.h>

static const char *TAG  = "timebase";
static int64_t s_anchor_us        = 0;
static int64_t s_anchor_epoch_ms  = 0;
static bool    s_timebase_valid   = false;

static int64_t wall_clock_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000; // convert s and us to ms
}

bool timebase_is_valid(void)
{
    return s_timebase_valid;
}

void timebase_reanchor(void)
{
    s_anchor_us = esp_timer_get_time();
    s_anchor_epoch_ms = wall_clock_ms();
    ESP_LOGI(TAG, "timebase anchored: %lld us, %lld ms", s_anchor_us, s_anchor_epoch_ms);
}

esp_err_t timebase_sync(uint32_t timeout_ms) {


    if (s_timebase_valid){return ESP_OK;} // sntp_init ONLY RUNS ONCE
    // init tiembase 
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t res = esp_netif_sntp_init(&config);
    if (res != ESP_OK) 
    {   
        ESP_LOGE(TAG, "sntp_init failed: %s", esp_err_to_name(res));
        return res;
    }


    // Wait for init 
    res = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms));
    if (res != ESP_OK) 
    {
        ESP_LOGE(TAG, "sntp_sync_wait failed: %s", esp_err_to_name(res));
        esp_netif_sntp_deinit();        /* release, so a retry can init again */
        return res;
    }

    // timebase is inited 
    timebase_reanchor();
    s_timebase_valid = true;
    return ESP_OK;

}

int64_t timebase_now_ms(void)
{   
    return s_anchor_epoch_ms + (esp_timer_get_time() - s_anchor_us) / 1000;
}