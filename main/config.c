#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "espnow_recv.h"
#include "esp_private/wifi.h"

static const char* TAG = "espnow_mic";
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
extern uint8_t* mic_read_buf;
extern uint8_t* spk_write_buf;

/* initialize nvm */
void init_non_volatile_storage(void) {
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        printf("Error initializing NVS\n");
    }
}

/**
 * @brief I2S config for using internal ADC and DAC
 * one time set up
 * dma calculation reference:
 * https://www.atomic14.com/2021/04/20/esp32-i2s-dma-buf-len-buf-count.html bit_per_sample: 16 bits
 * for adc num_of_channels: 1 for adc, 2 for speaker sample_rate: 44100 for adc mic buffer size
 * minumum: sample_rate * num_of_channels * worst_case_processing_time (100ms = 0.1s) = 44100 * 1 *
 * 0.1 = 4410 spk buffer size mimimum: sample_rate * num_of_channels * worst_case_processing_time
 * (100ms = 0.1s) = 44100 * 2 * 0.1 = 8810 realistic cap is 100KB = bit_per_sample * num_of_channels
 * * num_of_dma_descriptors * num_of_dma_frames / 8
 */
void i2s_adc_dac_config(fsm_state_t state) {
    int i2s_num = EXAMPLE_I2S_NUM;

    i2s_channel_fmt_t channel_format;
    if (state == TX_STATE) {
        channel_format = EXAMPLE_I2S_FORMAT; // only right channel for adc
    }
    else {
        channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; // only right channel for adc
    }

    i2s_config_t i2s_config = {
        .mode =
            I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN |
            I2S_MODE_ADC_BUILT_IN, // master and rx for mic, tx for speaker, adc for internal adc
        .sample_rate = EXAMPLE_I2S_SAMPLE_RATE,            // 16KHz for adc
        .bits_per_sample = EXAMPLE_I2S_SAMPLE_BITS,        // 16 bits for adc
        .communication_format = I2S_COMM_FORMAT_STAND_MSB, // standard format for adc
        .channel_format = channel_format,
        .intr_alloc_flags = 0, // default interrupt priority
        .dma_buf_count = 4,    // number of dma descriptors, or count for adc
        .dma_buf_len = 512,    // number of dma frames, or length for adc
        .use_apll =
            false, // use apll for adc. if false, peripheral clock is derived and used for better
                   // wifi transmission performance (pending test) .tx_desc_auto_clear = false, //
                   // i2s auto clear tx descriptor on underflow
    };
    // install and start i2s driver
    i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    // init ADC pad
    i2s_set_adc_mode(I2S_ADC_UNIT, I2S_ADC_CHANNEL);
    // // init DAC pad
    i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
    // set i2s clock source for i2s mic
    i2s_set_clk(i2s_num, EXAMPLE_I2S_SAMPLE_RATE * 1.25, EXAMPLE_I2S_SAMPLE_BITS,
                EXAMPLE_I2S_CHANNEL_NUM);
    // the audio quality was fixed by setting the i2s clock source to ((adc_bits+8)/16) times the
    // sample rate, which is 1.25 times the sample rate reference:
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
    // reference:
    // https://docs.espressif.com/projects/esp-idf/en/v4.2/esp32/api-reference/peripherals/i2s.html#_CPPv49i2s_write10i2s_port_tPKv6size_tP6size_t10TickType_t
    // reference: i2s_write(I2S_NUM, samples_data, ((bits+8)/16)*SAMPLE_PER_CYCLE*4,
    // &i2s_bytes_write, 100);

    if (state == RX_STATE) {
        // init DAC pad (GPIO25 & GPIO26) & mode
        i2s_set_pin(i2s_num, NULL);
        // set i2s clock source for i2s spk
        i2s_set_clk(i2s_num, EXAMPLE_I2S_SAMPLE_RATE, EXAMPLE_I2S_SAMPLE_BITS, 1);
        // set i2s sample rate for respective dac channel of i2s spk (clock source is set automatically
        // by the function)
        i2s_set_sample_rates(i2s_num, EXAMPLE_I2S_SAMPLE_RATE);
    }
}

void init_config(void) {
    // get the clock rate for adc and dac
    // float freq = i2s_get_clk(EXAMPLE_I2S_NUM);
    // printf("i2s clock rate: %f, sample rate: %d, bits per sample: %d \n", freq,
    //        EXAMPLE_I2S_SAMPLE_RATE, EXAMPLE_I2S_SAMPLE_BITS);

    /**
     * for configuring i2s-speaker only
     */
    // esp_log_level_set("I2S", ESP_LOG_INFO);
}

// terminate espnow, i2s, wifi
void deinit_config(fsm_state_t state) {
    // esp_now_deinit();
    if (state == RX_STATE) {
        i2s_adc_disable(EXAMPLE_I2S_NUM);
        ESP_LOGI(TAG, "ADC disabled");
        free(mic_read_buf);
    }
    else {
        i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
        ESP_LOGI(TAG, "DAC disabled");
        free(spk_write_buf);
    }
    i2s_driver_uninstall(EXAMPLE_I2S_NUM);
}