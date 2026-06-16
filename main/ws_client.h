#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start WebSocket client, connect to cloud server.
 * Sends {"type":"text","text":"..."} messages.
 * Receives tts_audio_chunk PCM and plays through I2S.
 */
void ws_client_start(const char *uri);

/**
 * Send a text message to the cloud LLM.
 * Wraps in {"type":"text","text":"..."} JSON.
 */
void ws_client_send_text(const char *text);

/**
 * Send a pre-built JSON string as-is (no wrapping).
 * Use for audio, control messages etc. that are already valid JSON.
 */
void ws_client_send_raw(const char *json_str);

#ifdef __cplusplus
}
#endif
