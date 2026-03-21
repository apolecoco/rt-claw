/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * WiFi provisioning — SoftAP + captive portal + web config page.
 * Shared across all Espressif SoCs (ESP32-C3, ESP32-S3, etc.).
 */

#include "wifi_provision.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "osal/claw_os.h"
#include "osal/claw_kv.h"
#include "claw_config.h"
#include "claw/shell/shell_commands.h"
#include "drivers/net/espressif/wifi_manager.h"

#define TAG "provision"

#define PROVISION_AP_CHANNEL    1
#define PROVISION_AP_MAX_CONN   4
#define DNS_PORT                53
#define DNS_TASK_STACK          3072
#define DNS_TASK_PRIO           5
#define REBOOT_DELAY_MS         3000
#define MAX_SCAN_APS            16
#define HTTP_RECV_BUF_SIZE      1024

/* ---- State ---- */

static bool           s_active;
static esp_netif_t   *s_ap_netif;
static httpd_handle_t s_httpd;
static TaskHandle_t   s_dns_task;
static volatile bool  s_dns_running;
static SemaphoreHandle_t s_dns_ready;

/* ---- Embedded HTML page ---- */

static const char s_html_page[] =
"<!DOCTYPE html><html><head>"
"<meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>RT-Claw Setup</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;"
"padding:16px;max-width:480px;margin:0 auto}"
"h1{text-align:center;color:#0f9;font-size:1.4em;margin:12px 0}"
"h2{font-size:1em;color:#0cf;margin:16px 0 8px;cursor:pointer}"
"h2::before{content:'\\25B6 ';font-size:.7em}"
"h2.open::before{content:'\\25BC '}"
".section{background:#16213e;border-radius:8px;padding:12px;margin:8px 0}"
".collapsible{display:none}.collapsible.open{display:block}"
"label{display:block;font-size:.85em;color:#aaa;margin:8px 0 2px}"
"input,select{width:100%;padding:8px;border:1px solid #333;"
"border-radius:4px;background:#0d1b2a;color:#fff;font-size:.9em}"
"select{appearance:none}"
".row{display:flex;gap:8px;align-items:end}"
".row input{flex:1}.row button{flex:0 0 auto;padding:8px 12px}"
"button{background:#0f9;color:#000;border:none;border-radius:4px;"
"padding:10px;font-size:1em;cursor:pointer;width:100%;margin-top:16px}"
"button:hover{background:#0da}button:disabled{background:#555;color:#999}"
".btn-scan{background:#0cf;width:auto;margin:0}"
"#status{text-align:center;margin:12px 0;font-size:.9em}"
".ok{color:#0f9}.err{color:#f55}"
"</style></head><body>"
"<h1>RT-Claw Setup</h1>"
"<div class='section'>"
"<h2 class='open' onclick='toggle(this)'>WiFi</h2>"
"<div class='collapsible open' id='sec-wifi'>"
"<label>SSID</label>"
"<select id='ap-sel' onchange='document.getElementById(\"ssid\").value="
"this.value'><option value=''>Scanning...</option></select>"
"<input id='ssid' placeholder='Or type SSID manually' style='margin-top:4px'>"
"<label>Password</label>"
"<input id='pass' type='password' placeholder='WiFi password'>"
"</div></div>"
"<div class='section'>"
"<h2 onclick='toggle(this)'>AI</h2>"
"<div class='collapsible' id='sec-ai'>"
"<label>API Key</label>"
"<input id='ai_key' type='password' placeholder='sk-...'>"
"<label>API URL</label>"
"<input id='ai_url' placeholder='https://...'>"
"<label>Model</label>"
"<input id='ai_model' placeholder='claude-sonnet-4-6'>"
"</div></div>"
"<div class='section'>"
"<h2 onclick='toggle(this)'>Feishu</h2>"
"<div class='collapsible' id='sec-feishu'>"
"<label>App ID</label>"
"<input id='feishu_id' placeholder='cli_...'>"
"<label>App Secret</label>"
"<input id='feishu_secret' type='password'>"
"</div></div>"
"<div class='section'>"
"<h2 onclick='toggle(this)'>Telegram</h2>"
"<div class='collapsible' id='sec-tg'>"
"<label>Bot Token</label>"
"<input id='tg_token' type='password'>"
"</div></div>"
"<button onclick='doSave()'>Save &amp; Reboot</button>"
"<div id='status'></div>"
"<script>"
"function toggle(el){"
"el.classList.toggle('open');"
"el.nextElementSibling.classList.toggle('open')}"
"function doScan(){"
"let sel=document.getElementById('ap-sel');"
"sel.innerHTML='<option value=\"\">Scanning...</option>';"
"fetch('/api/scan').then(r=>r.json()).then(d=>{"
"sel.innerHTML='<option value=\"\">-- Select WiFi --</option>';"
"d.forEach(a=>{let o=document.createElement('option');"
"o.value=a.ssid;o.textContent=a.ssid+' ('+a.rssi+'dBm, CH'+a.ch+')';"
"sel.appendChild(o)});"
"if(!d.length)sel.innerHTML='<option value=\"\">No APs found</option>';"
"}).catch(e=>{sel.innerHTML='<option value=\"\">Scan failed</option>'})}"
"function doSave(){"
"let b=document.querySelector('button:last-of-type');"
"b.disabled=true;b.textContent='Saving...';"
"let body=JSON.stringify({"
"wifi_ssid:document.getElementById('ssid').value,"
"wifi_pass:document.getElementById('pass').value,"
"ai_key:document.getElementById('ai_key').value,"
"ai_url:document.getElementById('ai_url').value,"
"ai_model:document.getElementById('ai_model').value,"
"feishu_id:document.getElementById('feishu_id').value,"
"feishu_secret:document.getElementById('feishu_secret').value,"
"tg_token:document.getElementById('tg_token').value"
"});"
"fetch('/api/save',{method:'POST',headers:{'Content-Type':"
"'application/json'},body:body}).then(r=>r.json()).then(d=>{"
"if(d.ok){showStatus('Saved! Rebooting in 3s...','ok')}"
"else{showStatus('Error: '+d.error,'err');b.disabled=false;"
"b.textContent='Save & Reboot'}"
"}).catch(e=>{showStatus('Save failed','err');"
"b.disabled=false;b.textContent='Save & Reboot'})}"
"function showStatus(msg,cls){"
"let s=document.getElementById('status');"
"s.textContent=msg;s.className=cls}"
"fetch('/api/config').then(r=>r.json()).then(d=>{"
"if(d.wifi_ssid)document.getElementById('ssid').value=d.wifi_ssid;"
"if(d.ai_url)document.getElementById('ai_url').value=d.ai_url;"
"if(d.ai_model)document.getElementById('ai_model').value=d.ai_model;"
"if(d.ai_key)document.getElementById('ai_key').placeholder=d.ai_key;"
"if(d.feishu_id)document.getElementById('feishu_id').value=d.feishu_id;"
"if(d.feishu_secret)document.getElementById('feishu_secret')"
".placeholder=d.feishu_secret;"
"if(d.tg_token)document.getElementById('tg_token')"
".placeholder=d.tg_token;"
"}).catch(e=>{});"
"doScan();"
"</script></body></html>";

/*
 * Extract a JSON string value for a given key from a JSON object string.
 * Minimal parser — handles flat {"key":"value",...} only.
 * Returns 0 on success, -1 if key not found.
 */
static int json_get_str(const char *json, const char *key,
                        char *buf, size_t buf_size)
{
    char pattern[64];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof(pattern)) {
        return -1;
    }

    const char *p = strstr(json, pattern);
    if (!p) {
        return -1;
    }
    p += strlen(pattern);

    /* skip whitespace and colon */
    while (*p == ' ' || *p == ':') {
        p++;
    }
    if (*p != '"') {
        return -1;
    }
    p++; /* skip opening quote */

    size_t i = 0;
    while (*p && *p != '"' && i < buf_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            if (*p == 'n') {
                buf[i++] = '\n';
            } else if (*p == 't') {
                buf[i++] = '\t';
            } else if (*p == 'u' && *(p + 1) && *(p + 2) &&
                       *(p + 3) && *(p + 4)) {
                /* Skip unicode escapes, just store '?' */
                buf[i++] = '?';
                p += 4;
            } else {
                buf[i++] = *p;
            }
        } else {
            buf[i++] = *p;
        }
        p++;
    }
    buf[i] = '\0';
    return 0;
}

/* ---- JSON string escaping ---- */

/*
 * Write a JSON-safe escaped version of src into dst.
 * Escapes ", \, and control characters (< 0x20).
 */
static void json_escape_str(const char *src, char *dst, size_t dst_size)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di < dst_size - 1; si++) {
        unsigned char c = (unsigned char)src[si];
        if (c == '"' || c == '\\') {
            if (di + 2 >= dst_size) {
                break;
            }
            dst[di++] = '\\';
            dst[di++] = (char)c;
        } else if (c < 0x20) {
            /* Escape control chars as \uXXXX */
            if (di + 6 >= dst_size) {
                break;
            }
            di += (size_t)snprintf(dst + di, dst_size - di,
                                   "\\u%04x", c);
        } else {
            dst[di++] = (char)c;
        }
    }
    dst[di] = '\0';
}

/* ---- Mask secrets for display ---- */

static void mask_secret(const char *src, char *dst, size_t dst_size)
{
    size_t len = strlen(src);
    if (len == 0) {
        dst[0] = '\0';
        return;
    }
    if (len <= 6) {
        snprintf(dst, dst_size, "***");
    } else {
        snprintf(dst, dst_size, "%.3s***%.3s",
                 src, src + len - 3);
    }
}

/* ---- DNS captive portal ---- */

/*
 * Minimal DNS responder: answer every A query with 192.168.4.1.
 * Runs as a FreeRTOS task with a UDP socket on port 53.
 */
static void dns_server_task(void *arg)
{
    SemaphoreHandle_t ready_sem = (SemaphoreHandle_t)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket failed");
        xSemaphoreGive(ready_sem);  /* signal failure (s_dns_running stays false) */
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS bind failed");
        close(sock);
        xSemaphoreGive(ready_sem);
        vTaskDelete(NULL);
        return;
    }

    /* Bind succeeded — signal ready to caller */
    s_dns_running = true;
    xSemaphoreGive(ready_sem);

    /* Set receive timeout so we can check s_dns_running */
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t buf[512];
    struct sockaddr_in client;
    socklen_t client_len;

    ESP_LOGI(TAG, "DNS captive portal started on port %d", DNS_PORT);

    while (s_dns_running) {
        client_len = sizeof(client);
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&client, &client_len);
        if (len < 12) {
            continue;  /* too short or timeout */
        }

        /*
         * Build a minimal DNS response:
         * - Copy transaction ID from query (bytes 0-1)
         * - Set flags: standard response, no error (0x8180)
         * - QDCOUNT=1, ANCOUNT=1, NSCOUNT=0, ARCOUNT=0
         * - Copy the original question section
         * - Append a single A record answer pointing to 192.168.4.1
         */
        uint8_t resp[512];
        int resp_len = 0;

        /* Header */
        resp[0] = buf[0]; /* Transaction ID */
        resp[1] = buf[1];
        resp[2] = 0x81;   /* Flags: QR=1, AA=1 */
        resp[3] = 0x80;
        resp[4] = 0x00;   /* QDCOUNT = 1 */
        resp[5] = 0x01;
        resp[6] = 0x00;   /* ANCOUNT = 1 */
        resp[7] = 0x01;
        resp[8] = 0x00;   /* NSCOUNT = 0 */
        resp[9] = 0x00;
        resp[10] = 0x00;  /* ARCOUNT = 0 */
        resp[11] = 0x00;
        resp_len = 12;

        /* Copy question section from query */
        int q_start = 12;
        int q_pos = q_start;
        while (q_pos < len && buf[q_pos] != 0) {
            q_pos += buf[q_pos] + 1;
        }
        q_pos += 1 + 4; /* null byte + QTYPE(2) + QCLASS(2) */
        if (q_pos > len) {
            continue;
        }
        int q_len = q_pos - q_start;
        if (resp_len + q_len + 16 > (int)sizeof(resp)) {
            continue;
        }
        memcpy(resp + resp_len, buf + q_start, q_len);
        resp_len += q_len;

        /* Answer: name pointer (0xC00C), type A, class IN, TTL=60, 4 bytes */
        resp[resp_len++] = 0xC0; /* Name pointer to offset 12 */
        resp[resp_len++] = 0x0C;
        resp[resp_len++] = 0x00; /* Type A */
        resp[resp_len++] = 0x01;
        resp[resp_len++] = 0x00; /* Class IN */
        resp[resp_len++] = 0x01;
        resp[resp_len++] = 0x00; /* TTL = 60 seconds */
        resp[resp_len++] = 0x00;
        resp[resp_len++] = 0x00;
        resp[resp_len++] = 0x3C;
        resp[resp_len++] = 0x00; /* RDLENGTH = 4 */
        resp[resp_len++] = 0x04;
        resp[resp_len++] = 192;  /* 192.168.4.1 */
        resp[resp_len++] = 168;
        resp[resp_len++] = 4;
        resp[resp_len++] = 1;

        sendto(sock, resp, resp_len, 0,
               (struct sockaddr *)&client, client_len);
    }

    close(sock);
    ESP_LOGI(TAG, "DNS captive portal stopped");
    vTaskDelete(NULL);
}

/* ---- HTTP handlers ---- */

static esp_err_t http_root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, s_html_page, sizeof(s_html_page) - 1);
}

static esp_err_t http_scan_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > MAX_SCAN_APS) {
        ap_count = MAX_SCAN_APS;
    }

    wifi_ap_record_t *ap_list = NULL;
    if (ap_count > 0) {
        ap_list = calloc(ap_count, sizeof(wifi_ap_record_t));
        if (!ap_list) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "[]");
            return ESP_OK;
        }
        uint16_t max = ap_count;
        esp_wifi_scan_get_ap_records(&max, ap_list);
        ap_count = max;
    }

    /* Build JSON array with escaped SSIDs */
    char *json = malloc(ap_count * 128 + 4);
    if (!json) {
        free(ap_list);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }

    char escaped_ssid[96];
    int pos = 0;
    json[pos++] = '[';
    for (uint16_t i = 0; i < ap_count; i++) {
        if (i > 0) {
            json[pos++] = ',';
        }
        json_escape_str((const char *)ap_list[i].ssid,
                        escaped_ssid, sizeof(escaped_ssid));
        pos += snprintf(json + pos, 128,
                        "{\"ssid\":\"%s\",\"rssi\":%d,\"ch\":%d}",
                        escaped_ssid,
                        ap_list[i].rssi,
                        ap_list[i].primary);
    }
    json[pos++] = ']';
    json[pos] = '\0';

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    free(json);
    free(ap_list);
    return ESP_OK;
}

/*
 * Get effective config value: KV persistent value first, then
 * compile-time default. Returns the value in buf.
 */
static void get_effective_str(const char *ns, const char *key,
                              const char *compile_default,
                              char *buf, size_t size)
{
    if (claw_kv_get_str(ns, key, buf, size) == CLAW_OK &&
        buf[0] != '\0') {
        return;
    }
    if (compile_default && compile_default[0] != '\0') {
        strncpy(buf, compile_default, size - 1);
        buf[size - 1] = '\0';
    } else {
        buf[0] = '\0';
    }
}

/* WiFi SSID fallback defaults from Kconfig */
#ifndef CONFIG_RTCLAW_WIFI_SSID
#define CONFIG_RTCLAW_WIFI_SSID ""
#endif

static esp_err_t http_config_handler(httpd_req_t *req)
{
    char buf[128];
    char escaped[256];
    char masked[32];
    char json[768];
    int pos = 0;

    pos += snprintf(json + pos, sizeof(json) - pos, "{");

    /* WiFi SSID: NVS → Kconfig default */
    get_effective_str("wifi_config", "ssid",
                      CONFIG_RTCLAW_WIFI_SSID,
                      buf, sizeof(buf));
    json_escape_str(buf, escaped, sizeof(escaped));
    pos += snprintf(json + pos, sizeof(json) - pos,
                    "\"wifi_ssid\":\"%s\"", escaped);

    /* AI API key: KV → compile-time default (masked) */
    get_effective_str(SHELL_NVS_NS_AI, "api_key",
                      CONFIG_RTCLAW_AI_API_KEY,
                      buf, sizeof(buf));
    if (buf[0] != '\0') {
        mask_secret(buf, masked, sizeof(masked));
    } else {
        masked[0] = '\0';
    }
    json_escape_str(masked, escaped, sizeof(escaped));
    pos += snprintf(json + pos, sizeof(json) - pos,
                    ",\"ai_key\":\"%s\"", escaped);

    /* AI API URL: KV → compile-time default */
    get_effective_str(SHELL_NVS_NS_AI, "api_url",
                      CONFIG_RTCLAW_AI_API_URL,
                      buf, sizeof(buf));
    json_escape_str(buf, escaped, sizeof(escaped));
    pos += snprintf(json + pos, sizeof(json) - pos,
                    ",\"ai_url\":\"%s\"", escaped);

    /* AI model: KV → compile-time default */
    get_effective_str(SHELL_NVS_NS_AI, "model",
                      CONFIG_RTCLAW_AI_MODEL,
                      buf, sizeof(buf));
    json_escape_str(buf, escaped, sizeof(escaped));
    pos += snprintf(json + pos, sizeof(json) - pos,
                    ",\"ai_model\":\"%s\"", escaped);

    /* Feishu app ID: KV → compile-time default */
    get_effective_str(SHELL_NVS_NS_FEISHU, "app_id",
                      CONFIG_RTCLAW_FEISHU_APP_ID,
                      buf, sizeof(buf));
    json_escape_str(buf, escaped, sizeof(escaped));
    pos += snprintf(json + pos, sizeof(json) - pos,
                    ",\"feishu_id\":\"%s\"", escaped);

    /* Feishu secret: KV → compile-time default (masked) */
    get_effective_str(SHELL_NVS_NS_FEISHU, "app_secret",
                      CONFIG_RTCLAW_FEISHU_APP_SECRET,
                      buf, sizeof(buf));
    if (buf[0] != '\0') {
        mask_secret(buf, masked, sizeof(masked));
    } else {
        masked[0] = '\0';
    }
    json_escape_str(masked, escaped, sizeof(escaped));
    pos += snprintf(json + pos, sizeof(json) - pos,
                    ",\"feishu_secret\":\"%s\"", escaped);

    /* Telegram bot token: KV → compile-time default (masked) */
    get_effective_str(SHELL_NVS_NS_TELEGRAM, "bot_token",
                      CONFIG_RTCLAW_TELEGRAM_BOT_TOKEN,
                      buf, sizeof(buf));
    if (buf[0] != '\0') {
        mask_secret(buf, masked, sizeof(masked));
    } else {
        masked[0] = '\0';
    }
    json_escape_str(masked, escaped, sizeof(escaped));
    pos += snprintf(json + pos, sizeof(json) - pos,
                    ",\"tg_token\":\"%s\"", escaped);

    pos += snprintf(json + pos, sizeof(json) - pos, "}");

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

/* Reboot timer callback */
static void reboot_timer_cb(void *arg)
{
    (void)arg;
    esp_restart();
}

static esp_err_t http_save_handler(httpd_req_t *req)
{
    size_t content_len = req->content_len;
    if (content_len == 0 || content_len >= HTTP_RECV_BUF_SIZE) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req,
            "{\"ok\":false,\"error\":\"invalid body size\"}");
        return ESP_OK;
    }

    char *body = malloc(content_len + 1);
    if (!body) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"out of memory\"}");
        return ESP_OK;
    }

    /* Read full body, looping for fragmented TCP */
    size_t received = 0;
    while (received < content_len) {
        int ret = httpd_req_recv(req, body + received,
                                 content_len - received);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            free(body);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req,
                "{\"ok\":false,\"error\":\"recv failed\"}");
            return ESP_OK;
        }
        received += (size_t)ret;
    }
    body[content_len] = '\0';

    char val[256];

    /* WiFi SSID (mandatory) */
    if (json_get_str(body, "wifi_ssid", val, sizeof(val)) != 0 ||
        val[0] == '\0') {
        free(body);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req,
            "{\"ok\":false,\"error\":\"WiFi SSID is required\"}");
        return ESP_OK;
    }

    char wifi_ssid[33];
    strncpy(wifi_ssid, val, sizeof(wifi_ssid) - 1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';

    char wifi_pass[65] = "";
    if (json_get_str(body, "wifi_pass", val, sizeof(val)) == 0) {
        strncpy(wifi_pass, val, sizeof(wifi_pass) - 1);
        wifi_pass[sizeof(wifi_pass) - 1] = '\0';
    }

    /* Save WiFi via NVS (existing API) */
    esp_err_t wifi_err = wifi_manager_set_credentials(wifi_ssid,
                                                       wifi_pass);
    if (wifi_err != ESP_OK) {
        free(body);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req,
            "{\"ok\":false,\"error\":\"WiFi save failed\"}");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "WiFi credentials saved: %s", wifi_ssid);

    /* Save optional fields via OSAL KV (only non-empty values) */
    int kv_err = 0;

    if (json_get_str(body, "ai_key", val, sizeof(val)) == 0 &&
        val[0] != '\0') {
        if (claw_kv_set_str(SHELL_NVS_NS_AI, "api_key", val) != CLAW_OK) {
            kv_err++;
        }
    }
    if (json_get_str(body, "ai_url", val, sizeof(val)) == 0 &&
        val[0] != '\0') {
        if (claw_kv_set_str(SHELL_NVS_NS_AI, "api_url", val) != CLAW_OK) {
            kv_err++;
        }
    }
    if (json_get_str(body, "ai_model", val, sizeof(val)) == 0 &&
        val[0] != '\0') {
        if (claw_kv_set_str(SHELL_NVS_NS_AI, "model", val) != CLAW_OK) {
            kv_err++;
        }
    }
    if (json_get_str(body, "feishu_id", val, sizeof(val)) == 0 &&
        val[0] != '\0') {
        if (claw_kv_set_str(SHELL_NVS_NS_FEISHU, "app_id", val) != CLAW_OK) {
            kv_err++;
        }
    }
    if (json_get_str(body, "feishu_secret", val, sizeof(val)) == 0 &&
        val[0] != '\0') {
        if (claw_kv_set_str(SHELL_NVS_NS_FEISHU, "app_secret", val) != CLAW_OK) {
            kv_err++;
        }
    }
    if (json_get_str(body, "tg_token", val, sizeof(val)) == 0 &&
        val[0] != '\0') {
        if (claw_kv_set_str(SHELL_NVS_NS_TELEGRAM, "bot_token", val) != CLAW_OK) {
            kv_err++;
        }
    }

    free(body);

    if (kv_err > 0) {
        ESP_LOGE(TAG, "%d KV write(s) failed", kv_err);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req,
            "{\"ok\":false,\"error\":\"config save partially failed\"}");
        return ESP_OK;
    }

    /* Schedule reboot before responding — verify timer works */
    const esp_timer_create_args_t timer_args = {
        .callback = reboot_timer_cb,
        .name = "reboot",
    };
    esp_timer_handle_t timer = NULL;
    esp_err_t t_err = esp_timer_create(&timer_args, &timer);
    if (t_err != ESP_OK) {
        ESP_LOGE(TAG, "reboot timer create failed: %s",
                 esp_err_to_name(t_err));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req,
            "{\"ok\":false,\"error\":\"reboot schedule failed\"}");
        return ESP_OK;
    }

    t_err = esp_timer_start_once(timer, REBOOT_DELAY_MS * 1000);
    if (t_err != ESP_OK) {
        ESP_LOGE(TAG, "reboot timer start failed: %s",
                 esp_err_to_name(t_err));
        esp_timer_delete(timer);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req,
            "{\"ok\":false,\"error\":\"reboot schedule failed\"}");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "rebooting in %d ms", REBOOT_DELAY_MS);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");

    return ESP_OK;
}

/* Captive portal redirect handler */
static esp_err_t http_captive_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

/* ---- HTTP server start/stop ---- */

static esp_err_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 4096;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    static const httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = http_root_handler,
    };
    static const httpd_uri_t uri_scan = {
        .uri = "/api/scan",
        .method = HTTP_GET,
        .handler = http_scan_handler,
    };
    static const httpd_uri_t uri_config = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = http_config_handler,
    };
    static const httpd_uri_t uri_save = {
        .uri = "/api/save",
        .method = HTTP_POST,
        .handler = http_save_handler,
    };
    static const httpd_uri_t uri_204 = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = http_captive_handler,
    };
    static const httpd_uri_t uri_hotspot = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = http_captive_handler,
    };
    static const httpd_uri_t uri_connecttest = {
        .uri = "/connecttest.txt",
        .method = HTTP_GET,
        .handler = http_captive_handler,
    };

    const httpd_uri_t *uris[] = {
        &uri_root, &uri_scan, &uri_config, &uri_save,
        &uri_204, &uri_hotspot, &uri_connecttest,
    };
    for (int i = 0; i < (int)(sizeof(uris) / sizeof(uris[0])); i++) {
        err = httpd_register_uri_handler(s_httpd, uris[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "URI register failed for %s: %s",
                     uris[i]->uri, esp_err_to_name(err));
            httpd_stop(s_httpd);
            s_httpd = NULL;
            return err;
        }
    }

    ESP_LOGI(TAG, "HTTP config server started on port 80");
    return ESP_OK;
}

/* ---- Public API ---- */

esp_err_t wifi_provision_start(void)
{
    if (s_active) {
        ESP_LOGW(TAG, "provisioning already active");
        return ESP_ERR_INVALID_STATE;
    }

    /* Create AP netif (only once) */
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (!s_ap_netif) {
            ESP_LOGE(TAG, "AP netif creation failed");
            return ESP_FAIL;
        }
    }

    /* Switch to APSTA mode */
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set APSTA mode failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    /* Generate AP SSID with MAC suffix */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid),
             "RT-Claw-%02X%02X", mac[4], mac[5]);

    wifi_config_t ap_cfg = {
        .ap = {
            .channel = PROVISION_AP_CHANNEL,
            .max_connection = PROVISION_AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    strncpy((char *)ap_cfg.ap.ssid, ap_ssid,
            sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = strlen(ap_ssid);

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AP config failed: %s", esp_err_to_name(ret));
        esp_wifi_set_mode(WIFI_MODE_STA);
        return ret;
    }

    /* Start WiFi in APSTA mode (may already be started for STA) */
    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        esp_wifi_set_mode(WIFI_MODE_STA);
        return ret;
    }

    /* Start DNS captive portal with startup handshake */
    s_dns_running = false;
    s_dns_ready = xSemaphoreCreateBinary();
    if (!s_dns_ready) {
        ESP_LOGE(TAG, "DNS semaphore creation failed");
        esp_wifi_set_mode(WIFI_MODE_STA);
        return ESP_FAIL;
    }

    BaseType_t dns_ok = xTaskCreate(dns_server_task, "dns_captive",
                                    DNS_TASK_STACK, s_dns_ready,
                                    DNS_TASK_PRIO, &s_dns_task);
    if (dns_ok != pdPASS) {
        ESP_LOGE(TAG, "DNS task creation failed");
        vSemaphoreDelete(s_dns_ready);
        s_dns_ready = NULL;
        esp_wifi_set_mode(WIFI_MODE_STA);
        return ESP_FAIL;
    }

    /* Wait up to 3s for DNS bind to succeed */
    if (xSemaphoreTake(s_dns_ready, pdMS_TO_TICKS(3000)) != pdTRUE ||
        !s_dns_running) {
        ESP_LOGE(TAG, "DNS server failed to start");
        s_dns_running = false;
        vSemaphoreDelete(s_dns_ready);
        s_dns_ready = NULL;
        esp_wifi_set_mode(WIFI_MODE_STA);
        return ESP_FAIL;
    }
    vSemaphoreDelete(s_dns_ready);
    s_dns_ready = NULL;

    /* Start HTTP config server */
    esp_err_t err = start_http_server();
    if (err != ESP_OK) {
        s_dns_running = false;
        esp_wifi_set_mode(WIFI_MODE_STA);
        return err;
    }

    s_active = true;
    ESP_LOGW(TAG, "WiFi provisioning started");
    ESP_LOGW(TAG, "Connect to '%s' and open http://192.168.4.1",
             ap_ssid);

    return ESP_OK;
}

esp_err_t wifi_provision_stop(void)
{
    if (!s_active) {
        return ESP_OK;
    }

    /* Stop HTTP server */
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }

    /* Stop DNS task and wait for it to exit */
    if (s_dns_task) {
        s_dns_running = false;
        /* DNS task has 1s recv timeout; wait up to 3s for cleanup */
        for (int i = 0; i < 30; i++) {
            if (eTaskGetState(s_dns_task) == eDeleted) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        s_dns_task = NULL;
    }

    /* Restore STA-only mode and stop AP */
    esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_LOGI(TAG, "WiFi mode restored to STA");

    s_active = false;
    ESP_LOGI(TAG, "WiFi provisioning stopped");
    return ESP_OK;
}

bool wifi_provision_is_active(void)
{
    return s_active;
}
