#ifndef _DS_HTTPS_REQUEST_H_
#define _DS_HTTPS_REQUEST_H_

#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_http_client.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "cJSON.h"

typedef enum{
    HTTPS_GET_WEATHER,
    HTTPS_GET_SPORT,
}HTTPS_REQUEST_TYPE_E;

typedef struct 
{
    char city[20];
    char weather_text[20];
    char weather_code[2];
    char temperature[3];
    char humidity[3];
    char sport[10];
}weather_info;

void cjson_weather_info(char *text);
esp_err_t _https_weather_event_handler(esp_http_client_event_t *evt);
static void https_weather_get(void);

void cjson_sport_info(char *text);
esp_err_t _https_sport_event_handler(esp_http_client_event_t *evt);
static void https_sport_get(void);

static void https_request_task(void *pvParameters);
void ds_https_request_type(HTTPS_REQUEST_TYPE_E type);
void ds_https_request_all();
void ds_https_request_init(void);

#endif






