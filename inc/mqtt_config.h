#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#include "mqtt_demo/logging_levels.h"

// Define for log name if log is kept.
#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME    "MQTTgreenhouse"
#endif

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_INFO
#endif


extern void vLoggingPrintf( const char * pcFormatString,
                            ... );

#include "mqtt_demo/logging_stack.h"

// Broker settings  
#define CLIENT_IDENTIFIER "SECRET"
char MQTT_BROKER_ENDPOINT[20] = "IP";
#define MQTT_BROKER_PORT 1883
#define MQTT_STACKSIZE    (configMINIMAL_STACK_SIZE + 256)

// Network SSID, PWD
#if 1
#define WIFI_SSID	    "SECRET"
#define WIFI_PASS       "SECRET"
#endif

#endif
