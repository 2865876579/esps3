#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "wifi.h"
#include "audio_out.h"
#include "afe_wake_word.h"
#include "ws_client.h"
#include "mbedtls/base64.h"

#define WIFI_SSID  "qwe"
#define WIFI_PASS  "12345678"
#define WS_URI     "ws://39.106.190.124:8000/ws/esp32"

#define SAMPLE_RATE     16000
#define REC_DURATION_MS 4000

static const char *TAG = "app";

static volatile bool s_wake_event = false;


// ── 录音并发送云端（在主任务上下文中调用）──
static void record_and_send(void)
{
    int total = SAMPLE_RATE * REC_DURATION_MS / 1000;

    ESP_LOGI(TAG, "录音中 (%dms)...", REC_DURATION_MS);
    afe_capture_start(total);
    while (!afe_capture_is_done()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    int samples = 0;
    int16_t *pcm = afe_capture_finish(&samples);
    ESP_LOGI(TAG, "录音完成: %d samples", samples);

    if (!pcm || samples < SAMPLE_RATE / 2) {
        ESP_LOGW(TAG, "录音太短，忽略");
        free(pcm);
        return;
    }

    // base64 编码
    size_t pcm_bytes = samples * sizeof(int16_t);
    size_t b64_len = ((pcm_bytes + 2) / 3) * 4 + 8;
    char *b64 = malloc(b64_len);
    if (!b64) { free(pcm); return; }

    size_t out = 0;
    mbedtls_base64_encode((unsigned char *)b64, b64_len, &out,
                          (const unsigned char *)pcm, pcm_bytes);
    free(pcm);

    // JSON: {"type":"audio","audio":"<base64>"}
    size_t json_len = 28 + out + 2;
    char *json = malloc(json_len);
    if (!json) { free(b64); return; }

    int pos = snprintf(json, json_len, "{\"type\":\"audio\",\"audio\":\"");
    memcpy(json + pos, b64, out);
    json[pos + out] = '"';
    json[pos + out + 1] = '}';
    json[pos + out + 2] = '\0';
    free(b64);

    ESP_LOGI(TAG, "发送音频: base64=%d bytes", (int)out);
    ws_client_send_raw(json);
    free(json);
}


// ── 唤醒词回调（afe_fetch_task 中调用，只设标志）──
static void on_wake_word(void)
{
    s_wake_event = true;
}


void app_main(void)
{
    ESP_LOGI(TAG, "=== 智能枕头固件启动 ===");
    audio_out_init();

    // 串口
    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM_0, 512, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_cfg);

    // 联网
    if (wifi_connect(WIFI_SSID, WIFI_PASS) != 0) {
        ESP_LOGE(TAG, "联网失败");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    ESP_LOGI(TAG, "联网成功");

    ws_client_start(WS_URI);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 启动 AFE 唤醒词管线（"你好小智"）
    if (afe_wake_word_init(on_wake_word) != 0) {
        ESP_LOGE(TAG, "AFE 唤醒词初始化失败");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    vTaskDelay(pdMS_TO_TICKS(1500));  // 等 AFE 稳定

    ESP_LOGI(TAG, "就绪 — 说 '你好小智' 唤醒");

    uint8_t rx_buf[128];
    while (1) {
        // ── 唤醒词事件 ──
        if (s_wake_event) {
            s_wake_event = false;
            record_and_send();
        }

        // ── 串口命令 ──
        int len = uart_read_bytes(UART_NUM_0, rx_buf, sizeof(rx_buf) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            rx_buf[len] = '\0';
            char *cmd = (char *)rx_buf;
            while (*cmd == '\r' || *cmd == '\n') cmd++;
            char *end = cmd + strlen(cmd) - 1;
            while (end > cmd && (*end == '\r' || *end == '\n')) { *end = '\0'; end--; }

            if (strlen(cmd) > 0) {
                ESP_LOGI(TAG, "发送文字: %s", cmd);
                ws_client_send_text(cmd);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
