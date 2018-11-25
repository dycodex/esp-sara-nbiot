# ESP SARA NBIOT Library

## Features
- Support MQTT over TCP
- Support publishing, subscribing, pings

## How To Use

Clone this repository as component

```git submodule add https://github.com/dycodex/esp-sara-nbiot components/esp_sara_nbiot```

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
    .apn = "internet",
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
- ```username```: username for MQTT authentication
- ```password```: password for MQTT authentication

example configuration of esp_sara_mqtt_client_config_t
```
esp_sara_mqtt_client_config_t mqtt_config = {
    .client_id = "dytrax",
    .host = "127.0.0.1",
    .port = 1883,
    .timeout = 120,
    .clean_session = false,
};

config.transport_config = (esp_sara_transport_config_t*)&mqtt_config;
```

### esp_sara_event_handle_t

- ```event_id```: id to identify which event is happen
- ```client```: pointer to esp_sara_client_handle_t client
- ```payload_size```: size of payload, contents of payload differ in each event and some may not have payload
- ```payload```: array to hold data, content of payload is:
    - ```SARA_EVENT_MQTT_SIGNAL_FOUND```: contain 1 unsigned char csq value
    - ```SARA_EVENT_MQTT_DATA```: contain message of subscription topic
    - ```SARA_EVENT_CME_ERROR```: contain message of CME ERROR
    - ```SARA_EVENT_MQTT_ERR```: contain error code and suplementary error code of MQTT
- ```topic```: topic when there is data from MQTT subscriptions

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
        esp_sara_subscribe_mqtt(client, "generic_brand_617/generic_device/v1nm/common", 1);
    }
    break;
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
    break;
    case SARA_EVENT_CME_ERROR:
    {
        ESP_LOGE(TAG, "CME ERROR %s", (char*)event->payload);
    }
    default:
        break;
    }
    return ESP_OK;
}

esp_sara_mqtt_client_config_t mqtt_config = {
    .client_id = "dytrax",
    .host = "127.0.0.1",
    .port = 1883,
    .timeout = 120,
    .clean_session = false,
};

esp_sara_client_config_t config = {
    .apn = "internet",
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