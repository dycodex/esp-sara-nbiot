#ifndef ESP_SARA_AT
#define ESP_SARA_AT

#include "esp_err.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <freertos/event_groups.h>

QueueHandle_t at_queue;
SemaphoreHandle_t uart_semaphore;
TaskHandle_t uart_task_handle;

void esp_sara_uart_init();

esp_err_t esp_sara_disable_echo();

esp_err_t esp_sara_req_attach(bool attach);

esp_err_t esp_sara_is_connected();
esp_err_t esp_sara_set_apn(const char *apn);
esp_err_t esp_sara_set_rat(int rat);

esp_err_t esp_sara_set_function(int fun);
esp_err_t esp_sara_check_signal();
esp_err_t esp_sara_check_modem();
esp_err_t esp_sara_check_sim();

esp_err_t esp_sara_set_mqtt_client_id(const char *client_id);
esp_err_t esp_sara_set_mqtt_server(const char *server, int port);
esp_err_t esp_sara_set_mqtt_timeout(uint16_t timeout);
esp_err_t esp_sara_set_clean_session(bool session);

esp_err_t esp_sara_set_mqtt_auth(const char * username, const char * password);

esp_err_t esp_sara_ping_mqtt_server(const char *server);
esp_err_t esp_sara_mqtt_read_message();
esp_err_t esp_sara_get_mqtt_error();

esp_err_t esp_sara_send_at_command(const char *command, int len, int timeout);

#endif