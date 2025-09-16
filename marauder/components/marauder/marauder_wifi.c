#include "marauder_wifi.h"

#include <string.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>

struct marauder_wifi_s {
    bool initialized;
};

static void ensure_nvs_init(void)
{
    static bool nvs_done = false;
    if (nvs_done)
        return;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }
    nvs_done = true;
}

marauder_wifi_t *marauder_wifi_init(void)
{
    static struct marauder_wifi_s wifi = {0};
    if (wifi.initialized)
        return &wifi;

    ensure_nvs_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    wifi.initialized = true;
    return &wifi;
}

int marauder_wifi_scan(marauder_wifi_t *wifi, marauder_wifi_scan_result_t *out, int max)
{
    (void)wifi;
    if (!out || max <= 0)
        return 0;

    wifi_scan_config_t scan_cfg = {0};
    scan_cfg.show_hidden = true;
    esp_wifi_scan_start(&scan_cfg, true);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    wifi_ap_record_t *records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!records)
        return 0;

    uint16_t to_get = ap_count;
    esp_wifi_scan_get_ap_records(&to_get, records);

    int written = 0;
    for (int i = 0; i < to_get && written < max; i++)
    {
        marauder_wifi_scan_result_t *r = &out[written++];
        memset(r, 0, sizeof(*r));
        strncpy(r->ssid, (const char *)records[i].ssid, sizeof(r->ssid) - 1);
        r->rssi = records[i].rssi;
        r->channel = records[i].primary;
        memcpy(r->bssid, records[i].bssid, 6);
        r->wpa2 = (records[i].authmode >= WIFI_AUTH_WPA2_PSK);
    }

    free(records);
    return written;
}

void marauder_wifi_shutdown(marauder_wifi_t *wifi)
{
    (void)wifi;
}


