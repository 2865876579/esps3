#include "audio_out.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "audio_out";

// I2S 引脚（接 MAX98357A）
// 避免 GPIO4/5/6 可能的冲突，换到安全引脚
#define I2S_BCLK_GPIO   GPIO_NUM_15
#define I2S_LRC_GPIO    GPIO_NUM_16
#define I2S_DIN_GPIO    GPIO_NUM_17

#define SAMPLE_RATE     16000

static i2s_chan_handle_t s_tx_chan = NULL;

void audio_out_init(void)
{
    // 创建 I2S TX 通道
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx_chan, NULL));

    // 标准 I2S 配置：16kHz / 16bit / 立体声（MAX98357A 锁定更稳定）
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws   = I2S_LRC_GPIO,
            .dout = I2S_DIN_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx_chan));

    ESP_LOGI(TAG, "I2S 初始化完成 (BCLK=%d, LRC=%d, DIN=%d, %dHz)",
             I2S_BCLK_GPIO, I2S_LRC_GPIO, I2S_DIN_GPIO, SAMPLE_RATE);
}

void audio_out_write(const uint8_t *data, size_t len)
{
    if (s_tx_chan == NULL || data == NULL || len == 0) {
        return;
    }
    size_t written = 0;
    i2s_channel_write(s_tx_chan, data, len, &written, portMAX_DELAY);
}

void audio_out_play_test_tone(void)
{
    const int freq = 440;          // A4 音
    const int duration_ms = 1000;  // 1 秒
    const int total_samples = SAMPLE_RATE * duration_ms / 1000;
    const int chunk_samples = 256;

    // 立体声：每个采样点写左右两个 int16，MAX98357A 接收左声道
    int16_t buf[chunk_samples * 2];
    ESP_LOGI(TAG, "播放测试音 %dHz, %dms...", freq, duration_ms);

    int sample_idx = 0;
    while (sample_idx < total_samples) {
        int n = (total_samples - sample_idx > chunk_samples)
                    ? chunk_samples : (total_samples - sample_idx);
        for (int i = 0; i < n; i++) {
            double t = (double)(sample_idx + i) / SAMPLE_RATE;
            int16_t val = (int16_t)(0.3 * 32767.0 * sin(2.0 * M_PI * freq * t));
            buf[i * 2]     = val;  // 左声道
            buf[i * 2 + 1] = val;  // 右声道
        }
        audio_out_write((const uint8_t *)buf, n * 2 * sizeof(int16_t));
        sample_idx += n;
    }

    ESP_LOGI(TAG, "测试音播放完成");
}
