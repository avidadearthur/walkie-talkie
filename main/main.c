#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "espnow_mic.h"
#include "espnow_send.h"
#include "espnow_recv.h"
#include "sd_record.h"

static const char* TAG = "main.c";

/* -------------------- Interrupt handling --------------------------------------*/
bool interrupt_flag = false;
uint32_t last_button_isr_time = 0;

SemaphoreHandle_t xSemaphore;

void IRAM_ATTR set_button_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t isr_time = xTaskGetTickCountFromISR();

    if ((isr_time - last_button_isr_time) > pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS)) {

        if (my_state == RX_STATE) {
            my_state = TX_STATE;
        } else if (my_state == TX_STATE) {
            my_state = RX_STATE;
        }

        xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

        last_button_isr_time = isr_time; // Save last ISR time
    }
}
/*--------------------------------------------------------------------------------------*/

// move to config.h later
void init_gipio(void) {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = (1ULL << SET_BUTTON_GPIO);
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(SET_BUTTON_GPIO, set_button_isr_handler, NULL);
}
/*--------------------------------------------------------------------------------------*/

#if (RECV)
static StreamBufferHandle_t network_stream_buf; // only for reciever
#else
static StreamBufferHandle_t mic_stream_buf;
static StreamBufferHandle_t record_stream_buf; // only for transmitter
#endif

void init(void) {
    // deafult transmission rate of esp_now_send is 1Mbps = 125KBps, stream buffer size has to be
    // larger than 125KBps
#if (!RECV)
    mic_stream_buf = xStreamBufferCreate(EXAMPLE_I2S_READ_LEN, 1);
    // check if the stream buffer is created
    if (mic_stream_buf == NULL) {
        printf("Error creating mic stream buffer: %d\n", errno);
        deinit_config();
        exit(errno);
    }

#if (!RECV) & (RECORD_TASK)
    record_stream_buf = xStreamBufferCreate(EXAMPLE_I2S_READ_LEN, 1);
    // check if the stream buffer is created
    if (record_stream_buf == NULL) {
        printf("Error creating record stream buffer: %d\n", errno);
        deinit_config();
        exit(errno);
    }
#endif

#else
    network_stream_buf = xStreamBufferCreate(BYTE_RATE, 1);
    // check if the stream buffer is created
    if (network_stream_buf == NULL) {
        printf("Error creating network stream buffer: %d\n", errno);
        deinit_config();
        exit(errno);
    }
#endif

#if (RECV)
    // initialize the reciever and audio (only for reciever)
    init_recv(network_stream_buf);
    init_audio_recv(network_stream_buf);
#endif

    // initialize espnow, nvm, wifi, and i2s configuration
    init_config();

#if (!RECV)
    init_transmit(mic_stream_buf);
    init_audio_trans(mic_stream_buf, record_stream_buf);
#endif
}

void app_main(void) {
    init();
}
