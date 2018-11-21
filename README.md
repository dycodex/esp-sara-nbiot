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

### Options for esp_sara_mqtt_client_config_t

- ```host```: ip address of MQTT broker
- ```port```: port of MQTT broker
- ```client_id```: id of MQTT client
- ```timeout```: keep alive timeout
- ```clean_session```: should client sesssion when connect