#include "app_system.h"
#include "app_config.h"

#include "esp_log.h"
#include "nvs.h"

#include <string.h>

static const char *TAG = "APP_SYSTEM";

#define NVS_NAMESPACE              "app_sys"

/* Threshold keys */
#define NVS_KEY_GAS                "gas_warn"
#define NVS_KEY_FLAME              "flame_warn"
#define NVS_KEY_HEAT_INDEX         "heat_warn"
#define NVS_KEY_DISTANCE           "dist_warn"

/* Pin keys */
#define NVS_KEY_GAS_ADC            "gas_adc"
#define NVS_KEY_FLAME_ADC          "flame_adc"
#define NVS_KEY_DHT_GPIO           "dht_gpio"
#define NVS_KEY_TRIG_GPIO          "trig_gpio"
#define NVS_KEY_ECHO_GPIO          "echo_gpio"
#define NVS_KEY_LED_GAS            "led_gas"
#define NVS_KEY_LED_FLAME          "led_flame"
#define NVS_KEY_LED_HEAT_INDEX     "led_heat"
#define NVS_KEY_LED_DISTANCE       "led_dist"
#define NVS_KEY_BUZZER             "buzzer"

static app_system_state_t system_state;
static app_system_thresholds_t thresholds;
static app_system_pin_config_t pin_config;

/*
 * Heat index calculation.
 * Input: Celsius and humidity percent.
 * Output: Celsius.
 */
static float app_system_compute_heat_index_c(float temperature_c, float humidity_percent)
{
    if (temperature_c < 27.0f || humidity_percent < 40.0f) {
        return temperature_c;
    }

    float temperature_f = (temperature_c * 9.0f / 5.0f) + 32.0f;
    float rh = humidity_percent;

    float heat_index_f =
        -42.379f +
        2.04901523f * temperature_f +
        10.14333127f * rh -
        0.22475541f * temperature_f * rh -
        0.00683783f * temperature_f * temperature_f -
        0.05481717f * rh * rh +
        0.00122874f * temperature_f * temperature_f * rh +
        0.00085282f * temperature_f * rh * rh -
        0.00000199f * temperature_f * temperature_f * rh * rh;

    return (heat_index_f - 32.0f) * 5.0f / 9.0f;
}

static void load_default_thresholds(void)
{
    thresholds.gas_warning_level = APP_GAS_WARNING_LEVEL;
    thresholds.flame_warning_level = APP_FLAME_WARNING_LEVEL;
    thresholds.heat_index_warning_level = APP_HEAT_INDEX_WARNING_LEVEL;
    thresholds.distance_warning_cm = APP_DISTANCE_WARNING_CM;
}

static void load_default_pins(void)
{
    pin_config.gas_adc_channel = APP_GAS_ADC_CHANNEL;
    pin_config.flame_adc_channel = APP_FLAME_ADC_CHANNEL;

    pin_config.dht_gpio = APP_DHT_GPIO;
    pin_config.ultrasonic_trig_gpio = APP_ULTRASONIC_TRIG_GPIO;
    pin_config.ultrasonic_echo_gpio = APP_ULTRASONIC_ECHO_GPIO;

    pin_config.led_gas_gpio = APP_LED_GAS_GPIO;
    pin_config.led_flame_gpio = APP_LED_FLAME_GPIO;
    pin_config.led_heat_index_gpio = APP_LED_HEAT_INDEX_GPIO;
    pin_config.led_distance_gpio = APP_LED_DISTANCE_GPIO;

    pin_config.buzzer_gpio = APP_BUZZER_GPIO;
}

static void load_thresholds_from_nvs(void)
{
    load_default_thresholds();

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved thresholds in NVS yet, using defaults");
        return;
    }

    int32_t value;

    if (nvs_get_i32(handle, NVS_KEY_GAS, &value) == ESP_OK) {
        thresholds.gas_warning_level = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_FLAME, &value) == ESP_OK) {
        thresholds.flame_warning_level = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_HEAT_INDEX, &value) == ESP_OK) {
        thresholds.heat_index_warning_level = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_DISTANCE, &value) == ESP_OK) {
        thresholds.distance_warning_cm = (int)value;
    }

    nvs_close(handle);

    ESP_LOGI(
        TAG,
        "Loaded thresholds: gas=%d flame=%d heat_index=%d distance=%d",
        thresholds.gas_warning_level,
        thresholds.flame_warning_level,
        thresholds.heat_index_warning_level,
        thresholds.distance_warning_cm
    );
}

static void load_pins_from_nvs(void)
{
    load_default_pins();

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved pin config in NVS yet, using defaults");
        return;
    }

    int32_t value;

    if (nvs_get_i32(handle, NVS_KEY_GAS_ADC, &value) == ESP_OK) {
        pin_config.gas_adc_channel = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_FLAME_ADC, &value) == ESP_OK) {
        pin_config.flame_adc_channel = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_DHT_GPIO, &value) == ESP_OK) {
        pin_config.dht_gpio = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_TRIG_GPIO, &value) == ESP_OK) {
        pin_config.ultrasonic_trig_gpio = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_ECHO_GPIO, &value) == ESP_OK) {
        pin_config.ultrasonic_echo_gpio = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_LED_GAS, &value) == ESP_OK) {
        pin_config.led_gas_gpio = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_LED_FLAME, &value) == ESP_OK) {
        pin_config.led_flame_gpio = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_LED_HEAT_INDEX, &value) == ESP_OK) {
        pin_config.led_heat_index_gpio = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_LED_DISTANCE, &value) == ESP_OK) {
        pin_config.led_distance_gpio = (int)value;
    }

    if (nvs_get_i32(handle, NVS_KEY_BUZZER, &value) == ESP_OK) {
        pin_config.buzzer_gpio = (int)value;
    }

    nvs_close(handle);

    ESP_LOGI(
        TAG,
        "Loaded pins: gas_adc=%d flame_adc=%d dht=%d trig=%d echo=%d led_gas=%d led_flame=%d led_heat=%d led_dist=%d buzzer=%d",
        pin_config.gas_adc_channel,
        pin_config.flame_adc_channel,
        pin_config.dht_gpio,
        pin_config.ultrasonic_trig_gpio,
        pin_config.ultrasonic_echo_gpio,
        pin_config.led_gas_gpio,
        pin_config.led_flame_gpio,
        pin_config.led_heat_index_gpio,
        pin_config.led_distance_gpio,
        pin_config.buzzer_gpio
    );
}

static bool save_thresholds_to_nvs(const app_system_thresholds_t *new_thresholds)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for threshold writing");
        return false;
    }

    bool ok = true;

    ok &= (nvs_set_i32(handle, NVS_KEY_GAS, new_thresholds->gas_warning_level) == ESP_OK);
    ok &= (nvs_set_i32(handle, NVS_KEY_FLAME, new_thresholds->flame_warning_level) == ESP_OK);
    ok &= (nvs_set_i32(handle, NVS_KEY_HEAT_INDEX, new_thresholds->heat_index_warning_level) == ESP_OK);
    ok &= (nvs_set_i32(handle, NVS_KEY_DISTANCE, new_thresholds->distance_warning_cm) == ESP_OK);

    if (ok) {
        ok = (nvs_commit(handle) == ESP_OK);
    }

    nvs_close(handle);

    return ok;
}

static bool save_pins_to_nvs(const app_system_pin_config_t *pins)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for pin writing");
        return false;
    }

    bool ok = true;

    ok &= (nvs_set_i32(handle, NVS_KEY_GAS_ADC, pins->gas_adc_channel) == ESP_OK);
    ok &= (nvs_set_i32(handle, NVS_KEY_FLAME_ADC, pins->flame_adc_channel) == ESP_OK);

    ok &= (nvs_set_i32(handle, NVS_KEY_DHT_GPIO, pins->dht_gpio) == ESP_OK);
    ok &= (nvs_set_i32(handle, NVS_KEY_TRIG_GPIO, pins->ultrasonic_trig_gpio) == ESP_OK);
    ok &= (nvs_set_i32(handle, NVS_KEY_ECHO_GPIO, pins->ultrasonic_echo_gpio) == ESP_OK);

    ok &= (nvs_set_i32(handle, NVS_KEY_LED_GAS, pins->led_gas_gpio) == ESP_OK);
    ok &= (nvs_set_i32(handle, NVS_KEY_LED_FLAME, pins->led_flame_gpio) == ESP_OK);
    ok &= (nvs_set_i32(handle, NVS_KEY_LED_HEAT_INDEX, pins->led_heat_index_gpio) == ESP_OK);
    ok &= (nvs_set_i32(handle, NVS_KEY_LED_DISTANCE, pins->led_distance_gpio) == ESP_OK);

    ok &= (nvs_set_i32(handle, NVS_KEY_BUZZER, pins->buzzer_gpio) == ESP_OK);

    if (ok) {
        ok = (nvs_commit(handle) == ESP_OK);
    }

    nvs_close(handle);

    return ok;
}

static bool gpio_is_valid(int gpio)
{
    return gpio >= 0 && gpio <= 16;
}

static bool adc_channel_is_valid(int channel)
{
    /*
     * ESP8266 internal ADC is channel 0 only.
     * Channels above 0 are allowed here only for future external ADC/multiplexer logic.
     */
    return channel >= 0 && channel <= 7;
}

static bool digital_pin_used_twice(const app_system_pin_config_t *pins)
{
    int values[] = {
        pins->dht_gpio,
        pins->ultrasonic_trig_gpio,
        pins->ultrasonic_echo_gpio,
        pins->led_gas_gpio,
        pins->led_flame_gpio,
        pins->led_heat_index_gpio,
        pins->led_distance_gpio,
        pins->buzzer_gpio
    };

    int count = sizeof(values) / sizeof(values[0]);

    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (values[i] == values[j]) {
                return true;
            }
        }
    }

    return false;
}

static bool pin_config_is_valid(const app_system_pin_config_t *pins)
{
    if (pins == NULL) {
        return false;
    }

    if (!adc_channel_is_valid(pins->gas_adc_channel)) {
        return false;
    }

    if (!adc_channel_is_valid(pins->flame_adc_channel)) {
        return false;
    }

    if (!gpio_is_valid(pins->dht_gpio)) {
        return false;
    }

    if (!gpio_is_valid(pins->ultrasonic_trig_gpio)) {
        return false;
    }

    if (!gpio_is_valid(pins->ultrasonic_echo_gpio)) {
        return false;
    }

    if (!gpio_is_valid(pins->led_gas_gpio)) {
        return false;
    }

    if (!gpio_is_valid(pins->led_flame_gpio)) {
        return false;
    }

    if (!gpio_is_valid(pins->led_heat_index_gpio)) {
        return false;
    }

    if (!gpio_is_valid(pins->led_distance_gpio)) {
        return false;
    }

    if (!gpio_is_valid(pins->buzzer_gpio)) {
        return false;
    }

    if (digital_pin_used_twice(pins)) {
        return false;
    }

    return true;
}

static void app_system_evaluate_warnings(void)
{
    system_state.heat_index_c = app_system_compute_heat_index_c(
        system_state.temperature_c,
        system_state.humidity_percent
    );

    system_state.gas_warning =
        system_state.gas_connected &&
        system_state.gas_raw >= thresholds.gas_warning_level;

#if APP_FLAME_HIGHER_IS_DANGER
    system_state.flame_warning =
        system_state.flame_connected &&
        system_state.flame_raw >= thresholds.flame_warning_level;
#else
    system_state.flame_warning =
        system_state.flame_connected &&
        system_state.flame_raw <= thresholds.flame_warning_level;
#endif

    system_state.heat_index_warning =
        system_state.dht_connected &&
        system_state.heat_index_c >= (float)thresholds.heat_index_warning_level;

    system_state.distance_warning =
        system_state.ultrasonic_connected &&
        system_state.distance_cm <= (float)thresholds.distance_warning_cm;

    /*
     * Alarm is the buzzer condition.
     * If any warning is active, the buzzer should sound.
     */
    system_state.alarm_active =
        system_state.gas_warning ||
        system_state.flame_warning ||
        system_state.heat_index_warning ||
        system_state.distance_warning;
}

void app_system_init(void)
{
    memset(&system_state, 0, sizeof(system_state));

    system_state.system_alive = true;
    system_state.wifi_connected = false;

    system_state.gas_connected = false;
    system_state.flame_connected = false;
    system_state.dht_connected = false;
    system_state.ultrasonic_connected = false;

    system_state.gas_raw = 0;
    system_state.flame_raw = 0;

    system_state.temperature_c = 0.0f;
    system_state.humidity_percent = 0.0f;
    system_state.heat_index_c = 0.0f;
    system_state.distance_cm = 999.0f;

    load_thresholds_from_nvs();
    load_pins_from_nvs();

    app_system_evaluate_warnings();
}

void app_system_update_sensors(
    int gas_raw,
    int flame_raw,
    float temperature_c,
    float humidity_percent,
    float distance_cm
)
{
    system_state.gas_raw = gas_raw;
    system_state.flame_raw = flame_raw;
    system_state.temperature_c = temperature_c;
    system_state.humidity_percent = humidity_percent;
    system_state.distance_cm = distance_cm;

    system_state.update_count++;

    app_system_evaluate_warnings();
}

void app_system_set_wifi_connected(bool connected)
{
    system_state.wifi_connected = connected;
}

void app_system_update_sensor_connections(
    bool gas_connected,
    bool flame_connected,
    bool dht_connected,
    bool ultrasonic_connected
)
{
    system_state.gas_connected = gas_connected;
    system_state.flame_connected = flame_connected;
    system_state.dht_connected = dht_connected;
    system_state.ultrasonic_connected = ultrasonic_connected;

    app_system_evaluate_warnings();
}

const app_system_state_t *app_system_get_state(void)
{
    return &system_state;
}

bool app_system_is_alarm_active(void)
{
    return system_state.alarm_active;
}

const app_system_thresholds_t *app_system_get_thresholds(void)
{
    return &thresholds;
}

bool app_system_set_thresholds(const app_system_thresholds_t *new_thresholds)
{
    if (new_thresholds == NULL) {
        return false;
    }

    if (new_thresholds->gas_warning_level < 0 || new_thresholds->gas_warning_level > 1023) {
        return false;
    }

    if (new_thresholds->flame_warning_level < 0 || new_thresholds->flame_warning_level > 1023) {
        return false;
    }

    if (new_thresholds->heat_index_warning_level < 0 || new_thresholds->heat_index_warning_level > 100) {
        return false;
    }

    if (new_thresholds->distance_warning_cm < 0 || new_thresholds->distance_warning_cm > 500) {
        return false;
    }

    if (!save_thresholds_to_nvs(new_thresholds)) {
        ESP_LOGE(TAG, "Failed to save thresholds to NVS");
        return false;
    }

    thresholds = *new_thresholds;

    app_system_evaluate_warnings();

    ESP_LOGI(
        TAG,
        "Thresholds updated: gas=%d flame=%d heat_index=%d distance=%d",
        thresholds.gas_warning_level,
        thresholds.flame_warning_level,
        thresholds.heat_index_warning_level,
        thresholds.distance_warning_cm
    );

    return true;
}

const app_system_pin_config_t *app_system_get_pins(void)
{
    return &pin_config;
}

bool app_system_set_pins(const app_system_pin_config_t *pins)
{
    if (!pin_config_is_valid(pins)) {
        ESP_LOGE(TAG, "Invalid pin configuration");
        return false;
    }

    if (!save_pins_to_nvs(pins)) {
        ESP_LOGE(TAG, "Failed to save pin configuration to NVS");
        return false;
    }

    pin_config = *pins;

    ESP_LOGI(
        TAG,
        "Pins updated: gas_adc=%d flame_adc=%d dht=%d trig=%d echo=%d led_gas=%d led_flame=%d led_heat=%d led_dist=%d buzzer=%d",
        pin_config.gas_adc_channel,
        pin_config.flame_adc_channel,
        pin_config.dht_gpio,
        pin_config.ultrasonic_trig_gpio,
        pin_config.ultrasonic_echo_gpio,
        pin_config.led_gas_gpio,
        pin_config.led_flame_gpio,
        pin_config.led_heat_index_gpio,
        pin_config.led_distance_gpio,
        pin_config.buzzer_gpio
    );

    return true;
}