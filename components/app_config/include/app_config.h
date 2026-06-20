#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_PROJECT_NAME        "Underground Helmet"
#define APP_FIRMWARE_VERSION    "1.0.0"

/*
 * Wi-Fi mode idea:
 * First version can use ESP8266 as Wi-Fi Access Point.
 * Phone connects directly to ESP8266 Wi-Fi and opens the webpage.
 */
#define APP_WIFI_AP_SSID        "UNDERGROUND_HELMET"
#define APP_WIFI_AP_PASSWORD    "12345678"
#define APP_WIFI_AP_CHANNEL     1
#define APP_WIFI_AP_MAX_CONN    4

/*
 * Sensor pins
 * MQ gas sensor uses ESP8266 analog input A0.
 * DHT and ultrasonic use digital GPIO pins.
 */
#define APP_MQ_ADC_CHANNEL      0

#define APP_DHT_GPIO            5
#define APP_ULTRASONIC_TRIG     4
#define APP_ULTRASONIC_ECHO     14

/*
 * Output pins
 * These control LEDs and buzzer.
 */
#define APP_LED_SYSTEM_GPIO     2
#define APP_LED_WIFI_GPIO       12
#define APP_LED_GAS_GPIO        13
#define APP_LED_DHT_GPIO        15
#define APP_LED_DISTANCE_GPIO   16

#define APP_BUZZER_GPIO         0

/*
 * Sensor danger thresholds
 * These values are starting values for testing.
 * Later we tune them after real measurements.
 */
#define APP_GAS_WARNING_LEVEL       600
#define APP_TEMP_WARNING_LEVEL      35
#define APP_HUMIDITY_WARNING_LEVEL  80
#define APP_DISTANCE_WARNING_CM     30

/*
 * Timing
 */
#define APP_SENSOR_READ_DELAY_MS    2000
#define APP_WEB_UPDATE_DELAY_MS     1000

#endif