#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 I2S 输出（接 MAX98357A）。
 * 采样率固定 16kHz / 16bit / 单声道，和服务端 TTS PCM 对齐。
 * 引脚：BCLK=GPIO4, LRC(WS)=GPIO5, DIN=GPIO6
 */
void audio_out_init(void);

/**
 * 写一段 PCM 数据到 I2S（阻塞直到写完）。
 * data: 16bit 小端 PCM，单声道，16kHz
 * len:  字节数
 */
void audio_out_write(const uint8_t *data, size_t len);

/**
 * 播放一段测试正弦音（约 1 秒，440Hz），用于自测喇叭。
 */
void audio_out_play_test_tone(void);

#ifdef __cplusplus
}
#endif
