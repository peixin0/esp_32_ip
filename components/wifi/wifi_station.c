#include <string.h>
#include "wifi_station.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"


static const int WIFI_CONNECTED_BIT = BIT0;
static const char *TAG = "WIFI_STATION";
static EventGroupHandle_t wifi_event_group;
static uint8_t retry_count = 0;


// private function
static void wifi_event_handler(void *arg, esp_event_base_t base,int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START )
    {
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {   
        /* Phone hotspots drop frequently; a monitoring node should keep
         * trying indefinitely rather than give up after N attempts.
         * esp_wifi_connect() itself takes time to scan/auth, so this does
         * not spin the CPU. See the note on backoff in the .h walkthrough. */
        xEventGroupClearBits(wifi_event_group,WIFI_CONNECTED_BIT);
        retry_count ++;
        ESP_LOGW(TAG, "disconnected; reconnect attempt %d", retry_count);   
        esp_wifi_connect();
    }
    else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    /* Association *and* DHCP are done
     TCP sockets (your MQTT client) can actually open*/
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}



esp_err_t wifi_station_init(void)
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Create the event group to signal when we are connected
    wifi_event_group = xEventGroupCreate();

    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handler for WiFi events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));


        wifi_config_t wifi_config = {

        .sta = {
            .ssid = CONFIG_WIFI_STA_SSID,
            .password = CONFIG_WIFI_STA_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            /* Allows association to WPA3-only and WPA2/WPA3 transitional
             * hotspots; 'required = false' keeps plain WPA2 working too. */
            .pmf_cfg = { .capable = true, .required = false },
        },
    };

    // Set WiFi mode to station and start
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    


    /* Disable modem power-save: a small, constant current cost in exchange for
     * lower and steadier RX latency, so WiFi adds less jitter to the rest of
     * the system. Drop this line if you later care about battery life. */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "STA started, connecting to \"%s\"", CONFIG_WIFI_STA_SSID);
    return ESP_OK;
}


bool wifi_station_wait_connected(uint32_t timeout_ms)
{
    EventBits_t bit = xEventGroupWaitBits(wifi_event_group,WIFI_CONNECTED_BIT,pdFALSE,pdFALSE,pdMS_TO_TICKS(timeout_ms));
    return (bit & WIFI_CONNECTED_BIT) != 0;
    // if bit = 1, which means wifi connected 1 == 0, Flase;
    // if bit = 0, wifi is not connected 0 == 0, true. 
}

bool wifi_station_is_connected(void)
{
    return (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) != 0;

    // connected 1 & 1 = 1, true
    // not connected 0 & 1 = 0, false
}