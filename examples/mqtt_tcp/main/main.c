#include "esp_log.h"
#include "esp_sara_nbiot.h"
#include "freeRTOS/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_err.h"

volatile bool mqtt_connected = false;

static esp_err_t sara_event_handle(esp_sara_event_handle_t *event)
{
    const char *TAG = "EVENT";
    esp_sara_client_handle_t * client = event->client;
    switch (event->event_id)
    {
    case SARA_EVENT_SIGNAL_FOUND:
    {
        ESP_LOGI(TAG, "Signal found");
        if (event->payload_size > 0)
            ESP_LOGI(TAG, "Signal strengh %d", event->payload[0]);
    }
    break;
    case SARA_EVENT_SIGNAL_NOT_FOUND:
    {
        ESP_LOGI(TAG, "Signal not found");
    }
    break;
    case SARA_EVENT_ATTACHED:
    {
        ESP_LOGI(TAG, "Atttached");
        if (event->payload_size > 0)
            ESP_LOGI(TAG, "Attached %d", event->payload[0]);
    }
    break;
    case SARA_EVENT_DETTACHED:
    {
        ESP_LOGI(TAG, "Dettached");
        if (event->payload_size > 0)
            ESP_LOGI(TAG, "Attached %d", event->payload[0]);
    }
    break;
    case SARA_EVENT_MQTT_CONNECTED:
    {
        ESP_LOGI(TAG, "MQTT_CONNECTED");
        mqtt_connected = true;
        esp_sara_subscribe_mqtt(client, "/test/rx", 1);
    }
    case SARA_EVENT_MQTT_DATA:
    {
        ESP_LOGI(TAG, "MQTT_DATA");
        ESP_LOGI(TAG, "topic: %s mesg: %s", event->topic, event->payload);
    }
    break;
    case SARA_EVENT_PUBLISHED:
    {
        ESP_LOGI(TAG, "MQTT_PUBLISHED");
    }
    break;
    case SARA_EVENT_PUBLISH_FAILED:
    {
        ESP_LOGE(TAG, "MQTT_PUBLISHED_FAILED");
    }
    break;
    case SARA_EVENT_MQTT_ERR:
    {
        ESP_LOGE(TAG, "MQTT ERROR %d %d", *(int*) event->payload, *(int*)(event->payload + 4));
    }
    case SARA_EVENT_CME_ERROR:
    {
        ESP_LOGE(TAG, "CME ERROR %s", (char)event->payload);
    }
    default:
        break;
    }
    return ESP_OK;
}

void app_main()
{
    ESP_LOGI("APP", "Starting...");

    esp_sara_client_config_t config = {};
    config.apn = "test.m2m.indosat.com";
    config.rat = 8;
    config.use_hex = false;
    config.transport = SARA_TRANSPORT_MQTT;
    config.event_handle = sara_event_handle;
    
    esp_sara_mqtt_client_config_t mqtt_config = {};
    mqtt_config.client_id = "dytrax";
    mqtt_config.host = "66.42.48.129";
    mqtt_config.port = 1883;
    mqtt_config.timeout = 120;
    mqtt_config.clean_session = false;

    config.transport_config = (esp_sara_transport_config_t*)&mqtt_config;

    esp_sara_client_handle_t *sara = esp_sara_client_init(&config);
    esp_sara_start(sara);

    int i = 0;
    while(1)
    {
        ESP_LOGI("APP", "esp_free_heap %u", esp_get_free_heap_size());
        if(mqtt_connected)
        {
            int csq = 99;
            esp_sara_get_csq(sara, &csq);
            char msg[32];
            sprintf(msg, "test %d", i++);
            esp_sara_publish_mqtt(sara, "/test/tx", msg, false, 1, 0);
            ESP_LOGI("APP", "%s", msg);
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
