#include "ds_https_client.h"
#include "ds_ui_weatherpage.h"
#include "ds_nvs.h"
#include "ds_system_data.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTPS_CLIENT";


#define WEB_API_SERVER "api.seniverse.com"
#define WEB_API_WEATHER_URL "https://api.seniverse.com/v3/weather/now.json?key=Stng9w-RWR5e1x5xa&location=beijing&language=zh-Hans&unit=c"

#define WEB_API_AIR_URL "https://api.seniverse.com/v3/life/suggestion.json?key=Stng9w-RWR5e1x5xa&location=beijing&language=zh-Hans"


/* Root cert, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const char server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const char server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");



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
    char count_url[130];
    char *city = ds_nvs_read_city();
    //判断是否有存储城市信息
    if(city != NULL){
        char url[] = "https://api.seniverse.com/v3/weather/now.json?key=Stng9w-RWR5e1x5xa&location=";
        uint8_t city_len = strlen(city);
        char citydata[30];
        memcpy(citydata,city,city_len);
        citydata[city_len] = '\0';
        strcat(url,citydata);
        strcat(url,"&language=zh-Hans&unit=c"); 
        printf("%s\n",url);
        strcpy(count_url,url);
        free(city);
    }else{
        strcpy(count_url,WEB_API_WEATHER_URL);
    }
    esp_http_client_config_t config = {
        .url = count_url,
        .method = HTTP_METHOD_GET,
        .event_handler = _https_weather_event_handler,
        .cert_pem = server_root_cert_pem_start,
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


// {
//     "results": [
//       {
//         "location": {
//           "id": "WTW3SJ5ZBJUY",
//           "name": "上海",
//           "country": "CN",
//           "path": "上海,上海,中国",
//           "timezone": "Asia/Shanghai",
//           "timezone_offset": "+08:00"
//         },
//         "suggestion": {
//           "ac": {
//             //空调开启
//             "brief": "较少开启", //简要建议
//             "details": "您将感到很舒适，一般不需要开启空调。" //详细建议
//           },
//           "air_pollution": {
//             //空气污染扩散条件
//             "brief": "较差",
//             "details": "气象条件较不利于空气污染物稀释、扩散和清除，请适当减少室外活动时间。"
//           },
//           "airing": {
//             //晾晒
//             "brief": "不太适宜",
//             "details": "天气阴沉，不利于水分的迅速蒸发，不太适宜晾晒。若需要晾晒，请尽量选择通风的地点。"
//           },
//           "allergy": {
//             //过敏
//             "brief": "极不易发",
//             "details": "天气条件极不易诱发过敏，可放心外出，享受生活。"
//           },
//           "beer": {
//             //啤酒
//             "brief": "较不适宜",
//             "details": "您将会感到有些凉意，建议饮用常温啤酒，并少量饮用为好。"
//           },
//           "boating": {
//             //划船
//             "brief": "较适宜",
//             "details": "白天较适宜划船，但天气阴沉，气温稍低，请注意加衣，小心着凉。"
//           },
//           "car_washing": {
//             //洗车
//             "brief": "不宜",
//             "details": "不宜洗车，未来24小时内有雨，如果在此期间洗车，雨水和路上的泥水可能会再次弄脏您的爱车。"
//           },
//           "chill": {
//             //风寒
//             "brief": "凉",
//             "details": "感觉有点凉，室外活动注意适当增减衣物。"
//           },
//           "comfort": {
//             //舒适度
//             "brief": "较舒适",
//             "details": "白天天气阴沉，会感到有点儿凉，但大部分人完全可以接受。"
//           },
//           "dating": {
//             //约会
//             "brief": "较适宜",
//             "details": "虽然天空有些阴沉，但情侣们可以放心外出，不用担心天气来调皮捣乱而影响了兴致。"
//           },
//           "dressing": {
//             //穿衣
//             "brief": "较冷",
//             "details": "建议着厚外套加毛衣等服装。年老体弱者宜着大衣、呢外套加羊毛衫。"
//           },
//           "fishing": {
//             //钓鱼
//             "brief": "较适宜",
//             "details": "较适合垂钓，但天气稍凉，会对垂钓产生一定的影响。"
//           },
//           "flu": {
//             //感冒
//             "brief": "较易发",
//             "details": "天气较凉，较易发生感冒，请适当增加衣服。体质较弱的朋友尤其应该注意防护。"
//           },
//           "hair_dressing": {
//             //美发
//             "brief": "一般",
//             "details": "注意防晒，洗发不宜太勤，建议选用保湿防晒型洗发护发品。出门请戴上遮阳帽或打遮阳伞。"
//           },
//           "kiteflying": {
//             //放风筝
//             "brief": "不宜",
//             "details": "天气不好，不适宜放风筝。"
//           },
//           "makeup": {
//             //化妆
//             "brief": "保湿",
//             "details": "皮肤易缺水，用润唇膏后再抹口红，用保湿型霜类化妆品。"
//           },
//           "mood": {
//             //心情
//             "brief": "较差",
//             "details": "天气阴沉，会感觉莫名的压抑，情绪低落，此时将所有的悲喜都静静地沉到心底，在喧嚣的尘世里，感受片刻恬淡的宁静。"
//           },
//           "morning_sport": {
//             //晨练
//             "brief": "不宜",
//             "details": "阴天，早晨天气寒冷，请尽量避免户外晨练，若坚持室外晨练请注意保暖防冻，建议年老体弱人群适当减少晨练时间。"
//           },
//           "night_life": {
//             //夜生活
//             "brief": "较不适宜",
//             "details": "有降水，会给您的出行带来很大的不便，建议就近或最好在室内进行夜生活。"
//           },
//           "road_condition": {
//             //路况
//             "brief": "干燥",
//             "details": "阴天，路面比较干燥，路况较好。"
//           },
//           "shopping": {
//             //购物
//             "brief": "适宜",
//             "details": "阴天，在这种天气里去逛街，省去了涂防晒霜，打遮阳伞的麻烦，既可放松身心，又会有很多意外收获。"
//           },
//           "sport": {
//             //运动
//             "brief": "较适宜",
//             "details": "阴天，较适宜进行各种户内外运动。"
//           },
//           "sunscreen": {
//             //防晒
//             "brief": "弱",
//             "details": "属弱紫外辐射天气，长期在户外，建议涂擦SPF在8-12之间的防晒护肤品。"
//           },
//           "traffic": {
//             //交通
//             "brief": "良好",
//             "details": "阴天，路面干燥，交通气象条件良好，车辆可以正常行驶。"
//           },
//           "travel": {
//             //旅游
//             "brief": "适宜",
//             "details": "天气较好，温度适宜，总体来说还是好天气哦，这样的天气适宜旅游，您可以尽情地享受大自然的风光。"
//           },
//           "umbrella": {
//             //雨伞
//             "brief": "不带伞",
//             "details": "阴天，但降水概率很低，因此您在出门的时候无须带雨伞。"
//           },
//           "uv": {
//             //紫外线
//             "brief": "最弱",
//             "details": "属弱紫外线辐射天气，无需特别防护。若长期在户外，建议涂擦SPF在8-12之间的防晒护肤品。"
//           }
//         },
//         "last_update": "2015-11-28T14:10:48+08:00"
//       }
//     ]
//   }

void cjson_sport_info(char *text)
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
        cJSON *suggestion = cJSON_GetObjectItem(arrayItem, "suggestion");
        if((locat!=NULL)&&(suggestion!=NULL))
        {
            psub=cJSON_GetObjectItem(suggestion,"sport");
            psub=cJSON_GetObjectItem(psub,"brief");
            sprintf(weather.sport,"%s",psub->valuestring);
            set_system_sport(weather.sport);
            ESP_LOGI(TAG,"sport:%s",weather.sport);
        }
    }
    cJSON_Delete(root);
}

esp_err_t _https_sport_event_handler(esp_http_client_event_t *evt)
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
                        cjson_sport_info((char*)evt->user_data);
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
                        cjson_sport_info((char*)output_buffer);
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

static void https_sport_get(void)
{
    char count_url[130];
    char *city = ds_nvs_read_city();
    //判断是否有存储城市信息
    if(city != NULL){
        char url[] = "https://api.seniverse.com/v3/life/suggestion.json?key=Stng9w-RWR5e1x5xa&location=";
        uint8_t city_len = strlen(city);
        char citydata[30];
        memcpy(citydata,city,city_len);
        citydata[city_len] = '\0';
        strcat(url,citydata);
        strcat(url,"&language=zh-Hans"); 
        printf("%s\n",url);
        strcpy(count_url,url);
        free(city);
    }else{
        strcpy(count_url,WEB_API_AIR_URL);
    }
    esp_http_client_config_t config = {
        .url = count_url,
        .method = HTTP_METHOD_GET,
        .event_handler = _https_sport_event_handler,
        .cert_pem = server_root_cert_pem_start,
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
        case HTTPS_GET_SPORT:
            // http_city_get();
            https_sport_get();
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
    ds_https_request_type(HTTPS_GET_SPORT);
}

void ds_https_request_init(void)
{
	https_request_event_queue = xQueueCreate(10, sizeof(HTTPS_REQUEST_TYPE_E));
    xTaskCreate(&https_request_task, "https_request_task", 4096, NULL, 5, NULL);
}