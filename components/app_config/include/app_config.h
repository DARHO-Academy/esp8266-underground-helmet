#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_PROJECT_NAME        "Underground Helmet"
#define APP_FIRMWARE_VERSION    "1.0.0"

/*
 * Wi-Fi mode:
 * STA mode means ESP8266 connects to normal Wi-Fi/router.
 * Phone or PC must be connected to the same Wi-Fi network.
 */
#define APP_WIFI_MODE_STA       1
#define APP_WIFI_MODE_AP        0

#define APP_WIFI_STA_SSID       "patrick"
#define APP_WIFI_STA_PASSWORD   "12345678"
#define APP_WIFI_MAX_RETRY      5

/*
 * Optional AP values.
 * Keep these only for later fallback mode.
 */
#define APP_WIFI_AP_SSID        "UNDERGROUND_HELMET"
#define APP_WIFI_AP_PASSWORD    "12345678"
#define APP_WIFI_AP_CHANNEL     1
#define APP_WIFI_AP_MAX_CONN    4

/*
 * Sensor input mode:
 * 0 = do not show fake readings; dashboard will show sensors as not connected
 *     until real sensor drivers are added.
 * 1 = use fixed demo readings for UI/testing without hardware.
 */
#define APP_SENSOR_USE_TEST_VALUES  0

/*
 * ESP8266 has only one real analog input: A0.
 * APP_GAS_ADC_CHANNEL 0 means internal A0.
 * APP_FLAME_ADC_CHANNEL 1 means external ADC channel 1.
 *
 * If you do not use external ADC/multiplexer, gas and flame cannot both be
 * analog at the same time on ESP8266.
 */
#define APP_GAS_ADC_CHANNEL         0
#define APP_FLAME_ADC_CHANNEL       1

/*
 * Default digital sensor pins.
 */
#define APP_DHT_GPIO                5
#define APP_ULTRASONIC_TRIG_GPIO    4
#define APP_ULTRASONIC_ECHO_GPIO    14

/*
 * Four LED outputs:
 * 1. Gas LED
 * 2. Flame LED
 * 3. Heat Index LED
 * 4. Distance LED
 *
 * Alarm is NOT a LED.
 * Alarm is the buzzer sound.
 */
#define APP_LED_GAS_GPIO            16
#define APP_LED_FLAME_GPIO          0
#define APP_LED_HEAT_INDEX_GPIO     2
#define APP_LED_DISTANCE_GPIO       12

#define APP_BUZZER_GPIO             13

/*
 * Danger thresholds.
 * Temperature and humidity are not separate LEDs anymore.
 * They create one heat index value and one heat index warning LED.
 */
#define APP_GAS_WARNING_LEVEL           600
#define APP_FLAME_WARNING_LEVEL         500
#define APP_HEAT_INDEX_WARNING_LEVEL    40
#define APP_DISTANCE_WARNING_CM         30

/*
 * Flame sensor logic:
 * 0 = flame warning when analog value is LOWER than threshold
 * 1 = flame warning when analog value is HIGHER than threshold
 *
 * Many flame modules give lower analog value when flame is near.
 * Test your module and change this if needed.
 */
#define APP_FLAME_HIGHER_IS_DANGER      0

/*
 * Timing
 */
#define APP_SENSOR_READ_DELAY_MS    2000
#define APP_WEB_UPDATE_DELAY_MS     1000

#endif