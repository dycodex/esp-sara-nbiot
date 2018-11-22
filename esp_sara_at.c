#include "esp_sara_config.h"
#include "esp_sara_at.h"
#include "string.h"
#include "esp_err.h"

static const char *TAG = "SARA_AT";

static void esp_sara_read_at_response(void *param)
{
    const char *TAG = "SARA_RX";
    while (1)
    {
        if (xSemaphoreTake(uart_semaphore, 5000 / portTICK_PERIOD_MS) == pdTRUE)
        {
            int length = 0;
            ESP_ERROR_CHECK(uart_get_buffered_data_len(SARA_UART_NUM, (size_t *)&length));
            if (length > 0)
            {
                char data[length + 1];
                data[length] = '\0';
                int len = uart_read_bytes(SARA_UART_NUM, (uint8_t *)data, length, 5000);

                if (len > 2)
                {
                    ESP_LOGI(TAG, "%d %s", len, data);

                    if (xQueueSendToBack(at_queue, (void *)&data, 1000) == pdFAIL)
                    {
                        ESP_LOGE(TAG, "Send to queue failed");
                    }
                }
            }
            xSemaphoreGive(uart_semaphore);
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

static esp_err_t esp_sara_wait_irc(int timeout)
{
    const char *TAG = "SARA_IRC";
    esp_err_t err = ESP_ERR_TIMEOUT;
    int length = 0;
    bool done = false;
    char message[SARA_BUFFER_SIZE];
    memset(message, '\0', SARA_BUFFER_SIZE);
    TickType_t now, start = xTaskGetTickCount();
    while (!done)
    {
        uint8_t data[64] = {'\0'};
        uart_get_buffered_data_len(SARA_UART_NUM, (size_t *)&length);
        if (length > 0)
        {
            uart_read_bytes(SARA_UART_NUM, (uint8_t *)data, length, timeout / portTICK_PERIOD_MS);
            strcat(message, (const char *)data);
            if (strstr(message, "OK\r\n") != NULL)
            {
                err = ESP_OK;
                done = true;
            }
            else if (strstr(message, "ERROR\r\n") != NULL)
            {
                err = ESP_FAIL;
                done = true;
            }
        }
        
        if((now = xTaskGetTickCount()) - start > (timeout /portTICK_PERIOD_MS)) {
            err = ESP_ERR_TIMEOUT;
            break;
        };
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    xSemaphoreGive(uart_semaphore);

    message[strlen(message)] = '\0';
    ESP_LOGI(TAG, "%s", message);
    if (err == ESP_OK)
    {
        char *ch = strtok(message, "OK");

        while (ch != NULL)
        {
            char rc[SARA_BUFFER_SIZE];
            strcpy(rc, ch);
            int len = strlen(rc);
            if (len > 2)
            {
                rc[len] = '\0';
                if (xQueueSendToBack(at_queue, (void *)&rc, 1000 / portTICK_PERIOD_MS) == pdFAIL)
                {
                    ESP_LOGE(TAG, "Send to queue failed");
                }
            }

            ch = strtok(NULL, "OK");
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
    if (xSemaphoreTake(uart_semaphore, timeout / portTICK_PERIOD_MS))
    {
        char *TAG = "SARA_TX";
        uart_wait_tx_done(SARA_UART_NUM, timeout / portTICK_PERIOD_MS);
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
    xTaskCreate(esp_sara_read_at_response, "sara_rx_task", 4096, NULL, 4, &uart_task_handle);
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
    int len = sprintf(cmd, "AT+UMQTT=3,\"%s\",%d\r\n", server, port);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_set_mqtt_timeout(uint16_t timeout)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTT=10,%d\r\n", timeout);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_set_clean_session(bool session)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTT=12,%d\r\n", session);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_set_mqtt_auth(const char * username, const char * password)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTT=4,\"%s\",\"%s\"\r\n", username, password);
    return esp_sara_send_at_command(cmd, len, 1000);
}

esp_err_t esp_sara_ping_mqtt_server(const char *server)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTTC=8,\"%s\"\r\n", server);
    return esp_sara_send_at_command(cmd, len, 60000);
}

esp_err_t esp_sara_mqtt_read_message()
{
    const char *cmd = "AT+UMQTTC=6\r\n";
    return esp_sara_send_at_command(cmd, strlen(cmd), 1000);
}

esp_err_t esp_sara_get_mqtt_error()
{
    const char *cmd = "AT+UMQTTER\r\n";
    return esp_sara_send_at_command(cmd, strlen(cmd), 1000);
}