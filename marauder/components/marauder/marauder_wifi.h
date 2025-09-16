#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct marauder_wifi_scan_result_s {
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    uint8_t bssid[6];
    bool wpa2;
} marauder_wifi_scan_result_t;

typedef struct marauder_wifi_s marauder_wifi_t;

marauder_wifi_t *marauder_wifi_init(void);
int marauder_wifi_scan(marauder_wifi_t *wifi, marauder_wifi_scan_result_t *out, int max);
void marauder_wifi_shutdown(marauder_wifi_t *wifi);


