#ifndef AUDIO_H
#define AUDIO_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"

// task handle for audio_capture_task
TaskHandle_t audio_capture_task_handle = NULL;
// task handle for audio_playback_task
TaskHandle_t audio_playback_task_handle = NULL;

void init_audio(StreamBufferHandle_t mic_stream_buf, StreamBufferHandle_t network_stream_buf);

#endif
