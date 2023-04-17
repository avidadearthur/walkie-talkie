#include "simple_transport.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_private/wifi.h"

#define WIFI_CHANNEL (12)

static uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t espnow_data[250];

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

    err = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting wifi channel");
    }

    err = esp_wifi_internal_set_fix_rate(ESP_IF_WIFI_STA, 1, WIFI_PHY_RATE_MCS7_SGI);
}

static void recv_callback(const uint8_t* mac_addr, const uint8_t* data, int len) {

    ESP_LOGI(TAG, "Received %u bytes from %02x:%02x:%02x:%02x:%02x:%02x", len, mac_addr[0],
             mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

static void init_esp_now() {
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        printf("Error starting ESP NOW\n");
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

static void sender_task(void* arg) {

    while (1) {

        for (int i = 0; i < 250; i++) {
            espnow_data[i] = i;
        }
        esp_err_t err = esp_now_send(broadcast_mac, espnow_data, 250);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error sending ESP NOW data");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
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

void init_simple_transport(void) {
    // Log init of Simple Transport with LOGI
    ESP_LOGI(TAG, "Initializing Simple Transport");

    init_non_volatile_storage();
    init_wifi();
    init_esp_now();

    xTaskCreate(sender_task, "sender_task", 4096, NULL, 10, NULL);
}
