#include <stdio.h>
#include "esp_sara_config.h"
#include "esp_sara_at.h"
#include "esp_sara_nbiot.h"
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

typedef enum
{
    SARA_UMQTTC_OP_LOGOUT = 0,
    SARA_UMQTTC_OP_LOGIN = 1,
    SARA_UMQTT_OP_PUBLISH = 2,
    SARA_UMQTTC_OP_SUBSCRIBE = 4,
    SARA_UMQTTC_OP_MESSAGE = 6,
} esp_sara_umqttc_op_t;

struct esp_sara_client
{
    TaskHandle_t esp_sara_task_handle;
    TaskHandle_t esp_sara_event_task_handle;
    TaskHandle_t esp_sara_transport_task_handle;
    esp_sara_event_callback_t event_handle;
    esp_sara_transport_t transport;
    esp_sara_transport_config_t transport_config;
    volatile esp_sara_state_t cgatt;
    volatile esp_sara_mqtt_state_t mqtt_state;
    esp_sara_config_storage_t *config;
    volatile uint8_t csq;
};

static void esp_sara_task(void *param);
static void esp_sara_event_task(void *param);
static void esp_sara_mqtt_task(void *param);

static esp_err_t esp_sara_config_mqtt(esp_sara_client_handle_t *client);
static esp_err_t esp_sara_login_mqtt(esp_sara_client_handle_t *client);
static esp_err_t esp_sara_logout_mqtt(esp_sara_client_handle_t *client);

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
    client->transport_config = config->transport_config;
    return client;
}

esp_err_t esp_sara_start(esp_sara_client_handle_t *client)
{
    xTaskCreate(esp_sara_task, "esp_sara_task", 4096, client, 5, client->esp_sara_task_handle);
    xTaskCreate(esp_sara_event_task, "esp_sara_event_task", 4096 * 3, client, 5, client->esp_sara_event_task_handle);

    switch (client->transport)
    {
    case SARA_TRANSPORT_TCP:
        break;
    case SARA_TRANSPORT_UDP:
        break;
    case SARA_TRANSPORT_MQTT:
    {
        xTaskCreate(esp_sara_mqtt_task, "esp_sara_mqtt_task", 4096, client, 5, client->esp_sara_transport_task_handle);
    }
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
    switch (client->transport)
    {
    case SARA_TRANSPORT_TCP:
        break;
    case SARA_TRANSPORT_UDP:
        break;
    case SARA_TRANSPORT_MQTT:
    {
        vTaskDelete(client->esp_sara_transport_task_handle);
    }
    break;
    default:
        break;
    }
    free(client);
    return ESP_OK;
}

void esp_sara_mqtt_task(void *param)
{
    esp_sara_client_handle_t *client = (esp_sara_client_handle_t *)param;
    esp_sara_mqtt_client_config_t *config = ((esp_sara_mqtt_client_config_t *)client->transport_config);
    int no_signal_counter = 0;
    int interval_ping = (config->timeout * 1000) / portTICK_PERIOD_MS;
    TickType_t xLastPing = xTaskGetTickCount();
    while (1)
    {
        if (client->cgatt == 1)
        {
            ESP_LOGI(TAG, "login state %d", client->mqtt_state);
            if (client->mqtt_state == SARA_MQTT_DISCONNECTED)
            {
                esp_sara_config_mqtt(client);
                esp_sara_login_mqtt(client);
                xLastPing = xTaskGetTickCount();
            }
            else
            {
                TickType_t now;
                if ((now = xTaskGetTickCount()) - xLastPing > interval_ping)
                {
                    esp_sara_ping_mqtt_server(config->host);
                    xLastPing = now;
                }
                esp_sara_mqtt_read_message();
            }
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void esp_sara_task(void *param)
{
    esp_sara_client_handle_t *client = (esp_sara_client_handle_t *)param;

    esp_err_t err = ESP_ERR_TIMEOUT;
    while (err != ESP_OK)
    {
        err = esp_sara_check_modem();
        if (err == ESP_ERR_TIMEOUT)
            ESP_LOGE(TAG, "Modem timeout. Retry...");
        vTaskDelay(1000);
    }

    int no_signal_counter = 0;
    bool reset = true;
    while (1)
    {
        if (reset)
        {
            esp_sara_disable_echo();
            esp_sara_set_apn(client->config->apn);
            esp_sara_set_rat(client->config->rat);
            reset = false;
        }
        esp_sara_check_signal();
        esp_sara_is_connected();
        if (client->csq > 31)
            no_signal_counter++;
        else
            no_signal_counter = 0;

        if (no_signal_counter > 12)
        {
            no_signal_counter = 0;
            esp_sara_set_function(15);
            reset = true;
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
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
        if (xQueueReceive(at_queue, &rc, 1000) == pdPASS)
        {
            //ESP_LOGI(TAG, "%s", rc);
            char *ch = strtok(rc, " ");

            if (ch == NULL)
                continue;

            bool should_callback = false;

            event.payload_size = 0;
            memset(event.payload, 0, 1024);

            if (strstr(ch, "+CSQ:") != NULL)
            {
                ch = strtok(NULL, ",");
                int csq = atoi(ch);

                event.event_id = csq < 32 ? SARA_EVENT_SIGNAL_FOUND : SARA_EVENT_SIGNAL_NOT_FOUND;

                esp_sara_event_id_t prev_event_id = client->csq < 32 ? SARA_EVENT_SIGNAL_FOUND : SARA_EVENT_SIGNAL_NOT_FOUND;
                should_callback = event.event_id != prev_event_id ? true : false;

                event.payload_size = 1;
                event.payload[0] = csq;
                client->csq = csq;
            }
            else if (strstr(ch, "+CGATT:") != NULL)
            {
                ch = strtok(NULL, ": ");
                int state = atoi(ch);

                event.event_id = state == 1 ? SARA_EVENT_ATTACHED : SARA_EVENT_DETTACHED;
                should_callback = state != client->cgatt ? true : false;

                event.payload_size = 0;
                client->cgatt = state;
            }
            else if (strstr(ch, "+UUMQTTC:") != NULL)
            {
                ch = strtok(NULL, ",");
                int op = atoi(ch);
                ch = strtok(NULL, ",");
                int result = atoi(ch);
                switch (op)
                {
                case SARA_UMQTTC_OP_LOGIN:
                {
                    esp_sara_mqtt_state_t mqtt_state = result == 0 ? SARA_MQTT_CONNECTED : SARA_MQTT_DISCONNECTED;

                    event.event_id = mqtt_state ? SARA_EVENT_MQTT_CONNECTED : SARA_EVENT_MQTT_DISCONNECTED;

                    should_callback = mqtt_state != client->mqtt_state ? true : false;

                    client->mqtt_state = mqtt_state;
                    if (client->mqtt_state == SARA_MQTT_DISCONNECTED)
                        esp_sara_get_mqtt_error();
                }
                break;
                case SARA_UMQTTC_OP_LOGOUT:
                {
                    esp_sara_mqtt_state_t mqtt_state = result == 0 ? SARA_MQTT_DISCONNECTED : client->mqtt_state;

                    should_callback = mqtt_state != client->mqtt_state ? true : false;

                    event.event_id = mqtt_state ? SARA_EVENT_MQTT_DISCONNECTED : SARA_EVENT_MQTT_CONNECTED;

                    client->mqtt_state = mqtt_state;
                    if (!result)
                        esp_sara_get_mqtt_error();
                }
                break;
                case SARA_UMQTTC_OP_SUBSCRIBE:
                {
                    if (result)
                    {
                        ch = strtok(NULL, ",");
                        ch = strtok(NULL, ",");
                        event.event_id = SARA_EVENT_MQTT_SUBSCRIBED;
                        memset(event.topic, '\0', 64);
                        strcpy((char *)&event.topic, ch);
                    }
                    else
                    {
                        event.event_id = SARA_EVENT_MQTT_SUBSCRIBE_FAILED;
                        esp_sara_get_mqtt_error();
                    }
                    should_callback = true;
                }
                break;
                default:
                    break;
                }
            }
            else if (strstr(ch, "+UMQTTC:") != NULL)
            {
                ch = strtok(NULL, ",");
                if (ch == NULL)
                    continue;
                int op = atoi(ch);
                ch = strtok(NULL, ",");
                if (ch == NULL)
                    continue;
                int result = atoi(ch);
                if (result == 0)
                    esp_sara_get_mqtt_error();
                switch (op)
                {
                case SARA_UMQTT_OP_PUBLISH:
                {
                    if (result)
                    {
                        event.event_id = SARA_EVENT_PUBLISHED;
                    }
                    else
                    {
                        event.event_id = SARA_EVENT_PUBLISH_FAILED;
                        esp_sara_get_mqtt_error();
                    }
                    should_callback = true;
                }
                break;
                case SARA_UMQTTC_OP_SUBSCRIBE:
                {
                    if (result)
                    {
                        event.event_id = SARA_EVENT_MQTT_SUBSCRIBED;
                    }
                    else
                    {
                        event.event_id = SARA_EVENT_MQTT_SUBSCRIBE_FAILED;
                        esp_sara_get_mqtt_error();
                    }
                    should_callback = true;
                }
                break;
                default:
                    break;
                }
            }
            else if (strstr(ch, "+UUMQTTCM:") != NULL)
            {
                ch = strtok(NULL, ",");
                if (ch == NULL)
                    continue;
                int op = atoi(ch);
                switch (op)
                {
                case SARA_UMQTTC_OP_MESSAGE:
                {
                    ch = strtok(NULL, "\r\n");
                    if (ch == NULL)
                        continue;
                    int num_messages = atoi(ch);
                    ESP_LOGI(TAG, "%d messages", num_messages);
                    for (int i = 0; i < num_messages; i++)
                    {
                        ch = strtok(NULL, "\r\n");
                        if (ch == NULL)
                            continue;
                        ESP_LOGI(TAG, "topic %s", ch + 6);
                        memset(event.topic, '\0', 64);
                        strcpy((char *)event.topic, ch + 6);
                        ch = strtok(NULL, "\r\n");
                        if (ch == NULL)
                            continue;
                        ESP_LOGI(TAG, "msg %s", ch + 4);
                        strcpy((char *)event.payload, ch + 4);
                        event.payload_size = strlen((char *)event.payload);
                        event.event_id = SARA_EVENT_MQTT_DATA;
                        if (client->event_handle)
                            client->event_handle(&event);
                    }
                }
                break;
                default:
                    break;
                }
            }
            else if (strstr(ch, "+UMQTTER:") != NULL)
            {
                ch = strtok(NULL, ",");
                if (ch == NULL)
                    continue;
                int err = atoi(ch);

                ch = strtok(NULL, ",");
                if (ch == NULL)
                    continue;
                int supl_err = atoi(ch);

                *(int*)event.payload = err;
                *(int*)(event.payload + 4) = supl_err;

                event.payload_size = 2 * sizeof(int);

                should_callback = err != 0;

                ESP_LOGE(TAG, "MQTT error %d %d", *(int*)event.payload, *(int*)event.payload + 4);
            }
            else if(strstr(ch, "+CME ERROR:") != NULL)
            {
                ch = strtok(NULL, "\r\n");
                ESP_LOGE(TAG, "%s", ch);
                
                event.event_id = SARA_EVENT_CME_ERROR;
                strcpy((char*)event.payload, ch);
                event.payload_size = strlen((char*)event.payload);
                
                should_callback = true;
            }

            if (should_callback && client->event_handle)
                client->event_handle(&event);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

esp_err_t esp_sara_get_csq(esp_sara_client_handle_t * client, int * csq)
{
    *csq = client->csq;
    return ESP_OK;
}

esp_err_t esp_sara_config_mqtt(esp_sara_client_handle_t *client)
{
    esp_sara_mqtt_client_config_t *config = (esp_sara_mqtt_client_config_t *)client->transport_config;
    esp_sara_set_mqtt_client_id(config->client_id);
    esp_sara_set_mqtt_server(config->host, config->port);
    esp_sara_set_mqtt_timeout(config->timeout);
    if(config->username && config->password)
        esp_sara_set_mqtt_auth(config->username, config->password);
    return ESP_OK;
}

esp_err_t esp_sara_login_mqtt(esp_sara_client_handle_t *client)
{
    return esp_sara_send_at_command("AT+UMQTTC=1\r\n", 14, 30000);
}

esp_err_t esp_sara_logout_mqtt(esp_sara_client_handle_t *client)
{
    return esp_sara_send_at_command("AT+UMQTTC=0\r\n", 14, 30000);
}

esp_err_t esp_sara_subscribe_mqtt(esp_sara_client_handle_t *client, const char *topic, int qos)
{
    char cmd[1024];
    int len = sprintf(cmd, "AT+UMQTTC=4,%d,\"%s\"\r\n", qos, topic);
    return esp_sara_send_at_command(cmd, len, 60000);
}

esp_err_t esp_sara_unsubscribe_mqtt(esp_sara_client_handle_t *client, const char *topic)
{
    char cmd[1024];
    int len = sprintf(cmd, "AT+UMQTTC=5,\"%s\"\r\n", topic);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_publish_mqtt(esp_sara_client_handle_t *client, const char *topic, char *data, bool use_hex, int qos, int retain)
{
    char cmd[1024];
    int len = strlen(data);
    if (use_hex)
    {
        char hex[2], hex_string[len * 2];
        int i = 0;
        for (i = 0; i < len; i++)
        {
            sprintf(hex, "%.2X", data[i]);
            strcat(hex_string, hex);
        }
        len = sprintf(cmd, "AT+UMQTTC=2,%d,%d,%d,\"%s\",\"%s\"\r\n", qos, retain, use_hex, topic, hex);
    }
    else
    {
        len = sprintf(cmd, "AT+UMQTTC=2,%d,%d,\"%s\",\"%s\"\r\n", qos, retain, topic, data);
    }
    return esp_sara_send_at_command(cmd, len, 60000);
}