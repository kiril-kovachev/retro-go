#include "marauder_ble.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#endif

typedef struct {
    marauder_ble_scan_result_t *out;
    int max;
    int count;
} ble_collect_ctx_t;

struct marauder_ble_s {
    bool initialized;
    ble_collect_ctx_t collect;
};

static marauder_ble_t g_ble = {0};

#if CONFIG_IDF_TARGET_ESP32
static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_SCAN_RESULT_EVT)
    {
        esp_ble_gap_cb_param_t *p = param;
        if (p->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT)
        {
            ble_collect_ctx_t *c = &g_ble.collect;
            if (!c->out || c->count >= c->max)
                return;

            marauder_ble_scan_result_t *r = &c->out[c->count];
            memset(r, 0, sizeof(*r));
            memcpy(r->addr, p->scan_rst.bda, 6);
            r->rssi = p->scan_rst.rssi;

            uint8_t *adv_name = NULL;
            uint8_t adv_name_len = 0;
            adv_name = esp_ble_resolve_adv_data(p->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            if (adv_name && adv_name_len)
            {
                int n = adv_name_len < (int)sizeof(r->name) - 1 ? adv_name_len : (int)sizeof(r->name) - 1;
                memcpy(r->name, adv_name, n);
                r->name[n] = '\0';
            }

            c->count++;
        }
    }
}
#endif

marauder_ble_t *marauder_ble_init(void)
{
    if (g_ble.initialized)
        return &g_ble;

#if CONFIG_IDF_TARGET_ESP32
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gap_register_callback(gap_cb);
#endif

    g_ble.initialized = true;
    return &g_ble;
}

int marauder_ble_scan(marauder_ble_t *ble, marauder_ble_scan_result_t *out, int max, int duration_seconds)
{
    (void)ble;
    if (!out || max <= 0)
        return 0;

#if CONFIG_IDF_TARGET_ESP32
    g_ble.collect.out = out;
    g_ble.collect.max = max;
    g_ble.collect.count = 0;

    esp_ble_gap_set_scan_params(&(esp_ble_scan_params_t){
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,
        .scan_window = 0x30,
    });

    esp_ble_gap_start_scanning(duration_seconds);
    // Note: API is blocking for duration; when it returns, collect.count is final

    return g_ble.collect.count;
#else
    (void)out;
    (void)max;
    (void)duration_seconds;
    return 0;
#endif
}

void marauder_ble_shutdown(marauder_ble_t *ble)
{
    (void)ble;
}


