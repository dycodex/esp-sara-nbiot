#include "esp_sara_config.h"
#include "esp_sara_at.h"
#include "string.h"
#include "esp_err.h"

static const char *TAG = "SARA_AT";

static esp_err_t esp_sara_at_parser(char *resp, char *result)
{
    esp_err_t err = ESP_ERR_INVALID_RESPONSE;
    char *ch = strtok(resp, "\r\n");
    while (ch != NULL)
    {
        if (strncmp(ch, "+", 1) == 0)
        {
            strcpy(result, ch);
            err = ESP_OK;
        }
        else if (strcmp(ch, "OK") == 0)
        {
            err = ESP_OK;
        }
        else if (strcmp(ch, "ERROR") == 0)
        {
            err = ESP_ERR_NOT_SUPPORTED;
        }
        ch = strtok(NULL, "\r\n");
    }

    return err;
}

static void esp_sara_read_at_response(void *param)
{
    const char *TAG = "SARA_RX";
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    while (1)
    {
        if (xSemaphoreTake(uart_semaphore, 5000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            int retry = 0;
            bool done = false;
            char message[SARA_BUFFER_SIZE];
            memset(message, '\0', SARA_BUFFER_SIZE);
            while (!done)
            {
                int length = 0;
                ESP_ERROR_CHECK(uart_get_buffered_data_len(SARA_UART_NUM, (size_t *)&length));
                if (length > 0)
                {
                    uint8_t data[length];
                    data[0] = '\0';
                    uart_read_bytes(SARA_UART_NUM, (uint8_t *)data, length, 5000);
                    ESP_LOGI(TAG, "%s", data);
                    strcat(message, (char *)data);
                    if (strstr(message, "\r\n") != NULL)
                        done = true;
                }
                if (retry++ == 10)
                    done = true;
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            char rc[SARA_BUFFER_SIZE];
            memset(rc, '\0', SARA_BUFFER_SIZE);

            esp_err_t err = esp_sara_at_parser((char *)&message, (char*)&rc);
            if (err != ESP_ERR_NOT_SUPPORTED)
            {
                if (strlen(rc) > 0)
                {
                    if (xQueueSendToBack(at_queue, (void *)&rc, 1000) == pdFAIL)
                    {
                        ESP_LOGE(TAG, "Send to queue failed");
                    }
                }
            }
            else
            {
                ESP_LOGE(TAG, "AT Command Error");
            }
            xSemaphoreGive(uart_semaphore);
        }
    }
}

static esp_err_t esp_sara_wait_irc(int timeout)
{
    esp_err_t err = ESP_ERR_TIMEOUT;
    int length = 0;
    bool done = false;
    char message[SARA_BUFFER_SIZE] = {'\0'};
    while (!done)
    {
        uint8_t data[64] = {'\0'};
        uart_get_buffered_data_len(SARA_UART_NUM, (size_t *)&length);
        if (length > 0)
        {
            uart_read_bytes(SARA_UART_NUM, (uint8_t *)data, length, 5000 / portTICK_PERIOD_MS);
            strcat(message, (const char *)data);
            if ((strstr(message, "OK") != NULL) || (strstr(message, "ERROR") != NULL))
            {
                done = true;
            }
        }
        vTaskDelay(10);
    }
    xSemaphoreGive(uart_semaphore);

    ESP_LOGI(TAG, "%s", message);

    char rc[SARA_BUFFER_SIZE];
    err = esp_sara_at_parser((char *)&message, (char*)&rc);
    if (err != ESP_ERR_NOT_SUPPORTED)
    {
        if (strlen(rc) > 0)
        {
            if (xQueueSendToBack(at_queue, (void *)&rc, 1000 / portTICK_PERIOD_MS) == pdFAIL)
            {
                ESP_LOGE(TAG, "Send to queue failed");
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "AT Command Error");
    }
    return err;
}

esp_err_t esp_sara_send_at_command(const char *command, int len, int timeout)
{
    esp_err_t err = ESP_ERR_TIMEOUT;
    if (xSemaphoreTake(uart_semaphore, 1000 / portTICK_PERIOD_MS))
    {
        char *TAG = "SARA_TX";
        uart_wait_tx_done(SARA_UART_NUM, 1000);
        uart_write_bytes(SARA_UART_NUM, command, len);
        ESP_LOGI(TAG, "%s", command);
        err = esp_sara_wait_irc(timeout);
    }
    return err;
}

void esp_sara_uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = SARA_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = SARA_UART_HW_CONTROL,
        .rx_flow_ctrl_thresh = 122,
    };
    ESP_ERROR_CHECK(uart_param_config(SARA_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(SARA_UART_NUM, SARA_UART_TX_PIN, SARA_UART_RX_PIN, SARA_UART_RTS_PIN, SARA_UART_DTR_PIN));
    ESP_ERROR_CHECK(uart_driver_install(SARA_UART_NUM, SARA_BUFFER_SIZE * 2, SARA_BUFFER_SIZE, SARA_BUFFER_SIZE, NULL, 0));

    at_queue = xQueueCreate(5, SARA_BUFFER_SIZE);
    uart_semaphore = xSemaphoreCreateMutex();
    xTaskCreate(esp_sara_read_at_response, "sara_rx_task", 4096, NULL, 5, &uart_task_handle);
    ESP_LOGI(TAG, "esp_sara_uart_client_handle_t init");
}

esp_err_t esp_sara_disable_echo()
{
    return esp_sara_send_at_command("ATE0\r\n", 7, 1000);
}

esp_err_t esp_sara_req_attach(bool attach)
{
    char cmd[32];
    int len = sprintf(cmd, "AT+CGATT=%d\r\n", attach);
    return esp_sara_send_at_command(cmd, len, 60000);
}

esp_err_t esp_sara_check_signal()
{
    const char *cmd = "AT+CSQ\r\n";
    return esp_sara_send_at_command(cmd, strlen(cmd), 1000);
}

esp_err_t esp_sara_check_modem()
{
    const char *cmd = "AT\r\n";
    return esp_sara_send_at_command(cmd, strlen(cmd), 1000);
}

esp_err_t esp_sara_is_connected()
{
    const char *cmd = "AT+CGATT?\r\n";
    return esp_sara_send_at_command(cmd, strlen(cmd), 1000);
}

esp_err_t esp_sara_set_apn(const char *apn)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", apn);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_set_rat(int rat)
{
    char cmd[32];
    int len = sprintf(cmd, "AT+URAT=%d\r\n", rat);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_set_function(int fun)
{
    char cmd[32];
    int len = sprintf(cmd, "AT+CFUN=%d\r\n", fun);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_set_mqtt_client_id(const char *client_id)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTT=0,\"%s\"\r\n", client_id);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_set_mqtt_server(const char *server, int port)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTT=1,%d\r\n", port);
    esp_sara_send_at_command(cmd, len, 1000);
    len = sprintf(cmd, "AT+UMQTT=2,\"%s\"\r\n", server);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_set_mqtt_timeout(uint16_t timeout)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTT=10,%d\r\n", timeout);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_ping_mqtt_server(const char *server)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTTC=8,\"%s\"\r\n", server);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_mqtt_read_message()
{
    const char *cmd = "AT+UMQTTC=6\r\n";
    return esp_sara_send_at_command(cmd, strlen(cmd), 1000);
}