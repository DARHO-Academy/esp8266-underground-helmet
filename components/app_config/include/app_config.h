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
 * 0 = read real sensors.
 * 1 = use fixed demo readings for UI/testing without hardware.
 */
#define APP_SENSOR_USE_TEST_VALUES  0

/*
 * ESP8266 has one real analog input: A0.
 * In this wiring, flame sensor uses analog A0.
 * MQ135 gas sensor uses digital DO on a GPIO.
 */
#define APP_FLAME_ADC_CHANNEL       0
#define APP_GAS_DIGITAL_GPIO        12

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
 * Alarm is the buzzer sound.
 */
#define APP_LED_GAS_GPIO            16
#define APP_LED_FLAME_GPIO          0
#define APP_LED_HEAT_INDEX_GPIO     2
#define APP_LED_DISTANCE_GPIO       15

#define APP_BUZZER_GPIO             13

/*
 * Danger thresholds.
 * Gas is digital, so firmware maps gas danger to 1023 and normal to 0.
 * Flame remains analog on A0.
 */
#define APP_GAS_WARNING_LEVEL           600
#define APP_FLAME_WARNING_LEVEL         500
#define APP_HEAT_INDEX_WARNING_LEVEL    40
#define APP_DISTANCE_WARNING_CM         30

/*
 * Digital MQ module logic:
 * 1 = gas warning when DO reads LOW
 * 0 = gas warning when DO reads HIGH
 *
 * Most MQ135 LM393 modules output LOW when the adjusted gas threshold is crossed.
 */
#define APP_GAS_DIGITAL_ACTIVE_LOW      1

/*
 * Flame analog logic:
 * 0 = flame warning when analog value is LOWER than threshold
 * 1 = flame warning when analog value is HIGHER than threshold
 *
 * Many flame modules give lower analog value when flame is near.
 */
#define APP_FLAME_HIGHER_IS_DANGER      0

/*
 * Timing
 */
#define APP_SENSOR_READ_DELAY_MS    2000
#define APP_WEB_UPDATE_DELAY_MS     1000

/*
 * Persistent reading log.
 * Old rows are removed automatically when this limit is reached.
 */
#define APP_LOG_MAX_LINES           100

#endif
