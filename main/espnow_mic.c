#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "espnow_mic.h"
#include "sd_record.h"
#include "udp_client.h"

static const char* TAG = "espnow_mic";
StreamBufferHandle_t fft_stream_buf;
StreamBufferHandle_t record_stream_buf;
bool is_first_time = true;
//

// initially both devices are in RX_STATE
extern fsm_state_t my_state;
extern fsm_state_t peer_state;

StreamBufferHandle_t mic_stream_buffer;
StreamBufferHandle_t spk_stream_buffer;

uint8_t* mic_read_buf;
uint8_t* spk_write_buf;

// reference: https://www.codeinsideout.com/blog/freertos/notification/#two-looping-tasks
TaskHandle_t adcTaskHandle = NULL;

//
TaskHandle_t adcdacTaskHandle = NULL;

// suspend i2s_adc_capture_task function
void suspend_adc_capture_task() {
    assert(adcTaskHandle != NULL);
    vTaskSuspend(adcTaskHandle);
    // ESP_LOGI(TAG, "adc capture task suspended\n");
}

// resume i2s_adc_capture_task function
void resume_adc_capture_task() {
    assert(adcTaskHandle != NULL);
    vTaskResume(adcTaskHandle);
    // ESP_LOGI(TAG, "adc capture task resumed\n");
}

// get the task handle of i2s_adc_capture_task
TaskHandle_t get_adc_task_handle() {
    assert(adcTaskHandle != NULL);
    return adcTaskHandle;
}

/**
 * @brief Scale data to 8bit for data from ADC.
 *        DAC can only output 8 bit data.
 *        Scale each 16bit-wide ADC data to 8bit DAC data.
 */
void i2s_adc_data_scale(uint8_t* des_buff, uint8_t* src_buff, uint32_t len) {
    uint32_t j = 0;
    uint32_t dac_value = 0;
    for (int i = 0; i < len; i += 2) {
        dac_value = ((((uint16_t)(src_buff[i + 1] & 0xf) << 8) | ((src_buff[i + 0]))));
        des_buff[j++] = 0;
        des_buff[j++] = dac_value * 256 / 4096;
    }
}

// i2s adc capture task
void i2s_adc_dac_task(void* task_param) {
    while (1) {
        if (my_state == TX_STATE) {
            i2s_adc_dac_config(my_state);

            int read_len = (EXAMPLE_I2S_READ_LEN / 2) * sizeof(char);
            mic_read_buf = calloc(EXAMPLE_I2S_READ_LEN, sizeof(char));

            size_t bytes_read = 0;          // to count the number of bytes read from the i2s adc
            TickType_t ticks_to_wait = 100; // wait 100 ticks for the mic_stream_buf to be available

            i2s_adc_enable(EXAMPLE_I2S_NUM);

            while (my_state == TX_STATE) {
                // read from i2s bus and use errno to check if i2s_read is successful
                if (i2s_read(EXAMPLE_I2S_NUM, (char*)mic_read_buf, read_len, &bytes_read,
                             ticks_to_wait) != ESP_OK) {
                    ESP_LOGE(TAG, "Error reading from i2s adc: %d", errno);
                    deinit_config(my_state);
                    exit(errno);
                }

                // check if the number of bytes read is equal to the number of bytes to read
                if (bytes_read != read_len) {
                    ESP_LOGE(TAG, "Error reading from i2s adc: %d", errno);
                    deinit_config(my_state);
                    exit(errno);
                }

                // scale the data to 8 bit
                i2s_adc_data_scale(mic_read_buf, mic_read_buf, read_len);

                size_t espnow_byte = xStreamBufferSend(mic_stream_buffer, (void*)mic_read_buf,
                                                       read_len, portMAX_DELAY);
                if (espnow_byte != read_len) {
                    ESP_LOGE(TAG, "Error: only sent %d bytes to the stream buffer out of %d",
                             espnow_byte, read_len);
                }
            }
            deinit_config(my_state);

        } else if (my_state == RX_STATE) {
            size_t bytes_written = 0;
            spk_write_buf = (uint8_t*)calloc(BYTE_RATE, sizeof(char));

            const TickType_t ticks_to_wait = 0xFF;

            i2s_adc_dac_config(my_state);
            while (my_state == RX_STATE) {
                ESP_LOGI(TAG, "RX_STATE");
                // read from the stream buffer, use errno to check if xstreambufferreceive is
                // successful
                size_t num_bytes = xStreamBufferReceive(spk_stream_buffer, (void*)spk_write_buf,
                                                        BYTE_RATE, ticks_to_wait);
                if (num_bytes > 0) {
                    // send data to i2s dac
                    esp_err_t err = i2s_write(EXAMPLE_I2S_NUM, spk_write_buf, num_bytes,
                                              &bytes_written, portMAX_DELAY);
                    if ((err != ESP_OK)) {
                        ESP_LOGE(TAG, "Error writing I2S: %0x", err);
                    }
                }
            }
            deinit_config(my_state);
        }
    }
    vTaskDelete(NULL);
}

/* new task combining adc and dac tasks */
esp_err_t init_audio_transport(StreamBufferHandle_t mic_stream_buf,
                               StreamBufferHandle_t spk_stream_buf) {

    ESP_LOGI(TAG, "initializing i2s adc_dac_task");
    mic_stream_buffer = mic_stream_buf;
    spk_stream_buffer = spk_stream_buf;

    // create the adc capture task and pin the task to core 0
    xTaskCreate(i2s_adc_dac_task, "i2s_adc_dac_task", 4096, NULL, IDLE_TASK_PRIO,
                &adcdacTaskHandle);
    configASSERT(adcdacTaskHandle);

    return ESP_OK;
}

// /** debug functions below */

/**
 * @brief debug buffer data
 */
void mic_disp_buf(uint8_t* buf, int length) {
#if EXAMPLE_I2S_BUF_DEBUG
    ESP_LOGI(TAG, "\n=== MIC ===\n");
    for (int i = 0; i < length; i++) {
        ESP_LOGI(TAG, "%02x ", buf[i]);
        if ((i + 1) % 8 == 0) {
            ESP_LOGI(TAG, "\n");
        }
    }
    ESP_LOGI(TAG, "\n=== MIC ===\n");
#endif
}
