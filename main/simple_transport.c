#include "simple_transport.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_private/wifi.h"
#include <string.h>

static uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t espnow_data[250];

extern fsm_state_t current_state;
static fsm_state_t peer_state;

static StreamBufferHandle_t network_stream_buf;
static StreamBufferHandle_t microphone_stream_buf;

// task handle for sender_task
TaskHandle_t sender_task_handle;

static const char* TAG = "simple_transport";

static void init_wifi() {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing netif");
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error creating event loop");
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing wifi");
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting wifi storage");
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting wifi mode");
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error starting wifi");
    }

    err = esp_wifi_internal_set_fix_rate(ESP_IF_WIFI_STA, 1, WIFI_PHY_RATE_MCS7_SGI);
}

static void recv_callback(const uint8_t* mac_addr, const uint8_t* data, int len) {

    // Bad way of checking if the message is a state message
    bool all_zeros = true;
    for (int i = 0; i < len; i++) {
        if (data[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    bool all_ones = true;
    for (int i = 0; i < len; i++) {
        if (data[i] != 1) {
            all_ones = false;
            break;
        }
    }

    if (all_ones) {
        peer_state = RX_STATE;
    } else if (all_zeros) {
        peer_state = TX_STATE;
    } else {
        ESP_LOGE(TAG, "Received invalid message");
    }

    // log the new state of the peer
    if (all_ones) {
        ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x is now in RX state", mac_addr[0], mac_addr[1],
                 mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    } else if (all_zeros) {
        ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x is now in TX state", mac_addr[0], mac_addr[1],
                 mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
        ESP_LOGI(TAG, "Received %d bytes from %02x:%02x:%02x:%02x:%02x:%02x", len, mac_addr[0],
                 mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    }
}

static void init_esp_now_peer() {

    // test setting the channel here instead of in init_wifi()
    esp_err_t err = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting wifi channel");
    }

    err = esp_now_register_recv_cb(recv_callback);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error registering ESP NOW receive callback");
    }

    esp_now_peer_info_t broadcast_peer = {
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        .lmk = {0},
        .channel = WIFI_CHANNEL, // ranges from 0-14
        .ifidx = ESP_IF_WIFI_STA,
        .encrypt = false,
        .priv = NULL,
    };

    err = esp_now_add_peer(&broadcast_peer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error adding peer");
    }
}

void sender_task(void* arg) {

    while (true) {
        size_t num_bytes = xStreamBufferReceive(microphone_stream_buf, (void*)esp_now_send_buf,
                                                sizeof(esp_now_send_buf), portMAX_DELAY);

        if (num_bytes > 0) {
            // printf("Received %u bytes\n", num_bytes);

            esp_err_t err = esp_now_send(broadcast_mac, esp_now_send_buf, sizeof(esp_now_send_buf));
            if (err != ESP_OK) {
                printf("Error sending ESP NOW packet: %x\n", err);
            }
        }
    }
}

void signal_RXTX_toggle() {
    uint8_t espnow_mode;

    if (current_state == RX_STATE) {
        espnow_mode = ESPNOW_TX_MODE;
    } else {
        espnow_mode = ESPNOW_RX_MODE;
    }

    for (int i = 0; i < 250; i++) {
        espnow_data[i] = espnow_mode;
    }
    // Log what is being sent to the peer (espnow_mode)
    ESP_LOGI(TAG, "Changing to mode %d", espnow_mode);
    esp_err_t err = esp_now_send(broadcast_mac, espnow_data, 250);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error sending ESP NOW data");
    }
}

static void init_non_volatile_storage() {
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing NVS");
    }
}

void init_simple_transport(StreamBufferHandle_t mic_stream_buf,
                           StreamBufferHandle_t net_stream_buf) {
    // Log init of Simple Transport with LOGI
    ESP_LOGI(TAG, "Initializing Simple Transport");

    network_stream_buf = net_stream_buf;
    microphone_stream_buf = mic_stream_buf;

    init_non_volatile_storage();
    init_wifi();

    // test moving this out of init_esp_now
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        printf("Error starting ESP NOW\n");
    }

    init_esp_now_peer();

    xTaskCreate(sender_task, "sender_task", 4096, NULL, 10, sender_task_handle);
    // suspend the sender task until the peer is in the RX state
    vTaskSuspend(sender_task_handle);
}
