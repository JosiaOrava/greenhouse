#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <cr_section_macros.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "heap_lock_monitor.h"
#include "retarget_uart.h"

extern "C" {
#include "mqtt_config.h"
#include "core_mqtt.h"
#include "backoff_algorithm.h"
#include "using_plaintext.h"
}

#include "ModbusRegister.h"
#include "DigitalIoPin.h"
#include "LiquidCrystal.h"

#include "HumTemp.h"
#include "Co2.h"

#include <rotaryTask.h>
#include <relayTask.h>


#define LCD_TASK_PRIORITY 2UL
#define LCD_TASK_DELAY 300

/*-----------------------------------------------------------------------------------*/

// Defines for MQTT
#define mqttRETRY_MAX_ATTEMPTS            ( 5U ) // Maximum number of retry attempts for MQTT operations
#define mqttRETRY_MAX_BACKOFF_DELAY_MS    ( 5000U ) // Maximum backoff delay in milliseconds for retrying MQTT operations
#define mqttRETRY_BACKOFF_BASE_MS         ( 500U ) // Base backoff delay in milliseconds for retrying MQTT operations
#define mqttCONNACK_RECV_TIMEOUT_MS           ( 1000U ) // Timeout in milliseconds for waiting to receive CONNACK packet after connecting
#define mqttTOPIC                             "channels/2352903/publish" // MQTT topic string for publishing messages
#define mqttTOPIC_COUNT                       ( 1 ) // Number of MQTT topics (in this case, 1)
#define mqttSHARED_BUFFER_SIZE                ( 500U ) // Size of the shared buffer for MQTT communication
#define mqttKEEP_ALIVE_TIMEOUT_SECONDS        ( 60U ) // Keep-alive timeout in seconds for MQTT connection
#define mqttTRANSPORT_SEND_RECV_TIMEOUT_MS    ( 200U ) // Timeout in milliseconds for sending and receiving data during MQTT transport

#define MILLISECONDS_PER_SECOND                      ( 1000U )
#define MILLISECONDS_PER_TICK                        ( MILLISECONDS_PER_SECOND / configTICK_RATE_HZ ) // Conversion factor: milliseconds per FreeRTOS tick


char mqttMESSAGE[100];

struct NetworkContext
{
    PlaintextTransportParams_t * pParams;
};

static void MQTTtask( void * pvParameters );
static PlaintextTransportStatus_t prvConnectToServerWithBackoffRetries( NetworkContext_t * pxNetworkContext );
static void prvCreateMQTTConnectionWithBroker( MQTTContext_t * pxMQTTContext, NetworkContext_t * pxNetworkContext );
static void prvMQTTPublishToTopic( MQTTContext_t * pxMQTTContext );

static uint32_t prvGetTimeMs( void );

static uint8_t ucSharedBuffer[ mqttSHARED_BUFFER_SIZE ];
static uint32_t ulGlobalEntryTimeMs;

typedef struct topicFilterContext
{
    const char * pcTopicFilter;
    MQTTSubAckStatus_t xSubAckStatus;
} topicFilterContext_t;


static MQTTFixedBuffer_t xBuffer =
{
    .pBuffer = ucSharedBuffer,
    .size    = mqttSHARED_BUFFER_SIZE
};

void StartMQTT( void ){

    xTaskCreate( MQTTtask,
                 "MQTT",
                 MQTT_STACKSIZE,
                 NULL,
                 tskIDLE_PRIORITY,
                 NULL );
}

// Main task for handling MQTT communication and periodic publishing of sensor data
static void MQTTtask( void * pvParameters ){


	// Network and MQTT structures
    NetworkContext_t xNetworkContext = { 0 };
    PlaintextTransportParams_t xPlaintextTransportParams = { 0 };
    MQTTContext_t xMQTTContext;
    MQTTStatus_t xMQTTStatus;
    PlaintextTransportStatus_t xNetworkStatus;


    ( void ) pvParameters;

    // using plaintext without encryption
    xNetworkContext.pParams = &xPlaintextTransportParams;

    ulGlobalEntryTimeMs = prvGetTimeMs();

    for(;;){
    	// Connect using BackOffRetries algorithm that tries to connect again if connection fails using exponential time
        xNetworkStatus = prvConnectToServerWithBackoffRetries(&xNetworkContext);
        configASSERT(xNetworkStatus == PLAINTEXT_TRANSPORT_SUCCESS);

        // Connecting to broker
        prvCreateMQTTConnectionWithBroker(&xMQTTContext, &xNetworkContext);

        // publishing to topic
        prvMQTTPublishToTopic(&xMQTTContext);


        xNetworkStatus = Plaintext_FreeRTOS_Disconnect(&xNetworkContext);
        configASSERT(xNetworkStatus == PLAINTEXT_TRANSPORT_SUCCESS);

        // Delay for how long between publishing
        vTaskDelay(15000);
    }
}


// Algorithm provided by freeRTOS
static PlaintextTransportStatus_t prvConnectToServerWithBackoffRetries( NetworkContext_t * pxNetworkContext ){
    PlaintextTransportStatus_t xNetworkStatus;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t xReconnectParams;
    uint16_t usNextRetryBackOff = 0U;

    BackoffAlgorithm_InitializeParams( &xReconnectParams,
                                       mqttRETRY_BACKOFF_BASE_MS,
                                       mqttRETRY_MAX_BACKOFF_DELAY_MS,
                                       mqttRETRY_MAX_ATTEMPTS );

    do{

        LogInfo( ( "Create a TCP connection to %s:%d.",
                   MQTT_BROKER_ENDPOINT,
                   MQTT_BROKER_PORT ) );
        xNetworkStatus = Plaintext_FreeRTOS_Connect( pxNetworkContext,
                                                     MQTT_BROKER_ENDPOINT,
                                                     MQTT_BROKER_PORT,
                                                     mqttTRANSPORT_SEND_RECV_TIMEOUT_MS,
                                                     mqttTRANSPORT_SEND_RECV_TIMEOUT_MS );

        if( xNetworkStatus != PLAINTEXT_TRANSPORT_SUCCESS ){
            xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &xReconnectParams, uxRand(), &usNextRetryBackOff );

            if( xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted ){
                LogError( ( "Connection to the broker failed, all attempts exhausted." ) );
            }
            else if( xBackoffAlgStatus == BackoffAlgorithmSuccess )
            {
                LogWarn( ( "Connection to the broker failed. "
                           "Retrying connection with backoff and jitter." ) );
                vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );
            }
        }
    } while( ( xNetworkStatus != PLAINTEXT_TRANSPORT_SUCCESS ) && ( xBackoffAlgStatus == BackoffAlgorithmSuccess ) );

    return xNetworkStatus;
}


// Function to create connection to MQTT broker
static void prvCreateMQTTConnectionWithBroker( MQTTContext_t * pxMQTTContext,
                                               NetworkContext_t * pxNetworkContext ){
    MQTTStatus_t xResult;
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent;
    TransportInterface_t xTransport;

    // Set up the transport interface with the provided network context and send/recv functions
    xTransport.pNetworkContext = pxNetworkContext;
    xTransport.send = Plaintext_FreeRTOS_send;
    xTransport.recv = Plaintext_FreeRTOS_recv;

    // Initialize MQTT with the configured transport interface and other parameters
    xResult = MQTT_Init( pxMQTTContext, &xTransport, prvGetTimeMs, NULL, &xBuffer );
    configASSERT( xResult == MQTTSuccess );

    // Clear the connect information structure
    ( void ) memset( ( void * ) &xConnectInfo, 0x00, sizeof( xConnectInfo ) );

    // Setting username & password & clientID required by MQTT broker
    xConnectInfo.cleanSession = true;
    xConnectInfo.pUserName = "SECRET";
    xConnectInfo.userNameLength = strlen("SECRET");
    xConnectInfo.pPassword = "SECRET";
    xConnectInfo.passwordLength = strlen("SECRET");

    xConnectInfo.pClientIdentifier = CLIENT_IDENTIFIER;
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( CLIENT_IDENTIFIER );


    xConnectInfo.keepAliveSeconds = mqttKEEP_ALIVE_TIMEOUT_SECONDS;

    // Connect to MQTT broker
    xResult = MQTT_Connect( pxMQTTContext,
                            &xConnectInfo,
                            NULL,
                            mqttCONNACK_RECV_TIMEOUT_MS,
                            &xSessionPresent );
    configASSERT( xResult == MQTTSuccess );
}




// Function to publish
static void prvMQTTPublishToTopic( MQTTContext_t * pxMQTTContext ){
    MQTTStatus_t xResult;
    MQTTPublishInfo_t xMQTTPublishInfo;


    char msg[100];
    uint16_t co2ppm = 65501;
    uint16_t humidity = 0;
    uint16_t temperature = 0;
    int tries = 0;

    // Checking if sensor value is valid. If modbus read fails, it will return: 655001
    while(co2ppm > 65500  && tries < 5){
    	co2ppm = Co2::readSensor();
		vTaskDelay(10);
		tries++;
    }

    // Same sensor validity check
    tries = 0;
   	while(humidity == 0 && tries < 5){
   		humidity = HumTemp::readHumidity() / 10;
   		vTaskDelay(10);
   		tries++;
   	}
    // Same sensor validity check
    tries = 0;
   	while(temperature == 0 && tries < 5){
		temperature = HumTemp::readTemperature() / 10;
		vTaskDelay(10);
		tries++;
   	}

    int setpoint = Co2::getSetpoint();
    int sensor = Co2::readSensor();
    int Valve;

    // Calculating wether to send relay open / close to broker
	if (!(sensor <= 0)) {
		if (setpoint > sensor) {
			Valve = 100;
		} else {
			Valve = 0;
		}
	}

	// Building the message
    snprintf(msg, sizeof(msg), "field1=%d&field2=%d&field3=%d&field4=%d&field5=%d", co2ppm, humidity, temperature, Valve, setpoint);
    int len = strlen(msg);

    // Clearing publish info
    ( void ) memset( ( void * ) &xMQTTPublishInfo, 0x00, sizeof( xMQTTPublishInfo ) );

    // Set up MQTT publish information and publish a message to a specified topic
    xMQTTPublishInfo.qos = MQTTQoS0;                  // Quality of Service: 0 (deliver at most once)
    xMQTTPublishInfo.retain = false;                   // Retain flag: false (message is not retained by the broker)
    xMQTTPublishInfo.pTopicName = mqttTOPIC;
    xMQTTPublishInfo.topicNameLength = (uint16_t)strlen(mqttTOPIC);

    xMQTTPublishInfo.pPayload = msg;
    xMQTTPublishInfo.payloadLength = len;

    // Publish the message to the MQTT broker using the provided MQTT context and publish information
    xResult = MQTT_Publish(pxMQTTContext, &xMQTTPublishInfo, 0U);

    // Ensure that the MQTT publish operation was successful
    configASSERT(xResult == MQTTSuccess);
}





static uint32_t prvGetTimeMs( void ){
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    xTickCount = xTaskGetTickCount();
    ulTimeMs = ( uint32_t ) xTickCount * MILLISECONDS_PER_TICK;
    ulTimeMs = ( uint32_t ) ( ulTimeMs - ulGlobalEntryTimeMs );

    return ulTimeMs;
}

/*------------------------------------------------------------------------------*/

/* The following is required if runtime statistics are to be collected */
extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}


}

void lcdTask(void *params)
{
	(void) params;

	retarget_init();

    //Initialize LCD screen pins
	DigitalIoPin *rs = new DigitalIoPin(0, 29, DigitalIoPin::output);
	DigitalIoPin *en = new DigitalIoPin(0, 9, DigitalIoPin::output);
	DigitalIoPin *d4 = new DigitalIoPin(0, 10, DigitalIoPin::output);
	DigitalIoPin *d5 = new DigitalIoPin(0, 16, DigitalIoPin::output);
	DigitalIoPin *d6 = new DigitalIoPin(1, 3, DigitalIoPin::output);
	DigitalIoPin *d7 = new DigitalIoPin(0, 0, DigitalIoPin::output);
	LiquidCrystal *lcd = new LiquidCrystal(rs, en, d4, d5, d6, d7);

	lcd->begin(16, 2);

	while(true){
		char buffer[32];
		uint16_t co2ppm = Co2::readSensor();
		vTaskDelay(20);

        //Filter wrong values that sometimes occur
		if(co2ppm < 65500 && co2ppm > 0){
			snprintf(buffer, 32, "CO2=%d  ", co2ppm);
			printf("%s\n",buffer);
			lcd->setCursor(0, 0);
			lcd->print(buffer);
		}
		uint16_t humidity = HumTemp::readHumidity() / 10;
		vTaskDelay(20);

		if (humidity > 0){
			snprintf(buffer, 32, "RH=%d%%  ", humidity);
			printf("%s\n",buffer);
			lcd->setCursor(9, 0);
			lcd->print(buffer);
		}
		int setpoint = Co2::getSetpoint();

		snprintf(buffer, 32, "SP=%d  ", setpoint);
		printf("%s\n",buffer);
		lcd->setCursor(0, 1);
		lcd->print(buffer);

		uint16_t temperature = HumTemp::readTemperature() / 10;
		if(temperature > 0){
			snprintf(buffer, 32, "T=%dC  ", temperature);
			printf("%s\n",buffer);
			lcd->setCursor(9, 1);
			lcd->print(buffer);
		}
		vTaskDelay(LCD_TASK_DELAY);
	}
}

void StartMQTT( void );

int main(void) {
#if defined (__USE_LPCOPEN)
    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();
#if !defined(NO_BOARD_LIB)
    // Set up and initialize all required blocks and
    // functions related to the board hardware
    Board_Init();
    // Set the LED to the state of "On"
    Board_LED_Set(0, true);
#endif
#endif

    //Initialize CO2, humidity, temperature and heap monitor
    Co2::init();
    HumTemp::init();
	heap_monitor_setup();

    //Create tasks
	xTaskCreate(vRotaryTask, "RotaryTask",
			configMINIMAL_STACK_SIZE * 4, NULL, (tskIDLE_PRIORITY + ROT_TASK_PRIORITY),
			(TaskHandle_t *) NULL);

	xTaskCreate(vRelayTask, "RelayTask",
			configMINIMAL_STACK_SIZE * 4, NULL, (tskIDLE_PRIORITY + REL_TASK_PRIORITY),
			(TaskHandle_t *) NULL);

	xTaskCreate(lcdTask, "LCDTask",
		configMINIMAL_STACK_SIZE * 4, NULL, (tskIDLE_PRIORITY + LCD_TASK_PRIORITY),
		(TaskHandle_t *) NULL);

	StartMQTT();
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}
