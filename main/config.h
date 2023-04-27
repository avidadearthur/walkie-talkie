#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_log.h"

#define WIFI_CHANNEL (12)
#define STATE_CHANNEL (0)

#define ESPNOW_TX_MODE (1)
#define ESPNOW_RX_MODE (0)