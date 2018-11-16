#include <stdio.h>
#include "esp_sara_config.h"
#include "esp_sara_at.h"
#include "esp_sara.h"
#include "freeRTOS/FreeRTOS.h"
#include "string.h"

static const char *TAG = "SARA_CLIENT";

typedef struct
{
    const char *apn;
    uint8_t rat;
    bool use_hex;
    uint16_t buffer_size;
} esp_sara_config_storage_t;

typedef enum
{
    SARA_DETTACHED = 0,
    SARA_ATTACHED
} esp_sara_state_t;

typedef enum
{
    SARA_MQTT_DISCONNECTED = 0,
    SARA_MQTT_CONNECTED
} esp_sara_mqtt_state_t;

struct esp_sara_client
{
    TaskHandle_t esp_sara_task_handle;
    TaskHandle_t esp_sara_event_task_handle;
    TaskHandle_t esp_sara_transport_task_handle;
    esp_sara_event_callback_t event_handle;
    esp_sara_transport_t transport;
    esp_sara_state_t cgatt;
    esp_sara_mqtt_state_t mqtt_state;
    esp_sara_config_storage_t *config;
    uint8_t csq;
};

static void esp_sara_task(void *param);
static void esp_sara_event_task(void *param);
static void esp_sara_mqtt_task(void *param);

static esp_err_t esp_sara_config_mqtt(esp_sara_client_handle_t *client, esp_sara_mqtt_client_config_t *config);

esp_sara_client_handle_t *esp_sara_client_init(esp_sara_client_config_t *config)
{
    esp_sara_client_handle_t *client = calloc(1, sizeof(struct esp_sara_client));

    esp_sara_uart_init();
    client->csq = 99;
    client->cgatt = SARA_DETTACHED;

    client->config = calloc(1, sizeof(esp_sara_config_storage_t));
    client->event_handle = config->event_handle;
    client->config->apn = config->apn;
    client->config->rat = config->rat;
    client->config->use_hex = config->use_hex;
    client->config->buffer_size = SARA_BUFFER_SIZE;
    client->transport = config->transport;

    switch (client->transport)
    {
    case SARA_TRANSPORT_TCP:
        break;
    case SARA_TRANSPORT_UDP:
        break;
    case SARA_TRANSPORT_MQTT:
        esp_sara_config_mqtt(client, (esp_sara_mqtt_client_config_t *)config->transport_config);
        break;
    default:
        break;
    }
    return client;
}

esp_err_t esp_sara_start(esp_sara_client_handle_t *client)
{
    xTaskCreate(esp_sara_task, "esp_sara_task", 4096, client, 5, client->esp_sara_task_handle);
    xTaskCreate(esp_sara_event_task, "esp_sara_event_task", 4096, client, 5, client->esp_sara_event_task_handle);
    switch (client->transport)
    {
    case SARA_TRANSPORT_TCP:
        break;
    case SARA_TRANSPORT_UDP:
        break;
    case SARA_TRANSPORT_MQTT:
        xTaskCreate(esp_sara_mqtt_task, "esp_sara_mqtt_task", 4096, client, 5, client->esp_sara_transport_task_handle);
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t esp_sara_stop(esp_sara_client_handle_t *client)
{
    vTaskDelete(client->esp_sara_task_handle);
    vTaskDelete(client->esp_sara_event_task_handle);
    free(client);
    return ESP_OK;
}

static void esp_sara_mqtt_task(void *param)
{
    esp_sara_client_handle_t *client = (esp_sara_client_handle_t *)param;

    while (1)
    {
        if (client->cgatt == 1)
        {
            if (client->mqtt_state == SARA_MQTT_DISCONNECTED)
            {
                esp_sara_login_mqtt(client);
            }
        }

        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}

static void esp_sara_task(void *param)
{
    esp_sara_client_handle_t *client = (esp_sara_client_handle_t *)param;
    esp_err_t err;
    err = esp_sara_check_modem();
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Timeout");
    //esp_sara_set_function(15);
    //vTaskDelay(3000/portTICK_PERIOD_MS);
    esp_sara_set_apn(client->config->apn);
    esp_sara_set_rat(client->config->rat);

    int no_signal_counter = 0;
    while (1)
    {
        esp_sara_check_signal();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_sara_is_connected();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        switch (client->transport)
        {
        case SARA_TRANSPORT_TCP:
            break;
        case SARA_TRANSPORT_UDP:
            break;
        case SARA_TRANSPORT_MQTT:
            break;
        default:
            break;
        }
        if(client->csq > 31) no_signal_counter++;

        if(no_signal_counter > 10)
        {
            no_signal_counter = 0;
            esp_sara_set_function(15);
            vTaskDelay(9000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void esp_sara_event_task(void *param)
{
    const char *TAG = "SARA_EVENT";
    esp_sara_client_handle_t *client = (esp_sara_client_handle_t *)param;
    esp_sara_event_handle_t event;
    event.client = client;
    while (1)
    {
        char rc[SARA_BUFFER_SIZE];
        if (xQueueReceive(at_queue, &rc, 10) == pdPASS)
        {
            if (rc != NULL)
            {
                ESP_LOGI(TAG, "%s", rc);
                char *ch = strtok(rc, ": ");
                bool should_callback = false;

                if (ch != NULL)
                {
                    if (strcmp(ch, "+CSQ") == 0)
                    {
                        ch = strtok(NULL, ",");
                        int csq = atoi(ch);
                        if (csq > 31 && client->csq < 32)
                        {
                            event.event_id = SARA_EVENT_SIGNAL_NOT_FOUND;
                            should_callback = true;
                        }
                        else if (csq < 32 && client->csq > 31)
                        {
                            event.event_id = SARA_EVENT_SIGNAL_FOUND;
                            should_callback = true;
                        }
                        event.payload_size = 1;
                        event.payload[0] = csq;
                        client->csq = csq;
                    }
                    else if (strcmp(ch, "+CGATT") == 0)
                    {
                        ch = strtok(NULL, ": ");
                        int state = atoi(ch);
                        if (state == 1 && client->cgatt == 0)
                        {
                            event.event_id = SARA_EVENT_ATTACHED;
                            should_callback = true;
                        }
                        else if (state == 0 && client->cgatt == 1)
                        {
                            event.event_id = SARA_EVENT_DETTACHED;
                            should_callback = true;
                        }
                        event.payload_size = 0;
                        client->cgatt = state;
                    }
                    else if (strcmp(ch, "+UUMQTTC") == 0)
                    {
                        ch = strtok(NULL, ",");
                        int op = atoi(ch);
                        ch = strtok(NULL, ",");
                        int result = atoi(ch);
                        bool mqtt_state = result == 0;

                        if(mqtt_state != client->mqtt_state)
                        {
                            event.event_id = SARA_EVENT_MQTT_CONNECTED;
                            should_callback = true;
                        }

                        client->mqtt_state = mqtt_state;
                    }
                }

                if (should_callback && client->event_handle)
                    client->event_handle(&event);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

esp_err_t esp_sara_config_mqtt(esp_sara_client_handle_t *client, esp_sara_mqtt_client_config_t *config)
{
    esp_sara_set_mqtt_client_id(config->client_id);
    esp_sara_set_mqtt_server(config->host, config->port);
    esp_sara_set_mqtt_timeout(config->timeout);
    return ESP_OK;
}

esp_err_t esp_sara_login_mqtt(esp_sara_client_handle_t *client)
{
    return esp_sara_send_at_command("AT+UMQTTC=1\r\n", 14);
}

esp_err_t esp_sara_logout_mqtt(esp_sara_client_handle_t *client)
{
    return esp_sara_send_at_command("AT+UMQTTC=0\r\n", 14);
}

esp_err_t esp_sara_subscribe_mqtt(esp_sara_client_handle_t *client, const char *topic, int qos)
{
    char cmd[1024];
    int len = sprintf(cmd, "AT+UMQTTC=4,%d,\"%s\"\r\n", qos, topic);
    return esp_sara_send_at_command(cmd, len);
}

esp_err_t esp_sara_unsubscribe_mqtt(esp_sara_client_handle_t *client, const char *topic)
{
    char cmd[1024];
    int len = sprintf(cmd, "AT+UMQTTC=5,\"%s\"\r\n", topic);
    return esp_sara_send_at_command(cmd, len);
}

esp_err_t esp_sara_publish_mqtt(esp_sara_client_handle_t *client, const char *topic, uint8_t *data, bool use_hex, int qos, int retain)
{
    char cmd[1024];
    int len = 0;
    if (use_hex)
    {
        char hex[len * 2];
        int i = 0, j = 0;
        for (i = 0; i < len * 2; i += 2)
        {
            sprintf(hex + i, "%.2X", data[j++]);
        }
        len = sprintf(cmd, "AT+UMQTTC=2,%d,%d,%d,\"%s\",\"%s\"\r\n", qos, retain, use_hex, topic, hex);
    }
    else
    {
        len = sprintf(cmd, "AT+UMQTTC=2,%d,%d,%d,\"%s\",\"%s\"\r\n", qos, retain, use_hex, topic, data);
    }
    return esp_sara_send_at_command(cmd, len);
}