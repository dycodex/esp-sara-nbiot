#ifndef SARA_H
#define SARA_H

#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_sara_client esp_sara_client_handle_t;

typedef enum {
    SARA_EVENT_ERROR = 0,
    SARA_EVENT_ATTACHED,
    SARA_EVENT_DETTACHED,
    SARA_EVENT_SIGNAL_FOUND,
    SARA_EVENT_SIGNAL_NOT_FOUND,
    SARA_EVENT_PUBLISHED,
    SARA_EVENT_PUBLISH_FAILED,
    SARA_EVENT_DATA,
    SARA_EVENT_MQTT_DATA,
    SARA_EVENT_SOCKET_CREATED,
    SARA_EVENT_SOCKET_CLOSED,
    SARA_EVENT_MQTT_CONNECTED,
    SARA_EVENT_MQTT_DISCONNECTED,
    SARA_EVENT_MQTT_SUBSCRIBED,
    SARA_EVENT_MQTT_SUBSCRIBE_FAILED,
    SARA_EVENT_UNKNOWN
} esp_sara_event_id_t;

typedef enum {
    SARA_TRANSPORT_TCP = 0,
    SARA_TRANSPORT_UDP,
    SARA_TRANSPORT_MQTT
} esp_sara_transport_t;

typedef struct {
    const char * host;
    uint32_t port;
    const char * client_id;
    uint16_t timeout;
    bool clean_session;
    const char * username;
    const char * password;
} esp_sara_mqtt_client_config_t;

typedef struct {
    esp_sara_event_id_t event_id;
    esp_sara_client_handle_t * client;
    int payload_size;
    uint8_t payload[1024];
    char topic[64];
} esp_sara_event_handle_t;

typedef esp_err_t (*esp_sara_event_callback_t)(esp_sara_event_handle_t * event);

typedef void * esp_sara_transport_config_t;

typedef struct {
    const char * apn;
    uint8_t rat;
    bool use_hex;
    esp_sara_event_callback_t event_handle;
    esp_sara_transport_t transport;
    esp_sara_transport_config_t * transport_config;
} esp_sara_client_config_t;

esp_sara_client_handle_t * esp_sara_client_init(esp_sara_client_config_t *config);
esp_err_t esp_sara_start(esp_sara_client_handle_t * client);
esp_err_t esp_sara_stop(esp_sara_client_handle_t * client);

/* UBLOX SARA R4 MQTTT */
esp_err_t esp_sara_subscribe_mqtt(esp_sara_client_handle_t * client, const char * topic, int qos);
esp_err_t esp_sara_unsubscribe_mqtt(esp_sara_client_handle_t * client, const char * topic);
esp_err_t esp_sara_publish_mqtt(esp_sara_client_handle_t * client, const char * topic, char * data, bool use_hex, int qos, int retain);


#ifdef __cplusplus
}
#endif
#endif