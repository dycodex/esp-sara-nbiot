# ESP SARA NBIOT Library

## Features
- Support MQTT over TCP
- Support publishing, subscribing, pings

## How To Use

Clone this repository as component

```git submodule add https://gitlab.com/DyCode/DycodeX/esp-sara-nbiot.git components/esp_sara_nbiot```

## Documentation

### Options for esp_sara_client_config_t

- ```apn```: internet apn
- ```rat```: radio access technology
- ```use_hex```: use hex in payload
- ```event_handle```: for sara event
- ```transport```: technology for communication
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

- ```host```: ip address of MQTT broker
- ```port```: port of MQTT broker
- ```client_id```: id of MQTT client
- ```timeout```: keep alive timeout
- ```clean_session```: should client sesssion when connect

example configuration of esp_sara_mqtt_client_config_t
```
esp_sara_mqtt_client_config_t mqtt_config = {
    mqtt_config.client_id = "dytrax",
    mqtt_config.host = "66.42.48.129",
    mqtt_config.port = 1883,
    mqtt_config.timeout = 120,
    mqtt_config.clean_session = false,
};

config.transport_config = (esp_sara_transport_config_t*)&mqtt_config;
```