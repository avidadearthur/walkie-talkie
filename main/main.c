#include "config.h"
#include "simple_transport.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
// #include "audio.h"
// #include "transport.h"

// static StreamBufferHandle_t mic_stream_buf;
// static StreamBufferHandle_t network_stream_buf;

#define SET_BUTTON_GPIO 33
#define BUTTON_DEBOUNCE_TIME_MS 300

static const char* TAG = "main";

fsm_state_t current_state = RX_STATE;

bool interrupt_flag = false;
uint32_t last_button_isr_time = 0;

SemaphoreHandle_t xSemaphore;

void IRAM_ATTR set_button_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t isr_time = xTaskGetTickCountFromISR();

    if ((isr_time - last_button_isr_time) > pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS)) {

        if (current_state == RX_STATE) {
            current_state = TX_STATE;
        } else if (current_state == TX_STATE) {
            current_state = RX_STATE;
        }

        xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

        last_button_isr_time = isr_time; // Save last ISR time
    }
}

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

void app_main(void) {

    ESP_LOGI(TAG, "Starting program");

    init_gipio();

    xSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(
        xSemaphore); // force if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) to be true

    init_simple_transport();

    if (current_state == TX_STATE) {
        xTaskCreate(sender_task, "sender_task", 4096, NULL, 10, NULL);
    }

    while (1) {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
            // Wait for semaphore
            xSemaphoreTakeFromISR(xSemaphore, NULL);

            // Perform actions for the current state
            switch (current_state) {
                case RX_STATE:
                    ESP_LOGI(TAG, "Current state: RX_STATE");

                    signal_RXTX_toggle();

                    // Log the peer state
                    if (peer_state == RX_STATE) {
                        ESP_LOGI(TAG, "Peer state: RX_STATE");
                    } else if (peer_state == TX_STATE) {
                        ESP_LOGI(TAG, "Peer state: TX_STATE");
                    }

                    break;

                case TX_STATE:
                    ESP_LOGI(TAG, "Current state: TX_STATE");

                    signal_RXTX_toggle();

                    // Log the peer state
                    if (peer_state == RX_STATE) {
                        ESP_LOGI(TAG, "Peer state: RX_STATE");
                    } else if (peer_state == TX_STATE) {
                        ESP_LOGI(TAG, "Peer state: TX_STATE");
                    }

                    break;

                default:
                    break;
            }
            interrupt_flag = false; // Clear interrupt flag
        } else {
            // Enter idle state to wait for new interrupts
            vTaskSuspend(NULL);
        }
    }
    vTaskDelete(NULL);

    // mic_stream_buf = xStreamBufferCreate(512, 1);
    // network_stream_buf = xStreamBufferCreate(512, 1);

    // init_transport(mic_stream_buf, network_stream_buf);
    // init_audio(mic_stream_buf, network_stream_buf);

    // Loopback testing
    // init_audio(mic_stream_buf, mic_stream_buf);
}
