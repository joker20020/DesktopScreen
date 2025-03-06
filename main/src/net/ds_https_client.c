#include "ds_https_client.h"
#include "ds_ui_weatherpage.h"
#include "ds_nvs.h"
#include "ds_system_data.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP_CLIENT";


#define WEB_API_WEATHER_SERVER "api.seniverse.com"
#define WEB_API_WEATHER_URL "https://api.seniverse.com/v3/weather/now.json?key=SmazqPcltzTft-X3v&location=guangzhou&language=zh-Hans&unit=c"


/* Root cert, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const char server_root_cert_api_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const char server_root_cert_api_pem_end[]   asm("_binary_server_root_cert_pem_end");

extern const char server_root_cert_bilibili_pem_start[] asm("_binary_server_root_cert_bilibili_pem_start");
extern const char server_root_cert_bilibili_pem_end[]   asm("_binary_server_root_cert_bilibili_pem_end");


QueueHandle_t https_request_event_queue;
weather_info weather;


// {
//     "results": [
//       {
//         "location": {
//           "id": "C23NB62W20TF",
//           "name": "西雅图",
//           "country": "US",
//           "path": "西雅图,华盛顿州,美国",
//           "timezone": "America/Los_Angeles",
//           "timezone_offset": "-07:00"
//         },
//         "now": {
//           "text": "多云", //天气现象文字
//           "code": "4", //天气现象代码
//           "temperature": "14", //温度，单位为c摄氏度或f华氏度
//           "feels_like": "14", //体感温度，单位为c摄氏度或f华氏度，暂不支持国外城市。
//           "pressure": "1018", //气压，单位为mb百帕或in英寸
//           "humidity": "76", //相对湿度，0~100，单位为百分比
//           "visibility": "16.09", //能见度，单位为km公里或mi英里
//           "wind_direction": "西北", //风向文字
//           "wind_direction_degree": "340", //风向角度，范围0~360，0为正北，90为正东，180为正南，270为正西
//           "wind_speed": "8.05", //风速，单位为km/h公里每小时或mph英里每小时
//           "wind_scale": "2", //风力等级，请参考：http://baike.baidu.com/view/465076.htm
//           "clouds": "90", //云量，单位%，范围0~100，天空被云覆盖的百分比 #目前不支持中国城市#
//           "dew_point": "-12" //露点温度，请参考：http://baike.baidu.com/view/118348.htm #目前数据缺失中#
//         },
//         "last_update": "2015-09-25T22:45:00-07:00" //数据更新时间（该城市的本地时间）
//       }
//     ]
//   }

void cjson_weather_info(char *text)
{
    cJSON *root,*psub;
    cJSON *arrayItem;
    //截取有效json
    char *index=strchr(text,'{');
    strcpy(text,index);
    root = cJSON_Parse(text);
    if(root!=NULL)
    {
        psub = cJSON_GetObjectItem(root, "results");
        arrayItem = cJSON_GetArrayItem(psub,0);

        cJSON *locat = cJSON_GetObjectItem(arrayItem, "location");
        cJSON *now = cJSON_GetObjectItem(arrayItem, "now");
        if((locat!=NULL)&&(now!=NULL))
        {
            psub=cJSON_GetObjectItem(locat,"name");
            sprintf(weather.city,"%s",psub->valuestring);
            ds_ui_weather_set(VALUE_CITY,psub->valuestring);
            ESP_LOGI(TAG,"city:%s",weather.city);

            psub=cJSON_GetObjectItem(now,"text");
            sprintf(weather.weather_text,"%s",psub->valuestring);
            ds_ui_weather_set(VALUE_WEATHER,psub->valuestring);
            ESP_LOGI(TAG,"weather:%s",weather.weather_text);
            
            psub=cJSON_GetObjectItem(now,"code");
            sprintf(weather.weather_code,"%s",psub->valuestring);
            ds_ui_weather_set(VALUE_CODE,psub->valuestring);
            ESP_LOGI(TAG,"%s",weather.weather_code);

            psub=cJSON_GetObjectItem(now,"temperature");
            sprintf(weather.temperature,"%s",psub->valuestring);
            ds_ui_weather_set(VALUE_TEMP,psub->valuestring);
            set_system_temperature(atoi(weather.temperature));
            ESP_LOGI(TAG,"temperatur:%s",weather.temperature);
        }
    }
    cJSON_Delete(root);
}

esp_err_t _https_weather_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                        printf("%.*s\n", output_len, (char*)evt->user_data);
                        cjson_weather_info((char*)evt->user_data);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                        printf("%.*s\n", output_len, output_buffer);
                        cjson_weather_info((char*)output_buffer);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            break;
    }
    return ESP_OK;
}

static void https_weather_get(void)
{
    esp_http_client_config_t config = {
        .url = WEB_API_WEATHER_SERVER,
        .method = HTTP_METHOD_GET,
        .event_handler = _https_weather_event_handler,
        .cert_pem = server_root_cert_api_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform https request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}




static void https_request_task(void *pvParameters)
{
    while(1) {
        HTTPS_REQUEST_TYPE_E evt;
        xQueueReceive(https_request_event_queue, &evt, portMAX_DELAY);
        ESP_LOGI(TAG, "https_get_task %d",evt);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        switch (evt)
        {
        case HTTPS_GET_WEATHER:
            https_weather_get();
            break;
        case HTTPS_GET_TEMPRATURE:
            // http_city_get();
            break;
        default:
            break;
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void ds_https_request_type(HTTPS_REQUEST_TYPE_E type){
    HTTPS_REQUEST_TYPE_E evt;
	evt = type;
	xQueueSend(https_request_event_queue, &evt, 0);
}

void ds_https_request_all(){
    ds_https_request_type(HTTPS_GET_WEATHER);
}

void ds_https_request_init(void)
{
	https_request_event_queue = xQueueCreate(10, sizeof(HTTPS_REQUEST_TYPE_E));
    xTaskCreate(&https_request_task, "https_request_task", 4096, NULL, 5, NULL);
}