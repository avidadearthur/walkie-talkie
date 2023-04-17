#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
// #include "audio.h"
// #include "transport.h"

// static StreamBufferHandle_t mic_stream_buf;
// static StreamBufferHandle_t network_stream_buf;

static const char* TAG = "main";

void app_main(void) {

    ESP_LOGI(TAG, "Starting program");

    // mic_stream_buf = xStreamBufferCreate(512, 1);
    // network_stream_buf = xStreamBufferCreate(512, 1);

    // init_transport(mic_stream_buf, network_stream_buf);
    // init_audio(mic_stream_buf, network_stream_buf);

    // Loopback testing
    // init_audio(mic_stream_buf, mic_stream_buf);
}
