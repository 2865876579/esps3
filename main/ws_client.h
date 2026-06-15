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
 */
void ws_client_send_text(const char *text);

#ifdef __cplusplus
}
#endif
