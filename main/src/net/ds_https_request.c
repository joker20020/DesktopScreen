/* HTTPS GET Example using plain mbedTLS sockets
 *
 * Contacts the howsmyssl.com API via TLS v1.2 and reads a JSON
 * response.
 *
 * Adapted from the ssl_client1 example in mbedtls.
 *
 * Original Copyright (C) 2006-2016, ARM Limited, All Rights Reserved, Apache 2.0 License.
 * Additions Copyright (C) Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD, Apache 2.0 License.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "cJSON.h"

#include "esp_tls.h"


#define WEB_API_SERVER "api.seniverse.com"
#define WEB_API_URL "https://api.seniverse.com/v3/weather/now.json?key=SmazqPcltzTft-X3v&location=guangzhou&language=zh-Hans&unit=c"

#define WEB_BILIBILI_SERVER "api.bilibili.com"
#define WEB_BILIBILI_URL "https://api.bilibili.com/x/relation/stat?vmid=383943678&jsonp=jsonp"

static const char *TAG = "example";

static const char *REQUEST = "GET " WEB_API_URL " HTTP/1.0\r\n"
    "Host: "WEB_API_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

static const char *REQUEST_BILIBILI = "GET " WEB_BILIBILI_URL " HTTP/1.0\r\n"
    "Host: "WEB_BILIBILI_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

/* Root cert for howsmyssl.com, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

extern const uint8_t server_root_cert_bilibili_pem_start[] asm("_binary_server_root_cert_bilibili_pem_start");
extern const uint8_t server_root_cert_bilibili_pem_end[]   asm("_binary_server_root_cert_bilibili_pem_end");

static void https_get_task(void *pvParameters)
{
    char buf[512];
    int ret, len;
    int flags = 1;
    struct esp_tls *tls;
    esp_tls_cfg_t api_cfg = {
        .cacert_buf  = server_root_cert_pem_start,
        .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };
    esp_tls_cfg_t bilibili_cfg = {
        .cacert_buf  = server_root_cert_bilibili_pem_start,
        .cacert_bytes = server_root_cert_bilibili_pem_end - server_root_cert_bilibili_pem_start,
    };

    tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        goto exit;
    }

    while(1) {
        if(flags == 0){
            // tls = esp_tls_conn_http_new(WEB_URL, &cfg);
            if (esp_tls_conn_http_new_sync(WEB_API_URL, &api_cfg, tls) == 1) {
                ESP_LOGI(TAG, "Connection established...");
            } 
            else {
                ESP_LOGE(TAG, "Connection failed...");
                int esp_tls_code = 0, esp_tls_flags = 0;
                esp_tls_error_handle_t tls_e = NULL;
                esp_tls_get_error_handle(tls, &tls_e);
                /* Try to get TLS stack level error and certificate failure flags, if any */
                ret = esp_tls_get_and_clear_last_error(tls_e, &esp_tls_code, &esp_tls_flags);
                if (ret == ESP_OK) {
                    ESP_LOGE(TAG, "TLS error = -0x%x, TLS flags = -0x%x", esp_tls_code, esp_tls_flags);
                }
                goto cleanup;
            }
        }
        else{
            // tls = esp_tls_conn_http_new(WEB_BILIBILI_URL, &bilibili_cfg);
            if (esp_tls_conn_http_new_sync(WEB_BILIBILI_URL, &bilibili_cfg, tls) == 1) {
                ESP_LOGI(TAG, "Connection established...");
            } 
            else {
                ESP_LOGE(TAG, "Connection failed...");
                int esp_tls_code = 0, esp_tls_flags = 0;
                esp_tls_error_handle_t tls_e = NULL;
                esp_tls_get_error_handle(tls, &tls_e);
                /* Try to get TLS stack level error and certificate failure flags, if any */
                ret = esp_tls_get_and_clear_last_error(tls_e, &esp_tls_code, &esp_tls_flags);
                if (ret == ESP_OK) {
                    ESP_LOGE(TAG, "TLS error = -0x%x, TLS flags = -0x%x", esp_tls_code, esp_tls_flags);
                }
                goto cleanup;
            }
        }

        
        size_t written_bytes = 0;

        if(flags == 0){
            flags = 1;
            do {
            ret = esp_tls_conn_write(tls, 
                                     REQUEST + written_bytes, 
                                     strlen(REQUEST) - written_bytes);
            if (ret >= 0) {
                ESP_LOGI(TAG, "%d bytes written", ret);
                written_bytes += ret;
            } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
                ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
                goto cleanup;
            }
            } while(written_bytes < strlen(REQUEST));
        }else{
            flags = 0;
           do {
            ret = esp_tls_conn_write(tls, 
                                     REQUEST_BILIBILI + written_bytes, 
                                     strlen(REQUEST_BILIBILI) - written_bytes);
            if (ret >= 0) {
                ESP_LOGI(TAG, "%d bytes written", ret);
                written_bytes += ret;
            } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
                ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
                goto cleanup;
            }
            } while(written_bytes < strlen(REQUEST_BILIBILI));   
        }


        ESP_LOGI(TAG, "Reading HTTP response...");

        do
        {
            len = sizeof(buf) - 1;
            memset(buf, 0x00, sizeof(buf));
            ret = esp_tls_conn_read(tls, (char *)buf, len);
            
            if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
                continue;
            } else if (ret < 0) {
                ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
                break;
            } else if (ret == 0) {
                ESP_LOGI(TAG, "connection closed");
                break;
            }

            len = ret;
            ESP_LOGD(TAG, "%d bytes read", len);
            /* Print response directly to stdout as it is read */
            for(int i = 0; i < len; i++) {
                putchar(buf[i]);
            }
            putchar('\n'); // JSON output doesn't have a newline at end
        } while(1);
    }

cleanup:
    esp_tls_conn_destroy(tls);
exit:
    for (int countdown = 10; countdown >= 0; countdown--) {
        ESP_LOGI(TAG, "%d...", countdown);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    
}


void ds_https_request_init(void)
{
    xTaskCreate(&https_get_task, "https_get_task", 8192, NULL, 5, NULL);
}
