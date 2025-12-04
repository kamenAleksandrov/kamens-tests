/* ======================= SIMPLE HTTP SERVER ======================= */
/*
 * This file runs a tiny HTTP server with toy endpoints for LED and string control.
 * By keeping it separate we reduce clutter in the main logic file.
 */

#include "WEB_Server.h"

#include <string.h>

#include "esp_log.h"
#include "esp_http_server.h"

#include "LED_Controler.h"
#include "Storage_Manager.h"

#include "esp_mac.h"

static const char *TAG = "web";

/* Helper: send a simple HTTP response with plain text. */
static esp_err_t send_text_response(httpd_req_t *req, const char *text)
{
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
}

/* ========== ROOT HANDLER ("/") ========== */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *html =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>ESP32 Control</title></head>\n"
        "<body>\n"
        "<h1>ESP32 LED and String Control</h1>\n"
        "<p>LED is currently: %s</p>\n"
        "<p>\n"
        "  <a href=\"/led?state=on\">Turn LED ON</a><br>\n"
        "  <a href=\"/led?state=off\">Turn LED OFF</a>\n"
        "</p>\n"
        "<p>Stored string: '%s'</p>\n"
        "<p>\n"
        "  <form method=\"POST\" action=\"/string\">\n"
        "    New string: <input type=\"text\" name=\"value\">\n"
        "    <input type=\"submit\" value=\"Save\">\n"
        "  </form>\n"
        "</p>\n"
        "<p>\n"
        "  <form method=\"POST\" action=\"/string?delete=1\">\n"
        "    <input type=\"submit\" value=\"Delete string\">\n"
        "  </form>\n"
        "</p>\n"
        "</body>\n"
        "</html>\n";

    char response[768];
    snprintf(response, sizeof(response), html,
             led_control_is_on() ? "ON" : "OFF",
             storage_manager_get_string()[0] ? storage_manager_get_string() : "(empty)");

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

/* ========== LED HANDLER ("/led") ========== */
static esp_err_t led_get_handler(httpd_req_t *req)
{
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        char state[8];
        if (httpd_query_key_value(query, "state", state, sizeof(state)) == ESP_OK)
        {
            if (strcmp(state, "on") == 0)
            {
                led_control_set(1);
                return send_text_response(req, "LED turned ON\n");
            }
            else if (strcmp(state, "off") == 0)
            {
                led_control_set(0);
                return send_text_response(req, "LED turned OFF\n");
            }
        }
    }
    return send_text_response(req, "Use /led?state=on or /led?state=off\n");
}

/* ========== STRING GET HANDLER ("/string", GET) ========== */
static esp_err_t string_get_handler(httpd_req_t *req)
{
    if (storage_manager_get_string()[0] == '\0')
    {
        return send_text_response(req, "(empty)\n");
    }
    return send_text_response(req, storage_manager_get_string());
}

/* ========== STRING POST HANDLER ("/string", POST) ========== */
static esp_err_t string_post_handler(httpd_req_t *req)
{
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        char del[8];
        if (httpd_query_key_value(query, "delete", del, sizeof(del)) == ESP_OK)
        {
            if (strcmp(del, "1") == 0)
            {
                storage_manager_delete_string();
                return send_text_response(req, "String deleted\n");
            }
        }
    }

    char buf[64];
    int total_len = req->content_len;
    int received = 0;

    if (total_len >= (int)sizeof(buf))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "String too long");
        return ESP_FAIL;
    }

    while (received < total_len)
    {
        int r = httpd_req_recv(req, buf + received, total_len - received);
        if (r <= 0)
        {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        received += r;
    }

    buf[received] = '\0';

    const char *value_prefix = "value=";
    const size_t prefix_len = strlen(value_prefix);
    const char *value_ptr = NULL;

    if (strncmp(buf, value_prefix, prefix_len) == 0)
    {
        value_ptr = buf + prefix_len;
    }
    else
    {
        value_ptr = buf;
    }

    storage_manager_save_string(value_ptr);
    return send_text_response(req, "String saved\n");
}

/* ========== STRING DELETE HANDLER ("/string", DELETE) ========== */
static esp_err_t string_delete_handler(httpd_req_t *req)
{
    storage_manager_delete_string();
    return send_text_response(req, "String deleted\n");
}

void web_server_start(void)
{
    static httpd_handle_t server = NULL;
    if (server != NULL)
    {
        ESP_LOGI(TAG, "HTTP server already running");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t led_uri = {
        .uri = "/led",
        .method = HTTP_GET,
        .handler = led_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &led_uri);

    httpd_uri_t string_get_uri = {
        .uri = "/string",
        .method = HTTP_GET,
        .handler = string_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &string_get_uri);

    httpd_uri_t string_post_uri = {
        .uri = "/string",
        .method = HTTP_POST,
        .handler = string_post_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &string_post_uri);

    httpd_uri_t string_delete_uri = {
        .uri = "/string",
        .method = HTTP_DELETE,
        .handler = string_delete_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &string_delete_uri);

    ESP_LOGI(TAG, "HTTP server started");
}
