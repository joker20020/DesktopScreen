/* LwIP SNTP example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"

#include "ds_sntp.h"
#include "ds_system_data.h"

static const char *TAG = "ds_sntp";


void get_system_time()
{
    static struct tm timeinfo = {0}; // 时间寄存器
    time_t now = 0;

    time(&now);
    localtime_r(&now, &timeinfo);

    /* 打印获取到的时间 */
    char str[64];
    strftime(str, sizeof(str), "%c", &timeinfo);
    ESP_LOGI(TAG, "time updated: %s", str);

    ESP_LOGI(TAG, "%d%d:%d%d", timeinfo.tm_hour / 10, timeinfo.tm_hour % 10, timeinfo.tm_min / 10, timeinfo.tm_min % 10);
    ESP_LOGI(TAG, "%d-%d-%d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

    update_system_time(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
    /* 设置时区 */
    setenv("TZ", "CST-8", 1);
    tzset();
    get_system_time();
}

void ds_init_sntp(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGI(TAG, "Initializing and starting SNTP");
    #if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
        /* This demonstrates configuring more than one server
         */
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                                   ESP_SNTP_SERVER_LIST(CONFIG_SNTP_TIME_SERVER, "pool.ntp.org" ) );
    #else
        /*
         * This is the basic default config with one server and starting the service
         */
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
    #endif
        config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want
    #ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
        config.smooth_sync = true;
    #endif
    
        esp_netif_sntp_init(&config);
        int retry = 0;
        const int retry_count = 15;
        while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
            ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        }
    
        esp_netif_sntp_deinit();
}

