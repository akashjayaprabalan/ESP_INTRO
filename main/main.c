#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

/*
 * ESP32 WiFi-controlled haptic and LED controller.
 *
 * This is written as literal C for ESP-IDF. The structure still uses familiar
 * setup() and loop() functions so it feels approachable for Arduino learners,
 * while app_main() is the ESP-IDF entry point.
 */

#define WIFI_AP_SSID "ESP32_HAPTIC_LED"
#define WIFI_AP_PASSWORD "esp32control"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4

#define MOTOR_COUNT 4
#define LED_COUNT 4

static const char *TAG = "haptic_led";

#define RETURN_IF_HTTP_ERROR(expression) do { \
    esp_err_t http_result = (expression); \
    if (http_result != ESP_OK) { \
        ESP_LOGW(TAG, "HTTP response error: %s", esp_err_to_name(http_result)); \
        return http_result; \
    } \
} while (0)

/*
 * GPIO pins are grouped in arrays so the output-control code scales cleanly.
 * Motors must be connected through transistor or MOSFET driver circuits.
 */
static const gpio_num_t motor_pins[MOTOR_COUNT] = {
    GPIO_NUM_25,
    GPIO_NUM_26,
    GPIO_NUM_27,
    GPIO_NUM_33,
};

static const gpio_num_t led_pins[LED_COUNT] = {
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_18,
    GPIO_NUM_19,
};

static int active_motor = -1;
static int active_led = -1;
static httpd_handle_t web_server = NULL;

static uint64_t build_output_pin_mask(void);
static void init_nvs(void);
static void init_gpio_outputs(void);
static void set_all_outputs_low(void);
static void all_outputs_off(void);
static bool activate_motor(int index);
static bool activate_led(int index);
static void start_wifi_ap(void);
static httpd_handle_t start_web_server(void);
static esp_err_t send_control_page(httpd_req_t *req);
static esp_err_t redirect_to_home(httpd_req_t *req);
static bool read_index_query(httpd_req_t *req, int *index);
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t motor_get_handler(httpd_req_t *req);
static esp_err_t led_get_handler(httpd_req_t *req);
static esp_err_t off_get_handler(httpd_req_t *req);

static uint64_t build_output_pin_mask(void)
{
    uint64_t pin_mask = 0;

    for (int i = 0; i < MOTOR_COUNT; i++) {
        pin_mask |= (1ULL << motor_pins[i]);
    }

    for (int i = 0; i < LED_COUNT; i++) {
        pin_mask |= (1ULL << led_pins[i]);
    }

    return pin_mask;
}

static void init_nvs(void)
{
    esp_err_t result = nvs_flash_init();

    /*
     * WiFi stores calibration and settings in NVS. If the partition was created
     * by another ESP-IDF version, erase it and initialize again.
     */
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

static void init_gpio_outputs(void)
{
    gpio_config_t output_config = {
        .pin_bit_mask = build_output_pin_mask(),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&output_config));
    all_outputs_off();
    ESP_LOGI(TAG, "GPIO outputs initialized");
}

static void set_all_outputs_low(void)
{
    for (int i = 0; i < MOTOR_COUNT; i++) {
        gpio_set_level(motor_pins[i], 0);
    }

    for (int i = 0; i < LED_COUNT; i++) {
        gpio_set_level(led_pins[i], 0);
    }
}

static void all_outputs_off(void)
{
    set_all_outputs_low();
    active_motor = -1;
    active_led = -1;
    ESP_LOGI(TAG, "All outputs OFF");
}

static bool activate_motor(int index)
{
    if (index < 0 || index >= MOTOR_COUNT) {
        ESP_LOGW(TAG, "Rejected invalid motor index: %d", index);
        return false;
    }

    /*
     * Turn every output off first. This guarantees only the selected output is
     * active after a button press.
     */
    set_all_outputs_low();
    active_motor = index;
    active_led = -1;
    gpio_set_level(motor_pins[index], 1);

    ESP_LOGI(TAG, "Active motor: Vibrator %d on GPIO %d", index + 1, motor_pins[index]);
    return true;
}

static bool activate_led(int index)
{
    if (index < 0 || index >= LED_COUNT) {
        ESP_LOGW(TAG, "Rejected invalid LED index: %d", index);
        return false;
    }

    set_all_outputs_low();
    active_motor = -1;
    active_led = index;
    gpio_set_level(led_pins[index], 1);

    ESP_LOGI(TAG, "Active LED: LED %d on GPIO %d", index + 1, led_pins[index]);
    return true;
}

static void start_wifi_ap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASSWORD,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(WIFI_AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi access point started");
    ESP_LOGI(TAG, "SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "Password: %s", WIFI_AP_PASSWORD);
    ESP_LOGI(TAG, "Open http://192.168.4.1 from a connected phone");
}

static httpd_handle_t start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    httpd_uri_t root_route = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t motor_route = {
        .uri = "/motor",
        .method = HTTP_GET,
        .handler = motor_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t led_route = {
        .uri = "/led",
        .method = HTTP_GET,
        .handler = led_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t off_route = {
        .uri = "/off",
        .method = HTTP_GET,
        .handler = off_get_handler,
        .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_route));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &motor_route));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &led_route));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &off_route));

    ESP_LOGI(TAG, "HTTP web server started");
    return server;
}

static esp_err_t send_button(httpd_req_t *req, const char *href, const char *class_name, const char *label)
{
    char button_html[192];
    int written = snprintf(
        button_html,
        sizeof(button_html),
        "<a class=\"button %s\" href=\"%s\">%s</a>",
        class_name,
        href,
        label);

    if (written < 0 || written >= (int)sizeof(button_html)) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Button HTML buffer too small");
    }

    return httpd_resp_sendstr_chunk(req, button_html);
}

static esp_err_t send_control_page(httpd_req_t *req)
{
    char status_html[128];
    const char *status_text = "All outputs are off";

    if (active_motor >= 0) {
        snprintf(status_html, sizeof(status_html), "Vibrator %d is active", active_motor + 1);
        status_text = status_html;
    } else if (active_led >= 0) {
        snprintf(status_html, sizeof(status_html), "LED %d is active", active_led + 1);
        status_text = status_html;
    }

    httpd_resp_set_type(req, "text/html");

    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req,
        "<!DOCTYPE html>"
        "<html lang=\"en\">"
        "<head>"
        "<meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>ESP32 Haptic LED Control</title>"
        "<style>"
        ":root{font-family:Arial,sans-serif;color:#17202a;background:#f4f7fb;}"
        "*{box-sizing:border-box;}"
        "body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px;}"
        "main{width:min(480px,100%);}"
        "h1{font-size:1.65rem;margin:0 0 8px;text-align:center;}"
        ".status{margin:0 0 18px;padding:12px 14px;border-radius:8px;background:#ffffff;border:1px solid #d9e2ec;text-align:center;font-weight:700;}"
        ".section-title{font-size:.8rem;font-weight:700;letter-spacing:.08em;text-transform:uppercase;color:#52606d;margin:18px 0 10px;}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;}"
        ".button{display:flex;align-items:center;justify-content:center;min-height:62px;padding:14px;border-radius:8px;text-decoration:none;color:#fff;font-size:1.05rem;font-weight:700;box-shadow:0 6px 14px rgba(18,38,63,.16);}"
        ".motor{background:#2563eb;}"
        ".led{background:#059669;}"
        ".off{width:100%;margin-top:18px;background:#dc2626;min-height:68px;}"
        ".button:active{transform:translateY(1px);box-shadow:0 3px 8px rgba(18,38,63,.2);}"
        "@media(max-width:360px){.grid{grid-template-columns:1fr;}h1{font-size:1.4rem;}.button{font-size:1rem;}}"
        "</style>"
        "</head>"
        "<body>"
        "<main>"
        "<h1>ESP32 Control</h1>"));

    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "<div class=\"status\">"));
    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, status_text));
    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "</div>"));

    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "<div class=\"section-title\">Vibration Motors</div><div class=\"grid\">"));
    RETURN_IF_HTTP_ERROR(send_button(req, "/motor?index=0", "motor", "Vibrator 1"));
    RETURN_IF_HTTP_ERROR(send_button(req, "/motor?index=1", "motor", "Vibrator 2"));
    RETURN_IF_HTTP_ERROR(send_button(req, "/motor?index=2", "motor", "Vibrator 3"));
    RETURN_IF_HTTP_ERROR(send_button(req, "/motor?index=3", "motor", "Vibrator 4"));
    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "</div>"));

    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "<div class=\"section-title\">LEDs</div><div class=\"grid\">"));
    RETURN_IF_HTTP_ERROR(send_button(req, "/led?index=0", "led", "LED 1"));
    RETURN_IF_HTTP_ERROR(send_button(req, "/led?index=1", "led", "LED 2"));
    RETURN_IF_HTTP_ERROR(send_button(req, "/led?index=2", "led", "LED 3"));
    RETURN_IF_HTTP_ERROR(send_button(req, "/led?index=3", "led", "LED 4"));
    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "</div>"));

    RETURN_IF_HTTP_ERROR(send_button(req, "/off", "off", "ALL OFF"));
    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "</main></body></html>"));
    return httpd_resp_sendstr_chunk(req, NULL);
}

static esp_err_t redirect_to_home(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static bool read_index_query(httpd_req_t *req, int *index)
{
    char query[64];
    char value[8];
    char *end = NULL;
    long parsed_value;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }

    if (httpd_query_key_value(query, "index", value, sizeof(value)) != ESP_OK) {
        return false;
    }

    parsed_value = strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        return false;
    }

    *index = (int)parsed_value;
    return true;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    return send_control_page(req);
}

static esp_err_t motor_get_handler(httpd_req_t *req)
{
    int index = -1;

    if (!read_index_query(req, &index) || !activate_motor(index)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid motor index. Use 0 through 3.");
    }

    ESP_LOGI(TAG, "Button press: Vibrator %d", index + 1);
    return redirect_to_home(req);
}

static esp_err_t led_get_handler(httpd_req_t *req)
{
    int index = -1;

    if (!read_index_query(req, &index) || !activate_led(index)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid LED index. Use 0 through 3.");
    }

    ESP_LOGI(TAG, "Button press: LED %d", index + 1);
    return redirect_to_home(req);
}

static esp_err_t off_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Button press: ALL OFF");
    all_outputs_off();
    return redirect_to_home(req);
}

void setup(void)
{
    ESP_LOGI(TAG, "Starting ESP32 haptic and LED controller");
    init_nvs();
    init_gpio_outputs();
    start_wifi_ap();
    web_server = start_web_server();
    ESP_LOGI(TAG, "System ready");
}

void loop(void)
{
    /*
     * All control work is event-driven by HTTP requests. This delay keeps the
     * main task alive without blocking the WiFi or web-server tasks.
     */
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void)
{
    setup();

    while (web_server != NULL) {
        loop();
    }
}
