#ifndef ESP_SARA_AT
#define ESP_SARA_AT

#include "esp_err.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freeRTOS/event_groups.h"

QueueHandle_t at_queue;
EventGroupHandle_t irc_event_group;

typedef struct
{
    uart_port_t uart_num;
    int buffer_size;
} esp_sara_uart_client_handle_t;

typedef struct
{
    uart_port_t uart_num;
    int rx_pin;
    int tx_pin;
    int rts_pin;
    int dtr_pin;
    uart_config_t uart_cfg;
    int buffer_size;
} esp_sara_uart_config_t;

void esp_sara_uart_init();

esp_err_t esp_sara_disable_echo();

esp_err_t esp_sara_is_connected();
esp_err_t esp_sara_set_apn(const char *apn);
esp_err_t esp_sara_set_rat(int rat);

esp_err_t esp_sara_set_function(int fun);
esp_err_t esp_sara_check_signal();
esp_err_t esp_sara_check_modem();

esp_err_t esp_sara_set_mqtt_client_id(const char *client_id);
esp_err_t esp_sara_set_mqtt_server(const char *server, int port);
esp_err_t esp_sara_set_mqtt_timeout(uint16_t timeout);

esp_err_t esp_sara_send_at_command(const char *command, int len);

#endif