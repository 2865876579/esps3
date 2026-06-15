#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "wifi.h"
#include "audio_out.h"
#include "ws_client.h"

// ===== Wi-Fi =====
#define WIFI_SSID  "qwe"
#define WIFI_PASS  "12345678"

// ===== Cloud server =====
#define WS_URI     "ws://39.106.190.124:8000/ws/esp32"

static const char *TAG = "app";
#define UART_BUF_SIZE 256

static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== 智能枕头固件启动 ===");

    uart_init();
    audio_out_init();

    if (wifi_connect(WIFI_SSID, WIFI_PASS) != 0) {
        ESP_LOGE(TAG, "联网失败");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    ESP_LOGI(TAG, "联网成功");

    // Connect to cloud WebSocket
    ws_client_start(WS_URI);

    // Wait a bit for WebSocket to connect
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "发送 'rec' 触发语音对话");

    uint8_t rx_buf[UART_BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM_0, rx_buf, UART_BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            rx_buf[len] = '\0';
            char *cmd = (char *)rx_buf;
            while (*cmd == '\r' || *cmd == '\n') cmd++;
            char *end = cmd + strlen(cmd) - 1;
            while (end > cmd && (*end == '\r' || *end == '\n')) { *end = '\0'; end--; }

            if (strcmp(cmd, "rec") == 0) {
                ESP_LOGI(TAG, "发送文字到云端...");
                ws_client_send_text("你好");
            } else if (strlen(cmd) > 0) {
                // Any other text: send directly to LLM
                ESP_LOGI(TAG, "发送: %s", cmd);
                ws_client_send_text(cmd);
            }
        }
    }
}
