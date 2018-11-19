#include "esp_sara_config.h"
#include "esp_sara_at.h"
#include "string.h"
#include "esp_err.h"

static const char *TAG = "SARA_AT";

static esp_err_t esp_sara_at_parser(const char *resp, char *result)
{
    const char *TAG = "SARA_PARSER";
    esp_err_t err = ESP_ERR_INVALID_RESPONSE;
    char *ch = strtok(resp, "\r\n");
    result[0] = '\0';
    while (ch != NULL)
    {
        if (strncmp(ch, "+", 1) == 0)
        {
            strcpy(result, ch);
            result[strlen(result) + 1] = '\0';
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

esp_err_t esp_sara_send_at_command(const char *command, int len)
{
    char *TAG = "SARA_TX";
    uart_wait_tx_done(SARA_UART_NUM, 1000);
    int txBytes = uart_write_bytes(SARA_UART_NUM, command, len);
    ESP_LOGI(TAG, "%s", command);
    if (txBytes >= 0)
        return ESP_OK;
    return ESP_ERR_INVALID_ARG;
}

static void esp_sara_read_at_response(void *param)
{
    const char *TAG = "SARA_RX";
    while (1)
    {
        if (xSemaphoreTake(uart_semaphore, 5000) == pdTRUE)
        {
            int length = 0;
            ESP_ERROR_CHECK(uart_get_buffered_data_len(SARA_UART_NUM, (size_t *)&length));
            if (length > 0)
            {
                uint8_t data[length + 1];
                data[length] = '\0';
                int len = uart_read_bytes(SARA_UART_NUM, (uint8_t *)data, length, 5000);
                ESP_LOGI(TAG, "%s", data);

                char rc[SARA_BUFFER_SIZE];
                esp_err_t err = esp_sara_at_parser((const char *)&data, &rc);
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
            }
            xSemaphoreGive(uart_semaphore);
        }
    }
}

esp_err_t esp_sara_wait_irc(int timeout)
{

    if (xSemaphoreTake(uart_semaphore, timeout))
    {
        int length = 0, retry = 0;
        while (length == 0)
        {
            uart_get_buffered_data_len(SARA_UART_NUM, (size_t *)&length);
            if (retry++ == 3)
                break;
            vTaskDelay(10);
        }

        uint8_t data[64] = {'\0'};
        int len = uart_read_bytes(SARA_UART_NUM, (uint8_t *)data, length, 5000);
        xSemaphoreGive(uart_semaphore);
        ESP_LOGI(TAG, "%s", data);

        char rc[SARA_BUFFER_SIZE];
        esp_err_t err = esp_sara_at_parser((const char *)&data, &rc);
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
        if (strstr((const char *)data, "ERROR") != NULL)
            return ESP_FAIL;
        else if (strstr((const char *)data, "OK") != NULL)
            return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

void esp_sara_uart_init()
{
    uart_config_t uart_config = {};
    uart_config.baud_rate = SARA_UART_BAUD;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = SARA_UART_HW_CONTROL;
    uart_config.rx_flow_ctrl_thresh = 122;

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
    esp_sara_send_at_command("ATE0\r\n", 7);
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_req_attach(bool attach)
{
    char cmd[32];
    int len = sprintf(cmd, "AT+CGATT=%d\r\n", attach);
    esp_sara_send_at_command(cmd, strlen(cmd));
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_check_signal()
{
    const char *cmd = "AT+CSQ\r\n";
    esp_sara_send_at_command(cmd, strlen(cmd));
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_check_modem()
{
    const char *cmd = "AT\r\n";
    esp_sara_send_at_command(cmd, strlen(cmd));
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_is_connected()
{
    const char *cmd = "AT+CGATT?\r\n";
    esp_sara_send_at_command(cmd, strlen(cmd));
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_set_apn(const char *apn)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", apn);
    esp_sara_send_at_command(cmd, len);
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_set_rat(int rat)
{
    char cmd[32];
    int len = sprintf(cmd, "AT+URAT=%d\r\n", rat);
    esp_sara_send_at_command(cmd, len);
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_set_function(int fun)
{
    const char cmd[32];
    int len = sprintf(cmd, "AT+CFUN=%d\r\n", fun);
    esp_sara_send_at_command(cmd, strlen(cmd));
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_set_mqtt_client_id(const char *client_id)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTT=0,\"%s\"\r\n", client_id);
    esp_sara_send_at_command(cmd, len);
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_set_mqtt_server(const char *server, int port)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTT=1,%d\r\n", port);
    esp_sara_send_at_command(cmd, len);
    esp_sara_wait_irc(1000);
    len = sprintf(cmd, "AT+UMQTT=2,\"%s\"\r\n", server);
    esp_sara_send_at_command(cmd, len);
    return esp_sara_wait_irc(1000);
}

esp_err_t esp_sara_set_mqtt_timeout(uint16_t timeout)
{
    char cmd[64];
    int len = sprintf(cmd, "AT+UMQTT=10,%d\r\n", timeout);
    esp_sara_send_at_command(cmd, len);
    return esp_sara_wait_irc(1000);
}
