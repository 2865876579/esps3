#include "ws_client.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "cJSON.h"
#include "audio_out.h"
#include "opus.h"

static const char *TAG = "ws_client";

static esp_websocket_client_handle_t s_client = NULL;
static OpusDecoder *s_opus_dec = NULL;

#define OPUS_SAMPLE_RATE    16000
#define OPUS_CHANNELS       1
#define OPUS_MAX_FRAME_SIZE (960 * 3)  // max 3 frames packed

static void handle_binary_audio(const uint8_t *data, int len)
{
    if (!s_opus_dec) {
        int err = 0;
        s_opus_dec = opus_decoder_create(OPUS_SAMPLE_RATE, OPUS_CHANNELS, &err);
        if (err != OPUS_OK || !s_opus_dec) {
            ESP_LOGE(TAG, "Opus decoder create failed: %d", err);
            return;
        }
    }

    // Decode one Opus frame → 960 samples mono int16
    int16_t pcm[OPUS_MAX_FRAME_SIZE];
    int samples = opus_decode(s_opus_dec, data, len, pcm, OPUS_MAX_FRAME_SIZE, 0);
    if (samples < 0) {
        ESP_LOGE(TAG, "Opus decode error: %d", samples);
        return;
    }

    // Expand mono to stereo for MAX98357A I2S
    int16_t stereo[samples * 2];
    for (int i = 0; i < samples; i++) {
        stereo[i * 2]     = pcm[i];
        stereo[i * 2 + 1] = pcm[i];
    }
    audio_out_write((const uint8_t *)stereo, samples * 4);
}

static void ws_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected to cloud");
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket disconnected");
        if (s_opus_dec) {
            opus_decoder_destroy(s_opus_dec);
            s_opus_dec = NULL;
        }
        break;

    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x02) {
            // Binary frame: Opus audio
            handle_binary_audio((const uint8_t *)data->data_ptr, data->data_len);
        } else if (data->op_code == 0x01 && data->data_len > 0) {
            // Text frame: JSON metadata
            cJSON *json = cJSON_ParseWithLength(data->data_ptr, data->data_len);
            if (!json) break;

            cJSON *type = cJSON_GetObjectItem(json, "type");
            if (type && cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "tts_audio_start") == 0) {
                    cJSON *text = cJSON_GetObjectItem(json, "text");
                    ESP_LOGI(TAG, "TTS start: %s",
                             (text && cJSON_IsString(text)) ? text->valuestring : "");
                } else if (strcmp(type->valuestring, "tts_audio_end") == 0) {
                    cJSON *chunks = cJSON_GetObjectItem(json, "chunks");
                    ESP_LOGI(TAG, "TTS end (%d frames)", chunks ? chunks->valueint : 0);
                    printf("回复完毕\n");
                } else if (strcmp(type->valuestring, "pong") == 0) {
                    // heartbeat
                }
            }
            cJSON_Delete(json);
        } else if (data->op_code == 0x08) {
            // Close frame
            ESP_LOGW(TAG, "Server closed connection");
        } else if (data->op_code == 0x0A) {
            // Pong frame, ignore
        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket error");
        break;

    default:
        break;
    }
}

void ws_client_start(const char *uri)
{
    esp_websocket_client_config_t ws_cfg = {
        .uri = uri,
        .buffer_size = 16384,
        .reconnect_timeout_ms = 10000,
        .network_timeout_ms = 15000,
    };

    s_client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
    esp_websocket_client_start(s_client);

    ESP_LOGI(TAG, "WebSocket client starting, uri=%s", uri);
}

void ws_client_send_text(const char *text)
{
    if (!s_client || !esp_websocket_client_is_connected(s_client)) {
        ESP_LOGW(TAG, "WebSocket not connected, cannot send");
        return;
    }

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", "text");
    cJSON_AddStringToObject(msg, "text", text);
    char *json_str = cJSON_PrintUnformatted(msg);

    esp_websocket_client_send_text(s_client, json_str, strlen(json_str), portMAX_DELAY);
    ESP_LOGI(TAG, "Sent: %s", json_str);

    free(json_str);
    cJSON_Delete(msg);
}
