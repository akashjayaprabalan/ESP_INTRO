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
 * ESP-IDF version of the four-LED web controller.
 *
 * The Arduino sketch is the recommended Nano ESP32 workflow in this repo, and
 * this C entry point is kept as a matching LED-only fallback.
 */

#define WIFI_AP_SSID "ESP32_LED_CONTROL"
#define WIFI_AP_PASSWORD "ledcontrol"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4

#define LED_COUNT 4

static const char *TAG = "led_controller";

#define RETURN_IF_HTTP_ERROR(expression) do { \
    esp_err_t http_result = (expression); \
    if (http_result != ESP_OK) { \
        ESP_LOGW(TAG, "HTTP response error: %s", esp_err_to_name(http_result)); \
        return http_result; \
    } \
} while (0)

typedef struct {
    const char *name;
    const char *label;
    gpio_num_t pin;
    bool is_on;
} led_output_t;

static led_output_t leds[LED_COUNT] = {
    { "red", "Red", GPIO_NUM_16, false },
    { "blue", "Blue", GPIO_NUM_17, false },
    { "yellow", "Yellow", GPIO_NUM_18, false },
    { "green", "Green", GPIO_NUM_19, false },
};

static httpd_handle_t web_server = NULL;

static uint64_t build_led_pin_mask(void);
static void init_nvs(void);
static void init_gpio_outputs(void);
static void write_led(int index);
static void set_led_state(int index, bool on);
static void all_leds_off(void);
static int find_led_index(const char *name);
static void start_wifi_ap(void);
static httpd_handle_t start_web_server(void);
static esp_err_t send_button(httpd_req_t *req, const char *color, bool on, const char *label, const char *class_name);
static esp_err_t send_control_page(httpd_req_t *req);
static esp_err_t redirect_to_home(httpd_req_t *req);
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t led_get_handler(httpd_req_t *req);
static esp_err_t off_get_handler(httpd_req_t *req);

static uint64_t build_led_pin_mask(void)
{
    uint64_t pin_mask = 0;

    for (int i = 0; i < LED_COUNT; i++) {
        pin_mask |= (1ULL << leds[i].pin);
    }

    return pin_mask;
}

static void init_nvs(void)
{
    esp_err_t result = nvs_flash_init();

    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

static void init_gpio_outputs(void)
{
    gpio_config_t output_config = {
        .pin_bit_mask = build_led_pin_mask(),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&output_config));
    all_leds_off();
    ESP_LOGI(TAG, "LED outputs initialized");
}

static void write_led(int index)
{
    if (index < 0 || index >= LED_COUNT) {
        return;
    }

    gpio_set_level(leds[index].pin, leds[index].is_on ? 1 : 0);
}

static void set_led_state(int index, bool on)
{
    if (index < 0 || index >= LED_COUNT) {
        ESP_LOGW(TAG, "Rejected invalid LED index: %d", index);
        return;
    }

    leds[index].is_on = on;
    write_led(index);

    ESP_LOGI(TAG, "%s LED %s on GPIO %d", leds[index].label, on ? "ON" : "OFF", leds[index].pin);
}

static void all_leds_off(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        leds[i].is_on = false;
        write_led(i);
    }

    ESP_LOGI(TAG, "All LEDs OFF");
}

static int find_led_index(const char *name)
{
    for (int i = 0; i < LED_COUNT; i++) {
        if (strcmp(name, leds[i].name) == 0) {
            return i;
        }
    }

    return -1;
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
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &led_route));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &off_route));

    ESP_LOGI(TAG, "HTTP web server started");
    return server;
}

static esp_err_t send_button(httpd_req_t *req, const char *color, bool on, const char *label, const char *class_name)
{
    char button_html[192];
    int written = snprintf(
        button_html,
        sizeof(button_html),
        "<a class=\"button %s\" href=\"/led?color=%s&on=%d\">%s</a>",
        class_name,
        color,
        on ? 1 : 0,
        label);

    if (written < 0 || written >= (int)sizeof(button_html)) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Button HTML buffer too small");
    }

    return httpd_resp_sendstr_chunk(req, button_html);
}

static esp_err_t send_control_page(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");

    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req,
        "<!DOCTYPE html>"
        "<html lang=\"en\">"
        "<head>"
        "<meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>ESP32 LED Control</title>"
        "<style>"
        ":root{font-family:Arial,sans-serif;color:#17202a;background:#f4f7fb;}"
        "*{box-sizing:border-box;}"
        "body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px;}"
        "main{width:min(520px,100%);}"
        "h1{font-size:1.65rem;margin:0 0 18px;text-align:center;}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;}"
        ".panel{padding:12px;border-radius:8px;background:#fff;border:1px solid #d9e2ec;}"
        ".title{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;font-weight:700;}"
        ".state{font-size:.8rem;color:#52606d;}"
        ".actions{display:grid;grid-template-columns:1fr 1fr;gap:8px;}"
        ".button{display:flex;align-items:center;justify-content:center;min-height:48px;padding:12px;border-radius:8px;text-decoration:none;color:#fff;font-weight:700;}"
        ".on{background:#059669;}"
        ".off{background:#dc2626;}"
        ".all-off{width:100%;margin-top:18px;background:#111827;min-height:58px;}"
        "@media(max-width:380px){.grid{grid-template-columns:1fr;}h1{font-size:1.4rem;}}"
        "</style>"
        "</head>"
        "<body>"
        "<main>"
        "<h1>ESP32 LED Control</h1>"
        "<div class=\"grid\">"));

    for (int i = 0; i < LED_COUNT; i++) {
        RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "<div class=\"panel\"><div class=\"title\"><span>"));
        RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, leds[i].label));
        RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "</span><span class=\"state\">"));
        RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, leds[i].is_on ? "ON" : "OFF"));
        RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "</span></div><div class=\"actions\">"));
        RETURN_IF_HTTP_ERROR(send_button(req, leds[i].name, true, "ON", "on"));
        RETURN_IF_HTTP_ERROR(send_button(req, leds[i].name, false, "OFF", "off"));
        RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "</div></div>"));
    }

    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "</div>"));
    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "<a class=\"button all-off\" href=\"/off\">ALL OFF</a>"));
    RETURN_IF_HTTP_ERROR(httpd_resp_sendstr_chunk(req, "</main></body></html>"));
    return httpd_resp_sendstr_chunk(req, NULL);
}

static esp_err_t redirect_to_home(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    return send_control_page(req);
}

static esp_err_t led_get_handler(httpd_req_t *req)
{
    char query[96];
    char color[16];
    char on_value[4];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "color", color, sizeof(color)) != ESP_OK ||
        httpd_query_key_value(query, "on", on_value, sizeof(on_value)) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Use color and on query parameters.");
    }

    int led_index = find_led_index(color);
    if (led_index < 0 || (strcmp(on_value, "0") != 0 && strcmp(on_value, "1") != 0)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid LED request.");
    }

    set_led_state(led_index, strcmp(on_value, "1") == 0);
    return redirect_to_home(req);
}

static esp_err_t off_get_handler(httpd_req_t *req)
{
    all_leds_off();
    return redirect_to_home(req);
}

void setup(void)
{
    ESP_LOGI(TAG, "Starting ESP32 LED controller");
    init_nvs();
    init_gpio_outputs();
    start_wifi_ap();
    web_server = start_web_server();
    ESP_LOGI(TAG, "System ready");
}

void loop(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void)
{
    setup();

    while (web_server != NULL) {
        loop();
    }
}
