# ESP SARA NBIOT Library

## Features
- Support MQTT over TCP
- Support publishing, subscribing, pings

## How To Use

Clone this repository as component

```git submodule add https://gitlab.com/DyCode/DycodeX/esp-sara-nbiot.git components/esp_sara_nbiot```

## Documentation

### Options for esp_sara_client_config_t

- ```apn```: internet apn string
- ```rat```: radio access technology
- ```use_hex```: use hex in payload
- ```event_handle```: for sara events
- ```transport```: transport protocol to use
    - ```SARA_TRANSPORT_MQTT```: Transport over MQTT tcp
- ```transport_config```: configuration for transport

example configuration of esp_sara_client_config_t

```
esp_sara_client_config_t config = {
    .apn = "test.m2m.indosat.com",
    .rat = 8,
    .use_hex = false,
    .transport = SARA_TRANSPORT_MQTT,
    .event_handle = sara_event_handle,
};
```

### Options for esp_sara_mqtt_client_config_t

- ```host```: ip address string of MQTT broker
- ```port```: port of MQTT broker
- ```client_id```: id of MQTT client
- ```timeout```: keep alive timeout
- ```clean_session```: should client sesssion when connect

example configuration of esp_sara_mqtt_client_config_t
```
esp_sara_mqtt_client_config_t mqtt_config = {
    .client_id = "dytrax",
    .host = "66.42.48.129",
    .port = 1883,
    .timeout = 120,
    .clean_session = false,
};

config.transport_config = (esp_sara_transport_config_t*)&mqtt_config;
```

### Change setting in ```menuconfig```
```
make menuconfig
Component config -> ESP SARA NBIOT
```

## Example

Check examples ```examples\mqtt_tcp```

```
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
        esp_sara_publish_mqtt(sara, "/test/tx", "test, false, 1, 0);
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
    default:
        break;
    }
    return ESP_OK;
}

esp_sara_mqtt_client_config_t mqtt_config = {
    .client_id = "dytrax",
    .host = "66.42.48.129",
    .port = 1883,
    .timeout = 120,
    .clean_session = false,
};

esp_sara_client_config_t config = {
    .apn = "test.m2m.indosat.com",
    .rat = 8,
    .use_hex = false,
    .transport = SARA_TRANSPORT_MQTT,
    .event_handle = sara_event_handle,
    .transport_config = (esp_sara_transport_config_t*)&mqtt_config,
};

esp_sara_client_handle_t *sara = esp_sara_client_init(&config);
esp_sara_start(sara);

```

## License